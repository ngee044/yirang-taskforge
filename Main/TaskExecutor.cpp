#include "TaskExecutor.h"

#include "Logger.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/process/v2/process.hpp>
#include <boost/process/v2/stdio.hpp>

#include <chrono>
#include <cstdio>
#include <format>
#include <fstream>

using namespace Utilities;

namespace
{
	constexpr size_t stdout_tail_bytes = 2048;
}

TaskExecutor::TaskExecutor(const std::vector<TaskDefinition>& whitelist, const std::string& root_path, int default_timeout_sec)
	: whitelist_(whitelist), root_path_(root_path), default_timeout_sec_(default_timeout_sec)
{
}

auto TaskExecutor::find_task(const std::string& name) const -> std::optional<TaskDefinition>
{
	for (const auto& definition : whitelist_)
	{
		if (definition.name == name)
		{
			return definition;
		}
	}

	return std::nullopt;
}

auto TaskExecutor::execute(const TaskDefinition& task,
						   const std::vector<std::string>& arguments,
						   const std::filesystem::path& inputs_dir,
						   const std::filesystem::path& outputs_dir,
						   const std::filesystem::path& stdout_path,
						   int timeout_sec) -> std::expected<TaskExecutionResult, std::string>
{
	auto executable_path = resolve_executable(task.executable);

	std::error_code error_code;
	if (!std::filesystem::exists(executable_path, error_code) || !std::filesystem::is_regular_file(executable_path, error_code))
	{
		return std::unexpected(std::format("task executable not found: {}", executable_path.string()));
	}

	int effective_timeout = (timeout_sec > 0) ? timeout_sec : default_timeout_sec_;
	if (effective_timeout > default_timeout_sec_)
	{
		// 요청이 지정한 타임아웃은 워커 설정값을 상한으로 제한합니다
		effective_timeout = default_timeout_sec_;
	}

	std::FILE* stdout_file = std::fopen(stdout_path.string().c_str(), "w");
	if (stdout_file == nullptr)
	{
		return std::unexpected(std::format("cannot open stdout capture file: {}", stdout_path.string()));
	}

	std::vector<std::string> process_arguments;
	process_arguments.push_back(inputs_dir.string());
	process_arguments.push_back(outputs_dir.string());
	for (const auto& argument : arguments)
	{
		process_arguments.push_back(argument);
	}

	TaskExecutionResult result{ -1, false, "" };

	try
	{
		boost::asio::io_context io_context;
		boost::process::v2::process process(io_context, executable_path.string(), process_arguments,
											boost::process::v2::process_stdio{ nullptr, stdout_file, stdout_file });

		boost::asio::steady_timer timeout_timer(io_context);
		timeout_timer.expires_after(std::chrono::seconds(effective_timeout));
		timeout_timer.async_wait(
			[&](const boost::system::error_code& timer_error)
			{
				if (timer_error)
				{
					return;
				}

				result.timed_out = true;

				boost::system::error_code ignored;
				process.terminate(ignored);
			});

		process.async_wait(
			[&](const boost::system::error_code& wait_error, int)
			{
				timeout_timer.cancel();

				if (wait_error)
				{
					Logger::handle().write(LogTypes::Warning, std::format("process wait error [{}]: {}", task.name, wait_error.message()));
				}
			});

		io_context.run();

		result.exit_code = process.exit_code();
	}
	catch (const std::exception& exception)
	{
		std::fclose(stdout_file);

		return std::unexpected(std::format("cannot execute task [{}]: {}", task.name, exception.what()));
	}

	std::fclose(stdout_file);

	result.stdout_tail = read_stdout_tail(stdout_path);

	if (result.timed_out)
	{
		return std::unexpected(std::format("task [{}] timed out after {} seconds", task.name, effective_timeout));
	}

	return result;
}

auto TaskExecutor::resolve_executable(const std::string& executable) const -> std::filesystem::path
{
	std::filesystem::path path(executable);
	if (path.is_absolute())
	{
		return path;
	}

	return std::filesystem::path(root_path_) / path;
}

auto TaskExecutor::read_stdout_tail(const std::filesystem::path& stdout_path) const -> std::string
{
	std::ifstream input(stdout_path, std::ios::in | std::ios::binary);
	if (!input.is_open())
	{
		return "";
	}

	input.seekg(0, std::ios::end);
	auto file_size = (size_t)input.tellg();

	size_t read_size = (file_size > stdout_tail_bytes) ? stdout_tail_bytes : file_size;
	input.seekg((std::streamoff)(file_size - read_size), std::ios::beg);

	std::string tail(read_size, '\0');
	input.read(tail.data(), (std::streamsize)read_size);

	// 바이트 단위 절단은 멀티바이트 문자를 쪼개 invalid UTF-8을 만든다. 그 값이 상태 문서에
	// 실리면 JSON을 UTF-8로 디코딩하는 클라이언트(Python 인터페이스)가 조회 시 실패한다.
	// 절단이 발생한 경우에만 선두의 continuation byte(10xxxxxx)를 버려 경계를 맞춘다
	if (read_size < file_size)
	{
		size_t offset = 0;
		while (offset < tail.size() && ((unsigned char)tail[offset] & 0xC0) == 0x80)
		{
			offset += 1;
		}

		if (offset > 0)
		{
			tail.erase(0, offset);
		}
	}

	return tail;
}
