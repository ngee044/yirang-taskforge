#pragma once

#include "ArgumentParser.h"
#include "LogTypes.h"

#include <string>
#include <vector>

struct TaskDefinition
{
	std::string name;
	std::string executable;
	std::string description;
};

class Configurations
{
public:
	Configurations(const Utilities::ArgumentParser& arguments);

	auto root_path(void) const -> std::string;
	auto worker_title(void) const -> std::string;

	auto write_file_log(void) const -> Utilities::LogTypes;
	auto write_console_log(void) const -> Utilities::LogTypes;
	auto write_interval(void) const -> uint16_t;
	auto log_root_path(void) const -> std::string;

	auto high_priority_count(void) const -> int;
	auto normal_priority_count(void) const -> int;
	auto low_priority_count(void) const -> int;

	auto redis_host(void) const -> std::string;
	auto redis_port(void) const -> int;
	auto redis_db_index(void) const -> int;
	auto redis_ttl_sec(void) const -> long;

	auto aws_region(void) const -> std::string;
	auto aws_access_key(void) const -> std::string;
	auto aws_secret_key(void) const -> std::string;
	auto aws_endpoint(void) const -> std::string;

	auto sqs_request_queue_url(void) const -> std::string;
	auto sqs_message_group_id(void) const -> std::string;
	auto sqs_wait_time_seconds(void) const -> int;
	auto sqs_visibility_timeout(void) const -> int;
	auto sqs_max_number_of_messages(void) const -> int;

	auto s3_bucket(void) const -> std::string;
	auto presign_ttl_sec(void) const -> int;

	auto dlq_path(void) const -> std::string;
	auto dlq_max_retry_count(void) const -> int;
	auto dlq_backlog_alert_count(void) const -> int;

	auto default_timeout_sec(void) const -> int;
	auto task_whitelist(void) const -> const std::vector<TaskDefinition>&;

private:
	auto load(void) -> void;
	auto parse(const Utilities::ArgumentParser& arguments) -> void;
	auto validate_configuration(void) -> void;

	std::string root_path_;
	std::string config_path_;
	std::string worker_title_;

	int write_file_log_;
	int write_console_log_;
	int write_interval_;
	std::string log_root_path_;

	int high_priority_count_;
	int normal_priority_count_;
	int low_priority_count_;

	std::string redis_host_;
	int redis_port_;
	int redis_db_index_;
	long redis_ttl_sec_;

	std::string aws_region_;
	std::string aws_access_key_;
	std::string aws_secret_key_;
	std::string aws_endpoint_;

	std::string sqs_request_queue_url_;
	std::string sqs_message_group_id_;
	int sqs_wait_time_seconds_;
	int sqs_visibility_timeout_;
	int sqs_max_number_of_messages_;

	std::string s3_bucket_;
	int presign_ttl_sec_;

	std::string dlq_path_;
	int dlq_max_retry_count_;
	int dlq_backlog_alert_count_;

	int default_timeout_sec_;
	std::vector<TaskDefinition> task_whitelist_;
};
