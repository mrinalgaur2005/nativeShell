#include "web_view.h"
#include "cairo.h"
#include "gdk/gdk.h"
#include "gtk/gtk.h"
#include <SDL2/SDL_log.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_stdinc.h>
#include <webkit2/webkit2.h>
#include <stdlib.h>
typedef struct {
    View base;
    GtkWidget *offscreen;
    WebKitWebView *wk;

    cairo_surface_t *surface;
    cairo_t *cr;
    int width;
    int height;

    SDL_Texture *texture;
    /* later: offscreen surface, texture, size */
} WebView;

static void web_view_ensure_surface(WebView *wv, int w, int h);
static void web_view_ensure_texture(WebView *wv,SDL_Renderer *renderer);
static void draw(View *v,
                 SDL_Renderer *renderer,
                 SDL_Rect rect,
                 bool focused)
{
    WebView *wv = (WebView *)v;
    (void)focused;

    web_view_ensure_surface(wv, rect.w, rect.h);

    /* Inform GTK of desired size */
    gtk_widget_set_size_request(wv->offscreen, rect.w, rect.h);

    /* Let GTK process pending layout / paint */
    while (gtk_events_pending()) {
        gtk_main_iteration();
    }

    /* Clear Cairo surface */
    cairo_save(wv->cr);
    cairo_set_operator(wv->cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(wv->cr);
    cairo_restore(wv->cr);

    /* GTK → Cairo */
    gtk_widget_draw(wv->offscreen, wv->cr);
    cairo_surface_flush(wv->surface);

    web_view_ensure_texture(wv, renderer);

    unsigned char *data = cairo_image_surface_get_data(wv->surface);
    int stride = cairo_image_surface_get_stride(wv->surface);

    SDL_UpdateTexture(
            wv->texture,
            NULL,
            data,
            stride
            );
    /* Debug pixel check */
    if (data) {
        SDL_Log("Cairo pixel ARGB = %02x %02x %02x %02x",
                data[2], data[1], data[0], data[3]);
    }

    /* TEMP placeholder until SDL texture upload */
    SDL_RenderCopy(renderer, wv->texture, NULL, &rect);

    /* Optional focus outline */
    if (focused) {
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderDrawRect(renderer, &rect);
    }
}

static void web_view_ensure_texture(WebView *wv,SDL_Renderer *renderer){
    if (wv->texture)
        return;

    wv->texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        wv->width,
        wv->height
    );

    SDL_SetTextureBlendMode(wv->texture, SDL_BLENDMODE_BLEND);
}
static void destroy(View *v)
{
    WebView *wv = (WebView *)v;

    if (wv->wk)
        g_object_unref(wv->wk);

    free(wv);
}

View *web_view_create(const char *url)
{
    WebView *wv = calloc(1, sizeof(*wv));
    wv->base.draw = draw;
    wv->base.destroy = destroy;

    wv->offscreen=gtk_offscreen_window_new();
    gtk_widget_set_size_request(wv->offscreen,800,600);
    wv->wk = WEBKIT_WEB_VIEW(webkit_web_view_new());
    gtk_container_add(GTK_CONTAINER(wv->offscreen),GTK_WIDGET(wv->wk));
    gtk_widget_show_all(wv->offscreen);
    gtk_widget_realize(wv->offscreen);
    
    webkit_web_view_load_uri(wv->wk, url);

    return (View *)wv;
}

static void web_view_ensure_surface(WebView *wv, int w, int h)
{
    if (wv->surface &&
        wv->width == w &&
        wv->height == h)
        return;

    if (wv->cr) {
        cairo_destroy(wv->cr);
        wv->cr = NULL;
    }
    if (wv->surface) {
        cairo_surface_destroy(wv->surface);
        wv->surface = NULL;
    }

    if (wv->texture) {
        SDL_DestroyTexture(wv->texture);
        wv->texture = NULL;
    }
    wv->width = w;
    wv->height = h;

    wv->surface =
        cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
    wv->cr = cairo_create(wv->surface);
}
void web_view_handle_key(View *v, SDL_KeyboardEvent *key)
{
    (void)v;
    (void)key;
    /* TODO: forward to WebKit */
}

void web_view_handle_mouse(View *v, SDL_MouseButtonEvent *btn)
{
    (void)v;
    (void)btn;
    /* TODO: forward to WebKit */
}

void web_view_handle_motion(View *v, SDL_MouseMotionEvent *motion)
{
    (void)v;
    (void)motion;
    /* TODO: forward to WebKit */
}
