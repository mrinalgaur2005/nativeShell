
#pragma once
#include "layout/layout.h"

void session_save(LayoutNode *root, LayoutNode *focused);
LayoutNode *session_load(LayoutNode **focused);
