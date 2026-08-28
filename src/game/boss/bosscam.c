#include "types.h"
#include "game/camera.h"

/*
 * game/boss/bosscam.c  --  BOSSCAM.OBJ (GameCube GUNE5D)
 * ---------------------------------------------------------------------------
 * Boss / game camera control TU.  .text 0x8001BC88 - 0x8001EAE0.
 * Sits between game/boss/boss.c (ends 0x8001BC88) and game/ui/btext.c
 * (starts 0x8001EAE0).  Names are the REAL Midway symbols recovered from the
 * Xbox shell3D PDB module ".\Release\BOSSCAM.OBJ"
 * (.claude/memory/xbox_symbols/functions_by_module.txt).
 *
 * The GameCube build emits 17 of the 25 PDB functions; the eight small/debug
 * helpers below were inlined or dead-stripped on GCN and have no out-of-line
 * body here:
 *     LimitCamVal, BossCamControlInputs, BossCamGetCurrent, BossCamLimit,
 *     BossCamStartCalc, BossCamSelectCalc, PointInView, GetBossCamViewDist.
 *
 * Status: NonMatching, but most of the small/medium functions are now real
 * reconstructions verified against the extracted target (fndiff):
 *   MATCHING (byte-exact):   TriggerCameraEnd, CameraLimitPlayerDpos,
 *                            BossCameraInit, BossCameraStart,
 *                            GetPlayerViewDist, GetBossAvgPos, GameCameraInit
 *   NEAR (correct logic, regalloc/scheduler/idiom residuals only):
 *       PointViewDist (real 0, pool-name noise), TriggerCamUpdate,
 *       TriggerCameraActivate, BossCameraUpdate, CamLimitPlayerDpos,
 *       LimitCamVal2, GetActualAvgVec
 *   FAITHFUL (reconstructed, NonMatching -- residual codegen ties):
 *       BossCamLimitAttn (full body; residual = LimitCamVal2 arg fmr
 *       coalescing tie + loop-body schedule tie, 238/241 insns)
 *   PARKED as documented stubs (large fp-math / giant):
 *       BossCamBossCalc (0xE24, GIANT), BossCamPlayerCalc (0x5C0)
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
extern s64 gControllerButtons;
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
extern s32 lbl_803443A8;
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
extern void CalcFrustrumNormals(f32* a, f32* b, f32* c, f32 fov);
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
    void* blit;
    f32* position;
    f32 x;
    f32 y;
    f32 z;
    s32 i;
    s32 offset;
    u8* cameraBuffer;

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
    y = *(f32*)(cameraBuffer + 52);
    gCameras[0].attn[1] = y;
    z = *(f32*)(cameraBuffer + 56);
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
            if ((gControllerButtons & 2) != 0) {
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
    f32 padTop[3];
    f32 world[3];
    f32 padMid[3];
    f32 viewpt[3];
    f32 padLow[4];
    f32 radius0, radius1, dist;
    s32 planeLR, flags;
    f32* py;
    f32* pz;
    f32 dy, dx, dz;
    f32 v0, v1, v2;

    u8* cam = (u8*)camera;

    if (lbl_80343C5C != 0) {
        CalcFrustrumNormals((f32*)(cam + 224), (f32*)(cam + 164), (f32*)(cam + 64),
                            *(f32*)(cam + 236));
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
    if (!(dist < lbl_80345B98)) return;
    if (!(dist < radius0)) return;

    flags = gPointViewClipFlags;
    if (arg == 0 || ((planeLR = flags & 0x30) != 0 && (flags & 0x300) != 0) ||
        (flags & 1) != 0) {
        dpos[0] = lbl_80345BA0;
        dpos[1] = lbl_80345BA0;
        dpos[2] = lbl_80345BA0;
    } else {
        if (flags == 0) return;
        MulBodyVecMat4((f32*)(ps + 84), viewpt, camera);
        py = (f32*)(buf + 68);
        pz = (f32*)(buf + 72);
        v0 = viewpt[0];
        dx = *(f32*)(buf + 64) - v0;
        v1 = viewpt[1];
        dy = *py - v1;
        v2 = viewpt[2];
        dz = *pz - v2;
        if (planeLR != 0) {
            dx = lbl_80345BA0;
        } else if ((flags & 0x300) != 0) {
            dy = lbl_80345BA0;
        }
        if (dz < lbl_80345BA0 && (flags & 0x230) != 0) {
            dz = lbl_80345BA0;
        } else if (dz > *(volatile f32*)&lbl_80345BA0 && (flags & 0x130) != 0) {
            dz = lbl_80345BA0;
        }
        *(f32*)(buf + 64) = v0 + dx;
        *py = v1 + dy;
        *pz = v2 + dz;
        MulVecMat4((f32*)(buf + 64), world, camera);
        dpos[0] = world[0] - *(f32*)(ps + 84);
        dpos[1] = world[1] - *(f32*)(ps + 88);
        dpos[2] = world[2] - *(f32*)(ps + 92);
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
 * LookInDirection pose. */
extern s32 gGameMode;
extern s32 gBossActive;
extern u8* CurTransmitter;
extern s32 lbl_803447B8;              /* scripted-camera gate */
extern s32 gScriptedCameraState;
extern s32 lbl_8034453C;
static void BossCamBossCalc(void);
static void BossCamPlayerCalc(void);
static void BossCameraStart(void);
extern void FatalError(char* fmt, u32 code);
extern f32 GetPlayerAvgPos(f32* out, void* a, f32* b, s32 c);
extern void GetYawPitch(f32* dir, f32* yaw, f32* pitch);
extern void DoShake(f32* pos, f32* attn);
extern void ExtractYPR(void* mtx, f32* pyr);
extern void dbgTextPrintfCell(s32 color, s32 a, s32 b, char* fmt, ...);
extern f64 __frsqrte(f64 x);
extern const f64 lbl_80345BB0;        /* 3.0 */
extern const f64 lbl_80345BC8;        /* rad->deg scale A */
extern const f64 lbl_80345BD0;        /* rad->deg scale B */
extern char lbl_801117B8[];           /* "BossCamStartCalc called with no b..." */
extern char lbl_801117E0[];           /* "BCAM Y=%.0f P=%.0f D=%.2f ..." */

#pragma opt_propagation off
s32 BossCameraUpdate(void) {
    f32 pitch;
    f32 yaw;
    f32 d2;
    u8 anglePad[4];
    f32 dir[3];
    f32 avg[3];
    volatile f32 tmp;
    f32* wp;

    if (lbl_803447B8 == 0) {
        gScriptedCameraState = 0;
    }
    gCameraWindowScaleY = lbl_80345BA0;
    if (lbl_803443A8 == 0) {
        BossCameraStart();
    }
    if (gGameMode == 0x4013 || gGameMode == 0x400D || gGameMode == 0x4017) {
        if (lbl_803443A8 == 0) {
            lbl_803443A8 = 1;
        }
    } else {
        if (lbl_803443D0 == 0) {
            return 0;
        }
        if (lbl_803443A8 == 0) {
            if (gBossObj == 0) {
                FatalError(lbl_801117B8, 0x800000);
            }
            wp = (f32*)CurTransmitter;
            lbl_803447B8 = 1;
            GetPlayerAvgPos(avg, 0, 0, 1);
            dir[0] = avg[0] - wp[1];
            dir[1] = avg[1] - wp[2];
            dir[2] = avg[2] - wp[3];
            d2 = dir[2] * dir[2] + (d2 = dir[0] * dir[0] + dir[1] * dir[1]);
            if (d2 > lbl_80345BA0) {
                f64 y = __frsqrte(d2);
                y = lbl_80345BA8 * y * (lbl_80345BB0 - y * y * d2);
                y = lbl_80345BA8 * y * (lbl_80345BB0 - y * y * d2);
                y = lbl_80345BA8 * y * (lbl_80345BB0 - y * y * d2);
                tmp = (f32)(d2 * (lbl_80345BA8 * y * (lbl_80345BB0 - y * y * d2)));
                d2 = tmp;
            }
            GetYawPitch(dir, &yaw, &pitch);
            *(f32*)((u8*)gGameCamera + 164) = avg[0];
            *(f32*)((u8*)gGameCamera + 168) = avg[1];
            *(f32*)((u8*)gGameCamera + 172) = avg[2];
            *(f32*)((u8*)gGameCamera + 176) = avg[0];
            *(f32*)((u8*)gGameCamera + 180) = avg[1];
            *(f32*)((u8*)gGameCamera + 184) = avg[2];
            *(f32*)((u8*)gGameCamera + 236) = yaw;
            *(f32*)((u8*)gGameCamera + 260) = pitch;
            *(f32*)((u8*)gGameCamera + 244) = d2;
        } else if (*(void**)((u8*)gCurLevel + 108) != 0) {
            if (gBossActive == 0) {
                BossCamPlayerCalc();
            } else {
                BossCamBossCalc();
            }
        }
    }

    {
        f64 a;
        wp = (f32*)((u8*)gGameCamera + 236);
        a = *wp;
        if (a > lbl_80345B88) {
            a = a - lbl_80345BB8;
        } else if (a <= lbl_80345BC0) {
            a = lbl_80345BB8 + a;
        }
        *wp = a;
    }
    {
        f64 a;
        wp = (f32*)((u8*)gGameCamera + 260);
        a = *wp;
        if (a > lbl_80345B88) {
            a = a - lbl_80345BB8;
        } else if (a <= lbl_80345BC0) {
            a = lbl_80345BB8 + a;
        }
        *wp = a;
    }

    YawVec3(lbl_80127D40, (f32*)((u8*)gGameCamera + 224),
            -*(f32*)((u8*)gGameCamera + 236));
    PitchVec3((f32*)((u8*)gGameCamera + 224), (f32*)((u8*)gGameCamera + 224),
              -*(f32*)((u8*)gGameCamera + 260));
    DoShake((f32*)((u8*)gGameCamera + 48), (f32*)((u8*)gGameCamera + 164));
    *(f32*)((u8*)gGameCamera + 48) =
        *(f32*)((u8*)gGameCamera + 224) * -*(f32*)((u8*)gGameCamera + 244) +
        *(f32*)((u8*)gGameCamera + 164);
    *(f32*)((u8*)gGameCamera + 52) =
        *(f32*)((u8*)gGameCamera + 228) * -*(f32*)((u8*)gGameCamera + 244) +
        *(f32*)((u8*)gGameCamera + 168);
    *(f32*)((u8*)gGameCamera + 56) =
        *(f32*)((u8*)gGameCamera + 232) * -*(f32*)((u8*)gGameCamera + 244) +
        *(f32*)((u8*)gGameCamera + 172);
    LookInDirection((f32*)((u8*)gGameCamera + 224), gGameCamera);

    lbl_8034453C = 0;
    gCameras[0].attn[0] = *(f32*)((u8*)gGameCamera + 164);
    gCameras[0].attn[1] = *(f32*)((u8*)gGameCamera + 168);
    gCameras[0].attn[2] = *(f32*)((u8*)gGameCamera + 172);
    gCameras[0].wpos[0] = *(f32*)((u8*)gGameCamera + 48);
    gCameras[0].wpos[1] = *(f32*)((u8*)gGameCamera + 52);
    gCameras[0].wpos[2] = *(f32*)((u8*)gGameCamera + 56);
    ExtractYPR(gGameCamera, gCameras[0].pyr);
    wp = &gCameras[0].pyr[1];
    gCameras[0].pyr[1] = gCameras[0].pyr[1] + lbl_80345B88;
    {
        f64 a;
        a = *wp;
        if (a > lbl_80345B88) {
            a = a - lbl_80345BB8;
        } else if (a <= lbl_80345BC0) {
            a = lbl_80345BB8 + a;
        }
        *wp = a;
    }
    MBCameraUpdate((f32*)((u8*)gGameCamera + 48), (f32*)gGameCamera);
    lbl_803443A8 = 1;
    if ((gControllerButtons & 1) != 0 &&
        (gControllerButtons & 16) != 0) {
        dbgTextPrintfCell(0xFFFF00, 1, 0x20, lbl_801117E0,
                          lbl_80345BC8 * (lbl_80345BD0 *
                              *(f32*)((u8*)gGameCamera + 236)),
                          lbl_80345BC8 * (lbl_80345BD0 *
                              *(f32*)((u8*)gGameCamera + 260)),
                          (f64)*(f32*)((u8*)gGameCamera + 244),
                          (f64)*(f32*)((u8*)gGameCamera + 256),
                          (f64)*(f32*)((u8*)gGameCamera + 164),
                          (f64)*(f32*)((u8*)gGameCamera + 168),
                          (f64)*(f32*)((u8*)gGameCamera + 172));
    }
    return 1;
}
#pragma opt_propagation reset

/* Boss-follow camera solve (0xE24).  Select an attention point from the
 * current boss/effect state, choose its horizontal viewing direction, keep
 * the boss and players inside the camera frustum, then either snap or rate
 * limit the active camera pose. */
static f32 GetPlayerViewDist(void* mtx);
static void GetBossAvgPos(f32* out, f32 t, f32* p4, f32* p5, s32 mode);
static f32 GetActualAvgVec(f32* out, f32* pos, s32 useBoss);
static void BossCamLimitAttn(f32* target);
static f32 LimitCamVal2(f32 value, f32 target, f32 minVelocity,
                        f32 maxVelocity, f32 acceleration, f32 stopScale,
                        f32* velocity, s32 wrapAngle);
extern f64 NormalVector2D(f32* vector);
extern f64 SlowNormalVector2D(f32* vector);
extern f32 atan2(f32 y, f32 x);
extern u8 lbl_8023E558[0xB0];
extern u8 Effects[];
extern s32 good_wiz_state;
extern s32 gBossKeyBlit;
extern const f32 lbl_80343B98;
extern const f32 lbl_80343B9C;
extern f32 lbl_80343B88;
extern f32 lbl_80343B8C;
extern f32 lbl_80343B90;
extern f32 lbl_80343B94;
extern const f32 lbl_80345BD8;
extern const f32 lbl_80345BDC;
extern const f32 lbl_80345BE8;
extern const f64 lbl_80345BF0;
extern const f64 lbl_80345BF8;
extern const f64 lbl_80345C00;
extern const f64 lbl_80345C08;
extern const f64 lbl_80345C10;
extern const f64 lbl_80345C18;
extern const f64 lbl_80345C20;
extern const f64 lbl_80345C30;
extern f32 lbl_80345C2C;
extern char lbl_80111818[];
extern char lbl_80111838[];

static void BossCamBossCalc(void)
{
    f32 pavg[3];
    f32 bossmat[3];
    f32 newattn[3];
    f32 dist;
    f32 bossrad;
    f32 yaw;
    f32 npavg[3];
    f32 pitch;
    f32 dvec[3];
    f32 attn[3];
    f32 dattn[3];
    f32 tmax[3];
    f32 tmin[3];
    f32 tmax2[3];
    f32 tmin2[3];
    f32 maxdist;
    volatile f32 rootslot;
    f32 length;
    f32 viewdist;
    f32 candidate;
    f32 dot;
    f32 side;
    f32 weight;
    f32* focus;
    f32* source;
    f32* cam;
    u8* cfg;
    u8* boss;
    u8* effect;
    u32 flags;
    s32 selectMode;
    s32 transition;
    s32 i;
    u8 unusedFrame[80];

    lbl_803447B8 = 0;
    boss = gBossObj;
    cfg = lbl_803443D0;
    cam = (f32*)gGameCamera;
    transition = 0;
    *(f32*)((u8*)cam + 220) = *(f32*)(cfg + 20);
    if (lbl_80343C5C != 0) {
        *(f32*)((u8*)cam + 220) =
            *(f32*)((u8*)cam + 220) * lbl_80343B84;
    }

    for (i = 0; i < 3; i++) {
        pavg[i] = *(f32*)((u8*)cam + 252) *
                      (*(f32*)(cfg + 48 + i * 4) -
                       *(f32*)(cfg + 36 + i * 4)) +
                  *(f32*)(cfg + 36 + i * 4);
    }

    if (lbl_803443A8 != 0) {
        selectMode = 1;
        if (good_wiz_state != 0 && *(void**)(lbl_8023E558 + 96) != NULL) {
            focus = (f32*)(lbl_8023E558 + 64);
            selectMode = 2;
            bossrad = lbl_80345BD8;
            newattn[0] = focus[0] + *(f32*)(cfg + 72);
            newattn[1] = focus[1] + *(f32*)(cfg + 76);
            newattn[2] = focus[2] + *(f32*)(cfg + 80);
        } else if (gBossKeyBlit >= 0 &&
                   *(void**)(Effects + gBossKeyBlit * 240 + 20) != NULL) {
            effect = *(u8**)(Effects + gBossKeyBlit * 240 + 20);
            focus = (f32*)(effect + 48);
            selectMode = 2;
            bossrad = lbl_80345BDC;
            newattn[0] = focus[0] + *(f32*)(cfg + 60);
            newattn[1] = focus[1] + *(f32*)(cfg + 64);
            newattn[2] = focus[2] + *(f32*)(cfg + 68);
        } else if (good_wiz_state != 0 || gBossDying != 0) {
            focus = newattn;
            bossrad = lbl_80345BDC;
            newattn[0] = *(f32*)((u8*)cam + 164);
            newattn[1] = *(f32*)((u8*)cam + 168);
            newattn[2] = *(f32*)((u8*)cam + 172);
        } else if (boss != NULL && *(void**)(boss + 4) != NULL) {
            if ((*(u32*)cfg & 0x10) != 0) {
                weight = GetPlayerAvgPos(attn, tmax, tmin, 1);
                GetBossAvgPos(attn, weight, tmax, tmin, 1);
                source = attn;
            } else if ((*(u32*)cfg & 1) != 0) {
                source = (f32*)(boss + 76);
            } else {
                source = (f32*)(boss + 1048);
            }
            newattn[0] = source[0] + pavg[0];
            newattn[1] = source[1] + pavg[1];
            newattn[2] = source[2] + pavg[2];
            focus = (f32*)(boss + 76);
            bossrad = *(f32*)(*(u8**)(boss + 4) + 120);
        } else {
            focus = newattn;
            bossrad = lbl_80345BDC;
            newattn[0] = *(f32*)(boss + 1048) + pavg[0];
            newattn[1] = *(f32*)(boss + 1052) + pavg[1];
            newattn[2] = *(f32*)(boss + 1056) + pavg[2];
        }

        dvec[0] = *(f32*)((u8*)cam + 164) - newattn[0];
        dvec[1] = *(f32*)((u8*)cam + 168) - newattn[1];
        dvec[2] = *(f32*)((u8*)cam + 172) - newattn[2];
        length = dvec[0] * dvec[0];
        length = dvec[1] * dvec[1] + length;
        length = dvec[2] * dvec[2] + length;
        if (length > lbl_80345BA0) {
            f64 estimate = __frsqrte(length);
            estimate = lbl_80345BA8 * estimate *
                       (lbl_80345BB0 - estimate * estimate * length);
            estimate = lbl_80345BA8 * estimate *
                       (lbl_80345BB0 - estimate * estimate * length);
            estimate = lbl_80345BA8 * estimate *
                       (lbl_80345BB0 - estimate * estimate * length);
            rootslot = (f32)(length *
                             (lbl_80345BA8 * estimate *
                              (lbl_80345BB0 - estimate * estimate * length)));
            length = rootslot;
        }
        if ((f64)gClockFrameStep > lbl_80345B98 &&
            length > lbl_80343B98 * gClockFrameStep) {
            transition = selectMode;
        }
    } else {
        focus = (f32*)(boss + 76);
        newattn[0] = *(f32*)(boss + 1048) + pavg[0];
        newattn[1] = *(f32*)(boss + 1052) + pavg[1];
        newattn[2] = *(f32*)(boss + 1056) + pavg[2];
        bossrad = *(f32*)(*(u8**)(boss + 4) + 120);
    }

    flags = *(u32*)cfg;
    if ((flags & 0x20) != 0) {
        weight = GetPlayerAvgPos(bossmat, tmax2, tmin2, 1);
        if ((flags & 8) != 0) {
            GetBossAvgPos(bossmat, weight, tmax2, tmin2, 1);
        }
        bossmat[0] = *(f32*)((u8*)cam + 164) - bossmat[0];
        bossmat[1] = *(f32*)((u8*)cam + 168) - bossmat[1];
        bossmat[2] = *(f32*)((u8*)cam + 172) - bossmat[2];
        if (NormalVector2D(bossmat) < lbl_80345BE0) {
            weight = lbl_80345BA0;
        }
    } else {
        weight = GetActualAvgVec(bossmat, (f32*)((u8*)cam + 164),
                                 flags & 8);
    }

    if ((f64)weight == lbl_80345B98) {
        yaw = *(f32*)((u8*)cam + 236);
    } else {
        if (lbl_803443A8 != 0) {
            dvec[0] = *(f32*)((u8*)cam + 48) -
                      *(f32*)((u8*)cam + 164);
            dvec[1] = lbl_80345BA0;
            dvec[2] = *(f32*)((u8*)cam + 56) -
                      *(f32*)((u8*)cam + 172);
            SlowNormalVector2D(dvec);
            if ((flags & 4) == 0) {
                dot = bossmat[2] * *(f32*)(boss + 1024) +
                      bossmat[0] * *(f32*)(boss + 1016);
                if (dot >= *(f32*)(cfg + 8)) {
                    side = dvec[2] * bossmat[2] + dvec[0] * bossmat[0];
                } else {
                    side = lbl_80345BE8;
                }
            } else {
                dot = lbl_80345BE8;
                side = dot;
            }
            if ((flags & 2) == 0) {
                npavg[0] = (f32)(lbl_80345BF0 * bossmat[0]);
                npavg[1] = (f32)(lbl_80345BF0 * bossmat[1]);
                npavg[2] = (f32)(lbl_80345BF0 * bossmat[2]);
                candidate = npavg[2] * *(f32*)(boss + 1024) +
                            npavg[0] * *(f32*)(boss + 1016);
                if (candidate >= *(f32*)(cfg + 8)) {
                    length = dvec[2] * npavg[2] + dvec[0] * npavg[0];
                } else {
                    length = lbl_80345BE8;
                }
                if (length > side) {
                    bossmat[0] = npavg[0];
                    bossmat[1] = npavg[1];
                    bossmat[2] = npavg[2];
                    dot = candidate;
                }
            }
        } else {
            dot = bossmat[2] * *(f32*)(boss + 1024) +
                  bossmat[0] * *(f32*)(boss + 1016);
        }

        if (dot < *(f32*)(cfg + 8)) {
            yaw = atan2(*(f32*)(boss + 1024), *(f32*)(boss + 1016));
            side = bossmat[0] * *(f32*)(boss + 1024) -
                   bossmat[2] * *(f32*)(boss + 1016);
            if ((f64)side >= lbl_80345B98) {
                yaw -= *(f32*)(cfg + 4);
            } else {
                yaw += *(f32*)(cfg + 4);
            }
            yaw = (f32)(yaw + lbl_80345B88);
            if ((f64)yaw > lbl_80345B88) {
                yaw = (f32)(yaw - lbl_80345BB8);
            } else if ((f64)yaw <= lbl_80345BC0) {
                yaw = (f32)(lbl_80345BB8 + yaw);
            }
        } else {
            yaw = atan2(bossmat[2], bossmat[0]);
            yaw = (f32)(yaw + lbl_80345B88);
            if ((f64)yaw > lbl_80345B88) {
                yaw = (f32)(yaw - lbl_80345BB8);
            } else if ((f64)yaw <= lbl_80345BC0) {
                yaw = (f32)(lbl_80345BB8 + yaw);
            }
        }
    }

    if (lbl_803443A8 == 0) {
        dvec[0] = newattn[0] - *(f32*)((u8*)lbl_80344EE8 + 148);
        dvec[1] = newattn[1] - *(f32*)((u8*)lbl_80344EE8 + 152);
        dvec[2] = newattn[2] - *(f32*)((u8*)lbl_80344EE8 + 156);
        bossrad = dvec[0] * dvec[0];
        bossrad = dvec[1] * dvec[1] + bossrad;
        bossrad = dvec[2] * dvec[2] + bossrad;
        if (bossrad > lbl_80345BA0) {
            f64 estimate = __frsqrte(bossrad);
            estimate = lbl_80345BA8 * estimate *
                       (lbl_80345BB0 - estimate * estimate * bossrad);
            estimate = lbl_80345BA8 * estimate *
                       (lbl_80345BB0 - estimate * estimate * bossrad);
            estimate = lbl_80345BA8 * estimate *
                       (lbl_80345BB0 - estimate * estimate * bossrad);
            rootslot = (f32)(bossrad *
                             (lbl_80345BA8 * estimate *
                              (lbl_80345BB0 - estimate * estimate * bossrad)));
            bossrad = rootslot;
        }
        *(f32*)((u8*)cam + 256) = lbl_80345BA0;
    } else {
        flags &= ~0x300;
        maxdist = *(f32*)(cfg + 20);
        viewdist = GetPlayerViewDist(cam);
        MulBodyVecMat4(focus, lbl_8023E8C0, cam);
        candidate = PointViewDist(lbl_8023E8C0, bossrad);
        if (candidate < viewdist) {
            viewdist = candidate;
        }
        *(f32*)((u8*)cam + 256) = viewdist;
        if (good_wiz_state != 0) {
            maxdist = (f32)(maxdist * lbl_80345BF8);
        }
        candidate = *(f32*)((u8*)cam + 244);
        if (viewdist < lbl_80345BA0) {
            flags |= 0x200;
            candidate = (f32)(candidate + lbl_80345C00);
        } else if ((f64)viewdist < lbl_80345BF8 && candidate < maxdist) {
            flags |= 0x200;
            candidate = (f32)(lbl_80345BF8 *
                              (lbl_80345C08 - viewdist) + candidate);
        } else if ((f64)viewdist < lbl_80345C10 && candidate < maxdist &&
                   (*(u32*)cfg & 0x200) != 0) {
            flags |= 0x200;
            candidate = (f32)(lbl_80345BF8 *
                              (lbl_80345C08 - viewdist) + candidate);
        } else if ((f64)viewdist > lbl_80345C18) {
            flags |= 0x100;
            candidate = (f32)(candidate - (viewdist - lbl_80345C08));
        } else if ((f64)viewdist > lbl_80345C08 &&
                   (*(u32*)cfg & 0x100) != 0) {
            flags |= 0x100;
            candidate = (f32)(candidate - (viewdist - lbl_80345C08));
        }
        *(u32*)cfg = flags;
        if (candidate < *(f32*)(cfg + 12)) {
            bossrad = *(f32*)(cfg + 12);
        } else if (candidate > lbl_80345BF8 * maxdist) {
            bossrad = (f32)(lbl_80345BF8 * maxdist);
        } else {
            bossrad = candidate;
        }
    }

    dist = *(f32*)(cfg + 20) - *(f32*)(cfg + 12);
    candidate = *(f32*)((u8*)cam + 244) - bossrad;
    if ((f64)dist > lbl_80345C20) {
        *(f32*)((u8*)cam + 252) =
            (*(f32*)((u8*)cam + 244) - *(f32*)(cfg + 12)) / dist;
    } else {
        *(f32*)((u8*)cam + 252) = lbl_80345C28;
    }
    if ((f64)*(f32*)((u8*)cam + 252) < lbl_80345B98) {
        *(f32*)((u8*)cam + 252) = lbl_80345BA0;
    } else if ((f64)*(f32*)((u8*)cam + 252) > lbl_80345BE0) {
        *(f32*)((u8*)cam + 252) = (f32)lbl_80345BE0;
    }
    dist = candidate;
    *(u32*)&dist &= 0x7FFFFFFF;
    if ((f64)dist < lbl_80345C00) {
        pitch = -(*(f32*)((u8*)cam + 252) *
                      (*(f32*)(cfg + 32) - *(f32*)(cfg + 28)) +
                  *(f32*)(cfg + 28));
    } else {
        pitch = *(f32*)((u8*)cam + 260);
    }

    if (lbl_803443A8 == 0) {
        *(f32*)((u8*)gGameCamera + 164) = newattn[0];
        *(f32*)((u8*)gGameCamera + 168) = newattn[1];
        *(f32*)((u8*)gGameCamera + 172) = newattn[2];
        *(f32*)((u8*)gGameCamera + 176) = newattn[0];
        *(f32*)((u8*)gGameCamera + 180) = newattn[1];
        *(f32*)((u8*)gGameCamera + 184) = newattn[2];
        dattn[0] = *(f32*)((u8*)gGameCamera + 164) -
                   *(f32*)((u8*)lbl_80344EE8 + 148);
        dattn[1] = *(f32*)((u8*)gGameCamera + 168) -
                   *(f32*)((u8*)lbl_80344EE8 + 152);
        dattn[2] = *(f32*)((u8*)gGameCamera + 172) -
                   *(f32*)((u8*)lbl_80344EE8 + 156);
        length = dattn[0] * dattn[0];
        length = dattn[1] * dattn[1] + length;
        length = dattn[2] * dattn[2] + length;
        if (length > lbl_80345BA0) {
            f64 estimate = __frsqrte(length);
            estimate = lbl_80345BA8 * estimate *
                       (lbl_80345BB0 - estimate * estimate * length);
            estimate = lbl_80345BA8 * estimate *
                       (lbl_80345BB0 - estimate * estimate * length);
            estimate = lbl_80345BA8 * estimate *
                       (lbl_80345BB0 - estimate * estimate * length);
            rootslot = (f32)(length *
                             (lbl_80345BA8 * estimate *
                              (lbl_80345BB0 - estimate * estimate * length)));
            length = rootslot;
        }
        *(f32*)((u8*)gGameCamera + 244) = length;
        GetYawPitch(dattn, (f32*)((u8*)gGameCamera + 236),
                    (f32*)((u8*)gGameCamera + 260));
    } else if (transition != 0) {
        BossCamLimitAttn(newattn);
        dvec[0] = *(f32*)((u8*)gGameCamera + 164) -
                  *(f32*)((u8*)gGameCamera + 48);
        dvec[1] = *(f32*)((u8*)gGameCamera + 168) -
                  *(f32*)((u8*)gGameCamera + 52);
        dvec[2] = *(f32*)((u8*)gGameCamera + 172) -
                  *(f32*)((u8*)gGameCamera + 56);
        length = dvec[0] * dvec[0];
        length = dvec[1] * dvec[1] + length;
        length = dvec[2] * dvec[2] + length;
        if (length > lbl_80345BA0) {
            f64 estimate = __frsqrte(length);
            estimate = lbl_80345BA8 * estimate *
                       (lbl_80345BB0 - estimate * estimate * length);
            estimate = lbl_80345BA8 * estimate *
                       (lbl_80345BB0 - estimate * estimate * length);
            estimate = lbl_80345BA8 * estimate *
                       (lbl_80345BB0 - estimate * estimate * length);
            rootslot = (f32)(length *
                             (lbl_80345BA8 * estimate *
                              (lbl_80345BB0 - estimate * estimate * length)));
            length = rootslot;
        }
        GetYawPitch(dvec, &yaw, &pitch);
        *(f32*)((u8*)gGameCamera + 236) = yaw;
        *(f32*)((u8*)gGameCamera + 260) = pitch;
        if (length > *(f32*)((u8*)gGameCamera + 244) || transition >= 2) {
            *(f32*)((u8*)gGameCamera + 244) = length;
        }
        if ((gControllerButtons & 1) != 0 &&
            (gControllerButtons & 16) != 0) {
            dbgTextPrintfCell(0x00FFFF00, 1, 33, lbl_80111818,
                              lbl_80345BC8 * (lbl_80345BD0 * yaw), length,
                              lbl_80345BC8 * (lbl_80345BD0 * pitch));
        }
    } else {
        BossCamLimitAttn(newattn);
        *(f32*)((u8*)gGameCamera + 236) = LimitCamVal2(
            *(f32*)((u8*)gGameCamera + 236), yaw, -lbl_80343B88,
            lbl_80343B88, lbl_80343B8C, lbl_80345C2C,
            (f32*)((u8*)gGameCamera + 240), 1);
        *(f32*)((u8*)gGameCamera + 244) = LimitCamVal2(
            *(f32*)((u8*)gGameCamera + 244), bossrad,
            (f32)(lbl_80345C30 * -lbl_80343B98), lbl_80343B98,
            lbl_80343B9C, lbl_80345C2C,
            (f32*)((u8*)gGameCamera + 248), 0);
        *(f32*)((u8*)gGameCamera + 260) = LimitCamVal2(
            *(f32*)((u8*)gGameCamera + 260), pitch, -lbl_80343B90,
            lbl_80343B90, lbl_80343B94, lbl_80345C2C,
            (f32*)((u8*)gGameCamera + 264), 1);
        if ((gControllerButtons & 1) != 0 &&
            (gControllerButtons & 16) != 0) {
            dbgTextPrintfCell(0x00FFFF00, 1, 33, lbl_80111838,
                              lbl_80345BC8 * (lbl_80345BD0 * yaw), bossrad,
                              lbl_80345BC8 * (lbl_80345BD0 * pitch));
        }
    }
}

/* Player-follow camera solve (0x5C0).  Parked: large fp-math body sharing
 * LimitCamVal2 and PointViewDist. */
static f32 LimitCamVal2(f32 value, f32 target, f32 minVelocity,
                        f32 maxVelocity, f32 acceleration, f32 stopScale,
                        f32* velocity, s32 wrapAngle);
extern u8* fn_8006FBAC(f32* pos);
extern s32 lbl_803444E0;
extern const f64 lbl_80345BF8;
extern const f64 lbl_80345C00;
extern const f64 lbl_80345C08;
extern const f64 lbl_80345C10;
extern f64 lbl_80345C38;
extern const f64 lbl_80345C30;
extern const f32 lbl_80343B98;
extern const f32 lbl_80343B9C;
extern f32 lbl_80343B88;
extern f32 lbl_80343B8C;
extern f32 lbl_80343B90;
extern f32 lbl_80343B94;
extern f32 lbl_80345C2C;
extern char lbl_80111838[];

static void BossCamPlayerCalc(void)
{
    f32 avg[3];
    f32 tpos[3];
    f32 d[3];
    f32 rootslot;
    f32 absd;
    f32 range;
    f32 tyaw;
    f32 tpitch;
    f32 minview;
    f32 pdist;
    u8* tr;
    u8* cam;
    u8* p;
    u32 fl;
    s32 i;
    s32 off;
    f64 t;

    lbl_803444E0 = 0;
    range = (f32)(lbl_80345B90 * *(f32*)((u8*)lbl_803443D0 + 24));
    *(f32*)((u8*)gGameCamera + 220) = range;
    if (lbl_80343C5C != 0) {
        *(f32*)((u8*)gGameCamera + 220) =
            *(f32*)((u8*)gGameCamera + 220) * lbl_80343B84;
    }
    GetPlayerAvgPos(avg, 0, 0, 2);
    if (lbl_803447B8 != 0) {
        tr = CurTransmitter;
    } else {
        tr = fn_8006FBAC(avg);
    }
    if (tr != NULL) {
        f32 y = (f32)(*(f32*)(tr + 24) - lbl_80345B88);
        if (y > lbl_80345B88) {
            tyaw = (f32)(y - lbl_80345BB8);
        } else if (y <= lbl_80345BC0) {
            tyaw = (f32)(lbl_80345BB8 + y);
        } else {
            tyaw = y;
        }
    } else {
        tyaw = lbl_80345BA0;
    }
    if (tr != NULL) {
        tpitch = -*(f32*)(tr + 20);
    } else {
        tpitch = lbl_80345BA0;
    }
    fl = *(u32*)lbl_803443D0 & ~0x300;
    minview = lbl_80345BA4;
    for (i = 0, off = 0; i < 4; i++, off += 13148) {
        p = gPlayers + off;
        if (*(s32*)(p + 232) != 1) {
            continue;
        }
        tpos[0] = *(f32*)(p + 84) + *(f32*)(p + 2184);
        tpos[1] = *(f32*)(p + 88) + *(f32*)(p + 2188);
        tpos[2] = *(f32*)(p + 92) + *(f32*)(p + 2192);
        pdist = *(f32*)(p + 2132);
        MulBodyVecMat4(tpos, lbl_8023E8C0, gGameCamera);
        pdist = PointViewDist(lbl_8023E8C0, pdist);
        if (pdist < minview) {
            minview = pdist;
        }
    }
    *(f32*)((u8*)gGameCamera + 256) = minview;
    {
        f32 vd = *(f32*)((u8*)gGameCamera + 256);
        f32 r3v = *(f32*)((u8*)gGameCamera + 244);
        if (vd < lbl_80345BA0) {
            fl |= 512;
            r3v = (f32)(r3v + lbl_80345C00);
        } else if (vd < lbl_80345BF8 && r3v < range) {
            fl |= 512;
            r3v = (f32)(lbl_80345BF8 * (lbl_80345C08 - vd) + r3v);
        } else if (vd < lbl_80345C10 && r3v < range &&
                   (*(u32*)lbl_803443D0 & 0x200)) {
            fl |= 512;
            r3v = (f32)(lbl_80345BF8 * (lbl_80345C08 - vd) + r3v);
        } else if (vd > lbl_80345C08) {
            fl |= 256;
            r3v = (f32)(r3v - lbl_80345BF8 * (vd - lbl_80345C08));
        } else if (vd > lbl_80345C10 && (*(u32*)lbl_803443D0 & 0x100)) {
            fl |= 256;
            r3v = (f32)(r3v - lbl_80345BF8 * (vd - lbl_80345C10));
        }
        *(u32*)lbl_803443D0 = fl;
        if (r3v < *(f32*)((u8*)lbl_803443D0 + 16)) {
            t = *(f32*)((u8*)lbl_803443D0 + 16);
        } else if (r3v > lbl_80345BF8 * range) {
            t = lbl_80345BF8 * range;
        } else {
            t = r3v;
        }
        range = (f32)t;
    }
    if (lbl_803447B8 != 0) {
        absd = range - *(f32*)((u8*)gGameCamera + 244);
        *(u32*)&absd &= 0x7FFFFFFF;
        if ((f64)absd < lbl_80345C38) {
            lbl_803447B8 = 0;
        }
    }
    if (lbl_803443A8 == 0) {
        f32 d2;
        *(f32*)((u8*)gGameCamera + 164) = avg[0];
        *(f32*)((u8*)gGameCamera + 168) = avg[1];
        *(f32*)((u8*)gGameCamera + 172) = avg[2];
        *(f32*)((u8*)gGameCamera + 176) = avg[0];
        *(f32*)((u8*)gGameCamera + 180) = avg[1];
        *(f32*)((u8*)gGameCamera + 184) = avg[2];
        d[0] = *(f32*)((u8*)gGameCamera + 164) - *(f32*)((u8*)lbl_80344EE8 + 148);
        d[1] = *(f32*)((u8*)gGameCamera + 168) - *(f32*)((u8*)lbl_80344EE8 + 152);
        d[2] = *(f32*)((u8*)gGameCamera + 172) - *(f32*)((u8*)lbl_80344EE8 + 156);
        d2 = d[0] * d[0];
        d2 = d[1] * d[1] + d2;
        d2 = d[2] * d[2] + d2;
        if (d2 > lbl_80345BA0) {
            f64 estimate = __frsqrte(d2);
            estimate = lbl_80345BA8 * estimate *
                       (lbl_80345BB0 - estimate * estimate * d2);
            estimate = lbl_80345BA8 * estimate *
                       (lbl_80345BB0 - estimate * estimate * d2);
            estimate = lbl_80345BA8 * estimate *
                       (lbl_80345BB0 - estimate * estimate * d2);
            rootslot = (f32)(d2 *
                             (lbl_80345BA8 * estimate *
                              (lbl_80345BB0 - estimate * estimate * d2)));
            d2 = rootslot;
        }
        *(f32*)((u8*)gGameCamera + 244) = d2;
        GetYawPitch(d, (f32*)((u8*)gGameCamera + 236),
                    (f32*)((u8*)gGameCamera + 260));
    } else {
        *(f32*)((u8*)gGameCamera + 164) = avg[0];
        *(f32*)((u8*)gGameCamera + 168) = avg[1];
        *(f32*)((u8*)gGameCamera + 172) = avg[2];
        *(f32*)((u8*)gGameCamera + 176) = avg[0];
        *(f32*)((u8*)gGameCamera + 180) = avg[1];
        *(f32*)((u8*)gGameCamera + 184) = avg[2];
        *(f32*)((u8*)gGameCamera + 236) = LimitCamVal2(
            *(f32*)((u8*)gGameCamera + 236), tyaw, -lbl_80343B88,
            lbl_80343B88, lbl_80343B8C, lbl_80345C2C,
            (f32*)((u8*)gGameCamera + 240), 1);
        *(f32*)((u8*)gGameCamera + 244) = LimitCamVal2(
            *(f32*)((u8*)gGameCamera + 244), range,
            (f32)(lbl_80345C30 * -lbl_80343B98), lbl_80343B98,
            lbl_80343B9C, lbl_80345C2C, (f32*)((u8*)gGameCamera + 248), 0);
        *(f32*)((u8*)gGameCamera + 260) = LimitCamVal2(
            *(f32*)((u8*)gGameCamera + 260), tpitch, -lbl_80343B90,
            lbl_80343B90, lbl_80343B94, lbl_80345C2C,
            (f32*)((u8*)gGameCamera + 264), 1);
        if ((gControllerButtons & 1) != 0 && (gControllerButtons & 16) != 0) {
            dbgTextPrintfCell(0x00FFFF00, 1, 33, lbl_80111838,
                              (f32)(lbl_80345BC8 * (lbl_80345BD0 * tyaw)),
                              range,
                              (f32)(lbl_80345BC8 * (lbl_80345BD0 * tpitch)));
        }
    }
}

/* Clamp the camera attention/look target + distance via LimitCamVal2 (0x3C4).
 * target points at the desired attention (look-at) position.  If the desired
 * delta is tiny the camera snaps; otherwise the approach speed (cam+212, with
 * velocity state at cam+216) and the per-axis direction (cam+188, velocity
 * state at cam+200) are rate-limited and the attention is re-aimed. */
static f32 LimitCamVal2(f32 value, f32 target, f32 minVelocity,
                        f32 maxVelocity, f32 acceleration, f32 stopScale,
                        f32* velocity, s32 wrapAngle);
extern f32 NormalVector(f32* vec);
extern s32 sprintf(char* s, const char* fmt, ...);
extern size_t strlen(const char* s);
extern const f32 lbl_80343BA0;        /* max per-axis dir speed (20.0f) */
extern const f32 lbl_80343BA4;        /* per-axis dir acceleration (20.0f) */
extern const f64 lbl_80345C40;        /* snap distance threshold (0.001) */
extern const f64 lbl_80345C48;        /* direction flip dot threshold (0.965) */
extern char lbl_80111858[];           /* " ATN:%.2Lf %.2Lf %.2Lf" */
extern char lbl_80111870[];           /* " REF:%.2f %.2f %.2f" */

#pragma opt_propagation off
static void BossCamLimitAttn(f32* target) {
    char buf[36];
    f32 v[3];
    f32 pad[11];
    f32 len;
    f32 dot;
    s32 i;
    s32 width;
    s32 col;

    v[0] = target[0] - *(f32*)((u8*)gGameCamera + 176);
    v[1] = target[1] - *(f32*)((u8*)gGameCamera + 180);
    v[2] = target[2] - *(f32*)((u8*)gGameCamera + 184);
    len = NormalVector(v);

    if (len <= lbl_80345C40) {
        *(f32*)((u8*)gGameCamera + 212) = lbl_80345BA0;
        *(f32*)((u8*)gGameCamera + 216) = lbl_80345BA0;
        *(f32*)((u8*)gGameCamera + 188) = lbl_80345BA0;
        *(f32*)((u8*)gGameCamera + 192) = lbl_80345BA0;
        *(f32*)((u8*)gGameCamera + 196) = lbl_80345BA0;
        *(f32*)((u8*)gGameCamera + 200) = lbl_80345BA0;
        *(f32*)((u8*)gGameCamera + 204) = lbl_80345BA0;
        *(f32*)((u8*)gGameCamera + 208) = lbl_80345BA0;
        *(f32*)((u8*)gGameCamera + 164) = target[0];
        *(f32*)((u8*)gGameCamera + 168) = target[1];
        *(f32*)((u8*)gGameCamera + 172) = target[2];
        *(f32*)((u8*)gGameCamera + 176) = target[0];
        *(f32*)((u8*)gGameCamera + 180) = target[1];
        *(f32*)((u8*)gGameCamera + 184) = target[2];
    } else if (*(f32*)((u8*)gGameCamera + 212) <= lbl_80345B98) {
        *(f32*)((u8*)gGameCamera + 212) = LimitCamVal2(
            *(f32*)((u8*)gGameCamera + 212), len,
            -lbl_80343B98, lbl_80343B98, lbl_80343B9C, lbl_80345BA0,
            (f32*)((u8*)gGameCamera + 216), 0);
        *(f32*)((u8*)gGameCamera + 188) = v[0];
        *(f32*)((u8*)gGameCamera + 192) = v[1];
        *(f32*)((u8*)gGameCamera + 196) = v[2];
        *(f32*)((u8*)gGameCamera + 200) = lbl_80345BA0;
        *(f32*)((u8*)gGameCamera + 204) = lbl_80345BA0;
        *(f32*)((u8*)gGameCamera + 208) = lbl_80345BA0;
        *(f32*)((u8*)gGameCamera + 164) =
            *(f32*)((u8*)gGameCamera + 188) * *(f32*)((u8*)gGameCamera + 212) +
            *(f32*)((u8*)gGameCamera + 176);
        *(f32*)((u8*)gGameCamera + 168) =
            *(f32*)((u8*)gGameCamera + 192) * *(f32*)((u8*)gGameCamera + 212) +
            *(f32*)((u8*)gGameCamera + 180);
        *(f32*)((u8*)gGameCamera + 172) =
            *(f32*)((u8*)gGameCamera + 196) * *(f32*)((u8*)gGameCamera + 212) +
            *(f32*)((u8*)gGameCamera + 184);
    } else {
        dot = v[0] * *(f32*)((u8*)gGameCamera + 188) +
              v[1] * *(f32*)((u8*)gGameCamera + 192) +
              v[2] * *(f32*)((u8*)gGameCamera + 196);
        if (dot < lbl_80345C48) {
            *(f32*)((u8*)gGameCamera + 212) = lbl_80345BA0;
            *(f32*)((u8*)gGameCamera + 176) = *(f32*)((u8*)gGameCamera + 164);
            *(f32*)((u8*)gGameCamera + 180) = *(f32*)((u8*)gGameCamera + 168);
            *(f32*)((u8*)gGameCamera + 184) = *(f32*)((u8*)gGameCamera + 172);
        } else {
            *(f32*)((u8*)gGameCamera + 212) = LimitCamVal2(
                *(f32*)((u8*)gGameCamera + 212), len,
                -lbl_80343B98, lbl_80343B98, lbl_80343B9C, lbl_80345BA0,
                (f32*)((u8*)gGameCamera + 216), 0);
        }
        i = 0;
        do {
            *(f32*)((u8*)gGameCamera + 188 + i * 4) = LimitCamVal2(
                *(f32*)((u8*)gGameCamera + 188 + i * 4), v[i],
                -lbl_80343BA0, lbl_80343BA0, lbl_80343BA4, lbl_80345BA0,
                (f32*)((u8*)gGameCamera + 200 + i * 4), 0);
            i++;
        } while (i < 3);
        *(f32*)((u8*)gGameCamera + 164) =
            *(f32*)((u8*)gGameCamera + 188) * *(f32*)((u8*)gGameCamera + 212) +
            *(f32*)((u8*)gGameCamera + 176);
        *(f32*)((u8*)gGameCamera + 168) =
            *(f32*)((u8*)gGameCamera + 192) * *(f32*)((u8*)gGameCamera + 212) +
            *(f32*)((u8*)gGameCamera + 180);
        *(f32*)((u8*)gGameCamera + 172) =
            *(f32*)((u8*)gGameCamera + 196) * *(f32*)((u8*)gGameCamera + 212) +
            *(f32*)((u8*)gGameCamera + 184);
    }

    if ((gControllerButtons & 16) != 0) {
        sprintf(buf, lbl_80111858, (f64)target[0], (f64)target[1],
                (f64)target[2]);
        width = strlen(buf) + 2;
        if ((gControllerButtons & 1) != 0) {
            col = 63 - width;
            dbgTextPrintfCell(0xFFFF00, col, 34, lbl_80111870,
                              (f64)*(f32*)((u8*)gGameCamera + 176),
                              (f64)*(f32*)((u8*)gGameCamera + 180),
                              (f64)*(f32*)((u8*)gGameCamera + 184));
            dbgTextPrintfCell(0xFFFF00, col, 35, buf);
        }
    }
}
#pragma opt_propagation reset
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
#pragma opt_propagation off
static void GetBossAvgPos(f32* out, f32 t, f32* p4, f32* p5, s32 mode) {
    f32* bpos = (f32*)(gBossObj + 76);
    s32 i;

    if (t <= lbl_80345B98) {
        out[0] = bpos[0];
        out[1] = bpos[1];
        out[2] = bpos[2];
    } else if (mode == 0) {
        f64 c;
        f32 s;
        f32 o0;
        f32 r;
        f64 w;
        f64 d;
        c = lbl_80345BE0;
        o0 = out[0];
        s = t + c;
        r = c / s;
        d = s - c;
        w = d * r;
        out[0] = o0 * w;
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
            f32 h;
            f32 l;
            f32 b = bpos[i];
            f32* hp = &hi[i];
            f32* lp;
            h = *hp;
            h = h < b ? h : b;
            *hp = h;
            lp = &lo[i];
            l = *lp;
            l = l > b ? l : b;
            *lp = l;
            out[i] = lbl_80345BA8 * (*hp + *lp);
        }
    }
}
#pragma opt_propagation reset
/* Averaged view/aim vector (0x4B4, atan2 + sin/cos).  Parked: large fp body. */
extern f64 NormalVector2D(f32* vec);
extern f32 atan2(f32 y, f32 x);
extern f32 sin(f32 x);
extern f64 __fabs(f64 x);
#define ABS(x) __fabs(x)
extern const f64 lbl_80345C60;        /* flip threshold (~pi/2) */
extern const f64 lbl_80345C68;        /* spread threshold scale */
extern f32 lbl_80343B80;
extern const f64 lbl_80345BE0;        /* weight step */
extern char lbl_801117B8[];           /* string pool base (fmts at +0xCC/+0xF0) */

static f32 GetActualAvgVec(f32* out, f32* pos, s32 useBoss) {
    f32 v[3];
    u8 unused[44];
    f32 v1x, v1y, v1z;
    f32 v2x, v2y, v2z;
    f32 w;
    f64 best;
    f64 second;
    f64 dzero;
    f64 step;
    f64 len;
    f32 yaw1;
    f32 yaw2;
    f32 avg;
    f64 wrapped;
    char* fmts;
    u8* p;
    s32 count;
    s32 i;
    s32 off;
    s32 zero;

    w = lbl_80345BA0;
    fmts = lbl_801117B8;
    dzero = lbl_80345B98;
    step = lbl_80345BE0;
    best = w;
    second = w;
    zero = 0;
    count = 0;
    i = -1;
    off = -0x335C;
    do {
        if (i < 0) {
            if (useBoss != 0) {
                v[0] = pos[0] - *(f32*)(gBossObj + 0x4C);
                v[1] = pos[1] - *(f32*)(gBossObj + 0x50);
                v[2] = pos[2] - *(f32*)(gBossObj + 0x54);
                goto measure;
            }
        } else {
            p = gPlayers + off;
            if (*(s32*)(p + 0xE8) == 1) {
                if ((*(s16*)(p + 0x964) & 0x20) != 0) {
                    v[0] = pos[0] - *(f32*)(p + 0xDC);
                    v[1] = pos[1] - *(f32*)(p + 0xE0);
                    v[2] = pos[2] - *(f32*)(p + 0xE4);
                } else {
                    v[0] = pos[0] - *(f32*)(p + 0x44);
                    v[1] = pos[1] - *(f32*)(p + 0x48);
                    v[2] = pos[2] - *(f32*)(p + 0x4C);
                }
                goto measure;
            }
        }
        goto next;
        measure:
            len = NormalVector2D(v);
            if (len > best) {
                if (best > dzero) {
                    v2x = v1x;
                    v2y = v1y;
                    v2z = v1z;
                    second = best;
                }
                best = len;
                v1x = v[0];
                v1y = v[1];
                count++;
                v1z = v[2];
            } else if (len > second) {
                second = len;
                v2x = v[0];
                v2y = v[1];
                count++;
                v2z = v[2];
            }
            w = (f32)(w + step);
        next:
        i++;
        off += 0x335C;
    } while (i < 4);

    if (count == 0) {
        w = lbl_80345BA0;
        return w;
    }
    if (count == 1) {
        avg = atan2(v1x, v1z);
        if ((((s64)(sFlags & 1) ^ zero) |
             (((s64)sFlags & zero) ^ zero)) != 0) {
            dbgTextPrintfCell(0xFFFF00, 1, 0x22, fmts + 0xCC,
                              lbl_80345BC8 * (lbl_80345BD0 * avg), v1x, v1y,
                              v1z);
        }
    } else {
        yaw1 = atan2(v1x, v1z);
        yaw2 = atan2(v2x, v2z);
        avg = (f32)(lbl_80345BA8 * (yaw1 + yaw2));
        if (avg > lbl_80345B88) {
            wrapped = avg - lbl_80345BB8;
        } else if (avg <= lbl_80345BC0) {
            wrapped = lbl_80345BB8 + avg;
        } else {
            wrapped = avg;
        }
        avg = (f32)wrapped;
        wrapped = avg - yaw1;
        if (wrapped > lbl_80345B88) {
            wrapped = wrapped - lbl_80345BB8;
        } else if (wrapped <= lbl_80345BC0) {
            wrapped = lbl_80345BB8 + wrapped;
        }
        if (ABS(wrapped) > lbl_80345C60) {
            wrapped = lbl_80345B88 + avg;
            if (wrapped > lbl_80345B88) {
                wrapped = wrapped - lbl_80345BB8;
            } else if (wrapped <= lbl_80345BC0) {
                wrapped = lbl_80345BB8 + wrapped;
            }
            avg = (f32)wrapped;
        }
        wrapped = yaw1 - yaw2;
        if (wrapped > lbl_80345B88) {
            wrapped = wrapped - lbl_80345BB8;
        } else if (wrapped <= lbl_80345BC0) {
            wrapped = lbl_80345BB8 + wrapped;
        }
        if (ABS(wrapped) > lbl_80345C68 * lbl_80345B88 * lbl_80343B80) {
            wrapped = *(f32*)((u8*)gGameCamera + 236) - avg;
            if (wrapped > lbl_80345B88) {
                wrapped = wrapped - lbl_80345BB8;
            } else if (wrapped <= lbl_80345BC0) {
                wrapped = lbl_80345BB8 + wrapped;
            }
            if (ABS(wrapped) > lbl_80345C60) {
                wrapped = lbl_80345B88 + avg;
                if (wrapped > lbl_80345B88) {
                    wrapped = wrapped - lbl_80345BB8;
                } else if (wrapped <= lbl_80345BC0) {
                    wrapped = lbl_80345BB8 + wrapped;
                }
                avg = (f32)wrapped;
            }
        }
        if ((((s64)(sFlags & 1) ^ zero) |
             (((s64)sFlags & zero) ^ zero)) != 0) {
            dbgTextPrintfCell(0xFFFF00, 1, 0x22, fmts + 0xCC,
                              lbl_80345BC8 * (lbl_80345BD0 * yaw1), v1x, v1y,
                              v1z);
            dbgTextPrintfCell(0xFFFF00, 1, 0x23, fmts + 0xF0,
                              lbl_80345BC8 * (lbl_80345BD0 * yaw2), v2x, v2y,
                              v2z);
        }
    }
    out[0] = -sin(avg);
    out[1] = lbl_80345BA0;
    out[2] = -cos(avg);
    return w;
}

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
    f32 ad;
    f32 av;
    f32 oldVelocity;
    f32 step;
    f32 accelerationStep;
    f32 delta;
    f32 lim;
    f32 absDelta;
    f32 absVelocity;
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

    absDelta = delta;
    target = stopScale * maxVelocity;
    step = *(volatile f32*)&gClockFrameStep;
    *(u32*)&absDelta &= 0x7FFFFFFF;
    oldVelocity = *velocity;
    ad = absDelta;
    absVelocity = oldVelocity;
    *(u32*)&absVelocity &= 0x7FFFFFFF;
    av = absVelocity;
    lim = step * target;

    if ((f64)stopScale > lbl_80345B98 && av < lim && ad < lim) {
        minVelocity = lbl_80345BA0;
    } else if (ad < step * (av + accelerationStep)) {
        minVelocity = delta * gClockFrameReciprocal;
    } else {
        if (av / acceleration >= ad / av) {
            f64 zero = *(volatile const f64*)&lbl_80345B98;

            if ((f64)oldVelocity > zero) {
                target = oldVelocity - accelerationStep;
                if ((f64)target < zero) {
                    target = lbl_80345BA0;
                }
            } else {
                target = oldVelocity + accelerationStep;
                if ((f64)target > zero) {
                    target = lbl_80345BA0;
                }
            }
        } else {
            f64 zero = *(volatile const f64*)&lbl_80345B98;

            if ((f64)delta > zero) {
                target = oldVelocity + accelerationStep;
            } else {
                target = oldVelocity - accelerationStep;
            }
        }

        if (!(target < minVelocity)) {
            if (!(target > maxVelocity)) {
                maxVelocity = target;
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
    GetActualAvgVec(&v, &v, 0);
    (void)LimitCamVal2(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, &v, 0);
}
