
#pragma once
#include "view.h"

typedef struct _WebKitWebView WebKitWebView;

View *web_view_create(const char *url);

void web_view_handle_key(View *v, SDL_KeyboardEvent *key);
void web_view_handle_mouse(View *v, SDL_MouseButtonEvent *btn);
void web_view_handle_motion(View *v, SDL_MouseMotionEvent *motion);
void web_view_handle_wheel(View *v, SDL_MouseWheelEvent *wheel);
