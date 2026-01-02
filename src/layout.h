
#pragma once
#include "view.h"
#include <SDL2/SDL.h>

typedef enum {
    NODE_LEAF,
    NODE_SPLIT
} NodeType;

typedef enum {
    SPLIT_HORIZONTAL,
    SPLIT_VERTICAL
} SplitDirection;

typedef struct LayoutNode {
    NodeType type;
    SDL_Rect rect;

    SplitDirection split;
    float ratio;
    struct LayoutNode *a;
    struct LayoutNode *b;
    struct LayoutNode *parent;

    View *view;
    int id;
} LayoutNode;

LayoutNode *layout_leaf(int id);
LayoutNode *layout_split_node(SplitDirection dir,float ratio);
LayoutNode *layout_split_leaf(LayoutNode *leaf,
                              SplitDirection dir,
                              float ratio,
                              LayoutNode **root);

LayoutNode *layout_close_leaf(LayoutNode *leaf,LayoutNode **root);
LayoutNode *layout_first_leaf(LayoutNode *node);
void layout_assign(LayoutNode *node, SDL_Rect rect);

void layout_traverse_leaves(LayoutNode *node,
                            void (*fn)(LayoutNode *, void *),
                            void *userdata);

LayoutNode *layout_leaf_at(LayoutNode *root, int x, int y);
void layout_destroy(LayoutNode *node);
