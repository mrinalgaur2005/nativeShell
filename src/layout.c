
#include "layout.h"
#include <SDL2/SDL_log.h>
#include <stdio.h>
#include <stdlib.h>

LayoutNode *layout_leaf(int id) {
    LayoutNode *n = calloc(1, sizeof(LayoutNode));
    n->type = NODE_LEAF;
    n->id = id;
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

    /* Save old parent BEFORE modifying anything */
    LayoutNode *old_parent = leaf->parent;

    LayoutNode *split = layout_split_node(dir, ratio);
    LayoutNode *new_leaf = layout_leaf(leaf->id + 1);

    /* Replace leaf in its old parent */
    if (old_parent) {
        if (old_parent->a == leaf) old_parent->a = split;
        else if (old_parent->b == leaf) old_parent->b = split;
    }

    /* Root case */
    if (*root == leaf)
        *root = split;

    /* Wire new relationships */
    split->parent = old_parent;
    split->a = leaf;
    split->b = new_leaf;

    leaf->parent = split;
    new_leaf->parent = split;

    return new_leaf;
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

void layout_destroy(LayoutNode *node) {
    if (!node) return;
    layout_destroy(node->a);
    layout_destroy(node->b);
    free(node);
}
