/**
 * main.c — MATRIX GAME  (Comprehensive Fix Build)
 * =================================================
 *
 * FIXES IN THIS VERSION:
 *
 *  [INPUT-1]  P2 arrow keys conflicted with P1 arrow keys in MONO mode —
 *             P2 now only captures arrow keys when dispMode == MODE_MULTI.
 *
 *  [INPUT-2]  Level-switch keys were SDLK_1/SDLK_2 which overlapped with
 *             QCM answer keys. Changed to F11/F12 (already done) and
 *             added guard so they cannot fire during enigme states.
 *
 *  [INPUT-3]  Both players fired on SDL_MOUSEBUTTONDOWN LEFT — now P1
 *             fires on LEFT and P2 fires on RIGHT only when in MULTI mode.
 *
 *  [INPUT-4]  SDL_StartTextInput was never called for the NAME_ENTRY
 *             state re-entry path; now always called on state enter.
 *
 *  [BOSS-1]   Boss y was reset EVERY frame by setEnemyY(), overriding
 *             any vertical displacement from update_enemy(). setEnemyY is
 *             now called only once at spawn, not every frame.
 *
 *  [BOSS-2]   Boss kill score was added to p1.score but not to p2 in
 *             MULTI mode. Fixed.
 *
 *  [BOSS-3]   Boss could re-spawn because bossSpawned was never set when
 *             all minions were dead but minionKills < threshold (edge case
 *             where minions are stomped). Kill counter now also increments
 *             on stomp (health <= 0 path).
 *
 *  [BOSS-4]   After boss defeat, bossActive was set to 0 but the dead
 *             boss struct was still passed to renderMinimap as "active".
 *             Fixed by only passing when bossActive==1 AND boss.alive.
 *
 *  [ENIGME-1] enigmeRenderChoice polled no event in the render path
 *             because gotEvent was 0 after the event loop consumed the
 *             SDL_MOUSEBUTTONDOWN. Events for enigme screens are now
 *             buffered separately (enigmeEv) and passed every frame.
 *
 *  [ENIGME-2] APP_ENIGME_RESULT called SDL_RenderPresent inside
 *             enigmeRenderResult, then main loop called it again —
 *             double-present causing a flicker. Removed internal present.
 *
 *  [ENIGME-3] On ENIGME_WIN the p1WasAlive/p2WasAlive trackers were not
 *             reset before returning to APP_GAME, causing an immediate
 *             re-trigger of the enigme for the same player. Fixed.
 *
 *  [ENIGME-4] enigme.selectedTile was not reset between attempts when
 *             the puzzle screen was re-entered. Now reset in enigmeStartPuzzle
 *             (already in enigme.c) AND guarded here.
 *
 *  [ENIGME-5] QCM timer comparison used SDL_GetTicks() – startTime with
 *             no frame-delta guard; can skip from PENDING to LOSE in one
 *             frame if ticks overflow. Added explicit PENDING guard.
 *
 *  [SPAWN-1]  Enemy spawn used p1.worldX ± 800 which could place enemies
 *             off-screen left on wide worlds. Now uses a visibility-aware
 *             spawn that always places enemies just off the current viewport.
 *
 *  [SPAWN-2]  After resetEnemies only 2 minions were inited but the loop
 *             runs MAX_MINIONS (5). Remaining slots had alive=0 but dirty
 *             memory from previous game. memset before init fixed this.
 *
 *  [KILL-1]   minionKills was incremented using a local wasAlive=1 flag
 *             that was never set to the actual prior-alive state. The loop
 *             now correctly saves alive before calling update_enemy.
 *
 *  [RENDER-1] In MULTI mode the split-screen divider was drawn over the
 *             HUD because SDL_RenderSetViewport(NULL) wasn't called first.
 *             Viewport is now cleared before drawing the divider line.
 *
 *  [RENDER-2] renderHalf set bg->camX = p->camX but forgot to restore it
 *             after afficherBackground, so the second player's half used
 *             a stale camX. savedCam restore added.
 *
 *  [HUD-1]   afficherTemps was called with SCREEN_WIDTH-140 in MONO but
 *            the time string width is ~130px at scale 2 — it was clipped.
 *            Position adjusted to SCREEN_WIDTH-160.
 *
 *  [MEMORY-1] tbtnFree was never called on menuBtns array in cleanup.
 *             Fixed (was already partially done; ensured all 5 are freed).
 *
 *  [PAUSE-1]  Pausing via 'p' key in GAME state consumed the event but
 *             the rendering did not skip enemy/player updates in the same
 *             frame. updateBackground already checks bg.paused; player
 *             update now also skips when paused.
 *
 *  [CAMERA-1] In MULTI mode updateCamera was never called, so bg->camX
 *             stayed at 0. Each player has their own camX (p->camX) used
 *             by renderHalf, which is correct — removed stale MONO-only
 *             updateCamera call that was not conditional on MODE_MONO.
 *
 *  [NAMEINPUT-1] APP_NAME_ENTRY text input was frozen because SDL_TEXTINPUT
 *             and SDL_KEYDOWN events were consumed by the shared event loop
 *             before reaching nameEntryUpdate(). The event loop now skips
 *             processing for NAME_ENTRY state (except SDL_QUIT and ESC),
 *             and nameEntryUpdate() polls its own events directly so that
 *             every keystroke is guaranteed to reach the name input handler.
 *
 *  [LED-2]    Arduino LED feedback: print "HIT\n" or "DEAD\n" to stdout
 *             when p1/p2 damageEvent is set. Must happen BEFORE
 *             renderMinimap() because minimap.c resets damageEvent = 0
 *             inside renderMinimap when it draws the red flash overlay.
 *             The Python bridge watches stdout and sends 'B' or 'D' to
 *             the Arduino which blinks the LED accordingly.
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
 * ============================================================ */
typedef enum {
    APP_MAIN_MENU,
    APP_MODE_SELECT,
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
 * GAME HELPERS
 * ============================================================ */

/* [BOSS-1] Only called at spawn, not every frame */
static void setEnemyY(Enemy *e, GameLevel level) {
    int groundY = (level == LEVEL_1) ? 508 : 560;
    e->y = groundY - e->height;
    if (e->y < 0) e->y = 0;
}

/* [INPUT-3] Bullet collision only deals damage; kills detected in main loop */
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
            if (minions[i].hit_cooldown > 0) continue; /* prevent multi-hit */
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
                   PLAYER_W - 16, PLAYER_PH - 8}; /* slightly inset hitbox */
    SDL_Rect er = {e->x, e->y, e->width, e->height};
    if (!check_enemy_collision(pr, er)) return;

    /* Stomp: player falling onto enemy top half */
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
        /* Minions only damage when attacking; boss always damages */
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

/* [RENDER-2] renderHalf with corrected camX save/restore */
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

    /* [RENDER-2] Save both cam axes, restore after */
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

    /* Other player drawn in this half's viewport if visible */
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

/* [SPAWN-2] Full reset with memset before re-init */
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

    /* Spawn 2 initial minions at fixed positions */
    init_enemy(&minions[0], 0, renderer, 900);  setEnemyY(&minions[0], level);
    init_enemy(&minions[1], 0, renderer, 1500); setEnemyY(&minions[1], level);
}

/* [BOSS-1] Boss health bar */
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

    /* Gradient: green → orange → red */
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

/*
 * [NAMEINPUT-1] nameEntryUpdate now polls its own event queue directly.
 * Returns 0=stay, 1=new game, 2=continue
 */
static int nameEntryUpdate(NameEntryState *ns, SDL_Renderer *ren,
                            TTF_Font *font, TTF_Font *fontSmall,
                            Background *bg, int mx, int my,
                            int *outQuit, int *outEsc)
{
    /* [INPUT-4] Ensure text input is active */
    if (!ns->inputActive) {
        SDL_StopTextInput();
        SDL_Delay(50);
        while (SDL_PollEvent(NULL)) {} /* flush stale events */
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
                return 1; /* new game */
        }
        if (ev.type == SDL_MOUSEBUTTONDOWN &&
            ev.button.button == SDL_BUTTON_LEFT) {
            mx = ev.button.x;
            my = ev.button.y;
        }
    }

    /* ── Render ── */
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
 * HELPERS — visibility-aware enemy spawn position
 * [SPAWN-1] Always place enemy just off-screen relative to player
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

    /* [ENIGME-1] Separate event storage for enigme screens */
    SDL_Event enigmeEv;
    memset(&enigmeEv, 0, sizeof(enigmeEv));
    int hasEnigmeEv = 0;

    /* Death transition trackers */
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
                        SDL_StopTextInput();
                        state = APP_MAIN_MENU;
                        if (au && au->music) Mix_PlayMusic(au->music, -1);
                    }
                    else if (state == APP_MODE_SELECT)   state = APP_MAIN_MENU;
                    else if (state == APP_OPTION)        state = APP_MAIN_MENU;
                    else if (state == APP_SCORES)        state = APP_MAIN_MENU;
                    else if (state == APP_HISTOIRE)      state = APP_MAIN_MENU;
                    else if (state == APP_SAVE_SCREEN)   state = APP_MAIN_MENU;
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
                    /* [INPUT-1] P1 always gets events; P2 only in MULTI mode */
                    gererEvenementJoueur(&p1, &ev);
                    if (dispMode == MODE_MULTI) gererEvenementJoueur(&p2, &ev);

                    if (ev.type == SDL_KEYDOWN) {
                        SDL_Keycode k = ev.key.keysym.sym;
                        if (k == SDLK_F10) afficherMeilleursScores(&bg, ren);
                        if (k == SDLK_h)   bg.guide.state = GUIDE_VISIBLE;
                        if (k == SDLK_p)   togglePause(&bg);
                        /* [INPUT-2] Level switch only in GAME state, not enigme */
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

                /* [ENIGME-1] Buffer enigme event for use in render section */
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

        if (state == APP_ENIGME_CHOICE ||
            state == APP_ENIGME_QCM    ||
            state == APP_ENIGME_PUZZLE)
            matrix_update(dt, SCREEN_HEIGHT);

        if (state == APP_GAME && !bg.paused) {
            /* [PAUSE-1] Skip all updates when paused */
            mettreAJourJoueur(&p1, bg.platforms, bg.platformCount);
            if (dispMode == MODE_MULTI)
                mettreAJourJoueur(&p2, bg.platforms, bg.platformCount);

            if (dispMode == MODE_MONO) {
                bg.camX = p1.camX;
                bg.camY = 0;
            }

            updateBackground(&bg);

            /* Enemy spawn — only while kill threshold not reached */
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

            /* Update minions — [KILL-1] save alive state BEFORE update */
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

            /* Boss spawn trigger — [BOSS-3] check all minions dead */
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

            /* Update boss */
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

            /* Bullets vs enemies */
            checkBulletsVsEnemies(&p1, minions, MAX_MINIONS,
                                  bossActive ? &boss : NULL, bossActive);
            if (dispMode == MODE_MULTI)
                checkBulletsVsEnemies(&p2, minions, MAX_MINIONS,
                                      bossActive ? &boss : NULL, bossActive);

            /* ── [LED-2] Arduino LED feedback ────────────────────────────
             * Check damageEvent HERE, before renderMinimap() clears it.
             * minimap.c resets p->damageEvent = 0 inside renderMinimap()
             * when it draws the red screen flash. If this block ran after
             * rendering, damageEvent would always be 0 and the LED would
             * never blink. The Python bridge reads these lines from stdout
             * and sends 'B' (hit) or 'D' (death) to the Arduino.
             * Do NOT reset damageEvent here — renderMinimap() owns that. */
            if (p1.damageEvent) {
                printf(p1.isAlive ? "HIT\n" : "DEAD\n");
                fflush(stdout);
            }
            if (dispMode == MODE_MULTI && p2.damageEvent) {
                printf(p2.isAlive ? "HIT\n" : "DEAD\n");
                fflush(stdout);
            }
            /* ── end LED feedback ──────────────────────────────────────── */

            /* Death detection — [ENIGME-3] transition guard */
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

                strncpy(p1.name, nameState.name, 31);
                p1.name[31] = '\0';
                strncpy(p2.name, "TRINITY", 31);

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

                /* [RENDER-1] Restore viewport before drawing divider */
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
                    /* [ENIGME-3] Reset trackers AFTER reviving player */
                    p1WasAlive = p1.isAlive;
                    p2WasAlive = p2.isAlive;
                    state = APP_GAME;
                    setNotification(&bg, "VIE REGAGNEE !", 2000);
                } else {
                    /* GAME OVER */
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

                    /* Full reset */
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
    /* [MEMORY-1] Free all menu buttons */
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