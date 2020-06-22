#include "SDL.h"

void runRadar() {
    SDL_Init(SDL_INIT_VIDEO);

    SDL_Window *window = SDL_CreateWindow(
            "SDL2Test",
            SDL_WINDOWPOS_UNDEFINED,
            SDL_WINDOWPOS_UNDEFINED,
            640,
            480,
            0
    );

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
    while(true) {
        SDL_RenderClear(renderer);
        SDL_RenderPresent(renderer);

        SDL_Delay(1000 / 60);
    }


    SDL_DestroyWindow(window);
    SDL_Quit();
}