// Package sqsrepo publishes job request messages to the SQS FIFO queue.
package sqsrepo

import (
	"context"
	"fmt"
	"time"

	awssdk "github.com/aws/aws-sdk-go-v2/aws"
	"github.com/aws/aws-sdk-go-v2/service/sqs"
	"github.com/aws/aws-sdk-go-v2/service/sqs/types"
)

// operationTimeout bounds every SQS call. Without it a stalled queue endpoint
// blocks the request handler indefinitely (the SDK default has no deadline).
const operationTimeout = 5 * time.Second

// Producer sends messages to a fixed queue URL.
type Producer struct {
	client   *sqs.Client
	queueURL string
}

func New(client *sqs.Client, queueURL string) *Producer {
	return &Producer{client: client, queueURL: queueURL}
}

// Enqueue publishes the message body with the given FIFO message group.
// Deduplication relies on the queue's ContentBasedDeduplication setting.
func (p *Producer) Enqueue(ctx context.Context, body, messageGroupID string) error {
	ctx, cancel := context.WithTimeout(ctx, operationTimeout)
	defer cancel()

	_, err := p.client.SendMessage(ctx, &sqs.SendMessageInput{
		QueueUrl:       awssdk.String(p.queueURL),
		MessageBody:    awssdk.String(body),
		MessageGroupId: awssdk.String(messageGroupID),
	})
	if err != nil {
		return fmt.Errorf("send message: %w", err)
	}
	return nil
}

// Ping verifies the queue is reachable.
func (p *Producer) Ping(ctx context.Context) error {
	ctx, cancel := context.WithTimeout(ctx, operationTimeout)
	defer cancel()

	_, err := p.client.GetQueueAttributes(ctx, &sqs.GetQueueAttributesInput{
		QueueUrl:       awssdk.String(p.queueURL),
		AttributeNames: []types.QueueAttributeName{types.QueueAttributeNameQueueArn},
	})
	if err != nil {
		return fmt.Errorf("get queue attributes: %w", err)
	}
	return nil
}
