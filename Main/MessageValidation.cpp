#include "MessageValidation.h"

#include <algorithm>
#include <format>

namespace
{
	constexpr size_t job_id_max_length = 64;
}

auto MessageValidation::is_valid_job_id(std::string_view job_id) -> bool
{
	if (job_id.empty() || job_id.size() > job_id_max_length)
	{
		return false;
	}

	return std::all_of(job_id.begin(), job_id.end(), [](char character) {
		return (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') || (character >= '0' && character <= '9')
			   || character == '-' || character == '_';
	});
}

auto MessageValidation::validate_request(const boost::json::object& message) -> std::expected<void, std::string>
{
	if (!message.contains("job_id") || !message.at("job_id").is_string() || message.at("job_id").as_string().empty())
	{
		return std::unexpected("job_id is required and must be a non-empty string");
	}

	const auto& job_id = message.at("job_id").as_string();
	if (!is_valid_job_id(std::string_view(job_id.data(), job_id.size())))
	{
		return std::unexpected(std::format("job_id must match [A-Za-z0-9_-]{{1,{}}}", job_id_max_length));
	}

	if (!message.contains("mode") || !message.at("mode").is_string())
	{
		return std::unexpected("mode is required and must be a string");
	}

	if (message.at("mode").as_string() != "execute")
	{
		return std::unexpected(std::format("unsupported mode: {}", message.at("mode").as_string().c_str()));
	}

	if (!message.contains("task") || !message.at("task").is_object())
	{
		return std::unexpected("task is required and must be an object");
	}

	auto task = message.at("task").as_object();
	if (!task.contains("name") || !task.at("name").is_string() || task.at("name").as_string().empty())
	{
		return std::unexpected("task.name is required and must be a non-empty string");
	}

	if (task.contains("arguments"))
	{
		if (!task.at("arguments").is_array())
		{
			return std::unexpected("task.arguments must be an array of strings");
		}

		for (const auto& argument : task.at("arguments").as_array())
		{
			if (!argument.is_string())
			{
				return std::unexpected("task.arguments must contain only strings");
			}
		}
	}

	if (task.contains("timeout_sec") && !task.at("timeout_sec").is_int64())
	{
		return std::unexpected("task.timeout_sec must be an integer");
	}

	if (!message.contains("download_s3") || !message.at("download_s3").is_array())
	{
		return std::unexpected("download_s3 is required and must be an array");
	}

	for (const auto& entry : message.at("download_s3").as_array())
	{
		if (!entry.is_object())
		{
			return std::unexpected("download_s3 entries must be objects");
		}

		auto entry_object = entry.as_object();
		if (!entry_object.contains("bucket") || !entry_object.at("bucket").is_string() || entry_object.at("bucket").as_string().empty())
		{
			return std::unexpected("download_s3.bucket is required and must be a non-empty string");
		}

		if (!entry_object.contains("key") || !entry_object.at("key").is_string() || entry_object.at("key").as_string().empty())
		{
			return std::unexpected("download_s3.key is required and must be a non-empty string");
		}

		if (!entry_object.contains("method") || !entry_object.at("method").is_string())
		{
			return std::unexpected("download_s3.method is required and must be a string");
		}
	}

	if (!message.contains("output_prefix") || !message.at("output_prefix").is_string() || message.at("output_prefix").as_string().empty())
	{
		return std::unexpected("output_prefix is required and must be a non-empty string");
	}

	return {};
}
