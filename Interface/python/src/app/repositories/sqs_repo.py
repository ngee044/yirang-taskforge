"""SQS FIFO 발행 어댑터. boto3(동기)는 서비스 계층에서 anyio.to_thread로 감싸 호출한다."""

from __future__ import annotations

import boto3
from botocore.config import Config as BotoConfig


class SQSRepository:
    def __init__(self, region: str, endpoint_url: str, queue_url: str) -> None:
        self._queue_url = queue_url
        self._client = boto3.client(
            "sqs",
            region_name=region,
            endpoint_url=endpoint_url or None,
            config=BotoConfig(connect_timeout=3, read_timeout=5, retries={"max_attempts": 2}),
        )

    def enqueue(self, body: str, message_group_id: str) -> None:
        # 중복 제거는 큐의 ContentBasedDeduplication 설정에 위임한다
        self._client.send_message(
            QueueUrl=self._queue_url,
            MessageBody=body,
            MessageGroupId=message_group_id,
        )

    def ping(self) -> None:
        self._client.get_queue_attributes(QueueUrl=self._queue_url, AttributeNames=["QueueArn"])
