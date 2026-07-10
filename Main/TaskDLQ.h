#pragma once

#include <expected>
#include <filesystem>
#include <string>

class TaskDLQ
{
public:
	TaskDLQ(const std::string& dlq_path, int max_retry_count);

	// 실패를 기록하고 누적 try_count를 반환합니다. 기존 항목이 있으면 try_count를 증가시킵니다.
	auto record_failure(const std::string& job_id, const std::string& request_message, const std::string& failure_reason)
		-> std::expected<int, std::string>;
	auto remove(const std::string& job_id) -> void;
	auto can_retry(int try_count) const -> bool;
	auto pending_count(void) const -> size_t;

private:
	auto entry_path(const std::string& job_id) const -> std::filesystem::path;

	std::filesystem::path dlq_path_;
	int max_retry_count_;
};
