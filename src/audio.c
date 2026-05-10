#include "audio.h"
#include <stdio.h>
#include <stdlib.h>

GameAudio* init_audio() {
    GameAudio* au = malloc(sizeof(GameAudio));
    if (!au) return NULL;

    // Initialisation du moteur audio (Fréquence 44100 Hz)
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        printf("Erreur Mix_OpenAudio: %s\n", Mix_GetError());
        free(au);
        return NULL;
    }

    // Chargement de la musique
    au->music = Mix_LoadMUS("assets/son/background.mp3");
    if (!au->music) {
        printf("Erreur chargement musique (background.mp3): %s\n", Mix_GetError());
    } else {
        Mix_PlayMusic(au->music, -1); // -1 pour jouer en boucle
        Mix_VolumeMusic(MIX_MAX_VOLUME / 2); // Volume à 50%
    }

    // Chargement du bruitage
    au->hover_sound = Mix_LoadWAV("assets/son/hover.wav");
    if (!au->hover_sound) {
        printf("Erreur chargement son (hover.wav): %s\n", Mix_GetError());
    }

    return au;
}

void free_audio(GameAudio* au) {
    if (!au) return;
    if (au->music) Mix_FreeMusic(au->music);
    if (au->hover_sound) Mix_FreeChunk(au->hover_sound);
    Mix_CloseAudio();
    free(au);
}
