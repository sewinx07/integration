#ifndef PLAYER_MODULE_H
#define PLAYER_MODULE_H
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include "common.h"
#include "background.h" 
#define PLAYER_W        96
#define PLAYER_PH       96
#define WALK_SPEED      3.0f
#define RUN_SPEED       6.0f
#define JUMP_FORCE    -15.0f
#define PLAYER_GRAVITY  0.6f
#define COYOTE_FRAMES   4
#define MAX_BULLETS     16
#define BULLET_SPEED    10.0f
#define BULLET_W        12
#define BULLET_H_PX     5

typedef enum {
    STATE_IDLE = 0,
    STATE_WALK,
    STATE_SPRINT,
    STATE_JUMP,
    STATE_SHOOT,
    STATE_DEATH,
    STATE_COUNT
} PlayerState;

typedef enum { DIR_RIGHT = 0, DIR_LEFT } PlayerDirection;
typedef enum { PLAYER_1 = 0, PLAYER_2 = 1 } PlayerID;

typedef struct {
    SDL_Texture **textures;
    int           frameCount;
    int           currentFrame;
    Uint32        lastFrameTime;
    int           frameDelay;
} Animation;

typedef struct {
    float x, y, vx, vy;
    int   active;
    int   dirRight;
} Bullet;

typedef struct {
    PlayerID        id;
    char            name[32];
    float           worldX, worldY;
    float           velX, velY;
    int             onGround;
    int             coyoteFrames;
    int             isAlive;
    int             isRunning;
    PlayerState     state;
    PlayerState     prevState;
    PlayerDirection direction;
    int             lives;
    int             score;
    int             health;
    Uint32          lastDamageTime;
    int             damageEvent;
    int             attackTimer;
    Bullet          bullets[MAX_BULLETS];
    Animation       anims[STATE_COUNT];
    SDL_Rect        dstRect;
    int             keyLeft;
    int             keyRight;
    int             keyJump;
    int             keyAttack;
    float           camX;
    float           camSmooth;
} Player;

int  initialiserJoueur(Player *p, SDL_Renderer *renderer,PlayerID id, float startX, float startY);
void libererJoueur(Player *p);
void gererEvenementJoueur(Player *p, SDL_Event *e);
void mettreAJourJoueur(Player *p, Platform *platforms, int platCount);
void afficherJoueur(Player *p, SDL_Renderer *renderer,float camX, float camY);
void afficherHUDJoueur(Player *p, SDL_Renderer *renderer,int hx, int hy);
void afficherBalles(Player *p, SDL_Renderer *renderer,float camX, float camY);
void tirerBalle(Player *p);
void mettreAJourBalles(Player *p);
void ajouterScore(Player *p, int pts);
void perdreVie(Player *p);
#endif