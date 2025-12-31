
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
    int best_dist = 0x7fffffff;

    SDL_Rect cur = focused->rect;

    for (size_t i = 0; i < leaves.count; i++) {
        LayoutNode *n = leaves.items[i];
        if (n == focused) continue;

        SDL_Rect r = n->rect;
        int dist = 0;
        bool valid = false;

        switch (dir) {
            case DIR_LEFT:
                valid = r.x + r.w <= cur.x;
                dist = cur.x - (r.x + r.w);
                break;

            case DIR_RIGHT:
                valid = r.x >= cur.x + cur.w;
                dist = r.x - (cur.x + cur.w);
                break;

            case DIR_UP:
                valid = r.y + r.h <= cur.y;
                dist = cur.y - (r.y + r.h);
                break;

            case DIR_DOWN:
                valid = r.y >= cur.y + cur.h;
                dist = r.y - (cur.y + cur.h);
                break;
        }

        if (valid && dist < best_dist) {
            best_dist = dist;
            best = n;
        }
    }

    free(leaves.items);
    return best;
}
