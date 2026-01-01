#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

gcc \
  "$SCRIPT_DIR/src/main.c" \
  "$SCRIPT_DIR/src/layout.c" \
  "$SCRIPT_DIR/src/render.c" \
  "$SCRIPT_DIR/src/window.c" \
  "$SCRIPT_DIR/src/focus.c" \
  "$SCRIPT_DIR/src/placedholderview.c" \
  "$SCRIPT_DIR/src/debug_view.c" \
  "$SCRIPT_DIR/src/view.c" \
  "$SCRIPT_DIR/src/web_view.c" \
  $(pkg-config --cflags --libs sdl2 SDL2_image gtk+-3.0 webkit2gtk-4.1) \
  -Wall -Wextra -O2 \
  -o "$SCRIPT_DIR/main"

GDK_BACKEND=x11 \
WEBKIT_DISABLE_COMPOSITING_MODE=1 \
WEBKIT_DISABLE_SANDBOX=1 \
"$SCRIPT_DIR/main"
