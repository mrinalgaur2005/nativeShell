
#include "command.h"
#include "web_view.h"
#include <SDL2/SDL_keyboard.h>
#include <SDL2/SDL_log.h>
#include <string.h>
#include <stdio.h>

static char buffer[256];
static int len = 0;
static bool active = false;

void cmd_enter(void)
{
    active = true;
    len = 0;
    buffer[0] = '\0';
    SDL_StartTextInput();
}

void cmd_exit(void)
{
    active = false;
    SDL_StopTextInput();
}

bool cmd_active(void)
{
    return active;
}

const char *cmd_buffer(void)
{
    return buffer;
}

void cmd_handle_key(SDL_KeyboardEvent *e)
{
    if (e->type != SDL_KEYDOWN)
        return;

    if (e->keysym.sym == SDLK_BACKSPACE) {
        if (len > 0) buffer[--len] = '\0';
        return;
    }
}
void cmd_handle_text(const char *text)
{
    for (const char *p = text; *p && len < 255; p++) {
        buffer[len++] = *p;
    }
    buffer[len] = '\0';
}
bool cmd_execute(LayoutNode *focused)
{
    SDL_Log("Inside command.c");
    if (!focused || !focused->view)
        return false;

    char cmd[32];
    char url[200];

    if (sscanf(buffer, "%31s %199s", cmd, url) != 2)
        return false;

    SDL_Log("cmd='%s', url='%s'", cmd, url);

    if (strcmp(cmd, "open") == 0) {
        if (focused->view->type != VIEW_WEB) {
            focused->view->destroy(focused->view);
            focused->view = web_view_create(url);
        } else {
            web_view_load_url(focused->view, url);
        }
        return true;
    }

    return false;
}
