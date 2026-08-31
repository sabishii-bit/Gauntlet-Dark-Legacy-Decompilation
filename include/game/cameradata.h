#ifndef GAME_CAMERADATA_H
#define GAME_CAMERADATA_H

#include "types.h"

/*
 * cameradata.h -- shared VIEW of the camera-supervisor .bss block that
 * src/game/game/combat.c and src/game/world/camera.c walk through the
 * `gCameraState` base symbol.
 *
 * WHY A VIEW AND NOT A TYPE FOR gCameraState
 * ------------------------------------------
 * `gCameraState` is NOT one object.  Per config/GUNE5D/symbols.txt its linked
 * symbol is only 0x34 bytes, yet both TUs address well past +0x97C off it.
 * The walk crosses five CONSECUTIVE .bss symbols that the linker happens to
 * lay out contiguously:
 *
 *   0x8023F808  gCameraState             size 0x34    -> view +0x00 .. +0x34
 *   0x8023F83C  gRecorderCameraPosition  size 0x0C    -> view +0x34 .. +0x40
 *   0x8023F848  lbl_8023F848             size 0x1C    -> view +0x40 .. +0x5C
 *   0x8023F864  gCameraTargetPositions   size 0x6C    -> view +0x5C .. +0xC8
 *   0x8023F8D0  gCameras                 size 0x948   -> view +0xC8 .. +0xA10
 *   0x80240218  gCameraTargets           size 0x348   -> view +0xA10 .. +0xD58
 *
 * (Each symbol's end address equals the next symbol's start, so the run is
 * gap-free; 0x8023F8D0 - 0x8023F808 == 0xC8 is where gCameras begins, and
 * 0x80240218 - 0x8023F808 == 0xA10 is where the target array begins.)
 *
 * Therefore this header does NOT retype gCameraState and MUST NOT be used to
 * re-base any access.  Per claim.law.walked-base-symbol-identity the symbol a
 * displacement belongs to is a per-function fact about the target's own
 * relocations, not something inferable from the offset.  That check was run
 * for this region and is recorded here as verified evidence:
 *
 *   tools/gdl/fnasm.py <unit> <fn>, reading the lis/addi relocation pair:
 *     combat.c   DiffRate_8002951C, cam_orient_to_80029E8C, init_game_cam,
 *                get_cam_wpos_8002ABE0, someone_will_be_off_screen,
 *                StandardCamera_8002B828, add_target, get_attn_pos_8002C9A8,
 *                InitCamera, ProcCamera_8002E548 ......... @gCameraState
 *     camera.c   do_camera, camera_init_for_gamemode, camera_mode_follow,
 *                camera_orbit_update, camera_mode_level, camera_mode_spin,
 *                debug_camera_pos, camera_debug_supervisor ... @gCameraState
 *     camera.c   camera_collide_step ......................... @gCameras
 *
 * Every one of those functions materialises exactly ONE base register and
 * reaches the whole block by displacement -- including the camera fields at
 * +0xC8 and beyond, which are NOT relocated against `gCameras`.  The single
 * exception is camera_collide_step, which really does relocate against
 * `gCameras`: its accesses must stay `&gCameras[i]`-derived and must NOT be
 * routed through this view.
 *
 * HOW TO USE IT
 * -------------
 * Only as an offsetof() addend on the ORIGINAL raw base expression, never as
 * a pointer cast and never as a replacement base:
 *
 *     *(f32*)(cs + offsetof(CameraStateData, attn_min[0]))          -- yes
 *     ((CameraStateData*)gCameraState)->attn_min[0]                 -- NO
 *
 * The cast form would change the alias/CSE web (claim.law.typed-global-member
 * -vs-view-cast-diverges) and re-attribute the address to the wrong symbol.
 *
 * Fields at +0xC8 and above belong to the `Camera` array and are spelled with
 * game/camera.h's already-verified `Camera` type instead, as
 *
 *     cs + CAMERA_STATE_CAMERAS_OFF + i * CAMERA_STATE_CAMERA_STRIDE
 *        + offsetof(Camera, field)
 *
 * so that no aggregate-typed member ever enters this header -- see
 * claim.law.embedded-struct-member-whole-tu-cascade, which showed that adding
 * a struct-typed member to a shared header can regress unrelated functions
 * across the whole including TU.  Everything below is a scalar or an array of
 * scalars for exactly that reason.
 */

/* gCameras - gCameraState.  Verified from symbols.txt addresses above. */
#define CAMERA_STATE_CAMERAS_OFF 0xC8

/* gCameraTargets - gCameraState (0xA10 == 2576), i.e. one element past the
 * end of the six-camera array.  Both TUs reconstruct the element type as a
 * 0x38-byte file-local `CameraTarget`; that type stays file-local here too,
 * because it is an aggregate (embedded-struct-member-whole-tu-cascade). */
#define CAMERA_STATE_TARGETS_OFF 0xA10

/* sizeof(Camera) == 0x18C.  Keep this spelled as the literal 396 in loop
 * strides: claim.law.sizeof-defeats-loop-stride-induction says a sizeof()
 * stride breaks MWCC's induction-variable form. */
#define CAMERA_STATE_CAMERA_STRIDE 396

/*
 * Scalar view of the 0xC8-byte prefix that precedes the camera array.
 *
 * Field-name evidence (behaviour-derived; no PDB record exists for this block
 * -- `gdlmem.py struct CAMERASTATE` returns no match):
 *   +0x10 unk10       zeroed as a 3-element run by InitCamera; use unknown,
 *                     so the name is deliberately left neutral.
 *   +0x1C start_wpos  InitCamera seeds cameras[0].wpos from these three
 *                     (x scaled by sin, z by cos, y copied straight).
 *   +0x28 start_attn  same three-term seeding of cameras[0].attn.
 *   +0x34 recorder_wpos   IS gRecorderCameraPosition; recorder.c copies its
 *                     saved camera position in and out of it.
 *   +0x40 reticle_depth   IS lbl_8023F848[7]; recorder.c mirrors it to a
 *                     static named sReticleDepth, one entry per reticle.
 *   +0x5C reticle_pos IS gCameraTargetPositions[0..20]; recorder.c mirrors it
 *                     to sReticlePos[7][3].
 *   +0xB0 attn_max    get_attn_pos_8002C9A8 clamps a candidate attention point
 *   +0xBC attn_min    DOWN to attn_max and UP to attn_min, per component --
 *                     the comparison direction in that loop is what fixes
 *                     which of the two trailing vectors is which.
 *
 * WIDTH RECONCILIATION (recorded, not guessed): gCameraTargetPositions is
 * declared `[27]` in combat.c but `[7][3]` (== 21) in camera.c and
 * recorder.c.  symbols.txt gives size 0x6C == 108 bytes == 27 f32, so `[27]`
 * is the true extent and `[7][3]` is a partial under-declaration covering
 * only the reticle positions.  The 6 f32 it omits are exactly the attn_max /
 * attn_min pair above, which is why they read as "past the end" of the
 * target-position array.  The two declarations do not contradict each other
 * about any shared byte; neither was changed by this pass.
 */
typedef struct CameraStateData {
    /* 0x00 */ u8  _pad00[0x10];    /* untouched by combat.c and camera.c */
    /* 0x10 */ f32 unk10[3];
    /* 0x1C */ f32 start_wpos[3];
    /* 0x28 */ f32 start_attn[3];
    /* 0x34 */ f32 recorder_wpos[3];
    /* 0x40 */ f32 reticle_depth[7];
    /* 0x5C */ f32 reticle_pos[21];
    /* 0xB0 */ f32 attn_max[3];
    /* 0xBC */ f32 attn_min[3];
} CameraStateData; /* 0xC8 == CAMERA_STATE_CAMERAS_OFF */

#endif /* GAME_CAMERADATA_H */
