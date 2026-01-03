
#pragma once
#include "layout.h"

void session_save(LayoutNode *root, LayoutNode *focused);
LayoutNode *session_load(LayoutNode **focused);
