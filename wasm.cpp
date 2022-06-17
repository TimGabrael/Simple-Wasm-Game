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
#include <array>



SDL_Rect CreateRect(int x, int y, int w, int h){ SDL_Rect rec; rec.x = x; rec.y = y; rec.w = w; rec.h = h; return rec; }


#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

static constexpr uint8_t U8_INVALID = -1;
static constexpr uint16_t U16_INVALID = -1;
static constexpr float INV_SQRT_2 = 0.70710678118f;


#define FONT_SIZE 24
static constexpr bool DRAW_DEBUG = false;
static constexpr bool DRAW_FPS = false;

SDL_Window *window;
SDL_Renderer *renderer;
SDL_Texture* background;
TTF_Font* font;

struct Entity;
struct MainCharacterInfo; struct SkeletonInfo; struct DeathBossInfo;
struct Animation; struct AttackInfo; struct BasicEnemyInfo; struct EntityInfo;
enum class ENT_TYPES : uint16_t
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
    IS_PLAYER = 64,
};
void SetDirection(Entity* ent);
void SetFullScreen();


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
    float health = 100.0f;   // 100.0f is default health
    int16_t centerX = 0;
    int16_t centerY = 0;
    int16_t moveX = 0;
    int16_t moveY = 0;
    ENT_TYPES typeID = ENT_TYPES::SKELETON;
    uint16_t multipurposeCounter = 0;
    uint8_t comboIdx = 0;
    uint8_t frameIdx = 0;
    uint8_t activeAnim = 0;
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
    bool CanMove(const EntityInfo* info);
    bool AnimIsAttack(const EntityInfo* info);
    void Attack(const EntityInfo* info, int atkID);

    const EntityInfo* GetEntityInfo();
    SDL_Rect GetHitBox(const EntityInfo* info);

    void TakeDmg(const EntityInfo* info,  const AttackInfo* atkInfo);
    void Draw(const EntityInfo* info);
    static Entity Create(int cx, int cy, ENT_TYPES type, bool player);
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
static SDL_Rect retryRect;
static SDL_Rect healthBar;
static bool canRetry = false;
static bool isFullscreen = false;
static float WORLD_SCALE = 2.0f;
static int currentScore = 0;
static float currentTime = 0;
static int areaStartX = 0;
static int areaStartY = 0;
static int screenWidth = 0;
static int screenHeight = 0;
static int g_gameStateCounter = 0;






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
    Input() : state{false}, attackStates{false}
    {
    }
    void SetMobileCircles(int width, int height)
    {
        LeftAttack.posX = width - 140;
        LeftAttack.posY = height - 50;
        LeftAttack.rad = 40;
        RightAttack.posX = width - 50;
        RightAttack.posY = height - 50;
        RightAttack.rad = 40;
        MoveCircle.rad = 40;
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
        case SDL_SCANCODE_F11:
            SetFullScreen();
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
    void GetMoveXY(float movementSpeed, int16_t& moveX, int16_t& moveY)
    {
        if(moveCircleIdx == -1)
        {
            if(IsPressed(Input::BTN_UP) == IsPressed(Input::BTN_DOWN))
            {
                moveY = 0;
            }
            else if(IsPressed(Input::BTN_UP)){
                moveY = -movementSpeed;
            }
            else if(IsPressed(Input::BTN_DOWN))
            {
                moveY = movementSpeed;
            }
            if(IsPressed(Input::BTN_LEFT) == IsPressed(Input::BTN_RIGHT))
            {
                moveX = 0;
            }
            else if(IsPressed(Input::BTN_LEFT)){
                moveX = -movementSpeed;
            }
            else if(IsPressed(Input::BTN_RIGHT))
            {
                moveX = movementSpeed;
            }
            if(moveX && moveY)
            {
                int diagMov = (int)(movementSpeed * INV_SQRT_2 + 0.5f);
                if(moveX < 0) moveX = -diagMov;
                else moveX = diagMov;
                if(moveY < 0) moveY = -diagMov;
                else moveY = diagMov;
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
            dx *= (dist / frad) * movementSpeed; dy *= (dist / frad) * movementSpeed;

            if(abs(dx) > 0.5){
                if(dx < 0.0f) moveX = dx - 0.5f;
                else moveX = dx + 0.5f;
            }
            if(abs(dy) > 0.5){
                if(dy < 0.0f) moveY = dy - 0.5f;
                else moveY = dy + 0.5f;
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


struct AnimationCreationData
{
    const char* filename = nullptr;
    int nFrames = 0;
    int numInColumn = 0;
};
struct BasicEntityInfo
{
    float maxHealth;
    float movementSpeed;
    int corpseDuration; // num frames that the corpse anim is shown if (corpseDuration == 0 or corpseAnim == U8_INVALID) -> deathAnim last frame gets shown forever
    int maxAttackCombo; // number of attacks that can be 
    uint8_t idleAnim;
    uint8_t walkAnim;
    uint8_t deathAnim;
    uint8_t hitAnim;
    uint8_t corpseAnim; // animation of the corpse
    bool spriteLooksLeft;
    bool canMoveWhileHit;
    static constexpr uint16_t comboTimeFrame = 3;   // number of animation changes until a combo attack is no longer possible
};
struct Animation
{
    Animation() {}
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
        SDL_Rect dst = src; dst.x += cx - src.x - (sx/2 + offX) * WORLD_SCALE; dst.y += cy - (sy/2 + offsetY) * WORLD_SCALE;
        dst.w *= WORLD_SCALE; dst.h *= WORLD_SCALE;
        
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
struct AttackInfo
{
    AttackInfo() {};
    SDL_Rect AtkRect;
    int startDmgframe;
    int duration;
    float damage;
    bool canMove;
    bool friendlyFire;
    uint8_t animID;
    SDL_Rect GetHitBox(Entity* ent);
};
struct BasicEnemyInfo
{
    BasicEnemyInfo(){}
    float attackRadius;
    float moveRadius;
    int removeIdx;  // num updates to wait until corpse is removed
};
void PlayerHitCallback(Entity* player, Entity* hitter, const AttackInfo* atkInfo);
void EnemyHitCallback(Entity* enemy, Entity* player, const AttackInfo* atkInfo);



struct EntityInfo
{  
    template<int nAnims>
    void SetAnims(const std::array<AnimationCreationData, nAnims>& Animations){
        if(anims) delete[] anims;
        this->numAnims = nAnims;
        anims = new Animation[nAnims];
        for(int i = 0; i < nAnims; i++) {
            auto& cAnim = Animations.at(i);
            anims[i] = Animation(cAnim.filename, cAnim.nFrames, cAnim.numInColumn);
        }
    }
    SDL_Rect HitBox;
    int numAttacks = 0;
    int numAnims = 0;
    AttackInfo* atks = nullptr;
    Animation* anims = nullptr;
    BasicEntityInfo baseInfo;
    BasicEnemyInfo enemyInfo;
};
static EntityInfo infoList[(size_t)ENT_TYPES::NUM_TYPES];
struct MainCharacterInfo : public EntityInfo
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
    MainCharacterInfo()
    {
        SetAnims<CharAnims::NUM_ANIMATIONS>(_animData);
        numAttacks = 4; atks = new AttackInfo[numAttacks];
        
        auto& a1 = atks[0]; a1.animID = Attack1; a1.AtkRect = CreateRect(0, -20 * WORLD_SCALE, 50 * WORLD_SCALE, 40 * WORLD_SCALE); a1.damage = 20.0f; a1.duration = 1; a1.startDmgframe = 3; a1.canMove = false; a1.friendlyFire = false;
        auto& a2 = atks[1]; a2.animID = Attack2; a2.AtkRect = CreateRect(0, -60 * WORLD_SCALE + hboxH / 2 * WORLD_SCALE, 70 * WORLD_SCALE, 50 * WORLD_SCALE); a2.damage = 20.0f; a2.duration = 1; a2.startDmgframe = 3; a2.canMove = false; a2.friendlyFire = false;
        auto& a3 = atks[2]; a3.animID = Attack3; a3.AtkRect = CreateRect(-hboxW * WORLD_SCALE, -5 * WORLD_SCALE, 80 * WORLD_SCALE, 10 * WORLD_SCALE); a3.damage = 20.0f; a3.duration = 1; a3.startDmgframe = 3; a3.canMove = false; a3.friendlyFire = false;
        auto& a4 = atks[3]; a4.animID = Attack4; a4.AtkRect = CreateRect(0, -60* WORLD_SCALE + hboxH / 2 * WORLD_SCALE, 70 * WORLD_SCALE, 60 * WORLD_SCALE); a4.damage = 20.0f; a4.duration = 1; a4.startDmgframe = 3; a4.canMove = false; a4.friendlyFire = false;

        baseInfo.maxHealth = 100.0f; baseInfo.movementSpeed = 6.0f * WORLD_SCALE; baseInfo.corpseDuration = 0; baseInfo.deathAnim = Death; baseInfo.idleAnim = Idle; baseInfo.walkAnim = Run; baseInfo.corpseAnim = U8_INVALID; baseInfo.spriteLooksLeft = false;
        baseInfo.maxAttackCombo = 2; baseInfo.hitAnim = TakeHitSilhoutte; baseInfo.canMoveWhileHit = true;
        HitBox.x = hboxW/2 * WORLD_SCALE; HitBox.y = hboxH/2 * WORLD_SCALE;
        HitBox.w = hboxW * WORLD_SCALE; HitBox.h = hboxH * WORLD_SCALE;
    }
    


private:
    static constexpr int hboxW = 20;
    static constexpr int hboxH = 40;
    static constexpr std::array<AnimationCreationData, NUM_ANIMATIONS> _animData = {{ {"Assets/MainChar/Attack1.png", 4, 4},
                     {"Assets/MainChar/Attack2.png", 4, 4},
                     {"Assets/MainChar/Attack3.png", 4, 4},
                     {"Assets/MainChar/Attack4.png", 4, 4},
                     {"Assets/MainChar/Death.png", 6, 6},
                     {"Assets/MainChar/Fall.png", 2, 2},
                     {"Assets/MainChar/Idle.png", 8, 8},
                     {"Assets/MainChar/Jump.png", 2, 2},
                     {"Assets/MainChar/Run.png", 8, 8},
                     {"Assets/MainChar/Take Hit - white silhouette.png", 4, 4},
                     {"Assets/MainChar/Take Hit.png", 4, 4} }};
};
struct SkeletonInfo : public EntityInfo
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
    SkeletonInfo()
    {
        SetAnims<SkeletonAnims::NUM_ANIMATIONS>(_animData);

        numAttacks = 2; atks = new AttackInfo[numAttacks];
        auto& a1 = atks[0]; a1.animID = Attack1; a1.AtkRect = CreateRect(-hboxW / 2 * WORLD_SCALE - 5 * WORLD_SCALE, -20 * WORLD_SCALE, 50 * WORLD_SCALE, 40 * WORLD_SCALE); a1.damage = 10.0f; a1.duration = 1; a1.startDmgframe = 3; a1.canMove = false; a1.friendlyFire = false;
        auto& a2 = atks[1]; a2.animID = Attack2; a2.AtkRect = CreateRect(-hboxW / 2 * WORLD_SCALE - 5 * WORLD_SCALE, -20 * WORLD_SCALE, 50 * WORLD_SCALE, 40 * WORLD_SCALE); a2.damage = 10.0f; a2.duration = 1; a2.startDmgframe = 3; a2.canMove = false; a2.friendlyFire = false;
        
        anims[0].SetOffset(10,10); anims[1].SetOffset(10,10);

        baseInfo.corpseDuration = 10; baseInfo.deathAnim = DeadFar; baseInfo.idleAnim = Ready; baseInfo.maxHealth = 40.0f; baseInfo.movementSpeed = 2.0f * WORLD_SCALE; baseInfo.walkAnim = Run; baseInfo.corpseAnim = Corpse; baseInfo.spriteLooksLeft = false;
        baseInfo.maxAttackCombo = 1; baseInfo.hitAnim = Hit; baseInfo.canMoveWhileHit = false;
        enemyInfo.attackRadius = 30.0f * WORLD_SCALE; enemyInfo.moveRadius = 20.0f * WORLD_SCALE; enemyInfo.removeIdx = 10;

        HitBox.x = hboxW / 2 * WORLD_SCALE; HitBox.y = hboxH / 2 * WORLD_SCALE;
        HitBox.w = hboxW * WORLD_SCALE; HitBox.h = hboxH * WORLD_SCALE;
    }



private:
    static constexpr int attackOffset = 10;
    static constexpr int hboxW = 20;
    static constexpr int hboxH = 40;
    static constexpr std::array<AnimationCreationData, NUM_ANIMATIONS> _animData = {{
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
    }};
};


struct DeathBossInfo : public EntityInfo
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
    DeathBossInfo()
    {
        SetAnims<DeathBossAnims::NUM_ANIMATIONS>(_animData);
        for(int i = 0; i < DeathBossAnims::NUM_ANIMATIONS; i++) anims[i].SetOffset(-35, 20);

        numAttacks = 2;
        atks = new AttackInfo[numAttacks];
        auto& a1 = atks[0]; a1.animID = Attack; a1.AtkRect = CreateRect(-hboxW/2*WORLD_SCALE-5*WORLD_SCALE,-70*WORLD_SCALE,120*WORLD_SCALE,100*WORLD_SCALE); a1.damage = 30.0f; a1.duration = 1; a1.startDmgframe = 8; a1.canMove = false; a1.friendlyFire = true;
        auto& a2 = atks[1]; a2.animID = Cast; a2.AtkRect = CreateRect(0,0,0,0); a2.duration = 0; a2.startDmgframe = 0; a2.canMove = false; a2.friendlyFire = false;

        baseInfo.corpseDuration = 0; baseInfo.deathAnim = Death; baseInfo.idleAnim = Idle; baseInfo.maxHealth = 100.0f; baseInfo.movementSpeed = 3.0f * WORLD_SCALE; baseInfo.walkAnim = Walk; baseInfo.corpseAnim = U8_INVALID; baseInfo.spriteLooksLeft = true;
        baseInfo.maxAttackCombo = 1; baseInfo.hitAnim = Hurt; baseInfo.canMoveWhileHit = true;
        enemyInfo.attackRadius = 75.0f * WORLD_SCALE; enemyInfo.moveRadius = 30.0f * WORLD_SCALE; enemyInfo.removeIdx = 0;

        HitBox.x = hboxW / 2 * WORLD_SCALE; HitBox.y = hboxH / 2 * WORLD_SCALE;
        HitBox.w = hboxW * WORLD_SCALE; HitBox.h = hboxH * WORLD_SCALE;
    }

private:
    static constexpr int hboxW = 30;
    static constexpr int hboxH = 60;

    static constexpr std::array<AnimationCreationData, NUM_ANIMATIONS> _animData = {{
        {"Assets/Death-Boss/Attack.png", 10, 10},
        {"Assets/Death-Boss/Cast.png", 9, 9},
        {"Assets/Death-Boss/Death.png", 10, 10},
        {"Assets/Death-Boss/Hurt.png", 3, 3},
        {"Assets/Death-Boss/Idle.png", 8, 8},
        {"Assets/Death-Boss/Spell.png", 16, 16},
        {"Assets/Death-Boss/Walk.png", 8, 8}
    }};
};


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

void HitTestAll(Entity* source, const AttackInfo* atkInfo, const SDL_Rect& hitRec) // doesn't hit source
{
    for(int i = 0; i < entList.vec.size(); i++)
    {
        if(source == &entList.vec.at(i)) continue;
        Entity* hit = &entList.vec.at(i);
        if(hit->flags & ENT_FLAGS::DEAD) continue;
        SDL_Rect enemyHBox = hit->GetHitBox(hit->GetEntityInfo());
        if(RectangleCollisionTest(hitRec, enemyHBox))
        {
            EnemyHitCallback(hit, source, atkInfo);
        }
    }
}












float GetMoveToPlayer(Entity* ent, const EntityInfo* info)
{
    const int px = entList.player->centerX; const int py = entList.player->centerY;

    float dx = px - ent->centerX; float dy = py - ent->centerY;
    float dist = sqrtf(dx * dx + dy * dy);
    if(dist < info->enemyInfo.moveRadius) {
        ent->moveX = 0; ent->moveY = 0;
        return dist;
    }

    float inv_dist = 1.0f / dist;
    dx *= inv_dist; dy *= inv_dist;
    
    ent->moveX = dx * info->baseInfo.movementSpeed;
    ent->moveY = dy * info->baseInfo.movementSpeed;
    return dist;
}









void HandleEntityDeathTick(Entity* ent, const EntityInfo* info, int frameCount)
{
    if(!(ent->flags & ENT_FLAGS::IS_PLAYER))
    {
        int rmidx = info->enemyInfo.removeIdx + info->anims[info->baseInfo.deathAnim].numFrames;
        if(rmidx <= ent->multipurposeCounter){
            ent->SetFlag(ENT_FLAGS::CLEAN_UP);
            entList._dirty = true;
        }
        ent->multipurposeCounter++;   // cleanup counter
    }
}
void HandleEntityMoveAndAttack(Entity* ent, const EntityInfo* info, int frameCount)
{
    if(ent->flags & ENT_FLAGS::IS_PLAYER)
    {
        userInput->GetMoveXY(info->baseInfo.movementSpeed, ent->moveX, ent->moveY);
        if(userInput->attackStates[0]){
            ent->Attack(info , ent->comboIdx);
        }
        else if(userInput->attackStates[1]) {
            ent->Attack(info, ent->comboIdx + 2);
        }
    }
    else{
        float distance = GetMoveToPlayer(ent, info);
        if(distance < info->enemyInfo.attackRadius)
        {
            int atkRand = rand() % 8;
            if(atkRand == 0)
            {
                ent->Attack(info, 0);
            }   
        }
    }
}

void UpdateEntity(Entity* ent, int frameCount)
{
    const EntityInfo* info = ent->GetEntityInfo();
    if((frameCount % 3) == 0)   // new animation frame
    {
        if(ent->flags & ENT_FLAGS::DEAD)
        {
            if(ent->activeAnim == info->baseInfo.deathAnim && info->baseInfo.deathAnim != U8_INVALID)
            {
                if(ent->frameIdx != (uint8_t)(info->anims[info->baseInfo.deathAnim].numFrames-1))
                {
                    ent->frameIdx++;
                }
                else if(info->baseInfo.corpseAnim != U8_INVALID && info->baseInfo.corpseDuration != 0)
                {
                    ent->activeAnim = info->baseInfo.corpseAnim;
                }
            }
            HandleEntityDeathTick(ent, info, frameCount);
            return;
        }
        else
        {
            ent->frameIdx++;
        }
        ent->ClearFlag(ENT_FLAGS::HIT_CHECKED);
        if(ent->AnimIsAttack(info) && ent->frameIdx == info->anims[ent->activeAnim].numFrames)
        {
            ent->frameIdx = 0; ent->activeAnim = info->baseInfo.idleAnim;
            ent->SetFlag(ENT_FLAGS::COMBO_ATTACK_AV);
        }
        else if(ent->flags & ENT_FLAGS::COMBO_ATTACK_AV)
        {
            if(info->baseInfo.comboTimeFrame <= ent->multipurposeCounter)
            {
                ent->multipurposeCounter = 0; ent->comboIdx = 0;
                ent->ClearFlag(ENT_FLAGS::COMBO_ATTACK_AV);
            }
            else
            {
                ent->multipurposeCounter++;
            }
        }
        if(info->baseInfo.hitAnim != U8_INVALID && ent->activeAnim == info->baseInfo.hitAnim && ent->frameIdx == info->anims[info->baseInfo.hitAnim].numFrames)
        {
            ent->frameIdx = 0; ent->activeAnim = info->baseInfo.idleAnim;
        }
    }
    if(ent->flags & ENT_FLAGS::DEAD) return;    // for whenever the framecount is not %3
    HandleEntityMoveAndAttack(ent, info, frameCount);

    if(ent->AnimIsAttack(info))
    {
        AttackInfo* curAttack = nullptr;
        for(int i = 0; i < info->numAttacks; i++)
        {
            if(info->atks[i].animID == ent->activeAnim){
                curAttack = &info->atks[i];
                break;
            }
        }
        if(curAttack)
        {
            SDL_Rect atkHitbox = curAttack->GetHitBox(ent);
            SDL_Rect entHitbox = ent->GetHitBox(info);

            if(DRAW_DEBUG)
            {
                SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
            }
            if(!curAttack->canMove){
                ent->moveX = 0; ent->moveY = 0;
            }
            if(curAttack->startDmgframe <= ent->frameIdx && ent->frameIdx < curAttack->startDmgframe + curAttack->duration && !(ent->flags & ENT_FLAGS::HIT_CHECKED)){
                if(DRAW_DEBUG)
                {
                    SDL_SetRenderDrawColor(renderer, 255, 0, 255, 255);
                }
                if(ent->flags & ENT_FLAGS::IS_PLAYER)
                {
                    HitTestAll(ent, curAttack, atkHitbox);
                    if(curAttack->friendlyFire)
                    {
                        if(RectangleCollisionTest(entHitbox, atkHitbox))
                        {
                            PlayerHitCallback(ent, ent, curAttack);
                        }
                    }
                }
                else{
                    const EntityInfo* playerInfo = entList.player->GetEntityInfo();
                    SDL_Rect playerHitbox = entList.player->GetHitBox(playerInfo);
                    if(RectangleCollisionTest(playerHitbox, atkHitbox))
                    {
                        PlayerHitCallback(entList.player, ent, curAttack);
                    }
                    if(curAttack->friendlyFire)
                    {
                        HitTestAll(ent, curAttack, atkHitbox);
                    }
                }
                ent->SetFlag(ENT_FLAGS::HIT_CHECKED);
            }

            if(DRAW_DEBUG)
            {
                SDL_RenderDrawRect(renderer, &atkHitbox);
            }

        }
    }
    else
    {
        SetDirection(ent);
        if(ent->CanMove(info))
        {
            if(ent->activeAnim != info->baseInfo.hitAnim)
            {
                if(ent->moveX == 0 && ent->moveY == 0)
                {
                    ent->activeAnim = info->baseInfo.idleAnim;
                }
                else{
                    ent->activeAnim = info->baseInfo.walkAnim;
                }
            }
        }
        else {
            ent->moveX = 0; ent->moveY = 0;
        }
    }

    ent->MoveEntity();
}









SDL_Rect AttackInfo::GetHitBox(Entity* ent)
{
    SDL_Rect res = AtkRect;
    if(ent->flags & ENT_FLAGS::SPRITE_INVERSE)
    {
        if(ent->ToRight())
        {
            res.x = -res.w - res.x;
        }
    }
    else
    {
        if(!ent->ToRight())
        {
            res.x = -res.w - res.x;
        }
    }
    res.x += ent->centerX;
    res.y += ent->centerY;
    return res;
}

bool Entity::CanMove(const EntityInfo* info)
{
    for(int i = 0; i < info->numAttacks; i++)
    {
        if(info->atks[i].animID == activeAnim){
            return info->atks[i].canMove;
        }
    }
    if(activeAnim == info->baseInfo.hitAnim && !info->baseInfo.canMoveWhileHit){
        return false;
    }
    return true;
}
bool Entity::AnimIsAttack(const EntityInfo* info)
{
    for(int i = 0; i < info->numAttacks; i++)
    {
        if(info->atks[i].animID == activeAnim) return true;
    }
    return false;
}
void Entity::Attack(const EntityInfo* info, int attackID)
{
    if(CanMove(info) && !AnimIsAttack(info) && attackID < info->numAttacks)
    {
        activeAnim = info->atks[attackID].animID;
        frameIdx = 0;
        comboIdx = (comboIdx + 1) % info->baseInfo.maxAttackCombo;
        multipurposeCounter = 0;
    }
}
SDL_Rect Entity::GetHitBox(const EntityInfo* info)
{
    SDL_Rect result = info->HitBox;
    result.x = centerX - result.w/2; result.y = centerY - result.h/2;
    return result;
}
const EntityInfo* Entity::GetEntityInfo()
{
    if(typeID < ENT_TYPES::NUM_TYPES) return &infoList[(int)typeID];
    return nullptr;
}
void Entity::TakeDmg(const EntityInfo* info, const AttackInfo* atkInfo)
{
    if(health <= 0 || flags & ENT_FLAGS::DEAD) { 
        health = 0;
        return;
    }
    health -= atkInfo->damage;
    if(health <= 0)
    {
        frameIdx = 0;
        multipurposeCounter = 0;
        comboIdx = 0;
        activeAnim = info->baseInfo.deathAnim;
        SetFlag(ENT_FLAGS::DEAD);
        if(!(flags & ENT_FLAGS::IS_PLAYER))
            currentScore++;
    }
    if(!(flags & ENT_FLAGS::DEAD) && !AnimIsAttack(info) && activeAnim != info->baseInfo.hitAnim)
    {
        activeAnim = info->baseInfo.hitAnim;
        frameIdx = 0;
    }
}
void Entity::Draw(const EntityInfo* info)
{
    Animation* anims = info->anims;
    if(anims && activeAnim < info->numAnims)
    {
        anims[activeAnim].DrawIndex(centerX, centerY, frameIdx, ToRight());
    }
}
Entity Entity::Create(int cx, int cy, ENT_TYPES type, bool player)
{
    const EntityInfo* entInfo = &infoList[(int)type];
    uint8_t flags = ENT_FLAGS::LOOK_RIGHT;
    if(player) flags |= ENT_FLAGS::IS_PLAYER;
    if(entInfo->baseInfo.spriteLooksLeft) flags |= ENT_FLAGS::SPRITE_INVERSE;
    Entity res(cx, cy, entInfo->baseInfo.maxHealth, entInfo->baseInfo.idleAnim, type, flags);
    return res;
}




void EntityList::Recreate()
{
    entList.vec.clear();
    entList.vec.push_back(Entity::Create(screenWidth/2, screenHeight/2, ENT_TYPES::PLAYER, true));
    currentScore = 0;
    currentTime = 0;
    areaStartX = 0;
    areaStartY = 0;
    player = nullptr;
    canRetry = false;
    g_gameStateCounter = 0;
}
void EntityList::AddRandomEnemy()
{
    int entType = (rand() % 1000);
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


    if(entType < 900)
    {
        vec.push_back(Entity::Create(randX, randY, ENT_TYPES::SKELETON, false));
    }
    else if(entType > 899)
    {
        vec.push_back(Entity::Create(randX, randY, ENT_TYPES::DEATH_BOSS, false));
    }
}









void PlayerHitCallback(Entity* player, Entity* hitter, const AttackInfo* atkInfo)
{
    player->TakeDmg(player->GetEntityInfo(), atkInfo);
}
void EnemyHitCallback(Entity* enemy, Entity* player, const AttackInfo* atkInfo)
{
    enemy->TakeDmg(enemy->GetEntityInfo(), atkInfo);
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

        int mx = cx * (5 + 5); int my = cy * (5 + 5);   // movement speed should probably stored in a variable
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
    if(!entList.player) entList.Sort();
    Entity* player = entList.player;

    SDL_SetRenderDrawColor(renderer, 10, 10, 255, 0);
    SDL_RenderClear(renderer);
    
    {   // Draw The Background
        uint32_t fmt;
        int access, w,h;
        SDL_QueryTexture(background, &fmt, &access, &w, &h);
        int clipAreaX = areaStartX % screenWidth; int clipAreaY = areaStartY % screenHeight;
        if(clipAreaX < 0) clipAreaX = screenWidth + clipAreaX;
        if(clipAreaY < 0) clipAreaY = screenHeight + clipAreaY;
        const float scaleX = (float)w / (float)screenWidth;
        const float scaleY = (float)h / (float)screenHeight;

        int scaleClipAreaX = clipAreaX * scaleX; int scaleClipAreaY = clipAreaY * scaleY;
        int scaleStartX = w - scaleClipAreaX; int scaleStartY = h - scaleClipAreaY;
        int scaleEndX = (screenWidth * scaleX) - scaleClipAreaX; int scaleEndY = (screenHeight * scaleY) - scaleClipAreaY;

        SDL_Rect curAreaRect; curAreaRect.x = 0; curAreaRect.y = 0;
        curAreaRect.w = clipAreaX; curAreaRect.h = clipAreaY;
        SDL_Rect bgRect; bgRect.x = scaleStartX; bgRect.y = scaleStartY;
        bgRect.w = scaleClipAreaX; bgRect.h = scaleClipAreaY;
        SDL_RenderCopy(renderer, background, &bgRect, &curAreaRect);

        curAreaRect.x = clipAreaX; curAreaRect.y = 0;
        curAreaRect.w = screenWidth - clipAreaX; curAreaRect.h = clipAreaY;
        bgRect.x = 0; bgRect.y = scaleStartY;
        bgRect.w = scaleEndX; bgRect.h = scaleClipAreaY;
        SDL_RenderCopy(renderer, background, &bgRect, &curAreaRect);

        curAreaRect.x = 0; curAreaRect.y = clipAreaY;
        curAreaRect.w = clipAreaX; curAreaRect.h = screenHeight - clipAreaY;
        bgRect.x = scaleStartX; bgRect.y = 0;
        bgRect.w = scaleClipAreaX; bgRect.h = scaleEndY;
        SDL_RenderCopy(renderer, background, &bgRect, &curAreaRect);

        curAreaRect.x = clipAreaX; curAreaRect.y = clipAreaY;
        curAreaRect.w = screenWidth - clipAreaX; curAreaRect.h = screenHeight - clipAreaY;
        bgRect.x = 0; bgRect.y = 0;
        bgRect.w = scaleEndX; bgRect.h = scaleEndY;
        SDL_RenderCopy(renderer, background, &bgRect, &curAreaRect);
    }

    





    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 150);
    SDL_RenderFillRect(renderer, NULL);
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);

    for(int i = 0; i < entList.vec.size(); i++)
    {
        Entity* ent = &entList.vec.at(i);
        ent->Draw(ent->GetEntityInfo());
        if(DRAW_DEBUG)
        {
            SDL_Rect rec = ent->GetHitBox(ent->GetEntityInfo());
            SDL_RenderDrawRect(renderer, &rec);
        }
    }
    userInput->DrawMobileControl();
    SDL_Rect playerHpRect = player->GetHitBox(player->GetEntityInfo());
    playerHpRect.y -= 10; playerHpRect.h = 5;
    DrawHealthBar(playerHpRect, player->health);        
    
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


    UpdateEntity(player, g_gameStateCounter);
    for(int i = 0; i < entList.vec.size(); i++)    // update Skeletons
    {
        if(&entList.vec.at(i) == player) continue;    // skip player
        Entity* ent = &entList.vec.at(i);
        UpdateEntity(ent, g_gameStateCounter);
    }
    if((g_gameStateCounter % 300) == 0 && !(player->flags & ENT_FLAGS::DEAD)){
        int NumCreation = (rand() % 20) + 10;
        for(int i = 0; i < NumCreation; i++)
            entList.AddRandomEnemy();
        entList.Sort();
        player = entList.player;
    }

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

    g_gameStateCounter++;
    SDL_RenderPresent(renderer);
    prev = std::chrono::high_resolution_clock::now();
}







void SetFullScreen()
{
    EmscriptenFullscreenStrategy fsStrat;
    fsStrat.scaleMode = 0;
    fsStrat.canvasResolutionScaleMode = 0;
    fsStrat.filteringMode = EMSCRIPTEN_FULLSCREEN_FILTERING_DEFAULT;
    fsStrat.canvasResizedCallback = nullptr;
    fsStrat.canvasResizedCallbackUserData = nullptr;
    emscripten_request_fullscreen_strategy("#canvas", true, &fsStrat);
}
void SetScreenSizedElements()
{
    retryRect.x = screenWidth / 2 - 50;
    retryRect.y = 150;
    retryRect.w = 100;
    retryRect.h = 35;

    healthBar.x = screenWidth - 250;
    healthBar.y = 25;
    healthBar.w = 200;
    healthBar.h = 24;
    userInput->SetMobileCircles(screenWidth, screenHeight);

    if(entList.player)
    {
        int px = entList.player->centerX;
        int py = entList.player->centerY;

    }

    if(screenWidth < 900 || screenHeight < 600){
        WORLD_SCALE = 1.0f;
    }
    else{
        WORLD_SCALE = 2.0f;
    }
    infoList[0] = MainCharacterInfo();
    infoList[1] = SkeletonInfo();
    infoList[2] = DeathBossInfo();

}
EM_BOOL TouchStartCB(int eventType, const EmscriptenTouchEvent* ev, void* userData)
{
    SetFullScreen();
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
    else
    {
        userInput->TouchButtonSetState(ev, true, false);
    }
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
    else
    {
        userInput->MouseButtonSetState(ev, true);
    }
    return true;
}
EM_BOOL MouseReleaseCB(int eventType, const EmscriptenMouseEvent* ev, void* userData)
{
    userInput->MouseButtonSetState(ev, false);
    return true;
}
EM_BOOL CanvasResizeCB(int eventType, const EmscriptenUiEvent* uiEvent, void* userData)
{
    double css_width; double css_height;
    emscripten_get_element_css_size("canvas", &css_width, &css_height);
    screenWidth = css_width; screenHeight = css_height;
    SDL_SetWindowSize(window, screenWidth, screenHeight);
    SetScreenSizedElements();
    return true;
}
EM_BOOL FullscreenChangeCB(int eventType, const EmscriptenFullscreenChangeEvent* fullscreenChangeEvent, void *userData)
{

    isFullscreen = fullscreenChangeEvent->isFullscreen;
    return true;
}


int main()
{
    double css_width; double css_height;
    emscripten_get_element_css_size("#canvas", &css_width, &css_height);
    screenWidth = css_width; screenHeight = css_height;

    SDL_Init(SDL_INIT_EVERYTHING);
    TTF_Init();

    SDL_CreateWindowAndRenderer(screenWidth, screenHeight, 0, &window, &renderer);
    userInput = new Input();

    background = LoadTextureFromFile("Assets/GrassBackground.png", nullptr, nullptr);

    font = TTF_OpenFont("Assets/OpenSans.ttf", FONT_SIZE);
    
    SetScreenSizedElements();

    infoList[0] = MainCharacterInfo();
    infoList[1] = SkeletonInfo();
    infoList[2] = DeathBossInfo();

    SDL_SetRenderDrawBlendMode(renderer, SDL_BlendMode::SDL_BLENDMODE_BLEND);


    emscripten_set_touchstart_callback("#canvas", nullptr, true, TouchStartCB);
    emscripten_set_touchend_callback("#canvas", nullptr, true, TouchEndCB);
    emscripten_set_touchmove_callback("#canvas", nullptr, true, TouchMoveCB);

    emscripten_set_mousedown_callback("#canvas", nullptr, true, MousePressCB);
    emscripten_set_mouseup_callback("#canvas", nullptr, true, MouseReleaseCB);


    emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, true, CanvasResizeCB);
    emscripten_set_fullscreenchange_callback("#canvas", nullptr, true, FullscreenChangeCB);

    entList.Recreate();

    emscripten_set_main_loop(MainLoop, 30, true);

}