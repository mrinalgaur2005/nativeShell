
#pragma once
#ifndef SESSION_H
#define SESSION_H

#include "layout.h"

void session_save(LayoutNode *root, LayoutNode *focused);

LayoutNode *session_load(LayoutNode **focused);

#endif 
