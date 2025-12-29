
#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

gcc "$SCRIPT_DIR/src/main.c" \
    $(sdl2-config --cflags --libs) \
    -o "$SCRIPT_DIR/main"

"$SCRIPT_DIR/main"
