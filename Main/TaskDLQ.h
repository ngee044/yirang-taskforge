#pragma once

#include <expected>
#include <filesystem>
#include <string>

class TaskDLQ
{
public:
	TaskDLQ(const std::string& dlq_path, int max_retry_count);

	// 실패를 기록하고 누적 try_count를 반환합니다.
	// inbound_try_count는 수신 메시지의 dlq_try_count로, 재시도 횟수의 SSOT입니다.
	// 로컬 파일은 캐시이므로 컨테이너 재생성·다중 인스턴스로 소실되어도 상한이 무력화되지 않도록
	// 둘 중 큰 값을 기준으로 증가시킵니다 (FR-WRK-07).
	auto record_failure(const std::string& job_id, const std::string& request_message, const std::string& failure_reason, int inbound_try_count)
		-> std::expected<int, std::string>;
	auto remove(const std::string& job_id) -> void;
	auto can_retry(int try_count) const -> bool;
	auto pending_count(void) const -> size_t;

private:
	auto entry_path(const std::string& job_id) const -> std::filesystem::path;

	std::filesystem::path dlq_path_;
	int max_retry_count_;
};
