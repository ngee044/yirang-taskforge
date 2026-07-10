import { Injectable } from '@nestjs/common';
import { ConfigService } from '@nestjs/config';
import { HeadBucketCommand, PutObjectCommand, S3Client } from '@aws-sdk/client-s3';
import { getSignedUrl } from '@aws-sdk/s3-request-presigner';

import { AppConfig } from '../config/configuration';

// S3 presign 어댑터. 도메인 계층에 AWS SDK 타입이 누출되지 않게 한다.
@Injectable()
export class S3Service {
  private readonly client: S3Client;
  private readonly bucketName: string;
  private readonly presignTtlSec: number;

  constructor(config: ConfigService<AppConfig, true>) {
    const endpoint = config.get('awsEndpointUrl', { infer: true });
    this.bucketName = config.get('s3Bucket', { infer: true });
    this.presignTtlSec = config.get('s3PresignTtlSec', { infer: true });

    this.client = new S3Client({
      region: config.get('awsRegion', { infer: true }),
      // 기본값(WHEN_SUPPORTED)은 presigned PUT URL에 빈 본문 기준 x-amz-checksum-crc32를
      // 서명에 포함시켜 실제 파일 업로드가 체크섬 불일치로 실패한다
      requestChecksumCalculation: 'WHEN_REQUIRED',
      ...(endpoint
        ? {
            endpoint,
            // LocalStack 등 커스텀 엔드포인트는 path-style 주소를 요구한다
            forcePathStyle: true,
          }
        : {}),
    });
  }

  get bucket(): string {
    return this.bucketName;
  }

  async presignPut(key: string): Promise<string> {
    return getSignedUrl(this.client, new PutObjectCommand({ Bucket: this.bucketName, Key: key }), {
      expiresIn: this.presignTtlSec,
    });
  }

  async ping(): Promise<void> {
    await this.client.send(new HeadBucketCommand({ Bucket: this.bucketName }));
  }
}
