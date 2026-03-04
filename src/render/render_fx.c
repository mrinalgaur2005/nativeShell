#include "render/render_fx.h"
#include <math.h>
#include <string.h>

#define FX_POOL_SIZE    64
#define GHOST_POOL_SIZE 16

#define SPAWN_DURATION_MS  120
#define CLOSE_DURATION_MS  100
#define FOCUS_DURATION_MS  140

#define FOCUS_OPACITY   1.0f
#define UNFOCUS_OPACITY 0.92f

#define FOCUS_BORDER_W   2.0f
#define UNFOCUS_BORDER_W 1.0f

static const SDL_Color COLOR_FOCUS = { 120, 170, 255, 220 };
static const SDL_Color COLOR_MUTED = {  50,  50,  60, 200 };

static LeafFx       fx_pool[FX_POOL_SIZE];
static ClosingGhost ghosts[GHOST_POOL_SIZE];
static int           prev_ids[FX_POOL_SIZE];
static int           prev_id_count;
static int           prev_focused_id = -1;
static Uint32        last_tick;

static float ease_out_quad(float t)
{
    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return 1.0f;
    return 1.0f - (1.0f - t) * (1.0f - t);
}

static float move_toward(float cur, float tgt, float dt, float dur)
{
    if (dur <= 0.0f) return tgt;
    float diff = tgt - cur;
    if (fabsf(diff) < 0.001f) return tgt;

    float step = diff * (dt / dur) * 4.0f;

    if (fabsf(step) > fabsf(diff))
        return tgt;
    return cur + step;
}

static SDL_Color lerp_color(SDL_Color a, SDL_Color b, float t)
{
    if (t <= 0.0f) return a;
    if (t >= 1.0f) return b;
    SDL_Color c;
    c.r = (Uint8)(a.r + (int)(((int)b.r - (int)a.r) * t));
    c.g = (Uint8)(a.g + (int)(((int)b.g - (int)a.g) * t));
    c.b = (Uint8)(a.b + (int)(((int)b.b - (int)a.b) * t));
    c.a = (Uint8)(a.a + (int)(((int)b.a - (int)a.a) * t));
    return c;
}

static LeafFx *find_fx(int id)
{
    for (int i = 0; i < FX_POOL_SIZE; i++)
        if (fx_pool[i].active && fx_pool[i].id == id)
            return &fx_pool[i];
    return NULL;
}

static LeafFx *alloc_fx(int id)
{
    for (int i = 0; i < FX_POOL_SIZE; i++) {
        if (!fx_pool[i].active) {
            memset(&fx_pool[i], 0, sizeof(LeafFx));
            fx_pool[i].id     = id;
            fx_pool[i].active = true;
            return &fx_pool[i];
        }
    }
    return NULL;
}

static bool id_in_set(int id, const int *set, int count)
{
    for (int i = 0; i < count; i++)
        if (set[i] == id) return true;
    return false;
}

static void collect_ids(LayoutNode *node, int *ids, int *count, int max)
{
    if (!node) return;
    if (node->type == NODE_LEAF) {
        if (*count < max)
            ids[(*count)++] = node->id;
        return;
    }
    collect_ids(node->a, ids, count, max);
    collect_ids(node->b, ids, count, max);
}

static void store_rects(LayoutNode *node)
{
    if (!node) return;
    if (node->type == NODE_LEAF) {
        LeafFx *fx = find_fx(node->id);
        if (fx)
            fx->last_rect = node->rect;
        return;
    }
    store_rects(node->a);
    store_rects(node->b);
}

static ClosingGhost *alloc_ghost(void)
{
    for (int i = 0; i < GHOST_POOL_SIZE; i++)
        if (!ghosts[i].active)
            return &ghosts[i];
    return NULL;
}

void render_fx_pre_frame(LayoutNode *root, LayoutNode *focused)
{
    Uint32 now = SDL_GetTicks();
    if (last_tick == 0) last_tick = now;
    float dt = (float)(now - last_tick) / 1000.0f;
    if (dt > 0.1f) dt = 0.1f;
    last_tick = now;

    int cur_ids[FX_POOL_SIZE];
    int cur_count = 0;
    collect_ids(root, cur_ids, &cur_count, FX_POOL_SIZE);

    int focused_id = (focused && focused->type == NODE_LEAF) ? focused->id : -1;

    /* detect new leaves → spawn */
    for (int i = 0; i < cur_count; i++) {
        if (!id_in_set(cur_ids[i], prev_ids, prev_id_count)) {
            LeafFx *fx = find_fx(cur_ids[i]);
            if (!fx) fx = alloc_fx(cur_ids[i]);
            if (fx) {
                fx->opacity        = 0.0f;
                fx->scale          = 0.96f;
                fx->target_scale   = 1.0f;
                fx->spawn_start    = now;
                fx->border_width   = UNFOCUS_BORDER_W;
                fx->border_color   = COLOR_MUTED;
                fx->target_border_width = UNFOCUS_BORDER_W;
                fx->target_border_color = COLOR_MUTED;
                fx->target_opacity = (cur_ids[i] == focused_id)
                                     ? FOCUS_OPACITY : UNFOCUS_OPACITY;
            }
        }
    }

    /* detect removed leaves → close ghost */
    for (int i = 0; i < prev_id_count; i++) {
        if (!id_in_set(prev_ids[i], cur_ids, cur_count)) {
            LeafFx *fx = find_fx(prev_ids[i]);
            if (fx) {
                ClosingGhost *g = alloc_ghost();
                if (g) {
                    g->rect   = fx->last_rect;
                    g->start  = now;
                    g->active = true;
                }
                fx->active = false;
            }
        }
    }

    /* ensure all current leaves have FX entries */
    for (int i = 0; i < cur_count; i++) {
        LeafFx *fx = find_fx(cur_ids[i]);
        if (!fx) {
            fx = alloc_fx(cur_ids[i]);
            if (fx) {
                fx->opacity        = 1.0f;
                fx->target_opacity = (cur_ids[i] == focused_id)
                                     ? FOCUS_OPACITY : UNFOCUS_OPACITY;
                fx->scale          = 1.0f;
                fx->target_scale   = 1.0f;
                fx->border_width   = UNFOCUS_BORDER_W;
                fx->target_border_width = UNFOCUS_BORDER_W;
                fx->border_color   = COLOR_MUTED;
                fx->target_border_color = COLOR_MUTED;
            }
        }
    }

    /* focus change → update targets */
    if (focused_id != prev_focused_id) {
        for (int i = 0; i < cur_count; i++) {
            LeafFx *fx = find_fx(cur_ids[i]);
            if (!fx) continue;
            if (cur_ids[i] == focused_id) {
                fx->target_opacity      = FOCUS_OPACITY;
                fx->target_border_width = FOCUS_BORDER_W;
                fx->target_border_color = COLOR_FOCUS;
            } else {
                fx->target_opacity      = UNFOCUS_OPACITY;
                fx->target_border_width = UNFOCUS_BORDER_W;
                fx->target_border_color = COLOR_MUTED;
            }
        }
        prev_focused_id = focused_id;
    }

    /* advance animations */
    float focus_dur = (float)FOCUS_DURATION_MS / 1000.0f;
    for (int i = 0; i < FX_POOL_SIZE; i++) {
        LeafFx *fx = &fx_pool[i];
        if (!fx->active) continue;

        /* spawn ease-in */
        if (fx->spawn_start) {
            float elapsed = (float)(now - fx->spawn_start);
            float t = elapsed / (float)SPAWN_DURATION_MS;
            float e = ease_out_quad(t);
            fx->opacity = e * fx->target_opacity;
            fx->scale   = 0.96f + 0.04f * e;
            if (t >= 1.0f) {
                fx->opacity     = fx->target_opacity;
                fx->scale       = 1.0f;
                fx->spawn_start = 0;
            }
        } else {
            fx->opacity = move_toward(fx->opacity, fx->target_opacity,
                                      dt, focus_dur);
        }

        fx->border_width = move_toward(fx->border_width,
                                       fx->target_border_width,
                                       dt, focus_dur);

        float color_t = 0.0f;
        if (fabsf(fx->border_width - fx->target_border_width) < 0.01f)
            color_t = 1.0f;
        else
            color_t = 1.0f - fabsf(fx->border_width - fx->target_border_width)
                      / fabsf(FOCUS_BORDER_W - UNFOCUS_BORDER_W + 0.001f);
        if (color_t < 0.0f) color_t = 0.0f;
        if (color_t > 1.0f) color_t = 1.0f;
        fx->border_color = lerp_color(fx->border_color,
                                      fx->target_border_color,
                                      color_t);

        fx->scale = move_toward(fx->scale, fx->target_scale, dt, focus_dur);
    }

    /* advance close ghosts */
    for (int i = 0; i < GHOST_POOL_SIZE; i++) {
        if (!ghosts[i].active) continue;
        float elapsed = (float)(now - ghosts[i].start);
        if (elapsed >= (float)CLOSE_DURATION_MS)
            ghosts[i].active = false;
    }

    store_rects(root);

    memcpy(prev_ids, cur_ids, sizeof(int) * (size_t)cur_count);
    prev_id_count = cur_count;
}

LeafFx *render_fx_get(int leaf_id)
{
    return find_fx(leaf_id);
}

int render_fx_closing_count(void)
{
    int n = 0;
    for (int i = 0; i < GHOST_POOL_SIZE; i++)
        if (ghosts[i].active) n++;
    return n;
}

void render_fx_draw_closing(SDL_Renderer *r)
{
    Uint32 now = SDL_GetTicks();
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

    for (int i = 0; i < GHOST_POOL_SIZE; i++) {
        if (!ghosts[i].active) continue;

        float elapsed = (float)(now - ghosts[i].start);
        float t = elapsed / (float)CLOSE_DURATION_MS;
        float e = ease_out_quad(t);
        if (e > 1.0f) e = 1.0f;

        float opacity = 1.0f - e;
        float scale   = 1.0f - 0.03f * e;

        SDL_Rect r0 = ghosts[i].rect;
        int dx = (int)((float)r0.w * (1.0f - scale) * 0.5f);
        int dy = (int)((float)r0.h * (1.0f - scale) * 0.5f);
        SDL_Rect vis = { r0.x + dx, r0.y + dy, r0.w - 2*dx, r0.h - 2*dy };

        if (vis.w <= 0 || vis.h <= 0) continue;

        Uint8 a = (Uint8)(opacity * 180.0f);
        SDL_SetRenderDrawColor(r, FX_BG_R + 10, FX_BG_G + 10, FX_BG_B + 10, a);
        SDL_RenderFillRect(r, &vis);

        Uint8 ba = (Uint8)(opacity * (float)COLOR_MUTED.a);
        SDL_SetRenderDrawColor(r, COLOR_MUTED.r, COLOR_MUTED.g,
                               COLOR_MUTED.b, ba);
        SDL_RenderDrawRect(r, &vis);
    }
}
