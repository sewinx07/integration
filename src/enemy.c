/* enemy.c — Fixed version
 *
 * Fix [ENEMY-ANIM-OOB-1]:
 *   render_enemy accessed e->walk_frames[e->current_frame % e->num_walk]
 *   without checking that e->num_walk > 0, causing a division-by-zero crash
 *   when sprite assets failed to load.  Same issue existed for num_attack,
 *   num_walk_trans, and num_attack_trans.  Each branch now falls back to
 *   e->idle1 when the frame count is zero.
 */

#include "enemy.h"
#include "player.h"
#include "common.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void init_enemy(Enemy *e, int type, SDL_Renderer *ren, int spawn_x)
{
    destroy_enemy(e);
    memset(e, 0, sizeof(Enemy));

    e->type        = type;
    e->x           = spawn_x;
    e->alive       = 1;
    e->state       = 1;
    e->dx          = (rand() % 2) ? 3 : -3;
    e->frame_delay = 6;
    e->health      = (type == 0) ? 120 : 400;
    e->max_health  = e->health;

    char base[200];
    snprintf(base, sizeof(base), "assets/ennemie/Spritesheet/%s/",
             type == 0 ? "minion" : "boss");

    char path[300];

    snprintf(path, sizeof(path), "%sidle1.png", base);
    e->idle1 = IMG_LoadTexture(ren, path);
    if (!e->idle1)
        fprintf(stderr, "[WARN] enemy: cannot load %s\n", path);

    if (type == 1) {
        snprintf(path, sizeof(path), "%sidle2.png", base);
        e->idle2 = IMG_LoadTexture(ren, path);
    }

    for (int i = 1; i <= 5; i++) {
        snprintf(path, sizeof(path), "%sWalk/frame%02d.png", base, i);
        SDL_Texture *t = IMG_LoadTexture(ren, path);
        if (!t) break;
        e->walk_frames[e->num_walk++] = t;
    }

    for (int i = 1; i <= 5; i++) {
        snprintf(path, sizeof(path), "%sAttack/frame%02d.png", base, i);
        SDL_Texture *t = IMG_LoadTexture(ren, path);
        if (!t) break;
        e->attack_frames[e->num_attack++] = t;
    }

    if (type == 1) {
        for (int i = 1; i <= 5; i++) {
            snprintf(path, sizeof(path), "%sTransformation/frame%02d.png", base, i);
            SDL_Texture *t = IMG_LoadTexture(ren, path);
            if (!t) break;
            e->transform_frames[e->num_transform++] = t;
        }
        for (int i = 1; i <= 5; i++) {
            snprintf(path, sizeof(path), "%sWalk_Trans/frame%02d.png", base, i);
            SDL_Texture *t = IMG_LoadTexture(ren, path);
            if (!t) break;
            e->walk_trans_frames[e->num_walk_trans++] = t;
        }
        for (int i = 1; i <= 5; i++) {
            snprintf(path, sizeof(path), "%sAttack_Trans/frame%02d.png", base, i);
            SDL_Texture *t = IMG_LoadTexture(ren, path);
            if (!t) break;
            e->attack_trans_frames[e->num_attack_trans++] = t;
        }
        fprintf(stderr, "[BOSS] Transform:%d Walk_Trans:%d Attack_Trans:%d\n",
                e->num_transform, e->num_walk_trans, e->num_attack_trans);
    }

    if (type == 0) {
        e->width  = PLAYER_W;
        e->height = PLAYER_PH;
    } else {
        if (e->idle1) {
            SDL_QueryTexture(e->idle1, NULL, NULL, &e->width, &e->height);
            if (e->width  < PLAYER_W)   e->width  = PLAYER_W;
            if (e->height < PLAYER_PH)  e->height = PLAYER_PH;
            if (e->width  > PLAYER_W  * 3) e->width  = PLAYER_W  * 3;
            if (e->height > PLAYER_PH * 3) e->height = PLAYER_PH * 3;
        } else {
            e->width  = PLAYER_W  * 2;
            e->height = PLAYER_PH * 2;
        }
    }
}

void update_enemy(Enemy *e, int player_x)
{
    if (!e->alive) return;

    if (e->health <= 0) {
        e->alive = 0;
        return;
    }

    int dist      = abs(player_x - e->x);
    int old_state = e->state;

    if (e->type == 0) {
        if (dist > 280) {
            e->state = 1;
            int maxX = WORLD_WIDTH - e->width;
            if (e->x <= 0)    e->dx =  3;
            if (e->x >= maxX) e->dx = -3;
        } else if (dist > 70) {
            e->state = 1;
            e->dx    = (player_x > e->x) ? 2 : -2;
        } else {
            e->state = 2;
            e->dx    = (player_x > e->x) ? 1 : -1;
        }
    } else {
        if (e->health <= e->max_health / 2 &&
            !e->transformed && e->state != 3)
        {
            e->state         = 3;
            e->dx            = 0;
            e->current_frame = 0;
            fprintf(stderr, "[BOSS] Transformation started!\n");
        } else if (e->state != 3) {
            int sp = e->transformed ? 5 : 3;
            if (dist > 70) {
                e->state = 1;
                e->dx    = (player_x > e->x) ? sp : -sp;
            } else {
                e->state = 2;
                int asp  = e->transformed ? 3 : 2;
                e->dx    = (player_x > e->x) ? asp : -asp;
            }
        }
    }

    if (e->state != old_state)
        e->current_frame = 0;

    e->x += e->dx;

    int maxX = WORLD_WIDTH - e->width;
    if (e->x < 0)    e->x = 0;
    if (e->x > maxX) e->x = maxX;

    if (++e->frame_timer >= e->frame_delay) {
        e->frame_timer = 0;
        e->current_frame++;

        if (e->state == 3 && e->num_transform > 0 &&
            e->current_frame >= e->num_transform)
        {
            e->transformed   = 1;
            e->state         = 1;
            e->current_frame = 0;
            fprintf(stderr, "[BOSS] Transformation finished!\n");
        }
    }

    if (e->hit_cooldown > 0) e->hit_cooldown--;
}

void render_enemy(Enemy *e, SDL_Renderer *ren, int cam_x)
{
    if (!e->alive) return;

    int sx = e->x - cam_x;
    if (sx + e->width < 0 || sx > SCREEN_WIDTH) return;

    SDL_Rect dest = {sx, e->y, e->width, e->height};
    SDL_Texture *tex = NULL;

    if (e->state == 3 && e->num_transform > 0) {
        int idx = e->current_frame < e->num_transform
                  ? e->current_frame : e->num_transform - 1;
        tex = e->transform_frames[idx];

    } else if (e->state == 1) {
        /* FIX [ENEMY-ANIM-OOB-1]: guard num_walk > 0 before modulo */
        if (e->transformed && e->num_walk_trans > 0)
            tex = e->walk_trans_frames[e->current_frame % e->num_walk_trans];
        else if (e->num_walk > 0)
            tex = e->walk_frames[e->current_frame % e->num_walk];
        /* else tex stays NULL → falls through to idle1 below */

    } else if (e->state == 2) {
        /* FIX [ENEMY-ANIM-OOB-1]: guard num_attack > 0 before modulo */
        if (e->transformed && e->num_attack_trans > 0)
            tex = e->attack_trans_frames[e->current_frame % e->num_attack_trans];
        else if (e->num_attack > 0)
            tex = e->attack_frames[e->current_frame % e->num_attack];
        /* else tex stays NULL → falls through to idle1 below */
    }

    if (!tex) tex = e->idle1;

    if (tex) {
        SDL_RendererFlip flip = (e->dx >= 0) ? SDL_FLIP_NONE : SDL_FLIP_HORIZONTAL;
        SDL_RenderCopyEx(ren, tex, NULL, &dest, 0, NULL, flip);
    } else {
        SDL_SetRenderDrawColor(ren,
                               (e->type == 0) ? 255 : 180,
                               80, 80, 255);
        SDL_RenderFillRect(ren, &dest);
    }

    if (e->health > 0) {
        /* Clamp health to valid range before width computation */
        int hp = e->health > 0 ? e->health : 0;
        int bar_w = (e->width * hp) / e->max_health;
        if (bar_w < 0) bar_w = 0;

        SDL_SetRenderDrawColor(ren, 40, 40, 40, 255);
        SDL_Rect bg_bar = {sx, e->y - 18, e->width, 8};
        SDL_RenderFillRect(ren, &bg_bar);

        int br = (hp < e->max_health / 2) ? 220 : 80;
        SDL_SetRenderDrawColor(ren, br, 200, 50, 255);
        SDL_Rect hp_bar = {sx, e->y - 18, bar_w, 8};
        SDL_RenderFillRect(ren, &hp_bar);

        if (e->type == 1) {
            SDL_SetRenderDrawColor(ren, 255, 80, 0, 255);
            SDL_RenderDrawRect(ren, &bg_bar);
        }
    }
}

int check_enemy_collision(SDL_Rect a, SDL_Rect b)
{
    if (a.x + a.w <= b.x || b.x + b.w <= a.x) return 0;
    if (a.y + a.h <= b.y || b.y + b.h <= a.y) return 0;
    return 1;
}

void destroy_enemy(Enemy *e)
{
    if (!e) return;
    if (e->idle1) { SDL_DestroyTexture(e->idle1); e->idle1 = NULL; }
    if (e->idle2) { SDL_DestroyTexture(e->idle2); e->idle2 = NULL; }
    for (int i = 0; i < e->num_walk;         i++) SDL_DestroyTexture(e->walk_frames[i]);
    for (int i = 0; i < e->num_attack;       i++) SDL_DestroyTexture(e->attack_frames[i]);
    for (int i = 0; i < e->num_transform;    i++) SDL_DestroyTexture(e->transform_frames[i]);
    for (int i = 0; i < e->num_walk_trans;   i++) SDL_DestroyTexture(e->walk_trans_frames[i]);
    for (int i = 0; i < e->num_attack_trans; i++) SDL_DestroyTexture(e->attack_trans_frames[i]);
    memset(e, 0, sizeof(Enemy));
}