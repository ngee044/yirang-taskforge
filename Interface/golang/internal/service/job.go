// Package service holds the job orchestration business logic shared by handlers.
package service

import (
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"time"

	"github.com/google/uuid"

	"github.com/ngee044/yirang-taskforge/Interface/golang/internal/repository/redisrepo"
)

// ErrNotFound is returned when a job or catalog document does not exist.
var ErrNotFound = errors.New("service: not found")

// S3Presigner is the storage capability the job service needs.
type S3Presigner interface {
	PresignPut(ctx context.Context, key string, ttl time.Duration) (string, error)
	Bucket() string
}

// QueueProducer publishes job request messages.
type QueueProducer interface {
	Enqueue(ctx context.Context, body, messageGroupID string) error
}

// StatusStore reads/writes job status documents.
type StatusStore interface {
	Get(ctx context.Context, key string) (string, error)
	Set(ctx context.Context, key, value string, ttl time.Duration) error
}

// JobService implements the create/get flows of the job API.
type JobService struct {
	s3         S3Presigner
	queue      QueueProducer
	store      StatusStore
	presignTTL time.Duration
	statusTTL  time.Duration
	groupID    string
}

func NewJobService(s3 S3Presigner, queue QueueProducer, store StatusStore, presignTTL, statusTTL time.Duration, groupID string) *JobService {
	return &JobService{
		s3:         s3,
		queue:      queue,
		store:      store,
		presignTTL: presignTTL,
		statusTTL:  statusTTL,
		groupID:    groupID,
	}
}

// CreateJobInput is the validated request payload.
type CreateJobInput struct {
	TaskName   string
	Arguments  []string
	InputFiles []string
	TimeoutSec int
}

// UploadURL is a presigned PUT target for one input file.
type UploadURL struct {
	Filename  string `json:"filename"`
	UploadURL string `json:"upload_url"`
	Method    string `json:"method"`
}

// CreateJobResult is returned to the client with HTTP 202.
type CreateJobResult struct {
	JobID      string      `json:"job_id"`
	UploadURLs []UploadURL `json:"upload_urls"`
}

// 계약 #1: SQS 요청 메시지 (docs/API.md)
type queueMessage struct {
	JobID        string            `json:"job_id"`
	Mode         string            `json:"mode"`
	Task         queueTask         `json:"task"`
	DownloadS3   []queueDownloadS3 `json:"download_s3"`
	OutputPrefix string            `json:"output_prefix"`
	EnqueuedAt   string            `json:"enqueued_at"`
}

type queueTask struct {
	Name       string   `json:"name"`
	Arguments  []string `json:"arguments"`
	TimeoutSec int      `json:"timeout_sec,omitempty"`
}

type queueDownloadS3 struct {
	Bucket string `json:"bucket"`
	Key    string `json:"key"`
	Method string `json:"method"`
}

// 계약 #2: Redis 상태 JSON의 초기(queued) 문서
type initialStatus struct {
	JobID      string `json:"job_id"`
	TaskName   string `json:"task_name"`
	Status     string `json:"status"`
	EnqueuedAt string `json:"enqueued_at"`
	UpdatedAt  string `json:"updated_at"`
}

// CreateJob issues a job_id, presigns input uploads, records the queued
// status, and publishes the request message. The queued status is written
// BEFORE the SQS publish so the worker's running update can never be
// overwritten by the initial document.
func (s *JobService) CreateJob(ctx context.Context, in CreateJobInput) (*CreateJobResult, error) {
	jobID := uuid.NewString()
	now := time.Now().UTC().Format(time.RFC3339)

	uploadURLs := make([]UploadURL, 0, len(in.InputFiles))
	downloads := make([]queueDownloadS3, 0, len(in.InputFiles))
	for _, filename := range in.InputFiles {
		key := fmt.Sprintf("jobs/%s/inputs/%s", jobID, filename)

		url, err := s.s3.PresignPut(ctx, key, s.presignTTL)
		if err != nil {
			return nil, fmt.Errorf("presign input %s: %w", filename, err)
		}

		uploadURLs = append(uploadURLs, UploadURL{Filename: filename, UploadURL: url, Method: "PUT"})
		downloads = append(downloads, queueDownloadS3{Bucket: s.s3.Bucket(), Key: key, Method: "GET"})
	}

	status, err := json.Marshal(initialStatus{
		JobID:      jobID,
		TaskName:   in.TaskName,
		Status:     "queued",
		EnqueuedAt: now,
		UpdatedAt:  now,
	})
	if err != nil {
		return nil, fmt.Errorf("marshal status: %w", err)
	}
	if err := s.store.Set(ctx, jobID, string(status), s.statusTTL); err != nil {
		return nil, fmt.Errorf("record queued status: %w", err)
	}

	message, err := json.Marshal(queueMessage{
		JobID:        jobID,
		Mode:         "execute",
		Task:         queueTask{Name: in.TaskName, Arguments: emptyIfNil(in.Arguments), TimeoutSec: in.TimeoutSec},
		DownloadS3:   downloads,
		OutputPrefix: fmt.Sprintf("jobs/%s/outputs/", jobID),
		EnqueuedAt:   now,
	})
	if err != nil {
		return nil, fmt.Errorf("marshal queue message: %w", err)
	}
	if err := s.queue.Enqueue(ctx, string(message), s.groupID); err != nil {
		return nil, fmt.Errorf("enqueue job: %w", err)
	}

	return &CreateJobResult{JobID: jobID, UploadURLs: uploadURLs}, nil
}

// GetJob returns the raw status document stored by the worker.
func (s *JobService) GetJob(ctx context.Context, jobID string) (json.RawMessage, error) {
	value, err := s.store.Get(ctx, jobID)
	if errors.Is(err, redisrepo.ErrNotFound) {
		return nil, ErrNotFound
	}
	if err != nil {
		return nil, err
	}

	if !json.Valid([]byte(value)) {
		return nil, fmt.Errorf("job %s status document is not valid JSON", jobID)
	}
	return json.RawMessage(value), nil
}

// GetTaskCatalog returns the whitelist catalog uploaded by the worker.
func (s *JobService) GetTaskCatalog(ctx context.Context) (json.RawMessage, error) {
	value, err := s.store.Get(ctx, "task_catalog")
	if errors.Is(err, redisrepo.ErrNotFound) {
		return nil, ErrNotFound
	}
	if err != nil {
		return nil, err
	}

	if !json.Valid([]byte(value)) {
		return nil, errors.New("task catalog document is not valid JSON")
	}
	return json.RawMessage(value), nil
}

func emptyIfNil(values []string) []string {
	if values == nil {
		return []string{}
	}
	return values
}
