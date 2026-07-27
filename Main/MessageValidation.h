#pragma once

#include <boost/json.hpp>

#include <expected>
#include <string>
#include <string_view>

class MessageValidation
{
public:
	static auto validate_request(const boost::json::object& message) -> std::expected<void, std::string>;

	// job_id는 임시 디렉토리·DLQ 파일 경로에 그대로 결합되므로, 경로 구분자와 '.'을
	// 원천 배제해 경로 탈출을 표현 불가능하게 만든다 (NFR-SEC-01)
	static auto is_valid_job_id(std::string_view job_id) -> bool;
};
