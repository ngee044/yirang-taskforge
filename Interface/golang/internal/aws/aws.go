// Package aws builds AWS SDK v2 clients with optional custom endpoint (LocalStack).
package aws

import (
	"context"

	awssdk "github.com/aws/aws-sdk-go-v2/aws"
	awsconfig "github.com/aws/aws-sdk-go-v2/config"
	"github.com/aws/aws-sdk-go-v2/service/s3"
	"github.com/aws/aws-sdk-go-v2/service/sqs"
)

// NewConfig loads the default credential chain with the given region.
func NewConfig(ctx context.Context, region string) (awssdk.Config, error) {
	return awsconfig.LoadDefaultConfig(ctx, awsconfig.WithRegion(region))
}

// NewS3Client returns an S3 client. A non-empty endpoint switches to
// path-style addressing, which LocalStack requires.
func NewS3Client(cfg awssdk.Config, endpoint string) *s3.Client {
	return s3.NewFromConfig(cfg, func(o *s3.Options) {
		if endpoint != "" {
			o.BaseEndpoint = awssdk.String(endpoint)
			o.UsePathStyle = true
		}
	})
}

// NewSQSClient returns an SQS client honoring the optional custom endpoint.
func NewSQSClient(cfg awssdk.Config, endpoint string) *sqs.Client {
	return sqs.NewFromConfig(cfg, func(o *sqs.Options) {
		if endpoint != "" {
			o.BaseEndpoint = awssdk.String(endpoint)
		}
	})
}
