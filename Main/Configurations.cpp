#include "Configurations.h"

#include "Converter.h"
#include "File.h"
#include "Logger.h"

#include <boost/json.hpp>

#include <filesystem>
#include <format>

using namespace Utilities;

Configurations::Configurations(const ArgumentParser& arguments)
	: root_path_("")
	, config_path_("")
	, worker_title_("TaskForgeWorker")
	, write_file_log_((int)LogTypes::Information)
	, write_console_log_((int)LogTypes::Information)
	, write_interval_(1000)
	, log_root_path_("")
	, high_priority_count_(1)
	, normal_priority_count_(2)
	, low_priority_count_(1)
	, redis_host_("127.0.0.1")
	, redis_port_(6379)
	, redis_db_index_(0)
	, redis_ttl_sec_(3600)
	, aws_region_("us-east-1")
	, aws_access_key_("")
	, aws_secret_key_("")
	, aws_endpoint_("")
	, sqs_request_queue_url_("")
	, sqs_message_group_id_("request")
	, sqs_wait_time_seconds_(5)
	, sqs_visibility_timeout_(60)
	, sqs_max_number_of_messages_(1)
	, s3_bucket_("taskforge-jobs")
	, presign_ttl_sec_(600)
	, dlq_path_("")
	, dlq_max_retry_count_(3)
	, dlq_backlog_alert_count_(10)
	, default_timeout_sec_(30)
{
	root_path_ = arguments.program_folder();

	auto config_path = arguments.to_string("--config_path");
	if (config_path != std::nullopt)
	{
		config_path_ = config_path.value();
	}
	else
	{
		config_path_ = (std::filesystem::path(root_path_) / "task_forge_worker_configurations.json").string();
	}

	load();
	parse(arguments);
	validate_configuration();
}

auto Configurations::root_path(void) const -> std::string { return root_path_; }

auto Configurations::worker_title(void) const -> std::string { return worker_title_; }

auto Configurations::write_file_log(void) const -> LogTypes { return (LogTypes)write_file_log_; }

auto Configurations::write_console_log(void) const -> LogTypes { return (LogTypes)write_console_log_; }

auto Configurations::write_interval(void) const -> uint16_t { return (uint16_t)write_interval_; }

auto Configurations::log_root_path(void) const -> std::string { return log_root_path_; }

auto Configurations::high_priority_count(void) const -> int { return high_priority_count_; }

auto Configurations::normal_priority_count(void) const -> int { return normal_priority_count_; }

auto Configurations::low_priority_count(void) const -> int { return low_priority_count_; }

auto Configurations::redis_host(void) const -> std::string { return redis_host_; }

auto Configurations::redis_port(void) const -> int { return redis_port_; }

auto Configurations::redis_db_index(void) const -> int { return redis_db_index_; }

auto Configurations::redis_ttl_sec(void) const -> long { return redis_ttl_sec_; }

auto Configurations::aws_region(void) const -> std::string { return aws_region_; }

auto Configurations::aws_access_key(void) const -> std::string { return aws_access_key_; }

auto Configurations::aws_secret_key(void) const -> std::string { return aws_secret_key_; }

auto Configurations::aws_endpoint(void) const -> std::string { return aws_endpoint_; }

auto Configurations::sqs_request_queue_url(void) const -> std::string { return sqs_request_queue_url_; }

auto Configurations::sqs_message_group_id(void) const -> std::string { return sqs_message_group_id_; }

auto Configurations::sqs_wait_time_seconds(void) const -> int { return sqs_wait_time_seconds_; }

auto Configurations::sqs_visibility_timeout(void) const -> int { return sqs_visibility_timeout_; }

auto Configurations::sqs_max_number_of_messages(void) const -> int { return sqs_max_number_of_messages_; }

auto Configurations::s3_bucket(void) const -> std::string { return s3_bucket_; }

auto Configurations::presign_ttl_sec(void) const -> int { return presign_ttl_sec_; }

auto Configurations::dlq_path(void) const -> std::string { return dlq_path_; }

auto Configurations::dlq_max_retry_count(void) const -> int { return dlq_max_retry_count_; }

auto Configurations::dlq_backlog_alert_count(void) const -> int { return dlq_backlog_alert_count_; }

auto Configurations::default_timeout_sec(void) const -> int { return default_timeout_sec_; }

auto Configurations::task_whitelist(void) const -> const std::vector<TaskDefinition>& { return task_whitelist_; }

auto Configurations::load(void) -> void
{
	File source;
	auto opened = source.open(config_path_, std::ios::in | std::ios::binary, std::locale(""));
	if (!opened)
	{
		Logger::handle().write(LogTypes::Warning, std::format("cannot open configuration file (defaults applied): {}", config_path_));
		return;
	}

	auto source_data = source.read_bytes();
	if (!source_data)
	{
		Logger::handle().write(LogTypes::Warning, std::format("cannot read configuration file (defaults applied): {}", source_data.error()));
		return;
	}

	boost::json::value parsed_value;
	try
	{
		parsed_value = boost::json::parse(Converter::to_string(source_data.value()));
	}
	catch (const std::exception& exception)
	{
		Logger::handle().write(LogTypes::Error, std::format("cannot parse configuration file (defaults applied): {}", exception.what()));
		return;
	}

	if (!parsed_value.is_object())
	{
		Logger::handle().write(LogTypes::Error, "configuration root is not a JSON object (defaults applied)");
		return;
	}

	auto message = parsed_value.as_object();

	// JSON 키와 멤버 이름을 1:1로 유지하기 위한 헬퍼 (키 불일치 실수 방지)
	auto read_string = [&message](const char* key, std::string& target) -> void
	{
		if (message.contains(key) && message.at(key).is_string())
		{
			target = message.at(key).as_string().c_str();
		}
	};
	auto read_int = [&message](const char* key, int& target) -> void
	{
		if (message.contains(key) && message.at(key).is_int64())
		{
			target = (int)message.at(key).as_int64();
		}
	};
	auto read_long = [&message](const char* key, long& target) -> void
	{
		if (message.contains(key) && message.at(key).is_int64())
		{
			target = (long)message.at(key).as_int64();
		}
	};

	read_string("worker_title", worker_title_);
	read_int("write_file_log", write_file_log_);
	read_int("write_console_log", write_console_log_);
	read_int("write_interval", write_interval_);
	read_string("log_root_path", log_root_path_);

	read_int("high_priority_count", high_priority_count_);
	read_int("normal_priority_count", normal_priority_count_);
	read_int("low_priority_count", low_priority_count_);

	read_string("redis_host", redis_host_);
	read_int("redis_port", redis_port_);
	read_int("redis_db_index", redis_db_index_);
	read_long("redis_ttl_sec", redis_ttl_sec_);

	read_string("aws_region", aws_region_);
	read_string("aws_access_key", aws_access_key_);
	read_string("aws_secret_key", aws_secret_key_);
	read_string("aws_endpoint", aws_endpoint_);

	read_string("sqs_request_queue_url", sqs_request_queue_url_);
	read_string("sqs_message_group_id", sqs_message_group_id_);
	read_int("sqs_wait_time_seconds", sqs_wait_time_seconds_);
	read_int("sqs_visibility_timeout", sqs_visibility_timeout_);
	read_int("sqs_max_number_of_messages", sqs_max_number_of_messages_);

	read_string("s3_bucket", s3_bucket_);
	read_int("presign_ttl_sec", presign_ttl_sec_);

	read_string("dlq_path", dlq_path_);
	read_int("dlq_max_retry_count", dlq_max_retry_count_);
	read_int("dlq_backlog_alert_count", dlq_backlog_alert_count_);

	read_int("default_timeout_sec", default_timeout_sec_);

	if (message.contains("task_whitelist") && message.at("task_whitelist").is_array())
	{
		task_whitelist_.clear();
		for (const auto& entry : message.at("task_whitelist").as_array())
		{
			if (!entry.is_object())
			{
				continue;
			}

			auto entry_object = entry.as_object();
			if (!entry_object.contains("name") || !entry_object.at("name").is_string() || !entry_object.contains("executable")
				|| !entry_object.at("executable").is_string())
			{
				Logger::handle().write(LogTypes::Warning, "task_whitelist entry without name/executable is ignored");
				continue;
			}

			TaskDefinition definition;
			definition.name = entry_object.at("name").as_string().c_str();
			definition.executable = entry_object.at("executable").as_string().c_str();
			if (entry_object.contains("description") && entry_object.at("description").is_string())
			{
				definition.description = entry_object.at("description").as_string().c_str();
			}

			task_whitelist_.push_back(definition);
		}
	}
}

auto Configurations::parse(const ArgumentParser& arguments) -> void
{
	auto int_target = arguments.to_int("--write_console_log");
	if (int_target != std::nullopt)
	{
		write_console_log_ = int_target.value();
	}

	int_target = arguments.to_int("--write_file_log");
	if (int_target != std::nullopt)
	{
		write_file_log_ = int_target.value();
	}

	auto string_target = arguments.to_string("--worker_title");
	if (string_target != std::nullopt)
	{
		worker_title_ = string_target.value();
	}

	string_target = arguments.to_string("--redis_host");
	if (string_target != std::nullopt)
	{
		redis_host_ = string_target.value();
	}

	string_target = arguments.to_string("--aws_endpoint");
	if (string_target != std::nullopt)
	{
		aws_endpoint_ = string_target.value();
	}

	string_target = arguments.to_string("--sqs_request_queue_url");
	if (string_target != std::nullopt)
	{
		sqs_request_queue_url_ = string_target.value();
	}
}

auto Configurations::validate_configuration(void) -> void
{
	if (log_root_path_.empty())
	{
		log_root_path_ = root_path_;
	}

	if (dlq_path_.empty())
	{
		dlq_path_ = (std::filesystem::path(root_path_) / "dlq").string();
	}

	if (redis_port_ <= 0 || redis_port_ > 65535)
	{
		redis_port_ = 6379;
	}

	if (redis_ttl_sec_ < 0)
	{
		redis_ttl_sec_ = 3600;
	}

	if (sqs_wait_time_seconds_ < 0 || sqs_wait_time_seconds_ > 20)
	{
		sqs_wait_time_seconds_ = 5;
	}

	if (sqs_visibility_timeout_ <= 0)
	{
		sqs_visibility_timeout_ = 60;
	}

	if (sqs_max_number_of_messages_ <= 0 || sqs_max_number_of_messages_ > 10)
	{
		sqs_max_number_of_messages_ = 1;
	}

	if (presign_ttl_sec_ <= 0)
	{
		presign_ttl_sec_ = 600;
	}

	if (dlq_max_retry_count_ < 0)
	{
		dlq_max_retry_count_ = 3;
	}

	if (default_timeout_sec_ <= 0)
	{
		default_timeout_sec_ = 30;
	}

	if (high_priority_count_ < 0)
	{
		high_priority_count_ = 1;
	}

	if (normal_priority_count_ < 0)
	{
		normal_priority_count_ = 2;
	}

	if (low_priority_count_ < 0)
	{
		low_priority_count_ = 1;
	}
}
