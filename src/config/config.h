
#pragma once
#include <SDL2/SDL_keycode.h>

typedef enum {
    ACTION_NONE = 0,

    ACTION_FOCUS_LEFT,
    ACTION_FOCUS_RIGHT,
    ACTION_FOCUS_UP,
    ACTION_FOCUS_DOWN,

    ACTION_SPLIT_VERTICAL,
    ACTION_SPLIT_HORIZONTAL,
    ACTION_CLOSE_PANE,

    ACTION_OPEN_WEBVIEW,
    ACTION_ENTER_CMD,

    ACTION_WEB_BACK,
    ACTION_WEB_FORWARD,
    ACTION_WEB_RELOAD,
    ACTION_WEB_STOP,

    ACTION_TAB_ENTER
} Action;

void config_load(void);
Action config_action_for_key(SDL_Keycode key);
const char *config_startup_url(void);
int config_restore_session(void);
