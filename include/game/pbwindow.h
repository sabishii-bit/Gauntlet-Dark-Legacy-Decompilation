#ifndef GAME_PBWINDOW_H
#define GAME_PBWINDOW_H

/* pbwindow.h -- Midway's pb rendering window / camera / projection state.
 *
 * Reconstructed struct header for pb_window.obj (shell3D.pdb module name
 * "pb_window"). Layout is offset-exact for the GameCube build (GUNE5D) and
 * matches the Xbox PDB struct PBWINDOW field-for-field.
 *
 * Sources:
 *   - research/xbox_symbols/misc.h : struct PBWINDOW (Size=0x3c0, Id=3397),
 *     the authoritative Xbox PDB transcription (real field names).
 *   - src/game/pb/pb_window.c      : the GCN NonMatching anchor, whose inline
 *     PBWINDOW/PBWINSTATIC/PBWINGLOBALS transcription this header consolidates.
 *
 * All offsets below were re-verified against the dtk-extracted GameCube object
 * build/GUNE5D/obj/game/pb/pb_window.o via tools/gdl/fnasm.py:
 *   MBWindowClip       -> clip_width@8 clip_height@C near_z@58 far_z@5C proj_dirty@3
 *   MBWindowProjection -> view_angle_horiz@50 aspect@54
 *   MBWindowViewport   -> left@70 right@74 top@78 bottom@7C
 *   pbInitCamera       -> cam_pos@18 cam_look@28 cam_dirty@2
 *   pbUpdateMatricies  -> projection@80 viewport@C0 clipport@140 camera@200
 *                         icamera@240 world_npc@280 world_screen@2C0 world_clip@300
 *   pbProjCalc         -> scissor sth@10/12/14/16 hva@60-6C view_screen@100
 *                         clip_screen@1C0 npc2clip@340 clip2npc@350
 *                         npc2screen@360 clip2screen@380
 *
 * GameCube-vs-Xbox delta: the 8-byte field at 0x10 is the Xbox/PS2
 * `sceGsScissor scissor` (a GS register pair). On GameCube it is stored as four
 * 11.5 fixed-point shorts (FIX115 scissor[4]) -- pbProjCalc writes the four
 * integer parts with `sth` at 0x10/0x12/0x14/0x16. Size and offset are
 * identical (0x8), only the interpretation differs.
 */

#include "types.h"

/* 11.5 fixed-point scissor coordinate (integer part in the low 11 bits). */
typedef struct FIX115 {
    u16 i : 11;
    u16 f : 5;
} FIX115; /* 0x2 */

typedef struct PBWINDOW {
    /* 0x000 */ u16 flags;
    /* 0x002 */ u8 cam_dirty;
    /* 0x003 */ u8 proj_dirty;
    /* 0x004 */ u32 pad;
    /* 0x008 */ f32 clip_width;
    /* 0x00C */ f32 clip_height;
    /* 0x010 */ FIX115 scissor[4]; /* Xbox: sceGsScissor (0x8) */
    /* 0x018 */ f32 cam_pos[4];
    /* 0x028 */ f32 cam_look[4];
    /* 0x038 */ f32 cam_up[4];
    /* 0x048 */ f32 cam_pitch;
    /* 0x04C */ f32 cam_yaw;
    /* 0x050 */ f32 view_angle_horiz;
    /* 0x054 */ f32 aspect;
    /* 0x058 */ f32 near_z;
    /* 0x05C */ f32 far_z;
    /* 0x060 */ f32 hva_sin_x;
    /* 0x064 */ f32 hva_cos_x;
    /* 0x068 */ f32 hva_sin_y;
    /* 0x06C */ f32 hva_cos_y;
    /* 0x070 */ f32 left;
    /* 0x074 */ f32 right;
    /* 0x078 */ f32 top;
    /* 0x07C */ f32 bottom;
    /* 0x080 */ f32 projection[4][4];
    /* 0x0C0 */ f32 viewport[4][4];
    /* 0x100 */ f32 view_screen[4][4];
    /* 0x140 */ f32 clipport[4][4];
    /* 0x180 */ f32 view_clip[4][4];
    /* 0x1C0 */ f32 clip_screen[4][4];
    /* 0x200 */ f32 camera[4][4];
    /* 0x240 */ f32 icamera[4][4];
    /* 0x280 */ f32 world_npc[4][4];
    /* 0x2C0 */ f32 world_screen[4][4];
    /* 0x300 */ f32 world_clip[4][4];
    /* 0x340 */ f32 npc2clip[4];
    /* 0x350 */ f32 clip2npc[4];
    /* 0x360 */ f32 npc2screen[2][4];
    /* 0x380 */ f32 clip2screen[2][4];
    /* 0x3A0 */ f32 screen2clip[2][4];
} PBWINDOW; /* 0x3C0 */

/* Window list node held by PBWINGLOBALS::list (gDefaultWinList @0x802C9B78). */
typedef struct PBWINLIST {
    /* 0x0 */ PBWINDOW* windows;
    /* 0x4 */ s32 count;
    /* 0x8 */ s32 unk8;
    /* 0xC */ s32 unkC;
} PBWINLIST; /* 0x10 */

/* Module-global state block (*gWinGlobals @0x80344FC0). Field names/offsets
 * come from src/game/pb/pb_window.c; ->current@0x04 confirmed in every
 * accessor (lwz r3,4(rX)). */
typedef struct PBWINGLOBALS {
    /* 0x00 */ void* unk00;
    /* 0x04 */ PBWINDOW* current;
    /* 0x08 */ void* framebuf;
    /* 0x0C */ void* unk0C;
    /* 0x10 */ void* screen;
    /* 0x14 */ void* unk14;
    /* 0x18 */ PBWINLIST* list;
    /* 0x1C */ void* lights;
    /* 0x20 */ u8 unk20[0x24];
    /* 0x44 */ void* unk44;
} PBWINGLOBALS;

/* Cast view over gWindows (@0x802C93F8, size 0x700): the single window
 * followed by the static per-frame packet scratch (GIF/VU1 heritage). */
typedef struct PBWINSTATIC {
    /* 0x000 */ PBWINDOW win;
    /* 0x3C0 */ u32 hdr0[4];         /* 0x802C97B8 */
    /* 0x3D0 */ u32 hdr1[4];
    /* 0x3E0 */ u32 quad0[40];       /* 0x802C97D8 */
    /* 0x480 */ u32 quad1[40];       /* 0x802C9878 */
    /* 0x520 */ u32 pkt[0x78];       /* 0x802C9918 default matrix packet */
    /* 0x700 */ f32 lightInv[4][4];  /* 0x802C9AF8 */
    /* 0x740 */ f32 lightRows[4][4]; /* 0x802C9B38 */
    /* 0x780 */ PBWINLIST list;      /* gDefaultWinList */
    /* 0x790 */ f32 camera[4][4];    /* gCameraMtx */
    /* 0x7D0 */ f32 projD3D[4][4];
} PBWINSTATIC;

/* --- struct-owned globals (already named in config/GUNE5D/symbols.txt) --- */
extern PBWINGLOBALS* gWinGlobals;   /* .sbss  0x80344FC0 (referenced by the
                                       Matching pb_global/pbutils TUs) */
extern PBWINDOW gWindows[];         /* .bss   0x802C93F8 (PBWINSTATIC view) */
extern PBWINLIST gDefaultWinList;   /* .bss   0x802C9B78 */
extern f32 gCameraMtx[4][4];        /* .bss   0x802C9B88 */
extern PBWINDOW** gCurWindowMirror; /* .sdata 0x80343F10 */

#endif /* GAME_PBWINDOW_H */
