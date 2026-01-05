
#pragma once
#include "view/view.h"

typedef struct {
    View *view;          // must be VIEW_WEB
    char title[256];
    char url[512];
    int loading;
    int alive;
} WebViewEntry;

void tab_registry_init(void);
int  tab_registry_add(View *web);
void tab_registry_remove(View *web);

int  tab_registry_count(void);
WebViewEntry *tab_registry_get(int index);
