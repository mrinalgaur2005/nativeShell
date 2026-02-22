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

    SDL_Color border = {34, 34, 40, 255};
    SDL_Color focus = {235, 235, 240, 220};
    SDL_Color focus_accent = {120, 170, 255, 220};

    SDL_SetRenderDrawBlendMode(ctx->renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(
        ctx->renderer,
        border.r, border.g, border.b, border.a
    );
    SDL_RenderDrawRect(ctx->renderer, &node->rect);

    if (node == ctx->focused) {
        SDL_Rect outer = node->rect;
        SDL_Rect mid = node->rect;
        SDL_Rect inner = node->rect;
        mid.x += 1;
        mid.y += 1;
        mid.w -= 2;
        mid.h -= 2;
        inner.x += 2;
        inner.y += 2;
        inner.w -= 4;
        inner.h -= 4;

        SDL_SetRenderDrawColor(
            ctx->renderer,
            focus.r, focus.g, focus.b, focus.a
        );
        SDL_RenderDrawRect(ctx->renderer, &outer);
        if (mid.w > 0 && mid.h > 0)
            SDL_RenderDrawRect(ctx->renderer, &mid);
        if (inner.w > 0 && inner.h > 0)
            SDL_RenderDrawRect(ctx->renderer, &inner);

        SDL_SetRenderDrawColor(
            ctx->renderer,
            focus_accent.r, focus_accent.g, focus_accent.b, focus_accent.a
        );
        SDL_Rect top = { outer.x + 1, outer.y + 1, outer.w - 2, 2 };
        SDL_Rect left = { outer.x + 1, outer.y + 1, 2, outer.h - 2 };
        if (top.w > 0 && top.h > 0)
            SDL_RenderFillRect(ctx->renderer, &top);
        if (left.w > 0 && left.h > 0)
            SDL_RenderFillRect(ctx->renderer, &left);

        SDL_Rect corner = { outer.x + 1, outer.y + 1, 6, 6 };
        SDL_RenderFillRect(ctx->renderer, &corner);
    }
}

void render_layout(SDL_Renderer *r, LayoutNode *root, LayoutNode *focused)
{
    struct {
        SDL_Renderer *r;
        LayoutNode *focused;
    } ctx = { r, focused };

    layout_traverse_leaves(root, draw_leaf, &ctx);
}
