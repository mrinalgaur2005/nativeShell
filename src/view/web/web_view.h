
#pragma once
#include "view/view.h"
#include "gtk/gtk.h"

typedef struct _WebKitWebView WebKitWebView;

typedef struct {
    View base;
    GtkWidget *offscreen;
    WebKitWebView *wk;
    char *url;
    cairo_surface_t *surface;
    cairo_t *cr;
    int width;
    int height;

    SDL_Texture *texture;
} WebView;

View *web_view_create(const char *url);

const char *web_view_get_title(View *v);
const char *web_view_get_url(View *v);
int web_view_is_loading(View *v);
void web_view_handle_key(View *v, SDL_KeyboardEvent *key);
void web_view_handle_mouse(View *v, SDL_MouseButtonEvent *btn,SDL_Rect leaf_rect);
void web_view_handle_motion(View *v, SDL_MouseMotionEvent *motion,SDL_Rect leaf_rec);
void web_view_handle_wheel(View *v, SDL_MouseWheelEvent *wheel,SDL_Rect leaf_rec);
void web_view_load_url(View *v,const char *url);
void web_view_undo(View *v);
void web_view_redo(View *v);
void web_view_reload(View *v);
void web_view_stop(View *v);
void web_view_close(View *v);
SDL_Texture *web_view_get_favicon(View *v, SDL_Renderer *r);
