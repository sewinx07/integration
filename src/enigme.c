/**
 * enigme.c — QCM + Puzzle mini-game for MATRIX GAME.
 *
 * FIXES IN THIS VERSION:
 *
 *  [ENIGME-QCM-1]  Timer expired in one frame when startTime was 0 at init.
 *                  enigmeStartQCM now always resets startTime to SDL_GetTicks().
 *
 *  [ENIGME-QCM-2]  Colour arithmetic for the timer bar used Uint8 subtraction
 *                  which could underflow. Now uses float ratio clamped to [0,1].
 *
 *  [ENIGME-QCM-3]  Keyboard bindings 1/2/3/4 conflicted with level-switch
 *                  hotkeys. Now exclusively F1/F2/F3/F4 + numpad 1/2/3/4.
 *                  Mouse click on answer buttons still works.
 *
 *  [ENIGME-QCM-4]  NULL event pointer crash: ev was dereferenced without a
 *                  NULL guard in two places. All ev accesses are now guarded.
 *
 *  [ENIGME-QCM-5]  Questions with bonne==0 (invalid) were accepted as correct
 *                  for any answer. Added validation: bonne must be 1-4.
 *
 *  [ENIGME-PUZZLE-1] shuffleTiles never produced a non-trivial shuffle when
 *                  rand() returned sequential values. Fisher-Yates is correct;
 *                  added a fallback swap if the result is identity.
 *
 *  [ENIGME-PUZZLE-2] selectedTile was not reset when entering the puzzle from
 *                  the choice screen. enigmeStartPuzzle now always sets it to -1.
 *
 *  [ENIGME-PUZZLE-3] Win check ran puzzleSolved() only after a swap but NOT
 *                  after the first tile was selected (which is correct), and
 *                  not immediately when tiles happened to be in order at start
 *                  (prevented by shuffleTiles, but guarded anyway).
 *
 *  [ENIGME-RESULT-1] enigmeRenderResult was calling SDL_RenderClear which
 *                  cleared everything drawn by matrix_render. Now it draws a
 *                  solid dark overlay instead of a full clear.
 *
 *  [ENIGME-RESULT-2] flashFrames was never reset between enigme sessions.
 *                  enigmeStartQCM / enigmeStartPuzzle now reset it to 0, and
 *                  enigmeRenderResult sets it on first call (flashFrames==0).
 *
 *  [ENIGME-OVERLAY] The dark overlay alpha was 150 which made the matrix rain
 *                  barely visible. Reduced to 190 for better readability of
 *                  question text while keeping the animated background visible.
 *
 *  [ENIGME-CHOICE] Button hover detection used mouse coords from the event
 *                  (ev->button.x/y) but the buttons were highlighted using
 *                  mx/my from SDL_GetMouseState. Unified to use SDL_GetMouseState.
 *
 *  Assets (all optional — text fallback if missing):
 *    assets/enigme/success.png
 *    assets/enigme/fail.png
 *    assets/enigme/questions.txt
 *
 *  questions.txt format: one question per line
 *    Question|Rep1|Rep2|Rep3|Rep4|CorrectIndex(1-based)
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
        SDL_SetRenderDrawColor(ren, 57, 255, 20, 140);
    else
        SDL_SetRenderDrawColor(ren, 0, 20, 0, 200);
    SDL_RenderFillRect(ren, &rc);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);

    SDL_SetRenderDrawColor(ren, 57, 255, 20, 255);
    SDL_RenderDrawRect(ren, &rc);
    /* Inner border for style */
    SDL_Rect inner = {rc.x+2, rc.y+2, rc.w-4, rc.h-4};
    SDL_RenderDrawRect(ren, &inner);

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

/* [ENIGME-OVERLAY] Slightly more opaque so text is readable */
static void drawOverlay(SDL_Renderer *ren)
{
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 190);
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
        /* Built-in Matrix-themed questions */
        static const char *def[][6] = {
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
            {"Combien de pilules Morpheus propose-t-il ?",
             "2","1","3","4","1"},
            {"Dans quel film Neo apparait-il la premiere fois ?",
             "Matrix","Matrix Reloaded","Matrix Revolutions","Animatrix","1"},
        };
        int nb = (int)(sizeof(def) / sizeof(def[0]));
        for (int i = 0; i < nb && i < MAX_QUESTIONS; i++) {
            strncpy(es->questions[i].question, def[i][0], 199);
            es->questions[i].question[199] = '\0';
            for (int r = 0; r < 4; r++) {
                strncpy(es->questions[i].rep[r], def[i][r + 1], 79);
                es->questions[i].rep[r][79] = '\0';
            }
            /* [ENIGME-QCM-5] Validate bonne is 1-4 */
            int b = atoi(def[i][5]);
            es->questions[i].bonne   = (b >= 1 && b <= 4) ? b : 1;
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
        if (len == 0 || line[0] == '#') continue; /* skip blank/comment lines */

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
        q->question[199] = '\0';
        for (int r = 0; r < 4; r++) {
            strncpy(q->rep[r], parts[r+1], 79);
            q->rep[r][79] = '\0';
        }
        int b = atoi(parts[5]);
        q->bonne   = (b >= 1 && b <= 4) ? b : 1; /* [ENIGME-QCM-5] validate */
        q->deja_vu = 0;
        es->nbQ++;
    }
    fclose(f);
    fprintf(stderr, "[ENIGME] Loaded %d questions\n", es->nbQ);
}

static int questionAleatoire(EnigmeSession *es)
{
    if (es->nbQ == 0) return 0;

    /* Collect unseen questions */
    int indices[MAX_QUESTIONS], count = 0;
    for (int i = 0; i < es->nbQ; i++)
        if (!es->questions[i].deja_vu) indices[count++] = i;

    /* If all seen, reset and pick randomly */
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

    /* Fisher-Yates shuffle */
    for (int i = 3; i > 0; i--) {
        int j   = rand() % (i + 1);
        int tmp = es->tiles[i]; es->tiles[i] = es->tiles[j]; es->tiles[j] = tmp;
    }

    /* [ENIGME-PUZZLE-1] Guarantee non-identity permutation */
    int identity = 1;
    for (int i = 0; i < 4; i++) if (es->tiles[i] != i) { identity = 0; break; }
    if (identity) {
        /* Swap first two — always makes it non-identity */
        int tmp = es->tiles[0]; es->tiles[0] = es->tiles[1]; es->tiles[1] = tmp;
    }

    /* Target is always sorted order 0-1-2-3 */
    for (int i = 0; i < 4; i++) es->puzzleTarget[i] = i;

    /* [ENIGME-PUZZLE-2] Always reset selection on start */
    es->selectedTile = -1;
}

static int puzzleSolved(EnigmeSession *es)
{
    for (int i = 0; i < 4; i++)
        if (es->tiles[i] != es->puzzleTarget[i]) return 0;
    return 1;
}

/* The words the player must sort: target order is indices 0,1,2,3 */
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
    es->flashFrames  = 0;

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
    /* matrix_render() already called by main.c — add dark overlay */
    drawOverlay(ren);

    SDL_Color green  = {57, 255, 20, 255};
    SDL_Color white  = {255, 255, 255, 255};
    SDL_Color yellow = {255, 230, 0, 255};

    ttfCentre(ren, es->font,
              "DERNIERE CHANCE !", yellow, SCREEN_WIDTH / 2, 100);
    ttfCentre(ren, es->fontSmall,
              "Resolvez l'enigme pour regagner une vie.",
              white, SCREEN_WIDTH / 2, 155);
    ttfCentre(ren, es->fontSmall,
              "Choisissez le type d'enigme :",
              green, SCREEN_WIDTH / 2, 205);

    int bw = 280, bh = 90, gap = 80;
    int totalW = bw * 2 + gap;
    int bx1 = SCREEN_WIDTH / 2 - totalW / 2;
    int bx2 = bx1 + bw + gap;
    int by  = SCREEN_HEIGHT / 2 - bh / 2 + 20;

    SDL_Rect btnQCM    = {bx1, by, bw, bh};
    SDL_Rect btnPuzzle = {bx2, by, bw, bh};

    /* [ENIGME-CHOICE] Use mx/my from SDL_GetMouseState for consistent hover */
    int hovQCM    = pointInRect(mx, my, btnQCM);
    int hovPuzzle = pointInRect(mx, my, btnPuzzle);

    drawBtn(ren, es->font, btnQCM,    hovQCM,    "QCM");
    drawBtn(ren, es->font, btnPuzzle, hovPuzzle, "PUZZLE");

    /* Sub-labels */
    ttfCentre(ren, es->fontSmall, "4 choix - 30 secondes",
              (SDL_Color){180, 255, 180, 255}, bx1 + bw/2, by + bh + 8);
    ttfCentre(ren, es->fontSmall, "Reordonnez les tuiles",
              (SDL_Color){180, 255, 180, 255}, bx2 + bw/2, by + bh + 8);

    ttfCentre(ren, es->fontSmall, "(Cliquez sur une option)",
              (SDL_Color){0, 180, 60, 255}, SCREEN_WIDTH / 2, by + bh + 50);

    /* [ENIGME-QCM-4] Guard NULL ev */
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
    /* [ENIGME-QCM-1] Always set startTime here */
    es->currentIdx  = questionAleatoire(es);
    es->startTime   = SDL_GetTicks();
    es->timeLeft    = QCM_TIME_SEC;
    es->result      = ENIGME_PENDING;
    es->flashFrames = 0; /* [ENIGME-RESULT-2] Reset flash */
}

EnigmeResult enigmeUpdateQCM(EnigmeSession *es, SDL_Renderer *ren,
                              SDL_Event *ev)
{
    /* [ENIGME-QCM-1] Guard against startTime == 0 */
    if (es->startTime == 0) es->startTime = SDL_GetTicks();

    Uint32 now     = SDL_GetTicks();
    int elapsed    = (int)((now - es->startTime) / 1000);
    es->timeLeft   = QCM_TIME_SEC - elapsed;
    if (es->timeLeft <= 0) {
        es->timeLeft = 0;
        es->result   = ENIGME_LOSE;
        return es->result;
    }

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

    /* Clamp currentIdx just in case */
    if (es->currentIdx < 0 || es->currentIdx >= es->nbQ)
        es->currentIdx = 0;

    Question *q = &es->questions[es->currentIdx];

    ttfCentre(ren, es->font, "ENIGME — QCM", yellow, SCREEN_WIDTH / 2, 30);

    /* Question text — wrapped manually at ~80 chars */
    ttfCentre(ren, es->fontSmall, q->question, green, SCREEN_WIDTH / 2, 90);

    /* Answer buttons */
    int bw = 520, bh = 60, bx = SCREEN_WIDTH / 2 - bw / 2;
    int by0 = 170;

    int curMx, curMy;
    SDL_GetMouseState(&curMx, &curMy);

    /* [ENIGME-QCM-3] F1-F4 and numpad 1-4 only */
    const char *keys[4] = {"F1", "F2", "F3", "F4"};
    for (int i = 0; i < 4; i++) {
        SDL_Rect btn = {bx, by0 + i * (bh + 16), bw, bh};
        int hov = pointInRect(curMx, curMy, btn);
        drawBtn(ren, NULL, btn, hov, NULL);

        char label[120];
        snprintf(label, sizeof(label), "[%s]  %s", keys[i], q->rep[i]);
        ttfAt(ren, es->fontSmall, label,
              hov ? white : (SDL_Color){160, 255, 160, 255},
              btn.x + 16, btn.y + (bh - 22) / 2);
    }

    /* Timer bar */
    SDL_Rect tbarBg = {SCREEN_WIDTH / 2 - 260, 460, 520, 20};
    SDL_SetRenderDrawColor(ren, 20, 20, 20, 200);
    SDL_RenderFillRect(ren, &tbarBg);

    int timerBarW = (int)(520.0f * es->timeLeft / QCM_TIME_SEC);
    if (timerBarW < 0) timerBarW = 0;
    if (timerBarW > 520) timerBarW = 520;

    /* [ENIGME-QCM-2] Float ratio — no Uint8 underflow */
    float ratio = (float)es->timeLeft / (float)QCM_TIME_SEC;
    if (ratio < 0.0f) ratio = 0.0f;
    if (ratio > 1.0f) ratio = 1.0f;
    Uint8 barR  = (Uint8)(255.0f * (1.0f - ratio));
    Uint8 barG  = (Uint8)(255.0f * ratio);
    SDL_SetRenderDrawColor(ren, barR, barG, 0, 255);
    SDL_Rect tbar = {SCREEN_WIDTH / 2 - 260, 460, timerBarW, 20};
    SDL_RenderFillRect(ren, &tbar);
    SDL_SetRenderDrawColor(ren, 57, 255, 20, 200);
    SDL_RenderDrawRect(ren, &tbarBg);

    char timeBuf[20];
    snprintf(timeBuf, sizeof(timeBuf), "%02d s", es->timeLeft);
    ttfCentre(ren, es->fontSmall, timeBuf,
              es->timeLeft <= 10 ? red : green,
              SCREEN_WIDTH / 2, 488);

    ttfCentre(ren, es->fontSmall,
              "Appuyez sur  F1  F2  F3  F4  (ou cliquez)",
              (SDL_Color){0, 140, 50, 255}, SCREEN_WIDTH / 2, 530);

    /* [ENIGME-QCM-4] Guard NULL ev */
    if (ev) {
        if (ev->type == SDL_KEYDOWN) {
            int choice = -1;
            SDL_Keycode k = ev->key.keysym.sym;
            if (k == SDLK_F1 || k == SDLK_KP_1) choice = 1;
            if (k == SDLK_F2 || k == SDLK_KP_2) choice = 2;
            if (k == SDLK_F3 || k == SDLK_KP_3) choice = 3;
            if (k == SDLK_F4 || k == SDLK_KP_4) choice = 4;
            if (choice >= 1 && choice <= 4) {
                es->result = (choice == q->bonne) ? ENIGME_WIN : ENIGME_LOSE;
                return es->result;
            }
        }
        if (ev->type == SDL_MOUSEBUTTONDOWN &&
            ev->button.button == SDL_BUTTON_LEFT) {
            for (int i = 0; i < 4; i++) {
                SDL_Rect btn = {bx, by0 + i * (bh + 16), bw, bh};
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
    shuffleTiles(es);          /* also resets selectedTile to -1 */
    es->result      = ENIGME_PENDING;
    es->flashFrames = 0; /* [ENIGME-RESULT-2] reset */
}

EnigmeResult enigmeUpdatePuzzle(EnigmeSession *es, SDL_Renderer *ren,
                                 SDL_Event *ev)
{
    drawOverlay(ren);

    SDL_Color green  = {57, 255, 20, 255};
    SDL_Color white  = {220, 255, 220, 255};
    SDL_Color yellow = {255, 230, 0, 255};
    SDL_Color sel_col = {255, 200, 0, 255};

    ttfCentre(ren, es->font, "ENIGME — PUZZLE",
              yellow, SCREEN_WIDTH / 2, 30);
    ttfCentre(ren, es->fontSmall,
              "Remettez les mots dans le bon ordre :",
              green, SCREEN_WIDTH / 2, 80);
    ttfCentre(ren, es->fontSmall,
              "Ordre correct :  NEO  |  MATRIX  |  CODE  |  VERT",
              (SDL_Color){0, 200, 80, 255}, SCREEN_WIDTH / 2, 115);

    int tw = 210, th = 90, gap = 22;
    int totalW = 4 * tw + 3 * gap;
    int tx0 = SCREEN_WIDTH / 2 - totalW / 2;
    int ty  = SCREEN_HEIGHT / 2 - th / 2;

    int curMx, curMy;
    SDL_GetMouseState(&curMx, &curMy);

    for (int slot = 0; slot < 4; slot++) {
        int tileIdx = es->tiles[slot];
        SDL_Rect r  = {tx0 + slot * (tw + gap), ty, tw, th};

        int sel = (slot == es->selectedTile);
        int hov = pointInRect(curMx, curMy, r) && !sel;

        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
        if (sel)
            SDL_SetRenderDrawColor(ren, 200, 160, 0, 220);
        else if (hov)
            SDL_SetRenderDrawColor(ren, 30, 100, 30, 200);
        else
            SDL_SetRenderDrawColor(ren, 0, 40, 0, 200);
        SDL_RenderFillRect(ren, &r);
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);

        Uint8 br = sel ? 255 : 57, bg2 = sel ? 200 : 255, bb = sel ? 0 : 20;
        SDL_SetRenderDrawColor(ren, br, bg2, bb, 255);
        SDL_RenderDrawRect(ren, &r);
        SDL_Rect inner2 = {r.x+2, r.y+2, r.w-4, r.h-4};
        SDL_RenderDrawRect(ren, &inner2);

        ttfCentre(ren, es->font, TILE_WORDS[tileIdx],
                  sel ? (SDL_Color){0, 0, 0, 255} : white,
                  r.x + tw / 2, r.y + (th - 30) / 2);

        /* Slot number hint in corner */
        char slotBuf[4];
        snprintf(slotBuf, sizeof(slotBuf), "%d", slot + 1);
        drawText(ren, r.x + 6, r.y + 5, slotBuf, 1, 0, 160, 50);

        (void)sel_col;
    }

    /* Instructions */
    if (es->selectedTile >= 0) {
        char buf[128];
        snprintf(buf, sizeof(buf),
                 "Tuile %d selectionnee — cliquez sur une autre pour echanger",
                 es->selectedTile + 1);
        ttfCentre(ren, es->fontSmall, buf, yellow, SCREEN_WIDTH / 2, ty + th + 30);
    } else {
        ttfCentre(ren, es->fontSmall,
                  "Cliquez sur deux tuiles pour les echanger  |  ESC = abandonner",
                  (SDL_Color){0, 160, 60, 255}, SCREEN_WIDTH / 2, ty + th + 30);
    }

    /* [ENIGME-QCM-4] Guard NULL ev */
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
                    /* First tile selected */
                    es->selectedTile = slot;
                } else if (es->selectedTile == slot) {
                    /* Clicked same tile — deselect */
                    es->selectedTile = -1;
                } else {
                    /* Swap the two tiles */
                    int tmp = es->tiles[es->selectedTile];
                    es->tiles[es->selectedTile] = es->tiles[slot];
                    es->tiles[slot] = tmp;
                    es->selectedTile = -1;

                    /* [ENIGME-PUZZLE-3] Check win after every swap */
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

    /* [ENIGME-RESULT-2] First call: set the hold counter */
    if (es->flashFrames == 0)
        es->flashFrames = RESULT_HOLD_FRAMES;

    /* [ENIGME-RESULT-1] Draw dark overlay instead of SDL_RenderClear
       so the matrix rain (already rendered by main) shows through */
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 220);
    SDL_Rect full = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
    SDL_RenderFillRect(ren, &full);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);

    SDL_Texture *tex = (es->result == ENIGME_WIN)
                       ? es->successTex : es->failTex;

    if (tex) {
        int texW, texH;
        SDL_QueryTexture(tex, NULL, NULL, &texW, &texH);
        float scale = (float)SCREEN_WIDTH / (float)texW * 0.75f;
        int   dw    = (int)(texW * scale);
        int   dh    = (int)(texH * scale);
        if (dh > SCREEN_HEIGHT - 80) {
            /* Clamp height */
            dh    = SCREEN_HEIGHT - 80;
            scale = (float)dh / (float)texH;
            dw    = (int)(texW * scale);
        }
        SDL_Rect dst = {SCREEN_WIDTH / 2 - dw / 2,
                        SCREEN_HEIGHT / 2 - dh / 2, dw, dh};
        SDL_RenderCopy(ren, tex, NULL, &dst);
    } else {
        /* Text fallback */
        SDL_Color col = (es->result == ENIGME_WIN)
                        ? (SDL_Color){57, 255, 20, 255}
                        : (SDL_Color){220, 30, 30, 255};
        const char *msg = (es->result == ENIGME_WIN) ? "BRAVO !" : "GAME OVER";
        ttfCentre(ren, es->font, msg, col,
                  SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 - 20);

        /* Sub-text */
        const char *sub = (es->result == ENIGME_WIN)
                          ? "Vous regagnez une vie !"
                          : "La partie est terminee.";
        ttfCentre(ren, es->fontSmall, sub,
                  (SDL_Color){200, 200, 200, 255},
                  SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 + 40);
    }

    /* Countdown indicator */
    int remaining = es->flashFrames;
    int dotCount  = (remaining / (RESULT_HOLD_FRAMES / 3)) + 1;
    if (dotCount > 3) dotCount = 3;
    char dots[8] = {0};
    for (int d = 0; d < dotCount; d++) dots[d] = '.';
    ttfCentre(ren, es->fontSmall, dots,
              (SDL_Color){200, 200, 200, 200},
              SCREEN_WIDTH / 2, SCREEN_HEIGHT - 60);

    es->flashFrames--;
    return (es->flashFrames <= 0) ? 1 : 0;
}