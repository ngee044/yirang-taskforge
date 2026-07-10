#!/usr/bin/env bash
# yirang-taskforge E2E 테스트
# 사전 조건: docker compose up -d 로 전체 스택이 기동된 상태
# 검증 내용: 3개 인터페이스(go/python/js) 각각에 대해
#   작업 등록(202) → presigned PUT 업로드 → 상태 폴링(done) → 결과 다운로드/내용 검증
#   + 미등록 태스크의 failed 전이 + task catalog 노출
set -uo pipefail

cd "$(dirname "$0")"

HOST="localhost"
APIS=("go:8080" "python:8081" "js:8082")
POLL_ATTEMPTS=45
POLL_INTERVAL=2

PASS_COUNT=0
FAIL_COUNT=0

log()  { printf '[e2e] %s\n' "$*"; }
pass() { PASS_COUNT=$((PASS_COUNT + 1)); log "PASS: $*"; }
fail() { FAIL_COUNT=$((FAIL_COUNT + 1)); log "FAIL: $*"; }

# 컨테이너 내부 호스트명(localstack)을 호스트에서 접근 가능한 주소로 치환
rewrite_url() {
    printf '%s' "$1" | sed -e "s/localstack:4566/${HOST}:4566/"
}

json_get() {
    # usage: json_get '<json>' '<python expression over j>'
    python3 -c "import json,sys; j=json.loads(sys.argv[1]); print(${2})" "$1" 2>/dev/null
}

wait_ready() {
    local port="$1"
    for _ in $(seq 1 60); do
        if curl -sf "http://${HOST}:${port}/readyz" 2>/dev/null | grep -q '"ready"'; then
            return 0
        fi
        sleep 2
    done
    return 1
}

poll_status() {
    # 터미널 상태(done/failed)가 될 때까지 폴링하고 마지막 문서를 출력
    local port="$1" job_id="$2"
    local body status
    for _ in $(seq 1 "${POLL_ATTEMPTS}"); do
        body="$(curl -sf "http://${HOST}:${port}/api/v1/jobs/${job_id}")" || body=""
        if [ -n "${body}" ]; then
            status="$(json_get "${body}" "j['data']['status']")"
            if [ "${status}" = "done" ] || [ "${status}" = "failed" ]; then
                printf '%s' "${body}"
                return 0
            fi
        fi
        sleep "${POLL_INTERVAL}"
    done
    printf '%s' "${body}"
    return 1
}

test_api() {
    local name="$1" port="$2"
    log "=== ${name} API (:${port}) ==="

    if ! wait_ready "${port}"; then
        fail "${name}: /readyz not ready"
        return
    fi
    pass "${name}: /readyz ready"

    # task catalog
    local catalog
    catalog="$(curl -sf "http://${HOST}:${port}/api/v1/tasks")"
    if printf '%s' "${catalog}" | grep -q 'wordcount'; then
        pass "${name}: task catalog exposes wordcount"
    else
        fail "${name}: task catalog missing wordcount: ${catalog}"
    fi

    # 작업 등록
    local created http_code response
    response="$(curl -s -w '\n%{http_code}' -X POST "http://${HOST}:${port}/api/v1/jobs" \
        -H 'Content-Type: application/json' \
        -d '{"task_name":"wordcount","input_files":[{"filename":"hello.txt"}]}')"
    http_code="$(printf '%s' "${response}" | tail -1)"
    created="$(printf '%s' "${response}" | sed '$d')"

    if [ "${http_code}" != "202" ]; then
        fail "${name}: POST /jobs expected 202, got ${http_code}: ${created}"
        return
    fi
    pass "${name}: POST /jobs returned 202"

    local job_id upload_url
    job_id="$(json_get "${created}" "j['data']['job_id']")"
    upload_url="$(json_get "${created}" "j['data']['upload_urls'][0]['upload_url']")"
    if [ -z "${job_id}" ] || [ -z "${upload_url}" ]; then
        fail "${name}: cannot parse job_id/upload_url: ${created}"
        return
    fi

    # presigned PUT 업로드 (5 단어)
    local payload_file
    payload_file="$(mktemp)"
    printf 'hello world from yirang taskforge' > "${payload_file}"
    if curl -sf -X PUT --data-binary "@${payload_file}" "$(rewrite_url "${upload_url}")" > /dev/null; then
        pass "${name}: input uploaded via presigned PUT"
    else
        fail "${name}: presigned PUT failed"
        rm -f "${payload_file}"
        return
    fi
    rm -f "${payload_file}"

    # 상태 폴링
    local final status
    final="$(poll_status "${port}" "${job_id}")"
    status="$(json_get "${final}" "j['data']['status']")"
    if [ "${status}" != "done" ]; then
        fail "${name}: job did not complete (status=${status}): ${final}"
        return
    fi
    pass "${name}: job reached done"

    # 결과 다운로드 및 내용 검증 (5 단어 → "5")
    local download_url result
    download_url="$(json_get "${final}" "j['data']['result_download_url'][0]['download_url']")"
    if [ -z "${download_url}" ]; then
        fail "${name}: result_download_url missing: ${final}"
        return
    fi
    result="$(curl -sf "$(rewrite_url "${download_url}")" | tr -d '[:space:]')"
    if [ "${result}" = "5" ]; then
        pass "${name}: result content verified (wordcount=5)"
    else
        fail "${name}: unexpected result content: '${result}'"
    fi
}

test_unregistered_task() {
    # 미등록 태스크는 재시도 소진 후 failed 로 전이해야 한다 (go API로 1회 확인)
    local port=8080
    log "=== failure case (unregistered task) ==="

    local response created http_code job_id final status
    response="$(curl -s -w '\n%{http_code}' -X POST "http://${HOST}:${port}/api/v1/jobs" \
        -H 'Content-Type: application/json' \
        -d '{"task_name":"no-such-task"}')"
    http_code="$(printf '%s' "${response}" | tail -1)"
    created="$(printf '%s' "${response}" | sed '$d')"

    if [ "${http_code}" != "202" ]; then
        fail "unregistered: POST expected 202, got ${http_code}"
        return
    fi

    job_id="$(json_get "${created}" "j['data']['job_id']")"
    final="$(poll_status "${port}" "${job_id}")"
    status="$(json_get "${final}" "j['data']['status']")"
    if [ "${status}" = "failed" ]; then
        pass "unregistered task transitioned to failed"
    else
        fail "unregistered task expected failed, got ${status}: ${final}"
    fi
}

for entry in "${APIS[@]}"; do
    name="${entry%%:*}"
    port="${entry##*:}"
    test_api "${name}" "${port}"
done

test_unregistered_task

log "==============================="
log "PASS=${PASS_COUNT} FAIL=${FAIL_COUNT}"
if [ "${FAIL_COUNT}" -gt 0 ]; then
    exit 1
fi
log "E2E ALL GREEN"
