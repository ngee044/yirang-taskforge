package service

import (
	"context"
	"encoding/json"
	"errors"
	"strings"
	"testing"
	"time"

	"github.com/ngee044/yirang-taskforge/Interface/golang/internal/repository/redisrepo"
)

type fakeS3 struct {
	keys []string
}

func (f *fakeS3) PresignPut(_ context.Context, key string, _ time.Duration) (string, error) {
	f.keys = append(f.keys, key)
	return "https://example.com/" + key, nil
}

func (f *fakeS3) Bucket() string { return "test-bucket" }

type fakeQueue struct {
	bodies []string
	groups []string
	err    error
}

func (f *fakeQueue) Enqueue(_ context.Context, body, group string) error {
	if f.err != nil {
		return f.err
	}
	f.bodies = append(f.bodies, body)
	f.groups = append(f.groups, group)
	return nil
}

type fakeStore struct {
	values map[string]string
	order  []string
}

func newFakeStore() *fakeStore {
	return &fakeStore{values: map[string]string{}}
}

func (f *fakeStore) Get(_ context.Context, key string) (string, error) {
	value, ok := f.values[key]
	if !ok {
		return "", redisrepo.ErrNotFound
	}
	return value, nil
}

func (f *fakeStore) Set(_ context.Context, key, value string, _ time.Duration) error {
	f.values[key] = value
	f.order = append(f.order, key)
	return nil
}

func newService(s3 *fakeS3, queue *fakeQueue, store *fakeStore) *JobService {
	return NewJobService(s3, queue, store, 10*time.Minute, time.Hour, "request")
}

func TestCreateJobBuildsContractMessage(t *testing.T) {
	s3 := &fakeS3{}
	queue := &fakeQueue{}
	store := newFakeStore()
	svc := newService(s3, queue, store)

	result, err := svc.CreateJob(context.Background(), CreateJobInput{
		TaskName:   "wordcount",
		Arguments:  []string{"--verbose"},
		InputFiles: []string{"a.txt", "b.txt"},
		TimeoutSec: 20,
	})
	if err != nil {
		t.Fatalf("CreateJob: %v", err)
	}

	if result.JobID == "" {
		t.Fatal("job_id must not be empty")
	}
	if len(result.UploadURLs) != 2 {
		t.Fatalf("expected 2 upload urls, got %d", len(result.UploadURLs))
	}
	for _, upload := range result.UploadURLs {
		if upload.Method != "PUT" {
			t.Errorf("upload method must be PUT, got %s", upload.Method)
		}
	}

	if len(queue.bodies) != 1 {
		t.Fatalf("expected 1 queue message, got %d", len(queue.bodies))
	}
	if queue.groups[0] != "request" {
		t.Errorf("message group must be request, got %s", queue.groups[0])
	}

	var message map[string]any
	if err := json.Unmarshal([]byte(queue.bodies[0]), &message); err != nil {
		t.Fatalf("queue message is not JSON: %v", err)
	}

	if message["mode"] != "execute" {
		t.Errorf("mode must be execute, got %v", message["mode"])
	}
	if message["job_id"] != result.JobID {
		t.Errorf("job_id mismatch: %v != %s", message["job_id"], result.JobID)
	}

	outputPrefix, _ := message["output_prefix"].(string)
	if !strings.HasPrefix(outputPrefix, "jobs/"+result.JobID+"/outputs/") {
		t.Errorf("unexpected output_prefix: %s", outputPrefix)
	}

	downloads, _ := message["download_s3"].([]any)
	if len(downloads) != 2 {
		t.Fatalf("expected 2 download entries, got %d", len(downloads))
	}
	first, _ := downloads[0].(map[string]any)
	if first["bucket"] != "test-bucket" {
		t.Errorf("bucket mismatch: %v", first["bucket"])
	}
	if first["method"] != "GET" {
		t.Errorf("download method must be GET: %v", first["method"])
	}

	// queued 상태가 SQS 발행 전에 기록되어야 한다
	stored, err := store.Get(context.Background(), result.JobID)
	if err != nil {
		t.Fatalf("queued status missing: %v", err)
	}
	var status map[string]any
	if err := json.Unmarshal([]byte(stored), &status); err != nil {
		t.Fatalf("status is not JSON: %v", err)
	}
	if status["status"] != "queued" {
		t.Errorf("initial status must be queued, got %v", status["status"])
	}
	if status["task_name"] != "wordcount" {
		t.Errorf("task_name mismatch: %v", status["task_name"])
	}
}

func TestCreateJobQueueFailure(t *testing.T) {
	svc := newService(&fakeS3{}, &fakeQueue{err: errors.New("sqs down")}, newFakeStore())

	_, err := svc.CreateJob(context.Background(), CreateJobInput{TaskName: "wordcount"})
	if err == nil {
		t.Fatal("expected error when queue is down")
	}
}

func TestGetJobNotFound(t *testing.T) {
	svc := newService(&fakeS3{}, &fakeQueue{}, newFakeStore())

	_, err := svc.GetJob(context.Background(), "missing")
	if !errors.Is(err, ErrNotFound) {
		t.Fatalf("expected ErrNotFound, got %v", err)
	}
}

func TestGetTaskCatalog(t *testing.T) {
	store := newFakeStore()
	store.values["task_catalog"] = `{"tasks":[{"name":"wordcount"}]}`
	svc := newService(&fakeS3{}, &fakeQueue{}, store)

	catalog, err := svc.GetTaskCatalog(context.Background())
	if err != nil {
		t.Fatalf("GetTaskCatalog: %v", err)
	}
	if !strings.Contains(string(catalog), "wordcount") {
		t.Errorf("catalog content unexpected: %s", catalog)
	}
}
