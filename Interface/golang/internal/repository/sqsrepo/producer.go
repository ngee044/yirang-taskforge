// Package sqsrepo publishes job request messages to the SQS FIFO queue.
package sqsrepo

import (
	"context"
	"fmt"

	awssdk "github.com/aws/aws-sdk-go-v2/aws"
	"github.com/aws/aws-sdk-go-v2/service/sqs"
	"github.com/aws/aws-sdk-go-v2/service/sqs/types"
)

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
	_, err := p.client.GetQueueAttributes(ctx, &sqs.GetQueueAttributesInput{
		QueueUrl:       awssdk.String(p.queueURL),
		AttributeNames: []types.QueueAttributeName{types.QueueAttributeNameQueueArn},
	})
	if err != nil {
		return fmt.Errorf("get queue attributes: %w", err)
	}
	return nil
}
