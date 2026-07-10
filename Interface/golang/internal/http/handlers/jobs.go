// Package handlers maps HTTP requests onto the job service.
package handlers

import (
	"errors"
	"log/slog"
	"net/http"
	"strings"

	"github.com/gin-gonic/gin"

	"github.com/ngee044/yirang-taskforge/Interface/golang/internal/response"
	"github.com/ngee044/yirang-taskforge/Interface/golang/internal/service"
)

// JobsHandler exposes POST /jobs and GET /jobs/:job_id.
type JobsHandler struct {
	jobs *service.JobService
}

func NewJobsHandler(jobs *service.JobService) *JobsHandler {
	return &JobsHandler{jobs: jobs}
}

type inputFileDTO struct {
	Filename string `json:"filename" binding:"required"`
}

type createJobDTO struct {
	TaskName   string         `json:"task_name" binding:"required"`
	Arguments  []string       `json:"arguments"`
	InputFiles []inputFileDTO `json:"input_files"`
	TimeoutSec int            `json:"timeout_sec" binding:"omitempty,gte=1,lte=3600"`
}

// Create handles POST /api/v1/jobs and replies 202 with presigned upload URLs.
func (h *JobsHandler) Create(c *gin.Context) {
	var dto createJobDTO
	if err := c.ShouldBindJSON(&dto); err != nil {
		response.Error(c, http.StatusBadRequest, "invalid_request", err.Error())
		return
	}

	filenames := make([]string, 0, len(dto.InputFiles))
	for _, file := range dto.InputFiles {
		if !validFilename(file.Filename) {
			response.Error(c, http.StatusBadRequest, "invalid_filename",
				"filename must not be empty or contain path separators")
			return
		}
		filenames = append(filenames, file.Filename)
	}

	result, err := h.jobs.CreateJob(c.Request.Context(), service.CreateJobInput{
		TaskName:   dto.TaskName,
		Arguments:  dto.Arguments,
		InputFiles: filenames,
		TimeoutSec: dto.TimeoutSec,
	})
	if err != nil {
		slog.Error("create job failed", "error", err)
		response.Error(c, http.StatusInternalServerError, "create_failed", "cannot enqueue job")
		return
	}

	slog.Info("job enqueued", "job_id", result.JobID, "task_name", dto.TaskName, "inputs", len(filenames))
	response.Success(c, http.StatusAccepted, result)
}

// Get handles GET /api/v1/jobs/:job_id.
func (h *JobsHandler) Get(c *gin.Context) {
	jobID := c.Param("job_id")

	document, err := h.jobs.GetJob(c.Request.Context(), jobID)
	if errors.Is(err, service.ErrNotFound) {
		response.Error(c, http.StatusNotFound, "job_not_found", "job not found")
		return
	}
	if err != nil {
		slog.Error("get job failed", "job_id", jobID, "error", err)
		response.Error(c, http.StatusInternalServerError, "get_failed", "cannot read job status")
		return
	}

	response.Success(c, http.StatusOK, document)
}

// validFilename rejects empty names and path traversal attempts.
func validFilename(filename string) bool {
	if filename == "" || len(filename) > 255 {
		return false
	}
	if strings.ContainsAny(filename, "/\\") {
		return false
	}
	if strings.Contains(filename, "..") {
		return false
	}
	return true
}
