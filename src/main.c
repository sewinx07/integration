/**
 * main.c — MATRIX GAME  (Comprehensive Fix Build + Character Select)
 * ====================================================================
 *
 * CHARACTER SELECT ADDITIONS (all other fixes preserved unchanged):
 *
 *  [CHARSEL-1] Added APP_CHARACTER_SELECT state between MODE_SELECT and
 *              NAME_ENTRY.  In solo mode, VALIDER now routes to character
 *              select first; multi-player skips it (unchanged).
 *
 *  [CHARSEL-2] renderCharacterSelect() draws two portrait cards (NEO /
 *              TRINITY) with hover glow, animated scan-line, and stats.
 *              No sprites are required — everything is drawn procedurally.
 *
 *  [CHARSEL-3] selectedCharacter (0=NEO, 1=TRINITY) is stored globally
 *              and applied to p1.name at game-start.  Game logic, player
 *              IDs, and sprite folders are NOT changed.
 *
 * All original fixes ([INPUT-1..4], [BOSS-1..4], [ENIGME-1..5],
 * [SPAWN-1..2], [KILL-1], [RENDER-1..2], [HUD-1], [MEMORY-1],
 * [PAUSE-1], [CAMERA-1], [NAMEINPUT-1], [LED-2]) are preserved verbatim.
 *
 * [ESC-SAVE-1] ESC during APP_GAME now saves the score before returning
 *              to the main menu: reuses nameState.name if available,
 *              otherwise prompts via saisirNomJoueur, then shows the
 *              leaderboard screen before resuming the menu music.
 */

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_mixer.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "common.h"
#include "background.h"
#include "player.h"
#include "font.h"
#include "enemy.h"
#include "minimap.h"
#include "matrix.h"
#include "audio.h"
#include "enigme.h"

/* ============================================================
 * CONSTANTS
 * ============================================================ */
#define WINDOW_TITLE          "MATRIX GAME"
#define FPS_TARGET            60
#define FRAME_MS              (1000 / FPS_TARGET)
#define MAX_MINIONS           5
#define BULLET_DAMAGE         40
#define BTN_COUNT             5
#define MINION_KILL_THRESHOLD 8

/* ============================================================
 * APP STATES
 * [CHARSEL-1] APP_CHARACTER_SELECT inserted between MODE_SELECT and NAME_ENTRY
 * ============================================================ */
typedef enum {
    APP_MAIN_MENU,
    APP_MODE_SELECT,
    APP_CHARACTER_SELECT,   /* ← NEW */
    APP_NAME_ENTRY,
    APP_OPTION,
    APP_SCORES,
    APP_HISTOIRE,
    APP_GAME,
    APP_SAVE_SCREEN,
    APP_ENIGME_CHOICE,
    APP_ENIGME_QCM,
    APP_ENIGME_PUZZLE,
    APP_ENIGME_RESULT,
} AppState;

/* [CHARSEL-3] 0 = NEO, 1 = TRINITY */
static int selectedCharacter = 0;

/* ============================================================
 * TTF BUTTON
 * ============================================================ */
typedef struct {
    SDL_Rect    rect;
    const char *label;
    SDL_Texture *tex;
    SDL_Rect    texRect;
    int         hover;
} TBtn;

static void tbtnBuild(TBtn *b, SDL_Renderer *ren, TTF_Font *font,
                      const char *label, int x, int y, int w, int h)
{
    b->rect  = (SDL_Rect){x, y, w, h};
    b->label = label;
    b->hover = 0;
    b->tex   = NULL;
    SDL_Color white = {255, 255, 255, 255};
    SDL_Surface *s  = TTF_RenderText_Blended(font, label, white);
    if (!s) return;
    b->tex     = SDL_CreateTextureFromSurface(ren, s);
    b->texRect = (SDL_Rect){x + (w - s->w) / 2,
                             y + (h - s->h) / 2, s->w, s->h};
    SDL_FreeSurface(s);
}

static void tbtnDraw(SDL_Renderer *ren, TBtn *b)
{
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    if (b->hover)
        SDL_SetRenderDrawColor(ren, 57, 255, 20, 120);
    else
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 180);
    SDL_RenderFillRect(ren, &b->rect);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(ren, 57, 255, 20, 255);
    SDL_RenderDrawRect(ren, &b->rect);
    if (b->tex) SDL_RenderCopy(ren, b->tex, NULL, &b->texRect);
}

static int tbtnHit(TBtn *b, int mx, int my)
{
    SDL_Point p = {mx, my};
    return SDL_PointInRect(&p, &b->rect);
}

static void tbtnFree(TBtn *b)
{
    if (b->tex) { SDL_DestroyTexture(b->tex); b->tex = NULL; }
}

/* ============================================================
 * BITMAP BUTTON
 * ============================================================ */
typedef struct { SDL_Rect r; const char *text; int hover; } BBtn;

static int bbtnMouseOn(BBtn *b, int x, int y) {
    return x >= b->r.x && x <= b->r.x + b->r.w &&
           y >= b->r.y && y <= b->r.y + b->r.h;
}

static void bbtnDraw(SDL_Renderer *ren, BBtn *b) {
    SDL_Rect d = b->r;
    if (b->hover) { d.x -= 4; d.y -= 4; d.w += 8; d.h += 8;
                    SDL_SetRenderDrawColor(ren, 0, 255, 80, 255); }
    else          { SDL_SetRenderDrawColor(ren, 0, 140, 40, 220); }
    SDL_RenderFillRect(ren, &d);
    SDL_SetRenderDrawColor(ren, 0, 255, 60, 255);
    SDL_RenderDrawRect(ren, &d);
    int tw = textWidth(b->text, 2), th = textHeight(2);
    drawText(ren, d.x + (d.w - tw) / 2, d.y + (d.h - th) / 2,
             b->text, 2, 0, 255, 100);
}

/* ============================================================
 * [CHARSEL-2] CHARACTER SELECTION SCREEN
 * ============================================================ */

static void drawFilledRect(SDL_Renderer *ren, SDL_Rect r,
                            Uint8 rr, Uint8 g, Uint8 b, Uint8 a)
{
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren, rr, g, b, a);
    SDL_RenderFillRect(ren, &r);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
}

static void drawBorderedRect(SDL_Renderer *ren, SDL_Rect r,
                              Uint8 rr, Uint8 g, Uint8 b, int thick)
{
    SDL_SetRenderDrawColor(ren, rr, g, b, 255);
    for (int i = 0; i < thick; i++) {
        SDL_Rect inner = {r.x + i, r.y + i, r.w - i*2, r.h - i*2};
        SDL_RenderDrawRect(ren, &inner);
    }
}

static void drawStatBar(SDL_Renderer *ren, int x, int y,
                         const char *label, int value,
                         Uint8 rr, Uint8 g, Uint8 b)
{
    drawText(ren, x, y, label, 1, 140, 200, 140);

    int bx = x + 80, bw = 120, bh = 8;
    SDL_SetRenderDrawColor(ren, 20, 40, 20, 255);
    SDL_Rect bg = {bx, y + 1, bw, bh};
    SDL_RenderFillRect(ren, &bg);
    int fw = bw * value / 100;
    SDL_SetRenderDrawColor(ren, rr, g, b, 255);
    SDL_Rect fill = {bx, y + 1, fw, bh};
    SDL_RenderFillRect(ren, &fill);
    SDL_SetRenderDrawColor(ren, 0, 120, 30, 255);
    SDL_RenderDrawRect(ren, &bg);
}

static int renderCharacterSelect(SDL_Renderer *ren,
                                  TTF_Font *font, TTF_Font *fontSmall,
                                  SDL_Event *ev, int mx, int my)
{
    SDL_SetRenderDrawColor(ren, 0, 8, 3, 255);
    SDL_Rect full = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
    SDL_RenderFillRect(ren, &full);

    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren, 0, 60, 0, 18);
    for (int gx = 0; gx < SCREEN_WIDTH;  gx += 40)
        SDL_RenderDrawLine(ren, gx, 0, gx, SCREEN_HEIGHT);
    for (int gy = 0; gy < SCREEN_HEIGHT; gy += 40)
        SDL_RenderDrawLine(ren, 0, gy, SCREEN_WIDTH, gy);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);

    SDL_Color green  = {57, 255, 20, 255};
    SDL_Color white  = {220, 255, 220, 255};

    SDL_Surface *ts = TTF_RenderText_Blended(font, "CHOISISSEZ VOTRE PERSONNAGE", green);
    if (ts) {
        SDL_Texture *tt = SDL_CreateTextureFromSurface(ren, ts);
        if (tt) {
            SDL_Rect tr = {SCREEN_WIDTH/2 - ts->w/2, 34, ts->w, ts->h};
            SDL_RenderCopy(ren, tt, NULL, &tr);
            SDL_DestroyTexture(tt);
        }
        SDL_FreeSurface(ts);
    }

    SDL_Surface *sub = TTF_RenderText_Blended(fontSmall,
        "Cliquez sur un personnage pour le selectionner", white);
    if (sub) {
        SDL_Texture *st = SDL_CreateTextureFromSurface(ren, sub);
        if (st) {
            SDL_Rect sr = {SCREEN_WIDTH/2 - sub->w/2, 80, sub->w, sub->h};
            SDL_RenderCopy(ren, st, NULL, &sr);
            SDL_DestroyTexture(st);
        }
        SDL_FreeSurface(sub);
    }

    int cardW = 320, cardH = 440;
    int gap   = 80;
    int totalW = cardW * 2 + gap;
    int cardX1 = SCREEN_WIDTH/2 - totalW/2;
    int cardX2 = cardX1 + cardW + gap;
    int cardY  = 115;

    SDL_Rect card1 = {cardX1, cardY, cardW, cardH};
    SDL_Rect card2 = {cardX2, cardY, cardW, cardH};

    int hov1 = (mx >= card1.x && mx <= card1.x + card1.w &&
                my >= card1.y && my <= card1.y + card1.h);
    int hov2 = (mx >= card2.x && mx <= card2.x + card2.w &&
                my >= card2.y && my <= card2.y + card2.h);

    Uint32 ticks = SDL_GetTicks();

    for (int card = 0; card < 2; card++) {
        SDL_Rect cr   = (card == 0) ? card1 : card2;
        int      hov  = (card == 0) ? hov1  : hov2;
        int      cx   = cr.x + cr.w / 2;

        drawFilledRect(ren, cr, 0, hov ? 28 : 14, 0, hov ? 240 : 200);

        if (hov) {
            for (int pass = 3; pass >= 1; pass--) {
                SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(ren, 57, 255, 20, (Uint8)(60 / pass));
                SDL_Rect glow = {cr.x - pass, cr.y - pass,
                                 cr.w + pass*2, cr.h + pass*2};
                SDL_RenderDrawRect(ren, &glow);
                SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
            }
            drawBorderedRect(ren, cr, 57, 255, 20, 2);
        } else {
            drawBorderedRect(ren, cr, 0, 80, 20, 1);
        }

        if (hov) {
            int scanY = cr.y + (int)((ticks / 6) % (Uint32)cr.h);
            SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(ren, 57, 255, 20, 35);
            SDL_Rect scan = {cr.x + 1, scanY, cr.w - 2, 3};
            SDL_RenderFillRect(ren, &scan);
            SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
        }

        {
            const char *imgPath = (card == 0)
                ? "assets/sprites/idle/frame0.png"
                : "assets/personage2/standing/s1.png";
            SDL_Texture *portrait = IMG_LoadTexture(ren, imgPath);
            if (portrait) {
                int imgW, imgH;
                SDL_QueryTexture(portrait, NULL, NULL, &imgW, &imgH);
                int maxW = cr.w - 20;
                int maxH = 230;
                float scale = (float)maxW / imgW;
                if (imgH * scale > maxH) scale = (float)maxH / imgH;
                int dw = (int)(imgW * scale);
                int dh = (int)(imgH * scale);
                if (!hov) SDL_SetTextureColorMod(portrait, 130, 130, 130);
                SDL_Rect pdst = {cx - dw/2, cardY + 18, dw, dh};
                SDL_RenderCopy(ren, portrait, NULL, &pdst);
                SDL_SetTextureColorMod(portrait, 255, 255, 255);
                SDL_DestroyTexture(portrait);
            }
        }

        const char *charName = (card == 0) ? "NEO" : "MORPHEUS";
        SDL_Color nameCol = hov ? (SDL_Color){57, 255, 20, 255}
                                : (SDL_Color){0, 140, 40, 255};
        SDL_Surface *ns = TTF_RenderText_Blended(font, charName, nameCol);
        if (ns) {
            SDL_Texture *nt = SDL_CreateTextureFromSurface(ren, ns);
            if (nt) {
                SDL_Rect nr = {cx - ns->w/2, cardY + 268, ns->w, ns->h};
                SDL_RenderCopy(ren, nt, NULL, &nr);
                SDL_DestroyTexture(nt);
            }
            SDL_FreeSurface(ns);
        }

        const char *role = (card == 0)
            ? "L'ELU - HACKER LEGENDAIRE"
            : "LE GUIDE - MAITRE DU COMBAT";
        SDL_Surface *rs = TTF_RenderText_Blended(fontSmall, role,
            (SDL_Color){0, 160, 60, 255});
        if (rs) {
            SDL_Texture *rt = SDL_CreateTextureFromSurface(ren, rs);
            if (rt) {
                SDL_Rect rr2 = {cx - rs->w/2, cardY + 300, rs->w, rs->h};
                SDL_RenderCopy(ren, rt, NULL, &rr2);
                SDL_DestroyTexture(rt);
            }
            SDL_FreeSurface(rs);
        }

        SDL_SetRenderDrawColor(ren, 0, 100, 30, 255);
        SDL_RenderDrawLine(ren, cr.x + 20, cardY + 326,
                                cr.x + cr.w - 20, cardY + 326);

        int statX  = cr.x + 20;
        int statY0 = cardY + 338;

        if (card == 0) {
            drawStatBar(ren, statX, statY0 +  0, "VITESSE",   78, 57, 255, 20);
            drawStatBar(ren, statX, statY0 + 22, "PUISSANCE", 92, 57, 255, 20);
            drawStatBar(ren, statX, statY0 + 44, "AGILITE",   85, 57, 255, 20);
        } else {
            drawStatBar(ren, statX, statY0 +  0, "VITESSE",   95, 20, 200, 255);
            drawStatBar(ren, statX, statY0 + 22, "PUISSANCE", 70, 20, 200, 255);
            drawStatBar(ren, statX, statY0 + 44, "AGILITE",   98, 20, 200, 255);
        }

        if (hov) {
            int pulse = (int)(200 + 55 * SDL_sin((double)ticks / 200.0));
            SDL_Surface *sel = TTF_RenderText_Blended(fontSmall, "[ SELECTIONNER ]",
                (SDL_Color){(Uint8)pulse, 255, (Uint8)pulse, 255});
            if (sel) {
                SDL_Texture *st2 = SDL_CreateTextureFromSurface(ren, sel);
                if (st2) {
                    SDL_Rect selr = {cx - sel->w/2,
                                     cardY + cardH - sel->h - 12,
                                     sel->w, sel->h};
                    SDL_RenderCopy(ren, st2, NULL, &selr);
                    SDL_DestroyTexture(st2);
                }
                SDL_FreeSurface(sel);
            }
        }
    }

    drawTextCentered(ren, cardY + cardH + 22, 0, SCREEN_WIDTH,
                     "ESC = RETOUR", 1, 0, 100, 30);

    if (ev && ev->type == SDL_MOUSEBUTTONDOWN &&
        ev->button.button == SDL_BUTTON_LEFT) {
        int ex = ev->button.x, ey = ev->button.y;
        if (ex >= card1.x && ex <= card1.x + card1.w &&
            ey >= card1.y && ey <= card1.y + card1.h)
            return 0;
        if (ex >= card2.x && ex <= card2.x + card2.w &&
            ey >= card2.y && ey <= card2.y + card2.h)
            return 1;
    }
    return -1;
}

/* ============================================================
 * GAME HELPERS
 * ============================================================ */

static void setEnemyY(Enemy *e, GameLevel level) {
    int groundY = (level == LEVEL_1) ? 508 : 560;
    e->y = groundY - e->height;
    if (e->y < 0) e->y = 0;
}

static void checkBulletsVsEnemies(Player *p,
                                   Enemy *minions, int minionCount,
                                   Enemy *boss,    int bossActive)
{
    for (int b = 0; b < MAX_BULLETS; b++) {
        if (!p->bullets[b].active) continue;
        SDL_Rect br = {(int)p->bullets[b].x, (int)p->bullets[b].y,
                       BULLET_W, BULLET_H_PX};

        for (int i = 0; i < minionCount; i++) {
            if (!minions[i].alive) continue;
            if (minions[i].hit_cooldown > 0) continue;
            SDL_Rect er = {minions[i].x, minions[i].y,
                           minions[i].width, minions[i].height};
            if (check_enemy_collision(br, er)) {
                minions[i].health -= BULLET_DAMAGE;
                minions[i].hit_cooldown = 8;
                p->bullets[b].active = 0;
                p->score += 10;
                if (minions[i].health <= 0) minions[i].alive = 0;
                break;
            }
        }

        if (!p->bullets[b].active) continue;

        if (bossActive && boss && boss->alive) {
            if (boss->hit_cooldown == 0) {
                SDL_Rect er = {boss->x, boss->y, boss->width, boss->height};
                if (check_enemy_collision(br, er)) {
                    boss->health -= BULLET_DAMAGE;
                    boss->hit_cooldown = 6;
                    p->bullets[b].active = 0;
                    p->score += 20;
                    if (boss->health <= 0) boss->alive = 0;
                }
            }
        }
    }
}

static void checkPlayerVsEnemy(Player *p, Enemy *e, Minimap *mm) {
    if (!e->alive || !p->isAlive) return;
    SDL_Rect pr = {(int)p->worldX + 8, (int)p->worldY + 4,
                   PLAYER_W - 16, PLAYER_PH - 8};
    SDL_Rect er = {e->x, e->y, e->width, e->height};
    if (!check_enemy_collision(pr, er)) return;

    if (p->velY > 1.0f && (int)p->worldY + PLAYER_PH < e->y + e->height / 2) {
        e->health -= 999;
        e->alive   = 0;
        p->velY    = -12.0f;
        p->score  += 100;
        return;
    }

    Uint32 now    = SDL_GetTicks();
    int    canHit = (now - p->lastDamageTime) > 1000;
    if (canHit) {
        if (e->state == 2 || e->type == 1) {
            p->health        -= 20;
            p->lastDamageTime = now;
            p->damageEvent    = 1;
            if (e->type == 1 && mm) minimapTriggerBossTouch(mm);
            if (p->health <= 0) {
                p->health  = 0;
                p->isAlive = 0;
                p->state   = STATE_DEATH;
            }
        }
    }
}

static void renderHalf(SDL_Renderer *renderer, Background *bg,
                        Player *p, Player *other, SDL_Rect vp,
                        Enemy *minions, int minionCount,
                        Enemy *boss,    int bossActive)
{
    SDL_RenderSetViewport(renderer, &vp);
    SDL_Rect localClip = {0, 0, vp.w, vp.h};
    SDL_RenderSetClipRect(renderer, &localClip);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderFillRect(renderer, &localClip);

    float savedCamX = bg->camX;
    float savedCamY = bg->camY;
    bg->camX = p->camX;
    bg->camY = 0;
    afficherBackground(bg, renderer, MODE_MULTI, &vp);
    bg->camX = savedCamX;
    bg->camY = savedCamY;

    for (int i = 0; i < minionCount; i++)
        render_enemy(&minions[i], renderer, (int)p->camX);
    if (bossActive && boss && boss->alive)
        render_enemy(boss, renderer, (int)p->camX);

    afficherJoueur(p, renderer, p->camX, 0);
    afficherBalles(p, renderer, p->camX, 0);

    if (other && other->isAlive) {
        int ox = (int)(other->worldX - p->camX);
        if (ox > -PLAYER_W && ox < vp.w) {
            afficherJoueur(other, renderer, p->camX, 0);
        }
    }

    afficherHUDJoueur(p, renderer, 8, 8);
    afficherTemps(bg, renderer, vp.w - 160, 8);
    afficherNotification(bg, renderer);

    const char *lbl = (p->id == PLAYER_1) ? "J1-NEO" : "J2-TRINITY";
    Uint8 cb = (p->id == PLAYER_1) ? 50 : 255;
    drawText(renderer, 8, vp.h - 20, lbl, 1, 0, 200, cb);

    SDL_RenderSetClipRect(renderer, NULL);
    SDL_RenderSetViewport(renderer, NULL);
}

static void resetEnemies(Enemy *minions, int count, Enemy *boss,
                          int *bossSpawned, int *bossActive,
                          int *spawnTimer,  int *killCounter,
                          SDL_Renderer *renderer, GameLevel level)
{
    for (int i = 0; i < count; i++) {
        destroy_enemy(&minions[i]);
        memset(&minions[i], 0, sizeof(Enemy));
    }
    if (*bossActive || (boss && boss->alive)) {
        destroy_enemy(boss);
        memset(boss, 0, sizeof(Enemy));
    }
    *bossSpawned = 0;
    *bossActive  = 0;
    *spawnTimer  = 90;
    if (killCounter) *killCounter = 0;

    init_enemy(&minions[0], 0, renderer, 900);  setEnemyY(&minions[0], level);
    init_enemy(&minions[1], 0, renderer, 1500); setEnemyY(&minions[1], level);
}

static void drawBossHealthBar(SDL_Renderer *ren, Enemy *boss)
{
    if (!boss || !boss->alive || boss->health <= 0) return;
    int bw = SCREEN_WIDTH - 40;
    int bh = 20;
    int bx = 20;
    int by = SCREEN_HEIGHT - 34;

    SDL_SetRenderDrawColor(ren, 30, 0, 0, 220);
    SDL_Rect bg = {bx, by, bw, bh};
    SDL_RenderFillRect(ren, &bg);

    int fillW = (int)((float)bw * boss->health / boss->max_health);
    if (fillW < 0) fillW = 0;
    if (fillW > bw) fillW = bw;

    float ratio = (float)boss->health / (float)boss->max_health;
    Uint8 br = (ratio > 0.5f) ? (Uint8)(255 * (1.0f - ratio) * 2) : 255;
    Uint8 bg2 = (ratio > 0.5f) ? 180 : (Uint8)(255 * ratio * 2);
    SDL_SetRenderDrawColor(ren, br, bg2, 0, 255);
    SDL_Rect fill = {bx, by, fillW, bh};
    SDL_RenderFillRect(ren, &fill);

    SDL_SetRenderDrawColor(ren, 255, 100, 0, 255);
    SDL_RenderDrawRect(ren, &bg);

    char hpbuf[32];
    snprintf(hpbuf, sizeof(hpbuf), "BOSS  %d/%d", boss->health, boss->max_health);
    int tw = textWidth(hpbuf, 1);
    drawText(ren, bx + bw / 2 - tw / 2, by + 4, hpbuf, 1, 255, 220, 0);
}

/* ============================================================
 * OPTION SCREEN
 * ============================================================ */
static void renderOption(SDL_Renderer *ren, TTF_Font *font,
                          TBtn *backBtn, int mx, int my,
                          int musicVol, GameAudio *au)
{
    SDL_SetRenderDrawColor(ren, 0, 10, 0, 255);
    SDL_Rect full = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
    SDL_RenderFillRect(ren, &full);

    SDL_Color green = {57, 255, 20, 255};
    SDL_Surface *ts = TTF_RenderText_Blended(font, "OPTIONS", green);
    if (ts) {
        SDL_Texture *tt = SDL_CreateTextureFromSurface(ren, ts);
        SDL_Rect tr = {SCREEN_WIDTH/2 - ts->w/2, 60, ts->w, ts->h};
        SDL_RenderCopy(ren, tt, NULL, &tr);
        SDL_DestroyTexture(tt); SDL_FreeSurface(ts);
    }

    SDL_Color white = {200, 255, 200, 255};
    char vbuf[64];
    snprintf(vbuf, sizeof(vbuf), "MUSIQUE :  %d %%", musicVol);
    SDL_Surface *vs = TTF_RenderText_Blended(font, vbuf, white);
    if (vs) {
        SDL_Texture *vt = SDL_CreateTextureFromSurface(ren, vs);
        SDL_Rect vr = {SCREEN_WIDTH/2 - vs->w/2, 200, vs->w, vs->h};
        SDL_RenderCopy(ren, vt, NULL, &vr);
        SDL_DestroyTexture(vt); SDL_FreeSurface(vs);
    }

    int barW = 400, barH = 20;
    int barX = SCREEN_WIDTH/2 - barW/2, barY = 250;
    SDL_SetRenderDrawColor(ren, 40, 40, 40, 255);
    SDL_Rect barBg = {barX, barY, barW, barH};
    SDL_RenderFillRect(ren, &barBg);
    SDL_SetRenderDrawColor(ren, 57, 255, 20, 255);
    int fillW2 = barW * musicVol / 100;
    SDL_Rect barFill = {barX, barY, fillW2, barH};
    SDL_RenderFillRect(ren, &barFill);
    SDL_SetRenderDrawColor(ren, 0, 200, 60, 200);
    SDL_RenderDrawRect(ren, &barBg);

    SDL_Surface *hs = TTF_RenderText_Blended(font,
        "FLECHE GAUCHE / DROITE  pour regler le volume", white);
    if (hs) {
        SDL_Texture *ht = SDL_CreateTextureFromSurface(ren, hs);
        SDL_Rect hr = {SCREEN_WIDTH/2 - hs->w/2, 290, hs->w, hs->h};
        SDL_RenderCopy(ren, ht, NULL, &hr);
        SDL_DestroyTexture(ht); SDL_FreeSurface(hs);
    }

    const char *ctls[] = {
        "J1 : A/D deplacement  W ou ESPACE saut  LSHIFT sprint  LCTRL tir",
        "J2 : GAUCHE/DROITE    ENTREE saut        RSHIFT sprint  RCTRL tir",
        "SOURIS — J1 : clic gauche  |  J2 : clic droit (multi seulement)",
        "QCM   — F1 / F2 / F3 / F4  ou clic sur le bouton",
        "PUZZLE— Cliquez deux tuiles pour les echanger  |  ESC = abandonner",
        "PAUSE — Touche P  |  F11 = Niveau 1  |  F12 = Niveau 2",
    };
    int cy = 350;
    for (int i = 0; i < 6; i++) {
        SDL_Surface *cs = TTF_RenderText_Blended(font, ctls[i], white);
        if (!cs) { cy += 36; continue; }
        SDL_Texture *ct = SDL_CreateTextureFromSurface(ren, cs);
        SDL_Rect cr = {SCREEN_WIDTH/2 - cs->w/2, cy, cs->w, cs->h};
        SDL_RenderCopy(ren, ct, NULL, &cr);
        SDL_DestroyTexture(ct); SDL_FreeSurface(cs);
        cy += 36;
    }

    (void)au;
    backBtn->hover = tbtnHit(backBtn, mx, my);
    tbtnDraw(ren, backBtn);
}

/* ============================================================
 * HISTOIRE SCREEN
 * ============================================================ */
static void renderHistoire(SDL_Renderer *ren, TTF_Font *font,
                            TBtn *backBtn, int mx, int my)
{
    SDL_SetRenderDrawColor(ren, 0, 10, 0, 255);
    SDL_Rect full = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
    SDL_RenderFillRect(ren, &full);

    SDL_Color green = {57, 255, 20, 255};
    SDL_Surface *ts = TTF_RenderText_Blended(font, "HISTOIRE", green);
    if (ts) {
        SDL_Texture *tt = SDL_CreateTextureFromSurface(ren, ts);
        SDL_Rect tr = {SCREEN_WIDTH/2 - ts->w/2, 50, ts->w, ts->h};
        SDL_RenderCopy(ren, tt, NULL, &tr);
        SDL_DestroyTexture(tt); SDL_FreeSurface(ts);
    }

    SDL_Color white = {200, 255, 200, 255};
    const char *story[] = {
        "Dans un monde ou la realite et le code se confondent,",
        "NEO, un hacker legendaire, decouvre que la ville entiere",
        "est sous le controle d une intelligence artificielle malveillante.",
        "",
        "Les Agents — programmes corrompus — patrouillent les rues numeriques,",
        "eliminant quiconque tente de briser le code.",
        "",
        "Arme de son agilite et de son pistolet a balles de donnees,",
        "NEO doit traverser deux niveaux de simulation pour atteindre",
        "le BOSS et liberer la matrice.",
        "",
        "Seul ou avec TRINITY a ses cotes, la survie dependra",
        "de votre vitesse, precision et strategie.",
    };
    int cy = 140;
    for (int i = 0; i < (int)(sizeof(story)/sizeof(story[0])); i++) {
        if (story[i][0] == '\0') { cy += 18; continue; }
        SDL_Surface *ss = TTF_RenderText_Blended(font, story[i], white);
        if (!ss) { cy += 34; continue; }
        SDL_Texture *st = SDL_CreateTextureFromSurface(ren, ss);
        SDL_Rect sr = {SCREEN_WIDTH/2 - ss->w/2, cy, ss->w, ss->h};
        SDL_RenderCopy(ren, st, NULL, &sr);
        SDL_DestroyTexture(st); SDL_FreeSurface(ss);
        cy += 34;
    }

    backBtn->hover = tbtnHit(backBtn, mx, my);
    tbtnDraw(ren, backBtn);
}

/* ============================================================
 * SCORES SCREEN
 * ============================================================ */
static void renderScoresScreen(SDL_Renderer *ren, Background *bg,
                                 TBtn *backBtn, int mx, int my,
                                 int showHint)
{
    SDL_SetRenderDrawColor(ren, 0, 6, 3, 255);
    SDL_Rect full = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
    SDL_RenderFillRect(ren, &full);
    drawTextCentered(ren, 40, 0, SCREEN_WIDTH, "MEILLEURS SCORES", 3, 0, 255, 70);
    drawText(ren, 60, 110, "# NOM                SCORE NIV TEMPS", 1, 220, 180, 0);
    SDL_SetRenderDrawColor(ren, 0, 200, 50, 255);
    SDL_RenderDrawLine(ren, 60, 126, SCREEN_WIDTH - 60, 126);

    if (bg->scoreCount == 0)
        drawTextCentered(ren, 200, 0, SCREEN_WIDTH,
                         "AUCUN SCORE ENREGISTRE", 2, 0, 140, 35);

    for (int s = 0; s < bg->scoreCount && s < MAX_SCORES; s++) {
        Score *sc = &bg->scores[s];
        char sline[80];
        snprintf(sline, sizeof(sline), "%-3d %-20s %5d  %2d %02d:%02d",
                 s+1, sc->name, sc->score, sc->level,
                 sc->time / 60, sc->time % 60);
        Uint8 sr2 = (Uint8)((s == 0) ? 220 : 180);
        Uint8 sg2 = (Uint8)((s == 0) ? 180 : 255);
        Uint8 sb2 = (Uint8)((s == 0) ?   0 : 180);
        drawText(ren, 60, 140 + s*22, sline, 1, sr2, sg2, sb2);
    }

    if (showHint)
        drawTextCentered(ren, SCREEN_HEIGHT - 80, 0, SCREEN_WIDTH,
                         "APPUYEZ SUR UNE TOUCHE OU RETOUR", 1, 0, 140, 35);

    backBtn->hover = tbtnHit(backBtn, mx, my);
    tbtnDraw(ren, backBtn);
}

/* ============================================================
 * NAME ENTRY
 * ============================================================ */
typedef struct {
    char name[MAX_NAME_LEN];
    int  nameLen;
    int  inputActive;
    int  bestScore;
    int  bestLevel;
} NameEntryState;

static void nameEntryInit(NameEntryState *ns, Background *bg)
{
    memset(ns, 0, sizeof(NameEntryState));
    ns->inputActive = 0;
    ns->bestScore   = -1;
    ns->bestLevel   = 0;
    (void)bg;
}

static void nameEntryLookup(NameEntryState *ns, Background *bg)
{
    ns->bestScore = -1; ns->bestLevel = 0;
    for (int i = 0; i < bg->scoreCount; i++) {
        if (SDL_strcasecmp(bg->scores[i].name, ns->name) == 0) {
            if (bg->scores[i].score > ns->bestScore) {
                ns->bestScore = bg->scores[i].score;
                ns->bestLevel = bg->scores[i].level;
            }
        }
    }
}

static int nameEntryUpdate(NameEntryState *ns, SDL_Renderer *ren,
                            TTF_Font *font, TTF_Font *fontSmall,
                            Background *bg, int mx, int my,
                            int *outQuit, int *outEsc)
{
    if (!ns->inputActive) {
        SDL_StopTextInput();
        SDL_Delay(50);
        while (SDL_PollEvent(NULL)) {}
        SDL_StartTextInput();
        ns->inputActive = 1;
    }

    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        if (ev.type == SDL_QUIT) {
            if (outQuit) *outQuit = 1;
            return 0;
        }
        if (ev.type == SDL_KEYDOWN &&
            ev.key.keysym.sym == SDLK_ESCAPE) {
            if (outEsc) *outEsc = 1;
            return 0;
        }
        if (ev.type == SDL_TEXTINPUT && ns->nameLen < MAX_NAME_LEN - 1) {
            for (int c = 0; ev.text.text[c] && ns->nameLen < MAX_NAME_LEN - 1; c++) {
                char ch = ev.text.text[c];
                if (ch >= 32 && ch < 127)
                    ns->name[ns->nameLen++] = ch;
            }
            ns->name[ns->nameLen] = '\0';
            nameEntryLookup(ns, bg);
        }
        if (ev.type == SDL_KEYDOWN) {
            SDL_Keycode k = ev.key.keysym.sym;
            if (k == SDLK_BACKSPACE && ns->nameLen > 0) {
                ns->name[--ns->nameLen] = '\0';
                nameEntryLookup(ns, bg);
            }
            if (k == SDLK_RETURN && ns->nameLen > 0)
                return 1;
        }
        if (ev.type == SDL_MOUSEBUTTONDOWN &&
            ev.button.button == SDL_BUTTON_LEFT) {
            mx = ev.button.x;
            my = ev.button.y;
        }
    }

    SDL_SetRenderDrawColor(ren, 0, 8, 3, 255);
    SDL_Rect full = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
    SDL_RenderFillRect(ren, &full);

    SDL_Color green  = {57, 255, 20, 255};
    SDL_Color white  = {200, 255, 200, 255};
    SDL_Color yellow = {255, 230, 0, 255};

    SDL_Surface *ts = TTF_RenderText_Blended(font, "MATRIX GAME", green);
    if (ts) {
        SDL_Texture *tt = SDL_CreateTextureFromSurface(ren, ts);
        SDL_Rect tr = {SCREEN_WIDTH/2 - ts->w/2, 60, ts->w, ts->h};
        SDL_RenderCopy(ren, tt, NULL, &tr);
        SDL_DestroyTexture(tt); SDL_FreeSurface(ts);
    }

    const char *charLabel = (selectedCharacter == 0)
        ? "Personnage : NEO" : "Personnage : MORPHEUS";
    SDL_Color charCol = (selectedCharacter == 0)
        ? (SDL_Color){57, 255, 20, 255} : (SDL_Color){20, 200, 255, 255};
    SDL_Surface *cls = TTF_RenderText_Blended(fontSmall, charLabel, charCol);
    if (cls) {
        SDL_Texture *clt = SDL_CreateTextureFromSurface(ren, cls);
        SDL_Rect clr = {SCREEN_WIDTH/2 - cls->w/2, 118, cls->w, cls->h};
        SDL_RenderCopy(ren, clt, NULL, &clr);
        SDL_DestroyTexture(clt); SDL_FreeSurface(cls);
    }

    SDL_Surface *sub = TTF_RenderText_Blended(fontSmall,
        "Entrez votre nom de joueur :", white);
    if (sub) {
        SDL_Texture *st2 = SDL_CreateTextureFromSurface(ren, sub);
        SDL_Rect sr = {SCREEN_WIDTH/2 - sub->w/2, 160, sub->w, sub->h};
        SDL_RenderCopy(ren, st2, NULL, &sr);
        SDL_DestroyTexture(st2); SDL_FreeSurface(sub);
    }

    SDL_Rect nameBox = {SCREEN_WIDTH/2 - 220, 205, 440, 52};
    SDL_SetRenderDrawColor(ren, 0, 60, 0, 255);
    SDL_RenderFillRect(ren, &nameBox);
    SDL_SetRenderDrawColor(ren, 0, 220, 60, 255);
    SDL_RenderDrawRect(ren, &nameBox);

    char display[MAX_NAME_LEN + 2];
    if ((SDL_GetTicks() / 400) % 2 == 0)
        snprintf(display, sizeof(display), "%s_", ns->name);
    else
        snprintf(display, sizeof(display), "%s",  ns->name);

    SDL_Surface *ns2 = TTF_RenderText_Blended(font,
        display[0] ? display : " ", white);
    if (ns2) {
        SDL_Texture *nt = SDL_CreateTextureFromSurface(ren, ns2);
        SDL_Rect nr = {SCREEN_WIDTH/2 - ns2->w/2, 216, ns2->w, ns2->h};
        SDL_RenderCopy(ren, nt, NULL, &nr);
        SDL_DestroyTexture(nt); SDL_FreeSurface(ns2);
    }

    if (ns->nameLen > 0 && ns->bestScore >= 0) {
        char buf[80];
        snprintf(buf, sizeof(buf), "Meilleur score : %d  (Niveau %d)",
                 ns->bestScore, ns->bestLevel);
        SDL_Surface *bs = TTF_RenderText_Blended(fontSmall, buf, yellow);
        if (bs) {
            SDL_Texture *bt = SDL_CreateTextureFromSurface(ren, bs);
            SDL_Rect bsr = {SCREEN_WIDTH/2 - bs->w/2, 275, bs->w, bs->h};
            SDL_RenderCopy(ren, bt, NULL, &bsr);
            SDL_DestroyTexture(bt); SDL_FreeSurface(bs);
        }
    } else if (ns->nameLen > 0) {
        SDL_Surface *ns3 = TTF_RenderText_Blended(fontSmall,
            "Nouveau joueur detecte", (SDL_Color){0, 180, 60, 255});
        if (ns3) {
            SDL_Texture *nt = SDL_CreateTextureFromSurface(ren, ns3);
            SDL_Rect nr = {SCREEN_WIDTH/2 - ns3->w/2, 275, ns3->w, ns3->h};
            SDL_RenderCopy(ren, nt, NULL, &nr);
            SDL_DestroyTexture(nt); SDL_FreeSurface(ns3);
        }
    }

    int result = 0;
    if (ns->nameLen > 0) {
        int bw = 280, bh = 60, gap = 40;
        int bx1 = SCREEN_WIDTH/2 - bw - gap/2;
        int bx2 = SCREEN_WIDTH/2 + gap/2;
        int by  = 340;

        SDL_Rect btnNew  = {bx1, by, bw, bh};
        SDL_Rect btnCont = {bx2, by, bw, bh};
        int hovNew  = (mx >= bx1 && mx <= bx1+bw && my >= by && my <= by+bh);
        int hovCont = (mx >= bx2 && mx <= bx2+bw && my >= by && my <= by+bh);

        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(ren,
            hovNew ? 57 : 0, hovNew ? 255 : 0, hovNew ? 20 : 0, hovNew ? 120 : 180);
        SDL_RenderFillRect(ren, &btnNew);
        SDL_SetRenderDrawColor(ren,
            hovCont ? 57 : 0, hovCont ? 255 : 0, hovCont ? 20 : 0, hovCont ? 120 : 180);
        SDL_RenderFillRect(ren, &btnCont);
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
        SDL_SetRenderDrawColor(ren, 57, 255, 20, 255);
        SDL_RenderDrawRect(ren, &btnNew);
        SDL_RenderDrawRect(ren, &btnCont);

        SDL_Surface *s1 = TTF_RenderText_Blended(fontSmall, "NOUVELLE PARTIE", white);
        SDL_Surface *s2 = TTF_RenderText_Blended(fontSmall,
            ns->bestScore >= 0 ? "CONTINUER" : "JOUER", white);
        if (s1) {
            SDL_Texture *t1 = SDL_CreateTextureFromSurface(ren, s1);
            SDL_Rect r1 = {bx1 + (bw-s1->w)/2, by + (bh-s1->h)/2, s1->w, s1->h};
            SDL_RenderCopy(ren, t1, NULL, &r1);
            SDL_DestroyTexture(t1); SDL_FreeSurface(s1);
        }
        if (s2) {
            SDL_Texture *t2 = SDL_CreateTextureFromSurface(ren, s2);
            SDL_Rect r2 = {bx2 + (bw-s2->w)/2, by + (bh-s2->h)/2, s2->w, s2->h};
            SDL_RenderCopy(ren, t2, NULL, &r2);
            SDL_DestroyTexture(t2); SDL_FreeSurface(s2);
        }

        SDL_Surface *h = TTF_RenderText_Blended(fontSmall,
            "ENTREE = Nouvelle Partie", (SDL_Color){0, 140, 35, 255});
        if (h) {
            SDL_Texture *ht = SDL_CreateTextureFromSurface(ren, h);
            SDL_Rect hr = {SCREEN_WIDTH/2 - h->w/2, 420, h->w, h->h};
            SDL_RenderCopy(ren, ht, NULL, &hr);
            SDL_DestroyTexture(ht); SDL_FreeSurface(h);
        }

        if (hovNew)  result = 1;
        if (hovCont) result = 2;

    } else {
        SDL_Surface *hint = TTF_RenderText_Blended(fontSmall,
            "Tapez votre nom puis choisissez une option",
            (SDL_Color){0, 140, 35, 255});
        if (hint) {
            SDL_Texture *ht = SDL_CreateTextureFromSurface(ren, hint);
            SDL_Rect hr = {SCREEN_WIDTH/2 - hint->w/2, 340, hint->w, hint->h};
            SDL_RenderCopy(ren, ht, NULL, &hr);
            SDL_DestroyTexture(ht); SDL_FreeSurface(hint);
        }
    }

    return result;
}

/* ============================================================
 * SPAWN HELPER
 * ============================================================ */
static int pickSpawnX(Player *p1, Player *p2, DisplayMode mode, int enemyW)
{
    int ref = (int)p1->worldX;
    if (mode == MODE_MULTI && p2) {
        ref = ((int)p1->worldX + (int)p2->worldX) / 2;
    }

    static int side = 0;
    side ^= 1;

    int spawnX;
    if (side == 0) {
        spawnX = ref + SCREEN_WIDTH / 2 + 100;
    } else {
        spawnX = ref - SCREEN_WIDTH / 2 - 100 - enemyW;
    }

    if (spawnX < 0) spawnX = 0;
    if (spawnX > WORLD_WIDTH - enemyW) spawnX = WORLD_WIDTH - enemyW;
    return spawnX;
}

/* ============================================================
 * MAIN
 * ============================================================ */
int main(int argc, char *argv[]) {
    (void)argc; (void)argv;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) return 1;
    IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
    TTF_Init();

    SDL_Window   *win = SDL_CreateWindow(WINDOW_TITLE,
                            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                            SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
    SDL_Renderer *ren = SDL_CreateRenderer(win, -1,
                            SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);

    /* ── Fonts ── */
    TTF_Font *fontMain   = TTF_OpenFont("assets/font/arial.ttf", 28);
    TTF_Font *fontMatrix = TTF_OpenFont("assets/font/arial.ttf", 16);
    TTF_Font *fontSmall  = TTF_OpenFont("assets/font/arial.ttf", 20);
    if (!fontMain || !fontMatrix) {
        printf("ERREUR : assets/font/arial.ttf manquant\n");
        return 1;
    }
    if (!fontSmall) fontSmall = fontMain;

    /* ── Title texture ── */
    SDL_Texture *titleTex = IMG_LoadTexture(ren, "assets/title.png");
    SDL_Rect titleRect    = {340, 20, 600, 200};

    /* ── Matrix rain ── */
    matrix_init(ren, SCREEN_WIDTH, SCREEN_HEIGHT, fontMatrix);

    /* ── Audio ── */
    GameAudio *au = init_audio();

    /* ── Main menu buttons ── */
    const char *menuLabels[BTN_COUNT] = {
        "JOUER", "OPTION", "MEILLEUR SCORE", "HISTOIRE", "QUITTER"
    };
    TBtn menuBtns[BTN_COUNT];
    for (int i = 0; i < BTN_COUNT; i++)
        tbtnBuild(&menuBtns[i], ren, fontMain, menuLabels[i],
                  100, 240 + i * 85, 320, 65);

    /* ── Mode-select buttons ── */
    int bw = 220, bh = 54;
    int cy_ms = SCREEN_HEIGHT / 2 - 20;
    BBtn btnMono    = {{SCREEN_WIDTH/2 - bw - 30, cy_ms,       bw, bh}, "MONO JOUEUR",   0};
    BBtn btnMulti   = {{SCREEN_WIDTH/2 + 30,       cy_ms,       bw, bh}, "MULTI JOUEURS", 0};
    BBtn btnValider = {{SCREEN_WIDTH/2 - bw/2,     cy_ms + 80,  bw, bh}, "VALIDER",       0};
    BBtn btnRetour  = {{SCREEN_WIDTH - 220, SCREEN_HEIGHT - 70, 180, 44}, "RETOUR",        0};

    /* ── Back button (sub-screens) ── */
    TBtn backBtn;
    tbtnBuild(&backBtn, ren, fontSmall, "< RETOUR",
              40, SCREEN_HEIGHT - 80, 200, 50);

    /* ── Game objects ── */
    Background  bg;
    GameLevel   currentLevel = LEVEL_1;
    DisplayMode dispMode     = MODE_MONO;
    memset(&bg, 0, sizeof(Background));
    initBackground(&bg, ren, LEVEL_1, MODE_MONO);

    float startY = (float)(508 - PLAYER_PH);
    Player p1, p2;
    memset(&p1, 0, sizeof(Player));
    memset(&p2, 0, sizeof(Player));
    initialiserJoueur(&p1, ren, PLAYER_1, 150.0f, startY);
    initialiserJoueur(&p2, ren, PLAYER_2, 250.0f, startY);

    Enemy minions[MAX_MINIONS];
    memset(minions, 0, sizeof(minions));
    Enemy boss;
    memset(&boss, 0, sizeof(Enemy));
    int bossSpawned = 0, bossActive = 0;
    int spawnTimer  = 90;
    int minionKills = 0;

    resetEnemies(minions, MAX_MINIONS, &boss,
                 &bossSpawned, &bossActive, &spawnTimer,
                 &minionKills, ren, currentLevel);

    Minimap mm;
    initMinimap(&mm, ren, &p1, &p2);

    /* ── Enigme session ── */
    EnigmeSession enigme;
    enigmeInit(&enigme, ren, fontMain, fontSmall);
    Player *enigmePlayer = NULL;

    SDL_Event enigmeEv;
    memset(&enigmeEv, 0, sizeof(enigmeEv));
    int hasEnigmeEv = 0;

    int p1WasAlive = 1, p2WasAlive = 1;

    /* ── Name entry ── */
    NameEntryState nameState;
    nameEntryInit(&nameState, &bg);

    /* ── App state ── */
    AppState state    = APP_MAIN_MENU;
    int musicVol      = 50;
    int last_hover    = -1;
    int running       = 1;

    Uint32 last_tick  = SDL_GetTicks();
    SDL_Event ev;

    /* ── Main loop ── */
    while (running) {
        Uint32 fs = SDL_GetTicks();
        float  dt = (fs - last_tick) / 1000.0f;
        if (dt > 0.05f) dt = 0.05f;
        last_tick = fs;

        int mx, my;
        SDL_GetMouseState(&mx, &my);

        SDL_Event noEv; memset(&noEv, 0, sizeof(noEv));
        hasEnigmeEv  = 0;

        if (state != APP_NAME_ENTRY) {
            while (SDL_PollEvent(&ev)) {
                if (ev.type == SDL_QUIT) { running = 0; break; }

                /* Global ESC */
                if (ev.type == SDL_KEYDOWN && ev.key.keysym.sym == SDLK_ESCAPE) {
                    if (state == APP_GAME) {
                        /* ── [ESC-SAVE-1] Save score before leaving ── */
                        SDL_StopTextInput();

                        /* Use the name already typed; if empty prompt for one */
                        char saveName[MAX_NAME_LEN] = {0};
                        strncpy(saveName, nameState.name, MAX_NAME_LEN - 1);
                        if (saveName[0] == '\0')
                            saisirNomJoueur(ren, saveName);

                        /* Save P1 score */
                        sauvegarderScore(&bg, saveName, p1.score, currentLevel);

                        /* In multiplayer also save P2 */
                        if (dispMode == MODE_MULTI) {
                            char saveName2[MAX_NAME_LEN] = {0};
                            saisirNomJoueur(ren, saveName2);
                            sauvegarderScore(&bg, saveName2, p2.score, currentLevel);
                        }

                        /* Show leaderboard so the player sees their rank */
                        afficherMeilleursScores(&bg, ren);

                        state = APP_MAIN_MENU;
                        if (au && au->music) Mix_PlayMusic(au->music, -1);
                    }
                    else if (state == APP_MODE_SELECT)      state = APP_MAIN_MENU;
                    else if (state == APP_CHARACTER_SELECT) state = APP_MODE_SELECT;
                    else if (state == APP_OPTION)           state = APP_MAIN_MENU;
                    else if (state == APP_SCORES)           state = APP_MAIN_MENU;
                    else if (state == APP_HISTOIRE)         state = APP_MAIN_MENU;
                    else if (state == APP_SAVE_SCREEN)      state = APP_MAIN_MENU;
                    else if (state == APP_ENIGME_CHOICE ||
                             state == APP_ENIGME_QCM    ||
                             state == APP_ENIGME_PUZZLE) {
                        enigme.result = ENIGME_LOSE;
                        enigme.flashFrames = 0;
                        state = APP_ENIGME_RESULT;
                    }
                    else running = 0;
                    break;
                }

                /* MAIN MENU */
                if (state == APP_MAIN_MENU) {
                    if (ev.type == SDL_MOUSEBUTTONDOWN &&
                        ev.button.button == SDL_BUTTON_LEFT) {
                        for (int i = 0; i < BTN_COUNT; i++) {
                            if (!tbtnHit(&menuBtns[i], mx, my)) continue;
                            switch (i) {
                                case 0: state = APP_MODE_SELECT; break;
                                case 1: state = APP_OPTION;      break;
                                case 2: chargerScores(&bg); state = APP_SCORES; break;
                                case 3: state = APP_HISTOIRE;    break;
                                case 4: running = 0;             break;
                            }
                            break;
                        }
                    }
                }

                /* MODE SELECT */
                else if (state == APP_MODE_SELECT) {
                    if (ev.type == SDL_MOUSEBUTTONDOWN &&
                        ev.button.button == SDL_BUTTON_LEFT) {
                        if (bbtnMouseOn(&btnMono,    mx, my)) dispMode = MODE_MONO;
                        if (bbtnMouseOn(&btnMulti,   mx, my)) dispMode = MODE_MULTI;
                        if (bbtnMouseOn(&btnRetour,  mx, my)) state = APP_MAIN_MENU;
                        if (bbtnMouseOn(&btnValider, mx, my)) {
                            chargerScores(&bg);
                            nameEntryInit(&nameState, &bg);
                            if (dispMode == MODE_MONO)
                                state = APP_CHARACTER_SELECT;
                            else
                                state = APP_NAME_ENTRY;
                        }
                    }
                }

                /* OPTION */
                else if (state == APP_OPTION) {
                    if (ev.type == SDL_KEYDOWN) {
                        if (ev.key.keysym.sym == SDLK_LEFT)  musicVol -= 5;
                        if (ev.key.keysym.sym == SDLK_RIGHT) musicVol += 5;
                        if (musicVol < 0)   musicVol = 0;
                        if (musicVol > 100) musicVol = 100;
                        Mix_VolumeMusic(musicVol * MIX_MAX_VOLUME / 100);
                    }
                    if (ev.type == SDL_MOUSEBUTTONDOWN &&
                        ev.button.button == SDL_BUTTON_LEFT)
                        if (tbtnHit(&backBtn, mx, my)) state = APP_MAIN_MENU;
                }

                /* SCORES */
                else if (state == APP_SCORES) {
                    if (ev.type == SDL_MOUSEBUTTONDOWN &&
                        ev.button.button == SDL_BUTTON_LEFT)
                        if (tbtnHit(&backBtn, mx, my)) state = APP_MAIN_MENU;
                    if (ev.type == SDL_KEYDOWN) state = APP_MAIN_MENU;
                }

                /* HISTOIRE */
                else if (state == APP_HISTOIRE) {
                    if (ev.type == SDL_MOUSEBUTTONDOWN &&
                        ev.button.button == SDL_BUTTON_LEFT)
                        if (tbtnHit(&backBtn, mx, my)) state = APP_MAIN_MENU;
                    if (ev.type == SDL_KEYDOWN) state = APP_MAIN_MENU;
                }

                /* SAVE SCREEN */
                else if (state == APP_SAVE_SCREEN) {
                    if (ev.type == SDL_MOUSEBUTTONDOWN &&
                        ev.button.button == SDL_BUTTON_LEFT)
                        if (tbtnHit(&backBtn, mx, my)) state = APP_MAIN_MENU;
                    if (ev.type == SDL_KEYDOWN) state = APP_MAIN_MENU;
                }

                /* GAME */
                else if (state == APP_GAME) {
                    gererEvenementJoueur(&p1, &ev);
                    if (dispMode == MODE_MULTI) gererEvenementJoueur(&p2, &ev);

                    if (ev.type == SDL_KEYDOWN) {
                        SDL_Keycode k = ev.key.keysym.sym;
                        if (k == SDLK_F10) afficherMeilleursScores(&bg, ren);
                        if (k == SDLK_h)   bg.guide.state = GUIDE_VISIBLE;
                        if (k == SDLK_p)   togglePause(&bg);
                        if (k == SDLK_F11) {
                            currentLevel = LEVEL_1;
                            freeBackground(&bg);
                            initBackground(&bg, ren, currentLevel, dispMode);
                            resetEnemies(minions, MAX_MINIONS, &boss,
                                         &bossSpawned, &bossActive,
                                         &spawnTimer, &minionKills, ren, currentLevel);
                            minimapSetLevel(&mm, 1);
                        }
                        if (k == SDLK_F12) {
                            currentLevel = LEVEL_2;
                            freeBackground(&bg);
                            initBackground(&bg, ren, currentLevel, dispMode);
                            resetEnemies(minions, MAX_MINIONS, &boss,
                                         &bossSpawned, &bossActive,
                                         &spawnTimer, &minionKills, ren, currentLevel);
                            minimapSetLevel(&mm, 2);
                        }
                    }
                }

                else if (state == APP_CHARACTER_SELECT) {
                    enigmeEv    = ev;
                    hasEnigmeEv = 1;
                }

                else if (state == APP_ENIGME_CHOICE ||
                         state == APP_ENIGME_QCM    ||
                         state == APP_ENIGME_PUZZLE) {
                    enigmeEv  = ev;
                    hasEnigmeEv = 1;
                }
            } /* end event poll */
        } /* end if (state != APP_NAME_ENTRY) */

        /* ── UPDATE ── */
        if (state == APP_MAIN_MENU) {
            matrix_update(dt, SCREEN_HEIGHT);
            for (int i = 0; i < BTN_COUNT; i++) {
                menuBtns[i].hover = tbtnHit(&menuBtns[i], mx, my);
                if (menuBtns[i].hover && last_hover != i) {
                    if (au && au->hover_sound)
                        Mix_PlayChannel(-1, au->hover_sound, 0);
                    last_hover = i;
                }
                if (!menuBtns[i].hover && last_hover == i)
                    last_hover = -1;
            }
        }

        if (state == APP_MODE_SELECT) {
            matrix_update(dt, SCREEN_HEIGHT);
            btnMono.hover    = bbtnMouseOn(&btnMono,    mx, my);
            btnMulti.hover   = bbtnMouseOn(&btnMulti,   mx, my);
            btnValider.hover = bbtnMouseOn(&btnValider, mx, my);
            btnRetour.hover  = bbtnMouseOn(&btnRetour,  mx, my);
        }

        if (state == APP_CHARACTER_SELECT)
            matrix_update(dt, SCREEN_HEIGHT);

        if (state == APP_ENIGME_CHOICE ||
            state == APP_ENIGME_QCM    ||
            state == APP_ENIGME_PUZZLE)
            matrix_update(dt, SCREEN_HEIGHT);

        if (state == APP_GAME && !bg.paused) {
            mettreAJourJoueur(&p1, bg.platforms, bg.platformCount);
            if (dispMode == MODE_MULTI)
                mettreAJourJoueur(&p2, bg.platforms, bg.platformCount);

            if (dispMode == MODE_MONO) {
                bg.camX = p1.camX;
                bg.camY = 0;
            }

            updateBackground(&bg);

            if (!bossSpawned && minionKills < MINION_KILL_THRESHOLD) {
                spawnTimer--;
                if (spawnTimer <= 0) {
                    for (int i = 0; i < MAX_MINIONS; i++) {
                        if (!minions[i].alive) {
                            int spawnX = pickSpawnX(&p1,
                                dispMode == MODE_MULTI ? &p2 : NULL,
                                dispMode, PLAYER_W);
                            destroy_enemy(&minions[i]);
                            memset(&minions[i], 0, sizeof(Enemy));
                            init_enemy(&minions[i], 0, ren, spawnX);
                            setEnemyY(&minions[i], currentLevel);
                            spawnTimer = 150;
                            break;
                        }
                    }
                }
            }

            for (int i = 0; i < MAX_MINIONS; i++) {
                if (!minions[i].alive) continue;

                int wasAlive = minions[i].alive;
                int ref_x    = (int)p1.worldX;
                if (dispMode == MODE_MULTI) {
                    int d1 = abs((int)p1.worldX - minions[i].x);
                    int d2 = abs((int)p2.worldX - minions[i].x);
                    ref_x  = (d2 < d1) ? (int)p2.worldX : (int)p1.worldX;
                }
                update_enemy(&minions[i], ref_x);

                if (wasAlive && !minions[i].alive) {
                    minionKills++;
                    fprintf(stderr, "[KILL] Minion %d killed. Total: %d/%d\n",
                            i, minionKills, MINION_KILL_THRESHOLD);
                }

                if (p1.isAlive) checkPlayerVsEnemy(&p1, &minions[i], &mm);
                if (dispMode == MODE_MULTI && p2.isAlive)
                    checkPlayerVsEnemy(&p2, &minions[i], &mm);
            }

            if (!bossSpawned && minionKills >= MINION_KILL_THRESHOLD) {
                int allDead = 1;
                for (int i = 0; i < MAX_MINIONS; i++)
                    if (minions[i].alive) { allDead = 0; break; }
                if (allDead) {
                    int spawnX = pickSpawnX(&p1,
                        dispMode == MODE_MULTI ? &p2 : NULL,
                        dispMode, PLAYER_W * 2);
                    destroy_enemy(&boss);
                    memset(&boss, 0, sizeof(Enemy));
                    init_enemy(&boss, 1, ren, spawnX);
                    setEnemyY(&boss, currentLevel);
                    bossSpawned = 1;
                    bossActive  = 1;
                    setNotification(&bg, "BOSS FINAL !", 3000);
                    fprintf(stderr, "[BOSS] FINAL BOSS SPAWNED at x=%d!\n", spawnX);
                }
            }

            if (bossActive && boss.alive) {
                int ref_x = (int)p1.worldX;
                if (dispMode == MODE_MULTI) {
                    int d1 = abs((int)p1.worldX - boss.x);
                    int d2 = abs((int)p2.worldX - boss.x);
                    ref_x  = (d2 < d1) ? (int)p2.worldX : (int)p1.worldX;
                }
                update_enemy(&boss, ref_x);

                if (!boss.alive) {
                    bossActive = 0;
                    setNotification(&bg, "BOSS VAINCU ! VICTOIRE !", 4000);
                    p1.score += 500;
                    if (dispMode == MODE_MULTI) p2.score += 500;
                }

                if (p1.isAlive) checkPlayerVsEnemy(&p1, &boss, &mm);
                if (dispMode == MODE_MULTI && p2.isAlive)
                    checkPlayerVsEnemy(&p2, &boss, &mm);
            }

            checkBulletsVsEnemies(&p1, minions, MAX_MINIONS,
                                  bossActive ? &boss : NULL, bossActive);
            if (dispMode == MODE_MULTI)
                checkBulletsVsEnemies(&p2, minions, MAX_MINIONS,
                                      bossActive ? &boss : NULL, bossActive);

            if (p1.damageEvent) {
                printf(p1.isAlive ? "HIT\n" : "DEAD\n");
                fflush(stdout);
            }
            if (dispMode == MODE_MULTI && p2.damageEvent) {
                printf(p2.isAlive ? "HIT\n" : "DEAD\n");
                fflush(stdout);
            }

            int p1Alive = p1.isAlive;
            int p2Alive = p2.isAlive;

            if (p1WasAlive && !p1Alive && enigmePlayer == NULL) {
                enigmePlayer        = &p1;
                enigme.result       = ENIGME_PENDING;
                enigme.flashFrames  = 0;
                enigme.selectedTile = -1;
                hasEnigmeEv         = 0;
                SDL_Delay(200);
                while (SDL_PollEvent(&ev)) {}
                state = APP_ENIGME_CHOICE;
            } else if (dispMode == MODE_MULTI &&
                       p2WasAlive && !p2Alive &&
                       enigmePlayer == NULL) {
                enigmePlayer        = &p2;
                enigme.result       = ENIGME_PENDING;
                enigme.flashFrames  = 0;
                enigme.selectedTile = -1;
                hasEnigmeEv         = 0;
                SDL_Delay(200);
                while (SDL_PollEvent(&ev)) {}
                state = APP_ENIGME_CHOICE;
            }

            p1WasAlive = p1Alive;
            p2WasAlive = p2Alive;

        } /* end APP_GAME update */

        /* ── RENDER ── */
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
        SDL_RenderClear(ren);

        /* ---- MAIN MENU ---- */
        if (state == APP_MAIN_MENU) {
            matrix_render(ren, SCREEN_WIDTH);
            if (titleTex) SDL_RenderCopy(ren, titleTex, NULL, &titleRect);
            for (int i = 0; i < BTN_COUNT; i++) tbtnDraw(ren, &menuBtns[i]);
        }

        /* ---- MODE SELECT ---- */
        else if (state == APP_MODE_SELECT) {
            matrix_render(ren, SCREEN_WIDTH);
            SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(ren, 0, 0, 0, 140);
            SDL_Rect full = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
            SDL_RenderFillRect(ren, &full);
            SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
            drawTextCentered(ren, 140, 0, SCREEN_WIDTH, "MODE DE JEU", 3, 0, 255, 80);
            const char *modeLbl = (dispMode == MODE_MONO)
                                  ? "Selectionne : MONO" : "Selectionne : MULTI";
            drawTextCentered(ren, 200, 0, SCREEN_WIDTH, modeLbl, 2, 0, 200, 60);
            bbtnDraw(ren, &btnMono);
            bbtnDraw(ren, &btnMulti);
            bbtnDraw(ren, &btnValider);
            bbtnDraw(ren, &btnRetour);
        }

        /* ---- CHARACTER SELECT ---- */
        else if (state == APP_CHARACTER_SELECT) {
            matrix_render(ren, SCREEN_WIDTH);

            SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(ren, 0, 0, 0, 155);
            SDL_Rect full = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
            SDL_RenderFillRect(ren, &full);
            SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);

            SDL_Event *charEvPtr = hasEnigmeEv ? &enigmeEv : NULL;
            int choice = renderCharacterSelect(ren, fontMain, fontSmall,
                                               charEvPtr, mx, my);
            if (choice == 0) {
                selectedCharacter = 0;
                state = APP_NAME_ENTRY;
            } else if (choice == 1) {
                selectedCharacter = 1;
                state = APP_NAME_ENTRY;
            }
        }

        /* ---- NAME ENTRY ---- */
        else if (state == APP_NAME_ENTRY) {
            matrix_render(ren, SCREEN_WIDTH);
            SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(ren, 0, 0, 0, 160);
            SDL_Rect full = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
            SDL_RenderFillRect(ren, &full);
            SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);

            int wantsQuit = 0, wantsEsc = 0;
            int nameResult = nameEntryUpdate(&nameState, ren,
                                              fontMain, fontSmall,
                                              &bg, mx, my,
                                              &wantsQuit, &wantsEsc);
            if (wantsQuit) { running = 0; }
            else if (wantsEsc) {
                SDL_StopTextInput();
                nameState.inputActive = 0;
                if (dispMode == MODE_MONO)
                    state = APP_CHARACTER_SELECT;
                else
                    state = APP_MODE_SELECT;
            }
            else if (nameResult == 1 || nameResult == 2) {
                SDL_StopTextInput();
                nameState.inputActive = 0;

                libererJoueur(&p1);
                libererJoueur(&p2);
                freeBackground(&bg);

                Mix_HaltMusic();
                initBackground(&bg, ren, currentLevel, dispMode);

                float sy = (currentLevel == LEVEL_1)
                           ? (float)(508 - PLAYER_PH)
                           : (float)(560 - PLAYER_PH);
                memset(&p1, 0, sizeof(Player));
                memset(&p2, 0, sizeof(Player));
                initialiserJoueur(&p1, ren, PLAYER_1, 150.0f, sy);
                initialiserJoueur(&p2, ren, PLAYER_2, 250.0f, sy);

                /* [CHARSEL-3] Apply selected character name */
                const char *charName = (selectedCharacter == 0) ? "NEO" : "MORPHEUS";
                strncpy(p1.name, charName, 31);
                p1.name[31] = '\0';
                if (nameState.name[0] != '\0') {
                    strncpy(p1.name, nameState.name, 31);
                    p1.name[31] = '\0';
                }
                strncpy(p2.name, (selectedCharacter == 0) ? "MORPHEUS" : "NEO", 31);

                resetEnemies(minions, MAX_MINIONS, &boss,
                             &bossSpawned, &bossActive,
                             &spawnTimer, &minionKills, ren, currentLevel);

                if (nameResult == 2 && nameState.bestScore > 0)
                    p1.score = nameState.bestScore;

                p1WasAlive   = 1;
                p2WasAlive   = 1;
                enigmePlayer = NULL;

                mm.p1Thumb = (p1.anims[STATE_IDLE].textures &&
                              p1.anims[STATE_IDLE].frameCount > 0)
                             ? p1.anims[STATE_IDLE].textures[0] : NULL;
                mm.p2Thumb = (p2.anims[STATE_IDLE].textures &&
                              p2.anims[STATE_IDLE].frameCount > 0)
                             ? p2.anims[STATE_IDLE].textures[0] : NULL;

                state = APP_GAME;
            }
        }

        /* ---- OPTION / HISTOIRE / SCORES / SAVE ---- */
        else if (state == APP_OPTION) {
            renderOption(ren, fontSmall, &backBtn, mx, my, musicVol, au);
        }
        else if (state == APP_SCORES) {
            renderScoresScreen(ren, &bg, &backBtn, mx, my, 1);
        }
        else if (state == APP_HISTOIRE) {
            renderHistoire(ren, fontSmall, &backBtn, mx, my);
        }
        else if (state == APP_SAVE_SCREEN) {
            renderScoresScreen(ren, &bg, &backBtn, mx, my, 0);
        }

        /* ---- GAME ---- */
        else if (state == APP_GAME) {
            if (dispMode == MODE_MULTI) {
                SDL_Rect vp1 = {0,              0, SCREEN_WIDTH/2, SCREEN_HEIGHT};
                SDL_Rect vp2 = {SCREEN_WIDTH/2, 0, SCREEN_WIDTH/2, SCREEN_HEIGHT};
                renderHalf(ren, &bg, &p1, &p2, vp1,
                           minions, MAX_MINIONS, bossActive ? &boss : NULL, bossActive);
                renderHalf(ren, &bg, &p2, &p1, vp2,
                           minions, MAX_MINIONS, bossActive ? &boss : NULL, bossActive);

                SDL_RenderSetViewport(ren, NULL);
                SDL_RenderSetClipRect(ren, NULL);

                SDL_SetRenderDrawColor(ren, 0, 255, 70, 255);
                for (int dx = -1; dx <= 1; dx++)
                    SDL_RenderDrawLine(ren,
                        SCREEN_WIDTH/2 + dx, 0,
                        SCREEN_WIDTH/2 + dx, SCREEN_HEIGHT);
            } else {
                afficherBackground(&bg, ren, MODE_MONO, NULL);

                for (int i = 0; i < MAX_MINIONS; i++)
                    if (minions[i].alive)
                        render_enemy(&minions[i], ren, (int)bg.camX);
                if (bossActive && boss.alive)
                    render_enemy(&boss, ren, (int)bg.camX);

                afficherJoueur(&p1, ren, bg.camX, bg.camY);
                afficherBalles(&p1, ren, bg.camX, bg.camY);
                afficherHUDJoueur(&p1, ren, 10, 10);
                afficherTemps(&bg, ren, SCREEN_WIDTH - 160, 10);
                afficherGuide(&bg, ren);
                afficherNotification(&bg, ren);

                renderMinimap(&mm, ren, &p1, &p2,
                              dispMode == MODE_MULTI ? 1 : 0,
                              minions, MAX_MINIONS,
                              bossActive ? &boss : NULL, bossActive);

                if (bossActive && boss.alive)
                    drawBossHealthBar(ren, &boss);

                if (!bossSpawned) {
                    char progBuf[48];
                    int  pct = minionKills * 100 / MINION_KILL_THRESHOLD;
                    if (pct > 100) pct = 100;
                    snprintf(progBuf, sizeof(progBuf), "AGENTS: %d%%", pct);
                    drawText(ren, SCREEN_WIDTH - 160, 30, progBuf, 1, 180, 60, 60);
                }

                if (bg.paused) {
                    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
                    SDL_SetRenderDrawColor(ren, 0, 0, 0, 120);
                    SDL_Rect full = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
                    SDL_RenderFillRect(ren, &full);
                    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
                    drawTextCentered(ren, SCREEN_HEIGHT/2 - 20,
                                     0, SCREEN_WIDTH, "PAUSE", 4, 0, 255, 80);
                    drawTextCentered(ren, SCREEN_HEIGHT/2 + 40,
                                     0, SCREEN_WIDTH, "P = REPRENDRE", 2, 0, 200, 60);
                }
            }
        }

        /* ---- ENIGME CHOICE ---- */
        else if (state == APP_ENIGME_CHOICE) {
            matrix_render(ren, SCREEN_WIDTH);
            SDL_Event *evPtr = hasEnigmeEv ? &enigmeEv : NULL;
            int choice = enigmeRenderChoice(&enigme, ren, evPtr, mx, my);
            if (choice == 0) {
                enigmeStartQCM(&enigme);
                hasEnigmeEv = 0;
                state = APP_ENIGME_QCM;
            } else if (choice == 1) {
                enigmeStartPuzzle(&enigme);
                hasEnigmeEv = 0;
                state = APP_ENIGME_PUZZLE;
            }
        }

        /* ---- ENIGME QCM ---- */
        else if (state == APP_ENIGME_QCM) {
            matrix_render(ren, SCREEN_WIDTH);
            SDL_Event *evPtr = hasEnigmeEv ? &enigmeEv : NULL;
            EnigmeResult res = enigmeUpdateQCM(&enigme, ren, evPtr);
            if (res != ENIGME_PENDING) {
                enigme.flashFrames = 0;
                hasEnigmeEv = 0;
                state = APP_ENIGME_RESULT;
            }
        }

        /* ---- ENIGME PUZZLE ---- */
        else if (state == APP_ENIGME_PUZZLE) {
            matrix_render(ren, SCREEN_WIDTH);
            SDL_Event *evPtr = hasEnigmeEv ? &enigmeEv : NULL;
            EnigmeResult res = enigmeUpdatePuzzle(&enigme, ren, evPtr);
            if (res != ENIGME_PENDING) {
                enigme.flashFrames = 0;
                hasEnigmeEv = 0;
                state = APP_ENIGME_RESULT;
            }
        }

        /* ---- ENIGME RESULT ---- */
        else if (state == APP_ENIGME_RESULT) {
            int done = enigmeRenderResult(&enigme, ren);
            if (done) {
                if (enigme.result == ENIGME_WIN) {
                    if (enigmePlayer) {
                        enigmePlayer->isAlive  = 1;
                        enigmePlayer->health   = 60;
                        enigmePlayer->lives    = (enigmePlayer->lives > 0)
                                                 ? enigmePlayer->lives : 1;
                        enigmePlayer->state    = STATE_IDLE;
                        enigmePlayer->velX     = 0;
                        enigmePlayer->velY     = 0;
                        enigmePlayer->onGround = 1;
                        float floorY = (float)((currentLevel == LEVEL_1 ? 508 : 560)
                                               - PLAYER_PH);
                        enigmePlayer->worldY   = floorY;
                        enigmePlayer = NULL;
                    }
                    p1WasAlive = p1.isAlive;
                    p2WasAlive = p2.isAlive;
                    state = APP_GAME;
                    setNotification(&bg, "VIE REGAGNEE !", 2000);
                } else {
                    SDL_RenderPresent(ren);
                    SDL_Delay(200);

                    char nom[MAX_NAME_LEN]  = {0};
                    char nom2[MAX_NAME_LEN] = {0};
                    snprintf(nom, sizeof(nom), "%s", nameState.name);

                    if (dispMode == MODE_MULTI) {
                        saisirNomJoueur(ren, nom);
                        sauvegarderScore(&bg, nom,  p1.score, currentLevel);
                        SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
                        SDL_RenderClear(ren); SDL_RenderPresent(ren);
                        SDL_Delay(300);
                        saisirNomJoueur(ren, nom2);
                        sauvegarderScore(&bg, nom2, p2.score, currentLevel);
                    } else {
                        if (nom[0] == '\0')
                            saisirNomJoueur(ren, nom);
                        sauvegarderScore(&bg, nom, p1.score, currentLevel);
                    }

                    afficherMeilleursScores(&bg, ren);
                    enigmePlayer = NULL;

                    libererJoueur(&p1);
                    libererJoueur(&p2);
                    freeBackground(&bg);
                    currentLevel = LEVEL_1;
                    initBackground(&bg, ren, LEVEL_1, dispMode);

                    float sy = (float)(508 - PLAYER_PH);
                    memset(&p1, 0, sizeof(Player));
                    memset(&p2, 0, sizeof(Player));
                    initialiserJoueur(&p1, ren, PLAYER_1, 150.0f, sy);
                    initialiserJoueur(&p2, ren, PLAYER_2, 250.0f, sy);
                    strncpy(p1.name, nameState.name, 31);
                    strncpy(p2.name, "TRINITY", 31);

                    resetEnemies(minions, MAX_MINIONS, &boss,
                                 &bossSpawned, &bossActive,
                                 &spawnTimer, &minionKills, ren, currentLevel);
                    minimapSetLevel(&mm, 1);
                    mm.p1Thumb = (p1.anims[STATE_IDLE].textures &&
                                  p1.anims[STATE_IDLE].frameCount > 0)
                                 ? p1.anims[STATE_IDLE].textures[0] : NULL;
                    mm.p2Thumb = (p2.anims[STATE_IDLE].textures &&
                                  p2.anims[STATE_IDLE].frameCount > 0)
                                 ? p2.anims[STATE_IDLE].textures[0] : NULL;

                    p1WasAlive = 1;
                    p2WasAlive = 1;
                    state = APP_MAIN_MENU;
                    if (au && au->music) Mix_PlayMusic(au->music, -1);
                }
            }
        }

        SDL_RenderPresent(ren);

        /* Frame cap */
        Uint32 ft = SDL_GetTicks() - fs;
        if (ft < FRAME_MS) SDL_Delay(FRAME_MS - ft);
    } /* end main loop */

    /* ── CLEANUP ── */
    SDL_StopTextInput();
    enigmeFree(&enigme);
    libererJoueur(&p1);
    libererJoueur(&p2);
    freeBackground(&bg);
    freeMinimap(&mm);
    matrix_free();
    if (au) free_audio(au);
    for (int i = 0; i < BTN_COUNT; i++) tbtnFree(&menuBtns[i]);
    tbtnFree(&backBtn);
    if (titleTex) SDL_DestroyTexture(titleTex);
    TTF_CloseFont(fontMain);
    TTF_CloseFont(fontMatrix);
    if (fontSmall && fontSmall != fontMain) TTF_CloseFont(fontSmall);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    TTF_Quit(); IMG_Quit(); SDL_Quit();
    return 0;
}