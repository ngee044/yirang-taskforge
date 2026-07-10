// Package s3repo wraps S3 presign/health operations used by the API.
package s3repo

import (
	"context"
	"fmt"
	"time"

	awssdk "github.com/aws/aws-sdk-go-v2/aws"
	"github.com/aws/aws-sdk-go-v2/service/s3"
)

// Repository provides presigned URL generation against a single bucket.
type Repository struct {
	client  *s3.Client
	presign *s3.PresignClient
	bucket  string
}

func New(client *s3.Client, bucket string) *Repository {
	return &Repository{
		client:  client,
		presign: s3.NewPresignClient(client),
		bucket:  bucket,
	}
}

// Bucket returns the configured bucket name.
func (r *Repository) Bucket() string {
	return r.bucket
}

// PresignPut issues a presigned PUT URL for the given object key.
func (r *Repository) PresignPut(ctx context.Context, key string, ttl time.Duration) (string, error) {
	request, err := r.presign.PresignPutObject(ctx, &s3.PutObjectInput{
		Bucket: awssdk.String(r.bucket),
		Key:    awssdk.String(key),
	}, s3.WithPresignExpires(ttl))
	if err != nil {
		return "", fmt.Errorf("presign put %s: %w", key, err)
	}
	return request.URL, nil
}

// Ping verifies the bucket is reachable.
func (r *Repository) Ping(ctx context.Context) error {
	_, err := r.client.HeadBucket(ctx, &s3.HeadBucketInput{Bucket: awssdk.String(r.bucket)})
	if err != nil {
		return fmt.Errorf("head bucket %s: %w", r.bucket, err)
	}
	return nil
}
