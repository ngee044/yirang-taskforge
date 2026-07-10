// Package http assembles the Gin router and middleware chain.
package http

import (
	"log/slog"
	"time"

	"github.com/gin-gonic/gin"

	"github.com/ngee044/yirang-taskforge/Interface/golang/internal/http/handlers"
	"github.com/ngee044/yirang-taskforge/Interface/golang/internal/response"
)

// NewRouter wires middleware and routes. Handlers are injected from main.
func NewRouter(jobs *handlers.JobsHandler, tasks *handlers.TasksHandler, health *handlers.HealthHandler) *gin.Engine {
	gin.SetMode(gin.ReleaseMode)

	router := gin.New()
	router.Use(gin.Recovery(), requestLogger())

	router.GET("/healthz", health.Liveness)
	router.GET("/readyz", health.Readiness)

	v1 := router.Group("/api/v1")
	{
		v1.POST("/jobs", jobs.Create)
		v1.GET("/jobs/:job_id", jobs.Get)
		v1.GET("/tasks", tasks.List)
	}

	router.NoRoute(func(c *gin.Context) {
		response.Error(c, 404, "not_found", "route not found")
	})

	return router
}

// requestLogger emits one structured log line per request.
func requestLogger() gin.HandlerFunc {
	return func(c *gin.Context) {
		start := time.Now()
		c.Next()

		slog.Info("request",
			"method", c.Request.Method,
			"path", c.Request.URL.Path,
			"status", c.Writer.Status(),
			"duration_ms", time.Since(start).Milliseconds(),
			"client_ip", c.ClientIP(),
		)
	}
}
