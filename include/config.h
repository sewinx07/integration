
// include/config.h — Constantes globales (taille fenêtre, couleurs…)
#pragma once
#include <SDL2/SDL.h>          // Pour SDL_Color

// Dimensions de la fenêtre
#define WIN_W 1280
#define WIN_H 720

// Couleurs style Matrix (définies dans config.c)
extern const SDL_Color COL_BG_DARK;  // Fond sombre
extern const SDL_Color COL_TEXT;     // Texte vert clair
extern const SDL_Color COL_HEAD;     // Tête de colonne (plus clair)
