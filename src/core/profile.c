#include "core/profile.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

static int path_printf(char *dst,
                       size_t dst_sz,
                       const char *fmt,
                       const char *a,
                       const char *b)
{
    if (!dst || dst_sz == 0)
        return 0;

    int n = snprintf(dst, dst_sz, fmt, a, b);
    if (n < 0 || (size_t)n >= dst_sz) {
        dst[0] = '\0';
        return 0;
    }
    return 1;
}

static int path_printf_single(char *dst,
                              size_t dst_sz,
                              const char *fmt,
                              const char *a)
{
    if (!dst || dst_sz == 0)
        return 0;

    int n = snprintf(dst, dst_sz, fmt, a);
    if (n < 0 || (size_t)n >= dst_sz) {
        dst[0] = '\0';
        return 0;
    }
    return 1;
}

static int ensure_dir_recursive(const char *path)
{
    if (!path || !*path)
        return 0;

    char tmp[PROFILE_PATH_MAX];
    if (!path_printf_single(tmp, sizeof(tmp), "%s", path))
        return 0;

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

static const char *resolve_home_dir(void)
{
    const char *home = getenv("HOME");
    if (home && *home)
        return home;

    home = getenv("USERPROFILE");
    if (home && *home)
        return home;

    return ".";
}

static int is_valid_profile_name(const char *name)
{
    if (!name || !*name)
        return 0;
    if (strstr(name, ".."))
        return 0;

    for (const unsigned char *p = (const unsigned char *)name; *p; p++) {
        if (isalnum(*p) || *p == '_' || *p == '-' || *p == '.')
            continue;
        return 0;
    }

    return 1;
}

int profile_list(char names[][PROFILE_NAME_MAX], int max_names)
{
    if (!names || max_names <= 0)
        return 0;

    const char *home = resolve_home_dir();
    char root[PROFILE_PATH_MAX];
    if (!path_printf_single(root,
                            sizeof(root),
                            "%s/.config/nativeshell/profiles",
                            home))
    {
        return 0;
    }

    if (!ensure_dir_recursive(root))
        return 0;

    DIR *dir = opendir(root);
    if (!dir)
        return 0;

    int count = 0;
    struct dirent *ent = NULL;
    while ((ent = readdir(dir)) != NULL) {
        if (!ent->d_name[0] ||
            strcmp(ent->d_name, ".") == 0 ||
            strcmp(ent->d_name, "..") == 0)
        {
            continue;
        }

        if (!is_valid_profile_name(ent->d_name))
            continue;

        char full[PROFILE_PATH_MAX];
        if (!path_printf(full,
                         sizeof(full),
                         "%s/%s",
                         root,
                         ent->d_name))
        {
            continue;
        }

        struct stat st;
        if (stat(full, &st) != 0 || !S_ISDIR(st.st_mode))
            continue;

        if (!path_printf_single(names[count],
                                PROFILE_NAME_MAX,
                                "%s",
                                ent->d_name))
        {
            continue;
        }

        count++;
        if (count >= max_names)
            break;
    }

    closedir(dir);

    /* stable alphabetical output */
    for (int i = 0; i < count - 1; i++) {
        for (int j = i + 1; j < count; j++) {
            if (strcmp(names[i], names[j]) > 0) {
                char tmp[PROFILE_NAME_MAX];
                path_printf_single(tmp, sizeof(tmp), "%s", names[i]);
                path_printf_single(names[i], PROFILE_NAME_MAX, "%s", names[j]);
                path_printf_single(names[j], PROFILE_NAME_MAX, "%s", tmp);
            }
        }
    }

    return count;
}

int profile_resolve(const char *profile_name, ProfilePaths *out)
{
    if (!out)
        return 0;

    memset(out, 0, sizeof(*out));

    const char *requested = profile_name;
    if (!requested || !*requested)
        requested = "default";

    if (!is_valid_profile_name(requested)) {
        fprintf(stderr,
                "Invalid profile '%s', using default\n",
                requested ? requested : "(null)");
        requested = "default";
    }

    if (!path_printf_single(out->name, sizeof(out->name), "%s", requested)) {
        fprintf(stderr, "Profile name too long, using default\n");
        if (!path_printf_single(out->name, sizeof(out->name), "%s", "default"))
            return 0;
    }

    const char *home = resolve_home_dir();

    if (!path_printf(out->config_dir,
                     sizeof(out->config_dir),
                     "%s/.config/nativeshell/profiles/%s",
                     home,
                     out->name))
    {
        return 0;
    }

    if (!path_printf(out->data_dir,
                     sizeof(out->data_dir),
                     "%s/.local/share/nativeshell/profiles/%s",
                     home,
                     out->name))
    {
        return 0;
    }

    if (!path_printf_single(out->config_path,
                            sizeof(out->config_path),
                            "%s/config.json",
                            out->config_dir))
    {
        return 0;
    }

    if (!path_printf_single(out->session_path,
                            sizeof(out->session_path),
                            "%s/session.json",
                            out->data_dir))
    {
        return 0;
    }

    if (!path_printf_single(out->webkit_data_dir,
                            sizeof(out->webkit_data_dir),
                            "%s/webkit-data",
                            out->data_dir))
    {
        return 0;
    }

    if (!ensure_dir_recursive(out->config_dir) ||
        !ensure_dir_recursive(out->data_dir) ||
        !ensure_dir_recursive(out->webkit_data_dir))
    {
        fprintf(stderr,
                "Failed to create profile directories for '%s'\n",
                out->name);
        return 0;
    }

    return 1;
}
