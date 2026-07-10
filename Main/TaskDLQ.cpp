#include "TaskDLQ.h"

#include "Logger.h"

#include <boost/json.hpp>

#include <format>
#include <fstream>
#include <sstream>

using namespace Utilities;

TaskDLQ::TaskDLQ(const std::string& dlq_path, int max_retry_count) : dlq_path_(dlq_path), max_retry_count_(max_retry_count)
{
	std::error_code error_code;
	std::filesystem::create_directories(dlq_path_, error_code);
	if (error_code)
	{
		Logger::handle().write(LogTypes::Warning, std::format("cannot create dlq directory [{}]: {}", dlq_path_.string(), error_code.message()));
	}
}

auto TaskDLQ::record_failure(const std::string& job_id, const std::string& request_message, const std::string& failure_reason)
	-> std::expected<int, std::string>
{
	auto path = entry_path(job_id);

	int try_count = 0;
	if (std::filesystem::exists(path))
	{
		std::ifstream existing(path);
		if (existing.is_open())
		{
			std::stringstream buffer;
			buffer << existing.rdbuf();

			try
			{
				auto parsed = boost::json::parse(buffer.str());
				if (parsed.is_object() && parsed.as_object().contains("try_count") && parsed.as_object().at("try_count").is_int64())
				{
					try_count = (int)parsed.as_object().at("try_count").as_int64();
				}
			}
			catch (const std::exception&)
			{
				// 손상된 DLQ 항목은 0회로 간주하고 덮어씁니다
			}
		}
	}

	try_count += 1;

	boost::json::object entry;
	entry["job_id"] = job_id;
	entry["try_count"] = try_count;
	entry["failure_reason"] = failure_reason;
	entry["request_message"] = request_message;

	std::ofstream output(path, std::ios::out | std::ios::trunc);
	if (!output.is_open())
	{
		return std::unexpected(std::format("cannot write dlq entry: {}", path.string()));
	}

	output << boost::json::serialize(entry);

	return try_count;
}

auto TaskDLQ::remove(const std::string& job_id) -> void
{
	std::error_code error_code;
	std::filesystem::remove(entry_path(job_id), error_code);
}

auto TaskDLQ::can_retry(int try_count) const -> bool { return try_count <= max_retry_count_; }

auto TaskDLQ::pending_count(void) const -> size_t
{
	std::error_code error_code;
	if (!std::filesystem::exists(dlq_path_, error_code))
	{
		return 0;
	}

	size_t count = 0;
	for (const auto& entry : std::filesystem::directory_iterator(dlq_path_, error_code))
	{
		if (entry.is_regular_file() && entry.path().extension() == ".json")
		{
			count += 1;
		}
	}

	return count;
}

auto TaskDLQ::entry_path(const std::string& job_id) const -> std::filesystem::path { return dlq_path_ / std::format("{}.json", job_id); }
