#include <SDL2/SDL_error.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_stdinc.h>
#include <SDL2/SDL_timer.h>
#include <SDL2/SDL_video.h>
#include <stdio.h>
#include <stdbool.h>
#include <SDL2/SDL.h>
#include <stdlib.h>
#include <wchar.h>

#define WINDOW_TITLE "Native Shell"
#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600

struct Window{
    SDL_Window *window;
    SDL_Renderer *renderer;
};
void game_cleanup(struct Window *Window ,int exit_status);
bool sdl_init(struct Window *Window);
int main(){
    struct Window window = {
        .window = NULL,
        .renderer = NULL,
    };
    if(sdl_init(&window)){
        game_cleanup(&window,EXIT_FAILURE);
        printf("All BAD");
        exit(1);
    }

    while (true){
        SDL_Event event;

        while(SDL_PollEvent(&event)){
            switch (event.type) {
                case SDL_QUIT:
                    game_cleanup(&window,EXIT_SUCCESS);
                    break;
                default:
                    break;
            }
        }
        SDL_RenderClear(window.renderer);
        SDL_RenderPresent(window.renderer);
        SDL_Delay(16);
    }
    game_cleanup(&window,EXIT_SUCCESS);
    printf("All good");
    return 0;
}

void game_cleanup(struct Window *Window,int exit_status){
    SDL_DestroyRenderer(Window->renderer);
    SDL_DestroyWindow(Window->window);
    SDL_Quit();
    exit(exit_status);
}

bool sdl_init(struct Window *Window){
    if(SDL_Init(SDL_INIT_EVERYTHING)!=0){
        fprintf(stderr,"Error in init %s\n", SDL_GetError());
        return true;
    }
    Window->window = SDL_CreateWindow(WINDOW_TITLE,SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,SCREEN_WIDTH,SCREEN_HEIGHT,0);
    if(!Window->window){
        fprintf(stderr, "Error creating window%s\n", SDL_GetError());
        return true;
    }
    Window->renderer=SDL_CreateRenderer(Window->window,-1,0); 
    if(!Window->renderer){
        fprintf(stderr, "Error creating window%s\n", SDL_GetError());
        return true;
    }
    return false;
}
