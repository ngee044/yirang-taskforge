# yirang-taskforge JS 인터페이스 — NestJS(tsc) 멀티스테이지
FROM node:22-alpine AS builder

WORKDIR /src

COPY package.json package-lock.json tsconfig.json tsconfig.build.json ./
RUN npm ci

COPY src ./src
RUN npm run build

FROM node:22-alpine AS runtime

WORKDIR /app

COPY package.json package-lock.json ./
RUN npm ci --omit=dev && npm cache clean --force

COPY --from=builder /src/dist ./dist

RUN adduser -D -H taskforge
USER taskforge

EXPOSE 8082

CMD ["node", "dist/main.js"]
