#include "TaskForgeWorker.h"

#include "Job.h"
#include "JobPriorities.h"
#include "Logger.h"
#include "MessageValidation.h"
#include "ThreadWorker.h"

#include <aws/core/http/Scheme.h>

#include <chrono>
#include <ctime>
#include <format>
#include <random>
#include <thread>

using namespace Thread;
using namespace Utilities;

namespace
{
	constexpr int redis_connect_attempts = 5;
	constexpr int s3_download_attempts = 3;

	constexpr int retry_base_delay_ms = 1000;
	constexpr int retry_max_delay_ms = 8000;
	constexpr int download_base_delay_ms = 500;
	constexpr int download_max_delay_ms = 4000;

	// 지수 백오프 + equal jitter (NFR-REL-01). jitter가 없으면 다중 인스턴스의
	// 재시도가 동시에 몰려(thundering herd) 복구를 방해한다
	auto backoff_with_jitter(int attempt, int base_delay_ms, int max_delay_ms) -> std::chrono::milliseconds
	{
		auto shift = (attempt > 1) ? (attempt - 1) : 0;
		if (shift > 16)
		{
			shift = 16;
		}

		auto exponential = (long long)base_delay_ms << shift;
		auto capped = (exponential > max_delay_ms) ? (long long)max_delay_ms : exponential;

		thread_local std::mt19937 generator(std::random_device{}());
		std::uniform_int_distribution<long long> distribution(0, capped / 2);

		return std::chrono::milliseconds(capped + distribution(generator));
	}

	// 계약 C-1은 task_name·enqueued_at을 "항상 존재"로 규정하지만 두 필드의 출처는
	// 인터페이스가 쓴 queued 초기 문서뿐이다. 그 문서가 없는 경우(SQS 직접 발행 ·
	// Redis 비영속 재시작 · TTL 만료)에도 규약을 지키려면 모든 상태 전이가 메시지에서
	// 복원해야 한다. 스키마 위반 메시지에도 쓰이므로 필드를 개별적으로 방어한다
	auto contract_fields(const boost::json::object& message) -> boost::json::object
	{
		boost::json::object fields;

		if (message.contains("task") && message.at("task").is_object())
		{
			const auto& task = message.at("task").as_object();
			if (task.contains("name") && task.at("name").is_string())
			{
				fields["task_name"] = task.at("name");
			}
		}

		if (message.contains("enqueued_at") && message.at("enqueued_at").is_string())
		{
			fields["enqueued_at"] = message.at("enqueued_at");
		}

		return fields;
	}

	auto current_utc_timestamp(void) -> std::string
	{
		auto now = std::chrono::system_clock::now();
		auto now_time = std::chrono::system_clock::to_time_t(now);

		std::tm utc_tm{};
		gmtime_r(&now_time, &utc_tm);

		char buffer[32];
		std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &utc_tm);

		return buffer;
	}

	auto key_filename(const std::string& object_key) -> std::string
	{
		auto separator = object_key.find_last_of('/');
		if (separator == std::string::npos)
		{
			return object_key;
		}

		return object_key.substr(separator + 1);
	}

	// 작업별 임시 디렉토리를 만들고 스코프 종료 시 재귀 삭제하는 가드
	struct TempDirGuard
	{
		std::filesystem::path root;

		TempDirGuard(const std::string& job_id) : root(std::filesystem::temp_directory_path() / "taskforge" / job_id)
		{
			std::error_code error_code;
			std::filesystem::create_directories(root / "inputs", error_code);
			std::filesystem::create_directories(root / "outputs", error_code);
		}

		~TempDirGuard(void)
		{
			std::error_code error_code;
			std::filesystem::remove_all(root, error_code);
		}

		auto inputs(void) const -> std::filesystem::path { return root / "inputs"; }
		auto outputs(void) const -> std::filesystem::path { return root / "outputs"; }
		auto stdout_log(void) const -> std::filesystem::path { return root / "stdout.log"; }
	};
}

TaskForgeWorker::TaskForgeWorker(std::shared_ptr<Configurations> configurations)
	: configurations_(configurations)
	, thread_pool_(nullptr)
	, redis_client_(nullptr)
	, s3_client_(nullptr)
	, sqs_consumer_(nullptr)
	, sqs_publisher_(nullptr)
	, task_executor_(nullptr)
	, task_dlq_(nullptr)
	, running_(false)
	, stop_future_(stop_promise_.get_future())
{
}

TaskForgeWorker::~TaskForgeWorker(void) { stop(); }

auto TaskForgeWorker::start(void) -> std::expected<void, std::string>
{
	auto required = configurations_->validate_required();
	if (!required)
	{
		return std::unexpected(std::format("invalid configuration: {}", required.error()));
	}

	running_.store(true);

	task_executor_ = std::make_unique<TaskExecutor>(configurations_->task_whitelist(), configurations_->root_path(), configurations_->default_timeout_sec());
	task_dlq_ = std::make_unique<TaskDLQ>(configurations_->dlq_path(), configurations_->dlq_max_retry_count());

	auto pool_created = create_thread_pool();
	if (!pool_created)
	{
		return std::unexpected(std::format("cannot create thread pool: {}", pool_created.error()));
	}

	auto redis_connected = connect_redis();
	if (!redis_connected)
	{
		return std::unexpected(std::format("cannot connect redis: {}", redis_connected.error()));
	}

	auto catalog_uploaded = upload_task_catalog();
	if (!catalog_uploaded)
	{
		return std::unexpected(std::format("cannot upload task catalog: {}", catalog_uploaded.error()));
	}

	auto aws_connected = connect_aws_service();
	if (!aws_connected)
	{
		return std::unexpected(std::format("cannot connect aws service: {}", aws_connected.error()));
	}

	auto monitor_started = start_dlq_backlog_monitor();
	if (!monitor_started)
	{
		return std::unexpected(std::format("cannot start dlq backlog monitor: {}", monitor_started.error()));
	}

	Logger::handle().write(LogTypes::Information, "TaskForgeWorker started");

	return {};
}

auto TaskForgeWorker::stop(void) -> void
{
	bool expected = true;
	if (!running_.compare_exchange_strong(expected, false))
	{
		return;
	}

	if (sqs_consumer_ != nullptr)
	{
		sqs_consumer_->stop_consume();
		sqs_consumer_->stop();
		sqs_consumer_.reset();
	}

	sqs_publisher_.reset();
	s3_client_.reset();

	if (thread_pool_ != nullptr)
	{
		thread_pool_->stop(true);
		thread_pool_.reset();
	}

	if (redis_client_ != nullptr)
	{
		redis_client_->disconnect();
		redis_client_.reset();
	}

	stop_promise_.set_value();

	Logger::handle().write(LogTypes::Information, "TaskForgeWorker stopped");
}

auto TaskForgeWorker::wait_stop(void) -> void { stop_future_.wait(); }

auto TaskForgeWorker::create_thread_pool(void) -> std::expected<void, std::string>
{
	thread_pool_ = std::make_shared<ThreadPool>("TaskForgeWorkerPool");

	for (auto i = 0; i < configurations_->high_priority_count(); i++)
	{
		thread_pool_->push(std::make_shared<ThreadWorker>(std::vector<JobPriorities>{ JobPriorities::High }));
	}

	for (auto i = 0; i < configurations_->normal_priority_count(); i++)
	{
		thread_pool_->push(std::make_shared<ThreadWorker>(std::vector<JobPriorities>{ JobPriorities::Normal, JobPriorities::High }));
	}

	for (auto i = 0; i < configurations_->low_priority_count(); i++)
	{
		thread_pool_->push(std::make_shared<ThreadWorker>(std::vector<JobPriorities>{ JobPriorities::Low, JobPriorities::Normal }));
	}

	// DLQ 백로그 모니터 전용 워커 (장기 실행 루프와 일반 작업을 격리)
	thread_pool_->push(std::make_shared<ThreadWorker>(std::vector<JobPriorities>{ JobPriorities::LongTerm }));

	return thread_pool_->start();
}

auto TaskForgeWorker::connect_redis(void) -> std::expected<void, std::string>
{
	redis_client_ = std::make_shared<Redis::RedisClient>(configurations_->redis_host(), configurations_->redis_port(), Redis::TLSOptions(),
														 configurations_->redis_db_index());

	std::string last_error = "";
	for (auto attempt = 1; attempt <= redis_connect_attempts; attempt++)
	{
		auto connected = redis_client_->connect();
		if (connected)
		{
			// connect()는 lazy이므로 실제 명령으로 연결을 검증합니다
			auto verified = redis_client_->set("taskforge:healthcheck", "ok", 5);
			if (verified)
			{
				Logger::handle().write(LogTypes::Information,
									   std::format("redis connected: {}:{}", configurations_->redis_host(), configurations_->redis_port()));

				return {};
			}

			last_error = verified.error();
		}
		else
		{
			last_error = connected.error();
		}

		Logger::handle().write(LogTypes::Warning, std::format("redis connect attempt {}/{} failed: {}", attempt, redis_connect_attempts, last_error));
		std::this_thread::sleep_for(std::chrono::milliseconds(500 * attempt));
	}

	return std::unexpected(last_error);
}

auto TaskForgeWorker::upload_task_catalog(void) -> std::expected<void, std::string>
{
	boost::json::array catalog;
	for (const auto& definition : configurations_->task_whitelist())
	{
		boost::json::object entry;
		entry["name"] = definition.name;
		entry["description"] = definition.description;
		catalog.push_back(entry);
	}

	boost::json::object document;
	document["tasks"] = catalog;
	document["updated_at"] = current_utc_timestamp();

	auto uploaded = redis_client_->set("task_catalog", boost::json::serialize(document));
	if (!uploaded)
	{
		return std::unexpected(uploaded.error());
	}

	Logger::handle().write(LogTypes::Information, std::format("task catalog uploaded: {} tasks", catalog.size()));

	return {};
}

auto TaskForgeWorker::make_client_config(void) const -> Aws::Client::ClientConfiguration
{
	Aws::Client::ClientConfiguration config;
	config.region = configurations_->aws_region().c_str();

	if (!configurations_->aws_endpoint().empty())
	{
		// LocalStack 등 커스텀 엔드포인트는 HTTP + TLS 미검증으로 접근합니다
		config.endpointOverride = configurations_->aws_endpoint().c_str();
		config.scheme = Aws::Http::Scheme::HTTP;
		config.verifySSL = false;
	}

	return config;
}

auto TaskForgeWorker::connect_aws_service(void) -> std::expected<void, std::string>
{
	auto config = make_client_config();
	bool use_virtual_addressing = configurations_->aws_endpoint().empty();

	if (!configurations_->aws_access_key().empty())
	{
		s3_client_ = std::make_unique<AWSService::AWSS3Client>(Aws::String(configurations_->aws_access_key().c_str()),
															   Aws::String(configurations_->aws_secret_key().c_str()), config, use_virtual_addressing);
		sqs_publisher_ = std::make_unique<AWSService::AWSSQSPublisher>(configurations_->aws_access_key(), configurations_->aws_secret_key(), config);
	}
	else
	{
		s3_client_ = std::make_unique<AWSService::AWSS3Client>(config, use_virtual_addressing);
		sqs_publisher_ = std::make_unique<AWSService::AWSSQSPublisher>(config);
	}

	sqs_publisher_->sqs_url(configurations_->sqs_request_queue_url());

	AWSService::AWSSQSConsumerConfig consume_config;
	consume_config.wait_time_seconds = configurations_->sqs_wait_time_seconds();
	consume_config.visibility_timeout = configurations_->sqs_visibility_timeout();
	consume_config.max_number_of_messages = configurations_->sqs_max_number_of_messages();

	if (!configurations_->aws_access_key().empty())
	{
		sqs_consumer_ = std::make_unique<AWSService::AWSSQSConsumer>(configurations_->aws_access_key(), configurations_->aws_secret_key(), config,
																	 consume_config);
	}
	else
	{
		sqs_consumer_ = std::make_unique<AWSService::AWSSQSConsumer>(config, consume_config);
	}

	sqs_consumer_->sqs_url(configurations_->sqs_request_queue_url());

	auto handler_registered = sqs_consumer_->register_consume_handler([this](const std::string& message_body) -> std::expected<void, std::string>
																	  { return handle_message(message_body); });
	if (!handler_registered)
	{
		return std::unexpected(handler_registered.error());
	}

	auto started = sqs_consumer_->start();
	if (!started)
	{
		return std::unexpected(started.error());
	}

	auto consuming = sqs_consumer_->start_consume();
	if (!consuming)
	{
		return std::unexpected(consuming.error());
	}

	Logger::handle().write(LogTypes::Information, std::format("sqs consuming started: {}", configurations_->sqs_request_queue_url()));

	return {};
}

auto TaskForgeWorker::start_dlq_backlog_monitor(void) -> std::expected<void, std::string>
{
	return thread_pool_->push(std::make_shared<Job>(JobPriorities::LongTerm,
													[this]() -> std::expected<void, std::string>
													{
														while (running_.load())
														{
															auto backlog = task_dlq_->pending_count();
															if ((int)backlog >= configurations_->dlq_backlog_alert_count())
															{
																Logger::handle().write(LogTypes::Warning,
																					   std::format("dlq backlog alert: {} entries pending", backlog));
															}

															std::this_thread::sleep_for(std::chrono::seconds(5));
														}

														return {};
													},
													"dlq_backlog_monitor"));
}

auto TaskForgeWorker::handle_message(const std::string& message_body) -> std::expected<void, std::string>
{
	Logger::handle().write(LogTypes::Debug, std::format("received message: {}", message_body));

	boost::json::value parsed_value;
	try
	{
		parsed_value = boost::json::parse(message_body);
	}
	catch (const std::exception& exception)
	{
		// 파싱 불가능한 메시지는 재시도해도 성공할 수 없으므로 삭제합니다
		Logger::handle().write(LogTypes::Error, std::format("cannot parse message (dropped): {}", exception.what()));

		return {};
	}

	if (!parsed_value.is_object())
	{
		Logger::handle().write(LogTypes::Error, "message root is not a JSON object (dropped)");

		return {};
	}

	auto message = parsed_value.as_object();

	auto validated = MessageValidation::validate_request(message);
	if (!validated)
	{
		Logger::handle().write(LogTypes::Error, std::format("invalid message (dropped): {}", validated.error()));

		// 형식이 유효한 job_id에만 failed를 기록한다. 미검증 값을 Redis 키로 쓰면
		// 조회 불가능한 쓰레기 키가 남고 경로 문자가 하위 계층으로 전파된다
		if (message.contains("job_id") && message.at("job_id").is_string())
		{
			const auto& job_id = message.at("job_id").as_string();
			if (MessageValidation::is_valid_job_id(std::string_view(job_id.data(), job_id.size())))
			{
				boost::json::object updates = contract_fields(message);
				updates["failure_reason"] = validated.error();
				set_job_status(job_id.c_str(), "failed", updates);
			}
		}

		return {};
	}

	auto processed = process_job(message, message_body);
	if (!processed)
	{
		std::string job_id = message.at("job_id").as_string().c_str();
		Logger::handle().write(LogTypes::Error, std::format("job [{}] failed: {}", job_id, processed.error()));

		handle_processing_failure(job_id, message_body, processed.error());
	}

	// 재시도는 DLQ 재발행으로 자체 관리하므로 원본 메시지는 항상 삭제합니다
	return {};
}

auto TaskForgeWorker::process_job(const boost::json::object& message, const std::string& message_body) -> std::expected<void, std::string>
{
	std::string job_id = message.at("job_id").as_string().c_str();
	auto task = message.at("task").as_object();
	std::string task_name = task.at("name").as_string().c_str();

	auto chrono_begin = Logger::handle().chrono_start();

	// SQS는 at-least-once이므로 이미 종결된 작업의 메시지가 재배달될 수 있다.
	// 가드가 없으면 done → running → done으로 상태가 역행하고 태스크가 재실행된다 (NFR-REL-01)
	if (is_terminal_status(job_id))
	{
		Logger::handle().write(LogTypes::Information, std::format("job [{}] already terminal: skipping redelivery", job_id));

		return {};
	}

	auto definition = task_executor_->find_task(task_name);
	if (definition == std::nullopt)
	{
		// 화이트리스트 확인 전에 running을 기록하면 결정적 실패에도 상태가 오락가락한다
		return std::unexpected(std::format("task [{}] is not registered in the whitelist", task_name));
	}

	set_job_status(job_id, "running", contract_fields(message));

	TempDirGuard temp_dir(job_id);

	auto downloaded = download_inputs(message, temp_dir.inputs());
	if (!downloaded)
	{
		return std::unexpected(downloaded.error());
	}

	std::vector<std::string> arguments;
	if (task.contains("arguments"))
	{
		for (const auto& argument : task.at("arguments").as_array())
		{
			arguments.push_back(argument.as_string().c_str());
		}
	}

	int timeout_sec = 0;
	if (task.contains("timeout_sec"))
	{
		timeout_sec = (int)task.at("timeout_sec").as_int64();
	}

	auto executed = task_executor_->execute(definition.value(), arguments, temp_dir.inputs(), temp_dir.outputs(), temp_dir.stdout_log(), timeout_sec);
	if (!executed)
	{
		return std::unexpected(executed.error());
	}

	if (executed.value().exit_code != 0)
	{
		return std::unexpected(std::format("task [{}] exited with code {}: {}", task_name, executed.value().exit_code, executed.value().stdout_tail));
	}

	std::string output_prefix = message.at("output_prefix").as_string().c_str();

	auto uploaded = upload_outputs(output_prefix, temp_dir.outputs());
	if (!uploaded)
	{
		return std::unexpected(uploaded.error());
	}

	boost::json::object done_updates;
	done_updates["task_name"] = task_name;
	done_updates["exit_code"] = executed.value().exit_code;
	done_updates["stdout_tail"] = executed.value().stdout_tail;
	done_updates["result_download_url"] = uploaded.value();
	// 재시도 끝에 성공한 경우 이전 실패 흔적이 남아 클라이언트가 성공을 실패로 오독한다 (계약 C-1)
	done_updates["failure_reason"] = nullptr;
	set_job_status(job_id, "done", done_updates);

	// 성공으로 종결된 작업의 DLQ 항목을 정리하지 않으면 백로그 경보(FR-WRK-09)가 영구 오탐한다
	task_dlq_->remove(job_id);

	Logger::handle().write(LogTypes::Information, std::format("job [{}] done: task={}, outputs={}", job_id, task_name, uploaded.value().size()),
						   chrono_begin);

	return {};
}

auto TaskForgeWorker::download_inputs(const boost::json::object& message, const std::filesystem::path& inputs_dir) -> std::expected<void, std::string>
{
	for (const auto& entry : message.at("download_s3").as_array())
	{
		auto entry_object = entry.as_object();
		std::string bucket = entry_object.at("bucket").as_string().c_str();
		std::string object_key = entry_object.at("key").as_string().c_str();

		auto local_path = inputs_dir / key_filename(object_key);

		std::string last_error = "";
		bool succeeded = false;
		for (auto attempt = 1; attempt <= s3_download_attempts; attempt++)
		{
			auto downloaded = s3_client_->download_file(Aws::String(bucket.c_str()), Aws::String(object_key.c_str()), Aws::String(local_path.string().c_str()));
			if (downloaded)
			{
				succeeded = true;
				break;
			}

			last_error = downloaded.error();
			Logger::handle().write(LogTypes::Warning,
								   std::format("s3 download attempt {}/{} failed [{}]: {}", attempt, s3_download_attempts, object_key, last_error));
			std::this_thread::sleep_for(backoff_with_jitter(attempt, download_base_delay_ms, download_max_delay_ms));
		}

		if (!succeeded)
		{
			return std::unexpected(std::format("cannot download s3 object [{}]: {}", object_key, last_error));
		}
	}

	return {};
}

auto TaskForgeWorker::upload_outputs(const std::string& output_prefix, const std::filesystem::path& outputs_dir) -> std::expected<boost::json::array, std::string>
{
	boost::json::array results;

	std::error_code error_code;
	for (const auto& entry : std::filesystem::directory_iterator(outputs_dir, error_code))
	{
		if (!entry.is_regular_file())
		{
			continue;
		}

		auto filename = entry.path().filename().string();
		auto object_key = std::format("{}{}", output_prefix, filename);

		auto uploaded = s3_client_->upload_file(Aws::String(configurations_->s3_bucket().c_str()), Aws::String(object_key.c_str()),
												Aws::String(entry.path().string().c_str()));
		if (!uploaded)
		{
			return std::unexpected(std::format("cannot upload output [{}]: {}", filename, uploaded.error()));
		}

		auto presigned = s3_client_->generate_presigned_url(Aws::String(configurations_->s3_bucket().c_str()), Aws::String(object_key.c_str()),
															configurations_->presign_ttl_sec(), Aws::Http::HttpMethod::HTTP_GET);
		if (!presigned)
		{
			return std::unexpected(std::format("cannot presign output [{}]: {}", filename, presigned.error()));
		}

		boost::json::object result;
		result["filename"] = filename;
		result["download_url"] = presigned.value();
		result["method"] = "GET";
		results.push_back(result);
	}

	if (error_code)
	{
		return std::unexpected(std::format("cannot iterate outputs directory: {}", error_code.message()));
	}

	return results;
}

auto TaskForgeWorker::handle_processing_failure(const std::string& job_id, const std::string& message_body, const std::string& reason) -> void
{
	boost::json::object republish;
	try
	{
		republish = boost::json::parse(message_body).as_object();
	}
	catch (const std::exception&)
	{
		republish["job_id"] = job_id;
	}

	// 재시도 횟수의 SSOT는 메시지의 dlq_try_count입니다. 이 값을 판독하지 않으면
	// 로컬 DLQ 파일이 소실된 인스턴스에서 카운트가 리셋되고, 이미 사용한 dedup ID를
	// 재사용해 재시도 메시지가 조용히 폐기됩니다(작업이 비터미널 상태로 고착)
	int inbound_try_count = 0;
	if (republish.contains("dlq_try_count") && republish.at("dlq_try_count").is_int64())
	{
		inbound_try_count = (int)republish.at("dlq_try_count").as_int64();
	}

	auto recorded = task_dlq_->record_failure(job_id, message_body, reason, inbound_try_count);
	if (!recorded)
	{
		Logger::handle().write(LogTypes::Error, std::format("cannot record dlq entry [{}]: {}", job_id, recorded.error()));

		boost::json::object updates = contract_fields(republish);
		updates["failure_reason"] = reason;
		set_job_status(job_id, "failed", updates);

		return;
	}

	int try_count = recorded.value();
	if (!task_dlq_->can_retry(try_count))
	{
		Logger::handle().write(LogTypes::Warning, std::format("job [{}] gave up after {} attempts", job_id, try_count));

		task_dlq_->remove(job_id);

		boost::json::object updates = contract_fields(republish);
		updates["failure_reason"] = reason;
		updates["retry_count"] = try_count;
		set_job_status(job_id, "failed", updates);

		return;
	}

	republish["dlq_try_count"] = try_count;

	// SQS FIFO는 메시지 단위 DelaySeconds를 지원하지 않으므로 재발행 전에 대기한다.
	// 컨슈머가 직렬(ADR-04)이라 이 대기는 head-of-line 지연을 만들지만, 백오프 없이
	// 재발행하면 재시도 4회가 수 밀리초에 소진되어 재시도 자체가 무의미해진다
	auto backoff = backoff_with_jitter(try_count, retry_base_delay_ms, retry_max_delay_ms);
	Logger::handle().write(LogTypes::Information,
						   std::format("job [{}] retry backoff {}ms before requeue {}/{}", job_id, backoff.count(), try_count,
									   configurations_->dlq_max_retry_count()));
	std::this_thread::sleep_for(backoff);

	auto republished = sqs_publisher_->send_message(Aws::String(boost::json::serialize(republish).c_str()),
													Aws::String(configurations_->sqs_message_group_id().c_str()),
													Aws::String(std::format("{}-retry-{}", job_id, try_count).c_str()));
	if (!republished)
	{
		Logger::handle().write(LogTypes::Error, std::format("cannot republish job [{}] for retry: {}", job_id, republished.error()));

		// 여기서 failed로 확정되고 원본 메시지도 이미 삭제되므로, 남은 DLQ 항목은 어떤 경로로도
		// 제거되지 않는 고아가 되어 백로그 경보(FR-WRK-09)를 영구 오탐시킨다
		task_dlq_->remove(job_id);

		boost::json::object updates = contract_fields(republish);
		updates["failure_reason"] = std::format("{} (republish failed: {})", reason, republished.error());
		updates["retry_count"] = try_count;
		set_job_status(job_id, "failed", updates);

		return;
	}

	Logger::handle().write(LogTypes::Information, std::format("job [{}] requeued for retry {}/{}", job_id, try_count, configurations_->dlq_max_retry_count()));

	boost::json::object updates = contract_fields(republish);
	updates["failure_reason"] = reason;
	updates["retry_count"] = try_count;
	set_job_status(job_id, "queued", updates);
}

auto TaskForgeWorker::set_job_status(const std::string& job_id, const std::string& status, const boost::json::object& updates) -> void
{
	boost::json::object document;

	// 기존 상태 JSON을 읽어 필드를 병합합니다 (없으면 새로 생성)
	auto existing = redis_client_->get(job_id);
	if (existing)
	{
		try
		{
			auto parsed = boost::json::parse(existing.value());
			if (parsed.is_object())
			{
				document = parsed.as_object();
			}
		}
		catch (const std::exception&)
		{
			// 손상된 상태 값은 새로 생성합니다
		}
	}

	document["job_id"] = job_id;
	document["status"] = status;
	for (const auto& update : updates)
	{
		// null은 "필드 제거" 의미로 사용한다 (터미널 전이 시 이전 결과 필드 정리)
		if (update.value().is_null())
		{
			document.erase(update.key());

			continue;
		}

		document[update.key()] = update.value();
	}
	document["updated_at"] = current_utc_timestamp();

	auto saved = redis_client_->set(job_id, boost::json::serialize(document), configurations_->redis_ttl_sec());
	if (!saved)
	{
		Logger::handle().write(LogTypes::Error, std::format("cannot save job status [{}={}]: {}", job_id, status, saved.error()));
	}
}

auto TaskForgeWorker::is_terminal_status(const std::string& job_id) -> bool
{
	auto existing = redis_client_->get(job_id);
	if (!existing)
	{
		return false;
	}

	try
	{
		auto parsed = boost::json::parse(existing.value());
		if (!parsed.is_object())
		{
			return false;
		}

		auto document = parsed.as_object();
		if (!document.contains("status") || !document.at("status").is_string())
		{
			return false;
		}

		auto status = document.at("status").as_string();

		return status == "done" || status == "failed";
	}
	catch (const std::exception&)
	{
		return false;
	}
}
