#include "MessageValidation.h"
#include "TaskDLQ.h"
#include "TaskExecutor.h"

#include <chrono>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

#include <pthread.h>

namespace
{
	auto require(bool condition, const std::string& message) -> void
	{
		if (!condition)
		{
			throw std::runtime_error(message);
		}
	}

	class TemporaryDirectory
	{
	public:
		TemporaryDirectory(void)
		{
			auto pattern = (std::filesystem::temp_directory_path() / "taskforge-test-XXXXXX").string();
			auto directory = ::mkdtemp(pattern.data());
			require(directory != nullptr, "cannot create test directory");
			path_ = directory;
		}

		~TemporaryDirectory(void)
		{
			std::error_code error;
			std::filesystem::remove_all(path_, error);
		}

		auto path(void) const -> const std::filesystem::path& { return path_; }

	private:
		std::filesystem::path path_;
	};

	auto test_validation(void) -> void
	{
		auto message
			= boost::json::parse(
				  R"({"job_id":"test-job","mode":"execute","task":{"name":"wordcount","arguments":[],"timeout_sec":30},"download_s3":[],"output_prefix":"jobs/test-job/outputs/"})")
				  .as_object();
		require(MessageValidation::validate_request(message).has_value(), "valid request rejected");
		for (const auto& job_id : { std::string("../escape"), std::string("a/b"), std::string(""), std::string(65, 'a') })
		{
			auto invalid = message;
			invalid["job_id"] = job_id;
			require(!MessageValidation::validate_request(invalid), "unsafe job_id accepted");
		}
		auto invalid = message;
		invalid["task"].as_object()["arguments"] = boost::json::array{ 1 };
		require(!MessageValidation::validate_request(invalid), "non-string argument accepted");
		invalid = message;
		invalid["task"].as_object()["timeout_sec"] = nullptr;
		require(!MessageValidation::validate_request(invalid), "null queue timeout accepted");
		invalid = message;
		invalid["download_s3"] = boost::json::array{ boost::json::object{ { "key", "file" }, { "method", "GET" } } };
		require(!MessageValidation::validate_request(invalid), "download without bucket accepted");
	}

	auto test_dlq(void) -> void
	{
		TemporaryDirectory directory;
		TaskDLQ dlq(directory.path().string(), 3);
		auto first = dlq.record_failure("job", "{}", "failure", 0);
		require(first && first.value() == 1, "first attempt is not 1");
		auto inherited = dlq.record_failure("job", "{}", "failure", 3);
		require(inherited && inherited.value() == 4 && !dlq.can_retry(inherited.value()), "inbound retry count lost");
		auto cached = dlq.record_failure("job", "{}", "failure", 0);
		require(cached && cached.value() == 5, "local retry count lost");
		dlq.remove("job");
		require(dlq.pending_count() == 0, "terminal entry was not removed");
		std::ofstream(directory.path() / "job.json") << "invalid json";
		auto repaired = dlq.record_failure("job", "{}", "failure", 2);
		require(repaired && repaired.value() == 3, "corrupt cache lost inbound count");
		std::ofstream(directory.path() / "not-a-directory") << "file";
		TaskDLQ unavailable((directory.path() / "not-a-directory").string(), 3);
		require(!unavailable.record_failure("job", "{}", "failure", 0), "unwritable DLQ reported success");
	}

	auto test_executor(void) -> void
	{
		TemporaryDirectory directory;
		auto inputs = directory.path() / "inputs";
		auto outputs = directory.path() / "outputs";
		std::filesystem::create_directories(inputs);
		std::filesystem::create_directories(outputs);
		auto script = directory.path() / "task.sh";
		std::ofstream(script) << "#!/bin/sh\n"
								 "case \"$3\" in\n"
								 "ok) cat \"$1/payload.txt\" ;;\n"
								 "fail) exit 7 ;;\n"
								 "signal) kill -TERM $$; echo signal-was-blocked ;;\n"
								 "timeout) (sleep 2; echo survived > \"$2/descendant.txt\") & wait ;;\n"
								 "esac\n";
		std::filesystem::permissions(script, std::filesystem::perms::owner_all);
		TaskDefinition definition{ "test", script.string(), "test fixture" };
		TaskExecutor executor({ definition }, directory.path().string(), 1);
		require(executor.find_task("test").has_value() && !executor.find_task("unknown"), "whitelist lookup failed");
		std::string payload;
		for (int index = 0; index < 700; ++index)
		{
			payload += "界";
		}
		std::ofstream(inputs / "payload.txt") << payload;
		auto normal = executor.execute(definition, { "ok" }, inputs, outputs, directory.path() / "stdout", 0);
		require(normal && normal.value().exit_code == 0, "normal task failed");
		require(normal.value().stdout_tail == payload.substr(54), "UTF-8 tail boundary is invalid");
		auto failure = executor.execute(definition, { "fail" }, inputs, outputs, directory.path() / "stdout", 0);
		require(failure && failure.value().exit_code == 7, "nonzero task exit was lost");

		sigset_t signals;
		sigset_t previous;
		sigemptyset(&signals);
		sigaddset(&signals, SIGTERM);
		require(pthread_sigmask(SIG_BLOCK, &signals, &previous) == 0, "cannot block test signal");
		auto signaled = executor.execute(definition, { "signal" }, inputs, outputs, directory.path() / "stdout", 0);
		require(pthread_sigmask(SIG_SETMASK, &previous, nullptr) == 0, "cannot restore test signal mask");
		require(signaled && signaled.value().exit_code != 0 && signaled.value().stdout_tail.empty(), "worker signal mask leaked into task");

		auto started = std::chrono::steady_clock::now();
		auto timed = executor.execute(definition, { "timeout" }, inputs, outputs, directory.path() / "stdout", 30);
		require(!timed && timed.error().find("timed out after 1 seconds") != std::string::npos, "timeout was not capped");
		require(std::chrono::steady_clock::now() - started < std::chrono::seconds(3), "timeout did not bound task execution");
		std::this_thread::sleep_for(std::chrono::seconds(2));
		require(!std::filesystem::exists(outputs / "descendant.txt"), "descendant survived timeout");
	}
}

auto main(int argc, char* argv[]) -> int
{
	try
	{
		require(argc == 2, "expected test group");
		std::string group = argv[1];
		if (group == "validation")
		{
			test_validation();
		}
		else if (group == "dlq")
		{
			test_dlq();
		}
		else if (group == "executor")
		{
			test_executor();
		}
		else
		{
			throw std::runtime_error("unknown test group");
		}
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
