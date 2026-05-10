#ifndef AUDIO_H
#define AUDIO_H
#include <SDL2/SDL_mixer.h>

typedef struct {
    Mix_Music* music;
    Mix_Chunk* hover_sound;
} GameAudio;

GameAudio* init_audio();
void free_audio(GameAudio* au);
#endif
