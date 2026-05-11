#include "player.h"
#include "font.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Player 1 uses normal player sprites */
static const char *STATE_FOLDERS_P1[STATE_COUNT] = { 
    "assets/sprites/idle/",   
    "assets/sprites/walk/", 
    "assets/sprites/sprint/", 
    "assets/sprites/jump/",
    "assets/sprites/shoot/",  
    "assets/sprites/death/"
};

/* Player 2 uses personage2 character sprites */
static const char *STATE_FOLDERS_P2[STATE_COUNT] = { 
    "assets/personage2/standing/",   
    "assets/personage2/walking/", 
    "assets/personage2/running/", 
    "assets/personage2/running/",
    "assets/personage2/fight/",  
    "assets/personage2/death/"
};

static const int FRAME_COUNTS_P1[STATE_COUNT] = {6, 6, 5, 5, 2, 4};
static const int FRAME_COUNTS_P2[STATE_COUNT] = {1, 5, 5, 5, 6, 5};
static const int FRAME_DELAYS[STATE_COUNT]    = {180,110,80,90,120,160};

static void chargerAnimation(Animation *anim, SDL_Renderer *renderer,
                              const char *folder, int count, int delay)
{
    anim->textures      = malloc(count * sizeof(SDL_Texture *));
    anim->frameCount    = count;
    anim->currentFrame  = 0;
    anim->lastFrameTime = SDL_GetTicks();
    anim->frameDelay    = delay;
    char path[256];
    
    int useFrame0 = 0;
    snprintf(path, sizeof(path), "%sframe0.png", folder);
    FILE *test = fopen(path, "r");
    if (test) { useFrame0 = 1; fclose(test); }
    
    for (int i = 0; i < count; i++) {
        if (useFrame0) {
            snprintf(path, sizeof(path), "%sframe%d.png", folder, i);
        } else {
            char prefix[4] = "f";
            if (strstr(folder, "standing")) snprintf(prefix, sizeof(prefix), "s");
            else if (strstr(folder, "walking")) snprintf(prefix, sizeof(prefix), "w");
            else if (strstr(folder, "running")) snprintf(prefix, sizeof(prefix), "r");
            else if (strstr(folder, "fight"))   snprintf(prefix, sizeof(prefix), "f");
            else if (strstr(folder, "death"))   snprintf(prefix, sizeof(prefix), "d");
            snprintf(path, sizeof(path), "%s%s%d.png", folder, prefix, i + 1);
        }
        
        SDL_Surface *s = IMG_Load(path);
        if (!s) {
            SDL_Surface *ph = SDL_CreateRGBSurface(0,
                PLAYER_W, PLAYER_PH, 32, 0,0,0,0);
            Uint32 col = SDL_MapRGB(ph->format,
                0, (i % 2) ? 200 : 80, (i % 2) ? 80 : 200);
            SDL_FillRect(ph, NULL, col);
            anim->textures[i] = SDL_CreateTextureFromSurface(renderer, ph);
            SDL_FreeSurface(ph);
        } else {
            anim->textures[i] = SDL_CreateTextureFromSurface(renderer, s);
            SDL_FreeSurface(s);
        }
    }
}

int initialiserJoueur(Player *p, SDL_Renderer *renderer,
                       PlayerID id, float startX, float startY)
{
    if (!p || !renderer) {
        fprintf(stderr, "[ERROR] Invalid player initialization parameters\n");
        return 1;
    }

    memset(p, 0, sizeof(Player));
    p->id        = id;
    p->worldX    = startX;
    p->worldY    = startY;
    p->isAlive   = 1;
    p->lives     = 3;
    p->health    = 100;
    p->lastDamageTime = SDL_GetTicks();
    p->damageEvent    = 0;
    p->camSmooth = 0.10f;
    p->camX      = startX - SCREEN_WIDTH / 2.0f;
    p->state     = STATE_IDLE;
    p->prevState = STATE_IDLE;
    p->direction = DIR_RIGHT;

    snprintf(p->name, sizeof(p->name),
             (id == PLAYER_1) ? "NEO" : "TRINITY");

    fprintf(stderr, "[INFO] Loading %s animations...\n", p->name);

    int loadFailed = 0;
    const char **stateFolders = (id == PLAYER_1) ? STATE_FOLDERS_P1 : STATE_FOLDERS_P2;
    const int  *frameCounts   = (id == PLAYER_1) ? FRAME_COUNTS_P1  : FRAME_COUNTS_P2;
    
    for (int i = 0; i < STATE_COUNT; i++) {
        chargerAnimation(&p->anims[i], renderer,
                         stateFolders[i], frameCounts[i], FRAME_DELAYS[i]);
        if (!p->anims[i].textures || p->anims[i].frameCount == 0) {
            fprintf(stderr, "[WARN] Animation state %d missing for %s\n", i, p->name);
            loadFailed = 1;
        }
    }

    if (loadFailed)
        fprintf(stderr, "[WARN] %s loaded with missing sprites\n", p->name);
    else
        fprintf(stderr, "[INFO] %s ready.\n", p->name);

    return 0;
}

void libererJoueur(Player *p)
{
    for (int i = 0; i < STATE_COUNT; i++) {
        if (!p->anims[i].textures) continue;
        for (int f = 0; f < p->anims[i].frameCount; f++)
            if (p->anims[i].textures[f])
                SDL_DestroyTexture(p->anims[i].textures[f]);
        free(p->anims[i].textures);
        p->anims[i].textures = NULL;
    }
}

void gererEvenementJoueur(Player *p, SDL_Event *e)
{
    if (e->type == SDL_KEYDOWN) {
        SDL_Keycode k = e->key.keysym.sym;
        if (p->id == PLAYER_1) {
            if (k == SDLK_q)      p->keyLeft   = 1;
            if (k == SDLK_d)      p->keyRight  = 1;
            if (k == SDLK_SPACE)  p->keyJump   = 1;
            if (k == SDLK_LSHIFT) p->isRunning = !p->isRunning;
            if (k == SDLK_LCTRL) { p->keyAttack = 1; tirerBalle(p); }
        } else {
            if (k == SDLK_LEFT)   p->keyLeft   = 1;
            if (k == SDLK_RIGHT)  p->keyRight  = 1;
            if (k == SDLK_RETURN) p->keyJump   = 1;
            if (k == SDLK_RSHIFT) p->isRunning = !p->isRunning;
            if (k == SDLK_RCTRL) { p->keyAttack = 1; tirerBalle(p); }
        }
    }
    if (e->type == SDL_KEYUP) {
        SDL_Keycode k = e->key.keysym.sym;
        if (p->id == PLAYER_1) {
            if (k == SDLK_q)     p->keyLeft  = 0;
            if (k == SDLK_d)     p->keyRight = 0;
            if (k == SDLK_SPACE) p->keyJump  = 0;
        } else {
            if (k == SDLK_LEFT)   p->keyLeft  = 0;
            if (k == SDLK_RIGHT)  p->keyRight = 0;
            if (k == SDLK_RETURN) p->keyJump  = 0;
        }
    }
    if (e->type == SDL_MOUSEBUTTONDOWN) {
        if (e->button.button == SDL_BUTTON_LEFT  && p->id == PLAYER_1)
            { p->keyAttack = 1; tirerBalle(p); }
        if (e->button.button == SDL_BUTTON_RIGHT && p->id == PLAYER_2)
            { p->keyAttack = 1; tirerBalle(p); }
    }
}

static void resoudreCollisions(Player *p, Platform *plats, int n)
{
    int touchedGround = 0;
    for (int i = 0; i < n; i++) {
        Platform *pl = &plats[i];
        if (pl->destroyed || pl->isVoid) continue;

        int px = (int)p->worldX, py = (int)p->worldY;
        int pw = PLAYER_W,       ph = PLAYER_PH;

        if (px + pw <= pl->rect.x) continue;
        if (px >= pl->rect.x + pl->rect.w) continue;
        if (py + ph <= pl->rect.y) continue;
        if (py >= pl->rect.y + pl->rect.h) continue;

        int ol  = (px + pw) - pl->rect.x;
        int or2 = (pl->rect.x + pl->rect.w) - px;
        int ot  = (py + ph) - pl->rect.y;
        int ob  = (pl->rect.y + pl->rect.h) - py;

        int mh = (ol < or2) ? ol : or2;
        int mv = (ot < ob)  ? ot : ob;

        if (mv <= mh) {
            /* Vertical resolution */
            if (ot < ob) {
                /* Landing on top — only resolve if actually moving downward
                   or barely overlapping. This prevents the "snap up" glitch
                   when jumping into the underside of a platform. */
                if (p->velY >= 0 || ot <= 4) {
                    p->worldY -= ot;
                    if (p->velY >= 0) { p->velY = 0; touchedGround = 1; }
                }
            } else {
                p->worldY += ob;
                if (p->velY < 0) p->velY = 0;
            }
        } else {
            /* Horizontal resolution */
            if (pl->type == PLAT_MOBILE) {
                Uint32 now = SDL_GetTicks();
                if (now - p->lastDamageTime > 600) {
                    perdreVie(p);
                    p->lastDamageTime = now;
                }
            }
            if (ol < or2) p->worldX -= ol;
            else          p->worldX += or2;
            p->velX = 0;
        }
    }

    if (touchedGround) {
        p->onGround     = 1;
        p->coyoteFrames = COYOTE_FRAMES;
    } else {
        if (p->coyoteFrames > 0) {
            p->coyoteFrames--;
            p->onGround = (p->coyoteFrames > 0) ? 1 : 0;
        } else {
            p->onGround = 0;
        }
    }
}

static void verifierPieges(Player *p, Platform *plats, int n)
{
    Uint32 now = SDL_GetTicks();
    for (int i = 0; i < n; i++) {
        Platform *pl = &plats[i];
        if (!pl->isVoid) continue;

        int playerCenterX = (int)(p->worldX + PLAYER_W  / 2);
        int playerCenterY = (int)(p->worldY + PLAYER_PH / 2);

        int touchesVoid =
            (playerCenterX >= pl->rect.x &&
             playerCenterX <= pl->rect.x + pl->rect.w &&
             playerCenterY >= pl->rect.y &&
             playerCenterY <= pl->rect.y + pl->rect.h);

        if (touchesVoid) {
            if (now - p->lastDamageTime > 500) {
                perdreVie(p);
                p->lastDamageTime = now;
                p->worldX = (float)(pl->rect.x - PLAYER_W - 10);
                p->worldY -= 60;
                p->velX = 0; p->velY = 0;
            }
            break;
        }
    }
}

static void determinerEtat(Player *p)
{
    if (!p->isAlive)              { p->state = STATE_DEATH;  return; }
    if (p->attackTimer > 0)       { p->state = STATE_SHOOT;  return; }
    int airborne = (!p->onGround && p->coyoteFrames == 0);
    if (airborne)                   p->state = STATE_JUMP;
    else if (p->isRunning && (p->keyLeft || p->keyRight))
                                    p->state = STATE_SPRINT;
    else if (p->keyLeft || p->keyRight)
                                    p->state = STATE_WALK;
    else                            p->state = STATE_IDLE;
}

static void deplacerJoueur(Player *p)
{
    if (!p->isAlive) return;
    float sp = p->isRunning ? RUN_SPEED : WALK_SPEED;
    if      (p->keyLeft  && !p->keyRight) { p->velX = -sp; p->direction = DIR_LEFT;  }
    else if (p->keyRight && !p->keyLeft)  { p->velX =  sp; p->direction = DIR_RIGHT; }
    else                                  { p->velX = 0; }

    if (p->keyJump && (p->onGround || p->coyoteFrames > 0)) {
        p->velY         = JUMP_FORCE;
        p->onGround     = 0;
        p->coyoteFrames = 0;
        p->keyJump      = 0;
    }
}

static void appliquerPhysique(Player *p)
{
    if (!p->isAlive) return;
    if (p->onGround && p->velY >= 0)
        p->velY = 0;
    else
        p->velY += PLAYER_GRAVITY;

    p->worldX += p->velX;
    p->worldY += p->velY;
    if (p->worldX < 0) { p->worldX = 0; p->velX = 0; }
    if (p->worldX > WORLD_WIDTH - PLAYER_W)
        { p->worldX = (float)(WORLD_WIDTH - PLAYER_W); p->velX = 0; }
    float solY = (float)(SCREEN_HEIGHT - PLAYER_PH - 2);
    if (p->worldY >= solY) { p->worldY = solY; p->velY = 0; p->onGround = 1; }
}

static void mettreAJourCamera(Player *p)
{
    float target = p->worldX - SCREEN_WIDTH * 0.4f;
    p->camX += (target - p->camX) * p->camSmooth;
    if (p->camX < 0) p->camX = 0;
    if (p->camX > WORLD_WIDTH - SCREEN_WIDTH)
        p->camX = (float)(WORLD_WIDTH - SCREEN_WIDTH);
}

static void mettreAJourAnimation(Player *p)
{
    Animation *a = &p->anims[p->state];
    if (!a || !a->textures || a->frameCount == 0) return;

    Uint32 now     = SDL_GetTicks();
    Uint32 elapsed = now - a->lastFrameTime;

    if (elapsed < (Uint32)a->frameDelay) return;

    Uint32 ticks = elapsed / (Uint32)a->frameDelay;
    a->lastFrameTime += ticks * (Uint32)a->frameDelay;

    if (p->state == STATE_DEATH) {
        int next = a->currentFrame + (int)ticks;
        a->currentFrame = (next < a->frameCount - 1) ? next : a->frameCount - 1;
    } else {
        a->currentFrame = (a->currentFrame + (int)ticks) % a->frameCount;
    }
}

/*
 * FIX 3 — GLITCH:
 * mettreAJourBalles() is called ONCE here, inside mettreAJourJoueur.
 * main.c must NOT call it a second time — that double-update was
 * causing bullets (and indirectly the player via collision response)
 * to move at twice the expected speed, producing the jitter/glitch.
 */
void mettreAJourJoueur(Player *p, Platform *platforms, int platCount)
{
    deplacerJoueur(p);
    appliquerPhysique(p);

    if (platforms && platCount > 0) {
        resoudreCollisions(p, platforms, platCount);
        verifierPieges(p, platforms, platCount);
    }

    if (p->attackTimer > 0) p->attackTimer--;

    determinerEtat(p);
    if (p->state != p->prevState) {
        p->anims[p->state].currentFrame  = 0;
        p->anims[p->state].lastFrameTime = SDL_GetTicks();
        p->prevState = p->state;
    }

    mettreAJourAnimation(p);
    mettreAJourCamera(p);

    /* Bullets updated ONCE here — do not call mettreAJourBalles from main.c */
    mettreAJourBalles(p);
}

void tirerBalle(Player *p)
{
    if (!p->isAlive) return;
    p->attackTimer = 14;
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!p->bullets[i].active) {
            p->bullets[i].active   = 1;
            p->bullets[i].dirRight = (p->direction == DIR_RIGHT);
            p->bullets[i].x  = p->worldX +
                (p->direction == DIR_RIGHT ? PLAYER_W : -BULLET_W);
            p->bullets[i].y  = p->worldY + PLAYER_PH / 2.0f;
            p->bullets[i].vx = (p->direction == DIR_RIGHT)
                                ? BULLET_SPEED : -BULLET_SPEED;
            p->bullets[i].vy = 0;
            break;
        }
    }
}

void mettreAJourBalles(Player *p)
{
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!p->bullets[i].active) continue;
        p->bullets[i].x += p->bullets[i].vx;
        p->bullets[i].y += p->bullets[i].vy;
        if (p->bullets[i].x < 0 || p->bullets[i].x > WORLD_WIDTH
         || p->bullets[i].y < 0 || p->bullets[i].y > WORLD_HEIGHT)
            p->bullets[i].active = 0;
    }
}

void afficherJoueur(Player *p, SDL_Renderer *renderer,
                    float camX, float camY)
{
    if (!p->isAlive && p->state != STATE_DEATH) return;
    Animation *a = &p->anims[p->state];
    if (!a->textures || !a->textures[a->currentFrame]) return;

    int sx = (int)(p->worldX - camX);
    int sy = (int)(p->worldY - camY);
    p->dstRect = (SDL_Rect){sx, sy, PLAYER_W, PLAYER_PH};

    SDL_RendererFlip flip = (p->direction == DIR_LEFT)
                          ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
    SDL_RenderCopyEx(renderer, a->textures[a->currentFrame],
                     NULL, &p->dstRect, 0, NULL, flip);

    int tw = textWidth(p->name, 1);
    drawText(renderer, sx + PLAYER_W/2 - tw/2, sy - 14,
             p->name, 1, 0, 255, 120);
}

void afficherBalles(Player *p, SDL_Renderer *renderer,
                    float camX, float camY)
{
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!p->bullets[i].active) continue;
        SDL_Rect r = {
            (int)(p->bullets[i].x - camX),
            (int)(p->bullets[i].y - camY),
            BULLET_W, BULLET_H_PX
        };
        SDL_SetRenderDrawColor(renderer, 0, 255, 100, 255);
        SDL_RenderFillRect(renderer, &r);
        int tx = r.x - (p->bullets[i].dirRight ? 8 : -8);
        SDL_Rect trail = {tx, r.y + 1, 8, BULLET_H_PX - 2};
        SDL_SetRenderDrawColor(renderer, 0, 160, 50, 180);
        SDL_RenderFillRect(renderer, &trail);
    }
}

void afficherHUDJoueur(Player *p, SDL_Renderer *renderer, int hx, int hy)
{
    Uint8 cr = 0;
    Uint8 cg = (p->id == PLAYER_1) ? 200 : 200;
    Uint8 cb = (p->id == PLAYER_1) ?  50 : 255;
    drawText(renderer, hx, hy, p->name, 1, cr, cg, cb);

    int barW = 160, barH = 10, bx = hx, by = hy + 12;
    SDL_SetRenderDrawColor(renderer, 40, 40, 40, 255);
    SDL_Rect bg = {bx, by, barW, barH};
    SDL_RenderFillRect(renderer, &bg);

    int vieW = (int)(barW * p->health / 100.0f);
    Uint8 gr = (p->health > 50) ? (Uint8)((100 - p->health) * 5) : 255;
    Uint8 gg = (p->health > 50) ? 255 : (Uint8)(p->health * 5);
    SDL_SetRenderDrawColor(renderer, gr, gg, 0, 255);
    SDL_Rect vie = {bx, by, vieW, barH};
    SDL_RenderFillRect(renderer, &vie);
    SDL_SetRenderDrawColor(renderer, 0, 180, 50, 255);
    SDL_RenderDrawRect(renderer, &bg);

    char buf[32];
    snprintf(buf, sizeof(buf), "SCR:%05d  VIE:%d", p->score, p->lives);
    drawText(renderer, hx, by + barH + 4, buf, 1, 0, 180, 80);

    if (p->isRunning)
        drawText(renderer, hx + 150, hy, "SPR", 1, 255, 200, 0);
}

void ajouterScore(Player *p, int pts) { p->score += pts; }

void perdreVie(Player *p)
{
    p->health -= 25;
    p->damageEvent = 1;
    if (p->health <= 0) {
        p->lives--;
        if (p->lives <= 0) {
            p->lives   = 0;
            p->health  = 0;
            p->isAlive = 0;
            p->state   = STATE_DEATH;
        } else {
            p->health = 50;
        }
    }
}