
#include <SDL2/SDL.h>
#include <SDL2/SDL_events.h>
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
                    if (e.key.keysym.sym == SDLK_v && !v_down ) {
                        v_down=true;
                        SDL_Log("Inside v");
                        focused = layout_split_leaf(
                            focused,
                            SPLIT_VERTICAL,
                            0.5f,
                            &root 
                        );
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

        render_layout(ren, root);

        SDL_RenderPresent(ren);
    }

    layout_destroy(root);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
}
