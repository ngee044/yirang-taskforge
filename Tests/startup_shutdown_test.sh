#!/usr/bin/env bash
set -euo pipefail

worker="$1"
test_dir="$(mktemp -d)"
worker_pid=""
cleanup() {
    if [ -n "${worker_pid}" ] && kill -0 "${worker_pid}" 2>/dev/null; then
        kill -KILL "${worker_pid}" 2>/dev/null || true
        wait "${worker_pid}" 2>/dev/null || true
    fi
    rm -rf "${test_dir}"
}
trap cleanup EXIT

cat > "${test_dir}/config.json" <<EOF
{"redis_host":"127.0.0.1","redis_port":1,"sqs_request_queue_url":"http://127.0.0.1:1/queue.fifo","s3_bucket":"test","task_whitelist":[{"name":"test","executable":"/bin/true"}],"write_file_log":0,"write_console_log":4,"write_interval":10,"dlq_path":"${test_dir}/dlq"}
EOF

"${worker}" --config_path "${test_dir}/config.json" > "${test_dir}/worker.log" 2>&1 &
worker_pid=$!
observed=0
for ((attempt=0; attempt<200; attempt++)); do
    if grep -q 'redis connect attempt' "${test_dir}/worker.log"; then
        observed=1
        break
    fi
    sleep 0.05
done
if [ "${observed}" -ne 1 ]; then
    cat "${test_dir}/worker.log"
    exit 1
fi

kill -TERM "${worker_pid}"
exit_code=0
wait "${worker_pid}" || exit_code=$?
worker_pid=""
if [ "${exit_code}" -ne 1 ]; then
    cat "${test_dir}/worker.log"
    echo "expected clean startup failure (1), got ${exit_code}" >&2
    exit 1
fi
grep -q 'TaskForgeWorker stopped' "${test_dir}/worker.log"
