
// src/button.c — Implémentation des boutons verts avec hover
#include <string.h>              // strncpy
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include "button.h"
#include "text.h"              // text_make/text_draw
#include "config.h"            // COL_TEXT

void btn_init(Button* b, const char* label, SDL_Rect rc) {
    if (!b) return;                                  // Sécurité pointeur
    b->rect = rc;                                    // Enregistrer le rectangle
    if (label) {                                     // Copier le label si fourni
        strncpy(b->label, label, sizeof(b->label)-1);// Copie protégée (63 chars)
        b->label[sizeof(b->label)-1] = '\0';        // Forcer terminaison
    } else {
        b->label[0] = '\0';                         // Label vide sinon
    }
    b->hovered = false;                              // Initialement, pas de survol
    b->visible = true;                               // Visible par défaut
}

bool btn_hit(const Button* b, int mx, int my) {
    if (!b || !b->visible) return false;             // Invisible → pas de hit
    // Test AABB (axis aligned bounding box) simple
    return (mx >= b->rect.x) && (mx <= b->rect.x + b->rect.w) &&
           (my >= b->rect.y) && (my <= b->rect.y + b->rect.h);
}

void btn_render(SDL_Renderer* r, TTF_Font* f, const Button* b) {
    if (!b || !b->visible) return;                   // Rien à dessiner si caché

    // 1) Couleur de fond (vert foncé → normal, vert vif → survol)
    SDL_Color base = b->hovered ? (SDL_Color){40,200,40,255} : (SDL_Color){10,120,10,255};

    // 2) Remplir le rectangle du bouton
    SDL_SetRenderDrawColor(r, base.r, base.g, base.b, 255);
    SDL_RenderFillRect(r, &((SDL_Rect){ b->rect.x, b->rect.y, b->rect.w, b->rect.h }));

    // 3) Double bordure verte (style Matrix)
    SDL_SetRenderDrawColor(r, 0,255,0,255);
    SDL_RenderDrawRect(r, &((SDL_Rect){ b->rect.x, b->rect.y, b->rect.w, b->rect.h }));
    SDL_RenderDrawRect(r, &((SDL_Rect){ b->rect.x+1, b->rect.y+1, b->rect.w-2, b->rect.h-2 }));

    // 4) Etiquette centrée
    if (f && b->label[0]) {
        SDL_Texture* t = text_make(r, f, b->label, COL_TEXT); // Texture du libellé
        if (t) {
            int tw=0, th=0; SDL_QueryTexture(t, NULL, NULL, &tw, &th); // Taille texte
            SDL_Rect dst = { b->rect.x + (b->rect.w - tw)/2,           // Centrage X
                             b->rect.y + (b->rect.h - th)/2,           // Centrage Y
                             tw, th };
            SDL_RenderCopy(r, t, NULL, &dst);                          // Dessiner le texte
            SDL_DestroyTexture(t);                                     // Libérer ressource
        }
    }
}
