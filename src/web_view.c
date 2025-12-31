#include "web_view.h"
#include <SDL2/SDL_stdinc.h>
#include <webkit2/webkit2.h>
#include <stdlib.h>

typedef struct {
    View base;
    WebKitWebView *wk;
    /* later: offscreen surface, texture, size */
} WebView;

static void draw(View *v,
                 SDL_Renderer *renderer,
                 SDL_Rect rect,
                 bool focused)
{
    SDL_Log("Inside web view");
    WebView *wv = (WebView *)v; 
    /* TEMP: placeholder rendering */
    SDL_SetRenderDrawColor(renderer, 180, 30, 30, 255);
    
    SDL_RenderFillRect(renderer, &rect); 
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer, &rect);

    /* REAL rendering comes later */
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
    static bool webkit_initialized = false;
    if (!webkit_initialized) {
        webkit_initialized = true;
        /* GTK/WebKit init will live here */
    }

    WebView *wv = calloc(1, sizeof(*wv));
    wv->base.draw = draw;
    wv->base.destroy = destroy;

    wv->wk = WEBKIT_WEB_VIEW(webkit_web_view_new());
    webkit_web_view_load_uri(wv->wk, url);

    return (View *)wv;
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
