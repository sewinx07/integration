/**
 * enigme.c — QCM + Puzzle mini-game for MATRIX GAME.
 *
 * FIXES:
 *  - QCM: Timer bar color arithmetic no longer overflows Uint8 (clamped).
 *  - QCM: NULL event pointer guard — ev may be NULL when called from
 *         the render path without a fresh event.
 *  - QCM: Key bindings changed from SDLK_1/2/3/4 (which conflict with
 *         main.c level-switch hotkeys) to SDLK_KP_1/2/3/4 (numpad) AND
 *         SDLK_F1/F2/F3/F4 as fallback. Mouse click still works.
 *  - PUZZLE: shuffleTiles always produces a truly shuffled state.
 *  - PUZZLE: selectedTile reset when entering puzzle from choice screen.
 *  - PUZZLE: Win condition verified correctly (tiles[i] == puzzleTarget[i]).
 *  - RESULT: flashFrames initialised to 0 before enigmeRenderResult; the
 *            function sets it to RESULT_HOLD_FRAMES on first call.
 *  - OVERLAY: matrix_render() is NOT called here; main.c renders it first,
 *             then calls the enigme function which draws the dark overlay on top.
 *
 * Assets (all optional — text fallback if missing):
 *   assets/enigme/success.png
 *   assets/enigme/fail.png
 *   assets/enigme/questions.txt
 *
 * questions.txt: one question per line
 *   Question|Rep1|Rep2|Rep3|Rep4|CorrectIndex(1-based)
 */

#include "enigme.h"
#include "font.h"
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ── helpers ────────────────────────────────────────────────────── */

static SDL_Texture *loadTex(SDL_Renderer *ren, const char *path)
{
    SDL_Texture *t = IMG_LoadTexture(ren, path);
    if (!t)
        fprintf(stderr, "[ENIGME] Cannot load %s: %s\n", path, IMG_GetError());
    return t;
}

static void ttfCentre(SDL_Renderer *ren, TTF_Font *font,
                       const char *txt, SDL_Color col, int cx, int y)
{
    if (!font || !txt || !txt[0]) return;
    SDL_Surface *s = TTF_RenderUTF8_Blended(font, txt, col);
    if (!s) return;
    SDL_Texture *t = SDL_CreateTextureFromSurface(ren, s);
    if (t) {
        SDL_Rect r = {cx - s->w / 2, y, s->w, s->h};
        SDL_RenderCopy(ren, t, NULL, &r);
        SDL_DestroyTexture(t);
    }
    SDL_FreeSurface(s);
}

static void ttfAt(SDL_Renderer *ren, TTF_Font *font,
                   const char *txt, SDL_Color col, int x, int y)
{
    if (!font || !txt || !txt[0]) return;
    SDL_Surface *s = TTF_RenderUTF8_Blended(font, txt, col);
    if (!s) return;
    SDL_Texture *t = SDL_CreateTextureFromSurface(ren, s);
    if (t) {
        SDL_Rect r = {x, y, s->w, s->h};
        SDL_RenderCopy(ren, t, NULL, &r);
        SDL_DestroyTexture(t);
    }
    SDL_FreeSurface(s);
}

static void drawBtn(SDL_Renderer *ren, TTF_Font *font,
                     SDL_Rect rc, int hovered, const char *label)
{
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    if (hovered)
        SDL_SetRenderDrawColor(ren, 57, 255, 20, 120);
    else
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 180);
    SDL_RenderFillRect(ren, &rc);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);

    SDL_SetRenderDrawColor(ren, 57, 255, 20, 255);
    SDL_RenderDrawRect(ren, &rc);

    if (font && label && label[0]) {
        SDL_Color white = {255, 255, 255, 255};
        SDL_Surface *s  = TTF_RenderUTF8_Blended(font, label, white);
        if (s) {
            SDL_Texture *t = SDL_CreateTextureFromSurface(ren, s);
            if (t) {
                SDL_Rect tr = {rc.x + (rc.w - s->w) / 2,
                               rc.y + (rc.h - s->h) / 2, s->w, s->h};
                SDL_RenderCopy(ren, t, NULL, &tr);
                SDL_DestroyTexture(t);
            }
            SDL_FreeSurface(s);
        }
    }
}

static void drawOverlay(SDL_Renderer *ren)
{
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 150);
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
        fprintf(stderr, "[ENIGME] questions.txt not found — using built-in defaults\n");
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
                strncpy(es->questions[i].rep[r], def[i][r + 1], 79);
            es->questions[i].bonne   = atoi(def[i][5]);
            es->questions[i].deja_vu = 0;
        }
        es->nbQ = nb;
        return;
    }

    char line[512];
    while (fgets(line, sizeof(line), f) && es->nbQ < MAX_QUESTIONS) {
        int len = (int)strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'))
            line[--len] = '\0';
        if (len == 0) continue;

        char *parts[6]; int cnt = 0;
        char *p = line;
        while (cnt < 6) {
            parts[cnt++] = p;
            char *pipe   = strchr(p, '|');
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
        return (es->nbQ > 0) ? rand() % es->nbQ : 0;
    }
    int chosen = indices[rand() % count];
    es->questions[chosen].deja_vu = 1;
    return chosen;
}

/* ── puzzle helpers ─────────────────────────────────────────────── */
static void shuffleTiles(EnigmeSession *es)
{
    for (int i = 0; i < 4; i++) es->tiles[i] = i;
    /* Fisher-Yates — guaranteed shuffle */
    for (int i = 3; i > 0; i--) {
        int j   = rand() % (i + 1);
        int tmp = es->tiles[i]; es->tiles[i] = es->tiles[j]; es->tiles[j] = tmp;
    }
    /* Ensure not identity permutation */
    int identity = 1;
    for (int i = 0; i < 4; i++) if (es->tiles[i] != i) { identity = 0; break; }
    if (identity) { int tmp = es->tiles[0]; es->tiles[0] = es->tiles[1]; es->tiles[1] = tmp; }

    /* Target is always the sorted order 0-1-2-3 */
    for (int i = 0; i < 4; i++) es->puzzleTarget[i] = i;

    es->selectedTile = -1;  /* FIX: always reset selection */
}

static int puzzleSolved(EnigmeSession *es)
{
    for (int i = 0; i < 4; i++)
        if (es->tiles[i] != es->puzzleTarget[i]) return 0;
    return 1;
}

static const char *TILE_WORDS[4] = { "NEO", "MATRIX", "CODE", "VERT" };

/* ================================================================
 *  PUBLIC API
 * ================================================================ */
void enigmeInit(EnigmeSession *es, SDL_Renderer *ren,
                TTF_Font *font, TTF_Font *fontSmall)
{
    memset(es, 0, sizeof(EnigmeSession));
    es->font         = font;
    es->fontSmall    = fontSmall ? fontSmall : font;
    es->result       = ENIGME_PENDING;
    es->selectedTile = -1;

    es->successTex = loadTex(ren, "assets/enigme/success.png");
    es->failTex    = loadTex(ren, "assets/enigme/fail.png");

    chargerQuestions(es);
    srand((unsigned)time(NULL));
}

void enigmeFree(EnigmeSession *es)
{
    if (es->successTex) { SDL_DestroyTexture(es->successTex); es->successTex = NULL; }
    if (es->failTex)    { SDL_DestroyTexture(es->failTex);    es->failTex    = NULL; }
}

/* ── CHOICE SCREEN ──────────────────────────────────────────────── */
int enigmeRenderChoice(EnigmeSession *es, SDL_Renderer *ren,
                        SDL_Event *ev, int mx, int my)
{
    /* matrix_render() already called by main.c — just add dark overlay */
    drawOverlay(ren);

    SDL_Color green  = {57, 255, 20, 255};
    SDL_Color white  = {255, 255, 255, 255};
    SDL_Color yellow = {255, 230, 0, 255};

    ttfCentre(ren, es->font,
              "DERNIERE CHANCE !", yellow, SCREEN_WIDTH / 2, 100);
    ttfCentre(ren, es->fontSmall,
              "Resolvez l'enigme pour regagner une vie.",
              white, SCREEN_WIDTH / 2, 160);
    ttfCentre(ren, es->fontSmall,
              "Choisissez le type d'enigme :",
              green, SCREEN_WIDTH / 2, 210);

    int bw = 260, bh = 80, gap = 60;
    int totalW = bw * 2 + gap;
    int bx1 = SCREEN_WIDTH / 2 - totalW / 2;
    int bx2 = bx1 + bw + gap;
    int by  = SCREEN_HEIGHT / 2 - bh / 2 + 40;

    SDL_Rect btnQCM    = {bx1, by, bw, bh};
    SDL_Rect btnPuzzle = {bx2, by, bw, bh};

    int hovQCM    = pointInRect(mx, my, btnQCM);
    int hovPuzzle = pointInRect(mx, my, btnPuzzle);

    drawBtn(ren, es->font, btnQCM,    hovQCM,    "QCM");
    drawBtn(ren, es->font, btnPuzzle, hovPuzzle, "PUZZLE");

    ttfCentre(ren, es->fontSmall, "(Cliquez sur une option)",
              (SDL_Color){0, 180, 60, 255}, SCREEN_WIDTH / 2, by + bh + 30);

    /* FIX: guard against NULL ev */
    if (ev && ev->type == SDL_MOUSEBUTTONDOWN &&
        ev->button.button == SDL_BUTTON_LEFT) {
        if (pointInRect(ev->button.x, ev->button.y, btnQCM))    return 0;
        if (pointInRect(ev->button.x, ev->button.y, btnPuzzle)) return 1;
    }
    return -1;
}

/* ── QCM ────────────────────────────────────────────────────────── */
void enigmeStartQCM(EnigmeSession *es)
{
    es->currentIdx  = questionAleatoire(es);
    es->startTime   = SDL_GetTicks();
    es->timeLeft    = QCM_TIME_SEC;
    es->result      = ENIGME_PENDING;
    es->flashFrames = 0;
}

EnigmeResult enigmeUpdateQCM(EnigmeSession *es, SDL_Renderer *ren,
                              SDL_Event *ev)
{
    /* Update timer */
    int elapsed  = (int)((SDL_GetTicks() - es->startTime) / 1000);
    es->timeLeft = QCM_TIME_SEC - elapsed;
    if (es->timeLeft <= 0) {
        es->timeLeft = 0;
        es->result   = ENIGME_LOSE;
        return es->result;
    }

    /* Render: overlay already on matrix rain drawn by main.c */
    drawOverlay(ren);

    SDL_Color green  = {57, 255, 20, 255};
    SDL_Color white  = {220, 255, 220, 255};
    SDL_Color yellow = {255, 230, 0, 255};
    SDL_Color red    = {255, 60, 60, 255};

    if (es->nbQ == 0) {
        ttfCentre(ren, es->font, "AUCUNE QUESTION CHARGEE",
                  red, SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2);
        return ENIGME_PENDING;
    }

    Question *q = &es->questions[es->currentIdx];

    ttfCentre(ren, es->font, "ENIGME — QCM", yellow, SCREEN_WIDTH / 2, 30);
    ttfCentre(ren, es->fontSmall, q->question, green, SCREEN_WIDTH / 2, 90);

    /* Answer buttons */
    int bw = 500, bh = 60, bx = SCREEN_WIDTH / 2 - bw / 2;
    int by0 = 180;
    int mx2, my2;
    SDL_GetMouseState(&mx2, &my2);

    /* FIX: key labels updated to reflect new bindings */
    const char *keys[4] = {"F1", "F2", "F3", "F4"};
    for (int i = 0; i < 4; i++) {
        SDL_Rect btn = {bx, by0 + i * (bh + 14), bw, bh};
        int hov = pointInRect(mx2, my2, btn);
        drawBtn(ren, NULL, btn, hov, NULL);

        char label[120];
        snprintf(label, sizeof(label), "[%s]  %s", keys[i], q->rep[i]);
        ttfAt(ren, es->fontSmall, label,
              hov ? white : (SDL_Color){160, 255, 160, 255},
              btn.x + 20, btn.y + (bh - 24) / 2);
    }

    /* Timer bar */
    SDL_Rect tbarBg = {SCREEN_WIDTH / 2 - 250, 510, 500, 18};
    SDL_SetRenderDrawColor(ren, 30, 30, 30, 255);
    SDL_RenderFillRect(ren, &tbarBg);

    int timerBarW = (int)(500.0f * es->timeLeft / QCM_TIME_SEC);
    if (timerBarW < 0) timerBarW = 0;

    /* FIX: clamp colour computation to avoid Uint8 overflow */
    float ratio = (float)es->timeLeft / (float)QCM_TIME_SEC; /* 0..1 */
    Uint8 barR  = (Uint8)(255 * (1.0f - ratio));
    Uint8 barG  = (Uint8)(255 * ratio);
    SDL_SetRenderDrawColor(ren, barR, barG, 0, 255);
    SDL_Rect tbar = {SCREEN_WIDTH / 2 - 250, 510, timerBarW, 18};
    SDL_RenderFillRect(ren, &tbar);
    SDL_SetRenderDrawColor(ren, 57, 255, 20, 200);
    SDL_RenderDrawRect(ren, &tbarBg);

    char timeBuf[16];
    snprintf(timeBuf, sizeof(timeBuf), "%02d s", es->timeLeft);
    ttfCentre(ren, es->fontSmall, timeBuf,
              es->timeLeft <= 10 ? red : green,
              SCREEN_WIDTH / 2, 535);

    /* FIX: updated hint text */
    ttfCentre(ren, es->fontSmall,
              "Appuyez sur F1  F2  F3  ou  F4  (ou cliquez)",
              (SDL_Color){0, 140, 50, 255}, SCREEN_WIDTH / 2, 580);

    /* Event handling — FIX: guard NULL ev */
    if (ev) {
        if (ev->type == SDL_KEYDOWN) {
            int choice = -1;
            SDL_Keycode k = ev->key.keysym.sym;
            /* FIX: use F-keys to avoid conflict with level-switch 1/2 */
            if (k == SDLK_F1 || k == SDLK_KP_1) choice = 1;
            if (k == SDLK_F2 || k == SDLK_KP_2) choice = 2;
            if (k == SDLK_F3 || k == SDLK_KP_3) choice = 3;
            if (k == SDLK_F4 || k == SDLK_KP_4) choice = 4;
            if (choice != -1) {
                es->result = (choice == q->bonne) ? ENIGME_WIN : ENIGME_LOSE;
                return es->result;
            }
        }
        if (ev->type == SDL_MOUSEBUTTONDOWN &&
            ev->button.button == SDL_BUTTON_LEFT) {
            for (int i = 0; i < 4; i++) {
                SDL_Rect btn = {bx, by0 + i * (bh + 14), bw, bh};
                if (pointInRect(ev->button.x, ev->button.y, btn)) {
                    es->result = ((i + 1) == q->bonne) ? ENIGME_WIN : ENIGME_LOSE;
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
    shuffleTiles(es);          /* also resets selectedTile */
    es->result      = ENIGME_PENDING;
    es->flashFrames = 0;
}

EnigmeResult enigmeUpdatePuzzle(EnigmeSession *es, SDL_Renderer *ren,
                                 SDL_Event *ev)
{
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
              (SDL_Color){0, 180, 60, 255}, SCREEN_WIDTH / 2, 115);

    int tw = 220, th = 90, gap = 20;
    int totalW = 4 * tw + 3 * gap;
    int tx0 = SCREEN_WIDTH / 2 - totalW / 2;
    int ty  = SCREEN_HEIGHT / 2 - th / 2;

    int mx2, my2;
    SDL_GetMouseState(&mx2, &my2);

    for (int slot = 0; slot < 4; slot++) {
        int tileIdx = es->tiles[slot];
        SDL_Rect r  = {tx0 + slot * (tw + gap), ty, tw, th};

        int sel = (slot == es->selectedTile);
        int hov = pointInRect(mx2, my2, r);

        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
        if (sel)
            SDL_SetRenderDrawColor(ren, 255, 200, 0, 200);
        else if (hov)
            SDL_SetRenderDrawColor(ren, 57, 255, 20, 140);
        else
            SDL_SetRenderDrawColor(ren, 0, 60, 0, 200);
        SDL_RenderFillRect(ren, &r);
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);

        SDL_SetRenderDrawColor(ren, 57, 255, 20, 255);
        SDL_RenderDrawRect(ren, &r);
        SDL_Rect inner = {r.x+2, r.y+2, r.w-4, r.h-4};
        SDL_RenderDrawRect(ren, &inner);

        /* Tile word centred */
        ttfCentre(ren, es->font, TILE_WORDS[tileIdx],
                  sel ? (SDL_Color){0, 0, 0, 255} : white,
                  r.x + tw / 2, r.y + (th - 30) / 2);

        /* Slot number hint */
        char slotBuf[4];
        snprintf(slotBuf, sizeof(slotBuf), "%d", slot + 1);
        drawText(ren, r.x + 6, r.y + 4, slotBuf, 1, 0, 140, 40);
    }

    /* Instructions */
    ttfCentre(ren, es->fontSmall,
              "Cliquez sur deux tuiles pour les echanger  |  ESC = abandonner",
              (SDL_Color){0, 140, 50, 255}, SCREEN_WIDTH / 2, ty + th + 30);

    if (es->selectedTile >= 0) {
        char buf[128];
        snprintf(buf, sizeof(buf),
                 "Tuile %d selectionnee — cliquez sur une autre pour echanger",
                 es->selectedTile + 1);
        ttfCentre(ren, es->fontSmall, buf, yellow, SCREEN_WIDTH / 2, ty + th + 65);
    }

    /* FIX: guard NULL ev */
    if (ev) {
        if (ev->type == SDL_KEYDOWN &&
            ev->key.keysym.sym == SDLK_ESCAPE) {
            es->result = ENIGME_LOSE;
            return es->result;
        }
        if (ev->type == SDL_MOUSEBUTTONDOWN &&
            ev->button.button == SDL_BUTTON_LEFT) {
            for (int slot = 0; slot < 4; slot++) {
                SDL_Rect r = {tx0 + slot * (tw + gap), ty, tw, th};
                if (!pointInRect(ev->button.x, ev->button.y, r)) continue;

                if (es->selectedTile == -1) {
                    es->selectedTile = slot;
                } else {
                    if (es->selectedTile != slot) {
                        /* Swap tiles */
                        int tmp = es->tiles[es->selectedTile];
                        es->tiles[es->selectedTile] = es->tiles[slot];
                        es->tiles[slot] = tmp;
                    }
                    es->selectedTile = -1;

                    /* FIX: check win condition after every swap */
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
        int tw, th;
        SDL_QueryTexture(tex, NULL, NULL, &tw, &th);
        float scale = (float)SCREEN_WIDTH / (float)tw * 0.85f;
        int   dw    = (int)(tw * scale);
        int   dh    = (int)(th * scale);
        SDL_Rect dst = {SCREEN_WIDTH / 2 - dw / 2,
                        SCREEN_HEIGHT / 2 - dh / 2, dw, dh};
        SDL_RenderCopy(ren, tex, NULL, &dst);
    } else {
        SDL_Color col = (es->result == ENIGME_WIN)
                        ? (SDL_Color){57, 255, 20, 255}
                        : (SDL_Color){220, 30, 30, 255};
        const char *msg = (es->result == ENIGME_WIN) ? "BRAVO !" : "GAME OVER";
        ttfCentre(ren, es->font, msg, col,
                  SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 - 20);
    }

    es->flashFrames--;
    return (es->flashFrames <= 0) ? 1 : 0;
}