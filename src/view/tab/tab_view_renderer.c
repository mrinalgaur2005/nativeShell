
#include <SDL2/SDL.h>

void render_tab_view(SDL_Renderer *r, int w, int h)
{
    SDL_Rect bg = { w/4, h/6, w/2, h*2/3 };
    SDL_SetRenderDrawColor(r, 20, 20, 20, 240);
    SDL_RenderFillRect(r, &bg);

    // text rendering can come later
}
