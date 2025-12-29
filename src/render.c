
#include "render.h"
#include "layout.h"

static void draw_leaf(LayoutNode *node, void *userdata) {
    SDL_Renderer *r = userdata;

    SDL_SetRenderDrawColor(r, 60, 60, 60, 255);
    SDL_RenderFillRect(r, &node->rect);

    SDL_SetRenderDrawColor(r, 20, 20, 20, 255);
    SDL_RenderDrawRect(r, &node->rect);
}
//this doesnt know the splits,ratio,shape of tree recursively apply draw_leaf to the tree
void render_layout(SDL_Renderer *r, LayoutNode *root) {
    layout_traverse_leaves(root, draw_leaf, r);
}
