#ifndef MATRIX_H
#define MATRIX_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

void matrix_init(SDL_Renderer* r, int w, int h, TTF_Font* fm);
void matrix_update(float dt, int h);
void matrix_render(SDL_Renderer* r, int w);
void matrix_free();

#endif
