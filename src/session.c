#include "session.h"
#include "layout.h"
#include "web_view.h"
#include <cjson/cJSON.h>
#include <SDL2/SDL_log.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
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

static char *read_file(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);

    char *buf = malloc(size + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }

    fread(buf, 1, size, f);
    buf[size] = '\0';
    fclose(f);
    return buf;
}

static char *json_find(char *json, const char *key)
{
    char needle[64];
    snprintf(needle, sizeof(needle), "\"%s\"", key);
    return strstr(json, needle);
}

static int json_read_int(char *json, const char *key, int def)
{
    char *p = json_find(json, key);
    if (!p) return def;
    int v;
    sscanf(p, "\"%*[^:]: %d", &v);
    return v;
}

static float json_read_float(char *json, const char *key, float def)
{
    char *p = json_find(json, key);
    if (!p) return def;
    float v;
    sscanf(p, "\"%*[^:]: %f", &v);
    return v;
}

static void json_read_string(char *json,
                             const char *key,
                             char *out,
                             size_t n)
{
    char *p = json_find(json, key);
    if (!p) {
        out[0] = '\0';
        return;
    }
    sscanf(p, "\"%*[^:]: \"%255[^\"]\"", out);
}

static void indent(FILE *f, int n)
{
    while (n--) fputs("  ", f);
}

static void save_node_json(FILE *f, LayoutNode *n, int indent_lvl)
{
    indent(f, indent_lvl);
    fputs("{\n", f);

    indent(f, indent_lvl + 1);
    fprintf(f, "\"type\": \"%s\",\n",
            n->type == NODE_LEAF ? "leaf" : "split");

    if (n->type == NODE_LEAF) {

        indent(f, indent_lvl + 1);
        fprintf(f, "\"id\": %d,\n", n->id);

        indent(f, indent_lvl + 1);
        fputs("\"view\": {\n", f);

        indent(f, indent_lvl + 2);
        if (n->view->type == VIEW_WEB) {
            fprintf(f, "\"type\": \"web\",\n");
            indent(f, indent_lvl + 2);
            fprintf(f, "\"url\": \"%s\"\n",
                    web_view_get_url(n->view));
        } else {
            fprintf(f, "\"type\": \"placeholder\"\n");
        }

        indent(f, indent_lvl + 1);
        fputs("}\n", f);
    }
    else {
        indent(f, indent_lvl + 1);
        fprintf(f, "\"dir\": \"%c\",\n",
                n->split == SPLIT_VERTICAL ? 'v' : 'h');

        indent(f, indent_lvl + 1);
        fprintf(f, "\"ratio\": %.3f,\n", n->ratio);

        indent(f, indent_lvl + 1);
        fputs("\"a\": ", f);
        save_node_json(f, n->a, indent_lvl + 1);
        fputs(",\n", f);

        indent(f, indent_lvl + 1);
        fputs("\"b\": ", f);
        save_node_json(f, n->b, indent_lvl + 1);
        fputc('\n', f);
    }

    indent(f, indent_lvl);
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

static LayoutNode *load_node_cjson(cJSON *jnode, LayoutNode *parent)
{
    const char *type = cJSON_GetObjectItem(jnode, "type")->valuestring;

    if (strcmp(type, "leaf") == 0) {
        int id = cJSON_GetObjectItem(jnode, "id")->valueint;
        LayoutNode *n = layout_leaf(id);
        n->parent = parent;

        cJSON *view = cJSON_GetObjectItem(jnode, "view");
        const char *vtype = cJSON_GetObjectItem(view, "type")->valuestring;

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

    n->a = load_node_cjson(cJSON_GetObjectItem(jnode, "a"), n);
    n->b = load_node_cjson(cJSON_GetObjectItem(jnode, "b"), n);

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

    cJSON *tree = cJSON_GetObjectItem(root_json, "tree");

    LayoutNode *root = load_node_cjson(tree, NULL);

    if (focus_id >= 0)
        *focused = layout_find_leaf_by_id(root, focus_id);

    cJSON_Delete(root_json);
    return root;
}
