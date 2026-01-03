
#include "layout.h"
#include "debug_view.h"
#include "placedholderview.h"
#include <SDL2/SDL_log.h>
#include <SDL2/SDL_pixels.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

static int next_leaf_id =1;
typedef struct {
    int x;
    int y;
    LayoutNode *hit;
} LeafHitTest;
int layout_next_leaf_id(void)
{
    return next_leaf_id++;
}

void layout_reset_leaf_ids(int start)
{
    next_leaf_id = start;
}
static void leaf_hit_test(LayoutNode *n, void *ud);
static void leaf_hit_test(LayoutNode *n, void *ud)
{
    LeafHitTest *t = ud;

    if (t->x >= n->rect.x &&
        t->y >= n->rect.y &&
        t->x <  n->rect.x + n->rect.w &&
        t->y <  n->rect.y + n->rect.h)
    {
        t->hit = n;
    }
}
LayoutNode *layout_leaf(void) {
    LayoutNode *n = calloc(1, sizeof(LayoutNode));
    n->type = NODE_LEAF;
    n->id = next_leaf_id++;
    SDL_Color color = { 80 + n->id * 20, 120, 160, 255 };
    n->view = placeholder_view_create(color);
    return n;
}

LayoutNode *layout_split_node(SplitDirection dir,float ratio) {
    LayoutNode *n = calloc(1, sizeof(LayoutNode));
    n->type = NODE_SPLIT;
    n->split=dir;
    n->ratio=ratio;
    return n;
}
LayoutNode *layout_split_leaf(LayoutNode *leaf,
                              SplitDirection dir,
                              float ratio,
                              LayoutNode **root)
{
    if (!leaf || leaf->type != NODE_LEAF) {
        SDL_Log("Refusing to split non-leaf");
        return leaf;
    }

    SDL_Log("Inside split leaf");

    LayoutNode *old_parent = leaf->parent;

    LayoutNode *split = layout_split_node(dir, ratio);
    LayoutNode *new_leaf = layout_leaf();

    if (old_parent) {
        if (old_parent->a == leaf) old_parent->a = split;
        else if (old_parent->b == leaf) old_parent->b = split;
    }

    if (*root == leaf)
        *root = split;

    split->parent = old_parent;
    split->a = leaf;
    split->b = new_leaf;

    leaf->parent = split;
    new_leaf->parent = split;

    return new_leaf;
}

LayoutNode *layout_first_leaf(LayoutNode *node){
    if (!node) {
        return NULL;    
    }
    if (node->type == NODE_LEAF)
        return node;
    return layout_first_leaf(node->a);
};
LayoutNode *layout_close_leaf(LayoutNode *leaf, LayoutNode **root)
{
    if (!leaf || !leaf->parent)
        return leaf;

    LayoutNode *split = leaf->parent;
    LayoutNode *survivor =
        (split->a == leaf) ? split->b : split->a;

    LayoutNode *grandparent = split->parent;

    if (grandparent) {
        if (grandparent->a == split)
            grandparent->a = survivor;
        else if (grandparent->b == split)
            grandparent->b = survivor;
    } else {
        *root = survivor;
    }

    survivor->parent = grandparent;

    free(leaf);
    free(split);

    return survivor;
}
void layout_assign(LayoutNode *node, SDL_Rect rect){
    if(!node) return;
    node->rect = rect;
    if(node->type==NODE_LEAF){
        return;
    }
    SDL_Rect r1 = rect;
    SDL_Rect r2 = rect;
    if(node->split == SPLIT_VERTICAL){
        int w1=(int)(rect.w *node->ratio);
        r1.w=w1;
        r2.x+=w1;
        r2.w-=w1;
    }else{
        int h1=(int)(rect.h *node->ratio);
        r1.h=h1;
        r2.y+=h1;
        r2.h-=h1;
    }

    layout_assign(node->a, r1);
    layout_assign(node->b, r2);
}
void layout_traverse_leaves(LayoutNode *node,
                            void (*fn)(LayoutNode *, void *),
                            void *userdata) {
    if (!node) return;

    if (node->type == NODE_LEAF) {
        fn(node, userdata);
        return;
    }

    layout_traverse_leaves(node->a, fn, userdata);
    layout_traverse_leaves(node->b, fn, userdata);
}

LayoutNode *layout_find_leaf_by_id(LayoutNode *node, int id)
{
    if (!node)
        return NULL;

    if (node->type == NODE_LEAF) {
        if (node->id == id)
            return node;
        return NULL;
    }

    LayoutNode *found = layout_find_leaf_by_id(node->a, id);
    if (found)
        return found;

    return layout_find_leaf_by_id(node->b, id);
}
LayoutNode *layout_leaf_at(LayoutNode *root, int x, int y)
{
    LeafHitTest t = {
        .x = x,
        .y = y,
        .hit = NULL
    };

    layout_traverse_leaves(root, leaf_hit_test, &t);
    return t.hit;
}
void layout_destroy(LayoutNode *node) {
    if (!node) return;
    layout_destroy(node->a);
    layout_destroy(node->b);
    if(node->view)node->view->destroy(node->view);
    free(node);
}
bool hit_test_split(LayoutNode *node, int x, int y, SplitHit *out)
{
    if (!node || node->type == NODE_LEAF)
        return false;

    SDL_Rect r = node->rect;

    if (node->split == SPLIT_VERTICAL) {
        int split_x = r.x + (int)(r.w * node->ratio);

        if (abs(x - split_x) <= SPLIT_GRAB_MARGIN &&
            y >= r.y &&
            y <= r.y + r.h)
        {
            out->node = node;
            out->vertical = true;
            return true;
        }
    }
    else if (node->split == SPLIT_HORIZONTAL) {
        int split_y = r.y + (int)(r.h * node->ratio);

        if (abs(y - split_y) <= SPLIT_GRAB_MARGIN &&
            x >= r.x &&
            x <= r.x + r.w)
        {
            out->node = node;
            out->vertical = false;
            return true;
        }
    }

    return hit_test_split(node->a, x, y, out) ||
           hit_test_split(node->b, x, y, out);
}
void layout_clear(LayoutNode **root, LayoutNode **focused)
{
    if (!root || !*root)
        return;

    layout_destroy(*root);

    layout_reset_leaf_ids(1);

    LayoutNode *n = layout_leaf();
    *root = n;
    *focused = n;
}
