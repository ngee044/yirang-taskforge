#pragma once

#include "Configurations.h"
#include "TaskDLQ.h"
#include "TaskExecutor.h"

#include "AWSS3Client.h"
#include "AWSSQSConsumer.h"
#include "AWSSQSPublisher.h"
#include "RedisClient.h"
#include "ThreadPool.h"

#include <boost/json.hpp>

#include <atomic>
#include <condition_variable>
#include <expected>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

class TaskForgeWorker
{
public:
	TaskForgeWorker(std::shared_ptr<Configurations> configurations);
	~TaskForgeWorker(void);

	auto start(void) -> std::expected<void, std::string>;
	auto stop(void) -> void;

protected:
	auto dlq_backlog_monitor(void) -> std::expected<void, std::string>;

private:
	auto create_thread_pool(void) -> std::expected<void, std::string>;
	auto connect_redis(void) -> std::expected<void, std::string>;
	auto upload_task_catalog(void) -> std::expected<void, std::string>;
	auto connect_aws_service(void) -> std::expected<void, std::string>;
	auto make_client_config(void) const -> Aws::Client::ClientConfiguration;
	auto start_dlq_backlog_monitor(void) -> std::expected<void, std::string>;

	auto handle_message(const std::string& message_body) -> std::expected<void, std::string>;
	auto process_job(const boost::json::object& message, const std::string& message_body) -> std::expected<void, std::string>;
	auto download_inputs(const boost::json::object& message, const std::filesystem::path& inputs_dir) -> std::expected<void, std::string>;
	auto upload_outputs(const std::string& output_prefix, const std::filesystem::path& outputs_dir) -> std::expected<boost::json::array, std::string>;
	auto handle_processing_failure(const std::string& job_id, const std::string& message_body, const std::string& reason) -> std::expected<void, std::string>;
	auto fail_without_retry(const std::string& job_id, const boost::json::object& message, const std::string& reason) -> std::expected<void, std::string>;
	auto read_job_status(const std::string& job_id) -> std::expected<std::optional<std::string>, std::string>;
	auto set_job_status(const std::string& job_id, const std::string& status, const boost::json::object& updates) -> std::expected<void, std::string>;
	auto is_terminal_status(const std::string& job_id) -> std::expected<bool, std::string>;

	std::shared_ptr<Configurations> configurations_;
	std::shared_ptr<Thread::ThreadPool> thread_pool_;
	std::shared_ptr<Redis::RedisClient> redis_client_;
	std::unique_ptr<AWSService::AWSS3Client> s3_client_;
	std::unique_ptr<AWSService::AWSSQSConsumer> sqs_consumer_;
	std::unique_ptr<AWSService::AWSSQSPublisher> sqs_publisher_;
	std::unique_ptr<TaskExecutor> task_executor_;
	std::unique_ptr<TaskDLQ> task_dlq_;

	std::atomic<bool> running_;
	std::mutex stop_mutex_;
	std::condition_variable stop_condition_;
};
