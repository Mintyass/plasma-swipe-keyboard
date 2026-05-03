#!/bin/bash
# KWin spawns this when a Wayland client needs an input method. We inject our
# Qt VKB plugin and layout paths, then exec the real plasma-keyboard.
#
# Restore the upstream daemon by reverting the kwinrc InputMethod key:
#   kwriteconfig6 --file kwinrc --group Wayland --key 'InputMethod[$e]' \
#     /usr/share/applications/org.kde.plasma.keyboard.desktop
#   qdbus6 org.kde.KWin /KWin reconfigure

REPO=/home/cmintzias/Projects/plasma-swipe-keyboard
LOG=/tmp/swipe-im.log

export QT_VIRTUALKEYBOARD_LAYOUT_PATH="$REPO/layouts"
export QML_IMPORT_PATH="$REPO/build-release${QML_IMPORT_PATH:+:$QML_IMPORT_PATH}"
export QT_PLUGIN_PATH="$REPO/build-release${QT_PLUGIN_PATH:+:$QT_PLUGIN_PATH}"
export PLASMA_SWIPE_LOG=1
# plasma-keyboard's main.cpp overrides QT_VIRTUALKEYBOARD_LAYOUT_PATH with its
# system layout dir unless this is set, which would silently load the upstream
# QWERTY layout instead of ours.
export PLASMA_KEYBOARD_USE_QT_LAYOUTS=1

{
  echo "=== $(date -Iseconds) swipe-im-wrapper starting (pid $$) ==="
  echo "QT_VIRTUALKEYBOARD_LAYOUT_PATH=$QT_VIRTUALKEYBOARD_LAYOUT_PATH"
  echo "QML_IMPORT_PATH=$QML_IMPORT_PATH"
  echo "args: $*"
} >> "$LOG" 2>&1

exec /usr/bin/plasma-keyboard "$@" >> "$LOG" 2>&1
