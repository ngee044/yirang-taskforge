#!/usr/bin/env bash
# usage: checksum.sh <inputs_dir> <outputs_dir> [extra args...]
# 각 입력 파일의 SHA-256 체크섬을 계산해 <파일명>.sha256.txt로 출력합니다.
set -euo pipefail

inputs_dir="$1"
outputs_dir="$2"

# ubuntu: sha256sum / macOS: shasum -a 256
if command -v sha256sum > /dev/null 2>&1; then
    hash_command() { sha256sum "$1"; }
else
    hash_command() { shasum -a 256 "$1"; }
fi

found=0
for input_file in "${inputs_dir}"/*; do
    [ -f "${input_file}" ] || continue
    found=1
    base_name="$(basename "${input_file}")"
    digest="$(hash_command "${input_file}" | awk '{print $1}')"
    echo "${digest}" > "${outputs_dir}/${base_name}.sha256.txt"
    echo "checksum: ${base_name} -> ${digest}"
done

if [ "${found}" -eq 0 ]; then
    echo "checksum: no input files" >&2
    exit 1
fi
