
#pragma once
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

    int id;
} LayoutNode;

/*helpers */
LayoutNode *layout_leaf(int id);
LayoutNode *layout_split_leaf(LayoutNode *leaf,
                              SplitDirection dir,
                              float ratio);
/* THE IMPORTANT FUNCTION */
void layout_assign(LayoutNode *node, SDL_Rect rect);

/* Traversal */
void layout_traverse_leaves(LayoutNode *node,
                            void (*fn)(LayoutNode *, void *),
                            void *userdata);

/* Cleanup */
void layout_destroy(LayoutNode *node);
