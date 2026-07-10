// Package redisrepo wraps Redis reads/writes for job status documents.
package redisrepo

import (
	"context"
	"errors"
	"fmt"
	"time"

	"github.com/redis/go-redis/v9"
)

// ErrNotFound marks a missing key so callers can map it to HTTP 404.
var ErrNotFound = errors.New("redis: key not found")

const operationTimeout = 2 * time.Second

// Repository provides bounded-timeout access to Redis.
type Repository struct {
	client *redis.Client
}

func New(addr string, db int, password string) *Repository {
	return &Repository{
		client: redis.NewClient(&redis.Options{
			Addr:     addr,
			DB:       db,
			Password: password,
		}),
	}
}

// Get returns the raw string value stored at key.
func (r *Repository) Get(ctx context.Context, key string) (string, error) {
	ctx, cancel := context.WithTimeout(ctx, operationTimeout)
	defer cancel()

	value, err := r.client.Get(ctx, key).Result()
	if errors.Is(err, redis.Nil) {
		return "", ErrNotFound
	}
	if err != nil {
		return "", fmt.Errorf("redis get %s: %w", key, err)
	}
	return value, nil
}

// Set stores a raw string value with the given TTL (0 keeps the key forever).
func (r *Repository) Set(ctx context.Context, key, value string, ttl time.Duration) error {
	ctx, cancel := context.WithTimeout(ctx, operationTimeout)
	defer cancel()

	if err := r.client.Set(ctx, key, value, ttl).Err(); err != nil {
		return fmt.Errorf("redis set %s: %w", key, err)
	}
	return nil
}

// Ping verifies connectivity.
func (r *Repository) Ping(ctx context.Context) error {
	ctx, cancel := context.WithTimeout(ctx, operationTimeout)
	defer cancel()

	if err := r.client.Ping(ctx).Err(); err != nil {
		return fmt.Errorf("redis ping: %w", err)
	}
	return nil
}

// Close releases the underlying connection pool.
func (r *Repository) Close() error {
	return r.client.Close()
}
