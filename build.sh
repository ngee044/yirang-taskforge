#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

# vcpkg 탐색 순서: VCPKG_ROOT > 형제 경로(../vcpkg) > ~/vcpkg
if [ -n "${VCPKG_ROOT:-}" ] && [ -f "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" ]; then
    TOOLCHAIN="${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
elif [ -f "../vcpkg/scripts/buildsystems/vcpkg.cmake" ]; then
    TOOLCHAIN="$(cd .. && pwd)/vcpkg/scripts/buildsystems/vcpkg.cmake"
elif [ -f "${HOME}/vcpkg/scripts/buildsystems/vcpkg.cmake" ]; then
    TOOLCHAIN="${HOME}/vcpkg/scripts/buildsystems/vcpkg.cmake"
else
    echo "error: vcpkg not found. set VCPKG_ROOT or clone vcpkg to ../vcpkg or ~/vcpkg" >&2
    exit 1
fi

if [ "${FRESH_DEPS:-0}" = "1" ]; then
    rm -rf build
fi

# macOS: CommandLineTools SDK의 libc++ 헤더가 Apple clang보다 새 버전이면
# (__builtin_clzg 미지원 등) 컴파일이 깨지므로 Xcode 내장 SDK를 sysroot로 강제한다
EXTRA_CMAKE_ARGS=()
if [ "$(uname -s)" = "Darwin" ]; then
    XCODE_SDK="$(xcode-select -p)/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
    if [ -d "${XCODE_SDK}" ]; then
        export SDKROOT="${XCODE_SDK}"
        export VCPKG_KEEP_ENV_VARS="SDKROOT${VCPKG_KEEP_ENV_VARS:+;${VCPKG_KEEP_ENV_VARS}}"
        EXTRA_CMAKE_ARGS+=("-DCMAKE_OSX_SYSROOT=${XCODE_SDK}")
    fi
fi

cmake -S . -B build -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="${TOOLCHAIN}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=OFF \
    "${EXTRA_CMAKE_ARGS[@]}"

cmake --build build -j
