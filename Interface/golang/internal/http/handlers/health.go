package handlers

import (
	"context"
	"net/http"
	"time"

	"github.com/gin-gonic/gin"
)

// Pinger is a minimal readiness probe over one dependency.
type Pinger interface {
	Ping(ctx context.Context) error
}

// HealthHandler exposes /healthz (liveness) and /readyz (readiness).
type HealthHandler struct {
	startedAt time.Time
	probes    map[string]Pinger
}

func NewHealthHandler(probes map[string]Pinger) *HealthHandler {
	return &HealthHandler{startedAt: time.Now(), probes: probes}
}

// Liveness handles GET /healthz.
func (h *HealthHandler) Liveness(c *gin.Context) {
	c.JSON(http.StatusOK, gin.H{
		"status": "ok",
		"uptime": time.Since(h.startedAt).String(),
		"time":   time.Now().UTC().Format(time.RFC3339),
	})
}

// Readiness handles GET /readyz and checks Redis/SQS/S3 with a bounded timeout.
func (h *HealthHandler) Readiness(c *gin.Context) {
	ctx, cancel := context.WithTimeout(c.Request.Context(), 3*time.Second)
	defer cancel()

	checks := make(map[string]string, len(h.probes))
	ready := true
	for name, probe := range h.probes {
		if err := probe.Ping(ctx); err != nil {
			checks[name] = err.Error()
			ready = false
			continue
		}
		checks[name] = "ok"
	}

	status := http.StatusOK
	state := "ready"
	if !ready {
		status = http.StatusServiceUnavailable
		state = "not_ready"
	}

	c.JSON(status, gin.H{"status": state, "checks": checks})
}
