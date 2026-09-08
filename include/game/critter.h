#ifndef GAME_CRITTER_H
#define GAME_CRITTER_H

#include "types.h"

/* Gauntlet Dark Legacy - critter/creature record (the "Critter" / CRITTER.OBJ
 * struct).  Critters are the large, scripted, multi-part creatures (golems,
 * bosses, generals, ...) distinct from the swarm-style Enemy record.
 *
 * The Xbox debug PDB (research/xbox_symbols/shell3D.pdb, module CRITTER.obj)
 * exports every CRITTER.OBJ *function* but retains NO named CRITTER instance
 * struct in its type stream (grep of game.h / misc.h / type_index.txt /
 * xbox_structs.tsv finds only PBMEM_CRITTER).  This layout is therefore
 * reconstructed directly from the GameCube (GUNE5D) DOL asm - the field
 * boundaries below are byte-exact against the target objects, with behavioural
 * names.  Offsets that could not be pinned to a use are left as reserved
 * (_blkXXX / _resXXX) padding so the struct stays offset-exact (0xAE0).
 *
 * GameCube (GUNE5D) anchors (config/GUNE5D/symbols.txt):
 *   Critter instance size  0xAE0  (2784 bytes)
 *   gCritterPool[16]  @0x80241204  == BigState gBig.blk234 (soundmgr.c),
 *                                     16 * 0xAE0 == 0xAE00
 *   gCritterHeaders[9][6] @0x8024C004 == gBig.arrB034 - CritterHeader* table
 *                                     indexed [type*0x18 + subtype*4]
 *   gCritterNextID   @0x80343BE8  u16 rolling unique-id counter (== CritterNewID)
 *   gCritterCountMax @0x8034462C  s32 high-water active count
 *   (gNumCritters    @0x8034466C  s32 active count - kept as lbl_8034466C
 *    because it is referenced by the already-matched soundmgr.c sndSysInit)
 *
 * Offsets VERIFIED against the GC DOL asm (dtk-extracted, via
 * tools/gdl/fnasm.py) across CritterEmptyInst, CritterInitInst, CritterNewInst,
 * CritterDelInst, ProcessCritter, CritterAnimate and CritterGetNextMove:
 *   index      0x000  sth   (pool slot index, CritterEmptyInst)
 *   id         0x002  sth   (gCritterNextID++, CritterEmptyInst)
 *   hdr        0x004  stw   (CritterHeader*; NULL == free slot; DelInst clears)
 *   state      0x008  lwz/stw (0/1/3 lifecycle; ProcessCritter)
 *   mtx        0x00C  arg   (GetWorldMat/CopyMat4 world matrix, 3x4)
 *   vel        0x03C  lfs   (per-frame delta added to pos in ProcessCritter)
 *   pos        0x05C  lfs   (world position; vel is accumulated into it)
 *   mbnode     0x06C  lwz   (scene/model node handle)
 *   anodes     0x0B4  lwz   (anode array base, stride 0x28; NewInst)
 *   anim       0x0C0  lwz   (anim-tree node; MBTreeClearFlags, CritterGetNextMove)
 *   curmove    0x118  lha   (current move index into hdr->moves[])
 *   nextmove   0x11A  lha   (next move index; CritterGetNextMove writes)
 *   rate       0x214  lfs   (move speed/timescale; CritterAnimate)
 *   healthmtr  0x44C  lha   (health-meter handle; >=0 -> HealthMeterUpdate)
 *   childcnt   0x44E  stb   (spawned child count; CritterNewInst)
 *   health     0x4B0  lfs   (current hp; hdr->maxhp * gCurLevel->0xAC on init)
 *   next       0xAD8  lwz   (sibling in active critter list)
 *   parent     0xADC  lwz   (parent critter; NULL for a root critter)
 * GC-vs-Xbox delta: the Xbox PDB has no CRITTER struct to compare, so no field
 * delta can be stated; the Xbox and GC share the CRITTER.OBJ function roster
 * 1:1 (see research/xbox_symbols/functions_by_module.txt), so the record is
 * expected to be the same Midway source struct.
 */

struct Critter;
struct CritterColDescriptor;  /* one stride-0x50 NODE record of a loaded
                               * CRITTER wad; completed in critter.c, the only
                               * TU that dereferences it */
struct CritterHeader;   /* loaded type template (CRITTER.OBJ CritterInitHeader);
                         * full layout not reconstructed - known offsets:
                         *   0x0E4 f32  base health scale
                         *   0x0E8 f32  (turn/anim rate)
                         *   0x0B4 f32  facing-yaw render offset
                         *   0x110 s16  move count
                         *   0x114 s16  (secondary per-move count)
                         *   0x118 s16  (child/hit-node count)
                         *   0x11C s16  first child def index
                         *   0x120 ptr  descriptor (->0x20 s16 ai/class type)
                         *   0x124 ptr  CritterMove moves[] (stride 0x90)
                         *   0x128 ptr  per-move sub-table (stride 0x50)
                         *   0x130 ptr  child def table (->0x14 base, stride 0x140)
                         *   0x138 ptr  geometry/type data (non-null == loaded) */

/* -- CritterTargetCriteria (0x20): the shared 8-float target-selection
 *    constraint block embedded in both CritterMove (@0x60) and CritterPattern
 *    (@0x30) and, at the type level, in CritterHeader (@0x80).  Verified
 *    field-for-field against every consumer that walks the raw float array:
 *    CritterCalcTarget/CritterReCalcTarget/CritterGetTargetSub read it as
 *    moveTarget[0..7] (indices 0/1 distance, 2 yaw, 3 dot, 4/5 rateScale,
 *    6 idle-time gate vs Critter.unk4AC, 7 max vertical delta); every index
 *    is read by at least one of the three consumers. -- */
typedef struct CritterTargetCriteria {
    f32 minDistance;    /* 0x00 CritterCalcTarget: reject if closer          */
    f32 maxDistance;    /* 0x04 CritterCalcTarget: reject if farther (<=0 == no cap) */
    f32 yaw;            /* 0x08 facing-cone yaw offset (YawVec3 -yaw)        */
    f32 minDot;         /* 0x0C facing-alignment dot-product threshold       */
    f32 minRateScale;   /* 0x10 reject if Critter.rateScale is lower         */
    f32 maxRateScale;   /* 0x14 reject if Critter.rateScale is >= (>0 == capped) */
    f32 idleGate;       /* 0x18 reject if Critter.unk4AC exceeds this (>0 == gated) */
    f32 maxVertical;    /* 0x1C CritterCalcTarget: reject if |vertical delta| exceeds */
} CritterTargetCriteria;  /* size 0x20 */

/* -- CritterMove (0x90): one scripted move/state in hdr->moves[].  Scanned by
 *    CritterGetNextMove (mulli *,0x90) and dispatched by CritterAnimate. -- */
typedef struct CritterMove {
    s32 type;             /* 0x00 move opcode (1, 0x11, 0xF0, ...)             */
    u32 flags;            /* 0x04 flag bits (bit 3 tested in ProcessCritter)   */
    s32 priority;         /* 0x08 crit_move.priority (compared against 0xF00
                             * in CritterAnimate)                              */
    s16 seqidx;           /* 0x0C resolved animation-sequence index            */
    s16 nodeidx;          /* 0x0E resolved attach-node index                   */
    char name[0x10];      /* 0x10 crit_move.name    -- editor/debug move name  */
    char anim[0x10];      /* 0x20 crit_move.anim    -- AtreeHeaderFindSeq name */
    char colnode[0x10];   /* 0x30 crit_move.colnode -- AtreeFindNodeIdx name   */
    s32 frameStart;       /* 0x40 primary event window start frame (CopyAnim)  */
    s32 frameStart2;      /* 0x44 secondary event window start frame           */
    s16 interruptAnim0;    /* 0x48 CritterAnimInterrupt anim index (event 1)    */
    s16 interruptAnim1;    /* 0x4A CritterAnimInterrupt anim index (event 2)    */
    f32 framePeriod;      /* 0x4C repeat period for the 0x85 (looped) move type */
    s16 frameEnd;         /* 0x50 primary event window end frame               */
    s16 frameEnd2;        /* 0x52 secondary event window end frame             */
    s16 link;             /* 0x54 chained/target move index (crit_move.nextidx) */
    s16 interrupt;        /* 0x56 crit_move.interrupt                          */
    s16 sfx;              /* 0x58 crit_move.movefx  -- file->sfx[] index       */
    s16 sfxFrame;         /* 0x5A crit_move.fxframe                            */
    s16 sfx2;             /* 0x5C crit_move.movefx2 -- file->sfx[] index       */
    s16 sfx2Frame;        /* 0x5E crit_move.fxframe2                           */
    CritterTargetCriteria target; /* 0x60 CritterMoveSetup/CritterLookForReady/
                                    * CritterChildGetPattern's moveTarget arg  */
    f32 cooldown;         /* 0x80 move reuse delay                             */
    f32 readyDistance;    /* 0x84 immediate-ready target threshold             */
    f32 turnRate;         /* 0x88 CritterRotate max turn rate (rad/tick, x frameStep) */
    f32 holdDuration;     /* 0x8C CritterAnimate move-hold/fade duration        */
} CritterMove;            /* size 0x90 */

/* -- CritterHitNode (0x5C): one runtime collision/attach node.  Critter has a
 *    fixed array of 16 of them at +0x4F8 (0x5C0 bytes == 16 * 0x5C); the live
 *    count is the owning type's CritterPackedType.colCount (+0x118), which
 *    CritterInitInst uses as the memset length (`colCount * 92`) and every
 *    collide/sfx walk uses as the loop bound.  Stride 0x5C is GC-verified
 *    (CritterUpdateSkinfx / CritterInitColnodes step by 0x5C; ProcessCritter
 *    indexes `c->unkAB8 * 0x5C`).  The 16-entry bound is consistent with the
 *    shipped assets: the NODE section of every CRITTER/*.WAD is a stride-0x50
 *    CritterColDescriptor table and the largest per-type colCount shipped is
 *    12 (GARM.WAD), with colBase + colCount within the file's NODE count for
 *    every type in all 18 files (build/a_lane/a_wad.py). -- */
typedef struct CritterHitNode {
    struct CritterColDescriptor *descriptor;
                              /* 0x00 the owning type's NODE record; set by
                               * CritterInitColnodes to
                               * `file->nodes + (hdr->colBase + i) * 0x50`,
                               * so the pointee is one stride-0x50 NODE entry
                               * (the WAD directory proves that stride) */
    void *volatile active;    /* 0x04 live atree/scene node; NULL == inactive  */
    void *boundNode;          /* 0x08 secondary node handle; walked via the
                                 * MBNode parent/child links in
                                 * CritterInitColnodes                          */
    f32 matrix[12];           /* 0x0C node world transform (3x4)                */
    f32 position[3];          /* 0x3C node world position                       */
    u8 _pad48[4];             /* 0x48                                           */
    void *dmgfx;              /* 0x4C optional DmgFxCircleAdd handle            */
    s32 state;                /* 0x50 per-node hit state                        */
    f32 activeUntil;          /* 0x54 window end   (active while from < until)  */
    f32 activeFrom;           /* 0x58 window start                              */
} CritterHitNode;             /* size 0x5C */

/* ==================================================================== *
 *  Critter - the active critter record (0xAE0 / 2784 bytes)            *
 * ==================================================================== */
typedef struct Critter {
    s16 index;                /* 0x000 pool slot index                        */
    s16 id;                   /* 0x002 unique id (gCritterNextID)             */
    struct CritterHeader *hdr;/* 0x004 loaded type template; NULL == free     */
    s32 state;                /* 0x008 lifecycle state (0/1/3)                */
    f32 mtx[3][4];            /* 0x00C world transform (3x4 Mtx)              */
    f32 vel[3];               /* 0x03C per-frame movement delta               */
    u8  _res048[4];           /* 0x048                                        */
    f32 movevec[3];           /* 0x04C vel copy w/ hdr facing-yaw applied      */
    u8  _res058[4];           /* 0x058                                        */
    f32 pos[3];               /* 0x05C world position                          */
    u8  _res068[4];           /* 0x068                                        */
    void *mbnode;             /* 0x06C scene/model node handle                 */
    u8  _res070[4];           /* 0x070                                        */
    void *colhandle;          /* 0x074 collision/link handle (AtreeDelete)     */
    u8  sound[0x20];          /* 0x078 embedded sound/voice control block      */
    f32 animtimer;            /* 0x098 anim blend accumulator                  */
    u8  _blk09C[0x14];        /* 0x09C .. 0x0B0                               */
    s32 anodeCount;           /* 0x0B0 number of animation-node records        */
    void *anodes;             /* 0x0B4 anode array base (stride 0x28)          */
    u8  _res0B8[4];           /* 0x0B8                                        */
    void *subnodes;           /* 0x0BC aux node list head (node->next @0x50)   */
    void *anim;               /* 0x0C0 anim-tree node (MBTreeClearFlags target)     */
    void *shadow;             /* 0x0C4 attached transform obj (->0x30 pos)     */
    void *hitnode0;           /* 0x0C8 hit/attach node (hdr->0x56 index)       */
    void *hitnode1;           /* 0x0CC hit/attach node (hdr->0x58 index)       */
    void *obj_d0;             /* 0x0D0                                        */
    void *emitter;            /* 0x0D4 particle/emitter handle (MBRemoveNode)   */
    u32  emitterset;          /* 0x0D8 emitter-present flag                    */
    void *hitnode2;           /* 0x0DC hit/attach node (hdr->0x5A index)       */
    f32 skinMatrix[12];       /* 0x0E0 ProcessSkinFX per-node transform (3x4) */
    f32 rateScale;            /* 0x110 move-rate scale (health-derived)       */
    f32 invRateScale;         /* 0x114 1.0 / rateScale                        */
    s16 curmove;              /* 0x118 current move index                     */
    s16 nextmove;             /* 0x11A next move index                        */
    s16 unk11C;               /* 0x11C (init -1)                              */
    s16 unk11E;               /* 0x11E (init -1)                              */
    s16 unk120;               /* 0x120 (init -1; CritterAnimate, <8)          */
    s16 movedone;             /* 0x122 move-complete flag (CritterAnimate)     */
    s16 unk124;               /* 0x124 (init -1)                              */
    s16 unk126;               /* 0x126 (init -1)                              */
    s16 unk128;               /* 0x128 (init -1)                              */
    s16 targetCount;          /* 0x12A active target count                     */
    s32 targetPlayer;         /* 0x12C targetInfo[0].player; also read alone as
                                * the single-target player index (CritterGetTarget) */
    u8  _blk130[4];           /* 0x130 .. 0x134 (targetInfo[0], +4)          */
    f32 targetAngle;          /* 0x134 targetInfo[0].angle; base of a walked,
                                * stride-0x24 per-target record array          */
    u8  _blk138[0x84];        /* 0x138 .. 0x1BC                              */
    f32 unk1BC[4][4];         /* 0x1BC 4x4 floats (init 0; per-limb scratch)   */
    f32 targetPos[3];         /* 0x1FC resolved target position                */
    u8  _blk208[4];           /* 0x208 .. 0x20C                              */
    s16 moveFlags;            /* 0x20C per-move activation flags             */
    s16 moveSfxFlags;         /* 0x20E per-move sound/particle flags          */
    u8  _blk210[4];           /* 0x210 .. 0x214                              */
    f32 rate;                 /* 0x214 move speed / timescale                 */
    f32 moveTimes[0x40];      /* 0x218 per-move last-use timestamps             */
    f32 patternTimes[0x20];   /* 0x318 per-pattern last-use timestamps         */
    f32 worldMoveMatrix[12];  /* 0x398 move-node world transform              */
    f32 moveOrigin[3];        /* 0x3C8 cached move origin                      */
    u8  _blk3D4[0x44];        /* 0x3D4 .. 0x418                              */
    f32 prevMovePathPos[3];   /* 0x418 prior-frame movePathPos snapshot        */
    u8  _blk424[4];           /* 0x424 .. 0x428                              */
    f32 moveMatrix[3];        /* 0x428 current move-space position             */
    u8  _blk434[4];           /* 0x434 .. 0x438                              */
    f32 floorContact[3];      /* 0x438 last FloorCollide contact point (CollideWorld) */
    u8  _blk444[8];           /* 0x444 .. 0x44C                              */
    s16 healthmtr;            /* 0x44C health-meter handle (>=0 == present)     */
    s8  childcnt;             /* 0x44E spawned child count                    */
    s8  alivecnt;             /* 0x44F live child count (ProcessCritter)       */
    u8  healthbar[0x48];      /* 0x450 health-bar object (AtreeDelete)         */
    void *damageflash;        /* 0x498 damage-flash object (MBTreeSetScale)       */
    f32 movePathPos[3];       /* 0x49C waypoint-move anchor position          */
    u8  _res4A8[4];           /* 0x4A8                                        */
    f32 unk4AC;               /* 0x4AC (init 0)                              */
    f32 health;               /* 0x4B0 current hp                             */
    f32 counterValue;         /* 0x4B4 transient move/event counter           */
    s32 counterState;         /* 0x4B8 state paired with counterValue          */
    f32 knockbackInput[3];    /* 0x4BC pending knockback direction/force       */
    u8  _res4C8[4];           /* 0x4C8                                        */
    f32 knockbackVelocity[3]; /* 0x4CC accumulated knockback vector            */
    u8  _res4D8[4];           /* 0x4D8                                        */
    f32 counterTime;          /* 0x4DC last counter-update timestamp           */
    s16 unk4E0[4];            /* 0x4E0 four ids (init -1)                     */
    f32 timed[4];              /* 0x4E8 expiry times paired with unk4E0 ids  */
    CritterHitNode hitnodes[16]; /* 0x4F8 collision/sfx nodes; hdr->colCount   */
                              /*       of them are live (0x5C0 bytes)          */
    s16 unkAB8;               /* 0xAB8                                        */
    s16 unkABA;               /* 0xABA (init -1)                              */
    s16 unkABC;               /* 0xABC (init -1)                              */
    s16 unkABE;               /* 0xABE (init 0)                              */
    s32 unkAC0;               /* 0xAC0 (init -1)                              */
    s16 pausecnt;             /* 0xAC4 anim pause counter (CritterAnimate)     */
    s16 unkAC6;               /* 0xAC6 (init 0)                              */
    f32 unkAC8;               /* 0xAC8 (init 0)                              */
    u8  _blkACC[4];           /* 0xACC .. 0xAD0                            */
    f32 unkAD0;               /* 0xAD0 target-score gate threshold          */
    void *particle;           /* 0xAD4 particle handle (FindClosestWaypoint)           */
    struct Critter *next;     /* 0xAD8 sibling in active critter list          */
    struct Critter *parent;   /* 0xADC parent critter (NULL if root)           */
} Critter;                    /* size 0xAE0 (2784) */

/* -- module globals (GC GUNE5D) -- */
extern Critter gCritterPool[16];             /* 0x80241204 (== gBig.blk234)    */
extern struct CritterHeader *gCritterHeaders[9][6]; /* 0x8024C004 hdr table    */
extern u16 gCritterNextID;                   /* 0x80343BE8 rolling unique id   */
extern s32 gCritterCountMax;                 /* 0x8034462C high-water count    */

#endif /* GAME_CRITTER_H */
