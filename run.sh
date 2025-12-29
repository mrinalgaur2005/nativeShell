#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

gcc "$SCRIPT_DIR/src/main.c" \
    $(pkg-config --cflags --libs sdl2 SDL2_image) \
    -o "$SCRIPT_DIR/main"

"$SCRIPT_DIR/main"
