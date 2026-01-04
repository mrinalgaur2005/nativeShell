#include "config.h"
#include "SDL_keycode.h"
#include <cjson/cJSON.h>
#include <SDL2/SDL_log.h>
#include <SDL2/SDL_keyboard.h>
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

    cfg.startup_url[0] = '\0';
    cfg.restore_session = 1;
}

/* ---------------- helpers ---------------- */

static Action action_from_string(const char *s)
{
    if (!strcmp(s, "focus_left")) return ACTION_FOCUS_LEFT;
    if (!strcmp(s, "focus_right")) return ACTION_FOCUS_RIGHT;
    if (!strcmp(s, "focus_up")) return ACTION_FOCUS_UP;
    if (!strcmp(s, "focus_down")) return ACTION_FOCUS_DOWN;
    if (!strcmp(s, "split_vertical")) return ACTION_SPLIT_VERTICAL;
    if (!strcmp(s, "split_horizontal")) return ACTION_SPLIT_HORIZONTAL;
    if (!strcmp(s, "close_pane")) return ACTION_CLOSE_PANE;
    if (!strcmp(s, "open_webview")) return ACTION_OPEN_WEBVIEW;
    if (!strcmp(s, "enter_cmd")) return ACTION_ENTER_CMD;
    if (!strcmp(s, "web_back")) return ACTION_WEB_BACK;
    if (!strcmp(s, "web_reload")) return ACTION_WEB_RELOAD;
    if (!strcmp(s, "web_stop")) return ACTION_WEB_STOP;
    return ACTION_NONE;
}

static void ensure_config_dir(void)
{
    const char *home = getenv("HOME");
    if (!home) return;

    char path[512];
    snprintf(path, sizeof(path),
             "%s/.config/nativeshell", home);
    mkdir(path, 0755);
}

/* ---------------- load ---------------- */

void config_load(void)
{
    memset(&cfg, 0, sizeof(cfg));
    config_set_defaults();

    const char *home = getenv("HOME");
    if (!home)
        return;

    char path[512];
    snprintf(path, sizeof(path),
             "%s/.config/nativeshell/config.json", home);

    FILE *f = fopen(path, "r");
    if (!f) {
        SDL_Log("No config file, using defaults");
        return;
    }

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    rewind(f);

    char *data = malloc(len + 1);
    fread(data, 1, len, f);
    data[len] = '\0';
    fclose(f);

    cJSON *root = cJSON_Parse(data);
    free(data);

    if (!root) {
        SDL_Log("Invalid config.json, using defaults");
        return;
    }

    /* ---- keys ---- */
    cJSON *keys = cJSON_GetObjectItem(root, "keys");
    if (keys && cJSON_IsObject(keys)) {
        cfg.binding_count = 0;

        cJSON *it;
        cJSON_ArrayForEach(it, keys) {
            SDL_Keycode key = SDL_GetKeyFromName(it->string);
            Action act = action_from_string(it->valuestring);

            if (key != SDLK_UNKNOWN && act != ACTION_NONE) {
                cfg.bindings[cfg.binding_count++] =
                    (typeof(cfg.bindings[0])){ key, act };
            }
        }
    }

    /* ---- startup ---- */
    cJSON *startup = cJSON_GetObjectItem(root, "startup");
    if (startup) {

        cJSON *url = cJSON_GetObjectItem(startup, "url");
        if (cJSON_IsString(url)) {
            strncpy(cfg.startup_url, url->valuestring,
                    sizeof(cfg.startup_url) - 1);
        }

        cJSON *restore = cJSON_GetObjectItem(startup, "restore_session");
        if (cJSON_IsBool(restore)) {
            cfg.restore_session = cJSON_IsTrue(restore);
        }
    }

    cJSON_Delete(root);

    SDL_Log("Config loaded: %d bindings, restore=%d",
            cfg.binding_count, cfg.restore_session);
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
