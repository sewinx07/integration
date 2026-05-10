/**
 * main.c  —  MATRIX GAME  (Unified)
 * ===================================
 * Integrates the Cyber-Veil main menu (matrix rain, TTF buttons,
 * hover sound, title image) with the full game engine.
 *
 * Asset layout expected:
 *   assets/font/arial.ttf
 *   assets/title.png
 *   assets/son/background.mp3
 *   assets/son/hover.wav
 *   assets/backgrounds/matrix_background_final.png
 *   assets/backgrounds/background_level2.png
 *   assets/sprites/  ...
 *   assets/ennemie/  ...
 *   assets/scores.dat  (auto-created)
 *
 * App states
 * ──────────
 *   APP_MAIN_MENU   Cyber-Veil menu  (JOUER / OPTION / MEILLEUR SCORE / HISTOIRE / QUITTER)
 *   APP_MODE_SELECT Choose MONO or MULTI
 *   APP_OPTION      Options screen (volume, controls hint)
 *   APP_SCORES      Best scores screen (reuses afficherMeilleursScores)
 *   APP_HISTOIRE    Story / lore screen
 *   APP_GAME        The actual game
 *   APP_SAVE_SCREEN Save screen
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

/* ============================================================
 * CONSTANTS
 * ============================================================ */
#define WINDOW_TITLE   "MATRIX GAME"
#define FPS_TARGET     60
#define FRAME_MS       (1000 / FPS_TARGET)
#define MAX_MINIONS    5
#define BULLET_DAMAGE  40
#define BTN_COUNT      5      /* main menu buttons          */
#define BTN_MODE_COUNT 3      /* mode-select buttons        */

/* ============================================================
 * APP STATES
 * ============================================================ */
typedef enum {
    APP_MAIN_MENU,
    APP_MODE_SELECT,
    APP_OPTION,
    APP_SCORES,
    APP_HISTOIRE,
    APP_GAME,
    APP_SAVE_SCREEN,
} AppState;

/* ============================================================
 * TTF BUTTON  (used for main menu & sub-screens)
 * ============================================================ */
typedef struct {
    SDL_Rect     rect;
    const char  *label;
    SDL_Texture *tex;
    SDL_Rect     texRect;
    int          hover;
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
                             y + (h - s->h) / 2,
                             s->w, s->h};
    SDL_FreeSurface(s);
}

static void tbtnDraw(SDL_Renderer *ren, TBtn *b)
{
    if (b->hover) {
        SDL_SetRenderDrawColor(ren, 57, 255, 20, 120);
    } else {
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 180);
    }
    SDL_RenderFillRect(ren, &b->rect);
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
 * BITMAP BUTTON  (used inside the game for mode-select)
 * ============================================================ */
typedef struct {
    SDL_Rect    r;
    const char *text;
    int         hover;
} BBtn;

static int bbtnMouseOn(BBtn *b, int x, int y) {
    return x >= b->r.x && x <= b->r.x + b->r.w &&
           y >= b->r.y && y <= b->r.y + b->r.h;
}

static void bbtnDraw(SDL_Renderer *ren, BBtn *b) {
    SDL_Rect d = b->r;
    if (b->hover) {
        d.x -= 4; d.y -= 4; d.w += 8; d.h += 8;
        SDL_SetRenderDrawColor(ren, 0, 255, 80, 255);
    } else {
        SDL_SetRenderDrawColor(ren, 0, 140, 40, 220);
    }
    SDL_RenderFillRect(ren, &d);
    SDL_SetRenderDrawColor(ren, 0, 255, 60, 255);
    SDL_RenderDrawRect(ren, &d);
    int tw = textWidth(b->text, 2);
    int th = textHeight(2);
    drawText(ren, d.x + (d.w - tw) / 2, d.y + (d.h - th) / 2,
             b->text, 2, 0, 255, 100);
}

/* ============================================================
 * GAME HELPERS  (unchanged from original)
 * ============================================================ */
static void clampBgCam(Background *bg) {
    if (bg->camX < 0) bg->camX = 0;
    if (bg->camY < 0) bg->camY = 0;
    if (bg->camX > WORLD_WIDTH  - SCREEN_WIDTH)
        bg->camX = (float)(WORLD_WIDTH  - SCREEN_WIDTH);
    if (bg->camY > WORLD_HEIGHT - SCREEN_HEIGHT)
        bg->camY = (float)(WORLD_HEIGHT - SCREEN_HEIGHT);
}

static void setEnemyY(Enemy *e, GameLevel level) {
    int groundY = (level == LEVEL_1) ? 508 : 560;
    e->y = groundY - e->height;
}

static void checkBulletsVsEnemies(Player *p,
                                   Enemy *minions, int minionCount,
                                   Enemy *boss, int bossActive) {
    for (int b = 0; b < MAX_BULLETS; b++) {
        if (!p->bullets[b].active) continue;
        SDL_Rect br = {(int)p->bullets[b].x, (int)p->bullets[b].y,
                       BULLET_W, BULLET_H_PX};
        for (int i = 0; i < minionCount; i++) {
            if (!minions[i].alive) continue;
            SDL_Rect er = {minions[i].x, minions[i].y,
                           minions[i].width, minions[i].height};
            if (check_enemy_collision(br, er)) {
                minions[i].health -= BULLET_DAMAGE;
                p->bullets[b].active = 0;
                p->score += 10;
                if (!minions[i].alive) p->score += 90;
                break;
            }
        }
        if (bossActive && boss->alive && p->bullets[b].active) {
            SDL_Rect er = {boss->x, boss->y, boss->width, boss->height};
            if (check_enemy_collision(br, er)) {
                boss->health -= BULLET_DAMAGE;
                p->bullets[b].active = 0;
                p->score += 20;
            }
        }
    }
}

static void checkPlayerVsEnemy(Player *p, Enemy *e) {
    if (!e->alive) return;
    SDL_Rect pr = {(int)p->worldX, (int)p->worldY, PLAYER_W, PLAYER_PH};
    SDL_Rect er = {e->x, e->y, e->width, e->height};
    if (!check_enemy_collision(pr, er)) return;
    if (p->velY > 0 && (int)p->worldY + PLAYER_PH <= e->y + 20) {
        e->health -= 999; p->velY = -12.0f; p->score += 100; return;
    }
    /* Use the player's own lastDamageTime so P1 and P2 each have
       an independent damage cooldown — e->hit_cooldown is shared
       and would block one player whenever the other got hit. */
    Uint32 now = SDL_GetTicks();
    int canHit = (now - p->lastDamageTime) > 1000;
    if (canHit && (e->state == 2 || e->type == 1)) {
        p->health -= 20;
        p->lastDamageTime = now;
        p->damageEvent = 1;
        if (p->health <= 0) p->isAlive = 0;
    }
}

static void renderHalf(SDL_Renderer *renderer, Background *bg,
                        Player *p, Player *other, SDL_Rect vp,
                        Enemy *minions, int minionCount,
                        Enemy *boss, int bossActive) {
    SDL_RenderSetViewport(renderer, &vp);
    /* ClipRect is in viewport-local coords — always {0,0,w,h} */
    SDL_Rect localClip = {0, 0, vp.w, vp.h};
    SDL_RenderSetClipRect(renderer, &localClip);
    /* Fill viewport with black */
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderFillRect(renderer, &localClip);
    float savedCam = bg->camX;
    bg->camX = p->camX;
    afficherBackground(bg, renderer, MODE_MULTI, &vp);
    bg->camX = savedCam;
    for (int i = 0; i < minionCount; i++)
        render_enemy(&minions[i], renderer, (int)p->camX);
    if (bossActive) render_enemy(boss, renderer, (int)p->camX);
    afficherJoueur(p, renderer, p->camX, 0);
    afficherBalles(p, renderer, p->camX, 0);
    if (other && other->isAlive) {
        int ox = (int)(other->worldX - p->camX);
        if (ox > -PLAYER_W && ox < vp.w) {
            afficherJoueur(other, renderer, p->camX, 0);
            afficherBalles(other, renderer, p->camX, 0);
        }
    }
    afficherHUDJoueur(p, renderer, 8, 8);
    afficherTemps(bg, renderer, vp.w - 130, 8);
    const char *lbl = (p->id == PLAYER_1) ? "J1-NEO" : "J2-TRINITY";
    Uint8 cb = (p->id == PLAYER_1) ? 50 : 255;
    drawText(renderer, 8, vp.h - 16, lbl, 1, 0, 200, cb);
    SDL_RenderSetClipRect(renderer, NULL);
    SDL_RenderSetViewport(renderer, NULL);
}

static void resetEnemies(Enemy *minions, int count, Enemy *boss,
                          int *bossSpawned, int *bossActive,
                          int *spawnTimer,
                          SDL_Renderer *renderer, GameLevel level) {
    for (int i = 0; i < count; i++) minions[i].alive = 0;
    if (*bossActive) { boss->alive = 0; *bossActive = 0; }
    *bossSpawned = 0;
    *spawnTimer  = 120;
    init_enemy(&minions[0], 0, renderer, 900);  setEnemyY(&minions[0], level);
    init_enemy(&minions[1], 0, renderer, 1500); setEnemyY(&minions[1], level);
}

/* ============================================================
 * OPTION SCREEN  — simple volume + controls display
 * ============================================================ */
static void renderOption(SDL_Renderer *ren, TTF_Font *font,
                          TBtn *backBtn, int mx, int my,
                          int musicVol, GameAudio *au)
{
    /* background overlay */
    SDL_SetRenderDrawColor(ren, 0, 10, 0, 255);
    SDL_Rect full = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
    SDL_RenderFillRect(ren, &full);

    /* title */
    SDL_Color green = {57, 255, 20, 255};
    SDL_Surface *ts = TTF_RenderText_Blended(font, "OPTIONS", green);
    if (ts) {
        SDL_Texture *tt = SDL_CreateTextureFromSurface(ren, ts);
        SDL_Rect tr = {SCREEN_WIDTH/2 - ts->w/2, 60, ts->w, ts->h};
        SDL_RenderCopy(ren, tt, NULL, &tr);
        SDL_DestroyTexture(tt); SDL_FreeSurface(ts);
    }

    /* volume display */
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

    /* volume bar */
    int barW = 400, barH = 20;
    int barX = SCREEN_WIDTH/2 - barW/2, barY = 250;
    SDL_SetRenderDrawColor(ren, 40, 40, 40, 255);
    SDL_Rect barBg = {barX, barY, barW, barH};
    SDL_RenderFillRect(ren, &barBg);
    SDL_SetRenderDrawColor(ren, 57, 255, 20, 255);
    SDL_Rect barFill = {barX, barY, barW * musicVol / 100, barH};
    SDL_RenderFillRect(ren, &barFill);

    /* volume arrows hint */
    SDL_Surface *hs = TTF_RenderText_Blended(font,
        "FLECHE GAUCHE / DROITE  pour regler le volume", white);
    if (hs) {
        SDL_Texture *ht = SDL_CreateTextureFromSurface(ren, hs);
        SDL_Rect hr = {SCREEN_WIDTH/2 - hs->w/2, 290, hs->w, hs->h};
        SDL_RenderCopy(ren, ht, NULL, &hr);
        SDL_DestroyTexture(ht); SDL_FreeSurface(hs);
    }

    /* controls reference */
    const char *ctls[] = {
        "CLAVIER — J1 : Q/D deplacement  SPACE saut  SHIFT sprint  CTRL tir",
        "CLAVIER — J2 : GAUCHE/DROITE    ENTREE saut RSHIFT sprint RCTRL tir",
        "SOURIS  — J1 : clic gauche  |  J2 : clic droit",
        "MANETTE — Joystick gauche mouvement  Bouton tir = CTRL  ESC = menu",
    };
    int cy = 380;
    for (int i = 0; i < 4; i++) {
        SDL_Surface *cs = TTF_RenderText_Blended(font, ctls[i], white);
        if (!cs) continue;
        SDL_Texture *ct = SDL_CreateTextureFromSurface(ren, cs);
        SDL_Rect cr = {SCREEN_WIDTH/2 - cs->w/2, cy, cs->w, cs->h};
        SDL_RenderCopy(ren, ct, NULL, &cr);
        SDL_DestroyTexture(ct); SDL_FreeSurface(cs);
        cy += 42;
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
        "Les Agents — programs corrompus — patrouillent les rues numeriques,",
        "eliminant quiconque tente de briser le code.",
        "",
        "Arme de son agilite et de son pistolet a balles de donnees,",
        "NEO doit traverser deux niveaux de simulation pour atteindre",
        "le BOSS et liberer la matrice.",
        "",
        "Seul ou avec TRINITY a ses cotes, la survie dependra",
        "de votre vitesse, precision et strategie.",
    };
    int cy = 150;
    TTF_Font *smallFont = font; /* reuse same font */
    for (int i = 0; i < (int)(sizeof(story)/sizeof(story[0])); i++) {
        if (story[i][0] == '\0') { cy += 20; continue; }
        SDL_Surface *ss = TTF_RenderText_Blended(smallFont, story[i], white);
        if (!ss) { cy += 36; continue; }
        SDL_Texture *st = SDL_CreateTextureFromSurface(ren, ss);
        SDL_Rect sr = {SCREEN_WIDTH/2 - ss->w/2, cy, ss->w, ss->h};
        SDL_RenderCopy(ren, st, NULL, &sr);
        SDL_DestroyTexture(st); SDL_FreeSurface(ss);
        cy += 36;
    }

    backBtn->hover = tbtnHit(backBtn, mx, my);
    tbtnDraw(ren, backBtn);
}

/* ============================================================
 * MAIN
 * ============================================================ */
int main(int argc, char *argv[]) {
    (void)argc; (void)argv;

    /* ── SDL init ── */
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

    /* ── Title texture (Cyber-Veil logo) ── */
    SDL_Texture *titleTex = IMG_LoadTexture(ren, "assets/title.png");
    if (!titleTex)
        printf("ATTENTION : assets/title.png introuvable (menu sans logo)\n");
    SDL_Rect titleRect = {340, 20, 600, 200};

    /* ── Matrix rain ── */
    matrix_init(ren, SCREEN_WIDTH, SCREEN_HEIGHT, fontMatrix);

    /* ── Audio ── */
    GameAudio *au = init_audio();
    if (!au) printf("ATTENTION : audio non initialise\n");

    /* ── Main menu TTF buttons ──
       0=JOUER  1=OPTION  2=MEILLEUR SCORE  3=HISTOIRE  4=QUITTER  */
    const char *menuLabels[BTN_COUNT] = {
        "JOUER", "OPTION", "MEILLEUR SCORE", "HISTOIRE", "QUITTER"
    };
    TBtn menuBtns[BTN_COUNT];
    for (int i = 0; i < BTN_COUNT; i++)
        tbtnBuild(&menuBtns[i], ren, fontMain, menuLabels[i],
                  100, 240 + i * 85, 320, 65);

    /* ── Mode-select bitmap buttons ── */
    int bw = 220, bh = 54;
    int cy_ms = SCREEN_HEIGHT / 2 - 20;
    BBtn btnMono    = {{SCREEN_WIDTH/2 - bw - 30, cy_ms,       bw, bh}, "MONO JOUEUR",   0};
    BBtn btnMulti   = {{SCREEN_WIDTH/2 + 30,       cy_ms,       bw, bh}, "MULTI JOUEURS", 0};
    BBtn btnValider = {{SCREEN_WIDTH/2 - bw/2,     cy_ms + 80,  bw, bh}, "VALIDER",       0};
    BBtn btnRetour  = {{SCREEN_WIDTH - 220,  SCREEN_HEIGHT - 70, 180, 44}, "RETOUR",       0};

    /* ── Sub-screen back buttons ── */
    TBtn backBtn;
    tbtnBuild(&backBtn, ren, fontSmall, "< RETOUR",
              40, SCREEN_HEIGHT - 80, 200, 50);

    /* ── Game objects ── */
    Background  bg;
    GameLevel   currentLevel = LEVEL_1;
    DisplayMode dispMode     = MODE_MONO;
    initBackground(&bg, ren, LEVEL_1, MODE_MONO);

    float startY = (float)(508 - PLAYER_PH);
    Player p1, p2;
    initialiserJoueur(&p1, ren, PLAYER_1, 150.0f, startY);
    initialiserJoueur(&p2, ren, PLAYER_2, 250.0f, startY);

    Enemy minions[MAX_MINIONS];
    memset(minions, 0, sizeof(minions));
    Enemy boss;
    memset(&boss, 0, sizeof(Enemy));
    int bossSpawned = 0, bossActive = 0, spawnTimer = 120;
    resetEnemies(minions, MAX_MINIONS, &boss,
                 &bossSpawned, &bossActive, &spawnTimer, ren, currentLevel);

    Minimap mm;
    initMinimap(&mm, ren, &p1, &p2);

    /* ── State ── */
    AppState state      = APP_MAIN_MENU;
    int      musicVol   = 50;         /* 0–100 for option screen */
    int      last_hover = -1;         /* main menu hover tracker */
    int      running    = 1;

    Uint32 last_tick = SDL_GetTicks();
    SDL_Event ev;

    /* ── Main loop ── */
    while (running) {
        Uint32 fs = SDL_GetTicks();
        float  dt = (fs - last_tick) / 1000.0f;
        last_tick = fs;

        int mx, my;
        SDL_GetMouseState(&mx, &my);

        /* ── Events ── */
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) { running = 0; break; }

            /* ESC: back or quit */
            if (ev.type == SDL_KEYDOWN && ev.key.keysym.sym == SDLK_ESCAPE) {
                if      (state == APP_GAME)        { state = APP_MAIN_MENU; if (au && au->music) Mix_PlayMusic(au->music, -1); }
                else if (state == APP_MODE_SELECT)   state = APP_MAIN_MENU;
                else if (state == APP_OPTION)        state = APP_MAIN_MENU;
                else if (state == APP_SCORES)        state = APP_MAIN_MENU;
                else if (state == APP_HISTOIRE)      state = APP_MAIN_MENU;
                else if (state == APP_SAVE_SCREEN)   state = APP_MAIN_MENU;
                else running = 0;
                break;
            }

            /* ── MAIN MENU ── */
            if (state == APP_MAIN_MENU) {
                if (ev.type == SDL_MOUSEBUTTONDOWN &&
                    ev.button.button == SDL_BUTTON_LEFT) {
                    for (int i = 0; i < BTN_COUNT; i++) {
                        if (!tbtnHit(&menuBtns[i], mx, my)) continue;
                        switch (i) {
                            case 0: state = APP_MODE_SELECT; break;
                            case 1: state = APP_OPTION;      break;
                            case 2:
                                chargerScores(&bg);
                                state = APP_SCORES;
                                break;
                            case 3: state = APP_HISTOIRE;    break;
                            case 4: running = 0;             break;
                        }
                        break;
                    }
                }
            }

            /* ── MODE SELECT ── */
            else if (state == APP_MODE_SELECT) {
                if (ev.type == SDL_MOUSEBUTTONDOWN &&
                    ev.button.button == SDL_BUTTON_LEFT) {
                    if (bbtnMouseOn(&btnMono,  mx, my)) dispMode = MODE_MONO;
                    if (bbtnMouseOn(&btnMulti, mx, my)) dispMode = MODE_MULTI;
                    if (bbtnMouseOn(&btnRetour, mx, my)) state = APP_MAIN_MENU;
                    if (bbtnMouseOn(&btnValider, mx, my)) {
                        Mix_HaltMusic();
                        initBackground(&bg, ren, currentLevel, dispMode);
                        initialiserJoueur(&p1, ren, PLAYER_1, 150.0f, startY);
                        initialiserJoueur(&p2, ren, PLAYER_2, 250.0f, startY);
                        resetEnemies(minions, MAX_MINIONS, &boss,
                                     &bossSpawned, &bossActive,
                                     &spawnTimer, ren, currentLevel);
                        state = APP_GAME;
                    }
                }
            }

            /* ── OPTION ── */
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

            /* ── SCORES ── */
            else if (state == APP_SCORES) {
                if (ev.type == SDL_MOUSEBUTTONDOWN &&
                    ev.button.button == SDL_BUTTON_LEFT)
                    if (tbtnHit(&backBtn, mx, my)) state = APP_MAIN_MENU;
                /* any key also goes back */
                if (ev.type == SDL_KEYDOWN) state = APP_MAIN_MENU;
            }

            /* ── HISTOIRE ── */
            else if (state == APP_HISTOIRE) {
                if (ev.type == SDL_MOUSEBUTTONDOWN &&
                    ev.button.button == SDL_BUTTON_LEFT)
                    if (tbtnHit(&backBtn, mx, my)) state = APP_MAIN_MENU;
                if (ev.type == SDL_KEYDOWN) state = APP_MAIN_MENU;
            }

            /* ── SAVE SCREEN ── */
            else if (state == APP_SAVE_SCREEN) {
                if (ev.type == SDL_MOUSEBUTTONDOWN &&
                    ev.button.button == SDL_BUTTON_LEFT)
                    if (tbtnHit(&backBtn, mx, my)) state = APP_MAIN_MENU;
                if (ev.type == SDL_KEYDOWN) state = APP_MAIN_MENU;
            }

            /* ── GAME ── */
            else if (state == APP_GAME) {
                gererEvenementJoueur(&p1, &ev);
                if (dispMode == MODE_MULTI) gererEvenementJoueur(&p2, &ev);
                if (ev.type == SDL_KEYDOWN) {
                    SDL_Keycode k = ev.key.keysym.sym;
                    if (k == SDLK_F10) afficherMeilleursScores(&bg, ren);
                    if (k == SDLK_h)   afficherGuide(&bg, ren);
                    if (k == SDLK_p)   togglePause(&bg);
                    /* level switches */
                    if (k == SDLK_1) {
                        currentLevel = LEVEL_1;
                        initBackground(&bg, ren, currentLevel, dispMode);
                        resetEnemies(minions, MAX_MINIONS, &boss,
                                     &bossSpawned, &bossActive,
                                     &spawnTimer, ren, currentLevel);
                        minimapSetLevel(&mm, 1);
                    }
                    if (k == SDLK_2) {
                        currentLevel = LEVEL_2;
                        initBackground(&bg, ren, currentLevel, dispMode);
                        resetEnemies(minions, MAX_MINIONS, &boss,
                                     &bossSpawned, &bossActive,
                                     &spawnTimer, ren, currentLevel);
                        minimapSetLevel(&mm, 2);
                    }
                }
            }
        } /* end event loop */

        /* ── UPDATE ── */
        if (state == APP_MAIN_MENU) {
            float dummy = 0;
            (void)dummy;
            matrix_update(dt, SCREEN_HEIGHT);

            /* hover sound for menu buttons */
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

        if (state == APP_SAVE_SCREEN) {
            backBtn.hover = tbtnHit(&backBtn, mx, my);
        }

        if (state == APP_GAME && !bg.paused) {
            mettreAJourJoueur(&p1, bg.platforms, bg.platformCount);

            /* Controller LED blink trigger */
            if (p1.damageEvent) {
                printf(p1.isAlive ? "HIT\n" : "DEAD\n");
                fflush(stdout);
                p1.damageEvent = 0;
            }

            if (dispMode == MODE_MULTI)
                mettreAJourJoueur(&p2, bg.platforms, bg.platformCount);
            if (dispMode == MODE_MONO)
                { updateCamera(&bg, p1.worldX, p1.worldY); clampBgCam(&bg); }
            updateBackground(&bg);

            /* enemy spawn */
            spawnTimer--;
            if (spawnTimer <= 0) {
                for (int i = 0; i < MAX_MINIONS; i++) {
                    if (!minions[i].alive) {
                        init_enemy(&minions[i], 0, ren,
                                   (int)p1.worldX + 800);
                        setEnemyY(&minions[i], currentLevel);
                        spawnTimer = 120; break;
                    }
                }
            }
            for (int i = 0; i < MAX_MINIONS; i++) {
                if (!minions[i].alive) continue;
                /* In multiplayer, find the closest player to this enemy */
                int enemy_ref_x = (int)p1.worldX;
                if (dispMode == MODE_MULTI) {
                    int dist_p1 = abs((int)p1.worldX - minions[i].x);
                    int dist_p2 = abs((int)p2.worldX - minions[i].x);
                    enemy_ref_x = (dist_p2 < dist_p1) ? (int)p2.worldX : (int)p1.worldX;
                }
                update_enemy(&minions[i], enemy_ref_x);
                checkPlayerVsEnemy(&p1, &minions[i]);
                if (dispMode == MODE_MULTI)
                    checkPlayerVsEnemy(&p2, &minions[i]);
            }
            
            /* Boss collision with players */
            if (bossActive) {
                /* In multiplayer, find the closest player to the boss */
                int boss_ref_x = (int)p1.worldX;
                if (dispMode == MODE_MULTI) {
                    int dist_p1 = abs((int)p1.worldX - boss.x);
                    int dist_p2 = abs((int)p2.worldX - boss.x);
                    boss_ref_x = (dist_p2 < dist_p1) ? (int)p2.worldX : (int)p1.worldX;
                }
                update_enemy(&boss, boss_ref_x);
                checkPlayerVsEnemy(&p1, &boss);
                if (dispMode == MODE_MULTI)
                    checkPlayerVsEnemy(&p2, &boss);
            }
            
            checkBulletsVsEnemies(&p1, minions, MAX_MINIONS,
                                   &boss, bossActive);
            if (dispMode == MODE_MULTI)
                checkBulletsVsEnemies(&p2, minions, MAX_MINIONS,
                                       &boss, bossActive);

            /* player death → save score → back to menu */
            int gameOver = !p1.isAlive || (dispMode == MODE_MULTI && !p2.isAlive);
            if (gameOver) {
                SDL_RenderPresent(ren);
                SDL_Delay(1000);
                char nom[MAX_NAME_LEN] = {0};
                char nom2[MAX_NAME_LEN] = {0};
                
                /* In multiplayer, save both player scores */
                if (dispMode == MODE_MULTI) {
                    saisirNomJoueur(ren, nom);
                    sauvegarderScore(&bg, nom, p1.score, currentLevel);
                    
                    /* Clear screen and ask for P2's name */
                    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
                    SDL_RenderClear(ren);
                    SDL_RenderPresent(ren);
                    SDL_Delay(500);
                    
                    saisirNomJoueur(ren, nom2);
                    sauvegarderScore(&bg, nom2, p2.score, currentLevel);
                } else {
                    saisirNomJoueur(ren, nom);
                    sauvegarderScore(&bg, nom, p1.score, currentLevel);
                }
                
                afficherMeilleursScores(&bg, ren);
                state = APP_MAIN_MENU;
                if (au && au->music) Mix_PlayMusic(au->music, -1);
                initialiserJoueur(&p1, ren, PLAYER_1, 150.0f, startY);
                initialiserJoueur(&p2, ren, PLAYER_2, 250.0f, startY);
                resetEnemies(minions, MAX_MINIONS, &boss,
                             &bossSpawned, &bossActive,
                             &spawnTimer, ren, currentLevel);
                continue;
            }
        }

        /* ── RENDER ── */
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
        SDL_RenderClear(ren);

        if (state == APP_MAIN_MENU) {
            /* matrix rain background */
            matrix_render(ren, SCREEN_WIDTH);
            /* title logo */
            if (titleTex) SDL_RenderCopy(ren, titleTex, NULL, &titleRect);
            /* menu buttons */
            for (int i = 0; i < BTN_COUNT; i++)
                tbtnDraw(ren, &menuBtns[i]);
        }

        else if (state == APP_MODE_SELECT) {
            /* matrix rain as background */
            matrix_render(ren, SCREEN_WIDTH);
            /* dark overlay */
            SDL_SetRenderDrawColor(ren, 0, 0, 0, 140);
            SDL_Rect full = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
            SDL_RenderFillRect(ren, &full);
            /* bitmap title */
            drawTextCentered(ren, 140, 0, SCREEN_WIDTH,
                             "MODE DE JEU", 3, 0, 255, 80);
            /* selected mode indicator */
            const char *modeLbl = (dispMode == MODE_MONO)
                                  ? "Selectionne : MONO" : "Selectionne : MULTI";
            drawTextCentered(ren, 200, 0, SCREEN_WIDTH,
                             modeLbl, 2, 0, 200, 60);
            bbtnDraw(ren, &btnMono);
            bbtnDraw(ren, &btnMulti);
            bbtnDraw(ren, &btnValider);
            bbtnDraw(ren, &btnRetour);
        }

        else if (state == APP_OPTION) {
            renderOption(ren, fontSmall, &backBtn, mx, my, musicVol, au);
        }

        else if (state == APP_SCORES) {
            SDL_SetRenderDrawColor(ren, 0, 6, 3, 255);
            SDL_Rect full = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
            SDL_RenderFillRect(ren, &full);
            drawTextCentered(ren, 40, 0, SCREEN_WIDTH, "MEILLEURS SCORES", 3, 0, 255, 70);
            drawText(ren, 60, 110, "# NOM                SCORE NIV TEMPS", 1, 220, 180, 0);
            SDL_SetRenderDrawColor(ren, 0, 200, 50, 255);
            SDL_RenderDrawLine(ren, 60, 126, SCREEN_WIDTH - 60, 126);
            if (bg.scoreCount == 0)
                drawTextCentered(ren, 200, 0, SCREEN_WIDTH, "AUCUN SCORE ENREGISTRE", 2, 0, 140, 35);
            for (int s = 0; s < bg.scoreCount && s < MAX_SCORES; s++) {
                Score *sc = &bg.scores[s];
                char sline[80];
                snprintf(sline, sizeof(sline), "%-3d %-20s %5d  %2d %02d:%02d",
                         s+1, sc->name, sc->score, sc->level,
                         sc->time / 60, sc->time % 60);
                Uint8 sr = (Uint8)((s==0)?220:180);
                Uint8 sg = (Uint8)((s==0)?180:255);
                Uint8 sb2 = (Uint8)((s==0)?0:180);
                drawText(ren, 60, 140 + s*22, sline, 1, sr, sg, sb2);
            }
            drawTextCentered(ren, SCREEN_HEIGHT - 80, 0, SCREEN_WIDTH,
                             "APPUYEZ SUR UNE TOUCHE OU RETOUR", 1, 0, 140, 35);
            backBtn.hover = tbtnHit(&backBtn, mx, my);
            tbtnDraw(ren, &backBtn);
        }

        else if (state == APP_HISTOIRE) {
            renderHistoire(ren, fontSmall, &backBtn, mx, my);
        }

        else if (state == APP_SAVE_SCREEN) {
            SDL_SetRenderDrawColor(ren, 0, 6, 3, 255);
            SDL_Rect full = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
            SDL_RenderFillRect(ren, &full);
            drawTextCentered(ren, 40, 0, SCREEN_WIDTH, "MEILLEURS SCORES", 3, 0, 255, 70);
            drawText(ren, 60, 110, "# NOM                SCORE NIV TEMPS", 1, 220, 180, 0);
            SDL_SetRenderDrawColor(ren, 0, 200, 50, 255);
            SDL_RenderDrawLine(ren, 60, 126, SCREEN_WIDTH - 60, 126);
            if (bg.scoreCount == 0)
                drawTextCentered(ren, 200, 0, SCREEN_WIDTH, "AUCUN SCORE ENREGISTRE", 2, 0, 140, 35);
            for (int s = 0; s < bg.scoreCount && s < MAX_SCORES; s++) {
                Score *sc = &bg.scores[s];
                char sline[80];
                snprintf(sline, sizeof(sline), "%-3d %-20s %5d  %2d %02d:%02d",
                         s+1, sc->name, sc->score, sc->level,
                         sc->time / 60, sc->time % 60);
                Uint8 sr = (Uint8)((s==0)?220:180);
                Uint8 sg = (Uint8)((s==0)?180:255);
                Uint8 sb2 = (Uint8)((s==0)?0:180);
                drawText(ren, 60, 140 + s*22, sline, 1, sr, sg, sb2);
            }
            backBtn.hover = tbtnHit(&backBtn, mx, my);
            tbtnDraw(ren, &backBtn);
        }

        else if (state == APP_GAME) {
            if (dispMode == MODE_MULTI) {
                SDL_Rect vp1 = {0, 0, SCREEN_WIDTH/2, SCREEN_HEIGHT};
                renderHalf(ren, &bg, &p1, &p2, vp1,
                           minions, MAX_MINIONS, &boss, bossActive);
                SDL_Rect vp2 = {SCREEN_WIDTH/2, 0,
                                SCREEN_WIDTH/2, SCREEN_HEIGHT};
                renderHalf(ren, &bg, &p2, &p1, vp2,
                           minions, MAX_MINIONS, &boss, bossActive);
                /* Draw separator between the two halves */
                SDL_RenderSetViewport(ren, NULL);
                SDL_RenderSetClipRect(ren, NULL);
                SDL_SetRenderDrawColor(ren, 0, 255, 70, 255);
                SDL_RenderDrawLine(ren, SCREEN_WIDTH/2 - 1, 0, SCREEN_WIDTH/2 - 1, SCREEN_HEIGHT);
                SDL_RenderDrawLine(ren, SCREEN_WIDTH/2,     0, SCREEN_WIDTH/2,     SCREEN_HEIGHT);
                SDL_RenderDrawLine(ren, SCREEN_WIDTH/2 + 1, 0, SCREEN_WIDTH/2 + 1, SCREEN_HEIGHT);
            } else {
                afficherBackground(&bg, ren, MODE_MONO, NULL);
                for (int i = 0; i < MAX_MINIONS; i++)
                    if (minions[i].alive)
                        render_enemy(&minions[i], ren, (int)bg.camX);
                if (bossActive)
                    render_enemy(&boss, ren, (int)bg.camX);
                afficherJoueur(&p1, ren, bg.camX, bg.camY);
                afficherBalles(&p1, ren, bg.camX, bg.camY);
                afficherHUDJoueur(&p1, ren, 10, 10);
                afficherTemps(&bg, ren, SCREEN_WIDTH - 140, 10);
                afficherGuide(&bg, ren);
                afficherNotification(&bg, ren);
                renderMinimap(&mm, ren, &p1, &p2, 0,
                              minions, MAX_MINIONS, &boss, bossActive);
            }
        }

        SDL_RenderPresent(ren);

        /* frame cap */
        Uint32 ft = SDL_GetTicks() - fs;
        if (ft < FRAME_MS) SDL_Delay(FRAME_MS - ft);
    }

    /* ── CLEANUP ── */
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
    if (fontSmall != fontMain) TTF_CloseFont(fontSmall);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    TTF_Quit(); IMG_Quit(); SDL_Quit();
    return 0;
}
