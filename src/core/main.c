#include "SDL_ttf.h"
#include "command/command.h"
#include "command/command_overlay.h"
#include "config/config.h"
#include "focus.h"
#include "core/profile.h"
#include <webkit2/webkit2.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_keycode.h>
#include <SDL2/SDL_log.h>
#include <SDL2/SDL_render.h>
#include <assert.h>
#include <sched.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <gtk/gtk.h>
#include "layout/layout.h"
#include "render/render.h"
#include "core/session.h"
#include "view/pane/pane_view.h"
#include "view/tab/tab_view.h"
#include "view/view.h"
#include "view/web/web_view.h"
#include "view/web/webview_registry.h"

typedef struct {
    bool active;
    LayoutNode *node;
    bool vertical;
    int start_x, start_y;
    float start_ratio;
} ResizeState;

static ResizeState resize = {0};

void layout_validate(LayoutNode *n)
{
    if (!n) return;

    if (n->type == NODE_LEAF) {
        assert(n->view != NULL);
        assert(n->a == NULL && n->b == NULL);
    } else {
        assert(n->a && n->b);
        assert(n->view == NULL);
        assert(n->a->parent == n);
        assert(n->b->parent == n);
        layout_validate(n->a);
        layout_validate(n->b);
    }
}

typedef enum{
    INPUT_MODE_WM, //for hjkl (moving), s(splitting horiontalu),v(splitting vertically) x(merging) 
                   //esc for entering this
    INPUT_MODE_VIEW,//press i to go insertmode(vim inspired obv) for using browser
    INPUT_MODE_CMD //uses for command mode again vim inspired
} InputMode;

InputMode input_mode = INPUT_MODE_WM;

static bool is_text_key(SDL_Keycode sym);
bool v_down = false;

static LayoutNode *pick_target_pane(LayoutNode *focused,
                                    LayoutNode *last_pane,
                                    LayoutNode *root)
{
    if (focused && focused->view && focused->view->type == VIEW_PANE)
        return focused;
    if (last_pane && last_pane->view && last_pane->view->type == VIEW_PANE)
        return last_pane;
    return layout_find_view_type(root, VIEW_PANE);
}

static WebView *pane_attached(LayoutNode *pane)
{
    if (!pane || !pane->view || pane->view->type != VIEW_PANE)
        return NULL;
    return pane_view_get_attached(pane->view);
}

static void detach_webview_from_pane(LayoutNode *pane)
{
    if (!pane || !pane->view || pane->view->type != VIEW_PANE)
        return;
    pane_view_detach(pane->view);
}

static void attach_webview_to_pane(LayoutNode *root,
                                   LayoutNode *pane,
                                   WebView *web)
{
    if (!root || !pane || !pane->view || pane->view->type != VIEW_PANE || !web)
        return;

    LayoutNode *existing = layout_find_pane_by_webview(root, web);
    if (existing && existing != pane &&
        existing->view && existing->view->type == VIEW_PANE)
    {
        pane_view_detach(existing->view);
    }

    if (pane_view_get_attached(pane->view))
        pane_view_detach(pane->view);

    pane_view_attach(pane->view, web);
}

static void hide_webview_from_pane(LayoutNode **root,
                                   LayoutNode **focused,
                                   LayoutNode **last_pane)
{
    if (!root || !*root || !focused || !*focused)
        return;

    LayoutNode *pane = *focused;
    if (!pane->view || pane->view->type != VIEW_PANE)
        return;

    detach_webview_from_pane(pane);

    if (pane->parent) {
        LayoutNode *new_focus = layout_close_leaf(pane, root);
        *focused = layout_leaf_from_node(new_focus);
        if (!*focused)
            *focused = layout_first_leaf(*root);
    }

    if (*focused && (*focused)->view &&
        (*focused)->view->type == VIEW_PANE)
    {
        *last_pane = *focused;
    } else {
        *last_pane = layout_find_view_type(*root, VIEW_PANE);
    }
}

static void cmd_open_webview(LayoutNode *pane,
                             LayoutNode *root,
                             const char *url)
{
    if (!pane || !pane->view || pane->view->type != VIEW_PANE)
        return;
    if (pane_view_get_attached(pane->view))
        return;

    WebView *web = (WebView *)web_view_create(url);
    attach_webview_to_pane(root, pane, web);
    input_mode = INPUT_MODE_VIEW;
}

static WebKitWebContext *create_profile_web_context(const ProfilePaths *profile,
                                                    bool *owns_web_context)
{
    if (owns_web_context)
        *owns_web_context = false;
    if (!profile)
        return webkit_web_context_get_default();

    WebKitWebsiteDataManager *manager = webkit_website_data_manager_new(
        "base-data-directory", profile->webkit_data_dir,
        "base-cache-directory", profile->webkit_data_dir,
        NULL
    );

    WebKitWebContext *ctx = NULL;
    if (manager) {
        ctx = webkit_web_context_new_with_website_data_manager(manager);
        g_object_unref(manager);
        if (ctx && owns_web_context)
            *owns_web_context = true;
    }

    if (!ctx)
        ctx = webkit_web_context_get_default();

    return ctx;
}

static void destroy_profile_web_context(WebKitWebContext **ctx,
                                        bool *owns_web_context)
{
    if (!ctx || !*ctx)
        return;
    if (owns_web_context && *owns_web_context)
        g_object_unref(*ctx);
    *ctx = NULL;
    if (owns_web_context)
        *owns_web_context = false;
}

static void load_profile_layout(LayoutNode **root,
                                LayoutNode **focused,
                                LayoutNode **last_pane)
{
    if (!root || !focused || !last_pane)
        return;

    *root = NULL;
    *focused = NULL;
    *last_pane = NULL;

    if (config_restore_session())
        *root = session_load(focused);

    if (!*root) {
        *root = layout_leaf();
        const char *url = config_startup_url();
        if (!url)
            url = "https://www.youtube.com";
        WebView *web = (WebView *)web_view_create(url);
        attach_webview_to_pane(*root, *root, web);
        *focused = *root;
    }

    if (*focused && (*focused)->view && (*focused)->view->type == VIEW_PANE)
        *last_pane = *focused;
    else
        *last_pane = layout_find_view_type(*root, VIEW_PANE);
}

static bool switch_active_profile(const char *profile_name,
                                  ProfilePaths *active_profile,
                                  WebKitWebContext **ctx,
                                  bool *owns_web_context,
                                  LayoutNode **root,
                                  LayoutNode **focused,
                                  LayoutNode **last_pane)
{
    ProfilePaths next = {0};
    if (!profile_resolve(profile_name, &next)) {
        SDL_Log("profile: invalid '%s'", profile_name ? profile_name : "(null)");
        return false;
    }

    if (root && *root)
        session_save(*root, focused ? *focused : NULL);

    if (root && *root) {
        tab_manager_destroy_all();
        layout_destroy(*root);
        *root = NULL;
    } else {
        tab_manager_destroy_all();
    }

    if (focused)
        *focused = NULL;
    if (last_pane)
        *last_pane = NULL;

    destroy_profile_web_context(ctx, owns_web_context);
    *ctx = create_profile_web_context(&next, owns_web_context);
    if (!*ctx) {
        SDL_Log("profile: failed to initialize web context for '%s'", next.name);
        return false;
    }

    web_view_set_context(*ctx);
    webkit_web_context_set_favicon_database_directory(*ctx, next.webkit_data_dir);

    config_load(next.config_path);
    session_set_path(next.session_path);
    load_profile_layout(root, focused, last_pane);
    if (!root || !*root || !focused || !*focused) {
        SDL_Log("profile: failed to load layout for '%s'", next.name);
        return false;
    }
    session_register(*root, *focused);

    if (active_profile)
        *active_profile = next;

    SDL_Log("Profile: %s", next.name);
    SDL_Log("Config path: %s", next.config_path);
    SDL_Log("Session path: %s", next.session_path);
    SDL_Log("WebKit data dir: %s", next.webkit_data_dir);

    return true;
}

static void overlay_profile_list(const ProfilePaths *active_profile)
{
    char names[64][PROFILE_NAME_MAX];
    int count = profile_list(names, (int)(sizeof(names) / sizeof(names[0])));

    char text[2048];
    size_t pos = 0;
    pos += (size_t)snprintf(text + pos, sizeof(text) - pos,
                            "Profiles:");

    if (count <= 0) {
        (void)snprintf(text + pos, sizeof(text) - pos, " (none)");
        cmd_overlay_set_info(text);
        return;
    }

    for (int i = 0; i < count && pos < sizeof(text); i++) {
        bool is_active = active_profile &&
                         strcmp(names[i], active_profile->name) == 0;
        pos += (size_t)snprintf(text + pos, sizeof(text) - pos,
                                "\n  %s%s",
                                names[i],
                                is_active ? "  *" : "");
    }

    cmd_overlay_set_info(text);
}

static void overlay_current_profile(const ProfilePaths *p)
{
    if (!p)
        return;

    char text[2048];
    snprintf(text, sizeof(text),
             "Profile: %s\n"
             "Config:  %s\n"
             "Session: %s\n"
             "WebKit:  %s",
             p->name,
             p->config_path,
             p->session_path,
             p->webkit_data_dir);

    cmd_overlay_set_info(text);
}


int main(void) {
    gtk_init(NULL,NULL);
    SDL_Init(SDL_INIT_VIDEO);
    if (TTF_Init() != 0) {
        SDL_Log("TTF init failed: %s", TTF_GetError());
    }
    cmd_overlay_init();
    pane_view_init();
    tab_view_init();

    ProfilePaths profile = {0};
    WebKitWebContext *ctx = NULL;
    bool owns_web_context = false;

    SDL_Cursor *cursor_we = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEWE);
    SDL_Cursor *cursor_ns = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENS);
    SDL_Cursor *cursor_arrow = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);
    SDL_Window *win = SDL_CreateWindow(
            "Layout Tree",
            SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED,
            800, 600,
            SDL_WINDOW_RESIZABLE
            );

    SDL_Renderer *ren = SDL_CreateRenderer(
            win, -1,
            SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
            );
    tab_manager_init(ren);

    LayoutNode *focused = NULL;
    LayoutNode *root = NULL;
    LayoutNode *last_pane = NULL;

    if (!switch_active_profile("default",
                               &profile,
                               &ctx,
                               &owns_web_context,
                               &root,
                               &focused,
                               &last_pane))
    {
        fprintf(stderr, "Failed to load default profile\n");
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        SDL_Quit();
        return 1;
    }

    bool running = true;
    SDL_Event e;
    while (running) {

        session_autosave_tick();
        /* ---- GTK pump (non-blocking) ---- */
        while (gtk_events_pending())
            gtk_main_iteration();

        while (SDL_PollEvent(&e)) {

            /* ---------- Split resize (mouse) ---------- */
            if (e.type == SDL_MOUSEBUTTONDOWN &&
                    e.button.button == SDL_BUTTON_LEFT &&
                    input_mode == INPUT_MODE_VIEW)
            {
                SplitHit hit;
                if (hit_test_split(root, e.button.x, e.button.y, &hit)) {
                    resize.active = true;
                    resize.node = hit.node;
                    resize.vertical = hit.vertical;
                    resize.start_x = e.button.x;
                    resize.start_y = e.button.y;
                    resize.start_ratio = hit.node->target_ratio;
                    continue; 
                }
            }
            if (e.type == SDL_QUIT) {
                running = false;
                break;
            }

            if (e.type == SDL_MOUSEMOTION) {

                if (resize.active) {
                    SDL_Rect r = resize.node->rect;

                    if (resize.vertical) {
                        int dx = e.motion.x - resize.start_x;
                        resize.node->target_ratio =
                            CLAMP(resize.start_ratio + (float)dx / r.w, 0.1f, 0.9f);
                    } else {
                        int dy = e.motion.y - resize.start_y;
                        resize.node->target_ratio =
                            CLAMP(resize.start_ratio + (float)dy / r.h, 0.1f, 0.9f);
                    }
                    continue;
                }

                SplitHit hit;
                if (hit_test_split(root, e.motion.x, e.motion.y, &hit)) {
                    SDL_SetCursor(hit.vertical ? cursor_we : cursor_ns);
                } else {
                    SDL_SetCursor(cursor_arrow);
                }
            }
            if (e.type == SDL_MOUSEBUTTONUP &&
                    e.button.button == SDL_BUTTON_LEFT)
            {
                resize.active = false;
                resize.node = NULL;
            }
            /* ---------- Mouse focus (ALWAYS) ---------- */
            if (e.type == SDL_MOUSEBUTTONDOWN) {

                LayoutNode *hit =
                    layout_leaf_at(root, e.button.x, e.button.y);

                if (hit && hit != focused) {
                    focused = hit;

                    if (focused->view &&
                        focused->view->type == VIEW_PANE)
                    {
                        last_pane = focused;
                        WebView *wv = pane_attached(focused);
                        if (wv) {
                            gtk_widget_grab_focus(GTK_WIDGET(wv->wk));
                            input_mode = INPUT_MODE_VIEW;
                        }
                    }
                }
            }

            /* ---------- Mode switching ---------- */
            if (e.type == SDL_KEYDOWN && !e.key.repeat &&
                focused && focused->view && focused->view->type != VIEW_TAB) {

                if (e.key.keysym.sym == SDLK_ESCAPE) {
                    input_mode = INPUT_MODE_WM;
                    continue;
                }

                if (e.key.keysym.sym == SDLK_i &&
                        input_mode == INPUT_MODE_WM &&
                        focused && focused->view &&
                        focused->view->type == VIEW_PANE &&
                        pane_attached(focused))
                {
                    input_mode = INPUT_MODE_VIEW;
                    continue;
                }

                if (input_mode == INPUT_MODE_WM &&
                        e.type == SDL_KEYDOWN &&
                        e.key.keysym.sym == SDLK_SEMICOLON &&
                        (e.key.keysym.mod & KMOD_SHIFT))
                {
                    SDL_Log("colon pressed");
                    cmd_enter();
                    input_mode = INPUT_MODE_CMD;
                    continue;
                }
            }

            /* ---------- VIEW MODE: forward everything ---------- */
            if (input_mode == INPUT_MODE_VIEW &&
                    focused && focused->view &&
                    focused->view->type == VIEW_PANE)
            {
                WebView *wv = pane_attached(focused);
                if (!wv) {
                    input_mode = INPUT_MODE_WM;
                    continue;
                }

                switch (e.type) {

                    case SDL_KEYDOWN:
                    case SDL_KEYUP:
                        web_view_handle_key((View *)wv, &e.key);
                        break;

                    case SDL_MOUSEBUTTONDOWN:
                    case SDL_MOUSEBUTTONUP:
                        web_view_handle_mouse(
                                (View *)wv, &e.button, focused->rect);
                        break;

                    case SDL_MOUSEMOTION:
                        web_view_handle_motion(
                                (View *)wv, &e.motion, focused->rect);
                        break;

                    case SDL_MOUSEWHEEL:
                        web_view_handle_wheel(
                                (View *)wv, &e.wheel, focused->rect);
                        break;
                }
                continue; /* IMPORTANT */
            }
            /* ---------- CMD MODE ONLY---------- */

            if (input_mode == INPUT_MODE_CMD) {
                if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) {
                    cmd_exit();
                    input_mode = INPUT_MODE_WM;
                    continue;
                }

                if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_RETURN) {
                    if (cmd_execute(&root, &focused)) {
                        bool stay = false;

                        if (cmd_take_profiles_list_request()) {
                            overlay_profile_list(&profile);
                            cmd_reset_buffer();
                            stay = true;
                        }

                        if (cmd_take_profile_show_request()) {
                            overlay_current_profile(&profile);
                            cmd_reset_buffer();
                            stay = true;
                        }

                        if (stay)
                            continue;

                        const char *requested_profile = cmd_take_profile_switch();
                        if (requested_profile) {
                            if (!switch_active_profile(requested_profile,
                                                       &profile,
                                                       &ctx,
                                                       &owns_web_context,
                                                       &root,
                                                       &focused,
                                                       &last_pane))
                            {
                                SDL_Log("profile: switch failed for '%s'",
                                        requested_profile);
                            }
                            input_mode = INPUT_MODE_WM;
                            cmd_exit();
                            continue;
                        }

                        if (focused && focused->view &&
                            focused->view->type == VIEW_PANE &&
                            pane_attached(focused))
                        {
                            input_mode = INPUT_MODE_VIEW;
                        } else {
                            input_mode = INPUT_MODE_WM;
                        }
                        if (focused && focused->view &&
                            focused->view->type == VIEW_PANE)
                        {
                            last_pane = focused;
                        }
                    } else {
                        input_mode = INPUT_MODE_WM;
                    }

                    cmd_exit();
                    continue;
                }

                if (e.type == SDL_KEYDOWN) {
                    cmd_handle_key(&e.key);
                    continue;
                }

                if (e.type == SDL_TEXTINPUT) {
                    cmd_handle_text(e.text.text);
                    continue;
                }

                continue; 
            }

            /* ---------- WM MODE ONLY ---------- */
            if (input_mode == INPUT_MODE_WM &&
                    e.type == SDL_KEYDOWN &&
                    e.key.repeat == 0)
            {
                if (e.key.keysym.sym == SDLK_h &&
                    (e.key.keysym.mod & KMOD_SHIFT))
                {
                    hide_webview_from_pane(&root, &focused, &last_pane);
                    input_mode = INPUT_MODE_WM;
                    continue;
                }

                if (focused &&
                        focused->view &&
                        focused->view->type == VIEW_TAB)
                {
                    tab_view_handle_key(focused->view, &e.key);

                    TabAction act = tab_view_take_action(focused->view);

                    switch (act) {

                        case TAB_EXIT: {
                                           LayoutNode *tab_leaf = focused;

                                           View *tab_view = tab_leaf->view;
                                           tab_leaf->view = NULL;
                                           if (tab_view)
                                               tab_view->destroy(tab_view);

                                           LayoutNode *new_focus = layout_close_leaf(tab_leaf, &root);
                                           focused = layout_leaf_from_node(new_focus);
                                           if (!focused)
                                               focused = layout_first_leaf(root);

                                           input_mode = INPUT_MODE_WM;
                                           if (focused && focused->view &&
                                               focused->view->type == VIEW_PANE)
                                           {
                                               last_pane = focused;
                                           }
                                           continue;
                                       }

                        case TAB_ATTACH: {
                                             WebView *web =
                                                 tab_manager_webview_at(
                                                     tab_view_selected(focused->view));
                                             if (!web)
                                                 break;

                                             LayoutNode *target =
                                                 pick_target_pane(focused, last_pane, root);
                                             if (target) {
                                                 attach_webview_to_pane(root, target, web);
                                                 last_pane = target;
                                             }
                                             break;
                                         }

                        case TAB_CLOSE:
                                         tab_view_close_selected(root, focused->view);
                                         continue;

                        default:
                                         continue;
                    }
                }

                /* ========== WEBVIEW COMMANDS ========== */
                Action action = config_action_for_key(e.key.keysym.sym);

                if (focused && focused->view &&
                        focused->view->type == VIEW_PANE)
                {
                    WebView *wv = pane_attached(focused);
                    if (wv) {
                        if (action == ACTION_WEB_BACK) {
                            web_view_undo((View *)wv);
                            continue;
                        }

                        if (action == ACTION_WEB_RELOAD &&
                                !(e.key.keysym.mod & KMOD_CTRL)) {
                            web_view_reload((View *)wv);
                            continue;
                        }

                        if (action == ACTION_WEB_RELOAD &&
                                (e.key.keysym.mod & KMOD_CTRL)) {
                            web_view_redo((View *)wv);
                            continue;
                        }

                        if (action == ACTION_WEB_STOP) {
                            web_view_stop((View *)wv);
                            continue;
                        }
                    }
                }

                /* ========== RESIZE (CTRL + HJKL) ========== */
                if (e.key.keysym.mod & KMOD_CTRL) {
                    switch (e.key.keysym.sym) {
                        case SDLK_h: layout_resize_relative(focused, DIR_LEFT);  break;
                        case SDLK_l: layout_resize_relative(focused, DIR_RIGHT); break;
                        case SDLK_k: layout_resize_relative(focused, DIR_UP);    break;
                        case SDLK_j: layout_resize_relative(focused, DIR_DOWN);  break;
                    }
                    continue;
                }

                /* ========== WINDOW MANAGER ACTIONS ========== */
                const char *url = config_startup_url(); 
                if(!url) url = "https://www.youtube.com";
                switch (action) {

                    case ACTION_SPLIT_VERTICAL:
                        focused = layout_split_leaf(
                                focused, SPLIT_VERTICAL, 0.5f, &root);
                        if (focused && focused->view &&
                            focused->view->type == VIEW_PANE)
                        {
                            last_pane = focused;
                        }
                        break;

                    case ACTION_SPLIT_HORIZONTAL:
                        focused = layout_split_leaf(
                                focused, SPLIT_HORIZONTAL, 0.5f, &root);
                        if (focused && focused->view &&
                            focused->view->type == VIEW_PANE)
                        {
                            last_pane = focused;
                        }
                        break;

                    case ACTION_CLOSE_PANE:
                        SDL_Log("just merged panes");
                        if (focused && focused->view) {
                            if (focused->view->type == VIEW_PANE) {
                                detach_webview_from_pane(focused);
                            } else {
                                focused->view->destroy(focused->view);
                                focused->view = NULL;
                            }
                        }

                        LayoutNode *new_focus = layout_close_leaf(focused, &root);
                        focused = layout_leaf_from_node(new_focus);
                        if (!focused)
                            focused = layout_first_leaf(root);
                        if (focused && focused->view &&
                            focused->view->type == VIEW_PANE)
                        {
                            last_pane = focused;
                        } else {
                            last_pane = layout_find_view_type(root, VIEW_PANE);
                        }
                        break;

                    case ACTION_HIDE_WEBVIEW:
                        hide_webview_from_pane(&root, &focused, &last_pane);
                        input_mode = INPUT_MODE_WM;
                        break;

                    case ACTION_FOCUS_LEFT:
                        focused = focus_move(root, focused, DIR_LEFT);
                        if (focused && focused->view &&
                            focused->view->type == VIEW_PANE)
                        {
                            last_pane = focused;
                        }
                        break;

                    case ACTION_FOCUS_RIGHT:
                        focused = focus_move(root, focused, DIR_RIGHT);
                        if (focused && focused->view &&
                            focused->view->type == VIEW_PANE)
                        {
                            last_pane = focused;
                        }
                        break;

                    case ACTION_FOCUS_UP:
                        focused = focus_move(root, focused, DIR_UP);
                        if (focused && focused->view &&
                            focused->view->type == VIEW_PANE)
                        {
                            last_pane = focused;
                        }
                        break;

                    case ACTION_FOCUS_DOWN:
                        focused = focus_move(root, focused, DIR_DOWN);
                        if (focused && focused->view &&
                            focused->view->type == VIEW_PANE)
                        {
                            last_pane = focused;
                        }
                        break;

                    case ACTION_OPEN_WEBVIEW:
                        {
                            LayoutNode *target =
                                pick_target_pane(focused, last_pane, root);
                            cmd_open_webview(target, root, url);
                            if (target)
                                last_pane = target;
                        }
                        break;

                    case ACTION_ENTER_CMD:
                        cmd_enter();
                        input_mode = INPUT_MODE_CMD;
                        break;

                    case ACTION_TAB_ENTER: {
                                               int w, h;
                                               SDL_GetWindowSize(win, &w, &h);

                                               LayoutNode *tab_leaf = layout_insert_tabview(&root, w);
                                               if (tab_leaf) {
                                                   focused = tab_leaf;
                                                   input_mode = INPUT_MODE_WM;
                                               }
                                               break;
                                           }

                    default:
                                           break;
                }
            }
        }

        layout_animate(root);
        int w, h;
        SDL_GetWindowSize(win, &w, &h);

        layout_assign(root, (SDL_Rect){0, 0, w, h});

        SDL_SetRenderDrawColor(ren, 30, 30, 30, 255);
        SDL_RenderClear(ren);

        render_layout(ren, root, focused);
        if (input_mode == INPUT_MODE_CMD)
            render_cmd_overlay(ren, w, h);

        SDL_RenderPresent(ren);
    }
    session_save(root,focused);
    tab_manager_destroy_all();
    layout_destroy(root);
    destroy_profile_web_context(&ctx, &owns_web_context);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}

static bool is_text_key(SDL_Keycode sym)
{
    return
        (sym >= SDLK_a && sym <= SDLK_z) ||
        (sym >= SDLK_0 && sym <= SDLK_9) ||
        sym == SDLK_SPACE ||
        sym == SDLK_RETURN ||
        sym == SDLK_BACKSPACE;
}
