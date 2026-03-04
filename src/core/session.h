
#pragma once
#include "layout/layout.h"

void session_set_path(const char *path);
void session_save(LayoutNode *root, LayoutNode *focused);
void session_autosave_tick(void);
void session_register(LayoutNode *root, LayoutNode *focused);
LayoutNode *session_load(LayoutNode **focused);
