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
 *   0x800229D0 do_camera                top-level per-frame camera update       [global]  giant, doc-only
 *   0x80022DAC camera_init_for_gamemode setup driven by game-mode g_800229D0    (local)   giant, doc-only
 *   0x800231D4 camera_run_mode          camera-mode state machine (2 jumptables)(local)   giant, doc-only
 *   0x80023ED0 camera_mode_follow       largest mode handler (MB blit, project) (local)   giant, doc-only
 *   0x80024F30 camera_mode_target       atan2 aiming toward a target            (local)   giant, doc-only
 *   0x80025640 debug_camera_pos         object-type + position debug overlay    (local)   giant, doc-only
 *   0x80025CEC camera_debug_supervisor  largest fn; drives debug_camera_pos      (local)   giant, doc-only
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
 *   0x80028A74 camera_mode_dest         scripted move-to ("DEST P=%d, Y=%4.1f") (local)   giant, doc-only
 *
 * Data globals in the 0x80344xxx range (shake/state) are SHARED with other
 * TUs (attract.c, sndfx.c, auxscreen.c, ...) and are left under their existing
 * names; only the shake_* set carries confident PDB names.
 */

#include "types.h"
#include "game/camera.h"

typedef struct Vec3 {
    f32 x;
    f32 y;
    f32 z;
} Vec3;

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

/* --- external projection / math helpers (G3D / pb layer) --- */
void MBWorldToScreen(f32* out_xy, void* world_pos);                   /* screen projection (INT path) */
void MBWindowProject(void* cam, int mode, short* out_xy, void* world_pos); /* per-camera projection */
f32  FixAngle(f32 rad);                                            /* angle wrap/reduce for sin/cos */
extern f64 sin(f64 x);
extern f64 cos(f64 x);

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
    MBWindowProject(cam->mat, 0, scr, world_pos);
    *xo = (f32)scr[0];
    *yo = (f32)scr[1];
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

    if (!shaking) {
        return;
    }
    if (gGameBusy | gGameplayPauseTimer) {
        return;
    }

    shake_delay -= gFrameTicks;
    if (shake_delay < 0) {
        shaking = 0;
        shake_priority = 0;
    }
    shake_count -= gFrameTicks;
    if (shake_count < 0) {
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

/*
 * camera_approach_yaw -- rate-limit the camera's yaw field (cam+0xA8) toward
 * `target`, wrapping through the shortest arc, at up to ~2 degrees per frame
 * step (scaled by the frame delta).  Returns the new yaw; the caller stores
 * it back.  Snaps to the target once within one step.
 */
f32 camera_approach_yaw(void* cam, f32 target) {
    f32 cur = CAM_F32(cam, CAM_YAW_OFF);
    f32 delta = cam_wrap_pi(target - cur);
    f32 step = (f32)(CAM_TURN_RATE_2DEG * (f64)gFrameTicks);
    f32 out;

    if (delta >= 0.0f) {
        out = (delta <= step) ? target : (f32)(cur + step);
    } else {
        out = (delta >= -step) ? target : (f32)(cur - step);
    }
    return cam_wrap_pi(out);
}

/*
 * camera_lerp_yaw -- rate-limit one angle (`current`) toward another
 * (`target`), wrapping through the shortest arc, at up to ~1 degree per frame
 * step (scaled by the frame delta).  Snaps to the target once within ~1
 * degree.  Returns the new angle.
 */
f32 camera_lerp_yaw(f32 current, f32 target) {
    f32 delta = cam_wrap_pi(target - current);
    f32 mag = (delta < 0.0f) ? -delta : delta;
    f32 step;
    f32 out;

    if (mag <= (f32)CAM_TURN_RATE_1DEG) {
        return cam_wrap_pi(target);
    }
    step = (f32)(CAM_TURN_RATE_1DEG * (f64)gFrameTicks);
    if (delta >= 0.0f) {
        out = (f32)(current + step);
    } else {
        out = (f32)(current - step);
    }
    return cam_wrap_pi(out);
}
