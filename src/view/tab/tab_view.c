#include "tab_view.h"
#include "SDL_keycode.h"
#include "SDL_log.h"
#include "SDL_ttf.h"
#include "layout/layout.h"
#include "view/web/web_view.h"
#include "view/web/webview_registry.h"
#include <SDL2/SDL.h>
#include <stdlib.h>


/* ---------- forward ---------- */
static void tab_draw(View *self,
                     SDL_Renderer *r,
                     SDL_Rect rect,
                     bool focused);

static void tab_key(View *self, SDL_KeyboardEvent *e);
static void tab_destroy(View *self);

static const char *tab_label(WebViewEntry *e);

static void draw_text(SDL_Renderer *r,
                      const char *text,
                      int x, int y,
                      int max_w);
/* ---------- create ---------- */


static TTF_Font *tab_font = NULL;

void tab_view_init(void)
{
    tab_font = TTF_OpenFont(
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        14
    );

    if (!tab_font)
        SDL_Log("TTF font load failed: %s", TTF_GetError());
}

typedef struct {
    View base;
    int selected;
    TabAction action;
} TabView;
/*-----------------helpers----------*/
static const char *tab_label(WebViewEntry *e)
{
    if (e->title[0])
        return e->title;

    if (e->url[0])
        return e->url;

    return "(untitled)";
}
static void draw_text(SDL_Renderer *r,
                      const char *text,
                      int x, int y,
                      int max_w)
{
    if (!tab_font || !text || !*text)
        return;

    SDL_Color fg = {255, 255, 255, 255};

    SDL_Surface *s =
        TTF_RenderUTF8_Blended(tab_font, text, fg);
    if (!s) return;

    SDL_Texture *t = SDL_CreateTextureFromSurface(r, s);

    SDL_Rect dst = { x, y, s->w, s->h };
    if (dst.w > max_w)
        dst.w = max_w;

    SDL_RenderCopy(r, t, NULL, &dst);

    SDL_FreeSurface(s);
    SDL_DestroyTexture(t);
}
/* ---------------- create ---------------- */

View *tab_view_create(void)
{
    TabView *tv = calloc(1, sizeof(TabView));
    tv->base.type = VIEW_TAB;
    tv->base.draw = tab_draw;
    tv->base.destroy = tab_destroy;
    tv->selected = 0;
    tv->action = TAB_NONE;
    return (View *)tv;
}

/* ---------------- input ---------------- */

void tab_view_handle_key(View *v, SDL_KeyboardEvent *e)
{
    TabView *tv = (TabView *)v;

    if (e->type != SDL_KEYDOWN || e->repeat)
        return;

    int n = tab_registry_count();
    if (n == 0)
        return;

    switch (e->keysym.sym) {

        case SDLK_j:
            if (tv->selected < n - 1)
                tv->selected++;
            break;

        case SDLK_k:
            if (tv->selected > 0)
                tv->selected--;
            break;

        case SDLK_y:
        case SDLK_RETURN:
            tv->action = TAB_ATTACH;
            break;

        case SDLK_x:
            tv->action = TAB_CLOSE;
            break;

        case SDLK_ESCAPE:
            SDL_Log("Inside tab_view for esc");
            tv->action = TAB_EXIT;
            break;
    }
}

/* ---------------- intent ---------------- */

TabAction tab_view_take_action(View *v)
{
    TabView *tv = (TabView *)v;
    TabAction a = tv->action;
    tv->action = TAB_NONE;
    return a;
}

int tab_view_selected(View *v)
{
    return ((TabView *)v)->selected;

}
/* ---------- render ---------- */
static void tab_draw(View *self,
                     SDL_Renderer *r,
                     SDL_Rect rect,
                     bool focused)
{
    TabView *tv = (TabView *)self;

    /* background */
    SDL_SetRenderDrawColor(r, 24, 24, 24, 255);
    SDL_RenderFillRect(r, &rect);

    int y = rect.y + 8;
    int line_h = 28;

    for (int i = 0; i < tab_registry_count(); i++) {

        WebViewEntry *e = tab_registry_get(i);
        if (!e || !e->alive || !e->view)
            continue;

        /* selected row */
        if (i == tv->selected) {
            SDL_SetRenderDrawColor(r, 60, 90, 160, 255);
            SDL_Rect sel = {
                rect.x,
                y - 2,
                rect.w,
                line_h
            };
            SDL_RenderFillRect(r, &sel);
        }

        /* title (fallback to URL) */
        const char *title = web_view_get_title(e->view);
        SDL_Log("HEY TITLE IS %s",title);
        if (!title || !*title)
            title = web_view_get_url(e->view);

        if (!title)
            title = "(untitled)";

        draw_text(
            r,
            title,
            rect.x + 28,
            y + 5,
            rect.w - 36
        );

        y += line_h;
    }
}
/* ---------- destroy ---------- */

static void tab_destroy(View *self)
{
    free(self);
}
void tab_view_close_selected(LayoutNode *root, View *v)
{
    TabView *tv = (TabView *)v;

    WebViewEntry *e = tab_registry_get(tv->selected);
    if (!e || !e->alive)
        return;

    layout_detach_view(root, e->view);
    web_view_close(e->view);
    
    if (tv->selected >= tab_registry_count())
        tv->selected = tab_registry_count() - 1;
}
