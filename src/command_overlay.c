#include "command.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

static TTF_Font *font = NULL;

void cmd_overlay_init(void)
{
    if (!TTF_WasInit())
        TTF_Init();

    font = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf", 16);
}

void render_cmd_overlay(SDL_Renderer *ren, int win_w, int win_h)
{
    if (!cmd_active())
        return;

    const char *text = cmd_buffer();

    SDL_Rect bar = {
        .x = 0,
        .y = win_h - 40,
        .w = win_w,
        .h = 40
    };

    /* Background */
    SDL_SetRenderDrawColor(ren, 20, 20, 20, 220);
    SDL_RenderFillRect(ren, &bar);

    /* Border */
    SDL_SetRenderDrawColor(ren, 80, 80, 80, 255);
    SDL_RenderDrawRect(ren, &bar);

    /* Text */
    char display[300];
    snprintf(display, sizeof(display), ":%s", text);

    SDL_Color fg = {220, 220, 220, 255};
    SDL_Surface *surf =
        TTF_RenderUTF8_Blended(font, display, fg);

    SDL_Texture *tex =
        SDL_CreateTextureFromSurface(ren, surf);

    SDL_Rect dst = {
        .x = 10,
        .y = bar.y + 10,
        .w = surf->w,
        .h = surf->h
    };

    SDL_RenderCopy(ren, tex, NULL, &dst);

    SDL_FreeSurface(surf);
    SDL_DestroyTexture(tex);
}
