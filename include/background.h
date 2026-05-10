/**
 * background.h - Module background MATRIX GAME
 */
#ifndef BACKGROUND_H
#define BACKGROUND_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include "common.h"

#define BG_SCALE_X          1.232f
#define MAX_PLATFORMS       48
#define MAX_SCORES          10
#define MAX_NAME_LEN        24
#define PLATFORM_THICKNESS  14
#define SCORES_FILE         "assets/scores.dat"

typedef enum { LEVEL_1 = 1, LEVEL_2 = 2 }           GameLevel;
typedef enum { PLAT_FIXED=0, PLAT_MOBILE=1,
               PLAT_DESTRUCTIBLE=2, PLAT_VOID=3 }    PlatformType;
typedef enum { MOVE_HORIZONTAL=0, MOVE_VERTICAL=1 }  MoveDir;
typedef enum { MODE_MONO=0, MODE_MULTI=1 }           DisplayMode;
typedef enum { GUIDE_HIDDEN=0, GUIDE_VISIBLE=1 }     GuideState;

typedef struct {
    SDL_Rect      rect;
    PlatformType  type;
    MoveDir       moveDir;
    float         speed;
    int           rangeMin, rangeMax;
    float         posF;
    int           moveSign;
    int           hits, maxHits;
    int           destroyed;
    Uint32        destroyTimer;
    SDL_Color     color;
    int           isVoid;
    char          label[32];
} Platform;

typedef struct {
    char  name[MAX_NAME_LEN];
    int   score;
    int   level;
    int   time;
} Score;

typedef struct {
    GuideState  state;
    SDL_Rect    rect;
    char        title[64];
    char        lines[8][128];
    int         lineCount;
    Uint32      showUntil;
    SDL_Color   bgColor;
    SDL_Color   textColor;
} GuideWindow;

typedef struct {
    GameLevel    level;
    SDL_Texture *texFull;
    int          srcActiveY;
    int          srcActiveH;
    int          tileW;
    float        camX, camY;
    float        camSmooth;
    Platform     platforms[MAX_PLATFORMS];
    int          platformCount;
    Uint32       startTime;
    int          elapsedSeconds;
    int          paused;
    Uint32       pauseStart;
    Score        scores[MAX_SCORES];
    int          scoreCount;
    GuideWindow  guide;
    GuideWindow  notification;
    DisplayMode  displayMode;
} Background;

int  initBackground(Background *bg, SDL_Renderer *renderer,
                    GameLevel level, DisplayMode mode);
void initGuide(GuideWindow *guide, GameLevel level);
void freeBackground(Background *bg);

void updateBackground(Background *bg);
void updateCamera(Background *bg, float targetX, float targetY);
void updatePlatforms(Background *bg);
void hitPlatform(Background *bg, int index);

void afficherBackground(Background *bg, SDL_Renderer *renderer,
                        DisplayMode mode, SDL_Rect *viewport);
void afficherPlateformes(Background *bg, SDL_Renderer *renderer);
void afficherTemps(Background *bg, SDL_Renderer *renderer, int x, int y);
void afficherGuide(Background *bg, SDL_Renderer *renderer);
void afficherNotification(Background *bg, SDL_Renderer *renderer);

void saisirNomJoueur(SDL_Renderer *renderer, char *outName);
void sauvegarderScore(Background *bg, const char *name, int score, int level);
void chargerScores(Background *bg);
void afficherMeilleursScores(Background *bg, SDL_Renderer *renderer);

void      worldToScreen(const Background *bg, float wx, float wy,
                        int *sx, int *sy);
Platform *getPlateformes(Background *bg, int *count);
void      setNotification(Background *bg, const char *msg, int durationMs);
void      togglePause(Background *bg);

#endif