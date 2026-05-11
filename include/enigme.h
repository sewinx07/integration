#ifndef ENIGME_H
#define ENIGME_H

/**
 * enigme.h — QCM + Puzzle mini-game triggered on player death.
 *
 * Flow:
 *   APP_ENIGME_CHOICE  → player picks QCM or PUZZLE
 *   APP_ENIGME_QCM     → 4-choice question, 30 s timer
 *   APP_ENIGME_PUZZLE  → 4-tile reorder puzzle
 *
 *   Correct  → enigmeResult = ENIGME_WIN  → caller restores 1 life
 *   Wrong    → enigmeResult = ENIGME_LOSE → caller ends game
 */

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>

/* ── result token set by runEnigme*() ───────────────────────────── */
typedef enum {
    ENIGME_PENDING = 0,
    ENIGME_WIN,
    ENIGME_LOSE
} EnigmeResult;

/* ── one QCM question ───────────────────────────────────────────── */
#define MAX_QUESTIONS  50
#define QCM_TIME_SEC   30

typedef struct {
    char question[200];
    char rep[4][80];    /* rep[0..3] */
    int  bonne;         /* 1-based correct answer index */
    int  deja_vu;
} Question;

/* ── enigme session state (kept alive for the duration) ─────────── */
typedef struct {
    /* assets */
    SDL_Texture *bgTex;       /* assets/enigme/background.png  */
    SDL_Texture *successTex;  /* assets/enigme/success.png     */
    SDL_Texture *failTex;     /* assets/enigme/fail.png        */
    SDL_Texture *btnHoverTex; /* assets/enigme/btn_qcm_hover.png */

    /* font (borrowed from main — not freed here) */
    TTF_Font    *font;
    TTF_Font    *fontSmall;

    /* QCM data */
    Question     questions[MAX_QUESTIONS];
    int          nbQ;
    int          currentIdx;

    /* timer */
    Uint32       startTime;
    int          timeLeft;

    /* puzzle data */
    int          tiles[4];          /* permutation of {0,1,2,3}  */
    int          selectedTile;      /* -1 = none selected        */
    int          puzzleTarget[4];   /* correct order {0,1,2,3}   */

    /* result */
    EnigmeResult result;

    /* flash animation */
    int          flashFrames;
    int          flashSuccess;      /* 1=green, 0=red            */
} EnigmeSession;

/* ── lifecycle ──────────────────────────────────────────────────── */

/**
 * Initialise an EnigmeSession.
 * @param font      main TTF font (28 pt)
 * @param fontSmall smaller TTF font (20 pt) — may equal font
 */
void enigmeInit(EnigmeSession *es, SDL_Renderer *ren,
                TTF_Font *font, TTF_Font *fontSmall);

/** Free textures loaded inside the session (fonts are NOT freed). */
void enigmeFree(EnigmeSession *es);

/* ── per-screen entry points ────────────────────────────────────── */

/** Render the CHOICE screen (QCM / PUZZLE buttons).
 *  Returns 0=QCM clicked, 1=PUZZLE clicked, -1=nothing yet. */
int enigmeRenderChoice(EnigmeSession *es, SDL_Renderer *ren,
                        SDL_Event *ev, int mx, int my);

/** Pick a random question and reset the timer. Call once before
 *  switching to APP_ENIGME_QCM. */
void enigmeStartQCM(EnigmeSession *es);

/** Update + render QCM screen.
 *  Sets es->result on answer or timeout; returns it. */
EnigmeResult enigmeUpdateQCM(EnigmeSession *es, SDL_Renderer *ren,
                              SDL_Event *ev);

/** Initialise the puzzle (shuffle tiles). Call once before
 *  switching to APP_ENIGME_PUZZLE. */
void enigmeStartPuzzle(EnigmeSession *es);

/** Update + render PUZZLE screen.
 *  Sets es->result when solved or player gives up (ESC); returns it. */
EnigmeResult enigmeUpdatePuzzle(EnigmeSession *es, SDL_Renderer *ren,
                                 SDL_Event *ev);

/** Show the result splash (success.png / fail.png) for ~1.5 s.
 *  Call every frame until it returns 1 (done). */
int enigmeRenderResult(EnigmeSession *es, SDL_Renderer *ren);

#endif /* ENIGME_H */