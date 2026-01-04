
#include "command/command.h"
#include "layout/layout.h"
#include "view/web/web_view.h"
#include <SDL2/SDL_keyboard.h>
#include <SDL2/SDL_log.h>
#include <string.h>
#include <stdio.h>

static char buffer[256];
static int len = 0;
static bool active = false;

static void url_encode(const char *in, char *out, size_t out_sz);
static void url_encode(const char *in, char *out, size_t out_sz)
{
    size_t j = 0;
    for (size_t i = 0; in[i] && j + 1 < out_sz; i++) {
        if (in[i] == ' ')
            out[j++] = '+';
        else
            out[j++] = in[i];
    }
    out[j] = '\0';
}
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
bool cmd_execute(LayoutNode **root, LayoutNode **focused)
{
    if (!root || !*root || !focused || !*focused)
        return false;

    char cmd[32];
    char arg[256] = {0};

    int n = sscanf(buffer, "%31s %255[^\n]", cmd, arg);
    if (n < 1)
        return false;

    SDL_Log("cmd='%s' arg='%s'", cmd, arg);

    /* ---------- open ---------- */
    if (strcmp(cmd, "open") == 0 || strcmp(cmd, "o")==0) {
        if (n != 2) {
            SDL_Log("open: missing URL");
            return false;
        }

        LayoutNode *leaf = *focused;

        if (leaf->view->type != VIEW_WEB) {
            leaf->view->destroy(leaf->view);
            leaf->view = web_view_create(arg);
        } else {
            web_view_load_url(leaf->view, arg);
        }

        return true;
    }
    if (strcmp(cmd, "search") == 0 || strcmp(cmd, "s") == 0) {

        if (!arg[0]) return false;

        char encoded[256];
        char url[512];

        url_encode(arg, encoded, sizeof(encoded));

        snprintf(url, sizeof(url),
                "https://www.google.com/search?q=%s",
                encoded);

        if ((*focused)->view->type != VIEW_WEB) {
            (*focused)->view->destroy((*focused)->view);
            (*focused)->view = web_view_create(url);
        } else {
            web_view_load_url((*focused)->view, url);
        }
        return true;
    }

    /* ---------- clear ---------- */
    if (strcmp(cmd, "clear") == 0) {
        SDL_Log("clear command");

        layout_clear(root, focused);
        return true;
    }

    SDL_Log("unknown command: %s", cmd);
    return false;
}
