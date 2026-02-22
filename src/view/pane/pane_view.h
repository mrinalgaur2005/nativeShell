#pragma once
#include "view/view.h"
#include "view/web/web_view.h"

typedef struct {
    View base;
    WebView *attached;
} PaneView;

void pane_view_init(void);
View *pane_view_create(void);

WebView *pane_view_get_attached(View *v);
void pane_view_attach(View *v, WebView *web);
WebView *pane_view_detach(View *v);
