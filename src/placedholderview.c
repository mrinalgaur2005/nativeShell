
#include "placedholderview.h"
#include <stdlib.h>

typedef struct {
    View base;
    SDL_Color color;
} PlaceholderView;

static void draw(View *v,
                 SDL_Renderer *r,
                 SDL_Rect rect,
                 bool focused)
{
    PlaceholderView *pv = (PlaceholderView *)v;

    SDL_SetRenderDrawColor(
        r,
        pv->color.r,
        pv->color.g,
        pv->color.b,
        255
    );
    SDL_RenderFillRect(r, &rect);

    /* Border */
    SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
    SDL_RenderDrawRect(r, &rect);

    if (focused) {
        SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
        SDL_RenderDrawRect(r, &rect);
        SDL_RenderDrawRect(r, &rect); /* double-draw for thickness */
    }
}

static void destroy(View *v)
{
    free(v);
}

View *placeholder_view_create(SDL_Color color)
{
    PlaceholderView *pv = calloc(1, sizeof(*pv));
    pv->base.draw = draw;
    pv->base.destroy = destroy;
    pv->color = color;
    return (View *)pv;
}
