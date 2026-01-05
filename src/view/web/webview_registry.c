
#include "webview_registry.h"
#include <string.h>
#include <stdlib.h>

#define MAX_TABS 128

static WebViewEntry tabs[MAX_TABS];
static int tab_count = 0;

void tab_registry_init(void)
{
    memset(tabs, 0, sizeof(tabs));
    tab_count = 0;
}

int tab_registry_add(View *web)
{
    if (tab_count >= MAX_TABS)
        return -1;

    WebViewEntry *e = &tabs[tab_count];
    memset(e, 0, sizeof(*e));

    e->view = web;
    e->alive = 1;

    tab_count++;
    return tab_count - 1;
}

void tab_registry_remove(View *web)
{
    for (int i = 0; i < tab_count; i++) {
        if (tabs[i].view == web) {
            tabs[i].alive = 0;

            // compact
            memmove(&tabs[i], &tabs[i+1],
                    sizeof(WebViewEntry) * (tab_count - i - 1));
            tab_count--;
            return;
        }
    }
}

int tab_registry_count(void)
{
    return tab_count;
}

WebViewEntry *tab_registry_get(int index)
{
    if (index < 0 || index >= tab_count)
        return NULL;
    return &tabs[index];
}
