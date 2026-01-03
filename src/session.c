#include "session.h"
#include "layout.h"
#include "web_view.h"

#include <cjson/cJSON.h>
#include <SDL2/SDL_log.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>


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
        if (n->view->type == VIEW_WEB) {
            fprintf(f, "\"type\": \"web\",\n");
            for (int i = 0; i < indent + 2; i++) fputs("  ", f);
            fprintf(f, "\"url\": \"%s\"\n",
                    web_view_get_url(n->view));
        } else {
            fprintf(f, "\"type\": \"placeholder\"\n");
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
    ensure_session_dir();

    const char *path = session_path();
    FILE *f = fopen(path, "w");
    if (!f) return;

    fputs("{\n", f);

    fprintf(f, "  \"focused\": %d,\n",
            focused ? focused->id : -1);

    fputs("  \"tree\": ", f);
    save_node_json(f, root, 1);
    fputs("\n}\n", f);

    fclose(f);
}


static LayoutNode *load_node_cjson(
    cJSON *jnode,
    LayoutNode *parent,
    int *max_id)
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
        const char *vtype =
            cJSON_GetObjectItem(view, "type")->valuestring;

        if (strcmp(vtype, "web") == 0) {
            const char *url =
                cJSON_GetObjectItem(view, "url")->valuestring;

            n->view->destroy(n->view);
            n->view = web_view_create(url);
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
        cJSON_GetObjectItem(jnode, "a"), n, max_id);
    n->b = load_node_cjson(
        cJSON_GetObjectItem(jnode, "b"), n, max_id);

    return n;
}

LayoutNode *session_load(LayoutNode **focused)
{
    const char *path = session_path();
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
    if (!root_json) return NULL;

    int focus_id =
        cJSON_GetObjectItem(root_json, "focused")->valueint;

    cJSON *tree =
        cJSON_GetObjectItem(root_json, "tree");

    int max_id = 0;
    LayoutNode *root =
        load_node_cjson(tree, NULL, &max_id);

    layout_reset_leaf_ids(max_id + 1);

    if (focus_id >= 0)
        *focused = layout_find_leaf_by_id(root, focus_id);

    cJSON_Delete(root_json);
    return root;
}
