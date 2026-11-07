// Package redisrepo wraps Redis reads/writes for job status documents.
package redisrepo

import (
	"context"
	"errors"
	"fmt"
	"time"

	"github.com/redis/go-redis/v9"
)

const operationTimeout = 2 * time.Second

// Repository provides bounded-timeout access to Redis.
type Repository struct {
	client *redis.Client
}

// New creates a Repository connected to the Redis server at addr.
func New(addr string, db int, password string) *Repository {
	return &Repository{
		client: redis.NewClient(&redis.Options{
			Addr:     addr,
			DB:       db,
			Password: password,
			// Without this, go-redis ignores ctx deadlines for socket I/O and falls back to the
			// 3s default ReadTimeout, so operationTimeout would never take effect.
			ContextTimeoutEnabled: true,
		}),
	}
}

// Get returns the raw value and whether the key exists, separately from I/O errors.
func (r *Repository) Get(ctx context.Context, key string) (string, bool, error) {
	ctx, cancel := context.WithTimeout(ctx, operationTimeout)
	defer cancel()

	value, err := r.client.Get(ctx, key).Result()
	if errors.Is(err, redis.Nil) {
		return "", false, nil
	}
	if err != nil {
		return "", false, fmt.Errorf("redis get %s: %w", key, err)
	}
	return value, true, nil
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
