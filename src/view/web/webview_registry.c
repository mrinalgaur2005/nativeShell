#include "webview_registry.h"
#include "SDL_log.h"
#include "view/web/web_view.h"
#include <webkit2/webkit2.h>
#include <string.h>
#include <stdlib.h>

static TabManager g_tabs = {0};
static SDL_Renderer *g_renderer = NULL;

static SDL_Texture *cairo_to_texture(cairo_surface_t *s);

static void ensure_capacity(int needed)
{
    if (g_tabs.capacity >= needed)
        return;

    int new_cap = g_tabs.capacity ? g_tabs.capacity * 2 : 8;
    while (new_cap < needed)
        new_cap *= 2;

    g_tabs.all = realloc(g_tabs.all, sizeof(WebView *) * new_cap);
    g_tabs.entries = realloc(g_tabs.entries, sizeof(TabEntry) * new_cap);
    g_tabs.capacity = new_cap;
}

int tab_manager_index_of(WebView *web)
{
    if (!web)
        return -1;
    for (int i = 0; i < g_tabs.count; i++) {
        if (g_tabs.all[i] == web)
            return i;
    }
    return -1;
}

WebView *tab_manager_webview_at(int index)
{
    if (index < 0 || index >= g_tabs.count)
        return NULL;
    return g_tabs.all[index];
}

TabEntry *tab_manager_entry_at(int index)
{
    if (index < 0 || index >= g_tabs.count)
        return NULL;
    return &g_tabs.entries[index];
}

int tab_manager_count(void)
{
    return g_tabs.count;
}

int tab_manager_selected(void)
{
    if (g_tabs.count <= 0)
        return -1;
    return g_tabs.selected_index;
}

void tab_manager_set_selected(int index)
{
    if (g_tabs.count <= 0) {
        g_tabs.selected_index = -1;
        return;
    }

    if (index < 0)
        index = 0;
    if (index >= g_tabs.count)
        index = g_tabs.count - 1;

    g_tabs.selected_index = index;
}

/* Cairo → SDL texture */
static SDL_Texture *
cairo_to_texture(cairo_surface_t *s)
{
    if (!s || !g_renderer)
        return NULL;

    cairo_surface_flush(s);

    int w = cairo_image_surface_get_width(s);
    int h = cairo_image_surface_get_height(s);
    unsigned char *src = cairo_image_surface_get_data(s);

    /* Create SDL surface we OWN */
    SDL_Surface *dst = SDL_CreateRGBSurfaceWithFormat(
        0,
        w,
        h,
        32,
        SDL_PIXELFORMAT_ARGB8888
    );

    if (!dst)
        return NULL;

    for (int y = 0; y < h; y++) {
        memcpy(
            (Uint8 *)dst->pixels + y * dst->pitch,
            src + y * cairo_image_surface_get_stride(s),
            w * 4
        );
    }

    SDL_Texture *tex = SDL_CreateTextureFromSurface(g_renderer, dst);
    SDL_FreeSurface(dst);

    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);

    return tex;
}

/* ---------- favicon async ---------- */
static void favicon_cb(
    GObject      *source_object,
    GAsyncResult *res,
    gpointer      user_data
)
{
    WebKitFaviconDatabase *db =
        WEBKIT_FAVICON_DATABASE(source_object);

    WebView *web = user_data;
    int idx = tab_manager_index_of(web);
    if (idx < 0) {
        SDL_Log("[favicon_cb] entry missing");
        return;
    }

    TabEntry *e = &g_tabs.entries[idx];
    SDL_Log("[favicon_cb] entered");

    GError *error = NULL;

    cairo_surface_t *surface =
        webkit_favicon_database_get_favicon_finish(
            db,
            res,
            &error
        );

    if (error) {
        SDL_Log("[favicon_cb] error: %s", error->message);
        g_error_free(error);
        e->favicon_requested = 0;
        return;
    }

    if (!surface) {
        SDL_Log("[favicon_cb] surface NULL");
        e->favicon_requested = 0;
        return;
    }

    SDL_Log(
        "[favicon_cb] surface %dx%d type=%d",
        cairo_image_surface_get_width(surface),
        cairo_image_surface_get_height(surface),
        cairo_surface_get_type(surface)
    );

    if (e->icon)
        SDL_DestroyTexture(e->icon);

    e->icon = cairo_to_texture(surface);
    e->favicon_requested = 0;

    SDL_Log("[favicon_cb] texture=%p", e->icon);

    cairo_surface_destroy(surface);
}

void tab_manager_request_favicon(WebView *web)
{
    int idx = tab_manager_index_of(web);
    if (idx < 0) {
        SDL_Log("[favicon] no entry");
        return;
    }

    TabEntry *e = &g_tabs.entries[idx];
    if (e->icon) {
        SDL_Log("[favicon] already have icon");
        return;
    }

    if (e->favicon_requested) {
        SDL_Log("[favicon] request already pending");
        return;
    }

    const char *uri = webkit_web_view_get_uri(web->wk);
    SDL_Log("[favicon] request for %s", uri ? uri : "(null)");

    if (!uri)
        return;

    WebKitWebContext *ctx =
        webkit_web_view_get_context(web->wk);

    WebKitFaviconDatabase *db =
        webkit_web_context_get_favicon_database(ctx);

    e->favicon_requested = 1;

    webkit_favicon_database_get_favicon(
        db,
        uri,
        NULL,
        favicon_cb,
        web
    );
}

/* ---------- public API ---------- */
void tab_manager_init(SDL_Renderer *renderer)
{
    memset(&g_tabs, 0, sizeof(g_tabs));
    g_renderer = renderer;
    g_tabs.selected_index = -1;
}

int tab_manager_add(WebView *web, const char *url)
{
    if (!web)
        return -1;

    ensure_capacity(g_tabs.count + 1);
    g_tabs.all[g_tabs.count] = web;

    TabEntry *e = &g_tabs.entries[g_tabs.count];
    memset(e, 0, sizeof(*e));
    if (url)
        strncpy(e->url, url, sizeof(e->url) - 1);
    e->loading = 1;

    g_tabs.count++;
    if (g_tabs.selected_index < 0)
        g_tabs.selected_index = 0;

    return g_tabs.count - 1;
}

void tab_manager_remove(WebView *web)
{
    int idx = tab_manager_index_of(web);
    if (idx < 0)
        return;

    TabEntry *e = &g_tabs.entries[idx];
    if (e->icon)
        SDL_DestroyTexture(e->icon);

    int tail = g_tabs.count - 1;
    if (idx < tail) {
        memmove(&g_tabs.all[idx],
                &g_tabs.all[idx + 1],
                sizeof(WebView *) * (tail - idx));
        memmove(&g_tabs.entries[idx],
                &g_tabs.entries[idx + 1],
                sizeof(TabEntry) * (tail - idx));
    }

    g_tabs.count--;

    if (g_tabs.count == 0) {
        g_tabs.selected_index = -1;
    } else if (g_tabs.selected_index > idx) {
        g_tabs.selected_index--;
    } else if (g_tabs.selected_index == idx &&
               g_tabs.selected_index >= g_tabs.count) {
        g_tabs.selected_index = g_tabs.count - 1;
    }
}

void tab_manager_destroy_all(void)
{
    while (g_tabs.count > 0) {
        WebView *web = g_tabs.all[0];
        web_view_close((View *)web);
    }

    free(g_tabs.all);
    free(g_tabs.entries);
    memset(&g_tabs, 0, sizeof(g_tabs));
    g_tabs.selected_index = -1;
}

void tab_manager_set_title(WebView *web, const char *title)
{
    int idx = tab_manager_index_of(web);
    if (idx < 0 || !title)
        return;

    TabEntry *e = &g_tabs.entries[idx];
    strncpy(e->title, title, sizeof(e->title) - 1);
}

void tab_manager_set_url(WebView *web, const char *url)
{
    int idx = tab_manager_index_of(web);
    if (idx < 0 || !url)
        return;

    TabEntry *e = &g_tabs.entries[idx];
    strncpy(e->url, url, sizeof(e->url) - 1);
}

void tab_manager_set_loading(WebView *web, int loading)
{
    int idx = tab_manager_index_of(web);
    if (idx < 0)
        return;

    g_tabs.entries[idx].loading = loading;
}
