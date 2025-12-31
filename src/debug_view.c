
#include "debug_view.h"
#include <stdlib.h>
#include <stdio.h>

typedef struct {
    View base;
    int id;
} DebugView;

static void draw(View *v,
                 SDL_Renderer *r,
                 SDL_Rect rect,
                 bool focused)
{
    DebugView *dv = (DebugView *)v;

    SDL_SetRenderDrawColor(r,
        focused ? 200 : 100,
        focused ? 200 : 100,
        focused ? 200 : 100,
        255
    );
    SDL_RenderFillRect(r, &rect);

    SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
    SDL_RenderDrawRect(r, &rect);

    /* Text rendering comes later – for now this is enough */
}

static void destroy(View *v)
{
    free(v);
}

View *debug_view_create(int id)
{
    DebugView *dv = calloc(1, sizeof(*dv));
    dv->base.draw = draw;
    dv->base.destroy = destroy;
    dv->id = id;
    return (View *)dv;
}
