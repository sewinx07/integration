#include "matrix.h"
#include <stdlib.h>

#define RAIN_COLS 120
#define RAIN_SPEED_MIN 140
#define RAIN_SPEED_MAX 420
#define RAIN_STEP 20
#define RAIN_LEVELS 8

typedef struct Col { float y; float speed; int trail; int x; } Col;
static Col cols[RAIN_COLS];
static SDL_Texture* DIGIT[2][RAIN_LEVELS];
static SDL_Color DARK = {0, 10, 0, 255};
static SDL_Color HEAD = {200, 255, 200, 255};

static SDL_Texture* make_digit(SDL_Renderer* r, TTF_Font* f, const char* c, SDL_Color col) {
    SDL_Surface* s = TTF_RenderUTF8_Blended(f, c, col);
    SDL_Texture* t = SDL_CreateTextureFromSurface(r, s);
    SDL_FreeSurface(s);
    return t;
}

void matrix_init(SDL_Renderer* r, int w, int h, TTF_Font* fm) {
    /* FIX [MATRIX-LEAK-1]:
     * Previously the loop created textures for all RAIN_LEVELS including l==0,
     * then the HEAD overwrite replaced DIGIT[0][0] and DIGIT[1][0] with new
     * textures — leaking the originals.  Now level 0 (the head / brightest
     * drop) is initialised directly with the HEAD colour and the loop starts
     * at l=1 for the dimming trail colours. */

    /* Level 0: head — brightest white-green */
    DIGIT[0][0] = make_digit(r, fm, "0", HEAD);
    DIGIT[1][0] = make_digit(r, fm, "1", HEAD);

    /* Levels 1..RAIN_LEVELS-1: progressively dimmer trail */
    for (int l = 1; l < RAIN_LEVELS; l++) {
        Uint8 g = 255 - l * 28;
        if (g < 40) g = 40;
        SDL_Color c = {0, g, 0, 255};
        DIGIT[0][l] = make_digit(r, fm, "0", c);
        DIGIT[1][l] = make_digit(r, fm, "1", c);
    }

    for (int i = 0; i < RAIN_COLS; i++) {
        cols[i].x     = (i * w) / RAIN_COLS;
        cols[i].y     = rand() % h;
        cols[i].speed = RAIN_SPEED_MIN + rand() % (RAIN_SPEED_MAX - RAIN_SPEED_MIN);
        cols[i].trail = 10 + rand() % 18;
    }
}

void matrix_update(float dt, int h) {
    for (int i = 0; i < RAIN_COLS; i++) {
        cols[i].y += cols[i].speed * dt;
        if (cols[i].y - cols[i].trail * RAIN_STEP > h) {
            cols[i].y     = -(rand() % 200);
            cols[i].speed = RAIN_SPEED_MIN + rand() % (RAIN_SPEED_MAX - RAIN_SPEED_MIN);
            cols[i].trail = 10 + rand() % 18;
        }
    }
}

void matrix_render(SDL_Renderer* r, int w) {
    (void)w;
    SDL_SetRenderDrawColor(r, DARK.r, DARK.g, DARK.b, 255);
    SDL_RenderClear(r);
    for (int i = 0; i < RAIN_COLS; i++) {
        int x     = cols[i].x;
        int headY = (int)cols[i].y;

        for (int t = 0; t <= cols[i].trail; t++) {
            int y = headY - t * RAIN_STEP;
            if (y < -RAIN_STEP) continue;
            int lvl = (t >= RAIN_LEVELS) ? RAIN_LEVELS - 1 : t;
            SDL_Texture* tex = DIGIT[rand() & 1][lvl];
            int tw, th;
            SDL_QueryTexture(tex, NULL, NULL, &tw, &th);
            SDL_Rect dst = {x, y, tw, th};
            SDL_RenderCopy(r, tex, NULL, &dst);
        }
    }
}

void matrix_free() {
    for (int d = 0; d < 2; d++)
        for (int l = 0; l < RAIN_LEVELS; l++)
            if (DIGIT[d][l]) SDL_DestroyTexture(DIGIT[d][l]);
}