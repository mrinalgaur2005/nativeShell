
#pragma once
#include "view/view.h"
#include "view/web/web_view.h"
#include <SDL2/SDL.h>

enum FocusDir;
typedef enum FocusDir FocusDir ;

#define SPLIT_GRAB_MARGIN 6
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

    float target_ratio;
} LayoutNode;

typedef struct{
    LayoutNode *node;
    bool vertical;
}SplitHit;
bool hit_test_split(LayoutNode *node,int x,int y,SplitHit *out);
LayoutNode *layout_leaf(void);
int layout_next_leaf_id(void);
void layout_reset_leaf_ids(int start);
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
LayoutNode *layout_find_leaf_by_id(LayoutNode *root, int id);
void layout_clear(LayoutNode **root, LayoutNode **focused);


LayoutNode *layout_find_resize_split( LayoutNode *leaf, SplitDirection dir, int *leaf_is_a);
void layout_resize_relative( LayoutNode *focused,FocusDir dir);
void layout_animate(LayoutNode *node);
void layout_detach_view_everywhere(LayoutNode *root, View *view);
void layout_detach_view(LayoutNode *node, View *view);
LayoutNode *layout_find_view(LayoutNode *root,View *v);
LayoutNode *layout_insert_tabview(LayoutNode **root, int window_width);

LayoutNode *layout_leaf_from_node(LayoutNode *n);
LayoutNode *layout_find_view_type(LayoutNode *root, ViewType type);
