#pragma once
#include <SDL2/SDL.h>
#include <stdbool.h>

typedef enum {
    VIEW_PLACEHOLDER,
    VIEW_DEBUG,
    VIEW_WEB,
    VIEW_TAB
} ViewType;
typedef struct View View;

struct View {
    ViewType type;
    void (*draw)(View *self,
                 SDL_Renderer *renderer,
                 SDL_Rect rect,
                 bool focused);
    void (*destroy)(View *self);
};

