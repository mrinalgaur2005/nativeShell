#include "view/web/web_view.h"
#include "cairo.h"
#include "gdk/gdk.h"
#include "glib-object.h"
#include "gtk/gtk.h"
#include "view/view.h"
#include "view/web/webview_registry.h"
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_keycode.h>
#include <SDL2/SDL_log.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_stdinc.h>
#include <stdio.h>
#include <webkit2/webkit2.h>
#include <stdlib.h>


static guint sdl_key_to_gdk(SDL_KeyCode key);
static void web_view_ensure_surface(WebView *wv, int w, int h);
static void web_view_ensure_texture(WebView *wv,SDL_Renderer *renderer);
static GdkDevice *get_pointer_device(GtkWidget *widget);
static GdkDevice *get_keyboard_device(GtkWidget *widget);

static guint guess_hardware_keycode(guint keyval);
static void draw(View *v,
                 SDL_Renderer *renderer,
                 SDL_Rect rect,
                 bool focused)
{
    WebView *wv = (WebView *)v;
    (void)focused;

    web_view_ensure_surface(wv, rect.w, rect.h);

    gtk_widget_set_size_request(wv->offscreen, rect.w, rect.h);

    while (gtk_events_pending()) {
        gtk_main_iteration();
    }

    cairo_save(wv->cr);
    cairo_set_operator(wv->cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(wv->cr);
    cairo_restore(wv->cr);

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

    SDL_RenderCopy(renderer, wv->texture, NULL, &rect);

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
const char *web_view_get_url(View *v)
{
    WebView *wv = (WebView *)v;
    return wv->url ? wv->url : "";
}
static void destroy(View *v)
{
    WebView *wv = (WebView *)v;

    if (wv->url)
        free(wv->url);

    if (wv->wk)
        g_object_unref(wv->wk);
    tab_registry_remove(v);
    webkit_web_view_stop_loading(wv->wk);
    gtk_widget_destroy(GTK_WIDGET(wv->wk));
    free(wv);
}

View *web_view_create(const char *url)
{
    WebView *wv = calloc(1, sizeof(WebView));
    wv->base.type=VIEW_WEB;
    wv->base.draw = draw;
    wv->base.destroy = destroy;

    wv->url = strdup(url);   // <-- STORE URL

    wv->offscreen = gtk_offscreen_window_new();
    gtk_widget_set_size_request(wv->offscreen, 800, 600);
    tab_registry_add((View *)wv);
    wv->wk = WEBKIT_WEB_VIEW(webkit_web_view_new());
    gtk_container_add(GTK_CONTAINER(wv->offscreen), GTK_WIDGET(wv->wk));
    gtk_widget_show_all(wv->offscreen);

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

void web_view_load_url(View *v, const char *url)
{
    WebView *wv = (WebView *)v;

    if (wv->url)
        free(wv->url);

    wv->url = strdup(url);
    webkit_web_view_load_uri(wv->wk, url);
}

//dont alter handels key typing
//doesnt include symbols and clipboard
//todo: gecko
void web_view_handle_key(View *v, SDL_KeyboardEvent *key)
{
    WebView *wv = (WebView *)v;

    guint keyval = sdl_key_to_gdk(key->keysym.sym);
    if (!keyval)
        return;

    GdkWindow *win = gtk_widget_get_window(GTK_WIDGET(wv->wk));
    GdkDevice *kbd = get_keyboard_device(wv->offscreen);
    if (!win || !kbd)
        return;

    GdkEventType type =
        (key->type == SDL_KEYDOWN) ? GDK_KEY_PRESS : GDK_KEY_RELEASE;

    GdkEvent *ev = gdk_event_new(type);

    ev->key.window = g_object_ref(win);
    ev->key.send_event = TRUE;
    ev->key.time = GDK_CURRENT_TIME;
    ev->key.keyval = keyval;

    ev->key.hardware_keycode = guess_hardware_keycode(keyval);

    ev->key.state =
        (key->keysym.mod & KMOD_SHIFT ? GDK_SHIFT_MASK : 0) |
        (key->keysym.mod & KMOD_CTRL  ? GDK_CONTROL_MASK : 0) |
        (key->keysym.mod & KMOD_ALT   ? GDK_MOD1_MASK : 0);

    ev->key.string = NULL;
    ev->key.length = 0;

    if (type == GDK_KEY_PRESS &&
            !(key->keysym.mod & (KMOD_CTRL | KMOD_ALT)))
    {
        gunichar uc = gdk_keyval_to_unicode(keyval);
        if (uc != 0 && !g_unichar_iscntrl(uc)) {
            gchar buf[8];
            gint len = g_unichar_to_utf8(uc, buf);
            ev->key.string = g_strndup(buf, len);
            ev->key.length = len;
        }
    }

    gdk_event_set_device(ev, kbd);
    gdk_event_put(ev);

    SDL_Log("[KEY %s] %s keyval=%u hw=%u string=%s",
        type == GDK_KEY_PRESS ? "PRESS" : "RELEASE",
        SDL_GetKeyName(key->keysym.sym),
        keyval,
        ev->key.hardware_keycode,
        ev->key.string ? ev->key.string : "(null)");

    gdk_event_free(ev);
}

void web_view_handle_mouse(View *v, SDL_MouseButtonEvent *btn,SDL_Rect rect)
{
    WebView *wv = (WebView *)v;

    GdkWindow *win = gtk_widget_get_window(GTK_WIDGET(wv->wk));
    GdkDevice *ptr = get_pointer_device(wv->offscreen);
    if (!win || !ptr)
        return;

    double local_x = btn->x - rect.x;
    double local_y = btn->y - rect.y;

    if (local_x < 0 || local_y < 0 ||
            local_x >= rect.w || local_y >= rect.h)
        return;

    GdkEvent *ev = gdk_event_new(
            btn->type == SDL_MOUSEBUTTONDOWN
            ? GDK_BUTTON_PRESS
            : GDK_BUTTON_RELEASE
            );

    ev->button.window = g_object_ref(win);
    ev->button.send_event = TRUE;
    ev->button.time = GDK_CURRENT_TIME;
    ev->button.x = local_x;
    ev->button.y = local_y;
    ev->button.button = btn->button;

    gdk_event_set_device(ev, ptr);
    gdk_event_put(ev);
    gdk_event_free(ev);}

void web_view_handle_motion(View *v,
                            SDL_MouseMotionEvent *motion,
                            SDL_Rect rect)
{
    WebView *wv = (WebView *)v;

    double local_x = motion->x - rect.x;
    double local_y = motion->y - rect.y;

    if (local_x < 0 || local_y < 0 ||
        local_x >= rect.w || local_y >= rect.h)
        return;

    GdkEvent *ev = gdk_event_new(GDK_MOTION_NOTIFY);
    ev->motion.window = g_object_ref(
        gtk_widget_get_window(GTK_WIDGET(wv->wk)));
    ev->motion.time = GDK_CURRENT_TIME;
    ev->motion.x = local_x;
    ev->motion.y = local_y;

    gdk_event_set_device(ev, get_pointer_device(wv->offscreen));
    gdk_event_put(ev);
    gdk_event_free(ev);
}

void web_view_handle_wheel(View *v,
                           SDL_MouseWheelEvent *wheel,
                           SDL_Rect rect)
{
    WebView *wv = (WebView *)v;

    GdkEvent *ev = gdk_event_new(GDK_SCROLL);
    ev->scroll.window = g_object_ref(
        gtk_widget_get_window(GTK_WIDGET(wv->wk)));
    ev->scroll.time = GDK_CURRENT_TIME;
    ev->scroll.direction =
        wheel->y > 0 ? GDK_SCROLL_UP : GDK_SCROLL_DOWN;

    gdk_event_set_device(ev, get_pointer_device(wv->offscreen));
    gdk_event_put(ev);
    gdk_event_free(ev);
}
static guint sdl_key_to_gdk(SDL_KeyCode key){
    /* ASCII range maps 1:1 */
    if (key >= 32 && key <= 126)
        return (guint)key;

    switch (key) {
        case SDLK_RETURN:    return GDK_KEY_Return;
        case SDLK_BACKSPACE: return GDK_KEY_BackSpace;
        case SDLK_TAB:       return GDK_KEY_Tab;
        case SDLK_ESCAPE:    return GDK_KEY_Escape;
        case SDLK_DELETE:    return GDK_KEY_Delete;

        case SDLK_LEFT:  return GDK_KEY_Left;
        case SDLK_RIGHT: return GDK_KEY_Right;
        case SDLK_UP:    return GDK_KEY_Up;
        case SDLK_DOWN:  return GDK_KEY_Down;

        default:
                         return 0;
    }
}
static GdkDevice *get_keyboard_device(GtkWidget *widget)
{
    GdkWindow *win = gtk_widget_get_window(widget);
    if (!win) return NULL;

    GdkDisplay *display = gdk_window_get_display(win);
    GdkSeat *seat = gdk_display_get_default_seat(display);

    return gdk_seat_get_keyboard(seat);
}

static GdkDevice *get_pointer_device(GtkWidget *widget)
{
    GdkWindow *win = gtk_widget_get_window(widget);
    if (!win) return NULL;

    GdkDisplay *display = gdk_window_get_display(win);
    GdkSeat *seat = gdk_display_get_default_seat(display);

    return gdk_seat_get_pointer(seat);
}

//gdk requires hardare keycode for backspace and following keys
static guint
guess_hardware_keycode(guint keyval)
{
    switch (keyval) {
        case GDK_KEY_BackSpace: return 22;
        case GDK_KEY_Return:    return 36;
        case GDK_KEY_Tab:       return 23;
        case GDK_KEY_Escape:    return 9;
        default:                return 0;
    }
}
//------ browser functionality--------
void web_view_redo(View *v)
{
    if (!v) return;

    WebView *wv = (WebView *)v;
    if (!wv->wk) return;

    if(webkit_web_view_can_go_forward(wv->wk))
            webkit_web_view_go_forward(wv->wk);
}

void web_view_undo(View *v)
{
    if (!v) return;

    WebView *wv = (WebView *)v;
    if (!wv->wk) return;
    if(webkit_web_view_can_go_back(wv->wk))
            webkit_web_view_go_back(wv->wk);
    else printf("hello");
}
void web_view_reload(View *v)
{
    if (!v) return;

    WebView *wv = (WebView *)v;
    if (!wv->wk) return;
    webkit_web_view_reload(wv->wk);
}
void web_view_stop(View *v){
    SDL_Log("stopppppp");
    if(!v)return;
    WebView *wv = (WebView *)v;
    if (!wv->wk) return;

    webkit_web_view_stop_loading(wv->wk);
}
void web_view_close(View *v)
{
    WebView *wv = (WebView *)v;
    if (!wv) return;

    tab_registry_remove(v);

    webkit_web_view_stop_loading(wv->wk);

    gtk_widget_destroy(GTK_WIDGET(wv->wk));
    gtk_widget_destroy(wv->offscreen);

    free(wv->url);
    free(wv);
}
