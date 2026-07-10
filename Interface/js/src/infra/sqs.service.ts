import { Injectable } from '@nestjs/common';
import { ConfigService } from '@nestjs/config';
import { GetQueueAttributesCommand, SendMessageCommand, SQSClient } from '@aws-sdk/client-sqs';

import { AppConfig } from '../config/configuration';

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
    );
  }

  async ping(): Promise<void> {
    await this.client.send(
      new GetQueueAttributesCommand({ QueueUrl: this.queueUrl, AttributeNames: ['QueueArn'] }),
    );
  }
}
