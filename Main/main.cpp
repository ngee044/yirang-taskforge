#include "Configurations.h"
#include "TaskForgeWorker.h"

#include "ArgumentParser.h"
#include "Logger.h"
#include "LogTypes.h"

#include <aws/core/Aws.h>

#include <csignal>
#include <format>
#include <iostream>
#include <memory>

using namespace Utilities;

std::shared_ptr<Configurations> configurations_ = nullptr;
std::shared_ptr<TaskForgeWorker> worker_ = nullptr;

auto register_signal(void) -> void;
auto deregister_signal(void) -> void;
auto signal_callback(int32_t signum) -> void;

auto main(int32_t argc, char* argv[]) -> int32_t
{
	// 컨테이너의 stdout은 파이프이므로 기본 블록 버퍼링(약 4KB)이 적용된다.
	// 그대로 두면 최신 로그가 버퍼가 찰 때까지 docker logs에 나타나지 않아
	// 장애 시점의 관측이 불가능해진다 (NFR-OBS-01)
	std::cout << std::unitbuf;

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

	// 기동 실패는 0이 아닌 종료 코드로 알린다. exit 0으로 종료하면 오케스트레이터의
	// restart 정책이 실패를 인지하지 못해 스택이 무증상 불능 상태가 된다 (FR-OPS-02)
	int32_t exit_code = 0;

	auto started = worker_->start();
	if (!started)
	{
		Logger::handle().write(LogTypes::Error, std::format("cannot start TaskForgeWorker: {}", started.error()));
		worker_->stop();
		exit_code = 1;
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

	return exit_code;
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
