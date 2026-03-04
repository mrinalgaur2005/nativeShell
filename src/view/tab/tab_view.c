#include "tab_view.h"
#include "SDL_keycode.h"
#include "SDL_log.h"
#include "SDL_ttf.h"
#include "layout/layout.h"
#include "view/pane/pane_view.h"
#include "view/web/web_view.h"
#include "view/web/webview_registry.h"
#include <SDL2/SDL.h>
#include <stdlib.h>


/* ---------- forward ---------- */
static void tab_draw(View *self,
                     SDL_Renderer *r,
                     SDL_Rect rect,
                     bool focused);

static void tab_destroy(View *self);

static const char *tab_label(TabEntry *e);

static void draw_text(SDL_Renderer *r,
                      const char *text,
                      int x, int y,
                      int max_w,
                      SDL_Color fg);
static void fit_text(const char *text,
                     int max_w,
                     char *out,
                     size_t out_sz);
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
    TabAction action;
    int count_prefix;
} TabView;
/*-----------------helpers----------*/
static const char *tab_label(TabEntry *e)
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
                      int max_w,
                      SDL_Color fg)
{
    if (!tab_font || !text || !*text)
        return;

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

static void fit_text(const char *text,
                     int max_w,
                     char *out,
                     size_t out_sz)
{
    if (!out || out_sz == 0) return;
    out[0] = '\0';

    if (!tab_font || !text || !*text || max_w <= 0)
        return;

    int w = 0;
    int h = 0;
    if (TTF_SizeUTF8(tab_font, text, &w, &h) == 0 && w <= max_w) {
        strncpy(out, text, out_sz - 1);
        out[out_sz - 1] = '\0';
        return;
    }

    const char *ellipsis = "...";
    int ell_w = 0;
    if (TTF_SizeUTF8(tab_font, ellipsis, &ell_w, &h) != 0)
        return;

    if (ell_w > max_w) {
        strncpy(out, ellipsis, out_sz - 1);
        out[out_sz - 1] = '\0';
        return;
    }

    int target_w = max_w - ell_w;
    size_t len = strlen(text);
    size_t best = 0;
    char tmp[512];

    for (size_t i = 0; i < len; i++) {
        size_t copy = i + 1;
        if (copy >= sizeof(tmp))
            copy = sizeof(tmp) - 1;
        memcpy(tmp, text, copy);
        tmp[copy] = '\0';
        if (TTF_SizeUTF8(tab_font, tmp, &w, &h) != 0)
            break;
        if (w <= target_w)
            best = copy;
        else
            break;
    }

    if (best == 0) {
        strncpy(out, ellipsis, out_sz - 1);
        out[out_sz - 1] = '\0';
        return;
    }

    if (best >= out_sz)
        best = out_sz - 1;

    memcpy(out, text, best);
    out[best] = '\0';
    strncat(out, ellipsis, out_sz - strlen(out) - 1);
}
/* ---------------- create ---------------- */

View *tab_view_create(void)
{
    TabView *tv = calloc(1, sizeof(TabView));
    tv->base.type = VIEW_TAB;
    tv->base.draw = tab_draw;
    tv->base.destroy = tab_destroy;
    tv->action = TAB_NONE;
    return (View *)tv;
}

/* ---------------- input ---------------- */

void tab_view_handle_key(View *v, SDL_KeyboardEvent *e)
{
    TabView *tv = (TabView *)v;

    if (e->type != SDL_KEYDOWN || e->repeat)
        return;

    int n = tab_manager_count();

    if (e->keysym.sym >= SDLK_0 && e->keysym.sym <= SDLK_9) {
        int digit = e->keysym.sym - SDLK_0;
        tv->count_prefix = tv->count_prefix * 10 + digit;
        return;
    }

    switch (e->keysym.sym) {

        case SDLK_j:
            if (n > 0) {
                int step = tv->count_prefix ? tv->count_prefix : 1;
                int sel = tab_manager_selected();
                if (sel < 0) sel = 0;
                sel += step;
                if (sel > n - 1) sel = n - 1;
                tab_manager_set_selected(sel);
            }
            tv->count_prefix = 0;
            break;

        case SDLK_k:
            if (n > 0) {
                int step = tv->count_prefix ? tv->count_prefix : 1;
                int sel = tab_manager_selected();
                if (sel < 0) sel = 0;
                sel -= step;
                if (sel < 0) sel = 0;
                tab_manager_set_selected(sel);
            }
            tv->count_prefix = 0;
            break;

        case SDLK_y:
        case SDLK_RETURN:
            tv->action = TAB_ATTACH;
            tv->count_prefix = 0;
            break;

        case SDLK_x:
            tv->action = TAB_CLOSE;
            tv->count_prefix = 0;
            break;

        case SDLK_ESCAPE:
            SDL_Log("Inside tab_view for esc");
            tv->action = TAB_EXIT;
            tv->count_prefix = 0;
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
    (void)v;
    return tab_manager_selected();
}
/* ---------- render ---------- */
static void tab_draw(View *self,
                     SDL_Renderer *r,
                     SDL_Rect rect,
                     bool focused)
{
    (void)self;
    (void)focused;

    /* background */
    SDL_SetRenderDrawColor(r, 20, 20, 22, 255);
    SDL_RenderFillRect(r, &rect);

    int y = rect.y + 10;
    int line_h = 30;
    int icon_size = line_h - 10;
    int icon_pad = 6;
    int text_pad = 8;
    int indicator_gap = 6;
    SDL_Color fg = {230, 230, 230, 255};
    SDL_Color muted = {150, 150, 150, 255};
    SDL_Color indicator_fg = {120, 160, 255, 255};

    int selected = tab_manager_selected();
    if (selected < 0 && tab_manager_count() > 0) {
        tab_manager_set_selected(0);
        selected = 0;
    }

    for (int i = 0; i < tab_manager_count(); i++) {

        TabEntry *e = tab_manager_entry_at(i);
        WebView *web = tab_manager_webview_at(i);
        if (!e || !web)
            continue;

        /* row background */
        if (i % 2 == 0) {
            SDL_SetRenderDrawColor(r, 24, 24, 28, 255);
            SDL_Rect row = { rect.x, y - 2, rect.w, line_h };
            SDL_RenderFillRect(r, &row);
        }

        /* selected row */
        if (i == selected) {
            SDL_SetRenderDrawColor(r, 42, 64, 108, 255);
            SDL_Rect sel = {
                rect.x,
                y - 2,
                rect.w,
                line_h
            };
            SDL_RenderFillRect(r, &sel);

            SDL_SetRenderDrawColor(r, 98, 142, 230, 255);
            SDL_Rect accent = { rect.x, y - 2, 3, line_h };
            SDL_RenderFillRect(r, &accent);
        }

        /* favicon */
        if (!e->icon && !e->favicon_requested)
            tab_manager_request_favicon(web);

        int icon_x = rect.x + icon_pad;
        int icon_y = y + (line_h - icon_size) / 2;
        if (e->icon) {
            int tex_w = 0;
            int tex_h = 0;
            if (SDL_QueryTexture(e->icon, NULL, NULL, &tex_w, &tex_h) == 0 &&
                tex_w > 0 && tex_h > 0)
            {
                int max_side = tex_w > tex_h ? tex_w : tex_h;
                float scale = (float)icon_size / (float)max_side;
                int dst_w = (int)(tex_w * scale);
                int dst_h = (int)(tex_h * scale);
                SDL_Rect dst = {
                    icon_x + (icon_size - dst_w) / 2,
                    icon_y + (icon_size - dst_h) / 2,
                    dst_w,
                    dst_h
                };
                SDL_RenderCopy(r, e->icon, NULL, &dst);
            }
        }

        /* title (fallback to URL) */
        bool has_title = e->title[0] != '\0';
        const char *title = tab_label(e);

        int text_x = rect.x + icon_pad + icon_size + text_pad;
        int text_max_w = rect.w - (text_x - rect.x) - text_pad;
        if (text_max_w < 0)
            text_max_w = 0;

        const char *indicator = e->loading ? "..." : NULL;
        int ind_w = 0;
        int ind_h = 0;
        if (indicator && tab_font)
            TTF_SizeUTF8(tab_font, indicator, &ind_w, &ind_h);

        int title_max_w = text_max_w;
        if (indicator && ind_w > 0)
            title_max_w = text_max_w - (ind_w + indicator_gap);
        if (title_max_w < 0)
            title_max_w = 0;

        char title_buf[512];
        fit_text(title, title_max_w, title_buf, sizeof(title_buf));

        draw_text(
            r,
            title_buf,
            text_x,
            y + 6,
            title_max_w,
            has_title ? fg : muted
        );

        if (indicator && ind_w > 0) {
            int ind_x = rect.x + rect.w - text_pad - ind_w;
            if (ind_x > text_x + 4) {
                draw_text(
                    r,
                    indicator,
                    ind_x,
                    y + 6,
                    ind_w,
                    indicator_fg
                );
            }
        }

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
    (void)v;
    int sel = tab_manager_selected();
    if (sel < 0 && tab_manager_count() > 0) {
        tab_manager_set_selected(0);
        sel = 0;
    }
    if (sel < 0)
        return;

    WebView *web = tab_manager_webview_at(sel);
    if (!web)
        return;

    LayoutNode *pane = layout_find_pane_by_webview(root, web);
    if (pane && pane->view && pane->view->type == VIEW_PANE)
        pane_view_detach(pane->view);

    web_view_close((View *)web);

    if (tab_manager_selected() >= tab_manager_count())
        tab_manager_set_selected(tab_manager_count() - 1);
}
