#include <math.h>
#include <stdarg.h>
#include "emscripten.h"
#include <iostream>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include <emscripten/html5.h>
#include <vector>
#include <algorithm>
#include <SDL2/SDL_ttf.h>
#include <string>
#include <chrono>
#include <sstream>




#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define PRESS_SPEED 10
#define DIAG_SPEED 7
#define ENEMY_SPEED 4
static constexpr uint8_t U8_INVALID = -1;
#define FONT_SIZE 24

SDL_Window *window;
SDL_Renderer *renderer;
SDL_Texture* background;
TTF_Font* font;

struct Entity;
struct MainCharacter; struct SkeletonEnemy; struct DeathBossEnemy;
struct Animation;
enum class ENT_TYPES : uint8_t
{
    PLAYER,
    SKELETON,
    DEATH_BOSS,
    NUM_TYPES,
};
enum ENT_FLAGS : uint8_t
{
    LOOK_RIGHT = 1,
    HIT_CHECKED = 2,
    DEAD = 4,
    CLEAN_UP = 8,           // will be removed at the end of the frame
    SPRITE_INVERSE = 16,    // set if the entity has a inverse sprite layout
    COMBO_ATTACK_AV = 32,   // set if combo attack is available
};
Animation* GetAnimationFromType(ENT_TYPES typ);
SDL_Rect* GetAttackHitboxFromType(ENT_TYPES typ);
SDL_Rect GetHitBoxFromType(ENT_TYPES typ);
SDL_Rect GetAttackHitboxFromEntity(Entity* ent, int atkId);
void SetDirection(Entity* ent);


struct Entity
{
    Entity(){}
    Entity(int cx, int cy, int hp, int startAnim, ENT_TYPES type, uint8_t flag)
    {
        centerX = cx; centerY = cy;
        health = hp;
        activeAnim = startAnim;
        moveX = 0; moveY = 0; frameIdx = 0;
        typeID = type; flags = flag;
    }
    int centerX = 0;
    int centerY = 0;
    int health = 100;   // 100 is default health
    int16_t moveX = 0;
    int16_t moveY = 0;
    uint8_t frameIdx = 0;
    uint8_t activeAnim = 0;
    ENT_TYPES typeID = ENT_TYPES::SKELETON;
    uint8_t flags = ENT_FLAGS::LOOK_RIGHT;

    void MoveEntity()
    {
        centerX += moveX;
        centerY += moveY;
    }
    bool ToRight()
    {
        return flags & 1;
    }
    void SetFlag(ENT_FLAGS flag)
    {
        flags |= flag;
    }
    void ClearFlag(ENT_FLAGS flag)
    {
        flags &= ~flag;
    }
    void FlipFlag(ENT_FLAGS flag)
    {
        flags ^= flag;
    }
    bool CanMove();
    bool AnimIsAttack();
    void Attack(int attackAnimID);
    SDL_Rect GetHitbox();
    void TakeDmg(int dmg, int hitAnim, int deathAnim);
    void Draw();
};
struct EntityList
{
    std::vector<Entity> vec;
    Entity* player = 0;
    bool _dirty = false;
    void Sort()
    {
        if(_dirty)
        {
            std::vector<Entity> newEntitys;
            for(int i = 0; i < vec.size(); i++)
            {
                if(!(vec.at(i).flags & ENT_FLAGS::CLEAN_UP))
                {
                    newEntitys.push_back(vec.at(i));
                }
            }
            vec = std::move(newEntitys);
        }
        player = nullptr;
        std::sort(vec.begin(), vec.end(), [](Entity& e1, Entity& e2) { return e1.centerY < e2.centerY; });
        for(int i = 0; i < vec.size(); i++){
            if(vec.at(i).typeID == ENT_TYPES::PLAYER) {
                player = &vec.at(i);
                break;
            }
        }
    }
    void Recreate();
    void AddRandomEnemy();
};
static EntityList entList;
static MainCharacter* mainInfo;
static SkeletonEnemy* skelInfo;
static DeathBossEnemy* deathBossInfo;
static SDL_Rect retryRect;
static SDL_Rect healthBar;
static bool canRetry = false;
static float scale = 2.0f;
static int currentScore = 0;
static float currentTime = 0;
static int areaStartX = 0;
static int areaStartY = 0;
static int screenWidth = 0;
static int screenHeight = 0;
static int g_gameStateCounter = 0;
static constexpr bool DRAW_DEBUG = false;
static constexpr bool DRAW_FPS = false;






void DrawCircle(int32_t centreX, int32_t centreY, int32_t radius)
{
   const int32_t diameter = (radius * 2);

   int32_t x = (radius - 1);
   int32_t y = 0;
   int32_t tx = 1;
   int32_t ty = 1;
   int32_t error = (tx - diameter);

   while (x >= y)
   {
      SDL_RenderDrawPoint(renderer, centreX + x, centreY - y);
      SDL_RenderDrawPoint(renderer, centreX + x, centreY + y);
      SDL_RenderDrawPoint(renderer, centreX - x, centreY - y);
      SDL_RenderDrawPoint(renderer, centreX - x, centreY + y);
      SDL_RenderDrawPoint(renderer, centreX + y, centreY - x);
      SDL_RenderDrawPoint(renderer, centreX + y, centreY + x);
      SDL_RenderDrawPoint(renderer, centreX - y, centreY - x);
      SDL_RenderDrawPoint(renderer, centreX - y, centreY + x);

      if (error <= 0)
      {
         ++y;
         error += ty;
         ty += 2;
      }

      if (error > 0)
      {
         --x;
         tx += 2;
         error += (tx - diameter);
      }
   }
}
void DrawText(const char* txt, int xPos, int yPos, int width, int r, int g, int b, int a, bool center)
{
    SDL_Color col;
    col.r = r; col.g = g; col.b = b; col.a = a;
    SDL_Surface* surface = TTF_RenderText_Blended(font, txt, col);
    col.r = 0; col.g = 0; col.b = 0; col.a = a;
    SDL_Surface* surface2 = TTF_RenderText_Blended(font, txt, col);
    SDL_Texture* message = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_Texture* message2 = SDL_CreateTextureFromSurface(renderer, surface2);
    int extend; int count;
    TTF_MeasureText(font, txt, width, &extend, &count);

    SDL_Rect textRect;
    textRect.x = xPos + (center ? width / 2 - extend / 2 : 0);
    textRect.y = yPos;
    textRect.h = FONT_SIZE * 3 / 2;
    textRect.w = extend;
    SDL_RenderCopy(renderer, message2, NULL, &textRect);
    textRect.x -= 2;
    textRect.y -= 2;
    SDL_RenderCopy(renderer, message, NULL, &textRect);

    
    SDL_FreeSurface(surface);
    SDL_DestroyTexture(message);

    SDL_FreeSurface(surface2);
    SDL_DestroyTexture(message2);
}
void DrawHealthBar(const SDL_Rect& bar, float hp)
{
    hp = hp / 100.0f;
    SDL_Rect healthyRect = bar;
    healthyRect.w *= hp;
    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
    SDL_RenderFillRect(renderer, &healthyRect);
    healthyRect.x += healthyRect.w;
    healthyRect.w = bar.w - healthyRect.w;
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    SDL_RenderFillRect(renderer, &healthyRect);
}
bool RectangleCollisionTest(const SDL_Rect& rect1, const SDL_Rect& rect2)
{
    if (rect1.x < rect2.x + rect2.w && rect1.x + rect1.w > rect2.x && rect1.y < rect2.y + rect2.h && rect1.h + rect1.y > rect2.y) {
        return true;
    }
    return false;
}


struct MobileControlCircle
{
    int posX;
    int posY;
    int rad;
    bool isInside(int x, int y)
    {
        x -= posX; y -= posY;
        if((x * x + y * y) < rad * rad)
        {
            return true;
        }
        return false;
    }
};
struct Input
{
    Input(int width, int height) : state{false}, attackStates{false}
    {
        LeftAttack.posX = width - 280;
        LeftAttack.posY = height - 100;
        LeftAttack.rad = 80;
        RightAttack.posX = width - 100;
        RightAttack.posY = height - 100;
        RightAttack.rad = 80;
        MoveCircle.rad = 80;
    }
    ~Input() {}
    enum Btns
    {
        BTN_W,
        BTN_A,
        BTN_S,
        BTN_D,
        BTN_UP,
        BTN_LEFT,
        BTN_DOWN,
        BTN_RIGHT,


        NUM_BUTTONS,
    };
    void ButtonSetState(SDL_Scancode key, bool down)
    {
        touched = false;
        switch(key)
        {
        case SDL_SCANCODE_W:
            state[Btns::BTN_W] = down;
            break;
        case SDL_SCANCODE_A:
            state[Btns::BTN_A] = down;
            break;
        case SDL_SCANCODE_S:
            state[Btns::BTN_S] = down;
            break;
        case SDL_SCANCODE_D:
            state[Btns::BTN_D] = down;
            break;
        case SDL_SCANCODE_UP:
            state[Btns::BTN_UP] = down;
            break;
        case SDL_SCANCODE_LEFT:
            state[Btns::BTN_LEFT] = down;
            break;
        case SDL_SCANCODE_DOWN:
            state[Btns::BTN_DOWN] = down;
            break;
        case SDL_SCANCODE_RIGHT:
            state[Btns::BTN_RIGHT] = down;
            break;
        default:
            break;
        }
    }
    void MouseButtonSetState(const EmscriptenMouseEvent* e, bool down)
    {
        switch(e->button)
        {
        case 0: // LEFT
            attackStates[0] = down;
            break;
        case 2: // RIGHT
            attackStates[1] = down;
            break;
        }
    }
    void TouchButtonSetState(const EmscriptenTouchEvent* e, bool down, bool move)
    {
        touched = true;
        if(move && moveCircleIdx != -1)
        {
            for(int i = 0; i < e->numTouches; i++)
            {
                EmscriptenTouchPoint p = e->touches[i];
                if(p.identifier == moveCircleIdx)
                {
                    moveCircTouchX = p.pageX;
                    moveCircTouchY = p.pageY;
                    break;
                }
            }
        }
        else
        {
            for(int i =  0; i < e->numTouches; i++)
            {
                bool anyHit = false;
                EmscriptenTouchPoint p = e->touches[i];
                if(!p.isChanged) continue;

                if(LeftAttack.isInside(p.pageX, p.pageY)) {
                    attackStates[0] = down;
                    anyHit = true;
                }
                if(RightAttack.isInside(p.pageX, p.pageY)) {
                    attackStates[1] = down;
                    anyHit = true;
                }
                if(!anyHit && moveCircleIdx == -1)
                {
                    MoveCircle.posX = p.pageX; MoveCircle.posY = p.pageY;
                    moveCircTouchX = p.pageX;
                    moveCircTouchY = p.pageY;
                    moveCircleIdx = p.identifier;
                }
                if((moveCircleIdx == p.identifier && !down))
                {
                    moveCircleIdx = -1;
                }
            }
        }
    }
    
    bool IsPressed(Btns btn)
    {
        if(btn < 8) // movement keys
        {
            Btns other = BTN_W;
            if(btn < 4){ other = (Btns)(btn + 4); }
            else { other = (Btns)(btn - 4); }
            return state[btn] | state[other];
        }
        return false;
    }
    void GetMoveXY(int16_t& moveX, int16_t& moveY)
    {
        if(moveCircleIdx == -1)
        {
            if(IsPressed(Input::BTN_UP) == IsPressed(Input::BTN_DOWN))
            {
                moveY = 0;
            }
            else if(IsPressed(Input::BTN_UP)){
                moveY = -PRESS_SPEED;
            }
            else if(IsPressed(Input::BTN_DOWN))
            {
                moveY = PRESS_SPEED;
            }
            if(IsPressed(Input::BTN_LEFT) == IsPressed(Input::BTN_RIGHT))
            {
                moveX = 0;
            }
            else if(IsPressed(Input::BTN_LEFT)){
                moveX = -PRESS_SPEED;
            }
            else if(IsPressed(Input::BTN_RIGHT))
            {
                moveX = PRESS_SPEED;
            }
            if(moveX && moveY)
            {
                if(moveX < 0) moveX = -DIAG_SPEED;
                else moveX = DIAG_SPEED;
                if(moveY < 0) moveY = -DIAG_SPEED;
                else moveY = DIAG_SPEED;
            }
        }
        else
        {
            float dx = (moveCircTouchX - MoveCircle.posX);
            float dy = (moveCircTouchY - MoveCircle.posY);
            float dist = sqrtf(dx * dx + dy * dy);
            float inv_dst = 1.0f / dist;
            float frad = MoveCircle.rad;
            dx *= inv_dst; dy *= inv_dst;
            if(dist > frad){
                dist = frad;
            }
            dx *= (dist / frad) * PRESS_SPEED; dy *= (dist / frad) * PRESS_SPEED;
            if(abs(dx) > 1.0){
                moveX = dx;
            }
            if(abs(dy) > 1.0){
                moveY = dy;
            }

        }
    }
    
    void DrawMobileControl()
    {
        if(touched)
        {
            if(moveCircleIdx != -1)
            {
                SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
                DrawCircle(MoveCircle.posX, MoveCircle.posY, MoveCircle.rad);
            }
            if(attackStates[0]){
                SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
            }
            else
            {
                SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            }
            DrawCircle(LeftAttack.posX, LeftAttack.posY, LeftAttack.rad);
            if(attackStates[1] != attackStates[0])
            {
                if(attackStates[1]){
                    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
                }
                else
                {
                    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
                }
            }
            DrawCircle(RightAttack.posX, RightAttack.posY, RightAttack.rad);
        }
    }
    bool attackStates[2];
private:
    int moveCircTouchX;
    int moveCircTouchY;
    MobileControlCircle MoveCircle;
    int moveCircleIdx = -1;
    MobileControlCircle LeftAttack;
    MobileControlCircle RightAttack;
    bool state[Btns::NUM_BUTTONS];
    bool touched = false;
};
Input* userInput;



SDL_Texture* LoadTextureFromFile(const char* filename, int* outx, int* outy)
{
    SDL_Texture* result = nullptr;
     int x,y,n,ok;
    ok = stbi_info(filename, &x, &y, &n);
    if(outx) *outx = x;
    if(outy) *outy = y;
    if(ok)
    {
        unsigned char* res = stbi_load(filename, &x, &y, &n, 4);
        
        SDL_Surface* surf = SDL_CreateRGBSurfaceFrom((void*)res, x, y, 32, 4 * x, 0xFF, 0xFF00, 0xFF0000, 0xFF000000);
        if(surf)
        {
            result = SDL_CreateTextureFromSurface(renderer, surf);
        }
        else{
             std::cout << filename << " ERROR FAILED TO LOAD ANIMATION CreateRBGSurfaceFrom failed" << std::endl;
        }
        SDL_FreeSurface(surf);
        stbi_image_free((void*)res);
    }
    else
    {
        std::cout << filename << " faild to load" << std::endl;
    }
    return result;
}


struct Animation
{
    Animation(const char* filename, int nFrames, int numInColumn)
    {
        offsetX = 0;
        offsetY = 0;
        numFrames = nFrames;
        int sizeX, sizeY;
        columnCount = numInColumn;
        animTex = LoadTextureFromFile(filename, &sizeX, &sizeY);
        sx = sizeX / numInColumn;
        int numRows = nFrames / numInColumn;
        sy = sizeY / numRows;
    }
    void DrawIndex(int cx, int cy, int frameIdx, bool toRight)
    {
        frameIdx = frameIdx % numFrames;
        int offX = toRight ? -offsetX : offsetX;
        SDL_Rect src= GetRectFromIdx(frameIdx);
        SDL_Rect dst = src; dst.x += cx - src.x - (sx/2 + offX) * scale; dst.y += cy - (sy/2 + offsetY) * scale;
        dst.w *= scale; dst.h *= scale;
        
        int res = SDL_RenderCopyEx(renderer, animTex, &src, &dst, 0.0, nullptr, toRight ? SDL_RendererFlip::SDL_FLIP_NONE : SDL_RendererFlip::SDL_FLIP_HORIZONTAL);
        if(res < 0)
        {
            //std::cout << "renderer Failed" << std::endl;
        }
    }  
    void SetOffset(int16_t xOff, int16_t yOff)
    {
        offsetX= xOff; offsetY = yOff;
    }

    int numFrames; 
private:
    SDL_Rect GetRectFromIdx(int frameIdx)
    {
        SDL_Rect outRect;
        outRect.x = sx * (frameIdx % columnCount);
        outRect.y = sy * (frameIdx / columnCount);
        outRect.w = sx;
        outRect.h = sy;
        return outRect;
    }

    int columnCount;
    SDL_Texture* animTex;
    int16_t offsetX; int16_t offsetY;
    int16_t sx; int16_t sy;
};
void PlayerHitCallback(Entity* player, Entity* hitter, int atkIdx);
void EnemyHitCallback(Entity* enemy, Entity* player, int atkIdx);



struct MainCharacter
{
    enum CharAnims : uint8_t
    {
        Attack1,
        Attack2,
        Attack3,
        Attack4,
        Death,
        Fall,
        Idle,
        Jump,
        Run,
        TakeHitSilhoutte,
        TakeHit,
        NUM_ANIMATIONS,
    };
    MainCharacter() : anims{{"Assets/MainChar/Attack1.png", 4, 4}, 
    {"Assets/MainChar/Attack2.png", 4, 4}, 
    {"Assets/MainChar/Attack3.png", 4, 4}, 
    {"Assets/MainChar/Attack4.png", 4, 4},
    {"Assets/MainChar/Death.png", 6, 6}, 
    {"Assets/MainChar/Fall.png", 2, 2}, 
    {"Assets/MainChar/Idle.png", 8, 8}, 
    {"Assets/MainChar/Jump.png", 2, 2}, 
    {"Assets/MainChar/Run.png", 8, 8}, 
    {"Assets/MainChar/Take Hit - white silhouette.png", 4, 4}, 
    {"Assets/MainChar/Take Hit.png", 4, 4}}
    {
        AtkRects[0].x = 0; AtkRects[0].y = -20 * scale; AtkRects[0].w = 50 * scale; AtkRects[0].h = 40 * scale;
        AtkRects[1].x = 0; AtkRects[1].y = -60 * scale + hboxH / 2 * scale; AtkRects[1].w = 70 * scale; AtkRects[1].h = 60 * scale;
        AtkRects[2].x = -hboxW * scale; AtkRects[2].y = -5 * scale; AtkRects[2].w = 80 * scale; AtkRects[2].h = 10 * scale;
        AtkRects[3].x = 0; AtkRects[3].y = -60 * scale + hboxH / 2 * scale; AtkRects[3].w = 70 * scale; AtkRects[3].h = 60 * scale;

        HitBox.x = hboxW/2 * scale; HitBox.y = hboxH/2 * scale;
        HitBox.w = hboxW * scale; HitBox.h = hboxH * scale;
    }
    

    static constexpr int hboxW = 20;
    static constexpr int hboxH = 40;

    SDL_Rect HitBox;
    SDL_Rect AtkRects[4];
    Animation anims[NUM_ANIMATIONS];
};
struct SkeletonEnemy
{
    enum SkeletonAnims
    {
        Attack1,
        Attack2,
        Corpse,
        DeadFar,
        DeadNear,
        Hit,
        Jump,
        Ready,
        Reborn,
        Run,
        Walk,
        NUM_ANIMATIONS,
    };
    SkeletonEnemy() : anims{
        {"Assets/Skeleton/attack1.png", 6, 6},
        {"Assets/Skeleton/attack2.png", 6, 6},
        {"Assets/Skeleton/corpse.png", 2, 2},
        {"Assets/Skeleton/dead_far.png", 6, 6},
        {"Assets/Skeleton/dead_near.png", 6, 6},
        {"Assets/Skeleton/hit.png", 3, 3},
        {"Assets/Skeleton/jump.png", 5, 5},
        {"Assets/Skeleton/ready.png", 3, 3},
        {"Assets/Skeleton/reborn.png", 3, 3},
        {"Assets/Skeleton/run.png", 6, 6},
        {"Assets/Skeleton/walk.png", 6, 6},
    }
    {
        anims[0].SetOffset(10,10); anims[1].SetOffset(10,10);
        AtkRects[0].x = -hboxW / 2 * scale - 5 * scale; AtkRects[0].y = -20 * scale; AtkRects[0].w = 50 * scale; AtkRects[0].h = 40 * scale;
        AtkRects[1].x = -hboxW / 2 * scale - 5 * scale; AtkRects[1].y = -20 * scale; AtkRects[1].w = 50 * scale; AtkRects[1].h = 40 * scale;


        HitBox.x = hboxW / 2 * scale; HitBox.y = hboxH / 2 * scale;
        HitBox.w = hboxW * scale; HitBox.h = hboxH * scale;
    }


    static constexpr int attackOffset = 10;
    static constexpr int hboxW = 20;
    static constexpr int hboxH = 40;

    SDL_Rect HitBox;
    SDL_Rect AtkRects[2];
    Animation anims[NUM_ANIMATIONS];
};


struct DeathBossEnemy
{
    enum DeathBossAnims
    {
        Attack,
        Cast,
        Death,
        Hurt,
        Idle,
        Spell,
        Walk,
        NUM_ANIMATIONS,
    };
    DeathBossEnemy() : anims{
        {"Assets/Death-Boss/Attack.png", 10, 10},
        {"Assets/Death-Boss/Cast.png", 9, 9},
        {"Assets/Death-Boss/Death.png", 3, 3},
        {"Assets/Death-Boss/Hurt.png", 8, 8},
        {"Assets/Death-Boss/Idle.png", 8, 8},
        {"Assets/Death-Boss/Spell.png", 16, 16},
        {"Assets/Death-Boss/Walk.png", 8, 8},
    }
    {
    }

    SDL_Rect HitBox;
    SDL_Rect AtkRects[2];
    Animation anims[DeathBossAnims::NUM_ANIMATIONS];
};


struct PlayerEntity : public Entity
{
    static Entity Create(int cx, int cy) {
        Entity res(cx, cy, 100, (int)MainCharacter::Idle, ENT_TYPES::PLAYER, (uint8_t)(ENT_FLAGS::LOOK_RIGHT));
        return res;
    }
};
struct SkeletonEntity : public Entity
{
    static Entity Create(int cx, int cy) {
        Entity res(cx, cy, 100, (int)SkeletonEnemy::Ready, ENT_TYPES::SKELETON, (uint8_t)(ENT_FLAGS::LOOK_RIGHT));
        return res;
    }
};
struct DeathBossEntity : public Entity
{
    static Entity Create(int cx, int cy){
        Entity res(cx, cy, 100, (int)DeathBossEnemy::Idle, ENT_TYPES::DEATH_BOSS, (uint8_t)(ENT_FLAGS::LOOK_RIGHT | ENT_FLAGS::SPRITE_INVERSE));
        return res;
    }
};



Animation* GetAnimationFromType(ENT_TYPES typ)
{
    switch(typ)
    {
    case ENT_TYPES::PLAYER: return mainInfo->anims;
    case ENT_TYPES::SKELETON: return skelInfo->anims;
    case ENT_TYPES::DEATH_BOSS: return deathBossInfo->anims;
    default:
        break;
    }
    return nullptr;
}
SDL_Rect* GetAttackHitboxFromType(ENT_TYPES typ)
{
    switch(typ)
    {
    case ENT_TYPES::PLAYER: return mainInfo->AtkRects;
    case ENT_TYPES::SKELETON: return skelInfo->AtkRects;
    case ENT_TYPES::DEATH_BOSS: return deathBossInfo->AtkRects;
    default:
        break;
    }
    return nullptr;
}
SDL_Rect GetHitBoxFromType(ENT_TYPES typ)
{
    SDL_Rect result;result.x = 0; result.y = 0; result.w = 0; result.h = 0;
    switch(typ)
    {
    case ENT_TYPES::PLAYER: 
        result = mainInfo->HitBox;
        break;
    case ENT_TYPES::SKELETON: 
        result = skelInfo->HitBox;
        break;
    case ENT_TYPES::DEATH_BOSS: 
        result = deathBossInfo->HitBox;
        break;
    default:
        break;
    }
    return result;
}
SDL_Rect GetAttackHitboxFromEntity(Entity* ent, int atkId)
{
    SDL_Rect* av = GetAttackHitboxFromType(ent->typeID);
    SDL_Rect result;result.x=0;result.y=0;result.w=0;result.h=0;
    if(av)
    {
        result = av[atkId];
        if(!ent->ToRight())
        {
            result.x = -result.x;
            result.x -= result.w;
        }
        result.x += ent->centerX; result.y += ent->centerY;
    }
    return result;
}
void SetDirection(Entity* ent)
{

    if(ent->moveX > 0) {
        if(ent->flags & ENT_FLAGS::SPRITE_INVERSE) ent->ClearFlag(ENT_FLAGS::LOOK_RIGHT);  
        else ent->SetFlag(ENT_FLAGS::LOOK_RIGHT);
    }
    else if(ent->moveX < 0)
    {
        if(ent->flags & ENT_FLAGS::SPRITE_INVERSE) ent->SetFlag(ENT_FLAGS::LOOK_RIGHT);  
        else ent->ClearFlag(ENT_FLAGS::LOOK_RIGHT);
    }
}

void HitTestAllEnemys(Entity* source, int atkID, const SDL_Rect& hitRec)
{
    for(int i = 0; i < entList.vec.size(); i++)
    {
        if(entList.player == &entList.vec.at(i)) continue;
        Entity* hit = &entList.vec.at(i);
        if(hit->flags & ENT_FLAGS::DEAD) continue;
        SDL_Rect enemyHBox = hit->GetHitbox();
        if(RectangleCollisionTest(hitRec, enemyHBox))
        {
            EnemyHitCallback(hit, source, atkID);
        }
    }
}












float GetMoveToPlayer(Entity* ent, float entSpeed)
{
    const int px = entList.player->centerX; const int py = entList.player->centerY;

    float dx = px - ent->centerX; float dy = py - ent->centerY;
    float dist = sqrtf(dx * dx + dy * dy);
    if(dist < 60.0f) {
        ent->moveX = 0; ent->moveY = 0;
        return dist;
    }

    float inv_dist = 1.0f / dist;
    dx *= inv_dist; dy *= inv_dist;
    
    ent->moveX = dx * entSpeed;
    ent->moveY = dy * entSpeed;
    return dist;
}













void UpdatePlayer(Entity* playerEnt, int framecount)
{
    static int repeatAttackIdx = 0;
    Animation* anims = mainInfo->anims;
    if((framecount % 3) == 0)
    {
        if(playerEnt->flags & ENT_FLAGS::DEAD) {
            if(playerEnt->frameIdx != (uint8_t)(anims[MainCharacter::CharAnims::Death].numFrames-1))
            {
                playerEnt->frameIdx++;
            }
        }
        else{
            playerEnt->frameIdx++;
        }
        if(playerEnt->activeAnim <= MainCharacter::Attack4 && playerEnt->frameIdx == anims[playerEnt->activeAnim].numFrames)
        {
            repeatAttackIdx = 0;
            playerEnt->frameIdx = 0;
            playerEnt->activeAnim = MainCharacter::CharAnims::Idle;
            playerEnt->ClearFlag(ENT_FLAGS::HIT_CHECKED);
            if(playerEnt->flags & ENT_FLAGS::COMBO_ATTACK_AV) playerEnt->ClearFlag(ENT_FLAGS::COMBO_ATTACK_AV);
            else playerEnt->SetFlag(ENT_FLAGS::COMBO_ATTACK_AV);
        }
        if(playerEnt->activeAnim == MainCharacter::CharAnims::TakeHitSilhoutte && playerEnt->frameIdx == anims[MainCharacter::CharAnims::TakeHitSilhoutte].numFrames)
        {
            playerEnt->frameIdx = 0;
            playerEnt->activeAnim = MainCharacter::CharAnims::Idle;
        }
        if(playerEnt->activeAnim > MainCharacter::Attack4 && playerEnt->flags & ENT_FLAGS::COMBO_ATTACK_AV) {
            repeatAttackIdx++;
        }
        if(repeatAttackIdx > 5)
        {
            playerEnt->ClearFlag(ENT_FLAGS::COMBO_ATTACK_AV);
            repeatAttackIdx = 0;
        }
    }
    userInput->GetMoveXY(playerEnt->moveX, playerEnt->moveY);
    if(playerEnt->flags & ENT_FLAGS::DEAD){
        playerEnt->activeAnim = MainCharacter::CharAnims::Death;
        playerEnt->moveX = 0; playerEnt->moveY = 0;
        return;
    }

    if(userInput->attackStates[0]){
        playerEnt->Attack(playerEnt->flags & ENT_FLAGS::COMBO_ATTACK_AV ? 1 : 0);
    }
    else if(userInput->attackStates[1]) {
        playerEnt->Attack(playerEnt->flags & ENT_FLAGS::COMBO_ATTACK_AV ? 3 : 2);
    }
    if(playerEnt->activeAnim <= MainCharacter::Attack4) // animation is attack
    {
        playerEnt->moveX = 0; playerEnt->moveY = 0;
        if(playerEnt->frameIdx == 3){
            SDL_Rect atkRect = GetAttackHitboxFromEntity(playerEnt, playerEnt->activeAnim);
            if(DRAW_DEBUG)
            {
                SDL_SetRenderDrawColor(renderer, 255, 0, 255, 255);
                SDL_RenderDrawRect(renderer, &atkRect);
            }
            if(!(playerEnt->flags & ENT_FLAGS::HIT_CHECKED))
            {
                HitTestAllEnemys(playerEnt, playerEnt->activeAnim, atkRect);
            }
            playerEnt->SetFlag(ENT_FLAGS::HIT_CHECKED);
        }
    }
    else
    {
        SetDirection(playerEnt);
        if(playerEnt->activeAnim == MainCharacter::Idle)
        {
            if(!(playerEnt->moveX == 0 && playerEnt->moveY == 0))
            {
                playerEnt->activeAnim = MainCharacter::CharAnims::Run;
            }
        }
        else if(playerEnt->activeAnim == MainCharacter::Run && (playerEnt->moveX == 0 && playerEnt->moveY == 0))
        {
            playerEnt->activeAnim = MainCharacter::Idle;
        }
    }
    playerEnt->MoveEntity();
}

void UpdateEnemy(Entity* ent, int framecount, int DeathAnim, int AttackAnim, int idleAnim, int runAnim)
{
    Animation* anims = GetAnimationFromType(ent->typeID);
    if((framecount % 3) == 0)
    {
        if(ent->flags & ENT_FLAGS::DEAD) {
            ent->activeAnim = DeathAnim;
            if(ent->frameIdx != (uint8_t)(anims[DeathAnim].numFrames-1))
            {
                ent->frameIdx++;
                ent->moveX = 0; // moveX gets used upon death to clean up the entity
            }
            else
            {
                ent->moveX++;
                if(ent->moveX > 10)
                {
                    entList._dirty = true;
                    ent->SetFlag(ENT_FLAGS::CLEAN_UP);
                }
                return;
            }
        }
        else{
            ent->frameIdx++;
        }
        if(ent && ent->frameIdx == anims[ent->activeAnim].numFrames)
        {
            ent->frameIdx = 0;
            ent->activeAnim = idleAnim;
            ent->ClearFlag(ENT_FLAGS::HIT_CHECKED);
        }
    }
    if(ent->flags & ENT_FLAGS::DEAD) return;

    float distance = GetMoveToPlayer(ent, 4.0f);
    if(distance < 80.0f)
    {
        int atkRand = rand() % 20;
        if(atkRand == 0)
        {
            ent->Attack(AttackAnim);
        }   
    }

    if(ent->AnimIsAttack()) // animation is attack
    {
        ent->moveX = 0; ent->moveY = 0;
        if(ent->frameIdx == 3){
            SDL_Rect atkRect = GetAttackHitboxFromEntity(ent, ent->activeAnim);
            if(DRAW_DEBUG)
            {
                SDL_SetRenderDrawColor(renderer, 255, 0, 255, 255);
                SDL_RenderDrawRect(renderer, &atkRect);
            }
            if(!(ent->flags & ENT_FLAGS::HIT_CHECKED))
            {
                SDL_Rect hbox = entList.player->GetHitbox();

                if(RectangleCollisionTest(atkRect, hbox))
                {
                    PlayerHitCallback(entList.player, ent, ent->activeAnim);
                    ent->SetFlag(ENT_FLAGS::HIT_CHECKED);
                }
            }
        }
    }
    else
    {
        if(!ent->CanMove()){
            ent->moveX = 0; ent->moveY = 0;
        }
        if(ent->flags & ENT_FLAGS::DEAD){
            ent->activeAnim = DeathAnim;
            ent->moveX = 0; ent->moveY = 0;
            return;
        }
        SetDirection(ent);
        if(ent->activeAnim == idleAnim){
            if(!(ent->moveX == 0 && ent->moveY == 0))
            {
                ent->activeAnim = runAnim;
            }
        }
        else if(ent->activeAnim == runAnim && (ent->moveX == 0 && ent->moveY == 0))
        {
            ent->activeAnim = idleAnim;
        }
    }
    ent->MoveEntity();
}




















bool Entity::CanMove()
{
    switch(typeID)
    {
    case ENT_TYPES::PLAYER: return (!(this->activeAnim <= MainCharacter::CharAnims::Attack4) && (activeAnim != MainCharacter::Death));
    case ENT_TYPES::SKELETON: return (this->activeAnim == SkeletonEnemy::Ready || activeAnim == SkeletonEnemy::Run);
    case ENT_TYPES::DEATH_BOSS: return (this->activeAnim == DeathBossEnemy::Walk || activeAnim == DeathBossEnemy::Idle || activeAnim == DeathBossEnemy::Hurt);
    default:
        break;
    }
    return false;
}
bool Entity::AnimIsAttack()
{
    switch(typeID)
    {
    case ENT_TYPES::PLAYER: return this->activeAnim <= MainCharacter::CharAnims::Attack4;
    case ENT_TYPES::SKELETON: return this->activeAnim <= SkeletonEnemy::Attack2;
    case ENT_TYPES::DEATH_BOSS: return this->activeAnim < DeathBossEnemy::Cast;
    default:
        break;
    }
    return false;
}
void Entity::Attack(int attackAnimID)
{
    if(CanMove())
    {
        activeAnim = attackAnimID;
        frameIdx = 0;
    }
}
SDL_Rect Entity::GetHitbox()
{
    SDL_Rect result = GetHitBoxFromType(this->typeID);
    result.x = centerX - result.w/2; result.y = centerY - result.h/2;
    return result;
}
void Entity::TakeDmg(int dmg, int hitAnim, int deathAnim)
{
    if(health <= 0 || flags & ENT_FLAGS::DEAD) { 
        health = 0;
        return;
    }
    health -= dmg;
    if(health <= 0)
    {
        frameIdx = 0;
        activeAnim = deathAnim;
        SetFlag(ENT_FLAGS::DEAD);
        if(typeID != ENT_TYPES::PLAYER)
            currentScore++;
    }
    if(!(flags & ENT_FLAGS::DEAD) && !AnimIsAttack() && activeAnim != hitAnim)
    {
        activeAnim = hitAnim;
        frameIdx = 0;
    }
}
void Entity::Draw()
{
    Animation* anims = GetAnimationFromType(typeID);
    if(anims)
    {
        anims[activeAnim].DrawIndex(centerX, centerY, frameIdx, ToRight());
    }
}




void EntityList::Recreate()
{
    entList.vec.clear();
    entList.vec.push_back(PlayerEntity::Create(screenWidth/2, screenHeight/2));
    entList.AddRandomEnemy();
    currentScore = 0;
    currentTime = 0;
    areaStartX = 0;
    areaStartY = 0;
    player = nullptr;
    canRetry = false;
    g_gameStateCounter = 0;
    Sort();
}
void EntityList::AddRandomEnemy()
{
    uint8_t num = ((uint8_t)rand() % ((uint8_t)ENT_TYPES::NUM_TYPES - 1)) + 1;
    int randX = rand() % 200;
    int randY = rand() % 200;
    int side = rand() % 4;
    switch(side)
    {
    case 0: // left
        randX = -randX - 100;
        randY = rand() % screenHeight;
        break;
    case 1: // top
        randY = -randY - 100;
        randX = rand() % screenWidth;
        break;
    case 2: // right
        randX = randX + screenWidth + 100;
        randY = rand() % screenHeight;
        break;
    case 3: // bottom
        randY = randY + screenHeight + 100;
        randX = rand() % screenWidth;
        break;

    }

    if(num == (uint8_t)ENT_TYPES::SKELETON)
    {
        vec.push_back(SkeletonEntity::Create(randX, randY));
    }
}









void PlayerHitCallback(Entity* player, Entity* hitter, int atkIdx)
{
    player->TakeDmg(10, MainCharacter::CharAnims::TakeHitSilhoutte, MainCharacter::CharAnims::Death);
}
void EnemyHitCallback(Entity* enemy, Entity* player, int atkIdx)
{
    switch(enemy->typeID)
    {
    case ENT_TYPES::SKELETON:
        enemy->TakeDmg(50, SkeletonEnemy::Hit, SkeletonEnemy::DeadFar);
        break;
    case ENT_TYPES::DEATH_BOSS:
        enemy->TakeDmg(10, DeathBossEnemy::Hurt, DeathBossEnemy::Death);
        break;
    default:
        break;
    }
}



EM_BOOL TouchStartCB(int eventType, const EmscriptenTouchEvent* ev, void* userData)
{
    if(canRetry)
    {
        for(int i = 0; i < ev->numTouches; i++)
        {
            auto p = ev->touches[i];
            if(retryRect.x < p.pageX && p.pageX < retryRect.w + retryRect.x && retryRect.y < p.pageY && p.pageY < retryRect.h + retryRect.y)
            {
                entList.Recreate();
            }
        }
    }
    userInput->TouchButtonSetState(ev, true, false);
    return true;
}
EM_BOOL TouchEndCB(int eventType, const EmscriptenTouchEvent* ev, void* userData)
{
    userInput->TouchButtonSetState(ev, false, false);
    return true;
}
EM_BOOL TouchMoveCB(int eventType, const EmscriptenTouchEvent* ev, void* userData)
{
    userInput->TouchButtonSetState(ev, true, true);
    return true;
}


EM_BOOL MousePressCB(int eventType, const EmscriptenMouseEvent* ev, void* userData)
{
    if(canRetry)
    {
        if(retryRect.x < ev->targetX && ev->targetX < retryRect.w + retryRect.x && retryRect.y < ev->targetY && ev->targetY < retryRect.h + retryRect.y)
        {
            entList.Recreate();
        }
    }
    userInput->MouseButtonSetState(ev, true);
    return true;
}
EM_BOOL MouseReleaseCB(int eventType, const EmscriptenMouseEvent* ev, void* userData)
{
    userInput->MouseButtonSetState(ev, false);
    return true;
}

void SetPlayerAsCenterOfScreenRoughly()
{ 
    float cx = screenWidth / 2 - entList.player->centerX;
    float cy = screenHeight / 2 - entList.player->centerY;
    float centerDist = sqrt(cx * cx + cy * cy);
    if(centerDist > 100.0f)
    {
        float inv_dist = 1.0f / centerDist;
        cx *= inv_dist; cy *= inv_dist;
        
        int perfectMatchX = cx * (centerDist - 100.0f);
        int perfectMatchY = cy * (centerDist - 100.0f);

        int mx = cx * (PRESS_SPEED + 5); int my = cy * (PRESS_SPEED + 5);
        if(abs(mx) > abs(perfectMatchX)) mx = perfectMatchX;
        if(abs(my) > abs(perfectMatchY)) my = perfectMatchY;
        areaStartX += mx;
        areaStartY += my;
        for(auto& e : entList.vec)
        {
            e.centerX += mx;
            e.centerY += my;
        }
    }

}


void MainLoop()
{
    static std::chrono::high_resolution_clock::time_point prev = std::chrono::high_resolution_clock::now();
    std::chrono::high_resolution_clock::time_point now = std::chrono::high_resolution_clock::now();
    currentTime += std::chrono::duration<float>(now - prev).count();
    SDL_Event event;
    Entity* player = entList.player;

    g_gameStateCounter++;
    SDL_SetRenderDrawColor(renderer, 10, 10, 255, 0);
    SDL_RenderClear(renderer);
    
    {   // Draw The Background
        uint32_t fmt;
        int access, w,h;
        SDL_QueryTexture(background, &fmt, &access, &w, &h);
        int clipAreaX = areaStartX % screenWidth; int clipAreaY = areaStartY % screenHeight;
        if(clipAreaX < 0) clipAreaX = screenWidth + clipAreaX;
        if(clipAreaY < 0) clipAreaY = screenHeight + clipAreaY;
        float scaleX = (float)w / (float)screenWidth;
        float scaleY = (float)h / (float)screenHeight;

        SDL_Rect curAreaRect; curAreaRect.x = 0; curAreaRect.y = 0;
        curAreaRect.w = clipAreaX; curAreaRect.h = clipAreaY;
        SDL_Rect bgRect; bgRect.x = w - clipAreaX * scaleX; bgRect.y = h - clipAreaY * scaleY;
        bgRect.w = clipAreaX * scaleX; bgRect.h = clipAreaY * scaleY;
        SDL_RenderCopy(renderer, background, &bgRect, &curAreaRect);

        curAreaRect.x = clipAreaX; curAreaRect.y = 0;
        curAreaRect.w = screenWidth - clipAreaX; curAreaRect.h = clipAreaY;
        bgRect.x = 0; bgRect.y = h - clipAreaY * scaleY;
        bgRect.w = (screenWidth - clipAreaX) * scaleX; bgRect.h = clipAreaY * scaleY;
        SDL_RenderCopy(renderer, background, &bgRect, &curAreaRect);

        curAreaRect.x = 0; curAreaRect.y = clipAreaY;
        curAreaRect.w = clipAreaX; curAreaRect.h = screenHeight - clipAreaY;
        bgRect.x = w - clipAreaX * scaleX; bgRect.y = 0;
        bgRect.w = clipAreaX * scaleX; bgRect.h = (screenHeight - clipAreaY) * scaleY;
        SDL_RenderCopy(renderer, background, &bgRect, &curAreaRect);

        curAreaRect.x = clipAreaX; curAreaRect.y = clipAreaY;
        curAreaRect.w = screenWidth - clipAreaX; curAreaRect.h = screenHeight - clipAreaY;
        bgRect.x = 0; bgRect.y = 0;
        bgRect.w = (screenWidth - clipAreaX) * scaleX; bgRect.h = (screenHeight - clipAreaY) * scaleY;
        SDL_RenderCopy(renderer, background, &bgRect, &curAreaRect);
    }

    





    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 150);
    SDL_RenderFillRect(renderer, NULL);
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);

    for(int i = 0; i < entList.vec.size(); i++)
    {
        Entity* ent = &entList.vec.at(i);
        ent->Draw();
        if(DRAW_DEBUG)
        {
            SDL_Rect rec = ent->GetHitbox();
            SDL_RenderDrawRect(renderer, &rec);
        }
    }
    userInput->DrawMobileControl();
    while(SDL_PollEvent(&event))
    {
        switch(event.type)
        {
        case SDL_KEYDOWN:
            userInput->ButtonSetState(event.key.keysym.scancode, true);
            break;
        case SDL_KEYUP:
            userInput->ButtonSetState(event.key.keysym.scancode, false);
            break;
        }
    }
    UpdatePlayer(player, g_gameStateCounter);
    for(int i = 0; i < entList.vec.size(); i++)    // update Skeletons
    {
        if(&entList.vec.at(i) == player) continue;    // skip player
        Entity* ent = &entList.vec.at(i);
        switch(ent->typeID)
        {
        case ENT_TYPES::SKELETON:
            UpdateEnemy(ent, g_gameStateCounter, SkeletonEnemy::DeadFar, SkeletonEnemy::Attack2, SkeletonEnemy::Ready, SkeletonEnemy::Run);
            break;
        case ENT_TYPES::DEATH_BOSS:
            UpdateEnemy(ent, g_gameStateCounter, DeathBossEnemy::Death, DeathBossEnemy::Attack, DeathBossEnemy::Idle, DeathBossEnemy::Walk);
            break;
        default:
            break;
        }
    }
    if((g_gameStateCounter % 300) == 0 && !(player->flags & ENT_FLAGS::DEAD)){
        int NumCreation = rand() % 50;
        for(int i = 0; i < NumCreation; i++)
            entList.AddRandomEnemy();
        entList.Sort();
        player = entList.player;
    }
    DrawHealthBar(healthBar, player->health);        
    if((player->flags & ENT_FLAGS::DEAD))
    {
        canRetry = true;
        SDL_SetRenderDrawColor(renderer, 0, 0, 255, 125);
        SDL_RenderFillRect(renderer, &retryRect);
        DrawText("RETRY", 0, 150, screenWidth, 0, 255, 0, 255, true);
    }
    
    entList.Sort();
    SetPlayerAsCenterOfScreenRoughly();
    DrawText((std::string("SCORE: ") + std::to_string(currentScore)).c_str(), 0, 20, screenWidth, 255, 0, 0, 255, true);
    {
        std::stringstream ss;
        ss.precision(0);
        ss << "Time: " << std::fixed << currentTime << "s";
        DrawText(ss.str().c_str(), 20, 20, 10000, 255, 255, 255, 255, false);
    }

    if(DRAW_FPS)
    {
        std::stringstream ss;
        ss.precision(0);
        ss << "FPS: " << std::fixed << (1.0f/ std::chrono::duration<float>(now - prev).count());
        DrawText(ss.str().c_str(), 200, 20, 10000, 255, 255, 255, 255, false);
    }

    SDL_RenderPresent(renderer);


    prev = std::chrono::high_resolution_clock::now();
}

int main()
{
    int fullScreen = 0;
    emscripten_get_canvas_size(&screenWidth, &screenHeight, &fullScreen);

    SDL_Init(SDL_INIT_EVERYTHING);
    TTF_Init();

    SDL_CreateWindowAndRenderer(screenWidth, screenHeight, 0, &window, &renderer);
    userInput = new Input(screenWidth, screenHeight);
    

    background = LoadTextureFromFile("Assets/GrassBackground.png", nullptr, nullptr);

    font = TTF_OpenFont("Assets/OpenSans.ttf", FONT_SIZE);
    
    retryRect.x = screenWidth / 2 - 50;
    retryRect.y = 150;
    retryRect.w = 100;
    retryRect.h = 35;

    healthBar.x = screenWidth - 250;
    healthBar.y = 25;
    healthBar.w = 200;
    healthBar.h = 24;
    
    MainCharacter mainInfoStackVar;
    SkeletonEnemy skelInfoStackVar;
    DeathBossEnemy deathBossInfoStackVar;
    mainInfo = &mainInfoStackVar;
    skelInfo = &skelInfoStackVar;
    deathBossInfo = &deathBossInfoStackVar;


    SDL_SetRenderDrawBlendMode(renderer, SDL_BlendMode::SDL_BLENDMODE_BLEND);


    emscripten_set_touchstart_callback("#canvas", nullptr, true, TouchStartCB);
    emscripten_set_touchend_callback("#canvas", nullptr, true, TouchEndCB);
    emscripten_set_touchmove_callback("#canvas", nullptr, true, TouchMoveCB);

    emscripten_set_mousedown_callback("#canvas", nullptr, true, MousePressCB);
    emscripten_set_mouseup_callback("#canvas", nullptr, true, MouseReleaseCB);


    entList.Recreate();


    emscripten_set_main_loop(MainLoop, 30, true);

}