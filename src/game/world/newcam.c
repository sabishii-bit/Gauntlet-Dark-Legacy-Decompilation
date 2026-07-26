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
 *                                          transform via fn_800B5554/B53B4),
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
 *   0x8006EC18 fn_8006EC18          0xBC  toggle debug-overlay blit
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
 *   0x8006FBAC fn_8006FBAC          0x130 calls fn_800BCB44 [bosscam]
 *   0x8006FCDC fn_8006FCDC          0x154 calls fn_800BA6C0 [internal]
 *   0x8006FE30 fn_8006FE30          0xEC  camera+projection setup (CamReset + proj
 *                                          params lbl_80344EE8) [game/sys/main]
 *   0x8006FF1C fn_8006FF1C          0x228 debug-camera update path; "CAM Y" HUD;
 *                                          calls CamReset/DebugCamControlInputs;
 *                                          DoShake [game/sys/main x2]
 *                                          (candidate: DebugCamUpdate)
 *   0x80070144 fn_80070144          0x1FC pure matrix/vector math (no calls) [fn_8006DF34]
 *   0x80070340 fn_80070340          0x1AC shared cam helper (fn_800BD9B0/BE8C8);
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
    u8  _000[0x0A4];
    f32 tx;            /* 0x0A4 translate accumulator X */
    f32 _0A8;          /* 0x0A8 */
    f32 tz;            /* 0x0AC translate accumulator Z */
    u8  _0B0[0x0EC - 0x0B0];
    f32 yaw;           /* 0x0EC */
    u8  _0F0[0x104 - 0x0F0];
    f32 pitch;         /* 0x104 */
    u8  _108[0x1A4 - 0x108];
    s32 field_1A4;     /* 0x1A4 */
    s32 field_1A8;     /* 0x1A8 (reset to -1) */
    f32 field_1AC;     /* 0x1AC */
} NcCamera;            /* 0x1B0 */

/*
 * Player record view (subset the camera reads).  Full record is game/player.h
 * Player (0x335C, base lbl_80275AE0, stride 0x335C).  Local view keeps newcam
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
extern NcPlayer  lbl_80275AE0[4];   /* the 4 player records (game/player.h Player[]) */
extern f32       lbl_802757D4[3];   /* default position when no player is valid */
extern NcCamera  lbl_80274AA0;      /* DebugCamera instance */
extern NcCamera* lbl_80344A68;      /* DebugCam: pointer to the live debug camera */
extern s32       lbl_80344A7C;      /* debug-camera active flag */
extern s32       lbl_80344A90;      /* StdCam freeze flag */
extern void*     gCurLevel;         /* level record; +0x60 = active CAMERA* (bounds) */

/* NEWCAM projection-parameter block (half-FOV tangents at +0x1C / +0x20). */
extern u8 lbl_80344EE8[];
/* seed vector for fn_800BDE08 (a fixed unit direction). */
extern const u8 lbl_80127D30[];

/* ----- external helpers (G3D math layer) ----- */
extern double tan(double);
/* rotate/derive a unit vector from a constant seed. */
extern void fn_800BDE08(const void* seed, Vec3* out);
/* transform a point through the current matrix stack (dst <- M * src). */
extern void fn_800B5554(Vec3* dst, const Vec3* src);
extern void fn_800B53B4(Vec3* dst, const Vec3* src);

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
void CamReset(NcCamera* cam) {
    u8* p = (u8*)cam;
    s32 i;

    /* bulk-clear the working set: matrix (0x40..0x78), orientation/target block
     * (0xA4..0x110) and the per-frame history rows (0x114..0x294). */
    for (i = 0x40; i < 0x294; i += 4) {
        *(f32*)(p + i) = 0.0f;
    }
    cam->field_1A4 = 0;
    cam->field_1A8 = -1;   /* no active target */
    cam->field_1AC = 0.0f;

    /* clear the module-wide current/freeze state */
    lbl_80344A90 = 0;      /* unfreeze */
}

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
 * default position (lbl_802757D4) is used.  bmax/bmin, when non-NULL, receive
 * the bounding box (meaningful only for mode>0).  [callers: bosscam, tower]
 */
void GetPlayerAvgPos(Vec3* avg, Vec3* bmax, Vec3* bmin, s32 mode) {
    Vec3 vmax, vmin;
    f32* src;
    f32 count;
    s32 i, k;

    avg->x = avg->y = avg->z = 0.0f;
    vmin.x = vmin.y = vmin.z = 1.0e18f;    /* lbl_803474B0 */
    vmax.x = vmax.y = vmax.z = -1.0e18f;   /* lbl_803474B4 */
    count = 0.0f;

    for (i = 0; i < 4; i++) {
        NcPlayer* pl = &lbl_80275AE0[i];
        if (pl->state != 1) {
            continue;
        }
        src = (pl->ncflags & 0x20) ? pl->altpos : pl->campos;
        if (mode == 0) {
            avg->x += src[0];
            avg->y += src[1];
            avg->z += src[2];
        } else {
            f32* mx = &vmax.x;
            f32* mn = &vmin.x;
            for (k = 0; k < 3; k++) {
                if (mx[k] < src[k]) mx[k] = src[k];
                if (mn[k] > src[k]) mn[k] = src[k];
            }
        }
        count += 1.0f;
    }

    if (count == 0.0f) {
        avg->x = lbl_802757D4[0];
        avg->y = lbl_802757D4[1];
        avg->z = lbl_802757D4[2];
    } else if (mode == 0) {
        f32 s = 1.0f / count;
        avg->x *= s;
        avg->y *= s;
        avg->z *= s;
    } else {
        avg->x = (vmax.x + vmin.x) * 0.5f;
        avg->y = (vmax.y + vmin.y) * 0.5f;
        avg->z = (vmax.z + vmin.z) * 0.5f;
    }

    if (bmax) {
        bmax->x = vmax.x; bmax->y = vmax.y; bmax->z = vmax.z;
    }
    if (bmin) {
        bmin->x = vmin.x; bmin->y = vmin.y; bmin->z = vmin.z;
    }

    if (mode == 2 && gCurLevel != 0) {
        f32* bounds = *(f32**)((u8*)gCurLevel + 0x60);   /* level->camera */
        if (bounds != 0) {
            f32* a = &avg->x;
            for (k = 0; k < 3; k++) {
                if (a[k] < bounds[3 + k]) a[k] = bounds[3 + k];   /* +0x0C low */
                if (a[k] > bounds[6 + k]) a[k] = bounds[6 + k];   /* +0x18 high */
            }
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
        NcPlayer* pl = &lbl_80275AE0[i];
        if ((pl->ncflags & 0x20) || !(pl->state == 1 || pl->state == 4)) {
            continue;
        }
        count++;
        if (flags & 0x2) {                 /* include world position (0x44) */
            if (flags & 0x1) fn_800B5554(&tmp, (Vec3*)pl->pos);
            else { tmp.x = pl->pos[0]; tmp.y = pl->pos[1]; tmp.z = pl->pos[2]; }
            { f32* mx = &vmax.x; f32* mn = &vmin.x; f32* t = &tmp.x;
              for (k = 0; k < 3; k++) { if (mx[k] < t[k]) mx[k] = t[k]; if (mn[k] > t[k]) mn[k] = t[k]; } }
        }
        if (flags & 0x4) {                 /* include follow position (0x54) */
            if (flags & 0x1) fn_800B5554(&tmp, (Vec3*)pl->campos);
            else { tmp.x = pl->campos[0]; tmp.y = pl->campos[1]; tmp.z = pl->campos[2]; }
            { f32* mx = &vmax.x; f32* mn = &vmin.x; f32* t = &tmp.x;
              for (k = 0; k < 3; k++) { if (mx[k] < t[k]) mx[k] = t[k]; if (mn[k] > t[k]) mn[k] = t[k]; } }
        }
    }

    /* midpoint of the box */
    tmp.x = (vmax.x + vmin.x) * 0.5f;
    tmp.y = (vmax.y + vmin.y) * 0.5f;
    tmp.z = (vmax.z + vmin.z) * 0.5f;

    if (flags & 0x1) fn_800B53B4(out, &tmp);
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
void CalcFrustrumNormals(const Vec3* look, f32 fov, Vec3* out) {
    Vec3 seed;
    f32 tanH, tanV;
    Vec3 tl, tr, bl, br; /* frustum corner directions */

    fn_800BDE08(lbl_80127D30, &seed);
    tanH = (f32)tan((f64)(*(f32*)(lbl_80344EE8 + 0x1C) * -fov));
    tanV = (f32)tan((f64)(*(f32*)(lbl_80344EE8 + 0x20) * -fov));

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

/*
 * CalcDist [0x8006ED50, size 0x248] -- NOT reconstructed (medium-complex).
 * Builds the frustum planes (CalcFrustrumNormals), then for each valid player
 * projects the player onto the four inward plane normals and keeps the largest
 * distance the camera must sit back to keep every player inside the frustum;
 * FatalError("level_data or level_data->camera NULL") if the level/camera is
 * missing.  Returns the clamped fit distance.  Caller: fn_8006DF34.
 *
 * DebugCamControlInputs [0x800704EC, size 0x444] -- NOT reconstructed (giant,
 * input handling).  Reads the debug-cam input bitmask (lbl_80344A84) and, per
 * direction bit, rotates yaw (0xEC) / pitch (0x104) and translates the debug
 * camera (0xA4 / 0xAC) by the current speed (lbl_80344590) scaled through the
 * yaw/pitch sin/cos.  Caller: fn_8006FF1C (DebugCamUpdate path).
 *
 * The remaining fn_ bodies (the fn_8006DF34 / fn_8006E654 / fn_8006F16C
 * per-mode updaters, the fn_8006DC64 frustum point-clip test, and the small
 * fn_8006ECD4 / fn_8006FBAC / fn_8006FCDC / fn_8006FE30 / fn_80070144 /
 * fn_80070340 helpers) are left as documented-only; see the .text map above.
 */
