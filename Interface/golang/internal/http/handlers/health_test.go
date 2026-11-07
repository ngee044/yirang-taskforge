package handlers

import (
	"context"
	"encoding/json"
	"errors"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"
	"time"

	"github.com/gin-gonic/gin"
)

type probeFunc func(context.Context) error

func (f probeFunc) Ping(ctx context.Context) error { return f(ctx) }

func TestReadiness(t *testing.T) {
	cases := []struct {
		name       string
		probeError error
		wantStatus int
		wantState  string
		wantCheck  string
	}{
		{name: "healthy", wantStatus: http.StatusOK, wantState: "ready", wantCheck: "ok"},
		{
			name:       "unavailable",
			probeError: errors.New("dial redis.internal:6379: connection refused"),
			wantStatus: http.StatusServiceUnavailable,
			wantState:  "not_ready",
			wantCheck:  "unavailable",
		},
	}

	for _, tc := range cases {
		t.Run(tc.name, func(t *testing.T) {
			called := 0
			probe := probeFunc(func(ctx context.Context) error {
				called++
				deadline, ok := ctx.Deadline()
				if !ok || time.Until(deadline) > 3*time.Second {
					t.Error("readiness probe must have a deadline within 3 seconds")
				}
				return tc.probeError
			})
			handler := NewHealthHandler(map[string]Pinger{"redis": probe, "sqs": probe, "s3": probe})
			router := gin.New()
			router.GET("/readyz", handler.Readiness)
			recorder := httptest.NewRecorder()
			router.ServeHTTP(recorder, httptest.NewRequest(http.MethodGet, "/readyz", nil))

			if recorder.Code != tc.wantStatus {
				t.Fatalf("status = %d, want %d", recorder.Code, tc.wantStatus)
			}
			var body struct {
				Status string            `json:"status"`
				Checks map[string]string `json:"checks"`
			}
			if err := json.Unmarshal(recorder.Body.Bytes(), &body); err != nil {
				t.Fatalf("decode readiness response: %v", err)
			}
			if body.Status != tc.wantState || called != 3 || len(body.Checks) != 3 {
				t.Fatalf("unexpected readiness result: %s (probes = %d)", recorder.Body, called)
			}
			for name, check := range body.Checks {
				if check != tc.wantCheck {
					t.Errorf("check %s = %s, want %s", name, check, tc.wantCheck)
				}
			}
			if strings.Contains(recorder.Body.String(), "redis.internal") {
				t.Error("readiness response leaked the dependency address")
			}
		})
	}
}

func TestReadinessPropagatesRequestCancellation(t *testing.T) {
	ctx, cancel := context.WithCancel(context.Background())
	cancel()
	called := 0
	probe := probeFunc(func(ctx context.Context) error {
		called++
		if !errors.Is(ctx.Err(), context.Canceled) {
			t.Fatal("readiness probe did not receive request cancellation")
		}
		return ctx.Err()
	})
	handler := NewHealthHandler(map[string]Pinger{"redis": probe, "sqs": probe, "s3": probe})
	router := gin.New()
	router.GET("/readyz", handler.Readiness)
	recorder := httptest.NewRecorder()
	router.ServeHTTP(recorder, httptest.NewRequest(http.MethodGet, "/readyz", nil).WithContext(ctx))
	if recorder.Code != http.StatusServiceUnavailable || called != 3 {
		t.Fatalf("status = %d, probes = %d", recorder.Code, called)
	}
}
