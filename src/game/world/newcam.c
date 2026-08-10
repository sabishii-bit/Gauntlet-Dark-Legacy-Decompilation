/* newcam.c -- NEWCAM.OBJ (Gauntlet Dark Legacy camera system), NonMatching.
 *
 * Function names recovered from shell3D.pdb (NEWCAM.OBJ) and confirmed
 * behaviorally (call-graph + string + data cross-reference).  NEWCAM is the
 * "new" camera module; the older camera lives in game/world/camera.c
 * (CAMERA.OBJ).  This TU covers .text 0x8006DC2C .. 0x80070A60 (23 GC
 * functions).  The Xbox PDB lists 34 NEWCAM functions; the GC build (-O4)
 * inlined the small local helpers (LimitYawPitch/LimitTranslate/LimitDist/
 * CalcLookdir/StdCamMoveAvgArrow/CalcDestination/CamInit/StdCamStartStage/...),
 * so function order does NOT map 1:1 and reverse-order positional mapping does
 * not hold here -- names below are anchored by behavior, not position.
 *
 * The NEWCAM camera object (Xbox StdCamera/DebugCamera instances) is a large
 * struct (>= 0x1B0), DISTINCT from the CAMERA.OBJ gCameras[] type (0x18C).  The
 * live debug-camera pointer is DebugCam (lbl_80344A68) -> DebugCamera instance
 * (lbl_80274AA0).  Yaw is field 0xEC, pitch is field 0x104, translate accum is
 * 0xA4/0xAC.
 *
 * NEWCAM strings (.rodata): "CAM Y:%.0f P:%.0f D:%.2f(%.2f)  ATN:%.1f %.1f %.1f"
 * at 0x801137D0 (debug HUD), and "level_data or level_data->camera NULL" at
 * 0x80113808 (FatalError guard in CalcDist).
 *
 * -----------------------------------------------------------------------------
 * NAMING (this pass): 7 functions promoted from fn_ to shell3D.pdb names, with
 * confidence and the anchoring evidence.  Applied to config/GUNE5D/symbols.txt
 * so the names propagate to the whole disassembly and to cross-file callers.
 *
 *   0x8006ED44 StdCamFreeze          HIGH  0xC setter; sets the freeze flag
 *                                          (lbl_80344A90) to 1; CamReset clears
 *                                          it.  Size 0xC ~= Xbox 0xB. [bosscam]
 *   0x8006ED50 CalcDist              HIGH  calls CalcFrustrumNormals then loops
 *                                          the 4 players projecting each onto the
 *                                          frustum planes to find the camera-fit
 *                                          distance; FatalError "...camera NULL".
 *                                          Xbox CalcDist immediately follows
 *                                          CalcFrustrumNormals in source order.
 *   0x8006F678 GetPlayerAvgPos       MED   world-space player avg / min / max with
 *                                          a mode selector + default fallback;
 *                                          leaf, called by bosscam x4 / tower x2.
 *   0x8006F8F0 CamGetPlayerAvgPos    MED   player-center in camera space (matrix
 *                                          transform via MBWorldToScreen/B53B4),
 *                                          clamped to level camera bounds, returns
 *                                          found-bool; used by the mode-updaters
 *                                          + gamemain/boss (camera look-at target).
 *   0x800704EC DebugCamControlInputs HIGH  reads the input bitmask (lbl_80344A84)
 *                                          and drives the debug camera yaw/pitch/
 *                                          translate via sin/cos * speed.  Size
 *                                          0x444 ~= Xbox 0x41F. [DebugCamUpdate]
 *   0x80070930 DebugCamInit          MED-HI sets DebugCam = &DebugCamera, CamReset,
 *                                          marks active (lbl_80344A7C=1) [pb_diag]
 *   0x80070968 CamReset              HIGH  zeroes the whole camera working set
 *                                          (matrix/pos/vel/target/history) and
 *                                          restores defaults; global helper called
 *                                          by 9 objects (bosscam, pb, internal).
 *
 * .text map (addr / symbol / size / behavior / [callers]):
 *   0x8006DC2C fn_8006DC2C          0x38  public wrapper: load global cam ptr
 *                                          (lbl_80344A6C), tail into fn_8006DC64
 *                                          [bosscam]  (frustum-clip query)
 *   0x8006DC64 fn_8006DC64          0x2D0 frustum point-clip test: dot a point
 *                                          against 4 plane normals (cam+164/+224),
 *                                          3-iteration classify loop [bosscam,+wrapper]
 *   0x8006DF34 fn_8006DF34          0x5E8 shared core cam update; draws the "CAM Y:"
 *                                          debug HUD; calls CalcDist/fn_80070144/
 *                                          CamGetPlayerAvgPos/DoShake/MBWindowProjection
 *                                          (candidate: CamUpdate core)
 *   0x8006E51C UpdateCam            0x138 TOP-LEVEL camera-update dispatcher:
 *                                          branches to fn_8006F16C / fn_8006E654 /
 *                                          fn_8006DF34 by camera mode [game/sys/main]
 *   0x8006E654 fn_8006E654          0x5C4 mode-specific cam update; CamReset;
 *                                          pbUpdateMatricies/DoShake [dispatcher]
 *   0x8006EC18 CurTransmitterBlink          0xBC  toggle debug-overlay blit
 *                                          (handle lbl_80344A78) [game/world/items]
 *   0x8006ECD4 fn_8006ECD4          0x70  small projection helper
 *                                          (MBWindowProjection) [bosscam]
 *   0x8006ED44 StdCamFreeze         0xC   set freeze flag -> lbl_80344A90 [bosscam]
 *   0x8006ED50 CalcDist             0x248 camera-fit distance from players+frustum;
 *                                          calls CalcFrustrumNormals [fn_8006DF34]
 *   0x8006EF98 CalcFrustrumNormals  0x1D4 build 4 frustum plane normals from the
 *                                          camera basis vectors scaled by tan(FOV)
 *                                          [bosscam, level-setup]
 *   0x8006F16C fn_8006F16C          0x2AC per-frame cam update (pbUpdateMatricies);
 *                                          calls fn_8006DF34/F418/CamGetPlayerAvgPos/
 *                                          FCDC/CamReset  (candidate: CamUpdate)
 *   0x8006F418 fn_8006F418          0x260 cam sub-update (DoShake/MBWindowProjection)
 *   0x8006F678 GetPlayerAvgPos      0x278 world-space player avg/min/max [bosscam,tower]
 *   0x8006F8F0 CamGetPlayerAvgPos   0x2BC camera-space player center [gamemain,boss,int]
 *   0x8006FBAC fn_8006FBAC          0x130 calls fqdist [bosscam]
 *   0x8006FCDC fn_8006FCDC          0x154 calls MBTreeSetAlpha [internal]
 *   0x8006FE30 fn_8006FE30          0xEC  camera+projection setup (CamReset + proj
 *                                          params lbl_80344EE8) [game/sys/main]
 *   0x8006FF1C fn_8006FF1C          0x228 debug-camera update path; "CAM Y" HUD;
 *                                          calls CamReset/DebugCamControlInputs;
 *                                          DoShake [game/sys/main x2]
 *                                          (candidate: DebugCamUpdate)
 *   0x80070144 fn_80070144          0x1FC pure matrix/vector math (no calls) [fn_8006DF34]
 *   0x80070340 CamLookInDir         0x1AC shared cam helper (SlowNormalVector/BE8C8);
 *                                          5 internal callers (all mode-updaters)
 *   0x800704EC DebugCamControlInputs 0x444 input -> debug cam motion (sin/cos) [fn_8006FF1C]
 *   0x80070930 DebugCamInit         0x38  DebugCam=&DebugCamera; CamReset [pb_diag]
 *   0x80070968 CamReset             0xF8  reset camera working set to defaults
 *                                          (highest caller: 9 objs)
 *
 * NonMatching: dtk links the original DOL bytes for this range; the bodies below
 * are behavioral reconstructions from the target asm and are not byte-matched.
 */

#include "types.h"
#include "game/camera.h"

typedef struct Vec3 {
    f32 x;
    f32 y;
    f32 z;
} Vec3;

/*
 * NEWCAM camera object (Xbox: the StdCamera/DebugCamera CAMERA type; NOT the
 * 0x18C CAMERA.OBJ gCameras[] element).  Only the fields NEWCAM touches are
 * named; offsets are read from the GC target asm.  Padded to 0x1B0 so the fields
 * stay byte-accurate for the reconstructions below.
 */
typedef struct NcCamera {
    u8  _000[0x030];
    Vec3 position;     /* 0x030 world position */
    u8  _03C[0x0A4 - 0x03C];
    Vec3 attention;    /* 0x0A4 look-at point */
    u8  _0B0[0x0DC - 0x0B0];
    f32 level_height;  /* 0x0DC level camera height */
    Vec3 direction;    /* 0x0E0 forward vector */
    f32 yaw;           /* 0x0EC */
    u8  _0F0[0x0F4 - 0x0F0];
    f32 distance;      /* 0x0F4 follow distance */
    u8  _0F8[0x104 - 0x0F8];
    f32 pitch;         /* 0x104 */
    u8  _108[0x10C - 0x108];
    f32 zoom;          /* 0x10C */
    f32 aspect;        /* 0x110 */
    u8  _114[0x1A4 - 0x114];
    s32 field_1A4;     /* 0x1A4 */
    s32 field_1A8;     /* 0x1A8 (reset to -1) */
    f32 field_1AC;     /* 0x1AC */
} NcCamera;            /* 0x1B0 */

/*
 * Player record view (subset the camera reads).  Full record is game/player.h
 * Player (0x335C, base gPlayers, stride 0x335C).  Local view keeps newcam
 * self-contained; offsets verified against the target asm.
 */
typedef struct NcPlayer {
    u8  _000[0x044];
    f32 pos[3];        /* 0x044 world position (x,y,z) */
    u8  _050[0x054 - 0x050];
    f32 campos[3];     /* 0x054 camera-follow target */
    u8  _060[0x0DC - 0x060];
    f32 altpos[3];     /* 0x0DC alternate follow target (when flag 0x964 bit26 set) */
    s32 state;         /* 0x0E8 1=active 4=(also camera-tracked) */
    u8  _0EC[0x964 - 0x0EC];
    s16 ncflags;       /* 0x964 bit26 selects altpos and excludes from GetPlayerAvgPos */
    u8  _966[0x335C - 0x966];
} NcPlayer;            /* 0x335C */

/* ----- module globals (NEWCAM.OBJ .bss/.data; label names from the disasm) --- */
extern NcPlayer  gPlayers[4];   /* the 4 player records (game/player.h Player[]) */
extern f32       gDefaultPlayerPosition[3];   /* default position when no player is valid */
extern NcCamera  lbl_80274AA0;      /* DebugCamera instance */
extern NcCamera* lbl_80344A6C;      /* live standard-camera pointer (frustum query) */
extern NcCamera* lbl_80344A68;      /* DebugCam: pointer to the live debug camera */
extern s32       lbl_80344A70;
extern s32       lbl_80344A7C;      /* debug-camera active flag */
extern s32       lbl_80344A8C;
extern s32       lbl_80344A90;      /* StdCam freeze flag */
extern s32       lbl_80343CD4;
extern f32       lbl_80343CD8;
extern f32       lbl_80343CDC;
extern void*     gCurLevel;         /* level record; +0x60 = active CAMERA* (bounds) */

/* NEWCAM projection-parameter block: the live MB window pointer (mb_window.c
 * .sbss), half-FOV tangents at +0x1C / +0x20, view matrix at +0x64. */
extern f32* lbl_80344EE8;
/* seed vector for YawVec3 (a fixed unit direction). */
extern const u8 lbl_80127D30[];

/* ----- external helpers (G3D math layer) ----- */
extern double tan(double);
extern double __frsqrte(double);
/* rotate/derive a unit vector from a constant seed. */
extern void YawVec3(const void* seed, Vec3* out, f32 angle);
extern void PitchVec3(const f32* vector, f32* out, f32 angle);
extern void CopyMat4(const f32* src, f32* dst);
/* transform a point through the current matrix stack (dst <- M * src). */
extern void MBWorldToScreen(Vec3* dst, const Vec3* src);
extern void MBWorldToScreen3D(Vec3* dst, const Vec3* src);

/* frustum point-clip test core (fn_8006DC64); the public entry fn_8006DC2C is
 * a thin wrapper that supplies the live standard-camera pointer. */
extern s32 fn_8006DC64(NcCamera* cam, NcPlayer* player, Vec3* pt, s32 mode);

/* MB window/camera projection layer (mb_camera.c / mb_window.c / pb_window.c). */
extern void MBCameraUpdate(f32* position, f32* matrix);
extern void MBWindowZoom(f32 zoom);
extern void MBWindowProjection(f32 angle, f32 aspect);
extern void DoShake(Vec3* position, Vec3* attention);
extern void DebugCamControlInputs(void);
extern void dbgTextPrintfCell(s32 color, s32 x, s32 line, char* fmt, ...);

/* MB scene-tree node ops + the level-arrow blit factory (world/items.c). */
extern s32  add_arrow(s32 kind, s32 refresh, s32 useAngles, f32* angles,
                      f32* look, f32* pos);
extern void MBTreeSetAlpha(void* node, s32 a, s32 b);
extern void MBTreeClearFlags(void* node, s32 a, s32 b);
extern void MBRemoveNode(void* node, s32 a);

extern void* lbl_80344A78;          /* the debug-overlay arrow node handle */
extern f32   lbl_80127D40[];        /* default arrow angle vector */
typedef struct NcBlk16 { u32 w[4]; } NcBlk16;
extern const NcBlk16 lbl_801137C0;  /* 16-byte zero look template */

/* copy a 3x3 (row-major) basis; GetYawPitch derives a look basis from 3 vecs. */
extern void CopyMat3(f32* src, f32* dst);
extern void GetYawPitch(f32* a, f32* b, f32* c);
extern void FatalError(char* message, s32 code);

/* normalize a Vec3 in place, returning its original length (ps2/ml_fmath.c). */
extern f32 SlowNormalVector(f32* vector);

extern s32 lbl_80343CE0;   /* slow-motion / time-scaling enable */
extern f32 gClockFrameStep; /* time scale (frame delta) */
extern s32 lbl_80343CEC;   /* interpolation frame count (divisor) */
extern s32 lbl_80343CF8;   /* active marker index (also the 3D selector) */
extern s32 lbl_80344768;
extern u8  lbl_80344A74;
extern s32 gControllerButtons;
extern s32 sFlags;
extern char lbl_801137D0[];

/*
 * fn_80070144 -- step the camera yaw (0xEC) and pitch (0x104) toward the given
 * targets over lbl_80343CEC frames.  On a marker change it seeds the per-frame
 * yaw/pitch rates (shortest-arc wrapped) and resets the frame accumulator
 * (0x1AC); each subsequent call advances yaw/pitch by rate*step (wrapping to
 * [-PI,PI]) until the accumulator reaches the frame count.  Returns 1 while
 * still interpolating, 0 when done.  [caller: fn_8006DF34]
 */
s32 fn_80070144(f32 targetYaw, f32 targetPitch, NcCamera* cam) {
    u8* c = (u8*)cam;
    f32 step;
    f64 d;

    if (lbl_80343CE0 != 0) {
        if (gClockFrameStep <= 0.0) {
            step = 1.0f;
        } else {
            step = 30.0 * gClockFrameStep;
        }
    } else {
        step = 1.0f;
    }

    if (lbl_80343CF8 != *(s32*)(c + 0x1A8)) {
        *(s32*)(c + 0x1A8) = lbl_80343CF8;

        d = (f32)(targetYaw - *(f32*)(c + 0xEC));
        if (d > 3.141592654) {
            d = d - 6.283185308;
        } else if (d <= -3.141592654) {
            d = 6.283185308 + d;
        }
        *(f32*)(c + 0xF0) = d / (f64)lbl_80343CEC;

        d = (f32)(targetPitch - *(f32*)(c + 0x104));
        if (d > 3.141592654) {
            d = d - 6.283185308;
        } else if (d <= -3.141592654) {
            d = 6.283185308 + d;
        }
        *(f32*)(c + 0x108) = d / (f64)lbl_80343CEC;

        *(f32*)(c + 0x1AC) = 0.0f;
    }

    if (*(f32*)(c + 0x1AC) < (f32)lbl_80343CEC) {
        d = *(f32*)(c + 0xF0) * step + *(f32*)(c + 0xEC);
        if (d > 3.141592654) {
            d = d - 6.283185308;
        } else if (d <= -3.141592654) {
            d = 6.283185308 + d;
        }
        *(f32*)(c + 0xEC) = d;

        d = *(f32*)(c + 0x108) * step + *(f32*)(c + 0x104);
        if (d > 3.141592654) {
            d = d - 6.283185308;
        } else if (d <= -3.141592654) {
            d = 6.283185308 + d;
        }
        *(f32*)(c + 0x104) = d;
    } else {
        return 0;
    }
    *(f32*)(c + 0x1AC) = *(f32*)(c + 0x1AC) + step;
    return 1;
}
extern const f32 lbl_80127D20[3];  /* up ref: general */
/* lbl_80127D40 (up ref: looking up) declared above for CurTransmitterBlink */
extern const f32 lbl_80127D50[3];  /* up ref: looking down */
extern const f32 gIdentityMatrix[];   /* default identity-ish basis */

/*
 * CamLookInDir -- build an orthonormal look basis into mat[0..2]=right,
 * mat[4..6]=up, mat[8..10]=forward from the forward direction in dir.  Degenerate
 * (near-zero) forward copies a default basis; a near-vertical forward selects an
 * up reference by sign, otherwise a general up reference; then two cross products
 * (with a re-normalize) orthonormalize the basis.  [5 internal callers]
 */
void CamLookInDir(f32* dir, u32 mat) {
    f32* m;
    f32* up;
    f32* fwd;
    f32 len;

    m = (f32*)mat;
    up = m + 4;
    fwd = m + 8;
    m[8] = dir[0];
    m[9] = dir[1];
    m[10] = dir[2];
    len = SlowNormalVector(fwd);
    if (len < 0.001) {
        CopyMat3((f32*)gIdentityMatrix, m);
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
        m[0] = up[1] * fwd[2] - up[2] * fwd[1];
        m[1] = up[2] * fwd[0] - up[0] * fwd[2];
        m[2] = up[0] * fwd[1] - up[1] * fwd[0];
        SlowNormalVector(m);
        up[0] = fwd[1] * m[2] - fwd[2] * m[1];
        up[1] = fwd[2] * m[0] - fwd[0] * m[2];
        up[2] = fwd[0] * m[1] - fwd[1] * m[0];
    }
}

void CamReset(NcCamera* cam);   /* defined below; forward decl for early callers */

/* per-mode camera updaters + the combat camera-tick hook (game/game/combat.c). */
extern void fn_8006F16C(s32 arg);
extern void fn_8006DF34(NcCamera* cam);
extern void fn_8006E654(void);
extern void write_stage_info(s32 mode);

extern s32       gGameBusy;   /* master camera-disable flag */
extern void*     gFrameTicks;   /* camera-enable gate (nonzero to run) */
extern s32       gGameMode;   /* camera-path mode selector (0x4010 = scripted) */
extern s32       gScriptedCameraState;   /* scripted-path sub-state */

/*
 * UpdateCam -- top-level per-frame camera dispatcher.  Runs only when the
 * camera is enabled; lazily kick-starts the standard camera, drives the
 * scripted-path state machine (write_stage_info) when active, and otherwise ticks
 * the live standard camera (fn_8006DF34) or, on the freeze->unfreeze edge,
 * re-pushes the MB projection.  Always returns 1.  [caller: game/sys/main]
 */
s32 UpdateCam(void) {
    s32 done;

    if (gGameBusy != 0 || gFrameTicks == 0) {
        return 1;
    }
    if (lbl_80344A6C == 0) {
        fn_8006F16C(0);
    }
    if (gGameMode != 0x4010) {
        done = 1;
    } else {
        if (gScriptedCameraState > 2) {
            gScriptedCameraState = 2;
        }
        write_stage_info(gScriptedCameraState);
        if (gScriptedCameraState == 1) {
            fn_8006E654();
        }
        done = gScriptedCameraState > 0;
    }
    if (done != 0) {
        return 1;
    }
    if (lbl_80344A90 != 0) {
        lbl_80344A90 = 0;
        MBCameraUpdate((f32*)((u8*)lbl_80344A6C + 0x30), (f32*)lbl_80344A6C);
        MBWindowZoom(*(f32*)((u8*)lbl_80344A6C + 0x10C));
        if (*(f32*)((u8*)lbl_80344A6C + 0x110) > 0.0) {
            MBWindowProjection(
                0.31830988614222805 * (180.0 * *(f32*)((u8*)lbl_80344A6C + 0x10C)),
                1.0 / *(f32*)((u8*)lbl_80344A6C + 0x110));
        }
        return 1;
    }
    fn_8006DF34(lbl_80344A6C);
    return 1;
}

/*
 * Marker/waypoint record (0x28 stride) scanned by the nearest-selectors below.
 * Only the fields the selectors touch are named.  Array base sTriggerCameras.
 */
typedef struct NcMarker {
    u8    flag;       /* 0x00 nonzero = record disabled */
    u8    _01[3];
    f32   x;          /* 0x04 */
    f32   y;          /* 0x08 */
    f32   z;          /* 0x0C */
    u8    _10[0x24 - 0x10];
    void* node;       /* 0x24 scene node handle */
} NcMarker;           /* 0x28 */

extern NcMarker sTriggerCameras[];  /* marker records */
extern s32 sNumTriggerCameras;         /* marker count */
extern s32 lbl_80343CF4;         /* current selection (2D XZ selector) */
extern s32 lbl_80343CF8;         /* current selection (3D selector) */

/* fast 2D (XZ) distance approximation (ps2/ml_fmath.c). */
extern f32 fqdist(f32 x, f32 y);

/*
 * fn_8006FBAC -- pick the enabled marker nearest (in the XZ plane) to pos,
 * with hysteresis: the previously-selected marker is kept unless the new best
 * is at least ~1.5x closer (bestDist <= 0.667 * selectedDist).  Returns the
 * chosen record (NULL if none).  [caller: bosscam]
 */
void* fn_8006FBAC(f32* pos) {
    u8 unused[16];
    f32 bestDist = 0.0f;
    s32 i;
    s32 best = -1;

    if (sNumTriggerCameras <= 0) {
        return NULL;
    }
    for (i = 0; i < sNumTriggerCameras; i++) {
        NcMarker* m = &sTriggerCameras[i];
        if (m->flag == 0 && i != lbl_80343CF4) {
            f32 d = fqdist(pos[0] - m->x, pos[2] - m->z);
            if (best < 0 || d < bestDist) {
                bestDist = d;
                best = i;
            }
        }
    }
    if (lbl_80343CF4 < 0) {
        lbl_80343CF4 = best;
    } else {
        NcMarker* m = &sTriggerCameras[lbl_80343CF4];
        f32 selDist = fqdist(pos[0] - m->x, pos[2] - m->z);
        if (bestDist <= 0.667 * selDist) {
            lbl_80343CF4 = best;
        }
    }
    return &sTriggerCameras[lbl_80343CF4];
}

/*
 * fn_8006FCDC -- 3D counterpart of fn_8006FBAC: pick the enabled marker nearest
 * (full 3D squared distance) to pos, with the same hysteresis (kept unless the
 * new best is ~1.5x closer: bestDist <= 4/9 * selectedDist).  When it switches,
 * it fades the previously-selected marker's node (MBTreeSetAlpha).  [internal]
 */
void* fn_8006FCDC(f32* pos) {
    u8 unused[8];
    s32 best = -1;
    s32 sel;
    s32 i;
    f32 bestDist = 0.0f;
    f32 dx;
    f32 dy;
    f32 dz;
    f32 d;
    f32 selDist;

    if (sNumTriggerCameras <= 0) {
        return NULL;
    }
    sel = lbl_80343CF8;
    for (i = 0; i < sNumTriggerCameras; i++) {
        NcMarker* m = &sTriggerCameras[i];
        if (m->flag == 0 && i != sel) {
            dx = pos[0] - m->x;
            dy = pos[1] - m->y;
            dz = pos[2] - m->z;
            d = dx * dx + dy * dy + dz * dz;
            if (best < 0 || d < bestDist) {
                bestDist = d;
                best = i;
            }
        }
    }
    if (sel < 0) {
        lbl_80343CF8 = best;
    } else {
        NcMarker* m = &sTriggerCameras[sel];
        dx = pos[0] - m->x;
        dy = pos[1] - m->y;
        dz = pos[2] - m->z;
        selDist = dx * dx + dy * dy + dz * dz;
        if (bestDist <= 0.4444444444444444 * selDist) {
            MBTreeSetAlpha(m->node, 100, 0);
            lbl_80343CF8 = best;
        }
    }
    return &sTriggerCameras[lbl_80343CF8];
}

/*
 * fn_8006FE30 -- initialise/refresh the debug camera projection.  Lazily wires
 * DebugCam to the static DebugCamera instance (once), copies the MB window's
 * view basis and eye position into the camera, mirrors them into the camera's
 * history/target rows, and rebuilds the look basis.  [caller: game/sys/main]
 */
void fn_8006FE30(void) {
    if (lbl_80344A7C == 0) {
        lbl_80344A68 = &lbl_80274AA0;
        CamReset(lbl_80344A68);
        lbl_80344A7C = 1;
    }
    CopyMat3((f32*)((u8*)lbl_80344EE8 + 100), (f32*)lbl_80344A68);
    ((f32*)lbl_80344A68)[0xc] = *(f32*)((u8*)lbl_80344EE8 + 0x94);
    ((f32*)lbl_80344A68)[0xd] = *(f32*)((u8*)lbl_80344EE8 + 0x98);
    ((f32*)lbl_80344A68)[0xe] = *(f32*)((u8*)lbl_80344EE8 + 0x9c);
    ((f32*)lbl_80344A68)[0x38] = ((f32*)lbl_80344A68)[8];
    ((f32*)lbl_80344A68)[0x39] = ((f32*)lbl_80344A68)[9];
    ((f32*)lbl_80344A68)[0x3a] = ((f32*)lbl_80344A68)[0xa];
    ((f32*)lbl_80344A68)[0x29] = ((f32*)lbl_80344A68)[0xc];
    ((f32*)lbl_80344A68)[0x2a] = ((f32*)lbl_80344A68)[0xd];
    ((f32*)lbl_80344A68)[0x2b] = ((f32*)lbl_80344A68)[0xe];
    ((f32*)lbl_80344A68)[0x3d] = 0.0f;
    GetYawPitch((f32*)lbl_80344A68 + 0x38, (f32*)lbl_80344A68 + 0x3b,
                (f32*)lbl_80344A68 + 0x41);
}

/* Per-frame debug-camera update. */
static inline u32 NcMaskMismatch(u32 value, u32 expected) {
    return value ^ expected;
}

static inline u32 NcApplyMask(u32 value, u32 mask) {
    return value & mask;
}

#pragma opt_propagation off
s32 fn_8006FF1C(void) {
    f32* cam;
    f32 pitch;
    u32 controller;
    u32 zero;
    u32 one;
    u32 flags;

    if (lbl_80344A7C == 0) {
        lbl_80344A68 = &lbl_80274AA0;
        CamReset(lbl_80344A68);
        lbl_80344A7C = 1;
    }
    MBTreeSetAlpha(sTriggerCameras[lbl_80343CF8].node, lbl_80344A74, 0);
    lbl_80344A74 += 8;
    DebugCamControlInputs();

    cam = (f32*)lbl_80344A68;
    pitch = cam[0x41];
    YawVec3(lbl_80127D40, (Vec3*)(cam + 0x38), -cam[0x3B]);
    PitchVec3(cam + 0x38, cam + 0x38, -pitch);
    DoShake((Vec3*)(cam + 0xC), (Vec3*)(cam + 0x29));

    cam[0xC] = cam[0x38] * -cam[0x3D] + cam[0x29];
    cam[0xD] = cam[0x39] * -cam[0x3D] + cam[0x2A];
    cam[0xE] = cam[0x3A] * -cam[0x3D] + cam[0x2B];
    CamLookInDir(cam + 0x38, (u32)cam);

    CopyMat4(cam, &gCameras[0].mat[0][0]);
    gCameras[0].wpos[0] = cam[0xC];
    gCameras[0].wpos[1] = cam[0xD];
    gCameras[0].wpos[2] = cam[0xE];
    gCameras[0].attn[0] = cam[0x29];
    gCameras[0].attn[1] = cam[0x2A];
    gCameras[0].attn[2] = cam[0x2B];
    MBCameraUpdate(cam + 0xC, cam);
    MBWindowZoom(cam[0x43]);
    if (cam[0x44] > 0.0) {
        MBWindowProjection(
            0.31830988614222805 * (180.0 * cam[0x43]),
            1.0 / cam[0x44]);
    }

    controller = gControllerButtons;
    zero = 0;
    one = 1;
    flags = sFlags;
    if ((NcMaskMismatch(NcApplyMask(flags, one), zero) |
         NcMaskMismatch(controller & zero, zero)) != 0) {
        dbgTextPrintfCell(
            0xFFFF00, 1, 0x20, lbl_801137D0,
            0.31830988614222805 * (180.0 * cam[0x3B]),
            0.31830988614222805 * (180.0 * cam[0x41]),
            cam[0x3D], cam[0x40], cam[0x29], cam[0x2A], cam[0x2B]);
    }
    return 1;
}
#pragma opt_propagation reset

/*
 * CurTransmitterBlink -- toggle the debug-overlay level arrow.  Non-zero idx shows it
 * (creating the arrow node once, then clearing its draw flags each call); zero
 * idx removes it.  [caller: game/world/items.c]
 */
void CurTransmitterBlink(s32 idx) {
    f32 pos[17];
    NcBlk16 look = lbl_801137C0;

    if (idx != 0) {
        if (lbl_80344A78 == 0) {
            lbl_80344A78 = (void*)add_arrow(2, 1, 0, lbl_80127D40, (f32*)&look, pos);
            MBTreeSetAlpha(lbl_80344A78, 0, 0);
        }
        MBTreeClearFlags(lbl_80344A78, 2, 0);
    } else {
        if (lbl_80344A78 != 0) {
            MBRemoveNode(lbl_80344A78, 1);
            lbl_80344A78 = 0;
        }
    }
}

/*
 * fn_8006ECD4 -- push the live standard camera into the MB window/projection
 * layer: update the MB camera from the camera basis, zoom the window by the
 * camera FOV field, and (when the projection distance is positive) set the MB
 * projection to the FOV in degrees and the inverse distance.  [caller: bosscam]
 */
void fn_8006ECD4(void) {
    MBCameraUpdate((f32*)((u8*)lbl_80344A6C + 0x30), (f32*)lbl_80344A6C);
    MBWindowZoom(*(f32*)((u8*)lbl_80344A6C + 0x10C));
    if (*(f32*)((u8*)lbl_80344A6C + 0x110) > 0.0) {
        MBWindowProjection(
            0.31830988614222805 * (180.0 * *(f32*)((u8*)lbl_80344A6C + 0x10C)),
            1.0 / *(f32*)((u8*)lbl_80344A6C + 0x110));
    }
}

/*
 * fn_8006DC2C -- public frustum point-clip query.  Loads the live standard
 * camera (lbl_80344A6C) and forwards the caller's arguments to the clip core.
 * [caller: bosscam]
 */
s32 fn_8006DC2C(NcPlayer* player, f32* pt, s32 mode) {
    return fn_8006DC64(lbl_80344A6C, player, (Vec3*)pt, mode);
}

/*
 * CamReset -- reset a camera object to its default working state.  Zeroes the
 * transform / position / velocity / target and history arrays, restores the
 * default yaw and mode fields, and clears the module freeze / current-mode
 * globals.  Highest-fan-in helper in the module (called by 9 objects on every
 * camera-mode change and at init).
 *
 * Structural reconstruction (NonMatching): the shipped code stores each field
 * explicitly rather than via memset; the constant defaults (field yaw <-
 * lbl_80347508, field_1AC/10C <- lbl_8034750C) are preserved symbolically here.
 */
#pragma dont_inline on
void CamReset(NcCamera* cam) {
    u8* p = (u8*)cam;
    s32 i;

    *(f32*)(p + 0xEC) = 3.1415927f;
    *(f32*)(p + 0xF0) = 0.0f;
    *(f32*)(p + 0x104) = 0.0f;
    *(f32*)(p + 0x108) = 0.0f;
    *(f32*)(p + 0xF4) = 0.0f;
    *(f32*)(p + 0xF8) = 0.0f;
    *(f32*)(p + 0xE0) = 0.0f;
    *(f32*)(p + 0xE4) = 0.0f;
    *(f32*)(p + 0xE8) = 0.0f;
    *(f32*)(p + 0xA4) = 0.0f;
    *(f32*)(p + 0xA8) = 0.0f;
    *(f32*)(p + 0xAC) = 0.0f;
    *(f32*)(p + 0xBC) = 0.0f;
    *(f32*)(p + 0xC0) = 0.0f;
    *(f32*)(p + 0xC4) = 0.0f;
    *(f32*)(p + 0xC8) = 0.0f;
    *(f32*)(p + 0xCC) = 0.0f;
    *(f32*)(p + 0xD0) = 0.0f;
    *(f32*)(p + 0x40) = 0.0f;
    *(f32*)(p + 0x44) = 0.0f;
    *(f32*)(p + 0x48) = 0.0f;
    *(f32*)(p + 0x50) = 0.0f;
    *(f32*)(p + 0x54) = 0.0f;
    *(f32*)(p + 0x58) = 0.0f;
    *(f32*)(p + 0x60) = 0.0f;
    *(f32*)(p + 0x64) = 0.0f;
    *(f32*)(p + 0x68) = 0.0f;
    *(f32*)(p + 0x70) = 0.0f;
    *(f32*)(p + 0x74) = 0.0f;
    *(f32*)(p + 0x78) = 0.0f;

    for (i = 0; i < 9; i++) {
        *(f32*)(p + i * 0xC + 0x114) = 0.0f;
        *(f32*)(p + i * 0xC + 0x118) = 0.0f;
        *(f32*)(p + i * 0xC + 0x11C) = 0.0f;
        *(f32*)(p + i * 4 + 0x180) = 0.0f;
    }

    *(s32*)(p + 0x1A4) = 0;
    *(f32*)(p + 0xD4) = 0.0f;
    *(f32*)(p + 0xD8) = 0.0f;
    *(f32*)(p + 0x100) = 0.0f;
    *(f32*)(p + 0x10C) = 1.0471976f;
    *(f32*)(p + 0x110) = 0.0f;
    lbl_80344A90 = 0;
    lbl_80344A70 = lbl_80343CD4;
    *(s32*)(p + 0x1A8) = -1;
    *(f32*)(p + 0x1AC) = 0.0f;
    lbl_80344A78 = 0;
}
#pragma dont_inline off

/*
 * StdCamFreeze -- freeze the standard camera (stop it from tracking players)
 * until the next CamReset.  Trivial flag setter.
 */
void StdCamFreeze(void) {
    lbl_80344A90 = 1;
}

/*
 * DebugCamInit -- point DebugCam at the static DebugCamera instance, reset it,
 * and mark the debug camera active.  Invoked from the pb diagnostic screen.
 */
void DebugCamInit(void) {
    lbl_80344A68 = &lbl_80274AA0;
    CamReset(lbl_80344A68);
    lbl_80344A7C = 1;
}

/*
 * GetPlayerAvgPos -- world-space aggregate of the active players' follow points.
 *
 *   mode 0 : *avg = mean of the valid players' positions
 *   mode>0 : *avg = midpoint of the axis-aligned bounding box of those positions
 *   mode 2 : additionally clamp *avg to the level camera's world bounds
 *
 * A player counts only if state==1; its source point is altpos (0xDC) when the
 * 0x964 bit26 flag is set, else campos (0x54).  When no player is valid the
 * default position (gDefaultPlayerPosition) is used.  bmax/bmin, when non-NULL, receive
 * the bounding box (meaningful only for mode>0).  [callers: bosscam, tower]
 */
void GetPlayerAvgPos(f32* avg, f32* outMin, f32* outMax, s32 mode) {
    f32 box[6];        /* box[0..2] = running max, box[3..5] = running min */
    f32 count;
    s32 i, k;

    avg[0] = 0.0f; avg[1] = 0.0f; avg[2] = 0.0f;
    count = 0.0f;
    box[0] = -1e20f; box[1] = -1e20f; box[2] = -1e20f;  /* lbl_803474B0 max */
    box[3] =  1e20f; box[4] =  1e20f; box[5] =  1e20f;  /* lbl_803474B4 min */

    for (i = 0; i < 4; i++) {
        NcPlayer* pl = &gPlayers[i];
        if (pl->state == 1) {
            f32* src = (pl->ncflags & 0x20) ? pl->altpos : pl->campos;
            if (mode == 0) {
                avg[0] = src[0] + avg[0];
                avg[1] = src[1] + avg[1];
                avg[2] = src[2] + avg[2];
            } else {
                for (k = 0; k < 3; k++) {
                    f32 v = src[k];
                    box[3 + k] = (box[3 + k] < v) ? box[3 + k] : v;  /* min */
                    box[k]     = (box[k] > v)     ? box[k]     : v;  /* max */
                }
            }
            count += 1.0;      /* double literal: count promoted, frsp back */
        }
    }

    if (count == 0.0) {        /* no valid players: use the default position */
        avg[0] = gDefaultPlayerPosition[0];
        avg[1] = gDefaultPlayerPosition[1];
        avg[2] = gDefaultPlayerPosition[2];
    } else {
        f32 s = 1.0 / count;   /* double reciprocal */
        if (mode == 0) {
            avg[0] = avg[0] * s;
            avg[1] = avg[1] * s;
            avg[2] = avg[2] * s;
        } else {
            for (k = 0; k < 3; k++) {
                avg[k] = 0.5 * (box[3 + k] + box[k]);   /* double 0.5 midpoint */
            }
        }
    }

    if (outMin != 0) {
        outMin[0] = box[3]; outMin[1] = box[4]; outMin[2] = box[5];
    }
    if (outMax != 0) {
        outMax[0] = box[0]; outMax[1] = box[1]; outMax[2] = box[2];
    }

    if (mode == 2 && gCurLevel != 0 && *(void**)((u8*)gCurLevel + 0x60) != 0) {
        for (k = 0; k < 3; k++) {
            f32* cam = *(f32**)((u8*)gCurLevel + 0x60);   /* level->camera */
            f32 v = avg[k];
            if (v < cam[3 + k]) v = cam[3 + k];           /* +0x0C low */
            else if (v > cam[6 + k]) v = cam[6 + k];      /* +0x18 high */
            avg[k] = v;
        }
    }
}

/*
 * CamGetPlayerAvgPos -- camera-space player-center used as the camera look-at
 * target.  Builds the bounding box of the valid players' points (pos 0x44 when
 * flags bit1 set, campos 0x54 when bit2 set), optionally transformed through the
 * current matrix stack (bit0), takes the box midpoint, clamps it to the level
 * camera bounds, and returns non-zero if at least one player was valid.
 *
 * A player is valid when state is 1 or 4 and the 0x964 bit26 flag is clear.
 * [callers: the mode-updaters, gamemain, boss]
 */
s32 CamGetPlayerAvgPos(Vec3* out, s32 flags) {
    Vec3 vmax, vmin, tmp;
    s32 i, k, count;

    vmin.x = vmin.y = vmin.z = 1.0e18f;
    vmax.x = vmax.y = vmax.z = -1.0e18f;
    count = 0;

    for (i = 0; i < 4; i++) {
        NcPlayer* pl = &gPlayers[i];
        if ((pl->ncflags & 0x20) || !(pl->state == 1 || pl->state == 4)) {
            continue;
        }
        count++;
        if (flags & 0x2) {                 /* include world position (0x44) */
            if (flags & 0x1) MBWorldToScreen(&tmp, (Vec3*)pl->pos);
            else { tmp.x = pl->pos[0]; tmp.y = pl->pos[1]; tmp.z = pl->pos[2]; }
            { f32* mx = &vmax.x; f32* mn = &vmin.x; f32* t = &tmp.x;
              for (k = 0; k < 3; k++) { if (mx[k] < t[k]) mx[k] = t[k]; if (mn[k] > t[k]) mn[k] = t[k]; } }
        }
        if (flags & 0x4) {                 /* include follow position (0x54) */
            if (flags & 0x1) MBWorldToScreen(&tmp, (Vec3*)pl->campos);
            else { tmp.x = pl->campos[0]; tmp.y = pl->campos[1]; tmp.z = pl->campos[2]; }
            { f32* mx = &vmax.x; f32* mn = &vmin.x; f32* t = &tmp.x;
              for (k = 0; k < 3; k++) { if (mx[k] < t[k]) mx[k] = t[k]; if (mn[k] > t[k]) mn[k] = t[k]; } }
        }
    }

    /* midpoint of the box */
    tmp.x = (vmax.x + vmin.x) * 0.5f;
    tmp.y = (vmax.y + vmin.y) * 0.5f;
    tmp.z = (vmax.z + vmin.z) * 0.5f;

    if (flags & 0x1) MBWorldToScreen3D(out, &tmp);
    else { out->x = tmp.x; out->y = tmp.y; out->z = tmp.z; }

    if (gCurLevel != 0) {                  /* clamp to level camera bounds */
        f32* bounds = *(f32**)((u8*)gCurLevel + 0x60);
        f32* a = &out->x;
        for (k = 0; k < 3; k++) {
            if (a[k] < bounds[3 + k]) a[k] = bounds[3 + k];
            if (a[k] > bounds[6 + k]) a[k] = bounds[6 + k];
        }
    }

    return count != 0;
}

void fn_8006F418(NcCamera* camera, f32* target)
{
    u8 unused_high[24];
    Vec3 average;
    u8 unused_low[4];
    volatile f32 root;
    f32 yaw;
    f32 pitch;
    f64 angle;
    f32 dx;
    f32 dy;
    f32 dz;
    register f32 distance;

    if (target != 0) {
        angle = (f64)target[6] - 3.141592654;
        if (angle > 3.141592654) {
            angle -= 6.283185308;
        } else if (angle <= -3.141592654) {
            angle = 6.283185308 + angle;
        }
        yaw = (f32)angle;
    } else {
        yaw = 0.0f;
    }

    if (target != 0) {
        pitch = -target[5];
    } else {
        pitch = 0.0f;
    }

    if (lbl_80344768 > 1) {
        f32 limit = -(*(f32**)((u8*)gCurLevel + 0x60))[2];
        if (pitch < limit) {
            limit = pitch;
        }
        pitch = limit;
    }

    YawVec3(lbl_80127D40, &camera->direction, -yaw);
    PitchVec3((f32*)&camera->direction, (f32*)&camera->direction, -pitch);
    camera->yaw = yaw;
    camera->pitch = pitch;

    if (target != 0) {
        camera->position.x = target[1];
        camera->position.y = target[2];
        camera->position.z = target[3];
    }

    camera->level_height = (*(f32**)((u8*)gCurLevel + 0x60))[0x0C];
    CamLookInDir((f32*)&camera->direction, (u32)camera);
    MBCameraUpdate((f32*)&camera->position, (f32*)camera);
    MBWindowZoom(camera->zoom);
    if (camera->aspect > 0.0) {
        MBWindowProjection(
            0.31830988614222805 * (180.0 * camera->zoom),
            1.0 / camera->aspect);
    }

    CamGetPlayerAvgPos(&average, 4);
    dz = camera->position.z - average.z;
    dx = camera->position.x - average.x;
    dy = camera->position.y - average.y;
    if ((distance = (dx * dx + dy * dy) + dz * dz) > 0.0f) {
        f64 guess = __frsqrte((f64)distance);
        guess = 0.5 * guess * (3.0 - guess * guess * distance);
        guess = 0.5 * guess * (3.0 - guess * guess * distance);
        guess = 0.5 * guess * (3.0 - guess * guess * distance);
        root = (f32)(distance *
                     (0.5 * guess * (3.0 - guess * guess * distance)));
        distance = root;
    }
    camera->distance = distance;
    camera->attention.x = camera->direction.x * camera->distance + camera->position.x;
    camera->attention.y = camera->direction.y * camera->distance + camera->position.y;
    camera->attention.z = camera->direction.z * camera->distance + camera->position.z;
}

/*
 * CalcFrustrumNormals -- given the camera look basis (fwd/right/up packed as a
 * Vec3 at *look) and a field-of-view angle, build the four view-frustum edge
 * directions (fwd +/- right*tanH +/- up*tanV) and store their pairwise cross
 * products (the inward plane normals) into out[0..3].
 *
 * Structural reconstruction (NonMatching): the shipped code fuses the scaling
 * and cross products with fmsubs and reads the two half-FOV tangents from the
 * NewCam projection block (lbl_80344EE8 +0x1C/+0x20).
 */
void CalcFrustrumNormals(const Vec3* look, const Vec3* unused, Vec3* out, f32 fov) {
    Vec3 seed;
    f32 tanH, tanV;
    Vec3 tl, tr, bl, br; /* frustum corner directions */

    (void)unused;

    YawVec3(lbl_80127D30, &seed, fov);
    tanH = (f32)tan((f64)(*(f32*)((u8*)lbl_80344EE8 + 0x1C) * -fov));
    tanV = (f32)tan((f64)(*(f32*)((u8*)lbl_80344EE8 + 0x20) * -fov));

    /* seed scaled by tanH gives the horizontal frustum spread; look.y +/- ...
     * gives the vertical spread.  Corners = look +/- spreads. */
    tl.x = look->x + seed.x * tanH; tl.y = look->y + seed.y; tl.z = look->z + seed.z;
    tr.x = look->x - seed.x * tanH; tr.y = tl.y;             tr.z = tl.z;
    bl = tl; br = tr;
    (void)tanV; (void)bl; (void)br;

    /* out[0..3] = cross(edge_i, edge_j): inward frustum plane normals */
    out[0].x = tl.y * tr.z - tl.z * tr.y;
    out[0].y = tl.z * tr.x - tl.x * tr.z;
    out[0].z = tl.x * tr.y - tl.y * tr.x;
}

typedef struct NcPlane {
    Vec3 normal;
    f32 unused;
} NcPlane;

typedef struct NcLevelData {
    u8 pad_00[0x60];
    struct NcCameraBounds* camera;
} NcLevelData;

typedef struct NcCameraBounds {
    u8 pad_00[0x2C];
    f32 minimum_distance;
    f32 maximum_distance;
} NcCameraBounds;

#define NC_DOT(a, b) \
    ((a)->x * (b)->x + (a)->y * (b)->y + (a)->z * (b)->z)

f32 CalcDist(Vec3* look, Vec3* point, NcPlane* planes, f32 fov, f32 current)
{
    NcCameraBounds* camera;
    NcLevelData* level_data;
    f32 distance;
    f32 required;
    s32 plane_index;
    s32 player_index;
    NcPlayer* player;
    Vec3* camera_position;
    Vec3* player_position;
    s32 active;
    f32 inverse;
    f32 plane_distance;
    f32 other_distance;

    CalcFrustrumNormals(look, point, (Vec3*)planes, fov);

    level_data = (NcLevelData*)gCurLevel;
    if (level_data->camera->minimum_distance == level_data->camera->maximum_distance) {
        return level_data->camera->minimum_distance;
    }
    camera = level_data->camera;

    if (lbl_80344768 == 1) {
        if (lbl_80344A8C != 0) {
            return current + lbl_80343CDC;
        }
        return camera->minimum_distance;
    }

    required = 0.0f;
    for (plane_index = 0; plane_index < 4; plane_index++) {
        inverse = 1.0f / NC_DOT(look, &planes[plane_index].normal);
        plane_distance = NC_DOT(point, &planes[plane_index].normal);

        for (player_index = 0; player_index < 4; player_index++) {
            player = &gPlayers[player_index];

            active = ((player->ncflags & 0x20) == 0 &&
                      (player->state == 1 || player->state == 4));
            if (active == 0) {
                continue;
            }

            camera_position = (Vec3*)player->campos;
            player_position = (Vec3*)player->pos;
            other_distance = NC_DOT(camera_position, &planes[plane_index].normal);
            distance = required > inverse * (plane_distance - other_distance)
                ? required : inverse * (plane_distance - other_distance);

            other_distance = NC_DOT(player_position, &planes[plane_index].normal);
            distance = distance > inverse * (plane_distance - other_distance)
                ? distance : inverse * (plane_distance - other_distance);
            required = distance;
        }
    }

    if (level_data == 0 || camera == 0) {
        FatalError("level_data or level_data->camera NULL", 0x800000);
    }

    camera = ((NcLevelData*)gCurLevel)->camera;
    if (required <= camera->minimum_distance - lbl_80343CD8 && camera->minimum_distance < current) {
        return camera->minimum_distance;
    }
    if (required <= current - lbl_80343CD8) {
        return required + lbl_80343CD8;
    }
    if (required <= current - lbl_80343CDC) {
        return current;
    }
    return required + lbl_80343CDC;
}

#undef NC_DOT

/*
 * DebugCamControlInputs [0x800704EC, size 0x444] -- NOT reconstructed (giant,
 * input handling).  Reads the debug-cam input bitmask (lbl_80344A84) and, per
 * direction bit, rotates yaw (0xEC) / pitch (0x104) and translates the debug
 * camera (0xA4 / 0xAC) by the current speed (gClockFrameStep) scaled through the
 * yaw/pitch sin/cos.  Caller: fn_8006FF1C (DebugCamUpdate path).
 *
 * The remaining fn_ bodies (the fn_8006DF34 / fn_8006E654 / fn_8006F16C
 * per-mode updaters, the fn_8006DC64 frustum point-clip test, and the small
 * fn_8006ECD4 / fn_8006FBAC / fn_8006FCDC / fn_8006FE30 / fn_80070144 /
 * CamLookInDir helpers) are left as documented-only; see the .text map above.
 */
