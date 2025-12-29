#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

#define WINDOW_TITLE "Native Shell"
#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600
#define IMAGE_FLAGS IMG_INIT_PNG

struct Window {
    SDL_Window *window;
    SDL_Renderer *renderer;
    int width;
    int height;
};

void game_cleanup(struct Window *window, int exit_status);
bool sdl_init(struct Window *window);

int main(void) {
    struct Window window = {0};

    if (!sdl_init(&window)) {
        game_cleanup(&window, EXIT_FAILURE);
    }

    bool running = true;
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            switch (event.type) {

                case SDL_QUIT:
                    running = false;
                    break;

                case SDL_WINDOWEVENT:
                    if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                        window.width  = event.window.data1;
                        window.height = event.window.data2;

                        SDL_RenderSetViewport(
                            window.renderer,
                            &(SDL_Rect){0, 0, window.width, window.height}
                        );

                        printf("Resized to %dx%d\n",
                               window.width, window.height);
                    }
                    break;
            }
        }

        SDL_RenderClear(window.renderer);
        SDL_RenderPresent(window.renderer);
    }

    game_cleanup(&window, EXIT_SUCCESS);
    return 0;
}

bool sdl_init(struct Window *window) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL init error: %s\n", SDL_GetError());
        return false;
    }

    if ((IMG_Init(IMAGE_FLAGS) & IMAGE_FLAGS) != IMAGE_FLAGS) {
        fprintf(stderr, "IMG init error: %s\n", IMG_GetError());
        return false;
    }

    window->window = SDL_CreateWindow(
        WINDOW_TITLE,
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        SCREEN_WIDTH,
        SCREEN_HEIGHT,
        SDL_WINDOW_RESIZABLE
    );

    if (!window->window) {
        fprintf(stderr, "Window error: %s\n", SDL_GetError());
        return false;
    }

    window->renderer = SDL_CreateRenderer(
        window->window,
        -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );

    if (!window->renderer) {
        fprintf(stderr, "Renderer error: %s\n", SDL_GetError());
        return false;
    }

    window->width  = SCREEN_WIDTH;
    window->height = SCREEN_HEIGHT;

    return true;
}

void game_cleanup(struct Window *window, int exit_status) {
    if (window->renderer) SDL_DestroyRenderer(window->renderer);
    if (window->window) SDL_DestroyWindow(window->window);
    IMG_Quit();
    SDL_Quit();
    exit(exit_status);
}
