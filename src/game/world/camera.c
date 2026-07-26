/* camera.c -- head of CAMERA.OBJ (game camera system), NonMatching.
 *
 * Function names recovered from shell3D.pdb (CAMERA.OBJ) and confirmed
 * behaviorally (call-graph + string + data cross-reference).  CAMERA is a
 * large, function-order-scrambled TU; this file covers only the head window
 * .text 0x80022734 .. 0x8002951C.  The remaining CAMERA functions (0x8002951C
 * onward, up to ~0x80031Exx) are still auto-split.
 *
 * Mapped in this window (see config/GUNE5D/symbols.txt):
 *   0x80022734 get_screen_pos         world pos -> INT screen x,y (fn_800B5554)  [global]
 *   0x80022794 get_actual_screen_pos  per-camera projection -> FLOAT x,y         [global]
 *   0x80022824 LookInDirection        build camera basis from a look vector      [global, 21 callers]
 *   0x800229D0 do_camera              top-level per-frame camera update          [global, main loop]
 *   0x80022DAC (fn_80022DAC)          do_camera pre-dispatch setup
 *   0x800231D4 (fn_800231D4)          camera-mode state-machine dispatcher (jumptables)
 *   0x80023ED0 (fn_80023ED0)          camera-mode handler
 *   0x80024F30 (fn_80024F30)          camera-mode handler (atan2 aiming)
 *   0x80025640 (fn_80025640)          target/object-type debug overlay (dbgTextPrintfCol)
 *   0x80025CEC (fn_80025CEC)          large camera-mode handler
 *   0x80026CA4 (fn_80026CA4)          small predicate
 *   0x80026CF0 (fn_80026CF0)          camera-mode handler
 *   0x80027608 DoShake                apply active shake offset to a position     [global, 23 callers]
 *   0x80027838 ShakeCamera            trigger a shake (priority-gated)            [global]
 *   0x80027870 (fn_80027870)          helper
 *   0x80027CA4 (fn_80027CA4)          helper
 *   0x80028394 (fn_80028394)          camera-mode handler (sin/cos)
 *   0x80028560 (fn_80028560)          helper for fn_80028394
 *   0x80028670 (fn_80028670)          camera-mode handler
 *   0x80028938 (fn_80028938)          helper for fn_80028670
 *   0x80028A74 (fn_80028A74)          large handler, prints "DEST P=%d, Y=%4.1f"
 *
 * NonMatching: dtk links the original DOL bytes for this range; the bodies
 * below document the confidently-recovered functions and are not byte-matched.
 */

#include "types.h"
#include "game/camera.h"

typedef struct Vec3 {
    f32 x;
    f32 y;
    f32 z;
} Vec3;

/* --- camera shake state (CAMERA.OBJ .sbss globals, names from PDB) --- */
extern s32 shake_type;      /* 0x80344478 */
extern s32 shaking;         /* 0x8034447C */
extern s32 shake_priority;  /* 0x80344480 */
extern s32 shake_count;     /* 0x80344484 */
extern s32 shake_delay;     /* 0x80344488 */
extern f32 shake_rad;       /* 0x8034448C */

/* --- external projection / math helpers (G3D / pb layer) --- */
void fn_800B5554(f32* out_xy, void* world_pos);            /* screen projection (INT path) */
void fn_800BB8E8(void* cam, int mode, short* out_xy, void* world_pos); /* per-camera projection */

/* gCameras[6] is declared in game/camera.h (@0x8023F8D0, stride 0x18C). */

/*
 * get_screen_pos -- project a world position to integer screen coordinates.
 * arg0 is passed but unused by the projection path.
 */
void get_screen_pos(int unused, int* xo, int* yo, void* world_pos) {
    f32 sp[2];
    (void)unused;
    fn_800B5554(sp, world_pos);
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
    fn_800BB8E8(cam->mat, 0, scr, world_pos);
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
