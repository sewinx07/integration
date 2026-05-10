/**
 * text.h - TTF Text rendering utilities
 */
#ifndef TEXT_H
#define TEXT_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

/**
 * Render text to an SDL_Texture using a TTF font
 * @param renderer SDL_Renderer to use
 * @param font TTF_Font to use
 * @param text Text string to render
 * @param color Color of the text
 * @return SDL_Texture* (caller must free with SDL_DestroyTexture)
 */
SDL_Texture* text_make(SDL_Renderer* renderer, TTF_Font* font, 
                       const char* text, SDL_Color color);

#endif
