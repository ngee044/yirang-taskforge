#pragma once

#include "Configurations.h"

#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

struct TaskExecutionResult
{
	int exit_code;
	bool timed_out;
	std::string stdout_tail;
};

class TaskExecutor
{
public:
	TaskExecutor(const std::vector<TaskDefinition>& whitelist, const std::string& root_path, int default_timeout_sec);

	auto find_task(const std::string& name) const -> std::optional<TaskDefinition>;

	// 화이트리스트 실행파일을 `executable <inputs_dir> <outputs_dir> [arguments...]` 형태로 실행합니다.
	// stdout/stderr는 stdout_path 파일로 리다이렉트하고, 종료 후 tail을 결과에 담습니다.
	auto execute(const TaskDefinition& task,
				 const std::vector<std::string>& arguments,
				 const std::filesystem::path& inputs_dir,
				 const std::filesystem::path& outputs_dir,
				 const std::filesystem::path& stdout_path,
				 int timeout_sec) -> std::expected<TaskExecutionResult, std::string>;

private:
	auto resolve_executable(const std::string& executable) const -> std::filesystem::path;
	auto read_stdout_tail(const std::filesystem::path& stdout_path) const -> std::string;

	std::vector<TaskDefinition> whitelist_;
	std::string root_path_;
	int default_timeout_sec_;
};
