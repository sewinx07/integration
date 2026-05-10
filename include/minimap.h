/**
 * minimap.h - Minimap overlay for MATRIX GAME
 * Top-right corner, 30% bigger than original (260x78).
 *
 * Features:
 *  - Per-level background image (updates when level changes)
 *  - Player sprite thumbnail visible on the minimap
 *  - Red dots for minions, orange dot for boss
 *  - Full-screen RED flash when hit by an enemy / void / mobile platform
 *  - Full-screen ORANGE flash when touching the boss
 */
#ifndef MINIMAP_H
#define MINIMAP_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include "common.h"
#include "enemy.h"
#include "player.h"

/* ── size: original 200x60 * 1.3 = 260x78 ── */
#define MM_W        260
#define MM_H        78
#define MM_X        (SCREEN_WIDTH  - MM_W - 4)
#define MM_Y        4

/* world dimensions the minimap covers */
#define MM_WORLD_W  WORLD_WIDTH
#define MM_WORLD_H  SCREEN_HEIGHT

/* dot sizes */
#define MM_DOT_PLAYER   7
#define MM_DOT_ENEMY    4

/* flash durations (frames at 60 fps) */
#define DAMAGE_FLASH_FRAMES       18   /* ~0.3 s — enemy/void/platform hit */
#define BOSS_CONTACT_FLASH_FRAMES 10   /* ~0.17 s — boss body touch        */

typedef struct {
    SDL_Renderer *renderer;      /* stored so minimapSetLevel can reload  */
    SDL_Texture  *bgTex;         /* current level's background thumbnail  */
    SDL_Texture  *p1Thumb;       /* P1 sprite thumbnail on minimap        */
    SDL_Texture  *p2Thumb;       /* P2 sprite thumbnail on minimap        */
    int           damageFlash;   /* countdown: damage hit  → red flash    */
    int           bossFlash;     /* countdown: boss touch  → orange flash */
} Minimap;

/* lifecycle */
void initMinimap    (Minimap *mm, SDL_Renderer *renderer,
                     Player *p1, Player *p2);
void freeMinimap    (Minimap *mm);

/* call this every time the game level changes */
void minimapSetLevel(Minimap *mm, int level);

/* main render call (once per frame, after all other rendering) */
void renderMinimap  (Minimap *mm, SDL_Renderer *renderer,
                     Player *p1, Player *p2, int showP2,
                     Enemy *minions, int minionCount,
                     Enemy *boss,    int bossActive);

/* optional manual triggers (perdreVie sets damageEvent automatically) */
void minimapTriggerEnemyHit (Minimap *mm);   /* red flash    */
void minimapTriggerBossTouch(Minimap *mm);   /* orange flash */

#endif