#include "render.h"

static void draw_leaf(LayoutNode *node, void *userdata)
{
    struct {
        SDL_Renderer *r;
        LayoutNode *focused;
    } *ctx = userdata;

    SDL_Renderer *r = ctx->r;

    if (node == ctx->focused) {
        /* Focused: brighter */
        SDL_SetRenderDrawColor(r, 200, 200, 200, 255);
    } else {
        SDL_SetRenderDrawColor(
            r,
            (node->id * 70) % 255,
            (node->id * 130) % 255,
            (node->id * 200) % 255,
            255
        );
    }

    SDL_RenderFillRect(r, &node->rect);

    /* Border */
    SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
    SDL_RenderDrawRect(r, &node->rect);
}

void render_layout(SDL_Renderer *r, LayoutNode *root, LayoutNode *focused)
{
    struct {
        SDL_Renderer *r;
        LayoutNode *focused;
    } ctx = { r, focused };

    layout_traverse_leaves(root, draw_leaf, &ctx);
}
