
// include/button.h — Boutons rectangulaires verts avec effet hover
#pragma once
#include <stdbool.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

// Représentation d'un bouton simple
typedef struct Button {
    SDL_Rect rect;     // Position et taille
    char     label[64];// Texte du bouton (affiché au centre)
    bool     hovered;  // Vrai si la souris survole le bouton
    bool     visible;  // Vrai si le bouton doit être dessiné/testé
} Button;

// Initialise le bouton avec un libellé et un rectangle
void btn_init(Button* b, const char* label, SDL_Rect rc);

// Teste si (mx,my) est à l'intérieur du bouton visible
bool btn_hit(const Button* b, int mx, int my);

// Dessine le bouton (fond vert, bordures vertes, texte centré).
void btn_render(SDL_Renderer* r, TTF_Font* f, const Button* b);
