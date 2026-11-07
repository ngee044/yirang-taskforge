#!/usr/bin/env bash
set -euo pipefail

tasks_dir="$1"
test_dir="$(mktemp -d)"
trap 'rm -rf "${test_dir}"' EXIT
mkdir -p "${test_dir}/inputs" "${test_dir}/wordcount" "${test_dir}/checksum"

printf 'hello world\n' > "${test_dir}/inputs/.hidden.txt"
for mode in hidden mixed; do
    if [ "${mode}" = mixed ]; then
        printf 'one two three\n' > "${test_dir}/inputs/plain.txt"
    fi
    bash "${tasks_dir}/wordcount.sh" "${test_dir}/inputs" "${test_dir}/wordcount"
    bash "${tasks_dir}/checksum.sh" "${test_dir}/inputs" "${test_dir}/checksum"
    test "$(cat "${test_dir}/wordcount/.hidden.txt.wordcount.txt")" = 2
    if command -v sha256sum >/dev/null 2>&1; then
        expected="$(sha256sum "${test_dir}/inputs/.hidden.txt" | awk '{print $1}')"
    else
        expected="$(shasum -a 256 "${test_dir}/inputs/.hidden.txt" | awk '{print $1}')"
    fi
    test "$(cat "${test_dir}/checksum/.hidden.txt.sha256.txt")" = "${expected}"
done
test "$(cat "${test_dir}/wordcount/plain.txt.wordcount.txt")" = 3
test -s "${test_dir}/checksum/plain.txt.sha256.txt"

mkdir "${test_dir}/empty"
for task in wordcount checksum; do
    if bash "${tasks_dir}/${task}.sh" "${test_dir}/empty" "${test_dir}/${task}"; then
        echo "${task} accepted empty input" >&2
        exit 1
    fi
done
