#pragma once
#include <SDL2/SDL_render.h>

void cmd_overlay_init(void);
void render_cmd_overlay(SDL_Renderer *ren, int win_w, int win_h);

void cmd_overlay_set_info(const char *text);
void cmd_overlay_clear_info(void);
