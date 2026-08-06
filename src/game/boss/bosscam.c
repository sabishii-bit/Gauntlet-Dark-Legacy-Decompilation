#include "types.h"
#include "game/camera.h"

/*
 * game/boss/bosscam.c  --  BOSSCAM.OBJ (GameCube GUNE5D)
 * ---------------------------------------------------------------------------
 * Boss / game camera control TU.  .text 0x8001BC88 - 0x8001EAE0.
 * Sits between game/boss/boss.c (ends 0x8001BC88) and game/ui/btext.c
 * (starts 0x8001EAE0).  Names are the REAL Midway symbols recovered from the
 * Xbox shell3D PDB module ".\Release\BOSSCAM.OBJ"
 * (research/xbox_symbols/functions_by_module.txt).
 *
 * The GameCube build emits 17 of the 25 PDB functions; the eight small/debug
 * helpers below were inlined or dead-stripped on GCN and have no out-of-line
 * body here:
 *     LimitCamVal, BossCamControlInputs, BossCamGetCurrent, BossCamLimit,
 *     BossCamStartCalc, BossCamSelectCalc, PointInView, GetBossCamViewDist.
 *
 * Status: NonMatching, but most of the small/medium functions are now real
 * reconstructions verified against the extracted target (fndiff):
 *   MATCHING (byte-exact):   TriggerCameraEnd, BossCameraInit, GameCameraInit,
 *                            BossCameraStart
 *   NEAR (correct logic, regalloc/scheduler/idiom residuals only):
 *       PointViewDist, CameraLimitPlayerDpos, GetPlayerViewDist,
 *       GetBossAvgPos, TriggerCameraActivate
 *   FAITHFUL (reconstructed, NonMatching -- intricate clip-flag control flow):
 *       CamLimitPlayerDpos
 *   PARKED as documented stubs (large fp-math / giant / unresolved prototype):
 *       TriggerCamUpdate (0x244, blit driver + (x&0)^0 flag idiom),
 *       BossCameraUpdate (0x4B4), BossCamBossCalc (0xE24, GIANT),
 *       BossCamPlayerCalc (0x5C0), BossCamLimitAttn (0x3C4),
 *       GetActualAvgVec (0x4B4), LimitCamVal2 (0x160)
 * Because bosscam.c is configured NonMatching (linked False) it is linked from
 * the original DOL bytes, so the tree stays sha1-green regardless of the source.
 * All globals/constants are referenced as out-of-object symbols because this TU
 * carries no data split of its own (only .text/extab in splits.txt).
 *
 * Function map (GCN addr -> PDB name, scope, and what it does):
 *   0x8001BC88 TriggerCamUpdate       (global) per-frame trigger-camera driver
 *                                              (called from main game loop)
 *   0x8001BECC TriggerCameraEnd       (global) clears the 4 trigger-cam flag words
 *   0x8001BEE4 TriggerCameraActivate  (global) arms a scripted trigger camera
 *                                              (5 external callers: boss triggers)
 *   0x8001BFD4 CameraLimitPlayerDpos  (global) wrapper: gate on boss state,
 *                                              then call CamLimitPlayerDpos
 *   0x8001C0D0 CamLimitPlayerDpos     (global) clamp a player's delta-position
 *                                              against the camera view frustum
 *   0x8001C32C PointViewDist          (global) frustum clip test of a point;
 *                                              returns worst-plane distance and
 *                                              writes clip-flag globals
 *   0x8001C440 BossCameraInit         (global) memset the 0x60 boss-cam state
 *   0x8001C478 BossCameraUpdate       (global) main boss-cam update; dispatches
 *                                              BossCameraStart / BossCamPlayerCalc
 *                                              / BossCamBossCalc (called from main)
 *   0x8001C92C BossCamBossCalc        (static) boss-follow camera solve (atan2)
 *   0x8001D750 BossCamPlayerCalc      (static) player-follow camera solve
 *   0x8001DD10 BossCamLimitAttn       (static) clamp the camera attention/look
 *                                              target + distance (uses LimitCamVal2)
 *   0x8001E0D4 BossCameraStart        (static) initialise camera pose (cos)
 *   0x8001E210 GetPlayerViewDist      (static) min PointViewDist over the 4
 *                                              players (stride 0x335C)
 *   0x8001E2E4 GetBossAvgPos          (static) averaged/interpolated boss pos
 *   0x8001E488 GetActualAvgVec        (static) averaged view vector
 *   0x8001E93C GameCameraInit         (global) set up gGameCamera pointer + pose
 *   0x8001E980 LimitCamVal2           (static) velocity/acceleration clamp of a
 *                                              scalar camera value (shared helper)
 *
 * Key data (renamed in symbols.txt):
 *   gGameCamera      0x803443D4  pointer to the active game-camera struct
 *   gGameCameraData  0x8023E92C  the game-camera struct instance
 *   gBossCamData     0x8023E8CC  0x60-byte boss-camera state (memset by init)
 *   gPointViewFlags  0x803443CC  clip-flag word written by PointViewDist
 *   gPointViewPlane  0x803443C8  which frustum plane was worst (PointViewDist)
 */

/* Externals referenced by the reconstructed bodies below.  Because bosscam.c
 * has no data split of its own, every constant/global the target functions
 * touch is an out-of-object symbol here; we mirror that with plain externs so
 * the compiled relocations line up with the extracted target by name. */
extern void* memset(void* dst, int val, size_t n);

extern void* gGameCamera;             /* .sbss 0x803443D4 */
extern u8 gGameCameraData[0x1B4];     /* .bss  0x8023E92C */
extern u8 gBossCamData[0x60];         /* .bss  0x8023E8CC */

typedef struct BossGameCameraView {
    u8 pad_000[220];
    f32 view_scale;
} BossGameCameraView;

/* bosscam-owned .sbss scratch (referenced externally, see split notes) */
extern s32 lbl_803443A8;
extern s32 lbl_803443AC;
extern s32 gTriggerCameraState;
extern s32 lbl_803443B8;
extern s32 lbl_803443BC;              /* trigger-cam frame timer */
extern s32 lbl_803443C0;
extern u8* lbl_803443C4;
extern f32 gCameraWindowScaleX;
extern f32 gCameraWindowScaleY;
extern s32 gFrameTicks;
extern s32 lbl_803447DC;
extern void* lbl_80257630[4];
extern s32 gControllerButtons;
extern s32 sFlags;
extern s32 lbl_80344BF8;
extern s32 lbl_80343BA8;
extern const f32 lbl_80345B80;

extern const f32 lbl_80345C28;        /* .sdata2 pooled float literal */

/* trigger-camera scripting helpers */
extern u8 lbl_8023E880[0x40];         /* .bss look-at matrix / target buffer */
extern u8 lbl_80127D40[];             /* rotation-axis constant */
extern const f64 lbl_80345B88;        /* base heading (radians) */
extern void DisablePlayerControls(void);
extern void YawVec3(void* axis, f32* out, f32 angle);
extern void PitchVec3(f32* a, f32* b, f32 angle);
extern void LookInDirection(f32* mtx, void* target);
extern void vibrators_off(void);
extern void StdCamFreeze(void);
extern void MBCameraUpdate(f32* position, f32* matrix);
extern void* MBNewTempBlit(s32 texture, s32 x, s32 y, s32 width, s32 height);
extern void MBBlitSetColor(void* blit, u32 color);
extern void mbBlitCvtCoord(void* blit, f64 depth);
extern void mbBlitInit3414(void* blit, s32 enabled);
extern void EnablePlayerControls(void);
extern void fn_8006ECD4(void);
extern void do_camera(void);
extern void chg_target_state(s32 mode);

/* PointViewDist frustum-test state */
extern f32* lbl_80344EE8;             /* pointer to the active view/frustum */
extern s32 gPointViewNearPlane;       /* .sbss 0x803443C8  worst-plane index */
extern s32 gPointViewClipFlags;       /* .sbss 0x803443CC  per-plane clip bits */
extern const f32 lbl_80345BA4;        /* +INF sentinel for the running minimum */
extern const f64 lbl_80345B98;        /* clip threshold (0.0) */
extern const f64 lbl_80345BB8;        /* 2 pi */
extern const f64 lbl_80345BC0;        /* -pi */
extern f32 gClockFrameReciprocal;
extern f32 gClockFrameStep;

extern void CamReset(void* camera);

/* Player array (stride 0x335C) and boss gating state used by the dpos clamps */
extern u8 gPlayers[];             /* base of the 4 player structs */
extern s32 gBossDead;
extern s32 gBossDying;
extern s32 gBossType;
extern s32 lbl_803443B0;
extern s32 lbl_80344A80;
extern f32 lbl_8023E8C0[3];           /* scratch view-space point */

extern s32 fn_800629B0(void);
extern s32 fn_8006DC2C(void* ps, f32* dpos, s32 arg);
extern s32 camera_debug_supervisor(s32 a, f32* dpos);
extern void MulBodyVecMat4(f32* in, f32* out, void* mtx);
extern void MulVecMat4(f32* in, f32* out, void* mtx);
extern void CalcFrustrumNormals(f32* a, f32* b, f32* c);
extern void fn_8006DC64(void* camera, u8* ps, f32* dpos, s32 arg);

/* level / camera-pose state and math libs */
extern u8* gCurLevel;
extern u8* lbl_803443D0;               /* the active scripted / boss camera */
extern u8* gBossObj;                   /* boss actor (pos vec at +0x4C) */
extern s32 lbl_80343C5C;               /* widescreen aspect-scale flag */
extern const f32 lbl_80343B84;         /* aspect scale factor */
extern const f32 lbl_80345BA0;         /* 0.0f */
extern const f32 lbl_80345C58;         /* default upper bound */
extern const f64 lbl_80345C50;
extern const f64 lbl_80345B90;
extern const f64 lbl_80345BE0;
extern const f64 lbl_80345BA8;         /* 0.5 */
extern f32 cos(f32);

/* forward decls for the in-file functions used before their definitions */
void CamLimitPlayerDpos(void* camera, u8* ps, f32* dpos, s32 arg);
f32 PointViewDist(f32* point, f32 dist);

#pragma opt_propagation off
s32 TriggerCamUpdate(void)
{
    u8* cameraBuffer;
    f32* position;
    void* blit;
    f32 x;
    f32 y;
    f32 z;
    s32 i;
    s32 offset;
    s32 zero;
    s32 two;

    cameraBuffer = lbl_8023E880;
    if (gTriggerCameraState == 0) {
        return 0;
    }
    if (lbl_803443B8 > 0) {
        lbl_803443B8 -= gFrameTicks;
        return 0;
    }

    StdCamFreeze();
    position = (f32*)(cameraBuffer + 48);
    MBCameraUpdate(position, (f32*)cameraBuffer);
    x = position[0];
    gCameras[0].attn[0] = x;
    y = position[1];
    gCameras[0].attn[1] = y;
    z = position[2];
    gCameras[0].attn[2] = z;
    gCameras[0].mat[3][0] = x;
    gCameras[0].mat[3][1] = y;
    gCameras[0].mat[3][2] = z;

    blit = MBNewTempBlit(lbl_80344BF8, 0, 0, 512, lbl_80343BA8);
    MBBlitSetColor(blit, 0);
    mbBlitCvtCoord(blit, lbl_80345B80);
    blit = MBNewTempBlit(lbl_80344BF8, 0, 304, 512, 80);
    MBBlitSetColor(blit, 0);
    mbBlitCvtCoord(blit, lbl_80345B80);

    if (lbl_803447DC != 0) {
        i = 0;
        offset = 0;
        do {
            mbBlitInit3414(*(void**)((u8*)lbl_80257630 + offset), 1);
            i++;
            offset += 4;
        } while (i < 4);
    }

    lbl_803443BC -= gFrameTicks;
    if (lbl_803443BC <= 0) {
        if (lbl_803443C0 > 0) {
            lbl_803443C0 -= gFrameTicks;
        } else if (lbl_803443C4 == 0 ||
                   (*(u32*)(lbl_803443C4 + 0x10) & 0x08000000) == 0) {
            if (gTriggerCameraState == 2) {
                EnablePlayerControls();
            }
            i = gTriggerCameraState = 0;
            if (lbl_803447DC != 0) {
                offset = 0;
                do {
                    mbBlitInit3414(*(void**)((u8*)lbl_80257630 + offset), 0);
                    i++;
                    offset += 4;
                } while (i < 4);
            }
            zero = 0;
            two = 2;
            if ((((sFlags & two) ^ zero) |
                 ((gControllerButtons & zero) ^ zero)) != 0) {
                fn_8006ECD4();
            } else {
                do_camera();
            }
            gCameras[0].attn[0] = gCameras[0].attn_dest[0];
            gCameras[0].attn[1] = gCameras[0].attn_dest[1];
            gCameras[0].attn[2] = gCameras[0].attn_dest[2];
            chg_target_state(10);
        }
    }
    return gTriggerCameraState;
}
#pragma opt_propagation reset

void TriggerCameraEnd(void) {
    lbl_803443B8 = 0;
    lbl_803443BC = 0;
    lbl_803443C0 = 0;
    lbl_803443C4 = 0;
}

void TriggerCameraActivate(s32 p1, f32* p2, f32* p3, s32 duration, s32 p5, s32 p6) {
    u8* buf;
    f32 mtx[4];

    buf = lbl_8023E880;
    if (duration >= 0) {
        lbl_803443BC = (duration != 0) ? duration * 6 : 40;
        gTriggerCameraState = 2;
    } else {
        lbl_803443BC = 1000000;
        gTriggerCameraState = 2;
    }
    if (gTriggerCameraState == 2) {
        DisablePlayerControls();
    }
    lbl_803443B8 = p5;
    lbl_803443C0 = p6;
    lbl_803443C4 = (u8*)p1;
    YawVec3(lbl_80127D40, mtx, lbl_80345B88 - p3[1]);
    PitchVec3(mtx, mtx, p3[0]);
    *(f32*)(buf + 48) = p2[0];
    *(f32*)(buf + 52) = p2[1];
    *(f32*)(buf + 56) = p2[2];
    LookInDirection(mtx, buf);
    vibrators_off();
}
s32 CameraLimitPlayerDpos(s32 player, f32* dpos, s32 arg) {
    s32 ret = 1;
    u8* ps = &gPlayers[player * 0x335C];
    f32 savedY = dpos[1];

    if (lbl_803443A8 != 0) {
        if (!(gBossDead || gBossDying)) goto limit_player;
        if (gBossType == 41) goto check_boss_camera;
        if (gBossType == 37) goto check_boss_camera;
        if (gBossType != 36) goto limit_player;
check_boss_camera:
        if (!fn_800629B0()) goto limit_player;
        if (lbl_803443B0 == 0) goto limit_done;
limit_player:
        CamLimitPlayerDpos(gGameCamera, ps, dpos, arg);
        dpos[1] = savedY;
limit_done:
        ;
    } else if (lbl_80344A80 == 2) {
        ret = fn_8006DC2C(ps, dpos, arg);
    } else {
        ret = camera_debug_supervisor(*(s32*)ps, dpos);
        dpos[1] = savedY;
    }
    return ret;
}

/* Clamp a player's requested delta-position so the player stays inside the
 * camera frustum.  Faithful reconstruction from asm; NonMatching (the clip-flag
 * control flow and rlwinm plane masks compile with different branch shapes). */
void CamLimitPlayerDpos(void* camera, u8* ps, f32* dpos, s32 arg) {
    u8* buf = lbl_8023E880;
    f32 viewpt[3];
    f32 world[3];
    f32 radius0, radius1, dist;
    s32 flags, planeLR;
    f32 dx, dy, dz;

    u8* cam = (u8*)camera;

    if (lbl_80343C5C != 0) {
        CalcFrustrumNormals((f32*)(cam + 224), (f32*)(cam + 164), (f32*)(cam + 64));
        fn_8006DC64(camera, ps, dpos, arg);
        return;
    }
    if (*(f32*)(cam + 244) > *(f32*)(cam + 220)) {
        arg = 0;
    }
    radius0 = lbl_80345B90 * *(f32*)(ps + 2132);
    MulBodyVecMat4((f32*)(ps + 84), (f32*)(buf + 64), camera);
    radius0 = PointViewDist((f32*)(buf + 64), radius0);

    world[0] = *(f32*)(ps + 84) + dpos[0];
    world[1] = *(f32*)(ps + 88) + dpos[1];
    world[2] = *(f32*)(ps + 92) + dpos[2];
    radius1 = lbl_80345B90 * *(f32*)(ps + 2132);
    MulBodyVecMat4(world, (f32*)(buf + 64), camera);
    dist = PointViewDist((f32*)(buf + 64), radius1);
    if (dist >= lbl_80345B98) return;
    if (dist >= radius0) return;

    flags = gPointViewClipFlags;
    planeLR = flags & 0x30;
    if (arg != 0 && !(planeLR != 0 && (flags & 0x300) != 0) && (flags & 1) == 0) {
        if (flags == 0) return;
        MulBodyVecMat4((f32*)(ps + 84), viewpt, camera);
        dx = *(f32*)(buf + 64) - viewpt[0];
        dy = *(f32*)(buf + 68) - viewpt[1];
        dz = *(f32*)(buf + 72) - viewpt[2];
        if (planeLR != 0) {
            dx = lbl_80345BA0;
        } else if ((flags & 0x300) != 0) {
            dy = lbl_80345BA0;
        }
        if (dz < lbl_80345BA0) {
            if ((flags & 0x230) != 0) dz = lbl_80345BA0;
        } else if (dz > lbl_80345BA0) {
            if ((flags & 0x130) != 0) dz = lbl_80345BA0;
        }
        *(f32*)(buf + 64) = viewpt[0] + dx;
        *(f32*)(buf + 68) = viewpt[1] + dy;
        *(f32*)(buf + 72) = viewpt[2] + dz;
        MulVecMat4((f32*)(buf + 64), world, camera);
        dpos[0] = world[0] - *(f32*)(ps + 84);
        dpos[1] = world[1] - *(f32*)(ps + 88);
        dpos[2] = world[2] - *(f32*)(ps + 92);
    } else {
        dpos[0] = lbl_80345BA0;
        dpos[1] = lbl_80345BA0;
        dpos[2] = lbl_80345BA0;
    }
}
f32 PointViewDist(f32* point, f32 dist) {
    f32* view = lbl_80344EE8;
    s32 flags = 0;
    s32 worst = 0;
    f32 best = 1.0e20f;
    f32 zx, zy;
    f32 d;
    f32 pointZ;
    f32 nearZ;

    pointZ = point[2];
    nearZ = view[23];
    d = pointZ - nearZ - dist;
    if (d < best) {
        best = d;
        worst = 1;
        if (d < lbl_80345B98) flags |= 1;
    }
    zx = point[2] * view[9];
    zy = point[2] * view[10];
    d = view[13] * (point[0] + zx) - dist;
    if (d < best) {
        best = d;
        worst = 16;
        if (d < lbl_80345B98) flags |= 16;
    }
    d = view[13] * (zx - point[0]) - dist;
    if (d < best) {
        best = d;
        worst = 32;
        if (d < lbl_80345B98) flags |= 32;
    }
    d = view[14] * (point[1] + zy) - dist;
    if (d < best) {
        best = d;
        worst = 512;
        if (d < lbl_80345B98) flags |= 512;
    }
    d = view[14] * (zy - point[1]) - dist;
    if (d < best) {
        best = d;
        worst = 256;
        if (d < lbl_80345B98) flags |= 256;
    }
    gPointViewNearPlane = worst;
    gPointViewClipFlags = flags;
    return best;
}

void BossCameraInit(void) {
    lbl_803443AC = 0;
    memset(gBossCamData, 0, 0x60);
}

/* Main boss-camera update (0x4B4).  Dispatches BossCameraStart on the first
 * frame, then BossCamPlayerCalc / BossCamBossCalc; applies DoShake and the
 * LookInDirection pose.  Parked: large, many unknown gGameCamera fields. */
void BossCameraUpdate(void) {}

/* Boss-follow camera solve (0xE24, atan2 heavy).  GIANT -- parked per the
 * project's iteration policy; documented in the map above.  Calls
 * GetBossAvgPos / GetActualAvgVec / GetPlayerViewDist / BossCamLimitAttn /
 * LimitCamVal2. */
static void BossCamBossCalc(void) {}

/* Player-follow camera solve (0x5C0).  Parked: large fp-math body sharing
 * LimitCamVal2 and PointViewDist. */
static void BossCamPlayerCalc(void) {}

/* Clamp the camera attention/look target + distance via LimitCamVal2 (0x3C4).
 * Parked: uses sprintf/strlen debug print + several unknown camera fields. */
static void BossCamLimitAttn(f32* target) { (void)target; }
static void BossCameraStart(void) {
    lbl_803443D0 = *(u8**)(gCurLevel + 0x6C);
    if (lbl_803443D0 != 0) {
        *(f32*)(lbl_803443D0 + 8) = cos(*(f32*)(lbl_803443D0 + 4));
        if (lbl_80345BA0 == *(f32*)(lbl_803443D0 + 24)) {
            *(f32*)(lbl_803443D0 + 24) = *(f32*)(lbl_803443D0 + 20);
        }
        if (*(f32*)(lbl_803443D0 + 52) >= lbl_80345C50) {
            *(f32*)(lbl_803443D0 + 48) = *(f32*)(lbl_803443D0 + 36);
            *(f32*)(lbl_803443D0 + 52) = *(f32*)(lbl_803443D0 + 40);
            *(f32*)(lbl_803443D0 + 56) = *(f32*)(lbl_803443D0 + 44);
        }
        ((BossGameCameraView*)gGameCamera)->view_scale =
            lbl_80345B90 * *(f32*)(lbl_803443D0 + 24);
        if (lbl_80343C5C != 0) {
            ((BossGameCameraView*)gGameCamera)->view_scale *= lbl_80343B84;
        }
    }
    *(f32*)((u8*)gGameCamera + 240) = lbl_80345BA0;
    *(f32*)((u8*)gGameCamera + 248) = lbl_80345BA0;
    *(f32*)((u8*)gGameCamera + 264) = lbl_80345BA0;
    *(f32*)((u8*)gGameCamera + 188) = lbl_80345BA0;
    *(f32*)((u8*)gGameCamera + 192) = lbl_80345BA0;
    *(f32*)((u8*)gGameCamera + 196) = lbl_80345BA0;
    *(f32*)((u8*)gGameCamera + 200) = lbl_80345BA0;
    *(f32*)((u8*)gGameCamera + 204) = lbl_80345BA0;
    *(f32*)((u8*)gGameCamera + 208) = lbl_80345BA0;
    *(f32*)((u8*)gGameCamera + 212) = lbl_80345BA0;
    *(f32*)((u8*)gGameCamera + 216) = lbl_80345BA0;
    *(f32*)((u8*)gGameCamera + 256) = lbl_80345BA0;
}
static f32 GetPlayerViewDist(void* mtx) {
    f32 d;
    f32 best = lbl_80345BA4;
    f32 pt[3];
    u8 frame_pad[12];
    s32 i;

    for (i = 0; i < 4; i++) {
        u8* ps = &gPlayers[i * 0x335C];
        if (*(s32*)(ps + 0xE8) == 1) {
            pt[0] = *(f32*)(ps + 0x54) + *(f32*)(ps + 0x888);
            pt[1] = *(f32*)(ps + 0x58) + *(f32*)(ps + 0x88C);
            pt[2] = *(f32*)(ps + 0x5C) + *(f32*)(ps + 0x890);
            d = *(f32*)(ps + 0x854);
            MulBodyVecMat4(pt, lbl_8023E8C0, mtx);
            d = PointViewDist(lbl_8023E8C0, d);
            if (d < best) best = d;
        }
    }
    return best;
}
static void GetBossAvgPos(f32* out, f32 t, f32* p4, f32* p5, s32 mode) {
    f32* bpos = (f32*)(gBossObj + 76);
    s32 i;

    if (t <= lbl_80345B98) {
        out[0] = bpos[0];
        out[1] = bpos[1];
        out[2] = bpos[2];
    } else if (mode == 0) {
        f32 s = t + lbl_80345BE0;
        f32 r = lbl_80345BE0 / s;
        f64 w = (s - lbl_80345BE0) * r;
        out[0] = out[0] * w;
        out[1] = out[1] * w;
        out[2] = out[2] * w;
        out[0] = bpos[0] * r + out[0];
        out[1] = bpos[1] * r + out[1];
        out[2] = bpos[2] * r + out[2];
    } else {
        f32 hi[3];
        f32 lo[3];

        if (p5 != 0) {
            lo[0] = p5[0];
            lo[1] = p5[1];
            lo[2] = p5[2];
        } else {
            lo[0] = lbl_80345C58;
            lo[1] = lbl_80345C58;
            lo[2] = lbl_80345C58;
        }
        if (p4 != 0) {
            hi[0] = p4[0];
            hi[1] = p4[1];
            hi[2] = p4[2];
        } else {
            hi[0] = lbl_80345BA4;
            hi[1] = lbl_80345BA4;
            hi[2] = lbl_80345BA4;
        }
        for (i = 0; i < 3; i++) {
            f32 b = bpos[i];
            f32 h = hi[i];
            f32 l = lo[i];
            if (h >= b) h = b;
            hi[i] = h;
            if (l <= b) l = b;
            lo[i] = l;
            out[i] = lbl_80345BA8 * (hi[i] + lo[i]);
        }
    }
}
/* Averaged view/aim vector (0x4B4, atan2 + sin/cos).  Parked: large fp body. */
static void GetActualAvgVec(f32* out) { (void)out; }

void GameCameraInit(void) {
    gGameCamera = gGameCameraData;
    lbl_803443A8 = 0;
    CamReset(gGameCamera);
    gCameraWindowScaleX = lbl_80345C28;
    gCameraWindowScaleY = lbl_80345C28;
}

/* Shared scalar camera-value clamp.  Advance `value` toward `target` with a
 * bounded velocity and acceleration, optionally treating the values as
 * wrapped angles. */
static f32 LimitCamVal2(f32 value, f32 target, f32 minVelocity,
                        f32 maxVelocity, f32 acceleration, f32 stopScale,
                        f32* velocity, s32 wrapAngle) {
    u8 unusedHigh[16];
    f32 delta;
    f32 accelerationStep;
    f32 absDelta;
    f32 oldVelocity;
    f32 absVelocity;
    f32 candidate;
    f64 wrapped;
    u8 unusedLow[8];

    delta = target - value;
    accelerationStep = acceleration * gClockFrameStep;
    if (wrapAngle != 0) {
        if ((f64)delta > lbl_80345B88) {
            wrapped = (f64)delta - lbl_80345BB8;
        } else if ((f64)delta <= lbl_80345BC0) {
            wrapped = lbl_80345BB8 + (f64)delta;
        } else {
            wrapped = delta;
        }
        delta = (f32)wrapped;
    }

    target = stopScale * maxVelocity;
    absDelta = delta;
    *(u32*)&absDelta &= 0x7FFFFFFF;
    oldVelocity = *velocity;
    absVelocity = oldVelocity;
    *(u32*)&absVelocity &= 0x7FFFFFFF;

    if ((f64)stopScale > lbl_80345B98 &&
        absVelocity < gClockFrameStep * target &&
        absDelta < gClockFrameStep * target) {
        minVelocity = lbl_80345BA0;
    } else if (absDelta <
               gClockFrameStep * (absVelocity + accelerationStep)) {
        minVelocity = delta * gClockFrameReciprocal;
    } else {
        if (absVelocity / acceleration >= absDelta / absVelocity) {
            if ((f64)oldVelocity > lbl_80345B98) {
                candidate = oldVelocity - accelerationStep;
                if ((f64)candidate < lbl_80345B98) {
                    candidate = lbl_80345BA0;
                }
            } else {
                candidate = oldVelocity + accelerationStep;
                if ((f64)candidate > lbl_80345B98) {
                    candidate = lbl_80345BA0;
                }
            }
        } else if ((f64)delta > lbl_80345B98) {
            candidate = oldVelocity + accelerationStep;
        } else {
            candidate = oldVelocity - accelerationStep;
        }

        if (!(candidate < minVelocity)) {
            if (!(candidate > maxVelocity)) {
                maxVelocity = candidate;
            }
            minVelocity = maxVelocity;
        }
    }

    *velocity = minVelocity;
    return minVelocity * gClockFrameStep + value;
}

/* Keep the static helpers referenced so -Wall stubs don't warn them away and
 * the intended call-graph is documented in one place. */
void bosscam_unused_refs(void) {
    f32 v = 0.0f;
    BossCamBossCalc();
    BossCamPlayerCalc();
    BossCamLimitAttn(&v);
    BossCameraStart();
    (void)GetPlayerViewDist(&v);
    GetBossAvgPos(&v, 0.0f, &v, &v, 0);
    GetActualAvgVec(&v);
    (void)LimitCamVal2(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, &v, 0);
}
