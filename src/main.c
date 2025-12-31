#include "focus.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_keycode.h>
#include <SDL2/SDL_render.h>
#include <stdbool.h>
#include <stdio.h>
#include "layout.h"
#include "render.h"

bool v_down = false;
int main(void) {
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
    LayoutNode *focused = root;

    bool running = true;
    SDL_Event e;

    while (running) {
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
                case SDL_QUIT:
                    running = false;
                    break;

                case SDL_KEYDOWN:
                    if (e.key.repeat) break;

                    switch (e.key.keysym.sym) {

                        case SDLK_v:
                            if (!v_down) {
                                v_down = true;
                                SDL_Log("Split vertical");
                                focused = layout_split_leaf(
                                        focused,
                                        SPLIT_VERTICAL,
                                        0.5f,
                                        &root
                                        );
                            }
                            break;
                        case SDLK_s:
                            focused = layout_split_leaf(
                                    focused,
                                    SPLIT_HORIZONTAL,
                                    0.5f,
                                    &root
                                    );
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
                    break;

                case SDL_KEYUP:
                    if (e.key.keysym.sym == SDLK_v) {
                        v_down = false;
    }
    break;
            }
        }

        int w, h;
        SDL_GetWindowSize(win, &w, &h);

        layout_assign(root, (SDL_Rect){0, 0, w, h});

        SDL_SetRenderDrawColor(ren, 30, 30, 30, 255);
        SDL_RenderClear(ren);

        render_layout(ren, root,focused);

        SDL_RenderPresent(ren);
    }

    layout_destroy(root);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
}
