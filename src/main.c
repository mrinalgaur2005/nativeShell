#include "command.h"
#include "command_overlay.h"
#include "focus.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_keycode.h>
#include <SDL2/SDL_log.h>
#include <SDL2/SDL_render.h>
#include <sched.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include "gtk/gtk.h"
#include "layout.h"
#include "render.h"
#include "view.h"
#include "web_view.h"

typedef struct {
    bool active;
    LayoutNode *node;
    bool vertical;
    int start_x, start_y;
    float start_ratio;
} ResizeState;

static ResizeState resize = {0};


typedef enum{
    INPUT_MODE_WM, //for hjkl (moving), s(splitting horiontalu),v(splitting vertically) x(merging) 
                   //esc for entering this
    INPUT_MODE_VIEW,//press i to go insertmode(vim inspired obv) for using browser
    INPUT_MODE_CMD //uses for command mode again vim inspired
} InputMode;

InputMode input_mode = INPUT_MODE_WM;

static bool is_text_key(SDL_Keycode sym);
bool v_down = false;

void cmd_open_webview(LayoutNode *leaf){
    if(!leaf || !leaf->view) return;
    if(leaf->view->type == VIEW_WEB) return;
    leaf->view->destroy(leaf->view);
    leaf->view=web_view_create("https://www.youtube.com");
    input_mode=INPUT_MODE_VIEW;

}


int main(void) {
    gtk_init(NULL,NULL);
    SDL_Init(SDL_INIT_VIDEO);
    cmd_overlay_init();

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

    LayoutNode *root = layout_leaf(1);
    root->view = web_view_create(
            "https://www.youtube.com"
            );
    LayoutNode *focused = root;
    bool running = true;
    SDL_Event e;

    while (running) {

        /* ---- GTK pump (non-blocking) ---- */
        while (gtk_events_pending())
            gtk_main_iteration();

        while (SDL_PollEvent(&e)) {

            /* ---------- Split resize (mouse) ---------- */
            if (e.type == SDL_MOUSEBUTTONDOWN &&
                    e.button.button == SDL_BUTTON_LEFT)
            {
                SplitHit hit;
                if (hit_test_split(root, e.button.x, e.button.y, &hit)) {
                    resize.active = true;
                    resize.node = hit.node;
                    resize.vertical = hit.vertical;
                    resize.start_x = e.button.x;
                    resize.start_y = e.button.y;
                    resize.start_ratio = hit.node->ratio;
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
                        resize.node->ratio =
                            CLAMP(resize.start_ratio + (float)dx / r.w, 0.1f, 0.9f);
                    } else {
                        int dy = e.motion.y - resize.start_y;
                        resize.node->ratio =
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
                            focused->view->type == VIEW_WEB)
                    {
                        WebView *wv = (WebView *)focused->view;
                        gtk_widget_grab_focus(GTK_WIDGET(wv->wk));
                        input_mode=INPUT_MODE_WM;
                    }
                }
            }

            /* ---------- Mode switching ---------- */
            if (e.type == SDL_KEYDOWN && !e.key.repeat) {

                if (e.key.keysym.sym == SDLK_ESCAPE) {
                    input_mode = INPUT_MODE_WM;
                    continue;
                }

                if (e.key.keysym.sym == SDLK_i &&
                        input_mode == INPUT_MODE_WM &&
                        focused && focused->view &&
                        focused->view->type == VIEW_WEB)
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
                    focused->view->type == VIEW_WEB)
            {
                switch (e.type) {

                    case SDL_KEYDOWN:
                    case SDL_KEYUP:
                        web_view_handle_key(focused->view, &e.key);
                        break;

                    case SDL_MOUSEBUTTONDOWN:
                    case SDL_MOUSEBUTTONUP:
                        web_view_handle_mouse(
                                focused->view, &e.button, focused->rect);
                        break;

                    case SDL_MOUSEMOTION:
                        web_view_handle_motion(
                                focused->view, &e.motion, focused->rect);
                        break;

                    case SDL_MOUSEWHEEL:
                        web_view_handle_wheel(
                                focused->view, &e.wheel, focused->rect);
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
                    if (cmd_execute(focused))
                        input_mode = INPUT_MODE_VIEW;
                    else
                        input_mode = INPUT_MODE_WM;

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
                    e.type == SDL_KEYDOWN && !e.key.repeat)
            {
                if (focused && focused->type != NODE_LEAF)
                    focused = layout_first_leaf(focused);

                switch (e.key.keysym.sym) {

                    case SDLK_v:
                        focused = layout_split_leaf(
                                focused, SPLIT_VERTICAL, 0.5f, &root);
                        break;

                    case SDLK_s:
                        focused = layout_split_leaf(
                                focused, SPLIT_HORIZONTAL, 0.5f, &root);
                        break;

                    case SDLK_x:
                        focused = layout_close_leaf(focused, &root);
                        break;

                    case SDLK_h:
                        focused = focus_move(root, focused, DIR_LEFT);
                        break;

                    case SDLK_l:
                        focused = focus_move(root, focused, DIR_RIGHT);
                        break;

                    case SDLK_k:
                        focused = focus_move(root, focused, DIR_UP);
                        break;

                    case SDLK_j:
                        focused = focus_move(root, focused, DIR_DOWN);
                        break;

                    case SDLK_o:
                        
                        cmd_open_webview(focused);
                        break;
                }
            }
        }

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

    layout_destroy(root);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
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
