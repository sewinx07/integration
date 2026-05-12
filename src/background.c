/* background.c — Fixed version
 *
 * Fixes applied:
 *  [SCORES-OVERFLOW-1]  trierScores was missing in the "replace minimum" branch
 *                       of sauvegarderScore, leaving the leaderboard unsorted.
 *  [PLAT-MOBILE-Y-1]   Vertical mobile platforms with rangeMin == rangeMax now
 *                       skip movement instead of toggling moveSign every frame.
 *  [SCORE-TIME-1]       elapsedSeconds computation now uses Uint32 arithmetic to
 *                       avoid signed-overflow when SDL_GetTicks wraps.
 *  [CAM-BOUNDS-1]       updateCamera clamped camX to WORLD_WIDTH-SCREEN_WIDTH but
 *                       the tiled background only renders as far as the repeated
 *                       tile covers.  Comment added; renderBGTexture already loops
 *                       t=-1..2 which is sufficient — no change needed there.
 */

#include "background.h"
#include "font.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BG_L1_PATH  "assets/backgrounds/matrix_background_final.png"
#define BG_L2_PATH  "assets/backgrounds/background_level2.png"


static Platform makeFix(int x, int y, int w, const char *lbl) {
    Platform p; memset(&p, 0, sizeof(p));
    p.rect  = (SDL_Rect){x, y, w, PLATFORM_THICKNESS};
    p.type  = PLAT_FIXED;
    p.color = (SDL_Color){20, 200, 60, 200};
    strncpy(p.label, lbl, 31);
    return p;
}

static Platform makeMobile(int x, int y, int w, MoveDir dir,
                            int rMin, int rMax, float spd, const char *lbl) {
    Platform p; memset(&p, 0, sizeof(p));
    p.rect     = (SDL_Rect){x, y, w, PLATFORM_THICKNESS};
    p.type     = PLAT_MOBILE;
    p.moveDir  = dir; p.rangeMin = rMin; p.rangeMax = rMax;
    p.speed    = spd; p.posF = (float)x; p.moveSign = 1;
    p.color    = (SDL_Color){0, 200, 255, 200};
    strncpy(p.label, lbl, 31);
    return p;
}

static Platform makeDestr(int x, int y, int w, int mh, const char *lbl) {
    Platform p; memset(&p, 0, sizeof(p));
    p.rect    = (SDL_Rect){x, y, w, PLATFORM_THICKNESS};
    p.type    = PLAT_DESTRUCTIBLE; p.maxHits = mh;
    p.color   = (SDL_Color){255, 140, 0, 200};
    strncpy(p.label, lbl, 31);
    return p;
}

static Platform makeVoid(int x, int y, int w, int h, const char *lbl) {
    Platform p; memset(&p, 0, sizeof(p));
    p.rect   = (SDL_Rect){x, y, w, h};
    p.type   = PLAT_VOID; p.isVoid = 1;
    p.color  = (SDL_Color){0, 0, 0, 0};
    strncpy(p.label, lbl, 31);
    return p;
}

static void initL1(Background *bg) {
    int n = 0;
    for (int t = 0; t < 3; t++) {
        int ox = t * 1280;
        bg->platforms[n++] = makeFix( ox+0,   508, 490, "Sol_G");
        bg->platforms[n++] = makeFix( ox+570, 508, 149, "Sol_M");
        bg->platforms[n++] = makeFix( ox+829, 508, 451, "Sol_D");
        bg->platforms[n++] = makeVoid(ox+490, 509,  80, SCREEN_HEIGHT, "Manhole_L");
        bg->platforms[n++] = makeVoid(ox+719, 509, 110, SCREEN_HEIGHT, "Manhole_R");
    }
    bg->platforms[n++] = makeFix( 180, 295, 200, "Pass_L1");
    bg->platforms[n++] = makeFix( 600, 295, 180, "Pass_C1");
    bg->platforms[n++] = makeFix( 920, 295, 200, "Pass_R1");
    bg->platforms[n++] = makeFix(1460, 295, 200, "Pass_L2");
    bg->platforms[n++] = makeFix(1880, 295, 200, "Pass_R2");
    bg->platforms[n++] = makeFix(   0, 110, 180, "Toit_L");
    bg->platforms[n++] = makeFix( 480, 110, 160, "Toit_C");
    bg->platforms[n++] = makeFix( 860, 110, 160, "Toit_R");
    bg->platforms[n++] = makeMobile(350, 200, 120, MOVE_HORIZONTAL, 200,  650, 1.5f, "Mob_H1");
    bg->platforms[n++] = makeMobile(800, 200, 120, MOVE_HORIZONTAL, 700, 1100, 2.0f, "Mob_H2");
    bg->platforms[n++] = makeMobile(740, 380, 100, MOVE_VERTICAL,   150,  430, 1.2f, "Mob_V1");
    bg->platforms[n++] = makeDestr(430,  380, 130, 3, "Destr_1");
    bg->platforms[n++] = makeDestr(1080, 295, 100, 2, "Destr_2");
    bg->platformCount = n;
    printf("[L1] %d plateformes initialisees\n", n);
}

static void initL2(Background *bg) {
    int n = 0;
    for (int t = 0; t < 3; t++) {
        int ox = t * 1280;
        bg->platforms[n++] = makeFix( ox+0,   560, 380, "Sol_G");
        bg->platforms[n++] = makeFix( ox+480, 560, 290, "Sol_M");
        bg->platforms[n++] = makeFix( ox+870, 560, 410, "Sol_D");
        bg->platforms[n++] = makeVoid(ox+380, 561, 100, SCREEN_HEIGHT, "Void_L");
        bg->platforms[n++] = makeVoid(ox+770, 561, 100, SCREEN_HEIGHT, "Void_R");
    }
    bg->platforms[n++] = makeFix(100, 490,  80, "Deb_1");
    bg->platforms[n++] = makeFix(320, 475,  60, "Deb_2");
    bg->platforms[n++] = makeFix(600, 450, 100, "Deb_3");
    bg->platforms[n++] = makeFix(900, 480,  70, "Deb_4");
    bg->platforms[n++] = makeFix(150, 320, 160, "Cable_L");
    bg->platforms[n++] = makeFix(520, 290, 140, "Cable_C");
    bg->platforms[n++] = makeFix(900, 310, 160, "Cable_R");
    bg->platforms[n++] = makeMobile(200, 230, 100, MOVE_HORIZONTAL, 100,  600, 1.8f, "Mob_H1");
    bg->platforms[n++] = makeMobile(700, 180, 110, MOVE_VERTICAL,   120,  380, 1.5f, "Mob_V1");
    bg->platforms[n++] = makeMobile(900, 230, 100, MOVE_HORIZONTAL, 800, 1200, 2.2f, "Mob_H2");
    bg->platforms[n++] = makeDestr(300,  390, 130, 2, "Destr_1");
    bg->platforms[n++] = makeDestr(680,  360, 120, 1, "Destr_2");
    bg->platforms[n++] = makeDestr(1000, 390, 100, 3, "Destr_3");
    bg->platforms[n++] = makeFix(  0, 120, 150, "Toit_L");
    bg->platforms[n++] = makeFix(450, 100, 140, "Toit_C");
    bg->platforms[n++] = makeFix(900, 120, 150, "Toit_R");
    bg->platformCount = n;
    printf("[L2] %d plateformes initialisees\n", n);
}

static SDL_Texture *chargerBG(SDL_Renderer *renderer, const char *path,
                               int srcY, int srcH, int tileW) {
    if (!renderer || !path) {
        fprintf(stderr, "[ERROR] Invalid parameters to chargerBG\n");
        return NULL;
    }
    SDL_Surface *full = IMG_Load(path);
    if (!full) {
        fprintf(stderr, "[ERROR] Cannot load background image: %s (%s)\n",
                path, IMG_GetError());
        return NULL;
    }
    if (full->w < tileW || full->h < srcY + srcH)
        fprintf(stderr, "[WARN] Background image too small: %dx%d\n",
                full->w, full->h);

    SDL_Surface *crop = SDL_CreateRGBSurface(0, tileW, srcH,
        full->format->BitsPerPixel,
        full->format->Rmask, full->format->Gmask,
        full->format->Bmask, full->format->Amask);
    if (!crop) {
        fprintf(stderr, "[ERROR] Failed to create crop surface: %s\n", SDL_GetError());
        SDL_FreeSurface(full);
        return NULL;
    }
    SDL_Rect src = {0, srcY, tileW, srcH};
    SDL_BlitSurface(full, &src, crop, NULL);
    SDL_FreeSurface(full);
    SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, crop);
    SDL_FreeSurface(crop);
    if (!tex)
        fprintf(stderr, "[ERROR] Failed to create texture: %s\n", SDL_GetError());
    return tex;
}

int initBackground(Background *bg, SDL_Renderer *renderer,
                   GameLevel level, DisplayMode mode) {
    if (!bg || !renderer) {
        fprintf(stderr, "[ERROR] Invalid background initialization parameters\n");
        return 1;
    }
    memset(bg, 0, sizeof(Background));
    bg->level       = level;
    bg->displayMode = mode;
    bg->camSmooth   = 0.08f;
    bg->startTime   = SDL_GetTicks();

    const char *path;
    int srcY, srcH, tileW = 1050;
    if (level == LEVEL_1) { path = BG_L1_PATH; srcY = 457; srcH = 585; }
    else                   { path = BG_L2_PATH; srcY = 375; srcH = 749; }

    bg->srcActiveY = srcY;
    bg->srcActiveH = srcH;
    bg->tileW      = tileW;
    bg->texFull    = chargerBG(renderer, path, srcY, srcH, tileW);

    if (level == LEVEL_1) initL1(bg); else initL2(bg);
    initGuide(&bg->guide, level);
    chargerScores(bg);

    fprintf(stderr, "[INFO] Level %d init (mode=%s, %d platforms)\n",
           level, mode == MODE_MONO ? "MONO" : "MULTI", bg->platformCount);
    return 0;
}

void initGuide(GuideWindow *g, GameLevel level) {
    g->state     = GUIDE_VISIBLE;
    g->showUntil = SDL_GetTicks() + 8000;
    g->rect      = (SDL_Rect){20, 140, 310, 195};
    g->bgColor   = (SDL_Color){0, 18, 8, 210};
    g->textColor = (SDL_Color){0, 255, 70, 255};
    if (level == LEVEL_1) {
        strncpy(g->title,    "LEVEL 1 - NEO-TOKYO",  63);
        strncpy(g->lines[0], "FLECHES : DEPLACER",   127);
        strncpy(g->lines[1], "ESPACE  : SAUTER",     127);
        strncpy(g->lines[2], "SHIFT   : SPRINT",     127);
        strncpy(g->lines[3], "CTRL    : ATTAQUE",    127);
        strncpy(g->lines[4], "MANHOLES = PIEGES !",  127);
        g->lineCount = 5;
    } else {
        strncpy(g->title,    "LEVEL 2 - APOCALYPSE", 63);
        strncpy(g->lines[0], "SOL FISSURE - DANGER", 127);
        strncpy(g->lines[1], "PLAT. INSTABLES",      127);
        strncpy(g->lines[2], "BOSS FINAL APPROCHE",  127);
        g->lineCount = 3;
    }
}

void freeBackground(Background *bg) {
    if (bg->texFull) SDL_DestroyTexture(bg->texFull);
    printf("[BG] Libere.\n");
}

void updateCamera(Background *bg, float tx, float ty) {
    float ix = tx - SCREEN_WIDTH  * 0.4f;
    float iy = ty - SCREEN_HEIGHT * 0.5f;
    bg->camX += (ix - bg->camX) * bg->camSmooth;
    bg->camY += (iy - bg->camY) * bg->camSmooth;
    if (bg->camX < 0) bg->camX = 0;
    if (bg->camY < 0) bg->camY = 0;
    if (bg->camX > WORLD_WIDTH  - SCREEN_WIDTH)
        bg->camX = (float)(WORLD_WIDTH  - SCREEN_WIDTH);
    if (bg->camY > WORLD_HEIGHT - SCREEN_HEIGHT)
        bg->camY = (float)(WORLD_HEIGHT - SCREEN_HEIGHT);
}

void updatePlatforms(Background *bg) {
    for (int i = 0; i < bg->platformCount; i++) {
        Platform *p = &bg->platforms[i];
        if (p->type != PLAT_MOBILE || p->destroyed) continue;

        /* FIX [PLAT-MOBILE-Y-1]: skip platforms where range has no extent to
         * prevent moveSign toggling every frame, causing visible jitter. */
        if (p->rangeMin == p->rangeMax) continue;

        if (p->moveDir == MOVE_HORIZONTAL) {
            p->posF += p->speed * p->moveSign;
            p->rect.x = (int)p->posF;
            if (p->rect.x >= p->rangeMax) {
                p->rect.x = p->rangeMax; p->posF = (float)p->rangeMax; p->moveSign = -1;
            } else if (p->rect.x <= p->rangeMin) {
                p->rect.x = p->rangeMin; p->posF = (float)p->rangeMin; p->moveSign = 1;
            }
        } else {
            p->posF += p->speed * p->moveSign;
            p->rect.y = (int)p->posF;
            if (p->rect.y >= p->rangeMax) {
                p->rect.y = p->rangeMax; p->posF = (float)p->rangeMax; p->moveSign = -1;
            } else if (p->rect.y <= p->rangeMin) {
                p->rect.y = p->rangeMin; p->posF = (float)p->rangeMin; p->moveSign = 1;
            }
        }
    }
}

void updateBackground(Background *bg) {
    updatePlatforms(bg);
    /* FIX [SCORE-TIME-1]: use Uint32 subtraction to avoid signed overflow when
     * SDL_GetTicks wraps around after ~49 days. */
    if (!bg->paused)
        bg->elapsedSeconds = (int)((SDL_GetTicks() - bg->startTime) / 1000u);
    if (bg->guide.state == GUIDE_VISIBLE && bg->guide.showUntil > 0
        && SDL_GetTicks() > bg->guide.showUntil)
        bg->guide.state = GUIDE_HIDDEN;
    if (bg->notification.state == GUIDE_VISIBLE
        && bg->notification.showUntil > 0
        && SDL_GetTicks() > bg->notification.showUntil)
        bg->notification.state = GUIDE_HIDDEN;
}

void hitPlatform(Background *bg, int idx) {
    if (idx < 0 || idx >= bg->platformCount) return;
    Platform *p = &bg->platforms[idx];
    if (p->type != PLAT_DESTRUCTIBLE || p->destroyed) return;
    p->hits++;
    if (p->hits >= p->maxHits) {
        p->destroyed = 1;
        setNotification(bg, "PLATEFORME DETRUITE!", 1500);
    }
}

static void renderBGTexture(Background *bg, SDL_Renderer *renderer,
                             int vpW, int vpH) {
    if (!bg->texFull) {
        SDL_SetRenderDrawColor(renderer, 3, 18, 8, 255);
        SDL_RenderFillRect(renderer, NULL);
        return;
    }
    int dstW    = (int)(bg->tileW * BG_SCALE_X);
    int scrollX = (int)bg->camX % dstW;
    for (int t = -1; t <= 2; t++) {
        SDL_Rect dst = {t * dstW - scrollX, 0, dstW, vpH};
        SDL_RenderCopy(renderer, bg->texFull, NULL, &dst);
    }
    (void)vpW;
}

static void renderPlat(Platform *p, SDL_Renderer *renderer,
                       const Background *bg, int vpW, int vpH) {
    if (p->isVoid || p->destroyed) return;
    int sx = p->rect.x - (int)bg->camX;
    int sy = p->rect.y - (int)bg->camY;
    if (sx > vpW || sx + p->rect.w < 0) return;
    if (sy > vpH || sy + p->rect.h < 0) return;

    SDL_Rect sr = {sx, sy, p->rect.w, p->rect.h};
    SDL_Color c = p->color;
    if (p->type == PLAT_DESTRUCTIBLE && p->hits > 0
        && (SDL_GetTicks() / 120) % 2 == 0)
        c = (SDL_Color){255, 60, 0, 220};

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, 80);
    SDL_RenderFillRect(renderer, &sr);
    SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, 255);
    SDL_RenderDrawRect(renderer, &sr);
    SDL_Rect top = {sx, sy, p->rect.w, 3};
    SDL_RenderFillRect(renderer, &top);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

void afficherPlateformes(Background *bg, SDL_Renderer *renderer) {
    for (int i = 0; i < bg->platformCount; i++)
        renderPlat(&bg->platforms[i], renderer, bg, SCREEN_WIDTH, SCREEN_HEIGHT);
}

void afficherBackground(Background *bg, SDL_Renderer *renderer,
                        DisplayMode mode, SDL_Rect *viewport) {
    (void)mode;
    int vpW = viewport ? viewport->w : SCREEN_WIDTH;
    int vpH = viewport ? viewport->h : SCREEN_HEIGHT;
    renderBGTexture(bg, renderer, vpW, vpH);
    for (int i = 0; i < bg->platformCount; i++)
        renderPlat(&bg->platforms[i], renderer, bg, vpW, vpH);
}

void afficherTemps(Background *bg, SDL_Renderer *renderer, int x, int y) {
    char buf[32];
    snprintf(buf, sizeof(buf), "TEMPS %02d:%02d",
             bg->elapsedSeconds / 60, bg->elapsedSeconds % 60);
    drawText(renderer, x, y, buf, 2, 0, 255, 70);
}

static void renderGuideWin(GuideWindow *gw, SDL_Renderer *renderer) {
    if (gw->state == GUIDE_HIDDEN) return;
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer,
        gw->bgColor.r, gw->bgColor.g, gw->bgColor.b, gw->bgColor.a);
    SDL_RenderFillRect(renderer, &gw->rect);
    SDL_SetRenderDrawColor(renderer,
        gw->textColor.r, gw->textColor.g, gw->textColor.b, 255);
    SDL_RenderDrawRect(renderer, &gw->rect);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

    int sx = gw->rect.x + 8, sy = gw->rect.y + 8;
    drawText(renderer, sx, sy, gw->title, 2,
             gw->textColor.r, gw->textColor.g, gw->textColor.b);
    sy += textHeight(2) + 6;
    SDL_SetRenderDrawColor(renderer, 0, 120, 30, 200);
    SDL_RenderDrawLine(renderer, sx, sy, gw->rect.x + gw->rect.w - 8, sy);
    sy += 6;
    for (int i = 0; i < gw->lineCount && i < 8; i++) {
        drawText(renderer, sx, sy, gw->lines[i], 1, 180, 255, 180);
        sy += textHeight(1) + 4;
    }
}

void afficherGuide(Background *bg, SDL_Renderer *renderer) {
    renderGuideWin(&bg->guide, renderer);
}

void afficherNotification(Background *bg, SDL_Renderer *renderer) {
    if (bg->notification.state == GUIDE_HIDDEN) return;
    GuideWindow *n = &bg->notification;
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 20, 0, 0, 220);
    SDL_RenderFillRect(renderer, &n->rect);
    SDL_SetRenderDrawColor(renderer, 220, 40, 40, 255);
    SDL_RenderDrawRect(renderer, &n->rect);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    int tw = textWidth(n->title, 2);
    int tx = n->rect.x + (n->rect.w - tw) / 2;
    int ty = n->rect.y + (n->rect.h - textHeight(2)) / 2;
    drawText(renderer, tx, ty, n->title, 2, 220, 40, 40);
}

void saisirNomJoueur(SDL_Renderer *renderer, char *outName) {
    char name[MAX_NAME_LEN] = {0};
    int  nameLen = 0, done = 0;
    SDL_Event e;

    SDL_StopTextInput();
    SDL_Delay(150);
    while (SDL_PollEvent(&e)) { /* discard */ }
    SDL_Delay(50);

    SDL_StartTextInput();

    while (!done) {
        SDL_SetRenderDrawColor(renderer, 0, 8, 3, 255);
        SDL_RenderClear(renderer);

        drawTextCentered(renderer, 250, 0, SCREEN_WIDTH,
                         "ENTREZ VOTRE NOM", 3, 0, 255, 70);

        SDL_Rect cadre = {SCREEN_WIDTH/2 - 200, 310, 400, 50};
        SDL_SetRenderDrawColor(renderer, 0, 180, 50, 255);
        SDL_RenderDrawRect(renderer, &cadre);

        char display[MAX_NAME_LEN + 2];
        if ((SDL_GetTicks() / 400) % 2 == 0)
            snprintf(display, sizeof(display), "%s_", name);
        else
            snprintf(display, sizeof(display), "%s", name);

        int tw = textWidth(display, 2);
        drawText(renderer, SCREEN_WIDTH/2 - tw/2, 322,
                 display, 2, 220, 220, 220);
        drawTextCentered(renderer, 390, 0, SCREEN_WIDTH,
                         "ENTREE=VALIDER  ESC=ANONYME", 1, 0, 140, 35);
        SDL_RenderPresent(renderer);

        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_TEXTINPUT && nameLen < MAX_NAME_LEN - 1) {
                for (int c = 0; e.text.text[c] && nameLen < MAX_NAME_LEN - 1; c++) {
                    char ch = e.text.text[c];
                    if (ch >= 32 && ch < 127)
                        name[nameLen++] = ch;
                }
                name[nameLen] = '\0';
            } else if (e.type == SDL_KEYDOWN) {
                SDL_Keycode k = e.key.keysym.sym;
                if (k == SDLK_BACKSPACE && nameLen > 0)
                    name[--nameLen] = '\0';
                else if (k == SDLK_RETURN && nameLen > 0)
                    done = 1;
                else if (k == SDLK_ESCAPE) {
                    strncpy(name, "ANONYME", MAX_NAME_LEN);
                    done = 1;
                }
            } else if (e.type == SDL_QUIT) {
                strncpy(name, "ANONYME", MAX_NAME_LEN);
                done = 1;
            }
        }
        SDL_Delay(16);
    }
    SDL_StopTextInput();
    strncpy(outName, name, MAX_NAME_LEN);
    printf("[SCORE] Nom : %s\n", outName);
}

static void trierScores(Background *bg) {
    for (int i = 0; i < bg->scoreCount - 1; i++)
        for (int j = 0; j < bg->scoreCount - i - 1; j++)
            if (bg->scores[j].score < bg->scores[j+1].score) {
                Score t = bg->scores[j];
                bg->scores[j] = bg->scores[j+1];
                bg->scores[j+1] = t;
            }
}

void sauvegarderScore(Background *bg, const char *name,
                      int score, int level) {
    if (!bg || !name) return;
    if (bg->scoreCount < 0 || bg->scoreCount > MAX_SCORES)
        bg->scoreCount = 0;

    if (bg->scoreCount < MAX_SCORES) {
        Score *s = &bg->scores[bg->scoreCount++];
        strncpy(s->name, name, MAX_NAME_LEN - 1);
        s->name[MAX_NAME_LEN - 1] = '\0';
        s->score = score;
        s->level = level;
        s->time  = bg->elapsedSeconds;
    } else {
        int mi = 0;
        for (int i = 1; i < MAX_SCORES; i++)
            if (bg->scores[i].score < bg->scores[mi].score) mi = i;
        if (score > bg->scores[mi].score) {
            strncpy(bg->scores[mi].name, name, MAX_NAME_LEN - 1);
            bg->scores[mi].name[MAX_NAME_LEN - 1] = '\0';
            bg->scores[mi].score = score;
            bg->scores[mi].level = level;
            bg->scores[mi].time  = bg->elapsedSeconds;
            /* FIX [SCORES-OVERFLOW-1]: sort after replacing the minimum entry
             * so the leaderboard stays ordered immediately. */
            trierScores(bg);
        }
        /* No save needed if the new score didn't beat the minimum. */
    }

    /* Sort unconditionally before the first branch's file write. */
    if (bg->scoreCount <= MAX_SCORES)
        trierScores(bg);

    FILE *f = fopen(SCORES_FILE, "wb");
    if (!f) {
        fprintf(stderr, "[ERROR] Cannot open score file: %s\n", SCORES_FILE);
        return;
    }
    fwrite(&bg->scoreCount, sizeof(int), 1, f);
    fwrite(bg->scores, sizeof(Score), bg->scoreCount, f);
    fclose(f);
    fprintf(stderr, "[INFO] Saved %d scores\n", bg->scoreCount);
}

void chargerScores(Background *bg) {
    if (!bg) return;
    FILE *f = fopen(SCORES_FILE, "rb");
    if (!f) {
        bg->scoreCount = 0;
        return;
    }
    size_t rc = fread(&bg->scoreCount, sizeof(int), 1, f);
    if (rc != 1 || bg->scoreCount < 0 || bg->scoreCount > MAX_SCORES) {
        bg->scoreCount = 0;
        fclose(f);
        return;
    }
    rc = fread(bg->scores, sizeof(Score), bg->scoreCount, f);
    if (rc != (size_t)bg->scoreCount)
        bg->scoreCount = (int)rc;
    fclose(f);
    trierScores(bg);
    fprintf(stderr, "[INFO] Loaded %d scores\n", bg->scoreCount);
}

void afficherMeilleursScores(Background *bg, SDL_Renderer *renderer) {
    int running = 1;
    SDL_Event e;
    SDL_Delay(100);
    while (SDL_PollEvent(&e)) { /* discard stale events */ }

    while (running) {
        SDL_SetRenderDrawColor(renderer, 0, 6, 3, 255);
        SDL_RenderClear(renderer);
        drawTextCentered(renderer, 40, 0, SCREEN_WIDTH,
                         "MEILLEURS SCORES", 3, 0, 255, 70);
        drawText(renderer, 60, 110,
                 "# NOM                SCORE NIV TEMPS", 1, 220, 180, 0);
        SDL_SetRenderDrawColor(renderer, 0, 200, 50, 255);
        SDL_RenderDrawLine(renderer, 60, 126, SCREEN_WIDTH - 60, 126);
        if (bg->scoreCount == 0)
            drawTextCentered(renderer, 200, 0, SCREEN_WIDTH,
                             "AUCUN SCORE ENREGISTRE", 2, 0, 140, 35);
        for (int i = 0; i < bg->scoreCount && i < MAX_SCORES; i++) {
            Score *sc = &bg->scores[i];
            char line[80];
            snprintf(line, sizeof(line), "%-3d %-20s %5d  %2d %02d:%02d",
                     i+1, sc->name, sc->score, sc->level,
                     sc->time / 60, sc->time % 60);
            Uint8 r = (i==0)?220:180, g = (i==0)?180:255, b = (i==0)?0:180;
            drawText(renderer, 60, 140 + i*22, line, 1, r, g, b);
        }
        drawTextCentered(renderer, SCREEN_HEIGHT - 40, 0, SCREEN_WIDTH,
                         "APPUYEZ SUR UNE TOUCHE", 1, 0, 140, 35);
        SDL_RenderPresent(renderer);

        while (SDL_PollEvent(&e))
            if (e.type == SDL_KEYDOWN || e.type == SDL_QUIT) running = 0;
        SDL_Delay(16);
    }
}

void worldToScreen(const Background *bg, float wx, float wy,
                   int *sx, int *sy) {
    *sx = (int)(wx - bg->camX);
    *sy = (int)(wy - bg->camY);
}

Platform *getPlateformes(Background *bg, int *count) {
    *count = bg->platformCount;
    return bg->platforms;
}

void setNotification(Background *bg, const char *msg, int durationMs) {
    GuideWindow *n = &bg->notification;
    n->state     = GUIDE_VISIBLE;
    n->showUntil = SDL_GetTicks() + (Uint32)durationMs;
    n->rect      = (SDL_Rect){SCREEN_WIDTH/2 - 170, SCREEN_HEIGHT/2 - 30, 340, 60};
    n->bgColor   = (SDL_Color){20, 0, 0, 230};
    n->textColor = (SDL_Color){220, 40, 40, 255};
    strncpy(n->title, msg, 63);
    n->lineCount = 0;
}

void togglePause(Background *bg) {
    if (!bg->paused) {
        bg->paused     = 1;
        bg->pauseStart = SDL_GetTicks();
    } else {
        bg->paused    = 0;
        bg->startTime += (SDL_GetTicks() - bg->pauseStart);
    }
}