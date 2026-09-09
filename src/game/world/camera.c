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
#include "game/cameradata.h"
#include "game/gamemode.h"
#include "game/leveldata.h"
#include "game/player.h"
#include "game/worldinfo.h"
#include "game/plyrdata.h"
#include "game/item.h"
#include "game/enemy.h"

#ifndef offsetof
#define offsetof(type, memb) ((u32) & ((type*)0)->memb)
#endif

typedef struct Vec3 {
    f32 x;
    f32 y;
    f32 z;
} Vec3;

/* OBJGRP prefix used by camera projection.  The GC target accesses the
 * world-matrix translation row at +0x30 and the attention position at +0x40. */
typedef struct CameraObjectView {
    f32 worldmat[4][4];
    f32 attn_pos[4];
    f32 coll_pos[4];
    struct mbnode* node;
} CameraObjectView;

typedef struct CameraMilestone {
    f32 matrix[12];
    f32 position[3];
    u8 _pad3C[0x2C];
} CameraMilestone; /* 0x68 */

typedef struct CameraTarget {
    /* 0x00 */ s32 active;
    /* 0x04 */ CameraObjectView* object;
    /* 0x08 */ f32 position[3];
    /* 0x14 */ u8 _pad14[4];
    /* 0x18 */ f32 projectedTop[2];
    /* 0x20 */ f32 projectedBottom[2];
    /* 0x28 */ f32 limitedTop[2];
    /* 0x30 */ f32 limitedBottom[2];
} CameraTarget; /* 0x38 */

/* Aggregate companion to game/cameradata.h's CameraStateData.  It stays
 * FILE-LOCAL on purpose: it carries struct-typed members, which must never
 * enter a shared header (claim.law.embedded-struct-member-whole-tu-cascade).
 * Only do_camera and camera_init_for_gamemode still use it, and only through
 * genuine member access (state->cameras[i], state->targets) -- per
 * claim.law.typed-global-member-vs-view-cast-diverges that cast form is
 * evidence about the original source shape, not debt to remove.  Every other
 * site in this TU now spells the same addresses as offsetof() addends on the
 * raw base.  The layout constants come from the header so there is one
 * authority for them. */
typedef struct CameraStateView {
    u8 _pad00[CAMERA_STATE_CAMERAS_OFF];
    Camera cameras[6];
    CameraTarget targets[15];
} CameraStateView;

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

/* Partial view of mb_window.c's MBWINDOW (the live projection window; this
 * TU's .sbss pointer lbl_80344EE8 shares that global with game/world/newcam.c
 * (NcProjWindow: cam.mat/cam.pos at +0x64) and game/boss/bosscam.c (cam.pos
 * at +0x94/+0x98/+0x9C) -- only xcenter/ycenter (+0x008/+0x00C), the fields
 * this TU touches, per mb_window.c's own MBWINDOW typedef (the TU that owns
 * and types the full 0x1A8 struct); not invented. */
typedef struct CameraProjWindowView {
    u8  _pad00[0x008];
    f32 xcenter;   /* 0x008 */
    f32 ycenter;   /* 0x00C */
} CameraProjWindowView;

/* Local view of the "transmitter" record (extern CurTransmitter; also
 * touched raw by game/boss/bosscam.c and game/anim/action.c, no shared
 * struct recovered project-wide yet).  Only the fields this TU reads/
 * writes: a s16 at +0x02 cleared before camera_collide_step (semantics
 * unconfirmed, left unk to avoid inventing a name), world position at
 * +0x04, and pitch/yaw/roll at +0x14 -- the 4-byte gap at +0x10 and the
 * leading 2 bytes are untouched by this TU. */
typedef struct CameraTransmitterView {
    u8  _pad00[2];
    s16 unk02;     /* 0x02; cleared before camera_collide_step */
    f32 wpos[3];   /* 0x04 world position */
    u8  _pad10[4];
    f32 pyr[3];    /* 0x14 pitch/yaw/roll */
} CameraTransmitterView;

/* (The former CameraLevelRecordView local view is gone: gCurLevel is now
 * declared with its real game/leveldata.h type, whose GC-verified `camera`
 * member sits at exactly the +0x60 the view reconstructed.) */

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

/* angle wrapping constants (double, to mirror the sdata2 pool)
 * The retail pool holds the 10-digit decimal literals, not full-precision
 * pi: lbl_80345F58 = 0x400921FB54524550 (3.141592654) against
 * 0x400921FB54442D18 (M_PI).  Nothing in the toolchain scores a pool
 * datum, so the wrong spelling was invisible. */
#define CAM_PI  3.141592654
#define CAM_2PI 6.283185308
/* per-frame turn/oscillation rates recovered from the sdata2 pool */
#define CAM_TURN_RATE_2DEG 0.03489724 /* lbl_80346000 (~2 deg) */
#define CAM_TURN_RATE_1DEG 0.01744862 /* lbl_80346068 (~1 deg) */
#define SHAKE_FREQ         0.6632251158444444 /* lbl_80346028 = 38 deg at CAM_PI */

/* --- camera shake state (CAMERA.OBJ .sbss globals, names from PDB) --- */
extern s32 shake_type;      /* 0x80344478 */
extern s32 shaking;         /* 0x8034447C */
extern s32 shake_priority;  /* 0x80344480 */
extern s32 shake_count;     /* 0x80344484 */
extern s32 shake_delay;     /* 0x80344488 */
extern f32 shake_rad;       /* 0x8034448C */

/* --- shared globals referenced by this window (names kept from codebase) --- */
extern u32 gFrameTicks;    /* integer frame delta (shared w/ auxscreen.c) */
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
extern Player gPlayers[4];   /* game/player.h; stride 0x335C verified */
extern level_data* gCurLevel;
extern u8* CurTransmitter;
/* gWorldInfo: see game/worldinfo.h (GC-verified WorldInfo, worldmin/worldmax
 * at +0x18/+0x24 confirmed against this TU's own target disassembly). */
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
extern f32 gCameraTargetPositions[9][3];
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
void StandardCamera_8002B828(s32 camIdx);
f32 PointLineColl(f32* point, f32* from, f32* to, f32* closest);
f32 AddAngle(f32 angle, f32 amount);
f32 SubAngle(f32 angle, f32 amount);
f32 get_pitch(f32* from, f32* to);
void get_attn_pos_8002C9A8(s32 camIdx, f32* out);
s32 init_game_cam(s32 camIdx);
s32 MoveCam_walk_8002A024(s32 camIdx);
void cam_orient_to_80029E8C(s32 camIdx);
int PlayerOnMovingObject(void);
void ProcCamera_8002E548(s32 camIdx, s32 mode);
void screen_limitation(void);
void MBCameraUpdate(f32* position, f32* matrix);
void MBRemoveBlit(void* blit);
void AverageCameraTargetPosition_8002A890(f32* out);
void calc_cam_pyr_8002A97C(s32 camIdx, s32 resetDelta);
void get_cam_wpos_8002ABE0(s32 camIdx);
s32 adjust_radius_8002B2D4(s32 camIdx);
void CopyCam(u8* source, u8* destination);
void UpdatePlayerWorldMat(void* player, s32 anchor);
void init_stage_info(void);
void DiffRate_8002951C(s32 camIdx);
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
    CameraStateView* state;
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

    state = (CameraStateView*)gCameraState;
    if (PlayerOnMovingObject() != 0) {
        moving = 0;
    } else {
        moving = 1;
    }
    lbl_80343BD8 = moving;

    cameraMatrix = state->cameras[0].mat[0];
    projectedTarget = state->targets;
    for (projectedIndex = 0; projectedIndex < 15;
         projectedIndex++, projectedTarget++) {
        sign = projectedTarget->active >> 31;
        if ((sign ^ projectedTarget->active) - sign == 1) {
            MBWindowProject(((CameraObjectView*)projectedTarget->object)->attn_pos,
                            cameraMatrix,
                            NULL, projectedTop);
            projectedTarget->projectedTop[0] = (f32)projectedTop[0];
            projectedTarget->projectedTop[1] = (f32)projectedTop[1];
            CLAMP_PROJECTED(projectedTarget->projectedTop[0]);
            CLAMP_PROJECTED(projectedTarget->projectedTop[1]);

            MBWindowProject(((CameraObjectView*)projectedTarget->object)->worldmat[3],
                            cameraMatrix,
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
    camera = state->cameras;
    for (; cameraIndex < 6; cameraIndex++, camera++) {
        if (camera->state == 1) {
            if ((gGameBusy | gGameplayPauseTimer) == 0) {
                lbl_803443F4 = moving;
                camera_init_for_gamemode(cameraIndex);
                camera_run_mode(cameraIndex);
            }
            if (camera->c_mode != CAM_OFF) {
                ProcCamera_8002E548(cameraIndex, lbl_803444DC);
            }
        }
    }

    limitedTarget = state->targets;
    for (limitedIndex = 0; limitedIndex < 15;
         limitedIndex++, limitedTarget++) {
        sign = limitedTarget->active >> 31;
        if ((sign ^ limitedTarget->active) - sign == 1) {
            MBWindowProject(((CameraObjectView*)limitedTarget->object)->attn_pos,
                            cameraMatrix,
                            NULL, limitedTop);
            limitedTarget->limitedTop[0] = (f32)limitedTop[0];
            limitedTarget->limitedTop[1] = (f32)limitedTop[1];
            CLAMP_PROJECTED(limitedTarget->limitedTop[0]);
            CLAMP_PROJECTED(limitedTarget->limitedTop[1]);

            MBWindowProject(((CameraObjectView*)limitedTarget->object)->worldmat[3],
                            cameraMatrix,
                            NULL, limitedBottom);
            limitedTarget->limitedBottom[0] = (f32)limitedBottom[0];
            limitedTarget->limitedBottom[1] = (f32)limitedBottom[1];
            CLAMP_PROJECTED(limitedTarget->limitedBottom[0]);
            CLAMP_PROJECTED(limitedTarget->limitedBottom[1]);
        }
    }

    screen_limitation();
    cameraIndex = lbl_8034453C;
    MBCameraUpdate(state->cameras[cameraIndex].mat[3],
                   state->cameras[cameraIndex].mat[0]);
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
        if (gGameMode == MG_ROUND_START) (cam_)->trans_mode = 3;              \
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
    CameraStateView* state = (CameraStateView*)gCameraState;
    Camera* cam = &state->cameras[camIndex];
    s32 tableMode;
    s32 changed;
    Camera* menuCam;
    s32 bossType;
    s32 bossIndex;

    switch (gGameMode) {
    case MG_PLAYER_SELECT:
    case MG_WORLD_SELECT:
    case MG_SHOP:
    case MG_LEVEL_ADVANCE:
    case MG_ENDING:
    case MG_STATS:
    case MG_GWIZ_SPEECH:
    case MA_INSTRUCT:
    case MA_DEMO:
    case MA_HSTABLE:
    case MA_VIEWMENU:
        return;
    case MG_ROUND_START:
    case MG_PLAY:
    case MG_OVER:
        goto gameplay_mode;
    case MA_FLYBY:
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
        menuCam = &state->cameras[1];
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
    CameraObjectView* playerObject;
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

    if (cam->camobj != 0 &&
        ((CameraObjectView*)cam->camobj)->node == 0) {
        cam->camobj = 0;
    }
    if (cam->attnobj != 0 &&
        ((CameraObjectView*)cam->attnobj)->node == 0) {
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
                cam->radius += rate * (f32)gFrameTicks;
                if ((f64)cam->radius >= 15.0) {
                    cam->radius = 15.0f;
                }
                lbl_803443F4 = 1;
            }
            if ((f64)cam->radius >= 15.0) {
                cam->trans_mode = -1;
            }
        }

        get_attn_pos_8002C9A8(camIdx, destination);
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
        stepDouble = 0.3 * (f64)gFrameTicks;
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
        if (gGameMode == MA_FLYBY) {
            camera_mode_dest(camIdx);
        } else if (gGameMode == MG_PLAY) {
            cam_orient_to_80029E8C(camIdx);
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
        StandardCamera_8002B828(camIdx);
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
        if (gGameMode == MA_FLYBY) {
            camera_mode_dest(camIdx);
        } else if (gGameMode == MA_HSTABLE) {
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
        } else if (gGameMode == MG_WORLD_SELECT || gGameMode == MG_LEVEL_ADVANCE ||
                   gGameMode == MG_GWIZ_SPEECH) {
            camera_mode_spin(camIdx);
        }

        if (lbl_803447B8 != 0) {
            if (lbl_803444F0 >= 0) {
                if (MoveCam_walk_8002A024(camIdx) == 0) {
                    break;
                }
            } else {
                if (init_game_cam(camIdx) == 0) {
                    break;
                }
            }
        } else {
            get_attn_pos_8002C9A8(camIdx, cam->attn);
        }

        if (((gGameMode != MA_FLYBY && gGameMode != MG_WORLD_SELECT &&
              gGameMode != MG_LEVEL_ADVANCE && gGameMode != MG_GWIZ_SPEECH)) ||
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
        StandardCamera_8002B828(camIdx);
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
        players = gPlayers;
        playerCursor = &gCameras[0].pn;
        playerIndex = *playerCursor;
        for (tries = 0; tries < 4; tries++) {
            player = &players[playerIndex];
            if (player->state == 1 || player->state == 4) {
                playerObject = (CameraObjectView*)player->mat;
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
        if (cam->camobj != 0 &&
            ((CameraObjectView*)cam->camobj)->node != 0) {
            cam->wpos[0] = ((CameraObjectView*)cam->camobj)->attn_pos[0];
            cam->wpos[1] = ((CameraObjectView*)cam->camobj)->attn_pos[1];
            cam->wpos[2] = ((CameraObjectView*)cam->camobj)->attn_pos[2];
        }
        if (attentionMode == ATN_OBJECT ||
            (u32)(attentionMode - ATN_PLAYER) <= 4) {
            if (cam->attnobj != 0) {
                cam->attn[0] =
                    ((CameraObjectView*)cam->attnobj)->attn_pos[0];
                cam->attn[1] =
                    ((CameraObjectView*)cam->attnobj)->attn_pos[1];
                cam->attn[2] =
                    ((CameraObjectView*)cam->attnobj)->attn_pos[2];
            }
        } else if (attentionMode == ATN_TARGET) {
            get_attn_pos_8002C9A8(camIdx, cam->attn);
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
        StandardCamera_8002B828(camIdx);
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
        if (gGameMode == MA_FLYBY && lbl_80344288 != 0) {
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
        if (gGameMode == MA_FLYBY) {
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
        StandardCamera_8002B828(camIdx);
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
        StandardCamera_8002B828(camIndex_);                                    \
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

#define backup                   ((Camera*)(state + CAMERA_STATE_CAMERAS_OFF + 5 * CAMERA_STATE_CAMERA_STRIDE))
#define followPositions          ((f32 (*)[3])(state + offsetof(CameraStateData, reticle_pos[0])))

/* Main gameplay camera.  This is the retail transition supervisor: it keeps
 * a short focus history, blends scripted camera changes, controls camera
 * speed from the projected target extent, and rejects views which put a live
 * player outside the safe viewport. */
void camera_mode_follow(s32 camIdx)
{
    u8* state = gCameraState;
    f32* projectionMatrix;
    Camera* cam = (Camera*)(state + CAMERA_STATE_CAMERAS_OFF + camIdx * sizeof(Camera));
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
    f64 speedStep;
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
            calc_cam_pyr_8002A97C(camIdx, 1);
            get_cam_wpos_8002ABE0(camIdx);
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
            ProcCamera_8002E548(camIdx, 0);
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

                calc_cam_pyr_8002A97C(camIdx, 1);
                get_attn_pos_8002C9A8(camIdx, cam->attn);
                zeroValue = lbl_80345EC8;
                cam->delta[0] = zeroValue;
                cam->delta[1] = zeroValue;
                cam->delta[2] = zeroValue;
                if (lbl_803447B8 == 0) {
                    get_cam_wpos_8002ABE0(camIdx);
                }
                if (adjust_radius_8002B2D4(camIdx) == 0) {
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
                        if (gPlayers[scriptedPlayer].state == 1 &&
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
                        if (gPlayers[resetPlayer].state == 1) {
                            gPlayers[resetPlayer].count_91C = 4;
                        }
                    }
                }
            }
            break;

        default:
            calc_cam_pyr_8002A97C(camIdx, 1);
            if (gCameraTargetCount > 0) {
                get_attn_pos_8002C9A8(camIdx, cam->attn);
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
            calc_cam_pyr_8002A97C(camIdx, 1);
            get_cam_wpos_8002ABE0(camIdx);
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
            ProcCamera_8002E548(camIdx, 0);
            lbl_803443FC = 0;
            cam->trans_mode = -1;
            break;
        }
    }

    if (cam->trans_mode >= 0) {
        return;
    }

    offscreen = 0;
    get_attn_pos_8002C9A8(camIdx, focus);
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
    if (adjust_radius_8002B2D4(camIdx) == 0) {
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
    speedStep = lbl_80345F90;
    if ((f64)targetExtent < speedStep) {
        lbl_80344464 = (f32)(lbl_80345F98 * (f64)gFrameTicks);
        lbl_80344468 = lbl_80345FA0;
    } else if ((f64)targetExtent >= lbl_80345FA8) {
        lbl_80344464 = (f32)(lbl_80345FB0 * (f64)gFrameTicks);
        lbl_80344468 = lbl_80345FB8;
    } else {
        f64 extentDelta = lbl_80345FA8 - (f64)targetExtent;
        lbl_80344464 = (f32)(lbl_80345FC0 * extentDelta + lbl_80345FB0);
        lbl_80344468 = (f32)-(lbl_80345FD0 * extentDelta *
            lbl_80345FD8 - lbl_80345FC8);
    }

    if (lbl_80344960 < 0 && (f64)targetExtent >= lbl_80345FE0) {
        lbl_80344464 = targetExtent * (f32)gFrameTicks;
        lbl_80344468 = lbl_80345FE8;
    } else {
        previousSpeed = lbl_80344460;
        desiredSpeed = lbl_80344464;
        if (previousSpeed < desiredSpeed) {
            if ((f64)(desiredSpeed - previousSpeed) > speedStep) {
                lbl_80344464 = (f32)(speedStep + (f64)previousSpeed);
            }
        } else if ((f64)(previousSpeed - desiredSpeed) > speedStep) {
            lbl_80344464 = (f32)((f64)previousSpeed - speedStep);
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

    calc_cam_pyr_8002A97C(camIdx, 0);
    get_cam_wpos_8002ABE0(camIdx);
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
        playerData = gPlayers;
        projectionMatrix = (f32*)(state + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, mat[0][0]));
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
    CameraObjectView* playerObject;
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
        player = gPlayers + playerIndex;
        if (player->state == 1 || player->state == 4) {
            playerObject = (CameraObjectView*)player->mat;
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
    if (((CameraObjectView*)cam->camobj)->node == 0) {
        return;
    }
    if (objectWasMissing != 0) {
        cam->trans_mode = 0;
    }

    targetDirectionZ = ((CameraObjectView*)cam->camobj)->worldmat[2][2];
    targetYaw =
        atan2(((CameraObjectView*)cam->camobj)->worldmat[2][0],
              targetDirectionZ);

    switch (cam->trans_mode) {
    case 0:
        cam->pyr[0] = 0.0f;
        cam->pyr[1] = targetYaw;
        cam->pyr[2] = 0.0f;
        cam->trans_mode++;
        /* fall through */
    case 1:
        cam->attn[0] = ((CameraObjectView*)cam->camobj)->attn_pos[0];
        cam->attn[1] = ((CameraObjectView*)cam->camobj)->attn_pos[1];
        cam->attn[2] = ((CameraObjectView*)cam->camobj)->attn_pos[2];
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

        if ((f64)distance >= 1.0) {
            if ((f64)distance >= 15.0) {
                divisorDouble = (f64)lbl_80345F14;
            }
            scale = (f32)(1.0 / divisorDouble);
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
            StandardCamera_8002B828(camIdx);
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
            angleStep = (f32)(0.05 * (f64)gFrameTicks);
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
            angleStep = (f32)(0.034906585044444445 * (f64)gFrameTicks);
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
        cam->wpos[0] = ((CameraObjectView*)cam->camobj)->attn_pos[0];
        cam->wpos[1] = ((CameraObjectView*)cam->camobj)->attn_pos[1];
        cam->wpos[2] = ((CameraObjectView*)cam->camobj)->attn_pos[2];
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
        StandardCamera_8002B828(camIdx);
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
                        (f64)gFrameTicks + prevSpin);
                    angle = targetAngles[lbl_80344538];
                    if (prevSpin < angle && (f64)lbl_80344534 >= angle) {
                        lbl_80344534 = targetAngles[lbl_80344538];
                        gScriptedCameraState = 0;
                    }
                } else {
                    lbl_80344534 = (f32)-(lbl_80346038 *
                        (f64)gFrameTicks - prevSpin);
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
            middleHistory = (f32*)(state + offsetof(CameraStateData, unk10[1]));
            middleHistory[1] = middleHistory[0];
            middleHistory[0] = *(f32*)(state + offsetof(CameraStateData, unk10[0]));
            *(f32*)(state + offsetof(CameraStateData, unk10[0])) = previous;
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
            rate = angleStep * (f32)gFrameTicks;

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
                *(f32*)(state + offsetof(CameraStateData, unk10[0])) < lbl_80345EC8) {
                goto orbit_crossed_zero;
            }
            zeroValue = *(volatile f32*)&lbl_80345EC8;
            if (cam->pyr[1] < zeroValue) {
                if (middleHistory[0] < zeroValue) {
                    if (*(f32*)(state + offsetof(CameraStateData, unk10[0])) > zeroValue) {
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
    f32 nearestDistance = *(volatile f32*)&lbl_80346030;
    f32 distanceFloor;
    f32 secondYaw = bestPitch;
    f32 nearestYaw = bestPitch;
    f32 bestYaw = bestPitch;
    f64 effectiveThreshold = blendThreshold;
    /* blendThreshold is dead once captured above; retail reuses it (still in
     * the incoming f1) as the second-best trigger distance. */
    f32 swapAngle;
    f32 swapDistance;
    f64 root;
    f32 distance;
    f32 segmentLength;
    f32 blendRatio;
    f64 finalAngleLimit;
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
    distanceFloor = *(volatile f32*)&lbl_80345EC8;
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
                if ((f64)distance > (f64)distanceFloor) {
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
                        lbl_8034450C = index;
                        blendThreshold = candidateDistance;
                        count++;
                        secondYaw = *(f32*)(sTriggerCameras + offset + 0x18);
                        bestYaw = *(f32*)(sTriggerCameras + offset + 0x14);
                    }
                } else if (index == loopSelection) {
                    lbl_8034450C = index;
                    blendThreshold = candidateDistance;
                    count++;
                    secondYaw = *(f32*)(sTriggerCameras + offset + 0x18);
                    bestYaw = *(f32*)(sTriggerCameras + offset + 0x14);
                } else if (candidateDistance < nearestDistance) {
                    lbl_80344510 = index;
                    nearestDistance = candidateDistance;
                    count++;
                    bestPitch = *(f32*)(sTriggerCameras + offset + 0x18);
                    nearestYaw = *(f32*)(sTriggerCameras + offset + 0x14);
                }
            }
    }

    best = lbl_80344510;
    if (lbl_80344508 >= 0 && nearestDistance > blendThreshold) {
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
    finalAngleLimit = lbl_80345F58;
    *(u32*)&scratch.finalAngle &= 0x7FFFFFFF;
    distance = (f32)__fabs(finalAngleLimit - (f64)scratch.finalAngle);
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
    u8* levelCamera = (u8*)gCurLevel->camera;
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
    player = gPlayers;
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
            gWorldInfo.worldmin[0] + levelOffset;
        *(f32*)(levelCamera + 0x10) =
            gWorldInfo.worldmin[1] + lbl_80345EC8;
        *(f32*)(levelCamera + 0x14) =
            gWorldInfo.worldmin[2] + levelOffset;
        *(f32*)(levelCamera + 0x18) =
            gWorldInfo.worldmax[0] + (levelOffset = lbl_80346014);
        *(f32*)(levelCamera + 0x1C) =
            gWorldInfo.worldmax[1] + lbl_80346018;
        *(f32*)(levelCamera + 0x20) =
            gWorldInfo.worldmax[2] + levelOffset;
    }
    *(f32*)(state + offsetof(CameraStateData, attn_min[0])) = *(f32*)(levelCamera + 0x0C);
    *(f32*)(state + offsetof(CameraStateData, attn_min[1])) = *(f32*)(levelCamera + 0x10);
    *(f32*)(state + offsetof(CameraStateData, attn_min[2])) = *(f32*)(levelCamera + 0x14);
    *(f32*)(state + offsetof(CameraStateData, attn_max[0])) = *(f32*)(levelCamera + 0x18);
    *(f32*)(state + offsetof(CameraStateData, attn_max[1])) = *(f32*)(levelCamera + 0x1C);
    *(f32*)(state + offsetof(CameraStateData, attn_max[2])) = *(f32*)(levelCamera + 0x20);
    lbl_80344414 = 2;

    cam0 = (Camera*)(state + CAMERA_STATE_CAMERAS_OFF);
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
        get_attn_pos_8002C9A8(0, cam0->attn);
    }
    lbl_80344534 = lbl_80118B60[lbl_80344538];
    if (gNumTransmitters != 0) {
        *(s16*)(CurTransmitter + offsetof(CameraTransmitterView, unk02)) = 0;
        camera_collide_step(0, lbl_8034601C);
        *(volatile f32*)&lbl_80344408 =
            *(volatile f32*)&lbl_80344530;
        cam0->pyr[0] = *(volatile f32*)&lbl_80344408;
        cam0->pyr[1] = *(volatile f32*)&lbl_80344534;
    }
    calc_cam_pyr_8002A97C(0, 1);
    get_cam_wpos_8002ABE0(0);
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
    StandardCamera_8002B828(0);
    DoShake(&scratch.levelPosition, &scratch.levelAttention);
    scratch.levelDirection.x =
        scratch.levelAttention.x - scratch.levelPosition.x;
    scratch.levelDirection.y =
        scratch.levelAttention.y - scratch.levelPosition.y;
    scratch.levelDirection.z =
        scratch.levelAttention.z - scratch.levelPosition.z;
    LookInDirection(&scratch.levelDirection.x, (u32)&cam0->mat[0][0]);
    cam0->state = 1;

    cam1 = (Camera*)(state + CAMERA_STATE_CAMERAS_OFF + CAMERA_STATE_CAMERA_STRIDE);
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
    playerIndex = *(playerNumber = (s32*)(state + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, pn)));
    for (tries = 0; tries < 4; tries++) {
        player = gPlayers + playerIndex;
        if (player->state == 1 || player->state == 4) {
            *playerNumber = playerIndex;
            playerObject = (struct OBJGRP*)&player->mat[0];
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
    cam2 = (Camera*)(state + CAMERA_STATE_CAMERAS_OFF + 2 * CAMERA_STATE_CAMERA_STRIDE);
    if (gGameMode == MA_DEMO || gGameMode == MA_INSTRUCT || reset != 0 ||
        CurTransmitter == 0) {
        cam2->state = 0;
        lbl_8034453C = 0;
    } else {
        cam2->wpos[0] = *(f32*)(CurTransmitter + offsetof(CameraTransmitterView, wpos[0]));
        cam2->wpos[1] = *(f32*)(CurTransmitter + offsetof(CameraTransmitterView, wpos[1]));
        cam2->wpos[2] = *(f32*)(CurTransmitter + offsetof(CameraTransmitterView, wpos[2]));
        cam2->pyr[0] = *(f32*)(CurTransmitter + offsetof(CameraTransmitterView, pyr[0]));
        cam2->pyr[1] = *(f32*)(CurTransmitter + offsetof(CameraTransmitterView, pyr[1]));
        cam2->pyr[2] = *(f32*)(CurTransmitter + offsetof(CameraTransmitterView, pyr[2]));
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
        distance = dx * dx + dy * dy + dz * dz;
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
        StandardCamera_8002B828(2);
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

    cam3 = (Camera*)(state + CAMERA_STATE_CAMERAS_OFF + 3 * CAMERA_STATE_CAMERA_STRIDE);
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
    cam3->trans_mode = (cameraIndex = 0);
    {
    f32 cam3Zero = lbl_80345EC8;
    cam3->vel[0] = cam3Zero;
    cam3->vel[1] = cam3Zero;
    cam3->vel[2] = cam3Zero;
    cam3->avel[0] = cam3Zero;
    cam3->avel[1] = cam3Zero;
    cam3->avel[2] = cam3Zero;
    cam3->attn[0] = *(f32*)(state + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, attn[0]));
    cam3->attn[1] = *(f32*)(state + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, attn[1]));
    cam3->attn[2] = *(f32*)(state + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, attn[2]));
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
    StandardCamera_8002B828(3);
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
        ProcCamera_8002E548(cameraIndex, 0);
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
    cam = &((Camera*)(state + CAMERA_STATE_CAMERAS_OFF))[camIdx];
    if (camIdx == 0 &&
        (gGameMode == MG_WORLD_SELECT || gGameMode == MG_LEVEL_ADVANCE ||
         gGameMode == MG_GWIZ_SPEECH)) {
        cam->pyr[1] = camera_approach_yaw(cam, cam->num1);
        cam = (Camera*)(state + CAMERA_STATE_CAMERAS_OFF);
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
        tickScale = 0.1 * (f64)gFrameTicks;
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
        StandardCamera_8002B828(camIdx);
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
    step = (f32)(0.034906585044444445 * (f64)gFrameTicks);
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
        step = (f32)(wrapped * (f64)gFrameTicks);
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
    f64 root;
    f64 angle;
    f64 rawAngle;
    s32 pitchReached;
    s32 yawReached;
    f32 matrix[16];
    f32 offset[3];
    f32 transformed[3];
    /* Retail leaves four words between its angle and transform vectors. */
    struct {
        f32 values[6];
        f32 reserved[2];
    } angles;
    f32 orbitNormalize[3];
    volatile f32 directionRoot;

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
        step = (f32)(lbl_80346098 * (f64)gFrameTicks);
        cam->radius -= (f32)((f64)gFrameTicks *
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
            (f32)(lbl_803460A8 * (f64)gFrameTicks);
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
    step = (f32)(lbl_80346098 * (f64)gFrameTicks);
    pitchReached = 0;
    cam->pyr[0] = -cam->pyr[0];
    DiffRate_8002951C(camIdx);
    cam->pyr[0] = -cam->pyr[0];

    if (lbl_80344510 != lbl_8034450C) {
        lbl_80344444 = get_pitch(cam->wpos,
            (f32*)(sTriggerCameras + lbl_8034450C * 0x28 + 4));
        lbl_80344448 = get_yaw(cam->wpos,
            (f32*)(sTriggerCameras + lbl_8034450C * 0x28 + 4));
        angle = (f64)FixAngle((f32)(lbl_80345F60 -
                                     (f64)lbl_80344448));
        angle = lbl_803460B8 * angle;
        angle = (f64)(f32)(lbl_803460B0 * angle);
        if (angle < (f64)lbl_80345EC8) {
            angle = (f64)(f32)(angle + lbl_803460C0);
        }
        if (angle > lbl_803460C8) {
            angle = lbl_80345EC8;
        }
        if (lbl_80344A28 == 0) {
            f64 pitchValue = lbl_803460B8 * (f64)lbl_80344444;
            s32 pitchDegrees = (s32)(lbl_803460B0 * pitchValue);
            dbgTextPrintfCol(2, 3, lbl_80111B3C, pitchDegrees,
                             angle);
        }
    }

    if (lbl_80344510 != lbl_8034450C) {
        f32* triggerX = (f32*)(sTriggerCameras + 4);
        f32* triggerY = (f32*)(sTriggerCameras + 8);
        f32* triggerZ = (f32*)(sTriggerCameras + 0xC);
        transformed[0] =
            triggerX[lbl_80344510 * 10] - cam->wpos[0];
        transformed[1] =
            triggerY[lbl_80344510 * 10] - cam->wpos[1];
        transformed[2] =
            triggerZ[lbl_80344510 * 10] - cam->wpos[2];
        *(volatile f32*)&transformed[0] =
            transformed[0] * lbl_8034445C;
        *(volatile f32*)&transformed[1] =
            transformed[1] * lbl_8034445C;
        *(volatile f32*)&transformed[2] =
            transformed[2] * lbl_8034445C;
        offset[0] =
            triggerX[lbl_8034450C * 10] - cam->wpos[0];
        offset[1] =
            triggerY[lbl_8034450C * 10] - cam->wpos[1];
        offset[2] =
            triggerZ[lbl_8034450C * 10] - cam->wpos[2];
        offset[0] = (f32)((f64)offset[0] *
            (lbl_80345FE0 - (f64)lbl_8034445C));
        offset[1] = (f32)((f64)offset[1] *
            (lbl_80345FE0 - (f64)lbl_8034445C));
        offset[2] = (f32)((f64)offset[2] *
            (lbl_80345FE0 - (f64)lbl_8034445C));
        scale = *(volatile f32*)&transformed[0];
        transformed[0] = scale + offset[0];
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
        if (lbl_8034442C < (targetPitch = lbl_80344444)) {
            if ((f64)lbl_80344424 < lbl_80345F70) {
                lbl_80344424 =
                    (f32)((f64)lbl_80344424 + lbl_803460D8);
            }
            lbl_8034442C += lbl_80344424 * (f32)gFrameTicks;
            if (lbl_8034442C >= targetPitch) {
                pitchReached = 1;
            }
        } else if (lbl_8034442C > targetPitch) {
            if ((f64)lbl_80344424 > lbl_803460E0) {
                lbl_80344424 =
                    (f32)((f64)lbl_80344424 - lbl_803460D8);
            }
            lbl_8034442C += lbl_80344424 * (f32)gFrameTicks;
            if (lbl_8034442C <= targetPitch) {
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
        yawStep = lbl_80344428 * (f32)gFrameTicks;
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

        angles.values[0] = lbl_8034442C;
        angle = CAM_PI + (f64)lbl_80344430;
        if (angle > CAM_PI) {
            angle -= CAM_2PI;
        } else if (angle <= -CAM_PI) {
            angle = CAM_2PI + angle;
        }
        angles.values[1] = (f32)angle;
        angles.values[2] = lbl_80345EC8;
        CreateYPRMatrix(matrix, angles.values);
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
    scale = lbl_80344450 * (f32)gFrameTicks;
    if (lbl_80344530 - lbl_80344408 > zero) {
        lbl_80344408 += scale;
        if (lbl_80344408 >= lbl_80344530) {
            lbl_80344408 = lbl_80344530;
        }
    } else {
        lbl_80344408 -= scale;
        if (lbl_80344408 <= lbl_80344530) {
            lbl_80344408 = lbl_80344530;
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
    {
        f32 finalNormalize[3];
        finalNormalize[0] = cam->attn[0] - cam->wpos[0];
        finalNormalize[1] = cam->attn[1] - cam->wpos[1];
        finalNormalize[2] = cam->attn[2] - cam->wpos[2];
        SlowNormalVector(finalNormalize);
        cam->attn[0] = cam->wpos[0] + finalNormalize[0] * distance;
        cam->attn[1] = cam->wpos[1] + finalNormalize[1] * distance;
        cam->attn[2] = cam->wpos[2] + finalNormalize[2] * distance;
    }
}
#pragma opt_common_subs on

#pragma opt_common_subs off
/* Simulate the debug camera and report whether an active player leaves it. */
s32 debug_camera_pos(s32 lastPlayer)
{
    char* debugText = lbl_80111A08;
    u8* state = gCameraState;
    Camera* cam = (Camera*)(state + CAMERA_STATE_CAMERAS_OFF + 5 * CAMERA_STATE_CAMERA_STRIDE);
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
    sourceCamera = (Camera*)(state + CAMERA_STATE_CAMERAS_OFF);
    CopyCam((u8*)sourceCamera, (u8*)cam);
    get_attn_pos_8002C9A8(cameraIndex, scratch.desiredAttention);
    lbl_803443F4 = 0;
    adjust_radius_8002B2D4(cameraIndex);

    cam->delta[0] = scratch.desiredAttention[0] - cam->attn[0];
    cam->delta[1] = scratch.desiredAttention[1] - cam->attn[1];
    cam->delta[2] = scratch.desiredAttention[2] - cam->attn[2];
    scale = cam->delta[0] * cam->delta[0];
    distance = cam->delta[1] * cam->delta[1];
    extent = cam->delta[2] * cam->delta[2];
    distance = extent + (scale + distance);
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
            (f32)(lbl_80345F98 * (f64)gFrameTicks);
        lbl_80344468 = lbl_80345FA0;
    } else if ((f64)extent >= lbl_80345FA8) {
        lbl_80344464 =
            (f32)(lbl_80345FB0 * (f64)gFrameTicks);
        lbl_80344468 = lbl_80345FB8;
    } else {
        difference = lbl_80345FA8 - (f64)extent;
        lbl_80344464 =
            (f32)(lbl_80345FC0 * difference + lbl_80345FB0);
        lbl_80344468 =
            (f32)-(difference * lbl_80345FD8 * lbl_80345FD0 -
                   lbl_80345FC8);
    }
    if (lbl_80344960 < 0 && (f64)extent >= lbl_80345FE0) {
        lbl_80344464 = extent * (f32)gFrameTicks;
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
    calc_cam_pyr_8002A97C(cameraIndex, 0);
    lbl_80344408 = savedPitch;
    get_cam_wpos_8002ABE0(cameraIndex);
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
    StandardCamera_8002B828(cameraIndex);
    DoShake((Vec3*)scratch.position, (Vec3*)scratch.attention);
    scratch.direction[0] = scratch.attention[0] - scratch.position[0];
    scratch.direction[1] = scratch.attention[1] - scratch.position[1];
    scratch.direction[2] = scratch.attention[2] - scratch.position[2];
    LookInDirection(scratch.direction, (u32)&cam->mat[0][0]);
    ProcCamera_8002E548(cameraIndex, 0);

    if (lbl_80344A28 == 0) {
        dbgTextPrintfCol(40, 9, debugText + 196,
                         (s32)cam->wpos[0], (s32)cam->wpos[1],
                         (s32)cam->wpos[2]);
        dbgTextPrintfCol(40, 10, debugText + 220,
                         (s32)cam->attn[0], (s32)cam->attn[1],
                         (s32)cam->attn[2]);
    }

    playerData = (u8*)gPlayers;
    for (player = 0; player <= lastPlayer;
         player++, playerData += 0x335C) {
        Player* pd = (Player*)playerData;
        if (pd->state == 1) {
            MBWindowProject(pd->col_pos,
                            &((Camera*)(state + CAMERA_STATE_CAMERAS_OFF))[cameraIndex].mat[0][0],
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

    previousAttention = *(s32*)(state + CAMERA_STATE_CAMERAS_OFF + 5 * CAMERA_STATE_CAMERA_STRIDE + offsetof(Camera, a_mode));
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
    u8* state = gCameraState;
    u8* playerData = (u8*)gPlayers + playerIndex * 0x335C;
    Player* pd = (Player*)playerData;
    f32* cameraMatrix;
    f32* cameraPositionX;
    f32* cameraPositionZ;
    CameraTarget* target;
    CameraSupervisorScratch scratch;
    u8 unused[16];
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
    f32 latchValue;
    f32 zeroValue;
    f64 root;
    s32 screenHeight;
    s32 screenWidth;
    s32 targetIndex;

    screenHeight = MBScreenHeight();
    screenWidth = MBScreenWidth();
    if (pd->vibe_on == 29) {
        return 0;
    }

    cameraMatrix = (f32*)(state + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, mat[0][0]));
    oldX = pd->floor_hi;
    oldY = pd->floor_lo;
    scratch.futurePosition[0] =
        pd->col_pos[0] + movementDelta[0];
    scratch.futurePosition[1] =
        pd->col_pos[1] + movementDelta[1];
    scratch.futurePosition[2] =
        pd->col_pos[2] + movementDelta[2];
    MBWindowProject(scratch.futurePosition, cameraMatrix, 0,
                    scratch.projected);
    currentX = (f32)scratch.projected[0];
    currentY = (f32)scratch.projected[1];

    target = (CameraTarget*)(state + CAMERA_STATE_TARGETS_OFF);
    for (targetIndex = 0; targetIndex < 15; targetIndex++, target++) {
        if (target->object == (CameraObjectView*)(playerData + 0x14)) {
            break;
        }
    }
    if (targetIndex < 15 &&
        (f64)(target->limitedTop[1] - target->limitedBottom[1]) >=
            (f64)lbl_80345F90 *
                (f64)((lbl_80344518 - 20) - (lbl_80344514 + 40))) {
        scratch.alternatePosition[0] =
            pd->pos[0] + movementDelta[0];
        scratch.alternatePosition[1] =
            pd->pos[1] + movementDelta[1];
        scratch.alternatePosition[2] =
            pd->pos[2] + movementDelta[2];
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
            ((f64)*(f32*)(state + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, radius)) >= lbl_80345FF0 ||
             lbl_80344960 < 0) &&
            lbl_80343BD8 != 0) {
            CAMERA_LATCH_CHANGE();
        }
        return 1;
    } else if (oldX > (f32)(lbl_80344520 + 30) &&
               oldX < (f32)(lbl_8034451C - 30) &&
               currentX > (f32)(lbl_80344520 + 30) &&
               currentX < (f32)(lbl_8034451C - 30) &&
               (CAMERA_SUPERVISOR_ABS(return2CurrentY,
                    currentY - *(f32*)(lbl_80344EE8 + offsetof(CameraProjWindowView, ycenter))),
                currentAbsX = scratch.return2CurrentY,
                CAMERA_SUPERVISOR_ABS(return2OldY,
                    oldY - *(f32*)(lbl_80344EE8 + offsetof(CameraProjWindowView, ycenter))) > currentAbsX)) {
        if (gCameraTargetCount > 1 &&
            (oldY <= (f32)(lbl_80344514 + 40) ||
             oldY >= (f32)(lbl_80344518 - 20)) &&
            ((f64)*(f32*)(state + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, radius)) >= lbl_80345FF0 ||
             lbl_80344960 < 0) &&
            lbl_80343BD8 != 0) {
            CAMERA_LATCH_CHANGE();
        }
        return 2;
    } else if (oldY > (f32)(lbl_80344514 + 40) &&
               oldY < (f32)(lbl_80344518 - 20) &&
               currentY > (f32)(lbl_80344514 + 40) &&
               currentY < (f32)(lbl_80344518 - 20) &&
               (CAMERA_SUPERVISOR_ABS(return3CurrentX,
                    currentX - *(f32*)(lbl_80344EE8 + offsetof(CameraProjWindowView, xcenter))),
                currentAbsX = scratch.return3CurrentX,
                CAMERA_SUPERVISOR_ABS(return3OldX,
                    oldX - *(f32*)(lbl_80344EE8 + offsetof(CameraProjWindowView, xcenter))) > currentAbsX)) {
        if (gCameraTargetCount > 1 &&
            (oldX <= (f32)(lbl_80344520 + 30) ||
             oldX >= (f32)(lbl_8034451C - 30)) &&
            ((f64)*(f32*)(state + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, radius)) >= lbl_80345FF0 ||
             lbl_80344960 < 0) &&
            lbl_80343BD8 != 0) {
            CAMERA_LATCH_CHANGE();
        }
        return 3;
    } else if ((CAMERA_SUPERVISOR_ABS(return4CurrentY,
                    currentY - *(f32*)(lbl_80344EE8 + offsetof(CameraProjWindowView, ycenter))),
                currentAbsY = scratch.return4CurrentY,
                CAMERA_SUPERVISOR_ABS(return4OldY,
                    oldY - *(f32*)(lbl_80344EE8 + offsetof(CameraProjWindowView, ycenter))) > currentAbsY) &&
               (CAMERA_SUPERVISOR_ABS(return4CurrentX,
                    currentX - *(f32*)(lbl_80344EE8 + offsetof(CameraProjWindowView, xcenter))),
                currentAbsX = scratch.return4CurrentX,
                CAMERA_SUPERVISOR_ABS(return4OldX,
                    oldX - *(f32*)(lbl_80344EE8 + offsetof(CameraProjWindowView, xcenter))) > currentAbsX)) {
        if (gCameraTargetCount > 1 &&
            (oldX <= (f32)(lbl_80344520 + 30) ||
             oldX >= (f32)(lbl_8034451C - 30) ||
             oldY <= (f32)(lbl_80344514 + 40) ||
             oldY >= (f32)(lbl_80344518 - 20)) &&
            ((f64)*(f32*)(state + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, radius)) >= lbl_80345FF0 ||
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
        s32 bottomEdge;
        s32 halfWidth;
        dy = (pd->pos[1] +
              pd->anchor_pos[1]) - *(f32*)(state + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, attn[1]));
        bottomEdge = screenHeight - 64;
        dx = (pd->pos[0] +
              pd->anchor_pos[0]) - *(f32*)(state + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, attn[0]));
        halfWidth = screenWidth / 2;
        dz = (pd->pos[2] +
              pd->anchor_pos[2]) - *(f32*)(state + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, attn[2]));
        currentDistance = dy * dy + dx * dx + dz * dz;
        movedX = dx + movementDelta[0];
        movedY = dy + movementDelta[1];
        movedZ = dz + movementDelta[2];
        cameraPositionX = (f32*)(state + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, attn[0]));
        cameraPositionZ = (f32*)(state + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, attn[2]));

        currentAbsY = oldY - (f32)bottomEdge;
        currentAbsX = oldX - (f32)halfWidth;
        oldAbsY = currentY - (f32)bottomEdge;
        oldAbsX = currentX - (f32)halfWidth;
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
        movedDistance = movedY * movedY + movedX * movedX + movedZ * movedZ;
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
            cameraDz = *cameraPositionZ - pd->col_pos[2];
            cameraDx = *cameraPositionX - pd->col_pos[0];
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

        {
            s32 cameraTarget = lbl_80344960;

            if (((cameraTarget < 0 &&
                  *(f32*)(state + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, radius)) >= lbl_80344528) ||
                 (cameraTarget >= 0 &&
                  (f64)*(f32*)(state + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, radius)) >= lbl_80345FF0)) &&
                (f64)(latchValue = lbl_803444E8) >= lbl_80346008) {
                if (gBossType >= 0) {
                    if (gCameraTargetCount > 1) {
                        if (lbl_803444E4 == 0) {
                            if (lbl_80343BD8 != 0) {
                                lbl_80344500 = 1;
                                lbl_803444FC = 1;
                                if (lbl_803444F8 < 60) {
                                    lbl_803444F8 = 60;
                                }
                                if ((f64)latchValue < lbl_80345FA8) {
                                    lbl_803444F8 = 0;
                                }
                            }
                        } else {
                            lbl_80344500 = 0;
                            lbl_803444FC = 0;
                            lbl_803444F8 = 0;
                        }
                    }
                } else if (gCameraTargetCount > 1 && lbl_80343BD8 != 0) {
                    lbl_80344500 = 1;
                    lbl_803444FC = 1;
                    if (lbl_803444F8 < 60) {
                        lbl_803444F8 = 60;
                    }
                    if ((f64)latchValue < lbl_80345FA8) {
                        lbl_803444F8 = 0;
                    }
                }
            }
        }
        return 5;
    }

    scratch.futurePosition[0] = pd->pos[0];
    scratch.futurePosition[1] = pd->pos[1];
    scratch.futurePosition[2] = pd->pos[2];
    pd->pos[0] += movementDelta[0];
    pd->pos[1] += movementDelta[1];
    pd->pos[2] += movementDelta[2];
    fn_8005A588((struct OBJGRP*)(playerData + 0x14), pd->anchor_pos);
    if (debug_camera_pos(playerIndex) != 0 && lbl_80343BD8 != 0) {
        CAMERA_LATCH_CHANGE();
    }
    pd->pos[0] = scratch.futurePosition[0];
    pd->pos[1] = scratch.futurePosition[1];
    pd->pos[2] = scratch.futurePosition[2];
    fn_8005A588((struct OBJGRP*)(playerData + 0x14), pd->anchor_pos);
    return 6;
}

#undef CAMERA_LATCH_CHANGE
#undef CAMERA_SUPERVISOR_ABS
#pragma opt_common_subs on

/* ------------------------------------------------------------------ */
/* CAMERA.OBJ tail, 0x8002951C..0x8002EFE8 (previously carried by combat.c). */
/* ------------------------------------------------------------------ */

extern plyr_data* lbl_80282930[];
extern CameraTarget gCameraTargets[15];
extern u8 gCameraState[];
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
void MBWindowSetRegion(f32 left, f32 right, f32 top, f32 bottom, f32 depth);
u32 pbGetTime(void);
void MBWindowZoom(f32 zoom);
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

void DiffRate_8002951C(s32 camIdx)
{
    f32* camState = (f32*)gCameraState;
    register f32* state5 = camState + 5;
    Camera* cam = (Camera*)((u8*)gCameraState + camIdx * 396 + 0xC8);
    f32 prevYaw = cam->pyr[1];
    f32 rate;
    f32 curYaw;
    f64 y;
    f32 yawDelta;
    u8 unused[12];

    camState[6] = camState[5];
    camState[5] = camState[4];
    camState[4] = prevYaw;
    CameraSupervisor(camIdx);
    rate = lbl_8034444C * (f32)gFrameTicks;
    yawDelta = lbl_80344534 - cam->pyr[1];
    *(u32*)&yawDelta = *(u32*)&yawDelta & 0x7FFFFFFF;

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
    if ((cam->pyr[1] > 0.0f && *state5 > 0.0f &&
         camState[4] < 0.0f) ||
        (cam->pyr[1] < 0.0f && *state5 < 0.0f &&
         camState[4] > 0.0f)) {
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

/* Trigger-camera (rail) marker record, stride 0x28.  Field boundaries are
 * confirmed by this TU's own accesses (active flag @0, an "armed this frame"
 * s16 test @2, world pos @4/8/0xC, pitch @0x14, yaw @0x18).  Same array as
 * game/world/newcam.c's NcMarker (node handle @0x24, confirmed there); the
 * +2 field is unnamed padding in NcMarker because that TU's own selectors
 * never test it, so it is named only here where CameraSupervisor reads it.
 * A view-only type: sTriggerCameras itself stays u8* and every access below
 * keeps its original raw-pointer-walk shape (offsetof spelling only) per
 * claim.law.pointer-walk-array-index-regression -- this loop accumulates
 * `offset` across iterations, so array-style [i] indexing is not attempted. */
typedef struct CombatTriggerCamera {
    s8  active;      /* 0x00 */
    u8  _pad01;      /* 0x01 */
    s16 armed;       /* 0x02 */
    f32 x;           /* 0x04 */
    f32 y;           /* 0x08 */
    f32 z;           /* 0x0C */
    u8  _pad10[4];   /* 0x10 */
    f32 pitch;       /* 0x14 */
    f32 yaw;         /* 0x18 */
    u8  _pad1C[0xC]; /* 0x1C */
} CombatTriggerCamera; /* 0x28 */

#define TC_X(i) (*(f32*)(sTriggerCameras + (i) * 0x28 + offsetof(CombatTriggerCamera, x)))
#define TC_Y(i) (*(f32*)(sTriggerCameras + (i) * 0x28 + offsetof(CombatTriggerCamera, y)))
#define TC_Z(i) (*(f32*)(sTriggerCameras + (i) * 0x28 + offsetof(CombatTriggerCamera, z)))

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
    u8 _pad40[4];
    f32 closest[3];
    u8 _pad50[8];
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
    f32 zero = lbl_80345EC8;
    f32 nearestYaw = zero;
    f32 secondYaw = zero;
    f32 nearestPitch = zero;
    f32 secondPitch = zero;
    f32 distance;
    f32 combinedDistance;
    f32 segmentLength;
    f32 projectedDistance;
    f32 projectedRatio;
    f32 selectedDistance;
    f32 rateDelta;
    CombatCameraSupervisorScratch scratch;
    f64 root;
    f64 maxYawRate;
    f64 maxYawStep;
    f64 maxPitchRate;
    f64 maxPitchStep;

    for (; index < remaining; index++, offset += 0x28) {
        if (sTriggerCameras[offset] == 1 &&
            *(s16*)(sTriggerCameras + offset + offsetof(CombatTriggerCamera, armed)) != 0) {
                f32 dy = cam->wpos[1] -
                    *(f32*)(sTriggerCameras + offset + offsetof(CombatTriggerCamera, y));
                f32 dx = cam->wpos[0] -
                    *(f32*)(sTriggerCameras + offset + offsetof(CombatTriggerCamera, x));
                f32 dz = cam->wpos[2] -
                    *(f32*)(sTriggerCameras + offset + offsetof(CombatTriggerCamera, z));

                distance = dy * dy;
                distance = dx * dx + distance;
                distance = dz * dz + distance;
                if (distance > zero) {
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
                    nearestYaw = *(f32*)(sTriggerCameras + offset + offsetof(CombatTriggerCamera, yaw));
                    nearestPitch = *(f32*)(sTriggerCameras + offset + offsetof(CombatTriggerCamera, pitch));
                } else if (distance < secondDistance) {
                    lbl_8034450C = index;
                    secondDistance = distance;
                    count++;
                    secondYaw = *(f32*)(sTriggerCameras + offset + offsetof(CombatTriggerCamera, yaw));
                    secondPitch = *(f32*)(sTriggerCameras + offset + offsetof(CombatTriggerCamera, pitch));
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

        PointLineColl(&cam->wpos[0], (f32*)(nearestTrigger + offsetof(CombatTriggerCamera, x)),
            (f32*)(secondTrigger + offsetof(CombatTriggerCamera, x)), scratch.closest);

        sy = *(f32*)(nearestTrigger + offsetof(CombatTriggerCamera, y)) -
             *(f32*)(secondTrigger + offsetof(CombatTriggerCamera, y));
        sx = *(f32*)(nearestTrigger + offsetof(CombatTriggerCamera, x)) -
             *(f32*)(secondTrigger + offsetof(CombatTriggerCamera, x));
        sz = *(f32*)(nearestTrigger + offsetof(CombatTriggerCamera, z)) -
             *(f32*)(secondTrigger + offsetof(CombatTriggerCamera, z));
        segmentLength = sy * sy;
        segmentLength = sx * sx + segmentLength;
        segmentLength = sz * sz + segmentLength;
        if (segmentLength > lbl_80345EC8) {
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

        sy = *(f32*)(nearestTrigger + offsetof(CombatTriggerCamera, y)) - scratch.closest[1];
        sx = *(f32*)(nearestTrigger + offsetof(CombatTriggerCamera, x)) - scratch.closest[0];
        sz = *(f32*)(nearestTrigger + offsetof(CombatTriggerCamera, z)) - scratch.closest[2];
        projectedDistance = sy * sy;
        projectedDistance = sx * sx + projectedDistance;
        projectedDistance = sz * sz + projectedDistance;
        if (projectedDistance > lbl_80345EC8) {
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
            if (selectedDistance > lbl_80345EC8) {
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
                maxYawRate = lbl_803460F8;
                scratch.yawRate = distance / selectedDistance;
                *(u32*)&scratch.yawRate &= 0x7FFFFFFF;
                lbl_8034444C = scratch.yawRate;
                if ((f64)scratch.yawRate >= maxYawRate) {
                    lbl_8034444C = lbl_80346100;
                }

                {
                    f32 yawRate = lbl_8034444C;
                    f32 yawRatePrev = lbl_80344454;

                    maxYawStep = lbl_803460D8;
                    rateDelta = yawRate - yawRatePrev;
                    scratch.yawRateDelta = rateDelta;
                    *(u32*)&scratch.yawRateDelta &= 0x7FFFFFFF;
                    if ((f64)scratch.yawRateDelta >= maxYawStep) {
                        if (yawRate > yawRatePrev) {
                            lbl_8034444C = (f32)(yawRatePrev + maxYawStep);
                        } else {
                            lbl_8034444C = (f32)(yawRatePrev - maxYawStep);
                        }
                    }
                }

                maxPitchRate = lbl_80346108;
                scratch.pitchRate =
                    (lbl_80344530 - lbl_80344408) / selectedDistance;
                *(u32*)&scratch.pitchRate &= 0x7FFFFFFF;
                lbl_80344450 = scratch.pitchRate;
                if ((f64)scratch.pitchRate >= maxPitchRate) {
                    lbl_80344450 = lbl_80346110;
                }

                {
                    f32 pitchRate = lbl_80344450;
                    f32 pitchRatePrev = lbl_80344458;

                    maxPitchStep = lbl_80346118;
                    rateDelta = pitchRate - pitchRatePrev;
                    scratch.pitchRateDelta = rateDelta;
                    *(u32*)&scratch.pitchRateDelta &= 0x7FFFFFFF;
                    if ((f64)scratch.pitchRateDelta >= maxPitchStep) {
                        if (pitchRate > pitchRatePrev) {
                            lbl_80344450 = (f32)(pitchRatePrev + maxPitchStep);
                        } else {
                            lbl_80344450 = (f32)(pitchRatePrev - maxPitchStep);
                        }
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
        *(s16*)(sTriggerCameras + oldNearest * 0x28 +
                offsetof(CombatTriggerCamera, armed)) = 0;
    }
}

/* Orient a camera around its attention point at the requested radius. */
/* 0x80029E8C - orient the transmitter camera (cam 3): spin its yaw, clamp the
 * radius, snap the look-at to camera 0's, then rebuild its world position. */
void cam_orient_to_80029E8C(s32 camIdx)
{
    u8* gcs = (u8*)gCameraState;
    Camera* cam = (Camera*)(gcs + camIdx * 396);
    f32 mat[16];
    f32 vec[3];
    f32 out[3];

    cam = (Camera*)((u8*)cam + 0xC8);

    if (lbl_803447B8 != 0 || lbl_803447B4 != 0 || gNumTransmitters == 0) {
        return;
    }
    if (camIdx != 3) {
        return;
    }

    cam->pyr[1] = (f32)(cam->pyr[1] + lbl_80346128);
    {
        f64 yaw = cam->pyr[1];
        if (yaw > lbl_80345F58) {
            yaw = yaw - lbl_80345F60;
        } else if (yaw <= lbl_80345F68) {
            yaw = lbl_80345F60 + yaw;
        }
        cam->pyr[1] = yaw;
    }

    cam->radius = (f32)(cam->radius / lbl_80346130);
    if (cam->radius < lbl_803460D0) {
        cam->radius = lbl_80346018;
    } else if (cam->radius > lbl_80345FF0) {
        cam->radius = lbl_80346020;
    }

    {
        f32 zero = lbl_80345EC8;
        cam->vel[0] = zero;
        cam->vel[1] = zero;
        cam->vel[2] = zero;
        cam->avel[0] = zero;
        cam->avel[1] = zero;
        cam->avel[2] = zero;
    }
    cam->attn[0] = *(f32*)(gcs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, attn[0]));
    cam->attn[1] = *(f32*)(gcs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, attn[1]));
    cam->attn[2] = *(f32*)(gcs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, attn[2]));

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
        u8* p = (u8*)gPlayers;
        s32 i;
        done = 1;
        for (i = 0; i < 4; i++, p += 13148) {
            if (PF(p, offsetof(Player, state), s32) == 1 && PF(p, offsetof(Player, vibe_on), s32) != 1) {
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
void write_stage_info(s32 mode);

/*
 * init_game_cam -- game-camera (index 2) zoom/transition driver.  Steps the
 * game camera's world position and attention toward camera 0's, each capped
 * per frame; when both converge it fires the level transition.  Returns 0 on
 * transition, -1 otherwise.
 */
s32 init_game_cam(s32 camIdx)
{
    u8* gcs = (u8*)gCameraState;
    Camera* cam = (Camera*)(gcs + camIdx * 396 + 0xC8);
    s32 prevTimer;
    u8* level = (u8*)gCurLevel->camera;
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
                if (PF(player, offsetof(Player, state), s32) == 1 &&
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
            if (PF(player, offsetof(Player, state), s32) == 1) {
                PF(player, offsetof(Player, count_91C), s32) = 4;
            }
        }
    }

    if (gScriptedCameraState == 1) {
        dy = *(f32*)(gcs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, wpos[1])) - cam[0].wpos[1];
        dx = *(f32*)(gcs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, wpos[0])) - cam[0].wpos[0];
        dz = *(f32*)(gcs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, wpos[2])) - cam[0].wpos[2];
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

        dy = *(f32*)(gcs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, attn[1])) - cam[0].attn[1];
        dx = *(f32*)(gcs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, attn[0])) - cam[0].attn[0];
        dz = *(f32*)(gcs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, attn[2])) - cam[0].attn[2];
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
                MBRemoveBlit(lbl_8034440C);
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
                if ((gControllerButtons.both & 4) != 0) {
                    lbl_803445D4 = lbl_803445D4 | 4;
                    *(volatile s32*)&sPreviousFlags.word.buttons = sPreviousFlags.word.buttons;
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

    lbl_80344410 = (f32)(lbl_80346150 * (f64)gFrameTicks + prev);
    if (mode <= 1 || lbl_80344410 > lbl_80345F40) {
        lbl_80344410 = lbl_80346158;
    }
    if (mode <= 0) {
        if (lbl_8034440C != NULL) {
            MBRemoveBlit(lbl_8034440C);
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
                          0xFFFFFF, gCurLevel->title);
    }
    level = gCurLevel->flags;
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
    level = gCurLevel->flags;
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
        lbl_8034440C = MBNewBlit("SCROLL_A", x, y);
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
            q = gCameraTargetPositions[i];
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
        f32 rate = (f32)(lbl_80346170 * (f64)gFrameTicks);
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
        if (t->active > 0 && CameraCollide(cam->wpos, t->object->attn_pos)) {
            blocked = 1;
            break;
        }
    }
    return blocked;
}

void get_cam_wpos_8002ABE0(s32 camIdx)
{
    s32* camState = (s32*)gCameraState;
    Camera* cam = (Camera*)(gCameraState + camIdx * sizeof(Camera) + 0xC8);
    f32 mat[18];
    f32 in[3];
    f32 out[9];
    s32 i;

    cam->old_wpos[0] = cam->wpos[0];
    cam->old_wpos[1] = cam->wpos[1];
    cam->old_wpos[2] = cam->wpos[2];

    if (gNumTransmitters == 0 && lbl_803443F8 <= 0) {
        s32 mode = lbl_80344538;
        for (i = 0; i < 4; i++) {
            camState[i] = 0;
        }
        {
        f32 lat = lbl_80345EC8;
        f64 yawMax = lbl_80345F58;
        f64 yawMin = lbl_80345F68;
        f64 yawRange = lbl_80345F60;
        f64 yawStep = lbl_80346180;
        for (i = 0; i < 4; i++) {
            f64 y;
            CreateYPRMatrix(mat, cam->pyr);
            in[0] = lat;
            in[1] = lat;
            in[2] = cam->radius;
            WorldVector(in, &out[6], mat);
            cam->wpos[0] = cam->attn[0] + out[6];
            cam->wpos[1] = cam->attn[1] + out[7];
            cam->wpos[2] = cam->attn[2] + out[8];
            camState[mode] = cam_blocked(cam);
            cam->pyr[1] = (f32)((f64)cam->pyr[1] + yawStep);
            y = cam->pyr[1];
            if (y > yawMax) {
                y = y - yawRange;
            } else if (y <= yawMin) {
                y = yawRange + y;
            }
            mode = mode & 3;
            cam->pyr[1] = (f32)y;
        }
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
                    if ((f64)cam->pyr_delta[0] > 0.0) {
                        cam->pyr_delta[0] = cam->pyr_delta[0] - 0.019999999552965164f;
                        if ((f64)cam->pyr_delta[0] < 0.0) {
                            cam->pyr_delta[0] = lbl_80345EC8;
                        }
                        cam->pyr[0] = cam->pyr[0] - 0.019999999552965164f;
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
                    if ((f64)cam->pyr_delta[0] < 0.0) {
                        cam->pyr_delta[0] = cam->pyr_delta[0] + 0.019999999552965164f;
                        if ((f64)cam->pyr_delta[0] > 0.0) {
                            cam->pyr_delta[0] = lbl_80345EC8;
                        }
                        cam->pyr[0] = cam->pyr[0] + 0.019999999552965164f;
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
                if ((f64)cam->pyr[0] <= lbl_80346178 - 0.019999999552965164f) {
                    cam->pyr_delta[0] = cam->pyr_delta[0] + 0.019999999552965164f;
                    cam->pyr[0] = cam->pyr[0] + 0.019999999552965164f;
                    place_cam(cam, mat, in, out);
                }
            } else {
                if (cam->pyr[0] >= 0.019999999552965164f) {
                    cam->pyr_delta[0] = cam->pyr_delta[0] - 0.019999999552965164f;
                    cam->pyr[0] = cam->pyr[0] - 0.019999999552965164f;
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
    void* levelData = gCurLevel->camera;
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
    f32* camMat;
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
    camMat = ((Camera*)(cameraState + camIdx * sizeof(Camera) + 0xC8))->mat[0];
    eyeX = (f32*)(cameraState + camIdx * sizeof(Camera) + 0xC8 + 0x34);
    eyeY = (f32*)(cameraState + camIdx * sizeof(Camera) + 0xC8 + 0x38);
    eyeZ = (f32*)(cameraState + camIdx * sizeof(Camera) + 0xC8 + 0x3C);
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
            MBWindowProject(target->object->attn_pos, camMat, 0, sp);
            sx = (f32)sp[0];
            sy = (f32)sp[1];
            if (sx < minX) minX = sx;
            if (sx > maxX) maxX = sx;
            if (sy < minY) minY = sy;
            if (sy > maxY) maxY = sy;
            MBWindowProject(target->object->worldmat[3], camMat, 0, sp);
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
    u8 padTop[32];
    f32 absErrXStart;
    f32 absErrXMoving;
    f32 absErrXEase;
    f32 absErrYStart;
    f32 absErrYMoving;
    f32 absErrYEase;
    f32 dir[3];
    u8* cameraState = gCameraState;
    Camera* cam = (Camera*)(cameraState + camIdx * sizeof(Camera) + 0xC8);
    s32 wasPanning = lbl_803444DC;
    f32 minX = lbl_803461D4, maxX = lbl_803461D8;
    f32 maxY = lbl_803461D8, minY = lbl_803461D4;
    f32 errX = lbl_80345EC8;
    f32 errY = lbl_80345EC8;
    s32 scrH = MBScreenHeight();
    s32 scrW = MBScreenWidth();
    f32 offX;
    f32 offZ;
    f32 yaw;
    f32 panXStart;
    u8* levelData;
    s32 i;

    if (gCurLevel == 0) {
        return;
    }
    levelData = (u8*)gCurLevel->camera;
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
    offX = lbl_80345EC8;
    offZ = lbl_80345EC8;
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
        f32 absPanX;
        f32 absPanY;

        absErrXStart = errX;
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
        u8 padBottom[104];
        CameraTarget* t = (CameraTarget*)(cameraState + 0xA10);
        for (i = 0; i < 15; i++, t++) {
            if (t->active > 0) {
                f32 ty = t->object->attn_pos[1];
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

void del_target(void* obj)
{
    s32 found = 0;
    s32 i;
    CameraTarget* p;

    if (gCameraTargetCount != 0) {
        p = gCameraTargets;
        for (i = 0; i < 15; i++, p++) {
            if (p->active != 0 && obj == p->object) {
                p->active = 0;
                found = 1;
                p->object = NULL;
                p->position[0] = 0.0f;
                p->position[1] = 0.0f;
                p->position[2] = 0.0f;
                gCameraTargetCount--;
                break;
            }
        }
        if (found) {
            recalc_lookat(0, 0);
        }
    }
}

void add_target(void* obj)
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
            p->object = NULL;
            p->position[0] = 0.0f;
            p->position[1] = 0.0f;
            p->position[2] = 0.0f;
        }
        gCameraTargetCount = 0;
    }

    p = (CameraTarget*)(state + 2576);
    for (i = 0; i < 15; i++, p++) {
        if (obj == p->object) {
            return;
        }
    }

    p = (CameraTarget*)(state + 2576);
    for (i = 0; i < 15; i++, p++) {
        if (p->active == 0) {
            p->active = 1;
            found = 1;
            p->object = obj;
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
        p->object = NULL;
        p->position[0] = v;
        p->position[1] = v;
        p->position[2] = v;
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
/* This TU's former file-local `CombatItem` was a partial view of the shipped
 * Item (game/item.h): info@0x00, attn_pos@0x44 == objgrp.attn_pos,
 * activetime@0xC6 and data@0xDC all line up, so the real type is used. */
extern f64 lbl_803461F0;

void get_attn_pos_8002C9A8(s32 camIdx, f32* out)
{
    u8 unused[92];
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
            f32 zero = 0.0f;
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
               ((gGameMode & MODE_GROUP_GAME) != 0 && (u32)lbl_80344824 == 0)) {
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
            Item* item;
            out[0] = *(f32*)(*(u8**)(*(u8**)((item =
                sItems + lbl_80344960)->data.raw) +
                0x28) + 0x30);
            out[1] = *(f32*)(*(u8**)(*(u8**)((item =
                sItems + lbl_80344960)->data.raw) +
                0x28) + 0x34);
            out[2] = *(f32*)(*(u8**)(*(u8**)((item =
                sItems + lbl_80344960)->data.raw) +
                0x28) + 0x38);
            cam->attn_dest[0] = out[0];
            cam->attn_dest[1] = out[1];
            cam->attn_dest[2] = out[2];
            cam->attn_dest_no_offset[0] = out[0];
            cam->attn_dest_no_offset[1] = out[1];
            cam->attn_dest_no_offset[2] = out[2];
        } else {
            f32 minX, maxX;
            f32 minY, maxY;
            f32 minZ, maxZ;
            f32 sv0, sv1, sv2;
            CameraTarget* target = (CameraTarget*)(cameraState + 2576);
            minX = minY = minZ = lbl_8034619C;
            maxX = maxY = maxZ = lbl_803461A0;
            for (i = 0; i < 15; i++, target++) {
                if (target->active > 0) {
                    f32 x = target->object->attn_pos[0];
                    f32 y = target->object->attn_pos[1];
                    f32 z = target->object->attn_pos[2];
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
                    register f64 half = lbl_80345F18;
                    out[2] = (f32)(half *
                        (half * (f64)(maxZ - minZ) *
                        cos(cam->pyr[0])) +
                        (f64)out[2]);
                } else {
                    f32 sy;
                    f32 cp;
                    f32 scale;
                    f32 cy;
                    f32 cp2;
                    f32 pitch;
                    sy = sin(cam->pyr[1]);
                    pitch = cam->pyr[0];
                    cp = cos(pitch);
                    scale = (f32)(lbl_80345F18 *
                        (f64)(maxX - minX) * lbl_803461F0);
                    out[0] = scale * cp * sy + out[0];
                    cy = cos(cam->pyr[1]);
                    pitch = cam->pyr[0];
                    cp2 = cos(pitch);
                    scale = (f32)(lbl_80345F18 *
                        (f64)(maxZ - minZ) * lbl_803461F0);
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
                    if (out[i] < *(f32*)(cameraState + i * 4 + offsetof(CameraStateData, attn_min[0]))) {
                        out[i] = *(f32*)(cameraState + i * 4 + offsetof(CameraStateData, attn_min[0]));
                        lbl_80344418 = 1;
                    } else if (out[i] > *(f32*)(cameraState + i * 4 + offsetof(CameraStateData, attn_max[0]))) {
                        out[i] = *(f32*)(cameraState + i * 4 + offsetof(CameraStateData, attn_max[0]));
                        lbl_80344418 = 1;
                    }
                }
            }
            if (lbl_80344414 != 0) {
                f32 d0 = sv0 - out[0];
                f32 d1 = sv1 - out[1];
                f32 d2 = sv2 - out[2];
                if (d0 == 0.0f && d1 == 0.0f &&
                    d2 == 0.0f) {
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
extern s32 lbl_80344520, gNumEnemies, lbl_803447F8;
extern s32 lbl_80344288, lbl_8034441C, lbl_80344420, lbl_80344A28;
extern u8* sSpecialTransmitter;
extern f32 lbl_80344524;
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
extern char lbl_80111B3C[];
void ChangeWindow(void);
f32 FloorPos(f32 fallback, f32 radius, f32* pos, s32 mode);
s32 fn_80051480(f32* pos);
f32 SlowNormalVector(f32* v);

#define CAM_SET_CMODE(camp, m)                                                \
    if (*(s32*)(cs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, c_mode)) != (m)) {                                           \
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
    Camera* c0 = (Camera*)(cs + CAMERA_STATE_CAMERAS_OFF);
    s32 scrH = MBScreenHeight();
    s32 scrW = MBScreenWidth();
    s32 uiFov = 0;
    s32 i;
    Camera* cam;
    f32 mat[16];
    f32 in[3];
    f32 out[3];
    u8 unused[8];
    f32 m2[16];

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

        cam = (Camera*)(cs + CAMERA_STATE_CAMERAS_OFF);
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
            f32 wxz;
            f32 axz;
            f32* py1;
            f32* pz1;
            f32* py2;
            f32* pz2;
            Camera* k;
            Camera* cm = (Camera*)((void*)(cs + CAMERA_STATE_CAMERAS_OFF));
            CAM_SET_CMODE(cm, 2);
            CAM_SET_AMODE(cm, 1);
            py1 = (f32*)(cs + offsetof(CameraStateData, start_wpos[1]));
            pz1 = (f32*)(cs + offsetof(CameraStateData, start_wpos[2]));
            py2 = (f32*)(cs + offsetof(CameraStateData, start_attn[1]));
            pz2 = (f32*)(cs + offsetof(CameraStateData, start_attn[2]));
            k = (Camera*)((void*)(cs + CAMERA_STATE_CAMERAS_OFF));
            c0->num1 = lbl_80345F48;
            c0->pyr[1] = c0->num1;
            c0->num2 = lbl_80345EC8;
            wxz = lbl_8034620C;
            *(f32*)(cs + offsetof(CameraStateData, start_wpos[0])) = wxz;
            *py1 = lbl_80346210;
            *pz1 = wxz;
            axz = lbl_80346214;
            *(f32*)(cs + offsetof(CameraStateData, start_attn[0])) = axz;
            *py2 = lbl_80346218;
            *pz2 = axz;
            ang = *(f32*)(cs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, pyr[1]));
            s = sin(ang);
            c = cos(ang);
            *(f32*)(cs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, wpos[0])) = s * *(f32*)(cs + offsetof(CameraStateData, start_wpos[0]));
            *(f32*)(cs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, wpos[1])) = *py1;
            *(f32*)(cs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, wpos[2])) = c * *pz1;
            *(f32*)(cs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, attn[0])) = s * *(f32*)(cs + offsetof(CameraStateData, start_attn[0]));
            *(f32*)(cs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, attn[1])) = *py2;
            *(f32*)(cs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, attn[2])) = c * *pz2;
            dy = *(f32*)(cs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, wpos[1])) - *(f32*)(cs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, attn[1]));
            dx = *(f32*)(cs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, wpos[0])) - *(f32*)(cs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, attn[0]));
            dz = *(f32*)(cs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, wpos[2])) - *(f32*)(cs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, attn[2]));
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
                u8 unused2[40];
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
                k = (Camera*)((void*)(cs + CAMERA_STATE_CAMERAS_OFF));
                c0->attn[0] = d[0] * r + c0->wpos[0];
                c0->attn[1] = d[1] * r + c0->wpos[1];
                c0->attn[2] = d[2] * r + c0->wpos[2];
                saveW[0] = *(f32*)(cs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, wpos[0]));
                saveW[1] = *(f32*)(cs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, wpos[1]));
                saveW[2] = *(f32*)(cs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, wpos[2]));
                saveA[0] = c0->attn[0];
                saveA[1] = c0->attn[1];
                saveA[2] = c0->attn[2];
                StandardCamera_8002B828(0);
                DoShake((Vec3*)saveW, (Vec3*)saveA);
                look[0] = saveA[0] - saveW[0];
                look[1] = saveA[1] - saveW[1];
                look[2] = saveA[2] - saveW[2];
                LookInDirection(look, (u32)&k->mat[0][0]);
                c0->trans_mode = -1;
                c0->state = 1;
            } else {
                s16* hdr = (s16*)gCurLevel->camera;
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
                    Camera* cm = (Camera*)((void*)(cs + CAMERA_STATE_CAMERAS_OFF));
                    CAM_SET_CMODE(cm, 5);
                    CAM_SET_AMODE(cm, 1);
                    wi = &gWorldInfo;
                    pa0 = (f32*)(cs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, attn[0]));
                    pa1 = (f32*)(cs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, attn[1]));
                    pa2 = (f32*)(cs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, attn[2]));
                    wcy = &gWorldInfo.worldcenter[1];
                    *(f32*)(cs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, attn[0])) = wi->worldcenter[0];
                    *(f32*)(cs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, attn[1])) = wi->worldcenter[1];
                    *(f32*)(cs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, attn[2])) = wi->worldcenter[2];
                    *(f32*)(cs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, attn[1])) = wi->worldmax[1];
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
                    *(f32*)(cs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, radius)) = *(f32*)((u8*)hdr + 40);
                    prad = (f32*)(cs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, radius));
                    *(f32*)(cs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, num1)) = (f32)(lbl_80346240 * *(f32*)(cs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, radius)));
                    *(f32*)(cs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, num2)) = (f32)(lbl_80345EF0 + *wcy);
                    *(f32*)(cs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, num3)) = (f32)(lbl_80345FE0 + *(f32*)(cs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, num2)));
                    *(f32*)(cs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, pyr[0])) = lbl_80346248;
                    *(f32*)(cs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, pyr[1])) = lbl_80345EC8;
                    *(f32*)(cs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, pyr[2])) = lbl_80345EC8;
                    CreateYPRMatrix(mat, (f32*)(cs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, pyr[0])));
                    in[0] = lbl_80345EC8;
                    in[1] = lbl_80345EC8;
                    in[2] = *prad;
                    WorldVector(in, out, mat);
                    pw0 = (f32*)(cs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, wpos[0]));
                    pw1 = (f32*)(cs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, wpos[1]));
                    pw2 = (f32*)(cs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, wpos[2]));
                    *pw0 = *pa0 + out[0];
                    *pw1 = *pa1 + out[1];
                    *pw2 = *pa2 + out[2];
                    r = *prad;
                    d[0] = *(f32*)(cs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, wpos[0])) - *pa0;
                    d[1] = *(f32*)(cs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, wpos[1])) - *pa1;
                    d[2] = *(f32*)(cs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, wpos[2])) - *pa2;
                    SlowNormalVector(d);
                    *pw0 = d[0] * r + *pa0;
                    *pw1 = d[1] * r + *pa1;
                    *pw2 = d[2] * r + *pa2;
                    *(s32*)(cs + CAMERA_STATE_CAMERAS_OFF) = 1;
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
                    Camera* cm = (Camera*)((void*)(cs + CAMERA_STATE_CAMERAS_OFF));
                    CAM_SET_CMODE(cm, 2);
                    CAM_SET_AMODE(cm, 1);
                    wi = &gWorldInfo;
                    dpp = gDefaultPlayerPosition;
                    p0 = (f32*)(cs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, wpos[0]));
                    p1 = (f32*)(cs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, wpos[1]));
                    p2 = (f32*)(cs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, wpos[2]));
                    prad = (f32*)(cs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, radius));
                    *(f32*)(cs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, wpos[0])) = wi->worldcenter[0];
                    *(f32*)(cs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, wpos[1])) = wi->worldcenter[1];
                    *(f32*)(cs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, wpos[2])) = wi->worldcenter[2];
                    *(f32*)(cs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, wpos[1])) = (f32)(lbl_80346250 + dpp[1]);
                    *(f32*)(cs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, radius)) = lbl_80346258;
                    *(f32*)(cs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, pyr[0])) = *(f32*)((u8*)hdr + 40);
                    *(f32*)(cs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, pyr[1])) = lbl_80345EC8;
                    *(f32*)(cs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, pyr[2])) = lbl_80345EC8;
                    CreateYPRMatrix(mat, (f32*)(cs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, pyr[0])));
                    in[0] = lbl_80345EC8;
                    in[1] = lbl_80345EC8;
                    in[2] = *prad;
                    WorldVector(in, out, mat);
                    pa0 = (f32*)(cs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, attn[0]));
                    pa1 = (f32*)(cs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, attn[1]));
                    pa2 = (f32*)(cs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, attn[2]));
                    *pa0 = *p0 + out[0];
                    *pa1 = *p1 + out[1];
                    *pa2 = *p2 + out[2];
                    r = *prad;
                    d[0] = *(f32*)(cs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, attn[0])) - *p0;
                    d[1] = *(f32*)(cs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, attn[1])) - *p1;
                    d[2] = *(f32*)(cs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, attn[2])) - *p2;
                    SlowNormalVector(d);
                    *pa0 = d[0] * r + *p0;
                    *pa1 = d[1] * r + *p1;
                    *pa2 = d[2] * r + *p2;
                    *(s32*)(cs + CAMERA_STATE_CAMERAS_OFF) = 1;
                    lbl_80344420 = 1500;
                    break;
                }
                default: {
                    void dbgTextPrintfCol(s32 x, s32 line, char* fmt, ...);
                    f32 saveA[3];
                    f32 saveW[3];
                    f32 d2[3];
                    u8* st = sSpecialTransmitter;
                    f32* p0 = (f32*)(cs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, wpos[0]));
                    f32* p1 = (f32*)(cs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, wpos[1]));
                    f32* p2 = (f32*)(cs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, wpos[2]));
                    Camera* k;
                    *p0 = *(f32*)(st + 4);
                    *p1 = *(f32*)(st + 8);
                    *p2 = *(f32*)(st + 12);
                    *(f32*)(cs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, pyr[0])) = *(f32*)(st + 20);
                    *(f32*)(cs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, pyr[1])) = *(f32*)(st + 24);
                    *(f32*)(cs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, pyr[2])) = *(f32*)(st + 28);
                    CreateYPRMatrix(m2, c0->pyr);
                    *(f32*)(cs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, radius)) = lbl_80346148;
                    in[0] = lbl_80345EC8;
                    in[1] = lbl_80345EC8;
                    in[2] = *(f32*)(cs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, radius));
                    WorldVector(in, out, m2);
                    *(f32*)(cs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, attn[0])) = *p0 + out[0];
                    *(f32*)(cs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, attn[1])) = *p1 + out[1];
                    *(f32*)(cs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, attn[2])) = *p2 + out[2];
                    CAM_SET_CMODE(c0, 2);
                    CAM_SET_AMODE(c0, 1);
                    c0->trans_mode = 0;
                    k = (Camera*)((void*)(cs + CAMERA_STATE_CAMERAS_OFF));
                    saveW[0] = *(f32*)(cs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, wpos[0]));
                    saveW[1] = *(f32*)(cs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, wpos[1]));
                    saveW[2] = *(f32*)(cs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, wpos[2]));
                    saveA[0] = c0->attn[0];
                    saveA[1] = c0->attn[1];
                    saveA[2] = c0->attn[2];
                    StandardCamera_8002B828(0);
                    DoShake((Vec3*)saveW, (Vec3*)saveA);
                    d2[0] = saveA[0] - saveW[0];
                    d2[1] = saveA[1] - saveW[1];
                    d2[2] = saveA[2] - saveW[2];
                    LookInDirection(d2, (u32)&k->mat[0][0]);
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
            s16* hdr = (s16*)gCurLevel->camera;
            f32 z = lbl_80345EC8;
            CAM_SET_CMODE(c0, 1);
            CAM_SET_AMODE(c0, 0);
            *(f32*)(cs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, attn[0])) = z;
            *(f32*)(cs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, attn[1])) = z;
            *(f32*)(cs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, attn[2])) = z;
            *(f32*)(cs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, radius)) = lbl_8034625C;
            *(f32*)(cs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, wpos[0])) = z;
            *(f32*)(cs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, wpos[1])) = z;
            *(f32*)(cs + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, wpos[2])) = z;
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
            *(s32*)(cs + CAMERA_STATE_CAMERAS_OFF) = 1;
        }
    }

    lbl_80344534 = lbl_80118B60[lbl_80344538];
    for (i = 0; i < 6; i++) {
        u8* row = cs + i * 396;
        *(f32*)(row + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, mat[3][0])) = *(f32*)(row + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, wpos[0]));
        *(f32*)(row + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, mat[3][1])) = *(f32*)(row + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, wpos[1]));
        *(f32*)(row + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, mat[3][2])) = *(f32*)(row + CAMERA_STATE_CAMERAS_OFF + offsetof(Camera, wpos[2]));
    }
    {
        f32 z0 = lbl_80345EC8;
        for (i = 0; i < 3; i++) {
            u8* row = cs + i * 4;
            *(f32*)(row + offsetof(CameraStateData, unk10[0])) = z0;
        }
    }
    {
        s32 zi = 0;
        u8* q;
        f32* q20;
        f32* q8;
        f32* q24;
        f32* q12;
        f32 zf;
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
        q20 = (f32*)(q + 20);
        q8 = (f32*)(q + 8);
        q24 = (f32*)(q + 24);
        q12 = (f32*)(q + 12);
        zf = lbl_80345EC8;
        lbl_80344520 = (s32)(*q8 - *q20 * lbl_8034601C);
        lbl_8034451C = (s32)(*q20 * lbl_8034601C + *q8);
        lbl_80344518 = (s32)(*q24 * lbl_8034601C + *q12);
        lbl_80344514 = (s32)(*q12 - *q24 * lbl_8034601C);
        t = (CameraTarget*)(cs + CAMERA_STATE_TARGETS_OFF);
        for (i = 0; i < 15; i++, t++) {
            t->active = zi;
            t->object = NULL;
            t->position[0] = zf;
            t->position[1] = zf;
            t->position[2] = zf;
        }
    }
    gCameraTargetCount = 0;
    ProcCamera_8002E548(0, 0);
    {
        f32 zv = lbl_80345EC8;
        cam = (Camera*)(cs + CAMERA_STATE_CAMERAS_OFF);
        for (i = 0; i < 6; i++, cam++) {
            cam->limit_pos[0] = cam->mat[3][0];
            cam->limit_pos[1] = cam->mat[3][1];
            cam->limit_pos[2] = cam->mat[3][2];
            cam->limit_vel[0] = zv;
            cam->limit_vel[1] = zv;
            cam->limit_vel[2] = zv;
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
    Camera* srcCam = (Camera*)src;
    Camera* dstCam = (Camera*)dst;

    CopyMat4(&srcCam->mat[0][0], &dstCam->mat[0][0]);
#define CF(field) dstCam->field = srcCam->field
#define CI(field) dstCam->field = srcCam->field
    CF(wpos[0]); CF(wpos[1]); CF(wpos[2]);
    CF(old_wpos[0]); CF(old_wpos[1]); CF(old_wpos[2]);
    CF(vel[0]); CF(vel[1]); CF(vel[2]);
    CF(avel[0]); CF(avel[1]); CF(avel[2]);
    CF(pyr[0]); CF(pyr[1]); CF(pyr[2]);
    CF(pyr_delta[0]); CF(pyr_delta[1]); CF(pyr_delta[2]);
    CI(unvib); CF(radius); CI(trans_mode); CI(timer); CI(mode); CI(flags);
    CF(value); CF(num1); CF(num2); CF(num3);
    CI(c_mode); CI(pc_mode); CI(camobj); CI(a_mode); CI(pa_mode); CI(pn);
    CI(en); CI(gn); CI(mn); CI(ln); CI(cn); CI(attnobj);
    CF(offset[0]); CF(offset[1]); CF(offset[2]);
    CF(attn[0]); CF(attn[1]); CF(attn[2]);
    CF(old_attn[0]); CF(old_attn[1]); CF(old_attn[2]);
    CF(delta[0]); CF(delta[1]); CF(delta[2]);
    CF(attn_dest[0]); CF(attn_dest[1]); CF(attn_dest[2]);
    CF(attn_dest_no_offset[0]);
    CF(attn_dest_no_offset[1]);
    CF(attn_dest_no_offset[2]);
    CF(cam_dest[0]); CF(cam_dest[1]); CF(cam_dest[2]);
#undef CF
#undef CI
}

void ProcCamera_8002E548(s32 camIdx, s32 useRecorderPosition)
{
    u8* gcs = (u8*)gCameraState;
    u8* cp = gcs + camIdx * 396;
    Camera* cam;
    u8 lo[16];
    f32 offset[3];
    u8 hi[12];

    if (*(s32*)(cp += 0xC8) == 0) {
        return;
    }
    cam = (Camera*)cp;
    if (cam->a_mode == ATN_FREE && cam->c_mode != CAM_OBJEYE &&
        cam->c_mode != CAM_VECDIST) {
        CreateYPRMatrix(&cam->mat[0][0], cam->pyr);
    }
    if ((gGameBusy | gGameplayPauseTimer) == 0) {
        WorldVector(cam->vel, offset, &cam->mat[0][0]);
        cam->wpos[0] += offset[0];
        cam->wpos[1] += offset[1];
        cam->wpos[2] += offset[2];
        WorldVector(cam->avel, offset, &cam->mat[0][0]);
        cam->attn[0] += offset[0];
        cam->attn[1] += offset[1];
        cam->attn[2] += offset[2];
    }
    if (useRecorderPosition == 0 || camIdx != 0 || cam->c_mode != CAM_GAME) {
        cam->mat[3][0] = cam->wpos[0];
        cam->mat[3][1] = cam->wpos[1];
        cam->mat[3][2] = cam->wpos[2];
    } else {
        cam->mat[3][0] = *(f32*)(gcs + offsetof(CameraStateData, recorder_wpos[0]));
        cam->mat[3][1] = *(f32*)(gcs + offsetof(CameraStateData, recorder_wpos[1]));
        cam->mat[3][2] = *(f32*)(gcs + offsetof(CameraStateData, recorder_wpos[2]));
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
 * screen_limitation -- debug overlay printing the active camera's world
 * position, attention, orientation, distance and its camera-/attention-mode
 * names.  The camera is selected by lbl_8034453C, not by a parameter.  Gated
 * by the debug flag (sFlags & 1); no effect in a normal build.
 */
void screen_limitation(void)
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
    yaw = FixAngle((f32)(lbl_80345F60 - (f64)cam->pyr[1]));
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
    case 0:  dbgTextPrintfCol(5, row - 7, "OFF    "); break;
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
        dbgTextPrintfCol(0x12, row - 7, "PLAYER %02X   ", cam->pn);
        dbgTextPrintfCol(0xE, row - 6, "                               ");
        break;
    case 6: {
        s32 enemy = cam->en;
        u8* e = (u8*)gEnemies + enemy * ENEMY_STRIDE;

        dbgTextPrintfCol(0x12, row - 7, "ENEMY %02X    ", enemy);
        dbgTextPrintfCol(0xE, row - 6, "%s (AI=%d)                     ",
                         lbl_8011B578[*(s32*)e], *(s16*)(e + 0x310));
        break;
    }
    case 8:
        dbgTextPrintfCol(0x12, row - 7, "MILESTONE %02X", cam->mn);
        dbgTextPrintfCol(0xE, row - 6, "                               ");
        break;
    case 9:
        dbgTextPrintfCol(0x12, row - 7, "LOOKOUT %02X  ", cam->ln);
        dbgTextPrintfCol(0xE, row - 6, "                               ");
        break;
    case 10:
        dbgTextPrintfCol(0x12, row - 7, "CAMERA %02X   ", cam->cn);
        dbgTextPrintfCol(0xE, row - 6, "                               ");
        break;
    case 7: {
        s32 index = cam->gn;
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
    dbgTextPrintfCol(1, row - 6, "MODE=%d ", cam->mode);
    fn_800C02F4(-1);
}
