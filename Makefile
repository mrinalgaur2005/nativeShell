
CC      := gcc
TARGET  := main

SRC_DIR := src

SRCS := \
	$(SRC_DIR)/core/main.c \
	$(SRC_DIR)/layout/layout.c \
	$(SRC_DIR)/render/render.c \
	$(SRC_DIR)/core/window.c \
	$(SRC_DIR)/core/focus.c \
	$(SRC_DIR)/view/placeholder/placeholder_view.c \
	$(SRC_DIR)/view/debug/debug_view.c \
	$(SRC_DIR)/view/view.c \
	$(SRC_DIR)/view/web/web_view.c \
	$(SRC_DIR)/view/web/webview_registry.c \
	$(SRC_DIR)/view/tab/tab_view.c \
	$(SRC_DIR)/view/tab/tab_view_renderer.c \
	$(SRC_DIR)/command/command.c \
	$(SRC_DIR)/command/command_overlay.c \
	$(SRC_DIR)/core/session.c \
	$(SRC_DIR)/third_party/cjson/cJSON.c\
	$(SRC_DIR)/config/config.c

CFLAGS := -Wall -Wextra -O2 -Isrc
PKGS   := sdl2 SDL2_image SDL2_ttf gtk+-3.0 webkit2gtk-4.1

CFLAGS += $(shell pkg-config --cflags $(PKGS))
LIBS   := $(shell pkg-config --libs $(PKGS))

$(TARGET): $(SRCS)
	$(CC) $(SRCS) $(CFLAGS) $(LIBS) -o $(TARGET)

run: $(TARGET)
	GDK_BACKEND=x11 \
	WEBKIT_DISABLE_COMPOSITING_MODE=1 \
	WEBKIT_DISABLE_SANDBOX=1 \
	./$(TARGET)

clean:
	rm -f $(TARGET)
