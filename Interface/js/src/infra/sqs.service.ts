import { Injectable } from '@nestjs/common';
import { ConfigService } from '@nestjs/config';
import { GetQueueAttributesCommand, SendMessageCommand, SQSClient } from '@aws-sdk/client-sqs';
import { NodeHttpHandler } from '@smithy/node-http-handler';

import { AppConfig } from '../config/configuration';
import {
  AWS_CONNECTION_TIMEOUT_MS,
  AWS_OPERATION_TIMEOUT_MS,
  AWS_REQUEST_TIMEOUT_MS,
} from './timeouts';

// SQS FIFO 발행 어댑터. 중복 제거는 큐의 ContentBasedDeduplication에 위임한다.
@Injectable()
export class SqsService {
  private readonly client: SQSClient;
  private readonly queueUrl: string;

  constructor(config: ConfigService<AppConfig, true>) {
    const endpoint = config.get('awsEndpointUrl', { infer: true });
    this.queueUrl = config.get('sqsRequestQueueUrl', { infer: true });

    this.client = new SQSClient({
      region: config.get('awsRegion', { infer: true }),
      // requestTimeout만 지정하면 @smithy/node-http-handler는 경고 로그만 남기고 계속 대기하므로
      // throwOnRequestTimeout으로 실제 중단시킨다
      requestHandler: new NodeHttpHandler({
        connectionTimeout: AWS_CONNECTION_TIMEOUT_MS,
        requestTimeout: AWS_REQUEST_TIMEOUT_MS,
        throwOnRequestTimeout: true,
      }),
      ...(endpoint ? { endpoint } : {}),
    });
  }

  async enqueue(body: string, messageGroupId: string): Promise<void> {
    await this.client.send(
      new SendMessageCommand({
        QueueUrl: this.queueUrl,
        MessageBody: body,
        MessageGroupId: messageGroupId,
      }),
      { abortSignal: AbortSignal.timeout(AWS_OPERATION_TIMEOUT_MS) },
    );
  }

  async ping(): Promise<void> {
    await this.client.send(
      new GetQueueAttributesCommand({ QueueUrl: this.queueUrl, AttributeNames: ['QueueArn'] }),
      { abortSignal: AbortSignal.timeout(AWS_OPERATION_TIMEOUT_MS) },
    );
  }
}
