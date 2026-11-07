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

#include <pthread.h>

using namespace Utilities;

auto main(int32_t argc, char* argv[]) -> int32_t
{
	// 모든 서비스 스레드가 같은 마스크를 상속하도록 Logger 시작 전에 차단한다.
	// 종료와 로깅은 비동기 시그널 핸들러가 아닌 main 스레드에서 수행한다.
	sigset_t termination_signals;
	sigemptyset(&termination_signals);
	sigaddset(&termination_signals, SIGINT);
	sigaddset(&termination_signals, SIGTERM);
	auto mask_error = pthread_sigmask(SIG_BLOCK, &termination_signals, nullptr);
	if (mask_error != 0)
	{
		std::cerr << std::format("cannot block termination signals: {}\n", mask_error);
		return 1;
	}

	// 컨테이너의 stdout은 파이프이므로 기본 블록 버퍼링(약 4KB)이 적용된다.
	// 그대로 두면 최신 로그가 버퍼가 찰 때까지 docker logs에 나타나지 않아
	// 장애 시점의 관측이 불가능해진다 (NFR-OBS-01)
	std::cout << std::unitbuf;

	auto configurations = std::make_shared<Configurations>(ArgumentParser(argc, argv));

	Logger::handle().file_mode(configurations->write_file_log());
	Logger::handle().console_mode(configurations->write_console_log());
	Logger::handle().write_interval(configurations->write_interval());
	Logger::handle().log_root(configurations->log_root_path());
	Logger::handle().start(configurations->worker_title());

	// AWS SDK 수명주기는 애플리케이션이 소유합니다 (AWSService 라이브러리가 대신 하지 않음)
	Aws::SDKOptions sdk_options;
	Aws::InitAPI(sdk_options);

	auto worker = std::make_shared<TaskForgeWorker>(configurations);

	// 기동 실패는 0이 아닌 종료 코드로 알린다. exit 0으로 종료하면 오케스트레이터의
	// restart 정책이 실패를 인지하지 못해 스택이 무증상 불능 상태가 된다 (FR-OPS-02)
	int32_t exit_code = 0;

	auto started = worker->start();
	if (!started)
	{
		Logger::handle().write(LogTypes::Error, std::format("cannot start TaskForgeWorker: {}", started.error()));
		exit_code = 1;
	}
	else
	{
		int signum = 0;
		auto signal_error = sigwait(&termination_signals, &signum);
		if (signal_error != 0)
		{
			Logger::handle().write(LogTypes::Error, std::format("cannot wait for termination signal: {}", signal_error));
			exit_code = 1;
		}
		else
		{
			Logger::handle().write(LogTypes::Information, std::format("received signal {}: stopping", signum));
		}
	}

	worker->stop();
	worker.reset();
	configurations.reset();

	Aws::ShutdownAPI(sdk_options);

	Logger::handle().stop();
	Logger::destroy();

	return exit_code;
}
