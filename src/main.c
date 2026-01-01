#include "focus.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_keycode.h>
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

typedef enum{
    INPUT_MODE_WM, //for hjkl (moving), s(splitting horiontalu),v(splitting vertically) x(merging) 
                   //esc for entering this
    INPUT_MODE_VIEW,//press i to go insertmode(vim inspired obv) for using browser
} InputMode;

InputMode input_mode = INPUT_MODE_WM;

static bool is_text_key(SDL_Keycode sym);
bool v_down = false;
int main(void) {
    gtk_init(NULL,NULL);
    SDL_Init(SDL_INIT_VIDEO);

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

    while (gtk_events_pending()) {
        gtk_main_iteration();
    }

    while (SDL_PollEvent(&e)) {

        if (e.type == SDL_QUIT) {
            running = false;
            break;
        }

        if (e.type == SDL_KEYDOWN && !e.key.repeat) {
            if (e.key.keysym.sym == SDLK_ESCAPE) {
                input_mode = INPUT_MODE_WM;
                continue;
            }
            if (e.key.keysym.sym == SDLK_i && input_mode == INPUT_MODE_WM 
                    && focused && focused->view && focused->view->type == VIEW_WEB)
            {
                input_mode = INPUT_MODE_VIEW;
                continue;
            }
        }

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
                web_view_handle_mouse(focused->view, &e.button);
                break;

            case SDL_MOUSEMOTION:
                web_view_handle_motion(focused->view, &e.motion);
                break;

            case SDL_MOUSEWHEEL:
                web_view_handle_wheel(focused->view, &e.wheel);
                break;
            }

            continue;
        }
        if (e.type == SDL_KEYDOWN && !e.key.repeat) {

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
            }
        }
    }

    int w, h;
    SDL_GetWindowSize(win, &w, &h);
    layout_assign(root, (SDL_Rect){0, 0, w, h});

    SDL_SetRenderDrawColor(ren, 30, 30, 30, 255);
    SDL_RenderClear(ren);
    render_layout(ren, root, focused);
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
