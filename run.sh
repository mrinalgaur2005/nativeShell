#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

gcc \
  "$SCRIPT_DIR/src/main.c" \
  "$SCRIPT_DIR/src/layout.c" \
  "$SCRIPT_DIR/src/render.c" \
  "$SCRIPT_DIR/src/window.c" \
  "$SCRIPT_DIR/src/focus.c" \
  $(pkg-config --cflags --libs sdl2 SDL2_image) \
  -Wall -Wextra -O2 \
  -o "$SCRIPT_DIR/main"

"$SCRIPT_DIR/main"
