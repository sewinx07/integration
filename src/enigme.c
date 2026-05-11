/**
 * enigme.c — QCM + Puzzle mini-game for MATRIX GAME.
 *
 * Visual style matches the main game exactly:
 *   - Background  : matrix_render() called by main.c before each enigme
 *                   render, plus a dark overlay (0,0,0,140) drawn here.
 *   - Buttons     : same tbtnDraw style — black fill (0,0,0,180),
 *                   green fill (57,255,20,120) on hover, single green
 *                   border (57,255,20,255), white TTF text centred.
 *
 * Assets expected (all optional — fallback text drawn if missing):
 *   assets/enigme/success.png
 *   assets/enigme/fail.png
 *   assets/enigme/questions.txt
 *
 * questions.txt format (one per line):
 *   Question text|Rep1|Rep2|Rep3|Rep4|CorrectIndex(1-based)
 */

#include "enigme.h"
#include "font.h"       /* drawText / drawTextCentered / textWidth */
#include "common.h"     /* SCREEN_WIDTH / SCREEN_HEIGHT            */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ── internal helpers ───────────────────────────────────────────── */

static SDL_Texture *loadTex(SDL_Renderer *ren, const char *path)
{
    SDL_Texture *t = IMG_LoadTexture(ren, path);
    if (!t)
        fprintf(stderr, "[ENIGME] Cannot load %s: %s\n", path, IMG_GetError());
    return t;
}

/* Render a single line of TTF text centred at (cx, y). */
static void ttfCentre(SDL_Renderer *ren, TTF_Font *font,
                       const char *txt, SDL_Color col, int cx, int y)
{
    if (!font || !txt || !txt[0]) return;
    SDL_Surface *s = TTF_RenderUTF8_Blended(font, txt, col);
    if (!s) return;
    SDL_Texture *t = SDL_CreateTextureFromSurface(ren, s);
    SDL_Rect r = { cx - s->w / 2, y, s->w, s->h };
    SDL_RenderCopy(ren, t, NULL, &r);
    SDL_DestroyTexture(t);
    SDL_FreeSurface(s);
}

/* Render TTF text left-aligned at (x, y). */
static void ttfAt(SDL_Renderer *ren, TTF_Font *font,
                   const char *txt, SDL_Color col, int x, int y)
{
    if (!font || !txt || !txt[0]) return;
    SDL_Surface *s = TTF_RenderUTF8_Blended(font, txt, col);
    if (!s) return;
    SDL_Texture *t = SDL_CreateTextureFromSurface(ren, s);
    SDL_Rect r = { x, y, s->w, s->h };
    SDL_RenderCopy(ren, t, NULL, &r);
    SDL_DestroyTexture(t);
    SDL_FreeSurface(s);
}

/**
 * drawBtn — exact clone of tbtnDraw() from main.c.
 *   Normal : black fill  (0,0,0,180)  + green border (57,255,20,255)
 *   Hovered: green fill  (57,255,20,120) + green border
 *   Label  : white TTF text, centred inside the rect
 */
static void drawBtn(SDL_Renderer *ren, TTF_Font *font,
                     SDL_Rect rc, int hovered, const char *label)
{
    /* fill — blend mode so black is semi-transparent over matrix rain */
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    if (hovered)
        SDL_SetRenderDrawColor(ren, 57, 255, 20, 120);
    else
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 180);
    SDL_RenderFillRect(ren, &rc);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);

    /* border */
    SDL_SetRenderDrawColor(ren, 57, 255, 20, 255);
    SDL_RenderDrawRect(ren, &rc);

    /* white TTF label centred */
    if (font && label && label[0]) {
        SDL_Color white = {255, 255, 255, 255};
        SDL_Surface *s = TTF_RenderUTF8_Blended(font, label, white);
        if (s) {
            SDL_Texture *t = SDL_CreateTextureFromSurface(ren, s);
            SDL_Rect tr = { rc.x + (rc.w - s->w) / 2,
                            rc.y + (rc.h - s->h) / 2,
                            s->w, s->h };
            SDL_RenderCopy(ren, t, NULL, &tr);
            SDL_DestroyTexture(t);
            SDL_FreeSurface(s);
        }
    }
}

/* Dark overlay drawn on top of matrix rain — same as APP_MODE_SELECT */
static void drawOverlay(SDL_Renderer *ren)
{
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 140);
    SDL_Rect full = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
    SDL_RenderFillRect(ren, &full);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
}

static int pointInRect(int x, int y, SDL_Rect r)
{
    SDL_Point p = {x, y};
    return SDL_PointInRect(&p, &r);
}

/* ── question loading ───────────────────────────────────────────── */

static void chargerQuestions(EnigmeSession *es)
{
    es->nbQ = 0;
    FILE *f = fopen("assets/enigme/questions.txt", "r");
    if (!f) {
        fprintf(stderr, "[ENIGME] questions.txt not found, using defaults\n");
        /* built-in fallback questions */
        const char *def[][6] = {
            {"Quelle est la couleur du code Matrix ?",
             "Vert","Rouge","Bleu","Jaune","1"},
            {"Neo est le heros de ?",
             "Matrix","Avatar","Titanic","Cars","1"},
            {"Qui donne la pilule rouge a Neo ?",
             "Morpheus","Agent Smith","Oracle","Tank","1"},
            {"La Matrix est un ?",
             "Programme","Animal","Livre","Ville","1"},
            {"Qui combat Neo a la fin ?",
             "Smith","Morpheus","Trinity","Oracle","1"},
            {"Pilule rouge = ?",
             "Verite","Sommeil","Oubli","Code","1"},
            {"Qui aide Neo dans la Matrix ?",
             "Trinity","Elsa","Anna","Mia","1"},
            {"La couleur principale du theme Matrix ?",
             "Vert","Rose","Orange","Blanc","1"},
            {"Neo sait faire ?",
             "Kung-Fu","Cuisine","Danse","Football","1"},
            {"Qui a cree la Matrix ?",
             "Machines","Chats","Humains","Aliens","1"},
        };
        int nb = (int)(sizeof(def) / sizeof(def[0]));
        for (int i = 0; i < nb && i < MAX_QUESTIONS; i++) {
            strncpy(es->questions[i].question, def[i][0], 199);
            for (int r = 0; r < 4; r++)
                strncpy(es->questions[i].rep[r], def[i][r+1], 79);
            es->questions[i].bonne   = atoi(def[i][5]);
            es->questions[i].deja_vu = 0;
        }
        es->nbQ = nb;
        return;
    }

    char line[512];
    while (fgets(line, sizeof(line), f) && es->nbQ < MAX_QUESTIONS) {
        /* strip newline */
        int len = (int)strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'))
            line[--len] = '\0';
        if (len == 0) continue;

        /* split by '|' */
        char *parts[6];
        int   cnt = 0;
        char *p   = line;
        while (cnt < 6) {
            parts[cnt++] = p;
            char *pipe = strchr(p, '|');
            if (!pipe) break;
            *pipe = '\0';
            p     = pipe + 1;
        }
        if (cnt < 6) continue;

        Question *q = &es->questions[es->nbQ];
        strncpy(q->question, parts[0], 199);
        for (int r = 0; r < 4; r++) strncpy(q->rep[r], parts[r+1], 79);
        q->bonne   = atoi(parts[5]);
        q->deja_vu = 0;
        es->nbQ++;
    }
    fclose(f);
    fprintf(stderr, "[ENIGME] Loaded %d questions\n", es->nbQ);
}

static int questionAleatoire(EnigmeSession *es)
{
    int indices[MAX_QUESTIONS], count = 0;
    for (int i = 0; i < es->nbQ; i++)
        if (!es->questions[i].deja_vu) indices[count++] = i;
    if (count == 0) {
        for (int i = 0; i < es->nbQ; i++) es->questions[i].deja_vu = 0;
        return rand() % es->nbQ;
    }
    int chosen = indices[rand() % count];
    es->questions[chosen].deja_vu = 1;
    return chosen;
}

/* ── puzzle helpers ─────────────────────────────────────────────── */

static void shuffleTiles(EnigmeSession *es)
{
    for (int i = 0; i < 4; i++) es->tiles[i] = i;
    /* Fisher-Yates */
    for (int i = 3; i > 0; i--) {
        int j = rand() % (i + 1);
        int tmp = es->tiles[i]; es->tiles[i] = es->tiles[j]; es->tiles[j] = tmp;
    }
    /* ensure it's actually shuffled (not identity) */
    int ok = 0;
    for (int i = 0; i < 4; i++) if (es->tiles[i] != i) { ok = 1; break; }
    if (!ok) { es->tiles[0] = 1; es->tiles[1] = 0; }

    es->puzzleTarget[0] = 0;
    es->puzzleTarget[1] = 1;
    es->puzzleTarget[2] = 2;
    es->puzzleTarget[3] = 3;
    es->selectedTile = -1;
}

static int puzzleSolved(EnigmeSession *es)
{
    for (int i = 0; i < 4; i++)
        if (es->tiles[i] != es->puzzleTarget[i]) return 0;
    return 1;
}

/* ── labels for the four puzzle tiles ──────────────────────────── */
static const char *TILE_LABELS[4] = { "A", "B", "C", "D" };
static const char *TILE_WORDS [4] = {
    "NEO",      /* tile 0 */
    "MATRIX",   /* tile 1 */
    "CODE",     /* tile 2 */
    "VERT"      /* tile 3 */
};
/* correct sentence: NEO BETWEEN MATRIX CODE VERT */

/* ================================================================
 *  PUBLIC API
 * ================================================================ */

void enigmeInit(EnigmeSession *es, SDL_Renderer *ren,
                TTF_Font *font, TTF_Font *fontSmall)
{
    memset(es, 0, sizeof(EnigmeSession));
    es->font      = font;
    es->fontSmall = fontSmall ? fontSmall : font;
    es->result    = ENIGME_PENDING;
    es->selectedTile = -1;

    /* load result splash assets only — background is matrix rain from main */
    es->successTex = loadTex(ren, "assets/enigme/success.png");
    es->failTex    = loadTex(ren, "assets/enigme/fail.png");

    chargerQuestions(es);

    srand((unsigned)time(NULL));
}

void enigmeFree(EnigmeSession *es)
{
    if (es->successTex)  { SDL_DestroyTexture(es->successTex);  es->successTex  = NULL; }
    if (es->failTex)     { SDL_DestroyTexture(es->failTex);     es->failTex     = NULL; }
}

/* ── CHOICE SCREEN ──────────────────────────────────────────────── */

int enigmeRenderChoice(EnigmeSession *es, SDL_Renderer *ren,
                        SDL_Event *ev, int mx, int my)
{
    /* matrix_render() already called by main.c before this — just add overlay */
    drawOverlay(ren);

    SDL_Color green  = {57, 255, 20, 255};
    SDL_Color white  = {255, 255, 255, 255};
    SDL_Color yellow = {255, 230, 0, 255};

    ttfCentre(ren, es->font,
              "DERNIERE CHANCE !",
              yellow, SCREEN_WIDTH / 2, 100);
    ttfCentre(ren, es->fontSmall,
              "Resolvez l'enigme pour regagner une vie.",
              white, SCREEN_WIDTH / 2, 160);
    ttfCentre(ren, es->fontSmall,
              "Choisissez le type d'enigme :",
              green, SCREEN_WIDTH / 2, 210);

    /* two side-by-side buttons — same proportions as main menu */
    int bw = 260, bh = 80;
    int gap = 60;
    int totalW = bw * 2 + gap;
    int bx1 = SCREEN_WIDTH / 2 - totalW / 2;
    int bx2 = bx1 + bw + gap;
    int by  = SCREEN_HEIGHT / 2 - bh / 2 + 40;

    SDL_Rect btnQCM    = { bx1, by, bw, bh };
    SDL_Rect btnPuzzle = { bx2, by, bw, bh };

    int hovQCM    = pointInRect(mx, my, btnQCM);
    int hovPuzzle = pointInRect(mx, my, btnPuzzle);

    drawBtn(ren, es->font, btnQCM,    hovQCM,    "QCM");
    drawBtn(ren, es->font, btnPuzzle, hovPuzzle, "PUZZLE");

    ttfCentre(ren, es->fontSmall,
              "(Cliquez sur une option)",
              (SDL_Color){0, 180, 60, 255},
              SCREEN_WIDTH / 2, by + bh + 30);

    /* event handling */
    if (ev && ev->type == SDL_MOUSEBUTTONDOWN &&
        ev->button.button == SDL_BUTTON_LEFT) {
        if (pointInRect(ev->button.x, ev->button.y, btnQCM))    return 0;
        if (pointInRect(ev->button.x, ev->button.y, btnPuzzle)) return 1;
    }
    return -1; /* nothing clicked */
}

/* ── QCM ────────────────────────────────────────────────────────── */

void enigmeStartQCM(EnigmeSession *es)
{
    es->currentIdx = questionAleatoire(es);
    es->startTime  = SDL_GetTicks();
    es->timeLeft   = QCM_TIME_SEC;
    es->result     = ENIGME_PENDING;
    es->flashFrames = 0;
}

EnigmeResult enigmeUpdateQCM(EnigmeSession *es, SDL_Renderer *ren,
                              SDL_Event *ev)
{
    /* ── compute time ── */
    int elapsed = (int)((SDL_GetTicks() - es->startTime) / 1000);
    es->timeLeft = QCM_TIME_SEC - elapsed;
    if (es->timeLeft <= 0) {
        es->timeLeft = 0;
        es->result   = ENIGME_LOSE;
        return es->result;
    }

    /* ── render overlay on top of matrix rain drawn by main.c ── */
    drawOverlay(ren);

    SDL_Color green  = {57, 255, 20, 255};
    SDL_Color white  = {220, 255, 220, 255};
    SDL_Color yellow = {255, 230, 0, 255};
    SDL_Color red    = {255, 60,  60, 255};

    Question *q = &es->questions[es->currentIdx];

    /* question */
    ttfCentre(ren, es->font, "ENIGME — QCM",
              yellow, SCREEN_WIDTH / 2, 30);
    ttfCentre(ren, es->fontSmall, q->question,
              green, SCREEN_WIDTH / 2, 90);

    /* answer buttons */
    int bw = 500, bh = 60, bx = SCREEN_WIDTH / 2 - bw / 2;
    int by0 = 180;
    const char *keys[4] = {"1", "2", "3", "4"};
    int mx2, my2; SDL_GetMouseState(&mx2, &my2);

    for (int i = 0; i < 4; i++) {
        SDL_Rect btn = { bx, by0 + i * (bh + 14), bw, bh };
        int hov = pointInRect(mx2, my2, btn);
        drawBtn(ren, NULL, btn, hov, NULL);

        char label[120];
        snprintf(label, sizeof(label), "[%s]  %s", keys[i], q->rep[i]);
        ttfAt(ren, es->fontSmall, label, hov ? white : (SDL_Color){160,255,160,255},
              btn.x + 20, btn.y + (bh - 24) / 2);
    }

    /* timer bar background then fill (bg first so fill is visible on top) */
    SDL_Rect tbarBg = { SCREEN_WIDTH/2 - 250, 510, 500, 18 };
    SDL_SetRenderDrawColor(ren, 30, 30, 30, 255);
    SDL_RenderFillRect(ren, &tbarBg);

    int timerBarW = (int)(500.0f * es->timeLeft / QCM_TIME_SEC);
    SDL_SetRenderDrawColor(ren,
        (Uint8)(255 - 2 * es->timeLeft),
        (Uint8)(2 * es->timeLeft), 0, 255);
    SDL_Rect tbar = { SCREEN_WIDTH/2 - 250, 510, timerBarW, 18 };
    SDL_RenderFillRect(ren, &tbar);

    SDL_SetRenderDrawColor(ren, 57, 255, 20, 200);
    SDL_RenderDrawRect(ren, &tbarBg);

    char timeBuf[16];
    snprintf(timeBuf, sizeof(timeBuf), "%02d s", es->timeLeft);
    ttfCentre(ren, es->fontSmall, timeBuf,
              es->timeLeft <= 10 ? red : green,
              SCREEN_WIDTH / 2, 535);

    /* hint */
    ttfCentre(ren, es->fontSmall,
              "Appuyez sur 1  2  3  ou  4",
              (SDL_Color){0, 140, 50, 255},
              SCREEN_WIDTH / 2, 580);

    /* ── event handling ── */
    if (ev) {
        if (ev->type == SDL_KEYDOWN) {
            int choice = -1;
            if (ev->key.keysym.sym == SDLK_1) choice = 1;
            if (ev->key.keysym.sym == SDLK_2) choice = 2;
            if (ev->key.keysym.sym == SDLK_3) choice = 3;
            if (ev->key.keysym.sym == SDLK_4) choice = 4;
            if (choice != -1) {
                es->result = (choice == q->bonne)
                             ? ENIGME_WIN : ENIGME_LOSE;
                return es->result;
            }
        }
        /* mouse click on answer buttons */
        if (ev->type == SDL_MOUSEBUTTONDOWN &&
            ev->button.button == SDL_BUTTON_LEFT) {
            for (int i = 0; i < 4; i++) {
                SDL_Rect btn = { bx, by0 + i * (bh + 14), bw, bh };
                if (pointInRect(ev->button.x, ev->button.y, btn)) {
                    es->result = ((i + 1) == q->bonne)
                                 ? ENIGME_WIN : ENIGME_LOSE;
                    return es->result;
                }
            }
        }
    }

    return ENIGME_PENDING;
}

/* ── PUZZLE ─────────────────────────────────────────────────────── */

void enigmeStartPuzzle(EnigmeSession *es)
{
    shuffleTiles(es);
    es->result = ENIGME_PENDING;
    es->flashFrames = 0;
}

EnigmeResult enigmeUpdatePuzzle(EnigmeSession *es, SDL_Renderer *ren,
                                 SDL_Event *ev)
{
    /* ── render overlay on top of matrix rain drawn by main.c ── */
    drawOverlay(ren);

    SDL_Color green  = {57, 255, 20, 255};
    SDL_Color white  = {220, 255, 220, 255};
    SDL_Color yellow = {255, 230, 0, 255};

    ttfCentre(ren, es->font, "ENIGME — PUZZLE",
              yellow, SCREEN_WIDTH / 2, 30);
    ttfCentre(ren, es->fontSmall,
              "Remettez les mots dans le bon ordre :",
              green, SCREEN_WIDTH / 2, 80);
    ttfCentre(ren, es->fontSmall,
              "Ordre correct :  NEO  |  MATRIX  |  CODE  |  VERT",
              (SDL_Color){0, 180, 60, 255},
              SCREEN_WIDTH / 2, 115);

    /* draw 4 tiles */
    int tw = 220, th = 90;
    int gap = 20;
    int totalW = 4 * tw + 3 * gap;
    int tx0 = SCREEN_WIDTH / 2 - totalW / 2;
    int ty  = SCREEN_HEIGHT / 2 - th / 2;

    int mx2, my2; SDL_GetMouseState(&mx2, &my2);

    for (int slot = 0; slot < 4; slot++) {
        int tileIdx = es->tiles[slot];
        SDL_Rect r  = { tx0 + slot * (tw + gap), ty, tw, th };

        int sel = (slot == es->selectedTile);
        int hov = pointInRect(mx2, my2, r);

        /* fill */
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
        if (sel)
            SDL_SetRenderDrawColor(ren, 255, 200, 0, 200);
        else if (hov)
            SDL_SetRenderDrawColor(ren, 57, 255, 20, 140);
        else
            SDL_SetRenderDrawColor(ren, 0, 60, 0, 200);
        SDL_RenderFillRect(ren, &r);
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);

        /* border */
        SDL_SetRenderDrawColor(ren, 57, 255, 20, 255);
        SDL_RenderDrawRect(ren, &r);
        SDL_Rect inner = {r.x+2, r.y+2, r.w-4, r.h-4};
        SDL_RenderDrawRect(ren, &inner);

        /* tile word */
        ttfCentre(ren, es->font, TILE_WORDS[tileIdx],
                  sel ? (SDL_Color){0,0,0,255} : white,
                  r.x + tw / 2, r.y + (th - 30) / 2);

        /* slot number hint */
        char slotBuf[4]; snprintf(slotBuf, sizeof(slotBuf), "%d", slot + 1);
        drawText(ren, r.x + 6, r.y + 4, slotBuf, 1, 0, 140, 40);
        (void)TILE_LABELS;
    }

    /* instructions */
    ttfCentre(ren, es->fontSmall,
              "Cliquez sur deux tuiles pour les echanger  |  ESC = abandonner",
              (SDL_Color){0, 140, 50, 255},
              SCREEN_WIDTH / 2, ty + th + 30);

    /* selected tile indicator */
    if (es->selectedTile >= 0) {
        char buf[128];
        snprintf(buf, sizeof(buf), "Tuile %d selectionnee — cliquez sur une autre pour echanger",
                 es->selectedTile + 1);
        ttfCentre(ren, es->fontSmall, buf,
                  yellow, SCREEN_WIDTH / 2, ty + th + 65);
    }

    /* ── event handling ── */
    if (ev) {
        if (ev->type == SDL_KEYDOWN &&
            ev->key.keysym.sym == SDLK_ESCAPE) {
            es->result = ENIGME_LOSE;
            return es->result;
        }
        if (ev->type == SDL_MOUSEBUTTONDOWN &&
            ev->button.button == SDL_BUTTON_LEFT) {
            for (int slot = 0; slot < 4; slot++) {
                SDL_Rect r = { tx0 + slot * (tw + gap), ty, tw, th };
                if (!pointInRect(ev->button.x, ev->button.y, r)) continue;
                if (es->selectedTile == -1) {
                    es->selectedTile = slot;
                } else {
                    /* swap */
                    if (es->selectedTile != slot) {
                        int tmp = es->tiles[es->selectedTile];
                        es->tiles[es->selectedTile] = es->tiles[slot];
                        es->tiles[slot] = tmp;
                    }
                    es->selectedTile = -1;
                    /* check win */
                    if (puzzleSolved(es)) {
                        es->result = ENIGME_WIN;
                        return es->result;
                    }
                }
                break;
            }
        }
    }

    return ENIGME_PENDING;
}

/* ── RESULT SPLASH ──────────────────────────────────────────────── */

int enigmeRenderResult(EnigmeSession *es, SDL_Renderer *ren)
{
#define RESULT_HOLD_FRAMES 90   /* 1.5 s at 60 fps */

    if (es->flashFrames == 0)
        es->flashFrames = RESULT_HOLD_FRAMES;

    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
    SDL_RenderClear(ren);

    SDL_Texture *tex = (es->result == ENIGME_WIN)
                       ? es->successTex : es->failTex;

    if (tex) {
        /* scale to fill most of the screen, keep aspect ratio */
        int tw, th;
        SDL_QueryTexture(tex, NULL, NULL, &tw, &th);
        float scale = (float)SCREEN_WIDTH / (float)tw * 0.85f;
        int dw = (int)(tw * scale);
        int dh = (int)(th * scale);
        SDL_Rect dst = { SCREEN_WIDTH/2 - dw/2,
                         SCREEN_HEIGHT/2 - dh/2, dw, dh };
        SDL_RenderCopy(ren, tex, NULL, &dst);
    } else {
        /* fallback text */
        SDL_Color col = (es->result == ENIGME_WIN)
                        ? (SDL_Color){57, 255, 20, 255}
                        : (SDL_Color){220, 30, 30, 255};
        const char *msg = (es->result == ENIGME_WIN) ? "BRAVO !" : "GAME OVER";
        ttfCentre(ren, es->font, msg, col, SCREEN_WIDTH/2, SCREEN_HEIGHT/2 - 20);
    }

    es->flashFrames--;
    return (es->flashFrames <= 0) ? 1 : 0; /* 1 = done */
}