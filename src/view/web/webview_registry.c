#include "webview_registry.h"
#include "SDL_log.h"
#include "view/web/web_view.h"
#include <webkit2/webkit2.h>
#include <string.h>
#include <stdlib.h>

#define MAX_TABS 128

static WebViewEntry tabs[MAX_TABS];
static int tab_count = 0;
static SDL_Renderer *g_renderer = NULL;

static SDL_Texture *cairo_to_texture(cairo_surface_t *s);
/* ---------- helpers ---------- */

WebViewEntry *tab_registry_get_by_view(View *v)
{
    for (int i = 0; i < tab_registry_count(); i++) {
        WebViewEntry *e = tab_registry_get(i);
        if (e && e->view == v)
            return e;
    }
    return NULL;
}
static WebViewEntry *find_entry(View *v)
{
    for (int i = 0; i < tab_count; i++)
        if (tabs[i].view == v)
            return &tabs[i];
    return NULL;
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

/* static – registry private */
static void favicon_cb(
    GObject      *source_object,
    GAsyncResult *res,
    gpointer      user_data
)
{
    WebKitFaviconDatabase *db =
        WEBKIT_FAVICON_DATABASE(source_object);

    WebViewEntry *e = user_data;
    SDL_Log("[favicon_cb] entered");

    if (!e || !e->alive) {
        SDL_Log("[favicon_cb] entry dead");
        return;
    }

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
        return;
    }

    if (!surface) {
        SDL_Log("[favicon_cb] surface NULL");
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

    SDL_Log("[favicon_cb] texture=%p", e->icon);

    cairo_surface_destroy(surface);
}

/* PUBLIC API */
void tab_registry_request_snapshot(View *web)
{
    WebViewEntry *e = find_entry(web);
    if (!e) {
        SDL_Log("[favicon] no entry");
        return;
    }

    if (e->icon) {
        SDL_Log("[favicon] already have icon");
        return;
    }

    WebView *wv = (WebView *)web;
    const char *uri = webkit_web_view_get_uri(wv->wk);

    SDL_Log("[favicon] request for %s", uri ? uri : "(null)");

    if (!uri)
        return;

    WebKitWebContext *ctx =
        webkit_web_view_get_context(wv->wk);

    WebKitFaviconDatabase *db =
        webkit_web_context_get_favicon_database(ctx);

    webkit_favicon_database_get_favicon(
        db,
        uri,
        NULL,
        favicon_cb,   // ✅ CORRECT CALLBACK
        e
    );
}

/* ---------- public API ---------- */

void tab_registry_init(SDL_Renderer *renderer)
{

    memset(tabs, 0, sizeof(tabs));
    tab_count = 0;
    g_renderer = renderer;

}

int tab_registry_add(View *web)
{
    if (tab_count >= MAX_TABS)
        return -1;

    WebViewEntry *e = &tabs[tab_count];
    memset(e, 0, sizeof(*e));

    e->view = web;
    e->alive = 1;

    tab_count++;
    return tab_count - 1;
}

void tab_registry_remove(View *web)
{
    for (int i = 0; i < tab_count; i++) {
        if (tabs[i].view == web) {

            if (tabs[i].icon)
                SDL_DestroyTexture(tabs[i].icon);

            memmove(&tabs[i], &tabs[i + 1],
                    sizeof(WebViewEntry) * (tab_count - i - 1));
            tab_count--;
            return;
        }
    }
}

int tab_registry_count(void)
{
    return tab_count;
}

WebViewEntry *tab_registry_get(int index)
{
    if (index < 0 || index >= tab_count)
        return NULL;
    return &tabs[index];
}

void tab_registry_set_title(View *web, const char *title)
{
    WebViewEntry *e = find_entry(web);
    if (!e || !title)
        return;

    strncpy(e->title, title, sizeof(e->title) - 1);
}

void tab_registry_set_loading(View *web, int loading)
{
    WebViewEntry *e = find_entry(web);
    if (!e)
        return;

    e->loading = loading;
}
