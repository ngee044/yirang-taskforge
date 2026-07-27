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
            # max_attempts는 botocore에서 "재시도 횟수"로 해석되어 총 시도가 1회 늘어난다.
            # 총 시도 횟수를 직접 고정해 최악 응답 시간을 약 8초(2회 × (연결 2초 + 응답 2초))로 유계화한다.
            config=BotoConfig(connect_timeout=2, read_timeout=2, retries={"total_max_attempts": 2}),
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
