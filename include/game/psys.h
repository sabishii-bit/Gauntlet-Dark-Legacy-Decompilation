#ifndef GAME_PSYS_H
#define GAME_PSYS_H

/*
 * psys.h - MB particle-system node/descriptor.
 *
 * Reconstructed from the Xbox shell3D PDB (research/xbox_symbols/misc.h):
 *   struct PSYS         Size=0x130 (Id=3340)
 *   struct PSYSDESCRIP  Size=0xe8  (Id=3429)  - setWorldParms input blob
 *   union  PSYSPARM     Size=0x10  (Id=3549)  + PSYSIPARM/OPARM/KPARM/PPARM
 *   struct PSYSMEMPOOL  Size=0x24  (Id=3547)  + PSYSMEMBLOCK Size=0x10
 *
 * The GameCube (GUNE5D) layout is byte-for-byte identical to the Xbox struct
 * for every field verified against src/game/mb/mb_particle.c asm
 * (build/GUNE5D/asm/game/mb/mb_particle.s). A live psys hangs off a scene
 * node at node->psys (mbnode + 0x70); MBInitPsys builds a 120000-byte block
 * pool over PSYSMEMPOOL and validates the preset table.
 *
 * GC offsets confirmed from asm (mb_particle.s):
 *   node->psys        mbnode + 0x70   (every MBPsysSet* loads lwz r,0x70(r3))
 *   e_phase   @ 0x37  drawing-begun guard (lbz r,0x37; ble)   0x800D0B44,+
 *   flags     @ 0x2c  u16 (lhz/sth halfword ops)              0x800D1010
 *   e_life    @ 0x3a  u16                                     0x800D0FD0
 *   e_fade    @ 0x3c  u16                                     0x800D100C
 *   e_angle   @ 0x40  f32 (deg->rad convert store)            0x800D0D6C
 *   p_life    @ 0x60  u8                                      0x800D0F18
 *   p_fade    @ 0x61  u8                                      0x800D0F54
 *   p_speed   @ 0x84  f32                                     0x800D0E28
 *   p_texidx  @ 0x88  u32                                     0x800D0B24
 *   p_tex     @ 0x8c  ROMTEX*                                 0x800D0B3C
 *   e_rate    @ 0xd0  4 x f32                                 0x800D0E84..
 *   p_parms   @ 0xe0  5 x 4 x f32                             0x800D0B98..
 * GC delta vs the local mb_particle.c reconstruction: the setter at 0x800D0CF0
 * (labelled "MBPsysSetPTime" there) actually writes e_angle @ 0x40 via a
 * degrees->radians conversion, matching the Xbox e_angle field - not a "pTime".
 */

#include "types.h"

struct mbnode; /* scene node (mbnode + 0x70 == the live Psys); opaque here */
struct ROMTEX; /* bound particle texture; opaque here */

/* --- per-particle / per-emitter 0x10 parameter cell (union of 4 views) --- */

typedef struct PsysIParm { /* interpolated: start -> end over life */
    /* 0x00 */ f32 life_start;
    /* 0x04 */ f32 life_end;
    /* 0x08 */ f32 fade_start;
    /* 0x0C */ f32 fade_end;
} PsysIParm;

typedef struct PsysOParm { /* linear: start + slope*t */
    /* 0x00 */ f32 life_start;
    /* 0x04 */ f32 life_slope;
    /* 0x08 */ f32 fade_start;
    /* 0x0C */ f32 fade_slope;
} PsysOParm;

typedef struct PsysKParm { /* keyed to an animation channel */
    /* 0x00 */ f32 life_value;
    /* 0x04 */ s32 life_anim;
    /* 0x08 */ f32 fade_value;
    /* 0x0C */ s32 fade_anim;
} PsysKParm;

typedef union PsysParm { /* 0x10 - PSYSPARM (Id=3549) */
    PsysIParm i;
    PsysOParm o;
    PsysKParm k;
} PsysParm;

/* Linked per-particle parameter (PSYSPPARM, Id=3553). */
typedef struct PsysPParm {
    /* 0x00 */ f32 value;
    /* 0x04 */ f32 start;
    /* 0x08 */ f32 slope;
    /* 0x0C */ struct PsysPParm* next;
} PsysPParm;

/* Emitter function-pointer generators (setupNewPMode wires these). */
struct Psys;
typedef int (*PsysDirFunc)(struct Psys*, f32* [4], int);
typedef int (*PsysPosFunc)(struct Psys*, f32* [4], int);
typedef void (*PsysPPosFunc)(struct Psys*, f32*, f32*, f32*, f32);

/* --- live particle system (node->psys, 0x130 bytes) - PSYS (Id=3340) --- */
typedef struct Psys {
    /* 0x000 */ s32 id;
    /* 0x004 */ char* worldname;         /* world-psys name / owner            */
    /* 0x008 */ u16* p_lst;              /* particle ring (packed pos|dir idx) */
    /* 0x00C */ f32 (*init_dir_lst)[3];  /* -> direction-slot vec3 pool (ptr)  */
    /* 0x010 */ f32 (*init_pos_lst)[3];  /* -> position-slot vec3 pool (ptr)   */
    /* 0x014 */ u8* dir_use_lst;         /* dir-slot usage bytes               */
    /* 0x018 */ u8* pos_use_lst;         /* pos-slot usage bytes               */
    /* 0x01C */ u16 dir_next;
    /* 0x01E */ u16 dir_last;
    /* 0x020 */ u16 pos_next;
    /* 0x022 */ u16 pos_last;
    /* 0x024 */ struct Psys* next;       /* active/free list link              */
    /* 0x028 */ struct mbnode* node;     /* owning scene node                  */
    /* 0x02C */ u16 flags;               /* PSYS state flags (bit1 = built)    */
    /* 0x02E */ u16 p_max;               /* max live particles                 */
    /* 0x030 */ u16 dir_max;
    /* 0x032 */ u16 pos_max;
    /* 0x034 */ u8 age_bits;
    /* 0x035 */ u8 dir_bits;
    /* 0x036 */ u8 pos_bits;
    /* 0x037 */ u8 e_phase;              /* emitter phase (delay/emit/.../dead) */
    /* 0x038 */ u16 e_delay;
    /* 0x03A */ u16 e_life;              /* emit duration (frames)             */
    /* 0x03C */ u16 e_fade;              /* emit fade / repeat count           */
    /* 0x03E */ u8 __align0[2];
    /* 0x040 */ f32 e_angle;             /* emission cone angle (radians)      */
    /* 0x044 */ f32 e_dir[3];            /* emission direction basis           */
    /* 0x050 */ f32 e_vol[3];            /* emission volume extents            */
    /* 0x05C */ s32 texcnt;
    /* 0x060 */ u8 p_life;               /* base particle life (clamped byte)  */
    /* 0x061 */ u8 p_fade;               /* particle fade (clamped byte)       */
    /* 0x062 */ u8 __align1[2];
    /* 0x064 */ PsysDirFunc dir_func;    /* new-direction generator            */
    /* 0x068 */ PsysPosFunc pos_func;    /* new-position generator             */
    /* 0x06C */ PsysPPosFunc ppos_func;  /* per-particle position integrator   */
    /* 0x070 */ u16* p_oldest_ptr;
    /* 0x074 */ u16* p_newest_ptr;
    /* 0x078 */ u16 p_oldest_age;
    /* 0x07A */ u16 p_newest_age;
    /* 0x07C */ f32 p_save_cnt;
    /* 0x080 */ s16 p_nactive;
    /* 0x082 */ s16 e_isvis;
    /* 0x084 */ f32 p_speed;             /* particle speed multiplier          */
    /* 0x088 */ u32 p_texidx;            /* packed texture handle              */
    /* 0x08C */ struct ROMTEX* p_tex;    /* resolved particle texture          */
    /* 0x090 */ u32 e_last_time;
    /* 0x094 */ u32 e_age;
    /* 0x098 */ f32 p_gravity;
    /* 0x09C */ f32 p_drag;
    /* 0x0A0 */ f32 nearest_z;
    /* 0x0A4 */ f32 max_dist;
    /* 0x0A8 */ f32 max_width;
    /* 0x0AC */ f32 vol_radius;
    /* 0x0B0 */ f32 pos_start[3];
    /* 0x0BC */ f32 pos_far[3];
    /* 0x0C8 */ f32 distsq;
    /* 0x0CC */ f32 e_rate_rand;
    /* 0x0D0 */ PsysParm e_rate;         /* emission-rate envelope (4 keys)    */
    /* 0x0E0 */ PsysParm p_parms[5];     /* per-particle gradients (b/g/r/i/w) */
} Psys; /* 0x130 */

/* MBPsysSetPParm parameter index (PSYSPPARMS enum). */
enum {
    MB_PSYSPPARM_blue = 0,
    MB_PSYSPPARM_green = 1,
    MB_PSYSPPARM_red = 2,
    MB_PSYSPPARM_intensity = 3,
    MB_PSYSPPARM_width = 4,
    MB_PSYSPPARM_END = 5
};

/* --- descriptor blob consumed by setWorldParms (PSYSDESCRIP, 0xe8) --- */
typedef struct PsysDescrip {
    /* 0x00 */ u32 pversion;
    /* 0x04 */ char dynamic;
    /* 0x05 */ char oneshot;
    /* 0x06 */ char forever;
    /* 0x07 */ char gravity;
    /* 0x08 */ char drag;
    /* 0x09 */ char notex_rgb;
    /* 0x0A */ char notex_a;
    /* 0x0B */ char fbadd;
    /* 0x0C */ char fbmul;
    /* 0x0D */ char sort;
    /* 0x0E */ char nozcompare;
    /* 0x0F */ char nozwrite;
    /* 0x10 */ char reserved[20];
    /* 0x24 */ u32 fields_used;          /* WORLDPSYS_EN_* enable mask         */
    /* 0x28 */ s32 max_particles;
    /* 0x2C */ s32 max_directions;
    /* 0x30 */ s32 max_positions;
    /* 0x34 */ struct mbnode* parent;
    /* 0x38 */ s32 preset_idx;
    /* 0x3C */ f32 e_lifefade[2];
    /* 0x44 */ f32 p_lifefade[2];
    /* 0x4C */ f32 e_pos[3];
    /* 0x58 */ f32 e_dir[3];
    /* 0x64 */ f32 e_angle;
    /* 0x68 */ f32 e_vol[3];
    /* 0x74 */ f32 e_rate[4];
    /* 0x84 */ f32 e_rate_rand;
    /* 0x88 */ f32 p_gravity;
    /* 0x8C */ f32 p_drag;
    /* 0x90 */ f32 p_speed;
    /* 0x94 */ char p_texname1[40];
    /* 0xBC */ char* p_texname2;
    /* 0xC0 */ u32 p_texidx;
    /* 0xC4 */ struct ROMTEX* p_romtex;
    /* 0xC8 */ u32 p_rgba[4];
    /* 0xD8 */ f32 p_width[4];
} PsysDescrip; /* 0xe8 */

/* --- block-pool allocator backing MBInitPsys (PSYSMEMPOOL/PSYSMEMBLOCK) --- */
typedef struct PsysMemBlock { /* 0x10 - PSYSMEMBLOCK (Id=3548) */
    /* 0x00 */ s32 bytes;
    /* 0x04 */ struct PsysMemBlock* next;
    /* 0x08 */ struct PsysMemBlock* prev;
    /* 0x0C */ s32 id;
} PsysMemBlock;

typedef struct PsysMemPool { /* 0x24 - PSYSMEMPOOL (Id=3547) */
    /* 0x00 */ u32 world_bytes;
    /* 0x04 */ u32 pool_bytes;           /* == 120000 (0x1D4C0) from MBInitPsys */
    /* 0x08 */ u32 free_bytes;
    /* 0x0C */ u32 free_cnt;
    /* 0x10 */ u32 alloc_cnt;
    /* 0x14 */ void* addr;               /* AllocMem block-pool base           */
    /* 0x18 */ PsysMemBlock* frst;
    /* 0x1C */ PsysMemBlock* last;
    /* 0x20 */ PsysMemBlock* next;
} PsysMemPool;

#endif /* GAME_PSYS_H */
