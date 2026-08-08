/*
 * combat.c -- the linker-combined tail of CAMERA.OBJ, CLOCK.OBJ, and
 * COMBAT.OBJ.
 *
 * The GameCube split map merges these three source modules into one object
 * window.  Function names come from the Xbox PDB module rosters where the
 * behavior agrees with the GCN call graph.  The large camera functions are
 * platform-specific translations; the clock and missile/collision functions
 * retain the original source names and contracts.
 *
 * CAMERA.OBJ tail  0x8002951C..0x8002EFE8
 * CLOCK.OBJ        0x8002EFE8..0x8002F2D4
 * COMBAT.OBJ       0x8002F2D4..0x8003104C
 */

#include "types.h"
#include "game/camera.h"
#include "game/worldinfo.h"

typedef struct CameraTarget {
    s32 active;   /* +0x00 */
    u32 object;   /* +0x04: object/group or matrix address */
    f32 x;        /* +0x08 */
    f32 y;        /* +0x0C */
    f32 z;        /* +0x10 */
    u8  _pad14[0x24];
} CameraTarget;

extern CameraTarget gCameraTargets[15];
extern u8 gCameraState[0x34];
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

extern s32 pbLoad;
extern s32 gGameBusy;
extern s32 gGameplayPauseTimer;
extern s32 gModalRenderDepth;
extern s32 gGameMode;
extern s32 gNumEnemies;
extern s32 gBossType;
extern s32 sFlags;
extern s32 gControllerButtons;
extern s32 sPreviousFlags;
extern u8 gPlayers[];
extern u8 gEnemies[];

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

extern s32 pmissile_sfxidx[5];
extern s32 WeapThrowFx[4][5];
extern void* WeapHoldFxTree[4][5];
extern void* FamiliarSpit[4];
extern void* PhoenixTree;
extern void* FamiliarTree[4][2];
extern void* EnemyMissileTree[28][3];
extern MissileTreeInfo PlayerMissileTreeInfo[4];
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
extern f64 sin(f64 angle);
extern f64 cos(f64 angle);
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
extern u8* gCurLevel;
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
extern char lbl_80111B50[];
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
extern s32 lbl_803445D4;
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
extern void damage_player();
extern void AddItem();
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
extern f64 lbl_80346348, lbl_80346340, lbl_803463C8;
extern char lbl_80111E28[];
extern s32 lbl_80274E9C, WeaponStreakTex;
extern u32 lbl_8011A178[], lbl_8011A188[];
extern void* lbl_80282930[];
void fn_80093E50();
void fn_80093D98();

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

void DiffRate_8002951C(s32 camIdx)
{
    f32* camState = (f32*)gCameraState;
    Camera* cam = (Camera*)((u8*)gCameraState + camIdx * 396 + 0xC8);
    f64 prevYaw = (f32)cam->pyr[1];
    f32 rate;
    f64 y;

    camState[6] = camState[5];
    camState[5] = camState[4];
    camState[4] = cam->pyr[1];
    CameraSupervisor(camIdx);
    rate = lbl_8034444C * (f32)(f64)(u32)gFrameTicks;

    if (lbl_80344400 < 1 || cam->pyr[1] == lbl_80344534) {
        if (lbl_80344400 < 0 && cam->pyr[1] != lbl_80344534) {
            cam->pyr[1] = cam->pyr[1] - rate;
            y = (f32)cam->pyr[1];
            if (y <= lbl_80345F58) {
                if (y <= lbl_80345F68) {
                    y = lbl_80345F60 + y;
                }
            } else {
                y = y - lbl_80345F60;
            }
            cam->pyr[1] = (f32)y;
            y = (f32)cam->pyr[1];
            if (y <= prevYaw) {
                if ((f32)((f64)lbl_80344534 - y) < lbl_80345F58 &&
                    y <= (f64)lbl_80344534) {
                    cam->pyr[1] = lbl_80344534;
                    lbl_80344400 = 0;
                }
            } else if ((f64)lbl_80344534 < prevYaw ||
                       y <= (f64)lbl_80344534) {
                cam->pyr[1] = lbl_80344534;
                lbl_80344400 = 0;
            }
        } else {
            cam->pyr[1] = lbl_80344534;
            lbl_80344400 = 0;
        }
    } else {
        cam->pyr[1] = cam->pyr[1] + rate;
        y = (f32)cam->pyr[1];
        if (y <= lbl_80345F58) {
            if (y <= lbl_80345F68) {
                y = lbl_80345F60 + y;
            }
        } else {
            y = y - lbl_80345F60;
        }
        cam->pyr[1] = (f32)y;
        y = (f32)cam->pyr[1];
        if (prevYaw <= y) {
            if ((f32)(y - (f64)lbl_80344534) < lbl_80345F58 &&
                (f64)lbl_80344534 <= y) {
                cam->pyr[1] = lbl_80344534;
                lbl_80344400 = 0;
            }
        } else if (prevYaw < (f64)lbl_80344534 ||
                   (f64)lbl_80344534 <= y) {
            cam->pyr[1] = lbl_80344534;
            lbl_80344400 = 0;
        }
    }
    if ((lbl_80345EC8 < cam->pyr[1] && lbl_80345EC8 < camState[5] &&
         camState[4] < lbl_80345EC8) ||
        (cam->pyr[1] < lbl_80345EC8 && camState[5] < lbl_80345EC8 &&
         lbl_80345EC8 < camState[4])) {
        cam->pyr[1] = lbl_80344534;
        lbl_80344400 = 0;
    }
}

/*
 * CameraSupervisor -- integrate camera position, angle, and attention
 * velocities for one camera.  The GCN implementation also applies collision
 * limits; those limits are represented by the per-axis limit vectors.
 */
extern u8 sTriggerCameras[];
extern f64 lbl_80346030, lbl_803460E8, lbl_803460F8, lbl_803460D8;
extern f64 lbl_80346108, lbl_80346118;
extern f32 lbl_8034445C, lbl_80344454, lbl_80344450, lbl_80344458;
extern f32 lbl_80346100, lbl_80346110, lbl_80344530, lbl_80344408;
extern f64 lbl_80345FE0;
extern s32 lbl_80344510, lbl_8034450C, sNumTriggerCameras, lbl_8034429C, lbl_80344404;

#define TC_X(i) (*(f32*)(sTriggerCameras + (i) * 0x28 + 4))
#define TC_Y(i) (*(f32*)(sTriggerCameras + (i) * 0x28 + 8))
#define TC_Z(i) (*(f32*)(sTriggerCameras + (i) * 0x28 + 0xC))

/*
 * CameraSupervisor -- trigger-camera (rail) selector for camera camIdx.  Finds
 * the two nearest active rail nodes, blends between them along the segment,
 * and drives the target yaw/pitch and their approach rates.
 */
void CameraSupervisor(s32 camIdx)
{
    Camera* cam = &gCameras[camIdx];
    s32 prevBest = lbl_80344510;
    s32 prevSel = lbl_80344508;
    s32 count = 0;
    s32 idx = 0;
    s32 off = 0;
    s32 n = sNumTriggerCameras;
    f64 d11 = lbl_80345EC8;
    f64 d14 = lbl_80345EC8;
    f64 d15 = lbl_80345EC8;
    f64 d17 = lbl_80345EC8;
    f64 d18 = lbl_80346030;
    f64 d19 = lbl_80346030;
    f64 d12, d13, d16;
    f32 local_68, local_64, local_60;
    s32 sel;
    s32 best;

    if (sNumTriggerCameras > 0) {
        do {
            if (sTriggerCameras[off] == 1 &&
                *(s16*)(sTriggerCameras + off + 2) != 0) {
                f32 fy = cam->wpos[1] - *(f32*)(sTriggerCameras + off + 8);
                f32 fx = cam->wpos[0] - *(f32*)(sTriggerCameras + off + 4);
                f32 fz = cam->wpos[2] - *(f32*)(sTriggerCameras + off + 0xC);
                d13 = fz * fz + fx * fx + fy * fy;
                if (lbl_80345EC8 < d13) {
                    d12 = __frsqrte(d13);
                    d12 = lbl_80345F18 * d12 * -(d13 * d12 * d12 - lbl_80345F20);
                    d12 = lbl_80345F18 * d12 * -(d13 * d12 * d12 - lbl_80345F20);
                    d12 = lbl_80345F18 * d12 * -(d13 * d12 * d12 - lbl_80345F20);
                    d13 = (f32)(d13 * lbl_80345F18 * d12 *
                                -(d13 * d12 * d12 - lbl_80345F20));
                }
                if (d19 <= d13) {
                    if (d13 < d18) {
                        count++;
                        d15 = *(f32*)(sTriggerCameras + off + 0x18);
                        d11 = *(f32*)(sTriggerCameras + off + 0x14);
                        d18 = d13;
                        lbl_8034450C = idx;
                    }
                } else {
                    count++;
                    lbl_8034450C = lbl_80344510;
                    d18 = d19;
                    d11 = d14;
                    d14 = *(f32*)(sTriggerCameras + off + 0x14);
                    d15 = d17;
                    d17 = *(f32*)(sTriggerCameras + off + 0x18);
                    d19 = d13;
                    lbl_80344510 = idx;
                }
            }
            idx++;
            off += 0x28;
            n--;
        } while (n != 0);
    }
    best = lbl_80344510;
    if (count == 1) {
        lbl_8034450C = lbl_80344510;
        d18 = d19;
        d11 = d14;
        d15 = d17;
    }
    sel = lbl_8034450C;
    if (count != 0) {
        d18 = (f32)(d19 + d18);
        if (count == 1 || lbl_80345F78 == d18) {
            lbl_8034429C = lbl_8034429C + gFrameTicks;
        } else {
            f32 sx, sy, sz;
            PointLineColl(&cam->wpos[0],
                (f32*)(sTriggerCameras + 4 + lbl_80344510 * 0x28),
                (f32*)(sTriggerCameras + 4 + lbl_8034450C * 0x28),
                &local_68);
            sz = TC_Z(best) - TC_Z(sel);
            sx = TC_X(best) - TC_X(sel);
            sy = TC_Y(best) - TC_Y(sel);
            d16 = sz * sz + sx * sx + sy * sy;
            if (lbl_80345EC8 < d16) {
                d13 = __frsqrte(d16);
                d13 = lbl_80345F18 * d13 * -(d16 * d13 * d13 - lbl_80345F20);
                d13 = lbl_80345F18 * d13 * -(d16 * d13 * d13 - lbl_80345F20);
                d13 = lbl_80345F18 * d13 * -(d16 * d13 * d13 - lbl_80345F20);
                d16 = (f32)(d16 * lbl_80345F18 * d13 *
                            -(d16 * d13 * d13 - lbl_80345F20));
            }
            sz = TC_Z(best) - local_60;
            sx = TC_X(best) - local_68;
            sy = TC_Y(best) - local_64;
            d13 = sz * sz + sx * sx + sy * sy;
            if (lbl_80345EC8 < d13) {
                d12 = __frsqrte(d13);
                d12 = lbl_80345F18 * d12 * -(d13 * d12 * d12 - lbl_80345F20);
                d12 = lbl_80345F18 * d12 * -(d13 * d12 * d12 - lbl_80345F20);
                d12 = lbl_80345F18 * d12 * -(d13 * d12 * d12 - lbl_80345F20);
                d13 = (f32)(d13 * lbl_80345F18 * d12 *
                            -(d13 * d12 * d12 - lbl_80345F20));
            }
            lbl_8034445C = (f32)(d19 / d18);
            d18 = lbl_8034445C;
            if (d18 < lbl_80345F28) {
                if (lbl_80346098 <= d18) {
                    lbl_8034445C = (f32)-(lbl_803460E8 * (d18 - lbl_80345F28) -
                                          lbl_80345FE0);
                }
            } else {
                lbl_8034445C = lbl_80345F80;
            }
            if (lbl_80345F18 < (f32)(d13 / d16)) {
                lbl_80344534 = (f32)d15;
                lbl_80344530 = (f32)d11;
                lbl_80344508 = lbl_8034450C;
                if (d11 <= (f64)lbl_80344408) {
                    lbl_80344404 = -1;
                } else {
                    lbl_80344404 = 1;
                }
            } else {
                lbl_80344534 = (f32)d17;
                lbl_80344530 = (f32)d14;
                lbl_80344508 = lbl_80344510;
                if (d14 <= (f64)lbl_80344408) {
                    lbl_80344404 = -1;
                } else {
                    lbl_80344404 = 1;
                }
            }
            d18 = (f32)(lbl_80344534 - cam->pyr[1]);
            if (lbl_80345F68 <= d18) {
                if (lbl_80345F78 <= d18) {
                    if (lbl_80345F58 <= d18) {
                        lbl_80344400 = -1;
                    } else {
                        lbl_80344400 = 1;
                    }
                } else {
                    lbl_80344400 = -1;
                }
            } else {
                lbl_80344400 = 1;
            }
            if (prevSel != lbl_80344508) {
                f32 ax, ay, az;
                az = cam->wpos[2] - TC_Y(lbl_80344508);
                ax = cam->wpos[0] - TC_X(lbl_80344508);
                ay = cam->wpos[1] - TC_Z(lbl_80344508);
                d18 = az * az + ax * ax + ay * ay;
                if (lbl_80345EC8 < d18) {
                    d11 = __frsqrte(d18);
                    d11 = lbl_80345F18 * d11 * -(d18 * d11 * d11 - lbl_80345F20);
                    d11 = lbl_80345F18 * d11 * -(d18 * d11 * d11 - lbl_80345F20);
                    d11 = lbl_80345F18 * d11 * -(d18 * d11 * d11 - lbl_80345F20);
                    d18 = (f32)(d18 * lbl_80345F18 * d11 *
                                -(d18 * d11 * d11 - lbl_80345F20));
                }
                d18 = (f32)(d18 * lbl_803460F0);
                if (lbl_80345F78 == d18) {
                    lbl_8034444C = lbl_80345EC8;
                    lbl_80344454 = lbl_80345EC8;
                    lbl_80344450 = lbl_80345EC8;
                    lbl_80344458 = lbl_80345EC8;
                } else {
                    f32 av;
                    if (d18 < lbl_80345FE0) {
                        d18 = lbl_80345F80;
                    }
                    d11 = (f32)(lbl_80344534 - cam->pyr[1]);
                    if (lbl_80345F58 < d11) {
                        d11 = (f32)(lbl_80345F60 - d11);
                    }
                    av = (f32)(d11 / d18);
                    if (av < 0.0f) {
                        av = -av;
                    }
                    lbl_8034444C = av;
                    if (lbl_803460F8 <= lbl_8034444C) {
                        lbl_8034444C = lbl_80346100;
                    }
                    d11 = lbl_80344454;
                    av = (f32)((f64)lbl_8034444C - d11);
                    if (av < 0.0f) {
                        av = -av;
                    }
                    if (lbl_803460D8 <= av) {
                        if (lbl_8034444C <= d11) {
                            lbl_8034444C = (f32)(d11 - lbl_803460D8);
                        } else {
                            lbl_8034444C = (f32)(lbl_803460D8 + d11);
                        }
                    }
                    av = (f32)((f64)(lbl_80344530 - lbl_80344408) / d18);
                    if (av < 0.0f) {
                        av = -av;
                    }
                    lbl_80344450 = av;
                    if (lbl_80346108 <= lbl_80344450) {
                        lbl_80344450 = lbl_80346110;
                    }
                    d18 = lbl_80344458;
                    av = (f32)((f64)lbl_80344450 - d18);
                    if (av < 0.0f) {
                        av = -av;
                    }
                    if (lbl_80346118 <= av) {
                        if (lbl_80344450 <= d18) {
                            lbl_80344450 = (f32)(d18 - lbl_80346118);
                        } else {
                            lbl_80344450 = (f32)(lbl_80346118 + d18);
                        }
                    }
                    lbl_80344454 = lbl_8034444C;
                    lbl_80344458 = lbl_80344450;
                }
            }
        }
    }
    if (prevBest >= 0 && prevBest == lbl_8034450C) {
        *(s16*)(sTriggerCameras + prevBest * 0x28 + 2) = 0;
    }
}

/* Orient a camera around its attention point at the requested radius. */
/* 0x80029E8C - orient the transmitter camera (cam 3): spin its yaw, clamp the
 * radius, snap the look-at to camera 0's, then rebuild its world position. */
void cam_orient_to_80029E8C(s32 camIdx)
{
    Camera* cam = (Camera*)((u8*)gCameraState + camIdx * 396 + 0xC8);
    f32 vec[3];
    f32 out[3];
    f32 mat[16];

    if (lbl_803447B8 != 0 || lbl_803447B4 != 0 || gNumTransmitters == 0 ||
        camIdx != 3) {
        return;
    }

    cam->pyr[1] = (f32)(cam->pyr[1] + lbl_80346128);
    {
        f32 yaw = cam->pyr[1];
        if (yaw > lbl_80345F58) {
            yaw = (f32)(yaw - lbl_80345F60);
        } else if (yaw <= lbl_80345F68) {
            yaw = (f32)(lbl_80345F60 + yaw);
        }
        cam->pyr[1] = yaw;
    }

    cam->radius = (f32)(cam->radius / lbl_80346130);
    if (cam->radius < lbl_803460D0) {
        cam->radius = lbl_80346018;
    } else if (cam->radius > lbl_80345FF0) {
        cam->radius = lbl_80346020;
    }

    cam->vel[0] = lbl_80345EC8;
    cam->vel[1] = lbl_80345EC8;
    cam->vel[2] = lbl_80345EC8;
    cam->avel[0] = lbl_80345EC8;
    cam->avel[1] = lbl_80345EC8;
    cam->avel[2] = lbl_80345EC8;
    cam->attn[0] = gCameras[0].attn[0];
    cam->attn[1] = gCameras[0].attn[1];
    cam->attn[2] = gCameras[0].attn[2];

    CreateYPRMatrix(mat, cam->pyr);
    vec[0] = lbl_80345EC8;
    vec[1] = lbl_80345EC8;
    vec[2] = cam->radius;
    WorldVector(vec, out, mat);
    cam->wpos[0] = cam->attn[0] + out[0];
    cam->wpos[1] = cam->attn[1] + out[1];
    cam->wpos[2] = cam->attn[2] + out[2];
    lbl_8034453C = 0;
}

/* Walk-mode camera completion/cleanup predicate. */
s32 MoveCam_walk_8002A024(s32 camIdx)
{
    Camera* cam = &gCameras[camIdx];
    s32 done;

    if (lbl_803444F0 == 1) {
        u8* p = gPlayers;
        s32 i;
        done = 1;
        for (i = 0; i < 4; i++, p += 13148) {
            if (*(s32*)(p + 232) == 1 && *(s32*)(p + 516) != 1) {
                done = 0;
            }
        }
    } else {
        done = 1;
    }
    if (done != 0) {
        s32 oldMode = cam->a_mode;
        lbl_803447B8 = 0;
        lbl_803444F0 = -1;
        lbl_803444EC = -1;
        gScriptedCameraState = 0;
        lbl_8034453C = 0;
        if (cam->c_mode != 0) {
            cam->pc_mode = cam->c_mode;
            cam->c_mode = CAM_OFF;
        }
        if (cam->a_mode != oldMode) {
            cam->pa_mode = cam->a_mode;
            cam->a_mode = oldMode;
        }
        cam->state = 0;
        if ((((gControllerButtons & cam->state) ^ cam->state) |
             ((sFlags & 4) ^ cam->state)) != 0) {
            lbl_803445D4 |= 4;
            sPreviousFlags = sPreviousFlags;
        }
    }
    return done == 0;
}

/* Initialize or advance the game camera's scripted transition. */
extern u8 lbl_80240E38[];
extern f32 lbl_80346138, lbl_80346148;
extern f64 lbl_80345FE0, lbl_80346140;
extern s32 lbl_8023FCD4, lbl_8023FCD8, sFlags, lbl_8023FBE8;
extern s32 lbl_8023FCE0, lbl_8023FCE4;
extern s32 gScriptedCameraState;
extern u32 gFrameTicks;
void write_stage_info(s32 mode);

/*
 * init_game_cam -- game-camera (index 2) zoom/transition driver.  Steps the
 * game camera's world position and attention toward camera 0's, each capped
 * per frame; when both converge it fires the level transition.  Returns 0 on
 * transition, -1 otherwise.
 */
s32 init_game_cam(s32 camIdx)
{
    Camera* cam = (Camera*)((u8*)gCameraState + camIdx * 396 + 0xC8);
    s32 prevTimer = gScriptedCameraState;
    s32 reached = 2;
    u8* level = *(u8**)((u8*)gCurLevel + 0x60);
    f32 dx, dy, dz;
    f32 len;
    f64 g;
    s32 i;

    if (camIdx != 2) {
        return -1;
    }

    if (gScriptedCameraState > 2) {
        gScriptedCameraState = gScriptedCameraState - gFrameTicks;
        if (gScriptedCameraState < 2) {
            gScriptedCameraState = 2;
        }
        if (gScriptedCameraState < 45) {
            for (i = 0; i < 4; i++) {
                u8* player = (u8*)gPlayers + i * PLAYER_STRIDE;
                if (PF(player, 0xE8, s32) == 1 &&
                    (*(u32*)(lbl_80240E38 + i * 0x3C) & 0x20000FF) != 0) {
                    gScriptedCameraState = 2;
                }
            }
        }
    }
    lbl_80344490 = gScriptedCameraState;
    write_stage_info(gScriptedCameraState);

    if (prevTimer > 1 && gScriptedCameraState == 1) {
        for (i = 0; i < 4; i++) {
            u8* player = (u8*)gPlayers + i * PLAYER_STRIDE;
            if (PF(player, 0xE8, s32) == 1) {
                PF(player, 0x91C, s32) = 4;
            }
        }
    }

    if (gScriptedCameraState == 1) {
        dx = gCameras[0].wpos[0] - cam[0].wpos[0];
        dy = gCameras[0].wpos[1] - cam[0].wpos[1];
        dz = gCameras[0].wpos[2] - cam[0].wpos[2];
        len = (f32)(dz * dz + (f32)(dx * dx + (f32)(dy * dy)));
        if (len > lbl_80345EC8) {
            g = __frsqrte((f64)len);
            g = lbl_80345F18 * g * -((f64)len * g * g - lbl_80345F20);
            g = lbl_80345F18 * g * -((f64)len * g * g - lbl_80345F20);
            g = lbl_80345F18 * g * -((f64)len * g * g - lbl_80345F20);
            len = (f32)((f64)len * lbl_80345F18 * g *
                        -((f64)len * g * g - lbl_80345F20));
        }
        if (len < lbl_80345F28) {
            reached = 1;
        } else {
            if (len > lbl_80346138) {
                len = lbl_80346138;
            }
            len = (f32)((f64)gFrameTicks / len);
            if (len > lbl_80345FE0) {
                len = lbl_80345F80;
            }
            dx = dx * len;
            dy = dy * len;
            dz = dz * len;
        }
        cam[0].wpos[0] = cam[0].wpos[0] + dx;
        cam[0].wpos[1] = cam[0].wpos[1] + dy;
        cam[0].wpos[2] = cam[0].wpos[2] + dz;

        dx = gCameras[0].attn[0] - cam[0].attn[0];
        dy = gCameras[0].attn[1] - cam[0].attn[1];
        dz = gCameras[0].attn[2] - cam[0].attn[2];
        len = (f32)(dz * dz + (f32)(dx * dx + (f32)(dy * dy)));
        if (len > lbl_80345EC8) {
            g = __frsqrte((f64)len);
            g = lbl_80345F18 * g * -((f64)len * g * g - lbl_80345F20);
            g = lbl_80345F18 * g * -((f64)len * g * g - lbl_80345F20);
            g = lbl_80345F18 * g * -((f64)len * g * g - lbl_80345F20);
            len = (f32)((f64)len * lbl_80345F18 * g *
                        -((f64)len * g * g - lbl_80345F20));
        }
        if (len < lbl_80345F28) {
            reached = reached - 1;
        } else {
            if (len > lbl_80346140) {
                len = lbl_80346148;
            }
            len = (f32)((f64)gFrameTicks / len);
            if (len > lbl_80345FE0) {
                len = lbl_80345F80;
            }
            dx = dx * len;
            dy = dy * len;
            dz = dz * len;
        }
        cam[0].attn[0] = cam[0].attn[0] + dx;
        cam[0].attn[1] = cam[0].attn[1] + dy;
        cam[0].attn[2] = cam[0].attn[2] + dz;

        if (reached < 1) {
            if (lbl_8034440C != 0) {
                MBRemoveBlit((s32)lbl_8034440C);
                lbl_8034440C = 0;
            }
            lbl_803444F0 = *(s8*)(level + 0x25);
            if (lbl_803444F0 < 0) {
                if (lbl_8023FCD4 != 0) {
                    lbl_8023FCD8 = lbl_8023FCD4;
                    lbl_8023FCD4 = 0;
                }
                if ((sFlags & 4) != 0) {
                    lbl_803445D4 = lbl_803445D4 | 4;
                }
                lbl_8034453C = 0;
                lbl_803447B8 = 0;
                gScriptedCameraState = 0;
                lbl_8023FBE8 = 0;
                return 0;
            }
            lbl_803444EC = 0;
            lbl_803447B8 = 2;
            if (lbl_8023FCD4 != 2) {
                lbl_8023FCD8 = lbl_8023FCD4;
                lbl_8023FCD4 = 2;
            }
            if (lbl_8023FCE0 != 2) {
                lbl_8023FCE4 = lbl_8023FCE0;
                lbl_8023FCE0 = 2;
            }
            lbl_8034453C = 0;
        }
    }
    return -1;
}

/* Stage-information overlay state.  The original drives its text/blit UI;
 * this translation preserves the timer and ownership transitions. */
void write_stage_info(s32 mode)
{
    f32 prev = lbl_80344410;
    u32 level;

    lbl_80344410 = (f32)(lbl_80346150 * (f64)(u32)gFrameTicks + prev);
    if (mode <= 1 || lbl_80344410 > lbl_80345F40) {
        lbl_80344410 = lbl_80346158;
    }
    if (mode <= 0) {
        if (lbl_8034440C != NULL) {
            MBRemoveBlit((s32)lbl_8034440C);
            lbl_8034440C = NULL;
        }
        return;
    }

    {
        f64 scale;
        f32 slide;

        slide = lbl_80344410;
        scale = lbl_80346160;

        DrawTextKeepScale(-256, 48 - (s32)(scale * slide), 6,
                          0xFFFFFF, (char*)gCurLevel + 20);
    }
    level = *(u32*)gCurLevel;
    if ((level & 1) != 0) {
        DrawStringText(-256, 4204, -1, 0x160C03, 175, 0);
        if (prev != lbl_80344410 && lbl_80345F40 == lbl_80344410) {
            fn_8009D300();
        }
    } else if ((level & 4) != 0) {
        DrawStringText(-256, 4204, -1, 0x160C03, 176, 0);
        if (prev != lbl_80344410 && lbl_80345F40 == lbl_80344410) {
            fn_8009FAB4();
        }
    } else if (lbl_80344498 != 0 && sMusicTrackLo == 0) {
        DrawStringText(-256, 4204, -1, 0x160C03, 177, 0);
        if (prev != lbl_80344410 && lbl_80345F40 == lbl_80344410) {
            fn_8009D2B4();
        }
    }
}

void init_stage_info(void)
{
    s32 width = 0;
    s32 height = 0;
    s32 x;
    s32 y;
    u8 unused[8];
    u32 level;

    lbl_80344490 = 91;
    lbl_80344410 = lbl_80346168;
    level = *(u32*)gCurLevel;
    if ((level & 1) != 0) {
        width = StringTextWidth(175, -1, lbl_80345F80);
        height = StringTextHeight(175, -1, 0, lbl_80345F80);
    } else if ((level & 4) != 0) {
        width = StringTextWidth(176, -1, lbl_80345F80);
        height = StringTextHeight(176, -1, 0, lbl_80345F80);
    } else if (sMusicTrackLo == 0) {
        lbl_80344498 = 0;
    }
    if (width > 0) {
        width += 60;
        height += 16;
        x = 256 - width / 2;
        y = 108 - height / 2;
        lbl_8034440C = MBNewBlit(lbl_80111B50, x, y);
        mbBlitProject(lbl_8034440C, width, height);
    }
}

void AverageCameraTargetPosition_8002A890(f32* out)
{
    f32* q;
    s32 n = gCameraTargetPositionCount;
    f32 sum[3];
    s32 i;
    s32 k;
    s32 off;
    f32 scale;

    if (n > 0) {
        sum[0] = lbl_80345EC8;
        sum[1] = lbl_80345EC8;
        sum[2] = lbl_80345EC8;
        i = 0;
        off = 0;
        for (; i < n; i++, off += 3) {
            q = gCameraTargetPositions + off;
            for (k = 0; k < 3; k++) {
                sum[k] += q[k];
            }
        }
        scale = (f32)(1.0 / (f64)n);
        sum[0] = sum[0] * scale;
        sum[1] = sum[1] * scale;
        sum[2] = sum[2] * scale;
        out[0] = sum[0];
        out[1] = sum[1];
        out[2] = sum[2];
    }
}

void chg_target_state(s32 mode)
{
    gCameraTargetMode = mode;
    gCameraTargetPositionCount = 0;
}

/* calc_cam_pyr_8002A97C: derive camera pitch/yaw for the active look mode. */
extern s32 lbl_8028CA90, gNumTransmitters, lbl_80344538, gScriptedCameraState;
extern f32 lbl_8034616C, lbl_80344530, lbl_80344408, lbl_80344534;
extern f32 lbl_8028CABC, lbl_8028CAC8, lbl_8028CAD0, lbl_8028CAC4;
extern f32 lbl_80118B60[];
extern f64 lbl_80346170, lbl_80346070, lbl_80345EF0, lbl_80346178;
extern u32 gFrameTicks;

void calc_cam_pyr_8002A97C(s32 camIdx, s32 resetDelta)
{
    Camera* cam = &gCameras[camIdx];
    f64 dv;
    f64 dv3;
    f32 v;
    f32 zero = lbl_80345EC8;

    cam->pyr[2] = zero;
    if (resetDelta != 0) {
        cam->pyr_delta[0] = zero;
        cam->pyr_delta[1] = zero;
        cam->pyr_delta[2] = zero;
    }
    if (gWorldInfo.wobjs == 0) {
        cam->pyr[0] = lbl_8034616C;
        cam->pyr[1] = zero;
        cam->pyr[2] = zero;
        goto apply;
    }
    if (gNumTransmitters != 0) {
        f32 rate = (f32)(lbl_80346170 * (f64)(u32)gFrameTicks);
        f32 diff = lbl_80344530 - lbl_80344408;
        f32 step;
        if (diff < 0.0f) {
            diff = -diff;
        }
        step = (f32)(lbl_80346070 * (f64)diff);
        if (step < rate) {
            step = rate;
        }
        if (lbl_80344530 <= lbl_80344408) {
            lbl_80344408 = lbl_80344408 - step;
            if (lbl_80344408 <= lbl_80344530) {
                lbl_80344408 = lbl_80344530;
            }
        } else {
            lbl_80344408 = lbl_80344408 + step;
            if (lbl_80344530 <= lbl_80344408) {
                lbl_80344408 = lbl_80344530;
            }
        }
    }
    cam->pyr[0] = lbl_80344408;
    if (gNumTransmitters != 0) {
        goto apply;
    }

    dv3 = lbl_8028CABC;
    if (lbl_80344538 == 2) {
        dv = lbl_8028CAC8 - cam->attn[0];
    } else if (lbl_80344538 < 2) {
        if (lbl_80344538 <= 0) {
            dv = cam->attn[0] - lbl_8028CAC8;
        } else {
            dv = lbl_8028CAD0 - cam->attn[2];
            dv3 = lbl_8028CAC4;
        }
    } else if (lbl_80344538 > 3) {
        dv = cam->attn[0] - lbl_8028CAC8;
    } else {
        dv = cam->attn[2] - lbl_8028CAD0;
        dv3 = lbl_8028CAC4;
    }
    v = lbl_80344534;
    if (gScriptedCameraState == 0) {
        v = lbl_80118B60[lbl_80344538];
    }
    cam->pyr[1] = FixAngle((f32)((f64)v + dv / (lbl_80345EF0 * dv3)));

apply:
    if (gNumTransmitters == 0) {
        if (lbl_80346178 <=
            (f32)((f64)cam->pyr[0] + (f64)cam->pyr_delta[0])) {
            cam->pyr[0] = (f32)(lbl_80346178 - (f64)cam->pyr_delta[0]);
        }
        cam->pyr[0] = cam->pyr[0] + cam->pyr_delta[0];
        cam->pyr[1] = cam->pyr[1] + cam->pyr_delta[1];
        cam->pyr[2] = cam->pyr[2] + cam->pyr_delta[2];
    }
}

/*
 * get_cam_wpos_8002ABE0 -- derive a collision-safe world position for the camera.
 * The target performs several world traces; retaining the radial placement
 * and last-good-position fallback makes this usable by a native port even
 * before the world-collision adapter is available.
 */
extern s32 lbl_803443F8;
extern f64 lbl_80346180;
extern f32 lbl_80346188;
s32 CameraCollide(f32* pos, f32* obj);

/* Place the camera radially behind its attention point, rotating yaw to a
 * clear angle and lifting pitch until no tracked target blocks the view. */
static void place_cam(Camera* cam, f32* wpos, f32* attn, f32* pyr, f32* mat)
{
    f32 in[3];
    f32 out[3];
    CreateYPRMatrix(mat, pyr);
    in[0] = lbl_80345EC8;
    in[1] = lbl_80345EC8;
    in[2] = cam->radius;
    WorldVector(in, out, mat);
    wpos[0] = attn[0] + out[0];
    wpos[1] = attn[1] + out[1];
    wpos[2] = attn[2] + out[2];
}

static s32 cam_blocked(f32* wpos)
{
    s32 j;
    for (j = 0; j < 15; j++) {
        CameraTarget* t = &gCameraTargets[j];
        if (t->active > 0 && CameraCollide(wpos, (f32*)(t->object + 0x40))) {
            return 1;
        }
    }
    return 0;
}

void get_cam_wpos_8002ABE0(s32 camIdx)
{
    s32* camState = (s32*)gCameraState;
    Camera* cam = (Camera*)((u8*)gCameraState + camIdx * 396 + 0xC8);
    f32* wpos = (f32*)((u8*)cam + 0x64);
    f32* attn = (f32*)((u8*)cam + 0x12C);
    f32* pyr = (f32*)((u8*)cam + 0xA4);
    f32* pyrDelta = (f32*)((u8*)cam + 0xB4);
    s32* timer = (s32*)((u8*)cam + 0xCC);
    f32 mat[18];
    s32 i;

    *(f32*)((u8*)cam + 0x74) = wpos[0];
    *(f32*)((u8*)cam + 0x78) = wpos[1];
    *(f32*)((u8*)cam + 0x7C) = wpos[2];

    if (gNumTransmitters == 0 && lbl_803443F8 <= 0) {
        s32 mode = lbl_80344538;
        for (i = 0; i < 4; i++) {
            camState[i] = 0;
        }
        for (i = 0; i < 4; i++) {
            f32 y;
            place_cam(cam, wpos, attn, pyr, mat);
            camState[mode] = cam_blocked(wpos);
            y = (f32)((f64)pyr[1] + lbl_80346180);
            if ((f64)y <= lbl_80345F58) {
                if ((f64)y <= lbl_80345F68) y = (f32)(lbl_80345F60 + (f64)y);
            } else {
                y = (f32)((f64)y - lbl_80345F60);
            }
            mode = mode & 3;
            pyr[1] = y;
        }
        if (camState[lbl_80344538] != 0) {
            s32 adj = (lbl_80344538 - 1) & 3;
            s32 found = 0;
            for (i = 4; i != 0; i--) {
                if (adj != lbl_80344538 && camState[adj] == 0) {
                    found = 1;
                    break;
                }
            }
            if (found) {
                s32 delta = adj - lbl_80344538;
                gScriptedCameraState = 1;
                lbl_80344400 = (delta == 1 || delta == -3) ? 1 : -1;
                lbl_80344534 = lbl_80118B60[lbl_80344538];
                lbl_80344538 = (lbl_80344538 + lbl_80344400) & 3;
                lbl_803443F8 = (delta == 2 || delta == -2) ? 0 : 0x168;
            }
        }
    }

    place_cam(cam, wpos, attn, pyr, mat);

    if (gNumTransmitters == 0) {
        if (cam_blocked(wpos)) {
            *timer = *timer + gFrameTicks;
            if (*timer > 0xB4) {
                *timer = 0xB4;
            }
            if (lbl_80344404 < 1) {
                if (lbl_80346188 <= pyr[0]) {
                    *pyrDelta = (f32)((f64)*pyrDelta - lbl_80346188);
                    pyr[0] = (f32)((f64)pyr[0] - lbl_80346188);
                    place_cam(cam, wpos, attn, pyr, mat);
                }
            } else {
                if ((f64)pyr[0] <= lbl_80346178 - lbl_80346188) {
                    *pyrDelta = (f32)((f64)*pyrDelta + lbl_80346188);
                    pyr[0] = (f32)((f64)pyr[0] + lbl_80346188);
                    place_cam(cam, wpos, attn, pyr, mat);
                }
            }
        } else if (*timer >= 0) {
            f32 savedW0 = wpos[0], savedW1 = wpos[1], savedW2 = wpos[2];
            f32 savedD = *pyrDelta;
            *timer = *timer - gFrameTicks;
            if (*timer < 0) {
                if (lbl_80344404 < 1) {
                    if (lbl_80345F78 <= (f64)*pyrDelta) {
                        *pyrDelta = lbl_80345EC8;
                    } else {
                        *pyrDelta = (f32)((f64)*pyrDelta + lbl_80346188);
                        if (lbl_80345F78 < (f64)*pyrDelta) {
                            *pyrDelta = lbl_80345EC8;
                        }
                        pyr[0] = (f32)((f64)pyr[0] + lbl_80346188);
                        place_cam(cam, wpos, attn, pyr, mat);
                        if (cam_blocked(wpos)) {
                            *pyrDelta = savedD;
                            wpos[0] = savedW0;
                            wpos[1] = savedW1;
                            wpos[2] = savedW2;
                        }
                    }
                } else {
                    if ((f64)*pyrDelta <= lbl_80345F78) {
                        *pyrDelta = lbl_80345EC8;
                    } else {
                        *pyrDelta = (f32)((f64)*pyrDelta - lbl_80346188);
                        if ((f64)*pyrDelta < lbl_80345F78) {
                            *pyrDelta = lbl_80345EC8;
                        }
                        pyr[0] = (f32)((f64)pyr[0] - lbl_80346188);
                        place_cam(cam, wpos, attn, pyr, mat);
                        if (cam_blocked(wpos)) {
                            *pyrDelta = savedD;
                            wpos[0] = savedW0;
                            wpos[1] = savedW1;
                            wpos[2] = savedW2;
                        }
                    }
                }
            }
        }
    }
}

f32 get_cam_dist(s32 camIdx)
{
    Camera* cam = &gCameras[camIdx];
    f64 base;
    f64 minDist;
    f64 farBase;
    f64 farDist;
    f64 result;

    base = lbl_80345F78;
    if (gBossType >= 0) {
        base = lbl_80345F28;
    }
    base = (f32)base;
    minDist = (f32)(lbl_80346190 + base);
    if (lbl_80344960 >= 0) {
        minDist = (f32)(minDist + lbl_80345F28);
    }
    farBase = lbl_80345F90 + base;
    farDist = (f32)farBase;
    if (lbl_80344960 >= 0) {
        farDist = (f32)(farDist + lbl_80345F28);
    }

    if (sMusicTrackHi < 0 || gCameraTargetCount == 0) {
        result = lbl_80345EC8;
    } else if (gCameraTargetCount == 1) {
        lbl_803444E8 = lbl_80346198;
        result = lbl_8034452C;
    } else {
        s32 i;
        f32 min24 = lbl_8034619C, max24 = lbl_803461A0;
        f32 min36 = lbl_8034619C, max28 = lbl_803461A0;
        f32 ratio, xr, yr, nearScale, farScale;
        f64 radius = (f32)cam->radius;

        for (i = 0; i < 15; i++) {
            CameraTarget* t = &gCameraTargets[i];
            if (t->active > 0) {
                f32 v24 = *(f32*)((u8*)t + 24);
                if (v24 < min24) min24 = v24;
                if (max24 < v24) max24 = v24;
                if (*(f32*)((u8*)t + 36) < min36) min36 = *(f32*)((u8*)t + 36);
                if (max28 < *(f32*)((u8*)t + 28)) max28 = *(f32*)((u8*)t + 28);
            }
        }
        xr = (max24 - min24) /
             (f32)((lbl_8034451C - 30) - (lbl_80344520 + 30));
        yr = (max28 - min36) /
             (f32)((lbl_80344518 - 20) - (lbl_80344514 + 40));
        ratio = xr;
        if (xr < yr) ratio = yr;

        nearScale = lbl_803461B0;
        if (lbl_803461A8 <= ratio) {
            nearScale = lbl_803461B4;
            if (ratio < lbl_80345F18 + base) {
                nearScale = (f32)(lbl_803461B8 +
                    ((lbl_80345F18 + base) - ratio) / lbl_803460F0);
            }
        }
        if (ratio <= lbl_803461C0) {
            farScale = lbl_803461D0;
            if (farBase < ratio) {
                farScale = (f32)(lbl_803461C8 -
                    (ratio - farBase) / lbl_80346018);
            }
        } else {
            f64 t2 = lbl_803461C8;
            if (lbl_803444E4 != 0) {
                t2 = lbl_80345F20;
            }
            farScale = (f32)t2;
        }
        lbl_803444E8 = ratio;

        if (lbl_803444E4 != 0 && lbl_803443FC >= 0) {
            lbl_80344418 = 0;
        }
        if (lbl_80344418 == 0 || lbl_803443FC >= 0) {
            if (lbl_803443FC == 0) {
                if (ratio < lbl_80345F18 + base && lbl_80344418 == 0) {
                    lbl_803443FC = -1;
                    radius = cam->radius * nearScale;
                }
                if (farDist < ratio ||
                    (lbl_803444F4 == 0 &&
                     ((lbl_80344960 < 0 && cam->radius < lbl_80344528) ||
                      (lbl_80344960 >= 0 &&
                       (f64)cam->radius < lbl_80345FF0)))) {
                    lbl_803443FC = 1;
                    radius = cam->radius * farScale;
                }
            } else if ((lbl_803443FC < 0 &&
                        lbl_80346190 + base <= ratio) ||
                       (lbl_803443FC > 0 && ratio <= minDist)) {
                lbl_803443FC = 0;
            } else if (lbl_803443FC < 1) {
                radius = cam->radius * nearScale;
            } else {
                radius = cam->radius * farScale;
            }
        } else {
            lbl_803443FC = 0;
        }

        result = lbl_8034452C;
        if (result <= radius) {
            result = radius;
            if (lbl_80344960 < 0) {
                if (lbl_80344528 < radius && lbl_803444E4 == 0) {
                    result = (f32)(radius -
                        lbl_80345F88 * (f32)(radius - lbl_80344528));
                }
            } else if (lbl_80345FF0 < radius) {
                result = (f32)(lbl_80345FF8 *
                    (lbl_80345FF0 - (f32)cam->radius) + (f32)cam->radius);
            }
        }
    }
    return (f32)result;
}

s32 adjust_radius_8002B2D4(s32 camIdx)
{
    Camera* cam = &gCameras[camIdx];
    f32 desired = get_cam_dist(camIdx);
    void* levelData = *(void**)((u8*)gCurLevel + 96);
    f32 val;
    f32 diff;
    f32 ad;
    f32 step;

    if (lbl_80345F78 == desired) {
        return 0;
    }
    val = desired;
    if (desired < cam->radius && lbl_803443FC > 0) {
        lbl_803443FC = 0;
        val = cam->radius;
    }
    if (val > cam->radius && lbl_803443FC < 0) {
        lbl_803443FC = 0;
        val = cam->radius;
    }
    if (lbl_80344960 < 0 && val > cam->radius && cam->radius > lbl_80344528) {
        val = cam->radius;
    }

    diff = val - cam->radius;
    ad = diff;
    *(u32*)&ad &= 0x7FFFFFFF;
    if (ad < lbl_8034618C) {
        cam->radius = val;
        lbl_803443F4 = 1;
        return -1;
    }
    step = (f32)(lbl_80346098 * (ad - lbl_8034618C) + lbl_8034618C);
    if (step > lbl_80346158 && lbl_803444E4 == 0) {
        step = lbl_80346158;
    }
    if (val > cam->radius) {
        cam->radius = cam->radius + step;
        lbl_803443F4 = 1;
        return -1;
    }
    if (val < cam->radius) {
        if (lbl_80344418 == 0 || *(s16*)((u8*)levelData + 54) != 0) {
            cam->radius = cam->radius - step;
            lbl_803443F4 = 1;
        }
    }
    return -1;
}

/*
 * someone_will_be_off_screen -- project active target positions into the
 * current camera and report whether their normalized extents exceed a margin.
 * The GCN renderer's integer viewport conversion is folded into the matrix
 * multiply here.
 */
extern f32 lbl_803461D4, lbl_803461D8;
void MBWindowProject();
s32 MBScreenHeight(void);
s32 MBScreenWidth(void);

/*
 * someone_will_be_off_screen -- temporarily place camera camIdx at pos, project
 * every active target into the window, and return the largest normalized screen
 * offset from center (a value > 1 means a target falls outside the frame).
 */
f32 someone_will_be_off_screen(s32 camIdx, f32* pos)
{
    Camera* cam = &gCameras[camIdx];
    f32* eye = (f32*)((u8*)cam + 0x34);
    f32 savedX = eye[0];
    f32 savedY = eye[1];
    f32 savedZ = eye[2];
    f32 minX = lbl_803461D4, maxX = lbl_803461D8;
    f32 minY = lbl_803461D4, maxY = lbl_803461D8;
    s32 scrH = MBScreenHeight();
    s32 scrW = MBScreenWidth();
    s32 i;
    f32 cx, cy, hd, vd, hd2, vd2, rx, ry;

    eye[0] = pos[0];
    eye[1] = pos[1];
    eye[2] = pos[2];
    for (i = 0; i < 15; i++) {
        CameraTarget* t = &gCameraTargets[i];
        if (t->active != 0) {
            s16 sp[2];
            f32 sx, sy;
            MBWindowProject((f32*)(t->object + 0x40),
                            (f32*)((u8*)cam + 4), 0, sp);
            sx = (f32)sp[0];
            sy = (f32)sp[1];
            if (sx < minX) minX = sx;
            if (maxX < sx) maxX = sx;
            if (sy < minY) minY = sy;
            if (maxY < sy) maxY = sy;
            MBWindowProject((f32*)(t->object + 0x30),
                            (f32*)((u8*)cam + 4), 0, sp);
            sx = (f32)sp[0];
            sy = (f32)sp[1];
            if (sx < minX) minX = sx;
            if (maxX < sx) maxX = sx;
            if (sy < minY) minY = sy;
            if (maxY < sy) maxY = sy;
        }
    }
    eye[0] = savedX;
    eye[1] = savedY;
    eye[2] = savedZ;

    cx = (f32)(scrW / 2);
    cy = (f32)(scrH - (scrH - 0x40) / 2);
    hd = minX - cx;
    if (hd < 0.0f) hd = -hd;
    hd2 = maxX - cx;
    if (hd2 < 0.0f) hd2 = -hd2;
    if (hd2 < hd) hd2 = hd;
    rx = hd2 / (f32)(scrW / 2);
    vd = minY - cy;
    if (vd < 0.0f) vd = -vd;
    vd2 = maxY - cy;
    if (vd2 < 0.0f) vd2 = -vd2;
    if (vd2 < vd) vd2 = vd;
    ry = vd2 / (f32)((scrH - 0x40) / 2);
    return rx < ry ? ry : rx;
}

/*
 * StandardCamera_8002B828 -- multiplayer framing loop.  It updates the focus point,
 * radius and radial camera position, then derives the camera orientation.
 */
extern s32 lbl_803444DC, lbl_803444CC, lbl_803444C8, lbl_80344500;
extern f32 lbl_803444D8, lbl_803444D4, lbl_803444D0;
extern f32 lbl_80346188, lbl_803461E8;
extern f64 lbl_803461E0, lbl_80346180;
f32 NormalVector(f32* v);

/*
 * StandardCamera_8002B828 -- game camera (camera 0) auto-pan.  When the tracked targets
 * drift toward the screen edges it eases pan angles in/out, applies them around
 * the look axis, then keeps whichever of the two candidate framings leaves the
 * fewest targets off screen.
 */
void StandardCamera_8002B828(s32 camIdx)
{
    Camera* c0 = &gCameras[0];
    f32* wpos0 = (f32*)((u8*)c0 + 0x64);
    f32* attn0 = (f32*)((u8*)c0 + 0x12C);
    s32 wasPanning = lbl_803444DC;
    s32 scrH = MBScreenHeight();
    s32 scrW = MBScreenWidth();
    f32 minX = lbl_803461D4, maxX = lbl_803461D8;
    f32 maxY = lbl_803461D8, minY = lbl_803461D4;
    f32 errX = lbl_80345EC8;
    f32 errY = lbl_80345EC8;
    f32 offX = lbl_80345EC8;
    f32 offZ = lbl_80345EC8;
    f32 yaw;
    s32 i;

    if (gCurLevel == 0 || camIdx != 0 || lbl_8034453C != 0 ||
        *(s32*)((u8*)c0 + 0xEC) != 3 || gBossType >= 0 || gBossType == 0x22) {
        return;
    }

    if (lbl_80345F78 == (f64)lbl_803444D8 && lbl_80345F78 == (f64)lbl_803444D4) {
        lbl_803444DC = 0;
        lbl_803444D0 = 0;
        lbl_80344528 = *(f32*)(*(s32*)((u8*)gCurLevel + 0x60) + 0x30);
    } else {
        lbl_803444DC = 1;
        lbl_803444D0 = lbl_803444D0 + gFrameTicks;
    }
    if (gCameraTargetCount > 0) {
        for (i = 0; i < 15; i++) {
            CameraTarget* t = &gCameraTargets[i];
            if (t->active != 0) {
                f32 tx = *(f32*)((u8*)t + 0x28);
                if (tx < minX) minX = tx;
                if (maxX < tx) maxX = tx;
                if (maxY < *(f32*)((u8*)t + 0x2C)) {
                    maxY = *(f32*)((u8*)t + 0x2C);
                }
                if (*(f32*)((u8*)t + 0x34) < minY) {
                    minY = *(f32*)((u8*)t + 0x34);
                }
            }
        }
        if (gCameraTargetCount == 1) minY = maxY;
        errX = (f32)(lbl_80345F18 * (f64)(minX + maxX)) - (f32)(scrW / 2);
        errY = (f32)(lbl_80345F18 * (f64)(maxY + minY)) -
               (f32)(scrH - (scrH - 0x40) / 2);
    }

    if (gCameraTargetCount < 1 ||
        c0->radius <= *(f32*)(*(s32*)((u8*)gCurLevel + 0x60) + 0x30) ||
        (lbl_80344500 == 0 && lbl_803444F4 != 0 && lbl_803444DC == 0) ||
        (f64)lbl_803444E8 < lbl_80345F90) {
        if (lbl_803461E0 <= (f64)errX) {
            if ((f64)lbl_803444D8 <= lbl_80345F78) {
                if ((f64)lbl_803444D8 < lbl_80345F78) {
                    lbl_803444D8 = lbl_803444D8 + lbl_80346188;
                    if (lbl_80345F78 <= (f64)lbl_803444D8) {
                        lbl_803444D8 = lbl_80345EC8;
                    }
                }
            } else {
                lbl_803444D8 = lbl_803444D8 - lbl_80346188;
                if ((f64)lbl_803444D8 <= lbl_80345F78) {
                    lbl_803444D8 = lbl_80345EC8;
                }
            }
            if ((lbl_803444D8 < 0.0f ? -lbl_803444D8 : lbl_803444D8) <
                lbl_80346188) {
                lbl_803444D8 = lbl_80345EC8;
            }
        }
        if (lbl_803461E0 <= (f64)errY) {
            if ((f64)lbl_803444D4 <= lbl_80345F78) {
                if ((f64)lbl_803444D4 < lbl_80345F78) {
                    lbl_803444D4 = (f32)((f64)lbl_803444D4 + lbl_80346188);
                    if (lbl_80345F78 <= (f64)lbl_803444D4) {
                        lbl_803444D4 = lbl_80345EC8;
                    }
                }
            } else {
                lbl_803444D4 = (f32)((f64)lbl_803444D4 - lbl_80346188);
                if ((f64)lbl_803444D4 <= lbl_80345F78) {
                    lbl_803444D4 = lbl_80345EC8;
                }
            }
            if ((lbl_803444D4 < 0.0f ? -lbl_803444D4 : lbl_803444D4) <
                lbl_80346188) {
                lbl_803444D4 = lbl_80345EC8;
            }
        }
    } else {
        f32 ax = errX < 0.0f ? -errX : errX;
        if ((lbl_80346160 <= (f64)ax && lbl_80345F78 == (f64)lbl_803444D8) ||
            (lbl_803461E0 <= (f64)ax && lbl_80345F78 != (f64)lbl_803444D8)) {
            if ((f64)errX < lbl_80345F78) {
                lbl_803444D8 = (f32)((f64)lbl_803444D8 - lbl_80346070);
            } else {
                lbl_803444D8 = (f32)((f64)lbl_803444D8 + lbl_80346070);
            }
        } else if ((f64)ax < lbl_803460D0) {
            if ((f64)lbl_803444D8 <= lbl_80345F78) {
                if ((f64)lbl_803444D8 < lbl_80345F78) {
                    lbl_803444D8 = (f32)((f64)lbl_803444D8 + lbl_803461E8);
                    if (lbl_80345F78 <= (f64)lbl_803444D8) {
                        lbl_803444D8 = lbl_80345EC8;
                    }
                }
            } else {
                lbl_803444D8 = (f32)((f64)lbl_803444D8 - lbl_803461E8);
                if ((f64)lbl_803444D8 <= lbl_80345F78) {
                    lbl_803444D8 = lbl_80345EC8;
                }
            }
            if ((lbl_803444D8 < 0.0f ? -lbl_803444D8 : lbl_803444D8) <
                lbl_803461E8) {
                lbl_803444D8 = lbl_80345EC8;
            }
        }
        {
            f32 ay = errY < 0.0f ? -errY : errY;
            if ((lbl_80346160 <= (f64)ay && lbl_80345F78 == (f64)lbl_803444D4) ||
                (lbl_803461E0 <= (f64)ay && lbl_80345F78 != (f64)lbl_803444D4)) {
                if ((f64)errY < lbl_80345F78) {
                    lbl_803444D4 = (f32)((f64)lbl_803444D4 + lbl_80346070);
                } else {
                    lbl_803444D4 = (f32)((f64)lbl_803444D4 - lbl_80346070);
                }
            } else if ((f64)ay < lbl_803460D0) {
                if ((f64)lbl_803444D4 <= lbl_80345F78) {
                    if ((f64)lbl_803444D4 < lbl_80345F78) {
                        lbl_803444D4 = (f32)((f64)lbl_803444D4 + lbl_803461E8);
                        if (lbl_80345F78 <= (f64)lbl_803444D4) {
                            lbl_803444D4 = lbl_80345EC8;
                        }
                    }
                } else {
                    lbl_803444D4 = (f32)((f64)lbl_803444D4 - lbl_803461E8);
                    if ((f64)lbl_803444D4 <= lbl_80345F78) {
                        lbl_803444D4 = lbl_80345EC8;
                    }
                }
                if ((lbl_803444D4 < 0.0f ? -lbl_803444D4 : lbl_803444D4) <
                    lbl_803461E8) {
                    lbl_803444D4 = lbl_80345EC8;
                }
            }
        }
    }

    if (lbl_80345F78 != (f64)lbl_803444D8) {
        f32 a = lbl_803444D8 < 0.0f ? -lbl_803444D8 : lbl_803444D8;
        if ((f64)lbl_803444D8 <= lbl_80345F78) {
            yaw = (f32)((f64)(*(f32*)((u8*)c0 + 0xA8)) + lbl_80346180);
        } else {
            yaw = (f32)((f64)(*(f32*)((u8*)c0 + 0xA8)) - lbl_80346180);
        }
        if ((f64)yaw <= lbl_80345F58) {
            if ((f64)yaw <= lbl_80345F68) yaw = (f32)(lbl_80345F60 + (f64)yaw);
        } else {
            yaw = (f32)((f64)yaw - lbl_80345F60);
        }
        offX = (f32)((f64)sin((f64)yaw) * (f64)a + (f64)offX);
        lbl_803444DC = 1;
        offZ = (f32)((f64)cos((f64)yaw) * (f64)a + (f64)offZ);
    }
    if (lbl_80345F78 != (f64)lbl_803444D4) {
        f32 a = lbl_803444D4 < 0.0f ? -lbl_803444D4 : lbl_803444D4;
        f64 y = (f64)(*(f32*)((u8*)c0 + 0xA8));
        if ((f64)lbl_803444D4 <= lbl_80345F78) y = (f32)(y + lbl_80345F58);
        if (y <= lbl_80345F58) {
            if (y <= lbl_80345F68) y = lbl_80345F60 + y;
        } else {
            y = y - lbl_80345F60;
        }
        yaw = (f32)y;
        offX = (f32)((f64)sin((f64)yaw) * (f64)a + (f64)offX);
        lbl_803444DC = 1;
        offZ = (f32)((f64)cos((f64)yaw) * (f64)a + (f64)offZ);
    }

    if (lbl_80345F78 == (f64)lbl_803444D8 && lbl_80345F78 == (f64)lbl_803444D4) {
        lbl_803444DC = 0;
        lbl_803444D0 = 0;
    } else {
        f32 mn = lbl_8034619C, mx = lbl_803461A0;
        f32 cand[3];
        f32 dir[3];
        for (i = 0; i < 15; i++) {
            CameraTarget* t = &gCameraTargets[i];
            if (t->active > 0) {
                f32 ty = *(f32*)(t->object + 0x44);
                if (ty < mn) mn = ty;
                if (mx < ty) mx = ty;
            }
        }
        gRecorderCameraPosition[0] = (f32)((f64)offX + (f64)wpos0[0]);
        gRecorderCameraPosition[1] = lbl_80345EC8 + wpos0[1];
        gRecorderCameraPosition[2] = (f32)((f64)offZ + (f64)wpos0[2]);
        cand[0] = (f32)((f64)offX + (f64)attn0[0]);
        cand[2] = (f32)((f64)offZ + (f64)attn0[2]);
        attn0[1] = (f32)(lbl_80345F18 * (f64)(mx + mn));
        cand[1] = attn0[1];
        *(f32*)((u8*)c0 + 0x160) = attn0[1];
        dir[0] = (f32)((f64)gRecorderCameraPosition[0] - (f64)cand[0]);
        dir[1] = (f32)((f64)gRecorderCameraPosition[1] - (f64)cand[1]);
        dir[2] = (f32)((f64)gRecorderCameraPosition[2] - (f64)cand[2]);
        NormalVector(dir);
        gRecorderCameraPosition[0] = (f32)((f64)dir[0] * (f64)c0->radius + (f64)cand[0]);
        gRecorderCameraPosition[1] = (f32)((f64)dir[1] * (f64)c0->radius + (f64)cand[1]);
        gRecorderCameraPosition[2] = (f32)((f64)dir[2] * (f64)c0->radius + (f64)cand[2]);
    }

    if (lbl_803444DC != 0) {
        f32 rNew = someone_will_be_off_screen(0, gRecorderCameraPosition);
        f32 rOld = someone_will_be_off_screen(0, wpos0);
        if (rOld <= rNew) {
            wpos0[0] = gRecorderCameraPosition[0];
            wpos0[1] = gRecorderCameraPosition[1];
            wpos0[2] = gRecorderCameraPosition[2];
            attn0[0] = (f32)((f64)attn0[0] + (f64)offX);
            attn0[1] = attn0[1] + lbl_80345EC8;
            attn0[2] = (f32)((f64)attn0[2] + (f64)offZ);
        } else {
            lbl_803444DC = 0;
            lbl_803444D0 = 0;
            lbl_803444D8 = lbl_80345EC8;
            lbl_803444D4 = lbl_80345EC8;
            gRecorderCameraPosition[0] = wpos0[0];
            gRecorderCameraPosition[1] = wpos0[1];
            gRecorderCameraPosition[2] = wpos0[2];
        }
    }
    if (wasPanning == 0 && lbl_803444DC == 1) {
        lbl_803444CC = lbl_80344510;
        lbl_803444C8 = lbl_8034450C;
    }
}

void del_target(u32 id)
{
    s32 found = 0;
    s32 i;
    CameraTarget* p;

    if (gCameraTargetCount != 0) {
        p = gCameraTargets;
        for (i = 0; i < 15; i++, p++) {
            if (p->active != 0 && id == p->object) {
                p->active = 0;
                found = 1;
                p->object = 0;
                p->x = 0.0f;
                p->y = 0.0f;
                p->z = 0.0f;
                gCameraTargetCount--;
                break;
            }
        }
        if (found) {
            recalc_lookat(0, 0);
        }
    }
}

void add_target(u32 id)
{
    s32 found = 0;
    s32 i;
    CameraTarget* p;
    u8* state = gCameraState;

    if (gCameraTargetCount >= 15) {
        return;
    }

    if (gCameraTargetCount == 0) {
        p = (CameraTarget*)(state + 2576);
        for (i = 0; i < 15; i++, p++) {
            p->active = 0;
            p->object = 0;
            p->x = 0.0f;
            p->y = 0.0f;
            p->z = 0.0f;
        }
        gCameraTargetCount = 0;
    }

    p = (CameraTarget*)(state + 2576);
    for (i = 0; i < 15; i++, p++) {
        if (id == p->object) {
            return;
        }
    }

    p = (CameraTarget*)(state + 2576);
    for (i = 0; i < 15; i++, p++) {
        if (p->active == 0) {
            p->active = 1;
            found = 1;
            p->object = id;
            gCameraTargetCount++;
            break;
        }
    }

    if (found) {
        gCameraTargetPositionCount = 0;
        gCameraTargetMode = 3;
        recalc_lookat(0, gCameraTargetCount == 1);
    }
}

void init_targets(void)
{
    f32 v = 0.0f;
    s32 i;
    CameraTarget* p = gCameraTargets;

    for (i = 0; i < 15; i++, p++) {
        p->active = 0;
        p->object = 0;
        p->x = v;
        p->y = v;
        p->z = v;
    }
    gCameraTargetCount = 0;
}

extern f64 lbl_80345EB8;  /* 0.001 */
extern f32 lbl_80346120;  /* 0.001f */

#pragma opt_propagation off
f32 get_yaw(f32* to, f32* from)
{
    f32 dx = to[0] - from[0];
    f32 dz = to[2] - from[2];
    f32 adz;
    f32 adx;
    f32 atanX;
    f32 angle;
    u8 tail[16];
    union {
        f32 f;
        u32 i;
    } uz, ux;
    u8 unused[24];

    uz.f = dz;
    uz.i &= 0x7FFFFFFF;
    adz = uz.f;
    atanX = adz;
    if (adz <= lbl_80345EB8) {
        atanX = lbl_80346120;
    }
    ux.f = dx;
    ux.i &= 0x7FFFFFFF;
    adx = ux.f;
    angle = atan2(adx, atanX);
    if (dz >= lbl_80345F78) {
        if (dx >= lbl_80345F78) {
            angle = angle;
            goto yaw_done;
        } else {
            angle = -angle;
            goto yaw_done;
        }
    } else if (dx >= lbl_80345F78) {
        angle = lbl_80345F58 - angle;
    } else {
        angle = lbl_80345F58 + angle;
    }
yaw_done:
    return FixAngle(angle);
}
#pragma opt_propagation reset

f32 get_pitch(f32* a, f32* b)
{
    f32 dz = a[2] - b[2];
    f32 dx = a[0] - b[0];
    f32 dy = a[1] - b[1];
    f32 len = dz * dz + dx * dx;
    f32 dist;
    f64 ang;
    volatile f32 root;
    union {
        f32 f;
        u32 i;
    } u;

    if (len > lbl_80345EC8) {
        f64 guess = __frsqrte(len);
        guess = lbl_80345F18 * guess * (lbl_80345F20 - len * guess * guess);
        guess = lbl_80345F18 * guess * (lbl_80345F20 - len * guess * guess);
        guess = lbl_80345F18 * guess * (lbl_80345F20 - len * guess * guess);
        root = (f32)(len * (lbl_80345F18 * guess * (lbl_80345F20 - len * guess * guess)));
        len = root;
    }
    dist = len;
    if (len <= lbl_80345EB8) {
        dist = lbl_80346120;
    }
    u.f = dy;
    u.i &= 0x7FFFFFFF;
    ang = atan2(u.f, dist);
    if (dy >= lbl_80345F78) {
        ang = -ang;
    }
    return FixAngle((f32)ang);
}

extern f32 lbl_8023F8C4[], lbl_8023F8B8[];
extern s32 gGameMode, lbl_80344824, lbl_80344414;
extern u8 sItems[];
extern f64 lbl_803461F0;
extern f64 cos(f64);
extern f64 sin(f64);

void get_attn_pos_8002C9A8(s32 camIdx, f32* out)
{
    Camera* cam = &gCameras[camIdx];
    f32* attnDest = (f32*)((u8*)cam + 0x15C);
    s32 aMode = cam->a_mode;
    s32 i;

    cam->old_attn[0] = cam->attn[0];
    cam->old_attn[1] = cam->attn[1];
    cam->old_attn[2] = cam->attn[2];

    if (sMusicTrackHi < 0) {
        if (aMode != 1) {
            cam->attn[0] = lbl_80345EC8;
            cam->attn[1] = lbl_80345EC8;
            cam->attn[2] = lbl_80345EC8;
            out[0] = cam->attn[0];
            out[1] = cam->attn[1];
            out[2] = cam->attn[2];
        }
        attnDest[0] = out[0];
        attnDest[1] = out[1];
        attnDest[2] = out[2];
        cam->attn_dest_no_offset[0] = out[0];
        cam->attn_dest_no_offset[1] = out[1];
        cam->attn_dest_no_offset[2] = out[2];
    } else if (aMode == 1 ||
               ((gGameMode & 0x4000) != 0 && lbl_80344824 == 0)) {
        out[0] = cam->attn[0];
        out[1] = cam->attn[1];
        out[2] = cam->attn[2];
        attnDest[0] = out[0];
        attnDest[1] = out[1];
        attnDest[2] = out[2];
        cam->attn_dest_no_offset[0] = out[0];
        cam->attn_dest_no_offset[1] = out[1];
        cam->attn_dest_no_offset[2] = out[2];
    } else if (aMode == 3 || (u32)(aMode - 5) < 5) {
        if (cam->attnobj == 0) {
            out[0] = cam->attn[0];
            out[1] = cam->attn[1];
            out[2] = cam->attn[2];
        } else {
            f32* p = (f32*)((u8*)cam->attnobj + 0x40);
            out[0] = p[0];
            out[1] = p[1];
            out[2] = p[2];
        }
        attnDest[0] = out[0];
        attnDest[1] = out[1];
        attnDest[2] = out[2];
        cam->attn_dest_no_offset[0] = out[0];
        cam->attn_dest_no_offset[1] = out[1];
        cam->attn_dest_no_offset[2] = out[2];
    } else if (aMode == 10) {
        s32 t = *(s32*)((u8*)cam + 0x114);
        if (*(s16*)(sTriggerCameras + t * 0x28 + 2) == 0) {
            out[0] = cam->attn[0];
            out[1] = cam->attn[1];
            out[2] = cam->attn[2];
        } else {
            out[0] = TC_X(t);
            out[1] = TC_Y(t);
            out[2] = TC_Z(t);
        }
        attnDest[0] = out[0];
        attnDest[1] = out[1];
        attnDest[2] = out[2];
        cam->attn_dest_no_offset[0] = out[0];
        cam->attn_dest_no_offset[1] = out[1];
        cam->attn_dest_no_offset[2] = out[2];
    } else {
        s32* timer = (s32*)((u8*)cam + 0xD8);
        if (lbl_803444F4 == 0) {
            *timer = 0;
        }
        if (*timer < 0xB4 || lbl_80344960 < 0) {
            f32 minX = lbl_8034619C, maxX = lbl_803461A0;
            f32 minY = lbl_8034619C, maxY = lbl_803461A0;
            f32 minZ = lbl_8034619C, maxZ = lbl_803461A0;
            f32 sv0, sv1, sv2;
            for (i = 0; i < 15; i++) {
                CameraTarget* target = &gCameraTargets[i];
                if (target->active > 0) {
                    f32* p = (f32*)(target->object + 0x40);
                    if (p[0] < minX) minX = p[0];
                    if (maxX < p[0]) maxX = p[0];
                    if (p[1] < minY) minY = p[1];
                    if (maxY < p[1]) maxY = p[1];
                    if (p[2] < minZ) minZ = p[2];
                    if (maxZ < p[2]) maxZ = p[2];
                }
            }
            out[0] = (f32)(lbl_80345F18 * (f64)(minX + maxX));
            out[1] = (f32)(lbl_80345F18 * (f64)(minY + maxY));
            out[2] = (f32)(lbl_80345F18 * (f64)(minZ + maxZ));
            cam->attn_dest_no_offset[0] = out[0];
            cam->attn_dest_no_offset[1] = out[1];
            cam->attn_dest_no_offset[2] = out[2];
            if (*(s32*)((u8*)cam + 0xEC) == 3) {
                if (gNumTransmitters == 0) {
                    f64 cp = cos((f64)cam->pyr[0]);
                    out[2] = (f32)(lbl_80345F18 * lbl_80345F18 *
                        (f64)(maxZ - minZ) * cp + (f64)out[2]);
                } else {
                    f64 sy = sin((f64)cam->pyr[1]);
                    f64 cp = cos((f64)cam->pyr[0]);
                    f64 cy;
                    f64 cp2;
                    out[0] = (f32)((f32)((f32)(lbl_803461F0 * lbl_80345F18 *
                        (f64)(maxX - minX)) * cp) * sy + (f64)out[0]);
                    cy = cos((f64)cam->pyr[1]);
                    cp2 = cos((f64)cam->pyr[0]);
                    out[2] = (f32)((f32)((f32)(lbl_803461F0 * lbl_80345F18 *
                        (f64)(maxZ - minZ)) * cp2) * cy + (f64)out[2]);
                }
            }
            attnDest[0] = out[0];
            attnDest[1] = out[1];
            attnDest[2] = out[2];
            sv0 = out[0];
            sv1 = out[1];
            sv2 = out[2];
            lbl_80344418 = 0;
            if (lbl_803447B8 == 0 && lbl_80344414 < 2) {
                for (i = 0; i < 3; i++) {
                    if (lbl_8023F8C4[i] <= out[i]) {
                        if (lbl_8023F8B8[i] < out[i]) {
                            out[i] = lbl_8023F8B8[i];
                            lbl_80344418 = 1;
                        }
                    } else {
                        out[i] = lbl_8023F8C4[i];
                        lbl_80344418 = 1;
                    }
                }
            }
            if (lbl_80344414 != 0) {
                if (sv0 - out[0] == lbl_80345EC8 &&
                    sv1 - out[1] == lbl_80345EC8 &&
                    sv2 - out[2] == lbl_80345EC8) {
                    lbl_80344414 = 0;
                } else {
                    out[0] = sv0;
                    out[1] = sv1;
                    out[2] = sv2;
                }
            }
            *timer = *timer + gFrameTicks;
        } else {
            u8* w = *(u8**)(*(u8**)(sItems +
                lbl_80344960 * 0xF0 + 0xDC) + 0x28);
            out[0] = *(f32*)(w + 0x30);
            out[1] = *(f32*)(w + 0x34);
            out[2] = *(f32*)(w + 0x38);
            attnDest[0] = out[0];
            attnDest[1] = out[1];
            attnDest[2] = out[2];
            cam->attn_dest_no_offset[0] = out[0];
            cam->attn_dest_no_offset[1] = out[1];
            cam->attn_dest_no_offset[2] = out[2];
        }
    }
}

void recalc_lookat(s32 camIdx, s32 snap)
{
    Camera* cam = &gCameras[camIdx];
    f32 pos[3];

    if (cam->a_mode == ATN_FREE || cam->a_mode == ATN_LOCK ||
        cam->a_mode == ATN_POINT) {
        return;
    }
    get_attn_pos_8002C9A8(camIdx, pos);
    if (snap != 0) {
        cam->attn[0] = pos[0];
        cam->attn[1] = pos[1];
        cam->attn[2] = pos[2];
        cam->delta[0] = lbl_80345EC8;
        cam->delta[1] = lbl_80345EC8;
        cam->delta[2] = lbl_80345EC8;
        gCameraTargetPositionCount = 0;
        gCameraTargetMode = ATN_TARGET;
        lbl_80344508 = -1;
    }
    {
        f32 dx = cam->wpos[0] - pos[0];
        f32 dy = cam->wpos[1] - pos[1];
        f32 dz = cam->wpos[2] - pos[2];
        f32 d2;
        volatile f32 root;

        if (snap == 0) {
            return;
        }
        d2 = dx * dx + dy * dy + dz * dz;
        if (d2 > lbl_80345EC8) {
            f64 g = __frsqrte((f64)d2);
            g = lbl_80345F18 * g * (lbl_80345F20 - d2 * g * g);
            g = lbl_80345F18 * g * (lbl_80345F20 - d2 * g * g);
            g = lbl_80345F18 * g * (lbl_80345F20 - d2 * g * g);
            g = lbl_80345F18 * g * (lbl_80345F20 - d2 * g * g);
            root = (f32)(d2 * g);
            d2 = root;
        }
        cam->radius = d2;
    }
}

extern s32 lbl_80344498, lbl_803444E0, lbl_803444F8, lbl_80344470, lbl_80344474;
extern s32 gCameraTargetPositionCount, gCameraTargetMode, lbl_8034446C, lbl_80344494, lbl_803443F0;
extern s32 lbl_80344550, shake_type, shaking, shake_count, shake_delay;
extern s32 shake_priority, lbl_803447B4, lbl_803444BC, gCameraWindowLeftLimit;
extern s32 gCameraWindowRightLimit, gCameraWindowTopLimit, gCameraWindowBottomLimit, lbl_80344514, lbl_80344518;
extern s32 lbl_80344520, gNumEnemies, lbl_803447F8, lbl_80344524;
extern s32 lbl_80344288, lbl_8034441C, sSpecialTransmitter, lbl_80344420, lbl_80344A28;
extern f32 lbl_80344460, lbl_80344464, shake_rad, lbl_803461F8, lbl_803461FC;
extern f32 gCameraWindowScaleX, gCameraWindowScaleY, lbl_80345F80;
extern f32 lbl_80344428, lbl_80344424, lbl_80344438, lbl_80344434;
extern f32 lbl_8034443C, lbl_80344440, lbl_80344444, lbl_80344448;
extern f32 lbl_8034442C, lbl_80344430;
extern f64 lbl_80346200, lbl_80345F48, lbl_803460B0, lbl_803460B8, lbl_803460C0;
extern f64 lbl_803460C8, lbl_80345EF0, lbl_80346238, lbl_80346240, lbl_80346078;
extern f64 lbl_80346080, lbl_80346250, lbl_8034601C;
extern f32 lbl_80346208, lbl_80346148, lbl_80346158, lbl_8034620C, lbl_80346210;
extern f32 lbl_80346214, lbl_80346218, lbl_8034621C, lbl_80346220, lbl_80346224;
extern f32 lbl_80346228, lbl_8034622C, lbl_80346230, lbl_80346204, lbl_80346248;
extern f32 lbl_80346258, lbl_8034625C, lbl_80345F14, lbl_80344880;
extern f32 lbl_8023F824, lbl_8023F828, lbl_8023F82C, lbl_8023F830, lbl_8023F834;
extern f32 lbl_8023F838, lbl_802757D8, lbl_8028CAC8, lbl_8028CAD0, lbl_8028CAB4;
extern f32 lbl_8028CAA8, lbl_8028CACC;
extern f32 gDefaultPlayerPosition[], lbl_8023F8D4[], lbl_80258E08[];
extern u8 lbl_80344EE8[], gIdentityMatrix[];
extern u8 sMilestones[];
void ChangeWindow(void);
s32 FloorPos(f32 a, f32* pos, s32 c);
s32 fn_80051480(f32* pos);
void dbgTextPrintfCol();

void InitCamera(s32 resetAll)
{
    Camera* c0 = (Camera*)(gCameraState + 0xC8);
    s32 scrH = MBScreenHeight();
    s32 scrW = MBScreenWidth();
    s32 uiFov = 0x40;
    s32 i;
    f32 mat[18];
    f32 in[3];
    f32 out[3];

    lbl_80344498 = 0;
    lbl_803444DC = 0;
    lbl_803444D0 = 0;
    lbl_803444D4 = lbl_80345EC8;
    lbl_803444D8 = lbl_80345EC8;
    lbl_80344960 = -1;
    lbl_803444F0 = -1;
    lbl_8034453C = 0;
    lbl_803444E0 = 0;
    lbl_80344500 = 0;
    lbl_803444F8 = 0;
    lbl_803444F4 = 1;
    lbl_803444E4 = 0;
    lbl_80344470 = 0;
    lbl_80344474 = 0;
    gCameraTargetPositionCount = 0;
    gCameraTargetMode = 0;
    lbl_80344508 = -1;
    lbl_8034446C = -1;
    lbl_80344494 = 0;
    lbl_80344460 = lbl_80345EC8;
    lbl_80344464 = lbl_80345EC8;
    lbl_803443F0 = 0;
    lbl_803443F8 = 600;
    lbl_80344538 = 0;
    lbl_80344400 = 1;
    gScriptedCameraState = 0;
    lbl_8034452C = lbl_803461F8;
    lbl_80344528 = lbl_803461FC;
    lbl_80344408 = lbl_8034616C;
    lbl_80344530 = lbl_8034616C;
    lbl_80344404 = 1;
    lbl_803447B4 = 0;
    lbl_803447B8 = 0;
    lbl_80344550 = 0;
    shake_type = 0;
    shaking = 0;
    shake_count = 0;
    shake_delay = 0;
    shake_rad = lbl_80345EC8;
    shake_priority = 0;

    {
    u32* c = (u32*)&gCameras[0];
    f32 zero = lbl_80345EC8;
    f32 radius = (f32)lbl_80346200;
    for (i = 0; i < 6; i++) {
        f32* cf = (f32*)c;
        c[0] = 0;
        CopyMat4((f32*)gIdentityMatrix, cf + 1);
        cf[0x11] = zero; cf[0x12] = zero; cf[0x13] = zero;
        cf[0x15] = zero; cf[0x16] = zero; cf[0x17] = zero;
        cf[0x19] = zero; cf[0x1a] = zero; cf[0x1b] = zero;
        cf[0x1d] = zero; cf[0x1e] = zero; cf[0x1f] = zero;
        cf[0x21] = zero; cf[0x22] = zero; cf[0x23] = zero;
        cf[0x25] = zero; cf[0x26] = zero; cf[0x27] = zero;
        cf[0x29] = zero; cf[0x2a] = zero; cf[0x2b] = zero;
        cf[0x2d] = zero; cf[0x2e] = zero; cf[0x2f] = zero;
        cf[0x47] = zero; cf[0x48] = zero; cf[0x49] = zero;
        cf[0x4b] = zero; cf[0x4c] = zero; cf[0x4d] = zero;
        cf[0x4f] = zero; cf[0x50] = zero; cf[0x51] = zero;
        cf[0x53] = zero; cf[0x54] = zero; cf[0x55] = zero;
        cf[0x57] = zero; cf[0x58] = zero; cf[0x59] = zero;
        cf[0x5b] = zero; cf[0x5c] = zero; cf[0x5d] = zero;
        cf[0x5f] = zero; cf[0x60] = zero; cf[0x61] = zero;
        c[0x36] = 0;
        cf[0x31] = radius;
        c[0x32] = (u32)-1;
        c[0x35] = 0;
        c[0x34] = 0;
        c[0x33] = 0;
        cf[0x3a] = zero;
        cf[0x39] = zero;
        cf[0x38] = zero;
        cf[0x37] = zero;
        c[0x3c] = (u32)-1;
        c[0x3b] = (u32)-1;
        c[0x3d] = 0;
        c[0x3f] = (u32)-1;
        c[0x3e] = (u32)-1;
        c[0x46] = 0;
        c[0x45] = 0;
        c[0x44] = 0;
        c[0x43] = 0;
        c[0x42] = 0;
        c[0x41] = 0;
        c[0x40] = 0;
        c += 99;
    }
    }

    if (resetAll == 0) {
        if (gGameMode == 0x400B) {
            if (*(s32*)((u8*)c0 + 0xEC) != 2) {
                *(s32*)((u8*)c0 + 0xF0) = *(s32*)((u8*)c0 + 0xEC);
                *(s32*)((u8*)c0 + 0xEC) = 2;
            }
            if (*(s32*)((u8*)c0 + 0xF8) != 1) {
                *(s32*)((u8*)c0 + 0xFC) = *(s32*)((u8*)c0 + 0xF8);
                *(s32*)((u8*)c0 + 0xF8) = 1;
            }
            c0->wpos[0] = lbl_80345EC8;
            c0->wpos[1] = lbl_80346208;
            c0->wpos[2] = lbl_80346148;
            *(f32*)((u8*)c0 + 0x12C) = lbl_80345EC8;
            *(f32*)((u8*)c0 + 0x130) = lbl_80346158;
            *(f32*)((u8*)c0 + 0x134) = lbl_80345EC8;
            c0->state = 1;
        } else if (gGameMode == 0x400D || gGameMode == 0x4013 ||
                   gGameMode == 0x4017) {
            f64 s, cc;
            f32 dx, dz, dy;
            f64 len2;
            f64 g;
            if (*(s32*)((u8*)c0 + 0xEC) != 2) {
                *(s32*)((u8*)c0 + 0xF0) = *(s32*)((u8*)c0 + 0xEC);
                *(s32*)((u8*)c0 + 0xEC) = 2;
            }
            if (*(s32*)((u8*)c0 + 0xF8) != 1) {
                *(s32*)((u8*)c0 + 0xFC) = *(s32*)((u8*)c0 + 0xF8);
                *(s32*)((u8*)c0 + 0xF8) = 1;
            }
            *(f32*)((u8*)c0 + 0xE0) = lbl_80345F48;
            *(f32*)((u8*)c0 + 0xA8) = lbl_80345F48;
            *(f32*)((u8*)c0 + 0xE4) = lbl_80345EC8;
            lbl_8023F824 = lbl_8034620C;
            lbl_8023F828 = lbl_80346210;
            lbl_8023F82C = lbl_8034620C;
            lbl_8023F830 = lbl_80346214;
            lbl_8023F834 = lbl_80346218;
            lbl_8023F838 = lbl_80346214;
            s = sin(lbl_80345F48);
            cc = cos(lbl_80345F48);
            dz = (f32)((f64)lbl_8023F828 - (f64)lbl_8023F834);
            dx = (f32)(s * (f64)lbl_8023F824) - (f32)(s * (f64)lbl_8023F830);
            dy = (f32)((f32)(cc * (f64)lbl_8023F82C) -
                       (f32)(cc * (f64)lbl_8023F838));
            len2 = dy * dy + dx * dx + dz * dz;
            if ((f64)lbl_80345EC8 < len2) {
                g = __frsqrte(len2);
                g = lbl_80345F18 * g * -(len2 * g * g - lbl_80345F20);
                g = lbl_80345F18 * g * -(len2 * g * g - lbl_80345F20);
                g = lbl_80345F18 * g * -(len2 * g * g - lbl_80345F20);
                len2 = (f32)(len2 * lbl_80345F18 * g * -(len2 * g * g - lbl_80345F20));
            }
            c0->radius = (f32)len2;
            c0->wpos[0] = (f32)(s * (f64)lbl_8023F824) +
                          (f32)(s * (f64)*(f32*)((u8*)c0 + 0xE4));
            c0->wpos[1] = lbl_8023F828 + lbl_80345EC8;
            c0->wpos[2] = (f32)(cc * (f64)lbl_8023F82C) +
                          (f32)(cc * (f64)*(f32*)((u8*)c0 + 0xE4));
            *(f32*)((u8*)c0 + 0x12C) = (f32)(s * (f64)lbl_8023F830) +
                          (f32)(s * (f64)*(f32*)((u8*)c0 + 0xE4));
            *(f32*)((u8*)c0 + 0x130) = lbl_8023F834 + lbl_80345EC8;
            *(f32*)((u8*)c0 + 0x134) = (f32)(cc * (f64)lbl_8023F838) +
                          (f32)(cc * (f64)*(f32*)((u8*)c0 + 0xE4));
            *(s32*)((u8*)c0 + 0xD0) = 0;
            *(s32*)((u8*)c0 + 0xCC) = 0;
            c0->state = 1;
        } else if (gGameMode == 0x8007) {
            if (*(s32*)((u8*)c0 + 0xEC) != 2) {
                *(s32*)((u8*)c0 + 0xF0) = *(s32*)((u8*)c0 + 0xEC);
                *(s32*)((u8*)c0 + 0xEC) = 2;
            }
            if (*(s32*)((u8*)c0 + 0xF8) != 1) {
                *(s32*)((u8*)c0 + 0xFC) = *(s32*)((u8*)c0 + 0xF8);
                *(s32*)((u8*)c0 + 0xF8) = 1;
            }
            c0->wpos[0] = lbl_8034621C;
            c0->wpos[1] = lbl_80346220;
            c0->wpos[2] = lbl_80346224;
            *(f32*)((u8*)c0 + 0x12C) = lbl_80346228;
            *(f32*)((u8*)c0 + 0x130) = lbl_8034622C;
            *(f32*)((u8*)c0 + 0x134) = lbl_80346230;
            c0->state = 1;
        } else if (gGameMode == 0x8008) {
            if (lbl_80344288 == 0) {
                u8* hdr = *(u8**)((u8*)gCurLevel + 0x60);
                gNumEnemies = *(s16*)(hdr + 0x34);
                lbl_8034441C = *(s16*)(hdr + 0x26);
                if (lbl_8034441C < 0 && sSpecialTransmitter == 0) {
                    lbl_8034441C = 0;
                    *(s16*)(hdr + 0x26) = 0;
                }
                if (lbl_8034441C == 0) {
                    f64 g;
                    f32 v[3];
                    if (*(s32*)((u8*)c0 + 0xEC) != 5) {
                        *(s32*)((u8*)c0 + 0xF0) = *(s32*)((u8*)c0 + 0xEC);
                        *(s32*)((u8*)c0 + 0xEC) = 5;
                    }
                    if (*(s32*)((u8*)c0 + 0xF8) != 1) {
                        *(s32*)((u8*)c0 + 0xFC) = *(s32*)((u8*)c0 + 0xF8);
                        *(s32*)((u8*)c0 + 0xF8) = 1;
                    }
                    *(f32*)((u8*)c0 + 0x12C) = lbl_8028CAC8;
                    *(f32*)((u8*)c0 + 0x134) = lbl_8028CAD0;
                    *(f32*)((u8*)c0 + 0x130) = lbl_8028CAB4;
                    do {
                        g = FloorPos(lbl_80344880, (f32*)((u8*)c0 + 0x12C), 0);
                        if (g != (f64)lbl_80344880 || g <= (f64)lbl_8028CAA8) {
                            break;
                        }
                        *(f32*)((u8*)c0 + 0x130) =
                            (f32)((f64)*(f32*)((u8*)c0 + 0x130) - lbl_80345EF0);
                    } while (1);
                    if (g == (f64)lbl_80344880) {
                        *(f32*)((u8*)c0 + 0x130) = lbl_8028CACC;
                    } else {
                        *(f32*)((u8*)c0 + 0x130) = (f32)(lbl_80346238 + g);
                    }
                    if (*(f32*)((u8*)c0 + 0x130) < lbl_802757D8) {
                        *(f32*)((u8*)c0 + 0x130) = lbl_802757D8;
                    }
                    c0->radius = *(f32*)(hdr + 0x28);
                    *(f32*)((u8*)c0 + 0xE0) =
                        (f32)(lbl_80346240 * (f64)c0->radius);
                    *(f32*)((u8*)c0 + 0xE4) = (f32)(lbl_80345EF0 + (f64)lbl_8028CACC);
                    *(f32*)((u8*)c0 + 0xE8) =
                        (f32)(lbl_80345FE0 + (f64)*(f32*)((u8*)c0 + 0xE4));
                    *(f32*)((u8*)c0 + 0xA4) = lbl_80346248;
                    *(f32*)((u8*)c0 + 0xA8) = lbl_80345EC8;
                    *(f32*)((u8*)c0 + 0xAC) = lbl_80345EC8;
                    CreateYPRMatrix(mat, (f32*)((u8*)c0 + 0xA4));
                    in[0] = lbl_80345EC8;
                    in[1] = lbl_80345EC8;
                    in[2] = c0->radius;
                    WorldVector(in, out, mat);
                    c0->wpos[0] = *(f32*)((u8*)c0 + 0x12C) + out[0];
                    c0->wpos[1] = *(f32*)((u8*)c0 + 0x130) + out[1];
                    c0->wpos[2] = *(f32*)((u8*)c0 + 0x134) + out[2];
                    v[0] = c0->wpos[0] - *(f32*)((u8*)c0 + 0x12C);
                    v[1] = c0->wpos[1] - *(f32*)((u8*)c0 + 0x130);
                    v[2] = c0->wpos[2] - *(f32*)((u8*)c0 + 0x134);
                    NormalVector(v);
                    c0->wpos[0] =
                        (f32)((f64)v[0] * (f64)c0->radius + (f64)*(f32*)((u8*)c0 + 0x12C));
                    c0->wpos[1] =
                        (f32)((f64)v[1] * (f64)c0->radius + (f64)*(f32*)((u8*)c0 + 0x130));
                    c0->wpos[2] =
                        (f32)((f64)v[2] * (f64)c0->radius + (f64)*(f32*)((u8*)c0 + 0x134));
                    c0->state = 1;
                    lbl_80344420 = 0x708;
                } else if (lbl_8034441C < 0 || lbl_8034441C > 1) {
                    f32 saveW[3];
                    f32 saveA[3];
                    f32 d[3];
                    c0->wpos[0] = *(f32*)(sSpecialTransmitter + 4);
                    c0->wpos[1] = *(f32*)(sSpecialTransmitter + 8);
                    c0->wpos[2] = *(f32*)(sSpecialTransmitter + 0xC);
                    *(f32*)((u8*)c0 + 0xA4) = *(f32*)(sSpecialTransmitter + 0x14);
                    *(f32*)((u8*)c0 + 0xA8) = *(f32*)(sSpecialTransmitter + 0x18);
                    *(f32*)((u8*)c0 + 0xAC) = *(f32*)(sSpecialTransmitter + 0x1C);
                    CreateYPRMatrix(mat, (f32*)((u8*)c0 + 0xA4));
                    c0->radius = lbl_80346148;
                    in[0] = lbl_80345EC8;
                    in[1] = lbl_80345EC8;
                    in[2] = lbl_80346148;
                    WorldVector(in, out, mat);
                    *(f32*)((u8*)c0 + 0x12C) = c0->wpos[0] + out[0];
                    *(f32*)((u8*)c0 + 0x130) = c0->wpos[1] + out[1];
                    *(f32*)((u8*)c0 + 0x134) = c0->wpos[2] + out[2];
                    if (*(s32*)((u8*)c0 + 0xEC) != 2) {
                        *(s32*)((u8*)c0 + 0xF0) = *(s32*)((u8*)c0 + 0xEC);
                        *(s32*)((u8*)c0 + 0xEC) = 2;
                    }
                    if (*(s32*)((u8*)c0 + 0xF8) != 1) {
                        *(s32*)((u8*)c0 + 0xFC) = *(s32*)((u8*)c0 + 0xF8);
                        *(s32*)((u8*)c0 + 0xF8) = 1;
                    }
                    *(s32*)((u8*)c0 + 0xC8) = 0;
                    saveW[0] = c0->wpos[0];
                    saveW[1] = c0->wpos[1];
                    saveW[2] = c0->wpos[2];
                    saveA[0] = *(f32*)((u8*)c0 + 0x12C);
                    saveA[1] = *(f32*)((u8*)c0 + 0x130);
                    saveA[2] = *(f32*)((u8*)c0 + 0x134);
                    StandardCamera_8002B828(0);
                    DoShake(saveW, saveA);
                    d[0] = saveA[0] - saveW[0];
                    d[1] = saveA[1] - saveW[1];
                    d[2] = saveA[2] - saveW[2];
                    LookInDirection(d, lbl_8023F8D4);
                    c0->state = 1;
                    lbl_80344428 = lbl_80345EC8;
                    lbl_80344424 = lbl_80345EC8;
                    lbl_80344438 = lbl_80345EC8;
                    lbl_80344434 = lbl_80345EC8;
                    lbl_80344454 = lbl_80345EC8;
                    lbl_8034444C = lbl_80345EC8;
                    lbl_80344458 = lbl_80345EC8;
                    lbl_80344450 = lbl_80345EC8;
                    CameraSupervisor(0);
                    if (lbl_80344510 != lbl_8034450C) {
                        f64 dv;
                        f32 fa;
                        lbl_80344444 = get_pitch(c0->wpos,
                            &lbl_80258E08[lbl_8034450C * 10]);
                        lbl_80344448 = get_yaw(c0->wpos,
                            &lbl_80258E08[lbl_8034450C * 10]);
                        fa = FixAngle((f32)(lbl_80345F60 - (f64)lbl_80344448));
                        dv = (f64)(f32)(lbl_803460B0 * lbl_803460B8 * (f64)fa);
                        if (dv < (f64)lbl_80345EC8) {
                            dv = (f64)(f32)(dv + lbl_803460C0);
                        }
                        if (lbl_803460C8 < dv) {
                            dv = (f64)lbl_80345EC8;
                        }
                        if (lbl_80344A28 == 0) {
                            s32 py = (s32)(lbl_803460B0 * lbl_803460B8 *
                                           (f64)lbl_80344444);
                            dbgTextPrintfCol(dv, lbl_803460B8, lbl_803460B0, 2, 3,
                                "DEST P=%d Y=%4.1f", py);
                        }
                    }
                    lbl_8034442C = lbl_80344444;
                    lbl_80344430 = lbl_80344448;
                    lbl_80344530 = *(f32*)((u8*)c0 + 0xA4);
                    lbl_8034443C = *(f32*)((u8*)c0 + 0xA4);
                    lbl_80344440 = *(f32*)((u8*)c0 + 0xA8);
                    lbl_80344408 = *(f32*)((u8*)c0 + 0xA4);
                } else {
                    f32 d[3];
                    if (*(s32*)((u8*)c0 + 0xEC) != 2) {
                        *(s32*)((u8*)c0 + 0xF0) = *(s32*)((u8*)c0 + 0xEC);
                        *(s32*)((u8*)c0 + 0xEC) = 2;
                    }
                    if (*(s32*)((u8*)c0 + 0xF8) != 1) {
                        *(s32*)((u8*)c0 + 0xFC) = *(s32*)((u8*)c0 + 0xF8);
                        *(s32*)((u8*)c0 + 0xF8) = 1;
                    }
                    c0->wpos[0] = lbl_8028CAC8;
                    c0->wpos[2] = lbl_8028CAD0;
                    c0->wpos[1] = (f32)(lbl_80346250 + (f64)lbl_802757D8);
                    c0->radius = lbl_80346258;
                    *(f32*)((u8*)c0 + 0xA4) = *(f32*)(hdr + 0x28);
                    *(f32*)((u8*)c0 + 0xA8) = lbl_80345EC8;
                    *(f32*)((u8*)c0 + 0xAC) = lbl_80345EC8;
                    CreateYPRMatrix(mat, (f32*)((u8*)c0 + 0xA4));
                    in[0] = lbl_80345EC8;
                    in[1] = lbl_80345EC8;
                    in[2] = c0->radius;
                    WorldVector(in, out, mat);
                    *(f32*)((u8*)c0 + 0x12C) = c0->wpos[0] + out[0];
                    *(f32*)((u8*)c0 + 0x130) = c0->wpos[1] + out[1];
                    *(f32*)((u8*)c0 + 0x134) = c0->wpos[2] + out[2];
                    d[0] = *(f32*)((u8*)c0 + 0x12C) - c0->wpos[0];
                    d[1] = *(f32*)((u8*)c0 + 0x130) - c0->wpos[1];
                    d[2] = *(f32*)((u8*)c0 + 0x134) - c0->wpos[2];
                    NormalVector(d);
                    *(f32*)((u8*)c0 + 0x12C) =
                        (f32)((f64)d[0] * (f64)c0->radius + (f64)c0->wpos[0]);
                    *(f32*)((u8*)c0 + 0x130) =
                        (f32)((f64)d[1] * (f64)c0->radius + (f64)c0->wpos[1]);
                    *(f32*)((u8*)c0 + 0x134) =
                        (f32)((f64)d[2] * (f64)c0->radius + (f64)c0->wpos[2]);
                    c0->state = 1;
                    lbl_80344420 = 0x5DC;
                }
            } else {
                if (*(s32*)((u8*)c0 + 0xEC) != 2) {
                    *(s32*)((u8*)c0 + 0xF0) = *(s32*)((u8*)c0 + 0xEC);
                    *(s32*)((u8*)c0 + 0xEC) = 2;
                }
                if (*(s32*)((u8*)c0 + 0xF8) != 0) {
                    *(s32*)((u8*)c0 + 0xFC) = *(s32*)((u8*)c0 + 0xF8);
                    *(s32*)((u8*)c0 + 0xF8) = 0;
                }
                c0->wpos[0] = gDefaultPlayerPosition[0];
                c0->wpos[1] = *(f32*)(gDefaultPlayerPosition + 1);
                c0->wpos[2] = *(f32*)(gDefaultPlayerPosition + 2);
                c0->wpos[1] = (f32)(lbl_80346078 +
                    (f64)FloorPos(lbl_80344880, c0->wpos, 0));
                *(s32*)((u8*)c0 + 0xD0) = fn_80051480(c0->wpos);
                *(f32*)((u8*)c0 + 0xA4) = lbl_80345EC8;
                *(f32*)((u8*)c0 + 0xA8) = get_yaw(
                    (f32*)(sMilestones + *(s32*)((u8*)c0 + 0xD0) * 0x68 + 0x30),
                    c0->wpos);
                *(f32*)((u8*)c0 + 0xE0) = *(f32*)((u8*)c0 + 0xA8);
                *(f32*)((u8*)c0 + 0xAC) = lbl_80345EC8;
                c0->radius = lbl_80345F14;
                c0->state = 1;
            }
        } else if (gCurLevel != 0) {
            s16* hdr = *(s16**)((u8*)gCurLevel + 0x60);
            if (*(s32*)((u8*)c0 + 0xEC) != 1) {
                *(s32*)((u8*)c0 + 0xF0) = *(s32*)((u8*)c0 + 0xEC);
                *(s32*)((u8*)c0 + 0xEC) = 1;
            }
            if (*(s32*)((u8*)c0 + 0xF8) != 0) {
                *(s32*)((u8*)c0 + 0xFC) = *(s32*)((u8*)c0 + 0xF8);
                *(s32*)((u8*)c0 + 0xF8) = 0;
            }
            *(f32*)((u8*)c0 + 0x12C) = lbl_80345EC8;
            *(f32*)((u8*)c0 + 0x130) = lbl_80345EC8;
            *(f32*)((u8*)c0 + 0x134) = lbl_80345EC8;
            c0->radius = lbl_8034625C;
            c0->wpos[0] = lbl_80345EC8;
            c0->wpos[1] = lbl_80345EC8;
            c0->wpos[2] = lbl_80345EC8;
            lbl_80344538 = *hdr;
            lbl_80344408 = *(f32*)(hdr + 2);
            lbl_80344404 = hdr[1];
            lbl_80344524 = *(s32*)(hdr + 4);
            lbl_8034452C = *(f32*)(hdr + 0x16);
            lbl_80344528 = *(f32*)(hdr + 0x18);
            lbl_803447F8 = 18000;
            gNumEnemies = hdr[0x1a];
            uiFov = scrH == 0x100 ? 0x2a : 0x40;
            c0->state = 1;
            lbl_80344530 = lbl_80344408;
        }
    } else {
        if (*(s32*)((u8*)c0 + 0xEC) != 2) {
            *(s32*)((u8*)c0 + 0xF0) = *(s32*)((u8*)c0 + 0xEC);
            *(s32*)((u8*)c0 + 0xEC) = 2;
        }
        if (*(s32*)((u8*)c0 + 0xF8) != 1) {
            *(s32*)((u8*)c0 + 0xFC) = *(s32*)((u8*)c0 + 0xF8);
            *(s32*)((u8*)c0 + 0xF8) = 1;
        }
        c0->wpos[0] = lbl_80345EC8;
        c0->wpos[1] = lbl_80345EC8;
        c0->wpos[2] = lbl_80345EC8;
        *(f32*)((u8*)c0 + 0x12C) = lbl_80345EC8;
        *(f32*)((u8*)c0 + 0x130) = lbl_80345EC8;
        *(f32*)((u8*)c0 + 0x134) = lbl_80346204;
        c0->state = 1;
    }

    lbl_80344534 = lbl_80118B60[lbl_80344538];
    {
        u8* mat3 = (u8*)&gCameras[0] + 0x34;
        u8* src = (u8*)&gCameras[0] + 0x64;
        for (i = 0; i < 6; i++) {
            *(u32*)(mat3 + 0) = *(u32*)(src + 0);
            *(u32*)(mat3 + 4) = *(u32*)(src + 4);
            *(u32*)(mat3 + 8) = *(u32*)(src + 8);
            mat3 += 0x18C;
            src += 0x18C;
        }
    }
    lbl_8023F818 = lbl_80345EC8;
    lbl_8023F81C = lbl_80345EC8;
    lbl_8023F820 = lbl_80345EC8;
    gCameraWindowLeftLimit = 0;
    gCameraWindowScaleX = lbl_80345F80;
    gCameraWindowScaleY = lbl_80345F80;
    lbl_803444BC = 0;
    gCameraWindowRightLimit = scrW;
    gCameraWindowTopLimit = scrH;
    gCameraWindowBottomLimit = uiFov;
    ChangeWindow();
    MBWindowZoom(lbl_80345F80);
    lbl_80344520 = (s32)-(f32)((f64)*(f32*)(lbl_80344EE8 + 0x14) * lbl_8034601C -
                              (f64)*(f32*)(lbl_80344EE8 + 8));
    lbl_8034451C = (s32)((f64)*(f32*)(lbl_80344EE8 + 0x14) * lbl_8034601C +
                         (f64)*(f32*)(lbl_80344EE8 + 8));
    lbl_80344518 = (s32)((f64)*(f32*)(lbl_80344EE8 + 0x18) * lbl_8034601C +
                         (f64)*(f32*)(lbl_80344EE8 + 0xC));
    lbl_80344514 = (s32)-(f32)((f64)*(f32*)(lbl_80344EE8 + 0x18) * lbl_8034601C -
                              (f64)*(f32*)(lbl_80344EE8 + 0xC));
    for (i = 0; i < 15; i++) {
        CameraTarget* t = &gCameraTargets[i];
        t->active = 0;
        t->object = 0;
        t->x = lbl_80345EC8;
        t->y = lbl_80345EC8;
        t->z = lbl_80345EC8;
    }
    gCameraTargetCount = 0;
    ProcCamera_8002E548(0, 0);
    {
        f32* cf = (f32*)&gCameras[0];
        f32 zero = lbl_80345EC8;
        for (i = 0; i < 6; i++) {
            cf[0x11] = cf[0xd];
            cf[0x12] = cf[0xe];
            cf[0x13] = cf[0xf];
            cf[0x15] = zero;
            cf[0x16] = zero;
            cf[0x17] = zero;
            cf += 99;
        }
    }
}

extern s32 gCameraWindowLeftLimit;
extern s32 gCameraWindowRightLimit;
extern s32 gCameraWindowTopLimit;
extern s32 gCameraWindowBottomLimit;
extern f32 gCameraWindowScaleX;
extern f32 gCameraWindowScaleY;

extern s32 lbl_803444AC, lbl_803444B0, lbl_803444B4, lbl_803444B8, lbl_803444BC;

void ChangeWindow(void)
{
    f32 halfY;
    f32 halfX;
    s32 centerX = (s32)(lbl_80345F18 *
        (f64)(gCameraWindowRightLimit + gCameraWindowLeftLimit));
    s32 centerY = (s32)(lbl_80345F18 *
        (f64)(gCameraWindowTopLimit + gCameraWindowBottomLimit));

    halfX = (f32)(s32)(lbl_80345F18 *
        (f64)(gCameraWindowRightLimit - gCameraWindowLeftLimit)) *
        gCameraWindowScaleY;
    halfY = (f32)(s32)(lbl_80345F18 *
        (f64)(gCameraWindowTopLimit - gCameraWindowBottomLimit)) *
        gCameraWindowScaleX;
    lbl_803444AC = (s32)((f32)centerX - halfX);
    lbl_803444B0 = (s32)((f32)centerX + halfX);
    lbl_803444B4 = (s32)((f32)centerY + halfY);
    lbl_803444B8 = (s32)((f32)centerY - halfY);
    if (lbl_803444AC < gCameraWindowLeftLimit) {
        lbl_803444AC = gCameraWindowLeftLimit;
    }
    if (gCameraWindowRightLimit < lbl_803444B0) {
        lbl_803444B0 = gCameraWindowRightLimit;
    }
    if (gCameraWindowTopLimit < lbl_803444B4) {
        lbl_803444B4 = gCameraWindowTopLimit;
    }
    if (lbl_803444B8 < gCameraWindowBottomLimit) {
        lbl_803444B8 = gCameraWindowBottomLimit;
    }
    MBWindowSetRegion((f32)lbl_803444AC, (f32)lbl_803444B0,
        (f32)lbl_803444B4, (f32)lbl_803444B8, (f32)lbl_803444BC);
}

void CopyCam(u8* src, u8* dst)
{
    CopyMat4((f32*)(src + 4), (f32*)(dst + 4));
#define CF(o) *(f32*)(dst + (o)) = *(f32*)(src + (o))
#define CI(o) *(u32*)(dst + (o)) = *(u32*)(src + (o))
    CF(0x64); CF(0x68); CF(0x6C); CF(0x74); CF(0x78); CF(0x7C);
    CF(0x84); CF(0x88); CF(0x8C); CF(0x94); CF(0x98); CF(0x9C);
    CF(0xA4); CF(0xA8); CF(0xAC); CF(0xB4); CF(0xB8); CF(0xBC);
    CI(0xD8); CF(0xC4); CI(0xC8); CI(0xCC); CI(0xD0); CI(0xD4);
    CF(0xDC); CF(0xE0); CF(0xE4); CF(0xE8);
    CI(0xEC); CI(0xF0); CI(0xF4); CI(0xF8); CI(0xFC); CI(0x100);
    CI(0x104); CI(0x108); CI(0x10C); CI(0x110); CI(0x114); CI(0x118);
    CF(0x11C); CF(0x120); CF(0x124); CF(0x12C); CF(0x130); CF(0x134);
    CF(0x13C); CF(0x140); CF(0x144); CF(0x14C); CF(0x150); CF(0x154);
    CF(0x15C); CF(0x160); CF(0x164); CF(0x16C); CF(0x170); CF(0x174);
    CF(0x17C); CF(0x180); CF(0x184);
#undef CF
#undef CI
}

void ProcCamera_8002E548(s32 camIdx, s32 useRecorderPosition)
{
    Camera* cam = &gCameras[camIdx];
    f32 offset[3];

    if (cam->state == 0) {
        return;
    }
    if (cam->a_mode == ATN_FREE) {
        CreateYPRMatrix(&cam->mat[0][0], cam->pyr);
    }
    if (gGameBusy == 0 && gGameplayPauseTimer == 0) {
        WorldVector(cam->vel, offset, &cam->mat[0][0]);
        cam->wpos[0] += offset[0];
        cam->wpos[1] += offset[1];
        cam->wpos[2] += offset[2];
        WorldVector(cam->avel, offset, &cam->mat[0][0]);
        cam->attn[0] += offset[0];
        cam->attn[1] += offset[1];
        cam->attn[2] += offset[2];
    }
    if (useRecorderPosition != 0 && camIdx == 0 && cam->c_mode == CAM_GAME) {
        cam->mat[3][0] = gRecorderCameraPosition[0];
        cam->mat[3][1] = gRecorderCameraPosition[1];
        cam->mat[3][2] = gRecorderCameraPosition[2];
    } else {
        cam->mat[3][0] = cam->wpos[0];
        cam->mat[3][1] = cam->wpos[1];
        cam->mat[3][2] = cam->wpos[2];
    }
}

/*
 * screen_limitation -- keep the active camera inside its configured world
 * limits.  The debug build also printed selectable object information here;
 * clamping is the runtime-relevant part of the routine.
 */
extern s32 sFlags;
extern f64 lbl_803460C0, lbl_803460C8;
void fn_800C02F4(s32 color);

/*
 * screen_limitation -- debug overlay printing camera camIdx's world position,
 * attention, orientation, distance and its camera-/attention-mode names.  Gated
 * by the debug flag (sFlags & 1); no effect in a normal build.
 */
void screen_limitation(s32 camIdx)
{
    s32 c = lbl_8034453C;
    Camera* cam = &gCameras[c];
    f32* wpos = (f32*)((u8*)cam + 0x64);
    f32* attn = (f32*)((u8*)cam + 0x12C);
    s32 aMode = *(s32*)((u8*)cam + 0xF8);
    s32 cMode = *(s32*)((u8*)cam + 0xEC);
    s32 row;
    f32 yaw;
    f64 yawDeg;
    s32 pitch;

    if ((sFlags & 1) == 0) {
        return;
    }
    row = MBScreenHeight() / 8;
    MBScreenWidth();
    if (row != 0x20) {
        row -= 2;
    }
    yaw = FixAngle((f32)(lbl_80345F60 - (f64)*(f32*)((u8*)cam + 0xA8)));
    yawDeg = (f64)(f32)(lbl_803460B0 * lbl_803460B8 * (f64)yaw);
    if (yawDeg < (f64)lbl_80345EC8) {
        yawDeg = (f64)(f32)(yawDeg + lbl_803460C0);
    }
    if (lbl_803460C8 < yawDeg) {
        yawDeg = (f64)lbl_80345EC8;
    }
    pitch = (s32)(lbl_803460B0 * lbl_803460B8 * (f64)*(f32*)((u8*)cam + 0xA4));
    fn_800C02F4(0xFF00);
    dbgTextPrintfCol(wpos[0], wpos[1], wpos[2], 1, row - 0xC,
                     "CAM %2f/%2f/%2f", wpos[0], wpos[1], wpos[2]);
    dbgTextPrintfCol(attn[0], attn[1], attn[2], 1, row - 0xB,
                     "ATN %2f/%2f/%2f", attn[0], attn[1], attn[2]);
    dbgTextPrintfCol(yawDeg, *(f32*)((u8*)cam + 0xA8), attn[2], 1, row - 0xA,
                     "YAW %1f/%2f", yawDeg);
    dbgTextPrintfCol(lbl_803460B8, lbl_803460B0, attn[2], 1, row - 9,
                     "PITCH %d", pitch);
    dbgTextPrintfCol(*(f32*)((u8*)cam + 0xC4), lbl_803460B0, attn[2], 1, row - 8,
                     "DISTANCE %2f", *(f32*)((u8*)cam + 0xC4));
    switch (cMode) {
    case 0:  dbgTextPrintfCol(0, 0, 0, 5, row - 7, "OFF"); break;
    case 1:  dbgTextPrintfCol(0, 0, 0, 5, row - 7, "FREE"); break;
    case 2:  dbgTextPrintfCol(0, 0, 0, 5, row - 7, "LOCK"); break;
    case 3:  dbgTextPrintfCol(0, 0, 0, 5, row - 7, "GAME"); break;
    case 4:  dbgTextPrintfCol(0, 0, 0, 5, row - 7, "TETHER"); break;
    case 5:  dbgTextPrintfCol(0, 0, 0, 5, row - 7, "VECDIST"); break;
    case 6:  dbgTextPrintfCol(0, 0, 0, 5, row - 7, "POINT"); break;
    case 7:  dbgTextPrintfCol(0, 0, 0, 5, row - 7, "DRAGON"); break;
    case 8:  dbgTextPrintfCol(0, 0, 0, 5, row - 7, "CHIMERA"); break;
    case 9:  dbgTextPrintfCol(0, 0, 0, 5, row - 7, "GENIE"); break;
    case 10: dbgTextPrintfCol(0, 0, 0, 5, row - 7, "DRIDER"); break;
    case 11: dbgTextPrintfCol(0, 0, 0, 5, row - 7, "DEMON"); break;
    case 12: dbgTextPrintfCol(0, 0, 0, 5, row - 7, "BOSS"); break;
    default: dbgTextPrintfCol(0, 0, 0, 5, row - 7, "UNKNOWN"); break;
    }
    switch (aMode) {
    case 0:  dbgTextPrintfCol(0, 0, 0, 0x12, row - 7, "FREE"); break;
    case 1:  dbgTextPrintfCol(0, 0, 0, 0x12, row - 7, "LOCK"); break;
    case 2:  dbgTextPrintfCol(0, 0, 0, 0x12, row - 7, "TARGET"); break;
    case 3:  dbgTextPrintfCol(0, 0, 0, 0x12, row - 7, "OBJECT"); break;
    case 4:  dbgTextPrintfCol(0, 0, 0, 0x12, row - 7, "POINT"); break;
    case 5:  dbgTextPrintfCol(0, 0, 0, 0x12, row - 7, "PLAYER %02X",
                              *(s32*)((u8*)cam + 0x104)); break;
    case 6:  dbgTextPrintfCol(0, 0, 0, 0x12, row - 7, "ENEMY %02X",
                              *(s32*)((u8*)cam + 0x108)); break;
    case 7:  dbgTextPrintfCol(0, 0, 0, 0x12, row - 7, "ITEM %02X",
                              *(s32*)((u8*)cam + 0x10C)); break;
    case 8:  dbgTextPrintfCol(0, 0, 0, 0x12, row - 7, "MILESTONE %02X",
                              *(s32*)((u8*)cam + 0x110)); break;
    case 9:  dbgTextPrintfCol(0, 0, 0, 0x12, row - 7, "LOOKOUT %02X",
                              *(s32*)((u8*)cam + 0x114)); break;
    case 10: dbgTextPrintfCol(0, 0, 0, 0x12, row - 7, "CAMERA %02X",
                              *(s32*)((u8*)cam + 0x118)); break;
    default: dbgTextPrintfCol(0, 0, 0, 0x12, row - 7, "UNKNOWN"); break;
    }
    dbgTextPrintfCol(0, 0, 0, 1, row - 6, "MODE %d", *(s32*)((u8*)cam + 0xD0));
    fn_800C02F4(-1);
}

void ResetClock(void)
{
    gClockFrameNumber = 0;
    sMusicFadeBase = 0.0f;
    gClockTime = 0.0f;
    InfFrame = 0;
    sLastVBlankCounter = 0;
    gClockFrameReciprocal = 0.0f;
    gClockFrameStep = 0.0f;
    gClockPreviousTime = 0.0f;
}

void InitializeClockIRQ(void)
{
    gClockFrameNumber = 0;
    sMusicFadeBase = 0.0f;
    gClockTime = 0.0f;
    InfFrame = 0;
    sLastVBlankCounter = 0;
    gClockFrameReciprocal = 0.0f;
    gClockFrameStep = 0.0f;
    gClockPreviousTime = 0.0f;
}

void ClockOncePerFrame(void)
{
    s32 freeze = 0;
    s32 resumed = 0;

    if ((sFlags & 8) != 0) {
        freeze = 1;
        if ((sPreviousFlags & 8) != 0) {
            sClockAccumulator = 0;
            freeze = 0;
            resumed = 1;
        } else if ((gControllerButtons & 8) != 0) {
            if (sClockAccumulator < 60) {
                sClockAccumulator += gClockStepTicks;
            } else {
                sClockAccumulator -= 4;
                freeze = 0;
            }
        }
    }
    if ((sFlags & 4) != 0 && gGameMode == 0x4010) {
        freeze = 1;
    }

    gClockStepTicks = pbLoad - sLastTimerCount;
    sLastTimerCount = pbLoad;
    gFrameTicks = gClockStepTicks;
    gClockCurrentTime = pbGetTime();
    gClockElapsedTime = gClockCurrentTime - sLastFrameTime;
    sLastFrameTime = gClockCurrentTime;

    if (gGameBusy != 0 || gModalRenderDepth != 0 || (freeze && !resumed)) {
        gFrameTicks = 0;
        if (gGameBusy != 100) {
            gClockElapsedTime = 0;
        }
        gClockFrameStep = 0.0f;
        gClockFrameReciprocal = 1.0f;
    } else if (resumed || gFrameTicks > 60 || gFrameTicks == 0) {
        gFrameTicks = 2;
        gClockElapsedTime = 10000000;
        gClockFrameStep = 2.0f;
        gClockFrameReciprocal = 0.5f;
    } else {
        gClockFrameStep = (f32)gFrameTicks / 2.0f;
        gClockFrameReciprocal = 1.0f / gClockFrameStep;
    }
    if (gClockElapsedTime > 300000000) {
        gClockElapsedTime = 10000000;
    }
    if (!freeze && gGameBusy == 0) {
        sMusicFadeBase += gClockFrameStep;
        if (sMusicFadeBase > 65536.0f) {
            sMusicFadeBase -= 65536.0f;
        }
        InfFrame++;
        gClockTime = sMusicFadeBase;
        gClockFrameNumber = (s32)(60.0f * sMusicFadeBase + 0.5f);
    }
    gClockPreviousTime = sMusicFadeBase;
}

void PlayerDamagedEnemy(void* player, void* enemy, s32 state, s32 damage,
                        s32 flag)
{
    s32 t;

    if (*(s32*)enemy == gBossType && gBossActive == 0) {
        return;
    }
    if (state != 1 && state != 6) {
        return;
    }
    if (damage > 0 && *(s16*)((u8*)enemy + 728) == 0) {
        s32 c = *(s32*)((u8*)player + 2328) + 1;
        *(s32*)((u8*)player + 2328) = c;
        if (c >= 10 && gBossType < 0) {
            msgPost(22, *(s32*)player, (u32)((u8*)player + 84));
        }
    }

    t = *(s32*)enemy;
    if (t == -2) {
        t = 1;
    } else if (t == -3) {
        t = 2;
    } else if (t < 0) {
        t = 0;
    }

    if (flag != 0) {
        if (damage != 0) {
            AddExp(*(s32*)player, lbl_8011BCB8[t], 1);
        } else {
            AddExp(*(s32*)player, lbl_8011BC30[t], 1);
        }
    } else if (damage != 0) {
        AddExp(*(s32*)player, lbl_8011BBA8[t], 0);
    } else {
        AddExp(*(s32*)player, lbl_8011BB20[t], 0);
    }
}

void PlayerDamagedItem(void* player, void* item, s32 flag)
{
    s32* info = *(s32**)item;
    s32 type = info[0];

    if (type == 2) {
        return;
    }
    if (type > 2) {
        if (type < 4) {
            s32 t;
            if (flag != 0) {
                PF(player, 0x918, s32) = 0;
                PF(player, PF(player, 0x0C, s32) * 0x1C + 0xC20, s32) =
                    PF(player, PF(player, 0x0C, s32) * 0x1C + 0xC20, s32) + 1;
            }
            t = PF(item, 0xDC, s16);
            if (t == -2) {
                t = 1;
            } else if (t == -3) {
                t = 2;
            } else if (t < 0) {
                t = 0;
            }
            if (flag == 0) {
                AddExp(PF(player, 0, s32), lbl_8011BB20[t] * 5, 0);
            } else {
                AddExp(PF(player, 0, s32), lbl_8011BBA8[t] * 5, 0);
            }
        }
    } else if (type >= 1) {
        if (PF(item, 0xC6, s16) < 1 && info[1] == 4) {
            s32 pos[3];
            s32 vec[3];
            s32 fx;
            pos[0] = *(s32*)((u8*)item + 0x44);
            pos[1] = *(s32*)((u8*)item + 0x48);
            pos[2] = *(s32*)((u8*)item + 0x4C);
            vec[0] = *(s32*)((u8*)player + 0x54);
            vec[1] = *(s32*)((u8*)player + 0x58);
            vec[2] = *(s32*)((u8*)player + 0x5C);
            fx = start_magic(PF(player, 0, s32), pos, info[0xF], 0,
                             lbl_80346310);
            msgPost(fx, 0xE, PF(player, 0, s32), (u32)vec);
            DeleteItem(item, 1);
        }
    }
}

extern f64 lbl_80346318, lbl_80346320;
extern f32 lbl_8034632C, lbl_80346330, lbl_80346334, lbl_80346338;

void ModifyDamage(f32 armor, f32* damage, u32* damageType, u32 shield)
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
    f32 fromDir[3];
    f32 centerDir[3];
    f32 distance;

    distance = PointLineColl(center, from, to, closest);
    if (distance > radius + halfHeight) {
        return 0;
    }
    delta[0] = closest[0] - center[0];
    delta[1] = closest[1] - center[1];
    delta[2] = closest[2] - center[2];
    if (fqdist(delta[0], delta[2]) > radius ||
        (delta[1] < 0.0f ? -delta[1] : delta[1]) > halfHeight) {
        return 0;
    }

    if (directional != 0) {
        fromDir[0] = center[0] - from[0];
        fromDir[1] = 0.0f;
        fromDir[2] = center[2] - from[2];
        distance = NormalVector2D(fromDir);
        if (distance <= radius) {
            centerDir[0] = to[0] - from[0];
            centerDir[1] = 0.0f;
            centerDir[2] = to[2] - from[2];
            {
                f32 lineLength = NormalVector2D(centerDir);
                if (distance < 0.001f) {
                    if (lineLength < 0.001f) {
                        if (hit != 0) {
                            hit[0] = closest[0];
                            hit[1] = closest[1];
                            hit[2] = closest[2];
                        }
                        return 1;
                    }
                    return 0;
                }
                if (centerDir[0] * fromDir[0] +
                    centerDir[2] * fromDir[2] < -0.01f) {
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
        u8* enemy = gEnemies + i * ENEMY_STRIDE;
        s32 state = PF(enemy, 0xB4, s32);
        if ((state == 1 || state == 6) &&
            (!respectCooldown ||
             PF(enemy, 0x2B4 + cooldownSlot * 4, f32) <= sMusicFadeBase)) {
            f32 enemyRadius = radius + PF(enemy, 0x238, f32);
            f32 enemyHeight = radius + PF(enemy, 0x23C, f32);
            f32 dx = PF(enemy, 0x54, f32) - to[0];
            f32 dy = PF(enemy, 0x58, f32) - to[1];
            f32 dz = PF(enemy, 0x5C, f32) - to[2];
            if (dx * dx + dz * dz <= enemyRadius * enemyRadius + horizontalLen2 &&
                dy <= verticalLen2 + enemyHeight &&
                LineCylinderCollide((f32*)(enemy + 0x54), enemyRadius,
                                    enemyHeight, from, to, hit, 0)) {
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
        u8* player = gPlayers + i * PLAYER_STRIDE;
        if (PF(player, 0xE8, s32) == 1) {
            f32 halfHeight = radius + PF(player, 0x854, f32);
            f32 cylinderRadius = radius + PF(player, 0x850, f32);
            if (LineCylinderCollide((f32*)(player + 0x64),
                                    cylinderRadius, halfHeight,
                                    from, to, hit, 0)) {
                return player;
            }
        }
    }
    return 0;
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
    s32 fx;
    u32 flg;
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
        FatalError(lbl_80111E28, 0x800000);
    }
    if (owner > 0) {
        if (lbl_80274E9C == 1) {
            flg = extraFlags | 0x200F;
        } else if (lbl_80274E9C == 2) {
            flg = extraFlags | 0xF;
        } else {
            flg = extraFlags | 0x20E;
        }
        if ((damageType & 0x100000) != 0) {
            flg &= ~0x4u;
        }
    } else {
        flg = extraFlags | 0x1107;
    }
    if ((f64)desc->angularVelocity[0] == lbl_80346340 &&
        (f64)desc->angularVelocity[1] == lbl_80346340 &&
        (f64)desc->angularVelocity[2] == lbl_80346340) {
        flg |= 0x20000;
    }
    flg |= 0x1000000;
    fx = StartFXTree(missileTree, position, flg, 0x80000, color);
    radius = desc->collisionRadius;
    if ((damageType & 0x2000000) != 0) {
        radius = (f32)((f64)radius * lbl_803463C8);
    }
    fn_80093E50(fx, vel, desc->angularVelocity, desc->weight, radius);
    SfxSetHit(fx, (s16)desc->hitEffect, desc->hitSound, wallSound);
    SfxSetDamage(fx, damageType | desc->damageType, owner, damageMag,
                 desc->hitRadius, lbl_80346328);
    if ((damageType & 0x2000000) != 0) {
        ScaleFX(fx, lbl_803463D0, lbl_803463D0, lbl_803463D0);
    }
    if (owner > 0) {
        s32 vibColor;
        s32 vibIntensity;
        if ((damageType & 0x100000) != 0 && (damageType & 0x2000000) == 0) {
            vibColor = 0xFFFFFF;
            vibIntensity = 64;
        } else {
            u8* pl = (u8*)gPlayers + owner * PLAYER_STRIDE;
            vibColor = lbl_8011A178[*(s32*)(pl - 13144)];
            vibIntensity = lbl_8011A188[*(s32*)(pl - 13140)];
            if ((damageType & 0x2000000) != 0) {
                vibIntensity += 64;
                if ((u32)vibIntensity >= 255) {
                    vibIntensity = 255;
                }
            }
        }
        fn_80093D98(fx, WeaponStreakTex, vibColor, vibIntensity, lbl_80346328,
            *(f32*)((u8*)lbl_80282930[owner - 1] + 0x17C));
    }
    return fx;
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
extern f64 lbl_80346318, lbl_80346368, lbl_80346360, lbl_80346370;
extern f64 lbl_80346378, lbl_80346388, lbl_80346390, lbl_80346398;
extern f32 lbl_80346358, lbl_8034635C, lbl_80346380, lbl_80346334;
extern f32 lbl_80346384, lbl_803447D8;
extern u8 lbl_8011A1B4[];
f32 NormalVector(f32* v);
f32 NormalVector2D(f32* v);
f64 Random(f32 range);
s32 WeaponWallCollide();
s32 fn_8005ED44();
void SfxSetLight();

s32 EnemyStartMissile(void* enemy, f32* launchPos, f32* target, s32 slot)
{
    s32 enemyType = PF(enemy, 0x00, s32);
    MissileInfo* desc = &EnemyMissileInfo[enemyType][slot];
    void* tree = EnemyMissileTree[enemyType][slot];
    f32 dir[3];
    f32 speed;
    f32 invSpeed;
    s32 fx;

    if (tree == 0) {
        ErrorPrintf("ENEMY %d HAS NO MISSILE TYPE %d", enemyType, slot);
        return 0;
    }
    speed = desc->speed * PF(gCurLevel, 0xC4, f32);
    dir[0] = launchPos[0] - target[0];
    invSpeed = (f32)(lbl_80346318 / (f64)speed);
    dir[1] = launchPos[1] - target[1];
    dir[2] = launchPos[2] - target[2];
    if (slot == 2) {
        NormalVector(dir);
    } else {
        f32 spread = slot == 1 ? lbl_80346358 : lbl_8034635C;
        f64 rnd = Random(lbl_80346368);
        f32 lead = (f32)((f64)PF(gCurLevel, 0xC8, f32) *
                         (lbl_80346360 + rnd) + (f64)spread);
        f32 horiz = (f32)fqdist(dir[0], dir[2]);
        f32 hn = horiz > lbl_80346348 ?
                 (f32)(lbl_80346318 / (f64)horiz) : (f32)lbl_80346318;
        dir[0] = (f32)((f64)dir[0] * hn);
        dir[2] = (f32)((f64)dir[2] * hn);
        dir[1] = (f32)((f64)invSpeed *
            (lbl_80346370 * (f64)desc->weight * (f64)(f32)((f64)horiz * invSpeed) +
             (f64)((f32)((f64)dir[1] + (f64)lead) * (f32)((f64)speed * hn))));
    }
    if ((f64)dir[1] < lbl_80346340) {
        dir[1] = lbl_80346328;
    }
    {
        f32 flat[3];
        flat[0] = dir[0];
        flat[1] = dir[1];
        flat[2] = dir[2];
        NormalVector2D(flat);
        if (lbl_80346378 <=
            flat[0] * PF(enemy, 0x24, f32) + flat[2] * PF(enemy, 0x2C, f32)) {
            u32 flags = PF(enemy, 0xC4, u32);
            u32 extraFlags = 0;
            f32 spawn[3];
            f32 aim[3];
            f32 height = slot == 2 ? lbl_80346328 : lbl_80346380;
            aim[0] = target[0];
            aim[2] = target[2];
            switch (enemyType) {
            case 4:
                if (slot == 0) height = lbl_80346334;
                break;
            case 7:
            case 0x18:
                if (slot == 2) height = lbl_80346334;
                break;
            case 0xD:
                if (slot == 0) height = lbl_80346384;
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
                extraFlags = 0x8000000;
                flags |= 0x100000;
                break;
            }
            aim[1] = (f32)((f64)target[1] + (f64)height);
            spawn[0] = (f32)(lbl_80346398 * (f64)dir[0] + (f64)aim[0]);
            spawn[1] = (f32)(lbl_80346398 * (f64)dir[1] + (f64)aim[1]);
            spawn[2] = (f32)(lbl_80346398 * (f64)dir[2] + (f64)aim[2]);
            if (WeaponWallCollide(aim, spawn, 0) == 0 &&
                fn_8005ED44(desc->collisionRadius, aim, spawn, 0, 0, -1) == 0) {
                f32 damage = desc->damage;
                if (lbl_803447D8 < lbl_80346318) {
                    damage = (f32)((f64)damage * lbl_80346370);
                }
                if (slot == 0) flags |= 0x20000;
                if (slot == 2) flags |= 0x40000;
                fx = StartMissile(0, spawn, dir, flags, desc, tree, 0,
                                  extraFlags, speed, damage);
                if (lbl_803447D8 < lbl_80346318) {
                    ScaleFX(fx, lbl_803447D8, lbl_803447D8, lbl_803447D8);
                }
                if (slot == 1) {
                    SfxSetLight(lbl_80346368, fx, lbl_8011A1B4);
                }
                return fx;
            }
        }
    }
    return 0;
}

extern u8 lbl_8011A1A8[];
extern f32 lbl_80111DE0[], lbl_80111DF4[];
extern void* EffectInfo[];
extern void *BossAcidTree, *BossElecTree, *BallistaTree;
extern f32 lbl_80285C20[];
extern s16 lbl_80285CA0[];
extern s32* lbl_80285BD0[];
extern s32* lbl_80285BCC[];
extern f32 lbl_803463A0, lbl_803463B0, lbl_803463A8, lbl_803463B8;
void MulVecMat3(f32* in, f32* out, f32* mat);
s32 fn_80094080();
f64 fn_8005C1DC();
s32 fn_80094440();
s32 StartFXSub();
void DeleteEffect();
void MBNodeSetParent(s32 a, s32 b);
void MBTreeSetFlags(s32 node, s32 mask, s32 value);

/*
 * PlayerStartMissile -- launch a player weapon.  Derives the aim direction
 * from the player's facing (or weapon object), applies wall/target aim-assist,
 * then spawns up to five missiles in the weapon's spread pattern, linking and
 * lighting each spawned effect.  Returns the primary effect handle.
 */
void PlayerStartMissile(s32* player, f32* direction, u32 damageType, s32 mode,
                        f32 scaleArg)
{
    s32 idx = player[0];
    u32 pflags = (u32)player[0x49];
    u32 trailFx = PlayerMissileTreeInfo[idx].throwFlags;
    u32 special = pflags & 0x8000;
    u32 dmgLow = damageType & 0xF;
    u32 extraFlags = 0;
    s32 useSpecial = 0;
    MissileInfo* desc;
    f32 aim[3];
    f32 pt0[3];
    f32 pt1[3];
    f32 wallHit[3];
    f32 spreadA[5];
    f32 spreadB[5];
    f32 launchDir[3];
    f32 scale;
    f32 scale2;
    f32 invSpeed;
    void* tree;
    s32 j;
    s32 i;

    if (special == 0) {
        if ((pflags & 0x4000) == 0) {
            if ((damageType & 0x100000) == 0 || (damageType & 0x2000000) != 0) {
                desc = &PlayerMissileInfo[player[2]];
            } else {
                desc = &BallistaMissileInfo;
                useSpecial = 1;
            }
        } else {
            desc = &BossAcidMissileInfo;
            useSpecial = 1;
        }
    } else {
        desc = &BossElecMissileInfo;
        useSpecial = 1;
    }
    scale = *(f32*)((u8*)player + 0x114) * scaleArg;
    scale2 = scale;

    if ((pflags & 0x400) == 0) {
        if (mode == 1) {
            MulVecMat3((f32*)((u8*)lbl_80282930[idx] + 0x5C), aim,
                       (f32*)(player + 5));
        } else if (mode == 2) {
            MulVecMat3((f32*)((u8*)lbl_80282930[idx] + 0x158), aim,
                       (f32*)(player + 5));
        } else {
            aim[0] = lbl_80346328;
            aim[1] = lbl_80346328;
            aim[2] = lbl_80346328;
        }
    } else {
        MulVecMat3((f32*)lbl_8011A1A8, aim, (f32*)(player + 5));
    }
    aim[0] = *(f32*)((u8*)player + 0x64) + aim[0];
    aim[1] = *(f32*)((u8*)player + 0x68) + aim[1];
    aim[2] = *(f32*)((u8*)player + 0x6C) + aim[2];
    pt0[0] = (f32)(lbl_803463A0 * (f64)direction[0] + (f64)aim[0]);
    pt0[1] = (f32)(lbl_803463A0 * (f64)direction[1] + (f64)aim[1]);
    pt0[2] = (f32)(lbl_803463A0 * (f64)direction[2] + (f64)aim[2]);
    pt1[0] = (f32)(lbl_80346388 * (f64)direction[0] + (f64)aim[0]);
    pt1[1] = (f32)(lbl_80346388 * (f64)direction[1] + (f64)aim[1]);
    pt1[2] = (f32)(lbl_80346388 * (f64)direction[2] + (f64)aim[2]);

    if ((damageType & 0x100000) == 0) {
        if (WeaponWallCollide(pt0, pt1, wallHit) != 0) {
            if ((damageType & 0x200000) == 0) {
                fn_80094080(wallHit, damageType, wallHit, mode, special);
                return;
            }
            pt1[0] = pt0[0];
            pt1[1] = pt0[1];
            pt1[2] = pt0[2];
        }
    }
    {
        s32* hit = (s32*)fn_8005ED44(desc->collisionRadius, pt0, pt1, wallHit,
                                     0, player[0]);
        if (hit != 0) {
            f64 dmg = fn_8005C1DC(scale, hit, damageType, player[0]);
            if (dmg < lbl_80346340) {
                fn_80094080(wallHit, damageType, player[0], 0);
            } else {
                u32 killed = lbl_80346340 == dmg;
                PlayerDamagedItem(player, hit, killed);
                fn_80094440(wallHit, damageType, killed);
            }
            if ((damageType & 0x100000) == 0) {
                return;
            }
        }
    }

    {
        f32 aimSpeed = *(f32*)((u8*)player + 0x118);
        f32 clamped = (f32)lbl_80346318;
        if ((f64)lbl_80346318 <= (f64)aimSpeed) {
            clamped = aimSpeed;
            if (lbl_803463A8 < (f64)aimSpeed) {
                clamped = (f32)lbl_803463A8;
            }
        }
        invSpeed = (f32)(lbl_80346318 / (f64)clamped);
        if ((damageType & 0x100000) == 0 && lbl_80346340 < (f64)scale2) {
            f32 horiz;
            f32 hn;
            direction[0] = (f32)((f64)direction[0] * (f64)scale2);
            direction[1] = (f32)((f64)direction[1] * (f64)scale2);
            direction[2] = (f32)((f64)direction[2] * (f64)scale2);
            horiz = (f32)fqdist(direction[0], direction[2]);
            hn = (f32)lbl_80346318;
            if (lbl_80346348 < (f64)horiz) {
                hn = (f32)(lbl_80346318 / (f64)horiz);
            }
            direction[0] = (f32)((f64)direction[0] * (f64)hn);
            direction[2] = (f32)((f64)direction[2] * (f64)hn);
            direction[1] = (f32)((f64)invSpeed *
                (lbl_80346370 * (f64)desc->weight *
                    (f64)(f32)((f64)horiz * (f64)invSpeed) +
                 (f64)((direction[1] + lbl_803463B0) *
                    (f32)((f64)clamped * (f64)hn))));
        }

        {
            u32 f = (u32)player[0x49];
            if ((f & 0x8000) == 0) {
                if ((f & 0x4000) == 0) {
                    if ((f & 0x400) == 0) {
                        if ((damageType & 0x100000) == 0 ||
                            (damageType & 0x2000000) != 0) {
                            tree = PlayerMissileTreeInfo[idx].throwHeader;
                        } else {
                            tree = BallistaTree;
                        }
                    } else {
                        extraFlags = 0x10000;
                        tree = PhoenixTree;
                    }
                } else {
                    extraFlags = 0x10000;
                    tree = BossAcidTree;
                }
            } else {
                extraFlags = 0x10000;
                tree = BossElecTree;
            }
        }

        {
        f64 c340 = lbl_80346340;
        f64 c318 = lbl_80346318;
        f64 c3B8 = lbl_803463B8;
        for (i = 0; i < 5; i++) {
            s32 fx;
            for (j = 0; j < 5; j++) {
                spreadB[j] = lbl_80111DE0[j];
            }
            for (j = 0; j < 5; j++) {
                spreadA[j] = lbl_80111DF4[j];
            }
            if ((i > 0 && (damageType & 0x480000) == 0) ||
                (i > 2 && (damageType & 0x400000) == 0)) {
                break;
            }
            launchDir[1] = direction[1];
            launchDir[0] = direction[0] * spreadB[i] +
                           (f32)((f64)direction[2] * (f64)spreadA[i]);
            launchDir[2] = (f32)(-(f64)direction[0] * (f64)spreadA[i] +
                                 (f64)(direction[2] * spreadB[i]));
            if ((player[2] == 2 || player[2] == 6) &&
                WeapThrowFx[player[0]][dmgLow] >= 0 &&
                !useSpecial) {
                tree = EffectInfo[0];
            }
            fx = StartMissile(player[0] + 1, pt1, launchDir, damageType, desc,
                              tree, i, extraFlags, clamped, scale);
            if (fx >= 0) {
                if (!useSpecial) {
                    s32 tw = WeapThrowFx[player[0]][dmgLow];
                    if (tw >= 0) {
                        u32 fxFlags = 0x81880;
                        s32 sub;
                        if (dmgLow == 1) {
                            fxFlags |= 0x800000;
                        }
                        if ((player[2] == 2 || player[2] == 6) && dmgLow == 0) {
                            fxFlags |= 0x3000000;
                        }
                        sub = StartFXSub(tw, 0, 0, fxFlags, desc, tree, i,
                                         extraFlags, lbl_80346328, scale);
                        if (sub == fx) {
                            DeleteEffect(sub, 1);
                            fx = -1;
                        } else if (c340 < (f64)lbl_80285C20[fx * 0x3C]) {
                            if (sub >= 0) {
                                lbl_80285CA0[fx * 0x78] = (s16)sub;
                                MBNodeSetParent(*lbl_80285BD0[sub * 0x3C],
                                                *lbl_80285BD0[fx * 0x3C]);
                            }
                        } else {
                            DeleteEffect(fx, 1);
                            DeleteEffect(sub, 1);
                            fx = -1;
                        }
                    }
                }
                if (fx >= 0) {
                    f32 scaleFx = lbl_80346384;
                    if (trailFx != 0) {
                        MBTreeSetFlags(*(s32*)((u8*)lbl_80285BCC[fx * 0x3C] +
                                       0x78), trailFx, 2);
                    }
                    if (c318 < (f64)scaleArg) {
                        scaleFx = (f32)((f64)scaleFx * (f64)scaleArg);
                    }
                    if (0x62 < player[0xCC9]) {
                        scaleFx = (f32)((f64)scaleFx * c3B8);
                    }
                    if (c318 != (f64)scaleFx) {
                        ScaleFX(scaleFx, scaleFx, scaleFx);
                    }
                }
            }
            pmissile_sfxidx[i] = fx;
        }
        }
    }
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

extern void *lbl_802753FC[], *lbl_802753E4[];
extern void *BallistaTree, *BossElecTree, *BossAcidTree, *lbl_803445B0;
extern void *WingsTree, *PojoTree, *BreatheFireTree, *BreatheElecTree;
extern void *BreatheAcidTree, *FireShieldTree, *PhoenixTree;
extern void *sWeaponsBuf, *sPowerupsBuf;
extern char lbl_803463EC[];
void* MBOX_FindTexture(char* name, void* arg);
s32 InitCustomEffect(void* tree, char* name, s32 zmod, s32 alpha);

void InitPlayerMissiles(void* player)
{
    s32 idx = PF(player, 0x00, s32);
    s32 charType = PF(player, 0x0C, s32);
    char* charName = PlayerMissileDesc[charType].throwDescription;
    u8 throwByte = *(u8*)(charName + PF(player, 0x3324, s32) / 10 + 4);
    void* weaponWad = lbl_802753FC[idx * 0x13];
    void* powerupWad = lbl_802753E4[idx * 0x13];
    char buf[24];
    s32 missing;
    s32 i;

    if (throwByte == 0x30) {
        sprintf(buf, "%s_THROW0", charName);
        PlayerMissileTreeInfo[idx].throwHeader = AtreeMatch(powerupWad, buf, 0);
    } else {
        sprintf(buf, "%s_THROW_%c", charName, (char)throwByte);
        PlayerMissileTreeInfo[idx].throwHeader = AtreeMatch(weaponWad, buf, 0);
    }
    if (PlayerMissileTreeInfo[idx].throwHeader == 0) {
        sprintf(buf, "%s_THROW1", charName);
        PlayerMissileTreeInfo[idx].throwHeader = AtreeMatch(powerupWad, buf, 0);
    }
    missing = PlayerMissileTreeInfo[idx].throwHeader == 0;
    if (missing) {
        ErrorPrintf("Player Missile not found: %s", buf);
    }
    PlayerMissileTreeInfo[idx].throwFlags = PlayerMissileDesc[charType].flags;

    for (i = 0; i < 5; i++) {
        char* name = DmgTypeDesc[i];
        if (name[0] == '\0') {
            WeapHoldFxTree[idx][i] = 0;
            WeapThrowFx[idx][i] = -1;
        } else {
            sprintf(buf, "WEAP_HOLD_%s", name);
            WeapHoldFxTree[idx][i] = AtreeMatch(weaponWad, buf, 1);
            sprintf(buf, "WEAP_TW_%c", name[0]);
            WeapThrowFx[idx][i] = InitCustomEffect(weaponWad, buf, 0, 0);
        }
    }
    WeaponStreakTex = (s32)MBOX_FindTexture("WEP_STREAK", 0);
    BallistaTree = AtreeMatch(sWeaponsBuf, "SUPERARROW", 1);
    BossElecTree = AtreeMatch(sWeaponsBuf, "BOSSG_ELEC", 1);
    BossAcidTree = AtreeMatch(sWeaponsBuf, "BOSSG_ACID", 1);
    if (sPowerupsBuf == 0) {
        lbl_803445B0 = 0;
        WingsTree = 0;
        PojoTree = 0;
        BreatheFireTree = 0;
        BreatheElecTree = 0;
        BreatheAcidTree = 0;
    } else {
        lbl_803445B0 = AtreeMatch(sPowerupsBuf, "PHOENIX", 1);
        WingsTree = AtreeMatch(sPowerupsBuf, "WINGS", 1);
        PojoTree = AtreeMatch(sPowerupsBuf, lbl_803463EC, 1);
        BreatheFireTree = AtreeMatch(sPowerupsBuf, "HEAD_BREATHEF", 1);
        BreatheElecTree = AtreeMatch(sPowerupsBuf, "HEAD_BREATHEE", 1);
        BreatheAcidTree = AtreeMatch(sPowerupsBuf, "HEAD_BREATHEA", 1);
    }
    FireShieldTree = AtreeMatch(sWeaponsBuf, "FW_SHLD_ACTIVE", 1);
    FamiliarTree[idx][0] = AtreeMatch(weaponWad, "FAMILIAR1", 1);
    FamiliarTree[idx][1] = AtreeMatch(weaponWad, "FAMILIAR2", 1);
    FamiliarSpit[idx] = AtreeMatch(weaponWad, "FAMILIAR_SPIT", 1);
    PhoenixTree = AtreeMatch(sWeaponsBuf, "PHOENIX_FBALL", 1);
    if (missing) {
        FatalError("InitPlayerMissiles failed!", 0x800000);
    }
}

void ResetPlayerMissiles(void)
{
    memset(PlayerMissileTreeInfo, 0, sizeof(PlayerMissileTreeInfo));
}
