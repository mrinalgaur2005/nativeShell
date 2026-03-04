#include "command/command.h"
#include "command/command_overlay.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <string.h>

static TTF_Font *font = NULL;
static char info_text[2048];

void cmd_overlay_init(void)
{
    if (!TTF_WasInit())
        TTF_Init();

    font = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf", 16);
    info_text[0] = '\0';
}

void cmd_overlay_set_info(const char *text)
{
    if (text)
        snprintf(info_text, sizeof(info_text), "%s", text);
    else
        info_text[0] = '\0';
}

void cmd_overlay_clear_info(void)
{
    info_text[0] = '\0';
}

static void render_line(SDL_Renderer *ren, const char *line, int line_len,
                        int x, int y, SDL_Color color)
{
    if (line_len <= 0 || !font)
        return;

    char buf[512];
    if (line_len >= (int)sizeof(buf))
        line_len = (int)sizeof(buf) - 1;
    memcpy(buf, line, (size_t)line_len);
    buf[line_len] = '\0';

    SDL_Surface *surf = TTF_RenderUTF8_Blended(font, buf, color);
    if (!surf)
        return;

    SDL_Texture *tex = SDL_CreateTextureFromSurface(ren, surf);
    SDL_Rect dst = { x, y, surf->w, surf->h };
    SDL_RenderCopy(ren, tex, NULL, &dst);
    SDL_FreeSurface(surf);
    SDL_DestroyTexture(tex);
}

static int count_lines(const char *text)
{
    if (!text || !text[0])
        return 0;

    int n = 0;
    for (const char *p = text; *p; p++)
        if (*p == '\n')
            n++;

    if (text[0] && text[strlen(text) - 1] != '\n')
        n++;

    return n;
}

void render_cmd_overlay(SDL_Renderer *ren, int win_w, int win_h)
{
    if (!cmd_active())
        return;

    int line_h = font ? TTF_FontLineSkip(font) : 20;
    int bar_h  = 40;

    /* --- Info panel above command bar --- */
    if (info_text[0] && font) {
        int nlines  = count_lines(info_text);
        int pad     = 12;
        int panel_h = nlines * line_h + pad * 2;
        int panel_y = win_h - bar_h - panel_h;

        SDL_Rect bg = { 0, panel_y, win_w, panel_h };
        SDL_SetRenderDrawColor(ren, 25, 25, 35, 235);
        SDL_RenderFillRect(ren, &bg);

        SDL_SetRenderDrawColor(ren, 70, 80, 100, 255);
        SDL_RenderDrawLine(ren, 0, panel_y, win_w, panel_y);

        SDL_Color fg = { 190, 200, 220, 255 };
        const char *p = info_text;
        int y = panel_y + pad;

        while (*p) {
            const char *eol = strchr(p, '\n');
            int seg = eol ? (int)(eol - p) : (int)strlen(p);

            if (seg > 0)
                render_line(ren, p, seg, 14, y, fg);

            y += line_h;
            p = eol ? eol + 1 : p + seg;
        }
    }

    /* --- Command bar (bottom) --- */
    const char *text = cmd_buffer();

    SDL_Rect bar = { 0, win_h - bar_h, win_w, bar_h };

    SDL_SetRenderDrawColor(ren, 20, 20, 20, 220);
    SDL_RenderFillRect(ren, &bar);

    SDL_SetRenderDrawColor(ren, 80, 80, 80, 255);
    SDL_RenderDrawRect(ren, &bar);

    char display[300];
    snprintf(display, sizeof(display), ":%s", text);

    if (font) {
        SDL_Color fg = { 220, 220, 220, 255 };
        SDL_Surface *surf = TTF_RenderUTF8_Blended(font, display, fg);
        if (surf) {
            SDL_Texture *tex = SDL_CreateTextureFromSurface(ren, surf);
            SDL_Rect dst = { 10, bar.y + 10, surf->w, surf->h };
            SDL_RenderCopy(ren, tex, NULL, &dst);
            SDL_FreeSurface(surf);
            SDL_DestroyTexture(tex);
        }
    }
}
