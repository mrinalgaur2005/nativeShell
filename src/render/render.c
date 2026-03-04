#include "render/render.h"
#include "render/render_fx.h"
#include <math.h>

/* ------------------------------------------------------------------ */
/*  Drawing primitives for rounded rectangles and corner covers        */
/* ------------------------------------------------------------------ */

static void draw_horizontal_line(SDL_Renderer *r, int x1, int x2, int y)
{
    SDL_RenderDrawLine(r, x1, y, x2, y);
}

/*
 * Fill a quarter-circle "cover" at one corner of `rect` with the
 * background color.  This masks the content behind rounded corners.
 *
 * corner: 0=TL  1=TR  2=BL  3=BR
 */
static void fill_corner_cover(SDL_Renderer *r,
                               SDL_Rect rect,
                               int radius,
                               int corner)
{
    if (radius <= 0) return;

    int cx, cy;
    switch (corner) {
    case 0: cx = rect.x + radius;              cy = rect.y + radius;               break;
    case 1: cx = rect.x + rect.w - radius - 1; cy = rect.y + radius;               break;
    case 2: cx = rect.x + radius;              cy = rect.y + rect.h - radius - 1;  break;
    case 3: cx = rect.x + rect.w - radius - 1; cy = rect.y + rect.h - radius - 1;  break;
    default: return;
    }

    /* Midpoint circle — fill scanlines outside the arc but inside the
       corner square with the background color. */
    int px = 0, py = radius;
    int d = 1 - radius;

    while (px <= py) {
        /* For each scanline row affected by the circle octant, fill
           the pixels between the rect edge and the circle boundary. */
        switch (corner) {
        case 0: /* top-left */
            draw_horizontal_line(r, rect.x, cx - py, cy - px);
            draw_horizontal_line(r, rect.x, cx - px, cy - py);
            break;
        case 1: /* top-right */
            draw_horizontal_line(r, cx + py, rect.x + rect.w - 1, cy - px);
            draw_horizontal_line(r, cx + px, rect.x + rect.w - 1, cy - py);
            break;
        case 2: /* bottom-left */
            draw_horizontal_line(r, rect.x, cx - py, cy + px);
            draw_horizontal_line(r, rect.x, cx - px, cy + py);
            break;
        case 3: /* bottom-right */
            draw_horizontal_line(r, cx + py, rect.x + rect.w - 1, cy + px);
            draw_horizontal_line(r, cx + px, rect.x + rect.w - 1, cy + py);
            break;
        }

        px++;
        if (d < 0) {
            d += 2 * px + 1;
        } else {
            py--;
            d += 2 * (px - py) + 1;
        }
    }
}

/*
 * Draw a quarter-circle arc (outline only).
 * corner: 0=TL  1=TR  2=BL  3=BR
 */
static void draw_quarter_arc(SDL_Renderer *r,
                              int cx, int cy,
                              int radius,
                              int corner)
{
    int px = 0, py = radius;
    int d = 1 - radius;

    while (px <= py) {
        int x1, y1, x2, y2;
        switch (corner) {
        case 0: x1 = cx - py; y1 = cy - px; x2 = cx - px; y2 = cy - py; break;
        case 1: x1 = cx + py; y1 = cy - px; x2 = cx + px; y2 = cy - py; break;
        case 2: x1 = cx - py; y1 = cy + px; x2 = cx - px; y2 = cy + py; break;
        case 3: x1 = cx + py; y1 = cy + px; x2 = cx + px; y2 = cy + py; break;
        default: return;
        }

        SDL_RenderDrawPoint(r, x1, y1);
        SDL_RenderDrawPoint(r, x2, y2);

        px++;
        if (d < 0) {
            d += 2 * px + 1;
        } else {
            py--;
            d += 2 * (px - py) + 1;
        }
    }
}

static void draw_rounded_border(SDL_Renderer *r,
                                 SDL_Rect rect,
                                 int radius,
                                 SDL_Color color,
                                 int thickness)
{
    SDL_SetRenderDrawColor(r, color.r, color.g, color.b, color.a);

    for (int t = 0; t < thickness; t++) {
        SDL_Rect inset = {
            rect.x + t,  rect.y + t,
            rect.w - 2*t, rect.h - 2*t
        };
        if (inset.w <= 0 || inset.h <= 0) break;

        int rad = radius - t;
        if (rad < 0) rad = 0;

        /* straight edges (excluding corner arcs) */
        SDL_RenderDrawLine(r, inset.x + rad, inset.y,
                              inset.x + inset.w - rad - 1, inset.y);
        SDL_RenderDrawLine(r, inset.x + rad, inset.y + inset.h - 1,
                              inset.x + inset.w - rad - 1, inset.y + inset.h - 1);
        SDL_RenderDrawLine(r, inset.x, inset.y + rad,
                              inset.x, inset.y + inset.h - rad - 1);
        SDL_RenderDrawLine(r, inset.x + inset.w - 1, inset.y + rad,
                              inset.x + inset.w - 1, inset.y + inset.h - rad - 1);

        if (rad > 0) {
            draw_quarter_arc(r, inset.x + rad,              inset.y + rad,               rad, 0);
            draw_quarter_arc(r, inset.x + inset.w - rad - 1, inset.y + rad,               rad, 1);
            draw_quarter_arc(r, inset.x + rad,              inset.y + inset.h - rad - 1,  rad, 2);
            draw_quarter_arc(r, inset.x + inset.w - rad - 1, inset.y + inset.h - rad - 1,  rad, 3);
        }
    }
}

/* ------------------------------------------------------------------ */
/*  Shadow rendering                                                   */
/* ------------------------------------------------------------------ */

static void draw_shadow(SDL_Renderer *r, SDL_Rect rect,
                         int blur, float opacity)
{
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

    int layers = SHADOW_LAYERS;
    for (int i = layers; i >= 1; i--) {
        int expand = blur * i / layers;
        float alpha_f = opacity * (float)(layers - i + 1) / (float)(layers + 1);
        Uint8 a = (Uint8)(alpha_f * 255.0f);
        if (a < 1) continue;

        SDL_Rect sr = {
            rect.x - expand,
            rect.y - expand + 1,
            rect.w + 2 * expand,
            rect.h + 2 * expand
        };

        SDL_SetRenderDrawColor(r, 0, 0, 0, a);
        SDL_RenderFillRect(r, &sr);
    }
}

/* ------------------------------------------------------------------ */
/*  Gap / inset computation                                            */
/* ------------------------------------------------------------------ */

static SDL_Rect inset_rect(SDL_Rect r, int win_w, int win_h)
{
    int half_inner = INNER_GAP / 2;

    int left   = (r.x == 0)                ? OUTER_GAP : half_inner;
    int top    = (r.y == 0)                ? OUTER_GAP : half_inner;
    int right  = (r.x + r.w >= win_w)      ? OUTER_GAP : half_inner;
    int bottom = (r.y + r.h >= win_h)      ? OUTER_GAP : half_inner;

    SDL_Rect out = {
        r.x + left,
        r.y + top,
        r.w - left - right,
        r.h - top  - bottom
    };

    if (out.w < 1) out.w = 1;
    if (out.h < 1) out.h = 1;
    return out;
}

static SDL_Rect scale_rect_center(SDL_Rect r, float scale)
{
    if (scale >= 1.0f) return r;

    int dx = (int)((float)r.w * (1.0f - scale) * 0.5f);
    int dy = (int)((float)r.h * (1.0f - scale) * 0.5f);
    SDL_Rect out = { r.x + dx, r.y + dy, r.w - 2*dx, r.h - 2*dy };
    if (out.w < 1) out.w = 1;
    if (out.h < 1) out.h = 1;
    return out;
}

/* ------------------------------------------------------------------ */
/*  draw_leaf — the visual effects hub                                 */
/* ------------------------------------------------------------------ */

static void draw_leaf(LayoutNode *node, void *userdata)
{
    struct {
        SDL_Renderer *renderer;
        LayoutNode   *focused;
        int           win_w;
        int           win_h;
    } *ctx = userdata;

    if (!node->view)
        return;

    bool is_focused = (node == ctx->focused);

    /* -- per-leaf FX state -- */
    LeafFx *fx = render_fx_get(node->id);

    float opacity      = fx ? fx->opacity      : (is_focused ? 1.0f : 0.92f);
    float scale        = fx ? fx->scale         : 1.0f;
    float border_w     = fx ? fx->border_width  : (is_focused ? 2.0f : 1.0f);
    SDL_Color bcol     = fx ? fx->border_color
                            : (SDL_Color){ 50, 50, 60, 200 };

    /* 1. Gap inset */
    SDL_Rect visual = inset_rect(node->rect, ctx->win_w, ctx->win_h);

    /* 2. Spawn / scale */
    visual = scale_rect_center(visual, scale);
    if (visual.w <= 0 || visual.h <= 0) return;

    /* 3. Shadow (behind content, outside clip) */
    SDL_RenderSetClipRect(ctx->renderer, NULL);
    SDL_SetRenderDrawBlendMode(ctx->renderer, SDL_BLENDMODE_BLEND);
    if (is_focused)
        draw_shadow(ctx->renderer, visual, 20, 0.30f * opacity);
    else
        draw_shadow(ctx->renderer, visual, 10, 0.15f * opacity);

    /* 4. Clip and draw content */
    SDL_RenderSetClipRect(ctx->renderer, &visual);

    node->view->draw(
        node->view,
        ctx->renderer,
        visual,
        is_focused
    );

    /* 5. Corner covers (rounded corners via background masking) */
    SDL_SetRenderDrawColor(ctx->renderer, FX_BG_R, FX_BG_G, FX_BG_B, 255);
    fill_corner_cover(ctx->renderer, visual, BORDER_RADIUS, 0);
    fill_corner_cover(ctx->renderer, visual, BORDER_RADIUS, 1);
    fill_corner_cover(ctx->renderer, visual, BORDER_RADIUS, 2);
    fill_corner_cover(ctx->renderer, visual, BORDER_RADIUS, 3);

    /* 6. Opacity dimming overlay */
    if (opacity < 0.999f) {
        Uint8 dim = (Uint8)((1.0f - opacity) * 255.0f);
        SDL_SetRenderDrawBlendMode(ctx->renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(ctx->renderer, 0, 0, 0, dim);
        SDL_RenderFillRect(ctx->renderer, &visual);
    }

    SDL_RenderSetClipRect(ctx->renderer, NULL);

    /* 7. Animated rounded border */
    int thickness = (int)(border_w + 0.5f);
    if (thickness < 1) thickness = 1;

    SDL_SetRenderDrawBlendMode(ctx->renderer, SDL_BLENDMODE_BLEND);
    draw_rounded_border(ctx->renderer, visual, BORDER_RADIUS,
                        bcol, thickness);
}

/* ------------------------------------------------------------------ */
/*  Public                                                             */
/* ------------------------------------------------------------------ */

void render_layout(SDL_Renderer *r, LayoutNode *root, LayoutNode *focused)
{
    render_fx_pre_frame(root, focused);

    int win_w = 0, win_h = 0;
    SDL_GetRendererOutputSize(r, &win_w, &win_h);

    struct {
        SDL_Renderer *r;
        LayoutNode   *focused;
        int           win_w;
        int           win_h;
    } ctx = { r, focused, win_w, win_h };

    layout_traverse_leaves(root, draw_leaf, &ctx);

    render_fx_draw_closing(r);
}
