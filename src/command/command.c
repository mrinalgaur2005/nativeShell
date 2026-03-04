
#define DEFAULT_SPLIT_DIR SPLIT_VERTICAL
#define DEFAULT_SPLIT_RATIO 0.5f


#include "command/command.h"
#include "command/command_overlay.h"
#include "core/profile.h"
#include "layout/layout.h"
#include "view/pane/pane_view.h"
#include "view/web/web_view.h"
#include <SDL2/SDL_keyboard.h>
#include <SDL2/SDL_log.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>

static char buffer[256];
static int len = 0;
static bool active = false;
static char pending_profile[128];
static bool has_pending_profile = false;
static bool has_pending_profile_show = false;
static bool has_pending_profiles_list = false;

static void url_encode(const char *in, char *out, size_t out_sz);
static void cmd_autocomplete(void);
static int is_probable_url(const char *s);
static LayoutNode *find_other_leaf(LayoutNode *root, LayoutNode *focused);
static void close_other_leaves(LayoutNode **root, LayoutNode **focused);

static const char *k_cmd_names[] = {
    "open",
    "search",
    "new",
    "only",
    "clear",
    "help",
    "profile",
    "profiles"
};
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

static void find_other_cb(LayoutNode *node, void *userdata)
{
    struct {
        LayoutNode *focused;
        LayoutNode *hit;
    } *ctx = userdata;

    if (!ctx->hit && node != ctx->focused)
        ctx->hit = node;
}

static LayoutNode *find_other_leaf(LayoutNode *root, LayoutNode *focused)
{
    struct {
        LayoutNode *focused;
        LayoutNode *hit;
    } ctx = { focused, NULL };

    layout_traverse_leaves(root, find_other_cb, &ctx);
    return ctx.hit;
}

static void close_other_leaves(LayoutNode **root, LayoutNode **focused)
{
    if (!root || !*root || !focused || !*focused)
        return;

    int focus_id = (*focused)->id;

    while (1) {
        LayoutNode *other = find_other_leaf(*root, *focused);
        if (!other || !other->parent)
            break;

        if (other->view && other->view->type == VIEW_PANE) {
            pane_view_detach(other->view);
        } else if (other->view) {
            other->view->destroy(other->view);
            other->view = NULL;
        }

        layout_close_leaf(other, root);

        LayoutNode *refocus = layout_find_leaf_by_id(*root, focus_id);
        if (refocus)
            *focused = refocus;
        else
            *focused = layout_first_leaf(*root);
    }
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
    cmd_overlay_clear_info();
    SDL_StopTextInput();
}

void cmd_reset_buffer(void)
{
    buffer[0] = '\0';
    len = 0;
}

bool cmd_active(void)
{
    return active;
}

const char *cmd_buffer(void)
{
    return buffer;
}

const char *cmd_take_profile_switch(void)
{
    if (!has_pending_profile)
        return NULL;
    has_pending_profile = false;
    return pending_profile;
}

bool cmd_take_profile_show_request(void)
{
    bool v = has_pending_profile_show;
    has_pending_profile_show = false;
    return v;
}

bool cmd_take_profiles_list_request(void)
{
    bool v = has_pending_profiles_list;
    has_pending_profiles_list = false;
    return v;
}

static size_t common_prefix_len(const char *a, const char *b)
{
    size_t i = 0;
    while (a[i] && b[i] && a[i] == b[i])
        i++;
    return i;
}

static bool command_needs_argument(const char *cmd)
{
    return strcmp(cmd, "open") == 0 ||
           strcmp(cmd, "search") == 0 ||
           strcmp(cmd, "new") == 0 ||
           strcmp(cmd, "profile") == 0;
}

static void set_buffer(const char *text)
{
    if (!text)
        text = "";
    snprintf(buffer, sizeof(buffer), "%s", text);
    len = (int)strlen(buffer);
}

static void cmd_autocomplete_command_name(void)
{
    char token[64];
    size_t token_len = 0;
    while (buffer[token_len] &&
           !isspace((unsigned char)buffer[token_len]) &&
           token_len + 1 < sizeof(token))
    {
        token[token_len] = buffer[token_len];
        token_len++;
    }
    token[token_len] = '\0';
    if (token_len == 0)
        return;

    const char *first = NULL;
    const char *exact = NULL;
    size_t lcp = 0;
    int matches = 0;

    for (size_t i = 0; i < sizeof(k_cmd_names) / sizeof(k_cmd_names[0]); i++) {
        const char *name = k_cmd_names[i];
        if (strncmp(name, token, token_len) != 0)
            continue;

        if (strcmp(name, token) == 0)
            exact = name;

        if (matches == 0) {
            first = name;
            lcp = strlen(name);
        } else {
            size_t c = common_prefix_len(first, name);
            if (c < lcp)
                lcp = c;
        }
        matches++;
    }

    if (matches == 0)
        return;

    if (exact) {
        char out[sizeof(buffer)];
        snprintf(out,
                 sizeof(out),
                 "%s%s",
                 exact,
                 command_needs_argument(exact) ? " " : "");
        set_buffer(out);
        return;
    }

    if (matches == 1 && first) {
        char out[sizeof(buffer)];
        snprintf(out,
                 sizeof(out),
                 "%s%s",
                 first,
                 command_needs_argument(first) ? " " : "");
        set_buffer(out);
        return;
    }

    if (!first || lcp == 0)
        return;

    char out[sizeof(buffer)];
    if (lcp >= sizeof(out))
        lcp = sizeof(out) - 1;
    memcpy(out, first, lcp);
    out[lcp] = '\0';
    set_buffer(out);
}

static void cmd_autocomplete_profile_name(void)
{
    size_t i = 0;
    while (buffer[i] && !isspace((unsigned char)buffer[i]))
        i++;
    if (strncmp(buffer, "profile", i) != 0)
        return;

    size_t arg_start = i;
    while (buffer[arg_start] && isspace((unsigned char)buffer[arg_start]))
        arg_start++;

    char prefix[PROFILE_NAME_MAX];
    size_t p = 0;
    while (buffer[arg_start + p] &&
           !isspace((unsigned char)buffer[arg_start + p]) &&
           p + 1 < sizeof(prefix))
    {
        prefix[p] = buffer[arg_start + p];
        p++;
    }
    prefix[p] = '\0';

    char names[64][PROFILE_NAME_MAX];
    int count = profile_list(names, (int)(sizeof(names) / sizeof(names[0])));
    if (count <= 0)
        return;

    const char *first = NULL;
    const char *exact = NULL;
    size_t lcp = 0;
    int matches = 0;

    for (int idx = 0; idx < count; idx++) {
        const char *name = names[idx];
        if (strncmp(name, prefix, strlen(prefix)) != 0)
            continue;

        if (strcmp(name, prefix) == 0)
            exact = name;

        if (matches == 0) {
            first = name;
            lcp = strlen(name);
        } else {
            size_t c = common_prefix_len(first, name);
            if (c < lcp)
                lcp = c;
        }
        matches++;
    }

    if (matches == 0)
        return;

    const char *completion = NULL;
    char partial[PROFILE_NAME_MAX];
    if (exact) {
        completion = exact;
    } else if (matches == 1 && first) {
        completion = first;
    } else if (first && lcp > 0) {
        if (lcp >= sizeof(partial))
            lcp = sizeof(partial) - 1;
        memcpy(partial, first, lcp);
        partial[lcp] = '\0';
        completion = partial;
    } else {
        return;
    }

    char out[sizeof(buffer)];
    snprintf(out, sizeof(out), "profile %s", completion);
    set_buffer(out);
}

static void cmd_autocomplete(void)
{
    size_t i = 0;
    while (buffer[i] && !isspace((unsigned char)buffer[i]))
        i++;

    if (!buffer[0] || !buffer[i]) {
        cmd_autocomplete_command_name();
        return;
    }

    if (strncmp(buffer, "profile", i) == 0) {
        cmd_autocomplete_profile_name();
        return;
    }
}

void cmd_handle_key(SDL_KeyboardEvent *e)
{
    if (e->type != SDL_KEYDOWN)
        return;

    cmd_overlay_clear_info();

    if (e->keysym.sym == SDLK_BACKSPACE) {
        if (len > 0) buffer[--len] = '\0';
        return;
    }

    if (e->keysym.sym == SDLK_TAB) {
        cmd_autocomplete();
        return;
    }
}

void cmd_handle_text(const char *text)
{
    cmd_overlay_clear_info();
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
                snprintf(final_url, sizeof(final_url), "%s", arg);
            }

        } else {
            char encoded[256];
            url_encode(arg, encoded, sizeof(encoded));

            snprintf(final_url, sizeof(final_url),
                    "https://www.google.com/search?q=%s",
                    encoded);
        }

        LayoutNode *leaf = *focused;
        if (leaf && leaf->view && leaf->view->type != VIEW_PANE)
            leaf = layout_find_view_type(*root, VIEW_PANE);

        if (!leaf || !leaf->view || leaf->view->type != VIEW_PANE)
            return false;

        WebView *attached = pane_view_get_attached(leaf->view);
        if (attached) {
            web_view_load_url((View *)attached, final_url);
        } else {
            WebView *web = (WebView *)web_view_create(final_url);
            pane_view_attach(leaf->view, web);
        }

        return true;
    }
    /* ---------- help ---------- */
    if (strcmp(cmd, "help") == 0 || strcmp(cmd, "h") == 0) {
        LayoutNode *leaf = *focused;
        if (leaf && leaf->view && leaf->view->type != VIEW_PANE)
            leaf = layout_find_view_type(*root, VIEW_PANE);
        if (!leaf || !leaf->view || leaf->view->type != VIEW_PANE)
            return false;

        if (pane_view_get_attached(leaf->view))
            pane_view_detach(leaf->view);

        const char *help_url =
            "data:text/plain,Commands:%0A"
            ":open%20<url|query>%20-%20open%20or%20search%0A"
            ":new%20<url|query>%20-%20open%20in%20new%20pane%0A"
            ":profile%20<name>%20-%20switch%20active%20profile%0A"
            ":profile%20-%20show%20current%20profile%0A"
            ":profiles%20-%20list%20available%20profiles%0A"
            ":only%20-%20close%20other%20panes%0A"
            "Shift%2Bh%20-%20hide%20current%20webview%0A"
            "TabView:%20j/k,%20counts,%20y%20attach,%20x%20close";

        WebView *web = (WebView *)web_view_create(help_url);
        pane_view_attach(leaf->view, web);
        *focused = leaf;
        return true;
    }
    /* ---------- profile ---------- */
    if (strcmp(cmd, "profile") == 0) {
        char *profile_arg = arg;
        while (*profile_arg && isspace((unsigned char)*profile_arg))
            profile_arg++;

        size_t arg_len = strlen(profile_arg);
        while (arg_len > 0 &&
               isspace((unsigned char)profile_arg[arg_len - 1])) {
            profile_arg[arg_len - 1] = '\0';
            arg_len--;
        }

        if (!profile_arg[0]) {
            has_pending_profile_show = true;
            return true;
        }

        int wrote = snprintf(pending_profile,
                             sizeof(pending_profile),
                             "%s",
                             profile_arg);
        if (wrote < 0 || (size_t)wrote >= sizeof(pending_profile)) {
            SDL_Log("profile: name too long");
            return false;
        }

        has_pending_profile = true;
        return true;
    }
    /* ---------- profiles ---------- */
    if (strcmp(cmd, "profiles") == 0) {
        has_pending_profiles_list = true;
        return true;
    }
    /* ---------- only ---------- */
    if (strcmp(cmd, "only") == 0) {
        close_other_leaves(root, focused);
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
                snprintf(final_url, sizeof(final_url), "%s", arg);
            }

        } else {
            char encoded[256];
            url_encode(arg, encoded, sizeof(encoded));

            snprintf(final_url, sizeof(final_url),
                    "https://www.google.com/search?q=%s",
                    encoded);
        }

        WebView *web = (WebView *)web_view_create(final_url);
        pane_view_attach(new_leaf->view, web);

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
