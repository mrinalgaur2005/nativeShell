#pragma once

#define PROFILE_NAME_MAX 64
#define PROFILE_PATH_MAX 1024

typedef struct {
    char name[PROFILE_NAME_MAX];
    char config_dir[PROFILE_PATH_MAX];
    char data_dir[PROFILE_PATH_MAX];
    char config_path[PROFILE_PATH_MAX];
    char session_path[PROFILE_PATH_MAX];
    char webkit_data_dir[PROFILE_PATH_MAX];
} ProfilePaths;

int profile_resolve(const char *profile_name, ProfilePaths *out);
int profile_list(char names[][PROFILE_NAME_MAX], int max_names);
