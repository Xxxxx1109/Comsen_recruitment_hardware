#!/usr/bin/env bash
set -euo pipefail
project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
build_type="${1:-Release}"
case "${build_type}" in Debug|Release|RelWithDebInfo|MinSizeRel) ;; *) echo "用法：$0 [Debug|Release|RelWithDebInfo|MinSizeRel]" >&2; exit 2;; esac
if [[ -n "${ARM_GNU_TOOLCHAIN_HOME:-}" ]]; then
    export PATH="${ARM_GNU_TOOLCHAIN_HOME}/bin:${PATH}"
fi
for command in arm-none-eabi-gcc cmake ninja; do
    command -v "${command}" >/dev/null 2>&1 || { echo "缺少 ${command}，请安装或加入 PATH。" >&2; exit 1; }
done
cmake -S "${project_dir}" -B "${project_dir}/build/${build_type}" -G Ninja -DCMAKE_BUILD_TYPE="${build_type}"
cmake --build "${project_dir}/build/${build_type}"
