#ifndef GAME_NEWCAM_H
#define GAME_NEWCAM_H

#include "types.h"

/* NEWCAM camera object - the Xbox StdCamera/DebugCamera CAMERA type, NOT the
 * 0x18C CAMERA.OBJ element that game/camera.h models as `Camera` and that
 * gCameras[] holds.
 *
 * This record has no Xbox PDB struct in research/xbox_symbols; every field
 * below was read out of the GameCube target asm of src/game/world/newcam.c,
 * where it lived as a file-local view until it gained a second consumer.
 * Ranges with no named field stay explicit `u8` runs rather than invented
 * members, so the 0x1B0 stride the target assumes is exact either way.
 *
 * The second consumer is src/game/world/gauntworld.c fn_800606FC, which
 * reads the live camera `lbl_80344A6C` at 0xA4/0xA8/0xAC as an f32 triple and
 * subtracts an item's OBJGRP.attn_pos from it - the same three words newcam.c
 * writes as the look-at point, which is what promotes `attention` from a
 * one-TU reading to a cross-TU one. */
typedef struct NcVec3 {
    f32 x;
    f32 y;
    f32 z;
} NcVec3;

typedef struct NcPlane {
    NcVec3 normal;
    f32 unused;
} NcPlane;

typedef struct NcCamera {
    /* 0x000 */ u8 _000[0x030];
    /* 0x030 */ NcVec3 position;      /* world position                      */
    /* 0x03C */ u8 _03C[0x040 - 0x03C];
    /* 0x040 */ NcPlane planes[4];    /* frustum plane normals (CalcDist)    */
    /* 0x080 */ u8 _080[0x0A4 - 0x080];
    /* 0x0A4 */ NcVec3 attention;     /* look-at point; gauntworld reads it  */
    /* 0x0B0 */ NcVec3 attn_prev;     /* previous attention (debug cam)      */
    /* 0x0BC */ NcVec3 velocity;      /* per-frame attention step            */
    /* 0x0C8 */ u8 _0C8[0x0DC - 0x0C8];
    /* 0x0DC */ f32 dist_current;     /* clamped working distance            */
    /* 0x0E0 */ NcVec3 direction;     /* forward vector                      */
    /* 0x0EC */ f32 yaw;
    /* 0x0F0 */ f32 yaw_rate;
    /* 0x0F4 */ f32 distance;         /* smoothed follow distance            */
    /* 0x0F8 */ f32 dist_rate;
    /* 0x0FC */ u8 _0FC[0x100 - 0x0FC];
    /* 0x100 */ f32 field_100;        /* debug HUD "(%.2f)" value            */
    /* 0x104 */ f32 pitch;
    /* 0x108 */ f32 pitch_rate;
    /* 0x10C */ f32 zoom;
    /* 0x110 */ f32 aspect;
    /* 0x114 */ NcVec3 ring_pos[9];   /* attention history ring              */
    /* 0x180 */ f32 ring_dist[9];     /* distance history ring               */
    /* 0x1A4 */ s32 field_1A4;        /* history ring index                  */
    /* 0x1A8 */ s32 field_1A8;        /* reset to -1                         */
    /* 0x1AC */ f32 field_1AC;
} NcCamera;                           /* 0x1B0 */

#ifdef __MWERKS__
/* offset-exact size guard */
typedef char _nccamera_size_check[sizeof(NcCamera) == 0x1B0 ? 1 : -1];
#endif

/* The live standard camera the frustum queries and gauntworld both read. */
extern NcCamera* lbl_80344A6C;

#endif /* GAME_NEWCAM_H */
