/* camera.c -- head of CAMERA.OBJ (game camera system), NonMatching.
 *
 * Function names recovered from shell3D.pdb (CAMERA.OBJ) where anchored by a
 * string or an unambiguous call signature, otherwise clear behavioral names
 * derived from the target asm (call-graph + string + data cross-reference).
 * CAMERA is a large, function-order-scrambled TU (the GC compiler did not emit
 * these in Xbox source order); this file covers only the head window
 * .text 0x80022734 .. 0x8002951C.  The remaining CAMERA functions
 * (0x8002951C onward, up to ~0x80031Exx) are still auto-split.
 *
 * Because this Object is registered NonMatching, the final DOL links the
 * ORIGINAL extracted bytes for this range; the C below is faithful
 * reconstruction for documentation / objdiff and does not need to byte-match.
 *
 * Window map (see config/GUNE5D/symbols.txt):
 *   0x80022734 get_screen_pos           world pos -> INT screen x,y            [global]  BODY
 *   0x80022794 get_actual_screen_pos    per-camera projection -> FLOAT x,y     [global]  BODY
 *   0x80022824 LookInDirection          build camera basis from a look vector  [global]  giant, doc-only
 *   0x800229D0 do_camera                top-level per-frame camera update       [global]  BODY
 *   0x80022DAC camera_init_for_gamemode setup driven by game-mode g_800229D0    (local)   giant, doc-only
 *   0x800231D4 camera_run_mode          camera-mode state machine (2 jumptables)(local)   BODY
 *   0x80023ED0 camera_mode_follow       largest mode handler (MB blit, project) (local)   BODY
 *   0x80024F30 camera_mode_target       atan2 aiming toward a target            (local)   BODY
 *   0x80025640 debug_camera_pos         object-type + position debug overlay    (local)   BODY
 *   0x80025CEC camera_debug_supervisor  largest fn; drives debug_camera_pos      (local)   BODY
 *   0x80026CA4 camera_request_change    small request/priority latch            (local)   BODY
 *   0x80026CF0 camera_mode_level        per-level scripted camera               (local)   giant, doc-only
 *   0x80027608 DoShake                  apply active shake offset to a position [global]  BODY
 *   0x80027838 ShakeCamera              trigger a shake (priority-gated)        [global]  BODY
 *   0x80027870 camera_orbit_update      orbit helper (-> camera_collide_step)   (local)   doc-only
 *   0x80027CA4 camera_collide_step      collision step (PointLineColl)           (local)   doc-only
 *   0x80028394 camera_mode_spin         sin/cos mode (-> camera_approach_yaw)   (local)   doc-only
 *   0x80028560 camera_approach_yaw      rate-limit a cam yaw toward a target    (local)   BODY
 *   0x80028670 camera_mode_orbit        sin/cos orbit mode (-> camera_lerp_yaw) (local)   doc-only
 *   0x80028938 camera_lerp_yaw          rate-limit one angle toward another     (local)   BODY
 *   0x80028A74 camera_mode_dest         scripted move-to ("DEST P=%d, Y=%4.1f") (local)   BODY
 *
 * Data globals in the 0x80344xxx range (shake/state) are SHARED with other
 * TUs (attract.c, sndfx.c, auxscreen.c, ...) and are left under their existing
 * names; only the shake_* set carries confident PDB names.
 */

#include "types.h"
#include "game/camera.h"
#include "game/player.h"

typedef struct Vec3 {
    f32 x;
    f32 y;
    f32 z;
} Vec3;

typedef struct CameraMilestone {
    f32 matrix[12];
    f32 position[3];
    u8 _pad3C[0x2C];
} CameraMilestone; /* 0x68 */

typedef struct CameraTarget {
    /* 0x00 */ s32 active;
    /* 0x04 */ u8* object;
    /* 0x08 */ f32 position[3];
    /* 0x14 */ u8 _pad14[4];
    /* 0x18 */ f32 projectedTop[2];
    /* 0x20 */ f32 projectedBottom[2];
    /* 0x28 */ f32 limitedTop[2];
    /* 0x30 */ f32 limitedBottom[2];
} CameraTarget; /* 0x38 */

/* Address-taken workspace used by camera_mode_level.  Keeping both camera
 * matrices in one object reproduces the retail function's 0x198-byte frame. */
typedef struct CameraLevelScratch {
    u8 _pad00[0x0C];
    Vec3 overheadDirection;        /* stack +0x18 */
    Vec3 overheadPosition;         /* stack +0x24 */
    Vec3 overheadAttention;        /* stack +0x30 */
    u8 _pad30[8];
    Vec3 transmitterDirection;     /* stack +0x44 */
    Vec3 transmitterPosition;      /* stack +0x50 */
    Vec3 transmitterAttention;     /* stack +0x5C */
    u8 _pad5C[8];
    volatile f32 transmitterRoot;  /* stack +0x70 */
    u8 _pad68[8];
    Vec3 levelDirection;           /* stack +0x7C */
    Vec3 levelPosition;            /* stack +0x88 */
    Vec3 levelAttention;           /* stack +0x94 */
    f32 normalizeLevel[3];         /* stack +0xA0 */
    u8 _padA0[0x14];
    f32 transmitterMatrix[16];     /* stack +0xC0 */
    u8 _padF4[0x0C];
    Vec3 transformed;              /* stack +0x10C */
    u8 _pad10C[0x0C];
    Vec3 offset;                   /* stack +0x124 */
    f32 overheadMatrix[16];        /* stack +0x130 */
    u8 _pad164[8];
} CameraLevelScratch; /* 0x16C; allocated at r1+0xC */

typedef struct CameraDebugScratch {
    f32 direction[3];              /* stack +0x18 */
    f32 position[3];               /* stack +0x24 */
    f32 attention[3];              /* stack +0x30 */
    f32 normalize[3];              /* stack +0x3C */
    volatile f32 root;             /* stack +0x48 */
    f32 desiredAttention[3];       /* stack +0x4C */
} CameraDebugScratch; /* 0x40; allocated above projected + pad */

/* Address-taken locals for camera_debug_supervisor.  The retail compiler
 * overlays the final saved player position on futurePosition. */
typedef struct CameraSupervisorScratch {
    u8 _pad00[0x14];
    volatile f32 movedRoot;          /* stack +0x44 */
    volatile f32 currentRoot;        /* stack +0x48 */
    f32 return4CurrentX;             /* stack +0x4C */
    f32 return4OldX;                 /* stack +0x50 */
    f32 return4CurrentY;             /* stack +0x54 */
    f32 return4OldY;                 /* stack +0x58 */
    f32 return3CurrentX;             /* stack +0x5C */
    f32 return3OldX;                 /* stack +0x60 */
    f32 return2CurrentY;             /* stack +0x64 */
    f32 return2OldY;                 /* stack +0x68 */
    s16 alternateProjected[2];       /* stack +0x6C */
    s16 projected[2];                /* stack +0x70 */
    u8 _pad68[0x30];
    f32 alternatePosition[3];        /* stack +0xA4 */
    u8 _padA4[8];
    f32 futurePosition[3];           /* stack +0xB8 */
} CameraSupervisorScratch; /* 0x94; allocated at r1+0x30 */

/* Address-taken roots and closest-point output used by camera_collide_step.
 * The gaps reproduce CAMERA.OBJ's 0x90-byte frame. */
typedef struct CameraCollideScratch {
    u8 _pad00[0x14];
    f32 finalAngle;                  /* stack +0x28 */
    volatile f32 closestRoot;        /* stack +0x2C */
    volatile f32 segmentRoot;        /* stack +0x30 */
    volatile f32 distanceRoot;       /* stack +0x34 */
    f32 verticalDifference;          /* stack +0x38 */
    u8 _pad2C[4];
    f32 closest[3];                  /* stack +0x40 */
    u8 _pad3C[12];
} CameraCollideScratch;

/* Camera field access (no full struct recovered; stride 0x18C). */
#define CAM_F32(c, off) (*(f32*)((u8*)(c) + (off)))
#define CAM_YAW_OFF 0xA8 /* yaw angle field used by camera_approach_yaw */

/* angle wrapping constants (double, to mirror the sdata2 pool) */
#define CAM_PI  3.141592653589793
#define CAM_2PI 6.283185307179586
/* per-frame turn/oscillation rates recovered from the sdata2 pool */
#define CAM_TURN_RATE_2DEG 0.03489724 /* lbl_80346000 (~2 deg) */
#define CAM_TURN_RATE_1DEG 0.01744862 /* lbl_80346068 (~1 deg) */
#define SHAKE_FREQ         0.66313    /* lbl_80346028 */

/* --- camera shake state (CAMERA.OBJ .sbss globals, names from PDB) --- */
extern s32 shake_type;      /* 0x80344478 */
extern s32 shaking;         /* 0x8034447C */
extern s32 shake_priority;  /* 0x80344480 */
extern s32 shake_count;     /* 0x80344484 */
extern s32 shake_delay;     /* 0x80344488 */
extern f32 shake_rad;       /* 0x8034448C */

/* --- shared globals referenced by this window (names kept from codebase) --- */
extern s32 gFrameTicks;    /* integer frame delta (shared w/ auxscreen.c) */
extern s32 gGameBusy;    /* shake pause flag A (shared w/ sndfx.c) */
extern s32 gGameplayPauseTimer;
extern s32 lbl_80343BD8;    /* camera-active gate (checked by do_camera too) */

/* camera_request_change latch (camera-internal state, purpose unconfirmed) */
extern s32 lbl_803444F8;    /* running-max request value */
extern s32 lbl_803444FC;    /* request-pending flag */
extern s32 lbl_80344500;    /* request mode */
extern f32 lbl_803444E8;    /* blend ratio (compared vs 0.9) */
extern f32 lbl_80345EC8;    /* CAMERA.OBJ shared 0.0f pool constant */
extern u8  gCameraState[];   /* camera supervisor state; gCameras begins at +0xC8 */
extern s32 gGameMode;
extern s32 gNumTransmitters;
extern s32 gScriptedCameraState;
extern s32 lbl_803447B8;
extern s32 gBossType;
extern s32 lbl_803444E0;
extern s32 lbl_803444E4;
extern s32 lbl_80344400;
extern s32 lbl_80344538;
extern s32 lbl_8034453C;
extern s32 gCameraTargetCount;
extern s32 gCameraTargetMode;
extern s32 gCameraTargetPositionCount;
extern s32 lbl_80344508;
extern s32 lbl_8034450C;
extern s32 lbl_80344510;
extern s32 sNumTriggerCameras;
extern s32 lbl_8034429C;
extern s32 lbl_80344288;
extern s32 lbl_80344404;
extern s32 lbl_8034446C;
extern s32 lbl_80344470;
extern s32 lbl_803444C8;
extern s32 lbl_803444CC;
extern s32 lbl_803444DC;
extern s32 lbl_803443F4;
extern s32 lbl_803443F8;
extern f32 lbl_8023F818;
extern f32 lbl_8023F81C;
extern f32 lbl_8023F820;
extern f32 lbl_80344534;
extern f32 lbl_80344530;
extern f32 lbl_80344524;
extern f32 lbl_80344504;
extern f32 lbl_80344408;
extern f32 lbl_8034444C;
extern f32 lbl_8034445C;
extern f32 lbl_80344454;
extern f32 lbl_80344450;
extern f32 lbl_80344458;
extern f32 lbl_80346100;
extern f32 lbl_80346110;
extern f32 lbl_803460F0;
extern f32 lbl_80118B60[];
extern s32 lbl_80118CD8[];
extern f64 lbl_80345F18;
extern f64 lbl_80345F20;
extern f64 lbl_80345F28;
extern f64 lbl_80345F58;
extern f64 lbl_80345F60;
extern f64 lbl_80345F68;
extern f64 lbl_80345F70;
extern f64 lbl_80345F78;
extern f32 lbl_80345F80;
extern f32 lbl_80346030;
extern f64 lbl_80346098;
extern f64 lbl_803460A0;
extern f64 lbl_803460A8;
extern f64 lbl_803460B0;
extern f64 lbl_803460B8;
extern f64 lbl_803460C0;
extern f64 lbl_803460C8;
extern f64 lbl_803460D0;
extern f64 lbl_803460D8;
extern f64 lbl_803460E8;
extern f64 lbl_803460E0;
extern f64 lbl_803460F8;
extern f64 lbl_80346108;
extern f64 lbl_80346118;
extern f32 lbl_80346010;
extern f32 lbl_80346014;
extern f32 lbl_80346018;
extern f32 lbl_8034601C;
extern f32 lbl_80346020;
extern f32 lbl_80346024;
extern f32 lbl_80346040;
extern f64 lbl_80346038;
extern f64 lbl_80346048;
extern f64 lbl_80346050;
extern f64 lbl_80346060;
extern f64 lbl_80346068;
extern f32 lbl_80346058;
extern s32 gGameOptions[];
extern u8 sTriggerCameras[];
extern u8 gPlayers[];
extern u8* gCurLevel;
extern u8* CurTransmitter;
extern u8 gWorldInfo[];
extern s32 gNumEnemies;
extern s32 lbl_80344414;
extern s32 lbl_8034441C;
extern s32 lbl_80344420;
extern s32 lbl_803444EC;
extern s32 lbl_803444F0;
extern f32 gClockFrameStep;
extern f32 lbl_80345F38;
extern f32 lbl_80345F14;
extern s32 lbl_803444F4;
extern s32 lbl_80344494;
extern s32 lbl_803447B4;
extern s32 lbl_803447B8;
extern s32 lbl_803443FC;
extern void* lbl_8034440C;
extern f32 lbl_80344460;
extern f32 lbl_80344424;
extern f32 lbl_80344428;
extern f32 lbl_8034442C;
extern f32 lbl_80344430;
extern f32 lbl_80344444;
extern f32 lbl_80344448;
extern f32 lbl_80344464;
extern f32 lbl_80344468;
extern f32 lbl_80344528;
extern s32 lbl_80344514;
extern s32 lbl_80344518;
extern s32 lbl_8034451C;
extern s32 lbl_80344520;
extern s32 lbl_80344960;
extern s32 lbl_80344A28;
extern u8* lbl_80344EE8;
extern f32 gCameraTargetPositions[7][3];
extern f32 gDefaultPlayerPosition[3];
extern u8 lbl_80240E30[];
extern f64 lbl_80345F50;
extern f64 lbl_80345F88;
extern f64 lbl_80345F90;
extern f64 lbl_80345F98;
extern f32 lbl_80345FA0;
extern f64 lbl_80345FA8;
extern f64 lbl_80345FB0;
extern f32 lbl_80345FB8;
extern f64 lbl_80345FC0;
extern f64 lbl_80345FC8;
extern f64 lbl_80345FD0;
extern f64 lbl_80345FD8;
extern f64 lbl_80345FE0;
extern f32 lbl_80345FE8;
extern f64 lbl_80345FF0;
extern f64 lbl_80346008;
extern char lbl_80111B3C[];
extern char lbl_80111A08[];

/* --- external projection / math helpers (G3D / pb layer) --- */
void MBWorldToScreen(f32* out_xy, void* world_pos);                   /* screen projection (INT path) */
void MBWindowProject(f32* world_pos, f32* camera_matrix, f32* out_eye,
                     s16* out_xy);
f32  FixAngle(f32 rad);                                            /* angle wrap/reduce for sin/cos */
f32  SlowNormalVector(f32* vector);
void CopyMat3(const f32* src, f32* dst);
/* CAMERA.OBJ uses the PS2-facing float trig ABI despite the MSL symbol names. */
extern f32 sin(f32 x);
extern f32 cos(f32 x);
extern f64 __frsqrte(f64 x);
extern f64 __fabs(f64 x);
extern f32 atan2(f32 y, f32 x);
extern const f32 lbl_80127D20[3];
extern const f32 lbl_80127D40[3];
extern const f32 lbl_80127D50[3];
extern const f32 gIdentityMatrix[];
extern CameraMilestone sMilestones[];

f32 FloorPos(f32 fallback, f32 radius, f32* position, s32 mode);
f32 fqdist(f32 x, f32 y);
s32 fn_800511D0(s32 milestone, f32 turnLimit);
f32 get_yaw(f32* to, f32* from);
void CreateYPRMatrix(f32* matrix, const f32* angles);
void WorldVector(const f32* vector, f32* out, const f32* matrix);
void StandardCamera(s32 camIdx);
f32 PointLineColl(f32* point, f32* from, f32* to, f32* closest);
f32 AddAngle(f32 angle, f32 amount);
f32 SubAngle(f32 angle, f32 amount);
f32 get_pitch(f32* from, f32* to);
void get_attn_pos(s32 camIdx, f32* out);
int init_game_cam(s32 camIdx);
int MoveCam_walk(s32 camIdx);
void cam_orient_to(s32 camIdx);
int PlayerOnMovingObject(void);
void ProcCamera(s32 camIdx, s32 mode);
void screen_limitation(void);
void MBCameraUpdate(f32* position, f32* matrix);
void MBRemoveBlit(void* blit);
void AverageCameraTargetPosition_8002A890(f32* out);
void calc_cam_pyr(s32 camIdx, s32 resetDelta);
void get_cam_wpos(s32 camIdx);
s32 adjust_radius(s32 camIdx);
void CopyCam(u8* source, u8* destination);
void UpdatePlayerWorldMat(void* player, s32 anchor);
void init_stage_info(void);
void DiffRate();
void dbgTextPrintfCol(s32 x, s32 line, char* fmt, ...);
void fn_8005A588(struct OBJGRP* group, f32* offset);
s32 MBScreenHeight(void);
s32 MBScreenWidth(void);

f32 camera_approach_yaw(void* cam, f32 target);
f32 camera_lerp_yaw(f32 current, f32 target);
void camera_orbit_update(s32 camIdx);
s32 camera_collide_step(s32 camIdx, f32 blendThreshold);
extern f32 lbl_80345EC8;  /* 0.0f (sdata2 pool) */
extern f64 lbl_80345F18;  /* 0.5 */
extern f64 lbl_80345F20;  /* 3.0 */
void camera_init_for_gamemode(s32 camIdx);
void camera_run_mode(s32 camIdx);
void camera_mode_follow(s32 camIdx);
void camera_mode_target(s32 camIdx);
void camera_mode_dest(s32 camIdx);
void camera_mode_spin(s32 camIdx);
void camera_mode_orbit(s32 camIdx);
void camera_mode_level(s32 reset);
void DoShake(Vec3* posA, Vec3* posB);
s32 debug_camera_pos(s32 lastPlayer);
s32 camera_debug_supervisor(s32 playerIndex, f32* movementDelta);

#define TC_X(i) (*(f32*)(sTriggerCameras + (i) * 0x28 + 4))
#define TC_Y(i) (*(f32*)(sTriggerCameras + (i) * 0x28 + 8))
#define TC_Z(i) (*(f32*)(sTriggerCameras + (i) * 0x28 + 0xC))

/* gCameras[6] is declared in game/camera.h (@0x8023F8D0, stride 0x18C). */

/* Normalize an angle into (-PI, PI] with a single fold (matches the asm). */
static f32 cam_wrap_pi(f32 a) {
    if (a > CAM_PI) {
        return (f32)(a - CAM_2PI);
    }
    if (a <= -CAM_PI) {
        return (f32)(a + CAM_2PI);
    }
    return a;
}

/*
 * get_screen_pos -- project a world position to integer screen coordinates.
 * arg0 is passed but unused by the projection path.
 */
void get_screen_pos(int unused, int* xo, int* yo, void* world_pos) {
    u8 frame_pad[8];
    f32 sp[2];
    (void)unused;
    MBWorldToScreen(sp, world_pos);
    *xo = (int)sp[0];
    *yo = (int)sp[1];
}

/*
 * get_actual_screen_pos -- project a world position through a specific
 * camera's viewport (gCameras[camIdx]) to float screen coordinates.
 */
void get_actual_screen_pos(int camIdx, f32* xo, f32* yo, void* world_pos) {
    Camera* cam = &gCameras[camIdx];
    short scr[2];
    MBWindowProject((f32*)world_pos, &cam->mat[0][0], NULL, scr);
    *xo = (f32)scr[0];
    *yo = (f32)scr[1];
}

/* Build an orthonormal basis from a forward direction. */
void LookInDirection(f32* dir, u32 matAddress)
{
    f32* mat;
    f32* up;
    f32* fwd;
    f32 len;

    mat = (f32*)matAddress;
    up = mat + 4;
    fwd = mat + 8;
    mat[8] = dir[0];
    mat[9] = dir[1];
    mat[10] = dir[2];
    len = SlowNormalVector(fwd);
    if (len < 0.001) {
        CopyMat3(gIdentityMatrix, mat);
    } else {
        if (fwd[0] * fwd[0] + fwd[2] * fwd[2] < 0.0001) {
            if (fwd[1] > 0.0f) {
                up[0] = lbl_80127D40[0];
                up[1] = lbl_80127D40[1];
                up[2] = lbl_80127D40[2];
            } else {
                up[0] = lbl_80127D50[0];
                up[1] = lbl_80127D50[1];
                up[2] = lbl_80127D50[2];
            }
        } else {
            up[0] = lbl_80127D20[0];
            up[1] = lbl_80127D20[1];
            up[2] = lbl_80127D20[2];
        }
        mat[0] = up[1] * fwd[2] - up[2] * fwd[1];
        mat[1] = up[2] * fwd[0] - up[0] * fwd[2];
        mat[2] = up[0] * fwd[1] - up[1] * fwd[0];
        SlowNormalVector(mat);
        up[0] = fwd[1] * mat[2] - fwd[2] * mat[1];
        up[1] = fwd[2] * mat[0] - fwd[0] * mat[2];
        up[2] = fwd[0] * mat[1] - fwd[1] * mat[0];
    }
}

#define CLAMP_PROJECTED(field_)                                                \
    do {                                                                       \
        if ((f64)(field_) < -2000.0) {                                        \
            (field_) = -2000.0f;                                               \
        } else if ((f64)(field_) > 2000.0) {                                  \
            (field_) = 2000.0f;                                                \
        }                                                                      \
    } while (0)

/* Project camera targets, advance each active camera, then refresh the
 * renderer-facing camera matrices. */
void do_camera(void)
{
    u8* state;
    f32* cameraMatrix;
    s32 projectedIndex;
    s32 limitedIndex;
    s32 cameraIndex;
    Camera* camera;
    CameraTarget* projectedTarget;
    CameraTarget* limitedTarget;
    u8 unused[8];
    s16 projectedTop[2];
    s16 projectedBottom[2];
    s16 limitedTop[2];
    s16 limitedBottom[2];
    s32 sign;
    s32 moving;

    state = gCameraState;
    if (PlayerOnMovingObject() != 0) {
        moving = 0;
    } else {
        moving = 1;
    }
    lbl_80343BD8 = moving;

    cameraMatrix = (f32*)(state + 0xCC);
    projectedTarget = (CameraTarget*)(state + 0xA10);
    for (projectedIndex = 0; projectedIndex < 15;
         projectedIndex++, projectedTarget++) {
        sign = projectedTarget->active >> 31;
        if ((sign ^ projectedTarget->active) - sign == 1) {
            MBWindowProject((f32*)(projectedTarget->object + 0x40), cameraMatrix,
                            NULL, projectedTop);
            projectedTarget->projectedTop[0] = (f32)projectedTop[0];
            projectedTarget->projectedTop[1] = (f32)projectedTop[1];
            CLAMP_PROJECTED(projectedTarget->projectedTop[0]);
            CLAMP_PROJECTED(projectedTarget->projectedTop[1]);

            MBWindowProject((f32*)(projectedTarget->object + 0x30), cameraMatrix,
                            NULL, projectedBottom);
            projectedTarget->projectedBottom[0] = (f32)projectedBottom[0];
            projectedTarget->projectedBottom[1] = (f32)projectedBottom[1];
            CLAMP_PROJECTED(projectedTarget->projectedBottom[0]);
            CLAMP_PROJECTED(projectedTarget->projectedBottom[1]);
        }
    }

    lbl_803443F4 = 0;
    if ((gGameBusy | gGameplayPauseTimer) == 0 && lbl_803443F8 > 0) {
        lbl_803443F8 -= gFrameTicks;
    }

    moving = cameraIndex = 0;
    camera = (Camera*)(state + 0xC8);
    for (; cameraIndex < 6; cameraIndex++, camera++) {
        if (camera->state == 1) {
            if ((gGameBusy | gGameplayPauseTimer) == 0) {
                lbl_803443F4 = moving;
                camera_init_for_gamemode(cameraIndex);
                camera_run_mode(cameraIndex);
            }
            if (camera->c_mode != CAM_OFF) {
                ProcCamera(cameraIndex, lbl_803444DC);
            }
        }
    }

    limitedTarget = (CameraTarget*)(state + 0xA10);
    for (limitedIndex = 0; limitedIndex < 15;
         limitedIndex++, limitedTarget++) {
        sign = limitedTarget->active >> 31;
        if ((sign ^ limitedTarget->active) - sign == 1) {
            MBWindowProject((f32*)(limitedTarget->object + 0x40), cameraMatrix,
                            NULL, limitedTop);
            limitedTarget->limitedTop[0] = (f32)limitedTop[0];
            limitedTarget->limitedTop[1] = (f32)limitedTop[1];
            CLAMP_PROJECTED(limitedTarget->limitedTop[0]);
            CLAMP_PROJECTED(limitedTarget->limitedTop[1]);

            MBWindowProject((f32*)(limitedTarget->object + 0x30), cameraMatrix,
                            NULL, limitedBottom);
            limitedTarget->limitedBottom[0] = (f32)limitedBottom[0];
            limitedTarget->limitedBottom[1] = (f32)limitedBottom[1];
            CLAMP_PROJECTED(limitedTarget->limitedBottom[0]);
            CLAMP_PROJECTED(limitedTarget->limitedBottom[1]);
        }
    }

    screen_limitation();
    cameraIndex = lbl_8034453C;
    MBCameraUpdate((f32*)(state + cameraIndex * sizeof(Camera) + 0xFC),
                   (f32*)(state + cameraIndex * sizeof(Camera) + 0xCC));
}

#undef CLAMP_PROJECTED

/* These setters are macro-expanded at each state-machine arm.  The retail
 * source did the same, which deliberately leaves duplicated blocks. */
#define CAMERA_SET_TABLE_MODE(cam_)                                           \
    do {                                                                      \
        bossIndex = lbl_80118CD8[bossIndex];                                  \
        bossType = 0;                                                         \
        if ((CAM_MODE)bossIndex != (cam_)->c_mode) {                          \
            (cam_)->pc_mode = (cam_)->c_mode;                                 \
            bossType |= 1;                                                    \
            (cam_)->c_mode = (CAM_MODE)bossIndex;                             \
        }                                                                     \
        if ((cam_)->a_mode != ATN_TARGET) {                                   \
            (cam_)->pa_mode = (cam_)->a_mode;                                 \
            bossType |= 2;                                                    \
            (cam_)->a_mode = ATN_TARGET;                                      \
        }                                                                     \
        if (bossType != 0) gScriptedCameraState = 0;                          \
        return;                                                               \
    } while (0)

#define CAMERA_SET_GAME_MODE(cam_, targetMode_)                              \
    do {                                                                      \
        changed = 0;                                                          \
        if ((cam_)->c_mode != CAM_GAME) {                                     \
            (cam_)->pc_mode = (cam_)->c_mode;                                 \
            changed |= 1;                                                     \
            (cam_)->c_mode = CAM_GAME;                                        \
        }                                                                     \
        if ((cam_)->a_mode != ATN_TARGET) {                                   \
            (cam_)->pa_mode = (cam_)->a_mode;                                 \
            changed |= 2;                                                     \
            (cam_)->a_mode = ATN_TARGET;                                      \
        }                                                                     \
        if (changed == 0) return;                                             \
        if (gGameMode == 0x400C) (cam_)->trans_mode = 3;                      \
        else if ((cam_)->pc_mode == CAM_OBJEYE) (cam_)->trans_mode = 0;       \
        else (cam_)->trans_mode = 1;                                          \
        gCameraTargetPositionCount = 0;                                       \
        gCameraTargetMode = (targetMode_);                                    \
        lbl_80344508 = -1;                                                    \
        gScriptedCameraState = 0;                                             \
        return;                                                               \
    } while (0)

/* Select the camera/attention modes required by the current game state. */
void camera_init_for_gamemode(s32 camIndex)
{
    u8* state = gCameraState;
    Camera* cam = (Camera*)(state + 200 + camIndex * 396);
    s32 tableMode;
    s32 changed;
    Camera* menuCam;
    s32 bossType;
    s32 bossIndex;

    switch (gGameMode) {
    case 0x400B:
    case 0x400D:
    case 0x4012:
    case 0x4013:
    case 0x4015:
    case 0x4016:
    case 0x4017:
    case 0x8003:
    case 0x8006:
    case 0x8007:
    case 0x800A:
        return;
    case 0x400C:
    case 0x4010:
    case 0x4014:
        goto gameplay_mode;
    case 0x8008:
        if (camIndex == 0) {
            switch (cam->mode) {
            case 0:
            case 1:
                gFrameTicks = 1;
                cam->mode++;
                break;
            default:
                break;
            }
menu_done:
            gScriptedCameraState = 0;
            return;
        }
        if (camIndex != 1) return;
        menuCam = (Camera*)(state + 596);
        tableMode = menuCam->a_mode;
        changed = menuCam->c_mode;
        if (changed != CAM_OFF) {
            menuCam->pc_mode = (CAM_MODE)changed;
            menuCam->c_mode = CAM_OFF;
        }
        if (tableMode != menuCam->a_mode) {
            menuCam->pa_mode = menuCam->a_mode;
            menuCam->a_mode = (ATN_MODE)tableMode;
        }
        menuCam->state = 0;
        lbl_8034453C = 0;
        return;
    default:
        goto general_mode;
    }

gameplay_mode:
    if (camIndex != 0) return;
    if (lbl_803447B8 != 0) return;
    bossType = gBossType;
    if (bossType >= 0 && lbl_803444E0 != 0) {
        bossIndex = bossType - 0x22;
        switch (bossType) {
        case 0x22:
        case 0x23:
        case 0x24:
        case 0x25:
        case 0x26:
        case 0x27:
        case 0x2A:
        case 0x2B:
            goto specialized_boss;
        default:
            goto normal_boss;
        }
normal_boss:
        CAMERA_SET_TABLE_MODE(cam);
specialized_boss:
        if (lbl_803444E0 != 0 && gCameraTargetCount >= 2) {
            CAMERA_SET_TABLE_MODE(cam);
        }
        CAMERA_SET_GAME_MODE(cam, 8);
    }
    CAMERA_SET_GAME_MODE(cam, 5);

general_mode:
    if (camIndex == 0) {
        changed = 0;
        if (cam->c_mode != CAM_FREE) {
            cam->pc_mode = cam->c_mode;
            changed |= 1;
            cam->c_mode = CAM_FREE;
        }
        if (cam->a_mode != ATN_FREE) {
            cam->pa_mode = cam->a_mode;
            changed |= 2;
            cam->a_mode = ATN_FREE;
        }
        if (changed != 0) gScriptedCameraState = 0;
        return;
    }
    if (cam->c_mode != CAM_FREE) {
        cam->pc_mode = cam->c_mode;
        cam->c_mode = CAM_FREE;
    }
    if (cam->a_mode != ATN_FREE) {
        cam->pa_mode = cam->a_mode;
        cam->a_mode = ATN_FREE;
    }
}
#undef CAMERA_SET_GAME_MODE
#undef CAMERA_SET_TABLE_MODE

/*
 * Run the active camera/attention mode.  The original source kept each
 * mode's scratch vectors separate, which is why the local arrays below are
 * intentionally not shared between the switch arms.
 */
#pragma opt_propagation off
void camera_run_mode(s32 camIdx)
{
    Camera* cam = &gCameras[camIdx];
    s32 attentionMode = cam->a_mode;
    s32 playerIndex;
    s32 tries;
    u8* playerObject;
    Player* players;
    Player* player;
    s32* playerCursor;
    f32 distance;
    f32 scale;
    f32 rate;
    f32 cameraRadius;
    f32 dx;
    f32 dy;
    f32 dz;
    f64 stepDouble;
    f64 divisorDouble;
    f32* cameraAttention;
    f32* cameraPosition;
    u8 framePad[8];
    f32 destination[3];
    u8 destinationPad[0x0C];
    volatile f32 rootVector;
    u8 rootVectorPad[8];
    f32 normalizeVector[3];
    Vec3 vectorAttention;
    Vec3 vectorPosition;
    Vec3 vectorDirection;
    u8 vectorPad[0x18];
    volatile f32 rootGame;
    f32 normalizeGame[3];
    f32 gameAttention[3];
    f32 gamePosition[3];
    f32 gameDirection[3];
    volatile f32 rootObject;
    f32 objectAttention[3];
    f32 objectPosition[3];
    f32 objectDirection[3];
    u8 objectPad[0x0C];
    f32 normalizeFree[3];
    f32 freeAttention[3];
    f32 freePosition[3];
    f32 freeDirection[3];
    volatile f32 rootGameMode;
    f32 normalizeGameMode[3];
    u8 runModePad[0x1C];

    if (cam->camobj != 0 && *(u32*)((u8*)cam->camobj + 0x60) == 0) {
        cam->camobj = 0;
    }
    if (cam->attnobj != 0 && *(u32*)((u8*)cam->attnobj + 0x60) == 0) {
        cam->attnobj = 0;
    }

    if (attentionMode == ATN_FREE) {
        goto free_attention;
    }
    if (attentionMode < 0) {
        return;
    }
    if (attentionMode >= 11) {
        return;
    }

    switch (cam->c_mode) {
    case CAM_VECDIST:
        if (cam->trans_mode == 0) {
            if ((f64)cam->radius < 15.0) {
                stepDouble = 0.08333333 * (15.0 - (f64)cam->radius);
                rate = (f32)stepDouble;
                if ((f64)rate < 0.15) {
                    rate = 0.15f;
                }
                cam->radius += rate * (f32)(u32)gFrameTicks;
                if ((f64)cam->radius >= 15.0) {
                    cam->radius = 15.0f;
                }
                lbl_803443F4 = 1;
            }
            if ((f64)cam->radius >= 15.0) {
                cam->trans_mode = -1;
            }
        }

        get_attn_pos(camIdx, destination);
        cam->delta[0] = destination[0] - cam->attn[0];
        cam->delta[1] = destination[1] - cam->attn[1];
        cam->delta[2] = destination[2] - cam->attn[2];
        distance = cam->delta[2] * cam->delta[2] +
                   (distance = cam->delta[0] * cam->delta[0] +
                               cam->delta[1] * cam->delta[1]);
        if (distance > lbl_80345EC8) {
            f64 guess = __frsqrte(distance);
            guess = lbl_80345F18 * guess * (lbl_80345F20 - guess * guess * distance);
            guess = lbl_80345F18 * guess * (lbl_80345F20 - guess * guess * distance);
            guess = lbl_80345F18 * guess * (lbl_80345F20 - guess * guess * distance);
            rootVector = (f32)(distance *
                (lbl_80345F18 * guess * (lbl_80345F20 - guess * guess * distance)));
            distance = rootVector;
        }
        stepDouble = 0.3 * (f64)(u32)gFrameTicks;
        divisorDouble = (f64)distance;
        if ((f64)distance >= stepDouble) {
            if ((f64)distance > 12.0) {
                divisorDouble = (f64)lbl_80345F38;
            }
            scale = (f32)(stepDouble / divisorDouble);
            cam->delta[0] *= scale;
            cam->delta[1] *= scale;
            cam->delta[2] *= scale;
            cam->wpos[0] += cam->delta[0];
            cam->wpos[1] += cam->delta[1];
            cam->wpos[2] += cam->delta[2];
            cam->attn[0] += cam->delta[0];
            cam->attn[1] += cam->delta[1];
            cam->attn[2] += cam->delta[2];
        } else {
            cam->wpos[0] += cam->delta[0];
            cam->wpos[1] += cam->delta[1];
            cam->wpos[2] += cam->delta[2];
            cam->attn[0] = destination[0];
            cam->attn[1] = destination[1];
            cam->attn[2] = destination[2];
        }
        if (gGameMode == 0x8008) {
            camera_mode_dest(camIdx);
        } else if (gGameMode == 0x4010) {
            cam_orient_to(camIdx);
        }
        if (lbl_803443F4 != 0) {
            cameraRadius = cam->radius;
            normalizeVector[0] = cam->wpos[0] - cam->attn[0];
            normalizeVector[1] = cam->wpos[1] - cam->attn[1];
            normalizeVector[2] = cam->wpos[2] - cam->attn[2];
            SlowNormalVector(normalizeVector);
            cam->wpos[0] = cam->attn[0] + normalizeVector[0] * cameraRadius;
            cam->wpos[1] = cam->attn[1] + normalizeVector[1] * cameraRadius;
            cam->wpos[2] = cam->attn[2] + normalizeVector[2] * cameraRadius;
        }
        cam->pyr[0] = get_pitch(cam->wpos, cam->attn);
        cam->pyr[1] = get_yaw(cam->wpos, cam->attn);
        cam->pyr[0] = -cam->pyr[0];
        vectorPosition.x = cam->wpos[0];
        vectorPosition.y = cam->wpos[1];
        vectorPosition.z = cam->wpos[2];
        vectorAttention.x = cam->attn[0];
        vectorAttention.y = cam->attn[1];
        vectorAttention.z = cam->attn[2];
        StandardCamera(camIdx);
        DoShake(&vectorPosition, &vectorAttention);
        vectorDirection.x = vectorAttention.x - vectorPosition.x;
        vectorDirection.y = vectorAttention.y - vectorPosition.y;
        vectorDirection.z = vectorAttention.z - vectorPosition.z;
        LookInDirection(&vectorDirection.x, (u32)&cam->mat[0][0]);
        break;

    case CAM_FREE:
    case CAM_LOCK:
    case CAM_POINT:
        if (cam->trans_mode == 0) {
            cam->trans_mode = -1;
        }
        if (gGameMode == 0x8008) {
            camera_mode_dest(camIdx);
        } else if (gGameMode == 0x8007) {
            if (camIdx == 0) {
                dx = cam->wpos[0] - cam->attn[0];
                dy = cam->wpos[1] - cam->attn[1];
                dz = cam->wpos[2] - cam->attn[2];
                distance = dz * dz + (dx * dx + dy * dy);
                if (distance > lbl_80345EC8) {
                    f64 guess = __frsqrte(distance);
                    guess = lbl_80345F18 * guess * (lbl_80345F20 - guess * guess * distance);
                    guess = lbl_80345F18 * guess * (lbl_80345F20 - guess * guess * distance);
                    guess = lbl_80345F18 * guess * (lbl_80345F20 - guess * guess * distance);
                    rootGameMode = (f32)(distance *
                        (lbl_80345F18 * guess * (lbl_80345F20 - guess * guess * distance)));
                    distance = rootGameMode;
                }
                cam->radius = (f32)((f64)distance + 2.0 * (f64)gClockFrameStep);
                cameraRadius = cam->radius;
                cameraPosition = cam->wpos;
                cameraAttention = cam->attn;
                normalizeGameMode[0] = cameraPosition[0] - cameraAttention[0];
                normalizeGameMode[1] = cameraPosition[1] - cameraAttention[1];
                normalizeGameMode[2] = cameraPosition[2] - cameraAttention[2];
                SlowNormalVector(normalizeGameMode);
                cameraPosition[0] = cameraAttention[0] + normalizeGameMode[0] * cameraRadius;
                cameraPosition[1] = cameraAttention[1] + normalizeGameMode[1] * cameraRadius;
                cameraPosition[2] = cameraAttention[2] + normalizeGameMode[2] * cameraRadius;
            }
        } else if (gGameMode == 0x400D || gGameMode == 0x4013 ||
                   gGameMode == 0x4017) {
            camera_mode_spin(camIdx);
        }

        if (lbl_803447B8 != 0) {
            if (lbl_803444F0 >= 0) {
                if (MoveCam_walk(camIdx) == 0) {
                    break;
                }
            } else {
                if (init_game_cam(camIdx) == 0) {
                    break;
                }
            }
        } else {
            get_attn_pos(camIdx, cam->attn);
        }

        if (((gGameMode != 0x8008 && gGameMode != 0x400D &&
              gGameMode != 0x4013 && gGameMode != 0x4017)) ||
            cam->c_mode != CAM_LOCK) {
            dx = cam->wpos[0] - cam->attn[0];
            dy = cam->wpos[1] - cam->attn[1];
            dz = cam->wpos[2] - cam->attn[2];
            distance = dz * dz + (dx * dx + dy * dy);
            if (distance > lbl_80345EC8) {
                f64 guess = __frsqrte(distance);
                guess = lbl_80345F18 * guess * (lbl_80345F20 - guess * guess * distance);
                guess = lbl_80345F18 * guess * (lbl_80345F20 - guess * guess * distance);
                guess = lbl_80345F18 * guess * (lbl_80345F20 - guess * guess * distance);
                rootGame = (f32)(distance *
                    (lbl_80345F18 * guess * (lbl_80345F20 - guess * guess * distance)));
                distance = rootGame;
            }
            cam->radius = distance;
            lbl_803443F4 = 1;
            cameraRadius = cam->radius;
            normalizeGame[0] = cam->wpos[0] - cam->attn[0];
            normalizeGame[1] = cam->wpos[1] - cam->attn[1];
            normalizeGame[2] = cam->wpos[2] - cam->attn[2];
            SlowNormalVector(normalizeGame);
            cam->wpos[0] = cam->attn[0] + normalizeGame[0] * cameraRadius;
            cam->wpos[1] = cam->attn[1] + normalizeGame[1] * cameraRadius;
            cam->wpos[2] = cam->attn[2] + normalizeGame[2] * cameraRadius;
            cam->pyr[0] = get_pitch(cam->wpos, cam->attn);
            cam->pyr[1] = get_yaw(cam->wpos, cam->attn);
            cam->pyr[0] = -cam->pyr[0];
        }
        gamePosition[0] = cam->wpos[0];
        gamePosition[1] = cam->wpos[1];
        gamePosition[2] = cam->wpos[2];
        gameAttention[0] = cam->attn[0];
        gameAttention[1] = cam->attn[1];
        gameAttention[2] = cam->attn[2];
        StandardCamera(camIdx);
        DoShake((Vec3*)gamePosition, (Vec3*)gameAttention);
        gameDirection[0] = gameAttention[0] - gamePosition[0];
        gameDirection[1] = gameAttention[1] - gamePosition[1];
        gameDirection[2] = gameAttention[2] - gamePosition[2];
        LookInDirection(gameDirection, (u32)&cam->mat[0][0]);
        break;

    case CAM_OBJEYE:
        if (cam->trans_mode == 0) {
            cam->trans_mode = -1;
        }
        players = (Player*)gPlayers;
        playerCursor = &gCameras[0].pn;
        playerIndex = *playerCursor;
        for (tries = 0; tries < 4; tries++) {
            player = &players[playerIndex];
            if (player->state == 1 || player->state == 4) {
                playerObject = (u8*)player + 0x14;
                *playerCursor = playerIndex;
                goto found_player_object;
            }
            playerIndex++;
            if (playerIndex >= 4) {
                playerIndex = 0;
            }
        }
        playerObject = 0;
found_player_object:
        cam->camobj = (struct OBJGRP*)playerObject;
        if (cam->camobj != 0 && *(u32*)((u8*)cam->camobj + 0x60) != 0) {
            cam->wpos[0] = *(f32*)((u8*)cam->camobj + 0x40);
            cam->wpos[1] = *(f32*)((u8*)cam->camobj + 0x44);
            cam->wpos[2] = *(f32*)((u8*)cam->camobj + 0x48);
        }
        if (attentionMode == ATN_OBJECT ||
            (u32)(attentionMode - ATN_PLAYER) <= 4) {
            if (cam->attnobj != 0) {
                cam->attn[0] = *(f32*)((u8*)cam->attnobj + 0x40);
                cam->attn[1] = *(f32*)((u8*)cam->attnobj + 0x44);
                cam->attn[2] = *(f32*)((u8*)cam->attnobj + 0x48);
            }
        } else if (attentionMode == ATN_TARGET) {
            get_attn_pos(camIdx, cam->attn);
        }
        cam->pyr[0] = get_pitch(cam->wpos, cam->attn);
        cam->pyr[1] = get_yaw(cam->wpos, cam->attn);
        cam->pyr[1] = AddAngle(cam->pyr[1], (f32)CAM_PI);
        dx = cam->wpos[0] - cam->attn[0];
        dy = cam->wpos[1] - cam->attn[1];
        dz = cam->wpos[2] - cam->attn[2];
        distance = dz * dz + (dx * dx + dy * dy);
        if (distance > lbl_80345EC8) {
            f64 guess = __frsqrte(distance);
            guess = lbl_80345F18 * guess * (lbl_80345F20 - guess * guess * distance);
            guess = lbl_80345F18 * guess * (lbl_80345F20 - guess * guess * distance);
            guess = lbl_80345F18 * guess * (lbl_80345F20 - guess * guess * distance);
            rootObject = (f32)(distance *
                (lbl_80345F18 * guess * (lbl_80345F20 - guess * guess * distance)));
            distance = rootObject;
        }
        cam->radius = distance;
        objectPosition[0] = cam->wpos[0];
        objectPosition[1] = cam->wpos[1];
        objectPosition[2] = cam->wpos[2];
        objectAttention[0] = cam->attn[0];
        objectAttention[1] = cam->attn[1];
        objectAttention[2] = cam->attn[2];
        StandardCamera(camIdx);
        DoShake((Vec3*)objectPosition, (Vec3*)objectAttention);
        objectDirection[0] = objectAttention[0] - objectPosition[0];
        objectDirection[1] = objectAttention[1] - objectPosition[1];
        objectDirection[2] = objectAttention[2] - objectPosition[2];
        LookInDirection(objectDirection, (u32)&cam->mat[0][0]);
        break;

    case CAM_GAME:
    case CAM_DRAGON:
    case CAM_CHIMERA:
    case CAM_DJINN:
    case CAM_DRIDER:
    case CAM_DEMON:
    case CAM_BOSS:
        camera_mode_follow(camIdx);
        break;

    case CAM_OFF:
        cam->state = 0;
        break;

    default:
        break;
    }
    return;

free_attention:
    switch (cam->c_mode) {
    case CAM_FREE:
    case CAM_LOCK:
    case CAM_POINT:
        if (cam->trans_mode == 0) {
            cam->pyr[0] = get_pitch(cam->wpos, cam->attn);
            cam->pyr[1] = get_yaw(cam->wpos, cam->attn);
            cam->pyr[1] = AddAngle(cam->pyr[1], (f32)CAM_PI);
            cam->trans_mode = -1;
        }
        if (gGameMode == 0x8008 && lbl_80344288 != 0) {
            camera_mode_orbit(camIdx);
        }
        break;

    case CAM_VECDIST:
        if (cam->trans_mode == 0) {
            cam->trans_mode = -1;
        }
        dx = cam->wpos[0] - cam->wpos[0];
        dy = cam->wpos[1] - cam->wpos[1];
        dz = cam->wpos[2] - cam->wpos[2];
        cam->attn[0] += (dx = dx);
        cam->attn[1] += (dy = dy);
        cam->attn[2] += (dz = dz);
        if (gGameMode == 0x8008) {
            camera_mode_dest(camIdx);
        }
        if (lbl_803443F4 != 0) {
            cameraRadius = cam->radius;
            normalizeFree[0] = cam->wpos[0] - cam->attn[0];
            normalizeFree[1] = cam->wpos[1] - cam->attn[1];
            normalizeFree[2] = cam->wpos[2] - cam->attn[2];
            SlowNormalVector(normalizeFree);
            cam->wpos[0] = cam->attn[0] + normalizeFree[0] * cameraRadius;
            cam->wpos[1] = cam->attn[1] + normalizeFree[1] * cameraRadius;
            cam->wpos[2] = cam->attn[2] + normalizeFree[2] * cameraRadius;
        }
        cam->pyr[0] = get_pitch(cam->wpos, cam->attn);
        cam->pyr[1] = get_yaw(cam->wpos, cam->attn);
        cam->pyr[0] = -cam->pyr[0];
        freePosition[0] = cam->wpos[0];
        freePosition[1] = cam->wpos[1];
        freePosition[2] = cam->wpos[2];
        freeAttention[0] = cam->attn[0];
        freeAttention[1] = cam->attn[1];
        freeAttention[2] = cam->attn[2];
        StandardCamera(camIdx);
        DoShake((Vec3*)freePosition, (Vec3*)freeAttention);
        freeDirection[0] = freeAttention[0] - freePosition[0];
        freeDirection[1] = freeAttention[1] - freePosition[1];
        freeDirection[2] = freeAttention[2] - freePosition[2];
        LookInDirection(freeDirection, (u32)&cam->mat[0][0]);
        break;

    case CAM_OBJEYE:
        camera_mode_target(camIdx);
        break;

    case CAM_OFF:
        cam->state = 0;
        break;

    case CAM_GAME:
    case CAM_DRAGON:
    case CAM_CHIMERA:
    case CAM_DJINN:
    case CAM_DRIDER:
    case CAM_DEMON:
        break;

    default:
        break;
    }
}
#pragma opt_propagation reset

/* Keep the rendered view in sync with the simulation camera.  CAMERA.OBJ
 * expands this sequence at each early-out and transition arm. */
#define FOLLOW_RENDER(camIndex_, cam_, position_, attention_, direction_)      \
    do {                                                                       \
        (position_).x = (cam_)->wpos[0];                                       \
        (position_).y = (cam_)->wpos[1];                                       \
        (position_).z = (cam_)->wpos[2];                                       \
        (attention_).x = (cam_)->attn[0];                                     \
        (attention_).y = (cam_)->attn[1];                                     \
        (attention_).z = (cam_)->attn[2];                                     \
        StandardCamera(camIndex_);                                             \
        DoShake((Vec3*)&(position_), (Vec3*)&(attention_));                    \
        (direction_).x = (attention_).x - (position_).x;                       \
        (direction_).y = (attention_).y - (position_).y;                       \
        (direction_).z = (attention_).z - (position_).z;                       \
        LookInDirection((f32*)&(direction_).x, (u32)&(cam_)->mat[0][0]);        \
    } while (0)

#define FOLLOW_NORMALIZE_POSITION(cam_, vector_, radius_)                     \
    do {                                                                       \
        (radius_) = (cam_)->radius;                                            \
        (vector_)[0] = (cam_)->wpos[0] - (cam_)->attn[0];                     \
        (vector_)[1] = (cam_)->wpos[1] - (cam_)->attn[1];                     \
        (vector_)[2] = (cam_)->wpos[2] - (cam_)->attn[2];                     \
        SlowNormalVector((f32*)(vector_));                                     \
        (cam_)->wpos[0] = (cam_)->attn[0] + (vector_)[0] * (radius_);         \
        (cam_)->wpos[1] = (cam_)->attn[1] + (vector_)[1] * (radius_);         \
        (cam_)->wpos[2] = (cam_)->attn[2] + (vector_)[2] * (radius_);         \
    } while (0)

#define backup                   ((Camera*)(state + 0x884))
#define followPositions          ((f32 (*)[3])(state + 0x5C))

/* Main gameplay camera.  This is the retail transition supervisor: it keeps
 * a short focus history, blends scripted camera changes, controls camera
 * speed from the projected target extent, and rejects views which put a live
 * player outside the safe viewport. */
void camera_mode_follow(s32 camIdx)
{
    u8* state = gCameraState;
    f32* projectionMatrix;
    Camera* cam = (Camera*)(state + 0xC8 + camIdx * sizeof(Camera));
    f32 focus[3];
    u8 followPad[0x38];
    Vec3 requestAttention;
    Vec3 requestPosition;
    Vec3 requestDirection;
    Vec3 emptyAttention;
    Vec3 emptyPosition;
    Vec3 emptyDirection;
    f32 normalizeInitial[3];
    Vec3 initialAttention;
    Vec3 initialPosition;
    Vec3 initialDirection;
    f32 normalizeTransition[3];
    volatile f32 transitionAttentionRoot;
    volatile f32 transitionPositionRoot;
    Vec3 transitionAttention;
    Vec3 transitionPosition;
    Vec3 transitionDirection;
    f32 normalizeDefault[3];
    Vec3 resetAttention;
    Vec3 resetPosition;
    Vec3 resetDirection;
    volatile f32 focusRoot;
    f32 normalizeFinal[3];
    Vec3 finalAttention;
    Vec3 finalPosition;
    Vec3 finalDirection;
    s16 projected[2];
    u8 followLowPad[4];
    f32 savedPitch;
    f32 savedYaw;
    f32 savedTurn;
    f32 zeroValue;
    f32 oldPositionX;
    f32 oldPositionY;
    f32 oldPositionZ;
    f32 oldAttentionX;
    f32 oldAttentionY;
    f32 oldAttentionZ;
    f32 followRadius;
    f32 dx;
    f32 dy;
    f32 dz;
    f32 distance;
    f32 desiredSpeed;
    f32 maximumStep;
    f32 scale;
    f32 previousSpeed;
    f32 targetExtent;
    f32 screenX;
    f32 screenY;
    f64 root;
    s32 scriptedPlayer;
    s32 resetPlayer;
    s32 offscreen;
    s32 transitionParts;
    Player* playerData;
    s32 viewportPlayer;
    s32 i;
    s32 positionCount;

    if (lbl_80344500 != 0) {
        lbl_803444F8 -= gFrameTicks;
        zeroValue = lbl_80345EC8;
        cam->delta[0] = zeroValue;
        cam->delta[1] = zeroValue;
        cam->delta[2] = zeroValue;
        cam->pyr_delta[0] = zeroValue;
        cam->pyr_delta[1] = zeroValue;
        cam->pyr_delta[2] = zeroValue;
        FOLLOW_RENDER(camIdx, cam, requestPosition, requestAttention,
                      requestDirection);
        return;
    }

    if (gCameraTargetCount == 0) {
        zeroValue = lbl_80345EC8;
        cam->delta[0] = zeroValue;
        cam->delta[1] = zeroValue;
        cam->delta[2] = zeroValue;
        cam->pyr_delta[0] = zeroValue;
        cam->pyr_delta[1] = zeroValue;
        cam->pyr_delta[2] = zeroValue;
        FOLLOW_RENDER(camIdx, cam, emptyPosition, emptyAttention,
                      emptyDirection);
        return;
    }

    if (lbl_803444F4 == 0 && lbl_80344500 == 0 &&
        lbl_803447B8 == 0 && lbl_803447B4 == 0 && camIdx == 0 &&
        cam->trans_mode < 0 && lbl_803444DC == 0) {
        if (camIdx == 0) {
            lbl_80344494 += gFrameTicks;
        }
        if (lbl_80344494 >= 90) {
            cam->trans_mode = 1;
            gCameraTargetPositionCount = 0;
            gCameraTargetMode = 4;
            lbl_80344508 = -1;
            lbl_80344494 = 0;
        }
    } else {
        lbl_80344494 = 0;
    }

    savedTurn = savedYaw = savedPitch = lbl_80345EC8;
    if (gNumTransmitters != 0) {
        CopyCam((u8*)cam, (u8*)backup);
        savedTurn = (f32)lbl_80344400;
        savedPitch = lbl_80344530;
        savedYaw = lbl_80344534;
    }

    camera_orbit_update(camIdx);
    if (cam->trans_mode >= 0) {
        transitionParts = 2;
        switch (cam->trans_mode) {
        case 0:
            cam->attn[0] = cam->wpos[0];
            cam->attn[1] = cam->wpos[1];
            cam->attn[2] = cam->wpos[2];
            cam->radius = lbl_80345F80;
            calc_cam_pyr(camIdx, 1);
            get_cam_wpos(camIdx);
            zeroValue = lbl_80345EC8;
            cam->vel[0] = zeroValue;
            cam->vel[1] = zeroValue;
            cam->vel[2] = zeroValue;
            cam->avel[0] = zeroValue;
            cam->avel[1] = zeroValue;
            cam->avel[2] = zeroValue;
            FOLLOW_NORMALIZE_POSITION(cam, normalizeInitial, followRadius);
            FOLLOW_RENDER(camIdx, cam, initialPosition, initialAttention,
                          initialDirection);
            ProcCamera(camIdx, 0);
            lbl_803443FC = 0;
            cam->trans_mode = -1;
            break;

        case 1:
            if (gScriptedCameraState <= 1) {
                oldPositionX = cam->wpos[0];
                oldPositionY = cam->wpos[1];
                oldPositionZ = cam->wpos[2];
                oldAttentionX = cam->attn[0];
                oldAttentionY = cam->attn[1];
                oldAttentionZ = cam->attn[2];

                calc_cam_pyr(camIdx, 1);
                get_attn_pos(camIdx, cam->attn);
                zeroValue = lbl_80345EC8;
                cam->delta[0] = zeroValue;
                cam->delta[1] = zeroValue;
                cam->delta[2] = zeroValue;
                if (lbl_803447B8 == 0) {
                    get_cam_wpos(camIdx);
                }
                if (adjust_radius(camIdx) == 0) {
                    return;
                }
                if (lbl_803443F4 != 0) {
                    FOLLOW_NORMALIZE_POSITION(cam, normalizeTransition,
                                              followRadius);
                }
                zeroValue = lbl_80345EC8;
                cam->vel[0] = zeroValue;
                cam->vel[1] = zeroValue;
                cam->vel[2] = zeroValue;
                cam->avel[0] = zeroValue;
                cam->avel[1] = zeroValue;
                cam->avel[2] = zeroValue;

                dx = cam->attn[0] - oldAttentionX;
                dy = cam->attn[1] - oldAttentionY;
                dz = cam->attn[2] - oldAttentionZ;
                distance = dz * dz + (dx * dx + dy * dy);
                if (distance > lbl_80345EC8) {
                    root = __frsqrte(distance);
                    root = lbl_80345F18 * root *
                           -(root * root * distance - lbl_80345F20);
                    root = lbl_80345F18 * root *
                           -(root * root * distance - lbl_80345F20);
                    root = lbl_80345F18 * root *
                           -(root * root * distance - lbl_80345F20);
                    transitionAttentionRoot =
                        (f32)(distance * (lbl_80345F18 * root *
                        -(root * root * distance - lbl_80345F20)));
                    distance = transitionAttentionRoot;
                }
                if ((f64)distance < lbl_80345F28) {
                    transitionParts = 1;
                }
                {
                f64 stepScale = lbl_80345F88;
                cam->attn[0] = oldAttentionX +
                    (f32)((f64)dx * stepScale);
                cam->attn[1] = oldAttentionY +
                    (f32)((f64)dy * stepScale);
                cam->attn[2] = oldAttentionZ +
                    (f32)((f64)dz * stepScale);
                }

                dx = cam->wpos[0] - oldPositionX;
                dy = cam->wpos[1] - oldPositionY;
                dz = cam->wpos[2] - oldPositionZ;
                distance = dz * dz + (dx * dx + dy * dy);
                if (distance > lbl_80345EC8) {
                    root = __frsqrte(distance);
                    root = lbl_80345F18 * root *
                           -(root * root * distance - lbl_80345F20);
                    root = lbl_80345F18 * root *
                           -(root * root * distance - lbl_80345F20);
                    root = lbl_80345F18 * root *
                           -(root * root * distance - lbl_80345F20);
                    transitionPositionRoot =
                        (f32)(distance * (lbl_80345F18 * root *
                        -(root * root * distance - lbl_80345F20)));
                    distance = transitionPositionRoot;
                }
                if ((f64)distance < lbl_80345F28) {
                    transitionParts--;
                }
                {
                f64 stepScale = lbl_80345F88;
                cam->wpos[0] = oldPositionX +
                    (f32)((f64)dx * stepScale);
                cam->wpos[1] = oldPositionY +
                    (f32)((f64)dy * stepScale);
                cam->wpos[2] = oldPositionZ +
                    (f32)((f64)dz * stepScale);
                }
                FOLLOW_RENDER(camIdx, cam, transitionPosition,
                              transitionAttention, transitionDirection);
            } else {
                gScriptedCameraState -= gFrameTicks;
                if (gScriptedCameraState <= 1) {
                    gScriptedCameraState = 1;
                }
                if (gScriptedCameraState < 45) {
                    for (scriptedPlayer = 0; scriptedPlayer < 4;
                         scriptedPlayer++) {
                        if (*(s32*)(gPlayers + scriptedPlayer * 0x335C +
                                    0xE8) == 1 &&
                            (*(u32*)(lbl_80240E30 + scriptedPlayer * 0x3C + 8) &
                             0x020000FF) != 0) {
                            gScriptedCameraState = 1;
                        }
                    }
                }
            }
            if (transitionParts == 0) {
                cam->trans_mode = -1;
                if (lbl_803447B8 != 0) {
                    if (lbl_8034440C != 0) {
                        MBRemoveBlit(lbl_8034440C);
                        lbl_8034440C = 0;
                    }
                    lbl_803447B8 = 0;
                    gScriptedCameraState = 0;
                    for (resetPlayer = 0; resetPlayer < 4; resetPlayer++) {
                        if (*(s32*)(gPlayers + resetPlayer * 0x335C +
                                    0xE8) == 1) {
                            *(s32*)(gPlayers + resetPlayer * 0x335C +
                                    0x91C) = 4;
                        }
                    }
                }
            }
            break;

        default:
            calc_cam_pyr(camIdx, 1);
            if (gCameraTargetCount > 0) {
                get_attn_pos(camIdx, cam->attn);
            } else {
                cam->attn[0] = gDefaultPlayerPosition[0];
                cam->attn[1] = gDefaultPlayerPosition[1];
                cam->attn[2] = gDefaultPlayerPosition[2];
                cam->attn_dest[0] = cam->attn[0];
                cam->attn_dest[1] = cam->attn[1];
                cam->attn_dest[2] = cam->attn[2];
                cam->attn_dest_no_offset[0] = cam->attn[0];
                cam->attn_dest_no_offset[1] = cam->attn[1];
                cam->attn_dest_no_offset[2] = cam->attn[2];
            }
            zeroValue = lbl_80345EC8;
            cam->delta[0] = zeroValue;
            cam->delta[1] = zeroValue;
            cam->delta[2] = zeroValue;
            calc_cam_pyr(camIdx, 1);
            get_cam_wpos(camIdx);
            zeroValue = lbl_80345EC8;
            cam->vel[0] = zeroValue;
            cam->vel[1] = zeroValue;
            cam->vel[2] = zeroValue;
            cam->avel[0] = zeroValue;
            cam->avel[1] = zeroValue;
            cam->avel[2] = zeroValue;
            FOLLOW_NORMALIZE_POSITION(cam, normalizeDefault,
                                      followRadius);
            FOLLOW_RENDER(camIdx, cam, resetPosition, resetAttention,
                          resetDirection);
            ProcCamera(camIdx, 0);
            lbl_803443FC = 0;
            cam->trans_mode = -1;
            break;
        }
    }

    if (cam->trans_mode >= 0) {
        return;
    }

    offscreen = 0;
    get_attn_pos(camIdx, focus);
    positionCount = gCameraTargetPositionCount;
    if (positionCount == 6) {
        for (i = 0; i < 6; i++) {
            followPositions[i][0] = followPositions[i + 1][0];
            followPositions[i][1] = followPositions[i + 1][1];
            followPositions[i][2] = followPositions[i + 1][2];
        }
    }
    followPositions[positionCount][0] = focus[0];
    followPositions[positionCount][1] = focus[1];
    followPositions[positionCount][2] = focus[2];
    if (positionCount < 6) {
        gCameraTargetPositionCount++;
    }

    AverageCameraTargetPosition_8002A890(focus);
    cam->delta[0] = focus[0] - cam->attn[0];
    cam->delta[1] = focus[1] - cam->attn[1];
    cam->delta[2] = focus[2] - cam->attn[2];
    if (adjust_radius(camIdx) == 0) {
        return;
    }

    dx = cam->delta[0];
    dy = cam->delta[1];
    dz = cam->delta[2];
    previousSpeed = dx * dx;
    desiredSpeed = dy * dy;
    maximumStep = dz * dz;
    distance = previousSpeed + desiredSpeed;
    distance = maximumStep + distance;
    if (distance > lbl_80345EC8) {
        root = __frsqrte(distance);
        root = lbl_80345F18 * root *
               -(root * root * distance - lbl_80345F20);
        root = lbl_80345F18 * root *
               -(root * root * distance - lbl_80345F20);
        root = lbl_80345F18 * root *
               -(root * root * distance - lbl_80345F20);
        focusRoot = (f32)(distance * (lbl_80345F18 * root *
            -(root * root * distance - lbl_80345F20)));
        distance = focusRoot;
    }

    targetExtent = lbl_803444E8;
    lbl_80344460 = lbl_80344464;
    if ((f64)targetExtent < lbl_80345F90) {
        lbl_80344464 = (f32)(lbl_80345F98 * (f64)(u32)gFrameTicks);
        lbl_80344468 = lbl_80345FA0;
    } else if ((f64)targetExtent >= lbl_80345FA8) {
        lbl_80344464 = (f32)(lbl_80345FB0 * (f64)(u32)gFrameTicks);
        lbl_80344468 = lbl_80345FB8;
    } else {
        f64 extentDelta = lbl_80345FA8 - (f64)targetExtent;
        lbl_80344464 = (f32)(lbl_80345FC0 * extentDelta + lbl_80345FB0);
        lbl_80344468 = (f32)-(lbl_80345FD0 * extentDelta *
            lbl_80345FD8 - lbl_80345FC8);
    }

    if (lbl_80344960 < 0 && (f64)targetExtent >= lbl_80345FE0) {
        lbl_80344464 = targetExtent * (f32)(u32)gFrameTicks;
        lbl_80344468 = lbl_80345FE8;
    } else {
        previousSpeed = lbl_80344460;
        desiredSpeed = lbl_80344464;
        if (previousSpeed < desiredSpeed) {
            if ((f64)(desiredSpeed - previousSpeed) > lbl_80345F90) {
                lbl_80344464 = (f32)(lbl_80345F90 + (f64)previousSpeed);
            }
        } else if ((f64)(previousSpeed - desiredSpeed) > lbl_80345F90) {
            lbl_80344464 = (f32)((f64)previousSpeed - lbl_80345F90);
        }
    }

    desiredSpeed = lbl_80344464;
    if (distance >= desiredSpeed) {
        maximumStep = lbl_80344468;
        if (distance > maximumStep) {
            distance = maximumStep;
        }
        scale = desiredSpeed / distance;
        cam->delta[0] *= scale;
        cam->delta[1] *= scale;
        cam->delta[2] *= scale;
    }
    cam->attn[0] += cam->delta[0];
    cam->attn[1] += cam->delta[1];
    cam->attn[2] += cam->delta[2];

    calc_cam_pyr(camIdx, 0);
    get_cam_wpos(camIdx);
    zeroValue = lbl_80345EC8;
    cam->vel[0] = zeroValue;
    cam->vel[1] = zeroValue;
    cam->vel[2] = zeroValue;
    cam->avel[0] = zeroValue;
    cam->avel[1] = zeroValue;
    cam->avel[2] = zeroValue;
    if (lbl_803443F4 != 0) {
        followRadius = cam->radius;
        normalizeFinal[0] = cam->wpos[0] - cam->attn[0];
        normalizeFinal[1] = cam->wpos[1] - cam->attn[1];
        normalizeFinal[2] = cam->wpos[2] - cam->attn[2];
        SlowNormalVector(normalizeFinal);
        cam->wpos[0] = cam->attn[0] + normalizeFinal[0] * followRadius;
        cam->wpos[1] = cam->attn[1] + normalizeFinal[1] * followRadius;
        cam->wpos[2] = cam->attn[2] + normalizeFinal[2] * followRadius;
    }
    FOLLOW_RENDER(camIdx, cam, finalPosition, finalAttention, finalDirection);

    if (lbl_80343BD8 != 0 && gNumTransmitters != 0 &&
        ((cam->radius >= lbl_80344528 && lbl_80344960 < 0) ||
         ((f64)cam->radius >= lbl_80345FF0 && lbl_80344960 >= 0)) &&
        (lbl_803444F4 == 0 || lbl_80344534 != savedYaw)) {
        Camera* backupCamera;
        s32 backupAttentionMode;
        playerData = (Player*)gPlayers;
        projectionMatrix = (f32*)(state + 0xCC);
        for (viewportPlayer = 0; viewportPlayer < 4;
             viewportPlayer++, playerData++) {
            if (playerData->state == 1) {
                MBWindowProject(playerData->col_pos, projectionMatrix, 0,
                                projected);
                screenX = (f32)projected[0];
                screenY = (f32)projected[1];
                if (screenX < (f32)(lbl_80344520 + 30) ||
                    screenX > (f32)(lbl_8034451C - 30) ||
                    screenY > (f32)(lbl_80344518 - 20) ||
                    screenY < (f32)(lbl_80344514 + 40)) {
                    offscreen = 1;
                }
            }
        }
        if (offscreen != 0) {
            CopyCam((u8*)backup, (u8*)cam);
            lbl_80344400 = (s32)savedTurn;
            lbl_80344530 = savedPitch;
            lbl_80344534 = savedYaw;
        }
        backupCamera = backup;
        backupAttentionMode = backupCamera->a_mode;
        if (backupCamera->c_mode != CAM_OFF) {
            backupCamera->pc_mode = backupCamera->c_mode;
            backupCamera->c_mode = CAM_OFF;
        }
        if (backupAttentionMode != backupCamera->a_mode) {
            backupCamera->pa_mode = backupCamera->a_mode;
            backupCamera->a_mode = (ATN_MODE)backupAttentionMode;
        }
        backupCamera->state = 0;
    }
}
#undef backup
#undef followPositions
#undef FOLLOW_NORMALIZE_POSITION
#undef FOLLOW_RENDER

/* Track the active player object, then settle into an orbit around it. */
void camera_mode_target(s32 camIdx)
{
    Camera* cam = &gCameras[camIdx];
    u8* playerObject;
    s32 playerIndex;
    s32 objectWasMissing = cam->camobj == 0 ? -1 : 0;
    Player* player;
    s32* playerNumber;
    s32 tries;
    f32 targetYaw;
    f32 targetDirectionZ;
    f32 scale;
    f32 angleDelta;
    f32 angleStep;
    f32 currentPitch;
    f32 dx;
    f32 dy;
    f32 dz;
    f32 squaredX;
    f32 squaredY;
    f32 squaredZ;
    f32 distance;
    u8 unused[8];
    volatile f32 moveDelta[3];
    f64 divisorDouble;
    f32 offset[3];
    f32 matrix[16];
    volatile f32 movingRoot;
    volatile f32 radiusRoot;
    Vec3 movingAttention;
    Vec3 movingPosition;
    Vec3 movingDirection;
    f32 normalize[3];
    Vec3 finalAttention;
    Vec3 finalPosition;
    struct {
        f32 pad;
        Vec3 direction;
    } finalDirection;

    playerNumber = &gCameras[0].pn;
    playerIndex = *playerNumber;
    for (tries = 0; tries < 4; tries++) {
        player = (Player*)gPlayers + playerIndex;
        if (player->state == 1 || player->state == 4) {
            playerObject = (u8*)player + 0x14;
            *playerNumber = playerIndex;
            goto found_target_player;
        }
        playerIndex++;
        if (playerIndex >= 4) {
            playerIndex = 0;
        }
    }
    playerObject = 0;

found_target_player:
    cam->camobj = (struct OBJGRP*)playerObject;
    if (cam->camobj == 0) {
        return;
    }
    if (*(u32*)((u8*)cam->camobj + 0x60) == 0) {
        return;
    }
    if (objectWasMissing != 0) {
        cam->trans_mode = 0;
    }

    targetDirectionZ = *(f32*)((u8*)cam->camobj + 0x28);
    targetYaw = atan2(*(f32*)((u8*)cam->camobj + 0x20), targetDirectionZ);

    switch (cam->trans_mode) {
    case 0:
        cam->pyr[0] = 0.0f;
        cam->pyr[1] = targetYaw;
        cam->pyr[2] = 0.0f;
        cam->trans_mode++;
        /* fall through */
    case 1:
        cam->attn[0] = *(f32*)((u8*)cam->camobj + 0x40);
        cam->attn[1] = *(f32*)((u8*)cam->camobj + 0x44);
        cam->attn[2] = *(f32*)((u8*)cam->camobj + 0x48);
        dx = cam->attn[0] - cam->wpos[0];
        dy = cam->attn[1] - cam->wpos[1];
        dz = cam->attn[2] - cam->wpos[2];
        distance = dz * dz + (squaredX = dx * dx + dy * dy);
        if (distance > lbl_80345EC8) {
            f64 guess = __frsqrte(distance);
            guess = lbl_80345F18 * guess * (lbl_80345F20 - guess * guess * distance);
            guess = lbl_80345F18 * guess * (lbl_80345F20 - guess * guess * distance);
            guess = lbl_80345F18 * guess * (lbl_80345F20 - guess * guess * distance);
            movingRoot = (f32)(distance *
                (lbl_80345F18 * guess * (lbl_80345F20 - guess * guess * distance)));
            distance = movingRoot;
        }
        moveDelta[0] = dx;
        divisorDouble = (f64)distance;
        moveDelta[1] = dy;
        moveDelta[2] = dz;

        if ((f64)distance >= 2.0) {
            if ((f64)distance >= 15.0) {
                divisorDouble = (f64)lbl_80345F14;
            }
            scale = (f32)(2.0 / divisorDouble);
            moveDelta[0] *= scale;
            moveDelta[1] *= scale;
            moveDelta[2] *= scale;
            cam->wpos[0] += moveDelta[0];
            cam->wpos[1] += moveDelta[1];
            cam->wpos[2] += moveDelta[2];
            moveDelta[0] = cam->wpos[0] - cam->attn[0];
            moveDelta[1] = cam->wpos[1] - cam->attn[1];
            moveDelta[2] = cam->wpos[2] - cam->attn[2];
            dx = moveDelta[0];
            dz = moveDelta[2];
            dy = moveDelta[1];
            squaredX = dx * dx;
            dz *= dz;
            squaredY = dy * dy;
            distance = squaredX + squaredY;
            distance = dz + distance;
            if (distance > *(volatile f32*)&lbl_80345EC8) {
                f64 guess = __frsqrte(distance);
                guess = lbl_80345F18 * guess * (lbl_80345F20 - guess * guess * distance);
                guess = lbl_80345F18 * guess * (lbl_80345F20 - guess * guess * distance);
                guess = lbl_80345F18 * guess * (lbl_80345F20 - guess * guess * distance);
                radiusRoot = (f32)(distance *
                    (lbl_80345F18 * guess * (lbl_80345F20 - guess * guess * distance)));
                distance = radiusRoot;
            }
            cam->radius = distance;
            cam->pyr[0] = get_pitch(cam->wpos, cam->attn);
            cam->pyr[1] = get_yaw(cam->wpos, cam->attn);
            movingPosition.x = cam->wpos[0];
            movingPosition.y = cam->wpos[1];
            movingPosition.z = cam->wpos[2];
            movingAttention.x = cam->attn[0];
            movingAttention.y = cam->attn[1];
            movingAttention.z = cam->attn[2];
            StandardCamera(camIdx);
            DoShake(&movingPosition, &movingAttention);
            movingDirection.x = movingAttention.x - movingPosition.x;
            movingDirection.y = movingAttention.y - movingPosition.y;
            movingDirection.z = movingAttention.z - movingPosition.z;
            LookInDirection(&movingDirection.x, (u32)&cam->mat[0][0]);
        } else {
            cam->pyr[1] = AddAngle(cam->pyr[1], (f32)CAM_PI);
            cam->radius = lbl_80345F14;
            lbl_803443F4 = 1;
            cam->trans_mode++;
        }
        break;

    case 2:
        currentPitch = cam->pyr[0];
        if ((f64)currentPitch != 0.0) {
            angleStep = (f32)(0.017453292522222223 * (f64)(u32)gFrameTicks);
            if (currentPitch < 0.0f) {
                cam->pyr[0] = AddAngle(cam->pyr[0], angleStep);
                if ((f64)cam->pyr[0] >= 0.0) cam->pyr[0] = 0.0f;
            } else {
                cam->pyr[0] = SubAngle(cam->pyr[0], angleStep);
                if ((f64)cam->pyr[0] <= 0.0) cam->pyr[0] = 0.0f;
            }
        }
        angleDelta = FixAngle(cam->pyr[1] - targetYaw);
        if ((f64)angleDelta != 0.0) {
            angleStep = (f32)(0.034906585044444445 * (f64)(u32)gFrameTicks);
            if (angleDelta < 0.0f) {
                cam->pyr[1] = AddAngle(cam->pyr[1], angleStep);
                angleDelta = FixAngle(cam->pyr[1] - targetYaw);
                if ((f64)angleDelta >= 0.0) cam->pyr[1] = targetYaw;
            } else {
                cam->pyr[1] = SubAngle(cam->pyr[1], angleStep);
                angleDelta = FixAngle(cam->pyr[1] - targetYaw);
                if ((f64)angleDelta <= 0.0) cam->pyr[1] = targetYaw;
            }
        }
        if ((f64)cam->pyr[0] == 0.0 && (f64)cam->pyr[1] == targetYaw) {
            cam->trans_mode = -1;
        }
        break;

    default:
        cam->pyr[1] = targetYaw;
        cam->pyr[2] = 0.0f;
        break;
    }

    if (cam->trans_mode != 1) {
        cam->wpos[0] = *(f32*)((u8*)cam->camobj + 0x40);
        cam->wpos[1] = *(f32*)((u8*)cam->camobj + 0x44);
        cam->wpos[2] = *(f32*)((u8*)cam->camobj + 0x48);
        CreateYPRMatrix(matrix, cam->pyr);
        offset[0] = 0.0f;
        offset[1] = 0.0f;
        offset[2] = cam->radius;
        WorldVector(offset, (f32*)moveDelta, matrix);
        cam->attn[0] = cam->wpos[0] + moveDelta[0];
        cam->attn[1] = cam->wpos[1] + moveDelta[1];
        cam->attn[2] = cam->wpos[2] + moveDelta[2];
        if (lbl_803443F4 != 0) {
            f32 cameraRadius = cam->radius;
            normalize[0] = cam->attn[0] - cam->wpos[0];
            normalize[1] = cam->attn[1] - cam->wpos[1];
            normalize[2] = cam->attn[2] - cam->wpos[2];
            SlowNormalVector(normalize);
            cam->attn[0] = cam->wpos[0] + normalize[0] * cameraRadius;
            cam->attn[1] = cam->wpos[1] + normalize[1] * cameraRadius;
            cam->attn[2] = cam->wpos[2] + normalize[2] * cameraRadius;
        }
        finalPosition.x = cam->wpos[0];
        finalPosition.y = cam->wpos[1];
        finalPosition.z = cam->wpos[2];
        finalAttention.x = cam->attn[0];
        finalAttention.y = cam->attn[1];
        finalAttention.z = cam->attn[2];
        StandardCamera(camIdx);
        DoShake(&finalPosition, &finalAttention);
        finalDirection.direction.x = finalAttention.x - finalPosition.x;
        finalDirection.direction.y = finalAttention.y - finalPosition.y;
        finalDirection.direction.z = finalAttention.z - finalPosition.z;
        LookInDirection(&finalDirection.direction.x, (u32)&cam->mat[0][0]);
    }
}

/*
 * ShakeCamera -- start a camera shake.  A running shake of higher priority is
 * not overridden.
 */
void ShakeCamera(int type, int count, int delay, f32 rad, int priority) {
    if (shaking && priority < shake_priority) {
        return;
    }
    shaking = 1;
    shake_type = type;
    shake_count = count;
    shake_delay = delay;
    shake_rad = rad;
    shake_priority = priority;
}

/*
 * DoShake -- add the current shake offset (if any) into one or both supplied
 * positions.  Called each frame with the camera position (posA) and the
 * look-at/attention position (posB).  The shake ages out over shake_delay and
 * only becomes visible once the shake_count startup counter reaches zero.
 *
 *   type 0 -> shakes posB only
 *   type 1 -> shakes posA only
 *   type 2 -> shakes both
 *
 * The offset lies in the horizontal plane (x,z), oscillating with the frame
 * countdown.
 */
void DoShake(Vec3* posA, Vec3* posB) {
    f32 angle;
    s32 elapsed;

    if (!shaking) {
        return;
    }
    switch (gGameBusy | gGameplayPauseTimer) {
    case 0:
        break;
    default:
        goto shake_done;
    }

    elapsed = gFrameTicks;
    if ((shake_delay -= elapsed) < 0) {
        shaking = 0;
        shake_priority = 0;
    }
    if ((shake_count -= elapsed) < 0) {
        shake_count = 0;
    }

    switch (shake_type) {
    case 0:
        if (shake_count > 0) {
            break;
        }
        angle = FixAngle((f32)(SHAKE_FREQ * (f64)shake_delay));
        posB->x += shake_rad * (f32)cos(angle);
        posB->z += shake_rad * (f32)sin(angle);
        break;
    case 1:
        if (shake_count > 0) {
            break;
        }
        angle = FixAngle((f32)(SHAKE_FREQ * (f64)shake_delay));
        posA->x += shake_rad * (f32)cos(angle);
        posA->z += shake_rad * (f32)sin(angle);
        break;
    case 2:
        if (shake_count > 0) {
            break;
        }
        angle = FixAngle((f32)(SHAKE_FREQ * (f64)shake_delay));
        posB->x += shake_rad * (f32)cos(angle);
        posB->z += shake_rad * (f32)sin(angle);
        posA->x += shake_rad * (f32)cos(angle);
        posA->z += shake_rad * (f32)sin(angle);
        break;
    }
shake_done:
    ;
}

/* Earlier CAMERA.OBJ form of DiffRate, including the no-transmitter path. */
#pragma opt_lifetimes off
void camera_orbit_update(s32 camIdx)
{
    struct {
        f32 difference;
        u8 pad[4];
    } scratch;
    f32* targetAngles = lbl_80118B60;
    u8* state = gCameraState;
    register u8* cameraBase;
    Camera* cam;
    f32* middleHistory;
    f32 crossing;
    f32 angleStep;
    f32 rate;
    f32 zeroValue;
    f64 angle;
    f32 previous;
    f32 prevSpin;

    if (gNumTransmitters == 0) {
        prevSpin = lbl_80344534;
        if (gScriptedCameraState == 0) {
            goto orbit_done;
        }
        switch (lbl_803447B8) {
        case 0:
                if (lbl_80344400 > 0) {
                    lbl_80344534 = (f32)(lbl_80346038 *
                        (f64)(u32)gFrameTicks + prevSpin);
                    angle = targetAngles[lbl_80344538];
                    if (prevSpin < angle && (f64)lbl_80344534 >= angle) {
                        lbl_80344534 = targetAngles[lbl_80344538];
                        gScriptedCameraState = 0;
                    }
                } else {
                    lbl_80344534 = (f32)-(lbl_80346038 *
                        (f64)(u32)gFrameTicks - prevSpin);
                    switch (lbl_80344538) {
                    case 2:
                        if (prevSpin > lbl_80345F68 &&
                            (f64)lbl_80344534 <= lbl_80345F68) {
                            lbl_80344534 = targetAngles[lbl_80344538];
                            gScriptedCameraState = 0;
                        }
                        break;
                    default:
                        angle = targetAngles[lbl_80344538];
                        if (prevSpin > angle &&
                            (f64)lbl_80344534 <= angle) {
                            lbl_80344534 = targetAngles[lbl_80344538];
                            gScriptedCameraState = 0;
                        }
                        break;
                    }
                }
                lbl_80344534 = FixAngle(lbl_80344534);
                break;
        default:
                goto orbit_done;
        }
        goto orbit_done;
    }

    cam = (Camera*)(state + camIdx * 0x18C);
    previous = *(f32*)((u8*)cam + 0x170);
    cam = (Camera*)((u8*)cam + 0xC8);
    (void)cameraBase;
    if (lbl_803447B8 != 0) {
        goto orbit_done;
    }
    switch (lbl_803444E4) {
    case 0:
            middleHistory = (f32*)(state + 0x14);
            middleHistory[1] = middleHistory[0];
            middleHistory[0] = *(f32*)(state + 0x10);
            *(f32*)(state + 0x10) = previous;
            camera_collide_step(camIdx, lbl_80346040);

            scratch.difference = lbl_80344534 - cam->pyr[1];
            *(u32*)&scratch.difference &= 0x7FFFFFFF;
            angleStep = scratch.difference;
            if ((f64)angleStep > lbl_80345F58) {
                angleStep = (f32)(lbl_80345F60 -
                    (f64)scratch.difference);
            }
            angleStep = (f32)((f64)angleStep * lbl_80346048);
            if ((f64)angleStep < lbl_80346050) {
                angleStep = lbl_80346058;
            }
            rate = angleStep * (f32)(u32)gFrameTicks;

            if (lbl_80344400 <= 0) {
                goto orbit_nonpositive;
            }
            if (cam->pyr[1] == lbl_80344534) {
                goto orbit_nonpositive;
            }

            cam->pyr[1] += rate;
            angle = cam->pyr[1];
            if (angle > lbl_80345F58) {
                angle -= lbl_80345F60;
            } else if (angle <= lbl_80345F68) {
                angle = lbl_80345F60 + angle;
            }
            cam->pyr[1] = (f32)angle;
            angle = cam->pyr[1];
            if (previous > angle) {
                if ((f64)lbl_80344534 > previous ||
                    (f64)lbl_80344534 <= angle) {
                    cam->pyr[1] = lbl_80344534;
                    lbl_80344400 = 0;
                }
            } else if ((crossing = cam->pyr[1] - lbl_80344534,
                        (f64)crossing < lbl_80345F58) &&
                       angle >= (f64)lbl_80344534) {
                cam->pyr[1] = lbl_80344534;
                lbl_80344400 = 0;
            }
            goto orbit_direction_done;

orbit_nonpositive:
            if (lbl_80344400 < 0 && cam->pyr[1] != lbl_80344534) {
                cam->pyr[1] -= rate;
                angle = cam->pyr[1];
                if (angle > lbl_80345F58) {
                    angle -= lbl_80345F60;
                } else if (angle <= lbl_80345F68) {
                    angle = lbl_80345F60 + angle;
                }
                cam->pyr[1] = (f32)angle;
                angle = cam->pyr[1];
                if (previous < angle) {
                    if ((f64)lbl_80344534 < previous ||
                        (f64)lbl_80344534 >= angle) {
                        cam->pyr[1] = lbl_80344534;
                        lbl_80344400 = 0;
                    }
                } else if ((crossing = lbl_80344534 - cam->pyr[1],
                            (f64)crossing < lbl_80345F58) &&
                               angle <= (f64)lbl_80344534) {
                    cam->pyr[1] = lbl_80344534;
                    lbl_80344400 = 0;
                }
            } else {
                cam->pyr[1] = lbl_80344534;
                lbl_80344400 = 0;
            }

orbit_direction_done:
            if (cam->pyr[1] > lbl_80345EC8 &&
                middleHistory[0] > lbl_80345EC8 &&
                *(f32*)(state + 0x10) < lbl_80345EC8) {
                goto orbit_crossed_zero;
            }
            zeroValue = *(volatile f32*)&lbl_80345EC8;
            if (cam->pyr[1] < zeroValue) {
                if (middleHistory[0] < zeroValue) {
                    if (*(f32*)(state + 0x10) > zeroValue) {
                        goto orbit_crossed_zero;
                    }
                }
            }
            goto orbit_done;
orbit_crossed_zero:
            cam->pyr[1] = lbl_80344534;
            lbl_80344400 = 0;
            break;
    default:
            goto orbit_done;
    }
orbit_done:
    ;
}
#pragma opt_lifetimes reset
/* Select and blend the two closest active trigger-camera rail nodes. */
s32 camera_collide_step(s32 camIdx, f32 blendThreshold)
{
    Camera* cam = &gCameras[camIdx];
    s32 count = 0;
    s32 rememberSelection = 0;
    s32 index;
    s32 offset;
    s32 remaining;
    s32 loopSelection;
    f32 bestPitch = *(volatile f32*)&lbl_80345EC8;
    f32 secondYaw = bestPitch;
    f32 nearestYaw = bestPitch;
    f32 bestYaw = bestPitch;
    f64 effectiveThreshold = blendThreshold;
    /* blendThreshold is dead once captured above; retail reuses it (still in
     * the incoming f1) as the second-best trigger distance. */
    f32 nearestDistance = *(volatile f32*)&lbl_80346030;
    f32 swapAngle;
    f32 swapDistance;
    f64 root;
    f32 distance;
    f32 segmentLength;
    f32 blendRatio;
    CameraCollideScratch scratch;
    s32 selected;
    s32 best;
    s32 holdSelection;
    s32 sameSelection;
    u8* bestTrigger;
    u8* selectedTrigger;

    blendThreshold = nearestDistance;
    if (lbl_80344508 < 0) {
        effectiveThreshold = lbl_8034601C;
    } else if (camIdx == 0 && lbl_803444DC != 0 && cam->c_mode == 3 &&
               (f64)lbl_803444E8 >= lbl_80346060 &&
               lbl_803444CC != lbl_80344510 &&
               lbl_803444CC != lbl_8034450C &&
               lbl_803444C8 != lbl_80344510 &&
               lbl_803444C8 != lbl_8034450C) {
        effectiveThreshold = lbl_8034601C;
        rememberSelection = 1;
    }

    index = 0;
    offset = 0;
    remaining = sNumTriggerCameras;
    loopSelection = lbl_80344508;
    for (; index < remaining; index++, offset += 0x28) {
            if (sTriggerCameras[offset] == 0 &&
                *(s16*)(sTriggerCameras + offset + 2) != 0) {
                f32 dy = cam->attn_dest_no_offset[1] -
                    *(f32*)(sTriggerCameras + offset + 8);
                f32 dx = cam->attn_dest_no_offset[0] -
                    *(f32*)(sTriggerCameras + offset + 4);
                f32 dz = cam->attn_dest_no_offset[2] -
                    *(f32*)(sTriggerCameras + offset + 0xC);
                f32 candidateDistance;

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
                    scratch.distanceRoot =
                        (f32)(distance * (lbl_80345F18 * root *
                        -(root * root * distance - lbl_80345F20)));
                    distance = scratch.distanceRoot;
                }
                scratch.verticalDifference = dy;
                *(u32*)&scratch.verticalDifference &= 0x7FFFFFFF;
                candidateDistance = distance + scratch.verticalDifference;

                if (loopSelection < 0) {
                    if (candidateDistance < nearestDistance) {
                        count++;
                        lbl_8034450C = lbl_80344510;
                        blendThreshold = nearestDistance;
                        bestYaw = nearestYaw;
                        nearestYaw = *(f32*)(sTriggerCameras + offset + 0x14);
                        secondYaw = bestPitch;
                        bestPitch = *(f32*)(sTriggerCameras + offset + 0x18);
                        nearestDistance = candidateDistance;
                        lbl_80344510 = index;
                    } else if (candidateDistance < blendThreshold) {
                        count++;
                        secondYaw = *(f32*)(sTriggerCameras + offset + 0x18);
                        bestYaw = *(f32*)(sTriggerCameras + offset + 0x14);
                        blendThreshold = candidateDistance;
                        lbl_8034450C = index;
                    }
                } else if (index == loopSelection) {
                    count++;
                    secondYaw = *(f32*)(sTriggerCameras + offset + 0x18);
                    bestYaw = *(f32*)(sTriggerCameras + offset + 0x14);
                    blendThreshold = candidateDistance;
                    lbl_8034450C = index;
                } else if (candidateDistance < nearestDistance) {
                    count++;
                    bestPitch = *(f32*)(sTriggerCameras + offset + 0x18);
                    nearestYaw = *(f32*)(sTriggerCameras + offset + 0x14);
                    nearestDistance = candidateDistance;
                    lbl_80344510 = index;
                }
            }
    }

    best = lbl_80344510;
    if (lbl_80344508 >= 0 && blendThreshold < nearestDistance) {
        lbl_80344510 = lbl_8034450C;
        lbl_8034450C = best;
        swapAngle = bestYaw;
        bestYaw = nearestYaw;
        nearestYaw = swapAngle;
        swapAngle = secondYaw;
        secondYaw = bestPitch;
        bestPitch = swapAngle;
        swapDistance = nearestDistance;
        nearestDistance = blendThreshold;
        blendThreshold = swapDistance;
    }

    if (count == 1) {
        lbl_8034450C = lbl_80344510;
        bestYaw = nearestYaw;
        secondYaw = bestPitch;
        blendThreshold = nearestDistance;
    }
    if (count == 0) {
        goto return_zero;
    }

    best = lbl_80344510;
    selected = lbl_8034450C;
    bestTrigger = sTriggerCameras + best * 0x28;
    selectedTrigger = sTriggerCameras + selected * 0x28;
    if (lbl_80345F78 == (f64)(nearestDistance + blendThreshold)) {
        return 0;
    }
    if (count == 1) {
        return 0;
    }

    {
            f32 sx;
            f32 sy;
            f32 sz;

            PointLineColl(cam->attn,
                (f32*)(bestTrigger + 4),
                (f32*)(selectedTrigger + 4),
                scratch.closest);
            sy = *(f32*)(bestTrigger + 8) - *(f32*)(selectedTrigger + 8);
            sx = *(f32*)(bestTrigger + 4) - *(f32*)(selectedTrigger + 4);
            sz = *(f32*)(bestTrigger + 0xC) -
                 *(f32*)(selectedTrigger + 0xC);
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
                scratch.segmentRoot = (f32)(segmentLength *
                    (lbl_80345F18 * root *
                    -(root * root * segmentLength - lbl_80345F20)));
                segmentLength = scratch.segmentRoot;
            }
            sy = *(f32*)(bestTrigger + 8) - scratch.closest[1];
            sx = *(f32*)(bestTrigger + 4) - scratch.closest[0];
            sz = *(f32*)(bestTrigger + 0xC) - scratch.closest[2];
            distance = sy * sy + sx * sx + sz * sz;
            if ((f64)distance > (f64)lbl_80345EC8) {
                root = __frsqrte(distance);
                root = lbl_80345F18 * root *
                       -(root * root * distance - lbl_80345F20);
                root = lbl_80345F18 * root *
                       -(root * root * distance - lbl_80345F20);
                root = lbl_80345F18 * root *
                       -(root * root * distance - lbl_80345F20);
                scratch.closestRoot = (f32)(distance * (lbl_80345F18 * root *
                    -(root * root * distance - lbl_80345F20)));
                distance = scratch.closestRoot;
            }

            blendRatio = (f32)(distance / segmentLength);
            lbl_80344504 = blendRatio;
            if ((lbl_80344470 -= gFrameTicks) < 0) {
                lbl_80344470 = 0;
            }

            if ((f64)blendRatio <= (f64)effectiveThreshold) {
                holdSelection = 0;
                sameSelection = holdSelection;
                if (lbl_80344508 == lbl_8034450C &&
                    lbl_8034446C == lbl_80344510) {
                    sameSelection = 1;
                }
                if (sameSelection != 0 && lbl_80344470 > 0) {
                    holdSelection = 1;
                }
                if (holdSelection == 0) {
                    lbl_80344534 = bestPitch;
                    lbl_80344530 = nearestYaw;
                    lbl_80344508 = lbl_80344510;
                    lbl_80344470 = 120;
                }
                if ((gCameraTargetCount > 1 || gGameOptions[3] >= 2) &&
                    lbl_80344530 < lbl_80344524) {
                    lbl_80344530 = lbl_80344524;
                }
                if (lbl_80344530 > lbl_80344408) {
                    lbl_80344404 = 1;
                } else {
                    lbl_80344404 = -1;
                }
            } else if ((f64)blendRatio >
                       lbl_80345FE0 - (f64)effectiveThreshold) {
                holdSelection = 0;
                sameSelection = holdSelection;
                if (lbl_80344508 == lbl_80344510 &&
                    lbl_8034446C == lbl_8034450C) {
                    sameSelection = 1;
                }
                if (sameSelection != 0 && lbl_80344470 > 0) {
                    holdSelection = 1;
                }
                if (holdSelection == 0) {
                    lbl_80344534 = secondYaw;
                    lbl_80344530 = bestYaw;
                    lbl_80344508 = lbl_8034450C;
                    lbl_80344470 = 120;
                }
                if ((gCameraTargetCount > 1 || gGameOptions[3] >= 2) &&
                    lbl_80344530 < lbl_80344524) {
                    lbl_80344530 = lbl_80344524;
                }
                if (lbl_80344530 > lbl_80344408) {
                    lbl_80344404 = 1;
                } else {
                    lbl_80344404 = -1;
                }
            }

            distance = (f32)(lbl_80344534 - cam->pyr[1]);
            if ((f64)distance < lbl_80345F68) {
                lbl_80344400 = 1;
            } else if ((f64)distance < lbl_80345F78) {
                lbl_80344400 = -1;
            } else if ((f64)distance < lbl_80345F58) {
                lbl_80344400 = 1;
            } else {
                lbl_80344400 = -1;
            }
    }

    scratch.finalAngle = FixAngle(lbl_80344530 - lbl_80344408);
    *(u32*)&scratch.finalAngle &= 0x7FFFFFFF;
    distance = (f32)__fabs(lbl_80345F58 - (f64)scratch.finalAngle);
    if (rememberSelection != 0) {
        lbl_803444CC = lbl_80344510;
        lbl_803444C8 = lbl_8034450C;
    }
    if ((f64)distance < lbl_80346068) {
        return -1;
    }
return_zero:
    return 0;
}

/*
 * camera_request_change -- latch a pending camera request.  Gated by the
 * camera-active flag.  Records the request mode, keeps the running maximum of
 * the request value, and clears that value again for low modes once the
 * blend ratio is still below 0.9.  (State-global purposes are unconfirmed;
 * transcribed faithfully from the target asm.)
 */
void camera_request_change(s32 value, s32 mode) {
    if (lbl_80343BD8 == 0) {
        return;
    }
    lbl_80344500 = mode;
    lbl_803444FC = 1;
    if (lbl_803444F8 < value) {
        lbl_803444F8 = value;
    }
    if (mode >= 2) {
        return;
    }
    if (lbl_803444E8 < 0.9) {
        lbl_803444F8 = 0;
    }
}

#pragma opt_common_subs off
/* Initialize the four gameplay views from the active level camera record. */
void camera_mode_level(s32 reset)
{
    u8* state = gCameraState;
    Camera* cam0;
    Camera* cam1;
    Camera* cam2;
    Camera* cam3;
    s32 cameraIndex;
    Player* player;
    s32 tries;
    s32 playerIndex;
    u8* levelCamera = *(u8**)(gCurLevel + 0x60);
    s32 cameraOffset;
    s32* playerNumber;
    struct OBJGRP* playerObject;
    f32 dx;
    f32 dy;
    f32 dz;
    f32 distance;
    f32 radius;
    f32 levelOffset;
    f64 root;
    f64 wrappedAngle;
    f32 zeroValue;
    CameraLevelScratch scratch;

    lbl_803444F0 = -1;
    lbl_803444EC = -1;
    lbl_80344960 = -1;
    player = (Player*)gPlayers;
    for (tries = 0; tries < 4;
         tries++, player++) {
        if (player->state == 1) {
            UpdatePlayerWorldMat(player, 1);
        }
    }

    gNumEnemies = *(s16*)(levelCamera + 0x34);
    if (*(s8*)(levelCamera + 0x24) == 0) {
        levelOffset = lbl_80346010;
        *(f32*)(levelCamera + 0x0C) =
            *(f32*)(gWorldInfo + 0x18) + levelOffset;
        *(f32*)(levelCamera + 0x10) =
            *(f32*)(gWorldInfo + 0x1C) + lbl_80345EC8;
        *(f32*)(levelCamera + 0x14) =
            *(f32*)(gWorldInfo + 0x20) + levelOffset;
        *(f32*)(levelCamera + 0x18) =
            *(f32*)(gWorldInfo + 0x24) + (levelOffset = lbl_80346014);
        *(f32*)(levelCamera + 0x1C) =
            *(f32*)(gWorldInfo + 0x28) + lbl_80346018;
        *(f32*)(levelCamera + 0x20) =
            *(f32*)(gWorldInfo + 0x2C) + levelOffset;
    }
    *(f32*)(state + 0xBC) = *(f32*)(levelCamera + 0x0C);
    *(f32*)(state + 0xC0) = *(f32*)(levelCamera + 0x10);
    *(f32*)(state + 0xC4) = *(f32*)(levelCamera + 0x14);
    *(f32*)(state + 0xB0) = *(f32*)(levelCamera + 0x18);
    *(f32*)(state + 0xB4) = *(f32*)(levelCamera + 0x1C);
    *(f32*)(state + 0xB8) = *(f32*)(levelCamera + 0x20);
    lbl_80344414 = 2;

    cam0 = (Camera*)(state + 0xC8);
    {
        CAM_MODE previousMode = cam0->c_mode;
        if (previousMode != CAM_GAME) {
            cam0->pc_mode = previousMode;
            cam0->c_mode = CAM_GAME;
        }
    }
    if (cam0->a_mode != ATN_TARGET) {
        cam0->pa_mode = cam0->a_mode;
        cam0->a_mode = ATN_TARGET;
    }
    cam0->trans_mode = 3;
    gCameraTargetPositionCount = 0;
    gCameraTargetMode = 7;
    lbl_80344508 = -1;
    lbl_80344494 = 0;
    gScriptedCameraState = 0;
    zeroValue = lbl_80345EC8;
    cam0->vel[0] = zeroValue;
    cam0->vel[1] = zeroValue;
    cam0->vel[2] = zeroValue;
    cam0->avel[0] = zeroValue;
    cam0->avel[1] = zeroValue;
    cam0->avel[2] = zeroValue;
    lbl_803444E0 = 0;

    if (gCameraTargetCount == 0) {
        cam0->attn[0] = gDefaultPlayerPosition[0];
        cam0->attn[1] = gDefaultPlayerPosition[1];
        cam0->attn[2] = gDefaultPlayerPosition[2];
        cam0->attn_dest[0] = cam0->attn[0];
        cam0->attn_dest[1] = cam0->attn[1];
        cam0->attn_dest[2] = cam0->attn[2];
        cam0->attn_dest_no_offset[0] = cam0->attn[0];
        cam0->attn_dest_no_offset[1] = cam0->attn[1];
        cam0->attn_dest_no_offset[2] = cam0->attn[2];
    } else {
        get_attn_pos(0, cam0->attn);
    }
    lbl_80344534 = lbl_80118B60[lbl_80344538];
    if (gNumTransmitters != 0) {
        *(s16*)(CurTransmitter + 2) = 0;
        camera_collide_step(0, lbl_8034601C);
        *(volatile f32*)&lbl_80344408 =
            *(volatile f32*)&lbl_80344530;
        cam0->pyr[0] = *(volatile f32*)&lbl_80344530;
        cam0->pyr[1] = *(volatile f32*)&lbl_80344534;
    }
    calc_cam_pyr(0, 1);
    get_cam_wpos(0);
    if (lbl_803443F4 != 0) {
        radius = cam0->radius;
        scratch.normalizeLevel[0] = cam0->wpos[0] - cam0->attn[0];
        scratch.normalizeLevel[1] = cam0->wpos[1] - cam0->attn[1];
        scratch.normalizeLevel[2] = cam0->wpos[2] - cam0->attn[2];
        SlowNormalVector(scratch.normalizeLevel);
        cam0->wpos[0] = cam0->attn[0] + scratch.normalizeLevel[0] * radius;
        cam0->wpos[1] = cam0->attn[1] + scratch.normalizeLevel[1] * radius;
        cam0->wpos[2] = cam0->attn[2] + scratch.normalizeLevel[2] * radius;
    }
    cam0->cam_dest[0] = cam0->wpos[0];
    cam0->cam_dest[1] = cam0->wpos[1];
    cam0->cam_dest[2] = cam0->wpos[2];
    cam0->attn_dest[0] = cam0->attn[0];
    cam0->attn_dest[1] = cam0->attn[1];
    cam0->attn_dest[2] = cam0->attn[2];
    scratch.levelPosition.x = cam0->wpos[0];
    scratch.levelPosition.y = cam0->wpos[1];
    scratch.levelPosition.z = cam0->wpos[2];
    scratch.levelAttention.x = cam0->attn[0];
    scratch.levelAttention.y = cam0->attn[1];
    scratch.levelAttention.z = cam0->attn[2];
    StandardCamera(0);
    DoShake(&scratch.levelPosition, &scratch.levelAttention);
    scratch.levelDirection.x =
        scratch.levelAttention.x - scratch.levelPosition.x;
    scratch.levelDirection.y =
        scratch.levelAttention.y - scratch.levelPosition.y;
    scratch.levelDirection.z =
        scratch.levelAttention.z - scratch.levelPosition.z;
    LookInDirection(&scratch.levelDirection.x, (u32)&cam0->mat[0][0]);
    cam0->state = 1;

    cam1 = (Camera*)(state + 0x254);
    {
        CAM_MODE previousMode = cam1->c_mode;
        if (previousMode != CAM_OBJEYE) {
            cam1->pc_mode = previousMode;
            cam1->c_mode = CAM_OBJEYE;
        }
    }
    if (cam1->a_mode != ATN_FREE) {
        cam1->pa_mode = cam1->a_mode;
        cam1->a_mode = ATN_FREE;
    }
    cam1->trans_mode = 0;
    playerIndex = *(playerNumber = (s32*)(state + 0x1C8));
    for (tries = 0; tries < 4; tries++) {
        player = (Player*)gPlayers + playerIndex;
        if (player->state == 1 || player->state == 4) {
            *playerNumber = playerIndex;
            playerObject = (struct OBJGRP*)((u8*)player + 0x14);
            goto level_player_found;
        }
        playerIndex++;
        if (playerIndex >= 4) {
            playerIndex = 0;
        }
    }
    playerObject = 0;

level_player_found:
    cam1->camobj = playerObject;
    cam1->state = 1;
    cam2 = (Camera*)(state + 0x3E0);
    if (gGameMode == 0x8006 || gGameMode == 0x8003 || reset != 0 ||
        CurTransmitter == 0) {
        cam2->state = 0;
        lbl_8034453C = 0;
    } else {
        cam2->wpos[0] = *(f32*)(CurTransmitter + 4);
        cam2->wpos[1] = *(f32*)(CurTransmitter + 8);
        cam2->wpos[2] = *(f32*)(CurTransmitter + 0x0C);
        cam2->pyr[0] = *(f32*)(CurTransmitter + 0x14);
        cam2->pyr[1] = *(f32*)(CurTransmitter + 0x18);
        cam2->pyr[2] = *(f32*)(CurTransmitter + 0x1C);
        cam2->pyr[0] = -cam2->pyr[0];
        wrappedAngle = cam2->pyr[1];
        wrappedAngle += CAM_PI;
        cam2->pyr[1] = (f32)wrappedAngle;
        wrappedAngle = cam2->pyr[1];
        if (wrappedAngle > CAM_PI) {
            wrappedAngle -= CAM_2PI;
        } else if (wrappedAngle <= -CAM_PI) {
            wrappedAngle = CAM_2PI + wrappedAngle;
        }
        cam2->pyr[1] = (f32)wrappedAngle;
        CreateYPRMatrix(scratch.transmitterMatrix, cam2->pyr);

        dy = cam2->wpos[1] - gDefaultPlayerPosition[1];
        dx = cam2->wpos[0] - gDefaultPlayerPosition[0];
        dz = cam2->wpos[2] - gDefaultPlayerPosition[2];
        distance = dy * dy + dx * dx + dz * dz;
        if (distance > lbl_80345EC8) {
            root = __frsqrte(distance);
            root = lbl_80345F18 * root *
                   -(root * root * distance - lbl_80345F20);
            root = lbl_80345F18 * root *
                   -(root * root * distance - lbl_80345F20);
            root = lbl_80345F18 * root *
                   -(root * root * distance - lbl_80345F20);
            scratch.transmitterRoot = (f32)(distance *
                (lbl_80345F18 * root *
                 -(root * root * distance - lbl_80345F20)));
            distance = scratch.transmitterRoot;
        }
        cam2->radius = distance;
        scratch.offset.x = lbl_80345EC8;
        scratch.offset.y = lbl_80345EC8;
        scratch.offset.z = cam2->radius;
        WorldVector(&scratch.offset.x, &scratch.transformed.x,
                    scratch.transmitterMatrix);
        cam2->attn[0] = cam2->wpos[0] + scratch.transformed.x;
        cam2->attn[1] = cam2->wpos[1] + scratch.transformed.y;
        cam2->attn[2] = cam2->wpos[2] + scratch.transformed.z;
        wrappedAngle = cam2->pyr[1];
        wrappedAngle += CAM_PI;
        cam2->pyr[1] = (f32)wrappedAngle;
        wrappedAngle = cam2->pyr[1];
        if (wrappedAngle > CAM_PI) {
            wrappedAngle -= CAM_2PI;
        } else if (wrappedAngle <= -CAM_PI) {
            wrappedAngle = CAM_2PI + wrappedAngle;
        }
        cam2->pyr[1] = (f32)wrappedAngle;
        if (cam2->c_mode != CAM_LOCK) {
            cam2->pc_mode = cam2->c_mode;
            cam2->c_mode = CAM_LOCK;
        }
        if (cam2->a_mode != ATN_LOCK) {
            cam2->pa_mode = cam2->a_mode;
            cam2->a_mode = ATN_LOCK;
        }
        cam2->trans_mode = 0;
        scratch.transmitterPosition.x = cam2->wpos[0];
        scratch.transmitterPosition.y = cam2->wpos[1];
        scratch.transmitterPosition.z = cam2->wpos[2];
        scratch.transmitterAttention.x = cam2->attn[0];
        scratch.transmitterAttention.y = cam2->attn[1];
        scratch.transmitterAttention.z = cam2->attn[2];
        StandardCamera(2);
        DoShake(&scratch.transmitterPosition, &scratch.transmitterAttention);
        scratch.transmitterDirection.x =
            scratch.transmitterAttention.x - scratch.transmitterPosition.x;
        scratch.transmitterDirection.y =
            scratch.transmitterAttention.y - scratch.transmitterPosition.y;
        scratch.transmitterDirection.z =
            scratch.transmitterAttention.z - scratch.transmitterPosition.z;
        LookInDirection(&scratch.transmitterDirection.x,
                        (u32)&cam2->mat[0][0]);
        gScriptedCameraState = 91;
        lbl_803447B8 = 1;
        init_stage_info();
        lbl_8034453C = 2;
        cam2->state = 1;
    }

    cam3 = (Camera*)(state + 0x56C);
    {
        CAM_MODE previousMode = cam3->c_mode;
        if (previousMode != CAM_VECDIST) {
            cam3->pc_mode = previousMode;
            cam3->c_mode = CAM_VECDIST;
        }
    }
    if (cam3->a_mode != ATN_TARGET) {
        cam3->pa_mode = cam3->a_mode;
        cam3->a_mode = ATN_TARGET;
    }
    cameraIndex = 0;
    cam3->trans_mode = cameraIndex;
    {
    f32 cam3Zero = lbl_80345EC8;
    cam3->vel[0] = cam3Zero;
    cam3->vel[1] = cam3Zero;
    cam3->vel[2] = cam3Zero;
    cam3->avel[0] = cam3Zero;
    cam3->avel[1] = cam3Zero;
    cam3->avel[2] = cam3Zero;
    cam3->attn[0] = *(f32*)(state + 0x1F4);
    cam3->attn[1] = *(f32*)(state + 0x1F8);
    cam3->attn[2] = *(f32*)(state + 0x1FC);
    cam3->radius = lbl_80346020;
    cam3->pyr[0] = lbl_80346024;
    cam3->pyr[1] = cam3Zero;
    cam3->pyr[2] = cam3Zero;
    CreateYPRMatrix(scratch.overheadMatrix, cam3->pyr);
    }
    scratch.offset.x = lbl_80345EC8;
    scratch.offset.y = lbl_80345EC8;
    scratch.offset.z = cam3->radius;
    WorldVector(&scratch.offset.x, &scratch.transformed.x,
                scratch.overheadMatrix);
    cam3->wpos[0] = cam3->attn[0] + scratch.transformed.x;
    cam3->wpos[1] = cam3->attn[1] + scratch.transformed.y;
    cam3->wpos[2] = cam3->attn[2] + scratch.transformed.z;
    scratch.overheadPosition.x = cam3->wpos[0];
    scratch.overheadPosition.y = cam3->wpos[1];
    scratch.overheadPosition.z = cam3->wpos[2];
    scratch.overheadAttention.x = cam3->attn[0];
    scratch.overheadAttention.y = cam3->attn[1];
    scratch.overheadAttention.z = cam3->attn[2];
    StandardCamera(3);
    DoShake(&scratch.overheadPosition, &scratch.overheadAttention);
    scratch.overheadDirection.x =
        scratch.overheadAttention.x - scratch.overheadPosition.x;
    scratch.overheadDirection.y =
        scratch.overheadAttention.y - scratch.overheadPosition.y;
    scratch.overheadDirection.z =
        scratch.overheadAttention.z - scratch.overheadPosition.z;
    LookInDirection(&scratch.overheadDirection.x, (u32)&cam3->mat[0][0]);
    cam3->state = 1;

    cameraOffset = cameraIndex;
    zeroValue = *(volatile f32*)&lbl_80345EC8;
    do {
        Camera* cam = (Camera*)(state + cameraOffset);
        cam = (Camera*)((u8*)cam + 0xC8);
        ProcCamera(cameraIndex, 0);
        cam->limit_pos[0] = cam->mat[3][0];
        cam->limit_pos[1] = cam->mat[3][1];
        cam->limit_pos[2] = cam->mat[3][2];
        cam->limit_vel[0] = zeroValue;
        cam->limit_vel[1] = zeroValue;
        cam->limit_vel[2] = zeroValue;
        cameraIndex++;
        cameraOffset += sizeof(Camera);
    } while (cameraIndex <= 3);
}
#pragma opt_common_subs on

/* Update the single-player rotating camera mode. */
void camera_mode_spin(s32 camIdx)
{
    u8* state;
    f32* settings;
    Camera* cam;
    f32 yaw;
    f32 sine;
    f32 cosine;
    f32 deltaX;
    f32 deltaY;
    f32 deltaZ;
    u8 vectorPad[20];
    f32 distance;
    volatile f32 root;
    u8 pad[8];

    state = gCameraState;
    settings = (f32*)state;
    cam = &((Camera*)(state + 0xC8))[camIdx];
    if (camIdx == 0 &&
        (gGameMode == 0x400D || gGameMode == 0x4013 ||
         gGameMode == 0x4017)) {
        cam->pyr[1] = camera_approach_yaw(cam, cam->num1);
        cam = (Camera*)(state + 0xC8);
        yaw = cam->pyr[1];
        sine = sin(yaw);
        cosine = cos(yaw);

        settings[75] = sine * settings[7];
        settings[76] = settings[8];
        settings[77] = cosine * settings[9];
        settings[125] = sine * settings[10];
        settings[126] = settings[11];
        settings[127] = cosine * settings[12];

        deltaX = settings[75] - settings[125];
        deltaY = settings[76] - settings[126];
        deltaZ = settings[77] - settings[127];
        distance = deltaZ * deltaZ +
                   (distance = deltaX * deltaX + deltaY * deltaY);
        if (distance > lbl_80345EC8) {
            f64 guess = __frsqrte(distance);
            guess = lbl_80345F18 * guess * (lbl_80345F20 - guess * guess * distance);
            guess = lbl_80345F18 * guess * (lbl_80345F20 - guess * guess * distance);
            guess = lbl_80345F18 * guess * (lbl_80345F20 - guess * guess * distance);
            root = (f32)(distance *
                         (lbl_80345F18 * guess * (lbl_80345F20 - guess * guess * distance)));
            distance = root;
        }
        cam->radius = distance;

        deltaX = sine * cam->num2;
        deltaY = 0.0f;
        deltaZ = cosine * cam->num2;
        cam->wpos[0] += deltaX;
        cam->wpos[1] += deltaY;
        cam->wpos[2] += deltaZ;
        cam->attn[0] += deltaX;
        cam->attn[1] += deltaY;
        cam->attn[2] += deltaZ;
    }
}

/* Follow the milestone route while maintaining an orbit around the focus. */
void camera_mode_orbit(s32 camIdx)
{
    Camera* cam;
    CameraMilestone* milestone;
    f32 matrix[16];
    f32 localOffset[3];
    f32 transformed[3];
    struct {
        f32 value[3];
        u8 gap[28];
    } directionWork;
    f32 shakenAttn[3];
    f32 shakenPos[3];
    struct {
        u8 pad[20];
        f32 value[3];
    } lookWork;
    f32 sine;
    f32 cosineInput;
    f32 zero;
    f32 cosine;
    f64 tickScale;

    cam = &gCameras[camIdx];
    if (camIdx == 0) {
        sine = sin(cam->num1);
        cosineInput = cam->num1;
        zero = lbl_80345EC8;
        cosine = cos(cosineInput);
        tickScale = 0.1 * (f64)(u32)gFrameTicks;
        cosineInput = (f32)((f64)sine * tickScale);
        zero *= tickScale;
        cosine = (f32)((f64)cosine * tickScale);
        cam->wpos[0] = cam->wpos[0] + (cosineInput = cosineInput);
        cam->wpos[1] += zero;
        cam->wpos[2] += cosine;

        cam->wpos[1] = (f32)(4.5 +
            (f64)FloorPos((f32)((f64)cam->wpos[1] - 4.5), 0.1f,
                          cam->wpos, 0));

        milestone = sMilestones;
        if ((f64)fqdist(milestone[cam->mode].position[0] - cam->wpos[0],
                        milestone[cam->mode].position[2] - cam->wpos[2]) <= 1.5) {
            cam->mode = fn_800511D0(cam->mode, 0.1745329201221466f);
        }
        milestone = &sMilestones[cam->mode];
        cam->num1 = get_yaw(milestone->position, cam->wpos);
        cam->pyr[1] = camera_lerp_yaw(cam->pyr[1], cam->num1);

        CreateYPRMatrix(matrix, cam->pyr);
        localOffset[0] = 0.0f;
        localOffset[1] = 0.0f;
        localOffset[2] = cam->radius;
        WorldVector(localOffset, transformed, matrix);
        cam->attn[0] = cam->wpos[0] + transformed[0];
        cam->attn[1] = cam->wpos[1] + transformed[1];
        cam->attn[2] = cam->wpos[2] + transformed[2];

        sine = cam->radius;
        directionWork.value[0] = cam->attn[0] - cam->wpos[0];
        directionWork.value[1] = cam->attn[1] - cam->wpos[1];
        directionWork.value[2] = cam->attn[2] - cam->wpos[2];
        SlowNormalVector(directionWork.value);
        cam->attn[0] = cam->wpos[0] + directionWork.value[0] * sine;
        cam->attn[1] = cam->wpos[1] + directionWork.value[1] * sine;
        cam->attn[2] = cam->wpos[2] + directionWork.value[2] * sine;

        shakenPos[0] = cam->wpos[0];
        shakenPos[1] = cam->wpos[1];
        shakenPos[2] = cam->wpos[2];
        shakenAttn[0] = cam->attn[0];
        shakenAttn[1] = cam->attn[1];
        shakenAttn[2] = cam->attn[2];
        StandardCamera(camIdx);
        DoShake((Vec3*)shakenPos, (Vec3*)shakenAttn);
        lookWork.value[0] = shakenAttn[0] - shakenPos[0];
        lookWork.value[1] = shakenAttn[1] - shakenPos[1];
        lookWork.value[2] = shakenAttn[2] - shakenPos[2];
        LookInDirection(lookWork.value, (u32)&cam->mat[0][0]);
    }
}

/*
 * camera_approach_yaw -- rate-limit the camera's yaw field (cam+0xA8) toward
 * `target`, wrapping through the shortest arc, at up to ~2 degrees per frame
 * step (scaled by the frame delta).  Returns the new yaw; the caller stores
 * it back.  Snaps to the target once within one step.
 */
#pragma opt_lifetimes off
#pragma opt_propagation off
f32 camera_approach_yaw(void* cam, f32 target) {
    s32 snap;
    s32 direction;
    f32 delta;
    f32 current;
    f32 step;
    f64 raw;
    f64 wrapped;
    f64 result;

    snap = 0;
    current = CAM_F32(cam, CAM_YAW_OFF);
    raw = (f32)(target - current);
    if (raw > 3.141592654) {
        wrapped = raw - 6.283185308;
    } else if (raw <= -3.141592654) {
        wrapped = 6.283185308 + raw;
    } else {
        wrapped = raw;
    }
    delta = (f32)wrapped;
    step = (f32)(0.034906585044444445 * (f64)(u32)gFrameTicks);
    if ((f64)delta >= 0.0) {
        if ((f64)delta <= (f64)step) {
            snap = 1;
        }
        direction = 1;
    } else {
        if ((f64)delta >= -(f64)step) {
            snap = 1;
        }
        direction = -1;
    }
    result = target;
    switch (snap) {
    case 0:
        if (direction > 0) {
            result = (f32)(current + step);
        } else {
            result = (f32)(current - step);
        }
        break;
    }
    if (result > 3.141592654) {
        result = result - 6.283185308;
    } else if (result <= -3.141592654) {
        result = 6.283185308 + result;
    }
    return (f32)result;
}
#pragma opt_lifetimes reset
#pragma opt_propagation reset

/*
 * camera_lerp_yaw -- rate-limit one angle (`current`) toward another
 * (`target`), wrapping through the shortest arc, at up to ~1 degree per frame
 * step (scaled by the frame delta).  Snaps to the target once within ~1
 * degree.  Returns the new angle.
 */
f32 camera_lerp_yaw(f32 current, f32 target) {
    s32 snap;
    s32 direction;
    f64 raw;
    f64 wrapped;
    f64 result;
    f32 delta;
    f32 step;
    struct {
        u8 pad[8];
        f32 value;
    } absWork;

    raw = (f32)(target - current);
    snap = 0;
    direction = 1;
    step = lbl_80345EC8;
    if (raw > 3.141592654) {
        wrapped = raw - 6.283185308;
    } else if (raw <= -3.141592654) {
        wrapped = 6.283185308 + raw;
    } else {
        wrapped = raw;
    }
    delta = (f32)wrapped;
    wrapped = lbl_80346068;
    absWork.value = delta;
    *(u32*)&absWork.value &= 0x7FFFFFFF;
    if ((f64)absWork.value <= wrapped) {
        snap = 1;
    } else {
        step = (f32)(wrapped * (f64)(u32)gFrameTicks);
        if ((f64)delta >= (f64)lbl_80345EC8) {
            if ((f64)delta <= (f64)step) {
                snap = 1;
            }
            direction = 1;
        } else {
            if ((f64)delta >= -(f64)step) {
                snap = 1;
            }
            direction = -1;
        }
    }
    result = target;
    switch (snap) {
    case 0:
        break;
    default:
        goto lerp_adjust_done;
    }
    {
        if (direction > 0) {
            result = (f32)(current + step);
        } else {
            result = (f32)(current - step);
        }
    }
lerp_adjust_done:
    if (result > 3.141592654) {
        result = result - 6.283185308;
    } else if (result <= -3.141592654) {
        result = 6.283185308 + result;
    }
    return (f32)result;
}

#pragma opt_common_subs off
/* Move the primary gameplay camera along a destination-camera rail. */
void camera_mode_dest(s32 camIdx)
{
    Camera* cam = &gCameras[camIdx];
    f32 step;
    f32 distance;
    f32 scale;
    f32 yawDelta;
    f32 yawStep;
    f32 zero;
    f32 targetPitch;
    f32 currentPitch;
    f64 root;
    f64 angle;
    f64 rawAngle;
    s32 pitchReached;
    s32 yawReached;
    f32 matrix[16];
    f32 offset[3];
    f32 transformed[3];
    /* Retail leaves four words between its angle and transform vectors. */
    f32 angles[7];
    f32 orbitNormalize[3];
    volatile f32 directionRoot;
    f32 finalNormalize[3];

    if (camIdx != 0) {
        return;
    }

    zero = lbl_80345EC8;
    cam->vel[0] = zero;
    cam->vel[1] = zero;
    cam->vel[2] = zero;
    cam->avel[0] = zero;
    cam->avel[1] = zero;
    cam->avel[2] = zero;
    if (cam->mode < 2) {
        return;
    }

    switch (lbl_8034441C) {
    case 0:
        step = (f32)(lbl_80346098 * (f64)(u32)gFrameTicks);
        cam->radius -= (f32)((f64)(u32)gFrameTicks *
            (lbl_803460A0 * (f64)(cam->radius - cam->num1)));
        lbl_803443F4 = 1;
        cam->vel[0] = -step;
        if (cam->wpos[1] < cam->num2) {
            cam->vel[1] = (f32)(lbl_80345F88 * (f64)step);
        } else if (cam->wpos[1] > cam->num3) {
            cam->vel[1] = (f32)(lbl_80345F88 * (f64)-step);
        }
        if ((lbl_80344420 -= gFrameTicks) <= 0) {
            lbl_8034429C += gFrameTicks;
        }
        return;

    case 1:
        cam->pyr[1] +=
            (f32)(lbl_803460A8 * (f64)(u32)gFrameTicks);
        angle = cam->pyr[1];
        if (angle > CAM_PI) {
            angle -= CAM_2PI;
        } else if (angle <= -CAM_PI) {
            angle = CAM_2PI + angle;
        }
        cam->pyr[1] = (f32)angle;
        offset[0] = lbl_80345EC8;
        offset[1] = lbl_80345EC8;
        offset[2] = cam->radius;
        CreateYPRMatrix(matrix, cam->pyr);
        WorldVector(offset, transformed, matrix);
        cam->attn[0] = cam->wpos[0] + transformed[0];
        cam->attn[1] = cam->wpos[1] + transformed[1];
        cam->attn[2] = cam->wpos[2] + transformed[2];
        distance = cam->radius;
        orbitNormalize[0] = cam->attn[0] - cam->wpos[0];
        orbitNormalize[1] = cam->attn[1] - cam->wpos[1];
        orbitNormalize[2] = cam->attn[2] - cam->wpos[2];
        SlowNormalVector(orbitNormalize);
        cam->attn[0] = cam->wpos[0] + orbitNormalize[0] * distance;
        cam->attn[1] = cam->wpos[1] + orbitNormalize[1] * distance;
        cam->attn[2] = cam->wpos[2] + orbitNormalize[2] * distance;
        if ((lbl_80344420 -= gFrameTicks) <= 0) {
            lbl_8034429C += gFrameTicks;
        }
        return;

    case -1:
    default:
        break;
    }
    step = (f32)(lbl_80346098 * (f64)(u32)gFrameTicks);
    pitchReached = 0;
    cam->pyr[0] = -cam->pyr[0];
    DiffRate();
    cam->pyr[0] = -cam->pyr[0];

    if (lbl_80344510 != lbl_8034450C) {
        lbl_80344444 = get_pitch(cam->wpos,
            (f32*)(sTriggerCameras + lbl_8034450C * 0x28 + 4));
        lbl_80344448 = get_yaw(cam->wpos,
            (f32*)(sTriggerCameras + lbl_8034450C * 0x28 + 4));
        angle = (f64)FixAngle((f32)(lbl_80345F60 -
                                     (f64)lbl_80344448));
        angle = lbl_803460B0 * angle;
        angle = (f64)(f32)(lbl_803460B8 * angle);
        if (angle < (f64)lbl_80345EC8) {
            angle = (f64)(f32)(angle + lbl_803460C0);
        }
        if (angle > lbl_803460C8) {
            angle = lbl_80345EC8;
        }
        if (lbl_80344A28 == 0) {
            f64 pitchValue = lbl_803460B0 * (f64)lbl_80344444;
            s32 pitchDegrees = (s32)(lbl_803460B8 * pitchValue);
            dbgTextPrintfCol(2, 3, lbl_80111B3C, pitchDegrees,
                             angle);
        }
    }

    if (lbl_80344510 != lbl_8034450C) {
        f32* triggerX = (f32*)(sTriggerCameras + 4);
        f32* triggerY = (f32*)(sTriggerCameras + 8);
        f32* triggerZ = (f32*)(sTriggerCameras + 0xC);
        f32 weight;
        transformed[0] =
            triggerX[lbl_80344510 * 10] - cam->wpos[0];
        transformed[1] =
            triggerY[lbl_80344510 * 10] - cam->wpos[1];
        transformed[2] =
            triggerZ[lbl_80344510 * 10] - cam->wpos[2];
        weight = lbl_8034445C;
        *(volatile f32*)&transformed[0] = transformed[0] * weight;
        *(volatile f32*)&transformed[1] = transformed[1] * weight;
        *(volatile f32*)&transformed[2] = transformed[2] * weight;
        offset[0] =
            triggerX[lbl_8034450C * 10] - cam->wpos[0];
        offset[1] =
            triggerY[lbl_8034450C * 10] - cam->wpos[1];
        offset[2] =
            triggerZ[lbl_8034450C * 10] - cam->wpos[2];
        offset[0] = (f32)((f64)offset[0] * (lbl_80345FE0 - (f64)weight));
        offset[1] = (f32)((f64)offset[1] * (lbl_80345FE0 - (f64)weight));
        offset[2] = (f32)((f64)offset[2] * (lbl_80345FE0 - (f64)weight));
        transformed[0] += offset[0];
        transformed[1] += offset[1];
        transformed[2] += offset[2];
    } else {
        transformed[0] = lbl_80345EC8;
        transformed[1] = lbl_80345EC8;
        transformed[2] = lbl_80345EC8;
        lbl_8034429C += gFrameTicks;
    }

    scale = transformed[0] * transformed[0];
    yawDelta = transformed[1] * transformed[1];
    distance = transformed[2] * transformed[2];
    yawDelta = scale + yawDelta;
    distance = distance + yawDelta;
    if (distance > lbl_80345EC8) {
        root = __frsqrte(distance);
        root = lbl_80345F18 * root *
               -(root * root * distance - lbl_80345F20);
        root = lbl_80345F18 * root *
               -(root * root * distance - lbl_80345F20);
        root = lbl_80345F18 * root *
               -(root * root * distance - lbl_80345F20);
        directionRoot = (f32)(distance *
            (lbl_80345F18 * root *
             -(root * root * distance - lbl_80345F20)));
        distance = directionRoot;
    }

    if ((f64)distance >= lbl_803460D0 ||
        lbl_80344510 != lbl_8034450C) {
        if (lbl_8034442C < lbl_80344444) {
            if ((f64)lbl_80344424 < lbl_80345F70) {
                lbl_80344424 =
                    (f32)((f64)lbl_80344424 + lbl_803460D8);
            }
            lbl_8034442C += lbl_80344424 * (f32)(u32)gFrameTicks;
            if (lbl_8034442C >= lbl_80344444) {
                pitchReached = 1;
            }
        } else if (lbl_8034442C > lbl_80344444) {
            if ((f64)lbl_80344424 > lbl_803460E0) {
                lbl_80344424 =
                    (f32)((f64)lbl_80344424 - lbl_803460D8);
            }
            lbl_8034442C += lbl_80344424 * (f32)(u32)gFrameTicks;
            if (lbl_8034442C <= lbl_80344444) {
                pitchReached = 1;
            }
        } else {
            pitchReached = 1;
        }
        if (pitchReached != 0) {
            if ((f64)lbl_80344424 > lbl_80345F78) {
                lbl_80344424 =
                    (f32)((f64)lbl_80344424 - lbl_803460D8);
            } else if ((f64)lbl_80344424 < lbl_80345F78) {
                lbl_80344424 =
                    (f32)((f64)lbl_80344424 + lbl_803460D8);
            }
        }

        yawReached = 0;
        rawAngle = (f64)(f32)(lbl_80344448 - lbl_80344430);
        if (rawAngle > CAM_PI) {
            angle = rawAngle - CAM_2PI;
        } else if (rawAngle <= -CAM_PI) {
            angle = CAM_2PI + rawAngle;
        } else {
            angle = rawAngle;
        }
        yawDelta = (f32)angle;
        yawStep = lbl_80344428 * (f32)(u32)gFrameTicks;
        if ((f64)yawDelta > (f64)lbl_80345EC8) {
            if ((f64)lbl_80344428 < lbl_80345F70) {
                lbl_80344428 =
                    (f32)((f64)lbl_80344428 + lbl_803460D8);
            }
            if (yawStep >= lbl_80345EC8 && yawDelta <= yawStep) {
                yawReached = 1;
            }
        } else if ((f64)yawDelta < (f64)lbl_80345EC8) {
            if ((f64)lbl_80344428 > lbl_803460E0) {
                lbl_80344428 =
                    (f32)((f64)lbl_80344428 - lbl_803460D8);
            }
            if (yawStep <= lbl_80345EC8 && yawDelta >= yawStep) {
                yawReached = 1;
            }
        } else {
            if (lbl_80344428 > lbl_80345EC8) {
                lbl_80344428 =
                    (f32)((f64)lbl_80344428 - lbl_803460D8);
            } else if (lbl_80344428 < lbl_80345EC8) {
                lbl_80344428 =
                    (f32)((f64)lbl_80344428 + lbl_803460D8);
            }
        }
        if (yawReached != 0) {
            if (lbl_80344428 > lbl_80345EC8) {
                lbl_80344428 =
                    (f32)((f64)lbl_80344428 - lbl_803460D8);
            } else if (lbl_80344428 < lbl_80345EC8) {
                lbl_80344428 =
                    (f32)((f64)lbl_80344428 + lbl_803460D8);
            }
        }
        lbl_80344430 += yawStep;
        angle = lbl_80344430;
        if (angle > CAM_PI) {
            angle -= CAM_2PI;
        } else if (angle <= -CAM_PI) {
            angle = CAM_2PI + angle;
        }
        lbl_80344430 = (f32)angle;

        angles[0] = lbl_8034442C;
        angle = CAM_PI + (f64)lbl_80344430;
        if (angle > CAM_PI) {
            angle -= CAM_2PI;
        } else if (angle <= -CAM_PI) {
            angle = CAM_2PI + angle;
        }
        angles[1] = (f32)angle;
        angles[2] = lbl_80345EC8;
        CreateYPRMatrix(matrix, angles);
        offset[0] = lbl_80345EC8;
        offset[1] = lbl_80345EC8;
        offset[2] = step;
        WorldVector(offset, transformed, matrix);
        cam->wpos[0] += transformed[0];
        cam->wpos[1] += transformed[1];
        cam->wpos[2] += transformed[2];
    }

    zero = lbl_80345EC8;
    cam->pyr_delta[0] = zero;
    cam->pyr_delta[1] = zero;
    cam->pyr_delta[2] = zero;
    targetPitch = lbl_80344530;
    currentPitch = lbl_80344408;
    scale = lbl_80344450 * (f32)(u32)gFrameTicks;
    if (targetPitch - currentPitch > zero) {
        lbl_80344408 = currentPitch + scale;
        if (lbl_80344408 >= targetPitch) {
            lbl_80344408 = targetPitch;
        }
    } else {
        lbl_80344408 = currentPitch - scale;
        if (lbl_80344408 <= targetPitch) {
            lbl_80344408 = targetPitch;
        }
    }
    cam->pyr[0] = lbl_80344408;
    CreateYPRMatrix(matrix, cam->pyr);
    offset[0] = lbl_80345EC8;
    offset[1] = lbl_80345EC8;
    offset[2] = cam->radius;
    WorldVector(offset, transformed, matrix);
    cam->attn[0] = cam->wpos[0] + transformed[0];
    cam->attn[1] = cam->wpos[1] + transformed[1];
    cam->attn[2] = cam->wpos[2] + transformed[2];
    distance = cam->radius;
    finalNormalize[0] = cam->attn[0] - cam->wpos[0];
    finalNormalize[1] = cam->attn[1] - cam->wpos[1];
    finalNormalize[2] = cam->attn[2] - cam->wpos[2];
    SlowNormalVector(finalNormalize);
    cam->attn[0] = cam->wpos[0] + finalNormalize[0] * distance;
    cam->attn[1] = cam->wpos[1] + finalNormalize[1] * distance;
    cam->attn[2] = cam->wpos[2] + finalNormalize[2] * distance;
}
#pragma opt_common_subs on

#pragma opt_common_subs off
/* Simulate the debug camera and report whether an active player leaves it. */
s32 debug_camera_pos(s32 lastPlayer)
{
    char* debugText = lbl_80111A08;
    u8* state = gCameraState;
    Camera* cam = (Camera*)(state + 0x884);
    Camera* sourceCamera;
    CameraDebugScratch scratch;
    s16 projected[2];
    u8 scratchPad[8];
    u8* playerData;
    f32 savedPitch;
    f32 distance;
    f32 extent;
    f32 scale;
    f32 screenX;
    f32 screenY;
    f64 root;
    f64 difference;
    s32 player;
    s32 cameraIndex;
    s32 offscreen;
    s32 previousAttention;
    f32 zeroValue;
    f32 ratio;

    cameraIndex = 5;
    offscreen = 0;
    sourceCamera = (Camera*)(state + 0xC8);
    CopyCam((u8*)sourceCamera, (u8*)cam);
    get_attn_pos(cameraIndex, scratch.desiredAttention);
    lbl_803443F4 = 0;
    adjust_radius(cameraIndex);

    cam->delta[0] = scratch.desiredAttention[0] - cam->attn[0];
    cam->delta[1] = scratch.desiredAttention[1] - cam->attn[1];
    cam->delta[2] = scratch.desiredAttention[2] - cam->attn[2];
    scale = cam->delta[0] * cam->delta[0];
    distance = cam->delta[1] * cam->delta[1];
    extent = cam->delta[2] * cam->delta[2];
    distance = extent + (distance = scale + distance);
    if (distance > lbl_80345EC8) {
        root = __frsqrte(distance);
        root = lbl_80345F18 * root *
               -(root * root * distance - lbl_80345F20);
        root = lbl_80345F18 * root *
               -(root * root * distance - lbl_80345F20);
        root = lbl_80345F18 * root *
               -(root * root * distance - lbl_80345F20);
        scratch.root = (f32)(distance *
            (lbl_80345F18 * root *
             -(root * root * distance - lbl_80345F20)));
        distance = scratch.root;
    }

    extent = lbl_803444E8;
    if ((f64)extent < lbl_80345F90) {
        lbl_80344464 =
            (f32)(lbl_80345F98 * (f64)(u32)gFrameTicks);
        lbl_80344468 = lbl_80345FA0;
    } else if ((f64)extent >= lbl_80345FA8) {
        lbl_80344464 =
            (f32)(lbl_80345FB0 * (f64)(u32)gFrameTicks);
        lbl_80344468 = lbl_80345FB8;
    } else {
        difference = lbl_80345FA8 - (f64)extent;
        lbl_80344464 =
            (f32)(lbl_80345FC0 * difference + lbl_80345FB0);
        lbl_80344468 =
            (f32)-(difference * lbl_80345FD0 * lbl_80345FD8 -
                   lbl_80345FC8);
    }
    if (lbl_80344960 < 0 && (f64)extent >= lbl_80345FE0) {
        lbl_80344464 = extent * (f32)(u32)gFrameTicks;
        lbl_80344468 = lbl_80345FE8;
    }

    scale = lbl_80344464;
    if (distance >= scale) {
        if (distance > lbl_80344468) {
            distance = lbl_80344468;
        }
        ratio = scale / distance;
        cam->delta[0] *= ratio;
        cam->delta[1] *= ratio;
        cam->delta[2] *= ratio;
    }
    cam->attn[0] += cam->delta[0];
    cam->attn[1] += cam->delta[1];
    cam->attn[2] += cam->delta[2];

    savedPitch = lbl_80344408;
    calc_cam_pyr(cameraIndex, 0);
    lbl_80344408 = savedPitch;
    get_cam_wpos(cameraIndex);
    zeroValue = lbl_80345EC8;
    cam->vel[0] = zeroValue;
    cam->vel[1] = zeroValue;
    cam->vel[2] = zeroValue;
    cam->avel[0] = zeroValue;
    cam->avel[1] = zeroValue;
    cam->avel[2] = zeroValue;
    if (lbl_803443F4 != 0) {
        savedPitch = cam->radius;
        scratch.normalize[0] = cam->wpos[0] - cam->attn[0];
        scratch.normalize[1] = cam->wpos[1] - cam->attn[1];
        scratch.normalize[2] = cam->wpos[2] - cam->attn[2];
        SlowNormalVector(scratch.normalize);
        cam->wpos[0] = cam->attn[0] + scratch.normalize[0] * savedPitch;
        cam->wpos[1] = cam->attn[1] + scratch.normalize[1] * savedPitch;
        cam->wpos[2] = cam->attn[2] + scratch.normalize[2] * savedPitch;
    }

    scratch.position[0] = cam->wpos[0];
    scratch.position[1] = cam->wpos[1];
    scratch.position[2] = cam->wpos[2];
    scratch.attention[0] = cam->attn[0];
    scratch.attention[1] = cam->attn[1];
    scratch.attention[2] = cam->attn[2];
    StandardCamera(cameraIndex);
    DoShake((Vec3*)scratch.position, (Vec3*)scratch.attention);
    scratch.direction[0] = scratch.attention[0] - scratch.position[0];
    scratch.direction[1] = scratch.attention[1] - scratch.position[1];
    scratch.direction[2] = scratch.attention[2] - scratch.position[2];
    LookInDirection(scratch.direction, (u32)&cam->mat[0][0]);
    ProcCamera(cameraIndex, 0);

    if (lbl_80344A28 == 0) {
        dbgTextPrintfCol(40, 9, debugText + 196,
                         (s32)cam->wpos[0], (s32)cam->wpos[1],
                         (s32)cam->wpos[2]);
        dbgTextPrintfCol(40, 10, debugText + 220,
                         (s32)cam->attn[0], (s32)cam->attn[1],
                         (s32)cam->attn[2]);
    }

    playerData = gPlayers;
    for (player = 0; player <= lastPlayer;
         player++, playerData += 0x335C) {
        if (*(s32*)(playerData + 0xE8) == 1) {
            MBWindowProject((f32*)(playerData + 0x54),
                            &((Camera*)(state + 0xC8))[cameraIndex].mat[0][0],
                            0, projected);
            screenX = (f32)projected[0];
            screenY = (f32)projected[1];
            if (screenX < (f32)(lbl_80344520 + 30) ||
                screenX > (f32)(lbl_8034451C - 30) ||
                screenY > (f32)(lbl_80344518 - 20) ||
                screenY < (f32)(lbl_80344514 + 40)) {
                lbl_803444F4 = 0;
                offscreen = 1;
                if (lbl_80344A28 == 0) {
                    dbgTextPrintfCol(10, player + 11,
                                     debugText + 244, player);
                }
            } else if (lbl_80344A28 == 0) {
                dbgTextPrintfCol(10, player + 11, debugText + 264);
            }
            if (lbl_80344A28 == 0) {
                dbgTextPrintfCol(40, player + 11, debugText + 284,
                                 (s32)screenX, (s32)screenY, offscreen);
            }
        }
    }

    previousAttention = *(s32*)(state + 0x97C);
    if (cam->c_mode != CAM_OFF) {
        cam->pc_mode = cam->c_mode;
        cam->c_mode = CAM_OFF;
    }
    if (previousAttention != cam->a_mode) {
        cam->pa_mode = cam->a_mode;
        cam->a_mode = (ATN_MODE)previousAttention;
    }
    cam->state = 0;

    if (cam->radius < lbl_80344528 ||
        ((f64)cam->radius < lbl_80345FF0 && lbl_80344960 != 0)) {
        offscreen = 0;
    }
    return offscreen;
}

#define CAMERA_LATCH_CHANGE()                                              \
    do {                                                                   \
        lbl_80344500 = 1;                                                  \
        lbl_803444FC = 1;                                                  \
        if (lbl_803444F8 < 60) {                                           \
            lbl_803444F8 = 60;                                             \
        }                                                                  \
        if ((f64)lbl_803444E8 < lbl_80345FA8) {                            \
            lbl_803444F8 = 0;                                              \
        }                                                                  \
    } while (0)

#define CAMERA_SUPERVISOR_ABS(field, value)                                \
    (scratch.field = (value),                                              \
     *(u32*)&scratch.field &= 0x7FFFFFFF, scratch.field)

/*
 * Classify a proposed player movement against the camera's safe rectangle.
 * The nonzero return values identify which screen-space escape/re-entry path
 * handled the movement; path 5 performs the full distance/boss check and path
 * 6 verifies a temporarily moved player with the debug camera.
 */
s32 camera_debug_supervisor(s32 playerIndex, f32* movementDelta)
{
    u8* playerData = gPlayers + playerIndex * 0x335C;
    u8* state = gCameraState;
    f32* cameraMatrix;
    f32* cameraPositionX;
    f32* cameraPositionZ;
    CameraTarget* target;
    CameraSupervisorScratch scratch;
    f32 oldX;
    f32 oldY;
    f32 currentX;
    f32 currentY;
    f32 dx;
    f32 dy;
    f32 dz;
    f32 movedX;
    f32 movedY;
    f32 movedZ;
    f32 currentDistance;
    f32 movedDistance;
    f32 currentAbsY;
    f32 currentAbsX;
    f32 oldAbsY;
    f32 oldAbsX;
    f32 currentScreenDistance;
    f32 movedScreenDistance;
    f32 cameraDx;
    f32 cameraDz;
    f32 movedCameraDx;
    f32 movedCameraDz;
    f32 zeroValue;
    f64 root;
    s32 screenHeight;
    s32 screenWidth;
    s32 targetIndex;

    screenHeight = MBScreenHeight();
    screenWidth = MBScreenWidth();
    if (*(s32*)(playerData + 0x204) == 29) {
        return 0;
    }

    cameraMatrix = (f32*)(state + 0xCC);
    scratch.futurePosition[0] =
        *(f32*)(playerData + 0x54) + movementDelta[0];
    oldX = *(f32*)(playerData + 0x8A0);
    scratch.futurePosition[1] =
        *(f32*)(playerData + 0x58) + movementDelta[1];
    oldY = *(f32*)(playerData + 0x8A4);
    scratch.futurePosition[2] =
        *(f32*)(playerData + 0x5C) + movementDelta[2];
    MBWindowProject(scratch.futurePosition, cameraMatrix, 0,
                    scratch.projected);
    currentX = (f32)scratch.projected[0];
    currentY = (f32)scratch.projected[1];

    target = (CameraTarget*)(state + 0xA10);
    for (targetIndex = 0; targetIndex < 15; targetIndex++, target++) {
        if (target->object == playerData + 0x14) {
            break;
        }
    }
    if (targetIndex < 15 &&
        (f64)(target->limitedTop[1] - target->limitedBottom[1]) >=
            (f64)lbl_80345F90 *
                (f64)((lbl_80344518 - 20) - (lbl_80344514 + 40))) {
        scratch.alternatePosition[0] =
            *(f32*)(playerData + 0x44) + movementDelta[0];
        scratch.alternatePosition[1] =
            *(f32*)(playerData + 0x48) + movementDelta[1];
        scratch.alternatePosition[2] =
            *(f32*)(playerData + 0x4C) + movementDelta[2];
        MBWindowProject(scratch.alternatePosition, cameraMatrix, 0,
                        scratch.alternateProjected);
        if (target->limitedTop[1] - target->limitedBottom[1] <=
            currentY - (f32)scratch.alternateProjected[1]) {
            zeroValue = lbl_80345EC8;
            movementDelta[0] = zeroValue;
            movementDelta[1] = zeroValue;
            movementDelta[2] = zeroValue;
        }
    }

    if (currentX > (f32)(lbl_80344520 + 30) &&
        currentX < (f32)(lbl_8034451C - 30) &&
        currentY > (f32)(lbl_80344514 + 40) &&
        currentY < (f32)(lbl_80344518 - 20)) {
        if (gCameraTargetCount > 1 &&
            (oldX <= (f32)(lbl_80344520 + 30) ||
             oldX >= (f32)(lbl_8034451C - 30) ||
             oldY <= (f32)(lbl_80344514 + 40) ||
             oldY >= (f32)(lbl_80344518 - 20)) &&
            ((f64)*(f32*)(state + 0x18C) >= lbl_80345FF0 ||
             lbl_80344960 < 0) &&
            lbl_80343BD8 != 0) {
            CAMERA_LATCH_CHANGE();
        }
        return 1;
    } else if (oldX > (f32)(lbl_80344520 + 30) &&
               oldX < (f32)(lbl_8034451C - 30) &&
               currentX > (f32)(lbl_80344520 + 30) &&
               currentX < (f32)(lbl_8034451C - 30) &&
               CAMERA_SUPERVISOR_ABS(return2OldY,
                   oldY - *(f32*)(lbl_80344EE8 + 0xC)) >
                   CAMERA_SUPERVISOR_ABS(return2CurrentY,
                       currentY - *(f32*)(lbl_80344EE8 + 0xC))) {
        if (gCameraTargetCount > 1 &&
            (oldY <= (f32)(lbl_80344514 + 40) ||
             oldY >= (f32)(lbl_80344518 - 20)) &&
            ((f64)*(f32*)(state + 0x18C) >= lbl_80345FF0 ||
             lbl_80344960 < 0) &&
            lbl_80343BD8 != 0) {
            CAMERA_LATCH_CHANGE();
        }
        return 2;
    } else if (oldY > (f32)(lbl_80344514 + 40) &&
               oldY < (f32)(lbl_80344518 - 20) &&
               currentY > (f32)(lbl_80344514 + 40) &&
               currentY < (f32)(lbl_80344518 - 20) &&
               CAMERA_SUPERVISOR_ABS(return3OldX,
                   oldX - *(f32*)(lbl_80344EE8 + 8)) >
                   CAMERA_SUPERVISOR_ABS(return3CurrentX,
                       currentX - *(f32*)(lbl_80344EE8 + 8))) {
        if (gCameraTargetCount > 1 &&
            (oldX <= (f32)(lbl_80344520 + 30) ||
             oldX >= (f32)(lbl_8034451C - 30)) &&
            ((f64)*(f32*)(state + 0x18C) >= lbl_80345FF0 ||
             lbl_80344960 < 0) &&
            lbl_80343BD8 != 0) {
            CAMERA_LATCH_CHANGE();
        }
        return 3;
    } else if (CAMERA_SUPERVISOR_ABS(return4OldY,
                   oldY - *(f32*)(lbl_80344EE8 + 0xC)) >
                   CAMERA_SUPERVISOR_ABS(return4CurrentY,
                       currentY - *(f32*)(lbl_80344EE8 + 0xC)) &&
               CAMERA_SUPERVISOR_ABS(return4OldX,
                   oldX - *(f32*)(lbl_80344EE8 + 8)) >
                   CAMERA_SUPERVISOR_ABS(return4CurrentX,
                       currentX - *(f32*)(lbl_80344EE8 + 8))) {
        if (gCameraTargetCount > 1 &&
            (oldX <= (f32)(lbl_80344520 + 30) ||
             oldX >= (f32)(lbl_8034451C - 30) ||
             oldY <= (f32)(lbl_80344514 + 40) ||
             oldY >= (f32)(lbl_80344518 - 20)) &&
            ((f64)*(f32*)(state + 0x18C) >= lbl_80345FF0 ||
             lbl_80344960 < 0) &&
            lbl_80343BD8 != 0) {
            CAMERA_LATCH_CHANGE();
        }
        return 4;
    }

    if (currentX <= (f32)(lbl_80344520 + 30) ||
        currentX >= (f32)(lbl_8034451C - 30) ||
        currentY <= (f32)(lbl_80344514 + 40) ||
        currentY >= (f32)(lbl_80344518 - 20)) {
        dy = (*(f32*)(playerData + 0x48) +
              *(f32*)(playerData + 0x83C)) - *(f32*)(state + 0x1F8);
        dx = (*(f32*)(playerData + 0x44) +
              *(f32*)(playerData + 0x838)) - *(f32*)(state + 0x1F4);
        dz = (*(f32*)(playerData + 0x4C) +
              *(f32*)(playerData + 0x840)) - *(f32*)(state + 0x1FC);
        currentDistance = dz * dz + dx * dx + dy * dy;
        movedX = dx + movementDelta[0];
        movedY = dy + movementDelta[1];
        movedZ = dz + movementDelta[2];
        cameraPositionX = (f32*)(state + 0x1F4);
        cameraPositionZ = (f32*)(state + 0x1FC);

        {
            s32 bottomEdge = screenHeight - 64;
            s32 halfWidth = screenWidth / 2;
            currentAbsY = oldY - (f32)bottomEdge;
            currentAbsX = oldX - (f32)halfWidth;
            oldAbsY = currentY - (f32)bottomEdge;
            oldAbsX = currentX - (f32)halfWidth;
        }
        currentScreenDistance =
            currentAbsY * currentAbsY + currentAbsX * currentAbsX;
        movedScreenDistance = oldAbsY * oldAbsY + oldAbsX * oldAbsX;

        if (currentDistance > lbl_80345EC8) {
            root = __frsqrte(currentDistance);
            root = lbl_80345F18 * root *
                   -(root * root * currentDistance - lbl_80345F20);
            root = lbl_80345F18 * root *
                   -(root * root * currentDistance - lbl_80345F20);
            root = lbl_80345F18 * root *
                   -(root * root * currentDistance - lbl_80345F20);
            scratch.currentRoot = (f32)(currentDistance *
                (lbl_80345F18 * root *
                 -(root * root * currentDistance - lbl_80345F20)));
            currentDistance = scratch.currentRoot;
        }
        movedDistance = movedZ * movedZ + movedX * movedX + movedY * movedY;
        if (movedDistance > lbl_80345EC8) {
            root = __frsqrte(movedDistance);
            root = lbl_80345F18 * root *
                   -(root * root * movedDistance - lbl_80345F20);
            root = lbl_80345F18 * root *
                   -(root * root * movedDistance - lbl_80345F20);
            root = lbl_80345F18 * root *
                   -(root * root * movedDistance - lbl_80345F20);
            scratch.movedRoot = (f32)(movedDistance *
                (lbl_80345F18 * root *
                 -(root * root * movedDistance - lbl_80345F20)));
            movedDistance = scratch.movedRoot;
        }

        if (gBossType < 0) {
            if (movedDistance > currentDistance &&
                ((s32)currentX != (s32)oldX ||
                 !(currentY > (f32)(lbl_80344514 + 40) &&
                   currentY < (f32)(lbl_80344518 - 20))) &&
                ((s32)currentY != (s32)oldY ||
                 !(currentX > (f32)(lbl_80344520 + 30) &&
                   currentX < (f32)(lbl_8034451C - 30)))) {
                zeroValue = lbl_80345EC8;
                movementDelta[0] = zeroValue;
                movementDelta[1] = zeroValue;
                movementDelta[2] = zeroValue;
            }
        } else if (movedScreenDistance > currentScreenDistance) {
            cameraDz = *cameraPositionZ - *(f32*)(playerData + 0x5C);
            cameraDx = *cameraPositionX - *(f32*)(playerData + 0x54);
            movedCameraDz = cameraDz - movementDelta[2];
            movedCameraDx = cameraDx - movementDelta[0];
            if (movedCameraDx * movedCameraDx +
                    movedCameraDz * movedCameraDz >
                cameraDx * cameraDx + cameraDz * cameraDz &&
                ((s32)currentX != (s32)oldX ||
                 !(currentY > (f32)(lbl_80344514 + 40) &&
                   currentY < (f32)(lbl_80344518 - 20))) &&
                ((s32)currentY != (s32)oldY ||
                 !(currentX > (f32)(lbl_80344520 + 30) &&
                   currentX < (f32)(lbl_8034451C - 30)))) {
                zeroValue = lbl_80345EC8;
                movementDelta[0] = zeroValue;
                movementDelta[1] = zeroValue;
                movementDelta[2] = zeroValue;
            }
        }

        if (oldX <= (f32)(lbl_80344520 + 30) ||
            oldX >= (f32)(lbl_8034451C - 30) ||
            oldY <= (f32)(lbl_80344514 + 40) ||
            oldY >= (f32)(lbl_80344518 - 20)) {
            lbl_803444E4 = 1;
        } else if (gBossType >= 0) {
            debug_camera_pos(playerIndex);
        }
        if (gBossType < 0) {
            debug_camera_pos(playerIndex);
        }

        if (((lbl_80344960 < 0 &&
              *(f32*)(state + 0x18C) >= lbl_80344528) ||
             (lbl_80344960 >= 0 &&
              (f64)*(f32*)(state + 0x18C) >= lbl_80345FF0)) &&
            (f64)lbl_803444E8 >= lbl_80346008) {
            if (gBossType >= 0) {
                if (gCameraTargetCount > 1) {
                    if (lbl_803444E4 == 0) {
                        if (lbl_80343BD8 != 0) {
                            CAMERA_LATCH_CHANGE();
                        }
                    } else {
                        lbl_80344500 = 0;
                        lbl_803444FC = 0;
                        lbl_803444F8 = 0;
                    }
                }
            } else if (gCameraTargetCount > 1 && lbl_80343BD8 != 0) {
                CAMERA_LATCH_CHANGE();
            }
        }
        return 5;
    }

    scratch.futurePosition[0] = *(f32*)(playerData + 0x44);
    scratch.futurePosition[1] = *(f32*)(playerData + 0x48);
    scratch.futurePosition[2] = *(f32*)(playerData + 0x4C);
    *(f32*)(playerData + 0x44) += movementDelta[0];
    *(f32*)(playerData + 0x48) += movementDelta[1];
    *(f32*)(playerData + 0x4C) += movementDelta[2];
    fn_8005A588((struct OBJGRP*)(playerData + 0x14),
                (f32*)(playerData + 0x838));
    if (debug_camera_pos(playerIndex) != 0 && lbl_80343BD8 != 0) {
        CAMERA_LATCH_CHANGE();
    }
    *(f32*)(playerData + 0x44) = scratch.futurePosition[0];
    *(f32*)(playerData + 0x48) = scratch.futurePosition[1];
    *(f32*)(playerData + 0x4C) = scratch.futurePosition[2];
    fn_8005A588((struct OBJGRP*)(playerData + 0x14),
                (f32*)(playerData + 0x838));
    return 6;
}

#undef CAMERA_LATCH_CHANGE
#undef CAMERA_SUPERVISOR_ABS
#pragma opt_common_subs on
