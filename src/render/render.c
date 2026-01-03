#include "render/render.h"

#define BORDER_THIN 1
#define BORDER_FOCUSED 3
#define GAP 4  

static void draw_leaf(LayoutNode *node, void *userdata)
{
    struct {
        SDL_Renderer *renderer;
        LayoutNode *focused;
    } *ctx = userdata;

    if (!node->view)
        return;

    /* Clip to leaf rect */
    SDL_RenderSetClipRect(ctx->renderer, &node->rect);

    node->view->draw(
        node->view,
        ctx->renderer,
        node->rect,
        node == ctx->focused
    );

    SDL_RenderSetClipRect(ctx->renderer, NULL);
}

void render_layout(SDL_Renderer *r, LayoutNode *root, LayoutNode *focused)
{
    struct {
        SDL_Renderer *r;
        LayoutNode *focused;
    } ctx = { r, focused };

    layout_traverse_leaves(root, draw_leaf, &ctx);
}
