// Package config loads API configuration from environment variables (12-factor).
package config

import (
	"errors"
	"fmt"
	"os"
	"strconv"
	"time"
)

// Config holds every runtime setting of the interface API.
type Config struct {
	HTTPAddr string

	AWSRegion      string
	AWSEndpointURL string

	S3Bucket   string
	PresignTTL time.Duration

	SQSRequestQueueURL string
	SQSMessageGroupID  string

	RedisAddr     string
	RedisDB       int
	RedisPassword string
	RedisTTL      time.Duration

	LogLevel string
}

// Load reads environment variables and applies defaults.
func Load() (*Config, error) {
	cfg := &Config{
		HTTPAddr:           getEnv("HTTP_ADDR", ":8080"),
		AWSRegion:          getEnv("AWS_REGION", "us-east-1"),
		AWSEndpointURL:     os.Getenv("AWS_ENDPOINT_URL"),
		S3Bucket:           getEnv("S3_BUCKET", "taskforge-jobs"),
		SQSRequestQueueURL: os.Getenv("SQS_REQUEST_QUEUE_URL"),
		SQSMessageGroupID:  getEnv("SQS_MESSAGE_GROUP_ID", "request"),
		RedisPassword:      os.Getenv("REDIS_PASSWORD"),
		LogLevel:           getEnv("LOG_LEVEL", "info"),
	}

	presignTTL, err := getDuration("S3_PRESIGN_TTL", 10*time.Minute)
	if err != nil {
		return nil, err
	}
	cfg.PresignTTL = presignTTL

	redisTTL, err := getDuration("REDIS_TTL", time.Hour)
	if err != nil {
		return nil, err
	}
	cfg.RedisTTL = redisTTL

	cfg.RedisAddr = os.Getenv("REDIS_ADDR")
	if cfg.RedisAddr == "" {
		host := getEnv("REDIS_HOST", "127.0.0.1")
		port := getEnv("REDIS_PORT", "6379")
		cfg.RedisAddr = host + ":" + port
	}

	redisDB, err := getInt("REDIS_DB", 0)
	if err != nil {
		return nil, err
	}
	cfg.RedisDB = redisDB

	return cfg, nil
}

// Validate fails fast when required settings are missing.
func (c *Config) Validate() error {
	if c.SQSRequestQueueURL == "" {
		return errors.New("SQS_REQUEST_QUEUE_URL is required")
	}
	if c.S3Bucket == "" {
		return errors.New("S3_BUCKET is required")
	}
	return nil
}

func getEnv(key, fallback string) string {
	if value := os.Getenv(key); value != "" {
		return value
	}
	return fallback
}

func getInt(key string, fallback int) (int, error) {
	value := os.Getenv(key)
	if value == "" {
		return fallback, nil
	}
	parsed, err := strconv.Atoi(value)
	if err != nil {
		return 0, fmt.Errorf("parse %s: %w", key, err)
	}
	return parsed, nil
}

func getDuration(key string, fallback time.Duration) (time.Duration, error) {
	value := os.Getenv(key)
	if value == "" {
		return fallback, nil
	}
	parsed, err := time.ParseDuration(value)
	if err != nil {
		return 0, fmt.Errorf("parse %s: %w", key, err)
	}
	return parsed, nil
}
