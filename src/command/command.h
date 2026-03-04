
#pragma once
#include "layout/layout.h"
#include <SDL2/SDL_events.h>
#include <stdbool.h>

void cmd_enter(void);
void cmd_exit(void);

void cmd_handle_key(SDL_KeyboardEvent *e);
void cmd_handle_text(const char *text);
bool cmd_execute(LayoutNode **root,LayoutNode **focused);
const char *cmd_take_profile_switch(void);
bool cmd_take_profile_show_request(void);
bool cmd_take_profiles_list_request(void);

const char *cmd_buffer(void);
bool cmd_active(void);
void cmd_reset_buffer(void);
