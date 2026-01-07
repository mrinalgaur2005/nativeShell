
#pragma once
#include "view/view.h"
#include <SDL2/SDL.h>

typedef struct {
    View *view;                 /* VIEW_WEB only */
    char title[256];
    char url[512];
    int loading;
    int alive;

    SDL_Texture *icon;       /* cached favicon texture */
} WebViewEntry;

void tab_registry_init(SDL_Renderer *renderer);
WebViewEntry *tab_registry_get_by_view(View *v);

int  tab_registry_add(View *web);
void tab_registry_remove(View *web);

int  tab_registry_count(void);
WebViewEntry *tab_registry_get(int index);

/* called by webview */
void tab_registry_set_title(View *web, const char *title);
void tab_registry_set_loading(View *web, int loading);
void tab_registry_request_favicon(View *web);
void tab_registry_request_snapshot(View *web);
