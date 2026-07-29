#ifndef GAME_ENEMY_H
#define GAME_ENEMY_H

#include "types.h"

/* Gauntlet Dark Legacy - enemy record (the "Enemy" / ENEMY.OBJ struct).
 *
 * Reconstructed from the Xbox debug PDB (research/xbox_symbols/game.h,
 * `struct enemy` Size=0x394 Id=3245) with the sub-structs pulled from
 * research/xbox_symbols/{misc,graphics}.h.  Field names and offsets are the
 * real Midway source names.
 *
 * GameCube (GUNE5D) anchor:
 *   gEnemies    @0x80251C18  Enemy[25]  (stride 0x394, total size 0x5978)
 *   gNumEnemies @0x80344744  s32
 *
 * Offsets VERIFIED against the GC DOL asm (dtk-extracted game/enemy/enemy.o,
 * via tools/gdl/fnasm.py) in uncouple_enemy / do_enemies / generate_enemy:
 *   type        0x000  lwz  (== 0/E_SCORP, 29/E_GOLEM, gBossType compares)
 *   atree.root  0x06C  lwz  (non-null test before animate)
 *   state       0x0B4  lwz  (== 1/ACTIVE)
 *   action      0x0CC  stw  (result of AI dispatch)
 *   daction     0x0D0  stw  (0/1/3)
 *   coll_offset 0x230  lfs  (Y collision offset add)
 *   rad         0x238  lfs
 *   genang_off  0x258  stfs
 *   generator   0x290  lwz/stw (item*)
 *   algorithm   0x310  lha/sth (short)
 *   prev_enemy  0x334  lwz/stw (int)
 *   next_enemy  0x338  lwz/stw (int)
 * GC-vs-Xbox delta: NONE observed - the GC build uses the identical layout.
 * (On GC the array base is materialised as 0x80250E00 + 0xE18; 0x80250E00 is a
 * separate preceding global, and gEnemies == 0x80250E00 + 0xE18.)
 */

/* -- enemy type ids (research/xbox_symbols/misc.h enum e_e_tpye Id=... ) -- */
typedef enum e_e_tpye {
    E_SCORP = 0,   E_TROLL = 1,     E_DEMON = 2,      E_RAT = 3,
    E_GRUNT = 4,   E_KNIGHT = 5,    E_SNAKE = 6,      E_SORCERER = 7,
    E_MUMMY = 8,   E_SPIDER = 9,    E_LIZARDMAN = 10, E_TREEFOLK = 11,
    E_MAGGOT = 12, E_ZOMBIE = 13,   E_PLAGUE = 14,    E_WOLF = 15,
    E_ICE = 16,    E_WORM = 17,     E_DOG = 18,       E_SKELETON = 19,
    E_GHOST = 20,  E_ACID = 21,     E_HAND = 22,      E_IMP = 23,
    E_WARLOCK = 24, E_SKY = 25,     E_WHIRLWIND = 26, E_GARM2 = 27,
    E_NTYPES = 28, E_GOLEM = 29,    E_DEATH = 30,     E_IT = 31,
    E_GARGOYLE = 32, E_GENERAL = 33, E_DRAGON = 34,   E_CHIMERA = 35,
    E_DJINN = 36,  E_DRIDER = 37,   E_PBOSS = 38,     E_YETI = 39,
    E_WRAITH = 40, E_LICH = 41,     E_SKORNE1 = 42,   E_SKORNE2 = 43,
    E_GARM = 44,   E_MAXTYPES = 45
} e_e_tpye;

/* -- entity lifecycle state (research/xbox_symbols/misc.h enum state) -- */
typedef enum state {
    INACTIVE = 0, ACTIVE = 1,  SELECT = 2,        SELECTED = 3,
    ON_EXIT = 4,  ON_NEXT_LEVEL = 5, SLEEP = 6,   DECORATION = 7,
    DYING = 8,    ERROR = 9,   CONTINUE = 10,     INTOWER = 11
} state;

/* -- per-action animation entry (enemy_action_type indexes actionlist[]) -- */
typedef enum enemy_action_type {
    E_READY = 0,       E_START = 1,       E_TAUNT = 2,        E_WALK = 3,
    E_RUN = 4,         E_FLY = 5,         E_HOVER = 6,        E_LANDING = 7,
    E_WALKTOREADY = 8, E_READYTOWALK = 9, E_RUNTOREADY = 10,  E_READYTORUN = 11,
    E_ATTACK = 12,     E_ATTACK_R = 13,   E_ATTACK2 = 14,     E_ATTACK2_R = 15,
    E_ATTACK_PWR = 16, E_ATTACK_PWR_R = 17, E_ATTACK4 = 18,   E_ATTACK4_R = 19,
    E_ATTACK5 = 20,    E_ATTACK5_R = 21,  E_RUNATTACK = 22,   E_RUNATTACK2 = 23,
    E_THROW = 24,      E_THROW2 = 25,     E_THROW_FINISH = 26, E_THROWTOREADY = 27,
    E_HIT_REACT1 = 28, E_HIT_REACT2 = 29, E_HIT_REACT3 = 30,  E_GETUP = 31,
    E_DYING = 32,      E_NACTIONS = 33
} enemy_action_type;

/* forward decls for pointer-only members (defined by their own modules) */
struct mbnode;
struct item;
struct worldobj;
struct anode;
struct anodeinfo;
struct atreeseq;
struct animheader;
struct objanimheader;

/* -- OBJGRP (misc.h Id=2284, 0x68): world transform + collision block -- */
#ifndef GAME_OBJGRP_DEFINED
#define GAME_OBJGRP_DEFINED
typedef struct OBJGRP {
    f32 worldmat[4][4];      /* 0x00 */
    f32 attn_pos[4];         /* 0x40 */
    f32 coll_pos[4];         /* 0x50 */
    struct mbnode *node;     /* 0x60 */
    s32 flags;               /* 0x64 */
} OBJGRP;                     /* size 0x68 */
#endif

/* -- animinfo (graphics.h Id=3256, 0x38): playback state for one atree -- */
typedef struct animinfo {
    struct atreeseq *seqheader;      /* 0x00 */
    struct animheader *animheader;   /* 0x04 */
    struct objanimheader *oanimheader; /* 0x08 */
    s16 numseqs;             /* 0x0C */
    s16 animseq;             /* 0x0E */
    s16 numframes;           /* 0x10 */
    s8 setpanim;             /* 0x12 */
    u8 flags;                /* 0x13 */
    f32 transfrac;           /* 0x14 */
    f32 frame;               /* 0x18 */
    s16 animseq0;            /* 0x1C */
    s16 active;              /* 0x1E */
    f32 starttime;           /* 0x20 */
    f32 transtime;           /* 0x24 */
    f32 animscale;           /* 0x28 */
    f32 seqscale;            /* 0x2C */
    f32 atime;               /* 0x30 */
    s16 repeat;              /* 0x34 */
    u16 stage;               /* 0x36 */
} animinfo;                  /* size 0x38 */

/* -- atree (misc.h Id=2219, 0x48): animation tree instance -- */
typedef struct atree {
    struct anode *root;      /* 0x00 */
    animinfo animinfo;       /* 0x04 */
    s32 nanodes;             /* 0x3C */
    struct anode *firstanode; /* 0x40 */
    struct anodeinfo *anodeinfo; /* 0x44 */
} atree;                     /* size 0x48 */

/* -- E_ATTRIBUTES (misc.h Id=3246, 0x14): per-enemy combat attributes -- */
typedef struct E_ATTRIBUTES {
    f32 invspeed;            /* 0x00 */
    f32 fight;               /* 0x04 */
    f32 armor;               /* 0x08 */
    s32 damagetype;          /* 0x0C */
    s32 armortype;           /* 0x10 */
} E_ATTRIBUTES;              /* size 0x14 */

/* -- ACTIONANIM (graphics.h Id=3247, 0x08): anim range for one action -- */
typedef struct ACTIONANIM {
    s32 animidx;             /* 0x00 */
    s32 nframes;             /* 0x04 */
} ACTIONANIM;                /* size 0x08 */

/* -- skinfx (misc.h Id=3250, 0x18): skin/texture FX playback -- */
typedef struct skinfx {
    f32 nframes;             /* 0x00 */
    f32 frame;               /* 0x04 */
    f32 rate;                /* 0x08 */
    s32 texidx;              /* 0x0C */
    s32 repeat;              /* 0x10 */
    f32 ambientadd;          /* 0x14 */
} skinfx;                    /* size 0x18 */

/* ==================================================================== *
 *  Enemy - the active enemy record (0x394 / 916 bytes)                  *
 * ==================================================================== */
typedef struct Enemy {
    e_e_tpye type;                     /* 0x000 enemy type id            */
    OBJGRP objgrp;                     /* 0x004 world transform + coll   */
    atree atree;                       /* 0x06C animation tree instance  */
    state state;                       /* 0x0B4 lifecycle state          */
    E_ATTRIBUTES atts;                 /* 0x0B8 combat attributes        */
    s32 action;                        /* 0x0CC current action           */
    s32 daction;                       /* 0x0D0 desired/next action      */
    ACTIONANIM actionlist[33];         /* 0x0D4 per-action anim ranges   */
    struct mbnode *shadow;             /* 0x1DC shadow scene node        */
    s32 specialfx;                     /* 0x1E0                          */
    skinfx skinfx;                     /* 0x1E4 skin FX state            */
    s16 gridnext;                      /* 0x1FC spatial grid link        */
    s16 area;                          /* 0x1FE                          */
    f32 health;                        /* 0x200                          */
    s16 damage_count;                  /* 0x204                          */
    s16 org_lvl;                       /* 0x206 original level           */
    s32 attack_timer;                  /* 0x208                          */
    s32 stun_timer;                    /* 0x20C                          */
    f32 trans[3];                      /* 0x210 world position           */
    f32 flooroffset;                   /* 0x21C                          */
    f32 attn_offset[3];                /* 0x220                          */
    f32 coll_offset[3];                /* 0x22C                          */
    f32 rad;                           /* 0x238 collision radius         */
    f32 hht;                           /* 0x23C half height              */
    f32 pyr[3];                        /* 0x240 pitch/yaw/roll           */
    f32 ang;                           /* 0x24C facing angle             */
    f32 angbak;                        /* 0x250                          */
    f32 anghit;                        /* 0x254                          */
    f32 genang_offset;                 /* 0x258                          */
    f32 pushed[3];                     /* 0x25C knockback vector         */
    f32 pushang;                       /* 0x268                          */
    f32 pushmag2;                      /* 0x26C                          */
    s32 push_cnt;                      /* 0x270                          */
    s16 closest;                       /* 0x274 closest player index     */
    s16 prev_closest;                  /* 0x276                          */
    f32 close_dist;                    /* 0x278                          */
    f32 actual_dist;                   /* 0x27C                          */
    s16 moved;                         /* 0x280                          */
    s16 stopped;                       /* 0x282                          */
    s32 coll_pnum;                     /* 0x284 collided player number   */
    s32 coll_enenum;                   /* 0x288 collided enemy number    */
    struct item *coll_ip;              /* 0x28C collided item            */
    struct item *generator;            /* 0x290 owning generator/head    */
    f32 floory;                        /* 0x294                          */
    struct worldobj *floor_wobj;       /* 0x298                          */
    s32 floor_surf;                    /* 0x29C floor surface type       */
    f32 damage;                        /* 0x2A0 pending damage           */
    s32 damagetype;                    /* 0x2A4                          */
    f32 damagedir[3];                  /* 0x2A8                          */
    f32 fxhittime[5];                  /* 0x2B4                          */
    s32 fxhitidx;                      /* 0x2C8                          */
    s16 attack_index;                  /* 0x2CC                          */
    s16 attack_count;                  /* 0x2CE                          */
    s32 attack_flag;                   /* 0x2D0                          */
    s16 endurance;                     /* 0x2D4                          */
    s16 watchdog;                      /* 0x2D6                          */
    s16 birth_style;                   /* 0x2D8                          */
    s16 visible;                       /* 0x2DA                          */
    s16 visactive;                     /* 0x2DC                          */
    s16 recognized;                    /* 0x2DE                          */
    f32 dest[3];                       /* 0x2E0 AI destination           */
    f32 birth_pos[3];                  /* 0x2EC spawn position           */
    u32 sensor;                        /* 0x2F8                          */
    f32 view;                          /* 0x2FC view cone                */
    f32 sight;                         /* 0x300 sight range              */
    f32 xspd;                          /* 0x304                          */
    f32 zspd;                          /* 0x308                          */
    f32 prev_dir;                      /* 0x30C                          */
    s16 algorithm;                     /* 0x310 AI algorithm id          */
    s16 old_ai;                        /* 0x312                          */
    s16 prev_ai;                       /* 0x314                          */
    s16 mode1;                         /* 0x316                          */
    s16 mode2;                         /* 0x318                          */
    u8 pad0x31a[2];                    /* 0x31A alignment (Xbox __align0)*/
    s32 flag1;                         /* 0x31C                          */
    s32 flag2;                         /* 0x320                          */
    s32 counter1;                      /* 0x324                          */
    s32 counter2;                      /* 0x328                          */
    s32 skip_itemcol;                  /* 0x32C                          */
    u32 ai_flags;                      /* 0x330                          */
    s32 prev_enemy;                    /* 0x334 gen spawn-list prev      */
    s32 next_enemy;                    /* 0x338 gen spawn-list next      */
    s32 guard_mode;                    /* 0x33C                          */
    s32 guard_closest;                 /* 0x340                          */
    f32 guard_dist;                    /* 0x344                          */
    s32 plr_ms;                        /* 0x348                          */
    s32 ms_idx;                        /* 0x34C milestone index          */
    s32 max_msidx;                     /* 0x350                          */
    s32 route;                         /* 0x354                          */
    s32 dead_end;                      /* 0x358                          */
    s32 count;                         /* 0x35C                          */
    s32 lost;                          /* 0x360                          */
    s16 collided;                      /* 0x364                          */
    s16 stuck_count;                   /* 0x366                          */
    s32 operation_speed;               /* 0x368                          */
    s32 operation_count;               /* 0x36C                          */
    s32 lv;                            /* 0x370 level                    */
    s32 play;                          /* 0x374                          */
    f32 idle_time;                     /* 0x378                          */
    f32 idle_secs;                     /* 0x37C                          */
    f32 idle_frac;                     /* 0x380                          */
    struct item *gotitem;              /* 0x384                          */
    s32 alpha;                         /* 0x388                          */
    f32 prev_frame;                    /* 0x38C                          */
    s32 anim_done;                     /* 0x390                          */
} Enemy;                               /* size 0x394 (916) */

/* -- module globals (GC GUNE5D) -- */
extern Enemy gEnemies[25];  /* 0x80251C18 active enemy records */
extern s32 gNumEnemies;     /* 0x80344744 number of active slots */

#endif /* GAME_ENEMY_H */
