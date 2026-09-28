#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/../build/linux-preview"
APP="${BUILD_DIR}/XVision/XVision"

if [[ ! -x "${APP}" ]]; then
    echo "Linux preview executable not found: ${APP}" >&2
    echo "Configure and build with XVISION_BUILD_SYSTEM_PLUGIN=OFF first." >&2
    exit 1
fi

LIB_DIRS=(
    "${BUILD_DIR}/CommonUsing/Project/XConcurrent"
    "${BUILD_DIR}/CommonUsing/Project/XFlowGraphics"
    "${BUILD_DIR}/CommonUsing/Project/XLanguage"
    "${BUILD_DIR}/CommonUsing/Project/XLog"
    "${BUILD_DIR}/CommonUsing/Project/XWidget/XWidget"
    "${BUILD_DIR}/XvCamera"
    "${BUILD_DIR}/XvCore"
    "${BUILD_DIR}/XvData"
    "${BUILD_DIR}/XvDisplay"
    "${BUILD_DIR}/XvTokenMsg"
    "${BUILD_DIR}/XvUtils"
    "${BUILD_DIR}/XVision"
)

LIB_PATH="$(IFS=:; echo "${LIB_DIRS[*]}")"
LIB_PATH="${LIB_PATH}${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"
XVISION_XVFB_ARGS="${XVISION_XVFB_ARGS:--screen 0 1920x1080x24}"

exec xvfb-run -a -s "${XVISION_XVFB_ARGS}" \
    env QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-xcb}" \
        LD_LIBRARY_PATH="${LIB_PATH}" \
        "${APP}" "$@"
