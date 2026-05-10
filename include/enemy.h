#ifndef ENEMY_H
#define ENEMY_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

#define MAX_FRAMES  20
#define MAX_ENEMIES 6

typedef struct {
    int type;           /* 0 = minion, 1 = boss */
    int x, y;
    int width, height;
    int health;
    int max_health;
    int alive;
    int state;          /* 1 = walk, 2 = attack, 3 = transform (boss only) */
    int transformed;
    int dx;

    SDL_Texture* idle1;
    SDL_Texture* idle2;

    SDL_Texture* walk_frames[MAX_FRAMES];
    int num_walk;

    SDL_Texture* attack_frames[MAX_FRAMES];
    int num_attack;

    SDL_Texture* walk_trans_frames[MAX_FRAMES];
    int num_walk_trans;

    SDL_Texture* attack_trans_frames[MAX_FRAMES];
    int num_attack_trans;

    SDL_Texture* transform_frames[MAX_FRAMES];
    int num_transform;

    int current_frame;
    int frame_timer;
    int frame_delay;

    int hit_cooldown;
} Enemy;

void init_enemy   (Enemy* e, int type, SDL_Renderer* renderer, int spawn_x);
void update_enemy (Enemy* e, int player_x);
void render_enemy (Enemy* e, SDL_Renderer* renderer, int camera_x);
void destroy_enemy(Enemy* e);
int  check_enemy_collision(SDL_Rect a, SDL_Rect b);

#endif