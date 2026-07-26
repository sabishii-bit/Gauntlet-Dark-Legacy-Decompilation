#ifndef GAME_CAMERA_H
#define GAME_CAMERA_H

#include "types.h"

/*
 * Camera -- game camera / view descriptor.
 *
 * Source of field names + offsets: shell3D.pdb "struct CAMERA" (Id=2249,
 * Size=0x18c), see research/xbox_symbols/misc.h.  The GameCube (GUNE5D) port
 * kept the layout byte-identical: the global camera array
 *
 *     CAMERA gCameras[6];   // @0x8023F8D0, stride 0x18C (already named)
 *
 * has a 0x18C element stride, matching the Xbox struct size exactly.
 *
 * GC offsets verified against camera.c (CAMERA.OBJ) target disassembly:
 *   get_actual_screen_pos: mulli r_,r_,396 (0x18C stride) + cam+4 -> mat
 *   field loads/stores on &gCameras[i]:
 *     0x04 mat   0x64 wpos  0xA4 pyr  0xC4 radius  0xC8 trans_mode
 *     0xD0 mode  0xE0 num1  0xE4 num2 0xE8 num3    0xEC c_mode
 *     0xF4 camobj (OBJGRP*)  0xF8 a_mode  0x100 pn 0x118 attnobj (OBJGRP*)
 *     0x12C attn 0x14C delta 0x16C attn_dest_no_offset
 *   Every offset < 0x18C matches the Xbox layout; no GC deltas found.
 *
 * The struct is naturally 4-byte packed to exactly 0x18C -- no padding needed.
 */

struct OBJGRP; /* forward decl -- game object group (defined elsewhere) */

/* Camera control mode (shell3D.pdb enum CAM_MODE). */
typedef enum CAM_MODE {
    CAM_OFF     = 0,
    CAM_FREE    = 1,
    CAM_LOCK    = 2,
    CAM_GAME    = 3,
    CAM_OBJEYE  = 4,
    CAM_VECDIST = 5,
    CAM_POINT   = 6,
    CAM_DRAGON  = 7,
    CAM_CHIMERA = 8,
    CAM_DJINN   = 9,
    CAM_DRIDER  = 10,
    CAM_DEMON   = 11,
    CAM_BOSS    = 12
} CAM_MODE;

/* Attention / look-at target mode (shell3D.pdb enum ATN_MODE). */
typedef enum ATN_MODE {
    ATN_FREE      = 0,
    ATN_LOCK      = 1,
    ATN_TARGET    = 2,
    ATN_OBJECT    = 3,
    ATN_POINT     = 4,
    ATN_PLAYER    = 5,
    ATN_ENEMY     = 6,
    ATN_ITEM      = 7,
    ATN_MILESTONE = 8,
    ATN_LOOKOUT   = 9,
    ATN_CAMERA    = 10
} ATN_MODE;

typedef struct Camera {
    /* 0x000 */ s32              state;         /* shell3D: enum state (INACTIVE/ACTIVE/...) */
    /* 0x004 */ f32              mat[4][4];     /* camera transform matrix */
    /* 0x044 */ f32              limit_pos[4];
    /* 0x054 */ f32              limit_vel[4];
    /* 0x064 */ f32              wpos[4];       /* world position */
    /* 0x074 */ f32              old_wpos[4];
    /* 0x084 */ f32              vel[4];        /* linear velocity */
    /* 0x094 */ f32              avel[4];       /* angular velocity */
    /* 0x0A4 */ f32              pyr[4];        /* pitch/yaw/roll */
    /* 0x0B4 */ f32              pyr_delta[4];
    /* 0x0C4 */ f32              radius;
    /* 0x0C8 */ s32              trans_mode;
    /* 0x0CC */ s32              timer;
    /* 0x0D0 */ s32              mode;
    /* 0x0D4 */ s32              flags;
    /* 0x0D8 */ s32              unvib;
    /* 0x0DC */ f32              value;
    /* 0x0E0 */ f32              num1;
    /* 0x0E4 */ f32              num2;
    /* 0x0E8 */ f32              num3;
    /* 0x0EC */ CAM_MODE         c_mode;        /* current camera mode */
    /* 0x0F0 */ CAM_MODE         pc_mode;       /* previous camera mode */
    /* 0x0F4 */ struct OBJGRP*   camobj;
    /* 0x0F8 */ ATN_MODE         a_mode;        /* current attention mode */
    /* 0x0FC */ ATN_MODE         pa_mode;       /* previous attention mode */
    /* 0x100 */ s32              pn;            /* player index */
    /* 0x104 */ s32              en;            /* enemy index */
    /* 0x108 */ s32              gn;
    /* 0x10C */ s32              mn;
    /* 0x110 */ s32              ln;
    /* 0x114 */ s32              cn;
    /* 0x118 */ struct OBJGRP*   attnobj;
    /* 0x11C */ f32              offset[4];
    /* 0x12C */ f32              attn[4];       /* current attention point */
    /* 0x13C */ f32              old_attn[4];
    /* 0x14C */ f32              delta[4];
    /* 0x15C */ f32              attn_dest[4];
    /* 0x16C */ f32              attn_dest_no_offset[4];
    /* 0x17C */ f32              cam_dest[4];
} Camera; /* sizeof == 0x18C */

/* The engine's camera array: 6 views, @0x8023F8D0 (GC). */
extern Camera gCameras[6];

#endif /* GAME_CAMERA_H */
