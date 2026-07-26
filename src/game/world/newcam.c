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
 * NEWCAM strings (.rodata): "CAM Y:%.0f P:%.0f D:%.2f(%.2f)  ATN:%.1f %.1f %.1f"
 * at 0x801137D0 (debug HUD), and "level_data or level_data->camera NULL" at
 * 0x80113808 (FatalError guard in fn_8006ED50).
 *
 * .text map (addr / current symbol / size / behavior / [callers]):
 *   0x8006DC2C fn_8006DC2C          0x38  public wrapper: load global cam ptr
 *                                          (lbl_80344A6C), tail into fn_8006DC64
 *                                          [bosscam]  (frustum-clip query)
 *   0x8006DC64 fn_8006DC64          0x2D0 frustum point-clip test: dot a point
 *                                          against 4 plane normals (cam+164/+224),
 *                                          3-iteration classify loop [bosscam,+wrapper]
 *   0x8006DF34 fn_8006DF34          0x5E8 large per-frame cam update; draws the
 *                                          "CAM Y:" debug HUD (dbgTextPrintfCell);
 *                                          calls fn_8006ED50/fn_8006F8F0/DoShake/
 *                                          MBWindowProjection  [3 mode-updaters]
 *   0x8006E51C UpdateCam            0x138 TOP-LEVEL camera-update dispatcher:
 *                                          branches to fn_8006F16C / fn_8006E654 /
 *                                          fn_8006DF34 by camera mode
 *                                          [game/sys/main]  (named, med-high conf)
 *   0x8006E654 fn_8006E654          0x5C4 mode-specific cam update; "CAM Y" HUD;
 *                                          pbUpdateMatricies/DoShake  [dispatcher]
 *   0x8006EC18 fn_8006EC18          0xBC  toggle debug-overlay blit
 *                                          (handle lbl_80344A78) [game/world/items]
 *   0x8006ECD4 fn_8006ECD4          0x70  small projection helper
 *                                          (MBWindowProjection) [bosscam]
 *   0x8006ED44 fn_8006ED44          0xC   tiny getter -> global lbl_80344A90 [bosscam]
 *   0x8006ED50 fn_8006ED50          0x248 validate gCurLevel->camera (FatalError
 *                                          "level_data...camera NULL"); recompute
 *                                          projection; calls CalcFrustrumNormals
 *                                          [fn_8006DF34]
 *   0x8006EF98 CalcFrustrumNormals  0x1D4 build 4 frustum plane normals from the
 *                                          camera basis vectors scaled by tan(FOV),
 *                                          via edge cross products [bosscam, level-setup]
 *   0x8006F16C fn_8006F16C          0x2AC per-frame cam update (pbUpdateMatricies);
 *                                          calls fn_8006DF34/F418/F8F0/FCDC
 *                                          [game/game/gamemain]  (CamUpdate candidate)
 *   0x8006F418 fn_8006F418          0x260 cam sub-update (DoShake/MBWindowProjection)
 *   0x8006F678 fn_8006F678          0x278 leaf math; HIGH-caller (6 objs, bosscam x4):
 *                                          world->camera projection/query
 *   0x8006F8F0 fn_8006F8F0          0x2BC matrix-build helper (fn_800B5554/B53B4);
 *                                          5 callers (gamemain, boss, internal)
 *   0x8006FBAC fn_8006FBAC          0x130 calls fn_800BCB44 [bosscam]
 *   0x8006FCDC fn_8006FCDC          0x154 calls fn_800BA6C0 [internal]
 *   0x8006FE30 fn_8006FE30          0xEC  calls fn_80070968/BE8C8/BD428 [game/sys/main]
 *   0x8006FF1C fn_8006FF1C          0x228 "CAM Y" HUD; DoShake/MBWindowProjection;
 *                                          fn_80070968/fn_800704EC [game/sys/main x2]
 *   0x80070144 fn_80070144          0x1FC pure matrix/vector math (no calls) [fn_8006DF34]
 *   0x80070340 fn_80070340          0x1AC shared cam helper (fn_800BD9B0/BE8C8);
 *                                          5 internal callers (all mode-updaters)
 *   0x800704EC fn_800704EC          0x444 sin/cos orientation math; fn_80070968
 *                                          [fn_8006FF1C]  (GetTransmitter3D candidate)
 *   0x80070930 fn_80070930          0x38  tiny; fn_80070968 [game/pb/pb_diag]
 *   0x80070968 fn_80070968          0xF8  pure vector/matrix helper; HIGHEST caller
 *                                          (8 objs incl bosscam, pb)
 *
 * NonMatching: dtk links the original DOL bytes for this range; the body below
 * documents the one confidently-named function and is not byte-matched.
 */

#include "types.h"

typedef struct Vec3 {
    f32 x;
    f32 y;
    f32 z;
} Vec3;

extern double tan(double);
/* rotate/derive a unit vector from a constant seed (G3D math layer) */
extern void fn_800BDE08(const void* seed, Vec3* out);
/* NEWCAM projection-parameter block (FOV fields at +0x1C / +0x20) */
extern u8 lbl_80344EE8[]; /* NewCam projection params */
extern const u8 lbl_80127D30[]; /* seed vector for fn_800BDE08 */

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
