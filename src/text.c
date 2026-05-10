// src/text.c - TTF Text rendering implementation
#include "text.h"

SDL_Texture* text_make(SDL_Renderer* renderer, TTF_Font* font,
                       const char* text, SDL_Color color)
{
    if (!renderer || !font || !text) {
        return NULL;
    }

    SDL_Surface* surface = TTF_RenderText_Solid(font, text, color);
    if (!surface) {
        return NULL;
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);

    return texture;
}
