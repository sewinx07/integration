/**
 * minimap.c - Minimap overlay for MATRIX GAME
 *
 * FIXES:
 *  - Full-screen damage flash is now drawn LAST (after the minimap box)
 *    so it is never covered by background or platform renders.
 *  - damageEvent is consumed here; it is now set reliably by perdreVie()
 *    in player.c for ALL damage sources (enemy contact, void trap,
 *    mobile-platform crush).
 *  - Added a bright "DAMAGE!" text banner that appears on the main screen
 *    alongside the red flash so players can't miss it.
 */

#include "minimap.h"
#include "font.h"
#include <stdio.h>

/* ── world-to-minimap coordinate ───────────────────────────────── */
static int mmX(float worldX) {
    return MM_X + (int)((worldX / (float)MM_WORLD_W) * MM_W);
}
static int mmY(float worldY) {
    return MM_Y + (int)((worldY / (float)MM_WORLD_H) * MM_H);
}

/* ── filled dot, clamped inside the box ────────────────────────── */
static void drawDot(SDL_Renderer *r, int cx, int cy, int size,
                    Uint8 red, Uint8 green, Uint8 blue)
{
    SDL_Rect dot = { cx - size/2, cy - size/2, size, size };
    if (dot.x < MM_X)                 dot.x = MM_X;
    if (dot.y < MM_Y)                 dot.y = MM_Y;
    if (dot.x + dot.w > MM_X + MM_W) dot.x = MM_X + MM_W - dot.w;
    if (dot.y + dot.h > MM_Y + MM_H) dot.y = MM_Y + MM_H - dot.h;
    SDL_SetRenderDrawColor(r, red, green, blue, 255);
    SDL_RenderFillRect(r, &dot);
}

/* ── grab first frame of idle animation as thumbnail ───────────── */
static SDL_Texture *grabThumb(Player *p)
{
    if (!p) return NULL;
    Animation *a = &p->anims[STATE_IDLE];
    if (!a->textures || a->frameCount == 0) return NULL;
    return a->textures[0];
}

/* ── draw the player dot on the minimap itself ──────────────────── */
static void drawPlayerOnMinimap(SDL_Renderer *renderer, Minimap *mm,
                                 Player *p, SDL_Texture *thumb,
                                 Uint8 tr, Uint8 tg, Uint8 tb)
{
    if (!p || !p->isAlive) return;
    int px = mmX(p->worldX);
    int py = mmY(p->worldY);
    SDL_Rect dst = { px - 7, py - 7, 14, 14 };
    if (dst.x < MM_X)              dst.x = MM_X;
    if (dst.y < MM_Y)              dst.y = MM_Y;
    if (dst.x + dst.w > MM_X+MM_W) dst.x = MM_X+MM_W-dst.w;
    if (dst.y + dst.h > MM_Y+MM_H) dst.y = MM_Y+MM_H-dst.h;

    if (thumb) {
        SDL_SetTextureColorMod(thumb, tr, tg, tb);
        SDL_RenderCopy(renderer, thumb, NULL, &dst);
        SDL_SetTextureColorMod(thumb, 255, 255, 255);
    } else {
        drawDot(renderer, px, py, MM_DOT_PLAYER, tr, tg, tb);
    }
    (void)mm;
}

/* ── background image path per level ───────────────────────────── */
static const char *BG_PATH[3] = {
    NULL,                                               /* index 0 unused */
    "assets/backgrounds/matrix_background_final.png",  /* LEVEL_1        */
    "assets/backgrounds/background_level2.png"         /* LEVEL_2        */
};

/* ── internal: (re)load the minimap background texture ─────────── */
static void loadBgTex(Minimap *mm, SDL_Renderer *renderer, int level)
{
    /* free previous texture if any */
    if (mm->bgTex) {
        SDL_DestroyTexture(mm->bgTex);
        mm->bgTex = NULL;
    }

    const char *path = (level >= 1 && level <= 2) ? BG_PATH[level] : BG_PATH[1];
    SDL_Surface *surf = IMG_Load(path);
    if (!surf) {
        fprintf(stderr, "[MINIMAP] Cannot load background (%s): %s\n",
                path, IMG_GetError());
        return;
    }
    mm->bgTex = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_FreeSurface(surf);
    if (!mm->bgTex)
        fprintf(stderr, "[MINIMAP] Texture creation failed: %s\n", SDL_GetError());
    else
        fprintf(stderr, "[MINIMAP] Loaded level %d background.\n", level);
}

/* ================================================================
 *  INIT
 * ================================================================ */
void initMinimap(Minimap *mm, SDL_Renderer *renderer,
                 Player *p1, Player *p2)
{
    mm->bgTex       = NULL;
    mm->renderer    = renderer;   /* store for later reloads */
    mm->p1Thumb     = grabThumb(p1);
    mm->p2Thumb     = grabThumb(p2);
    mm->damageFlash = 0;
    mm->bossFlash   = 0;

    loadBgTex(mm, renderer, 1);   /* start on level 1 */
}

/* ================================================================
 *  LEVEL CHANGE  — call this from main.c whenever the level switches
 * ================================================================ */
void minimapSetLevel(Minimap *mm, int level)
{
    loadBgTex(mm, mm->renderer, level);
}

/* ================================================================
 *  TRIGGER HELPERS
 * ================================================================ */
void minimapTriggerEnemyHit(Minimap *mm)
{
    mm->damageFlash = DAMAGE_FLASH_FRAMES;
}

void minimapTriggerBossTouch(Minimap *mm)
{
    mm->bossFlash = BOSS_CONTACT_FLASH_FRAMES;
}

/* ================================================================
 *  RENDER
 *
 *  Draw order:
 *    1. Minimap dark bg
 *    2. Minimap background texture
 *    3. Green border
 *    4. Enemy dots
 *    5. Player thumbnails / dots
 *    6. Full-screen flash overlays  ← LAST so they cover everything
 *    7. "DAMAGE!" / "BOSS!" text banner
 * ================================================================ */
void renderMinimap(Minimap *mm, SDL_Renderer *renderer,
                   Player *p1, Player *p2, int showP2,
                   Enemy *minions, int minionCount,
                   Enemy *boss,    int bossActive)
{
    /* ── consume damageEvent flags from players ── */
    if (p1 && p1->damageEvent) {
        mm->damageFlash = DAMAGE_FLASH_FRAMES;
        p1->damageEvent = 0;
    }
    if (p2 && p2->damageEvent) {
        mm->damageFlash = DAMAGE_FLASH_FRAMES;
        p2->damageEvent = 0;
    }

    /* ── 1. dark background box ── */
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 170);
    SDL_Rect box = {MM_X, MM_Y, MM_W, MM_H};
    SDL_RenderFillRect(renderer, &box);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

    /* ── 2. minimap background texture ── */
    if (mm->bgTex)
        SDL_RenderCopy(renderer, mm->bgTex, NULL, &box);

    /* ── 3. green border ── */
    SDL_SetRenderDrawColor(renderer, 0, 220, 60, 255);
    SDL_RenderDrawRect(renderer, &box);

    /* ── 4. enemy dots ── */
    for (int i = 0; i < minionCount; i++) {
        if (!minions[i].alive) continue;
        drawDot(renderer,
                mmX((float)minions[i].x),
                mmY((float)minions[i].y),
                MM_DOT_ENEMY, 220, 50, 50);
    }
    if (bossActive && boss && boss->alive)
        drawDot(renderer,
                mmX((float)boss->x),
                mmY((float)boss->y),
                MM_DOT_ENEMY + 3, 255, 100, 0);

    /* ── 5. player thumbnails ── */
    if (showP2)
        drawPlayerOnMinimap(renderer, mm, p2, mm->p2Thumb, 80, 220, 255);
    drawPlayerOnMinimap(renderer, mm, p1, mm->p1Thumb, 80, 255, 100);

    /* ── 6. full-screen flash overlays (drawn LAST) ── */
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    if (mm->damageFlash > 0) {
        Uint8 alpha = (Uint8)(220 * mm->damageFlash / DAMAGE_FLASH_FRAMES);
        SDL_SetRenderDrawColor(renderer, 220, 0, 0, alpha);
        SDL_Rect full = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
        SDL_RenderFillRect(renderer, &full);

        /* ── 7a. "DAMAGE!" text banner in centre of screen ── */
        if (mm->damageFlash > DAMAGE_FLASH_FRAMES / 2) {
            drawTextCentered(renderer,
                             SCREEN_HEIGHT / 2 - 20,
                             0, SCREEN_WIDTH,
                             "DAMAGE!", 4, 255, 60, 60);
        }

        mm->damageFlash--;
    }

    if (mm->bossFlash > 0) {
        Uint8 alpha = (Uint8)(200 * mm->bossFlash / BOSS_CONTACT_FLASH_FRAMES);
        SDL_SetRenderDrawColor(renderer, 255, 120, 0, alpha);
        SDL_Rect full = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
        SDL_RenderFillRect(renderer, &full);

        /* ── 7b. "BOSS!" text banner ── */
        if (mm->bossFlash > BOSS_CONTACT_FLASH_FRAMES / 2) {
            drawTextCentered(renderer,
                             SCREEN_HEIGHT / 2 - 20,
                             0, SCREEN_WIDTH,
                             "BOSS!", 4, 255, 160, 0);
        }

        mm->bossFlash--;
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

/* ================================================================
 *  FREE
 * ================================================================ */
void freeMinimap(Minimap *mm)
{
    if (mm->bgTex) {
        SDL_DestroyTexture(mm->bgTex);
        mm->bgTex = NULL;
    }
    mm->p1Thumb = NULL;
    mm->p2Thumb = NULL;
}