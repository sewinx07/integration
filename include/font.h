/**
 * font.h - Police bitmap 5x7
 */
#ifndef FONT_H
#define FONT_H

#include <SDL2/SDL.h>

#define FONT_CHAR_W  5
#define FONT_CHAR_H  7
#define FONT_SPACING 2

void drawChar(SDL_Renderer *renderer, int x, int y, char c,
              int scale, Uint8 r, Uint8 g, Uint8 b);
void drawText(SDL_Renderer *renderer, int x, int y, const char *text,
              int scale, Uint8 r, Uint8 g, Uint8 b);
int  textWidth(const char *text, int scale);
int  textHeight(int scale);
void drawTextCentered(SDL_Renderer *renderer, int y,
                      int zoneX, int zoneW, const char *text,
                      int scale, Uint8 r, Uint8 g, Uint8 b);

#endif