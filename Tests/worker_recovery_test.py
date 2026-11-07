"""LocalStack과 격리 Redis/워커를 사용한 상태 오류·SQS ACK 회귀 검사."""

from __future__ import annotations

import json
import subprocess
import tempfile
import time
import uuid
from collections.abc import Callable
from contextlib import suppress
from pathlib import Path


def docker(*arguments: str) -> str:
    return subprocess.check_output(["docker", *arguments], text=True, stderr=subprocess.STDOUT, timeout=40).strip()


def wait_for(check: Callable[[], bool], description: str) -> None:
    deadline = time.monotonic() + 45
    last_error = ""
    while time.monotonic() < deadline:
        try:
            if check():
                return
        except subprocess.CalledProcessError as error:
            last_error = error.output
        time.sleep(0.25)
    raise AssertionError(f"timed out: {description}; {last_error}")


def main() -> None:
    suffix = uuid.uuid4().hex[:12]
    redis_name = f"taskforge-recovery-redis-{suffix}"
    worker_name = f"taskforge-recovery-worker-{suffix}"
    queue_name = f"taskforge-recovery-{suffix}.fifo"
    localstack = "taskforge-localstack"
    metadata = json.loads(docker("inspect", localstack))[0]
    network = next(iter(metadata["NetworkSettings"]["Networks"]))
    queue_url = f"http://{localstack}:4566/000000000000/{queue_name}"

    def sqs(*arguments: str) -> str:
        return docker("exec", localstack, "awslocal", "sqs", *arguments)

    def redis(*arguments: str) -> str:
        return docker("exec", redis_name, "redis-cli", "--raw", *arguments)

    def document(job_id: str) -> dict[str, object]:
        value = redis("GET", job_id)
        return json.loads(value) if value.startswith("{") else {}

    def queue_size() -> int:
        response = json.loads(
            sqs(
                "get-queue-attributes",
                "--queue-url",
                queue_url,
                "--attribute-names",
                "All",
            )
        )
        attributes = response["Attributes"]
        return int(attributes["ApproximateNumberOfMessages"]) + int(attributes["ApproximateNumberOfMessagesNotVisible"])

    def send(job_id: str, task_name: str) -> None:
        message = {
            "job_id": job_id,
            "mode": "execute",
            "task": {"name": task_name, "arguments": []},
            "download_s3": [],
            "output_prefix": f"jobs/{job_id}/outputs/",
            "enqueued_at": "2026-11-07T00:00:00Z",
        }
        sqs(
            "send-message",
            "--queue-url",
            queue_url,
            "--message-body",
            json.dumps(message),
            "--message-group-id",
            "recovery",
            "--message-deduplication-id",
            str(uuid.uuid4()),
        )

    with tempfile.TemporaryDirectory(prefix="taskforge-recovery-") as directory:
        path = Path(directory)
        path.chmod(0o755)
        configuration = {
            "redis_host": redis_name,
            "redis_port": 6379,
            "aws_endpoint": f"http://{localstack}:4566",
            "aws_region": "us-east-1",
            "aws_access_key": "test",
            "aws_secret_key": "test",
            "sqs_request_queue_url": queue_url,
            "sqs_wait_time_seconds": 1,
            "sqs_visibility_timeout": 2,
            "s3_bucket": "taskforge-jobs",
            "task_whitelist": [{"name": "success", "executable": "/bin/true", "description": "test"}],
            "write_file_log": 0,
            "write_console_log": 4,
            "write_interval": 10,
            "dlq_path": "/tmp/taskforge-recovery-dlq",
        }
        config_path = path / "task_forge_worker_configurations.json"
        config_path.write_text(json.dumps(configuration), encoding="utf-8")
        config_path.chmod(0o644)
        try:
            sqs(
                "create-queue",
                "--queue-name",
                queue_name,
                "--attributes",
                '{"FifoQueue":"true","ContentBasedDeduplication":"true"}',
            )
            docker(
                "run",
                "-d",
                "--name",
                redis_name,
                "--network",
                network,
                "redis:7-alpine",
            )
            wait_for(lambda: redis("PING") == "PONG", "Redis startup")
            docker(
                "run",
                "-d",
                "--name",
                worker_name,
                "--network",
                network,
                "-v",
                f"{path}:/app/config:ro",
                "taskforge-worker:latest",
            )
            wait_for(
                lambda: "TaskForgeWorker started" in docker("logs", worker_name),
                "worker startup",
            )

            write_job = str(uuid.uuid4())
            redis("CONFIG", "SET", "min-replicas-to-write", "1")
            send(write_job, "unregistered")
            wait_for(
                lambda: f"cannot save job status [{write_job}=failed]" in docker("logs", worker_name),
                "write failure reaches callback boundary",
            )
            assert queue_size() == 1, "message was acknowledged while status writes failed"
            redis("CONFIG", "SET", "min-replicas-to-write", "0")
            wait_for(lambda: document(write_job).get("status") == "failed", "write recovery")
            wait_for(lambda: queue_size() == 0, "ACK after terminal status")
            print("PASS: failed Redis write preserves message; recovery records failed and ACKs")

            read_job = str(uuid.uuid4())
            redis("LPUSH", read_job, "wrong-type")
            send(read_job, "unregistered")
            wait_for(
                lambda: read_job in docker("logs", worker_name) and "WRONGTYPE" in docker("logs", worker_name),
                "read failure reaches callback boundary",
            )
            assert queue_size() == 1, "message was acknowledged while status reads failed"
            assert redis("TYPE", read_job) == "list", "read failure overwrote the existing value"
            redis("DEL", read_job)
            wait_for(lambda: document(read_job).get("status") == "failed", "read recovery")
            wait_for(lambda: queue_size() == 0, "ACK after read recovery")
            print("PASS: failed Redis read preserves message and value; nil recovery succeeds")

            success_job = str(uuid.uuid4())
            send(success_job, "success")
            wait_for(lambda: document(success_job).get("status") == "done", "successful task")
            wait_for(lambda: queue_size() == 0, "successful task ACK")
            saved = document(success_job)
            assert saved["task_name"] == "success" and saved["enqueued_at"] == "2026-11-07T00:00:00Z"
            send(success_job, "success")
            wait_for(
                lambda: f"job [{success_job}] already terminal" in docker("logs", worker_name),
                "terminal redelivery",
            )
            wait_for(lambda: queue_size() == 0, "terminal redelivery ACK")
            assert document(success_job) == saved, "terminal redelivery changed the status"
            print("PASS: missing status is created; terminal redelivery preserves it")

            docker("stop", "-t", "30", worker_name)
            exit_code = json.loads(docker("inspect", worker_name))[0]["State"]["ExitCode"]
            assert exit_code == 0, f"worker shutdown failed: {exit_code}"
            print("PASS: normal SIGTERM exits cleanly")
        except BaseException:
            with suppress(subprocess.CalledProcessError):
                print(docker("logs", worker_name))
            raise
        finally:
            for name in (worker_name, redis_name):
                subprocess.run(
                    ["docker", "rm", "-f", name],
                    capture_output=True,
                    timeout=40,
                    check=False,
                )
            sqs("delete-queue", "--queue-url", queue_url)


if __name__ == "__main__":
    main()
