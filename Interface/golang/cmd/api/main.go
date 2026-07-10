// Command api runs the yirang-taskforge Go interface (Gin REST API).
package main

import (
	"context"
	"errors"
	"log/slog"
	"net/http"
	"os"
	"os/signal"
	"syscall"
	"time"

	appaws "github.com/ngee044/yirang-taskforge/Interface/golang/internal/aws"
	"github.com/ngee044/yirang-taskforge/Interface/golang/internal/config"
	apphttp "github.com/ngee044/yirang-taskforge/Interface/golang/internal/http"
	"github.com/ngee044/yirang-taskforge/Interface/golang/internal/http/handlers"
	"github.com/ngee044/yirang-taskforge/Interface/golang/internal/repository/redisrepo"
	"github.com/ngee044/yirang-taskforge/Interface/golang/internal/repository/s3repo"
	"github.com/ngee044/yirang-taskforge/Interface/golang/internal/repository/sqsrepo"
	"github.com/ngee044/yirang-taskforge/Interface/golang/internal/service"
)

func main() {
	if err := run(); err != nil {
		slog.Error("fatal", "error", err)
		os.Exit(1)
	}
}

func run() error {
	cfg, err := config.Load()
	if err != nil {
		return err
	}

	setupLogger(cfg.LogLevel)

	if err := cfg.Validate(); err != nil {
		return err
	}

	ctx := context.Background()

	awsCfg, err := appaws.NewConfig(ctx, cfg.AWSRegion)
	if err != nil {
		return err
	}

	s3Repo := s3repo.New(appaws.NewS3Client(awsCfg, cfg.AWSEndpointURL), cfg.S3Bucket)
	sqsProducer := sqsrepo.New(appaws.NewSQSClient(awsCfg, cfg.AWSEndpointURL), cfg.SQSRequestQueueURL)
	redisRepo := redisrepo.New(cfg.RedisAddr, cfg.RedisDB, cfg.RedisPassword)
	defer redisRepo.Close()

	jobService := service.NewJobService(s3Repo, sqsProducer, redisRepo, cfg.PresignTTL, cfg.RedisTTL, cfg.SQSMessageGroupID)

	router := apphttp.NewRouter(
		handlers.NewJobsHandler(jobService),
		handlers.NewTasksHandler(jobService),
		handlers.NewHealthHandler(map[string]handlers.Pinger{
			"redis": redisRepo,
			"sqs":   sqsProducer,
			"s3":    s3Repo,
		}),
	)

	server := &http.Server{
		Addr:              cfg.HTTPAddr,
		Handler:           router,
		ReadHeaderTimeout: 5 * time.Second,
		WriteTimeout:      30 * time.Second,
		IdleTimeout:       60 * time.Second,
	}

	errCh := make(chan error, 1)
	go func() {
		slog.Info("listening", "addr", cfg.HTTPAddr)
		if err := server.ListenAndServe(); err != nil && !errors.Is(err, http.ErrServerClosed) {
			errCh <- err
		}
	}()

	quit := make(chan os.Signal, 1)
	signal.Notify(quit, syscall.SIGINT, syscall.SIGTERM)

	select {
	case err := <-errCh:
		return err
	case sig := <-quit:
		slog.Info("shutting down", "signal", sig.String())
	}

	shutdownCtx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()
	return server.Shutdown(shutdownCtx)
}

func setupLogger(level string) {
	logLevel := slog.LevelInfo
	switch level {
	case "debug":
		logLevel = slog.LevelDebug
	case "warn":
		logLevel = slog.LevelWarn
	case "error":
		logLevel = slog.LevelError
	}

	slog.SetDefault(slog.New(slog.NewJSONHandler(os.Stdout, &slog.HandlerOptions{Level: logLevel})))
}
