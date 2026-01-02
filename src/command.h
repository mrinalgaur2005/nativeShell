
#pragma once
#include "layout.h"
#include <SDL2/SDL_events.h>
#include <stdbool.h>

void cmd_enter(void);
void cmd_exit(void);

void cmd_handle_key(SDL_KeyboardEvent *e);
void cmd_handle_text(const char *text);
bool cmd_execute(LayoutNode *focused);

const char *cmd_buffer(void);
bool cmd_active(void);
