#!/bin/bash
# LocalStack ready hook: 버킷과 FIFO 요청 큐를 생성한다
set -euo pipefail

awslocal s3 mb s3://taskforge-jobs || true

awslocal sqs create-queue \
    --queue-name taskforge-jobs-request.fifo \
    --attributes FifoQueue=true,ContentBasedDeduplication=true || true

echo "taskforge localstack init done"
