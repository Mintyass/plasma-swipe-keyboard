#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

export QML_IMPORT_PATH=$PWD/build-release
export QT_VIRTUALKEYBOARD_LAYOUT_PATH=$PWD/layouts
export QT_IM_MODULE=qtvirtualkeyboard
export QT_QPA_PLATFORM=xcb
export QT_LOGGING_RULES="plasma.swipe.*=true;qt.virtualkeyboard.*=true"
export QT_FORCE_STDERR_LOGGING=1
export QT_QUICK_CONTROLS_STYLE=Basic

exec qml-qt6 test/testapp.qml 2>&1
