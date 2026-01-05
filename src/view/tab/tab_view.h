#pragma once
#include "layout/layout.h"
#include "view/view.h"

typedef enum {
    TAB_NONE,
    TAB_ATTACH,
    TAB_CLOSE,
    TAB_EXIT
} TabAction;

/* lifecycle */
View *tab_view_create(void);

/* input */
void tab_view_handle_key(View *v, SDL_KeyboardEvent *e);

/* state */
int tab_view_selected(View *v);

void tab_view_close_selected(LayoutNode *root, View *v);
/* intent */
TabAction tab_view_take_action(View *v);
