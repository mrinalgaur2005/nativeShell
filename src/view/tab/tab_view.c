#include "tab_view.h"
#include "SDL_keycode.h"
#include "SDL_log.h"
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

/* ---------- create ---------- */


typedef struct {
    View base;
    int selected;
    TabAction action;
} TabView;

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

    SDL_SetRenderDrawColor(r, 22, 22, 22, 255);
    SDL_RenderFillRect(r, &rect);

    int y = rect.y + 10;
    int line_h = 22;

    for (int i = 0; i < tab_registry_count(); i++) {
        WebViewEntry *e = tab_registry_get(i);
        if (!e) continue;

        if (i == tv->selected) {
            SDL_SetRenderDrawColor(r, 60, 60, 140, 255);
            SDL_Rect sel = { rect.x, y - 2, rect.w, line_h };
            SDL_RenderFillRect(r, &sel);
        }

        /* placeholder text marker */
        SDL_SetRenderDrawColor(r, 200, 200, 200, 255);
        SDL_RenderDrawLine(r,
            rect.x + 10, y + line_h / 2,
            rect.x + rect.w - 10, y + line_h / 2);

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
