"""S3 presign 어댑터. boto3(동기)는 서비스 계층에서 anyio.to_thread로 감싸 호출한다."""

from __future__ import annotations

from typing import Any

import boto3
from botocore.config import Config as BotoConfig


class S3Repository:
    def __init__(self, region: str, endpoint_url: str, bucket: str) -> None:
        self._bucket = bucket
        # LocalStack 등 커스텀 엔드포인트는 path-style 주소를 요구한다
        addressing: dict[str, Any] = {"addressing_style": "path"} if endpoint_url else {}
        self._client = boto3.client(
            "s3",
            region_name=region,
            endpoint_url=endpoint_url or None,
            # signature_version을 명시하지 않으면 botocore가 us-east-1 등 legacy 리전에서
            # presign 서명기를 SigV2로 되돌린다(_default_s3_presign_to_sigv2). AWS는 2020-06-24 이후
            # 생성된 버킷에서 SigV2를 지원하지 않으므로 SigV4를 강제한다 (FR-JOB-02, NFR-PORT-01)
            # 총 시도 횟수를 직접 고정해 최악 응답 시간을 유계화한다 (sqs_repo와 동일 근거)
            config=BotoConfig(
                s3=addressing,
                signature_version="s3v4",
                connect_timeout=2,
                read_timeout=2,
                retries={"total_max_attempts": 2},
            ),
        )

    @property
    def bucket(self) -> str:
        return self._bucket

    def presign_put(self, key: str, ttl_sec: int) -> str:
        return self._client.generate_presigned_url(
            "put_object",
            Params={"Bucket": self._bucket, "Key": key},
            ExpiresIn=ttl_sec,
            HttpMethod="PUT",
        )

    def ping(self) -> None:
        self._client.head_bucket(Bucket=self._bucket)
