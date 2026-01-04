
#define DEFAULT_SPLIT_DIR SPLIT_VERTICAL
#define DEFAULT_SPLIT_RATIO 0.5f


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
static int is_probable_url(const char *s);
static int is_probable_url(const char *s)
{
    return
        strstr(s, "://") ||          /* https://, http:// */
        strncmp(s, "www.", 4) == 0;  /* www.example.com */
}
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
    if (strcmp(cmd, "open") == 0 ||
            strcmp(cmd, "o") == 0 ||
            strcmp(cmd, "search") == 0 ||
            strcmp(cmd, "s") == 0)
    {
        if (!arg[0]) {
            SDL_Log("%s: missing argument", cmd);
            return false;
        }

        char final_url[512];

        if (is_probable_url(arg)) {

            /* normalize scheme */
            if (!strstr(arg, "://")) {
                snprintf(final_url, sizeof(final_url),
                        "https://%s", arg);
            } else {
                strncpy(final_url, arg, sizeof(final_url));
            }

        } else {
            char encoded[256];
            url_encode(arg, encoded, sizeof(encoded));

            snprintf(final_url, sizeof(final_url),
                    "https://www.google.com/search?q=%s",
                    encoded);
        }

        LayoutNode *leaf = *focused;

        if (leaf->view->type != VIEW_WEB) {
            leaf->view->destroy(leaf->view);
            leaf->view = web_view_create(final_url);
        } else {
            web_view_load_url(leaf->view, final_url);
        }

        return true;
    }
    //--------------------open new pane------------------
    if (strcmp(cmd, "new") == 0 || strcmp(cmd, "n") == 0) {

        if (!arg[0])
            return false;

        LayoutNode *new_leaf =
            layout_split_leaf(*focused,
                    DEFAULT_SPLIT_DIR,
                    DEFAULT_SPLIT_RATIO,
                    root);

        if (!new_leaf)
            return false;

        char final_url[512];

        if (is_probable_url(arg)) {

            if (!strstr(arg, "://")) {
                snprintf(final_url, sizeof(final_url),
                        "https://%s", arg);
            } else {
                strncpy(final_url, arg, sizeof(final_url));
            }

        } else {
            char encoded[256];
            url_encode(arg, encoded, sizeof(encoded));

            snprintf(final_url, sizeof(final_url),
                    "https://www.google.com/search?q=%s",
                    encoded);
        }

        new_leaf->view->destroy(new_leaf->view);
        new_leaf->view = web_view_create(final_url);

        *focused = new_leaf;

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
