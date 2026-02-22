#include "pane_view.h"
#include "SDL_log.h"
#include "SDL_ttf.h"
#include <SDL2/SDL.h>
#include <stdlib.h>

static TTF_Font *pane_font = NULL;

static void pane_draw(View *self,
                      SDL_Renderer *r,
                      SDL_Rect rect,
                      bool focused);
static void pane_destroy(View *self);

static void draw_text_wrapped(SDL_Renderer *r,
                              const char *text,
                              int x, int y,
                              int max_w,
                              SDL_Color fg);
static void draw_text_simple(SDL_Renderer *r,
                             const char *text,
                             int x, int y,
                             SDL_Color fg);

void pane_view_init(void)
{
    pane_font = TTF_OpenFont(
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        14
    );

    if (!pane_font)
        SDL_Log("Pane font load failed: %s", TTF_GetError());
}

View *pane_view_create(void)
{
    PaneView *pv = calloc(1, sizeof(PaneView));
    pv->base.type = VIEW_PANE;
    pv->base.draw = pane_draw;
    pv->base.destroy = pane_destroy;
    pv->attached = NULL;
    return (View *)pv;
}

WebView *pane_view_get_attached(View *v)
{
    if (!v || v->type != VIEW_PANE)
        return NULL;
    return ((PaneView *)v)->attached;
}

void pane_view_attach(View *v, WebView *web)
{
    if (!v || v->type != VIEW_PANE)
        return;
    ((PaneView *)v)->attached = web;
}

WebView *pane_view_detach(View *v)
{
    if (!v || v->type != VIEW_PANE)
        return NULL;
    PaneView *pv = (PaneView *)v;
    WebView *old = pv->attached;
    pv->attached = NULL;
    return old;
}

static void pane_draw(View *self,
                      SDL_Renderer *r,
                      SDL_Rect rect,
                      bool focused)
{
    PaneView *pv = (PaneView *)self;
    if (pv->attached) {
        View *wv = (View *)pv->attached;
        wv->draw(wv, r, rect, focused);
        return;
    }

    SDL_SetRenderDrawColor(r, 20, 20, 22, 255);
    SDL_RenderFillRect(r, &rect);

    (void)focused;
    const char *msg = "No WebView attached - press y in TabView to attach one";
    SDL_Color fg = {160, 160, 160, 255};

    int w = 0;
    int h = 0;
    int max_w = rect.w - 24;
    int x = rect.x + 12;
    int y = rect.y + 12;

    if (pane_font &&
        TTF_SizeUTF8(pane_font, msg, &w, &h) == 0 &&
        w > 0 && w <= max_w)
    {
        int cx = rect.x + (rect.w - w) / 2;
        int cy = rect.y + (rect.h - h) / 2;
        draw_text_simple(r, msg, cx, cy, fg);
    } else {
        draw_text_wrapped(r, msg, x, y, max_w, fg);
    }
}

static void pane_destroy(View *self)
{
    free(self);
}

static void draw_text_wrapped(SDL_Renderer *r,
                              const char *text,
                              int x, int y,
                              int max_w,
                              SDL_Color fg)
{
    if (!pane_font || !text || !*text)
        return;

    SDL_Surface *s =
        TTF_RenderUTF8_Blended_Wrapped(pane_font, text, fg, (Uint32)max_w);
    if (!s) return;

    SDL_Texture *t = SDL_CreateTextureFromSurface(r, s);

    SDL_Rect dst = { x, y, s->w, s->h };
    SDL_RenderCopy(r, t, NULL, &dst);

    SDL_FreeSurface(s);
    SDL_DestroyTexture(t);
}

static void draw_text_simple(SDL_Renderer *r,
                             const char *text,
                             int x, int y,
                             SDL_Color fg)
{
    if (!pane_font || !text || !*text)
        return;

    SDL_Surface *s =
        TTF_RenderUTF8_Blended(pane_font, text, fg);
    if (!s) return;

    SDL_Texture *t = SDL_CreateTextureFromSurface(r, s);
    SDL_Rect dst = { x, y, s->w, s->h };
    SDL_RenderCopy(r, t, NULL, &dst);

    SDL_FreeSurface(s);
    SDL_DestroyTexture(t);
}
