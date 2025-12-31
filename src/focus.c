
#include "focus.h"
#include <stdbool.h>
#include <stdlib.h>


typedef struct {
    LayoutNode **items;
    size_t count;
    size_t cap;
} LeafList;

static void collect_leaf(LayoutNode *node, void *userdata)
{
    LeafList *list = userdata;

    if (list->count == list->cap) {
        list->cap = list->cap ? list->cap * 2 : 8;
        list->items = realloc(list->items,
                               list->cap * sizeof(LayoutNode *));
    }
    list->items[list->count++] = node;
}

static int center_x(SDL_Rect r) { return r.x + r.w / 2; }
static int center_y(SDL_Rect r) { return r.y + r.h / 2; }


LayoutNode *focus_move(LayoutNode *root,
                       LayoutNode *focused,
                       FocusDir dir)
{
    LeafList leaves = {0};
    layout_traverse_leaves(root, collect_leaf, &leaves);

    LayoutNode *best = focused;
    int best_score = 0x7fffffff;

    SDL_Rect cur = focused->rect;

    int cur_cx = center_x(cur);
    int cur_cy = center_y(cur);

    for (size_t i = 0; i < leaves.count; i++) {
        LayoutNode *n = leaves.items[i];
        if (n == focused) continue;

        SDL_Rect r = n->rect;

        int primary = 0;
        int align = 0;
        bool valid = false;

        switch (dir) {
            case DIR_LEFT:
                valid = r.x + r.w <= cur.x;
                primary = cur.x - (r.x + r.w);
                align = abs(center_y(r) - cur_cy);
                break;

            case DIR_RIGHT:
                valid = r.x >= cur.x + cur.w;
                primary = r.x - (cur.x + cur.w);
                align = abs(center_y(r) - cur_cy);
                break;

            case DIR_UP:
                valid = r.y + r.h <= cur.y;
                primary = cur.y - (r.y + r.h);
                align = abs(center_x(r) - cur_cx);
                break;

            case DIR_DOWN:
                valid = r.y >= cur.y + cur.h;
                primary = r.y - (cur.y + cur.h);
                align = abs(center_x(r) - cur_cx);
                break;
        }

        if (!valid)
            continue;

        int score = primary * 1000 + align;

        if (score < best_score) {
            best_score = score;
            best = n;
        }
    }

    free(leaves.items);
    return best;
}
