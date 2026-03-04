#include "config.h"
#include "SDL_keycode.h"
#include <cjson/cJSON.h>
#include <SDL2/SDL_log.h>
#include <SDL2/SDL_keyboard.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>

static struct {
    struct {
        SDL_Keycode key;
        Action action;
    } bindings[64];
    int binding_count;

    char startup_url[256];
    int restore_session;
} cfg;

/* ---------------- defaults ---------------- */

static void config_set_defaults(void)
{
    cfg.binding_count = 0;

    cfg.bindings[cfg.binding_count++] = (typeof(cfg.bindings[0])){ SDLK_h, ACTION_FOCUS_LEFT };
    cfg.bindings[cfg.binding_count++] = (typeof(cfg.bindings[0])){ SDLK_l, ACTION_FOCUS_RIGHT };
    cfg.bindings[cfg.binding_count++] = (typeof(cfg.bindings[0])){ SDLK_k, ACTION_FOCUS_UP };
    cfg.bindings[cfg.binding_count++] = (typeof(cfg.bindings[0])){ SDLK_j, ACTION_FOCUS_DOWN };

    cfg.bindings[cfg.binding_count++] = (typeof(cfg.bindings[0])){ SDLK_v, ACTION_SPLIT_VERTICAL };
    cfg.bindings[cfg.binding_count++] = (typeof(cfg.bindings[0])){ SDLK_s, ACTION_SPLIT_HORIZONTAL };
    cfg.bindings[cfg.binding_count++] = (typeof(cfg.bindings[0])){ SDLK_x, ACTION_CLOSE_PANE };
    cfg.bindings[cfg.binding_count++] = (typeof(cfg.bindings[0])){ SDLK_o, ACTION_OPEN_WEBVIEW };
    cfg.bindings[cfg.binding_count++] = (typeof(cfg.bindings[0])){ SDLK_SEMICOLON, ACTION_ENTER_CMD };

    cfg.bindings[cfg.binding_count++] = (typeof(cfg.bindings[0])){ SDLK_u, ACTION_WEB_BACK };
    cfg.bindings[cfg.binding_count++] = (typeof(cfg.bindings[0])){ SDLK_r, ACTION_WEB_RELOAD };

    cfg.bindings[cfg.binding_count++] = (typeof(cfg.bindings[0])){ SDLK_PERIOD, ACTION_WEB_STOP };

    cfg.bindings[cfg.binding_count++] = (typeof(cfg.bindings[0])){ SDLK_t, ACTION_TAB_ENTER };
    cfg.startup_url[0] = '\0';
    cfg.restore_session = 1;
}

/* ---------------- helpers ---------------- */

static Action action_from_string(const char *s)
{
    if (!s)
        return ACTION_NONE;

    if (!strcmp(s, "focus_left")) return ACTION_FOCUS_LEFT;
    if (!strcmp(s, "focus_right")) return ACTION_FOCUS_RIGHT;
    if (!strcmp(s, "focus_up")) return ACTION_FOCUS_UP;
    if (!strcmp(s, "focus_down")) return ACTION_FOCUS_DOWN;
    if (!strcmp(s, "split_vertical")) return ACTION_SPLIT_VERTICAL;
    if (!strcmp(s, "split_horizontal")) return ACTION_SPLIT_HORIZONTAL;
    if (!strcmp(s, "close_pane")) return ACTION_CLOSE_PANE;
    if (!strcmp(s, "hide_webview")) return ACTION_HIDE_WEBVIEW;
    if (!strcmp(s, "open_webview")) return ACTION_OPEN_WEBVIEW;
    if (!strcmp(s, "enter_cmd")) return ACTION_ENTER_CMD;
    if (!strcmp(s, "web_back")) return ACTION_WEB_BACK;
    if (!strcmp(s, "web_reload")) return ACTION_WEB_RELOAD;
    if (!strcmp(s, "web_stop")) return ACTION_WEB_STOP;
    if (!strcmp(s, "tab_enter")) return ACTION_TAB_ENTER;
    return ACTION_NONE;
}

static void copy_string(char *dst, size_t dst_sz, const char *src)
{
    if (!dst || dst_sz == 0)
        return;
    snprintf(dst, dst_sz, "%s", src ? src : "");
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

static int ensure_parent_dir_for_file(const char *file_path)
{
    if (!file_path || !*file_path)
        return 0;

    char dir[1024];
    copy_string(dir, sizeof(dir), file_path);

    char *slash = strrchr(dir, '/');
    char *backslash = strrchr(dir, '\\');
    char *sep = slash;
    if (backslash && (!sep || backslash > sep))
        sep = backslash;

    if (!sep)
        return 1;

    *sep = '\0';
    return ensure_dir_recursive(dir);
}

static const char *default_config_path(void)
{
    static char path[1024];
    const char *home = getenv("HOME");
    if (!home || !*home)
        home = getenv("USERPROFILE");
    if (!home || !*home)
        home = ".";

    int n = snprintf(path, sizeof(path),
                     "%s/.config/nativeshell/profiles/default/config.json",
                     home);
    if (n < 0 || (size_t)n >= sizeof(path))
        return NULL;

    return path;
}

static int config_write_default_file(const char *path)
{
    if (!path || !*path)
        return 0;

    if (!ensure_parent_dir_for_file(path))
        return 0;

    FILE *f = fopen(path, "w");
    if (!f)
        return 0;

    fputs("{\n", f);
    fputs("  \"keys\": {\n", f);
    fputs("    \"h\": \"focus_left\",\n", f);
    fputs("    \"l\": \"focus_right\",\n", f);
    fputs("    \"k\": \"focus_up\",\n", f);
    fputs("    \"j\": \"focus_down\",\n", f);
    fputs("    \"v\": \"split_vertical\",\n", f);
    fputs("    \"s\": \"split_horizontal\",\n", f);
    fputs("    \"x\": \"close_pane\",\n", f);
    fputs("    \"o\": \"open_webview\",\n", f);
    fputs("    \";\": \"enter_cmd\",\n", f);
    fputs("    \"u\": \"web_back\",\n", f);
    fputs("    \"r\": \"web_reload\",\n", f);
    fputs("    \".\": \"web_stop\",\n", f);
    fputs("    \"t\": \"tab_enter\"\n", f);
    fputs("  },\n", f);
    fputs("  \"startup\": {\n", f);
    fputs("    \"url\": \"\",\n", f);
    fputs("    \"restore_session\": true\n", f);
    fputs("  }\n", f);
    fputs("}\n", f);

    fclose(f);
    return 1;
}

/* ---------------- load ---------------- */

void config_load(const char *config_path)
{
    memset(&cfg, 0, sizeof(cfg));
    config_set_defaults();
    const char *path = config_path;
    if (!path || !*path)
        path = default_config_path();
    if (!path) {
        SDL_Log("Could not resolve config path, using defaults");
        return;
    }

    if (!ensure_parent_dir_for_file(path))
        SDL_Log("Failed to create config directory for %s", path);

    FILE *f = fopen(path, "r");
    if (!f) {
        if (config_write_default_file(path))
            SDL_Log("No config file, wrote defaults to %s", path);
        else
            SDL_Log("No config file at %s, using defaults", path);
        return;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        SDL_Log("config.json seek failed, using defaults");
        fclose(f);
        return;
    }

    long len = ftell(f);
    if (len < 0 || len > (1024 * 1024)) {
        SDL_Log("config.json size invalid, using defaults");
        fclose(f);
        return;
    }

    rewind(f);

    char *data = malloc((size_t)len + 1);
    if (!data) {
        SDL_Log("config.json alloc failed, using defaults");
        fclose(f);
        return;
    }

    size_t read_n = fread(data, 1, (size_t)len, f);
    fclose(f);
    data[read_n] = '\0';

    if (read_n == 0 && len > 0) {
        SDL_Log("config.json read failed, using defaults");
        free(data);
        return;
    }

    cJSON *root = cJSON_Parse(data);
    free(data);

    if (!root || !cJSON_IsObject(root)) {
        SDL_Log("Invalid config.json, using defaults");
        if (root)
            cJSON_Delete(root);
        return;
    }

    /* ---- keys ---- */
    cJSON *keys = cJSON_GetObjectItem(root, "keys");
    if (keys && cJSON_IsObject(keys)) {
        int parsed_count = 0;
        typeof(cfg.bindings[0]) parsed[64];

        cJSON *it;
        cJSON_ArrayForEach(it, keys) {
            if (parsed_count >= (int)(sizeof(parsed) / sizeof(parsed[0])))
                break;

            if (!it->string || !cJSON_IsString(it) || !it->valuestring)
                continue;

            SDL_Keycode key = SDL_GetKeyFromName(it->string);
            Action act = action_from_string(it->valuestring);

            if (key != SDLK_UNKNOWN && act != ACTION_NONE) {
                parsed[parsed_count++] =
                    (typeof(cfg.bindings[0])){key, act};
            }
        }

        if (parsed_count > 0) {
            cfg.binding_count = parsed_count;
            memcpy(cfg.bindings,
                   parsed,
                   (size_t)parsed_count * sizeof(parsed[0]));
        } else {
            SDL_Log("No valid key overrides in config, keeping defaults");
        }
    }

    /* ---- startup ---- */
    cJSON *startup = cJSON_GetObjectItem(root, "startup");
    if (startup && cJSON_IsObject(startup)) {

        cJSON *url = cJSON_GetObjectItem(startup, "url");
        if (cJSON_IsString(url) && url->valuestring)
            copy_string(cfg.startup_url, sizeof(cfg.startup_url), url->valuestring);

        cJSON *restore = cJSON_GetObjectItem(startup, "restore_session");
        if (cJSON_IsBool(restore)) {
            cfg.restore_session = cJSON_IsTrue(restore);
        }
    }

    cJSON_Delete(root);

    SDL_Log("Config loaded from %s: %d bindings, restore=%d",
            path, cfg.binding_count, cfg.restore_session);
    SDL_Log("Startup URL: %s",
            config_startup_url() ? config_startup_url() : "(none)");
}


Action config_action_for_key(SDL_Keycode key)
{
    for (int i = 0; i < cfg.binding_count; i++) {
        if (cfg.bindings[i].key == key)
            return cfg.bindings[i].action;
    }
    return ACTION_NONE;
}

const char *config_startup_url(void)
{
    return cfg.startup_url[0] ? cfg.startup_url : NULL;
}

int config_restore_session(void)
{
    return cfg.restore_session;
}
