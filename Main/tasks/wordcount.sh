#!/usr/bin/env bash
# usage: wordcount.sh <inputs_dir> <outputs_dir> [extra args...]
# 각 입력 파일의 단어 수를 세어 <파일명>.wordcount.txt로 출력합니다.
set -euo pipefail

inputs_dir="$1"
outputs_dir="$2"

found=0
for input_file in "${inputs_dir}"/*; do
    [ -f "${input_file}" ] || continue
    found=1
    base_name="$(basename "${input_file}")"
    word_count="$(wc -w < "${input_file}" | tr -d '[:space:]')"
    echo "${word_count}" > "${outputs_dir}/${base_name}.wordcount.txt"
    echo "wordcount: ${base_name} -> ${word_count} words"
done

if [ "${found}" -eq 0 ]; then
    echo "wordcount: no input files" >&2
    exit 1
fi
