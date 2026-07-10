package handlers

import (
	"errors"
	"log/slog"
	"net/http"

	"github.com/gin-gonic/gin"

	"github.com/ngee044/yirang-taskforge/Interface/golang/internal/response"
	"github.com/ngee044/yirang-taskforge/Interface/golang/internal/service"
)

// TasksHandler exposes GET /tasks (worker whitelist catalog).
type TasksHandler struct {
	jobs *service.JobService
}

func NewTasksHandler(jobs *service.JobService) *TasksHandler {
	return &TasksHandler{jobs: jobs}
}

// List handles GET /api/v1/tasks.
func (h *TasksHandler) List(c *gin.Context) {
	catalog, err := h.jobs.GetTaskCatalog(c.Request.Context())
	if errors.Is(err, service.ErrNotFound) {
		response.Error(c, http.StatusNotFound, "catalog_not_found",
			"task catalog is not published yet (worker not started?)")
		return
	}
	if err != nil {
		slog.Error("get task catalog failed", "error", err)
		response.Error(c, http.StatusInternalServerError, "catalog_failed", "cannot read task catalog")
		return
	}

	response.Success(c, http.StatusOK, catalog)
}
