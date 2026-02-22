#include "session.h"
#include "layout/layout.h"
#include "view/pane/pane_view.h"
#include "view/tab/tab_view.h"
#include "view/web/web_view.h"
#include "view/web/webview_registry.h"

#include <cjson/cJSON.h>
#include <SDL2/SDL_log.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

static LayoutNode *g_root = NULL;
static LayoutNode *g_focused = NULL;
static int g_autosave_interval_sec = 30;
static volatile sig_atomic_t g_should_exit = 0;


static const char *session_path(void)
{
    static char path[512];
    const char *home = getenv("HOME");
    if (!home) return NULL;

    snprintf(path, sizeof(path),
             "%s/.local/share/nativeshell/session.json", home);
    return path;
}

static void ensure_session_dir(void)
{
    const char *home = getenv("HOME");
    if (!home) return;

    char dir[512];
    snprintf(dir, sizeof(dir),
             "%s/.local/share/nativeshell", home);

    mkdir(dir, 0755);
}


static void save_node_json(FILE *f, LayoutNode *n, int indent)
{
    for (int i = 0; i < indent; i++) fputs("  ", f);
    fputs("{\n", f);

    for (int i = 0; i < indent + 1; i++) fputs("  ", f);
    fprintf(f, "\"type\": \"%s\",\n",
            n->type == NODE_LEAF ? "leaf" : "split");

    if (n->type == NODE_LEAF) {

        for (int i = 0; i < indent + 1; i++) fputs("  ", f);
        fprintf(f, "\"id\": %d,\n", n->id);

        for (int i = 0; i < indent + 1; i++) fputs("  ", f);
        fputs("\"view\": {\n", f);

        for (int i = 0; i < indent + 2; i++) fputs("  ", f);
        if (n->view && n->view->type == VIEW_TAB) {
            fprintf(f, "\"type\": \"tab\"\n");
        } else {
            fprintf(f, "\"type\": \"pane\",\n");
            int attached = -1;
            if (n->view && n->view->type == VIEW_PANE) {
                WebView *web = pane_view_get_attached(n->view);
                if (web)
                    attached = tab_manager_index_of(web);
            }
            for (int i = 0; i < indent + 2; i++) fputs("  ", f);
            fprintf(f, "\"attached\": %d\n", attached);
        }

        for (int i = 0; i < indent + 1; i++) fputs("  ", f);
        fputs("}\n", f);

    } else {

        for (int i = 0; i < indent + 1; i++) fputs("  ", f);
        fprintf(f, "\"dir\": \"%c\",\n",
                n->split == SPLIT_VERTICAL ? 'v' : 'h');

        for (int i = 0; i < indent + 1; i++) fputs("  ", f);
        fprintf(f, "\"ratio\": %.3f,\n", n->ratio);

        for (int i = 0; i < indent + 1; i++) fputs("  ", f);
        fputs("\"a\": ", f);
        save_node_json(f, n->a, indent + 1);
        fputs(",\n", f);

        for (int i = 0; i < indent + 1; i++) fputs("  ", f);
        fputs("\"b\": ", f);
        save_node_json(f, n->b, indent + 1);
        fputc('\n', f);
    }

    for (int i = 0; i < indent; i++) fputs("  ", f);
    fputs("}", f);
}

void session_save(LayoutNode *root, LayoutNode *focused)
{
    if (!root) return;

    ensure_session_dir();
    const char *path = session_path();
    if (!path) return;

    FILE *f = fopen(path, "w");
    if (!f) return;

    fputs("{\n", f);

    fprintf(f, "  \"focused\": %d,\n",
            focused ? focused->id : -1);

    fputs("  \"webviews\": [\n", f);
    for (int i = 0; i < tab_manager_count(); i++) {
        WebView *web = tab_manager_webview_at(i);
        TabEntry *e = tab_manager_entry_at(i);
        const char *url = NULL;

        if (e && e->url[0])
            url = e->url;
        if (!url)
            url = web_view_get_url((View *)web);
        if (!url)
            url = "";

        fprintf(f, "    { \"url\": \"%s\" }%s\n",
                url,
                (i == tab_manager_count() - 1) ? "" : ",");
    }
    fputs("  ],\n", f);

    fputs("  \"tree\": ", f);
    save_node_json(f, root, 1);
    fputs("\n}\n", f);

    fclose(f);
}


static int session_validate_json(cJSON *root_json)
{
    if (!root_json) return 0;

    cJSON *focused = cJSON_GetObjectItem(root_json, "focused");
    cJSON *webviews = cJSON_GetObjectItem(root_json, "webviews");
    cJSON *tree = cJSON_GetObjectItem(root_json, "tree");

    if (!focused || !cJSON_IsNumber(focused))
        return 0;

    if (!webviews || !cJSON_IsArray(webviews))
        return 0;

    if (!tree || !cJSON_IsObject(tree))
        return 0;

    return 1;
}


static LayoutNode *load_node_cjson(
    cJSON *jnode,
    LayoutNode *parent,
    int *max_id,
    WebView **webs,
    int web_count)
{
    const char *type =
        cJSON_GetObjectItem(jnode, "type")->valuestring;

    if (strcmp(type, "leaf") == 0) {

        int saved_id =
            cJSON_GetObjectItem(jnode, "id")->valueint;

        LayoutNode *n = layout_leaf();
        n->id = saved_id;
        n->parent = parent;

        if (saved_id > *max_id)
            *max_id = saved_id;

        cJSON *view = cJSON_GetObjectItem(jnode, "view");
        const char *vtype = NULL;
        if (view)
            vtype = cJSON_GetObjectItem(view, "type")->valuestring;

        if (vtype && strcmp(vtype, "tab") == 0) {
            if (n->view)
                n->view->destroy(n->view);
            n->view = tab_view_create();
        } else {
            int attached = -1;
            if (view) {
                cJSON *att = cJSON_GetObjectItem(view, "attached");
                if (att)
                    attached = att->valueint;
            }
            if (attached >= 0 && attached < web_count)
                pane_view_attach(n->view, webs[attached]);
        }

        return n;
    }

    const char *dir =
        cJSON_GetObjectItem(jnode, "dir")->valuestring;

    float ratio =
        (float)cJSON_GetObjectItem(jnode, "ratio")->valuedouble;

    LayoutNode *n = layout_split_node(
        dir[0] == 'v' ? SPLIT_VERTICAL : SPLIT_HORIZONTAL,
        ratio);

    n->parent = parent;

    n->a = load_node_cjson(
        cJSON_GetObjectItem(jnode, "a"), n, max_id, webs, web_count);
    n->b = load_node_cjson(
        cJSON_GetObjectItem(jnode, "b"), n, max_id, webs, web_count);

    return n;
}

LayoutNode *session_load(LayoutNode **focused)
{
    const char *path = session_path();
    if (!path) return NULL;

    FILE *f = fopen(path, "r");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    rewind(f);

    char *data = malloc(len + 1);
    fread(data, 1, len, f);
    data[len] = '\0';
    fclose(f);

    cJSON *root_json = cJSON_Parse(data);
    free(data);

    if (!session_validate_json(root_json)) {
        if (root_json)
            cJSON_Delete(root_json);
        return NULL;
    }

    int focus_id =
        cJSON_GetObjectItem(root_json, "focused")->valueint;

    cJSON *webviews =
        cJSON_GetObjectItem(root_json, "webviews");

    int web_count = cJSON_GetArraySize(webviews);
    WebView **webs = calloc((size_t)web_count, sizeof(WebView *));
    for (int i = 0; i < web_count; i++) {
        cJSON *item = cJSON_GetArrayItem(webviews, i);
        cJSON *url = item ? cJSON_GetObjectItem(item, "url") : NULL;
        const char *u = (url && cJSON_IsString(url)) ? url->valuestring : "about:blank";
        webs[i] = (WebView *)web_view_create(u);
    }

    cJSON *tree =
        cJSON_GetObjectItem(root_json, "tree");

    int max_id = 0;
    LayoutNode *root =
        load_node_cjson(tree, NULL, &max_id, webs, web_count);

    free(webs);
    layout_reset_leaf_ids(max_id + 1);

    if (focus_id >= 0)
        *focused = layout_find_leaf_by_id(root, focus_id);

    cJSON_Delete(root_json);
    return root;
}


static void session_save_safe(void)
{
    if (g_root)
        session_save(g_root, g_focused);
}

static void session_signal_handler(int sig)
{
    (void)sig;
    g_should_exit = 1;
}

void session_register(LayoutNode *root, LayoutNode *focused)
{
    g_root = root;
    g_focused = focused;

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = session_signal_handler;

    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}


void session_autosave_tick(void)
{
    static time_t last_save = 0;
    time_t now = time(NULL);

    if (g_should_exit) {
        session_save_safe();
        exit(0);
    }

    if (now - last_save >= g_autosave_interval_sec) {
        session_save_safe();
        last_save = now;
    }
}
