#pragma once
#include <SDL2/SDL.h>
#include <stdbool.h>

typedef struct View View;

struct View {
    void (*draw)(View *self,
                 SDL_Renderer *renderer,
                 SDL_Rect rect,
                 bool focused);
    void (*destroy)(View *self);
};

