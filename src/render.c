#include "render.h"

/* Internal helper: draw one leaf */
static void draw_leaf(LayoutNode *node, void *userdata)
{
    SDL_Renderer *r = userdata;

    /* Color by id so splits are visible */
    SDL_SetRenderDrawColor(
        r,
        (node->id * 70) % 255,
        (node->id * 130) % 255,
        (node->id * 200) % 255,
        255
    );
    SDL_RenderFillRect(r, &node->rect);

    /* Border */
    SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
    SDL_RenderDrawRect(r, &node->rect);
}

void render_layout(SDL_Renderer *renderer, LayoutNode *root)
{
    layout_traverse_leaves(root, draw_leaf, renderer);
}
