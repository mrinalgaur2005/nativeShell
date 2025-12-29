#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_pixels.h>
#include <SDL2/SDL_render.h>
#include <stddef.h>
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


enum UIRegion{
    UI_SIDEBAR,
    UI_CONTENT,
    UI_STATUSBAR,
};
struct Rect{
    float x,y,w,h;
    SDL_Color color;
    enum UIRegion region;
};
struct Rect ui_rects[] = {
    // Left sidebar (20% width)
    {0.0f, 0.0f, 0.2f, 0.9f, {40, 40, 40, 255}, UI_SIDEBAR},

    // Main content area
    {0.2f, 0.0f, 0.8f, 0.9f, {70, 70, 70, 255}, UI_CONTENT},

    // Bottom status bar (10% height)
    {0.0f, 0.9f, 1.0f, 0.1f, {30, 30, 30, 255}, UI_STATUSBAR}
};

void game_cleanup(struct Window *window, int exit_status);
bool sdl_init(struct Window *window);
void render_rects(SDL_Renderer *renderer,struct Rect *rect, size_t count,int win_w,int win_h);


int main(void) {
    struct Window window = {0};

    if (!sdl_init(&window)) {
        game_cleanup(&window, EXIT_FAILURE);
    }

    struct Rect rects[] = {
        {0.1f, 0.1f, 0.3f, 0.2f, {255, 0, 0, 255}},
        {0.5f, 0.2f, 0.4f, 0.3f, {0, 255, 0, 255}},
        {0.2f, 0.6f, 0.5f, 0.3f, {0, 0, 255, 255}}
    };
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

        /* -------- RENDER PHASE -------- */

        SDL_SetRenderDrawColor(window.renderer, 20, 20, 20, 255);
        SDL_RenderClear(window.renderer);

        render_rects(
                window.renderer,
                ui_rects,
                sizeof(ui_rects) / sizeof(ui_rects[0]),
                window.width,
                window.height
                );

        SDL_RenderPresent(window.renderer);
    }
    game_cleanup(&window,EXIT_SUCCESS);
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

void render_rects(SDL_Renderer *renderer,
                  struct Rect *rects,
                  size_t count,
                  int win_w,
                  int win_h)
{
    for (size_t i = 0; i < count; i++) {
        SDL_Rect r = {
            .x = (int)(rects[i].x * win_w),
            .y = (int)(rects[i].y * win_h),
            .w = (int)(rects[i].w * win_w),
            .h = (int)(rects[i].h * win_h)
        };

        SDL_SetRenderDrawColor(
            renderer,
            rects[i].color.r,
            rects[i].color.g,
            rects[i].color.b,
            rects[i].color.a
        );

        SDL_RenderFillRect(renderer, &r);
    }
}

void game_cleanup(struct Window *window, int exit_status) {
    if (window->renderer) SDL_DestroyRenderer(window->renderer);
    if (window->window) SDL_DestroyWindow(window->window);
    IMG_Quit();
    SDL_Quit();
    exit(exit_status);
}
