#pragma once

/* forward declaration */
typedef struct LayoutNode LayoutNode;

typedef enum FocusDir {
    DIR_LEFT,
    DIR_RIGHT,
    DIR_UP,
    DIR_DOWN
} FocusDir;

LayoutNode *focus_move(LayoutNode *root,
                       LayoutNode *focused,
                       FocusDir dir);
