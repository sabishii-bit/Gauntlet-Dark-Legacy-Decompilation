#ifndef GAME_EFFECT_H
#define GAME_EFFECT_H

#include "types.h"

/*
 * SFX live-effect record and effect-definition tables for Gauntlet Dark Legacy.
 *
 * Reconstructed from the Xbox debug PDB (research/xbox_symbols/misc.h):
 *   struct effect       -> Effect        (Id=3298, Size=0xF0)
 *   struct EFFECTHEADER -> EffectHeader  (Id=3354, Size=0x0C)
 *   struct EFFECTINFO   -> (documented below; Id=3504, Size=0x28)
 *   enum   fx_type      (Id spanning 0..217, MAXEFFECTTYPES=218)
 *   enum   DMG_TYPE
 *
 * Every field offset below was VERIFIED against the GameCube (GUNE5D) target
 * asm of the SFX.OBJ effect routines (ZeroEffect@0x800979D4,
 * FindEffectIdx@0x80097394, ChangeEffect, DeleteEffect, ProcessEffects):
 *   - stride is 0xF0  (ZeroEffect/FindEffectIdx: "mulli rX,rIdx,240")
 *   - node   @+0x14   (ChangeEffect: lwz r,20(effect))
 *   - atree  @+0x18   (ChangeEffect: lwz r,24(effect); deref .root at +0)
 *   - type   @+0x60   (FindEffectIdx: stw 0,96(effect) before DeleteEffect)
 *   - flags  @+0x64   (FindEffectIdx: lwz 0,100; andis. ...; ZeroEffect same)
 *   - endtime@+0x68   (FindEffectIdx: lfs f,104(effect) life compare)
 *   - vel[3]@+0x80, pyrvel[3]@+0x8c, colrad@+0x98, weight@+0xa0,
 *     dragx@+0xa4, dragz@+0xa8, fxhit@+0xbe, fxmorph@+0xc0, fxmorph2@+0xc2,
 *     hit_audio@+0xcc, wall_sound@+0xd0, streak@+0xd4, minendtime@+0x70,
 *     damagedelay@+0x7c, childfx@+0xe8  (all confirmed in ZeroEffect stores).
 * The GC layout matches the Xbox `struct effect` byte-for-byte. (The earlier
 * hand notes of node@+0x04 / flags@+0x54 / timer@+0x58 were off by 0x10 and
 * are superseded by the asm above.)
 *
 * Live effect array : Effects[64]   @ .bss 0x80285BB8  (64 * 0xF0 = 0x3C00)
 * Active-slot count : NumEffects    @ .sbss 0x80344BDC (high-water mark)
 * Effect def table  : EffectInfo[]  @ .bss 0x80285018  (0xBA0 = 248 * 0x0C,
 *                     entries are EffectHeader: [atreeheader*, zmod, alpha])
 */

/* Shared engine types referenced only by pointer; defined by their own
 * subsystems. Forward declarations keep this header self-contained. */
struct mbnode;      /* MB (model-behaviour) scene node */
struct polyinst;    /* streak / poly instance          */
struct item;        /* pickup item spawned by an effect */
struct atreeheader; /* animation-tree header (def table) */

/* Effect / SFX kind. `Effect.type` selects the def-table row. */
typedef enum fx_type {
    FX_NONE = 0,
    FX_HIT = 1,
    FX_DMG = 2,
    FX_DIE = 3,
    FX_BLOODDMG = 4,
    FX_BLOODDIE = 5,
    FX_BLOOD1 = 6,
    FX_BLOOD2 = 7,
    FX_BLOOD3 = 8,
    FX_BLOOD4 = 9,
    FX_FIREHIT = 10,
    FX_FIREDMG = 11,
    FX_FIREDIE = 12,
    FX_ELECHIT = 13,
    FX_ELECDMG = 14,
    FX_ELECDIE = 15,
    FX_LIGHTHIT = 16,
    FX_LIGHTDMG = 17,
    FX_LIGHTDIE = 18,
    FX_ACIDHIT = 19,
    FX_ACIDDMG = 20,
    FX_ACIDDIE = 21,
    FX_EXPLODE = 22,
    FX_EXPSMALL = 23,
    FX_EXPBARREL = 24,
    FX_EXPPOISON = 25,
    FX_EXPPOISON2 = 26,
    FX_EXPPOISON3 = 27,
    FX_EXPRING = 28,
    FX_EXPCHEST = 29,
    FX_GENDEST = 30,
    FX_CHESTDEST = 31,
    FX_ITEMDEST = 32,
    FX_DESTSMOKE = 33,
    FX_START_MAGIC = 34,
    FX_MAGIC_FIRE = 35,
    FX_MAGIC_ELEC = 36,
    FX_MAGIC_LIGHT = 37,
    FX_MAGIC_ACID = 38,
    FX_SHIELD_FIRE = 39,
    FX_SHIELD_ELEC = 40,
    FX_SHIELD_LIGHT = 41,
    FX_SHIELD_ACID = 42,
    FX_MAGIC_THROW_FIRE = 43,
    FX_MAGIC_THROW_ELEC = 44,
    FX_MAGIC_THROW_LIGHT = 45,
    FX_MAGIC_THROW_ACID = 46,
    FX_HEALFOOD = 47,
    FX_HEALGOLD = 48,
    FX_HEALTRAP = 49,
    FX_HEALENEMY = 50,
    FX_MAGICHEAL = 51,
    FX_BREATHE_FIRE = 52,
    FX_BREATHE_ACID = 53,
    FX_BREATHE_ELEC = 54,
    FX_SHLDFX_ELEC = 55,
    FX_BOSS_BREATHE = 56,
    FX_LEVELUP_YEL = 57,
    FX_LEVELUP_BLU = 58,
    FX_LEVELUP_RED = 59,
    FX_LEVELUP_GRE = 60,
    FX_COMBO_SPHERE = 61,
    FX_COMBO_YEL = 62,
    FX_COMBO_BLU = 63,
    FX_COMBO_RED = 64,
    FX_COMBO_GRE = 65,
    FX_BLOCK = 66,
    FX_ENTER = 67,
    FX_BAG_THROW = 68,
    FX_BAG_HIT = 69,
    FX_GEM_ORANGE = 70,
    FX_GEM_RED = 71,
    FX_GEM_PURPLE = 72,
    FX_GEM_CYAN = 73,
    FX_GEM_GREEN = 74,
    FX_GEM_YELLOW = 75,
    FX_GEM_WHITE = 76,
    FX_GEM_BLACK = 77,
    FX_GET_GARG = 78,
    FX_GET_RUNE = 79,
    FX_SUICIDEEXP = 80,
    FX_GENFX1 = 81,
    FX_GENFX2 = 82,
    FX_GENFX3 = 83,
    FX_ALTHIT = 84,
    FX_ALTDIE = 85,
    FX_ENEATK1 = 86,
    FX_ENEATK2 = 87,
    FX_ENEDEATH1 = 88,
    FX_ENEDEATH2 = 89,
    FX_LEGEND1 = 90,
    FX_LEGEND2 = 91,
    FX_LEGENDHLD = 92,
    FX_LEGENDPRJ = 93,
    FX_WORLD_EXP = 94,
    FX_DEATH_HEALTH = 95,
    FX_DEATH_EXP = 96,
    FX_CUSTOM1 = 97,
    FX_CUSTOM_LAST = 217,
    MAXEFFECTTYPES = 218
} fx_type;

/* Damage class + modifier bitmask. `Effect.damagetype` combines a base class
 * (DMG_NORMAL..DMG_MAXTYPES) with modifier bits (DMG_KNOCKBACK and up). */
typedef enum DMG_TYPE {
    DMG_NORMAL = 0,
    DMG_FIRE = 1,
    DMG_ELEC = 2,
    DMG_LIGHT = 3,
    DMG_ACID = 4,
    DMG_MAXTYPES = 5,
    DMG_KNOCKBACK = 0x10,
    DMG_KNOCKDOWN = 0x20,
    DMG_BLOWNAWAY = 0x40,
    DMG_STUN = 0x80,
    DMG_KNOCKOVER = 0x100,
    DMG_MAGIC = 0x200,
    DMG_EXPLODE = 0x400,
    DMG_POISONGAS = 0x800,
    DMG_DEATHSTUN = 0x1000,
    DMG_SPIKE = 0x2000,
    DMG_GRABBED = 0x4000,
    DMG_THROWN = 0x8000,
    DMG_WHIRLWIND = 0x10000,
    DMG_ARROW = 0x20000,
    DMG_FBALL = 0x40000,
    DMG_3WAY = 0x80000,
    DMG_SUPER = 0x100000,
    DMG_REFLECT = 0x200000,
    DMG_5WAY = 0x400000,
    DMG_HEAL = 0x800000,
    DMG_NOHITFX = 0x1000000,
    DMG_TURBO = 0x2000000,
    DMG_STICKY = 0x4000000,
    DMG_BOSSHIT = 0x8000000,
    DMG_HAMMER = 0x10000000,
    DMG_RAPIDFIRE = 0x20000000,
    DMG_LOW = 0x40000000
} DMG_TYPE;

/*
 * Effect-definition table entry (Xbox `struct EFFECTHEADER`, 0x0C bytes).
 * This is the element type of the runtime EffectInfo[] table @0x80285018:
 * a resolved animation-tree pointer plus the per-def z-bias and alpha.
 */
typedef struct EffectHeader {
    struct atreeheader* atree; /* 0x00 resolved anim-tree for this def   */
    s32 zmod;                  /* 0x04 z-bias applied to the effect polys */
    s32 alpha;                 /* 0x08 base alpha / blend for the effect  */
} EffectHeader; /* size 0x0C */

/*
 * Live SFX effect record. One entry per active on-screen effect;
 * ProcessEffects walks the Effects[64] pool each frame.
 * Xbox `struct effect`, size 0xF0 -- GC layout verified identical.
 */
typedef struct Effect {
    f32 lightcolor[4];         /* 0x00 dynamic light RGBA emitted by fx   */
    f32 lightrad;              /* 0x10 dynamic light radius               */
    struct mbnode* node;       /* 0x14 owning scene node                  */
    u8 atree[0x48];            /* 0x18 struct atree (0x48): anode* root@+0,
                                *      animinfo@+0x04 (0x38), int nanodes@+0x3c,
                                *      anode* firstanode@+0x40,
                                *      anodeinfo* anodeinfo@+0x44             */
    fx_type type;              /* 0x60 which effect kind (enum fx_type)   */
    s32 flags;                 /* 0x64 runtime state bits                 */
    f32 endtime;               /* 0x68 life timer / death time            */
    f32 maxtime;               /* 0x6c total lifetime                     */
    f32 minendtime;            /* 0x70 earliest allowed end time          */
    f32 morphtime;             /* 0x74 time to morph into fxmorph         */
    f32 webtime;               /* 0x78 web/link timer                     */
    f32 damagedelay;           /* 0x7c delay before damage is applied     */
    f32 vel[3];                /* 0x80 linear velocity                    */
    f32 pyrvel[3];             /* 0x8c pitch/yaw/roll angular velocity    */
    f32 colrad;                /* 0x98 collision radius                   */
    f32 mindp;                 /* 0x9c min dot-product for facing tests   */
    f32 weight;                /* 0xa0 gravity weight                     */
    f32 dragx;                 /* 0xa4 horizontal drag                    */
    f32 dragz;                 /* 0xa8 vertical drag                      */
    f32 damage;                /* 0xac damage dealt on hit                */
    f32 hitscale;              /* 0xb0 hit scaling factor                 */
    DMG_TYPE damagetype;       /* 0xb4 damage class + modifier bitmask    */
    f32 damageradius;          /* 0xb8 splash/AoE radius                  */
    s16 owner;                 /* 0xbc spawning entity index              */
    s16 fxhit;                 /* 0xbe fx spawned on hit (fx_type)        */
    s16 fxmorph;               /* 0xc0 fx to morph into                   */
    s16 fxmorph2;              /* 0xc2 second morph fx                    */
    s16 hitcount;              /* 0xc4 remaining hits                     */
    s16 debugcount;            /* 0xc6 debug counter                      */
    f32 fxfade;                /* 0xc8 fade rate                          */
    s32 hit_audio;             /* 0xcc sound id played on hit             */
    s32 wall_sound;            /* 0xd0 sound id played on wall impact     */
    struct polyinst* streak;   /* 0xd4 trailing streak poly instance      */
    f32 streakfwdmul;          /* 0xd8 streak forward multiplier          */
    f32 streakscale;           /* 0xdc streak width scale                 */
    struct mbnode* dmgdebug;   /* 0xe0 damage debug node                  */
    struct mbnode* targetnode; /* 0xe4 homing / attach target node        */
    s16 childfx;               /* 0xe8 child fx slot index (-1 = none)    */
    s16 id;                    /* 0xea unique effect id                   */
    struct item* additem;      /* 0xec item to spawn on completion        */
} Effect; /* size 0xF0 */

/*
 * Authoring/source effect-def struct (Xbox `struct EFFECTINFO`, 0x28 bytes):
 *   char desc[32];  int zmod;  int alpha;
 * The name string is resolved to an atree pointer at load time, producing the
 * runtime EffectHeader rows above; the GC EffectInfo[] global holds the
 * resolved EffectHeader form, so the source struct is documented here only.
 */

#endif /* GAME_EFFECT_H */
