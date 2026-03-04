#include "session.h"
#include "layout/layout.h"
#include "view/pane/pane_view.h"
#include "view/tab/tab_view.h"
#include "view/web/web_view.h"
#include "view/web/webview_registry.h"

#include <cjson/cJSON.h>
#include <SDL2/SDL_log.h>
#include <errno.h>
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
static char g_session_path_override[1024] = {0};

static void close_webviews(WebView **webs, int web_count);


static void copy_string(char *dst, size_t dst_sz, const char *src)
{
    if (!dst || dst_sz == 0)
        return;
    snprintf(dst, dst_sz, "%s", src ? src : "");
}

static const char *session_path(void)
{
    if (g_session_path_override[0])
        return g_session_path_override;

    static char path[1024];
    const char *home = getenv("HOME");
    if (!home || !*home)
        home = getenv("USERPROFILE");
    if (!home || !*home)
        home = ".";

    snprintf(path, sizeof(path),
             "%s/.local/share/nativeshell/profiles/default/session.json",
             home);
    return path;
}

void session_set_path(const char *path)
{
    if (!path || !*path) {
        g_session_path_override[0] = '\0';
        return;
    }

    copy_string(g_session_path_override,
                sizeof(g_session_path_override),
                path);
}

static int ensure_dir_recursive(const char *path)
{
    if (!path || !*path)
        return 0;

    char tmp[1024];
    copy_string(tmp, sizeof(tmp), path);

    char *start = tmp + 1;
    if (tmp[1] == ':' && (tmp[2] == '/' || tmp[2] == '\\'))
        start = tmp + 3;

    for (char *p = start; *p; p++) {
        if (*p == '/' || *p == '\\') {
            char saved = *p;
            *p = '\0';
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST)
                return 0;
            *p = saved;
        }
    }

    if (mkdir(tmp, 0755) != 0 && errno != EEXIST)
        return 0;

    return 1;
}

static void ensure_session_dir(void)
{
    const char *path = session_path();
    if (!path || !*path)
        return;

    char dir[1024];
    copy_string(dir, sizeof(dir), path);

    char *slash = strrchr(dir, '/');
    char *backslash = strrchr(dir, '\\');
    char *sep = slash;
    if (backslash && (!sep || backslash > sep))
        sep = backslash;
    if (!sep)
        return;

    *sep = '\0';
    (void)ensure_dir_recursive(dir);
}

static void close_webviews(WebView **webs, int web_count)
{
    if (!webs)
        return;

    for (int i = 0; i < web_count; i++) {
        if (webs[i])
            web_view_close((View *)webs[i]);
    }
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
    if (!root_json || !cJSON_IsObject(root_json))
        return 0;

    cJSON *focused = cJSON_GetObjectItem(root_json, "focused");
    cJSON *webviews = cJSON_GetObjectItem(root_json, "webviews");
    cJSON *tree = cJSON_GetObjectItem(root_json, "tree");

    return focused && cJSON_IsNumber(focused) &&
           webviews && cJSON_IsArray(webviews) &&
           tree && cJSON_IsObject(tree);
}


static LayoutNode *load_node_cjson(
    cJSON *jnode,
    LayoutNode *parent,
    int *max_id,
    WebView **webs,
    int web_count,
    int *ok)
{
    if (!ok || !*ok || !jnode || !cJSON_IsObject(jnode)) {
        if (ok)
            *ok = 0;
        return NULL;
    }

    cJSON *type_item = cJSON_GetObjectItemCaseSensitive(jnode, "type");
    if (!cJSON_IsString(type_item) || !type_item->valuestring) {
        *ok = 0;
        return NULL;
    }
    const char *type = type_item->valuestring;

    if (strcmp(type, "leaf") == 0) {
        cJSON *id_item = cJSON_GetObjectItemCaseSensitive(jnode, "id");
        if (!cJSON_IsNumber(id_item)) {
            *ok = 0;
            return NULL;
        }
        int saved_id = id_item->valueint;

        LayoutNode *n = layout_leaf();
        if (!n) {
            *ok = 0;
            return NULL;
        }

        n->id = saved_id;
        n->parent = parent;

        if (saved_id > *max_id)
            *max_id = saved_id;

        cJSON *view = cJSON_GetObjectItemCaseSensitive(jnode, "view");
        if (view && !cJSON_IsObject(view)) {
            *ok = 0;
            layout_destroy(n);
            return NULL;
        }

        const char *vtype = "pane";
        if (view) {
            cJSON *vtype_item = cJSON_GetObjectItemCaseSensitive(view, "type");
            if (!cJSON_IsString(vtype_item) || !vtype_item->valuestring) {
                *ok = 0;
                layout_destroy(n);
                return NULL;
            }
            vtype = vtype_item->valuestring;
        }

        if (vtype && strcmp(vtype, "tab") == 0) {
            if (n->view)
                n->view->destroy(n->view);
            n->view = tab_view_create();
        } else if (strcmp(vtype, "pane") == 0) {
            int attached = -1;
            if (view) {
                cJSON *att = cJSON_GetObjectItemCaseSensitive(view, "attached");
                if (att && cJSON_IsNumber(att))
                    attached = att->valueint;
            }
            if (attached >= 0 && attached < web_count)
                pane_view_attach(n->view, webs[attached]);
        } else {
            *ok = 0;
            layout_destroy(n);
            return NULL;
        }

        return n;
    }

    if (strcmp(type, "split") != 0) {
        *ok = 0;
        return NULL;
    }

    cJSON *dir_item = cJSON_GetObjectItemCaseSensitive(jnode, "dir");
    cJSON *ratio_item = cJSON_GetObjectItemCaseSensitive(jnode, "ratio");
    cJSON *a_item = cJSON_GetObjectItemCaseSensitive(jnode, "a");
    cJSON *b_item = cJSON_GetObjectItemCaseSensitive(jnode, "b");

    if (!cJSON_IsString(dir_item) || !dir_item->valuestring ||
        !cJSON_IsNumber(ratio_item) ||
        !a_item || !cJSON_IsObject(a_item) ||
        !b_item || !cJSON_IsObject(b_item))
    {
        *ok = 0;
        return NULL;
    }

    const char *dir = dir_item->valuestring;
    if (dir[0] != 'v' && dir[0] != 'h') {
        *ok = 0;
        return NULL;
    }
    float ratio = (float)ratio_item->valuedouble;
    if (ratio <= 0.0f || ratio >= 1.0f)
        ratio = 0.5f;

    LayoutNode *n = layout_split_node(
        dir[0] == 'v' ? SPLIT_VERTICAL : SPLIT_HORIZONTAL,
        ratio);
    if (!n) {
        *ok = 0;
        return NULL;
    }

    n->parent = parent;

    n->a = load_node_cjson(a_item, n, max_id, webs, web_count, ok);
    if (!*ok || !n->a) {
        layout_destroy(n);
        return NULL;
    }

    n->b = load_node_cjson(b_item, n, max_id, webs, web_count, ok);
    if (!*ok || !n->b) {
        layout_destroy(n);
        return NULL;
    }

    return n;
}

LayoutNode *session_load(LayoutNode **focused)
{
    if (focused)
        *focused = NULL;

    const char *path = session_path();
    if (!path)
        return NULL;

    FILE *f = fopen(path, "r");
    if (!f)
        return NULL;

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }

    long len = ftell(f);
    if (len < 0 || len > (8 * 1024 * 1024)) {
        fclose(f);
        SDL_Log("Session file size invalid, ignoring session");
        return NULL;
    }

    rewind(f);

    char *data = malloc((size_t)len + 1);
    if (!data) {
        fclose(f);
        return NULL;
    }

    size_t read_n = fread(data, 1, (size_t)len, f);
    fclose(f);
    data[read_n] = '\0';

    if (read_n == 0 && len > 0) {
        free(data);
        SDL_Log("Session file read failed, ignoring session");
        return NULL;
    }

    cJSON *root_json = cJSON_Parse(data);
    free(data);

    if (!session_validate_json(root_json)) {
        if (root_json)
            cJSON_Delete(root_json);
        SDL_Log("Session JSON invalid, ignoring session");
        return NULL;
    }

    cJSON *focused_json = cJSON_GetObjectItem(root_json, "focused");
    cJSON *webviews = cJSON_GetObjectItem(root_json, "webviews");
    cJSON *tree = cJSON_GetObjectItem(root_json, "tree");
    int focus_id = focused_json->valueint;

    int web_count = cJSON_GetArraySize(webviews);
    if (web_count < 0 || web_count > 2048) {
        cJSON_Delete(root_json);
        SDL_Log("Session webview list invalid, ignoring session");
        return NULL;
    }

    WebView **webs = NULL;
    if (web_count > 0) {
        webs = calloc((size_t)web_count, sizeof(WebView *));
        if (!webs) {
            cJSON_Delete(root_json);
            SDL_Log("Session alloc failed, ignoring session");
            return NULL;
        }
    }

    for (int i = 0; i < web_count; i++) {
        cJSON *item = cJSON_GetArrayItem(webviews, i);
        if (!item || !cJSON_IsObject(item)) {
            close_webviews(webs, web_count);
            free(webs);
            cJSON_Delete(root_json);
            SDL_Log("Session webview entry invalid, ignoring session");
            return NULL;
        }

        cJSON *url = item ? cJSON_GetObjectItem(item, "url") : NULL;
        const char *u = (url && cJSON_IsString(url) && url->valuestring)
            ? url->valuestring
            : "about:blank";
        webs[i] = (WebView *)web_view_create(u);
        if (!webs[i]) {
            close_webviews(webs, web_count);
            free(webs);
            cJSON_Delete(root_json);
            SDL_Log("Session webview create failed, ignoring session");
            return NULL;
        }
    }

    int max_id = 0;
    int ok = 1;
    LayoutNode *root = load_node_cjson(tree,
                                       NULL,
                                       &max_id,
                                       webs,
                                       web_count,
                                       &ok);
    if (!ok || !root) {
        if (root)
            layout_destroy(root);
        close_webviews(webs, web_count);
        free(webs);
        cJSON_Delete(root_json);
        SDL_Log("Session tree invalid, ignoring session");
        return NULL;
    }

    layout_reset_leaf_ids(max_id + 1);

    if (focused && focus_id >= 0)
        *focused = layout_find_leaf_by_id(root, focus_id);
    if (focused && !*focused)
        *focused = layout_first_leaf(root);

    free(webs);
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
