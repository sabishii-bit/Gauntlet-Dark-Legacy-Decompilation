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

s32 pmissile_sfxidx[5];
s32 WeapThrowFx[4][5];
void* WeapHoldFxTree[4][5];
void* FamiliarSpit[4];
void* PhoenixTree;
void* FamiliarTree[4][2];
void* EnemyMissileTree[28][3];
MissileTreeInfo PlayerMissileTreeInfo[4];
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
extern f64 lbl_80346348, lbl_80346350, lbl_80346340, lbl_803463C8;
extern char lbl_80111E28[];
extern s32 optionsAudioAndPrefs30[8];
extern s32 WeaponStreakTex;
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
    register f32* state5 = camState + 5;
    f32 prevYaw = cam->pyr[1];
    f32 rate;
    f32 curYaw;
    f64 y;
    u8 unused[16];

    camState[6] = camState[5];
    camState[5] = camState[4];
    camState[4] = prevYaw;
    CameraSupervisor(camIdx);
    rate = lbl_8034444C * (f32)(u32)gFrameTicks;

    if (lbl_80344400 > 0 && cam->pyr[1] != lbl_80344534) {
        cam->pyr[1] = cam->pyr[1] + rate;
        y = (f32)cam->pyr[1];
        if (y > lbl_80345F58) {
            y = y - lbl_80345F60;
        } else if (y <= lbl_80345F68) {
            y = lbl_80345F60 + y;
        }
        cam->pyr[1] = (f32)y;
        curYaw = cam->pyr[1];
        if (prevYaw > curYaw) {
            if (lbl_80344534 > prevYaw ||
                lbl_80344534 <= curYaw) {
                cam->pyr[1] = lbl_80344534;
                lbl_80344400 = 0;
            }
        } else if (curYaw - lbl_80344534 < lbl_80345F58 &&
                   curYaw >= lbl_80344534) {
            cam->pyr[1] = lbl_80344534;
            lbl_80344400 = 0;
        }
    } else {
        if (lbl_80344400 < 0 && cam->pyr[1] != lbl_80344534) {
            cam->pyr[1] = cam->pyr[1] - rate;
            y = (f32)cam->pyr[1];
            if (y > lbl_80345F58) {
                y = y - lbl_80345F60;
            } else if (y <= lbl_80345F68) {
                y = lbl_80345F60 + y;
            }
            cam->pyr[1] = (f32)y;
            curYaw = cam->pyr[1];
            if (prevYaw < curYaw) {
                if (lbl_80344534 < prevYaw ||
                    lbl_80344534 >= curYaw) {
                    cam->pyr[1] = lbl_80344534;
                    lbl_80344400 = 0;
                }
            } else if (lbl_80344534 - curYaw < lbl_80345F58 &&
                       curYaw <= lbl_80344534) {
                cam->pyr[1] = lbl_80344534;
                lbl_80344400 = 0;
            }
        } else {
            cam->pyr[1] = lbl_80344534;
            lbl_80344400 = 0;
        }
    }
    if ((cam->pyr[1] > lbl_80345EC8 && *state5 > lbl_80345EC8 &&
         camState[4] < lbl_80345EC8) ||
        (cam->pyr[1] < lbl_80345EC8 && *state5 < lbl_80345EC8 &&
         camState[4] > lbl_80345EC8)) {
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
extern f32 lbl_80346030;
extern f64 lbl_803460E8, lbl_803460F8, lbl_803460D8;
extern f64 lbl_80346108, lbl_80346118;
extern f32 lbl_8034445C, lbl_80344454, lbl_80344450, lbl_80344458;
extern f32 lbl_80346100, lbl_80346110, lbl_80344530, lbl_80344408;
extern f64 lbl_80345FE0;
extern s32 lbl_80344510, lbl_8034450C, sNumTriggerCameras, lbl_8034429C, lbl_80344404;

#define TC_X(i) (*(f32*)(sTriggerCameras + (i) * 0x28 + 4))
#define TC_Y(i) (*(f32*)(sTriggerCameras + (i) * 0x28 + 8))
#define TC_Z(i) (*(f32*)(sTriggerCameras + (i) * 0x28 + 0xC))

/* Address-taken roots, absolute-value temporaries, and closest-point output.
 * Their order is fixed by CameraSupervisor's target stack accesses. */
typedef struct CombatCameraSupervisorScratch {
    u8 _pad00[0x20];
    f32 pitchRateDelta;
    f32 pitchRate;
    f32 yawRateDelta;
    f32 yawRate;
    volatile f32 selectedRoot;
    volatile f32 projectedRoot;
    volatile f32 segmentRoot;
    volatile f32 candidateRoot;
    u8 _pad40[12];
    f32 closest[3];
} CombatCameraSupervisorScratch;

/*
 * CameraSupervisor -- trigger-camera (rail) selector for camera camIdx.  Finds
 * the two nearest active rail nodes, blends between them along the segment,
 * and drives the target yaw/pitch and their approach rates.
 */
void CameraSupervisor(s32 camIdx)
{
    Camera* cam = &gCameras[camIdx];
    s32 oldSelected = lbl_80344508;
    s32 oldNearest = lbl_80344510;
    s32 count = 0;
    s32 index = 0;
    s32 offset = 0;
    s32 remaining = sNumTriggerCameras;
    s32 nearest;
    s32 second;
    u8* nearestTrigger;
    u8* secondTrigger;
    f32 nearestDistance = lbl_80346030;
    f32 secondDistance = nearestDistance;
    f32 nearestYaw = lbl_80345EC8;
    f32 secondYaw = nearestYaw;
    f32 nearestPitch = nearestYaw;
    f32 secondPitch = nearestYaw;
    f32 distance;
    f32 combinedDistance;
    f32 segmentLength;
    f32 projectedDistance;
    f32 projectedRatio;
    f32 selectedDistance;
    f32 rateDelta;
    CombatCameraSupervisorScratch scratch;
    f64 root;

    for (; index < remaining; index++, offset += 0x28) {
        if (sTriggerCameras[offset] == 1 &&
            *(s16*)(sTriggerCameras + offset + 2) != 0) {
                f32 dy = cam->wpos[1] -
                    *(f32*)(sTriggerCameras + offset + 8);
                f32 dx = cam->wpos[0] -
                    *(f32*)(sTriggerCameras + offset + 4);
                f32 dz = cam->wpos[2] -
                    *(f32*)(sTriggerCameras + offset + 0xC);

                distance = dy * dy;
                distance = dx * dx + distance;
                distance = dz * dz + distance;
                if ((f64)distance > (f64)lbl_80345EC8) {
                    root = __frsqrte(distance);
                    root = lbl_80345F18 * root *
                           -(root * root * distance - lbl_80345F20);
                    root = lbl_80345F18 * root *
                           -(root * root * distance - lbl_80345F20);
                    root = lbl_80345F18 * root *
                           -(root * root * distance - lbl_80345F20);
                    scratch.candidateRoot =
                        (f32)(distance * (lbl_80345F18 * root *
                        -(root * root * distance - lbl_80345F20)));
                    distance = scratch.candidateRoot;
                }

                if (distance < nearestDistance) {
                    count++;
                    lbl_8034450C = lbl_80344510;
                    secondDistance = nearestDistance;
                    secondYaw = nearestYaw;
                    secondPitch = nearestPitch;
                    lbl_80344510 = index;
                    nearestDistance = distance;
                    nearestYaw = *(f32*)(sTriggerCameras + offset + 0x18);
                    nearestPitch = *(f32*)(sTriggerCameras + offset + 0x14);
                } else if (distance < secondDistance) {
                    lbl_8034450C = index;
                    secondDistance = distance;
                    count++;
                    secondYaw = *(f32*)(sTriggerCameras + offset + 0x18);
                    secondPitch = *(f32*)(sTriggerCameras + offset + 0x14);
                }
            }
        }

    if (count == 1) {
        lbl_8034450C = lbl_80344510;
        secondDistance = nearestDistance;
        secondYaw = nearestYaw;
        secondPitch = nearestPitch;
    }
    if (count == 0) {
        goto deactivate_previous;
    }

    nearest = lbl_80344510;
    second = lbl_8034450C;
    combinedDistance = nearestDistance + secondDistance;
    nearestTrigger = sTriggerCameras + nearest * 0x28;
    secondTrigger = sTriggerCameras + second * 0x28;
    if (count == 1 || (f64)combinedDistance == lbl_80345F78) {
        lbl_8034429C += gFrameTicks;
        return;
    }

    {
        f32 sx;
        f32 sy;
        f32 sz;

        PointLineColl(&cam->wpos[0], (f32*)(nearestTrigger + 4),
            (f32*)(secondTrigger + 4), scratch.closest);

        sy = *(f32*)(nearestTrigger + 8) - *(f32*)(secondTrigger + 8);
        sx = *(f32*)(nearestTrigger + 4) - *(f32*)(secondTrigger + 4);
        sz = *(f32*)(nearestTrigger + 0xC) -
             *(f32*)(secondTrigger + 0xC);
        segmentLength = sy * sy;
        segmentLength = sx * sx + segmentLength;
        segmentLength = sz * sz + segmentLength;
        if ((f64)segmentLength > (f64)lbl_80345EC8) {
            root = __frsqrte(segmentLength);
            root = lbl_80345F18 * root *
                   -(root * root * segmentLength - lbl_80345F20);
            root = lbl_80345F18 * root *
                   -(root * root * segmentLength - lbl_80345F20);
            root = lbl_80345F18 * root *
                   -(root * root * segmentLength - lbl_80345F20);
            scratch.segmentRoot =
                (f32)(segmentLength * (lbl_80345F18 * root *
                -(root * root * segmentLength - lbl_80345F20)));
            segmentLength = scratch.segmentRoot;
        }

        sy = *(f32*)(nearestTrigger + 8) - scratch.closest[1];
        sx = *(f32*)(nearestTrigger + 4) - scratch.closest[0];
        sz = *(f32*)(nearestTrigger + 0xC) - scratch.closest[2];
        projectedDistance = sy * sy;
        projectedDistance = sx * sx + projectedDistance;
        projectedDistance = sz * sz + projectedDistance;
        if ((f64)projectedDistance > (f64)lbl_80345EC8) {
            root = __frsqrte(projectedDistance);
            root = lbl_80345F18 * root *
                   -(root * root * projectedDistance - lbl_80345F20);
            root = lbl_80345F18 * root *
                   -(root * root * projectedDistance - lbl_80345F20);
            root = lbl_80345F18 * root *
                   -(root * root * projectedDistance - lbl_80345F20);
            scratch.projectedRoot =
                (f32)(projectedDistance * (lbl_80345F18 * root *
                -(root * root * projectedDistance - lbl_80345F20)));
            projectedDistance = scratch.projectedRoot;
        }

        lbl_8034445C = nearestDistance / combinedDistance;
        distance = lbl_8034445C;
        projectedRatio = projectedDistance / segmentLength;
        if ((f64)distance >= lbl_80345F28) {
            lbl_8034445C = lbl_80345F80;
        } else if ((f64)distance >= lbl_80346098) {
            lbl_8034445C = (f32)-(lbl_803460E8 *
                (distance - lbl_80345F28) - lbl_80345FE0);
        }

        if ((f64)projectedRatio <= lbl_80345F18) {
            lbl_80344534 = nearestYaw;
            lbl_80344530 = nearestPitch;
            lbl_80344508 = lbl_80344510;
            if (nearestPitch > lbl_80344408) {
                lbl_80344404 = 1;
            } else {
                lbl_80344404 = -1;
            }
        } else if ((f64)projectedRatio > lbl_80345F18) {
            lbl_80344534 = secondYaw;
            lbl_80344530 = secondPitch;
            lbl_80344508 = lbl_8034450C;
            if (secondPitch > lbl_80344408) {
                lbl_80344404 = 1;
            } else {
                lbl_80344404 = -1;
            }
        }

        distance = lbl_80344534 - cam->pyr[1];
        if ((f64)distance < lbl_80345F68) {
            lbl_80344400 = 1;
        } else if ((f64)distance < lbl_80345F78) {
            lbl_80344400 = -1;
        } else if ((f64)distance < lbl_80345F58) {
            lbl_80344400 = 1;
        } else {
            lbl_80344400 = -1;
        }

        if (oldSelected != lbl_80344508) {
            f32 dx = cam->wpos[0] - TC_X(lbl_80344508);
            f32 dy = cam->wpos[1] - TC_Y(lbl_80344508);
            f32 dz = cam->wpos[2] - TC_Z(lbl_80344508);

            selectedDistance = dy * dy;
            selectedDistance = dx * dx + selectedDistance;
            selectedDistance = dz * dz + selectedDistance;
            if ((f64)selectedDistance > (f64)lbl_80345EC8) {
                root = __frsqrte(selectedDistance);
                root = lbl_80345F18 * root *
                       -(root * root * selectedDistance - lbl_80345F20);
                root = lbl_80345F18 * root *
                       -(root * root * selectedDistance - lbl_80345F20);
                root = lbl_80345F18 * root *
                       -(root * root * selectedDistance - lbl_80345F20);
                scratch.selectedRoot =
                    (f32)(selectedDistance * (lbl_80345F18 * root *
                    -(root * root * selectedDistance - lbl_80345F20)));
                selectedDistance = scratch.selectedRoot;
            }

            selectedDistance *= lbl_803460F0;
            if ((f64)selectedDistance != lbl_80345F78) {
                if ((f64)selectedDistance < lbl_80345FE0) {
                    selectedDistance = lbl_80345F80;
                }

                distance = lbl_80344534 - cam->pyr[1];
                if ((f64)distance > lbl_80345F58) {
                    distance = (f32)(lbl_80345F60 - distance);
                }
                scratch.yawRate = distance / selectedDistance;
                *(u32*)&scratch.yawRate &= 0x7FFFFFFF;
                lbl_8034444C = scratch.yawRate;
                if ((f64)scratch.yawRate >= lbl_803460F8) {
                    lbl_8034444C = lbl_80346100;
                }

                rateDelta = lbl_8034444C - lbl_80344454;
                scratch.yawRateDelta = rateDelta;
                *(u32*)&scratch.yawRateDelta &= 0x7FFFFFFF;
                if ((f64)scratch.yawRateDelta >= lbl_803460D8) {
                    if (lbl_8034444C > lbl_80344454) {
                        lbl_8034444C = (f32)(lbl_80344454 + lbl_803460D8);
                    } else {
                        lbl_8034444C = (f32)(lbl_80344454 - lbl_803460D8);
                    }
                }

                scratch.pitchRate =
                    (lbl_80344530 - lbl_80344408) / selectedDistance;
                *(u32*)&scratch.pitchRate &= 0x7FFFFFFF;
                lbl_80344450 = scratch.pitchRate;
                if ((f64)scratch.pitchRate >= lbl_80346108) {
                    lbl_80344450 = lbl_80346110;
                }

                rateDelta = lbl_80344450 - lbl_80344458;
                scratch.pitchRateDelta = rateDelta;
                *(u32*)&scratch.pitchRateDelta &= 0x7FFFFFFF;
                if ((f64)scratch.pitchRateDelta >= lbl_80346118) {
                    if (lbl_80344450 > lbl_80344458) {
                        lbl_80344450 = (f32)(lbl_80344458 + lbl_80346118);
                    } else {
                        lbl_80344450 = (f32)(lbl_80344458 - lbl_80346118);
                    }
                }

                lbl_80344454 = lbl_8034444C;
                lbl_80344458 = lbl_80344450;
            } else {
                lbl_8034444C = lbl_80345EC8;
                lbl_80344454 = lbl_80345EC8;
                lbl_80344450 = lbl_80345EC8;
                lbl_80344458 = lbl_80345EC8;
            }
        }
    }

deactivate_previous:
    if (oldNearest >= 0 && oldNearest == lbl_8034450C) {
        *(s16*)(sTriggerCameras + oldNearest * 0x28 + 2) = 0;
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
    s32 oldMode;
    Camera* cam = &gCameras[camIdx];
    s32 done;

    switch (lbl_803444F0) {
    case 1: {
        u8* p = gPlayers;
        s32 i;
        done = 1;
        for (i = 0; i < 4; i++, p += 13148) {
            if (*(s32*)(p + 232) == 1 && *(s32*)(p + 516) != 1) {
                done = 0;
            }
        }
        break;
    }
    default:
        done = 1;
        break;
    }
    if (done != 0) {
        lbl_803447B8 = 0;
        lbl_803444F0 = -1;
        lbl_803444EC = -1;
        gScriptedCameraState = 0;
        lbl_8034453C = 0;
        oldMode = cam->a_mode;
        if (cam->c_mode != 0) {
            cam->pc_mode = cam->c_mode;
            cam->c_mode = CAM_OFF;
        }
        if (oldMode != cam->a_mode) {
            cam->pa_mode = cam->a_mode;
            cam->a_mode = oldMode;
        }
        cam->state = 0;
        if ((gControllerButtons.both & 4) != 0) {
            sPreviousFlags.both |= 4;
        }
    }
    return done == 0;
}

/* Initialize or advance the game camera's scripted transition. */
extern u8 lbl_80240E30[];
extern f32 lbl_80346138, lbl_80346148;
extern f64 lbl_80345FE0, lbl_80346140;
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
    Camera* cameras = gCameras;
    Camera* cam = &cameras[camIdx];
    s32 prevTimer;
    u8* level = *(u8**)((u8*)gCurLevel + 0x60);
    s32 reached = 2;
    f32 dx, dy, dz;
    f32 len;
    f32 posDistance;
    u8 tail[20];
    volatile f32 posRoot;
    volatile f32 attnRoot;
    u8 unused[12];
    f64 g;
    s32 i;

    if (camIdx != 2) {
        return -1;
    }
    prevTimer = gScriptedCameraState;

    if (gScriptedCameraState > 2) {
        gScriptedCameraState = gScriptedCameraState - gFrameTicks;
        if (gScriptedCameraState < 2) {
            gScriptedCameraState = 2;
        }
        if (gScriptedCameraState < 45) {
            for (i = 0; i < 4; i++) {
                u8* player = (u8*)gPlayers + i * PLAYER_STRIDE;
                if (PF(player, 0xE8, s32) == 1 &&
                    (*(u32*)(lbl_80240E30 + i * 0x3C + 8) & 0x20000FF) != 0) {
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
        dy = cameras[0].wpos[1] - cam[0].wpos[1];
        dx = cameras[0].wpos[0] - cam[0].wpos[0];
        dz = cameras[0].wpos[2] - cam[0].wpos[2];
        len = dy * dy;
        len = dx * dx + len;
        len = dz * dz + len;
        if (len > lbl_80345EC8) {
            g = __frsqrte((f64)len);
            g = lbl_80345F18 * g * (lbl_80345F20 - (f64)len * (g * g));
            g = lbl_80345F18 * g * (lbl_80345F20 - (f64)len * (g * g));
            g = lbl_80345F18 * g * (lbl_80345F20 - (f64)len * (g * g));
            posRoot = (f32)((f64)len * (lbl_80345F18 * g *
                            (lbl_80345F20 - (f64)len * (g * g))));
            len = posRoot;
        }
        posDistance = len;
        if ((f64)len >= lbl_80345F28) {
            if (posDistance > lbl_80346138) {
                posDistance = lbl_80346138;
            }
            len = (f32)((f64)gFrameTicks / posDistance);
            if (len > lbl_80345FE0) {
                len = lbl_80345F80;
            }
            dx = dx * len;
            dy = dy * len;
            dz = dz * len;
        } else {
            reached = 1;
        }
        cam[0].wpos[0] = cam[0].wpos[0] + dx;
        cam[0].wpos[1] = cam[0].wpos[1] + dy;
        cam[0].wpos[2] = cam[0].wpos[2] + dz;

        dy = cameras[0].attn[1] - cam[0].attn[1];
        dx = cameras[0].attn[0] - cam[0].attn[0];
        dz = cameras[0].attn[2] - cam[0].attn[2];
        len = dy * dy;
        len = dx * dx + len;
        len = dz * dz + len;
        if (len > lbl_80345EC8) {
            g = __frsqrte((f64)len);
            g = lbl_80345F18 * g * (lbl_80345F20 - (f64)len * (g * g));
            g = lbl_80345F18 * g * (lbl_80345F20 - (f64)len * (g * g));
            g = lbl_80345F18 * g * (lbl_80345F20 - (f64)len * (g * g));
            attnRoot = (f32)((f64)len * (lbl_80345F18 * g *
                             (lbl_80345F20 - (f64)len * (g * g))));
            len = attnRoot;
        }
        if ((f64)len >= lbl_80345F28) {
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
        } else {
            reached = reached - 1;
        }
        cam[0].attn[0] = cam[0].attn[0] + dx;
        cam[0].attn[1] = cam[0].attn[1] + dy;
        cam[0].attn[2] = cam[0].attn[2] + dz;

        if (reached <= 0) {
            if (lbl_8034440C != 0) {
                MBRemoveBlit((s32)lbl_8034440C);
                lbl_8034440C = 0;
            }
            lbl_803444F0 = *(s8*)(level + 0x25);
            if (lbl_803444F0 >= 0) {
                lbl_803444EC = 0;
                lbl_803447B8 = 2;
                if (cam->c_mode != CAM_LOCK) {
                    cam->pc_mode = cam->c_mode;
                    cam->c_mode = CAM_LOCK;
                }
                if (cam->a_mode != ATN_TARGET) {
                    cam->pa_mode = cam->a_mode;
                    cam->a_mode = ATN_TARGET;
                }
                lbl_8034453C = 0;
            } else {
                s32 oldMode;

                lbl_803447B8 = 0;
                gScriptedCameraState = 0;
                lbl_8034453C = 0;
                oldMode = cam->a_mode;
                if (cam->c_mode != CAM_OFF) {
                    cam->pc_mode = cam->c_mode;
                    cam->c_mode = CAM_OFF;
                }
                if (oldMode != cam->a_mode) {
                    cam->pa_mode = cam->a_mode;
                    cam->a_mode = oldMode;
                }
                cam->state = 0;
                if ((sFlags & 4) != 0) {
                    lbl_803445D4 = lbl_803445D4 | 4;
                }
                return 0;
            }
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
    u8 unusedBefore[8];
    union {
        f32 value;
        u32 bits;
    } absDiff;
    u8 unusedAfter[8];
    Camera* cam = &gCameras[camIdx];
    f64 dv;
    f32 v;
    f64 dv3;
    f32 angle;
    f32 delta;

    {
        f32 zero = lbl_80345EC8;
        cam->pyr[2] = zero;
        if (resetDelta != 0) {
            cam->pyr_delta[0] = zero;
            cam->pyr_delta[1] = zero;
            cam->pyr_delta[2] = zero;
        }
    }
    if (gWorldInfo.wobjs == 0) {
        cam->pyr[0] = lbl_8034616C;
        {
            f32 zero = lbl_80345EC8;
            cam->pyr[1] = zero;
            cam->pyr[2] = zero;
        }
        goto apply;
    }
    if (gNumTransmitters != 0) {
        f32 step;
        f32 rate = (f32)(lbl_80346170 * (f64)(u32)gFrameTicks);
        f32 target = lbl_80344530;
        absDiff.value = target - *(volatile f32*)&lbl_80344408;
        absDiff.bits &= 0x7FFFFFFF;
        step = (f32)(lbl_80346070 * (f64)absDiff.value);
        if (step < rate) {
            step = rate;
        }
        {
            f32 current = lbl_80344408;
            if (target > current) {
                lbl_80344408 = current + step;
                if (target <= lbl_80344408) {
                    lbl_80344408 = target;
                }
            } else {
                lbl_80344408 = current - step;
                if (target >= lbl_80344408) {
                    lbl_80344408 = target;
                }
            }
        }
    }
    cam->pyr[0] = lbl_80344408;
    if (gNumTransmitters != 0) {
        goto apply;
    }

    {
        s32 mode = lbl_80344538;
        f32 centerX;
        f32 centerZ;
        f32 sizeZ;

        dv3 = gWorldInfo.worldsize[0];
        centerX = gWorldInfo.worldcenter[0];
        sizeZ = gWorldInfo.worldsize[2];
        centerZ = gWorldInfo.worldcenter[2];
        switch (mode) {
        default:
        case 0:
            dv = cam->attn[0] - centerX;
            break;
        case 2:
            dv = centerX - cam->attn[0];
            break;
        case 1:
            dv = centerZ - cam->attn[2];
            dv3 = sizeZ;
            break;
        case 3:
            dv = cam->attn[2] - centerZ;
            dv3 = sizeZ;
            break;
        }
        if (gScriptedCameraState != 0) {
            v = lbl_80344534;
        } else {
            v = lbl_80118B60[mode];
        }
    }
    cam->pyr[1] = FixAngle((f32)((f64)v + dv / (lbl_80345EF0 * dv3)));

apply:
    if (gNumTransmitters == 0) {
        angle = cam->pyr[0];
        if ((f64)(angle += (delta = cam->pyr_delta[0])) >= lbl_80346178) {
            cam->pyr[0] = (f32)(lbl_80346178 - (f64)delta);
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
static void place_cam(Camera* cam, f32* mat, f32* in, f32* out)
{
    CreateYPRMatrix(mat, cam->pyr);
    in[0] = lbl_80345EC8;
    in[1] = lbl_80345EC8;
    in[2] = cam->radius;
    WorldVector(in, &out[6], mat);
    cam->wpos[0] = cam->attn[0] + out[6];
    cam->wpos[1] = cam->attn[1] + out[7];
    cam->wpos[2] = cam->attn[2] + out[8];
}

static s32 cam_blocked(Camera* cam)
{
    s32 blocked = 0;
    s32 j;
    CameraTarget* t = gCameraTargets;
    for (j = 0; j < 15; j++, t++) {
        if (t->active > 0 && CameraCollide(cam->wpos, (f32*)(t->object + 0x40))) {
            blocked = 1;
            break;
        }
    }
    return blocked;
}

void get_cam_wpos_8002ABE0(s32 camIdx)
{
    s32* camState = (s32*)gCameraState;
    Camera* cam = &gCameras[camIdx];
    f32 mat[18];
    f32 in[3];
    f32 out[9];
    s32 i;

    cam->old_wpos[0] = cam->wpos[0];
    cam->old_wpos[1] = cam->wpos[1];
    cam->old_wpos[2] = cam->wpos[2];

    if (gNumTransmitters == 0 && lbl_803443F8 <= 0) {
        s32 mode = lbl_80344538;
        f64 yawMin = lbl_80345F68;
        f64 yawRange = lbl_80345F60;
        f64 yawStep = lbl_80346180;
        f64 yawMax = lbl_80345F58;
        for (i = 0; i < 4; i++) {
            camState[i] = 0;
        }
        for (i = 0; i < 4; i++) {
            f64 y;
            place_cam(cam, mat, in, out);
            camState[mode] = cam_blocked(cam);
            cam->pyr[1] = (f32)((f64)cam->pyr[1] + yawStep);
            y = cam->pyr[1];
            if (y > yawMax) {
                y -= yawRange;
            } else if (y <= yawMin) {
                y = yawRange + y;
            }
            mode = mode & 3;
            cam->pyr[1] = (f32)y;
        }
        if (camState[lbl_80344538] != 0) {
            s32 adj = (lbl_80344538 - 1) & 3;
            s32 found = 0;
            for (i = 4; i != 0; i--, adj &= 3) {
                if (adj != lbl_80344538 && camState[adj] == 0) {
                    found = 1;
                    break;
                }
            }
            if (found) {
                s32 delta = adj - lbl_80344538;
                gScriptedCameraState = 1;
                if (delta == 1 || delta == -3) {
                    lbl_80344400 = 1;
                } else {
                    lbl_80344400 = -1;
                }
                lbl_80344534 = lbl_80118B60[lbl_80344538];
                lbl_80344538 += lbl_80344400;
                lbl_80344538 &= 3;
                if (delta == 2 || delta == -2) {
                    lbl_803443F8 = 0;
                } else {
                    lbl_803443F8 = 0x168;
                }
            }
        }
    }

    place_cam(cam, mat, in, out);

    if (gNumTransmitters == 0) {
        f32 savedW0 = cam->wpos[0], savedW1 = cam->wpos[1], savedW2 = cam->wpos[2];
        f32 savedD = cam->pyr_delta[0];
        if (!cam_blocked(cam)) {
            if (cam->timer >= 0) {
                cam->timer = cam->timer - gFrameTicks;
            }
            if (cam->timer < 0) {
                if (lbl_80344404 > 0) {
                    if ((f64)cam->pyr_delta[0] > lbl_80345F78) {
                        cam->pyr_delta[0] = cam->pyr_delta[0] - lbl_80346188;
                        if ((f64)cam->pyr_delta[0] < lbl_80345F78) {
                            cam->pyr_delta[0] = lbl_80345EC8;
                        }
                        cam->pyr[0] = cam->pyr[0] - lbl_80346188;
                        place_cam(cam, mat, in, out);
                        if (cam_blocked(cam)) {
                            cam->pyr_delta[0] = savedD;
                            cam->wpos[0] = savedW0;
                            cam->wpos[1] = savedW1;
                            cam->wpos[2] = savedW2;
                        }
                    } else {
                        cam->pyr_delta[0] = lbl_80345EC8;
                    }
                } else {
                    if ((f64)cam->pyr_delta[0] < lbl_80345F78) {
                        cam->pyr_delta[0] = cam->pyr_delta[0] + lbl_80346188;
                        if ((f64)cam->pyr_delta[0] > lbl_80345F78) {
                            cam->pyr_delta[0] = lbl_80345EC8;
                        }
                        cam->pyr[0] = cam->pyr[0] + lbl_80346188;
                        place_cam(cam, mat, in, out);
                        if (cam_blocked(cam)) {
                            cam->pyr_delta[0] = savedD;
                            cam->wpos[0] = savedW0;
                            cam->wpos[1] = savedW1;
                            cam->wpos[2] = savedW2;
                        }
                    } else {
                        cam->pyr_delta[0] = lbl_80345EC8;
                    }
                }
            }
        } else {
            cam->timer = cam->timer + gFrameTicks;
            if (cam->timer > 0xB4) {
                cam->timer = 0xB4;
            }
            if (lbl_80344404 > 0) {
                if ((f64)cam->pyr[0] <= lbl_80346178 - lbl_80346188) {
                    cam->pyr_delta[0] = cam->pyr_delta[0] + lbl_80346188;
                    cam->pyr[0] = cam->pyr[0] + lbl_80346188;
                    place_cam(cam, mat, in, out);
                }
            } else {
                if (cam->pyr[0] >= lbl_80346188) {
                    cam->pyr_delta[0] = cam->pyr_delta[0] - lbl_80346188;
                    cam->pyr[0] = cam->pyr[0] - lbl_80346188;
                    place_cam(cam, mat, in, out);
                }
            }
        }
    }
}

f32 get_cam_dist(s32 camIdx)
{
    Camera* cam = &gCameras[camIdx];
    f64 base;
    f32 minDist;
    f64 farBase;
    f32 farDist;
    f32 result;

    if (gBossType >= 0) {
        base = lbl_80345F28;
    } else {
        base = lbl_80345F78;
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
        CameraTarget* t;
        f32 min24, max24, min36, max28;
        f32 ratio, xr, yr, nearScale, farScale;
        f32 radius = cam->radius;

        min24 = min36 = lbl_8034619C;
        max24 = max28 = lbl_803461A0;
        t = gCameraTargets;
        for (i = 15; i != 0; i--, t++) {
            if (t->active > 0) {
                f32 v24 = *(f32*)((u8*)t + 24);
                f32 v28 = *(f32*)((u8*)t + 28);
                f32 v36 = *(f32*)((u8*)t + 36);
                if (v24 < min24) min24 = v24;
                if (v24 > max24) max24 = v24;
                if (v36 < min36) min36 = v36;
                if (v28 > max28) max28 = v28;
            }
        }
        xr = (max24 - min24) /
             (f32)((lbl_8034451C - 30) - (lbl_80344520 + 30));
        yr = (max28 - min36) /
             (f32)((lbl_80344518 - 20) - (lbl_80344514 + 40));
        ratio = xr;
        if (yr > xr) ratio = yr;

        if (ratio < lbl_803461A8) {
            nearScale = lbl_803461B0;
        } else if (ratio >= lbl_80345F18 + base) {
            nearScale = lbl_803461B4;
        } else {
            nearScale = (f32)(lbl_803461B8 +
                ((lbl_80345F18 + base) - ratio) / lbl_803460F0);
        }

        if (ratio > lbl_803461C0) {
            f64 t2;
            if (lbl_803444E4 != 0) {
                t2 = lbl_80345F20;
            } else {
                t2 = lbl_803461C8;
            }
            farScale = (f32)t2;
        } else if (ratio <= farBase) {
            farScale = lbl_803461D0;
        } else {
            farScale = (f32)(lbl_803461C8 -
                (ratio - farBase) / lbl_80346018);
        }
        lbl_803444E8 = ratio;

        if (lbl_803444E4 != 0 && lbl_803443FC >= 0) {
            lbl_80344418 = 0;
        }
        if (lbl_80344418 != 0 && lbl_803443FC < 0) {
            lbl_803443FC = 0;
        } else if (lbl_803443FC != 0) {
            if ((lbl_803443FC < 0 &&
                 ratio >= base + *(volatile f64*)&lbl_80346190) ||
                (lbl_803443FC > 0 && ratio <= minDist)) {
                lbl_803443FC = 0;
            } else if (lbl_803443FC > 0) {
                radius = cam->radius * farScale;
            } else {
                radius = cam->radius * nearScale;
            }
        } else {
            if (ratio < lbl_80345F18 + base && lbl_80344418 == 0) {
                lbl_803443FC = -1;
                radius = cam->radius * nearScale;
            }
            if (ratio > farDist ||
                (lbl_803444F4 == 0 &&
                 ((lbl_80344960 < 0 && cam->radius < lbl_80344528) ||
                  (lbl_80344960 >= 0 &&
                   (f64)cam->radius < lbl_80345FF0)))) {
                lbl_803443FC = 1;
                radius = cam->radius * farScale;
            }
        }

        result = lbl_8034452C;
        if (radius < result) {
            result = result;
        } else {
            if (lbl_80344960 < 0) {
                if (radius > lbl_80344528 && lbl_803444E4 == 0) {
                    result = (f32)(radius -
                        lbl_80345F88 * (f32)(radius - lbl_80344528));
                } else {
                    result = radius;
                }
            } else {
                if (radius > lbl_80345FF0) {
                    result = (f32)(lbl_80345FF8 *
                        (lbl_80345FF0 - (f32)cam->radius) + (f32)cam->radius);
                } else {
                    result = radius;
                }
            }
        }
    }
done:
    return (f32)result;
}

s32 adjust_radius_8002B2D4(s32 camIdx)
{
    Camera* cam = &gCameras[camIdx];
    f32 r;
    f32 desired = get_cam_dist(camIdx);
    void* levelData = *(void**)((u8*)gCurLevel + 96);
    u8 _pad[8];
    f32 ad;
    u8 _pad2[4];
    f32 k;

    if (lbl_80345F78 == desired) {
        return 0;
    }
    if (desired < cam->radius && lbl_803443FC > 0) {
        lbl_803443FC = 0;
        desired = cam->radius;
    }
    if (desired > cam->radius && lbl_803443FC < 0) {
        lbl_803443FC = 0;
        desired = cam->radius;
    }
    if (lbl_80344960 < 0) {
        if (desired > (r = cam->radius) && r > lbl_80344528) {
            desired = r;
        }
    }

    k = lbl_8034618C;
    ad = desired - cam->radius;
    *(u32*)&ad &= 0x7FFFFFFF;
    if (ad < k) {
        cam->radius = desired;
        lbl_803443F4 = 1;
        goto done;
    }
    r = ad - k;
    r = (f32)(lbl_80346098 * r + k);
    if (r > lbl_80346158 && lbl_803444E4 == 0) {
        r = lbl_80346158;
    }
    if (desired > cam->radius) {
        cam->radius = cam->radius + r;
        lbl_803443F4 = 1;
        goto done;
    }
    if (desired < cam->radius) {
        if (lbl_80344418 == 0 || *(s16*)((u8*)levelData + 54) != 0) {
            cam->radius -= r;
            lbl_803443F4 = 1;
        }
    }
done:
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
    typedef union FloatBits {
        f32 value;
        u32 bits;
    } FloatBits;
    u8 stackLayout[56];
    u8* cameraState = gCameraState;
    Camera* cam;
    f32* eyeX;
    f32* eyeY;
    f32* eyeZ;
    f32 savedX;
    f32 savedY;
    f32 savedZ;
    f32 minX = lbl_803461D4, maxX = lbl_803461D8;
    f32 maxY = maxX, minY = minX;
    CameraTarget* target = (CameraTarget*)(cameraState + 0xA10);
    s32 i;
    s32 halfW, halfH;
    f32 cx, cy, horizontal, vertical, rx, ry;
    FloatBits extent0, extent1;
    s32 scrH = MBScreenHeight();
    s32 scrW = MBScreenWidth();
    cam = (Camera*)(cameraState + camIdx * sizeof(Camera) + 0xC8);
    eyeX = (f32*)((u8*)cam + 0x34);
    eyeY = eyeX + 1;
    eyeZ = eyeX + 2;
    savedX = *eyeX;
    savedY = *eyeY;
    savedZ = *eyeZ;

    *eyeX = pos[0];
    *eyeY = pos[1];
    *eyeZ = pos[2];
    for (i = 0; i < 15; i++, target++) {
        if (target->active != 0) {
            s16 sp[2];
            f32 sx, sy;
            MBWindowProject((f32*)(target->object + 0x40), cam->mat[0], 0, sp);
            sx = (f32)sp[0];
            sy = (f32)sp[1];
            if (sx < minX) minX = sx;
            if (sx > maxX) maxX = sx;
            if (sy < minY) minY = sy;
            if (sy > maxY) maxY = sy;
            MBWindowProject((f32*)(target->object + 0x30), cam->mat[0], 0, sp);
            sx = (f32)sp[0];
            sy = (f32)sp[1];
            if (sx < minX) minX = sx;
            if (sx > maxX) maxX = sx;
            if (sy < minY) minY = sy;
            if (sy > maxY) maxY = sy;
        }
    }

    halfW = scrW / 2;
    halfH = (scrH - 0x40) / 2;
    cx = (f32)halfW;
    cy = (f32)(scrH - halfH);

    extent0.value = minX - cx;
    extent1.value = maxX - cx;
    extent0.bits &= 0x7FFFFFFF;
    extent1.bits &= 0x7FFFFFFF;
    horizontal = extent0.value;
    if (extent0.value < extent1.value) {
        extent1.value = maxX - cx;
        extent1.bits &= 0x7FFFFFFF;
        horizontal = extent1.value;
    }
    rx = horizontal / (f32)halfW;

    extent0.value = minY - cy;
    extent1.value = maxY - cy;
    extent0.bits &= 0x7FFFFFFF;
    extent1.bits &= 0x7FFFFFFF;
    vertical = extent0.value;
    if (extent0.value < extent1.value) {
        extent1.value = maxY - cy;
        extent1.bits &= 0x7FFFFFFF;
        vertical = extent1.value;
    }
    ry = vertical / (f32)halfH;
    if (rx < ry) {
        rx = ry;
    }

    *eyeX = savedX;
    *eyeY = savedY;
    *eyeZ = savedZ;
    return rx;
}

/*
 * StandardCamera_8002B828 -- multiplayer framing loop.  It updates the focus point,
 * radius and radial camera position, then derives the camera orientation.
 */
extern s32 lbl_803444DC, lbl_803444CC, lbl_803444C8, lbl_80344500;
extern s32 lbl_803444D0;
extern f32 lbl_803444D8, lbl_803444D4;
extern f32 lbl_80346188, lbl_803461E8;
extern f64 lbl_803461E0, lbl_80346180;
f32 SlowNormalVector(f32* v);

/*
 * StandardCamera_8002B828 -- game camera (camera 0) auto-pan.  When the tracked targets
 * drift toward the screen edges it eases pan angles in/out, applies them around
 * the look axis, then keeps whichever of the two candidate framings leaves the
 * fewest targets off screen.
 */
void StandardCamera_8002B828(s32 camIdx)
{
    u8* cameraState = gCameraState;
    Camera* cam = (Camera*)(cameraState + camIdx * sizeof(Camera) + 0xC8);
    s32 wasPanning = lbl_803444DC;
    f32 minX = lbl_803461D4, maxX = lbl_803461D8;
    f32 maxY = lbl_803461D8, minY = lbl_803461D4;
    f32 errX = lbl_80345EC8;
    f32 errY = lbl_80345EC8;
    s32 scrH = MBScreenHeight();
    s32 scrW = MBScreenWidth();
    f32 offX = lbl_80345EC8;
    f32 offZ = lbl_80345EC8;
    f32 yaw;
    f32 panXStart;
    u8* levelData;
    s32 i;

    if (gCurLevel == 0) {
        return;
    }
    levelData = *(u8**)((u8*)gCurLevel + 0x60);
    if (camIdx != 0) {
        return;
    }
    if (lbl_8034453C != 0) {
        return;
    }
    if (cam->c_mode != 3) {
        return;
    }
    if (gBossType < 0) {
        goto valid_boss_type;
    }
    return;
valid_boss_type:
    if (gBossType == 0x22) {
        return;
    }

    panXStart = lbl_803444D8;
    if (lbl_80345F78 != (f64)lbl_803444D8 || lbl_80345F78 != (f64)lbl_803444D4) {
        lbl_803444DC = 1;
        lbl_803444D0 = lbl_803444D0 + gFrameTicks;
    } else {
        lbl_803444DC = 0;
        lbl_803444D0 = 0;
        lbl_80344528 = *(f32*)(levelData + 0x30);
    }
    if (gCameraTargetCount > 0) {
        CameraTarget* t = (CameraTarget*)(cameraState + 0xA10);
        for (i = 0; i < 15; i++, t++) {
            if (t->active != 0) {
                f32 tx = *(f32*)((u8*)t + 0x28);
                if (minX > tx) minX = tx;
                if (maxX < tx) maxX = tx;
                if (maxY < *(f32*)((u8*)t + 0x2C)) {
                    maxY = *(f32*)((u8*)t + 0x2C);
                }
                if (minY > *(f32*)((u8*)t + 0x34)) {
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
        cam->radius <= *(f32*)(levelData + 0x30) ||
        (lbl_80344500 == 0 && lbl_803444F4 != 0 && lbl_803444DC == 0) ||
        (f64)lbl_803444E8 < lbl_80345F90) {
        if ((f64)errX >= lbl_803461E0) {
            f32 absPanX;
            f32 newPanX;

            if ((f64)panXStart > lbl_80345F78) {
                newPanX = lbl_803444D8 - lbl_80346188;
                lbl_803444D8 = newPanX;
                if ((f64)newPanX <= lbl_80345F78) {
                    lbl_803444D8 = lbl_80345EC8;
                }
            } else if ((f64)panXStart < lbl_80345F78) {
                newPanX = lbl_803444D8 + lbl_80346188;
                lbl_803444D8 = newPanX;
                if (lbl_80345F78 <= (f64)newPanX) {
                    lbl_803444D8 = lbl_80345EC8;
                }
            }
            absPanX = lbl_803444D8;
            *(u32*)&absPanX &= 0x7FFFFFFF;
            if (absPanX < lbl_80346188) {
                lbl_803444D8 = lbl_80345EC8;
            }
        }
        if ((f64)errY >= lbl_803461E0) {
            f32 absPanY;
            f32 newPanY;
            f32 panYStart = lbl_803444D4;

            if ((f64)panYStart > lbl_80345F78) {
                newPanY = panYStart - lbl_80346188;
                lbl_803444D4 = newPanY;
                if ((f64)newPanY <= lbl_80345F78) {
                    lbl_803444D4 = lbl_80345EC8;
                }
            } else if ((f64)panYStart < lbl_80345F78) {
                newPanY = panYStart + lbl_80346188;
                lbl_803444D4 = newPanY;
                if (lbl_80345F78 <= (f64)newPanY) {
                    lbl_803444D4 = lbl_80345EC8;
                }
            }
            absPanY = lbl_803444D4;
            *(u32*)&absPanY &= 0x7FFFFFFF;
            if (absPanY < lbl_80346188) {
                lbl_803444D4 = lbl_80345EC8;
            }
        }
    } else {
        f32 absErrXStart = errX;
        f32 absErrXMoving;
        f32 absErrXEase;
        f32 absPanX;
        f32 absErrYStart;
        f32 absErrYMoving;
        f32 absErrYEase;
        f32 absPanY;
        u8 unused[136];

        *(u32*)&absErrXStart &= 0x7FFFFFFF;
        absErrXMoving = errX;
        *(u32*)&absErrXMoving &= 0x7FFFFFFF;
        if ((lbl_80346160 <= (f64)absErrXStart &&
             lbl_80345F78 == (f64)lbl_803444D8) ||
            (lbl_803461E0 <= (f64)absErrXMoving &&
             lbl_80345F78 != (f64)lbl_803444D8)) {
            if ((f64)errX >= lbl_80345F78) {
                lbl_803444D8 = (f32)((f64)lbl_803444D8 + lbl_80346070);
            } else {
                lbl_803444D8 = (f32)((f64)lbl_803444D8 - lbl_80346070);
            }
        } else {
            absErrXEase = errX;
            *(u32*)&absErrXEase &= 0x7FFFFFFF;
            if ((f64)absErrXEase < lbl_803460D0) {
                f32 easeX;
                if ((f64)lbl_803444D8 > lbl_80345F78) {
                    easeX = lbl_803444D8 - lbl_803461E8;
                    lbl_803444D8 = easeX;
                    if ((f64)easeX <= lbl_80345F78) {
                        lbl_803444D8 = lbl_80345EC8;
                    }
                } else if ((f64)lbl_803444D8 < lbl_80345F78) {
                    easeX = lbl_803444D8 + lbl_803461E8;
                    lbl_803444D8 = easeX;
                    if (lbl_80345F78 <= (f64)easeX) {
                        lbl_803444D8 = lbl_80345EC8;
                    }
                }
                absPanX = lbl_803444D8;
                *(u32*)&absPanX &= 0x7FFFFFFF;
                if (absPanX < lbl_803461E8) {
                    lbl_803444D8 = lbl_80345EC8;
                }
            }
        }

        absErrYStart = errY;
        *(u32*)&absErrYStart &= 0x7FFFFFFF;
        absErrYMoving = errY;
        *(u32*)&absErrYMoving &= 0x7FFFFFFF;
        if ((lbl_80346160 <= (f64)absErrYStart &&
             lbl_80345F78 == (f64)lbl_803444D4) ||
            (lbl_803461E0 <= (f64)absErrYMoving &&
             lbl_80345F78 != (f64)lbl_803444D4)) {
            if ((f64)errY >= lbl_80345F78) {
                lbl_803444D4 = (f32)((f64)lbl_803444D4 - lbl_80346070);
            } else {
                lbl_803444D4 = (f32)((f64)lbl_803444D4 + lbl_80346070);
            }
        } else {
            absErrYEase = errY;
            *(u32*)&absErrYEase &= 0x7FFFFFFF;
            if ((f64)absErrYEase < lbl_803460D0) {
                f32 easeY;
                if ((f64)lbl_803444D4 > lbl_80345F78) {
                    easeY = lbl_803444D4 - lbl_803461E8;
                    lbl_803444D4 = easeY;
                    if ((f64)easeY <= lbl_80345F78) {
                        lbl_803444D4 = lbl_80345EC8;
                    }
                } else if ((f64)lbl_803444D4 < lbl_80345F78) {
                    easeY = lbl_803444D4 + lbl_803461E8;
                    lbl_803444D4 = easeY;
                    if (lbl_80345F78 <= (f64)easeY) {
                        lbl_803444D4 = lbl_80345EC8;
                    }
                }
                absPanY = lbl_803444D4;
                *(u32*)&absPanY &= 0x7FFFFFFF;
                if (absPanY < lbl_803461E8) {
                    lbl_803444D4 = lbl_80345EC8;
                }
            }
        }
    }

    if (lbl_80345F78 != (f64)lbl_803444D8) {
        f32 sinScale;
        f32 cosScale;
        f32 sinValue;
        f32 cosValue;

        if ((f64)lbl_803444D8 > lbl_80345F78) {
            yaw = (f32)((f64)cam->pyr[1] - lbl_80346180);
        } else {
            yaw = (f32)((f64)cam->pyr[1] + lbl_80346180);
        }
        if ((f64)yaw > lbl_80345F58) {
            yaw = (f32)((f64)yaw - lbl_80345F60);
        } else if ((f64)yaw <= lbl_80345F68) {
            yaw = (f32)(lbl_80345F60 + (f64)yaw);
        }
        sinValue = sin(yaw);
        sinScale = lbl_803444D8;
        *(u32*)&sinScale &= 0x7FFFFFFF;
        offX = sinValue * sinScale + offX;
        cosValue = cos(yaw);
        cosScale = lbl_803444D8;
        *(u32*)&cosScale &= 0x7FFFFFFF;
        lbl_803444DC = 1;
        offZ = cosValue * cosScale + offZ;
    }
    if (lbl_80345F78 != (f64)lbl_803444D4) {
        f32 sinScale;
        f32 cosScale;
        f32 sinValue;
        f32 cosValue;
        f64 y = (f64)cam->pyr[1];
        if ((f64)lbl_803444D4 > lbl_80345F78) {
        } else {
            y = (f32)(y + lbl_80345F58);
        }
        if (y > lbl_80345F58) {
            y = y - lbl_80345F60;
        } else if (y <= lbl_80345F68) {
            y = lbl_80345F60 + y;
        }
        yaw = (f32)y;
        sinValue = sin(yaw);
        sinScale = lbl_803444D4;
        *(u32*)&sinScale &= 0x7FFFFFFF;
        offX = sinValue * sinScale + offX;
        cosValue = cos(yaw);
        cosScale = lbl_803444D4;
        *(u32*)&cosScale &= 0x7FFFFFFF;
        lbl_803444DC = 1;
        offZ = cosValue * cosScale + offZ;
    }

    if (lbl_80345F78 == (f64)lbl_803444D8 && lbl_80345F78 == (f64)lbl_803444D4) {
        lbl_803444DC = 0;
        lbl_803444D0 = 0;
    } else {
        f32 mn = lbl_8034619C, mx = lbl_803461A0;
        f32 cand[3];
        f32 dir[3];
        CameraTarget* t = (CameraTarget*)(cameraState + 0xA10);
        for (i = 0; i < 15; i++, t++) {
            if (t->active > 0) {
                f32 ty = *(f32*)(t->object + 0x44);
                if (ty < mn) mn = ty;
                if (ty > mx) mx = ty;
            }
        }
        ((f32*)(cameraState + 0x34))[0] = offX + cam->wpos[0];
        ((f32*)(cameraState + 0x34))[1] = lbl_80345EC8 + cam->wpos[1];
        ((f32*)(cameraState + 0x34))[2] = offZ + cam->wpos[2];
        cand[0] = offX + cam->attn[0];
        cand[2] = offZ + cam->attn[2];
        cam->attn[1] = (f32)(lbl_80345F18 * (f64)(mx + mn));
        cand[1] = cam->attn[1];
        cam->attn_dest[1] = cam->attn[1];
        dir[0] = ((f32*)(cameraState + 0x34))[0] - cand[0];
        dir[1] = ((f32*)(cameraState + 0x34))[1] - cand[1];
        dir[2] = ((f32*)(cameraState + 0x34))[2] - cand[2];
        SlowNormalVector(dir);
        ((f32*)(cameraState + 0x34))[0] = dir[0] * cam->radius + cand[0];
        ((f32*)(cameraState + 0x34))[1] = dir[1] * cam->radius + cand[1];
        ((f32*)(cameraState + 0x34))[2] = dir[2] * cam->radius + cand[2];
    }

    if (lbl_803444DC != 0) {
        f32 rOld = someone_will_be_off_screen(camIdx, cam->wpos);
        f32 rNew = someone_will_be_off_screen(camIdx, (f32*)(cameraState + 0x34));
        if (rOld < rNew) {
            lbl_803444DC = 0;
            lbl_803444D0 = 0;
            lbl_803444D8 = lbl_80345EC8;
            lbl_803444D4 = lbl_80345EC8;
            ((f32*)(cameraState + 0x34))[0] = cam->wpos[0];
            ((f32*)(cameraState + 0x34))[1] = cam->wpos[1];
            ((f32*)(cameraState + 0x34))[2] = cam->wpos[2];
        } else {
            cam->wpos[0] = ((f32*)(cameraState + 0x34))[0];
            cam->wpos[1] = ((f32*)(cameraState + 0x34))[1];
            cam->wpos[2] = ((f32*)(cameraState + 0x34))[2];
            cam->attn[0] += offX;
            cam->attn[1] += lbl_80345EC8;
            cam->attn[2] += offZ;
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

#pragma opt_propagation off
f32 get_pitch(f32* a, f32* b)
{
    f32 dx = a[0] - b[0];
    f32 dz = a[2] - b[2];
    f32 dy = a[1] - b[1];
    f32 len = dx * dx + dz * dz;
    f32 dist;
    f32 ang;
    u8 tail[8];
    volatile f32 root;
    union {
        f32 f;
        u32 i;
    } u;
    u8 unused[12];

    if (len > lbl_80345EC8) {
        f64 guess = __frsqrte(len);
        guess = lbl_80345F18 * guess * (lbl_80345F20 - len * (guess * guess));
        guess = lbl_80345F18 * guess * (lbl_80345F20 - len * (guess * guess));
        guess = lbl_80345F18 * guess * (lbl_80345F20 - len * (guess * guess));
        root = (f32)(len * (lbl_80345F18 * guess *
                            (lbl_80345F20 - len * (guess * guess))));
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
    return FixAngle(ang);
}
#pragma opt_propagation reset

extern f32 lbl_8023F8C4[], lbl_8023F8B8[];
extern s32 gGameMode, lbl_80344824, lbl_80344414;
typedef struct CombatItem {
    u8 pad[0xDC];
    u8 data[0x14];
} CombatItem;
extern CombatItem* sItems;
extern f64 lbl_803461F0;

void get_attn_pos_8002C9A8(s32 camIdx, f32* out)
{
    u8 unused[44];
    u8* cameraState = gCameraState;
    Camera* cam = &((Camera*)(cameraState + 0xC8))[camIdx];
    s32 aMode;
    s32 i;

    cam->old_attn[0] = cam->attn[0];
    cam->old_attn[1] = cam->attn[1];
    cam->old_attn[2] = cam->attn[2];
    aMode = cam->a_mode;

    if (sMusicTrackHi < 0) {
        if (aMode != 1) {
            f32 zero = lbl_80345EC8;
            cam->attn[0] = zero;
            cam->attn[1] = zero;
            cam->attn[2] = zero;
            out[0] = cam->attn[0];
            out[1] = cam->attn[1];
            out[2] = cam->attn[2];
        }
        cam->attn_dest[0] = out[0];
        cam->attn_dest[1] = out[1];
        cam->attn_dest[2] = out[2];
        cam->attn_dest_no_offset[0] = out[0];
        cam->attn_dest_no_offset[1] = out[1];
        cam->attn_dest_no_offset[2] = out[2];
    } else if (aMode == 1 ||
               ((gGameMode & 0x4000) != 0 && (u32)lbl_80344824 == 0)) {
        out[0] = cam->attn[0];
        out[1] = cam->attn[1];
        out[2] = cam->attn[2];
        cam->attn_dest[0] = out[0];
        cam->attn_dest[1] = out[1];
        cam->attn_dest[2] = out[2];
        cam->attn_dest_no_offset[0] = out[0];
        cam->attn_dest_no_offset[1] = out[1];
        cam->attn_dest_no_offset[2] = out[2];
    } else if (aMode == 3 || (u32)(aMode - 5) <= 4) {
        if (cam->attnobj != 0) {
            out[0] = *(f32*)((u8*)cam->attnobj + 0x40);
            out[1] = *(f32*)((u8*)cam->attnobj + 0x44);
            out[2] = *(f32*)((u8*)cam->attnobj + 0x48);
        } else {
            out[0] = cam->attn[0];
            out[1] = cam->attn[1];
            out[2] = cam->attn[2];
        }
        cam->attn_dest[0] = out[0];
        cam->attn_dest[1] = out[1];
        cam->attn_dest[2] = out[2];
        cam->attn_dest_no_offset[0] = out[0];
        cam->attn_dest_no_offset[1] = out[1];
        cam->attn_dest_no_offset[2] = out[2];
    } else if (aMode == 10) {
        if (*(s16*)(sTriggerCameras + cam->cn * 0x28 + 2) != 0) {
            out[0] = TC_X(cam->cn);
            out[1] = TC_Y(cam->cn);
            out[2] = TC_Z(cam->cn);
        } else {
            out[0] = cam->attn[0];
            out[1] = cam->attn[1];
            out[2] = cam->attn[2];
        }
        cam->attn_dest[0] = out[0];
        cam->attn_dest[1] = out[1];
        cam->attn_dest[2] = out[2];
        cam->attn_dest_no_offset[0] = out[0];
        cam->attn_dest_no_offset[1] = out[1];
        cam->attn_dest_no_offset[2] = out[2];
    } else {
        if (lbl_803444F4 == 0) {
            cam->unvib = 0;
        }
        if (cam->unvib >= 0xB4 && lbl_80344960 >= 0) {
            CombatItem* item;
            out[0] = *(f32*)(*(u8**)(*(u8**)((item =
                sItems + lbl_80344960)->data) +
                0x28) + 0x30);
            out[1] = *(f32*)(*(u8**)(*(u8**)((item =
                sItems + lbl_80344960)->data) +
                0x28) + 0x34);
            out[2] = *(f32*)(*(u8**)(*(u8**)((item =
                sItems + lbl_80344960)->data) +
                0x28) + 0x38);
            cam->attn_dest[0] = out[0];
            cam->attn_dest[1] = out[1];
            cam->attn_dest[2] = out[2];
            cam->attn_dest_no_offset[0] = out[0];
            cam->attn_dest_no_offset[1] = out[1];
            cam->attn_dest_no_offset[2] = out[2];
        } else {
            f32 minX = lbl_8034619C, maxX = lbl_803461A0;
            f32 minY = lbl_8034619C, maxY = lbl_803461A0;
            f32 minZ = lbl_8034619C, maxZ = lbl_803461A0;
            f32 sv0, sv1, sv2;
            CameraTarget* target = (CameraTarget*)(cameraState + 2576);
            for (i = 0; i < 15; i++, target++) {
                if (target->active > 0) {
                    f32* p = (f32*)(target->object + 0x40);
                    f32 x = p[0];
                    f32 y = p[1];
                    f32 z = p[2];
                    if (x < minX) minX = x;
                    if (x > maxX) maxX = x;
                    if (y < minY) minY = y;
                    if (y > maxY) maxY = y;
                    if (z < minZ) minZ = z;
                    if (z > maxZ) maxZ = z;
                }
            }
            {
                register f64 half = lbl_80345F18;
                out[0] = (f32)(half * (f64)(minX + maxX));
                out[1] = (f32)(half * (f64)(minY + maxY));
                out[2] = (f32)(half * (f64)(minZ + maxZ));
            }
            cam->attn_dest_no_offset[0] = out[0];
            cam->attn_dest_no_offset[1] = out[1];
            cam->attn_dest_no_offset[2] = out[2];
            if (*(s32*)((u8*)cam + 0xEC) == 3) {
                if (gNumTransmitters == 0) {
                    f32 cp = cos(cam->pyr[0]);
                    register f64 half = lbl_80345F18;
                    out[2] = (f32)(half *
                        (half * (f64)(maxZ - minZ) * (f64)cp) +
                        (f64)out[2]);
                } else {
                    f32 sy = sin(cam->pyr[1]);
                    f32 cp = cos(cam->pyr[0]);
                    f32 scale;
                    f32 cy;
                    f32 cp2;
                    {
                        register f64 half = lbl_80345F18;
                        register f64 spread = lbl_803461F0;
                        scale = (f32)(spread * half *
                            (f64)(maxX - minX));
                    }
                    out[0] = scale * cp * sy + out[0];
                    cy = cos(cam->pyr[1]);
                    cp2 = cos(cam->pyr[0]);
                    {
                        register f64 half = lbl_80345F18;
                        register f64 spread = lbl_803461F0;
                        scale = (f32)(spread * half *
                            (f64)(maxZ - minZ));
                    }
                    out[2] = scale * cp2 * cy + out[2];
                }
            }
            cam->attn_dest[0] = out[0];
            cam->attn_dest[1] = out[1];
            cam->attn_dest[2] = out[2];
            sv0 = out[0];
            sv1 = out[1];
            sv2 = out[2];
            lbl_80344418 = 0;
            if (lbl_803447B8 == 0 && lbl_80344414 < 2) {
                for (i = 0; i < 3; i++) {
                    if (out[i] < *(f32*)(cameraState + i * 4 + 188)) {
                        out[i] = *(f32*)(cameraState + i * 4 + 188);
                        lbl_80344418 = 1;
                    } else if (out[i] > *(f32*)(cameraState + i * 4 + 176)) {
                        out[i] = *(f32*)(cameraState + i * 4 + 176);
                        lbl_80344418 = 1;
                    }
                }
            }
            if (lbl_80344414 != 0) {
                f32 d0 = sv0 - out[0];
                f32 d1 = sv1 - out[1];
                f32 d2 = sv2 - out[2];
                if (d0 == lbl_80345EC8 && d1 == lbl_80345EC8 &&
                    d2 == lbl_80345EC8) {
                    lbl_80344414 = 0;
                } else {
                    out[0] = sv0;
                    out[1] = sv1;
                    out[2] = sv2;
                }
            }
            cam->unvib = cam->unvib + gFrameTicks;
        }
    }
}

#pragma opt_propagation off
void recalc_lookat(s32 camIdx, s32 snap)
{
    Camera* cam = &gCameras[camIdx];
    u8 tail[12];
    f32 pos[3];
    u8 gap[4];
    f32 zero;

    if (cam->a_mode == ATN_FREE || cam->a_mode == ATN_LOCK ||
        cam->a_mode == ATN_POINT) {
        return;
    }
    get_attn_pos_8002C9A8(camIdx, pos);
    if (snap != 0) {
        cam->attn[0] = pos[0];
        cam->attn[1] = pos[1];
        cam->attn[2] = pos[2];
        zero = lbl_80345EC8;
        cam->delta[0] = zero;
        cam->delta[1] = zero;
        cam->delta[2] = zero;
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
        d2 = dz * dz + (dx * dx + dy * dy);
        if (d2 > lbl_80345EC8) {
            f64 g = __frsqrte((f64)d2);
            g = lbl_80345F18 * g * (lbl_80345F20 - d2 * (g * g));
            g = lbl_80345F18 * g * (lbl_80345F20 - d2 * (g * g));
            g = lbl_80345F18 * g * (lbl_80345F20 - d2 * (g * g));
            g = lbl_80345F18 * g * (lbl_80345F20 - d2 * (g * g));
            root = (f32)(d2 * g);
            d2 = root;
        }
        cam->radius = d2;
    }
}
#pragma opt_propagation reset

extern s32 lbl_80344498, lbl_803444E0, lbl_803444F8, lbl_80344470, lbl_80344474;
extern s32 gCameraTargetPositionCount, gCameraTargetMode, lbl_8034446C, lbl_80344494, lbl_803443F0;
extern s32 lbl_80344550, shake_type, shaking, shake_count, shake_delay;
extern s32 shake_priority, lbl_803447B4, lbl_803444BC, gCameraWindowLeftLimit;
extern s32 gCameraWindowRightLimit, gCameraWindowTopLimit, gCameraWindowBottomLimit, lbl_80344514, lbl_80344518;
extern s32 lbl_80344520, gNumEnemies, lbl_803447F8, lbl_80344524;
extern s32 lbl_80344288, lbl_8034441C, lbl_80344420, lbl_80344A28;
extern u8* sSpecialTransmitter;
extern f32 lbl_80344460, lbl_80344464, shake_rad, lbl_803461F8, lbl_803461FC;
extern f32 gCameraWindowScaleX, gCameraWindowScaleY, lbl_80345F80;
extern f32 lbl_80344428, lbl_80344424, lbl_80344438, lbl_80344434;
extern f32 lbl_8034443C, lbl_80344440, lbl_80344444, lbl_80344448;
extern f32 lbl_8034442C, lbl_80344430;
extern f32 lbl_80346200, lbl_80345F48, lbl_80346080, lbl_8034601C, lbl_80346260;
extern f64 lbl_803460B0, lbl_803460B8, lbl_803460C0;
extern f64 lbl_803460C8, lbl_80345EF0, lbl_80346238, lbl_80346240, lbl_80346078;
extern f64 lbl_80346250;
extern f32 lbl_80346208, lbl_80346148, lbl_80346158, lbl_8034620C, lbl_80346210;
extern f32 lbl_80346214, lbl_80346218, lbl_8034621C, lbl_80346220, lbl_80346224;
extern f32 lbl_80346228, lbl_8034622C, lbl_80346230, lbl_80346204, lbl_80346248;
extern f32 lbl_80346258, lbl_8034625C, lbl_80345F14, lbl_80344880;
extern f32 lbl_8023F824, lbl_8023F828, lbl_8023F82C, lbl_8023F830, lbl_8023F834;
extern f32 lbl_8023F838, lbl_802757D8, lbl_8028CAC8, lbl_8028CAD0, lbl_8028CAB4;
extern f32 lbl_8028CAA8, lbl_8028CACC;
extern f32 gDefaultPlayerPosition[], lbl_8023F8D4[], lbl_80258E08[];
extern u8* lbl_80344EE8;
extern u8 gIdentityMatrix[];
extern u8 sMilestones[];
extern char lbl_80111B3C[];
void ChangeWindow(void);
f32 FloorPos(f32 fallback, f32 radius, f32* pos, s32 mode);
s32 fn_80051480(f32* pos);
f32 SlowNormalVector(f32* v);

#define CAM_SET_CMODE(camp, m)                                                \
    if (*(s32*)(cs + 436) != (m)) {                                           \
        (camp)->pc_mode = (camp)->c_mode;                                     \
        (camp)->c_mode = (m);                                                 \
    }
#define CAM_SET_AMODE(camp, m)                                                \
    if ((camp)->a_mode != (m)) {                                              \
        (camp)->pa_mode = (camp)->a_mode;                                     \
        (camp)->a_mode = (m);                                                 \
    }

void InitCamera(s32 resetAll)
{
    u8* cs = (u8*)gCameraState;
    Camera* c0 = (Camera*)(cs + 200);
    s32 scrH = MBScreenHeight();
    s32 scrW = MBScreenWidth();
    s32 uiFov = 0;
    s32 i;
    Camera* cam;
    f32 mat[16];
    f32 in[3];
    f32 out[3];

    {
        f32 zero = lbl_80345EC8;
        f32 initrad = lbl_80346200;
        f32* idmat = (f32*)gIdentityMatrix;

        lbl_80344498 = 0;
        lbl_803444DC = 0;
        lbl_803444D0 = 0;
        lbl_803444D4 = zero;
        lbl_803444D8 = zero;
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
        lbl_80344460 = zero;
        lbl_80344464 = zero;
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
        shake_rad = zero;
        shake_priority = 0;

        cam = (Camera*)(cs + 200);
        for (i = 0; i < 6; i++, cam++) {
            cam->state = 0;
            CopyMat4(idmat, &cam->mat[0][0]);
            cam->limit_pos[0] = zero;
            cam->limit_pos[1] = zero;
            cam->limit_pos[2] = zero;
            cam->limit_vel[0] = zero;
            cam->limit_vel[1] = zero;
            cam->limit_vel[2] = zero;
            cam->wpos[0] = zero;
            cam->wpos[1] = zero;
            cam->wpos[2] = zero;
            cam->old_wpos[0] = zero;
            cam->old_wpos[1] = zero;
            cam->old_wpos[2] = zero;
            cam->vel[0] = zero;
            cam->vel[1] = zero;
            cam->vel[2] = zero;
            cam->avel[0] = zero;
            cam->avel[1] = zero;
            cam->avel[2] = zero;
            cam->pyr[0] = zero;
            cam->pyr[1] = zero;
            cam->pyr[2] = zero;
            cam->pyr_delta[0] = zero;
            cam->pyr_delta[1] = zero;
            cam->pyr_delta[2] = zero;
            cam->offset[0] = zero;
            cam->offset[1] = zero;
            cam->offset[2] = zero;
            cam->attn[0] = zero;
            cam->attn[1] = zero;
            cam->attn[2] = zero;
            cam->old_attn[0] = zero;
            cam->old_attn[1] = zero;
            cam->old_attn[2] = zero;
            cam->delta[0] = zero;
            cam->delta[1] = zero;
            cam->delta[2] = zero;
            cam->attn_dest[0] = zero;
            cam->attn_dest[1] = zero;
            cam->attn_dest[2] = zero;
            cam->attn_dest_no_offset[0] = zero;
            cam->attn_dest_no_offset[1] = zero;
            cam->attn_dest_no_offset[2] = zero;
            cam->cam_dest[0] = zero;
            cam->cam_dest[1] = zero;
            cam->cam_dest[2] = zero;
            cam->unvib = 0;
            cam->radius = initrad;
            cam->trans_mode = -1;
            cam->flags = 0;
            cam->mode = 0;
            cam->timer = 0;
            cam->num3 = zero;
            cam->num2 = zero;
            cam->num1 = zero;
            cam->value = zero;
            cam->pc_mode = (CAM_MODE)-1;
            cam->c_mode = (CAM_MODE)-1;
            cam->camobj = 0;
            cam->pa_mode = (ATN_MODE)-1;
            cam->a_mode = (ATN_MODE)-1;
            cam->attnobj = 0;
            cam->cn = 0;
            cam->ln = 0;
            cam->mn = 0;
            cam->gn = 0;
            cam->en = 0;
            cam->pn = 0;
        }
    }

    if (resetAll != 0) {
        f32 z;
        CAM_SET_CMODE(c0, 2);
        CAM_SET_AMODE(c0, 1);
        z = lbl_80345EC8;
        c0->wpos[0] = z;
        c0->wpos[1] = z;
        c0->wpos[2] = z;
        c0->attn[0] = z;
        c0->attn[1] = z;
        c0->attn[2] = lbl_80346204;
        c0->state = 1;
    } else {
        s32 mode = gGameMode;
        if (mode == 0x400B) {
            f32 z;
            CAM_SET_CMODE(c0, 2);
            CAM_SET_AMODE(c0, 1);
            z = lbl_80345EC8;
            c0->wpos[0] = z;
            c0->wpos[1] = lbl_80346208;
            c0->wpos[2] = lbl_80346148;
            c0->attn[0] = z;
            c0->attn[1] = lbl_80346158;
            c0->attn[2] = z;
            c0->state = 1;
        } else if (mode == 0x400D || mode == 0x4013 || mode == 0x4017) {
            f32 ang;
            f32 s;
            f32 c;
            f32 dy, dx, dz, len;
            f32* py1;
            f32* pz1;
            f32* py2;
            f32* pz2;
            Camera* k;
            CAM_SET_CMODE((Camera*)(cs + 200), 2);
            CAM_SET_AMODE((Camera*)(cs + 200), 1);
            py1 = (f32*)(cs + 32);
            pz1 = (f32*)(cs + 36);
            py2 = (f32*)(cs + 44);
            pz2 = (f32*)(cs + 48);
            k = (Camera*)((void*)(cs + 200));
            c0->num1 = lbl_80345F48;
            c0->pyr[1] = c0->num1;
            c0->num2 = lbl_80345EC8;
            *(f32*)(cs + 28) = lbl_8034620C;
            *(f32*)(cs + 32) = lbl_80346210;
            *(f32*)(cs + 36) = lbl_8034620C;
            *(f32*)(cs + 40) = lbl_80346214;
            *(f32*)(cs + 44) = lbl_80346218;
            *(f32*)(cs + 48) = lbl_80346214;
            ang = *(f32*)(cs + 368);
            s = sin(ang);
            c = cos(ang);
            *(f32*)(cs + 300) = s * *(f32*)(cs + 28);
            *(f32*)(cs + 304) = *py1;
            *(f32*)(cs + 308) = c * *pz1;
            *(f32*)(cs + 500) = s * *(f32*)(cs + 40);
            *(f32*)(cs + 504) = *py2;
            *(f32*)(cs + 508) = c * *pz2;
            dy = *(f32*)(cs + 304) - *(f32*)(cs + 504);
            dx = *(f32*)(cs + 300) - *(f32*)(cs + 500);
            dz = *(f32*)(cs + 308) - *(f32*)(cs + 508);
            len = dz * dz + (dx * dx + dy * dy);
            if (len > lbl_80345EC8) {
                f64 g = __frsqrte((f64)len);
                g = lbl_80345F18 * g * -((f64)len * g * g - lbl_80345F20);
                g = lbl_80345F18 * g * -((f64)len * g * g - lbl_80345F20);
                g = lbl_80345F18 * g * -((f64)len * g * g - lbl_80345F20);
                len = (f32)((f64)len * (lbl_80345F18 * g *
                            -((f64)len * g * g - lbl_80345F20)));
            }
            k->radius = len;
            {
                f32 ox = s * k->num2;
                f32 oz = c * k->num2;
                f32 z = lbl_80345EC8;
                k->wpos[0] = k->wpos[0] + ox;
                k->wpos[1] = k->wpos[1] + z;
                k->wpos[2] = k->wpos[2] + oz;
                k->attn[0] = k->attn[0] + ox;
                k->attn[1] = k->attn[1] + z;
                k->attn[2] = k->attn[2] + oz;
            }
            c0->mode = 0;
            c0->timer = 0;
            c0->state = 1;
        } else if ((u32)mode == 0x8007) {
            CAM_SET_CMODE(c0, 2);
            CAM_SET_AMODE(c0, 1);
            c0->wpos[0] = lbl_8034621C;
            c0->wpos[1] = lbl_80346220;
            c0->wpos[2] = lbl_80346224;
            c0->attn[0] = lbl_80346228;
            c0->attn[1] = lbl_8034622C;
            c0->attn[2] = lbl_80346230;
            c0->state = 1;
        } else if ((u32)mode == 0x8008) {
            if (lbl_80344288 != 0) {
                f32 d[3];
                f32 saveA[3];
                f32 saveW[3];
                f32 look[3];
                f32* dpp;
                f32 yaw;
                f32 r;
                Camera* k;
                CAM_SET_CMODE(c0, 2);
                CAM_SET_AMODE(c0, 0);
                dpp = gDefaultPlayerPosition;
                c0->wpos[0] = dpp[0];
                c0->wpos[1] = dpp[1];
                c0->wpos[2] = dpp[2];
                c0->wpos[1] = (f32)(lbl_80346078 +
                    FloorPos(lbl_80344880, lbl_80346080, c0->wpos, 0));
                c0->mode = fn_80051480(c0->wpos);
                c0->pyr[0] = lbl_80345EC8;
                yaw = get_yaw((f32*)(sMilestones + c0->mode * 104 + 48),
                              c0->wpos);
                c0->num1 = yaw;
                c0->pyr[1] = yaw;
                c0->pyr[2] = lbl_80345EC8;
                c0->radius = lbl_80345F14;
                CreateYPRMatrix(mat, c0->pyr);
                in[0] = lbl_80345EC8;
                in[1] = lbl_80345EC8;
                in[2] = c0->radius;
                WorldVector(in, out, mat);
                c0->attn[0] = c0->wpos[0] + out[0];
                c0->attn[1] = c0->wpos[1] + out[1];
                c0->attn[2] = c0->wpos[2] + out[2];
                r = c0->radius;
                d[0] = c0->attn[0] - c0->wpos[0];
                d[1] = c0->attn[1] - c0->wpos[1];
                d[2] = c0->attn[2] - c0->wpos[2];
                SlowNormalVector(d);
                k = (Camera*)((void*)(cs + 200));
                c0->attn[0] = d[0] * r + c0->wpos[0];
                c0->attn[1] = d[1] * r + c0->wpos[1];
                c0->attn[2] = d[2] * r + c0->wpos[2];
                saveW[0] = *(f32*)(cs + 300);
                saveW[1] = *(f32*)(cs + 304);
                saveW[2] = *(f32*)(cs + 308);
                saveA[0] = c0->attn[0];
                saveA[1] = c0->attn[1];
                saveA[2] = c0->attn[2];
                StandardCamera_8002B828(0);
                DoShake(saveW, saveA);
                look[0] = saveA[0] - saveW[0];
                look[1] = saveA[1] - saveW[1];
                look[2] = saveA[2] - saveW[2];
                LookInDirection(look, &k->mat[0][0]);
                c0->trans_mode = -1;
                c0->state = 1;
            } else {
                s16* hdr = *(s16**)((u8*)gCurLevel + 96);
                gNumEnemies = hdr[26];
                lbl_8034441C = hdr[19];
                if (lbl_8034441C < 0 && sSpecialTransmitter == 0) {
                    lbl_8034441C = 0;
                    hdr[19] = 0;
                }
                switch (lbl_8034441C) {
                case 0: {
                    f32 d[3];
                    WorldInfo* wi;
                    f32* pa0;
                    f32* pa1;
                    f32* pa2;
                    f32* wcy;
                    f32* prad;
                    f32* pw0;
                    f32* pw1;
                    f32* pw2;
                    f32 g;
                    f32 r;
                    CAM_SET_CMODE((Camera*)(cs + 200), 5);
                    CAM_SET_AMODE((Camera*)(cs + 200), 1);
                    wi = &gWorldInfo;
                    pa0 = (f32*)(cs + 500);
                    pa1 = (f32*)(cs + 504);
                    pa2 = (f32*)(cs + 508);
                    wcy = &gWorldInfo.worldcenter[1];
                    *(f32*)(cs + 500) = wi->worldcenter[0];
                    *(f32*)(cs + 504) = wi->worldcenter[1];
                    *(f32*)(cs + 508) = wi->worldcenter[2];
                    *(f32*)(cs + 504) = wi->worldmax[1];
                    {
                        f64 step = lbl_80345EF0;
                        do {
                            g = FloorPos(lbl_80344880, lbl_80346080, pa0, 0);
                            if (g != lbl_80344880) {
                                break;
                            }
                            if (g <= wi->worldmin[1]) {
                                break;
                            }
                            *pa1 = (f32)(*pa1 - step);
                        } while (1);
                    }
                    if (g != lbl_80344880) {
                        *pa1 = (f32)(lbl_80346238 + g);
                    } else {
                        *pa1 = *wcy;
                    }
                    if (*pa1 < gDefaultPlayerPosition[1]) {
                        *pa1 = gDefaultPlayerPosition[1];
                    }
                    *(f32*)(cs + 396) = *(f32*)((u8*)hdr + 40);
                    prad = (f32*)(cs + 396);
                    *(f32*)(cs + 424) = (f32)(lbl_80346240 * *(f32*)(cs + 396));
                    *(f32*)(cs + 428) = (f32)(lbl_80345EF0 + *wcy);
                    *(f32*)(cs + 432) = (f32)(lbl_80345FE0 + *(f32*)(cs + 428));
                    *(f32*)(cs + 364) = lbl_80346248;
                    *(f32*)(cs + 368) = lbl_80345EC8;
                    *(f32*)(cs + 372) = lbl_80345EC8;
                    CreateYPRMatrix(mat, (f32*)(cs + 364));
                    in[0] = lbl_80345EC8;
                    in[1] = lbl_80345EC8;
                    in[2] = *prad;
                    WorldVector(in, out, mat);
                    pw0 = (f32*)(cs + 300);
                    pw1 = (f32*)(cs + 304);
                    pw2 = (f32*)(cs + 308);
                    *(f32*)(cs + 300) = *pa0 + out[0];
                    *(f32*)(cs + 304) = *pa1 + out[1];
                    *(f32*)(cs + 308) = *pa2 + out[2];
                    r = *prad;
                    d[0] = *(f32*)(cs + 300) - *pa0;
                    d[1] = *(f32*)(cs + 304) - *pa1;
                    d[2] = *(f32*)(cs + 308) - *pa2;
                    SlowNormalVector(d);
                    *pw0 = d[0] * r + *pa0;
                    *pw1 = d[1] * r + *pa1;
                    *pw2 = d[2] * r + *pa2;
                    *(s32*)(cs + 200) = 1;
                    lbl_80344420 = 1800;
                    break;
                }
                case 1: {
                    f32 d[3];
                    WorldInfo* wi;
                    f32* dpp;
                    f32* p0;
                    f32* p1;
                    f32* p2;
                    f32* prad;
                    f32* pa0;
                    f32* pa1;
                    f32* pa2;
                    f32 r;
                    CAM_SET_CMODE((Camera*)(cs + 200), 2);
                    CAM_SET_AMODE((Camera*)(cs + 200), 1);
                    wi = &gWorldInfo;
                    dpp = gDefaultPlayerPosition;
                    p0 = (f32*)(cs + 300);
                    p1 = (f32*)(cs + 304);
                    p2 = (f32*)(cs + 308);
                    prad = (f32*)(cs + 396);
                    *(f32*)(cs + 300) = wi->worldcenter[0];
                    *(f32*)(cs + 304) = wi->worldcenter[1];
                    *(f32*)(cs + 308) = wi->worldcenter[2];
                    *(f32*)(cs + 304) = (f32)(lbl_80346250 + dpp[1]);
                    *(f32*)(cs + 396) = lbl_80346258;
                    *(f32*)(cs + 364) = *(f32*)((u8*)hdr + 40);
                    *(f32*)(cs + 368) = lbl_80345EC8;
                    *(f32*)(cs + 372) = lbl_80345EC8;
                    CreateYPRMatrix(mat, (f32*)(cs + 364));
                    in[0] = lbl_80345EC8;
                    in[1] = lbl_80345EC8;
                    in[2] = *prad;
                    WorldVector(in, out, mat);
                    pa0 = (f32*)(cs + 500);
                    pa1 = (f32*)(cs + 504);
                    pa2 = (f32*)(cs + 508);
                    *(f32*)(cs + 500) = *p0 + out[0];
                    *(f32*)(cs + 504) = *p1 + out[1];
                    *(f32*)(cs + 508) = *p2 + out[2];
                    r = *prad;
                    d[0] = *(f32*)(cs + 500) - *p0;
                    d[1] = *(f32*)(cs + 504) - *p1;
                    d[2] = *(f32*)(cs + 508) - *p2;
                    SlowNormalVector(d);
                    *pa0 = d[0] * r + *p0;
                    *pa1 = d[1] * r + *p1;
                    *pa2 = d[2] * r + *p2;
                    *(s32*)(cs + 200) = 1;
                    lbl_80344420 = 1500;
                    break;
                }
                default: {
                    void dbgTextPrintfCol(s32 x, s32 line, char* fmt, ...);
                    f32 m2[16];
                    f32 saveW[3];
                    f32 saveA[3];
                    f32 d2[3];
                    u8* st = sSpecialTransmitter;
                    f32* p0 = (f32*)(cs + 300);
                    f32* p1 = (f32*)(cs + 304);
                    f32* p2 = (f32*)(cs + 308);
                    Camera* k;
                    *(f32*)(cs + 300) = *(f32*)(st + 4);
                    *(f32*)(cs + 304) = *(f32*)(st + 8);
                    *(f32*)(cs + 308) = *(f32*)(st + 12);
                    *(f32*)(cs + 364) = *(f32*)(st + 20);
                    *(f32*)(cs + 368) = *(f32*)(st + 24);
                    *(f32*)(cs + 372) = *(f32*)(st + 28);
                    CreateYPRMatrix(m2, c0->pyr);
                    *(f32*)(cs + 396) = lbl_80346148;
                    in[0] = lbl_80345EC8;
                    in[1] = lbl_80345EC8;
                    in[2] = *(f32*)(cs + 396);
                    WorldVector(in, out, m2);
                    *(f32*)(cs + 500) = *p0 + out[0];
                    *(f32*)(cs + 504) = *p1 + out[1];
                    *(f32*)(cs + 508) = *p2 + out[2];
                    CAM_SET_CMODE(c0, 2);
                    CAM_SET_AMODE(c0, 1);
                    c0->trans_mode = 0;
                    k = (Camera*)((void*)(cs + 200));
                    saveW[0] = *(f32*)(cs + 300);
                    saveW[1] = *(f32*)(cs + 304);
                    saveW[2] = *(f32*)(cs + 308);
                    saveA[0] = c0->attn[0];
                    saveA[1] = c0->attn[1];
                    saveA[2] = c0->attn[2];
                    StandardCamera_8002B828(0);
                    DoShake(saveW, saveA);
                    d2[0] = saveA[0] - saveW[0];
                    d2[1] = saveA[1] - saveW[1];
                    d2[2] = saveA[2] - saveW[2];
                    LookInDirection(d2, &k->mat[0][0]);
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
                        u8* tc = sTriggerCameras;
                        f64 v;
                        lbl_80344444 = get_pitch(k->wpos,
                            (f32*)(tc + lbl_8034450C * 40 + 4));
                        lbl_80344448 = get_yaw(k->wpos,
                            (f32*)(tc + lbl_8034450C * 40 + 4));
                        v = (f64)(f32)(lbl_803460B0 * (lbl_803460B8 *
                            (f64)FixAngle((f32)(lbl_80345F60 -
                                                (f64)lbl_80344448))));
                        if (v < (f64)lbl_80345EC8) {
                            v = (f64)(f32)(v + lbl_803460C0);
                        }
                        if (v > lbl_803460C8) {
                            v = lbl_80345EC8;
                        }
                        if (lbl_80344A28 == 0) {
                            dbgTextPrintfCol(2, 3, lbl_80111B3C,
                                (s32)(lbl_803460B0 * (lbl_803460B8 *
                                                     (f64)lbl_80344444)),
                                v);
                        }
                    }
                    lbl_8034442C = lbl_80344444;
                    lbl_80344430 = lbl_80344448;
                    {
                        f32 p = c0->pyr[0];
                        lbl_80344530 = p;
                        lbl_8034443C = p;
                    }
                    {
                        f32 q = lbl_80344530;
                        f32 y = c0->pyr[1];
                        lbl_80344534 = y;
                        lbl_80344440 = y;
                        lbl_80344408 = q;
                    }
                    break;
                }
                }
            }
        } else if (gCurLevel != 0) {
            s16* hdr = *(s16**)((u8*)gCurLevel + 96);
            CAM_SET_CMODE(c0, 1);
            CAM_SET_AMODE(c0, 0);
            *(f32*)(cs + 500) = lbl_80345EC8;
            *(f32*)(cs + 504) = lbl_80345EC8;
            *(f32*)(cs + 508) = lbl_80345EC8;
            *(f32*)(cs + 396) = lbl_8034625C;
            *(f32*)(cs + 300) = lbl_80345EC8;
            *(f32*)(cs + 304) = lbl_80345EC8;
            *(f32*)(cs + 308) = lbl_80345EC8;
            lbl_80344538 = hdr[0];
            {
                f32 t = *(f32*)((u8*)hdr + 4);
                lbl_80344408 = t;
                lbl_80344530 = t;
            }
            lbl_80344404 = hdr[1];
            lbl_80344524 = *(f32*)((u8*)hdr + 8);
            lbl_8034452C = *(f32*)((u8*)hdr + 44);
            lbl_80344528 = *(f32*)((u8*)hdr + 48);
            lbl_803447F8 = 18000;
            gNumEnemies = hdr[26];
            uiFov = scrH == 256 ? 42 : 64;
            *(s32*)(cs + 200) = 1;
        }
    }

    lbl_80344534 = lbl_80118B60[lbl_80344538];
    for (i = 0; i < 6; i++) {
        u8* row = cs + i * 396;
        *(f32*)(row + 252) = *(f32*)(row + 300);
        *(f32*)(row + 256) = *(f32*)(row + 304);
        *(f32*)(row + 260) = *(f32*)(row + 308);
    }
    for (i = 0; i < 3; i++) {
        *(f32*)(cs + 16 + i * 4) = lbl_80345EC8;
    }
    {
        s32 zi = 0;
        u8* q;
        CameraTarget* t;
        gCameraWindowLeftLimit = zi;
        gCameraWindowRightLimit = scrW;
        gCameraWindowTopLimit = scrH;
        gCameraWindowBottomLimit = uiFov;
        gCameraWindowScaleX = lbl_80345F80;
        gCameraWindowScaleY = lbl_80345F80;
        lbl_803444BC = zi;
        ChangeWindow();
        MBWindowZoom(lbl_80346260);
        q = lbl_80344EE8;
        lbl_80344520 = (s32)(*(f32*)(q + 8) - *(f32*)(q + 20) * lbl_8034601C);
        lbl_8034451C = (s32)(*(f32*)(q + 20) * lbl_8034601C + *(f32*)(q + 8));
        lbl_80344518 = (s32)(*(f32*)(q + 24) * lbl_8034601C + *(f32*)(q + 12));
        lbl_80344514 = (s32)(*(f32*)(q + 12) - *(f32*)(q + 24) * lbl_8034601C);
        t = (CameraTarget*)(cs + 2576);
        for (i = 0; i < 15; i++, t++) {
            t->active = zi;
            t->object = zi;
            t->x = lbl_80345EC8;
            t->y = lbl_80345EC8;
            t->z = lbl_80345EC8;
        }
    }
    gCameraTargetCount = 0;
    ProcCamera_8002E548(0, 0);
    cam = (Camera*)(cs + 200);
    for (i = 0; i < 6; i++, cam++) {
        cam->limit_pos[0] = cam->mat[3][0];
        cam->limit_pos[1] = cam->mat[3][1];
        cam->limit_pos[2] = cam->mat[3][2];
        cam->limit_vel[0] = lbl_80345EC8;
        cam->limit_vel[1] = lbl_80345EC8;
        cam->limit_vel[2] = lbl_80345EC8;
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
    s32 halfXi;
    s32 halfYi;
    s32 centerX;
    s32 centerY;

    halfXi = (s32)(lbl_80345F18 *
        (f64)(gCameraWindowRightLimit - gCameraWindowLeftLimit));
    centerX = (s32)(lbl_80345F18 *
        (f64)(gCameraWindowRightLimit + gCameraWindowLeftLimit));
    halfYi = (s32)(lbl_80345F18 *
        (f64)(gCameraWindowTopLimit - gCameraWindowBottomLimit));
    centerY = (s32)(lbl_80345F18 *
        (f64)(gCameraWindowTopLimit + gCameraWindowBottomLimit));
    lbl_803444AC = (s32)((f32)centerX - (f32)halfXi * gCameraWindowScaleY);
    lbl_803444B0 = (s32)((f32)centerX + (f32)halfXi * gCameraWindowScaleY);
    lbl_803444B4 = (s32)((f32)centerY + (f32)halfYi * gCameraWindowScaleX);
    lbl_803444B8 = (s32)((f32)centerY - (f32)halfYi * gCameraWindowScaleX);
    if (lbl_803444AC < gCameraWindowLeftLimit) {
        lbl_803444AC = gCameraWindowLeftLimit;
    }
    if (lbl_803444B0 > gCameraWindowRightLimit) {
        lbl_803444B0 = gCameraWindowRightLimit;
    }
    if (lbl_803444B4 > gCameraWindowTopLimit) {
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
    if (cam->a_mode == ATN_FREE && cam->c_mode != CAM_OBJEYE &&
        cam->c_mode != CAM_VECDIST) {
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
extern f64 lbl_803460C0, lbl_803460C8;
void fn_800C02F4(s32 color);
void dbgTextPrintfCol(s32 column, s32 row, char* format, ...);
extern s32 EnemyDescType(char* desc);
extern char* lbl_8011B578[];

typedef struct DebugNameTables {
    u8 pad00[0x10];
    char* type[14];
    char* subtype[59];
    char* action[17];
} DebugNameTables;

/*
 * screen_limitation -- debug overlay printing camera camIdx's world position,
 * attention, orientation, distance and its camera-/attention-mode names.  Gated
 * by the debug flag (sFlags & 1); no effect in a normal build.
 */
void screen_limitation(s32 camIdx)
{
    Camera* cam;
    DebugNameTables* debugNames = (DebugNameTables*)lbl_80118B60;
    register volatile s32 buttons = gControllerButtons.word.buttons;
    s32 row;
    f32 yaw;
    f32 yawDeg;

    if (((buttons & 0) | (sFlags & 1)) == 0) {
        return;
    }
    cam = &gCameras[lbl_8034453C];
    row = MBScreenHeight();
    MBScreenWidth();
    row /= 8;
    if (row != 0x20) {
        row -= 2;
    }
    yaw = FixAngle((f32)(lbl_80345F60 - (f64)*(f32*)((u8*)cam + 0xA8)));
    yawDeg = (f32)(lbl_803460B0 * (lbl_803460B8 * (f64)yaw));
    if (yawDeg < lbl_80345EC8) {
        yawDeg = (f32)(yawDeg + lbl_803460C0);
    }
    if (yawDeg > lbl_803460C8) {
        yawDeg = *(volatile f32*)&lbl_80345EC8;
    }
    fn_800C02F4(0xFF00);
    dbgTextPrintfCol(1, row - 0xC, "CAM: %.2f %.2f %.2f    ",
                     cam->wpos[0], cam->wpos[1], cam->wpos[2]);
    dbgTextPrintfCol(1, row - 0xB, "ATN: %.2f %.2f %.2f    ",
                     cam->attn[0], cam->attn[1], cam->attn[2]);
    dbgTextPrintfCol(1, row - 0xA, "YAW=%.1f(%.2f)    ",
                     yawDeg, cam->pyr[1]);
    dbgTextPrintfCol(1, row - 9, "PITCH=%d    ",
                     (s32)(lbl_803460B0 *
                           (lbl_803460B8 * (f64)cam->pyr[0])));
    dbgTextPrintfCol(1, row - 8, "DISTANCE:  %.2f    ",
                     cam->radius);
    dbgTextPrintfCol(1, row - 7, "CAM=");
    switch (cam->c_mode) {
    case 0:  dbgTextPrintfCol(5, row - 7, "OFF   "); break;
    case 1:  dbgTextPrintfCol(5, row - 7, "FREE   "); break;
    case 2:  dbgTextPrintfCol(5, row - 7, "LOCK   "); break;
    case 3:  dbgTextPrintfCol(5, row - 7, "GAME   "); break;
    case 4:  dbgTextPrintfCol(5, row - 7, "OBJEYE "); break;
    case 5:  dbgTextPrintfCol(5, row - 7, "VECDIST"); break;
    case 6:  dbgTextPrintfCol(5, row - 7, "POINT  "); break;
    case 7:  dbgTextPrintfCol(5, row - 7, "DRAGON "); break;
    case 8:  dbgTextPrintfCol(5, row - 7, "CHIMERA"); break;
    case 9:  dbgTextPrintfCol(5, row - 7, "GENIE  "); break;
    case 10: dbgTextPrintfCol(5, row - 7, "DRIDER "); break;
    case 11: dbgTextPrintfCol(5, row - 7, "DEMON  "); break;
    case 12: dbgTextPrintfCol(5, row - 7, "BOSS   "); break;
    default: dbgTextPrintfCol(5, row - 7, "UNKNOWN"); break;
    }

    dbgTextPrintfCol(0xE, row - 7, "ATN=");
    switch (cam->a_mode) {
    case 0:
        dbgTextPrintfCol(0x12, row - 7, "FREE        ");
        dbgTextPrintfCol(0xE, row - 6, "                               ");
        break;
    case 1:
        dbgTextPrintfCol(0x12, row - 7, "LOCK        ");
        dbgTextPrintfCol(0xE, row - 6, "                               ");
        break;
    case 3:
        dbgTextPrintfCol(0x12, row - 7, "OBJECT      ");
        dbgTextPrintfCol(0xE, row - 6, "                               ");
        break;
    case 2:
        dbgTextPrintfCol(0x12, row - 7, "TARGET      ");
        dbgTextPrintfCol(0xE, row - 6, "                               ");
        break;
    case 4:
        dbgTextPrintfCol(0x12, row - 7, "POINT       ");
        dbgTextPrintfCol(0xE, row - 6, "                               ");
        break;
    case 5:
        dbgTextPrintfCol(0x12, row - 7, "PLAYER %02X   ",
                         *(s32*)((u8*)cam + 0x100));
        dbgTextPrintfCol(0xE, row - 6, "                               ");
        break;
    case 6: {
        s32 enemy = *(s32*)((u8*)cam + 0x104);
        u8* e = gEnemies + enemy * ENEMY_STRIDE;

        dbgTextPrintfCol(0x12, row - 7, "ENEMY %02X    ", enemy);
        dbgTextPrintfCol(0xE, row - 6, "%s (AI=%d)                     ",
                         lbl_8011B578[*(s32*)e], *(s16*)(e + 0x310));
        break;
    }
    case 8:
        dbgTextPrintfCol(0x12, row - 7, "MILESTONE %02X",
                         *(s32*)((u8*)cam + 0x10C));
        dbgTextPrintfCol(0xE, row - 6, "                               ");
        break;
    case 9:
        dbgTextPrintfCol(0x12, row - 7, "LOOKOUT %02X  ",
                         *(s32*)((u8*)cam + 0x110));
        dbgTextPrintfCol(0xE, row - 6, "                               ");
        break;
    case 10:
        dbgTextPrintfCol(0x12, row - 7, "CAMERA %02X   ",
                         *(s32*)((u8*)cam + 0x114));
        dbgTextPrintfCol(0xE, row - 6, "                               ");
        break;
    case 7: {
        s32 index = *(s32*)((u8*)cam + 0x108);
        u8* item = (u8*)&sItems[index];
        u8* info;
        s32 type;

        dbgTextPrintfCol(0x12, row - 7, "ITEM %02X (%dP)", index,
                         (s8)item[0xCC]);
        info = *(u8**)item;
        type = *(s32*)info;
        if (type < 0) {
            type = 0;
        }
        switch (*(s32*)info) {
        case 2: {
            u8* record = (u8*)gWorldInfo.iteminfo + *(s16*)(item + 0xDC) * 0x50;
            s32 recordType = *(s32*)record;

            if (recordType < 0) {
                recordType = 0;
            }
            switch (recordType) {
            case 4:
                dbgTextPrintfCol(0xE, row - 6, "%s (%s)                        ",
                                 debugNames->type[type],
                                 lbl_8011B578[EnemyDescType((char*)record + 0x28)]);
                break;
            case 1:
                dbgTextPrintfCol(0xE, row - 6, "%s (%s)                        ",
                                 debugNames->type[type],
                                 debugNames->subtype[*(s32*)(record + 4)]);
                break;
            default:
                if (*(s32*)(info + 4) == 0x30) {
                    dbgTextPrintfCol(0xE, row - 6, "%s (%s)                        ",
                                     debugNames->type[type],
                                     debugNames->type[recordType]);
                } else {
                    record = (u8*)gWorldInfo.iteminfo + *(s16*)(record + 8) * 0x50;
                    dbgTextPrintfCol(0xE, row - 6, "%s (%s)                        ",
                                     debugNames->type[type],
                                     debugNames->subtype[*(s32*)(record + 4)]);
                }
                break;
            }
            break;
        }
        case 3:
            dbgTextPrintfCol(0xE, row - 6, "%s (%s-%d) Lv%d Max=%d   ",
                             debugNames->type[type],
                             lbl_8011B578[*(s16*)(item + 0xDC)],
                             (s8)item[0xE3], (s8)item[0xE2], (s8)item[0xDF]);
            break;
        case 7:
            dbgTextPrintfCol(0xE, row - 6, "%s (%s)                      ",
                             debugNames->type[type],
                             debugNames->action[(s8)item[0xC8]]);
            break;
        case 11:
            dbgTextPrintfCol(0xE, row - 6, "%s (%d)                      ",
                             debugNames->type[type],
                             *(s32*)(item + 0xDC));
            break;
        case 8:
        case 9:
            dbgTextPrintfCol(0xE, row - 6, "%s                          ",
                             debugNames->type[type]);
            break;
        case 10:
            if ((s8)item[0xCF] >= 0) {
                dbgTextPrintfCol(0xE, row - 6, "%s (HP=%d)                    ",
                                 debugNames->type[type],
                                 *(s16*)(item + 0xD0));
            } else {
                dbgTextPrintfCol(0xE, row - 6, "%s (%s)            ",
                                 debugNames->type[type],
                                 (char*)info + 0x28);
            }
            break;
        case 12:
            dbgTextPrintfCol(0xE, row - 6, "%s (GRP=%d)                  ",
                             debugNames->type[type],
                             *(s32*)(info + 4));
            break;
        case 13:
            dbgTextPrintfCol(0xE, row - 6, "%s (%s: RAD=%d)             ",
                             debugNames->type[type],
                             (char*)info + 0x28,
                             (s32)*(f32*)(item + 0xDC));
            break;
        case -1: {
            s32 subtype = *(s32*)(info + 4);
            if (subtype < 0) {
                subtype = 0;
            }
            dbgTextPrintfCol(0xE, row - 6, "%s (%s)                      ",
                             debugNames->type[type], debugNames->subtype[subtype]);
            break;
        }
        default: {
            s32 subtype = *(s32*)(info + 4);
            if (subtype < 0) {
                subtype = 0;
            }
            dbgTextPrintfCol(0xE, row - 6, "%s (%s)                      ",
                             debugNames->type[type], debugNames->subtype[subtype]);
            break;
        }
        }
        break;
    }
    default:
        dbgTextPrintfCol(0x12, row - 7, "UNKNOWN     ");
        dbgTextPrintfCol(0xE, row - 6, "                               ");
        break;
    }
    dbgTextPrintfCol(1, row - 6, "MODE=%d ", *(s32*)((u8*)cam + 0xD0));
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

    if ((gControllerButtons.both & 8) != 0) {
        freeze = 1;
        if ((sPreviousFlags.both & ((u64)8 << 32)) != 0) {
            sClockAccumulator = 0;
            freeze = 0;
            resumed = 1;
        } else if ((gControllerButtons.both & ((u64)8 << 32)) != 0) {
            if (sClockAccumulator >= 60) {
                sClockAccumulator -= 4;
                freeze = 0;
            } else {
                sClockAccumulator += gClockStepTicks;
            }
        }
    }
    if ((gControllerButtons.both & 4) != 0 &&
        gGameMode == 0x4010) {
        freeze = 1;
    }

    gFrameTicks = *(volatile u32*)&pbLoad - sLastTimerCount;
    gClockStepTicks = gFrameTicks;
    sLastTimerCount = *(volatile u32*)&pbLoad;
    gClockCurrentTime = pbGetTime();
    gClockElapsedTime = gClockCurrentTime - sLastFrameTime;
    sLastFrameTime = gClockCurrentTime;

    if (options_state != 0 || gModalRenderDepth != 0 || (freeze && !resumed)) {
        gFrameTicks = 0;
        if (options_state != 100) {
            gClockElapsedTime = 0;
        }
        gClockFrameStep = lbl_803462E8;
        gClockFrameReciprocal = lbl_803462EC;
    } else if (resumed || gFrameTicks > 60 || gFrameTicks == 0) {
        gFrameTicks = 2;
        gClockElapsedTime = 10000000;
        gClockFrameStep = lbl_803462F0;
        gClockFrameReciprocal = lbl_803462EC;
    } else {
        gClockFrameStep = (f32)gFrameTicks / lbl_803462F4;
        gClockFrameReciprocal = lbl_803462F8 / gClockFrameStep;
    }
    if (gClockElapsedTime > 300000000) {
        gClockElapsedTime = 10000000;
    }
    if (!freeze && options_state == 0) {
        sMusicFadeBase += gClockFrameStep;
        if (sMusicFadeBase > 18000.0f) {
            sMusicFadeBase =
                *(volatile f32*)&sMusicFadeBase - 18000.0f;
        }
        gClockFrameNumber =
            (s32)(1000.0f * *(volatile f32 *)&sMusicFadeBase + 0.5f);
        gClockTime = *(volatile f32*)&sMusicFadeBase;
        InfFrame++;
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
        PF(player, 0x918, s32) = 0;
    }
    if (flag != 0) {
        s32* count = (s32*)((u8*)player +
                           PF(player, 0x0C, s32) * 0x1C);
        count[0xC20 / sizeof(s32)] = count[0xC20 / sizeof(s32)] + 1;
    }
    t = PF(item, 0xDC, s16);
    if (t == -2) {
        t = 1;
    } else if (t == -3) {
        t = 2;
    } else if (t < 0) {
        t = 0;
    }
    if (flag != 0) {
        AddExp(PF(player, 0, s32), lbl_8011BBA8[t] * 5, 0);
    } else {
        AddExp(PF(player, 0, s32), lbl_8011BB20[t] * 5, 0);
    }
    goto out;
small:
    if (PF(item, 0xC6, s16) > 0) {
        goto out;
    }
    switch (info[1]) {
    case 4:
    {
        f32 pos[3];
        f32 vec[3];
        s32 magic = info[0xF];
        pos[0] = *(f32*)((u8*)item + 0x44);
        pos[1] = *(f32*)((u8*)item + 0x48);
        pos[2] = *(f32*)((u8*)item + 0x4C);
        vec[0] = *(f32*)((u8*)player + 0x54);
        vec[1] = *(f32*)((u8*)player + 0x58);
        vec[2] = *(f32*)((u8*)player + 0x5C);
        start_magic(PF(player, 0, s32), (s32*)pos, magic, 0,
                    lbl_80346310);
        msgPost(0xE, PF(player, 0, s32), (u32)vec);
        DeleteItem(item, 1);
        break;
    }
    }
out:
    return;
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
        u8* enemy = gEnemies + i * ENEMY_STRIDE;
        s32 state = PF(enemy, 0xB4, s32);
        f32* cool;
        if ((state == 1 || state == 6) &&
            (!respectCooldown ||
             !(sMusicFadeBase < (cool = (f32*)(enemy + 0x2B4))[cooldownSlot]))) {
            u8 _pad[8];
            f32 dx;
            f32 enemyRadius;
            f32 dz = PF(enemy, 0x5C, f32) - to[2];
            f32 enemyHeight;
            f32 eh2;
            enemyRadius = radius + PF(enemy, 0x238, f32);
            dx = PF(enemy, 0x54, f32) - to[0];
            enemyHeight = radius + PF(enemy, 0x23C, f32);
            eh2 = enemyHeight;
            if (!(dx * dx + dz * dz > enemyRadius * enemyRadius + horizontalLen2) &&
                !(PF(enemy, 0x58, f32) - to[1] > verticalLen2 + enemyHeight) &&
                LineCylinderCollide((f32*)(enemy + 0x54), enemyRadius,
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
        FatalError(lbl_80111E28, 0x800000);
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
    fn_80093E50(fx, vel, desc->angularVelocity, desc->weight, radius);
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
        fn_80093D98(fx, tex, vibColor, vibIntensity, lbl_80346328,
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
extern f64 lbl_80346318, lbl_80346360, lbl_80346370;
extern f64 lbl_80346378, lbl_80346388, lbl_80346390, lbl_80346398;
extern f32 lbl_80346368, lbl_80346358, lbl_8034635C, lbl_80346380, lbl_80346334;
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
        f32 weight = desc->weight;
        f32 lead = (f32)((f64)PF(gCurLevel, 0xC8, f32) *
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
    if (flat[0] * PF(enemy, 0x24, f32) + flat[2] * PF(enemy, 0x2C, f32) <
        lbl_80346378) {
        return 0;
    }
    {
        flags = PF(enemy, 0xC4, u32);
        aim[0] = target[0];
        aim[1] = target[1];
        aim[2] = target[2];
        if (slot == 2) {
            height = lbl_80346328;
        } else {
            height = lbl_80346380;
        }
        switch (PF(enemy, 0x00, s32)) {
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

extern u8 lbl_8011A1A8[];
typedef struct MissileSpread {
    f32 value[5];
} MissileSpread;
extern MissileSpread lbl_80111DE0, lbl_80111DF4;
extern void* EffectInfo[];
extern void *BossAcidTree, *BossElecTree, *BallistaTree;
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
    s32 idx = player[0];
    s32* missileFx = pmissile_sfxidx;
    MissileTreeInfo* treeInfo = &PlayerMissileTreeInfo[idx];
    s32 pflags = player[0x49];
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
        desc = &PlayerMissileInfo[player[2]];
    }
    scale = *(f32*)((u8*)player + 0x114) * scaleArg;

    if ((pflags & 0x400) != 0) {
        MulVecMat3((f32*)lbl_8011A1A8, aim, (f32*)(player + 5));
    } else {
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
                                     0, player[0]);
        if (hit != 0) {
            f32 dmg = fn_8005C1DC(hit, scale, damageType, player[0]);
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
        if ((f64)*(f32*)((u8*)player + 0x118) < lbl_80346318) {
            clampedValue = lbl_80346318;
        } else if ((f64)*(f32*)((u8*)player + 0x118) > lbl_803463A8) {
            clampedValue = lbl_803463A8;
        } else {
            clampedValue = (f64)*(f32*)((u8*)player + 0x118);
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
            s32 f = player[0x49];
            if ((f & 0x8000) != 0) {
                extraFlags |= 0x10000;
                tree = BossElecTree;
            } else if ((f & 0x4000) != 0) {
                extraFlags |= 0x10000;
                tree = BossAcidTree;
            } else if ((f & 0x400) != 0) {
                extraFlags |= 0x10000;
                tree = PhoenixTree;
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
                        if ((player[2] == 2 || player[2] == 6) &&
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
                    if (player[0xCC9] >= 99) {
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

extern void *BallistaTree, *BossElecTree, *BossAcidTree, *lbl_803445B0;
extern void *WingsTree, *PojoTree, *BreatheFireTree, *BreatheElecTree;
extern void *BreatheAcidTree, *FireShieldTree, *PhoenixTree;
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
    s32 idx = PF(player, 0x00, s32);
    s32 charType = PF(player, 0x0C, s32);
    s32 throwLevel = PF(player, 0x3324, s32) / 10;
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
        sprintf(buf, "%s_THROW_%c", charName, (char)throwByte);
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
        lbl_803445B0 = AtreeMatch(sPowerupsBuf, "PHOENIX", 1);
        WingsTree = AtreeMatch(sPowerupsBuf, "WINGS", 1);
        PojoTree = AtreeMatch(sPowerupsBuf, "POJO", 1);
        BreatheFireTree = AtreeMatch(sPowerupsBuf, "HEAD_BREATHEF", 1);
        BreatheElecTree = AtreeMatch(sPowerupsBuf, "HEAD_BREATHEE", 1);
        BreatheAcidTree = AtreeMatch(sPowerupsBuf, "HEAD_BREATHEA", 1);
    } else {
        lbl_803445B0 = 0;
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
    PhoenixTree = AtreeMatch(sWeaponsBuf, "PHOENIX_FBALL", 1);
    if (missing) {
        FatalError("InitPlayerMissiles failed!", 0x800000);
    }
}

void ResetPlayerMissiles(void)
{
    memset(PlayerMissileTreeInfo, 0, sizeof(PlayerMissileTreeInfo));
}
