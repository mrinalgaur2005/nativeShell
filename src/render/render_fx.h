#pragma once
#include "layout/layout.h"
#include <SDL2/SDL.h>
#include <stdbool.h>

#define OUTER_GAP     12
#define INNER_GAP     10
#define BORDER_RADIUS 8
#define SHADOW_LAYERS 4

#define FX_BG_R 30
#define FX_BG_G 30
#define FX_BG_B 30

typedef struct {
    int   id;
    bool  active;

    float opacity;
    float target_opacity;

    float scale;
    float target_scale;

    float border_width;
    float target_border_width;

    SDL_Color border_color;
    SDL_Color target_border_color;

    SDL_Rect last_rect;

    Uint32 spawn_start;
} LeafFx;

typedef struct {
    SDL_Rect rect;
    Uint32   start;
    bool     active;
} ClosingGhost;

void      render_fx_pre_frame(LayoutNode *root, LayoutNode *focused);
LeafFx   *render_fx_get(int leaf_id);
int       render_fx_closing_count(void);
void      render_fx_draw_closing(SDL_Renderer *r);
