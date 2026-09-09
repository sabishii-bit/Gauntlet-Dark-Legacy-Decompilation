/*
 * combat.c -- COMBAT.OBJ: damage bookkeeping, missiles and the line/cylinder
 * collision helpers, 0x8002F2D4..0x8003104C.
 *
 * Function names come from the Xbox PDB module roster.
 */

#include "types.h"
#include "game/camera.h"
#include "game/cameradata.h"
#include "game/enemy.h"
#include "game/gamemode.h"
#include "game/player.h"
#include "game/worldinfo.h"
#include "game/leveldata.h"
#include "game/item.h"
#include "game/plyrdata.h"

#ifndef offsetof
#define offsetof(type, memb) ((u32) & ((type*)0)->memb)
#endif

typedef struct CameraTarget {
    s32 active;   /* +0x00 */
    u32 object;   /* +0x04: object/group or matrix address */
    f32 x;        /* +0x08 */
    f32 y;        /* +0x0C */
    f32 z;        /* +0x10 */
    u8  _pad14[0x24];
} CameraTarget;

extern CameraTarget gCameraTargets[15];
extern u8 gCameraState[];
extern f32 gCameraTargetPositions[27];
extern f32 gRecorderCameraPosition[3];

extern s32 gCameraTargetCount;
extern s32 gCameraTargetMode;
extern s32 gCameraTargetPositionCount;
extern s32 gClockFrameNumber;
extern f32 sMusicFadeBase;
extern f32 gClockTime;
extern s32 InfFrame;
extern s32 sLastVBlankCounter;
extern f32 gClockFrameReciprocal;
extern f32 gClockFrameStep;
extern f32 gClockPreviousTime;
extern s32 sLastTimerCount;
extern u32 sLastFrameTime;
extern s32 sClockAccumulator;
extern u32 gClockElapsedTime;
extern u32 gClockCurrentTime;
extern s32 gClockStepTicks;
extern u32 gFrameTicks;

extern u32 pbLoad;
extern s32 gGameBusy;
extern s32 options_state;
extern s32 gGameplayPauseTimer;
extern s32 gModalRenderDepth;
extern s32 gGameMode;
extern s32 gNumEnemies;
extern s32 gBossType;

typedef struct ClockInputWords {
    s32 buttons;
    s32 flags;
} ClockInputWords;

typedef union ClockInputPair {
    u64 both;
    ClockInputWords word;
} ClockInputPair;

extern ClockInputPair gControllerButtons;
extern ClockInputPair sPreviousFlags;
extern s32 sFlags;
extern s32 lbl_803445D4;
extern Player gPlayers[];

typedef struct MissileInfo {
    u32 damageType;
    f32 damage;
    f32 speed;
    f32 collisionRadius;
    f32 hitRadius;
    f32 angularVelocity[3];
    f32 weight;
    s32 hitEffect;
    s32 hitSound;
    s32 wallSound;
} MissileInfo;

typedef struct MissileTreeInfo {
    void* throwHeader;
    u32 throwFlags;
} MissileTreeInfo;

typedef struct MissileDescription {
    char throwDescription[4];
    char throwLevel[11];
    u8 _pad0F;
    u32 flags;
} MissileDescription;

s32 pmissile_sfxidx[5];
s32 WeapThrowFx[4][5];
void* WeapHoldFxTree[4][5];
void* FamiliarSpit[5];
void* FamiliarTree[4][2];
void* EnemyMissileTree[28][3];
MissileTreeInfo PlayerMissileTreeInfo[4];
void* BallistaTree;
void* BossElecTree;
void* BossAcidTree;
void* FireShieldTree;
void* PhoenixTree;
void* WingsTree;
void* PojoTree;
void* BreatheFireTree;
void* BreatheAcidTree;
void* BreatheElecTree;
s32 WeaponStreakTex;
extern MissileInfo PlayerMissileInfo[8];
extern MissileInfo EnemyMissileInfo[28][3];
extern MissileInfo BallistaMissileInfo;
extern MissileInfo BossElecMissileInfo;
extern MissileInfo BossAcidMissileInfo;
extern char EnemyMissileDesc[3][8];
extern MissileDescription PlayerMissileDesc[16];
extern char DmgTypeDesc[5][8];

/* cross-TU references */
void CopyMat4(f32* src, f32* dst);
extern f64 __frsqrte(f64 x);
extern f32 atan2(f32 y, f32 x);
extern f32 sin(f32 angle);
extern f32 cos(f32 angle);
f32 FixAngle(f32 angle);
f32 fqdist(f32 x, f32 y);
f32 smallsqrt(f32 value);
f32 NormalVector2D(f32* v);
f32 PointLineColl(f32* point, f32* from, f32* to, f32* closest);
void CreateYPRMatrix(f32* matrix, f32* pyr);
void WorldVector(f32* local, f32* world, f32* matrix);
void MBWindowSetRegion(f32 left, f32 right, f32 top, f32 bottom, f32 depth);
u32 pbGetTime(void);
void MBRemoveBlit(s32 blit);
void MBWindowZoom(f32 zoom);
void DoShake(void* camera, void* attention);
void LookInDirection(f32* matrix, f32* direction);
void ErrorPrintf(char* format, ...);
void FatalError(char* format, s32 code);
void* EnemyTypePrefix(s32 enemyType);
void* AtreeMatch(void* tree, char* name, s32 required);
void DeleteItem(void* item, s32 immediate);
extern void* memset(void* dst, int value, size_t size);

/* stage-info banner (combat.c title-card display) */
extern level_data* gCurLevel;
extern s32 sMusicTrackLo;
extern s32 lbl_80344490;
extern s32 lbl_80344498;
extern void* lbl_8034440C;
extern f32 lbl_80344410;
extern f32 lbl_80346168;
extern f32 lbl_80345F80;
extern f32 lbl_80346158;
extern f64 lbl_80346150;
extern f64 lbl_80345F50;
extern f64 lbl_80345F40;
extern f64 lbl_80346160;
extern s32 StringTextWidth(s32 id, s32 a, f32 scale);
extern s32 StringTextHeight(s32 id, s32 a, s32 b, f32 scale);
extern void* MBNewBlit(void* tex, s32 x, s32 y);
extern void mbBlitProject(void* blit, s32 w, s32 h);
extern void DrawTextKeepScale(s32 x, s32 y, s32 flags, u32 color, char* str);
extern void DrawStringText(s32 x, s32 y, s32 a, u32 color, s32 id, ...);
extern void fn_8009D300(void);
extern void fn_8009FAB4(void);
extern void fn_8009D2B4(void);

/* missile atree lookup */
extern void* gWadAtreeHeaders[];
extern char lbl_803463D4[5];  /* "%s%s" */
extern char* EnemyTypeDesc(s32 type);
extern s32 toupper(s32 c);
extern s32 sprintf(char* dst, const char* fmt, ...);

/* shared camera math constants (.sdata2) */
extern f32 lbl_80345EC8;  /* 0.0f */
extern f64 lbl_80345F18;  /* 0.5 (rsqrt newton) */
extern f64 lbl_80345F20;  /* 3.0 (rsqrt newton) */
extern s32 lbl_80344508;
extern s32 lbl_803444F0;
extern s32 lbl_803444EC;
extern s32 lbl_803447B8;
extern s32 lbl_8034453C;
extern const f32 lbl_803462E8;
extern const f32 lbl_803462EC;
extern const f32 lbl_803462F0;
extern const f32 lbl_803462F4;
extern const f32 lbl_803462F8;
extern s32 gScriptedCameraState;
extern s32 lbl_803447B4;
extern s32 gNumTransmitters;
extern f64 lbl_80346128;  /* transmitter yaw step */
extern f64 lbl_80345F58;  /* +pi */
extern f64 lbl_80345F60;  /* 2pi */
extern f64 lbl_80345F68;  /* -pi */
extern f64 lbl_80346130;  /* radius divisor */
extern f64 lbl_803460D0;  /* radius min */
extern f32 lbl_80346018;  /* radius min clamp */
extern f64 lbl_80345FF0;  /* radius max */
extern f32 lbl_80346020;  /* radius max clamp */
extern f64 lbl_80345F78;  /* no-dist sentinel */
extern s32 lbl_803443FC;  /* wall-hug flag */
extern s32 lbl_80344960;
extern f32 lbl_80344528;
extern f32 lbl_8034618C;  /* radius snap epsilon */
extern s32 lbl_803443F4;  /* radius-moved flag */
extern f64 lbl_80346098;  /* radius step gain */
extern s32 lbl_803444E4;
extern s32 lbl_80344418;
/* get_cam_dist: FOV/screen-fit constants + wall-hug state */
extern f64 lbl_80345F28, lbl_80346190, lbl_80345F90, lbl_80345EB0;
extern f64 lbl_803461A8, lbl_803461B8, lbl_803461C0, lbl_803461C8;
extern f64 lbl_80345F88, lbl_80345FF8;
extern f32 lbl_80346198, lbl_8034452C, lbl_8034619C, lbl_803461A0;
extern f32 lbl_803460F0, lbl_803461B0, lbl_803461B4, lbl_803461D0, lbl_803444E8;
extern s32 lbl_8034451C, lbl_80344520, lbl_80344518, lbl_80344514;
extern s32 lbl_803444F4, sMusicTrackHi;
extern s32 gBossActive;
extern s32 lbl_8011BCB8[];  /* exp: hit+flag */
extern s32 lbl_8011BC30[];  /* exp: kill+flag */
extern s32 lbl_8011BBA8[];  /* exp: hit */
extern s32 lbl_8011BB20[];  /* exp: kill */
extern void AddExp(s32 playerIdx, s32 exp, s32 award);
extern s32 msgPost();
extern f32 lbl_80346310;
s32 start_magic();

/* Low-level combat services recovered in other game TUs.  K&R declarations
 * retain the original vararg/floating-register call contracts. */
extern void damage_enemy();
extern s32 damage_player(s32 i, f32 dmg, s32 mode, u32 flags, f32* dir);
extern s32 StartFXTree();
extern void SfxSetDamage();
extern void SfxSetHit();
extern void SfxSetMat();
extern void SfxSetOwner();

/* in-TU forward references */
void recalc_lookat(s32 camIdx, s32 snap);
void get_attn_pos_8002C9A8(s32 camIdx, f32* out);
void ProcCamera_8002E548(s32 camIdx, s32 useRecorderPosition);
void StandardCamera_8002B828(s32 camIdx);
void init_targets(void);
s32 LineCylinderCollide(f32* center, f32 radius, f32 halfHeight,
                        f32* from, f32* to, f32* hit, s32 directional);
s32 StartMissile(s32 owner, f32* position, f32* velocity, u32 damageType,
                 MissileInfo* desc, void* missileTree, s32 variant,
                 u32 extraFlags, f32 scale, f32 damageMag);
/* StartMissile FX/vibration constants */
extern f32 lbl_803463C0, lbl_8034633C, lbl_80346328, lbl_803463D0;
extern f64 lbl_80346348, lbl_80346350, lbl_80346340, lbl_803463C8;
extern s32 optionsAudioAndPrefs30[8];
extern u32 lbl_8011A178[], lbl_8011A188[];
extern plyr_data* lbl_80282930[];
void SfxSetPhysics();
void SfxSetStreak();

#define PF(base, off, type) (*(type*)((u8*)(base) + (off)))
#define PLAYER_STRIDE 0x335C
#define ENEMY_STRIDE  0x394

/*
 * DiffRate_8002951C -- rate-limit a camera angular value.  CameraSupervisor supplies
 * the destination and rate state; wrapping before the comparison is critical
 * because the shortest turn can cross +/-pi.
 */
extern f32 lbl_8023F818, lbl_8023F81C, lbl_8023F820, lbl_8034444C;
extern f32 lbl_80344534;
extern s32 lbl_80344400;
void CameraSupervisor(s32 camIdx);

typedef struct MissileSpread {
    f32 value[5];
} MissileSpread;

/* PlayerStartMissile's 5-way spread: cosine and sine of 0, +/-15 and
   +/-30 degrees. Retail .rodata 0x80111DE0 and 0x80111DF4. */
static const MissileSpread lbl_80111DE0 = {{1.0f, 0.966f, 0.966f, 0.866f, 0.866f}};
static const MissileSpread lbl_80111DF4 = {{0.0f, 0.259f, -0.259f, 0.5f, -0.5f}};

void PlayerDamagedEnemy(void* player, void* enemy, s32 state, s32 damage,
                        s32 flag)
{
    s32 t;

    if (((Enemy*)enemy)->type == gBossType && gBossActive == 0) {
        return;
    }
    if (state != 1 && state != 6) {
        return;
    }
    if (damage > 0 && ((Enemy*)enemy)->birth_style == 0) {
        s32 c = *(s32*)((u8*)player + offsetof(Player, hit_streak)) + 1;
        *(s32*)((u8*)player + offsetof(Player, hit_streak)) = c;
        if (c >= 10 && gBossType < 0) {
            msgPost(22, ((Player*)player)->index,
                    (u32)((Player*)player)->col_pos);
        }
    }

    t = ((Enemy*)enemy)->type;
    if (t == -2) {
        t = 1;
    } else if (t == -3) {
        t = 2;
    } else if (t < 0) {
        t = 0;
    }

    if (flag != 0) {
        if (damage != 0) {
            AddExp(((Player*)player)->index, lbl_8011BCB8[t], 1);
        } else {
            AddExp(((Player*)player)->index, lbl_8011BC30[t], 1);
        }
    } else if (damage != 0) {
        AddExp(((Player*)player)->index, lbl_8011BBA8[t], 0);
    } else {
        AddExp(((Player*)player)->index, lbl_8011BB20[t], 0);
    }
}

void PlayerDamagedItem(void* player, void* item, s32 flag)
{
    s32* info = *(s32**)item;
    s32 type = info[0];
    s32 t;

    if (type == 2) {
        goto out;
    }
    if (type >= 2) {
        goto big;
    }
    if (type >= 1) {
        goto small;
    }
    goto out;
big:
    if (type >= 4) {
        goto out;
    }
    if (flag != 0) {
        PF(player, offsetof(Player, hit_streak), s32) = 0;
    }
    if (flag != 0) {
        s32* count = (s32*)((u8*)player +
                           ((Player*)player)->character * 0x1C);
        count[0xC20 / sizeof(s32)] = count[0xC20 / sizeof(s32)] + 1;
    }
    t = *(s16*)((Item*)item)->data.raw;
    if (t == -2) {
        t = 1;
    } else if (t == -3) {
        t = 2;
    } else if (t < 0) {
        t = 0;
    }
    if (flag != 0) {
        AddExp(((Player*)player)->index, lbl_8011BBA8[t] * 5, 0);
    } else {
        AddExp(((Player*)player)->index, lbl_8011BB20[t] * 5, 0);
    }
    goto out;
small:
    if (((Item*)item)->activetime > 0) {
        goto out;
    }
    switch (info[1]) {
    case 4:
    {
        f32 pos[3];
        f32 vec[3];
        s32 magic = info[0xF];
        pos[0] = ((Item*)item)->objgrp.attn_pos[0];
        pos[1] = ((Item*)item)->objgrp.attn_pos[1];
        pos[2] = ((Item*)item)->objgrp.attn_pos[2];
        vec[0] = ((Player*)player)->col_pos[0];
        vec[1] = ((Player*)player)->col_pos[1];
        vec[2] = ((Player*)player)->col_pos[2];
        start_magic(((Player*)player)->index, (s32*)pos, magic, 0,
                    lbl_80346310);
        msgPost(0xE, ((Player*)player)->index, (u32)vec);
        DeleteItem(item, 1);
        break;
    }
    }
out:
    return;
}

extern f64 lbl_80346318, lbl_80346320;
extern f32 lbl_8034632C, lbl_80346330, lbl_80346334, lbl_80346338;

void ModifyDamage(f32* damage, u32* damageType, u32 shield, f32 armor)
{
    f32 value = *damage;
    u32 type = *damageType;
    f32 weak;
    f32 resist;
    f32 strong;
    u32 color;

    if ((shield & 0x100000) != 0) {
        if ((f64)value > lbl_80346318) {
            *damage = (f32)(lbl_80346320 * -(f64)value);
        } else {
            *damage = lbl_80346328;
        }
        return;
    }
    if ((shield & 0x10000) != 0 ||
        ((shield & 0x1000) != 0 && (type & 0x200) != 0) ||
        ((shield & 0x2000) != 0 && (type & 0x800) != 0)) {
        *damage = lbl_80346328;
        return;
    }

    if (gBossType >= 0) {
        weak = lbl_8034632C;
        resist = lbl_80346330;
        strong = lbl_80346334;
    } else {
        weak = lbl_80346338;
        resist = lbl_80346334;
        strong = lbl_8034633C;
    }
    if ((shield & 0x40000) != 0) {
        *damageType &= 0xFFFEFE8F;
    }
    if ((shield & 0x10) != 0 && (type & 0x200) != 0) {
        value *= weak;
    }

    if ((type & 0xA00) == 0) {
        if (value >= lbl_80346328) {
            if (value <= armor) {
                value = lbl_80346328;
            } else {
                value = value - armor;
            }
        } else {
            value = -value;
        }
    }
    if ((f64)value > lbl_80346340) {
        color = type & 0xF;
        switch (color) {
        case 1:
            if ((shield & 1) != 0) {
                value *= weak;
            } else if ((shield & 0x100) != 0) {
                value = lbl_80346328;
            } else if ((shield & 2) != 0 || (shield & 0x200) != 0) {
                value *= strong;
            } else {
                value *= resist;
            }
            break;
        case 2:
            if ((shield & 2) != 0) {
                value *= weak;
            } else if ((shield & 0x200) != 0) {
                value = lbl_80346328;
            } else if ((shield & 1) != 0 || (shield & 0x100) != 0) {
                value *= strong;
            } else {
                value *= resist;
            }
            break;
        case 3:
            if ((shield & 4) != 0) {
                value *= weak;
            } else if ((shield & 0x400) != 0) {
                value = lbl_80346328;
            } else if ((shield & 8) != 0 || (shield & 0x800) != 0) {
                value *= strong;
            } else {
                value *= resist;
            }
            break;
        case 4:
            if ((shield & 8) != 0) {
                value *= weak;
            } else if ((shield & 0x800) != 0) {
                value = lbl_80346328;
            } else if ((shield & 4) != 0 || (shield & 0x400) != 0) {
                value *= strong;
            } else {
                value *= resist;
            }
            break;
        }
    }
    *damage = value;
}

s32 DamageColor(s32 type)
{
    switch (type & 0xF) {
    default:
        return -1;
    case 1:
        return 2;
    case 2:
        return 1;
    case 3:
        return 0;
    case 4:
        return 3;
    }
}

s32 LineCylinderCollide(f32* center, f32 radius, f32 halfHeight,
                        f32* from, f32* to, f32* hit, s32 directional)
{
    f32 closest[3];
    f32 delta[3];
    f32 centerDir[3];
    union {
        f32 value;
        u32 bits;
    } absDeltaY[4];
    f32 distance;
    f32 absoluteY;

    distance = PointLineColl(center, from, to, closest);
    if (distance > radius + halfHeight) {
        return 0;
    }
    delta[0] = closest[0] - center[0];
    delta[1] = closest[1] - center[1];
    delta[2] = closest[2] - center[2];
    distance = fqdist(delta[0], delta[2]);
    absDeltaY[1].value = delta[1];
    absDeltaY[1].bits &= 0x7FFFFFFF;
    absoluteY = absDeltaY[1].value;
    if (distance > radius || absoluteY > halfHeight) {
        return 0;
    }

    if (directional != 0) {
        delta[0] = center[0] - from[0];
        delta[1] = 0.0f;
        delta[2] = center[2] - from[2];
        distance = NormalVector2D(delta);
        if (distance <= radius) {
            centerDir[0] = to[0] - from[0];
            centerDir[1] = 0.0f;
            centerDir[2] = to[2] - from[2];
            {
                f32 lineLength = NormalVector2D(centerDir);
                if (distance < lbl_80346348) {
                    if (lineLength < lbl_80346348) {
                        if (hit != 0) {
                            hit[0] = closest[0];
                            hit[1] = closest[1];
                            hit[2] = closest[2];
                        }
                        return 1;
                    }
                    return 0;
                }
                if (centerDir[0] * delta[0] +
                    centerDir[2] * delta[2] < lbl_80346350) {
                    return 0;
                }
            }
        }
    }
    if (hit != 0) {
        hit[0] = closest[0];
        hit[1] = closest[1];
        hit[2] = closest[2];
    }
    return 1;
}

s32 MissileCollideEnemy(f32 radius, f32* from, f32* to, f32* hit,
                        s32 cooldownSlot, s32 respectCooldown, s32 firstEnemy)
{
    f32 segX = to[0] - from[0];
    f32 segY = to[1] - from[1];
    f32 segZ = to[2] - from[2];
    f32 horizontalLen2 = segX * segX + segZ * segZ;
    f32 verticalLen2 = segY * segY;
    s32 i;

    for (i = firstEnemy; i < 25; i++) {
        u8* enemy = (u8*)gEnemies + i * ENEMY_STRIDE;
        s32 state = PF(enemy, offsetof(Enemy, state), s32);
        f32* cool;
        if ((state == 1 || state == 6) &&
            (!respectCooldown ||
             !(sMusicFadeBase < (cool = (f32*)(enemy + offsetof(Enemy, fxhittime)))[cooldownSlot]))) {
            u8 _pad[8];
            f32 dx;
            f32 enemyRadius;
            f32 dz;
            f32 enemyHeight;
            f32 eh2;
            enemyRadius = radius + PF(enemy, offsetof(Enemy, rad), f32);
            dz = PF(enemy, offsetof(Enemy, objgrp.coll_pos[2]), f32) - to[2];
            dx = PF(enemy, offsetof(Enemy, objgrp.coll_pos[0]), f32) - to[0];
            enemyHeight = radius + PF(enemy, offsetof(Enemy, hht), f32);
            eh2 = enemyHeight;
            if (!(dx * dx + dz * dz > enemyRadius * enemyRadius + horizontalLen2) &&
                !(PF(enemy, offsetof(Enemy, objgrp.coll_pos[1]), f32) - to[1] > verticalLen2 + enemyHeight) &&
                LineCylinderCollide((f32*)(enemy + offsetof(Enemy, objgrp.coll_pos[0])), enemyRadius,
                                    eh2, from, to, hit, 0)) {
                return i;
            }
        }
    }
    return -1;
}

void* MissileCollidePlayer(f32 radius, f32* from, f32* to, f32* hit)
{
    u8 framePad[8];
    s32 i;

    for (i = 0; i < 4; i++) {
        Player* player = &gPlayers[i];
        if (player->state == 1) {
            f32 halfHeight = radius + player->col_height;
            f32 cylinderRadius = radius + player->col_radius;
            if (LineCylinderCollide(player->effectpos,
                                    cylinderRadius, halfHeight,
                                    from, to, hit, 0)) {
                return player;
            }
        }
    }
    return 0;
}

void CalcTargetDir(f32* velocity, f32 targetScale, f32 speed,
                   f32 gravity, f32 lift)
{
    u8 framePad[8];
    f32 horizontal = fqdist(velocity[0], velocity[2]);
    f64 inverse;

    if ((f64)horizontal > 0.001) {
        inverse = 1.0 / (f64)horizontal;
    } else {
        inverse = 1.0;
    }
    {
        f32 norm = (f32)inverse;
        velocity[0] = velocity[0] * norm;
        velocity[2] = velocity[2] * norm;
        velocity[1] =
            (f32)((f64)speed *
                  (0.5 * (f64)gravity *
                       (f64)(f32)(horizontal * speed) +
                   (f64)((velocity[1] + lift) * (targetScale * norm))));
    }
}

/* EnemyStartMissile constants + helpers */
extern f64 lbl_80346318, lbl_80346360, lbl_80346370;
extern f64 lbl_80346378, lbl_80346388, lbl_80346390, lbl_80346398;
extern f32 lbl_80346368, lbl_80346358, lbl_8034635C, lbl_80346380, lbl_80346334;
extern f32 lbl_80346384, lbl_803447D8;
extern u8 lbl_8011A1B4[];
f32 NormalVector(f32* v);
f32 NormalVector2D(f32* v);
f32 Random(f32 range);
s32 WeaponWallCollide();
s32 fn_8005ED44();
void SfxSetLight();

s32 EnemyStartMissile(void* enemy, f32* launchPos, f32* target, s32 slot)
{
    s32 enemyType = PF(enemy, offsetof(Enemy, type), s32);
    MissileInfo* desc = &EnemyMissileInfo[enemyType][slot];
    void* tree = EnemyMissileTree[enemyType][slot];
    f32 dir[3];
    f32 aim[3];
    f32 spawn[3];
    f32 flat[3];
    f32 _pad[3];
    f32 speed;
    f32 invSpeed;
    f32 height;
    u32 flags;
    u32 extraFlags = 0;
    s32 fx;

    if (tree == 0) {
        ErrorPrintf("ENEMY %d HAS NO MISSILE TYPE %d", enemyType, slot);
        return 0;
    }
    invSpeed = (f32)(lbl_80346318 /
                     (f64)(speed = desc->speed *
                                   PF(gCurLevel, offsetof(level_data, ene_mspeed), f32)));
    dir[0] = launchPos[0] - target[0];
    dir[1] = launchPos[1] - target[1];
    dir[2] = launchPos[2] - target[2];
    if (slot == 2) {
        NormalVector(dir);
    } else {
        f32 spread = slot == 1 ? lbl_80346358 : lbl_8034635C;
        f64 rnd = Random(lbl_80346368);
        f32 weight = desc->weight;
        f32 lead = (f32)((f64)PF(gCurLevel, offsetof(level_data, ene_macc), f32) *
                         (lbl_80346360 + rnd) + (f64)spread);
        f32 horiz = fqdist(dir[0], dir[2]);
        f32 hn = (f32)(horiz > lbl_80346348 ?
                       lbl_80346318 / (f64)horiz : lbl_80346318);
        dir[0] = dir[0] * hn;
        dir[2] = dir[2] * hn;
        dir[1] = (f32)((f64)invSpeed *
            (lbl_80346370 * (f64)weight * (f64)(horiz * invSpeed) +
             (f64)((dir[1] + lead) * (speed * hn))));
    }
    if (dir[1] < lbl_80346340) {
        dir[1] = lbl_80346328;
    }
    flat[0] = dir[0];
    flat[1] = dir[1];
    flat[2] = dir[2];
    NormalVector2D(flat);
    if (flat[0] * PF(enemy, offsetof(Enemy, objgrp.worldmat[2][0]), f32) +
        flat[2] * PF(enemy, offsetof(Enemy, objgrp.worldmat[2][2]), f32) <
        lbl_80346378) {
        return 0;
    }
    {
        flags = PF(enemy, offsetof(Enemy, atts.damagetype), u32);
        aim[0] = target[0];
        aim[1] = target[1];
        aim[2] = target[2];
        if (slot == 2) {
            height = lbl_80346328;
        } else {
            height = lbl_80346380;
        }
        switch (PF(enemy, offsetof(Enemy, type), s32)) {
        case 4:
            if (slot == 0) height = lbl_80346334;
            break;
        case 0xD:
            if (slot == 0) height = lbl_80346384;
            break;
        case 7:
        case 0x18:
            if (slot == 2) height = lbl_80346334;
            break;
        case 0xE:
            if (slot == 2) height = lbl_80346384;
            else if (slot == 0) height = lbl_80346334;
            break;
        case 0x11:
            height = lbl_8034633C;
            aim[0] = (f32)(lbl_80346388 * (f64)dir[0] + (f64)aim[0]);
            aim[2] = (f32)(lbl_80346388 * (f64)dir[2] + (f64)aim[2]);
            break;
        case 0x17:
            if (slot == 0) {
                height = lbl_80346328;
            } else if (slot == 1) {
                height = lbl_80346328;
                aim[0] = (f32)-(lbl_80346390 * (f64)dir[0] - (f64)aim[0]);
                aim[2] = (f32)-(lbl_80346390 * (f64)dir[2] - (f64)aim[2]);
            }
            break;
        case 0x1B:
            height = lbl_80346328;
            extraFlags |= 0x8000000;
            flags |= 0x100000;
            break;
        }
        aim[1] = aim[1] + height;
        spawn[0] = (f32)(lbl_80346398 * (f64)dir[0] + (f64)aim[0]);
        spawn[1] = (f32)(lbl_80346398 * (f64)dir[1] + (f64)aim[1]);
        spawn[2] = (f32)(lbl_80346398 * (f64)dir[2] + (f64)aim[2]);
        if ((u32)WeaponWallCollide(aim, spawn, 0, desc->collisionRadius) != 0) {
            return 0;
        }
        if ((u32)fn_8005ED44(aim, spawn, 0, 0, -1, desc->collisionRadius) != 0) {
            return 0;
        }
        {
            f32 damage = desc->damage;
            if (lbl_803447D8 < lbl_80346318) {
                damage = (f32)((f64)damage * lbl_80346370);
            }
            if (slot == 0) flags |= 0x20000;
            if (slot == 2) flags |= 0x40000;
            fx = StartMissile(0, spawn, dir, flags, desc, tree, 0,
                              extraFlags, speed, damage);
        }
        if (lbl_803447D8 < lbl_80346318) {
            ScaleFX(fx, lbl_803447D8, lbl_803447D8, lbl_803447D8);
        }
        if (slot == 1) {
            SfxSetLight(fx, lbl_8011A1B4, lbl_80346368);
        }
    }
    return 1;
}

/* PlayerStartMissile's local-space launch offset at retail 0x8011A1A8. */
f32 lbl_8011A1A8[3] = {0.0f, -0.5f, -1.25f};
extern void* EffectInfo[];
typedef struct PlayerMissileAnode {
    s32 node;
} PlayerMissileAnode;
typedef struct PlayerMissileAtree {
    PlayerMissileAnode* root;
    u8 _pad04[0x44];
} PlayerMissileAtree;
typedef struct PlayerMissileEffect {
    u8 _pad00[0x14];
    void* node;
    PlayerMissileAtree atree;
    u8 _pad60[8];
    f32 endtime;
    u8 _pad6C[0x7C];
    s16 childfx;
    u8 _padEA[6];
} PlayerMissileEffect;
extern PlayerMissileEffect Effects[64];
extern f32 lbl_803463B0;
extern f64 lbl_803463A0, lbl_803463A8, lbl_803463B8;
void MulVecMat3(f32* in, f32* out, f32* mat);
s32 fn_80094080(f32* position, u32 damageType);
f32 fn_8005C1DC(void* item, f32 power, s32 flags, s32 owner);
s32 fn_80094440(f32* position, u32 damageType, s32 destroyed);
s32 StartFXSub(s32 type, f32* position, u32 flagsA, u32 flagsB, f32 time);
void DeleteEffect();
void MBNodeSetParent(s32 a, s32 b);
void MBTreeSetFlags(s32 node, s32 mask, s32 value);

/*
 * PlayerStartMissile -- launch a player weapon.  Derives the aim direction
 * from the player's facing (or weapon object), applies wall/target aim-assist,
 * then spawns up to five missiles in the weapon's spread pattern, linking and
 * lighting each spawned effect.  Returns the number of launch slots processed.
 */
s32 PlayerStartMissile(s32* player, f32* direction, s32 damageType, s32 mode,
                       f32 speedArg, f32 scaleArg)
{
    Player* playerView = (Player*)player;
    s32 idx = playerView->index;
    s32* missileFx = pmissile_sfxidx;
    MissileTreeInfo* treeInfo = &PlayerMissileTreeInfo[idx];
    s32 pflags = playerView->flags;
    s32 trailFx = (s32)treeInfo->throwFlags;
    s32 special = pflags & 0x8000;
    s32 dmgLow = damageType & 0xF;
    s32 extraFlags = 0;
    s32 useSpecial = 0;
    MissileInfo* desc;
    f32 aim[3];
    f32 pt0[3];
    f32 pt1[3];
    f32 wallHit[3];
    u8 unused[24];
    MissileSpread spreadB;
    MissileSpread spreadA;
    f32 launchDir[3];
    f32 scale;
    f32 invSpeed;
    void* tree;
    s32 i;

    if (special != 0) {
        desc = &BossElecMissileInfo;
        useSpecial = 1;
    } else if ((pflags & 0x4000) != 0) {
        desc = &BossAcidMissileInfo;
        useSpecial = 1;
    } else if ((damageType & 0x100000) != 0 &&
               (damageType & 0x2000000) == 0) {
        desc = &BallistaMissileInfo;
        useSpecial = 1;
    } else {
        desc = &PlayerMissileInfo[playerView->char_type];
    }
    scale = playerView->stat_missile_dmg * scaleArg;

    if ((pflags & 0x400) != 0) {
        MulVecMat3(lbl_8011A1A8, aim, playerView->mat);
    } else {
        if (mode == 1) {
            MulVecMat3(lbl_80282930[idx]->weapon_offset, aim,
                       playerView->mat);
        } else if (mode == 2) {
            MulVecMat3(lbl_80282930[idx]->turboa_offset, aim,
                       playerView->mat);
        } else {
            aim[0] = lbl_80346328;
            aim[1] = lbl_80346328;
            aim[2] = lbl_80346328;
        }
    }
    aim[0] = playerView->effectpos[0] + aim[0];
    aim[1] = playerView->effectpos[1] + aim[1];
    aim[2] = playerView->effectpos[2] + aim[2];
    pt0[0] = (f32)(lbl_803463A0 * (f64)direction[0] + (f64)aim[0]);
    pt0[1] = (f32)(lbl_803463A0 * (f64)direction[1] + (f64)aim[1]);
    pt0[2] = (f32)(lbl_803463A0 * (f64)direction[2] + (f64)aim[2]);
    pt1[0] = (f32)(lbl_80346388 * (f64)direction[0] + (f64)aim[0]);
    pt1[1] = (f32)(lbl_80346388 * (f64)direction[1] + (f64)aim[1]);
    pt1[2] = (f32)(lbl_80346388 * (f64)direction[2] + (f64)aim[2]);

    if ((damageType & 0x100000) == 0) {
        if ((u32)WeaponWallCollide(desc->collisionRadius, pt0, pt1,
                                   wallHit) != 0) {
            if ((damageType & 0x200000) != 0) {
                pt1[0] = pt0[0];
                pt1[1] = pt0[1];
                pt1[2] = pt0[2];
            } else {
                fn_80094080(wallHit, damageType);
                return -1;
            }
        }
    }
    {
        s32* hit = (s32*)fn_8005ED44(desc->collisionRadius, pt0, pt1, wallHit,
                                     0, playerView->index);
        if (hit != 0) {
            f32 dmg = fn_8005C1DC(hit, scale, damageType, playerView->index);
            if (dmg >= lbl_80346340) {
                u32 killed;
                if (lbl_80346340 == dmg) {
                    killed = 1;
                } else {
                    killed = 0;
                }
                PlayerDamagedItem(player, hit, killed);
                fn_80094440(wallHit, damageType, killed);
            } else {
                fn_80094080(wallHit, damageType);
            }
            if ((damageType & 0x100000) == 0) {
                return -1;
            }
        }
    }

    {
        f64 clampedValue;
        f32 clamped;
        if ((f64)playerView->stat_missile_spd < lbl_80346318) {
            clampedValue = lbl_80346318;
        } else if ((f64)playerView->stat_missile_spd > lbl_803463A8) {
            clampedValue = lbl_803463A8;
        } else {
            clampedValue = (f64)playerView->stat_missile_spd;
        }
        clamped = (f32)clampedValue;
        invSpeed = (f32)(lbl_80346318 / (f64)clamped);
        if ((damageType & 0x100000) == 0 && lbl_80346340 < (f64)speedArg) {
            f32 horiz;
            f32 hn;
            f32 weight;
            direction[0] *= speedArg;
            direction[1] *= speedArg;
            direction[2] *= speedArg;
            weight = desc->weight;
            horiz = (f32)fqdist(direction[0], direction[2]);
            hn = (f32)(horiz > lbl_80346348 ?
                       lbl_80346318 / (f64)horiz : lbl_80346318);
            direction[0] = direction[0] * hn;
            direction[2] = direction[2] * hn;
            direction[1] = (f32)((f64)invSpeed *
                (lbl_80346370 * (f64)weight *
                    (f64)(horiz * invSpeed) +
                 (f64)((direction[1] + lbl_803463B0) *
                    (clamped * hn))));
        }

        {
            s32 f = playerView->flags;
            if ((f & 0x8000) != 0) {
                extraFlags |= 0x10000;
                tree = BossElecTree;
            } else if ((f & 0x4000) != 0) {
                extraFlags |= 0x10000;
                tree = BossAcidTree;
            } else if ((f & 0x400) != 0) {
                extraFlags |= 0x10000;
                tree = FamiliarSpit[4];
            } else if ((damageType & 0x100000) != 0 &&
                       (damageType & 0x2000000) == 0) {
                tree = BallistaTree;
            } else {
                tree = treeInfo->throwHeader;
            }
        }

        {
        f64 c340 = lbl_80346340;
        f64 c318 = lbl_80346318;
        f64 c3B8 = lbl_803463B8;
        for (i = 0; i < 5; i++) {
            s32 fx;
            spreadB = lbl_80111DE0;
            spreadA = lbl_80111DF4;
            if ((i > 0 && (damageType & 0x480000) == 0) ||
                (i > 2 && (damageType & 0x400000) == 0)) {
                break;
            }
            launchDir[1] = direction[1];
            launchDir[0] = direction[0] * spreadB.value[i] +
                           direction[2] * spreadA.value[i];
            launchDir[2] = -direction[0] * spreadA.value[i] +
                           direction[2] * spreadB.value[i];
            if ((playerView->char_type == 2 || playerView->char_type == 6) &&
                WeapThrowFx[playerView->index][dmgLow] >= 0 &&
                !useSpecial) {
                tree = EffectInfo[0];
            }
            fx = StartMissile(playerView->index + 1, pt1, launchDir, damageType, desc,
                              tree, i, extraFlags, clamped, scale);
            if (fx >= 0) {
                if (!useSpecial) {
                    s32 tw = WeapThrowFx[playerView->index][dmgLow];
                    if (tw >= 0) {
                        u32 fxFlags = 0x81880;
                        s32 sub;
                        if (dmgLow == 1) {
                            fxFlags |= 0x800000;
                        }
                        if ((playerView->char_type == 2 || playerView->char_type == 6) &&
                            !useSpecial && dmgLow == 0) {
                            fxFlags |= 0x3000000;
                        }
                        sub = StartFXSub(tw, 0, 0, fxFlags, lbl_80346328);
                        if (sub == fx) {
                            DeleteEffect(sub, 1);
                            fx = -1;
                        } else {
                            PlayerMissileEffect* effect = &Effects[fx];
                            if ((f64)effect->endtime <= c340) {
                                DeleteEffect(fx, 1);
                                DeleteEffect(sub, 1);
                                fx = -1;
                            } else if (sub >= 0) {
                                effect->childfx = (s16)sub;
                                MBNodeSetParent(Effects[sub].atree.root->node,
                                                effect->atree.root->node);
                            }
                        }
                    }
                }
                if (fx >= 0) {
                    f32 scaleFx = lbl_80346384;
                    if (trailFx != 0) {
                        MBTreeSetFlags(*(s32*)((u8*)Effects[fx].node + 0x78),
                                       trailFx, 2);
                    }
                    if ((f64)scaleArg > c318) {
                        scaleFx *= scaleArg;
                    }
                    if (playerView->level >= 99) {
                        scaleFx = (f32)((f64)scaleFx * c3B8);
                    }
                    if (c318 != (f64)scaleFx) {
                        ScaleFX(fx, scaleFx, scaleFx, scaleFx);
                    }
                }
            }
            missileFx[i] = fx;
        }
        }
    }
    return i;
}

/*
 * StartMissile -- common effect allocator/configurator shared by enemy and
 * player launch paths.  MissileInfo is the PDB-described runtime definition;
 * the effect system owns movement after these attributes are installed.
 */
s32 StartMissile(s32 owner, f32* position, f32* velocity, u32 damageType,
                 MissileInfo* desc, void* missileTree, s32 variant,
                 u32 extraFlags, f32 scale, f32 damageMag)
{
    f32 color = lbl_803463C0;
    s32 wallSound = desc->wallSound;
    f32 vel[3];
    s32 big;
    s32 fx;
    f32 radius;

    if ((damageType & 0x480000) != 0) {
        color = lbl_8034633C;
        if (wallSound == 5) {
            if (variant == 0) {
                if ((damageType & 0x400000) != 0) {
                    wallSound = 7;
                } else {
                    wallSound = 6;
                }
            } else {
                wallSound = 0;
            }
        }
    }
    vel[0] = velocity[0] * scale;
    vel[1] = velocity[1] * scale;
    vel[2] = velocity[2] * scale;
    if ((f64)(vel[2] * vel[2] +
        (f32)(vel[0] * vel[0] + (f32)(vel[1] * vel[1]))) < lbl_80346348) {
        FatalError("ERROR: ZERO LENGTH MISSILE VEL", 0x800000);
    }
    if (owner > 0) {
        if (optionsAudioAndPrefs30[7] == 1) {
            extraFlags |= 0x200F;
        } else if (optionsAudioAndPrefs30[7] == 2) {
            extraFlags |= 0xF;
        } else {
            extraFlags |= 0x20E;
        }
        if ((damageType & 0x100000) != 0) {
            extraFlags &= ~0x4u;
        }
    } else {
        extraFlags |= 0x1107;
    }
    if (lbl_80346340 == (f64)desc->angularVelocity[0] &&
        lbl_80346340 == (f64)desc->angularVelocity[1] &&
        lbl_80346340 == (f64)desc->angularVelocity[2]) {
        extraFlags |= 0x20000;
    }
    extraFlags |= 0x1000000;
    fx = StartFXTree(missileTree, position, extraFlags, 0x80000, color);
    big = damageType & 0x2000000;
    radius = desc->collisionRadius;
    if (big != 0) {
        radius = (f32)((f64)radius * lbl_803463C8);
    }
    SfxSetPhysics(fx, vel, desc->angularVelocity, desc->weight, radius);
    SfxSetHit(fx, desc->hitEffect, desc->hitSound, wallSound);
    SfxSetDamage(fx, damageType | desc->damageType, owner, damageMag,
                 desc->hitRadius, lbl_80346328);
    if (big != 0) {
        ScaleFX(fx, lbl_803463D0, lbl_803463D0, lbl_803463D0);
    }
    if (owner > 0) {
        s32 tex = WeaponStreakTex;
        s32 vibColor;
        s32 vibIntensity;
        if ((damageType & 0x100000) != 0 && big == 0) {
            vibColor = 0xFFFFFF;
            vibIntensity = 64;
        } else {
            u8* pl = (u8*)gPlayers + owner * PLAYER_STRIDE;
            vibColor = lbl_8011A178[*(s32*)(pl - 13144)];
            vibIntensity = lbl_8011A188[*(s32*)(pl - 13140)];
            if (big != 0) {
                vibIntensity += 64;
                if ((u32)vibIntensity >= 255) {
                    vibIntensity = 255;
                }
            }
        }
        SfxSetStreak(fx, tex, vibColor, vibIntensity, lbl_80346328,
            lbl_80282930[owner - 1]->streakfwdmul);
    }
    return fx;
}

void InitEnemyMissiles(s32 enemyType)
{
    char buf[32];
    s32 slot;

    for (slot = 0; slot < 3; slot++) {
        if (gWadAtreeHeaders[enemyType] != NULL) {
            char* p;
            sprintf(buf, lbl_803463D4, EnemyTypeDesc(enemyType),
                    EnemyMissileDesc[slot]);
            for (p = buf; *p != '\0'; p++) {
                *p = (char)toupper(*p);
            }
            EnemyMissileTree[enemyType][slot] =
                AtreeMatch(gWadAtreeHeaders[enemyType], buf, 0);
        } else {
            EnemyMissileTree[enemyType][slot] = NULL;
        }
    }
}

extern void *sWeaponsBuf, *sPowerupsBuf;
void* MBOX_FindTexture(char* name, void* arg);
s32 InitCustomEffect(void* tree, char* name, s32 zmod, s32 alpha);

typedef struct CombatPlayerModelSlot {
    u8 _pad00[0x30];
    void* powerupWad;
    u8 _pad34[0x14];
    void* weaponWad;
} CombatPlayerModelSlot;

extern CombatPlayerModelSlot player_multiple_models[4];

void InitPlayerMissiles(void* player)
{
    s32 idx = PF(player, offsetof(Player, index), s32);
    s32 charType = PF(player, offsetof(Player, character), s32);
    s32 throwLevel = PF(player, offsetof(Player, level), s32) / 10;
    char* charName = PlayerMissileDesc[charType].throwDescription;
    s32 throwByte = (u8)PlayerMissileDesc[charType].throwLevel[throwLevel];
    void* weaponWad = player_multiple_models[idx].weaponWad;
    void* powerupWad = player_multiple_models[idx].powerupWad;
    void** holdFx = WeapHoldFxTree[idx];
    s32* throwFx = WeapThrowFx[idx];
    char buf[24];
    s32 missing = 0;
    s32 i;

    if (throwByte == 0x30) {
        sprintf(buf, "%s_THROW0", charName);
        PlayerMissileTreeInfo[idx].throwHeader = AtreeMatch(powerupWad, buf, 0);
    } else {
        sprintf(buf, "%s_THROW%c", charName, (char)throwByte);
        PlayerMissileTreeInfo[idx].throwHeader = AtreeMatch(weaponWad, buf, 0);
    }
    if (PlayerMissileTreeInfo[idx].throwHeader == 0) {
        sprintf(buf, "%s_THROW1", charName);
        PlayerMissileTreeInfo[idx].throwHeader = AtreeMatch(powerupWad, buf, 0);
        if (PlayerMissileTreeInfo[idx].throwHeader == 0) {
            ErrorPrintf("Player Missile not found: %s", buf);
            missing = 1;
        }
    }
    PlayerMissileTreeInfo[idx].throwFlags = PlayerMissileDesc[charType].flags;

    for (i = 0; i < 5; i++) {
        char* name = DmgTypeDesc[i];
        if (name[0] != '\0') {
            sprintf(buf, "WEAP_HOLD_%s", name);
            buf[15] = '\0';
            holdFx[i] = AtreeMatch(weaponWad, buf, 1);
            sprintf(buf, "WEAP_TW_%c", name[0]);
            buf[15] = '\0';
            throwFx[i] = InitCustomEffect(weaponWad, buf, 0, 0);
        } else {
            holdFx[i] = 0;
            throwFx[i] = -1;
        }
    }
    WeaponStreakTex = (s32)MBOX_FindTexture("WEP_STREAK", 0);
    BallistaTree = AtreeMatch(sWeaponsBuf, "SUPERARROW", 1);
    BossElecTree = AtreeMatch(sWeaponsBuf, "BOSSG_ELEC", 1);
    BossAcidTree = AtreeMatch(sWeaponsBuf, "BOSSG_ACID", 1);
    if (sPowerupsBuf != 0) {
        PhoenixTree = AtreeMatch(sPowerupsBuf, "PHOENIX", 1);
        WingsTree = AtreeMatch(sPowerupsBuf, "WINGS", 1);
        PojoTree = AtreeMatch(sPowerupsBuf, "POJO", 1);
        BreatheFireTree = AtreeMatch(sPowerupsBuf, "HEAD_BREATHEF", 1);
        BreatheElecTree = AtreeMatch(sPowerupsBuf, "HEAD_BREATHEE", 1);
        BreatheAcidTree = AtreeMatch(sPowerupsBuf, "HEAD_BREATHEA", 1);
    } else {
        PhoenixTree = 0;
        WingsTree = 0;
        PojoTree = 0;
        BreatheFireTree = 0;
        BreatheElecTree = 0;
        BreatheAcidTree = 0;
    }
    FireShieldTree = AtreeMatch(sWeaponsBuf, "FW_SHLD_ACTIVE", 1);
    FamiliarTree[idx][0] = AtreeMatch(weaponWad, "FAMILIAR1", 1);
    FamiliarTree[idx][1] = AtreeMatch(weaponWad, "FAMILIAR2", 1);
    FamiliarSpit[idx] = AtreeMatch(weaponWad, "FAMILIAR_SPIT", 1);
    FamiliarSpit[4] = AtreeMatch(sWeaponsBuf, "PHOENIX_FBALL", 1);
    if (missing) {
        FatalError("InitPlayerMissiles failed.", 0x800000);
    }
}

void ResetPlayerMissiles(void)
{
    memset(PlayerMissileTreeInfo, 0, sizeof(PlayerMissileTreeInfo));
}
