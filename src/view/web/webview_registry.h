
#pragma once
#include "view/web/web_view.h"
#include <SDL2/SDL.h>

typedef struct {
    char title[256];
    char url[512];
    int loading;
    SDL_Texture *icon;
    int favicon_requested;
} TabEntry;

typedef struct {
    WebView **all;
    TabEntry *entries;
    int count;
    int capacity;
    int selected_index;
} TabManager;

void tab_manager_init(SDL_Renderer *renderer);
int  tab_manager_add(WebView *web, const char *url);
void tab_manager_remove(WebView *web);
void tab_manager_destroy_all(void);

int  tab_manager_count(void);
WebView *tab_manager_webview_at(int index);
TabEntry *tab_manager_entry_at(int index);
int tab_manager_index_of(WebView *web);

int tab_manager_selected(void);
void tab_manager_set_selected(int index);

/* called by webview */
void tab_manager_set_title(WebView *web, const char *title);
void tab_manager_set_url(WebView *web, const char *url);
void tab_manager_set_loading(WebView *web, int loading);
void tab_manager_request_favicon(WebView *web);
