# yirang-taskforge Python 인터페이스 — FastAPI + uvicorn
FROM python:3.12-slim AS runtime

ENV PYTHONDONTWRITEBYTECODE=1 \
    PYTHONUNBUFFERED=1

WORKDIR /app

COPY pyproject.toml ./
COPY src ./src

RUN pip install --no-cache-dir . \
    && useradd --create-home --shell /usr/sbin/nologin taskforge

USER taskforge

EXPOSE 8081

CMD ["uvicorn", "app.main:app", "--host", "0.0.0.0", "--port", "8081", "--app-dir", "src"]
