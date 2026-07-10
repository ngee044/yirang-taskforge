# yirang-taskforge Go 인터페이스 — 정적 빌드 + alpine 런타임
FROM golang:1.24-alpine AS builder

WORKDIR /src

COPY go.mod go.sum ./
RUN go mod download

COPY . .
RUN CGO_ENABLED=0 go build -ldflags="-s -w" -o /out/api ./cmd/api

FROM alpine:3.21 AS runtime

RUN apk add --no-cache ca-certificates \
    && adduser -D -H taskforge

COPY --from=builder /out/api /usr/local/bin/api

USER taskforge

EXPOSE 8080

ENTRYPOINT ["api"]
