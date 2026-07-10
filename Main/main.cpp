#include "Configurations.h"
#include "TaskForgeWorker.h"

#include "ArgumentParser.h"
#include "Logger.h"
#include "LogTypes.h"

#include <aws/core/Aws.h>

#include <csignal>
#include <format>
#include <memory>

using namespace Utilities;

std::shared_ptr<Configurations> configurations_ = nullptr;
std::shared_ptr<TaskForgeWorker> worker_ = nullptr;

auto register_signal(void) -> void;
auto deregister_signal(void) -> void;
auto signal_callback(int32_t signum) -> void;

auto main(int32_t argc, char* argv[]) -> int32_t
{
	configurations_ = std::make_shared<Configurations>(ArgumentParser(argc, argv));

	Logger::handle().file_mode(configurations_->write_file_log());
	Logger::handle().console_mode(configurations_->write_console_log());
	Logger::handle().write_interval(configurations_->write_interval());
	Logger::handle().log_root(configurations_->log_root_path());
	Logger::handle().start(configurations_->worker_title());

	register_signal();

	// AWS SDK 수명주기는 애플리케이션이 소유합니다 (AWSService 라이브러리가 대신 하지 않음)
	Aws::SDKOptions sdk_options;
	Aws::InitAPI(sdk_options);

	worker_ = std::make_shared<TaskForgeWorker>(configurations_);

	auto started = worker_->start();
	if (!started)
	{
		Logger::handle().write(LogTypes::Error, std::format("cannot start TaskForgeWorker: {}", started.error()));
		worker_->stop();
	}
	else
	{
		worker_->wait_stop();
	}

	worker_.reset();
	configurations_.reset();

	Aws::ShutdownAPI(sdk_options);

	deregister_signal();

	Logger::handle().stop();
	Logger::destroy();

	return 0;
}

auto register_signal(void) -> void
{
	std::signal(SIGINT, signal_callback);
	std::signal(SIGILL, signal_callback);
	std::signal(SIGABRT, signal_callback);
	std::signal(SIGFPE, signal_callback);
	std::signal(SIGSEGV, signal_callback);
	std::signal(SIGTERM, signal_callback);
}

auto deregister_signal(void) -> void
{
	std::signal(SIGINT, nullptr);
	std::signal(SIGILL, nullptr);
	std::signal(SIGABRT, nullptr);
	std::signal(SIGFPE, nullptr);
	std::signal(SIGSEGV, nullptr);
	std::signal(SIGTERM, nullptr);
}

auto signal_callback(int32_t signum) -> void
{
	deregister_signal();

	if (worker_ != nullptr)
	{
		Logger::handle().write(LogTypes::Information, std::format("received signal {}: stopping", signum));
		worker_->stop();
	}
}
