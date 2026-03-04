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
#include <ctype.h>
#include <stdio.h>
#include <webkit2/webkit2.h>
#include <stdlib.h>
#include <string.h>

static WebKitWebContext *g_web_context = NULL;

static guint sdl_key_to_gdk(SDL_KeyCode key);
static void web_view_ensure_surface(WebView *wv, int w, int h);
static void web_view_ensure_texture(WebView *wv,SDL_Renderer *renderer);
static GdkDevice *get_pointer_device(GtkWidget *widget);
static GdkDevice *get_keyboard_device(GtkWidget *widget);
static void web_view_show_error_page(WebView *wv,
                                     const char *heading,
                                     const char *message,
                                     const char *url,
                                     const char *details);
static int web_view_normalize_url(const char *raw,
                                  char *out,
                                  size_t out_sz);
static void web_view_load_checked(WebView *wv, const char *url);
static void web_view_set_url_copy(WebView *wv, const char *url);
static char *html_escape(const char *src);

static void on_load_changed(WebKitWebView *wk,
                            WebKitLoadEvent ev,
                            gpointer userdata);
static gboolean on_load_failed(WebKitWebView *wk,
                               WebKitLoadEvent event,
                               gchar *failing_uri,
                               GError *error,
                               gpointer user_data);
static guint guess_hardware_keycode(guint keyval);
void web_view_set_context(WebKitWebContext *ctx)
{
    g_web_context = ctx;
}





static void
on_load_changed(WebKitWebView *wk,
                WebKitLoadEvent event,
                gpointer user_data)
{
    WebView *wv = (WebView *)user_data;
    if (!wv) return;

    switch (event) {

        case WEBKIT_LOAD_STARTED:
            tab_manager_set_loading(wv, 1);
            break;


        case WEBKIT_LOAD_FINISHED:
            {
                tab_manager_set_loading(wv, 0);

                const char *title = webkit_web_view_get_title(wk);
                if (title)
                    tab_manager_set_title(wv, title);

                /* ask registry to handle visuals */
                const char *uri = webkit_web_view_get_uri(wk);
                SDL_Log("[favicon] load finished uri=%s", uri ? uri : "(null)");
                tab_manager_request_favicon(wv);
                break;
            }

        default:
            break;
    }
}

static gboolean
on_load_failed(WebKitWebView *wk,
               WebKitLoadEvent event,
               gchar *failing_uri,
               GError *error,
               gpointer user_data)
{
    (void)wk;
    (void)event;

    WebView *wv = (WebView *)user_data;
    if (!wv)
        return FALSE;

    if (error &&
        g_error_matches(error,
                        WEBKIT_NETWORK_ERROR,
                        WEBKIT_NETWORK_ERROR_CANCELLED))
    {
        return FALSE;
    }

    const char *uri = failing_uri ? failing_uri : (wv->url ? wv->url : "");
    const char *details = (error && error->message)
        ? error->message
        : "Unknown WebKit error";

    SDL_Log("[web] load failed uri=%s error=%s",
            uri[0] ? uri : "(empty)",
            details);

    tab_manager_set_loading(wv, 0);
    tab_manager_set_title(wv, "Load error");
    tab_manager_set_url(wv, uri);

    web_view_show_error_page(
        wv,
        "Couldn't load this page",
        "The page could not be loaded. Check the address and try again.",
        uri,
        details
    );

    return TRUE;
}


static void draw(View *v,
                 SDL_Renderer *renderer,
                 SDL_Rect rect,
                 bool focused)
{
    WebView *wv = (WebView *)v;
    (void)focused;

    web_view_ensure_surface(wv, rect.w, rect.h);

    gtk_widget_set_size_request(wv->offscreen, rect.w, rect.h);

    GtkAllocation alloc = { 0, 0, rect.w, rect.h };
    gtk_widget_size_allocate(wv->offscreen, &alloc);

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
    (void)focused;
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
const char *web_view_get_title(View *v)
{
    if (!v || v->type != VIEW_WEB)
        return NULL;

    WebView *wv = (WebView *)v;

    const char *title = webkit_web_view_get_title(wv->wk);
    if (title && *title)
        return title;

    return wv->url;  /* fallback */
}

const char *web_view_get_url(View *v)
{
    if (!v || v->type != VIEW_WEB)
        return NULL;

    WebView *wv = (WebView *)v;
    return wv->url;
}

int web_view_is_loading(View *v)
{
    if (!v || v->type != VIEW_WEB)
        return 0;

    WebView *wv = (WebView *)v;
    return webkit_web_view_is_loading(wv->wk);
}
static void destroy(View *v)
{
    WebView *wv = (WebView *)v;

    if (wv->url)
        free(wv->url);

    tab_manager_remove(wv);

    if (wv->wk) {
        webkit_web_view_stop_loading(wv->wk);
        gtk_widget_destroy(GTK_WIDGET(wv->wk));
        wv->wk = NULL;
    }
    if (wv->offscreen) {
        gtk_widget_destroy(wv->offscreen);
        wv->offscreen = NULL;
    }
    if (wv->texture) {
        SDL_DestroyTexture(wv->texture);
        wv->texture = NULL;
    }
    if (wv->cr) {
        cairo_destroy(wv->cr);
        wv->cr = NULL;
    }
    if (wv->surface) {
        cairo_surface_destroy(wv->surface);
        wv->surface = NULL;
    }

    free(wv);
}

View *web_view_create(const char *url)
{
    WebView *wv = calloc(1, sizeof(WebView));
    if (!wv)
        return NULL;

    wv->base.type = VIEW_WEB;
    wv->base.draw = draw;
    wv->base.destroy = destroy;

    web_view_set_url_copy(wv, url ? url : "");

    /* ---- GTK objects FIRST ---- */
    wv->offscreen = gtk_offscreen_window_new();
    gtk_widget_set_size_request(wv->offscreen, 800, 600);

    if (g_web_context)
        wv->wk = WEBKIT_WEB_VIEW(webkit_web_view_new_with_context(g_web_context));
    else
        wv->wk = WEBKIT_WEB_VIEW(webkit_web_view_new());
    if (!wv->offscreen || !wv->wk) {
        destroy((View *)wv);
        return NULL;
    }
    gtk_container_add(GTK_CONTAINER(wv->offscreen),
                      GTK_WIDGET(wv->wk));
    gtk_widget_show_all(wv->offscreen);

    /* ---- registry AFTER wk exists ---- */
    tab_manager_add(wv, wv->url ? wv->url : "");
    /* ---- NOW connect signals ---- */
    g_signal_connect(
            wv->wk,
            "load-changed",
            G_CALLBACK(on_load_changed),
            wv
            );
    g_signal_connect(
            wv->wk,
            "load-failed",
            G_CALLBACK(on_load_failed),
            wv
            );

    /* ---- load ---- */
    web_view_load_checked(wv, wv->url);

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
    if (!v || v->type != VIEW_WEB)
        return;

    WebView *wv = (WebView *)v;
    web_view_load_checked(wv, url);
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
    if (!v)
        return;
    destroy(v);
}

static int web_view_has_scheme(const char *url)
{
    if (!url || !*url)
        return 0;

    const char *colon = strchr(url, ':');
    if (!colon || colon == url)
        return 0;

    if (!isalpha((unsigned char)url[0]))
        return 0;

    for (const char *p = url + 1; p < colon; p++) {
        unsigned char c = (unsigned char)*p;
        if (!isalnum(c) && c != '+' && c != '-' && c != '.')
            return 0;
    }

    return 1;
}

static int web_view_has_spaces_or_controls(const char *url)
{
    for (const unsigned char *p = (const unsigned char *)url; *p; p++) {
        if (isspace(*p) || iscntrl(*p))
            return 1;
    }
    return 0;
}

static int web_view_looks_like_host(const char *url)
{
    if (!url || !*url)
        return 0;

    if (strncmp(url, "localhost", 9) == 0)
        return 1;

    return strchr(url, '.') != NULL;
}

static int web_view_normalize_url(const char *raw,
                                  char *out,
                                  size_t out_sz)
{
    if (!out || out_sz == 0)
        return 0;
    out[0] = '\0';

    if (!raw)
        return 0;

    while (*raw && isspace((unsigned char)*raw))
        raw++;

    size_t len = strlen(raw);
    while (len > 0 && isspace((unsigned char)raw[len - 1]))
        len--;

    if (len == 0 || len >= 1024)
        return 0;

    char trimmed[1024];
    memcpy(trimmed, raw, len);
    trimmed[len] = '\0';

    if (web_view_has_spaces_or_controls(trimmed))
        return 0;

    if (web_view_has_scheme(trimmed)) {
        if (len >= out_sz)
            return 0;
        memcpy(out, trimmed, len + 1);
        return 1;
    }

    if (!web_view_looks_like_host(trimmed))
        return 0;

    int wrote = snprintf(out, out_sz, "https://%s", trimmed);
    return wrote > 0 && (size_t)wrote < out_sz;
}

static void web_view_set_url_copy(WebView *wv, const char *url)
{
    if (!wv)
        return;

    char *copy = strdup(url ? url : "");
    if (!copy)
        return;

    if (wv->url)
        free(wv->url);
    wv->url = copy;
}

static char *html_escape(const char *src)
{
    if (!src)
        src = "";

    size_t out_len = 0;
    for (const char *p = src; *p; p++) {
        switch (*p) {
            case '&': out_len += 5; break;   /* &amp; */
            case '<': out_len += 4; break;   /* &lt; */
            case '>': out_len += 4; break;   /* &gt; */
            case '"': out_len += 6; break;   /* &quot; */
            case '\'': out_len += 5; break;  /* &#39; */
            default: out_len += 1; break;
        }
    }

    char *out = malloc(out_len + 1);
    if (!out)
        return NULL;

    char *dst = out;
    for (const char *p = src; *p; p++) {
        switch (*p) {
            case '&':
                memcpy(dst, "&amp;", 5);
                dst += 5;
                break;
            case '<':
                memcpy(dst, "&lt;", 4);
                dst += 4;
                break;
            case '>':
                memcpy(dst, "&gt;", 4);
                dst += 4;
                break;
            case '"':
                memcpy(dst, "&quot;", 6);
                dst += 6;
                break;
            case '\'':
                memcpy(dst, "&#39;", 5);
                dst += 5;
                break;
            default:
                *dst++ = *p;
                break;
        }
    }
    *dst = '\0';

    return out;
}

static void web_view_show_error_page(WebView *wv,
                                     const char *heading,
                                     const char *message,
                                     const char *url,
                                     const char *details)
{
    if (!wv || !wv->wk)
        return;

    char *esc_heading = html_escape(heading ? heading : "Load error");
    char *esc_message = html_escape(message ? message : "Unknown error");
    char *esc_url = html_escape(url ? url : "(none)");
    char *esc_details = html_escape(details ? details : "No details");

    if (!esc_heading || !esc_message || !esc_url || !esc_details) {
        free(esc_heading);
        free(esc_message);
        free(esc_url);
        free(esc_details);
        return;
    }

    const char *tpl =
        "<!doctype html>"
        "<html><head><meta charset=\"utf-8\">"
        "<title>Load error</title>"
        "<style>"
        "body{font-family:sans-serif;background:#15171a;color:#e4e7ec;"
        "margin:0;padding:24px;}"
        ".card{max-width:760px;margin:0 auto;background:#20242b;"
        "border:1px solid #2d3440;border-radius:10px;padding:18px;}"
        "h1{margin:0 0 10px 0;font-size:22px;}"
        "p{margin:8px 0;line-height:1.45;}"
        "code{display:block;padding:10px;background:#0f1217;border-radius:6px;"
        "border:1px solid #2a313c;white-space:pre-wrap;word-break:break-word;}"
        "</style></head><body>"
        "<div class=\"card\">"
        "<h1>%s</h1>"
        "<p>%s</p>"
        "<p><strong>URL</strong></p><code>%s</code>"
        "<p><strong>Details</strong></p><code>%s</code>"
        "</div></body></html>";

    int needed = snprintf(NULL, 0, tpl,
                          esc_heading, esc_message, esc_url, esc_details);
    if (needed <= 0) {
        free(esc_heading);
        free(esc_message);
        free(esc_url);
        free(esc_details);
        return;
    }

    char *html = malloc((size_t)needed + 1);
    if (!html) {
        free(esc_heading);
        free(esc_message);
        free(esc_url);
        free(esc_details);
        return;
    }

    snprintf(html, (size_t)needed + 1, tpl,
             esc_heading, esc_message, esc_url, esc_details);
    webkit_web_view_load_html(wv->wk, html, NULL);

    free(html);
    free(esc_heading);
    free(esc_message);
    free(esc_url);
    free(esc_details);
}

static void web_view_load_checked(WebView *wv, const char *url)
{
    if (!wv || !wv->wk)
        return;

    char normalized[1024];
    if (!web_view_normalize_url(url, normalized, sizeof(normalized))) {
        const char *bad = url ? url : "";
        web_view_set_url_copy(wv, bad);
        tab_manager_set_loading(wv, 0);
        tab_manager_set_title(wv, "Invalid URL");
        tab_manager_set_url(wv, bad);
        web_view_show_error_page(
            wv,
            "Invalid URL",
            "The address is not a valid URL. Use a full URL like https://example.com.",
            bad[0] ? bad : "(empty)",
            "Missing scheme or invalid URL format"
        );
        return;
    }

    web_view_set_url_copy(wv, normalized);
    tab_manager_set_url(wv, normalized);
    webkit_web_view_load_uri(wv->wk, normalized);
}
