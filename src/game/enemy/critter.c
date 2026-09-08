/*
 * critter.c -- GCN CRITTER.OBJ.
 *
 * The critter behavior/load object between CONTROLS.OBJ and the sound-manager
 * object.  Critters are the large, scripted, multi-part creatures (golems,
 * bosses, generals) distinct from the swarm-style Enemy record.
 *
 * Bodies are transcribed from the GC (GUNE5D) DOL asm (tools/gdl/fnasm.py) with
 * Ghidra structure hints; function names are recovered from the Xbox
 * CRITTER.OBJ roster where cross-version behavior makes the mapping clear.
 * Every target function now has a translated body; remaining work is compiler
 * matching and replacing raw field offsets with recovered structures.
 *
 * .text       0x80034CFC..0x8004229C
 * extab       0x80005CE0..0x80005F28
 * extabindex  0x800093A0..0x8000970C
 */
#include "types.h"
#include "game/critter.h"
#include "game/effect.h"
#include "game/enemy.h"
#include "game/leveldata.h"
#include "game/mbobject.h"
#include "game/player.h"
#include "game/worldobj.h"

#define offsetof(type, memb) ((u32) & ((type*)0)->memb)

/* -- module-local BigState siblings (bss, pooled off gBig) -- */
typedef struct CritterBigState {
    f32 scratch[4];
    f32 safeRockTimers[16];
    s32 safeRockIndices[16];
    u8 _pad090[0x1A4];
    Critter pool[16];
} CritterBigState;

/* -- CritterItemView (0xF0): a file-local partial view of game/item.h's
 *    verified GC-exact Item record, covering only the fields
 *    CritterDropItem touches.  item.h itself is not #included here to avoid
 *    an extern-signature conflict with this TU's own PlaceItem/AddItemSub
 *    prototypes (a cross-TU extern-conflict fix is a separate claimed pass);
 *    offsets are GC-verified against include/game/item.h's Item/OBJGRP
 *    layout (info@0x00, objgrp.worldmat[3]@0x34, objgrp.node@0x64,
 *    minoff@0xCD). -- */
typedef struct CritterItemView {
    void *info;          /* 0x00 iteminfo* */
    u8 _pad04[0x30];
    f32 pos[3];           /* 0x34 objgrp.worldmat[3][0..2] (translation row) */
    u8 _pad40[0x24];
    void *node;            /* 0x64 objgrp.node */
    u8 _pad68[0x65];
    s8 minoff;               /* 0xCD */
    u8 _padCE[0x22];
} CritterItemView;             /* size 0xF0 */

/* -- atreeheader (misc.h, 0x38): the per-model animation-tree header that
 *    CritterPackedType.atree points at.  game/effect.h already forward-
 *    declares `struct atreeheader` (it types EffectHeader.atree) but never
 *    completes it, so the body is completed here, in the only TU that reaches
 *    into it.  GC-VERIFIED: CritterInitColnodes calls
 *    AtreeFindNodeIdx(atc+0xC, atc+0x10, name, 0x10) -- a find-node-by-name
 *    over a nodeinfo array of numnodes entries -- which pins nodeinfo@0x0C
 *    (pointer, lwz) and numnodes@0x10 (s32, lwz).  The remaining fields are
 *    Xbox-PDB reference only and are unreferenced by this TU. -- */
struct atreeheader {
    void *seq;               /* 0x00 atreeseq* sequence table  (PDB ref)   */
    void *animheader;        /* 0x04                           (PDB ref)   */
    void *oanimheader;       /* 0x08                           (PDB ref)   */
    void *nodeinfo;          /* 0x0C GC-verified: AtreeFindNodeIdx arg 0   */
    s32 numnodes;            /* 0x10 GC-verified: AtreeFindNodeIdx arg 1   */
    s32 numseqs;             /* 0x14                           (PDB ref)   */
    char prefix[30];         /* 0x18                           (PDB ref)   */
    s16 model;               /* 0x36                           (PDB ref)   */
};                           /* size 0x38 */

/* -- CritterDamageDef (0x50): the file->damage[] action-descriptor record,
 *    a type-tagged union read by CritterFirePlayerCollide/
 *    CritterNodePlayerCollide/CritterDamagePlayer (ray and melee damage
 *    delivery) and by CritterDoTexmodNode/CritterDoSfx's switch(desc->type)
 *    (visual/sound payload playback for the same table).  This reconciles
 *    two earlier partial reconstructions that disagreed at 0x2C ("damage" in
 *    one, "damageRadius" in the other, flagged 2026-08-30): three independent
 *    consumers (CritterDamagePlayer, CritterNodePlayerCollide, and
 *    CritterDoTexmodNode's coin-spawn path) all compute
 *    `field * gCurLevel->0xBC`, the project's standard damage-scale formula,
 *    from this offset -- "damage" is correct, "damageRadius" described only
 *    its secondary reuse as a node-collide proximity radius in
 *    CritterNodePlayerCollide.  flags@0x04/damage@0x2C/sfx@0x42 are the
 *    pre-existing, already-matched typed member names and keep their exact
 *    offset+type so CritterDamagePlayer's codegen is unaffected; every other
 *    field is new evidence from the CritterInitHeader swap loop plus its
 *    downstream readers (CritterFirePlayerCollide, CritterNodePlayerCollide,
 *    CritterDoTexmodNode, CritterDoSfx, CritterInitColnodes's
 *    ColDescriptor.sfxIndex indirection). -- */
typedef struct CritterDamageDef {
    s16 type;                /* 0x00 switch(desc->type) discriminant in CritterDoTexmodNode */
    s16 behaviorFlags;       /* 0x02 bit tests: 0x4000/0x1000/0x2000/0x800/0x40/8/4/1 */
    u32 flags;               /* 0x04 OR'd into damage_player's mode flags; also read as
                               * a DMG_TYPE-tagged bitfield (0x00020000/0x04000000) in
                               * CritterDoTexmodNode */
    f32 radius;             /* 0x08 hit cylinder radius (scaled by mbnode/body scale) */
    f32 maxDistance;         /* 0x0C max fire distance / ray-collide extent   */
    f32 minDistance;          /* 0x10 min fire distance   */
    f32 yaw;                    /* 0x14 fire direction yaw offset */
    f32 mindp;                     /* 0x18 Effects[].mindp min dot-product/cosine threshold
                                     * (also acosf'd directly at one CritterDoTexmodNode site) */
    f32 pitch;                    /* 0x1C fire direction pitch offset */
    f32 offset[3];                  /* 0x20 muzzle/node offset (MulVecMat3 input) */
    f32 damage;                       /* 0x2C damage amount, x gCurLevel->0xBC (see banner);
                                        * reused directly as a node-collide radius by
                                        * CritterNodePlayerCollide */
    f32 minSpeed;                       /* 0x30 CritterDoTexmodNode coin-spawn speed lerp lo */
    f32 maxSpeed;                         /* 0x34 coin-spawn speed lerp hi */
    f32 gravity;                            /* 0x38 CalcTargetDir/fn_80093E50 gravity-or-weight arg */
    f32 morphSpeed;                           /* 0x3C SfxSetMorph time (fallback if <= 0) */
    s16 sfxIndex;                               /* 0x40 primary trigger index into file->sfx[]
                                                  * (CritterDoSfx(c, desc+0x40, ...)) */
    s16 sfx;                                      /* 0x42 secondary/hit index into file->sfx[]
                                                     * (CritterDamagePlayer.sfx; also
                                                     * ColDescriptor.sfxIndex -> desc+0x40/42/44/46
                                                     * batch in CritterInitColnodes) */
    s16 morphTargetIndex;                            /* 0x44 index into file->sfx[]+8 (morph target) */
    s16 morphIndex;                                    /* 0x46 index into file->sfx[]+8 (morph shape) */
    f32 yawSpread;                                       /* 0x48 random yaw jitter range (Random()) */
    f32 unk4C;                                             /* 0x4C crit_damage.dummy2 -- no GC consumer */
} CritterDamageDef;                  /* size 0x50 (== Xbox PDB crit_damage) */

/* -- CritterColDescriptor (0x50): the type-table record CritterHitNode's
 *    descriptor points at once resolved by CritterInitColnodes.  Verified
 *    against CritterInitColnodes, CritterCollideWorld and CritterCollideItems
 *    (flags@0x10 bit 8, radius@0x2C used identically in both). -- */
typedef struct CritterColDescriptor {
    char nodeName[0x10];   /* 0x00 the node's model-object name, NUL-terminated.
                            * FILE EVIDENCE: DRAGON.WAD NODE records 1 and 2 hold
                            * "NODE#00" and "NODE#06" here (build/a_lane/a_dump.py
                            * DRAGON.WAD NODE), and CritterDamage passes this
                            * address straight to sprintf("%sD%s", desc->prefix,
                            * node->descriptor) to build the broken-part object
                            * name -- so offset 0 is text, not padding.        */
    s16 flags;             /* 0x10 bit 0x8 = enabled for wall/item collide checks */
    s16 sfxIndex;           /* 0x12 index into file->damage[] (CritterInitColnodes) */
    s16 nodeIndex;           /* 0x14 attach-node lookup index (>=0 == direct)      */
    s16 zsortParam;           /* 0x16 MBTreeSetZsortAdd priority                    */
    f32 maxTargetDistance;     /* 0x18 CritterLineRootColSub rejects the node past
                                * this range (<=0 == uncapped); shipped 0.0 / 20.0 */
    f32 targetScoreScale;        /* 0x1C CritterLineRootColSub score denominator
                                  * `nd / (scale * (dot - thresh))`; shipped 1.0/10.0 */
    f32 position[3];           /* 0x20 dmg-fx circle add position/offset            */
    f32 radius;                 /* 0x2C wall/item collide radius (DRAGON NODE#00
                                 * carries 4.0 next to its own " rad 4.0" note)     */
    char name[0x10];             /* 0x30 attach spec: an atree node name, or '+'/'-'
                                  * plus a digit meaning "walk N child/parent links"
                                  * (shipped "-1", "-3")                             */
    f32 damageScale;               /* 0x40 per-node incoming-damage multiplier
                                    * (CritterDamage: `damage *= desc->damageScale`);
                                    * shipped 1.0                                     */
    f32 healthScale;               /* 0x44 per-node health-decay multiplier             */
    u8 _pad48[8];
} CritterColDescriptor;

/* -- CritterSfxRecord (0x50): one entry of a loaded type's file->sfx[] table,
 *    resolved lazily and recursively by CritterInitSfx, then triggered by
 *    CritterDoSfx/CritterDoSfxSub/CritterDoParticle.  The 0x20-0x30 text run
 *    was over-claimed as one 0x24-byte levelFmt by an earlier pass; the swap
 *    loop proves 0x30-0x44 are five real floats (CritterDoSfx reads
 *    entry+0x30/34/38 as a color[3] scaled by node scale), so levelFmt is the
 *    16 bytes at 0x20 only. -- */
typedef struct CritterSfxRecord {
    u32 flags;              /* 0x00 texture/effect kind bits (& 0x0F000100 branch)   */
    s32 linkIndex;           /* 0x04 next linked sfx-record index (-1 == none)        */
    s32 textureId;            /* 0x08 cached texmod/texture id (<0 == unresolved); also
                                * read as an effect id by CritterDoSfxSub               */
    s32 audioId;                /* 0x0C cached audio sound id (<0 == unresolved)         */
    char name[0x10];              /* 0x10 texmod/effect name                               */
    char levelFmt[0x10];            /* 0x20 sprintf format string keyed by level (CORRECTED
                                      * from 0x24: swap evidence puts real floats at 0x30) */
    f32 color[3];                     /* 0x30 CritterDoSfx: RGB scaled by node scale        */
    f32 life;                           /* 0x3C particle lifetime (CritterDoParticle etime)/
                                          * StartFXSub duration (CritterDoSfxSub)             */
    f32 rate;                             /* 0x40 particle emission rate scale (also read as
                                            * skinValue in the 0x100-flag SetSkinFX branch)   */
    s16 custom0;                      /* 0x44 InitCustomEffect param a                        */
    s16 custom1;                        /* 0x46 InitCustomEffect param b                        */
    u32 tintColor;                        /* 0x48 MBTreeSetColor RGBA (sentinel 0xFFFFFFFF == none) */
    f32 scale;                              /* 0x4C uniform Effects[].node scale (CritterDoSfxSub) */
} CritterSfxRecord;                      /* size 0x50 */

typedef struct CritterPattern {
    u8 _pad00[0x10];
    s16 flags;
    s16 unk12;              /* 0x12 swapped s16, no consumer found in this TU */
    f32 cooldown;
    u8 _pad18[8];
    s16 moveidx[8];         /* 0x20 the pattern's move-index chain, -1 padded.
                             * FILE EVIDENCE (build/a_lane/a_dump.py CHIMERA.WAD
                             * PTRN): every shipped record holds eight halfwords
                             * here, e.g. {6, 6, -1, -1, -1, -1, -1, -1} and
                             * {9, 10, -1 x6}; CritterInitHeader's swap loop
                             * walks exactly 8 of them from 0x20, and the Xbox
                             * PDB crit_pattern carries s16 moveidx[8] at 0x20.
                             * The two readers index it as [unk120 + 1], which
                             * is the same address the earlier `move` +
                             * `sequence[7]` split produced. */
    CritterTargetCriteria target; /* 0x30 CritterChildCriticalMove's
                                    * CritterGetTargetSub(c, pattern+0x30, 0)
                                    * arg -- same block as CritterMove.target */
} CritterPattern;

/* (A second, partial reconstruction of the type header -- CritterWorldHeader,
 *  used by the collide family -- was reconciled into CritterPackedType below:
 *  flags->typeFlags@0x5C, radius@0x78, wallRadius@0x7C, floorOffset@0xB0,
 *  damageScale@0xB8, hitNodeCount->colCount@0x118 all agreed byte-for-byte,
 *  and CritterPackedType is the GC-evidence winner: self-consistent to 0x140,
 *  the container->types[] stride, and equal to the Xbox PDB crit_type size.) */

extern CritterBigState gBig;
extern void *lbl_80241060[4];         /* 0x80241060 loaded-file handle table    */
extern u8    lbl_80241070[4][0x50];   /* 0x80241070 per-type header buffers      */
extern Player gPlayers[4];        /* 0x80275AE0 player records (gPlayerRecords) */

/* -- module-local sbss variables -- */
extern void *lbl_80344648;            /* 0x80344648 pending callback context     */
extern s32   lbl_80344644;            /* 0x80344644 pending callback flag        */
extern s32   lbl_8034465C;            /* 0x8034465C active-player count           */
extern s16   lbl_80344664;            /* 0x80344664 rolling tick counter          */
extern s32   lbl_80344660;            /* 0x80344660 loaded-type count             */
extern s32   lbl_8034466C;            /* 0x8034466C active critter count (gNumCritters) */
extern s32   lbl_80344650;            /* 0x80344650 safe-rock collection flags    */
extern s32   lbl_80344654;            /* 0x80344654 selected safe-rock slot        */
extern s32   lbl_80344658;            /* 0x80344658 collected safe-rock count      */
extern f32   gClockFrameStep;         /* 0x80344590 frame delta                     */
extern f32   lbl_803447D8;            /* boss/player damage scaling gate             */
extern s32   sMusicTrackHi;
extern void *lbl_80344EB4;
extern f32   lbl_80343BEC;            /* 0x80343BEC tunable float (10.0)             */
extern volatile f32 sMusicFadeBase;   /* 0x80344594 shared game-time / fade base   */
extern f32   lbl_80346480;
extern f32   lbl_80346470;
extern f64   lbl_80346478;
extern f64   lbl_80346488;
extern f64   lbl_80346490;
extern f64   lbl_803464B0;
extern f32   lbl_803464B8;
extern f32   lbl_803464BC;
extern f64   lbl_803464F8;
extern f64   lbl_803464C8;
extern f64   lbl_803464D0;
extern f64   lbl_803464D8;
extern f64   lbl_803464E0;
extern f64   lbl_80346500;
extern f32   lbl_80346590;
extern f32   lbl_80346594;
extern f32   lbl_80346598;
extern f64   lbl_803465A0;
extern f64   lbl_803465A8;
extern f64   lbl_803465B0;
extern f32   lbl_803464C0;
extern f32   lbl_803465F8;
extern f32   lbl_80346508;
extern f64   lbl_80346558;
extern f64   lbl_80346510;
extern f32   lbl_80346518;
extern f32   lbl_8034651C;
extern f32   lbl_80346520;
extern f32   lbl_80346524;
extern f64   lbl_80346528;
extern f64   lbl_80346530;
extern f64   lbl_80346540;
extern f32   lbl_803465F4;
extern f32   lbl_80346548;
extern f32   lbl_8034654C;
extern f32   lbl_80346560;
extern f64   lbl_80346568;
extern const char lbl_80346574[];
extern f64   lbl_80346580;
extern f64   lbl_80346600;
extern f64   lbl_80346630;
extern f32   lbl_8034663C;
extern f32   lbl_80346638;
extern f64   lbl_80346620;
extern f32   lbl_80346628;
extern f32   lbl_8034662C;
extern f32   lbl_803464A8;
extern f32   lbl_803464E8;
extern f32   gClockTime;
extern char  lbl_801121D4[];
extern char  lbl_80112174[];
extern Effect Effects[];
extern void  MBPsysSetEVolume(void *psys, f32 a, f32 b);
extern void  MBPsysSetPParm(void *psys, s32 n, f32 a, f32 b, f32 c, f32 d);

/* -- external helpers -- */
extern void *AllocFile(const char *wad, const char *name);
extern void *NextWaypoint(void *player);
extern void  AddExp(s32 player, s32 amount, s32 kind);
extern void  HealthMeterUpdate(f32 value, s32 meter);
extern void *memset(void *dst, int c, u32 n);
extern void *memcpy(void *dst, const void *src, u32 n);
extern void  ErrorPrintf(const char *fmt, ...);
extern void  FatalError(const char *msg, int code);
extern void  MBRemoveNode(void *node, s32 kind);
extern void  MBSetObject(void *node, s32 object);
extern s32   GetWorldMat(void *node, f32 *matrix, f32 *offset);
extern void  GetYawPitch(const f32 *vector, f32 *yaw, f32 *pitch);
extern void  ExtractPYR(void *matrix, f32 *angles);
extern void  CreatePYRMatrix(void *matrix, const f32 *angles);
extern s32   HealthMeterStart(void *header, s32 x, s32 y, s32 width,
                              s32 height, s32 style, f32 health);
extern void *AtreeMatch(void *header, const char *name, s32 report);
extern void *AtreeInit(void *header, void *tree, s32 flags, s32 size);
extern void  MBNodeSetParent(void *node, void *parent);
extern void  MBTreeSetFlags(void *node, u32 flags, s32 mode);
extern void  MBTreeSetAltTex(void *node, u32 mask, u32 texture, s32 mode);
extern void  MBTreeSetAmbientAdd(void *node, s32 value, s32 mode);
extern void  MBTreeSetZsortAdd(void *node, s32 value, s32 mode);
extern void *AtreeFindNode(void *tree, const char *name, s32 length);
extern s32   AtreeFindNodeIdx(void *tree, s32 count, const char *name,
                              s32 length);
extern void  AtreeNodeSetParent(void *node, void *parent, void *root,
                                s32 reparent);
extern void  SfxDeleteParented(void *sfx, s32 a, s32 b);
extern void  BossDeath(void);
extern void  del_target(void *mtx);
extern void  AtreeDelete(void *handle);
extern s32   CollectSafeRocks(s32 *out, s32 max, s32 flags);
extern void  SafeRockActivate(s32 index);
extern u32   RandInt(u32 limit);
extern s32   NextGridEnemy(void);
extern void  StartEnemyGrid(f32 *position, f32 radius);
extern s32   fn_8005D5C8(Critter *c, u8 *item);
extern void *FindClosestWaypoint(f32 maxDist, f32 *pos, s32 all);
extern f32   fn_8005F0F4(void *item, f32 *nodepos, f32 *center, f32 *out,
                         f32 radius, f32 height);
extern f32   fn_8005C1DC(u8 *item, s32 a, s32 b, void *hdr, f32 damage);
extern s32   NextGridItem(void);
extern void  StartItemGrid(f32 radius, f32 *position);
extern void  MulVecMat3(const f32 *vector, f32 *out, const f32 *matrix);
extern void  MulVecMat4(const f32 *vector, f32 *out, const f32 *matrix);
extern void  MulBodyVecMat4(const f32 *vector, f32 *out, const f32 *matrix);
extern s32   LineCylinderCollide(f32 *p1, f32 a, f32 b, f32 *p2, f32 *dest,
                                 f32 *contact, s32 flag);
extern void  damage_enemy(void *enemy, s32 a, s32 b, f32 radius, void *p1,
                          void *p2, s32 c);
extern s32   lbl_803447DC;
extern s32   damage_player(s32 player, f32 damage, s32 mode, u32 flags,
                           f32 *direction);
extern f32   NormalVector(f32 *vector);
extern f32   NormalVector2D(f32 *vector);
extern u32   WorldObjGetAllFlags(void *object);
extern f32   SlowNormalVector(f32 *vector);
extern f32   fqdist(f32 x, f32 z);
extern s32   fn_8005FB48(f32 radius, f32 *from, f32 *to,
                         f32 *limitPosition, s32 stopAtFirst);
extern void  ProcessSkinFX(f32 *state, void *root, void *node);
extern void *MBNewNode(void *parent, const f32 *matrix, s32 mode);
extern void  CopyMat3(const f32 *src, f32 *dst);
extern void  CopyMat4(const f32 *src, f32 *dst);
extern void  MulVec4Mat3(const f32 *src, f32 *dst, const f32 *matrix);
extern void  UnparentMatrix(void *node, f32 *matrix);
extern void *lbl_8034473C;
extern s32   gBossType;
extern f32   lbl_8011AEAC[];
extern s32   gFrameTicks;
extern u32   lbl_80344BF8;
extern u8    lbl_802411B0[0x540];
extern s32   lbl_80344668;
extern void *crit_load_desc;
extern s32  *lbl_80344640;
extern s32   lbl_80344630;
extern s32   lbl_80344634;
extern s32   lbl_80344638;
extern char  lbl_801120E0[];          /* 0x801120E0 rodata format-string anchor    */
extern s32  *lbl_8025776C[8];         /* 0x8025776C item/def pointer table          */
DECL_SECT(".sdata2") extern const char lbl_8034664C[]; /* 0x8034664C wad name       */
extern void *gWorldData;              /* 0x80344838 world data record                */
extern s32   FileSize(char *name, const char *wad);
extern s32  *StartFileRead(char *name, const char *wad, s32 mode, s32 size,
                           s32 arg, void *callback);

/* 0x30 == Xbox PDB crit_desc (name/prefix/etype/model/loaded/didcount/
 * atreelist/dummy1); GC behavioral names kept for the already-adopted tail. */
typedef struct CritterDescriptor {
    char name[0x10];    /* 0x00 crit_desc.name                                */
    char prefix[0x10];  /* 0x10 crit_desc.prefix -- ErrorPrintf id string     */
    s16 type;
    s16 modelIndex;
    s16 loadState;
    s16 loadTick;
    void *model;
    u8 _pad2C[4];
} CritterDescriptor;

/* crit_type (Xbox PDB misc.h, Size=0x140) is the loaded type template that
 * Critter.hdr points at, so this completes the header's `struct
 * CritterHeader` tag rather than declaring a separate type. */
typedef struct CritterHeader {
    char suffix[0x10];      /* 0x00 crit_type.suffix -- appended to the descriptor
                             * prefix to build the atree name (CritterLoadFinish
                             * passes this address straight to sprintf "%s%s")  */
    char rootnode[0x10];    /* 0x10 crit_type.rootnode                          */
    char nodeName0[0x10];   /* 0x20 attach-node name (CritterLoadFinish -> 0x56 idx) */
    char nodeName1[0x10];   /* 0x30 attach-node name (CritterLoadFinish -> 0x58 idx) */
    char nodeName2[0x10];   /* 0x40 attach-node name (CritterLoadFinish -> 0x5A idx) */
    s16 descriptorIndex;
    s16 subtype;
    s8 level;               /* 0x54 crit_type.level                             */
    s8 ai;                  /* 0x55 crit_type.ai                                */
    s16 node0Index;         /* 0x56 resolved nodeName0 atree index                  */
    s16 node1Index;         /* 0x58 resolved nodeName1 atree index                  */
    s16 node2Index;         /* 0x5A resolved nodeName2(0x40) atree index             */
    u32 typeFlags;         /* 0x5C runtime flag bits (bit 0x10000 = expanded moves) */
    f32 lookYawRate0;       /* 0x60 CritterLookAtPlayer hitnode0 max yaw turn/tick  */
    f32 lookYawRate1;        /* 0x64 hitnode1 max yaw turn/tick                     */
    f32 lookPitchRate0;       /* 0x68 hitnode0 max pitch turn/tick                   */
    f32 lookPitchRate1;        /* 0x6C hitnode1 max pitch turn/tick                  */
    f32 lookPitchBias0;         /* 0x70 hitnode0 static pitch offset                 */
    f32 lookPitchBias1;          /* 0x74 hitnode1 static pitch offset                */
    f32 radius;                    /* 0x78 world/player collide radius (collide family) */
    f32 wallRadius;                  /* 0x7C wall collide radius (collide family) */
    CritterTargetCriteria target;      /* 0x80 CritterLookForReady/CritterGetSingleTargetPlayer's
                                         * default CritterCalcTarget(c, hdr+0x80, ...) constraints */
    f32 defaultPos[3];        /* 0xA0 default movePathPos seed (CritterInitInst) */
    f32 speed;               /* 0xAC CritterTranslate move speed                    */
    f32 floorOffset;          /* 0xB0 floor-contact Y offset (CritterCollideWorld)     */
    f32 vertDrift;             /* 0xB4 constant Y addend folded into c->movevec before
                                 * MulVec4Mat3(hdr+0xC0, c->pos, ...) (ProcessCritter-family) */
    f32 damageScale;             /* 0xB8 per-type damage multiplier (x gCurLevel dmg scale) */
    f32 armor;                     /* 0xBC flat damage-reduction constant                 */
    f32 originOffset[3];             /* 0xC0 MulVec4Mat3 local-space body offset input     */
    f32 turnLimit;                     /* 0xCC CritterRotate max facing-correction angle    */
    f32 unkD0[3];                        /* 0xD0 swapped f32 vec3, no consumer found in TU  */
    f32 unkDC;                             /* 0xDC swapped f32, no consumer found in TU     */
    u32 shieldFlags;                         /* 0xE0 tested by ModifyDamage-adjacent code   */
    f32 maxHealth;                             /* 0xE4 x gCurLevel->0xAC == starting/max hp */
    f32 expValue;                                /* 0xE8 base experience award, x hit ratio */
    f32 wakeThreshold;                             /* 0xEC min best-target score to stay active */
    f32 unkF0;                                       /* 0xF0 swapped f32, no consumer found in TU */
    s16 sfxIndex0;          /* 0xF4 type-level sfx descriptor index (idle/ambient)  */
    s16 sfxIndex1;          /* 0xF6 type-level sfx descriptor index (idle/ambient)  */
    s16 meterX;              /* 0xF8 health-meter HealthMeterStart x                */
    s16 meterY;              /* 0xFA health-meter HealthMeterStart y                */
    s16 meterW;              /* 0xFC health-meter HealthMeterStart width            */
    s16 meterH;              /* 0xFE health-meter HealthMeterStart height           */
    f32 healthbarOffset[3];  /* 0x100 healthbar root-node position offset           */
    u8 _pad10C[4];
    s16 moveCount;          /* 0x110 move table entry count (CritterInitMoves)      */
    s16 moveIndex;          /* 0x112 base index into container->moves[]             */
    s16 auxMoveCount;       /* 0x114 secondary move count (high-water tracked)      */
    s16 patternIndex;       /* 0x116 base index into container->patterns[]          */
    s16 colCount;           /* 0x118 collision/hit-node entry count                 */
    s16 colBase;            /* 0x11A base index into container->nodes[]             */
    s16 childIndex;         /* 0x11C first child type index (container->types[]);
                              * < 0 == no child (CritterInitInst's child-spawn walk) */
    s16 parentIndex;        /* 0x11E parent type index (container->types[]); < 0 == none */
    CritterDescriptor *descriptor;
    CritterMove *movesPtr;      /* 0x124 resolved move table base (stride 0x90)     */
    struct CritterPattern *patternsPtr; /* 0x128 resolved pattern table base (stride 0x50) */
    CritterColDescriptor *colnodesPtr; /* 0x12C resolved NODE table base           */
    struct CritterFileHeader *file;
    struct CritterAddAnim *attachments; /* 0x134 head of this type's ADDA list,
                              * threaded by CritterInitHeader through
                              * CritterAddAnim.next                          */
    void *atree;
    u8 _pad13C[4];
} CritterPackedType;

/* -- CritterFileHeader: the runtime record for one loaded CRITTER wad.  The
 *    eight (count, pointer) pairs are the wad's eight sections, filled by
 *    MBGetFromWad from the file's own trailing directory.  FILE EVIDENCE
 *    (build/a_lane/a_wad.py over all 18 orig/GUNE5D/Gauntlet/CRITTER/*.wad):
 *    the container is `u32 dirOffset; u32 sectionCount; u32 0; u32 0;` then the
 *    payloads back to back, then sectionCount 16-byte directory entries
 *    `char tag[4] (stored reversed); u32 offset; u32 count; u32 count`.  Every
 *    file's section extents divide exactly by these strides, which is what
 *    types the pointers below:
 *      SFXX 0x50 CritterSfxRecord      DAMG 0x50 CritterDamageDef
 *      DESC 0x30 CritterDescriptor     ADDA 0x30 CritterAddAnim
 *      NODE 0x50 CritterColDescriptor  MOVE 0x90 CritterMove
 *      PTRN 0x50 CritterPattern        TYPE 0x140 CritterPackedType
 *    (e.g. DRAGON.WAD: SFXX@0x10 x21, DAMG@0x6A0 x17, DESC@0xBF0 x1,
 *    ADDA@0xC20 x0, NODE@0xC20 x8, MOVE@0xEA0 x41, PTRN@0x25B0 x0,
 *    TYPE@0x25B0 x1, directory at 0x26F0 -- each section ends exactly where
 *    the next begins.)  No stride is inferred from spacing alone: the
 *    directory states each section's start and count independently. -- */
typedef struct CritterFileHeader {
    s32 state;
    s32 wad[3];
    s32 typeCount;
    CritterPackedType *types;
    s32 descriptorCount;
    CritterDescriptor *descriptors;
    s32 addAnimCount;
    struct CritterAddAnim *addAnims;
    s32 moveCount;
    CritterMove *moves;
    s32 patternCount;
    CritterPattern *patterns;
    s32 nodeCount;
    CritterColDescriptor *nodes;
    s32 damageCount;
    CritterDamageDef *damage;
    s32 sfxCount;
    CritterSfxRecord *sfx;
} CritterFileHeader;

/* -- CritterAddAnim (0x30): one file->addAnims[] entry, a singly-linked list
 *    node CritterInitHeader threads onto its owning type's
 *    CritterPackedType.attachments head (walking the existing tail+8 chain)
 *    and CritterAddAnimInsts (0x8003F1F0) later instantiates.  Every field
 *    below is read there: typeIndex/flags/atree/next drive the walk and
 *    AtreeInit call, name is passed to ErrorPrintf("%s") on a bad instance,
 *    attachNodeName feeds AtreeFindNode(..., 8), and offset[3] seeds the new
 *    node's matrix translation row (mbnode+0x30/0x34/0x38). -- */
typedef struct CritterAddAnim {
    s16 typeIndex;    /* 0x00 owning CritterPackedType index (container->types[]) */
    s16 flags;        /* 0x02 bit 0x1 = has an attach-node name (else default parent) */
    void *atree;      /* 0x04 secondary atree handle for AtreeInit                */
    struct CritterAddAnim *next; /* 0x08 next entry linked onto the same type      */
    u8 _pad0C[4];
    char name[8];      /* 0x10 debug name (ErrorPrintf "Bad critter anim inst: %s") -- text, unswapped */
    char attachNodeName[8]; /* 0x18 AtreeFindNode(&c->colhandle, name, 8) target -- text, unswapped */
    f32 offset[3];      /* 0x20 local position offset applied to the new node's matrix */
    u8 _pad2C[4];
} CritterAddAnim;   /* size 0x30 */

extern void  fn_8001267C(s32 handle, s32 index, s32 flag);
extern void  InitTexMods(s32 handle, s32 index);
extern s32   LoadModel(char *name, void *out, s32 a, s32 b);
extern s32   fn_8005A1EC(char *name, void *out);
extern s32   MBOX_BGLoadModelDone(void);
extern void  MBOX_BGLoadModelStart(const char *name, s32 model);
extern s32   AtreeHeaderFindSeq(void *atree, const char *name);
extern s32   FindTexMod(void *atree, const char *name, void *unused);
extern s32   AudioFindSound(const char *name, s32 bank, s32 global);
extern s32   MBOX_FindTexture(const char *name, void *unused);
extern s32   MBOX_FindTexture_Sub(const char *name, void *unused,
                                  s32 model, s32 fallback, s32 mode);
extern s32   AtreeModel(void *atree);
extern s32   InitCustomEffect(void *atree, char *name, s32 zmod, s32 alpha);
extern s32   sprintf(char *dst, const char *fmt, ...);
extern f32   atan2(f32 y, f32 x);
extern void  YawMat3(f32 angle, f32 *matrix);
extern void  WPitchMat3(f32 *matrix, f32 angle);
extern void  YawVec3(f32 *source, f32 *out, f32 angle);
extern void  PitchVec3(f32 *source, f32 *out, f32 angle);
extern f32   Random(f32 range);
extern void  CalcTargetDir(f32 *velocity, f32 targetScale, f32 speed,
                           f32 gravity, f32 lift);
extern void  PlaceEffectOnFloor(s32 effect, f32 *matrix);
extern void  SfxSetHit(s32 effect, s32 hitEffect, s32 hitAudio,
                       s32 wallSound);
extern void  SfxSetMorph(f32 time, s32 effect, s32 morph1, s32 morph2);
extern void  SfxSetPhysics(s32 effect, f32 *velocity, f32 *angularVelocity,
                         f32 weight, f32 radius);
extern void  DmgFxAdd(s32 effect);
extern u16   AnimateATree(void *tree, s32 sequence, s32 transition);
extern s32   AnimDone(void *animation);
extern void  MBTreeClearFlags(void *node, u32 flags, s32 mode);
extern s32   StartFXSub(s32 effect, f32 *position, u32 flags,
                        u32 treeFlags, f32 time);
extern void  SfxSetOwner(s32 effect, s32 owner);
extern void  SfxSetParent(s32 effect, void *parent);
extern void  SfxSetMat(s32 effect, f32 *matrix, void *unused);
extern void  MBTreeSetColor(void *node, s32 color, s32 mode);
extern void  MBTreeSetScale(f32 x, f32 y, f32 z, void *node);
extern void *MBNewPsysDefault(const f32 *matrix, void *parent,
                              s32 localSpace, s32 active);
extern void  MBPsysSetPTex(void *psys, s32 texture);
extern void  MBPsysSetERate4(f32 a, f32 b, f32 c, f32 d, void *psys);
extern void  MBPsysSetETime(f32 life, f32 variance, void *psys);
extern void  MBPsysSetPSpeed(void *psys, f32 speed);
extern void *PlaceItem(s32 type, s32 subtype, const char *name, f32 *position);
extern void  AddItemSub(void *item);
extern void  StartBagFX(f32 *position, void *item, f32 scale);
extern char  lbl_803465EC;
extern void  msgPost(s32 message, s32 target, s32 value);
extern char *fn_80057ACC(s32 slot);
extern s32   toupper(s32 c);
extern void  DoTexMods(void *atree);
extern s32   MBSetupWad(s32 *wad, s32 base);
extern s32   MBGetFromWad(s32 *wad, s32 key, s32 *sizeOut);
extern u8   *sItems;
extern void *lbl_80241020[16];
extern s32   SafeRockActive(void *rock);
extern void *ItemGetNode(void *rock);
extern s32   PlayerAttacking(s32 player, s32 mode);
extern s32   player_can_be_damaged(void *player);
extern void  GetPlayerColPos(s32 i, f32 *out);
extern f64   __fabs(f64 x);
extern char  lbl_8011221C[];          /* 0x8011221C critter-overflow message      */
extern const char lbl_80112238[];
extern char  lbl_801122F0[];
extern f32   gIdentityMatrix[12];
DECL_SECT(".sdata2") extern const char lbl_80346644[];
extern level_data *gCurLevel;         /* current level record (game/leveldata.h)  */
extern char  lbl_8011219C[];          /* move-type lookup failure message          */
extern void *MBOX_ReallyFindObject(const char *name, s32 type1, s32 type2,
                                    s32 exact);
extern void *MBNewObject(void *object, f32 *matrix, void *parent, u32 flags);
extern void *FloorCollide(f32 *pos, s32 a, s32 b, s32 mode, f32 x, f32 y,
                          f32 z);
extern u8    gFloorCollisionResult[]; /* 0x8023CAE0 world-collide result, mtx+Y   */
extern f32   lbl_8023CA98[];
extern void *EnemyWallCollide(f32 radius, f32 *from, f32 *to, f32 *normal);
extern s32   SlideAlongWall(f32 radius, f32 *pos, f32 *vel, f32 *wallpt,
                            f32 *normal);
extern char *lbl_8011AEA0[3];         /* 0x8011AEA0 shadow model-name table        */
extern f32   lbl_80346588;
extern f32   lbl_8034658C;
extern f32   lbl_80346618;
extern f32   lbl_80346640;
extern void  BossActivate(void *obj, s32 flag);
extern s32   gTriggerCameraState;
extern void  MBTreeSetAlpha(void *node, s32 alpha, s32 propagate);
extern f32   lbl_803464EC;
extern f64   lbl_803465C0;
extern u64   gControllerButtons;      /* 0x803445C8 (low word aliases sFlags)     */
extern void  SetSkinFX(void *fx, s32 base, s32 frames, s32 loops, f32 rate);
extern void  AudioPlay3DSel(s32 sound, s32 volume, f32 *position, s32 selector);
extern void  ShakeCamera(s32 type, s32 count, s32 delay, f32 radius,
                         s32 priority);
extern void  SafeRockSetup(void);
extern s32   lbl_802897B8[];          /* 0x802897B8 skinfx palette table          */
extern char  lbl_801121C0[];          /* 0x801121C0 killfx overflow message       */
extern f32   lbl_80346570;
extern f32   lbl_8034464C;
extern u32   sFlags;
extern s32   gBossDead;
extern s32   gGameOptions[];
DECL_SECT(".sdata2") extern const char lbl_803465E0[];
DECL_SECT(".sdata2") extern const char lbl_803465E4[];
DECL_SECT(".sdata2") extern const char lbl_803465E8[];
extern char  lbl_80112104[];
extern char  lbl_8011213C[];
extern char *strcpy(char *dst, const char *src);
extern f32   acosf(f32 value);
extern void  camera_request_change(s32 value, s32 mode);
extern s32   DoAnimateTreeFrame(void *tree, s32 sequence, s32 frame,
                                s32 recurse);
extern void  DrawText(s32 x, s32 y, s32 font, u32 color,
                      const char *format, ...);
extern void  ModifyDamage(f32 armor, f32 *damage, u32 *damageType,
                          u32 shield);
extern void  do_heal_players(void *player, f32 *matrix, f32 amount);
extern s32   fn_800945D0(f32 *position, f32 *matrix, s32 damageType,
                         s32 alternate, s32 kind, f32 scale);
extern void  BossDying(void);
extern f32   lbl_8011AEC0[];

/* -- CRITTER.OBJ internal roster (forward declarations) -- */
struct CritterDamageDef;
s32  CritterCollideEnemies();
s32 CritterCollideItems(Critter *c, f32 *delta, s32 hits);
s32 CritterCollidePlayers(Critter *c, f32 *delta, s32 hits);
u32 CritterCollideWorld();
void CritterWorldDamage(Critter *c, void *surface, f32 *origin,
                        f32 *contact);
s32 CritterNodeEnemyCollide(Critter *c, void *damageDef);
s32  SafeRockNearestTarget(s32 player);
void CritterLookAtPlayer(Critter *c, CritterMove *move);
void NodeLookAtPos(void *node, f32 *target, f32 a, f32 b, f32 *yaw, f32 c,
                   f32 d, f32 *pitch);
void CritterFirePlayerCollide(Critter *c, struct CritterDamageDef *damage);
s32 CritterNodePlayerCollide(Critter *c, struct CritterDamageDef *damage,
                              s32 enabled);
void CritterAwardExp(s32 who, f32 amount);
struct CritterDamageDef;
void CritterDamagePlayer(Player *player, Critter *c,
                         struct CritterDamageDef *damageDef, u32 flags,
                         f32 *direction, s32 playSfx, f32 scale);
void CritterSetFxHitTime(s32 slot, s32 id, f32 amount);
s32  CritterGetTarget(Critter *c, f32 *out);
s32  CritterGetTargetSub(Critter *c, f32 *target, s32 mode);
f32  CritterReCalcTarget(Critter *c, f32 *moveTarget, s32 target);
void CritterGetSingleTargetPlayer(Critter *c);
void CritterResolveMultipleTargets(Critter *c);
void CritterGetTargetPlayers(Critter *c);
void CritterInsertTarget(Critter *c, CritterTargetInfo *target);
f32  CritterCalcTarget(Critter *c, f32 *moveTarget, f32 *target,
                       CritterTargetInfo *record);
void *CritterMoveNodeCol(f32 radius, f32 height, f32 *origin,
                         f32 *destination, f32 *contact, s32 ignore,
                         s32 mode);
s32  CritterMoveNodeColSub(Critter *c, f32 radius, f32 height,
                           f32 *offset, f32 *lineStart, f32 *contact,
                           s32 first);
s32  CritterExpNodeColSub(Critter *c, f32 radius, f32 squaredExpand,
                          f32 height, f32 *origin, f32 *destination,
                          f32 *contact, s32 mode);
Critter *CritterExpCollide(f32 *origin, f32 *forward, f32 radius,
                           f32 dot, f32 *contact, s32 timedId);
s32  CritterLineNodeColSub(Critter *c, f32 *origin, f32 *forward,
                           f32 *delta, f32 radius, f32 dotThreshold);
void CritterCollideStart(s32 unused, void *ctx);
s32  CritterNoHit(Critter *c, s32 id);
s32  CritterNoHitSub(Critter *c, s32 id);
void fn_80037ED0(f32 add, Critter *c, s32 id);
Critter *CritterLineCollide(f32 dotThresh, f32 limit, f32 *origin,
                            f32 *forward, f32 *out, f32 *score);
f32  CritterLineRootColSub(Critter *c, f32 *origin, f32 *forward, f32 *out,
                           f32 dotThresh, f32 limit);
s32 CritterDamage(f32 damage, Critter *c, s32 player, u32 flags,
                  f32 *hitPosition, f32 *direction, s32 source);
s32  ProcessCritter(Critter *c);
s32  ProcessCritterList(void);
void CritterDoKnockback(Critter *c);
void CritterUpdateCounters(Critter *c);
 s32 CritterGolemAI(Critter *c);
s32  CritterBossAI(Critter *c);
void CritterProcessSafeRocks(void);
void CritterDropItem(Critter *c);
s32  CritterTranslate(Critter *c, CritterMove *move);
void CritterRotate(Critter *c, CritterMove *move);
s32  CritterMoveSetup(Critter *c, CritterMove *move);
void CritterActivate(Critter *c, CritterMove *move, s32 frame);
void CritterGetNextMove(Critter *c);
void CritterLookForReady(Critter *c);
void CritterChildCriticalMove(Critter *c);
void CritterLookForCriticalMove(Critter *c);
void CritterChildGetPattern(Critter *c);
void CritterGetDoAction(Critter *c);
u32  CritterCopyAnim(Critter *c, CritterMove *move, s32 frame);
void CritterAnimate(Critter *c);
void CritterMoveDone(Critter *c, s32 moveIndex);
extern s32 lbl_8034489C;
extern s32 lbl_80344628;
extern f64 lbl_80346608;
extern f32 lbl_80346470;
s32  CritterGetDmove(CritterMove *a, CritterMove *b);
s32  CritterFindMoveType(Critter *c, s32 type, s32 mode);
void CritterAnimInterrupt(Critter *c, s32 action, s32 phase, s32 active);
s32 CritterDoTexmodNode(Critter *c, s32 action, s32 local,
                         f32 *position);
s32  CritterDoSfx(Critter *c, s32 sfx, void *parent, s32 arg3, s32 arg4);
s32  CritterDoSfxSub(Critter *c, u8 *sfx, f32 *position,
                     s32 parented, u32 flags);
void CritterDoParticle(Critter *c, void *sfx, s32 node);
void DmgFxNodeUpdate(void *node, s32 absolute, f32 rx, f32 rz, f32 rotp, f32 roty);
Critter *CritterNewInst(s32 type, s32 subtype, void *object);
void CritterInitGeo(Critter *c, void *object, s32 subtype);
void CritterAddHealthMeter(Critter *c);
void CritterInitInst(Critter *c, struct CritterHeader *hdr);
Critter *CritterEmptyInst(void);
void CritterDelInst(Critter *c);
void CritterUpdateSkinfx(Critter *c);
struct CritterColnode;
void CritterRemoveColnodeSub(Critter *c, struct CritterColnode *node, s32 mode);
void CritterInitColnodes(Critter *c);
void CritterAddAnimInsts(Critter *c, f32 *matrix);
s32  CritterLoadFile(const char *wad, const char *name);
/* Background type-file loader callbacks. */
s32 CritterLoadDone(s32 maxBytes);
void CritterBGLoadFile(s32 *loader);
s32 CritterLoadStartNext(void);
void CritterLoadAllTypes(s32 arg);
struct CritterHeader *CritterTypeLoaded(s32 type, s32 subtype);
void CritterAllocType(void *hdr, void *move, s32 arg);
void CritterLoadFinish(CritterPackedType *header);
void CritterInitAllMoves(void);
void CritterInitMoves(CritterPackedType *header);
void CritterInitSfx(void *file, s32 index, void *atreeHeader);
void CritterInitHeader(void *hdr, void *file);

/* ==================================================================== */

/* 0x80034CFC -- stop or deflect this frame's translation when it overlaps
 * a live swarm enemy. */
s32 CritterCollideEnemies(c, delta)
Critter *c;
f32 *delta;
{
    u8 unusedHigh[16];
    f32 contact[3];
    u8 unusedMid[4];
    f32 center[3];
    f32 bestContact[3];
    u8 unusedLow[8];
    Enemy *enemy;
    s32 bestIndex;
    f32 *cpos;
    f32 best;
    f32 radius;
    f32 height;
    f32 combinedRadius;
    f32 combinedHeight;
    f32 distance;
    s32 index;
    s32 hit;

    cpos = c->pos;
    best = 0.0f;
    radius = c->hdr->wallRadius;
    height = c->hdr->radius;
    center[0] = cpos[0] + delta[0];
    bestIndex = -1;
    center[1] = cpos[1] + delta[1];
    center[2] = cpos[2] + delta[2];
    StartItemGrid(radius, center);
    while ((index = NextGridItem()) >= 0) {
        enemy = &gEnemies[index];
        if (enemy->state != 1 &&
            enemy->state != 6 &&
            (enemy->state != 8 || lbl_803447DC == 0)) {
            continue;
        }
        if (enemy->type == 31) {
            continue;
        }
        combinedRadius = radius + enemy->rad;
        combinedHeight = height + enemy->hht;
        if ((c->hdr->typeFlags & 0x100) != 0) {
            hit = CritterMoveNodeColSub(c, combinedRadius, combinedHeight, delta,
                    enemy->objgrp.coll_pos, contact, 1);
        } else {
            contact[0] = enemy->objgrp.coll_pos[0] - center[0];
            contact[1] = enemy->objgrp.coll_pos[1] - center[1];
            contact[2] = enemy->objgrp.coll_pos[2] - center[2];
            if (contact[0] * contact[0] + contact[2] * contact[2] >
                combinedRadius * combinedRadius) {
                continue;
            }
            hit = LineCylinderCollide(enemy->objgrp.coll_pos, combinedRadius,
                    combinedHeight, cpos, center,
                    contact, 1);
        }
        if (hit == 0) {
            continue;
        }
        distance = fqdist(contact[0] - center[0],
                          contact[2] - center[2]);
        if (bestIndex < 0 || distance < best) {
            best = distance;
            bestIndex = index;
            bestContact[0] = contact[0];
            bestContact[1] = contact[1];
            bestContact[2] = contact[2];
        }
    }
    if (bestIndex >= 0) {
        if (*(s16 *)((u8 *)*(CritterDescriptor **)((u8 *)c->hdr +
                     offsetof(CritterPackedType, descriptor)) +
                     offsetof(CritterDescriptor, type)) == 3) {
            enemy = &gEnemies[bestIndex];
            if ((f64)enemy->hht <= 2.0) {
                damage_enemy(enemy, -1, 0,
                             c->hdr->damageScale *
                             gCurLevel->ene_damage,
                             bestContact, NULL, 1);
                return 0;
            }
        }
        distance = 0.0f;
        delta[2] = distance;
        delta[0] = distance;
        return 1;
    }
    return 0;
}

/* 0x80034F60 -- stop translation against collidable item records returned by
 * the item grid. */
s32 CritterCollideItems(Critter *c, f32 *delta, s32 hits)
{
    /* lint-allow-next-line FM003: measured frame slot -- deleting it moves CritterCollideItems by 7 words at unchanged size; original local unrecovered */
    u8 unusedHigh[12];
    f32 center[3];
    f32 out[3];
    /* lint-allow-next-line FM003: measured frame slot -- deleting it moves CritterCollideItems by 21 words at unchanged size; original local unrecovered */
    u8 unusedLow[4];
    s32 hit;
    s32 j;
    CritterHitNode *node;
    CritterColDescriptor *desc;
    f32 result;
    f32 radius;
    f32 height;
    f32 damage;
    s32 index;
    u8 *item;
    s32 type;
    f32 *cpos;

    cpos = c->pos;
    radius = c->hdr->wallRadius;
    height = c->hdr->radius;
    result = -1.0f;
    center[0] = cpos[0] + delta[0];
    center[1] = cpos[1] + delta[1];
    center[2] = cpos[2] + delta[2];
    StartEnemyGrid(center, radius);
    while ((index = NextGridEnemy()) >= 0) {
        item = sItems + index * 0xF0;
        type = fn_8005D5C8(c, item);
        if (type == 0) {
            continue;
        }
        if ((c->hdr->typeFlags & 0x100) != 0) {
            for (j = 0; j < c->hdr->colCount; j++) {
                f32 *npos;
                node = &c->hitnodes[j];
                if (node->active == NULL) {
                    continue;
                }
                if (node->activeFrom >= node->activeUntil) {
                    continue;
                }
                desc = node->descriptor;
                if ((desc->flags & 8) == 0) {
                    continue;
                }
                npos = node->position;
                center[0] = npos[0] + delta[0];
                center[1] = npos[1] + delta[1];
                center[2] = npos[2] + delta[2];
                desc = node->descriptor;
                result = fn_8005F0F4(
                    item, npos,
                    center, out, desc->radius, desc->radius);
                if (result >= 0.0) {
                    break;
                }
            }
        } else {
            center[0] = cpos[0] + delta[0];
            center[1] = cpos[1] + delta[1];
            center[2] = cpos[2] + delta[2];
            result = fn_8005F0F4(item, cpos, center, out, radius, height);
        }
        hit = 0;
        if (result >= 0.0) {
            if (type != 2) {
                if (type == 3) {
                    damage = c->hdr->damageScale *
                             gCurLevel->ene_damage;
                    if (fn_8005C1DC(item, 0, -1, c->hdr, damage) != 0.0f) {
                        hit = 1;
                    }
                } else {
                    hit = 1;
                }
            }
        }
        if (hit) {
            delta[2] = 0.0f;
            delta[0] = 0.0f;
        }
    }
    return 0;
}

/* 0x800351B0 -- separate active players from a translating critter and feed
 * the displacement into their push vectors. */
s32 CritterCollidePlayers(Critter *c, f32 *delta, s32 hits)
{
    Player *player;
    f32 dest[3];
    f32 contact[3];
    f32 sep[3];
    f32 radiusX;
    f32 radiusZ;
    f32 combined;
    f32 combinedZ;
    f32 length;
    f64 penetration;
    f32 scale;
    f64 maxPen;
    f64 minPen;
    f64 pushScale;
    s32 result;
    s32 count;
    s32 i;

    radiusX = c->hdr->wallRadius;
    radiusZ = c->hdr->radius;
    dest[0] = c->pos[0] + delta[0];
    dest[1] = c->pos[1] + delta[1];
    dest[2] = c->pos[2] + delta[2];
    result = 0;
    count = 0;
    maxPen = 3.0;
    minPen = 1.0;
    pushScale = 2.0;
    for (i = 0; i < 4; i++) {
        player = &gPlayers[i];
        if (player->state != 1 && player->state != 4) {
            continue;
        }
        if ((*(s16 *)((u8 *)player + offsetof(Player, hud_flags)) & 0x20) != 0) {
            continue;
        }
        combined = radiusX + *(f32 *)((u8 *)player + offsetof(Player, col_radius));
        combinedZ = radiusZ + *(f32 *)((u8 *)player + offsetof(Player, col_height));
        if ((c->hdr->typeFlags & 0x100) != 0) {
            result = CritterMoveNodeColSub(
                c, *(f32 *)((u8 *)player + offsetof(Player, col_radius)),
                *(f32 *)((u8 *)player + offsetof(Player, col_height)), delta,
                (f32 *)((u8 *)player + offsetof(Player, effectpos)), contact, 0);
            if (result != 0) {
                sep[0] = *(f32 *)((u8 *)player + offsetof(Player, effectpos)) -
                         c->hitnodes[result - 1].position[0];
                sep[1] = *(f32 *)((u8 *)player + offsetof(Player, effectpos) + 4) -
                         c->hitnodes[result - 1].position[1];
                sep[2] = *(f32 *)((u8 *)player + offsetof(Player, effectpos) + 8) -
                         c->hitnodes[result - 1].position[2];
            }
        } else {
            result = LineCylinderCollide(
                (f32 *)((u8 *)player + offsetof(Player, effectpos)), combined,
                combinedZ, &c->pos[0], dest, contact, 1);
            if (result != 0) {
                sep[0] = *(f32 *)((u8 *)player + offsetof(Player, effectpos)) - dest[0];
                sep[1] = *(f32 *)((u8 *)player + offsetof(Player, effectpos) + 4) - dest[1];
                sep[2] = *(f32 *)((u8 *)player + offsetof(Player, effectpos) + 8) - dest[2];
            }
        }
        if (result != 0) {
            count++;
            length = NormalVector(sep);
            penetration = combined - length;
            if (penetration < minPen) {
                penetration = minPen;
            } else if (penetration > maxPen) {
                penetration = maxPen;
            }
            scale = (f32)penetration;
            sep[0] = sep[0] * scale;
            sep[1] = sep[1] * scale;
            sep[2] = sep[2] * scale;
            *(f32 *)((u8 *)player + offsetof(Player, vel[0])) =
                (f32)(pushScale * sep[0] + *(f32 *)((u8 *)player + offsetof(Player, vel[0])));
            *(f32 *)((u8 *)player + offsetof(Player, vel[1])) =
                (f32)(pushScale * sep[1] + *(f32 *)((u8 *)player + offsetof(Player, vel[1])));
            *(f32 *)((u8 *)player + offsetof(Player, vel[2])) =
                (f32)(pushScale * sep[2] + *(f32 *)((u8 *)player + offsetof(Player, vel[2])));
        }
    }
    if (result != 0) {
        delta[2] = 0.0f;
        delta[0] = 0.0f;
    }
    return count;
}

/* 0x80035408 -- integrate the world-contact portion of a movement delta and
 * cache a floor point/status for item drops and shadows. */
u32 CritterCollideWorld(c, delta)
Critter *c;
f32 *delta;
{
    f32 probe[3];
    f32 direction[3];
    u8 unusedMid[12];
    f32 floorResult[15];
    f32 contact[3];
    u8 unusedLow[4];
    f32 *cpos;
    f32 *from;
    f32 wallRadius;
    f32 radius;
    f64 bottom;
    f32 baseY;
    f32 floorY;
    f32 minRise;
    f32 reachLimit;
    f32 reach;
    f32 length;
    f32 difference;
    s32 offset;
    s32 i;
    u32 result;
    s32 grounded;
    void *wallSurface;
    void *surface;

    minRise = (f32)(-16.0 * (f64)gClockFrameStep);
    cpos = c->pos;
    from = NULL;
    wallRadius = c->hdr->wallRadius;
    radius = c->hdr->radius;
    wallSurface = NULL;
    if ((c->hdr->typeFlags & 0x100) != 0) {
        /* The byte-offset induction is load-bearing here: `&c->hitnodes[i]`
         * with the `offset` accumulator removed rebuilds this function at the
         * same 1192 bytes with 55 differing words against the banked object,
         * so the cursor form below is retained and the cast is scoped. */
        for (i = 0, offset = 0; i < c->hdr->colCount;
             i++, offset += sizeof(CritterHitNode)) {
            CritterHitNode *hitNode =
                (CritterHitNode *)((u8 *)c->hitnodes + offset);
            if (hitNode->active == NULL) {
                continue;
            }
            if (hitNode->activeFrom >= hitNode->activeUntil) {
                continue;
            }
            if ((hitNode->descriptor->flags & 8) == 0) {
                continue;
            }
            from = hitNode->position;
            probe[0] = from[0] + delta[0];
            probe[1] = from[1] + delta[1];
            probe[2] = from[2] + delta[2];
            wallSurface = EnemyWallCollide(
                hitNode->descriptor->radius, from, probe,
                contact);
            if (wallSurface != NULL) {
                break;
            }
        }
    } else {
        probe[0] = cpos[0] + delta[0];
        probe[1] = cpos[1] + delta[1];
        probe[2] = cpos[2] + delta[2];
        from = cpos;
        wallSurface = EnemyWallCollide(wallRadius, from, probe, contact);
    }

    if (wallSurface != NULL) {
        CritterWorldDamage(c, wallSurface, cpos, contact);
        if (((u32)((WorldObj *)wallSurface)->flags & 0x38) != 0) {
            result = 0;
        } else if (SlideAlongWall(wallRadius, from, delta, contact,
                                  lbl_8023CA98 + 4) < 0) {
            delta[0] = delta[2] = 0.0f;
            result = 2;
        } else {
            result = 0;
        }
    } else {
        result = 0;
    }

    direction[0] = delta[0];
    direction[1] = delta[1];
    direction[2] = delta[2];
    length = NormalVector(direction);
    reach = wallRadius + length;
    probe[0] = cpos[0] + direction[0] * reach;
    probe[1] = cpos[1] + direction[1] * reach;
    probe[2] = cpos[2] + direction[2] * reach;
    reachLimit = (f32)(2.0 * (f64)reach);
    bottom = -(f64)radius - 3.0;
    if ((surface = FloorCollide(probe, (s32)floorResult, 0, 2,
                                1.0f, radius, bottom)) != NULL) {
        grounded = 1;
        CritterWorldDamage(c, surface, cpos, floorResult + 12);
        baseY = c->vel[1] - c->hdr->floorOffset;
        c->floorContact[0] = floorResult[12];
        c->floorContact[1] = floorResult[13];
        c->floorContact[2] = floorResult[14];
        floorY = floorResult[13];
        difference = floorY - baseY;
        if (difference < 0.0f) {
            difference = -difference;
        }
        if (difference > reachLimit) {
            grounded = 0;
        } else if ((f64)length > 0.0 &&
                   (f64)difference > 0.1 * (f64)length) {
            probe[0] = cpos[0] + delta[0];
            probe[1] = cpos[1] + delta[1];
            probe[2] = cpos[2] + delta[2];
            surface = FloorCollide(probe, (s32)floorResult, 0, 2,
                                   1.0f, radius, bottom);
            if (surface == NULL) {
                grounded = 0;
            } else {
                c->floorContact[0] = floorResult[12];
                c->floorContact[1] = floorResult[13];
                c->floorContact[2] = floorResult[14];
                floorY = floorResult[13];
            }
        }
        if (grounded == 0) {
            surface = FloorCollide(cpos, (s32)floorResult, 0, 2,
                                   1.0f, radius, bottom);
            if (surface == NULL) {
                floorY = baseY;
            } else {
                floorY = floorResult[13];
                c->floorContact[0] = floorResult[12];
                c->floorContact[1] = floorResult[13];
                c->floorContact[2] = floorResult[14];
            }
        }
        difference = floorY - baseY;
        if (difference < minRise) {
            difference = minRise;
        }
        delta[1] += difference;
    } else {
        grounded = 0;
    }
    if (grounded == 0) {
        result |= 0x10;
        delta[0] = delta[2] = 0.0f;
    }
    *(u32 *)((u8 *)c + 0x448) = result;
    if (surface != NULL) {
        if (*(void **)((u8 *)surface + offsetof(WorldObj, nodeptr)) != NULL &&
            ((u32)((WorldObj *)surface)->flags & 0x1000) !=
                0) {
            MBNodeSetParent(c->mbnode,
                            *(void **)((u8 *)surface +
                                       offsetof(WorldObj, nodeptr)));
        } else {
            MBNodeSetParent(c->mbnode, lbl_8034473C);
        }
        if (c->shadow != NULL) {
            CopyMat3(floorResult, (f32 *)c->shadow);
            ((MBObject *)c->shadow)->mat[3][0] = c->vel[0];
            ((MBObject *)c->shadow)->mat[3][1] = c->vel[1];
            ((MBObject *)c->shadow)->mat[3][2] = c->vel[2];
            ((MBObject *)c->shadow)->mat[3][1] =
                (f32)(0.1 + (f64)floorResult[13]);
        }
    }
    return result;
}

/* 0x800358B0 -- translate collision material flags into a critter damage
 * class and direction. */
void CritterWorldDamage(Critter *c, void *surface, f32 *origin,
                        f32 *contact)
{
    u32 allFlags;
    u32 material;
    u32 flags;
    f32 direction[3];
    f32 damage;

    flags = 0;
    damage = lbl_80346470;
    if (((allFlags = WorldObjGetAllFlags(surface)) & 0xF0000) == 0) {
        return;
    }
    if ((allFlags & 0x02000000) != 0 &&
        (allFlags & 0x08000000) == 0) {
        return;
    }
    direction[0] = origin[0] - contact[0];
    direction[1] = lbl_80346470;
    direction[2] = origin[2] - contact[2];
    NormalVector2D(direction);
    material = allFlags & 0xF0000;
    switch (material) {
    case 0x10000:
        damage = 5.0f;
        break;
    case 0x20000:
        damage = 5.0f;
        flags = 0x10;
        break;
    case 0x30000:
    case 0x40000:
    case 0x50000:
        damage = 15.0f;
        flags = 0x20;
        break;
    case 0x60000:
        break;
    }
    if (damage > lbl_80346488) {
        CritterDamage(damage, c, -1, flags, contact, direction, 1);
    }
}

/* 0x800359F0 -- damage swarm enemies intersecting an active critter node. */
s32 CritterNodeEnemyCollide(Critter *c, void *damageDef)
{
    /* lint-allow-next-line FM003: measured frame slot -- deleting it moves CritterNodeEnemyCollide by 7 words at unchanged size; original local unrecovered */
    u8 unusedHigh[8];
    u8 *dmg = (u8 *)damageDef;
    f32 pos[3];
    /* lint-allow-next-line FM003: measured frame slot -- deleting it moves CritterNodeEnemyCollide by 15 words at unchanged size; original local unrecovered */
    u8 unusedMid[20];
    f32 out[3];
    /* lint-allow-next-line FM003: measured frame slot -- deleting it moves CritterNodeEnemyCollide by 21 words at unchanged size; original local unrecovered */
    u8 unusedLow[4];
    f32 delta[3];
    f64 zero;
    f32 bx;
    f32 by;
    f32 bz;
    f32 radius;
    f32 f26v;
    f64 k;
    s32 count;
    s32 idx;
    Enemy *e;

    radius = ((CritterDamageDef *)dmg)->damage * gCurLevel->ene_damage;
    f26v = ((CritterDamageDef *)dmg)->maxDistance;
    count = 0;
    MulVecMat3(((CritterDamageDef *)dmg)->offset, out, c->worldMoveMatrix);
    bx = c->moveMatrix[0] + out[0];
    by = c->moveMatrix[1] + out[1];
    bz = c->moveMatrix[2] + out[2];
    pos[0] = c->moveOrigin[0] + out[0];
    pos[1] = c->moveOrigin[1] + out[1];
    pos[2] = c->moveOrigin[2] + out[2];
    StartItemGrid(f26v, pos);
    k = lbl_80346478;
    zero = lbl_80346488;
    while ((idx = NextGridItem()) >= 0) {
        s32 state;
        e = &gEnemies[idx];
        state = e->state;
        if (state != 1 && state != 6 && (state != 8 || lbl_803447DC == 0)) {
            continue;
        }
        if (e->type == 31) {
            continue;
        }
        if (radius > zero && sMusicFadeBase < e->fxhittime[0]) {
            continue;
        }
        if (LineCylinderCollide(e->objgrp.coll_pos, e->rad + f26v,
                                e->hht + f26v, pos, pos, out, 0)) {
            delta[0] = pos[0] - bx;
            delta[1] = pos[1] - by;
            delta[2] = pos[2] - bz;
            delta[0] = (f32)(k * delta[0]);
            delta[1] = (f32)(k * delta[1]);
            delta[2] = (f32)(k * delta[2]);
            damage_enemy(e, -1, 0, radius, out, delta, 1);
            count++;
        }
    }
    return count;
}

/* 0x80035BC8 -- choose an available safe rock, or the available rock nearest
 * a requested player. */
s32 SafeRockNearestTarget(s32 player)
{
    f32 matrix[16];
    /* lint-allow-next-line FM003: measured frame slot -- deleting it moves SafeRockNearestTarget by 10 words at unchanged size; original local unrecovered */
    u8 unused[40];
    f32 playerX;
    f32 playerZ;
    f32 distance;
    f32 best;
    s32 i;
    s32 bestIndex;
    void *node;

    bestIndex = -1;
    best = 1e21f;
    if (player < 0) {
        s32 sum;
        for (i = 0; i < lbl_80344658; i++) {
            node = lbl_80241020[i];
            sum = *(volatile s32 *)&lbl_80344654 + i;
            sum++;
            bestIndex = sum % lbl_80344658;
            if (SafeRockActive(node) == 0) {
                return bestIndex;
            }
        }
        return -1;
    }
    playerX = gPlayers[player].pos[0];
    playerZ = gPlayers[player].pos[2];
    for (i = 0; i < lbl_80344658; i++) {
        if (SafeRockActive(lbl_80241020[i]) != 0) {
            continue;
        }
        node = ItemGetNode(lbl_80241020[i]);
        if (node == NULL) {
            continue;
        }
        GetWorldMat(node, matrix, NULL);
        distance = fqdist(matrix[12] - playerX, matrix[14] - playerZ);
        if (distance < best) {
            bestIndex = i;
            best = distance;
        }
    }
    return bestIndex;
}

/* 0x80035D08 -- aim the two optional look-at nodes at the selected player. */
void CritterLookAtPlayer(Critter *c, CritterMove *move)
{
    f32 target[3];
    struct CritterHeader *hdr;
    f32 *targetPtr;
    s32 look;

    look = 0;
    hdr = c->hdr;
    if (move != NULL && (move->type == 16 || move->type == 0)) {
        return;
    }
    if (move != NULL) {
        if (move->type == 17 || (move->flags & 1)) {
            look = 1;
        }
    }
    if (c->pausecnt > 0) {
        look = 1;
    }
    if (c->unkAC6 > 0) {
        look = 1;
        c->unkAC6 -= gFrameTicks;
    }
    if (look == 0 && c->unk124 >= 0) {
        Player *p = &gPlayers[c->unk124];
        target[0] = p->col_pos[0];
        target[1] = p->col_pos[1];
        target[2] = p->col_pos[2];
        targetPtr = target;
    } else {
        targetPtr = NULL;
    }
    if (c->hitnode0 != NULL) {
        NodeLookAtPos(c->hitnode0, targetPtr,
                      ((CritterPackedType *)hdr)->lookYawRate0, lbl_80346470,
                      &c->headyaw,
                      ((CritterPackedType *)hdr)->lookPitchRate0, ((CritterPackedType *)hdr)->lookPitchBias0,
                      &c->headpitch);
    }
    if (c->hitnode1 != NULL) {
        NodeLookAtPos(c->hitnode1, targetPtr,
                      ((CritterPackedType *)hdr)->lookYawRate1, lbl_80346470,
                      &c->eyeyaw,
                      ((CritterPackedType *)hdr)->lookPitchRate1, ((CritterPackedType *)hdr)->lookPitchBias1,
                      &c->eyepitch);
    }
}

/* 0x80035E48 -- smoothly yaw and pitch a scene node toward a world point. */
void NodeLookAtPos(void *node, f32 *target, f32 a, f32 b, f32 *yaw, f32 c,
                   f32 d, f32 *pitch)
{
    union { f64 _align; f32 v[3]; } pyrU;
    f32 delta[3];
    f32 nodeYaw;
    f32 nodePitch;
    f32 yawv;
    f32 pitchv;
    f32 matrix[16];
    f32 dd;
    f64 nd;
    f32 r;
    f32 step;
#define pyr (pyrU.v)

    if (target != NULL) {
        GetWorldMat(node, matrix, NULL);
        GetYawPitch(&matrix[8], &nodeYaw, &nodePitch);
        delta[0] = target[0] - matrix[12];
        delta[1] = target[1] - matrix[13];
        delta[2] = target[2] - matrix[14];
        GetYawPitch(delta, &yawv, &pitchv);
        yawv = yawv - nodeYaw;
        pitchv = pitchv - nodePitch;
        yawv = yawv + b;
        pitchv = pitchv + d;
    } else {
        yawv = lbl_80346470;
        pitchv = lbl_80346470;
    }

    {
        dd = yawv - *yaw;
        if (dd > 3.141592654) {
            nd = dd - 6.283185308;
        } else if (dd <= -3.141592654) {
            nd = 6.283185308 + dd;
        } else {
            nd = dd;
        }
        r = (f32)nd;
        step = (f32)(1.570796327 * gClockFrameStep);
        if (r > step) {
            r = step;
        }
        if (r < -step) {
            r = -step;
        }
        yawv = *yaw + r;
        *yaw = yawv;
    }

    {
        f64 nd;
        f32 r;
        f32 step;
        f32 dd = pitchv - *pitch;
        if (dd > 3.141592654) {
            nd = dd - 6.283185308;
        } else if (dd <= -3.141592654) {
            nd = 6.283185308 + dd;
        } else {
            nd = dd;
        }
        r = (f32)nd;
        step = (f32)(1.570796327 * gClockFrameStep);
        if (r > step) {
            r = step;
        }
        if (r < -step) {
            r = -step;
        }
        pitchv = *pitch + r;
        *pitch = pitchv;
    }

    ExtractPYR(node, pyr);
    {
        f32 *pyrYaw = &pyr[1];
        nd = yawv - *pyrYaw;
        if (nd > 3.141592654) {
            nd = nd - 6.283185308;
        } else if (nd <= -3.141592654) {
            nd = 6.283185308 + nd;
        }
        r = (f32)nd;
        if (r > a) {
            r = a;
        }
        if (r < -a) {
            r = -a;
        }
        *pyrYaw += r;
    }

    {
        f32 *pyrPitch = &pyr[0];
        nd = pitchv - *pyrPitch;
        if (nd > 3.141592654) {
            nd = nd - 6.283185308;
        } else if (nd <= -3.141592654) {
            nd = 6.283185308 + nd;
        }
        r = (f32)nd;
        if (r > c) {
            r = c;
        }
        if (r < -c) {
            r = -c;
        }
        *pyrPitch += r;
    }
    CreatePYRMatrix(node, pyr);
#undef pyr
}

/* Scale the critter's animation rate by its remaining health and the level's
   enemy-health multiplier.  Inlined into CritterGolemAI and CritterBossAI. */
static void CritterSetDifficulty(Critter *c)
{
    f32 ratio;
    f32 speed;
    f32 rateBase;
    f32 rateScale;

    rateBase = 0.5f;
    rateScale = 4.5f;
    ratio = c->health /
            (1.0 + c->hdr->maxHealth *
                       gCurLevel->ene_health);
    speed = 1.0 - ratio;
    speed = rateBase + speed * rateScale;
    c->invRateScale = 1.0 / speed;
    c->rateScale = speed;
}

extern void  PlayerSetParent(Player *p, void *node, f32 *offset);
extern void  PlayerUnsetParent(Player *p);

/* Detach the grabbed player and throw it along the critter's forward axis.
   Inlined into CritterAnimInterrupt's phase-2 grab release. */
static void CritterReleasePlayer(Critter *c, u8 *damageDef, f32 *dir, s32 held)
{
    Player *pp;

    pp = &gPlayers[held];
    PlayerUnsetParent(pp);
    dir[0] = ((MBObject *)c->mbnode)->mat[2][0];
    dir[1] = ((MBObject *)c->mbnode)->mat[2][1];
    dir[2] = ((MBObject *)c->mbnode)->mat[2][2];
    dir[1] = -0.1f;
    NormalVector(dir);
    dir[0] = dir[0] * ((CritterDamageDef *)damageDef)->minSpeed;
    dir[1] = dir[1] * ((CritterDamageDef *)damageDef)->minSpeed;
    dir[2] = dir[2] * ((CritterDamageDef *)damageDef)->minSpeed;
    CritterDamagePlayer(pp, c, (CritterDamageDef *)damageDef, 0x8050, dir, 0,
                        0.5f);
    c->unk128 = -1;
}

static inline void CritterDamagePlayerInline(Player *player, Critter *c,
                                              u8 *damageDef, u32 flags,
                                              f32 *direction, s32 playSfx,
                                              f64 damageGate, f64 damageScale,
                                              f32 zero, f64 hitTimeBase)
{
    u32 damageFlags;
    s32 playerIndex;
    f32 damage;
    CritterDescriptor *descriptor;
    u8 *counter;
    u8 *hit;

    damageFlags = ((CritterDamageDef *)damageDef)->flags | flags;
    playerIndex = player->index;
    damage = ((CritterDamageDef *)damageDef)->damage *
             gCurLevel->ene_damage;
    if (playSfx != 0 && ((CritterDamageDef *)damageDef)->sfx >= 0) {
        CritterDoSfx(c, ((CritterDamageDef *)damageDef)->sfx, &player->pos[0], 0, -1);
        damageFlags |= 0x01000000;
    }
    descriptor = c->hdr->descriptor;
    if (descriptor->type != 4 &&
        (f64)lbl_803447D8 < damageGate) {
        damage = (f32)((f64)damage * damageScale);
    }
    damage_player(playerIndex, damage, 1, damageFlags, direction);
    hit = (u8 *)gPlayers + playerIndex * sizeof(Player);
    ((Player *)hit)->bossdamage = zero;
    ((Player *)hit)->fxhittime =
        (f32)(hitTimeBase + (f64)sMusicFadeBase);
    counter = (u8 *)c + playerIndex * 0x10;
    ((Critter *)counter)->playerDamage[0].received += damage;
    *(f32 *)(counter + (offsetof(Critter, playerDamage[0].receivedTime))) = sMusicFadeBase;
}

static inline void CritterDamagePlayerInlineNode(Player *player, Critter *c,
                                                  u8 *damageDef, u32 flags,
                                                  f32 *direction, s32 playSfx,
                                                  f64 damageGate,
                                                  f64 damageScale, f32 zero,
                                                  f64 hitTimeBase,
                                                  f32 *damage)
{
    u32 damageFlags;
    s32 playerIndex;
    CritterDescriptor *descriptor;
    u8 *counter;
    u8 *hit;

    damageFlags = ((CritterDamageDef *)damageDef)->flags | flags;
    playerIndex = player->index;
    *damage = ((CritterDamageDef *)damageDef)->damage *
              gCurLevel->ene_damage;
    if (playSfx != 0 && ((CritterDamageDef *)damageDef)->sfx >= 0) {
        CritterDoSfx(c, ((CritterDamageDef *)damageDef)->sfx, &player->pos[0], 0, -1);
        damageFlags |= 0x01000000;
    }
    descriptor = c->hdr->descriptor;
    if (descriptor->type != 4 &&
        (f64)lbl_803447D8 < damageGate) {
        *damage = (f32)((f64)*damage * damageScale);
    }
    damage_player(playerIndex, *damage, 1, damageFlags, direction);
    hit = (u8 *)gPlayers + playerIndex * sizeof(Player);
    ((Player *)hit)->bossdamage = zero;
    ((Player *)hit)->fxhittime =
        (f32)(hitTimeBase + (f64)sMusicFadeBase);
    counter = (u8 *)c + playerIndex * 0x10;
    ((Critter *)counter)->playerDamage[0].received += *damage;
    *(f32 *)(counter + (offsetof(Critter, playerDamage[0].receivedTime))) = sMusicFadeBase;
}

/* 0x80036138 -- test the critter's forward fire segment against players. */
void CritterFirePlayerCollide(Critter *c, struct CritterDamageDef *damage)
{
    u8 *dmg = (u8 *)damage;
    /* lint-allow-next-line FM003: measured frame slot -- deleting it moves CritterFirePlayerCollide by 7 words at unchanged size; original local unrecovered */
    u8 framePad[8];
    f32 start[3];
    /* lint-allow-next-line FM003: measured frame slot -- deleting it moves CritterFirePlayerCollide by 14 words at unchanged size; original local unrecovered */
    u8 startPad[4];
    f32 end[3];
    /* lint-allow-next-line FM003: measured frame slot -- deleting it moves CritterFirePlayerCollide by 21 words at unchanged size; original local unrecovered */
    u8 endPad[4];
    f32 delta[3];
    /* lint-allow-next-line FM003: measured frame slot -- deleting it moves CritterFirePlayerCollide by 40 words at unchanged size; original local unrecovered */
    u8 deltaPad[4];
    f32 transformed[3];
    /* lint-allow-next-line FM003: measured frame slot -- deleting it moves CritterFirePlayerCollide by 45 words at unchanged size; original local unrecovered */
    u8 transformedPad[4];
    f32 playerPos[3];
    /* lint-allow-next-line FM003: measured frame slot -- deleting it moves CritterFirePlayerCollide by 60 words at unchanged size; original local unrecovered */
    u8 unused[16];
    Player *player;
    f32 maxDistance;
    f32 radius;
    f32 minDistance;
    f32 distance;
    f32 halfForCollision;
    f64 hitTimeBase;
    f32 zeroForDamage;
    f64 damageScale;
    f64 damageGate;
    s32 i;

    maxDistance = ((CritterDamageDef *)dmg)->maxDistance;
    radius = ((CritterDamageDef *)dmg)->radius;
    minDistance = ((CritterDamageDef *)dmg)->minDistance;
    delta[0] = c->worldMoveMatrix[8];
    delta[1] = c->worldMoveMatrix[9];
    delta[2] = c->worldMoveMatrix[10];
    NormalVector(delta);
    YawVec3(delta, delta, ((CritterDamageDef *)dmg)->yaw);
    PitchVec3(delta, delta, ((CritterDamageDef *)dmg)->pitch);
    MulVecMat3(((CritterDamageDef *)dmg)->offset, transformed,
               c->worldMoveMatrix);
    start[0] = c->moveOrigin[0] + transformed[0];
    start[1] = c->moveOrigin[1] + transformed[1];
    start[2] = c->moveOrigin[2] + transformed[2];
    end[0] = delta[0] * maxDistance + start[0];
    end[1] = delta[1] * maxDistance + start[1];
    end[2] = delta[2] * maxDistance + start[2];
    damageGate = *(volatile f64 *)&lbl_80346490;
    damageScale = *(volatile f64 *)&lbl_803464F8;
    halfForCollision = *(volatile f32 *)&lbl_803464E8;
    zeroForDamage = *(volatile f32 *)&lbl_80346470;
    hitTimeBase = *(volatile f64 *)&lbl_80346500;

    for (i = 0; i < 4; i++) {
        player = &gPlayers[i];
        if (player->state != 1 || sMusicFadeBase < player->fxhittime) {
            continue;
        }
        playerPos[0] = player->effectpos[0];
        playerPos[1] = player->effectpos[1];
        playerPos[2] = player->effectpos[2];
        delta[0] = playerPos[0] - start[0];
        delta[1] = playerPos[1] - start[1];
        delta[2] = playerPos[2] - start[2];
        distance = fqdist(delta[0], delta[2]);
        if (distance < minDistance || distance > maxDistance) {
            continue;
        }
        if (!LineCylinderCollide(playerPos,
                                 player->col_radius + radius,
                                 player->col_height + radius,
                                 start, end, transformed, 0)) {
            continue;
        }
        if (fn_8005FB48(halfForCollision, start, playerPos, playerPos, 1) >= 0) {
            continue;
        }
        delta[0] = end[0] - start[0];
        delta[1] = end[1] - start[1];
        delta[2] = end[2] - start[2];
        NormalVector2D(delta);
        CritterDamagePlayerInline(player, c, dmg, 0, delta, 1, damageGate,
                                  damageScale, zeroForDamage, hitTimeBase);
    }
}

/* 0x80036424 -- test an expanded critter node/body volume against players. */
s32 CritterNodePlayerCollide(Critter *c, struct CritterDamageDef *damage,
                              s32 enabled)
{
    u8 *dmg = (u8 *)damage;
    /* lint-allow-next-line FM003: measured frame slot -- deleting it moves CritterNodePlayerCollide by 7 words at unchanged size; original local unrecovered */
    u8 framePad[16];
    f32 start[3];
    /* lint-allow-next-line FM003: measured frame slot -- deleting it moves CritterNodePlayerCollide by 14 words at unchanged size; original local unrecovered */
    u8 startGap[20];
    f32 transformed[3];
    /* lint-allow-next-line FM003: measured frame slot -- deleting it moves CritterNodePlayerCollide by 22 words at unchanged size; original local unrecovered */
    u8 transformedPad[4];
    f32 deltaFromNode[3];
    /* lint-allow-next-line FM003: measured frame slot -- deleting it moves CritterNodePlayerCollide by 29 words at unchanged size; original local unrecovered */
    u8 nodePad[4];
    f32 deltaFromCritter[3];
    /* lint-allow-next-line FM003: measured frame slot -- deleting it moves CritterNodePlayerCollide by 37 words at unchanged size; original local unrecovered */
    u8 critterPad[4];
    f32 playerPos[3];
    /* lint-allow-next-line FM003: measured frame slot -- deleting it moves CritterNodePlayerCollide by 51 words at unchanged size; original local unrecovered */
    u8 unused[12];
    Player *player;
    f64 hitTimeBase;
    f32 zeroDamage;
    f64 damageScale;
    f64 damageGate;
    f64 half;
    f32 oneForY;
    f64 zeroRadius;
    f32 playerDamage;
    f32 distance;
    f32 nodeX;
    f32 nodeY;
    f32 nodeZ;
    f32 radius;
    f32 expansion;
    f32 bestDistance;
    s32 i;
    s32 bestPlayer;

    radius = (f32)(enabled != 0
                       ? (f64)(((CritterDamageDef *)dmg)->damage *
                               gCurLevel->ene_damage)
                       : lbl_80346488);
    expansion = ((CritterDamageDef *)dmg)->maxDistance;
    bestDistance = lbl_80346508;
    bestPlayer = -1;
    MulVecMat3(((CritterDamageDef *)dmg)->offset, transformed,
               c->worldMoveMatrix);
    nodeX = c->moveMatrix[0] + transformed[0];
    nodeY = c->moveMatrix[1] + transformed[1];
    nodeZ = c->moveMatrix[2] + transformed[2];
    start[0] = c->moveOrigin[0] + transformed[0];
    start[1] = c->moveOrigin[1] + transformed[1];
    start[2] = c->moveOrigin[2] + transformed[2];
    half = *(volatile f64 *)&lbl_80346478;
    damageGate = *(volatile f64 *)&lbl_80346490;
    damageScale = *(volatile f64 *)&lbl_803464F8;
    oneForY = *(volatile f32 *)&lbl_803464A8;
    zeroRadius = *(volatile f64 *)&lbl_80346488;
    zeroDamage = *(volatile f32 *)&lbl_80346470;
    hitTimeBase = *(volatile f64 *)&lbl_80346500;

    for (i = 0; i < 4; i++) {
        player = &gPlayers[i];
        if (player->state != 1 || player_can_be_damaged(player) == 0) {
            continue;
        }
        if (radius > zeroRadius && sMusicFadeBase < player->fxhittime) {
            continue;
        }
        playerPos[0] = player->effectpos[0];
        playerPos[1] = player->effectpos[1];
        playerPos[2] = player->effectpos[2];
        if (!LineCylinderCollide(playerPos,
                                 player->col_radius + expansion,
                                 player->col_height + expansion,
                                 start, start, transformed, 0)) {
            continue;
        }
        deltaFromNode[0] = start[0] - nodeX;
        deltaFromNode[1] = start[1] - nodeY;
        deltaFromNode[2] = start[2] - nodeZ;
        deltaFromCritter[0] = playerPos[0] - c->pos[0];
        *(volatile f32 *)&deltaFromCritter[1] =
            playerPos[1] - c->pos[1];
        deltaFromCritter[2] = playerPos[2] - c->pos[2];
        deltaFromCritter[1] = oneForY;
        NormalVector(deltaFromNode);
        distance = NormalVector(deltaFromCritter);
        if (distance < bestDistance) {
            bestPlayer = i;
            bestDistance = distance;
        }
        if (radius > zeroRadius) {
            transformed[0] = deltaFromNode[0] + deltaFromCritter[0];
            transformed[1] = deltaFromNode[1] + deltaFromCritter[1];
            transformed[2] = deltaFromNode[2] + deltaFromCritter[2];
            transformed[0] = (f32)(half * (f64)transformed[0]);
            transformed[1] = (f32)(half * (f64)transformed[1]);
            transformed[2] = (f32)(half * (f64)transformed[2]);
            CritterDamagePlayerInlineNode(player, c, dmg, 0, transformed, 1,
                                          damageGate, damageScale, zeroDamage,
                                          hitTimeBase, &playerDamage);
        }
    }
    return bestPlayer;
}
/* 0x80036740 -- award experience to one player (who >= 0) or all four active
 * players (who < 0), by the integer part of `amount`. */
void CritterAwardExp(s32 who, f32 amount)
{
    s32 end;
    Player *player;

    if (who >= 0) {
        end = who + 1;
    } else {
        who = 0;
        end = 4;
    }
    player = &gPlayers[who];
    for (; who < end; who++, player++) {
        if (player->state == 1) {
            AddExp(who, (s32)amount, 0);
        }
    }
}

/* 0x800367CC -- apply one critter damage event to a player and update both
 * the player's feedback timers and the critter's per-player hit counters. */
void CritterDamagePlayer(Player *player, Critter *c,
                         CritterDamageDef *damageDef, u32 flags,
                         f32 *direction, s32 playSfx, f32 scale)
{
    u32 damageFlags;
    s32 playerIndex;
    f32 damage;
    CritterDescriptor *descriptor;

    damageFlags = damageDef->flags | flags;
    playerIndex = player->index;
    damage = damageDef->damage * gCurLevel->ene_damage;

    if (playSfx != 0 && damageDef->sfx >= 0) {
        CritterDoSfx(c, damageDef->sfx, &player->pos[0], 0, -1);
        damageFlags |= 0x01000000;
    }

    descriptor = c->hdr->descriptor;
    if (descriptor->type != 4 &&
        (f64)lbl_803447D8 < lbl_80346490) {
        damage = (f32)((f64)damage * lbl_803464F8);
    }

    damage_player(playerIndex, damage, 1, damageFlags, direction);

    {
        Player *hit;
        u8 *counter;
        hit = &gPlayers[playerIndex];
        hit->bossdamage = lbl_80346470;
        counter = (u8 *)c + playerIndex * 0x10;
        hit->fxhittime = (f32)(lbl_80346500 + (f64)sMusicFadeBase);
        *(f32 *)(counter + 0x1BC) += damage;
        *(f32 *)(counter + 0x1C0) = sMusicFadeBase;
    }
}

/* 0x800368DC -- add `amount` to a per-limb counter of the critter whose id
 * matches `id`, then stamp the companion slot with the current game time. */
void CritterSetFxHitTime(s32 slot, s32 id, f32 amount)
{
    s32 i;
    Critter *c;
    CritterBigState *big;

    big = &gBig;
    for (i = 0; i < lbl_8034466C; i++) {
        c = &big->pool[i];
        if (c->hdr != NULL && id == c->id) {
            break;
        }
    }
    if (i >= lbl_8034466C) {
        return;
    }
    big->pool[i].playerDamage[slot].received += amount;
    big->pool[i].playerDamage[slot].receivedTime = sMusicFadeBase;
}

/* 0x80036958 -- resolve a critter target position from either its selected
 * player or the current waypoint chain. */
s32 CritterGetTarget(Critter *c, f32 *out)
{
    /* lint-allow-next-line FM003: measured frame slot -- deleting it moves CritterGetTarget by 7 words at unchanged size; original local unrecovered */
    u8 unused[16];
    void *waypoint;
    f64 minimum_distance;
    s32 result;

    if (c->targetCount <= 0) {
        goto init_waypoint_search;
    } else {
        s32 player = c->targets[0].pidx;
        u8 *record = (u8 *)&gPlayers[player];

        out[0] = ((Player *)record)->effectpos[0];
        out[1] = ((Player *)record)->effectpos[1];
        out[2] = ((Player *)record)->effectpos[2];
        result = 1;
        goto done;
    }

waypoint_body:
    {
        f32 dx;
        f32 dy;
        f32 x;
        f32 dz;
        f32 distance;

        dy = ((OBJGRP *)waypoint)->worldmat[3][1] -
             c->vel[1];
        dx = (x = ((OBJGRP *)waypoint)->worldmat[3][0]) - c->vel[0];
        dz = ((OBJGRP *)waypoint)->worldmat[3][2] -
             c->vel[2];
        distance = dx * dx + dy * dy;
        distance = dz * dz + distance;

        if ((f64)distance < minimum_distance) {
            c->particle = NextWaypoint(waypoint);
            goto waypoint_test;
        } else {
            out[0] = x;
            out[1] = ((OBJGRP *)c->particle)->worldmat[3][1];
            out[2] = ((OBJGRP *)c->particle)->worldmat[3][2];
            result = 1;
            goto done;
        }
    }

init_waypoint_search:
    minimum_distance = lbl_80346490;
waypoint_test:
    waypoint = c->particle;
    if (waypoint != NULL) {
        goto waypoint_body;
    }

    out[0] = 0.0f;
    out[1] = 0.0f;
    out[2] = 0.0f;
    result = 0;
done:
    return result;
}
#pragma dont_inline on
#pragma opt_propagation off
/* 0x80036A58 */ s32 CritterGetTargetSub(Critter *c, f32 *target, s32 mode)
{
    s32 i;
    s32 best;
    f32 bestScore;

    best = -1;
    bestScore = lbl_80346508;
    if (target != NULL && (f64)target[6] > lbl_80346488 &&
        c->unk4AC > target[6]) {
        return -1;
    }

    for (i = 0; i < c->targetCount; i++) {
        f32 score = CritterReCalcTarget(c, target, i);
        if (best < 0 || score < bestScore) {
            best = i;
            bestScore = score;
        }
    }
    if (mode == 0 && (f64)bestScore >= lbl_80346510) {
        best = -1;
    }
    if (best >= 0) {
        u32 address = (u32)c + best * 0x24;
        best = ((Critter *)address)->targets[0].pidx;
    } else if (mode != 0 && c->parent != NULL) {
        best = CritterGetTargetSub(c->parent, target, mode);
    }
    return best;
}
#pragma opt_propagation reset
#pragma dont_inline off
/* 0x80036B5C -- score one entry in the critter's target list against the
 * optional move targeting constraints. */
f32 CritterReCalcTarget(Critter *c, f32 *moveTarget, s32 target)
{
    f32 *entry;
    f32 forward[3];
    f32 dot;
    f32 range;

    entry = (f32 *)&c->targets[target];
    if (moveTarget != NULL) {
        if (c->rateScale < moveTarget[4]) {
            return lbl_80346518;
        }
        if (moveTarget[5] > moveTarget[4] &&
            c->rateScale >= moveTarget[5]) {
            return lbl_80346518;
        }
    }

    range = entry[2];
    if (moveTarget != NULL) {
        if (range < moveTarget[0]) {
            return lbl_8034651C;
        }
        if (moveTarget[1] > lbl_80346488 && range > moveTarget[1]) {
            return lbl_80346520;
        }
        YawVec3((f32 *)((u8 *)c + offsetof(Critter, mtx) + 0x20), forward, -moveTarget[2]);
        forward[1] = lbl_80346470;
        SlowNormalVector(forward);
        dot = entry[5] * forward[0] + entry[7] * forward[2];
        if (dot < moveTarget[3]) {
            return lbl_80346524;
        }
    }
    range = range * entry[4];
    return range;
}

/* 0x80036C70 -- choose the single best live player target. */
void CritterGetSingleTargetPlayer(Critter *c)
{
    f32 targetpos[3];
    CritterTargetInfo candidate;
    Player *player;
    s32 i;
    f32 score;
    f32 one;
    f64 thousand;
    f64 zero;

    c->targetCount = 0;
    if (c->health <= 0.0f) {
        return;
    }
    one = lbl_803464A8;
    thousand = lbl_80346528;
    zero = lbl_80346488;
    player = gPlayers;
    for (i = 0; i < 4; i++, player++) {
        if (player->state != 1 || (player->flags & 4) != 0) {
            continue;
        }
        targetpos[0] = player->effectpos[0];
        targetpos[1] = player->effectpos[1];
        targetpos[2] = player->effectpos[2];
        score = CritterCalcTarget(c, (f32 *)&c->hdr->target,
                                  targetpos,
                                  &candidate);
        if (c->particle != NULL && c->unkAD0 > zero && score > c->unkAD0) {
            continue;
        }
        if (sMusicFadeBase < player->fxhittime) {
            score = score * thousand;
        }
        if (c->targetCount == 0 ||
            score < c->targets[0].testdist) {
            c->targetCount = 1;
            candidate.testdist = score;
            candidate.pidx = i;
            candidate.invanger = one;
            c->targets[0] = candidate;
        }
    }
    if (c->targetCount != 0) {
        c->particle = NULL;
        gBig.scratch[c->targets[0].pidx] += lbl_803464A8;
    }
}

/* 0x80036E00 -- distribute over-subscribed player targets among a root
 * critter and its child chain. */
void CritterResolveMultipleTargets(Critter *c)
{
    s32 threshold;
    s32 i;
    s32 outerOffset;
    s32 player;
    f32 decrement;

    if (c->alivecnt <= 0) {
        return;
    }
    if (c->unk11C >= 0) {
        return;
    }
    decrement = 1.0f;
    outerOffset = 0;
    for (i = 0; i < c->targetCount; i++, outerOffset += 0x24) {
        CritterTargetInfo *record = (CritterTargetInfo *)
            ((u8 *)c + offsetof(Critter, targets[0].pidx) + outerOffset);
        player = (s32)record->pidx;
        if (record->invanger > lbl_80346490) {
            threshold = 2;
        } else if (record->invanger > lbl_80346530) {
            threshold = 3;
        } else {
            threshold = 4;
        }
        while (gBig.scratch[player] > threshold) {
            Critter *child;
            Critter *owner;
            s32 selected;
            s32 j;
            f32 best;

            owner = NULL;
            best = 0.0f;
            selected = 0;
            for (child = c->next; child != NULL; child = child->next) {
                if (child->unk11C >= 0) {
                    continue;
                }
                for (j = 0; j < child->targetCount; j++) {
                    CritterTargetInfo *entry;
                    entry = &child->targets[j];
                    if ((s32)entry->pidx == player &&
                        (owner == NULL || entry->testdist > best)) {
                        owner = child;
                        best = entry->testdist;
                        selected = j;
                        break;
                    }
                }
            }
            if (owner == NULL) {
                break;
            }
            for (j = selected; j < owner->targetCount - 1; j++) {
                owner->targets[j] =
                    owner->targets[j + 1];
            }
            owner->targetCount--;
            gBig.scratch[player] -= decrement;
        }
        record++;
    }
}

/* 0x80036FBC -- collect and distance-sort all eligible player targets. */
void CritterGetTargetPlayers(Critter *c)
{
    /* lint-allow-next-line FM003: measured frame slot -- deleting it moves CritterGetTargetPlayers by 7 words at unchanged size; original local unrecovered */
    u8 unused2[4];
    f32 targetpos[3];
    CritterTargetInfo record;
    /* lint-allow-next-line FM003: measured frame slot -- deleting it moves CritterGetTargetPlayers by 20 words at unchanged size; original local unrecovered */
    u8 unused[4];
    Player *player;
    s32 i;
    f32 score;
    f32 damage;
    f32 base;
    f64 ratio;
    f64 quot;
    f32 thr;
    f32 result;
    f64 pt01;
    f64 one;
    f64 huge;
    f64 thousand;
    f64 zero;

    c->targetCount = 0;
    if (c->health <= 0.0f) {
        return;
    }
    pt01 = lbl_80346540;
    one = lbl_80346490;
    thousand = lbl_80346528;
    zero = lbl_80346488;
    huge = lbl_80346510;
    player = gPlayers;
    for (i = 0; i < 4; i++, player++) {
        if (player->state != 1) {
            continue;
        }
        if ((player->flags & 4) && c->state != 0) {
            if (*(s16 *)((u8 *)*(void **)((u8 *)c->hdr + offsetof(CritterPackedType, descriptor)) + offsetof(CritterDescriptor, type)) != 4) {
                continue;
            }
        }
        targetpos[0] = player->effectpos[0];
        targetpos[1] = player->effectpos[1];
        targetpos[2] = player->effectpos[2];
        score = CritterCalcTarget(c, (f32 *)&c->hdr->target, targetpos,
                                  &record);
        if (c->particle != NULL) {
            thr = c->unkAD0;
            if (thr > zero && score > thr) {
                continue;
            }
        }
        if (sMusicFadeBase < player->fxhittime) {
            record.testdist = record.testdist * thousand;
        }
        if (score < huge) {
            record.pidx = i;
            damage = c->playerDamage[i].dealt;
            base = c->playerDamage[i].received;
            if (damage < one) {
                result = one + lbl_80343BEC;
            } else {
                if ((quot = base / damage) < pt01) {
                    ratio = pt01;
                } else if (quot > lbl_80343BEC) {
                    ratio = lbl_80343BEC;
                } else {
                    ratio = quot;
                }
                result = ratio;
            }
            record.invanger = result;
            record.testdist = record.testdist * record.invanger;
            CritterInsertTarget(c, &record);
        }
    }
    for (i = 0; i < c->targetCount; i++) {
        s32 index = c->targets[i].pidx;
        if (index >= 0) {
            gBig.scratch[index] += lbl_803464A8;
        }
    }
}
/* 0x800371BC -- insert a target record into the four-entry distance-sorted
 * target list. */
void CritterInsertTarget(Critter *c, CritterTargetInfo *target)
{
    s32 count;
    s32 insert;
    s32 shift;
    f32 distance;

    count = c->targetCount;
    insert = 0;
    distance = target->testdist;

    while (insert < count) {
        if (distance < c->targets[insert].testdist) {
            for (shift = count; shift > insert; shift--) {
                if (shift < 4) {
                    c->targets[shift] = c->targets[shift - 1];
                }
            }
            break;
        }
        insert++;
    }

    if (c->targetCount < 4) {
        c->targetCount++;
    }
    if (insert < 4) {
        c->targets[insert] = *target;
    }
}

static inline f32 CritterCalcTargetScore(f32 distance, f32 dot, f32 *absolute)
{
    if (dot > lbl_803464F8) {
        *absolute = dot;
        *(u32 *)absolute &= 0x7FFFFFFF;
        return distance / *absolute;
    }
    return lbl_8034654C * distance;
}

/* 0x800372A0 -- calculate range, facing and score for a world-space target. */
f32 CritterCalcTarget(Critter *c, f32 *moveTarget, f32 *target,
                      CritterTargetInfo *record)
{
    f32 forward[3];
    /* lint-allow-next-line FM003: measured frame slot -- deleting it moves CritterCalcTarget by 18 words at unchanged size; original local unrecovered */
    u8 vectorGap[4];
    f32 delta[3];
    f32 distance;
    f32 vertical;
    f32 dot;
    f32 score;
    f32 absdot;
    f32 absdot2;
    /* lint-allow-next-line FM003: measured frame slot -- deleting it moves CritterCalcTarget by 39 words at unchanged size; original local unrecovered */
    u8 unused[16];

    if (moveTarget != NULL) {
        if (c->rateScale < moveTarget[4]) {
            return lbl_80346518;
        }
        if (moveTarget[5] > lbl_80346488 &&
            c->rateScale >= moveTarget[5]) {
            return lbl_80346518;
        }
    }

    delta[0] = target[0] - c->pos[0];
    delta[1] = target[1] - c->pos[1];
    delta[2] = target[2] - c->pos[2];
    vertical = delta[1];
    delta[1] = lbl_80346470;
    distance = SlowNormalVector(delta);

    if (moveTarget != NULL) {
        if (distance < moveTarget[0]) {
            return lbl_8034651C;
        }
        if (moveTarget[1] > lbl_80346488 && distance > moveTarget[1]) {
            return lbl_80346520;
        }
        if (vertical < lbl_80346470) {
            vertical = -vertical;
        }
        if (moveTarget[7] > *(volatile f64 *)&lbl_80346488 &&
            vertical > moveTarget[7]) {
            return lbl_80346548;
        }
        YawVec3((f32 *)((u8 *)c + offsetof(Critter, mtx) + 0x20), forward, -moveTarget[2]);
        forward[1] = lbl_80346470;
        SlowNormalVector(forward);
        dot = delta[0] * forward[0] + delta[2] * forward[2];
        if (dot < moveTarget[3]) {
            return lbl_80346524;
        }
        score = CritterCalcTargetScore(distance, dot, &absdot);
    } else {
        forward[0] = c->mtx[2][0];
        forward[1] = lbl_80346470;
        forward[2] = c->mtx[2][2];
        SlowNormalVector(forward);
        dot = delta[0] * forward[0] + delta[2] * forward[2];
        score = CritterCalcTargetScore(distance, dot, &absdot2);
    }
    if (record != NULL) {
        f32 *out = (f32 *)record;
        out[1] = dot;
        out[2] = distance;
        out[3] = score;
        out[5] = delta[0];
        out[6] = delta[1];
        out[7] = delta[2];
    }
    return score;
}
static inline s32 CritterMoveNoHit(Critter *c, s32 id)
{
    Critter *relative;

    if (CritterNoHitSub(c, id)) {
        return 1;
    }
    relative = c->parent;
    if (relative != NULL) {
        if (CritterNoHitSub(relative, id)) {
            return 1;
        }
    } else {
        relative = c->next;
        while (relative != NULL) {
            if (CritterNoHitSub(relative, id)) {
                return 1;
            }
            relative = relative->next;
        }
    }
    return 0;
}

/* 0x800374FC -- sweep a movement segment against every other live critter. */
void *CritterMoveNodeCol(f32 radius, f32 height, f32 *origin,
                         f32 *destination, f32 *contact, s32 ignore,
                         s32 mode)
{
    Critter *c;
    f32 dz;
    f32 dx;
    f32 dy;
    f32 horizontalSquared;
    f32 verticalSquared;
    /* lint-allow-next-line FM003: measured frame slot -- deleting it moves CritterMoveNodeCol by 7 words at unchanged size; original local unrecovered */
    u8 unused[8];
    s32 result;

    dz = destination[2] - origin[2];
    dx = destination[0] - origin[0];
    dy = destination[1] - origin[1];
    horizontalSquared = dx * dx + dz * dz;
    verticalSquared = dy * dy;
    result = 0;
    while (lbl_80344644 < lbl_8034466C) {
        c = &gCritterPool[lbl_80344644++];
        if (c->hdr == NULL || c == lbl_80344648) {
            continue;
        }
        if (ignore >= 0) {
            if (CritterMoveNoHit(c, ignore)) {
                continue;
            }
        }
        if ((c->hdr->typeFlags & 2) != 0) {
            result = CritterExpNodeColSub(c, radius, horizontalSquared,
                                          verticalSquared, origin, destination,
                                          contact, mode);
        }
        if (!result) {
            f32 verticalRadius;
            f32 horizontalRadius;
            s32 collided;

            horizontalRadius = radius + c->hdr->wallRadius;
            dz = c->pos[2] - destination[2];
            verticalRadius = radius + c->hdr->radius;
            dx = c->pos[0] - destination[0];
            if (dx * dx + dz * dz >
                horizontalRadius * horizontalRadius + horizontalSquared) {
                collided = 0;
            } else if (c->pos[1] - destination[1] >
                       verticalRadius + verticalSquared) {
                collided = 0;
            } else if (LineCylinderCollide(c->pos, horizontalRadius,
                                           verticalRadius, origin, destination,
                                           contact, mode)) {
                collided = 1;
            } else {
                collided = 0;
            }
            result = collided;
        }
        if (result) {
            return c;
        }
    }
    return NULL;
}

/* 0x80037734 -- sweep against one critter's active collision nodes and keep
 * either the first or nearest contact. */
s32 CritterMoveNodeColSub(Critter *c, f32 radius, f32 height,
                          f32 *offsetVec, f32 *lineStart, f32 *contact,
                          s32 first)
{
    f32 nodePosition[3];
    /* lint-allow-next-line FM003: measured frame slot -- deleting it moves CritterMoveNodeColSub by 13 words at unchanged size; original local unrecovered */
    u8 positionPad[20];
    f32 hit[3];
    f32 best;
    f32 distance;
    f32 nodeRadius;
    u8 *base;
    u8 *node;
    CritterColDescriptor *nodeDef;
    s32 byteOffset;
    s32 i;
    s32 resultIndex;

    resultIndex = -1;
    i = 0;
    byteOffset = 0;
    best = lbl_80346480;
    /* `node` is deliberately a byte cursor that is ADVANCED onto the node's
     * position vector and then passed to LineCylinderCollide: the target emits
     * `lfsu f3,60(r4)` at 0x80037734+0x80 and reuses r4 as the call's p2
     * argument, which only a pointer that is reassigned in place produces.
     * Three typed rewrites were measured against the banked object and all
     * three move bytes because the update-form load is lost:
     *   `node = &c->hitnodes[i]` + `node->position[k]`   404 -> 408 B, 74 words
     *   walking `CritterHitNode *node; node++`           404 -> 400 B, 88 words
     *   typed node off the byte offset + `f32 *npos`     404 -> 408 B, 75 words
     * (the last one still shows `opcode multiset DIFFERS target-only: +1 lfsu`
     * under `fndiff game/enemy/critter CritterMoveNodeColSub --ops`). */
    /* lint-begin FM001: cursor advanced in place for the target's lfsu, deltas above */
    while (i < c->hdr->colCount) {
        base = (u8 *)c + byteOffset;
        node = base + offsetof(Critter, hitnodes);
        if (*(void **)(base + (offsetof(Critter, hitnodes) + offsetof(CritterHitNode, active))) == NULL ||
            ((CritterHitNode *)node)->activeFrom >= ((CritterHitNode *)node)->activeUntil ||
            (((CritterColDescriptor *)*(u8 **)node)->flags & 8) == 0) {
            goto next;
        }
        nodeDef = *(CritterColDescriptor **)node;
        nodeRadius = nodeDef->radius;
        nodePosition[0] = *(f32 *)(node += offsetof(CritterHitNode, position)) + offsetVec[0];
        nodePosition[1] = *(f32 *)(node + 4) + offsetVec[1];
        nodePosition[2] = *(f32 *)(node + 8) + offsetVec[2];
        if (LineCylinderCollide(lineStart, radius + nodeRadius,
                                height + nodeRadius, (f32 *)node,
                                nodePosition, hit, 1)) {
            if (first) {
                contact[0] = hit[0];
                contact[1] = hit[1];
                contact[2] = hit[2];
                return i + 1;
            }
            distance = fqdist(lineStart[0] - nodePosition[0],
                              lineStart[2] - nodePosition[2]);
            if (resultIndex < 0 || distance < best) {
                best = distance;
                resultIndex = i;
                contact[0] = hit[0];
                contact[1] = hit[1];
                contact[2] = hit[2];
            }
        }
next:
        i++;
        byteOffset += 0x5C;
    }
    /* lint-end FM001 */
    if (resultIndex >= 0) {
        return resultIndex + 1;
    }
    return 0;
}

/* 0x800378C8 -- test an expanded sphere/capsule against one critter's active
 * hit nodes. */
s32 CritterExpNodeColSub(Critter *c, f32 radius, f32 squaredExpand,
                         f32 height, f32 *origin, f32 *destination,
                         f32 *contact, s32 mode)
{
    f32 dx;
    f32 dy;
    f32 dz;
    f32 nodeRadius;
    CritterHitNode *node;
    s32 offset;

    offset = 0;
    while (offset < c->hdr->colCount) {
        node = &c->hitnodes[offset];
        if (node->active == NULL) {
            goto next;
        }
        if (node->activeFrom >= node->activeUntil) {
            goto next;
        }
        if (mode != 2 || (node->descriptor->flags & 8) != 0) {
            dz = node->position[2] - destination[2];
            nodeRadius = radius + node->descriptor->radius;
            dx = node->position[0] - destination[0];
            if (dx * dx + dz * dz >
                nodeRadius * nodeRadius + squaredExpand) {
                goto next;
            }
            dy = node->position[1] - destination[1];
            if (dy > nodeRadius + height) {
                goto next;
            }
            if (LineCylinderCollide(node->position, nodeRadius,
                                    nodeRadius, origin, destination,
                                    contact, mode)) {
                c->unkAB8 = (s16)offset;
                return 1;
            }
        }
    next:
        offset++;
    }
    c->unkAB8 = -1;
    return 0;
}

/* 0x80037A10 -- expanded collision query across all critter roots, excluding
 * families that already own the supplied timed hit id. */
Critter *CritterExpCollide(f32 *origin, f32 *forward, f32 radius,
                           f32 dot, f32 *contact, s32 timedId)
{
    Critter *c;
    /* lint-allow-next-line FM003: measured frame slot -- deleting it moves CritterExpCollide by 7 words at unchanged size; original local unrecovered */
    u8 unused2[8];
    f32 delta[3];
    /* lint-allow-next-line FM003: measured frame slot -- deleting it moves CritterExpCollide by 8 words at unchanged size; original local unrecovered */
    u8 unused[16];
    f32 bodyRadius;
    f32 distance;
    s32 result;

    result = 0;
    if (contact == NULL) {
        contact = delta;
    }
    while (lbl_80344644 < lbl_8034466C) {
        c = &gCritterPool[lbl_80344644++];
        if (c->hdr == NULL || c == lbl_80344648) {
            continue;
        }
        if (CritterMoveNoHit(c, timedId)) {
            continue;
        }
        if ((c->hdr->typeFlags & 2) != 0) {
            result = CritterLineNodeColSub(c, origin, forward, contact,
                                           radius, dot);
        }
        if (!result) {
            s32 collided;

            contact[0] = c->pos[0] - origin[0];
            contact[1] = c->pos[1] - origin[1];
            contact[2] = c->pos[2] - origin[2];
            distance = NormalVector2D(contact);
            bodyRadius = radius + c->hdr->wallRadius;
            if (distance > bodyRadius) {
                collided = 0;
            } else if (dot > -1.0 &&
                       contact[0] * forward[0] +
                       contact[2] * forward[2] < dot) {
                collided = 0;
            } else {
                collided = 1;
            }
            result = collided;
        }
        if (result) {
            return c;
        }
    }
    return NULL;
}
/* 0x80037C08 -- find an active collision node within `radius` and, when
 * requested, inside the caller's forward-facing half-space. */
s32 CritterLineNodeColSub(Critter *c, f32 *origin, f32 *forward,
                          f32 *delta, f32 radius, f32 dotThreshold)
{
    s32 i;
    CritterHitNode *node;
    f32 distance;
    /* lint-allow-next-line FM003: measured frame slot -- deleting it moves CritterLineNodeColSub by 7 words at unchanged size; original local unrecovered */
    u8 unused[16];

    i = 0;
    while (i < c->hdr->colCount) {
        node = &c->hitnodes[i];
        if (node->active == NULL) {
            goto next;
        }
        if (node->activeFrom >= node->activeUntil) {
            goto next;
        }

        delta[0] = node->position[0] - origin[0];
        delta[1] = node->position[1] - origin[1];
        delta[2] = node->position[2] - origin[2];
        distance = NormalVector2D(delta);
        if (distance > radius + node->descriptor->radius) {
            goto next;
        }
        if (dotThreshold > -1.0 &&
            delta[0] * forward[0] + delta[2] * forward[2] < dotThreshold) {
            goto next;
        }
        c->unkAB8 = (s16)i;
        return 1;

    next:
        i++;
    }

    c->unkAB8 = -1;
    return 0;
}
/* 0x80037D34 */
void CritterCollideStart(s32 unused, void *ctx)
{
    lbl_80344648 = ctx;
    lbl_80344644 = 0;
}

static inline s32 CritterTimedSlotActive(Critter *c, s32 id)
{
    s32 i;

    for (i = 0; i < 4; i++) {
        if (c->unk4E0[i] == id) {
            if (sMusicFadeBase < c->timed[i]) {
                return 1;
            }
        }
    }
    return 0;
}

/* 0x80037D44 -- search this critter and its immediate family for an active
 * timed slot.  Children are searched only for a root critter. */
s32 CritterNoHit(Critter *c, s32 id)
{
    Critter *relative;

    if (CritterTimedSlotActive(c, id)) {
        return 1;
    }
    relative = c->parent;
    if (relative != NULL) {
        if (CritterTimedSlotActive(relative, id)) {
            return 1;
        }
    } else {
        relative = c->next;
        while (relative != NULL) {
            if (CritterTimedSlotActive(relative, id)) {
                return 1;
            }
            relative = relative->next;
        }
    }
    return 0;
}
/* 0x80037E80 -- test whether a timed per-player slot is active for `id`. */
s32 CritterNoHitSub(Critter *c, s32 id)
{
    s32 i;

    for (i = 0; i < 4; i++) {
        if (c->unk4E0[i] == id) {
            if (sMusicFadeBase < c->timed[i]) {
                return 1;
            }
        }
    }
    return 0;
}
/* 0x80037ED0 -- allocate or replace one of the four timed id slots. */
void fn_80037ED0(f32 add, Critter *c, s32 id)
{
    s32 i;
    s32 oldest;
    f32 oldest_time;
    s32 offset;

    oldest = -1;
    oldest_time = lbl_80346480;
    if ((f64)add <= lbl_80346488) {
        return;
    }

    for (i = 0, offset = 0; i < 4; i++, offset += sizeof(f32)) {
        if (sMusicFadeBase > c->timed[i]) {
            c->unk4E0[i] = id;
            c->timed[i] = sMusicFadeBase + add;
            return;
        }
        if ((f64)oldest_time < lbl_80346488 ||
            c->timed[i] < oldest_time) {
            oldest_time = c->timed[i];
            oldest = i;
        }
    }
    if (oldest < 0) {
        return;
    }
    c->unk4E0[oldest] = id;
    c->timed[oldest] = sMusicFadeBase + add;
}
/* 0x80037F84 -- find the nearest live critter intersected by a directed
 * safe-rock query, considering both roots and their child chains. */
Critter *CritterLineCollide(f32 dotThresh, f32 limit, f32 *origin,
                            f32 *forward, f32 *out, f32 *score)
{
    /* lint-allow-next-line FM003: measured frame slot -- deleting it moves CritterLineCollide by 7 words at unchanged size; original local unrecovered */
    u8 unused[8];
    f32 contact[3];
    Critter *pool;
    Critter *cur;
    Critter *bestC;
    s32 count;
    s32 i;
    f32 cx;
    f32 cy;
    f32 cz;
    f32 best;
    f32 d;
    /* lint-allow-next-line FM003: measured frame slot -- deleting it moves CritterLineCollide by 11 words at unchanged size; original local unrecovered */
    u8 pad24[20];

    best = lbl_80346508;
    pool = gCritterPool;
    cur = NULL;
    bestC = NULL;
    i = -1;
    while ((count = lbl_8034466C) >= 0) {
        if (cur != NULL && cur->next != NULL) {
            cur = cur->next;
        } else {
            i++;
            for (; i < count; i++) {
                cur = &pool[i];
                if (cur->state >= 2 && cur->parent == NULL) {
                    break;
                }
            }
            if (i >= count) {
                break;
            }
        }
        d = CritterLineRootColSub(cur, origin, forward, contact, dotThresh,
                                  limit);
        if (d < best) {
            best = d;
            cx = contact[0];
            cy = contact[1];
            bestC = cur;
            cz = contact[2];
        }
    }
    if (out != NULL) {
        if (best >= lbl_80346558) {
            if (out != NULL) {
                out[0] = forward[0];
                out[1] = forward[1];
                out[2] = forward[2];
            }
        } else {
            out[0] = cx;
            out[1] = cy;
            out[2] = cz;
        }
    }
    if (score != NULL) {
        *score = best;
    }
    return bestC;
}

/* 0x800380F0 -- score a swept point against a critter's active hit nodes,
 * falling back to its body radius when it has no qualifying node. */
f32 CritterLineRootColSub(Critter *c, f32 *origin, f32 *forward, f32 *out,
                          f32 dotThresh, f32 limit)
{
    f32 delta[3];
    u8 *hdr;
    s32 i;
    CritterHitNode *node;
    CritterColDescriptor *nodeDef;
    f32 best;
    f32 bestScore;
    f32 slope;
    f32 nd;
    f32 dr;
    f32 dist;
    f32 q;
    f32 dot;
    f32 thresh;
    f32 score;
    f32 nodeMax;
    f64 eps2;

    best = *(volatile f32 *)&lbl_80346508;
    hdr = (u8 *)c->hdr;
    bestScore = best;
    if (limit > *(volatile f64 *)&lbl_80346488) {
        slope = (lbl_80346490 - dotThresh) / limit;
    } else {
        slope = lbl_80346470;
    }
    if (c->health <= lbl_80346470) {
        return lbl_80346508;
    }
    if (c->state < 2) {
        return lbl_80346508;
    }
    if ((((CritterPackedType *)hdr)->typeFlags & 2) != 0) {
        eps2 = lbl_80346488;
        for (i = 0; i < ((CritterPackedType *)hdr)->colCount; i++) {
            node = &c->hitnodes[i];
            if (node->active == NULL) {
                continue;
            }
            if (node->activeFrom >= node->activeUntil) {
                continue;
            }
            delta[0] = node->position[0] - origin[0];
            delta[1] = node->position[1] - origin[1];
            delta[2] = node->position[2] - origin[2];
            dist = NormalVector(delta);
            if (limit > eps2 && dist > limit) {
                continue;
            }
            nodeDef = node->descriptor;
            nodeMax = nodeDef->maxTargetDistance;
            if (nodeMax > eps2 && dist > nodeMax) {
                continue;
            }
            nd = dist - nodeDef->radius;
            q = fqdist(delta[0], delta[2]);
            dot = delta[0] * forward[0] + delta[2] * forward[2];
            if (slope > eps2) {
                thresh = q * (nd * slope + dotThresh);
            } else {
                thresh = dotThresh;
            }
            if (dot <= thresh) {
                continue;
            }
            score = nd / (node->descriptor->targetScoreScale * (dot - thresh));
            if (score < bestScore) {
                out[0] = delta[0];
                bestScore = score;
                best = nd;
                out[1] = delta[1];
                out[2] = delta[2];
            }
        }
        if (best < lbl_80346558) {
            return best;
        }
    }
    delta[0] = c->pos[0] - origin[0];
    delta[1] = c->pos[1] - origin[1];
    delta[2] = c->pos[2] - origin[2];
    dist = NormalVector(delta);
    if (limit > *(volatile f64 *)&lbl_80346488 && dist > limit) {
        return lbl_80346508;
    }
    dr = dist - c->hdr->wallRadius;
    q = fqdist(delta[0], delta[2]);
    dot = delta[0] * forward[0] + delta[2] * forward[2];
    thresh = q * (dr * slope + dotThresh);
    if (dot <= thresh) {
        return lbl_80346508;
    }
    out[0] = delta[0];
    out[1] = delta[1];
    out[2] = delta[2];
    return dr;
}

/* 0x800383A8 -- apply damage to a critter/hit node, accumulate combat
 * bookkeeping and transition a depleted critter into its death state. */
#pragma dont_inline on
s32 CritterDamage(f32 damage, Critter *c, s32 player, u32 flags,
                  f32 *hitPosition, f32 *direction, s32 source)
{
    typedef struct CritterDamageMove {
        s32 type;
        u8 unused04[0x54];
        s16 sfx;
        s16 sfxFrame;
    } CritterDamageMove;
    u8 *hitNode;
    CritterDamageMove *move;
    Critter *child;
    Critter *parent;
    Player *playerData;
    f32 creditedDamage;
    f32 ratio;
    f32 livingChildren;
    f32 childDamage;
    f32 damageScale;
    s32 critterClass;
    s32 experience;
    s32 lastPlayer;
    s32 i;

    if (c->hdr == NULL) {
        return -1;
    }
    if (c->state < 2) {
        return -1;
    }

    move = (CritterDamageMove *)&
        (c->hdr->movesPtr)[c->curmove];
    if (move->type == 35) {
        damage = (f32)((f64)damage * lbl_80346500);
        flags &= ~0x130;
        if (move->sfxFrame >= 1000 &&
            (c->moveSfxFlags & 1) == 0 &&
            move->sfx >= 0) {
            c->moveSfxFlags |= 1;
            CritterDoSfx(c, move->sfx, NULL, 1, -1);
        }
    }

    {
        u32 shieldFlags = c->hdr->shieldFlags;
        f32 armor = c->hdr->armor;

        ModifyDamage(armor, &damage, &flags, shieldFlags);
    }
    critterClass = c->hdr->descriptor->type;

    if (gGameOptions[0] == 3 && player >= 0) {
        damage = lbl_80346560;
    }
    if (critterClass != 4 &&
        (f64)lbl_803447D8 < lbl_80346490) {
        damage = (f32)((f64)damage * lbl_80346478);
    }

    c->counterValue += damage;
    if (critterClass == 4 &&
        (lbl_8034489C < 1 || lbl_8034489C > 4)) {
        damage *= lbl_8011AEC0[lbl_8034465C];
    }

    if (player >= 0) {
        f32 maximumHealth;

        maximumHealth = c->hdr->maxHealth *
                        *(f32 *)((u8 *)gCurLevel + offsetof(level_data, ene_health));
        creditedDamage = lbl_80346470;
        if (damage < creditedDamage) {
            goto credited_damage_done;
        }
        creditedDamage = c->health;
        if (damage > creditedDamage) {
            goto credited_damage_done;
        }
        creditedDamage = damage;

credited_damage_done:

        ratio = (f32)((f64)creditedDamage /
                      (lbl_80346490 + (f64)maximumHealth));
        if ((f64)ratio > lbl_80346490) {
            ratio = lbl_803464A8;
        }
        experience = (s32)(ratio * c->hdr->expValue);
        if (critterClass == 4) {
            experience *= lbl_8034465C;
        }

        i = player;
        if (player >= 0) {
            lastPlayer = player + 1;
        } else {
            i = 0;
            lastPlayer = 4;
        }
        playerData = &gPlayers[i];
        for (; i < lastPlayer; i++, playerData++) {
            if (playerData->state == 1) {
                AddExp(i, (s32)(f32)experience, 0);
            }
        }

        c->playerDamage[player].dealt += creditedDamage;
        c->playerDamage[player].dealtTime = sMusicFadeBase;
        if (flags & 0x00800000) {
            do_heal_players(&gPlayers[player], &c->mtx[0][0],
                            creditedDamage);
        }

        if (critterClass != 4 &&
            *(f32 *)((u8 *)gCurLevel + offsetof(level_data, plevel)) > lbl_80346470) {
            s32 level;

            playerData = &gPlayers[player];
            level = *(s32 *)((u8 *)playerData + offsetof(Player, level));
            damageScale = lbl_803464A8;
            if ((f32)level < *(f32 *)((u8 *)gCurLevel + offsetof(level_data, plevel))) {
                damageScale = (f32)(lbl_80346490 -
                    lbl_80346568 *
                    (f64)(*(f32 *)((u8 *)gCurLevel + offsetof(level_data, plevel)) -
                          (f32)level));
            }
            if ((f64)damageScale < lbl_803464B0) {
                damageScale = lbl_80346570;
            }
            damage *= damageScale;
        }
    }

    if ((flags & 0x00100320) == 0 && c->unkAB8 >= 0) {
        /* Measured load-bearing (banked-object comparison, this lane):
          * retyping `hitNode` to `CritterHitNode *` and spelling these as
          * members rebuilds CritterDamage at 2424 bytes (target 604 insns vs
          * ours 606) with 286 differing words, whether the cursor is written
          * `&c->hitnodes[c->unkAB8]` or as the cast byte offset below, and
          * whether or not the descriptor chain is also converted -- all three
          * measured the same 2416 -> 2424 B / 286 words.  The element type is
          * recovered (CritterHitNode, 0x5C, in game/critter.h); only the access
          * spelling is held back, so the residual is a separate matching
          * obligation on this function's register web. */
        /* lint-begin FM001, FM002: measured load-bearing spelling, delta above */
        hitNode = (u8 *)c + offsetof(Critter, hitnodes) + c->unkAB8 * 0x5C;
        if (*(f32 *)(hitNode + offsetof(CritterHitNode, activeFrom)) >= *(f32 *)(hitNode + offsetof(CritterHitNode, activeUntil))) {
            damage = lbl_80346470;
        } else {
            damage *= *(f32 *)(*(u8 **)hitNode +
                               offsetof(CritterColDescriptor, damageScale));
            if (*(f32 *)(hitNode + offsetof(CritterHitNode, activeFrom)) + damage >
                *(f32 *)(hitNode + offsetof(CritterHitNode, activeUntil))) {
                damage = *(f32 *)(hitNode + offsetof(CritterHitNode, activeUntil)) -
                         *(f32 *)(hitNode + offsetof(CritterHitNode, activeFrom));
                if (*(s16 *)(*(u8 **)hitNode +
                             offsetof(CritterColDescriptor, flags)) & 2) {
                    char objectName[40];
                    s32 object;
                    CritterDescriptor *hitCritterDesc;
                    s32 modelIndex;

                    if (*(s16 *)(*(u8 **)hitNode +
                                 offsetof(CritterColDescriptor, sfxIndex)) >= 0) {
                        CritterDoTexmodNode(c,
                            *(s16 *)(*(u8 **)hitNode + offsetof(CritterColDescriptor, sfxIndex)), 0,
                            (f32 *)(hitNode + offsetof(CritterHitNode, position)));
                    }
                    hitCritterDesc =
                        c->hdr->descriptor;
                    modelIndex = hitCritterDesc->modelIndex;
                    sprintf(objectName, "%sD%s", hitCritterDesc->prefix,
                            ((CritterColDescriptor *)*(u8 **)hitNode)->nodeName);
                    object = (s32)MBOX_ReallyFindObject(objectName, modelIndex,
                                                        modelIndex, 1);
                    if (*(void **)(hitNode + offsetof(CritterHitNode, active)) != NULL) {
                        void *activeNode;

                        if (object >= 0) {
                            MBSetObject(*(void **)(hitNode + offsetof(CritterHitNode, active)), object);
                        }
                        activeNode = *(void **)(hitNode + offsetof(CritterHitNode, active));
                        for (i = 0; i < c->anodeCount; i++) {
                            u8 *anode;

                            anode = (u8 *)c->anodes + i * 0x28;
                            if (*(void **)anode == activeNode) {
                                s32 j;

                                *(s32 *)(anode + 0x20) = 0;
                                *(void **)((u8 *)c->anodes + i * 0x28) = NULL;
                                for (j = 0;
                                     j < c->hdr->moveCount;
                                     j++) {
                                    if ((c->hdr->movesPtr)[j].nodeidx ==
                                        i) {
                                        (c->hdr->movesPtr)[j].nodeidx =
                                            -1;
                                    }
                                }
                            }
                        }
                        if ((*(s16 *)(*(u8 **)hitNode +
                                      offsetof(CritterColDescriptor, flags)) & 4) &&
                            *(void **)((u8 *)*(void **)(hitNode + offsetof(CritterHitNode, active)) + offsetof(MBObject, child)) != NULL) {
                            CritterRemoveColnodeSub(c,
                                *(struct CritterColnode **)
                                    ((u8 *)*(void **)(hitNode + offsetof(CritterHitNode, active)) + offsetof(MBObject, child)), 2);
                        }
                    }
                    if (*(void **)(hitNode + offsetof(CritterHitNode, dmgfx)) != NULL) {
                        MBRemoveNode(*(void **)(hitNode + offsetof(CritterHitNode, dmgfx)), 1);
                        *(void **)(hitNode + offsetof(CritterHitNode, dmgfx)) = NULL;
                    }
                }
            }
            *(f32 *)(hitNode + offsetof(CritterHitNode, activeFrom)) += damage;
        }
        /* lint-end FM001, FM002 */
    }

    if ((f64)damage <= lbl_80346488) {
        return 0;
    }

    c->counterState |= flags;
    if (direction != NULL) {
        c->knockbackInput[0] += direction[0];
        c->knockbackInput[1] += direction[1];
        c->knockbackInput[2] += direction[2];
    }
    c->counterTime = sMusicFadeBase;
    if (hitPosition == NULL) {
        hitPosition = c->movevec;
    }
    c->health -= damage;

#define CRITTER_DIE(victim)                                                   \
    do {                                                                       \
        Critter *deathChild;                                                   \
        (victim)->state = 1;                                                   \
        CritterAwardExp(-1, (f32)(lbl_80346580 *                             \
                                  (f64)(victim)->hdr->expValue));            \
        if ((victim)->parent == NULL) {                                        \
            f32 deadHealth;                                                    \
            deathChild = (victim)->next;                                       \
            deadHealth = lbl_803464A8;                                         \
            for (; deathChild != NULL; deathChild = deathChild->next) {        \
                deathChild->health = deadHealth;                               \
            }                                                                  \
        }                                                                      \
        switch ((victim)->hdr->descriptor->type) {                            \
        case 4:                                                                \
            if ((victim)->parent == NULL) {                                    \
                BossDying();                                                   \
            }                                                                  \
            break;                                                             \
        }                                                                      \
    } while (0)

    if ((f64)c->health <= lbl_80346488) {
        if (c->state != 1) {
            CRITTER_DIE(c);
        }
        if (player >= 0) {
            playerData = &gPlayers[player];
            playerData = (Player *)((u8 *)playerData +
                                    playerData->character * 0x1C);
            (*(s32 *)((u8 *)playerData + 0xC10))++;
        }
        return 1;
    }

    if (c->parent != NULL) {
        c->parent->health -= damage;
        parent = c->parent;
        if ((f64)parent->health <= lbl_80346488) {
            if (parent->state != 1) {
                CRITTER_DIE(parent);
            }
            return 1;
        }
    } else if (c->childcnt > 0) {
        f64 childZero;
        f32 childOne;
        f64 childAwardScale;

        livingChildren = lbl_80346470;
        for (child = c->next; child != NULL; child = child->next) {
            if (child->state >= 2) {
                livingChildren = (f32)((f64)livingChildren +
                                       lbl_80346490);
            }
        }
        childZero = lbl_80346488;
        if ((f64)livingChildren > childZero) {
            childOne = lbl_803464A8;
            childAwardScale = lbl_80346580;
            childDamage = (f32)(lbl_803464F8 *
                                (f64)(damage / livingChildren));
            for (child = c->next; child != NULL; child = child->next) {
                if (child->state >= 2) {
                    child->health -= childDamage;
                    if ((f64)child->health <= childZero &&
                        child->state != 1) {
                        Critter *deathChild;

                        child->state = 1;
                        CritterAwardExp(-1,
                            (f32)(childAwardScale *
                                  (f64)child->hdr->expValue));
                        if (child->parent == NULL) {
                            for (deathChild = child->next;
                                 deathChild != NULL;
                                 deathChild = deathChild->next) {
                                deathChild->health = childOne;
                            }
                        }
                        switch (child->hdr->descriptor->type) {
                        case 4:
                            if (child->parent == NULL) {
                                BossDying();
                            }
                            break;
                        }
                    }
                }
            }
        }
    }

#undef CRITTER_DIE

    if ((flags & 0x01000000) == 0) {
        u8 *damageHeader;

        damageHeader = (u8 *)c->hdr;
        if ((flags & 0xF) == 0) {
            if (source == 2 && *(s16 *)(damageHeader + offsetof(CritterPackedType, sfxIndex1)) >= 0) {
                CritterDoSfx(c, *(s16 *)(damageHeader + offsetof(CritterPackedType, sfxIndex1)), hitPosition, 0,
                            -1);
            } else {
                CritterDoSfx(c, *(s16 *)(damageHeader + offsetof(CritterPackedType, sfxIndex0)), hitPosition, 0,
                            -1);
            }
        } else {
            fn_800945D0(hitPosition, &c->mtx[0][0], flags, 0,
                        critterClass, *(f32 *)(damageHeader + offsetof(CritterPackedType, radius)));
        }

        if (flags & 0x00100320) {
            c->unkABC = 2;
        } else if (c->unkAB8 >= 0) {
            c->hitnodes[c->unkAB8].state = 2;
        }
    }
    if (c->particle != NULL) {
        c->particle = NULL;
    }
    return 0;
}
#pragma dont_inline off
/* 0x80038D18 -- per-frame critter list step: reset per-player scratch, count
 * active players, then process every live critter, summing their results. */
s32 ProcessCritterList(void)
{
    Player *player;
    s32 activePlayers;
    s32 i;
    s32 total;

    activePlayers = 0;
    total = 0;
    player = gPlayers;
    lbl_80344664++;
    for (i = 0; i < 4; i++, player++) {
        if (player->state == 1) {
            activePlayers++;
        }
        gBig.scratch[i] = 0.0f;
    }
    lbl_8034465C = activePlayers;

    for (i = 0; i < lbl_8034466C; i++) {
        if (gCritterPool[i].hdr != NULL) {
            total += ProcessCritter(&gCritterPool[i]);
        }
    }
    return total;
}
#pragma dont_inline on
/* 0x80038DDC -- update one root critter and its child chain, including world
 * transforms, hit nodes, AI, animation, skin effects and render matrices. */
s32 ProcessCritter(Critter *c)
{
    s32 alive;
    s32 i;
    Critter *skinChild;
    Critter *child;
    CritterHitNode *node;
    CritterMove *move;
    s32 type;
    s32 allDead;
    s32 collided;
    f32 scale;
    f32 childHealth;
    f64 zero;
    /* lint-allow-next-line FM003: measured frame slot -- deleting it moves ProcessCritter by 7 words at unchanged size; original local unrecovered */
    u8 unused[8];

    if (c->parent != NULL) {
        return 0;
    }
    alive = 0;
    for (child = c->next; child != NULL; child = child->next) {
        if (child->health > 0.0f) {
            alive++;
        }
    }
    c->alivecnt = (s8)alive;

    GetWorldMat(c->mbnode, &c->mtx[0][0], NULL);
    c->movevec[0] = c->vel[0];
    c->movevec[1] = c->vel[1] + c->hdr->vertDrift;
    c->movevec[2] = c->vel[2];
    MulVec4Mat3((f32 *)((u8 *)c->hdr + 0xC0), c->pos, &c->mtx[0][0]);
    c->pos[0] = c->vel[0] + c->pos[0];
    c->pos[1] = c->vel[1] + c->pos[1];
    c->pos[2] = c->vel[2] + c->pos[2];

    for (i = 0; i < c->hdr->colCount; i++) {
        node = &c->hitnodes[i];
        if (node->active != NULL) {
            GetWorldMat(node->active, node->matrix,
                        node->descriptor->position);
        } else {
            CopyMat4(&c->mtx[0][0], node->matrix);
        }
    }
    CritterDoKnockback(c);
    CritterUpdateCounters(c);
    if (c->healthmtr >= 0) {
        HealthMeterUpdate(c->health, c->healthmtr);
    }
    if (c->damageflash != NULL) {
        scale = c->health /
                (c->hdr->maxHealth *
                 gCurLevel->ene_health);
        if ((f64)c->health <= lbl_80346488) {
            AtreeDelete(&c->healthbar[0]);
            c->damageflash = NULL;
        } else {
            MBTreeSetScale(scale, lbl_803464A8, lbl_803464A8,
                           c->damageflash);
        }
    }

    {
        Critter *current = c->next;
        zero = lbl_80346488;
        while (current != NULL) {
            CopyMat4(&c->mtx[0][0], &current->mtx[0][0]);
            current->movevec[0] = c->movevec[0];
            current->movevec[1] = c->movevec[1];
            current->movevec[2] = c->movevec[2];
            current->pos[0] = c->pos[0];
            current->pos[1] = c->pos[1];
            current->pos[2] = c->pos[2];
            if (current->obj_d0 != NULL) {
                GetWorldMat(current->obj_d0, current->worldMoveMatrix, NULL);
                current->vel[0] = current->moveOrigin[0];
                current->vel[1] = current->moveOrigin[1];
                current->vel[2] = current->moveOrigin[2];
            } else {
                CopyMat4(&c->mtx[0][0], current->worldMoveMatrix);
            }
            for (i = 0; i < current->hdr->colCount; i++) {
                node = &current->hitnodes[i];
                if (node->active != NULL) {
                    GetWorldMat(node->active, node->matrix,
                                node->descriptor->position);
                } else {
                    CopyMat4(&current->mtx[0][0], node->matrix);
                }
            }
            CritterUpdateCounters(current);
            if (current->healthmtr >= 0) {
                HealthMeterUpdate(current->health, current->healthmtr);
            }
            if (current->damageflash != NULL) {
                scale = current->health /
                        (current->hdr->maxHealth *
                         gCurLevel->ene_health);
                if ((f64)current->health <= zero) {
                    AtreeDelete(&current->healthbar[0]);
                    current->damageflash = NULL;
                } else {
                    MBTreeSetScale(scale, lbl_803464A8, lbl_803464A8,
                                   current->damageflash);
                }
            }
            current = current->next;
        }
    }

    if (c->state == 3) {
        allDead = 1;
        for (child = c->next; child != NULL; child = child->next) {
            if (allDead > 0 &&
                (childHealth = child->health) <= lbl_80346470) {
                allDead++;
            } else {
                allDead = 0;
            }
        }
        if (allDead > 1) {
            c->health = lbl_80346480;
        }
        if (c->health <= lbl_80346470 && c->state != 1) {
            c->state = 1;
            CritterAwardExp(
                -1, (f32)(lbl_80346580 *
                          (f64)c->hdr->expValue));
            if (c->parent == NULL) {
                child = c->next;
                scale = lbl_803464A8;
                while (child != NULL) {
                    child->health = scale;
                    child = child->next;
                }
            }
            type = c->hdr->descriptor->type;
            switch (type) {
            case 4:
                if (c->parent == NULL) {
                    BossDying();
                }
                break;
            }
        }
    }

    type = c->hdr->descriptor->type;
    switch (type) {
    case 3:
    case 8:
        goto golem_ai_3_8;
    case 4:
        goto boss_ai;
    case 7:
        goto golem_ai_7;
    case 0:
    case 1:
    case 2:
    case 5:
    case 6:
        goto animate_ai;
    }
    goto ai_done;

golem_ai_3_8:
    if (!CritterGolemAI(c)) {
        return 0;
    }
    goto ai_done;

golem_ai_7:
    if (!CritterGolemAI(c)) {
        return 0;
    }
    goto ai_done;

boss_ai:
    if (!CritterBossAI(c)) {
        return 0;
    }
    goto ai_done;

animate_ai:
    CritterAnimate(c);
    if (FloorCollide(c->vel, 0, 0, 2, lbl_803464B8,
                     lbl_80346588, lbl_8034658C) != NULL) {
        collided = 1;
    } else {
        collided = 0;
    }
    if (collided) {
        c->vel[1] =
            *(f32 *)(gFloorCollisionResult + 0x34) +
            c->hdr->floorOffset;
        if (c->shadow != NULL) {
            CopyMat3((f32 *)gFloorCollisionResult, (f32 *)c->shadow);
            ((MBObject *)c->shadow)->mat[3][0] = c->vel[0];
            ((MBObject *)c->shadow)->mat[3][1] = c->vel[1];
            ((MBObject *)c->shadow)->mat[3][2] = c->vel[2];
            ((MBObject *)c->shadow)->mat[3][1] =
                (f32)(lbl_803464B0 +
                      (f64)*(f32 *)(gFloorCollisionResult + 0x34));
        }
    }

ai_done:

    move = c->hdr->movesPtr;
    move += c->curmove;
    if ((move->flags & 8) != 0) {
        if ((c->anim->flags & 0x40) == 0) {
            MBTreeSetFlags(c->anim, 0x40, 1);
        }
    } else if ((c->anim->flags & 0x40) != 0) {
        MBTreeClearFlags(c->anim, 0x40, 1);
    }
    CritterUpdateSkinfx(c);
    skinChild = c->next;
    while (skinChild != NULL) {
        CritterUpdateSkinfx(skinChild);
        skinChild = skinChild->next;
    }
    if (c->hdr->descriptor->loadTick !=
        lbl_80344664) {
        if (c->hdr->descriptor->model != NULL) {
            DoTexMods(c->hdr->descriptor->model);
        }
        c->hdr->descriptor->loadTick =
            lbl_80344664;
    }
    CopyMat4(&c->mtx[0][0], (f32 *)c->mbnode);
    UnparentMatrix(c->mbnode, (f32 *)c->mbnode->parent);

    c->movevec[0] = c->vel[0];
    c->movevec[1] = c->vel[1] + c->hdr->vertDrift;
    c->movevec[2] = c->vel[2];
    MulVec4Mat3((f32 *)((u8 *)c->hdr + 0xC0), c->pos,
                &c->mtx[0][0]);
    c->pos[0] = c->vel[0] + c->pos[0];
    c->pos[1] = c->vel[1] + c->pos[1];
    c->pos[2] = c->vel[2] + c->pos[2];
    return 1;
}
#pragma dont_inline off
/* 0x8003946C -- consume a critter's pending knockback vector, applying the
 * damage-class scale and clamping the accumulated velocity. */
void CritterDoKnockback(Critter *c)
{
    s16 type;
    f32 scale;
    f32 lengthSquared;
    f64 clampScale;

    scale = 0.0f;
    type = c->hdr->descriptor->type;
    if (type == 4) {
        return;
    }

    if ((f64)c->health <= 0.0) {
        scale = lbl_80346590;
    } else if ((c->counterState & 0x10140) != 0) {
        scale = lbl_80346594;
    } else if ((c->counterState & 0x20) != 0) {
        scale = lbl_80346598;
    } else if ((c->counterState & 0x10) != 0) {
        scale = lbl_803464B8;
    }

    if (type == 3) {
        scale = (f32)((f64)scale - lbl_803465A0);
    }
    if ((f64)scale > 0.0) {
        c->knockbackVelocity[0] += c->knockbackInput[0] * scale;
        c->knockbackVelocity[1] += c->knockbackInput[1] * scale;
        c->knockbackVelocity[2] += c->knockbackInput[2] * scale;

        lengthSquared =
            c->knockbackVelocity[0] * c->knockbackVelocity[0] +
            c->knockbackVelocity[1] * c->knockbackVelocity[1] +
            c->knockbackVelocity[2] * c->knockbackVelocity[2];
        if ((f64)lengthSquared > lbl_803465A8) {
            NormalVector(c->knockbackVelocity);
            clampScale = lbl_803465B0;
            c->knockbackVelocity[0] =
                (f32)(clampScale * (f64)c->knockbackVelocity[0]);
            c->knockbackVelocity[1] =
                (f32)(clampScale * (f64)c->knockbackVelocity[1]);
            c->knockbackVelocity[2] =
                (f32)(clampScale * (f64)c->knockbackVelocity[2]);
        }

        c->knockbackInput[0] = 0.0f;
        c->knockbackInput[1] = 0.0f;
        c->knockbackInput[2] = 0.0f;
    }
}
/* 0x800395C8 -- expire the critter's transient move counter and both timed
 * counter pairs for each of its four effect slots. */
void CritterUpdateCounters(Critter *c)
{
    s32 i;
    s32 moveType;
    f32 *counterTime;
    u8 *base;
    f64 zero;
    f64 timeout;
    f32 clear;
    f32 current;

    moveType = *(s32 *)(*(u8 **)((u8 *)c->hdr + offsetof(CritterPackedType, movesPtr)) + c->curmove * 0x90);
    if ((((f64)c->counterTime > 0.0) &&
         ((f64)(sMusicFadeBase - c->counterTime) > 3.0)) ||
        moveType == 0x22 || (moveType >= 0x40 && moveType < 0x7F)) {
        c->counterValue = 0.0f;
        c->counterState = 0;
        c->counterTime = 0.0f;
    }

    zero = 0.0;
    timeout = 15.0;
    clear = 0.0f;
    for (i = 0; i < 4; i++) {
        base = (u8 *)c + i * 0x10;
        counterTime = (f32 *)(base + (offsetof(Critter, playerDamage[0].receivedTime)));
        current = *counterTime;
        if ((f64)current > zero &&
            (f64)(sMusicFadeBase - current) > timeout) {
            ((Critter *)base)->playerDamage[0].received = clear;
            *counterTime = clear;
        }
        current = *(counterTime = (f32 *)(base + (offsetof(Critter, playerDamage[0].dealtTime))));
        if ((f64)current > zero &&
            (f64)(sMusicFadeBase - current) > timeout) {
            *(f32 *)(base + (offsetof(Critter, playerDamage[0].dealt))) = clear;
            *counterTime = clear;
        }
    }
}
/* 0x800396A4 -- run the compact golem/general AI path. */
s32 CritterGolemAI(Critter *c)
{
    s32 mt;
    CritterMove *move0;
    CritterMove *move;
    CritterMove *nm;
    Critter *child;
    s32 i;
    f32 speed;
    f32 ratio;
    f32 best;
    s32 anim32;
    /* lint-allow-next-line FM003: measured frame slot -- deleting it moves CritterGolemAI by 7 words at unchanged size; original local unrecovered */
    u8 unused[8];

    CritterGetSingleTargetPlayer(c);
    CritterSetDifficulty(c);

    if (c->state == 0) {
        if (c->particle == NULL) {
            best = lbl_80346470;
            for (i = 0; i < c->targetCount; i++) {
                f32 v = c->targets[i].dist;
                if (v > best) {
                    best = v;
                }
            }
        }
        c->state = 3;
        for (child = c->next; child != NULL; child = child->next) {
            child->state = 3;
        }
        if (c->hdr->descriptor->type == 4) {
            BossActivate(c, 1);
        }
    }

    move0 = &(c->hdr->movesPtr)[
                c->curmove >= 0 ? c->curmove : 0];
    mt = -1;
    c->nextmove = mt;
    c->unk11E = mt;
    c->unk126 = mt;
    CritterGetDoAction(c);

    if (gTriggerCameraState != 0) {
        if (c->nextmove < 0) {
            if (c->rateScale < lbl_803465C0) {
                mt = CritterFindMoveType(c, MOVE_TAUNT, 0);
            }
            if (mt < 0) {
                mt = CritterFindMoveType(c, MOVE_READY, 1);
            }
            c->nextmove = (s16)mt;
        }
    } else if (lbl_803447DC == 0) {
        if (c->nextmove < 0) {
            CritterLookForCriticalMove(c);
        }
        if (c->nextmove < 0) {
            CritterChildCriticalMove(c);
        }
        if (c->nextmove < 0) {
            CritterLookForReady(c);
        }
        if (c->nextmove < 0) {
            mt = -1;
            if (c->rateScale < lbl_803465C0) {
                mt = CritterFindMoveType(c, MOVE_TAUNT, 0);
            }
            if (mt < 0) {
                mt = CritterFindMoveType(c, MOVE_READY, 1);
            }
            c->nextmove = (s16)mt;
        }
    }

    if (c->nextmove < 0) {
        c->nextmove = c->curmove;
    }

    nm = &(c->hdr->movesPtr)[c->nextmove];
    if (lbl_803447DC == 0 || c->curmove < 0 ||
        move0->type == MOVE_DEATH || nm->type == MOVE_DEATH ||
        move0->type == MOVE_START || nm->type == MOVE_START) {
        CritterAnimate(c);
    }

    if (c->curmove < 0) {
        c->curmove = 0;
    }
    anim32 = (s32)*(f32 *)((u8 *)c + 0x90);
    move = c->hdr->movesPtr;
    move += c->curmove;
    switch (move->type) {
    case MOVE_DEATH:
        if (AnimDone(c->sound)) {
            CritterDropItem(c);
            CritterDelInst(c);
            return 0;
        }
        {
            s32 dur = move->frameStart;
            if (dur > 0) {
                s32 elapsed = anim32 - dur;
                s32 total = *(s16 *)((u8 *)c + 0x88) - dur;
                if (elapsed > 0 && total > 0) {
                    MBTreeSetAlpha(c->anim, 255 - elapsed * 255 / total, 1);
                }
            }
        }
        break;
    }

    if (move->type == MOVE_DEATH || lbl_803447DC == 0) {
        CritterMoveSetup(c, move);
        CritterActivate(c, move, anim32);
        if (!CritterTranslate(c, move)) {
            CritterRotate(c, move);
        }
        CritterLookAtPlayer(c, move);
    }

    if (lbl_80346490 != lbl_803447D8) {
        if (c->mbnode != NULL) {
            MBTreeSetScale(lbl_803447D8, lbl_803447D8, lbl_803447D8, c->mbnode);
        }
        if (c->shadow != NULL) {
            MBTreeSetScale(lbl_803447D8, lbl_803447D8, lbl_803447D8, c->shadow);
        }
    } else {
        if (c->mbnode != NULL) {
            MBTreeClearFlags(c->mbnode, 8, 0);
        }
        if (c->shadow != NULL) {
            MBTreeClearFlags(c->shadow, 8, 0);
        }
    }
    return 1;
}

/* 0x80039AD8 -- run boss target distribution, pattern selection and the
 * coordinated root/child animation pass. */
s32 CritterBossAI(Critter *c)
{
    char moveName[12];
    /* lint-allow-next-line FM003: measured frame slot -- deleting it moves CritterBossAI by 41 words at unchanged size; original local unrecovered */
    u8 unused[40];
    Critter *child;
    CritterMove *move;
    CritterMove *childMove;
    u8 *header;
    u8 *surface;
    f32 best;
    f32 duration;
    f32 angle;
    f32 distance;
    f32 dot;
    f32 displayScale;
    f64 angleLimit;
    f64 dotMinimum;
    f64 angleScale;
    f64 radianScale;
    f64 half;
    f64 frameHalf;
    f64 rateThreshold;
    s32 frame;
    s32 childFrame;
    s32 moveIndex;
    s32 moveType;
    s32 selected;
    s32 linkedChildren;
    s32 done;
    s32 floorHit;
    s32 i;
    s32 y;
    f64 one;

    CritterGetTargetPlayers(c);
    CritterSetDifficulty(c);

    for (child = c->next; child != NULL; child = child->next) {
        CritterGetTargetPlayers(child);
        CritterSetDifficulty(child);
    }

    CritterResolveMultipleTargets(c);
    CritterProcessSafeRocks();

    if (c->state == 0) {
        if ((f64)lbl_8034464C == 0.0) {
            if ((c->hdr->typeFlags & 0x80) == 0) {
                lbl_8034464C = (f32)(2.0 + (f64)sMusicFadeBase);
            }
        } else if ((f64)sMusicFadeBase >= (f64)lbl_8034464C) {
            distance = c->hdr->wakeThreshold;
            best = 0.0f;
            for (i = 0; i < c->targetCount; i++) {
                f32 candidate = c->targets[i].dist;
                if (candidate > best) {
                    best = candidate;
                }
            }
            if ((f64)best <= 0.0) {
                best = 1e21f;
            }
            if ((f64)distance <= 0.0 || best < distance) {
                c->state = 3;
                for (child = c->next; child != NULL; child = child->next) {
                    child->state = 3;
                }
                if (c->hdr->descriptor->type == 4) {
                    BossActivate(c, 1);
                }
            }
        }
    }

    if (c->unk128 >= 0) {
        camera_request_change(30, 2);
    }

    moveIndex = c->curmove >= 0 ? c->curmove : 0;
    header = (u8 *)c->hdr;
    move = (CritterMove *)(*(u8 **)(header + offsetof(CritterPackedType, movesPtr)) + moveIndex * 0x90);

    if ((gControllerButtons & 0x80) != 0) {
        CritterGetNextMove(c);
    } else {
        c->nextmove = -1;
        c->unk11E = -1;
        c->unk126 = -1;
        CritterGetDoAction(c);
        if (c->nextmove < 0) {
            CritterLookForCriticalMove(c);
        }
        if (c->nextmove < 0) {
            CritterChildCriticalMove(c);
            child = c->next;
            if (child != NULL) {
                rateThreshold = 0.8;
                linkedChildren = 0;
                for (; child != NULL; child = child->next) {
                    child->nextmove = -1;
                    child->unk11E = -1;
                    child->unk126 = -1;
                    if (c->unk11C >= 0) {
                        if (c->unk11C < child->hdr->auxMoveCount) {
                            child->unk11C = c->unk11C;
                            child->unk120 = c->unk120;
                            child->nextmove =
                                child->hdr->patternsPtr[child->unk11C]
                                    .moveidx[child->unk120];
                        }
                    } else if (c->nextmove < 0) {
                        CritterChildGetPattern(child);
                        if (child->nextmove < 0) {
                            CritterChildCriticalMove(child);
                        }
                        selected = child->nextmove;
                        if ((s16)selected < 0) {
                            selected = -1;
                            if ((f64)child->rateScale < rateThreshold) {
                                selected = CritterFindMoveType(child, MOVE_TAUNT, 0);
                            }
                            if (selected < 0) {
                                selected = CritterFindMoveType(child, MOVE_READY, 1);
                            }
                            child->nextmove = (s16)selected;
                        } else if (child->unk11C >= 0) {
                            linkedChildren++;
                        } else {
                            childMove = (CritterMove *)(
                                *(u8 **)((u8 *)child->hdr + offsetof(CritterPackedType, movesPtr)) +
                                selected * 0x90);
                            if (childMove->type >= MOVE_ATTACKS) {
                                linkedChildren++;
                            }
                        }
                    }
                }
                if (linkedChildren != 0) {
                    c->nextmove = (s16)CritterFindMoveType(c, 1, 1);
                }
            }
        }
        if (c->nextmove < 0 && move->type < MOVE_ATTACKS) {
            CritterLookForReady(c);
        }
        if (c->nextmove < 0) {
            selected = -1;
            if ((f64)c->rateScale < 0.8) {
                selected = CritterFindMoveType(c, MOVE_TAUNT, 0);
            }
            if (selected < 0) {
                selected = CritterFindMoveType(c, MOVE_READY, 1);
            }
            c->nextmove = (s16)selected;
        }
    }

    CritterAnimate(c);
    if (c->curmove < 0) {
        c->curmove = 0;
    }
    move = (CritterMove *)(*(u8 **)((u8 *)c->hdr + offsetof(CritterPackedType, movesPtr)) +
                           c->curmove * 0x90);
    frameHalf = 0.5;
    for (child = c->next; child != NULL; child = child->next) {
        if (child->state == 1) {
            child->nextmove = (s16)CritterFindMoveType(child, MOVE_DEATH, 1);
            CritterAnimate(child);
        } else if ((move->type == 1 || c->unk11C >= 0) &&
                   (lbl_8034489C < 2 || lbl_8034489C > 3) &&
                   (child->curmove >= 0 || child->nextmove >= 0)) {
            CritterAnimate(child);
        } else {
            DoAnimateTreeFrame(
                (u8 *)child + 0x74, *(s16 *)&c->sound[0x0E],
                (s32)(frameHalf +
                      (f64)*(f32 *)&c->sound[0x18]),
                1);
            child->movedone = c->movedone;
            child->curmove = -1;
            child->unk11C = -1;
            child->unk120 = -1;
            if (move->type == 1) {
                child->nextmove = 0;
            }
        }
    }

    frame = (s32)*(f32 *)&c->sound[0x18];
    duration = move->holdDuration;
    done = AnimDone(c->sound);
    if ((f64)duration > 0.0) {
        switch (move->type) {
        case MOVE_DEATH:
            if (done == 0) {
                c->rate = sMusicFadeBase + duration;
            } else if ((f64)sMusicFadeBase >= (f64)c->rate &&
                       (gControllerButtons & 0x80) == 0) {
                CritterDelInst(c);
                return 0;
            } else if ((gControllerButtons & 0x80) == 0) {
                f32 remaining = c->rate - sMusicFadeBase;
                gBossDead = 1;
                if ((f64)remaining < 0.5) {
                    s32 fade = 255 -
                        (s32)(2.0 * (255.0 * remaining));
                    MBTreeSetAlpha(c->anim, fade, 1);
                }
            }
            break;
        default:
            if (done == 0 || (f64)c->rate == 0.0) {
                c->rate = sMusicFadeBase + duration;
            }
            break;
        }
    } else {
        c->rate = 0.0f;
    }

    moveType = move->type;
    switch (moveType) {
    case MOVE_START:
        if (move->link < 0 && done != 0 && lbl_8034489C == 1) {
            lbl_8034489C = 2;
        }
        break;
    case MOVE_ROAR:
        if (done != 0 && lbl_8034489C == 3) {
            lbl_8034489C = 4;
        }
        break;
    }
    if (lbl_8034489C < 4) {
        if (lbl_8034489C >= 2) {
            c->unkABE = 0xFF;
        }
    }

    CritterMoveSetup(c, move);
    CritterActivate(c, move, frame);
    CritterTranslate(c, move);
    CritterRotate(c, move);
    CritterLookAtPlayer(c, move);

    for (child = c->next; child != NULL; child = child->next) {
        childMove = NULL;
        if (child->curmove >= 0) {
            childFrame = (s32)*(f32 *)((u8 *)child + 0x90);
            childMove = (CritterMove *)(
                *(u8 **)((u8 *)child->hdr + offsetof(CritterPackedType, movesPtr)) + child->curmove * 0x90);
            CritterMoveSetup(child, childMove);
            CritterActivate(child, childMove, childFrame);
            CritterTranslate(child, childMove);
            CritterRotate(child, childMove);
        }
        CritterLookAtPlayer(child, childMove);
    }

    floorHit = FloorCollide(c->vel, 0, 0, 2, 5.0f,
                            4.0f, -1000.0f) != NULL
                   ? 1
                   : 0;
    if (floorHit != 0) {
        c->vel[1] = *(f32 *)(gFloorCollisionResult + 0x34) +
                    c->hdr->floorOffset;
        if (c->state == 0 && (f64)lbl_8034464C == 0.0 &&
            (c->hdr->typeFlags & 0x80) != 0) {
            s32 surfaceFlags = 0;
            surface = *(u8 **)(gFloorCollisionResult + 0x44);
            if (surface != NULL) {
                surfaceFlags = (s8)surface[0x16];
                if (*(u8 **)(surface + 0x18) != NULL) {
                    surfaceFlags |= (s8)(*(u8 **)(surface + 0x18))[0x16];
                }
            }
            if ((surfaceFlags & 0x10) != 0) {
                lbl_8034464C = (f32)(2.0 + (f64)sMusicFadeBase);
                BossActivate(c, 0);
            }
        }
        if (c->shadow != NULL) {
            CopyMat3((f32 *)gFloorCollisionResult, (f32 *)c->shadow);
            ((MBObject *)c->shadow)->mat[3][0] = c->vel[0];
            ((MBObject *)c->shadow)->mat[3][1] = c->vel[1];
            ((MBObject *)c->shadow)->mat[3][2] = c->vel[2];
            ((MBObject *)c->shadow)->mat[3][1] =
                (f32)(0.1 +
                      (f64)*(f32 *)(gFloorCollisionResult + 0x34));
        }
    }

    if ((gControllerButtons & 0x10) != 0 && gGameOptions[8] != 0) {
        distance = -1.0f;
        angle = distance;
        if (c->targetCount > 0) {
            angle = c->targets[0].dist;
            if ((f64)angle >= 1e21) {
                angle = distance;
            }
            dot = *(f32 *)((u8 *)c + 0x130);
            distance = (f32)(0.31830988614222805 *
                             (180.0 *
                              (f64)acosf((f32)(
                                  (f64)dot < -1.0
                                      ? -1.0
                                      : ((f64)dot > 1.0
                                             ? 1.0
                                             : (f64)dot)))));
        }
        if (c->unk11C >= 0) {
            sprintf(moveName, lbl_803465E0, c->unk11C);
        } else {
            strcpy(moveName, lbl_803465E4);
        }
        DrawText(8, 214, 0, 0xFFFFFF, lbl_80112104, moveName,
                 (u8 *)move + 0x10, (s32)c->health,
                 (s32)(10.0f * c->rateScale),
                 (s32)(0.5 + *(f32 *)&c->sound[0x18]),
                 c->unk124, (s32)(0.5 + angle),
                 (s32)(0.5 + distance));

        c = c->next;
        one = 1.0;
        angleLimit = 1e21;
        dotMinimum = -1.0;
        angleScale = 0.31830988614222805;
        radianScale = 180.0;
        displayScale = 10.0f;
        half = 0.5;
        i = 0;
        y = 224;
        for (; c != NULL; c = c->next, y += 10, i++) {
            distance = -1.0f;
            angle = distance;
            if (c->targetCount > 0) {
                angle = c->targets[0].dist;
                if ((f64)angle >= angleLimit) {
                    angle = distance;
                }
                dot = *(f32 *)((u8 *)c + 0x130);
                distance = (f32)(angleScale *
                                 (radianScale *
                                  (f64)acosf((f32)(
                                      (f64)dot < dotMinimum
                                          ? dotMinimum
                                          : ((f64)dot > one
                                                 ? one
                                                 : (f64)dot)))));
            }
            if (c->unk11C >= 0) {
                sprintf(moveName, lbl_803465E0, c->unk11C);
            } else {
                strcpy(moveName, lbl_803465E4);
            }
            childFrame = -1;
            if (c->curmove >= 0) {
                childFrame = (s32)*(f32 *)&c->sound[0x18];
            }
            DrawText(8, y, 0, 0xFFFFFF, lbl_8011213C, i,
                     moveName,
                     c->curmove >= 0
                         ? (char *)(*(u8 **)((u8 *)c->hdr + offsetof(CritterPackedType, movesPtr)) +
                                    c->curmove * 0x90 + 0x10)
                         : (char *)lbl_803465E8,
                     (s32)c->health,
                     (s32)(displayScale * c->rateScale), childFrame,
                     c->unk124, (s32)(half + angle),
                     (s32)(half + distance));
        }
    }
    return 1;
}
/* 0x8003A73C -- collect the level's safe rocks once, then count down each
 * reactivation timer and restore the corresponding item when it expires. */
void CritterProcessSafeRocks(void)
{
    CritterBigState *big;
    s32 i;
    s32 count;

    big = &gBig;
    if (lbl_80344658 == 0) {
        lbl_80344658 =
            CollectSafeRocks(big->safeRockIndices, 16, lbl_80344650);
        count = lbl_80344658;
        for (i = 0; i < count; i++) {
            gBig.safeRockTimers[i] = 0.0f;
        }
        if (count <= 0) {
            lbl_80344658 = -1;
        } else {
            lbl_80344654 = RandInt(count);
        }
    } else if (lbl_80344658 > 0) {
        for (i = 0; i < lbl_80344658; i++) {
            f32 *timer = (f32 *)&gBig + i;
            if ((f64)*(timer += 4) > 0.0) {
                *timer -= gClockFrameStep;
                if ((f64)*timer <= 0.0) {
                    SafeRockActivate(gBig.safeRockIndices[i]);
                }
            }
        }
    }
}
/* 0x8003A838 -- release or spawn the item carried by a critter and place it
 * at the cached floor contact point. */
void CritterDropItem(Critter *c)
{
    void *item;
    char name[32];
    s32 type;

    item = NULL;
    type = 0;
    if (*(void **)((u8 *)c + offsetof(Critter, _blkACC)) != NULL) {
        item = *(void **)((u8 *)c + offsetof(Critter, _blkACC));
        *(void **)c->_blkACC = NULL;
        type = 1;
    } else {
        switch (c->hdr->descriptor->type) {
        case 7: {
            char *p;

            sprintf(name, &lbl_803465EC, fn_80057ACC(0x20));
            p = name;
            while (*p != '\0') {
                *p = (char)toupper(*p);
                p++;
            }
            item = PlaceItem(1, 0x10, name, NULL);
            type = 2;
            break;
        }
        }
    }
    if (item == NULL) {
        return;
    }
    switch (c->hdr->descriptor->type) {
    case 8:
        msgPost(0x86, -1, 0);
        break;
    case 7:
        msgPost(0x8A, -1, 0);
        break;
    }
    if (type != 0) {
        ((CritterItemView *)item)->minoff = 10;
        StartBagFX(c->floorContact, item,
                    lbl_80346470);
        return;
    }

    ((CritterItemView *)item)->minoff = 0;
    MBTreeClearFlags(*(void **)((u8 *)item + offsetof(CritterItemView, node)),
                      2, 0);
    if (**(s32 **)((u8 *)item + offsetof(CritterItemView, info)) == 1) {
        *(s16 *)((u8 *)item + 0xEC) = 60;
    }
    ((CritterItemView *)item)->pos[0] =
        c->floorContact[0];
    ((CritterItemView *)item)->pos[1] =
        c->floorContact[1];
    ((CritterItemView *)item)->pos[2] =
        c->floorContact[2];
    AddItemSub(item);
}

/* 0x8003A9C4 -- integrate scripted translation and knockback, then clamp the
 * result through world and critter collision. */
s32 CritterTranslate(Critter *c, CritterMove *move)
{
    /* lint-allow-next-line FM003: measured frame slot -- deleting it moves CritterTranslate by 7 words at unchanged size; original local unrecovered */
    u8 pad16[16];
    f32 delta[3];
    f32 t0;
    f32 t1;
    f32 t2;
    /* lint-allow-next-line FM003: measured frame slot -- deleting it moves CritterTranslate by 71 words at unchanged size; original local unrecovered */
    u8 pad4[4];
    f32 contact[3];
    f32 dest[3];
    register f32 zero;
    f32 speed;
    f32 spd;
    f32 dx;
    f32 dz;
    f32 fx;
    f32 fz;
    f32 ax;
    f32 az;
    f32 bz;
    f32 bx;
    f32 tx;
    f32 tz;
    f32 gz;
    f64 kdamp;
    f32 nx;
    f32 ny;
    f32 nz;
    f32 dist;
    f32 scale;
    s32 result = 0;
    s32 hits;
    s32 pr;
    s32 tmpr;
    f32 rad;
    /* lint-allow-next-line FM003: measured frame slot -- deleting it moves CritterTranslate by 76 words at unchanged size; original local unrecovered */
    u8 pad24[24];

    speed = c->hdr->speed;
    spd = gCurLevel->ene_speed *
          (move->readyDistance * gClockFrameStep);
    if (speed <= 0.0f) {
        return 0;
    }
    if (move->type == MOVE_STEPTOWARD) {
        delta[0] = c->targetPos[0] - c->vel[0];
        delta[1] = c->targetPos[1] - c->vel[1];
        delta[2] = c->targetPos[2] - c->vel[2];
        tx = delta[0];
        tz = delta[2];
        dist = fqdist(tx, tz);
        if (dist > lbl_80346488) {
            scale = lbl_80346490 / dist;
            tx *= scale;
            tz *= scale;
        }
        dx = tx * spd;
        dz = tz * spd;
    } else {
        if ((c->hdr->typeFlags & 0x40) != 0) {
            ax = *(f32 *)((u8 *)c + 1016);
            az = *(f32 *)((u8 *)c + 1024);
            dist = fqdist(ax, az);
            if (dist > lbl_80346488) {
                scale = lbl_80346490 / dist;
                ax *= scale;
                az *= scale;
            }
            fx = ax;
            fz = az;
        } else {
            bx = c->mtx[2][0];
            bz = c->mtx[2][2];
            dist = fqdist(bx, bz);
            if (dist > lbl_80346488) {
                scale = lbl_80346490 / dist;
                bx *= scale;
                bz *= scale;
            }
            fx = bx;
            fz = bz;
        }
        switch (move->type) {
        default:
            dx = spd * fx;
            dz = spd * fz;
            break;
        case MOVE_STEPB:
            dx = -spd * fx;
            dz = -spd * fz;
            break;
        case MOVE_STEPL:
            dz = spd * fx;
            dx = -spd * fz;
            break;
        case MOVE_STEPR:
            dx = spd * fz;
            dz = -spd * fx;
            break;
        case MOVE_STEPFL:
            dx = spd * fx;
            dz = spd * fz;
            dz = dz + dx;
            dx = -spd * fz + dx;
            break;
        case MOVE_STEPFR:
            dx = spd * fx;
            dz = spd * fz;
            break;
        }
    }
    delta[0] = dx;
    zero = lbl_80346470;
    delta[1] = zero;
    delta[2] = dz;
    delta[0] += c->knockbackVelocity[0] * gClockFrameStep;
    delta[1] += c->knockbackVelocity[1] * gClockFrameStep;
    delta[2] += c->knockbackVelocity[2] * gClockFrameStep;
    kdamp = lbl_803465C0;
    nx = c->vel[0] + delta[0];
    ny = c->vel[1] + delta[1];
    nz = c->vel[2] + delta[2];
    c->knockbackVelocity[0] = (f32)(kdamp * c->knockbackVelocity[0]);
    c->knockbackVelocity[1] = (f32)(kdamp * c->knockbackVelocity[1]);
    c->knockbackVelocity[2] = (f32)(kdamp * c->knockbackVelocity[2]);
    t0 = c->knockbackVelocity[0];
    *(u32 *)&t0 &= 0x7FFFFFFF;
    if (t0 < lbl_80346540) {
        c->knockbackVelocity[0] = zero;
    }
    t1 = c->knockbackVelocity[1];
    *(u32 *)&t1 &= 0x7FFFFFFF;
    if (t1 < lbl_80346540) {
        c->knockbackVelocity[1] = lbl_80346470;
    }
    t2 = c->knockbackVelocity[2];
    *(u32 *)&t2 &= 0x7FFFFFFF;
    if (t2 < lbl_80346540) {
        c->knockbackVelocity[2] = lbl_80346470;
    }
    gz = lbl_80346470;
    if (c->knockbackVelocity[1] > gz) {
        c->knockbackVelocity[1] =
            c->knockbackVelocity[1] - lbl_803465F4 * gClockFrameStep;
        if (c->knockbackVelocity[1] < gz) {
            c->knockbackVelocity[1] = gz;
        }
    }
    if (c->hdr->descriptor->type == 4) {
        delta[0] = nx - c->movePathPos[0];
        delta[1] = ny - c->movePathPos[1];
        delta[2] = nz - c->movePathPos[2];
        if (c->state != 1) {
            dist = fqdist(delta[0], delta[2]);
            c->unk4AC = dist;
            if ((c->hdr->typeFlags & 0x20) != 0) {
                if (delta[0] > speed) {
                    delta[0] = speed;
                }
                if (delta[0] < -speed) {
                    delta[0] = -speed;
                }
                if (delta[2] > speed) {
                    delta[2] = speed;
                }
                if (delta[2] < -speed) {
                    delta[2] = -speed;
                }
            } else if (dist > speed) {
                scale = speed / dist;
                delta[0] *= scale;
                delta[2] *= scale;
            }
        }
        c->vel[0] = c->movePathPos[0] + delta[0];
        c->vel[1] = c->movePathPos[1] + delta[1];
        c->vel[2] = c->movePathPos[2] + delta[2];
    } else {
        delta[0] = nx - c->vel[0];
        delta[1] = ny - c->vel[1];
        delta[2] = nz - c->vel[2];
        hits = CritterCollideWorld(c, delta, 0);
        hits += CritterCollideItems(c, delta, hits);
        tmpr = CritterCollidePlayers(c, delta, hits);
        hits = hits + tmpr;
        pr = tmpr;
        CritterCollideEnemies(c, delta, hits);
        rad = c->hdr->wallRadius;
        dest[0] = c->pos[0] + delta[0];
        dest[1] = c->pos[1] + delta[1];
        dest[2] = c->pos[2] + delta[2];
        lbl_80344644 = 0;
        lbl_80344648 = c;
        if (CritterMoveNodeCol(rad, lbl_80346470,
                               &c->pos[0], dest, contact, -1, 1) != NULL) {
            delta[2] = lbl_80346470;
            delta[0] = lbl_80346470;
        }
        c->vel[0] = c->vel[0] + delta[0];
        c->vel[1] = c->vel[1] + delta[1];
        c->vel[2] = c->vel[2] + delta[2];
        if (pr != 0) {
            result = 1;
        }
    }
    return result;
}
/* 0x8003AF4C -- turn the critter toward the move-selected player, waypoint
 * or explicit facing, clamping angular speed by the move and header rates. */
void CritterRotate(Critter *c, CritterMove *move)
{
    f32 delta;
    f32 turn;
    f32 limit;
    /* lint-allow-next-line FM003: measured frame slot -- deleting it moves CritterRotate by 7 words at unchanged size; original local unrecovered */
    volatile f64 highPad;
    f32 target[3];
    /* lint-allow-next-line FM003: measured frame slot -- deleting it moves CritterRotate by 23 words at unchanged size; original local unrecovered */
    u8 unused[8];

    turn = move->turnRate;
    limit = c->hdr->turnLimit;
    if ((move->flags & 0x20) != 0) {
        delta = c->inityaw - c->curyaw;
    } else if (c->unk128 >= 0) {
        delta = lbl_80346470;
    } else {
        if (c->unk124 >= 0 && (f64)turn > 0.0) {
            GetPlayerColPos(c->unk124, target);
            target[0] -= c->vel[0];
            target[1] -= c->vel[1];
            target[2] -= c->vel[2];
            if ((c->hdr->typeFlags & 0x400) != 0) {
                register f32 z = target[2];
                delta = atan2(target[0], z) - c->curyaw;
            } else {
                register f32 z = target[2];
                f32 angle = atan2(target[0], z) - c->inityaw;
                f64 wideAngle;
                f32 clamped;
                if ((f64)angle > 3.141592654) {
                    wideAngle = (f64)angle - 6.283185308;
                } else if ((f64)angle <= -3.141592654) {
                    wideAngle = 6.283185308 + (f64)angle;
                } else {
                    wideAngle = angle;
                }
                angle = (f32)wideAngle;
                clamped = angle < -limit ? -limit :
                          angle > limit ? limit : angle;
                delta = (c->inityaw + clamped) -
                        c->curyaw;
            }
        } else if (c->particle != NULL && c->targetCount == 0) {
            target[0] = ((OBJGRP *)c->particle)->worldmat[3][0] - c->vel[0];
            target[1] = ((OBJGRP *)c->particle)->worldmat[3][1] - c->vel[1];
            target[2] = ((OBJGRP *)c->particle)->worldmat[3][2] - c->vel[2];
            {
                register f32 z = target[2];
                delta = atan2(target[0], z) - c->curyaw;
            }
        } else {
            delta = lbl_80346470;
        }
    }

    if ((f64)delta != 0.0) {
        f64 wideDelta = delta;
        f32 wrapped;
        f32 turnStep;
        if (wideDelta > 3.141592654) {
            wideDelta -= 6.283185308;
        } else if (wideDelta <= -3.141592654) {
            wideDelta = 6.283185308 + wideDelta;
        }
        wrapped = (f32)wideDelta;
        turnStep = turn * gClockFrameStep;
        if (c->unkAC6 > 0) {
            turnStep *= 0.1;
        }
        if (wrapped < -turnStep) {
            delta = -turnStep;
        } else {
            if (wrapped > turnStep) {
                wrapped = turnStep;
            }
            delta = wrapped;
        }
        c->curyaw =
            c->curyaw + delta;
        CopyMat3((f32 *)gIdentityMatrix, &c->mtx[0][0]);
        YawMat3(*(f32 *)((u32)c + 0xFC), &c->mtx[0][0]);
    }
}
/* 0x8003B1CC -- select the critter's current target/node and refresh the
 * world-space movement matrix used by the active move. */
/* sCritterMoveNode: the animation node a move drives -- the move's own
 * anode when it has one, else the critter's default anim node. */
static inline void *sCritterMoveNode(Critter *c, s32 nodeIndex)
{
    void *node;

    node = c->anim;
    if (nodeIndex < 0) {
        return node;
    }
    {
        void *candidate = ((void **)((u8 *)c->anodes + nodeIndex * 0x28))[0];

        if (candidate == NULL) {
            candidate = node;
        }
        return candidate;
    }
}

s32 CritterMoveSetup(Critter *c, CritterMove *move)
{
    f32 *target;
    s16 currentMove;
    s16 queuedTarget;

    target = NULL;
    currentMove = c->curmove;
    if (currentMove >= 0) {
        target = (f32 *)(*(u8 **)((u8 *)c->hdr + offsetof(CritterPackedType, movesPtr)) +
                         currentMove * sizeof(CritterMove) + 0x60);
    }

    if (c->unk124 < 0 || c->movedone != 0) {
        if (c->unkAC6 > 0) {
            c->unk124 = -1;
        } else if ((queuedTarget = c->unk126) >= 0) {
            c->unk124 = queuedTarget;
        } else {
            c->unk124 = CritterGetTargetSub(c, target, 1);
        }
    }

    if (c->movedone != 0) {
        c->moveFlags = 0;
        c->moveSfxFlags = 0;
        if (c->emitter != NULL) {
            MBRemoveNode(c->emitter, 1);
            c->emitter = NULL;
        }

        c->obj_d0 = sCritterMoveNode(c, move->nodeidx);
    }

    c->moveMatrix[0] = c->moveOrigin[0];
    c->moveMatrix[1] = c->moveOrigin[1];
    c->moveMatrix[2] = c->moveOrigin[2];
    return GetWorldMat(c->obj_d0, c->worldMoveMatrix, NULL);
}
/* 0x8003B300 -- activate frame-gated move actions, sounds and particles. */
void CritterActivate(Critter *c, CritterMove *move, s32 frame)
{
    u32 events;
    s16 oldFlags;
    u8 *entry;

    if (c->emitter != NULL) {
        DmgFxNodeUpdate(c->emitter, 0, 0.0f, 0.0f, 0.0f, 0.0f);
    }
    if (move->frameStart >= 0) {
        events = CritterCopyAnim(c, move, frame);
        oldFlags = c->moveFlags;
        if ((events & 1) != 0) {
            if (move->type != MOVE_SPRAY) {
                c->moveFlags = oldFlags | 1;
            }
            if (move->interruptAnim0 >= 0) {
                CritterAnimInterrupt(c, move->interruptAnim0, 1,
                                     !(oldFlags & 1));
            }
        }
        if ((events & 2) != 0) {
            if (move->type != MOVE_SPRAY) {
                c->moveFlags |= 2;
            }
            if (move->interruptAnim1 >= 0) {
                CritterAnimInterrupt(c, move->interruptAnim1, 2,
                                     !(oldFlags & 2));
            }
        }
    }
    if (move->interruptAnim0 >= 0) {
        entry = *(u8 **)(*(u8 **)((u8 *)c->hdr + offsetof(CritterPackedType,
                          file)) + offsetof(CritterFileHeader, damage)) +
                move->interruptAnim0 * 0x50;
        if ((*(s16 *)(entry + 2) & 0x4000) && c->unkAC8 > lbl_80346488 &&
            *(s16 *)(entry + 0) != 1) {
            return;
        }
    }
    if ((c->moveSfxFlags & 1) == 0 && move->sfx >= 0 && frame >= move->sfxFrame) {
        c->moveSfxFlags |= 1;
        CritterDoSfx(c, move->sfx, NULL, 1, -1);
    }
    if ((c->moveSfxFlags & 2) == 0 && move->sfx2 >= 0 && frame >= move->sfx2Frame) {
        c->moveSfxFlags |= 2;
        CritterDoSfx(c, move->sfx2, NULL, 1, -1);
    }
}

/* 0x8003B4CC -- follow an explicit move link or advance to the next legal
 * move while skipping pattern-marker entries. */
void CritterGetNextMove(Critter *c)
{
    s16 count;
    CritterMove *moves;
    CritterMove *move;
    Critter *child;
    s16 linked;
    s32 childrenDone;

    moves = c->hdr->movesPtr;
    count = c->hdr->moveCount;
    move = &moves[c->curmove];
    if (c->curmove < 0) {
        c->nextmove = 0;
        return;
    }
    linked = move->link;
    if (linked >= 0) {
        c->nextmove = linked;
        return;
    }

    if (move->type == 1) {
        childrenDone = 1;
        for (child = c->next; child != NULL; child = child->next) {
            if (child->curmove >= 0 || child->nextmove >= 0) {
                child->nextmove = child->curmove + 1;
                if (child->nextmove < child->hdr->moveCount) {
                    childrenDone = 0;
                } else {
                    child->nextmove = -1;
                    if (child->curmove >= 0) {
                        childrenDone = 0;
                    }
                }
            }
        }
        if (!childrenDone) {
            c->nextmove = c->curmove;
            return;
        }
    }

    c->nextmove = c->curmove + 1;
    for (;;) {
        move = &moves[c->nextmove];
        if (c->nextmove >= count) {
            c->nextmove = 0;
            continue;
        }
        if (move->type == MOVE_FINISH) {
            c->nextmove++;
            continue;
        }
        if (move->link == c->curmove) {
            c->nextmove++;
            continue;
        }
        break;
    }
    for (child = c->next; child != NULL; child = child->next) {
        child->nextmove = -1;
    }
    if (c->curmove >= 0 && moves[c->curmove].type == 0x11) {
        MBTreeClearFlags(c->anim, 1, 0);
        MBTreeClearFlags(c->anim->child, 2, 2);
    }
}
/* 0x8003B67C -- choose the closest ready move in the 0x30..0x39 family. */
void CritterLookForReady(Critter *c)
{
    s32 timeOffset;
    s32 moveOffset;
    s32 i;
    CritterMove *moves;
    s32 result;
    s32 moveCount;
    CritterMove *move;
    s32 type;
    f64 zeroDouble;
    f32 zeroFloat;
    f32 best;
    f32 distance;

    moveCount = c->hdr->moveCount;
    moves = c->hdr->movesPtr;
    result = -1;
    best = lbl_803464C0;

    if ((c->hdr->typeFlags & 0x10000) == 0) {
        return;
    }
    if (CritterGetTarget(c, c->targetPos) == 0) {
        return;
    }

    zeroFloat = lbl_80346470;
    i = 0;
    zeroDouble = lbl_80346488;
    timeOffset = 0;
    moveOffset = 0;
    while (i < moveCount) {
        move = (CritterMove *)((u8 *)moves + moveOffset);
        type = move->type;
        if (type < 0x30 || type > 0x39) {
            goto next;
        }
        if (type == 0x38 && c->unk124 < 0) {
            goto next;
        }
        if ((move->flags & 4) != 0) {
            goto next;
        }

        if (c->targetCount == 0 && c->particle != NULL) {
            if (c->targetCount == 0 &&
                move->readyDistance > zeroFloat) {
                result = i;
                break;
            }
        }

        if ((f64)move->cooldown > zeroDouble &&
            sMusicFadeBase <
                *(f32 *)((u8 *)c + offsetof(Critter, moveTimes) + timeOffset) +
                    move->cooldown) {
            goto next;
        }

        distance = CritterCalcTarget(c, (f32 *)((u8 *)move + 0x60),
                                     c->targetPos, 0);
        if (distance < best) {
            result = i;
            best = distance;
        }

    next:
        i++;
        timeOffset += 4;
        moveOffset += sizeof(CritterMove);
    }

    if (result >= 0) {
        c->nextmove = (s16)result;
    }
}
/* 0x8003B7D8 -- continue the current child-pattern sequence or choose the
 * oldest ready child pattern / critical move and its target. */
void CritterChildCriticalMove(Critter *c)
{
    s32 i;
    s32 patternChoice;
    s32 moveChoice;
    s32 playerChoice;
    CritterPattern *patterns;
    CritterPattern *pattern;
    CritterMove *moves;
    CritterMove *move;
    f32 *time;
    s32 player;
    s32 type;
    u32 flags;
    f64 zero;
    f32 best;

    patternChoice = -1;
    moveChoice = -1;
    playerChoice = -1;
    best = lbl_803465F8;

    if (c->unk11C >= 0 && c->unk120 + 1 < 8) {
        patterns = c->hdr->patternsPtr;
        c->nextmove = patterns[c->unk11C].moveidx[c->unk120 + 1];
        if (c->nextmove >= 0) {
            c->unk11E = c->unk11C;
            return;
        }
    }

    i = 0;
    patterns = c->hdr->patternsPtr;
    while (i < c->hdr->auxMoveCount) {
        pattern = &patterns[i];
        if (i == c->unk11C) {
            goto next_pattern;
        }
        if ((pattern->flags & 2) != 0 &&
            c->childcnt != c->alivecnt) {
            goto next_pattern;
        }
        if ((pattern->flags & 0x1000) != 0) {
            goto next_pattern;
        }
        time = &c->patternTimes[i];
        if (sMusicFadeBase < *time + pattern->cooldown) {
            goto next_pattern;
        }
        player = CritterGetTargetSub(c, (f32 *)((u8 *)pattern + 0x30), 0);
        if (player >= 0 && *time < best) {
            patternChoice = i;
            playerChoice = player;
            best = *time;
        }

    next_pattern:
        i++;
    }

    moves = c->hdr->movesPtr;
    i = 0;
    zero = lbl_80346488;
    while (i < c->hdr->moveCount) {
        if (i == c->curmove) {
            goto next_move;
        }
        move = &moves[i];
        type = move->type;
        if (type < 0x7F || type >= 0xF0) {
            goto next_move;
        }
        flags = move->flags;
        if ((flags & 4) != 0) {
            goto next_move;
        }
        if ((flags & 2) != 0 &&
            c->childcnt != c->alivecnt) {
            goto next_move;
        }
        if ((flags & 0x10) != 0) {
            if (move->nodeidx < 0) {
                goto next_move;
            }
            if (move->link >= 0 && moves[move->link].nodeidx < 0) {
                goto next_move;
            }
        }

        if (c->unk128 >= 0 && type == 0x81) {
            patternChoice = -1;
            moveChoice = i;
            playerChoice = c->unk128;
            break;
        }

        if ((f64)move->cooldown > zero &&
            sMusicFadeBase < c->moveTimes[i] + move->cooldown) {
            goto next_move;
        }
        player = CritterGetTargetSub(c, (f32 *)((u8 *)move + 0x60), 0);
        if (player >= 0) {
            time = &c->moveTimes[i];
            if (*time < best) {
                best = *time;
                patternChoice = -1;
                moveChoice = i;
                playerChoice = player;
            } else {
                if (patternChoice < 0 &&
                    (moveChoice < 0 ||
                     CritterGetDmove(&moves[moveChoice], move) > 1)) {
                    best = *time;
                    patternChoice = -1;
                    moveChoice = i;
                    playerChoice = player;
                }
            }
        }

    next_move:
        i++;
    }

    if (patternChoice >= 0) {
        c->unk11E = (s16)patternChoice;
        c->unk126 = (s16)playerChoice;
        pattern = c->hdr->patternsPtr;
        c->nextmove = pattern[patternChoice].moveidx[0];
    } else if (moveChoice >= 0) {
        c->nextmove = (s16)moveChoice;
        c->unk126 = (s16)playerChoice;
    }
}
/* 0x8003BAFC -- select a ready critical move when its target player is
 * currently attacking. */
void CritterLookForCriticalMove(Critter *c)
{
    s32 i;
    CritterMove *moves;
    CritterMove *move;
    u32 flags;
    s32 player;

    i = 0;
    moves = c->hdr->movesPtr;

    while (i < c->hdr->moveCount) {
        move = &moves[i];
        if (move->type != MOVE_BLOCK) {
            goto next;
        }
        flags = move->flags;
        if ((flags & 4) != 0) {
            goto next;
        }
        if ((flags & 2) != 0 && c->childcnt != c->alivecnt) {
            goto next;
        }
        if ((flags & 0x10) != 0) {
            if (move->nodeidx < 0) {
                goto next;
            }
            if (move->link >= 0 && moves[move->link].nodeidx < 0) {
                goto next;
            }
        }
        if (move->cooldown > 0.0 &&
            sMusicFadeBase < c->moveTimes[i] + move->cooldown) {
            goto next;
        }
        player = CritterGetTargetSub(c, (f32 *)((u8 *)move + 0x60), 0);
        if (player < 0 || PlayerAttacking(player, 1) == 0) {
            goto next;
        }
        c->nextmove = (s16)i;

    next:
        i++;
    }
}
/* 0x8003BC28 -- select a child-pattern move, with damage reactions taking
 * precedence over the ordinary linked continuation. */
void CritterChildGetPattern(Critter *c)
{
    CritterMove *move;

    move = &(c->hdr->movesPtr)[c->curmove];
    if (c->state == 1) {
        c->nextmove = (s16)CritterFindMoveType(c, MOVE_DEATH, 1);
    } else if (lbl_8034489C >= 3 && lbl_8034489C <= 5 && gBossType == 35) {
        c->nextmove = (s16)CritterFindMoveType(c, MOVE_READY, 0);
    } else if (move->link >= 0) {
        c->nextmove = move->link;
    }
    if (c->nextmove < 0 && (c->counterState & 0x120) != 0) {
        if ((c->counterState & 0x100) != 0) {
            c->nextmove = (s16)CritterFindMoveType(c, MOVE_KNOCKDOWN, 0);
        }
        if (c->nextmove < 0) {
            c->nextmove = (s16)CritterFindMoveType(c, MOVE_KNOCKBACK, 0);
        }
    }
    if (c->nextmove < 0 &&
        c->counterValue >=
            (f32)(s32)(lbl_80346600 * lbl_8011AEAC[lbl_8034465C])) {
        c->nextmove = (s16)CritterFindMoveType(c, MOVE_ROAR, 0);
    }
    if (c->nextmove < 0 && (c->counterState & 0x10) != 0) {
        c->nextmove = (s16)CritterFindMoveType(c, MOVE_HITREACT, 0);
    }
    if (c->nextmove >= 0) {
        c->unk11E = -2;
    }
}

/* 0x8003BDF4 -- choose the root critter's next action from lifecycle,
 * explicit links, AI mode and pending damage reactions. */
void CritterGetDoAction(Critter *c)
{
    CritterMove *move;
    Critter *child;
    s32 aiType;

    move = &(c->hdr->movesPtr)[c->curmove];
    aiType = c->hdr->descriptor->type;
    if (c->curmove < 0 || c->state == 0) {
        c->nextmove = (s16)CritterFindMoveType(c, 0, 1);
    } else if (c->state == 2) {
        c->nextmove = (s16)CritterFindMoveType(c, 0, 1);
    } else if (move->type == 0) {
        c->nextmove = (s16)CritterFindMoveType(c, MOVE_START, 0);
    } else if (c->state == 1) {
        c->nextmove = (s16)CritterFindMoveType(c, MOVE_DEATH, 1);
    } else if (move->link >= 0) {
        c->nextmove = move->link;
    } else if ((u32)(lbl_8034489C - 1) <= 1) {
        c->nextmove = (s16)CritterFindMoveType(c, MOVE_READY, 1);
    } else if (lbl_8034489C == 3 && move->type != MOVE_ROAR) {
        c->nextmove = (s16)CritterFindMoveType(c, MOVE_ROAR, 1);
    } else if (lbl_8034489C >= 3 && lbl_8034489C <= 5 && gBossType == 0x23) {
        c->nextmove = (s16)CritterFindMoveType(c, MOVE_READY, 0);
    } else if (move->type == MOVE_START && aiType == 4) {
        c->nextmove = (s16)CritterFindMoveType(c, MOVE_READY, 0);
    }

    if (c->nextmove < 0 && (c->counterState & 0x120) != 0) {
        if ((c->counterState & 0x100) != 0) {
            c->nextmove = (s16)CritterFindMoveType(c, MOVE_KNOCKDOWN, 0);
        }
        if (c->nextmove < 0) {
            c->nextmove = (s16)CritterFindMoveType(c, MOVE_KNOCKBACK, 0);
        }
    }
    if (c->nextmove < 0 &&
        c->counterValue >=
            (f32)(s32)(lbl_80346600 * lbl_8011AEAC[lbl_8034465C])) {
        c->nextmove = (s16)CritterFindMoveType(c, MOVE_ROAR, 0);
    }
    if (c->nextmove < 0 && (c->counterState & 0x10) != 0) {
        c->nextmove = (s16)CritterFindMoveType(c, MOVE_HITREACT, 0);
    }
    if (c->nextmove <= 1 && c->nextmove >= 0) {
        lbl_80344628++;
    }
    if (c->nextmove >= 0 && c->unk11C >= 0) {
        c->unk11C = -1;
        c->unk120 = -1;
        for (child = c->next; child != NULL; child = child->next) {
            child->unk11C = -1;
            child->unk120 = -1;
        }
    }
    c->counterState &= ~0x130;
}

static f32 CritterAnimMod(s32 delta, f32 period)
{
    f64 absPeriod;
    f32 t = (f32)delta;
    absPeriod = __fabs(period);
    if (absPeriod > __fabs(t)) {
        return t;
    }
    return t - period * (f32)(s64)(t / period);
}

/* 0x8003C11C -- convert move frame windows into the two activation edges
 * consumed by CritterActivate. */
u32 CritterCopyAnim(Critter *c, CritterMove *move, s32 frame)
{
    u32 result;
    /* lint-allow-next-line FM003: measured frame slot -- deleting it moves CritterCopyAnim by 17 words at unchanged size; original local unrecovered */
    u8 unused[16];

    result = 0;
    switch (move->type) {
    case MOVE_CLAW:
    case MOVE_BREATH:
    case MOVE_CHARGE: {
        s32 second;
        if (frame >= move->frameStart &&
            frame <= move->frameEnd) {
            result |= 1;
        }
        second = move->frameStart2;
        if (second >= 0 && frame >= second &&
            frame <= move->frameEnd2) {
            result |= 2;
        }
        break;
    }
    case MOVE_GRAB: {
        s32 second;
        if (frame >= move->frameStart &&
            frame <= move->frameEnd) {
            result |= 1;
        }
        second = move->frameStart2;
        if (second >= 0 && (c->moveFlags & 2) == 0 && frame >= second) {
            result |= 2;
        }
        break;
    }
    case MOVE_SHOOT: {
        s16 flags = c->moveFlags;
        s32 second;
        if ((flags & 1) == 0 && frame >= move->frameStart) {
            result |= 1;
        }
        second = move->frameStart2;
        if (second >= 0 && (flags & 2) == 0 && frame >= second) {
            result |= 2;
        }
        break;
    }
    case MOVE_SPRAY: {
        s32 first = move->frameStart;
        s32 second;
        f32 period;
        if (frame >= first && frame <= move->frameEnd) {
            period = move->framePeriod;
            if (period <= lbl_80346488 ||
                (s32)CritterAnimMod(frame - first, period) == 0) {
                result |= 1;
            }
        }
        second = move->frameStart2;
        if (second >= 0 && frame >= second &&
            frame <= move->frameEnd2) {
            period = move->framePeriod;
            if (period <= lbl_80346488 ||
                (s32)CritterAnimMod(frame - second, period) == 0) {
                result |= 2;
            }
        }
        break;
    }
    case MOVE_TARGETED: {
        s16 idx;
        s16 flags;
        s32 second;
        if ((c->moveFlags & 1) == 0 && frame >= move->frameStart &&
            (idx = c->unk124) >= 0) {
            GetPlayerColPos(idx, c->targetPos);
            result |= 1;
        }
        second = move->frameStart2;
        if (second >= 0 && ((flags = c->moveFlags) & 1) != 0 &&
            (flags & 2) == 0 && frame >= second) {
            result |= 2;
        }
        break;
    }
    default: {
        s16 flags = c->moveFlags;
        s32 second;
        if ((flags & 1) == 0 && frame >= move->frameStart) {
            result |= 1;
        }
        second = move->frameStart2;
        if (second >= 0 && (flags & 2) == 0 && frame >= second) {
            result |= 2;
        }
        break;
    }
    }
    return result;
}

/* 0x8003C40C -- select/blend the active sequence, animate auxiliary trees,
 * and hand completed moves to CritterMoveDone.
 * (A second pattern-row reconstruction that lived here -- CritterAnimPatternRow,
 *  s16 sequence[8] @0x22 -- and a later move@0x20 + sequence[7]@0x22 split are
 *  both superseded: CritterPattern now carries the Xbox PDB crit_pattern's
 *  s16 moveidx[8] at 0x20, which the shipped PTRN records and
 *  CritterInitHeader's 8-entry swap loop both confirm, and the readers here
 *  index it as moveidx[c->unk120 + 1] -- the same addresses as before.) */
#pragma opt_propagation off
void CritterAnimate(Critter *c)
{
    CritterMove *current;
    CritterMove *next;
    u8 *subnode;
    s32 currentIndex;
    s32 nextIndex;
    s32 selectedSequence;
    s32 sequence;
    s32 transition;
    register s32 doneResult;
    u16 done;
    u64 controllerFlag;
    s32 candidate;

    current = NULL;
    next = NULL;
    currentIndex = c->curmove;
    if (c->unk11E < 0 || c->unk120 < 0 || c->unk120 >= 8 ||
        (candidate = ((CritterPackedType *)c->hdr)
                         ->patternsPtr[c->unk11E]
                         .moveidx[c->unk120 + 1]) < 0) {
        candidate = c->nextmove;
    }
    nextIndex = candidate;
    if (currentIndex >= 0) {
        current = &c->hdr->movesPtr[currentIndex];
    }
    if (candidate >= 0) {
        next = &c->hdr->movesPtr[candidate];
    }

    if (next == NULL) {
        sequence = current->seqidx;
        transition = 0;
    } else {
        controllerFlag = gControllerButtons & 0x80;
        if (controllerFlag == 0 && next != current && next->priority >= 0xF00 &&
            (current == NULL || current->interrupt != 0)) {
            sequence = next->seqidx;
            transition = 3;
        } else if (c->rate > sMusicFadeBase) {
            sequence = current != NULL ? current->seqidx : -1;
            nextIndex = currentIndex;
            transition = 0;
        } else {
            selectedSequence = 0;
            if (controllerFlag != (u64)(u32)selectedSequence) {
                if (next != NULL) {
                    selectedSequence = next->seqidx;
                }
                sequence = selectedSequence;
                if (current == NULL) {
                    transition = 3;
                } else if (current != next && current->type == 1) {
                    transition = 3;
                } else {
                    transition = 1;
                }
            } else {
                sequence = next->seqidx;
                if (current != NULL) {
                    transition = CritterGetDmove(current, next);
                } else {
                    transition = 3;
                }
            }
        }
    }

    if (sequence < 0) {
        FatalError(lbl_80112174, 0x800000);
    }
    if (current != next && transition == 0 && sMusicFadeBase > c->rate &&
        AnimDone(&c->sound[0])) {
        transition = 1;
    }

    if (c->pausecnt > 0) {
        c->animtimer += gClockFrameStep;
        done = 0;
    } else {
        done = AnimateATree(&c->colhandle, sequence, transition);
    }
    if (current != NULL && current == next && current->type == 0) {
        done = 0;
    }
    for (subnode = (u8 *)c->subnodes; subnode != NULL;
         subnode = *(u8 **)(subnode + 0x50)) {
        if (sequence >= *(s16 *)(subnode + 0x10)) {
            sequence = 0;
        }
        AnimateATree(subnode, sequence, transition);
    }

    done &= 3;
    doneResult = (s16)done;
    c->movedone = (s16)doneResult;
    if ((s16)doneResult != 0) {
        CritterMoveDone(c, nextIndex);
    } else if (nextIndex < 0 && AnimDone(&c->sound[0])) {
        c->curmove = -1;
    }
}
#pragma opt_propagation reset

/* 0x8003C6FC -- record cooldown/pattern progress and install the move that
 * just completed its blend. */
void CritterMoveDone(Critter *c, s32 moveIndex)
{
    CritterMove* move;
    Critter* child;
    s32 nextPatternMove;
    s32 currentMove;

    nextPatternMove = -1;
    move = NULL;
    currentMove = c->curmove;
    if (currentMove >= 0) {
        move = &(c->hdr->movesPtr)[currentMove];
    }
    if (move != NULL) {
        switch (move->type) {
        case 16:
            if (move->link < 0) {
                if (lbl_8034489C == 1) {
                    lbl_8034489C = 2;
                }
            }
            break;
        case 34:
            if (lbl_8034489C == 3) {
                lbl_8034489C = 4;
            }
            break;
        }
    }

    if (c->unk11C >= 0) {
        if (c->parent == NULL || c->parent->unk11C < 0) {
            c->unk120++;
            if (c->unk120 < 8) {
                CritterPattern* patterns =
                    c->hdr->patternsPtr;
                nextPatternMove =
                    patterns[c->unk11C].moveidx[c->unk120];
            }
            if (nextPatternMove < 0) {
                c->unk11C = -1;
                c->unk120 = -1;
                for (child = c->next; child != NULL; child = child->next) {
                    child->unk11C = -1;
                    child->unk120 = -1;
                }
            }
        }
    } else {
        if (c->unk11E >= 0) {
            c->patternTimes[c->unk11E] = sMusicFadeBase;
            c->unk11C = c->unk11E;
            c->unk120 = 0;
        } else {
            c->moveTimes[moveIndex] =
                (f32)(lbl_80346608 * (f32)(*(s16 *)((u8 *)c + 0x88) - 2) +
                      sMusicFadeBase);
        }
    }

    if (c->unkABA >= 0) {
        c->unkABA = DeleteEffect(c->unkABA, 1);
    }
    c->curmove = (s16)moveIndex;
    c->rate = lbl_80346470;
}

/* 0x8003C8D4 -- classify two critters' facing/positions into a 0/1/2 code by
 * the relation encoded in a->curmove (0x56). */
s32 CritterGetDmove(CritterMove *a, CritterMove *b)
{
    s32 av;
    s32 bv;
    s32 result;

    result = 1;
    av = a->priority;
    bv = b->priority;
    switch (a->interrupt) {
    case 0:
        result = 0;
        break;
    case 20:
        if ((bv & ~0xFF) > (av & ~0xFF)) {
            result = 2;
        }
        break;
    case 40:
    default:
        if (bv > av) {
            result = 2;
        }
        break;
    case 60:
        if (bv >= av) {
            result = 2;
        }
        break;
    case 80:
        if (bv > 0) {
            result = 2;
        }
        break;
    case 90:
        result = 2;
        break;
    }
    return result;
}

/* 0x8003C988 -- select an available move of the requested type, preferring
 * the candidate whose cooldown expires first. */
#pragma opt_propagation off
s32 CritterFindMoveType(Critter *c, s32 type, s32 mode)
{
    s32 timeOffset;
    s32 moveOffset;
    CritterMove *move;
    u8 *hdr;
    s32 i;
    s32 result;
    f32 best;
    f32 remaining;

    hdr = (u8 *)c->hdr;
    i = 0;
    timeOffset = 0;
    moveOffset = 0;
    result = -1;
    best = 0.0f;

    for (; i < ((CritterPackedType *)hdr)->moveCount;
         i++, timeOffset += 4, moveOffset += sizeof(CritterMove)) {
        move = (CritterMove *)(*(u8 **)(hdr + offsetof(CritterPackedType,
                                movesPtr)) + moveOffset);
        if ((move->flags & 4) == 0 && move->type == type) {
            if ((f64)move->cooldown > lbl_80346488) {
                remaining = c->moveTimes[i] + move->cooldown - sMusicFadeBase;
            } else {
                remaining = *(volatile f32 *)&lbl_80346470;
            }
            if ((f64)remaining <= lbl_80346488 || mode != 0) {
                if (result < 0 || remaining < best) {
                    best = remaining;
                    result = i;
                    if (mode == 2) {
                        break;
                    }
                }
            }
        }
    }

    if (result < 0 && mode != 0) {
        ErrorPrintf(lbl_8011219C, type, mode);
        result = CritterFindMoveType(c, MOVE_READY, 1);
    }
    return result;
}
#pragma opt_propagation on
/* -- externs used by CritterAnimInterrupt -- */
extern void *SfxGetNode(s32 node);
extern void  PlayerSetParent(Player *p, void *node, f32 *offset);
extern void  PlayerUnsetParent(Player *p);
extern void  DmgFxCircleUpdate(void *fx, f32 radius, s32 flag);
extern void *DmgFxCircleAdd(void *emitter, f32 a, f32 b, f32 c, f32 *v, s32 z);
extern void  DmgFxConeUpdate(void *fx, f32 a, f32 b, f32 c, f32 d, s32 flag);
extern void *DmgFxConeAdd(void *emitter, f32 a, f32 b, f32 c, f32 d, f32 *v,
                          s32 z);
extern void  BossSpewCoins(f32 *origin, f32 *dir, f32 angle);
extern f32   acosf(f32 x);
extern s32   gGameOptions[];
extern f32   lbl_80127D00[];
extern f64   lbl_80346610;
extern f32   lbl_803464F0;
/* 0x8003CA98 -- dispatch one move action descriptor on activation or release. */
void CritterAnimInterrupt(Critter *c, s32 action, s32 phase, s32 active)
{
    CritterBigState *big = &gBig;
    u8 *desc;
    Player *pp;
    s16 type;
    s32 i;
    s32 node;
    /* lint-allow-next-line FM003: measured frame slot -- deleting it moves CritterAnimInterrupt by 10 words at unchanged size; original local unrecovered */
    u8 unused0[4];
    f32 v[3];
    /* lint-allow-next-line FM003: measured frame slot -- deleting it moves CritterAnimInterrupt by 22 words at unchanged size; original local unrecovered */
    u8 unused1[8];
    f32 dir[3];
    /* lint-allow-next-line FM003: measured frame slot -- deleting it moves CritterAnimInterrupt by 34 words at unchanged size; original local unrecovered */
    u8 unused2[4];

    desc = *(u8 **)(*(u8 **)((u8 *)c->hdr + offsetof(CritterPackedType, file)) +
                    offsetof(CritterFileHeader, damage)) + action * 0x50;
    type = ((CritterDamageDef *)desc)->type;
    switch (type) {
    case 5:
        if (active) {
            for (i = 0; i < lbl_80344658; i++) {
                node = CritterDoTexmodNode(c, action, 0, lbl_80127D00);
                if (node >= 0) {
                    u8 *row = (u8 *)big + i * 4;
                    MBNodeSetParent(SfxGetNode(node),
                                    ItemGetNode((void *)*(u32 *)(row + 0x50)));
                }
            }
        }
        break;
    case 6:
        if (active) {
            if (lbl_80344658 != 0) {
                lbl_80344654 = SafeRockNearestTarget(c->unk124);
                if (lbl_80344654 >= 0) {
                    node = CritterDoTexmodNode(c, action, 0, lbl_80127D00);
                    if (node >= 0) {
                        s32 frames;
                        MBNodeSetParent(
                            SfxGetNode(node),
                            ItemGetNode(
                                (void *)big->safeRockIndices[lbl_80344654]));
                        frames = -1;
                        if (node >= 0) {
                            frames = *(s16 *)&Effects[node].atree[0x14];
                        }
                        frames = frames - 1;
                        big->safeRockTimers[lbl_80344654] =
                            (f32)(lbl_80346610 * (f64)frames);
                    }
                }
            }
        }
        break;
    case 8:
        if (active) {
            CritterDoTexmodNode(c, action, 0, c->targetPos);
        }
        break;
    case 0:
        CritterNodePlayerCollide(c, (CritterDamageDef *)desc, 1);
        CritterNodeEnemyCollide(c, desc);
        if (active) {
            CritterDoTexmodNode(c, action, 1, NULL);
        }
        if (c->emitter != NULL) {
            DmgFxCircleUpdate(c->emitter, ((CritterDamageDef *)desc)->maxDistance, 1);
        } else if ((gControllerButtons & 0x10) && gGameOptions[8]) {
            c->emitter = DmgFxCircleAdd(c->obj_d0, ((CritterDamageDef *)desc)->maxDistance,
                                        ((CritterDamageDef *)desc)->pitch,
                                        ((CritterDamageDef *)desc)->yaw,
                                        ((CritterDamageDef *)desc)->offset, 0);
        }
        break;
    case 4:
        CritterFirePlayerCollide(c, (CritterDamageDef *)desc);
        if (active) {
            CritterDoTexmodNode(c, action, 1, NULL);
        }
        if (c->emitter != NULL) {
            DmgFxConeUpdate(c->emitter, ((CritterDamageDef *)desc)->radius,
                            ((CritterDamageDef *)desc)->maxDistance, ((CritterDamageDef *)desc)->pitch,
                            ((CritterDamageDef *)desc)->yaw, 1);
        } else if ((gControllerButtons & 0x10) && gGameOptions[8]) {
            c->emitter = DmgFxConeAdd(c->obj_d0, ((CritterDamageDef *)desc)->radius,
                                      ((CritterDamageDef *)desc)->maxDistance,
                                      ((CritterDamageDef *)desc)->pitch,
                                      ((CritterDamageDef *)desc)->yaw,
                                      ((CritterDamageDef *)desc)->offset, 0);
        }
        break;
    case 1:
        if (active) {
            CritterDoTexmodNode(c, action, 0, c->moveOrigin);
        }
        break;
    case 7:
        if (phase == 1) {
            if (c->unk128 < 0) {
                node = CritterNodePlayerCollide(c, (CritterDamageDef *)desc, 0);
                if (node >= 0) {
                    pp = &gPlayers[node];
                    PlayerSetParent(pp, c->obj_d0, ((CritterDamageDef *)desc)->offset);
                    c->unk128 = (s16)node;
                    if (((CritterDamageDef *)desc)->sfx >= 0) {
                        SfxSetParent(
                            CritterDoSfx(c, ((CritterDamageDef *)desc)->sfx, NULL, 0, -1),
                            *(void **)((u8 *)pp + 0x74));
                    }
                }
            }
            if (active) {
                CritterDoTexmodNode(c, action, 0, c->moveOrigin);
            }
            if (c->emitter != NULL) {
                DmgFxCircleUpdate(c->emitter, ((CritterDamageDef *)desc)->maxDistance, 1);
            } else if ((gControllerButtons & 0x10) && gGameOptions[8]) {
                c->emitter = DmgFxCircleAdd(c->obj_d0, ((CritterDamageDef *)desc)->maxDistance,
                                            ((CritterDamageDef *)desc)->pitch,
                                            ((CritterDamageDef *)desc)->yaw,
                                            ((CritterDamageDef *)desc)->offset, 0);
            }
        } else if (phase == 2) {
            node = c->unk128;
            if (node >= 0) {
                CritterReleasePlayer(c, desc, dir, node);
            }
        }
        break;
    case 2:
    case 3:
        if (active) {
            CritterDoTexmodNode(c, action, 1, NULL);
        }
        break;
    case 9:
        if (active) {
            f32 angle = acosf(((CritterDamageDef *)desc)->mindp);
            v[0] = *(f32 *)((u8 *)c + 0x3F8);
            v[1] = *(f32 *)((u8 *)c + 0x3FC);
            v[2] = *(f32 *)((u8 *)c + 0x400);
            YawVec3(v, v, ((CritterDamageDef *)desc)->yaw);
            PitchVec3(v, v, ((CritterDamageDef *)desc)->pitch);
            v[0] = v[0] * ((CritterDamageDef *)desc)->minSpeed;
            v[1] = v[1] * ((CritterDamageDef *)desc)->minSpeed;
            v[2] = v[2] * ((CritterDamageDef *)desc)->minSpeed;
            BossSpewCoins(c->moveOrigin, v, angle);
        }
        break;
    default:
        if (active) {
            CritterDoTexmodNode(c, action, 1, NULL);
        }
        break;
    }
}

/* 0x8003D0A4 -- execute the visual/sound payload attached to an action
 * descriptor at either a supplied world position or the critter node. */
#pragma opt_lifetimes off
s32 CritterDoTexmodNode(Critter *c, s32 action, s32 local, f32 *position)
{
    /* lint-allow-next-line FM003: measured frame slot -- deleting it moves CritterDoTexmodNode by 7 words at unchanged size; original local unrecovered */
    u8 unused[8];
    f32 world[3];
    /* lint-allow-next-line FM003: measured frame slot -- deleting it moves CritterDoTexmodNode by 18 words at unchanged size; original local unrecovered */
    u8 worldPad[4];
    f32 velocity[3];
    /* lint-allow-next-line FM003: measured frame slot -- deleting it moves CritterDoTexmodNode by 44 words at unchanged size; original local unrecovered */
    u8 velocityPad[4];
    f32 offset[3];
    f32 angularVelocity[3];
    u8 *container;
    u8 *desc;
    s32 result;
    u32 flags;
    u8 *sfxDesc;
    u8 *hitDesc;
    u8 *morphDesc;
    s32 morph;
    s32 morphTarget;
    f32 scale;
    f32 radius;
    f32 damage;
    f32 damageRadius;
    f32 speed;
    f32 yaw;

    container = *(u8 **)((u8 *)c->hdr + offsetof(CritterPackedType, file));
    desc = *(u8 **)(container + offsetof(CritterFileHeader, damage)) + action * 0x50;
    if ((*(s16 *)(desc + offsetof(CritterDamageDef, behaviorFlags)) & 0x4000) != 0 &&
        (f64)c->unkAC8 > lbl_80346488 && *(s16 *)(desc + offsetof(CritterDamageDef, type)) != 1) {
        return -1;
    }
    if (c->mbnode != NULL &&
        (c->mbnode->flags & 8) != 0) {
        scale = c->mbnode->scale[1];
    } else {
        scale = lbl_803464A8;
    }
    offset[0] = *(f32 *)(desc + offsetof(CritterDamageDef, offset)) * scale;
    offset[1] = *(f32 *)(desc + (offsetof(CritterDamageDef, offset) + 4)) * scale;
    offset[2] = *(f32 *)(desc + (offsetof(CritterDamageDef, offset) + 8)) * scale;
    if (local) {
        world[0] = offset[0];
        world[1] = offset[1];
        world[2] = offset[2];
        if (position != NULL) {
            MulBodyVecMat4(position, position, &c->mtx[0][0]);
            world[0] = position[0] + world[0];
            world[1] = position[1] + world[1];
            world[2] = position[2] + world[2];
        }
    } else if (position != NULL) {
        MulVecMat3(offset, world, &c->mtx[0][0]);
        world[0] = position[0] + world[0];
        world[1] = position[1] + world[1];
        world[2] = position[2] + world[2];
    } else {
        MulVecMat4(offset, world, &c->mtx[0][0]);
    }

    result = CritterDoSfx(c, *(s16 *)(desc + offsetof(CritterDamageDef, sfxIndex)), world, local, -1);
    if (result < 0) {
        goto done;
    }

    flags = 0x801;
    sfxDesc = *(u8 **)(container + offsetof(CritterFileHeader, sfx)) + *(s16 *)(desc + offsetof(CritterDamageDef, sfxIndex)) * 0x50;
    radius = *(f32 *)(desc + offsetof(CritterDamageDef, damage));
    if (*(s16 *)((u8 *)*(CritterDescriptor **)((u8 *)c->hdr +
                 offsetof(CritterPackedType, descriptor)) +
                 offsetof(CritterDescriptor, type)) != 4) {
        flags |= 8;
        fn_80037ED0(lbl_80346618, c, Effects[result].id);
    }

    switch (*(s16 *)(desc + offsetof(CritterDamageDef, type))) {
    case 1:
        flags |= 6;
        if ((*(s16 *)(desc + offsetof(CritterDamageDef, behaviorFlags)) & 0x4000) != 0 &&
            (f64)c->unkAC8 > lbl_80346488) {
            Effects[result].endtime = gClockTime + c->unkAC8;
        }
        break;
    case 2:
        flags |= 0x30;
        break;
    case 3:
    case 5:
    case 6:
        flags |= 0x20;
        break;
    case 8:
        flags |= 0x20;
        break;
    case 4:
        radius = lbl_80346480;
        flags = 0;
        break;
    }

    if ((*(u32 *)(desc + offsetof(CritterDamageDef, flags)) & 0x00020000) != 0) {
        flags |= 0x00020000;
    }
    if ((*(s16 *)(desc + offsetof(CritterDamageDef, behaviorFlags)) & 0x40) != 0) {
        flags &= ~6;
    }
    if ((*(s16 *)(desc + offsetof(CritterDamageDef, behaviorFlags)) & 0x1000) != 0) {
        flags &= ~1;
    }
    if ((*(s16 *)(desc + offsetof(CritterDamageDef, behaviorFlags)) & 0x2000) != 0) {
        flags |= 0x400;
    }
    Effects[result].flags |= flags;

    if ((*(u32 *)sfxDesc & 0x10) != 0 && *(s16 *)(desc + offsetof(CritterDamageDef, sfx)) < 0) {
        PlaceEffectOnFloor(result, (f32 *)Effects[result].node);
    }
    if (*(s16 *)(desc + offsetof(CritterDamageDef, type)) != 1) {
        if (lbl_80346470 != *(f32 *)(desc + offsetof(CritterDamageDef, yaw))) {
            YawMat3(*(f32 *)(desc + offsetof(CritterDamageDef, yaw)), (f32 *)Effects[result].node);
        }
        if (lbl_80346470 != *(f32 *)(desc + offsetof(CritterDamageDef, pitch))) {
            WPitchMat3((f32 *)Effects[result].node, *(f32 *)(desc + offsetof(CritterDamageDef, pitch)));
        }
    }

    if (radius >= lbl_80346470) {
        damageRadius = *(f32 *)(desc + offsetof(CritterDamageDef, maxDistance)) * scale;
        damage = radius * *(f32 *)((u8 *)gCurLevel + offsetof(level_data, ene_damage));
        radius = *(f32 *)(desc + offsetof(CritterDamageDef, radius)) * scale;
        Effects[result].damage = damage;
        Effects[result].mindp = *(f32 *)(desc + offsetof(CritterDamageDef, mindp));
        Effects[result].damageradius = damageRadius;
        Effects[result].damagetype = *(DMG_TYPE *)(desc + offsetof(CritterDamageDef, flags));

        if (*(s16 *)(desc + offsetof(CritterDamageDef, sfx)) >= 0) {
            hitDesc = *(u8 **)(container + offsetof(CritterFileHeader, sfx)) +
                      *(s16 *)(desc + offsetof(CritterDamageDef, sfx)) * 0x50;
            SfxSetHit(result, *(s32 *)(hitDesc + offsetof(CritterSfxRecord, textureId)), *(s32 *)(hitDesc + offsetof(CritterSfxRecord, audioId)),
                      *(s32 *)(hitDesc + offsetof(CritterSfxRecord, audioId)));
            if ((*(u32 *)hitDesc & 0x10) != 0) {
                Effects[result].flags |= 0x200000;
            }
        }

        if (*(s16 *)(desc + offsetof(CritterDamageDef, morphTargetIndex)) >= 0) {
            morphDesc = *(u8 **)(container + offsetof(CritterFileHeader, sfx)) + 8;
            morphTarget = *(s32 *)(morphDesc +
                                    *(s16 *)(desc + offsetof(CritterDamageDef, morphTargetIndex)) * 0x50);
            morph = 0;
            if (*(s16 *)(desc + offsetof(CritterDamageDef, morphIndex)) >= 0) {
                morph = *(s32 *)(morphDesc +
                                  *(s16 *)(desc + offsetof(CritterDamageDef, morphIndex)) * 0x50);
            }
            speed = *(f32 *)(desc + offsetof(CritterDamageDef, morphSpeed));
            if ((f64)speed <= lbl_80346488) {
                speed = lbl_803464BC;
            }
            SfxSetMorph(speed, result, morphTarget, morph);
            if ((*(s16 *)(desc + offsetof(CritterDamageDef, behaviorFlags)) & 0x800) != 0) {
                Effects[result].flags |= 0x8000;
            }
            if ((*(u32 *)(desc + offsetof(CritterDamageDef, flags)) & 0x04000000) != 0) {
                Effects[result].webtime = speed;
                Effects[result].flags &= ~0x20;
            }
        }

        if (*(f32 *)(desc + offsetof(CritterDamageDef, minSpeed)) > lbl_80346470) {
            speed = (f32)(((f64)(damage = c->rateScale) < lbl_803464F8)
                              ? lbl_803464F8
                              : ((f64)damage > lbl_80346620)
                                    ? lbl_80346620
                                    : (f64)damage);
            speed = (f32)(lbl_80346530 *
                          ((f64)speed -
                           *(volatile f64 *)&lbl_803464F8)) *
                        (*(f32 *)(desc + offsetof(CritterDamageDef, maxSpeed)) - *(f32 *)(desc + offsetof(CritterDamageDef, minSpeed))) +
                    *(f32 *)(desc + offsetof(CritterDamageDef, minSpeed));

            if ((*(s16 *)(desc + offsetof(CritterDamageDef, behaviorFlags)) & 4) != 0) {
                velocity[0] = *(f32 *)((u8 *)c + offsetof(Critter, mtx) + 0x20);
                velocity[1] = *(f32 *)((u8 *)c + 0x30);
                velocity[2] = *(f32 *)((u8 *)c + offsetof(Critter, mtx) + 0x28);
            } else if ((*(s16 *)(desc + offsetof(CritterDamageDef, behaviorFlags)) & 1) != 0 && c->unk124 >= 0) {
                GetPlayerColPos(c->unk124, velocity);
                velocity[0] -= *(f32 *)((u8 *)Effects[result].node + offsetof(MBObject, mat[3][0]));
                velocity[1] -= *(f32 *)((u8 *)Effects[result].node + offsetof(MBObject, mat[3][1]));
                velocity[2] -= *(f32 *)((u8 *)Effects[result].node + offsetof(MBObject, mat[3][2]));
            } else {
                velocity[0] = *(f32 *)((u8 *)c + offsetof(Critter, mtx) + 0x20);
                velocity[1] = *(f32 *)((u8 *)c + 0x30);
                velocity[2] = *(f32 *)((u8 *)c + offsetof(Critter, mtx) + 0x28);
                velocity[1] = lbl_80346628;
            }

            if ((*(s16 *)(desc + offsetof(CritterDamageDef, behaviorFlags)) & 8) == 0) {
                CalcTargetDir(velocity, speed,
                              (f32)(lbl_80346490 / (f64)speed),
                              *(f32 *)(desc + offsetof(CritterDamageDef, gravity)),
                              lbl_80346470);
            } else {
                NormalVector(velocity);
            }

            if (*(s16 *)(desc + offsetof(CritterDamageDef, type)) == 1) {
                if (lbl_80346470 != *(f32 *)(desc + offsetof(CritterDamageDef, yaw)) ||
                    lbl_80346470 != *(f32 *)(desc + offsetof(CritterDamageDef, yawSpread))) {
                    yaw = *(f32 *)(desc + offsetof(CritterDamageDef, yaw));
                    if (*(f32 *)(desc + offsetof(CritterDamageDef, yawSpread)) >
                        *(volatile f32 *)&lbl_80346470) {
                        yaw += lbl_803464F8 *
                                   -(f64)*(f32 *)(desc + offsetof(CritterDamageDef, yawSpread)) +
                               (f64)Random(*(f32 *)(desc + offsetof(CritterDamageDef, yawSpread)));
                    }
                    YawVec3(velocity, velocity, yaw);
                }
                if ((*(s16 *)(desc + offsetof(CritterDamageDef, behaviorFlags)) & 8) != 0 &&
                    lbl_80346470 != *(f32 *)(desc + offsetof(CritterDamageDef, pitch))) {
                    PitchVec3(velocity, velocity, *(f32 *)(desc + offsetof(CritterDamageDef, pitch)));
                }
            }
            velocity[0] *= speed;
            velocity[1] *= speed;
            velocity[2] *= speed;

            if ((*(u32 *)sfxDesc & 8) != 0) {
                angularVelocity[0] = Random(lbl_8034662C);
                angularVelocity[1] = lbl_80346470;
                angularVelocity[2] = Random(lbl_8034662C);
                SfxSetPhysics(result, velocity, angularVelocity,
                            *(f32 *)(desc + offsetof(CritterDamageDef, gravity)), radius);
            } else {
                SfxSetPhysics(result, velocity, NULL, *(f32 *)(desc + offsetof(CritterDamageDef, gravity)),
                            radius);
            }
        } else {
            SfxSetPhysics(result, NULL, NULL, *(f32 *)(desc + offsetof(CritterDamageDef, gravity)), radius);
        }

        if ((gControllerButtons & 0x10) != 0 && gGameOptions[8] != 0) {
            DmgFxAdd(result);
        }
    }
done:
    return result;
}
#pragma opt_lifetimes reset
/* 0x8003D7E0 */
s32 CritterDoSfx(Critter *c, s32 sfx, void *parent, s32 arg3, s32 arg4)
{
    u8 *entry;
    s32 result;
    u32 flags;
    f32 mtxTmp[16];
    /* lint-allow-next-line FM003: measured frame slot -- deleting it moves CritterDoSfx by 11 words at unchanged size; original local unrecovered */
    u32 unusedHigh;
    f32 world[3];
    /* lint-allow-next-line FM003: measured frame slot -- deleting it moves CritterDoSfx by 40 words at unchanged size; original local unrecovered */
    u32 unusedLow;
    f32 color[3];
    f32 scale;
    f32 skinValue;
    s32 nodeCount;
    s32 audio;
    s16 skinParam;

    result = -1;
    if (sfx < 0) {
        return -1;
    }
    entry = *(u8 **)(*(u8 **)((u8 *)c->hdr + offsetof(CritterPackedType, file)) +
                     offsetof(CritterFileHeader, sfx)) + sfx * 0x50;
    flags = ((CritterSfxRecord *)entry)->flags;

    if ((flags & 0x400) != 0) {
        if ((gControllerButtons & 0x80) != 0) {
            return -1;
        }
        MBTreeSetFlags(c->anim, 1, 0);
        MBTreeSetFlags(c->anim->child, 2, 2);
    }

    if (c->mbnode != NULL &&
        (((MBObject *)c->mbnode)->flags & 8) != 0) {
        scale = ((MBObject *)c->mbnode)->scale[1];
    } else {
        scale = lbl_803464A8;
    }
    color[0] = ((CritterSfxRecord *)entry)->color[0] * scale;
    color[1] = *(f32 *)(entry + (offsetof(CritterSfxRecord, color) + 4)) * scale;
    color[2] = *(f32 *)(entry + (offsetof(CritterSfxRecord, color) + 8)) * scale;

    if ((flags & 0x0F000000) != 0) {
        CritterDoParticle(c, entry, arg4);
    } else if ((flags & 0x100) != 0) {
        skinValue = ((CritterSfxRecord *)entry)->rate;
        nodeCount = (s32)(lbl_80346630 * ((CritterSfxRecord *)entry)->life);
        skinParam = ((CritterSfxRecord *)entry)->custom0;
        if (((CritterSfxRecord *)entry)->textureId >= 0) {
            SetSkinFX((u8 *)&c->skinfx, ((CritterSfxRecord *)entry)->textureId,
                      nodeCount, skinParam, skinValue);
        } else {
            SetSkinFX((u8 *)&c->skinfx, lbl_802897B8[c->counterState & 0xF], 10,
                      0, lbl_803464E8);
        }
    } else if ((flags & 0x200) != 0) {
        nodeCount = 0;
        arg4 = 0;
        /* Measured against the banked object: the already-consumed `arg4`
         * parameter and the raw member spellings are BOTH load-bearing here.
         * `&c->hitnodes[nodeCount]` with typed members holds the size at 1156
         * bytes but replaces the target's `li` at +0x190 with `addi`
         * (1 differing word, `fndiff ... CritterDoSfx --ops` reports
         * `target-only: +1 li`); keeping `arg4` and typing only the members
         * gives 1156 -> 1152 bytes and 193 differing words.  The element type
         * is recovered (CritterHitNode in game/critter.h); the spelling is a
         * separate matching obligation. */
        /* lint-begin FM001, FM002: measured load-bearing cursor and spelling, deltas above */
        for (; nodeCount < c->hdr->colCount;
             nodeCount++, arg4 += sizeof(CritterHitNode)) {
            u8 *node = (u8 *)c + offsetof(Critter, hitnodes) + arg4;
            if ((*(s16 *)(*(u8 **)(node + offsetof(CritterHitNode, descriptor)) + offsetof(CritterColDescriptor, flags)) & 1) == 0) {
                world[0] = ((CritterHitNode *)node)->position[0] + color[0];
                world[1] = *(f32 *)(node + (offsetof(CritterHitNode, position) + 4)) + color[1];
                world[2] = *(f32 *)(node + (offsetof(CritterHitNode, position) + 8)) + color[2];
                result = CritterDoSfxSub(c, entry, world, 0, flags);
            }
        }
        /* lint-end FM001, FM002 */
    } else if (((CritterSfxRecord *)entry)->textureId >= 0) {
        if ((flags & 0x801) != 0) {
            arg3 = 1;
            world[0] = color[0];
            world[1] = color[1];
            world[2] = color[2];
        } else if ((flags & 0x80) != 0) {
            arg3 = 0;
            world[0] = c->prevMovePathPos[0] + color[0];
            world[1] = c->prevMovePathPos[1] + color[1];
            world[2] = c->prevMovePathPos[2] + color[2];
        } else if ((flags & 0x40) != 0) {
            if (c->obj_d0 != NULL) {
                GetWorldMat(c->obj_d0, mtxTmp, color);
                world[0] = mtxTmp[12];
                world[1] = mtxTmp[13];
                world[2] = mtxTmp[14];
            } else {
                world[0] = c->vel[0];
                world[1] = c->vel[1];
                world[2] = c->vel[2];
            }
            if (parent != NULL) {
                world[0] = ((f32 *)parent)[0] + world[0];
                world[1] = ((f32 *)parent)[1] + world[1];
                world[2] = ((f32 *)parent)[2] + world[2];
            }
            arg3 = 0;
        } else if (parent != NULL) {
            world[0] = ((f32 *)parent)[0] + color[0];
            world[1] = ((f32 *)parent)[1] + color[1];
            world[2] = ((f32 *)parent)[2] + color[2];
        } else {
            world[0] = color[0];
            world[1] = color[1];
            world[2] = color[2];
        }
        result = CritterDoSfxSub(c, entry, world, arg3, flags);
    } else {
        result = -1;
    }

    audio = ((CritterSfxRecord *)entry)->audioId;
    if (audio >= 0) {
        if (c->curmove >= 0 &&
            (c->hdr->movesPtr)[c->curmove].type == 17) {
            AudioPlay3DSel(audio, 224, c->vel, 0);
        } else {
            AudioPlay3DSel(audio, 224, c->vel, 1);
        }
    }
    if ((flags & 2) != 0) {
        ShakeCamera(0, 0, 90, lbl_80346570, 100);
    }
    if ((flags & 0x20) != 0) {
        SafeRockSetup();
    }
    if ((((CritterSfxRecord *)entry)->flags & 0x40000) != 0) {
        if (c->unkABA >= 0) {
            ErrorPrintf(lbl_801121C0);
        } else {
            c->unkABA = (s16)result;
        }
    }
    if (((CritterSfxRecord *)entry)->linkIndex >= 0) {
        CritterDoSfx(c, ((CritterSfxRecord *)entry)->linkIndex, parent, arg3, result);
    }
    return result;
}

/* 0x8003DC64 -- create one ordinary effect and apply owner, parent, color
 * and scale properties encoded by the critter sound descriptor. */
s32 CritterDoSfxSub(Critter *c, u8 *sfx, f32 *position,
                    s32 parented, u32 flags)
{
    u32 color;
    u32 treeFlags;
    u32 effectFlags;
    s32 useSceneRoot;
    s32 result;
    s32 effect;
    void *parent;
    u8 *effectData;
    f32 scale;

    effect = ((CritterSfxRecord *)sfx)->textureId;
    if (effect < 0) goto fail;
    treeFlags = 0x800;
    effectFlags = 0;
    if ((flags & 4) != 0) treeFlags |= 0x80080;
    if ((flags & 0x100000) != 0) treeFlags |= 0x80040;
    if ((flags & 0x1000) != 0) treeFlags |= 0x801000;
    if ((flags & 0x80000) != 0) treeFlags |= 0x40000000;
    useSceneRoot = flags & 0x2000;
    if (useSceneRoot != 0) treeFlags &= ~0x800;
    if ((flags & 0x10) != 0) effectFlags |= 0x200000;
    if ((flags & 0x8000) != 0) {
        effectFlags |= sMusicTrackHi == 8 ? 0x08000000 : 0x10000;
    }
    if ((flags & 0x10000) != 0) effectFlags |= 0x400000;
    if ((flags & 0x20000) != 0) effectFlags |= 0x02000000;
    if ((flags & 0x400000) != 0) effectFlags |= 0x04000000;
    if ((flags & 0x800000) != 0) effectFlags |= 0x20000000;

    result = StartFXSub(effect, position, effectFlags, treeFlags,
                        ((CritterSfxRecord *)sfx)->life);
    SfxSetOwner(result, c->id | 0x1000);
    if (useSceneRoot != 0) {
        SfxSetParent(result, lbl_80344EB4);
    } else if ((flags & 0x40) != 0) {
        SfxSetMat(result, (f32 *)c->mbnode, NULL);
    } else if (parented) {
        if ((flags & 0x800) != 0) {
            parent = c->anim->parent;
        } else if ((flags & 1) != 0) {
            parent = c->anim;
        } else {
            parent = c->obj_d0;
        }
        SfxSetParent(result, parent);
    }
    color = ((CritterSfxRecord *)sfx)->tintColor;
    if (color != 0xFFFFFFFF) {
        effectData = (u8 *)Effects;
        effectData += result * sizeof(Effect);
        MBTreeSetColor(**(void ***)(effectData += offsetof(Effect, atree)),
                       color, 1);
    }
    scale = ((CritterSfxRecord *)sfx)->scale;
    if (c->mbnode != NULL &&
        (((MBObject *)c->mbnode)->flags & 8) != 0) {
        scale *= ((MBObject *)c->mbnode)->scale[1];
    }
    if (scale != 1.0) {
        MBTreeSetScale(scale, scale, scale, Effects[result].node);
    }
    goto done;

fail:
    result = -1;
done:
    return result;
}

/* 0x8003DE70 -- create and configure a particle system from one descriptor. */
void CritterDoParticle(Critter *c, void *sfx, s32 node)
{
    u8 *s = (u8 *)sfx;
    f32 rate;
    f32 speed;
    f32 etime;
    s32 tex;
    u32 flags;
    u32 kind;
    void *parent;
    void *psys;

    rate = (f32)(lbl_80346630 * ((CritterSfxRecord *)s)->rate);
    flags = ((CritterSfxRecord *)s)->flags;
    tex = ((CritterSfxRecord *)s)->textureId;
    etime = ((CritterSfxRecord *)s)->life;
    speed = (f32)(lbl_80346540 * (f64)((CritterSfxRecord *)s)->custom1);
    kind = flags & 0x0F000000;
    if ((flags & 0x4000) && node >= 0) {
        parent = Effects[node].node;
    } else if (c->obj_d0 != NULL) {
        parent = c->obj_d0;
    } else {
        parent = c->mbnode;
    }
    switch (kind) {
    case 0x02000000:
        psys = MBNewPsysDefault(gIdentityMatrix, parent, 0, 1);
        if (psys != NULL) {
            MBPsysSetEVolume(psys, lbl_80346638, lbl_80346638);
            MBTreeSetFlags(psys, 0x880, 1);
            MBPsysSetPParm(psys, 3, lbl_803464A8, lbl_803464A8, lbl_803464A8,
                           lbl_80346470);
        }
        break;
    case 0x01000000:
    default:
        psys = MBNewPsysDefault(gIdentityMatrix, parent, 0, 1);
        MBPsysSetEVolume(psys, lbl_803464E8, lbl_803464E8);
        break;
    }
    if (psys == NULL) {
        ErrorPrintf(lbl_801121D4);
    } else {
        *(f32 *)((u8 *)psys + 0x30) = ((CritterSfxRecord *)s)->color[0];
        *(f32 *)((u8 *)psys + 0x34) = *(f32 *)(s + (offsetof(CritterSfxRecord, color) + 4));
        *(f32 *)((u8 *)psys + 0x38) = *(f32 *)(s + (offsetof(CritterSfxRecord, color) + 8));
        MBPsysSetPTex(psys, tex);
        MBPsysSetERate4(rate, rate, rate, rate, psys);
        MBPsysSetETime(etime, lbl_8034663C, psys);
        MBPsysSetPSpeed(psys, speed);
    }
}
/* 0x8003E048 -- allocate and initialize a root critter and the child chain
 * described by its loaded type header. */
typedef struct CritterChildLinks {
    u32 words[18];
} CritterChildLinks;

Critter *CritterNewInst(s32 type, s32 subtype, void *object)
{
    u8 *childDef;
    u8 *header;
    Critter *root;
    Critter *tail;
    u8 *childHeader;
    Critter *child;
    void *node;
    u8 *geo;
    s32 nodeIndex;
    s32 childIndex;

    header = (u8 *)gCritterHeaders[type][subtype];
    if (header == NULL) {
        ErrorPrintf("No Critter type %d subtype %d loaded", type, subtype);
        return NULL;
    }
    if (*(void **)(header + offsetof(CritterPackedType, atree)) == NULL) {
        return NULL;
    }

    root = CritterEmptyInst();
    if (root == NULL) {
        return NULL;
    }
    CritterInitInst(root, (struct CritterHeader *)header);
    CritterInitGeo(root, object, subtype);
    CritterAddAnimInsts(root, &root->mtx[0][0]);
    CritterInitColnodes(root);
    CritterAddHealthMeter(root);

    childIndex = ((CritterPackedType *)header)->childIndex;
    root->childcnt = 0;
    tail = root;
    while (childIndex >= 0) {
        childHeader = *(u8 **)(*(u8 **)(header + offsetof(CritterPackedType, file)) + offsetof(CritterFileHeader, types)) +
                      childIndex * sizeof(CritterPackedType);
        child = CritterEmptyInst();
        CritterInitInst(child, (struct CritterHeader *)childHeader);
        childDef = (u8 *)child->hdr;
        geo = *(u8 **)((u8 *)root->hdr + offsetof(CritterPackedType, atree));
        *(CritterChildLinks *)&child->colhandle =
            *(CritterChildLinks *)&root->colhandle;

        nodeIndex = AtreeFindNodeIdx(*(void **)(geo + 0x0C),
                                     *(s32 *)(geo + 0x10),
                                     (char *)child->hdr + 0x10, 0x10);
        child->colhandle = (u8 *)root->anodes + nodeIndex * 0x28;
        AtreeNodeSetParent(child->colhandle, NULL, NULL, 0);
        child->anim = *(void **)child->colhandle;

        nodeIndex = ((CritterPackedType *)childDef)->node0Index;
        if (nodeIndex < 0) {
            node = NULL;
        } else {
            node = *(void **)((u8 *)child->anodes + nodeIndex * 0x28);
            if (node == NULL) {
                node = NULL;
            }
        }
        child->hitnode0 = node;
        if ((((CritterPackedType *)childDef)->typeFlags & 0x10) != 0 &&
            child->hitnode0 != NULL &&
            *(void **)((u8 *)child->hitnode0 + 0x74) != NULL) {
            child->hitnode0 = *(void **)((u8 *)child->hitnode0 + 0x74);
        }

        nodeIndex = ((CritterPackedType *)childDef)->node1Index;
        if (nodeIndex < 0) {
            node = NULL;
        } else {
            node = *(void **)((u8 *)child->anodes + nodeIndex * 0x28);
            if (node == NULL) {
                node = NULL;
            }
        }
        child->hitnode1 = node;

        nodeIndex = ((CritterPackedType *)childDef)->node2Index;
        if (nodeIndex < 0) {
            node = NULL;
        } else {
            node = *(void **)((u8 *)child->anodes + nodeIndex * 0x28);
            if (node == NULL) {
                node = NULL;
            }
        }
        child->hitnode2 = node;

        CritterInitColnodes(child);
        CritterAddHealthMeter(child);

        tail->next = child;
        tail = child;
        child->parent = root;
        childIndex = ((CritterPackedType *)childHeader)->childIndex;
        root->childcnt++;
    }
    switch (*(s16 *)((u8 *)*(CritterDescriptor **)((u8 *)root->hdr +
                     offsetof(CritterPackedType, descriptor)) +
                     offsetof(CritterDescriptor, type))) {
    case 8:
        root->particle = FindClosestWaypoint(lbl_80346594,
                                             (f32 *)((u8 *)root + 0x3C), 0);
        break;
    default:
        break;
    }
    return root;
}
/* 0x8003E2E8 -- reserve the first free critter pool slot, wipe it, and stamp
 * it with a fresh index + rolling unique id. */
#pragma opt_propagation off
#pragma opt_common_subs off
Critter *CritterEmptyInst(void)
{
    u8 *c;
    s32 byte_offset;
    s32 i;
    s32 count;
    s32 scan_offset;
    u8 *scan;
    CritterBigState *big;

    big = &gBig;
    count = lbl_8034466C;
    for (i = 0, scan_offset = 0; i < count;
         i++, scan_offset += sizeof(Critter)) {
        scan = (u8 *)big + scan_offset;
        if (*(void **)(scan + (offsetof(CritterBigState, pool) + offsetof(Critter, hdr))) == NULL) {
            break;
        }
    }
    if (i >= 16) {
        ErrorPrintf(lbl_8011221C, i, count);
        return NULL;
    }
    if (i == count) {
        lbl_8034466C++;
        if (lbl_8034466C > gCritterCountMax) {
            gCritterCountMax = lbl_8034466C;
        }
    }
    byte_offset = i * sizeof(Critter);
    c = (u8 *)big + byte_offset;
    c += offsetof(CritterBigState, pool);
    memset(c, 0, sizeof(Critter));
    *(s16 *)c = (s16)i;
    scan = (u8 *)big + byte_offset;
    *(s16 *)(scan + (offsetof(CritterBigState, pool) + offsetof(Critter, id))) = gCritterNextID;
    if ((u16)(gCritterNextID = gCritterNextID + 1) > 4095) {
        gCritterNextID = 1;
    }
    return (Critter *)c;
}
#pragma opt_common_subs reset
#pragma opt_propagation reset
/* 0x8003E3E8 -- instantiate the model/animation tree and cache the principal
 * scene nodes and world-space transforms used by movement and collision. */
void CritterInitGeo(Critter *c, void *object, s32 subtype)
{
    typedef struct CritterInitGeoView {
        u8 unused000[0xFC];
        f32 yaw;
        u8 unused100[0x318];
        f32 cachedVelocity[3];
    } CritterInitGeoView;
    u8 *header;
    f32 *gid = gIdentityMatrix;
    s32 atreeFlags;
    void *node;
    void *n;
    s32 idx;
    s32 floorHit;
    f32 atanX;
    /* lint-allow-next-line FM003: measured frame slot -- deleting it moves CritterInitGeo by 5 words at unchanged size; original local unrecovered */
    u8 unused[8];

    atreeFlags = 0;
    header = (u8 *)c->hdr;
    c->mbnode = MBNewNode(lbl_8034473C, gid, 1);
    atanX = *(f32 *)((u8 *)object + 0x28);
    *(f32 *)((u8 *)c + 0xF8) =
        atan2(*(f32 *)((u8 *)object + 0x20), atanX);
    *(f32 *)((u8 *)c + 0xFC) = *(f32 *)((u8 *)c + 0xF8);
    CopyMat3(gid, &c->mtx[0][0]);
    c->vel[0] = *(f32 *)((u8 *)object + 0x30);
    c->vel[1] = *(f32 *)((u8 *)object + 0x34);
    c->vel[2] = *(f32 *)((u8 *)object + 0x38);
    YawMat3(((CritterInitGeoView *)c)->yaw, &c->mtx[0][0]);

    if ((*(u32 *)(header + offsetof(CritterPackedType, typeFlags)) & 0x1000) == 0) {
        atreeFlags |= 0x800;
    }
    c->colhandle = AtreeInit(*(void **)(header + offsetof(CritterPackedType, atree)), &c->colhandle, 0,
                             atreeFlags);
    c->anim = *(void **)c->colhandle;
    MBNodeSetParent(*(void **)c->colhandle, c->mbnode);

    if ((*(u32 *)(header + offsetof(CritterPackedType, typeFlags)) & 1) != 0) {
        s16 shadowType = c->hdr->descriptor->modelIndex;
        s32 shadowIdx = subtype > 2 ? 1 : subtype;
        node = MBOX_ReallyFindObject(lbl_8011AEA0[shadowIdx], shadowType,
                                     shadowType, 1);
        c->shadow = MBNewObject(node, gIdentityMatrix, NULL, 0x880);
        *(f32 *)((u8 *)c->shadow + offsetof(MBObject, mat[3][0])) = c->vel[0];
        *(f32 *)((u8 *)c->shadow + offsetof(MBObject, mat[3][1])) = c->vel[1];
        *(f32 *)((u8 *)c->shadow + offsetof(MBObject, mat[3][2])) = c->vel[2];
        *(f32 *)((u8 *)c->shadow + offsetof(MBObject, zsort_add)) = lbl_80346640;
        *(s16 *)((u8 *)c->shadow + offsetof(MBObject, zmod)) = -32;
    }

    idx = *(s16 *)(header + offsetof(CritterPackedType, node0Index));
    if (idx < 0) {
        node = NULL;
    } else {
        n = *(void **)((u8 *)c->anodes + idx * 0x28);
        node = n;
        if (node == NULL) {
            node = NULL;
        }
    }
    c->hitnode0 = node;
    if ((*(u32 *)(header + offsetof(CritterPackedType, typeFlags)) & 0x10) != 0 && c->hitnode0 != NULL &&
        *(void **)((u8 *)c->hitnode0 + offsetof(MBObject, parent)) != NULL) {
        c->hitnode0 = *(void **)((u8 *)c->hitnode0 + offsetof(MBObject, parent));
    }
    idx = *(s16 *)(header + offsetof(CritterPackedType, node1Index));
    if (idx < 0) {
        node = NULL;
    } else {
        n = *(void **)((u8 *)c->anodes + idx * 0x28);
        node = n;
        if (node == NULL) {
            node = NULL;
        }
    }
    c->hitnode1 = node;
    idx = *(s16 *)(header + offsetof(CritterPackedType, node2Index));
    if (idx < 0) {
        node = NULL;
    } else {
        n = *(void **)((u8 *)c->anodes + idx * 0x28);
        node = n;
        if (node == NULL) {
            node = NULL;
        }
    }
    c->hitnode2 = node;

    floorHit = FloorCollide(c->vel, 0, 0, 2, lbl_803464B8,
                            lbl_80346588, lbl_8034658C) != NULL
                   ? 1
                   : 0;
    if (floorHit != 0) {
        c->vel[1] = *(f32 *)(gFloorCollisionResult + 0x34) +
                    *(f32 *)(header + offsetof(CritterPackedType, floorOffset));
        if (c->shadow != NULL) {
            CopyMat3((f32 *)gFloorCollisionResult, (f32 *)c->shadow);
            *(f32 *)((u8 *)c->shadow + offsetof(MBObject, mat[3][0])) = c->vel[0];
            *(f32 *)((u8 *)c->shadow + offsetof(MBObject, mat[3][1])) = c->vel[1];
            *(f32 *)((u8 *)c->shadow + offsetof(MBObject, mat[3][2])) = c->vel[2];
            *(f32 *)((u8 *)c->shadow + offsetof(MBObject, mat[3][1])) =
                *(f32 *)(gFloorCollisionResult + 0x34);
        }
    } else {
        c->vel[1] = c->vel[1] + *(f32 *)(header + offsetof(CritterPackedType, floorOffset));
    }

    CopyMat4(&c->mtx[0][0], (f32 *)c->mbnode);
    UnparentMatrix(c->mbnode, *(f32 **)((u8 *)c->mbnode + offsetof(MBObject, parent)));
    CopyMat3(&c->mtx[0][0], (f32 *)((u8 *)c + 0x3D8));
    ((CritterInitGeoView *)c)->cachedVelocity[0] = c->vel[0];
    ((CritterInitGeoView *)c)->cachedVelocity[1] = c->vel[1];
    ((CritterInitGeoView *)c)->cachedVelocity[2] = c->vel[2];
    MulVec4Mat3((f32 *)(header + offsetof(CritterPackedType, originOffset)), c->pos, &c->mtx[0][0]);
    c->pos[0] = c->vel[0] + c->pos[0];
    c->pos[1] = c->vel[1] + c->pos[1];
    c->pos[2] = c->vel[2] + c->pos[2];
    c->movevec[0] = c->vel[0];
    c->movevec[1] = c->vel[1] + *(f32 *)(header + offsetof(CritterPackedType, vertDrift));
    c->movevec[2] = c->vel[2];
    c->obj_d0 = c->anim;
    GetWorldMat(c->obj_d0, c->worldMoveMatrix, NULL);

    if (*(f32 *)((u8 *)c->hdr + offsetof(CritterPackedType, defaultPos[1])) < lbl_80346618) {
        *(f32 *)((u8 *)c + 0x49C) = *(f32 *)((u8 *)c->hdr + offsetof(CritterPackedType, defaultPos[0]));
        *(f32 *)((u8 *)c + 0x4A0) = *(f32 *)((u8 *)c->hdr + offsetof(CritterPackedType, defaultPos[1]));
        *(f32 *)((u8 *)c + 0x4A4) = *(f32 *)((u8 *)c->hdr + offsetof(CritterPackedType, defaultPos[2]));
    } else {
        *(f32 *)((u8 *)c + 0x49C) = *(f32 *)((u8 *)c + 0x418);
        *(f32 *)((u8 *)c + 0x4A0) = *(f32 *)((u8 *)c + 0x41C);
        *(f32 *)((u8 *)c + 0x4A4) = *(f32 *)((u8 *)c + 0x420);
    }
}
/* 0x8003E7D0 -- create the optional HUD meter and attach the optional
 * in-world red health-fill geometry described by the critter header. */
void CritterAddHealthMeter(Critter *c)
{
    u8 *header;
    s32 meter;
    s32 style;
    void *match;
    void *root;

    header = (u8 *)c->hdr;
    meter = -1;
    if ((((CritterPackedType *)header)->typeFlags & 4) != 0) {
        style = (((CritterPackedType *)header)->typeFlags & 8)
                != 0 ? 1 : 0;
        meter = HealthMeterStart(
            header, ((CritterPackedType *)header)->meterX,
            ((CritterPackedType *)header)->meterY,
            ((CritterPackedType *)header)->meterW,
            ((CritterPackedType *)header)->meterH,
            style, c->health);
    }
    c->healthmtr = (s16)meter;

    if ((((CritterPackedType *)header)->typeFlags & 0x800)
        != 0) {
        match = AtreeMatch(
            c->hdr->descriptor->model,
            lbl_80346644, 1);
        if (match != NULL) {
            *(void **)&c->healthbar[0] =
                AtreeInit(match, &c->healthbar[0], 0, 0x800);
            MBNodeSetParent(**(void ***)&c->healthbar[0], c->mbnode);
            MBTreeSetFlags(**(void ***)&c->healthbar[0], 0x02000000, 0);

            root = **(void ***)&c->healthbar[0];
            ((MBObject *)root)->mat[3][0] =
                ((MBObject *)root)->mat[3][0] +
                c->hdr->healthbarOffset[0];
            *(f32 *)((u8 *)**(void ***)&c->healthbar[0] +
                     offsetof(MBObject, mat[3][1])) +=
                c->hdr->healthbarOffset[1];
            *(f32 *)((u8 *)**(void ***)&c->healthbar[0] +
                     offsetof(MBObject, mat[3][2])) +=
                c->hdr->healthbarOffset[2];

            c->damageflash =
                AtreeFindNode(&c->healthbar[0], lbl_80112238, 9);
        }
    }
}
/* 0x8003E920 -- bind a fresh critter instance to its loaded header: reset move
 * bookkeeping, scale starting health by the level, and zero the per-move and
 * hit-node scratch tables. */
void CritterInitInst(Critter *c, struct CritterHeader *hdr)
{
    s32 i;
    u8 *h;

    h = (u8 *)hdr;
    c->hdr = hdr;
    c->state = 0;
    c->curmove = -1;
    c->nextmove = 0;
    c->unk11C = -1;
    c->unk11E = -1;
    c->unk120 = -1;
    c->unk124 = -1;
    c->unk126 = -1;
    c->unk128 = -1;
    c->unkABA = -1;
    c->unkABC = -1;
    c->unkABE = 0;
    c->unkAC0 = -1;
    c->pausecnt = 0;
    c->unkAC6 = 0;
    c->unkAC8 = 0.0f;
    c->unk4AC = 0.0f;
    c->health = ((CritterPackedType *)h)->maxHealth * gCurLevel->ene_health;
    for (i = 0; i < 4; i++) {
        c->playerDamage[i].received = 0.0f;
        c->playerDamage[i].receivedTime = 0.0f;
        c->playerDamage[i].dealt = 0.0f;
        c->playerDamage[i].dealtTime = 0.0f;
    }
    for (i = 0; i < 4; i++) {
        c->unk4E0[i] = -1;
    }
    if ((s16)((CritterPackedType *)h)->moveCount > 0) {
        memset(c->moveTimes, 0, ((CritterPackedType *)h)->moveCount * 4);
    }
    if ((s16)((CritterPackedType *)h)->auxMoveCount > 0) {
        memset(c->patternTimes, 0, ((CritterPackedType *)h)->auxMoveCount * 4);
    }
    if ((s16)((CritterPackedType *)h)->colCount > 0) {
        memset(c->hitnodes, 0, ((CritterPackedType *)h)->colCount * 92);
    }
}
/* 0x8003EA4C -- tear down a critter instance: detach scene nodes, kill sfx,
 * recurse into linked children, free colnode list, then clear the slot. */
typedef struct CritterSubnode {
    void *atree;
    u8 _pad04[68];
    void *mbnode;
    u8 _pad4C[4];
    struct CritterSubnode *next;
} CritterSubnode;

void CritterDelInst(Critter *c)
{
    CritterSubnode *node;

    if (*(s16 *)((u8 *)*(void **)((u8 *)c->hdr + 288) + 32) == 4) {
        del_target(c->mtx);
        if (c->parent == NULL) {
            BossDeath();
        }
    }
    if (c->next != NULL) {
        CritterDelInst(c->next);
        c->next = NULL;
    }
    if (c->emitter != NULL) {
        MBRemoveNode(c->emitter, 1);
    }
    if (c->shadow != NULL) {
        MBRemoveNode(c->shadow, 0);
    }
    SfxDeleteParented(c->anim, 1, -1);
    if (c->emitterset != 0) {
        MBRemoveNode(c->emitter, 2);
    }
    if (c->colhandle != NULL) {
        AtreeDelete(&c->colhandle);
    }
    if (c->mbnode != NULL) {
        MBRemoveNode(c->mbnode, 0);
    }
    c->anim = NULL;
    while ((node = c->subnodes) != NULL) {
        if (node->atree != NULL) {
            AtreeDelete(node);
        }
        if (node->mbnode != NULL) {
            MBRemoveNode(node->mbnode, 1);
        }
        node->mbnode = NULL;
        c->subnodes = ((CritterSubnode *)c->subnodes)->next;
    }
    c->hdr = NULL;
    c->state = 0;
}
/* 0x8003EB8C -- step skin effects and service the per-node flash/fade
 * counters without leaving temporary texture flags on the attachment node. */
void CritterUpdateSkinfx(Critter *c)
{
    s32 i;
    CritterHitNode *node;
    u32 savedFlags;

    savedFlags = 0;
    ProcessSkinFX((f32 *)&c->skinfx, c->anim,
                  c->hitnode2);
    if (c->hitnode2 != NULL) {
        u32 *flags = (u32 *)c->hitnode2;
        savedFlags = *(flags += 0x18);
        *flags = savedFlags | 0x10;
    }

    for (i = 0; i < c->hdr->colCount; i++) {
        s32 counter = c->hitnodes[i].state;
        node = &c->hitnodes[i];
        if (counter > 0) {
            MBTreeSetAltTex(node->boundNode, 0xFFFFFFFC, lbl_80344BF8, 1);
            MBTreeSetAmbientAdd(node->boundNode, 0xFF, 1);
            node->state--;
        } else if (node->state == 0) {
            MBTreeSetAltTex(node->boundNode, 0xFFFFFFFF, 0, 1);
            MBTreeSetAmbientAdd(node->boundNode, 0, 1);
            node->state = -1;
        }
    }

    if (c->unkABC > 0) {
        MBTreeSetAltTex(c->anim, 0xFFFFFFFC, lbl_80344BF8, 1);
        MBTreeSetAmbientAdd(c->anim, 0xFF, 1);
        c->unkABC--;
    } else if (c->unkABC == 0) {
        MBTreeSetAltTex(c->anim, 0xFFFFFFFF, 0, 1);
        MBTreeSetAmbientAdd(c->anim, 0, 1);
        c->unkABC = -1;
    }
    if (c->pausecnt > 0) {
        c->pausecnt -= gFrameTicks;
        if (c->pausecnt <= 0) {
            MBTreeSetAltTex(c->anim, 0xFFFFFFFF, 0, 1);
            c->pausecnt = 0;
        } else if (c->pausecnt < 180 && (c->pausecnt & 8) != 0) {
            MBTreeSetAltTex(c->anim, 0xFFFFFFFF, 0, 1);
        } else {
            MBTreeSetAltTex(c->anim, 0xFFFFFFFC, (u32)c->unkAC0, 1);
        }
    }
    if (c->unkABE > 0) {
        s32 ambient = c->unkABE;
        c->unkABE -= 16;
        if (c->unkABE < 0) {
            c->unkABE = 0;
            ambient = 0;
        }
        MBTreeSetAmbientAdd(c->anim, ambient, 1);
    }
    if (c->hitnode2 != NULL) {
        c->hitnode2->flags = savedFlags;
    }
}
typedef struct CritterColnode {
    u8 _pad00[0x78];
    struct CritterColnode *child;
    struct CritterColnode *next;
} CritterColnode;

typedef struct CritterAnimNode {
    CritterColnode *node;
    u8 _pad04[0x1C];
    void *attachment;
    u8 _pad24[4];
} CritterAnimNode;

/* 0x8003EDC4 -- recursively remove a collision-node chain and clear every
 * animation/move/hit-node reference that pointed at the removed nodes. */
void CritterRemoveColnodeSub(Critter *c, CritterColnode *node, s32 mode)
{
    CritterColnode *next;
    s32 animOffset;
    s32 moveOffset;
    s32 hitOffset;
    s32 i;
    s32 j;

    while (node != NULL) {
        if (node->child != NULL) {
            CritterRemoveColnodeSub(c, node->child, 2);
        }
        next = node->next;
        MBRemoveNode(node, 0);

        for (i = 0, animOffset = 0; i < c->anodeCount;
             i++, animOffset += sizeof(CritterAnimNode)) {
            if (*(CritterColnode **)((u8 *)c->anodes + animOffset) == node) {
                j = 0;
                *(void **)((u8 *)c->anodes + animOffset + 0x20) = NULL;
                *(CritterColnode **)((u8 *)c->anodes + animOffset) = NULL;
                moveOffset = j;
                while (j < c->hdr->moveCount) {
                    if (*(s16 *)(*(u8 **)((u8 *)c->hdr + offsetof(CritterPackedType, movesPtr)) +
                                 moveOffset + 0x0E) == i) {
                        *(s16 *)(*(u8 **)((u8 *)c->hdr + offsetof(CritterPackedType, movesPtr)) +
                                 moveOffset + 0x0E) = -1;
                    }
                    j++;
                    moveOffset += sizeof(CritterMove);
                }
            }
        }

        for (i = 0, hitOffset = 0;
             i < c->hdr->colCount;
             i++, hitOffset += 0x5C) {
            u8 *hitRecord = (u8 *)c + hitOffset;
            if (*(void **)(hitRecord + 0x4FC) == node) {
                *(void **)(hitRecord + 0x4FC) = NULL;
            }
        }

        if (mode != 2) {
            break;
        }
        node = next;
    }
}
static inline void *CritterColnodeAnimNode(Critter *c, s32 index)
{
    void *node = c->anim;
    void *candidate;

    if (index < 0) {
        return node;
    }
    candidate = *(void **)((u8 *)c->anodes + index * 0x28);
    if (candidate == NULL) {
        candidate = node;
    }
    return candidate;
}
/* 0x8003EEF8 -- bind each collision descriptor to its animation node and
 * initialize its health, flash and optional damage-effect state. */
void CritterInitColnodes(Critter *c)
{
    u8 *header;
    s32 i;
    CritterColDescriptor *descriptorBase;
    CritterColDescriptor *descriptor;
    CritterHitNode *record;
    char *name;
    s8 ch;
    s32 nodeIndex;
    f32 zerof;
    /* lint-allow-next-line FM003: measured frame slot -- deleting it moves CritterInitColnodes by 7 words at unchanged size; original local unrecovered */
    u8 unused[8];

    header = (u8 *)c->hdr;
    if (((CritterPackedType *)header)->colCount <= 0) {
        return;
    }
    descriptorBase = (CritterColDescriptor *)
                     (*(u8 **)(*(u8 **)(header + offsetof(CritterPackedType, file)) +
                               offsetof(CritterFileHeader, nodes)) +
                      ((CritterPackedType *)header)->colBase * 0x50);
    c->unkAB8 = -1;
    zerof = lbl_80346470;
    for (i = 0; i < c->hdr->colCount;
         i++) {
        record = &c->hitnodes[i];
        record->descriptor = descriptorBase + i;
        nodeIndex = record->descriptor->nodeIndex;
        if ((s16)nodeIndex >= 0) {
            record->active = CritterColnodeAnimNode(c, nodeIndex);
        } else {
            record->active = NULL;
        }
        record->boundNode = record->active;
        if (record->active != NULL) {
            descriptor = record->descriptor;
            ch = *(s8 *)(name = descriptor->name);
            if (ch != 0) {
                if (ch == '-') {
                    s32 n = record->descriptor->name[1] - '0';
                    while (n > 0) {
                        record->boundNode =
                            ((MBObject *)record->boundNode)->parent;
                        n--;
                    }
                } else if (ch == '+') {
                    s32 n = record->descriptor->name[1] - '0';
                    while (n > 0) {
                        record->boundNode =
                            ((MBObject *)record->boundNode)->child;
                        n--;
                    }
                } else {
                    s32 idx = -1;
                    void *atc = *(void **)((u8 *)c->hdr +
                                offsetof(CritterPackedType, atree));
                    if (atc != NULL && name != NULL && ch != 0 &&
                        name[1] != 0) {
                        idx = AtreeFindNodeIdx(
                            *(void **)((u8 *)atc +
                                       offsetof(struct atreeheader, nodeinfo)),
                            *(s32 *)((u8 *)atc +
                                     offsetof(struct atreeheader, numnodes)),
                            name, 0x10);
                    }
                    record->boundNode = CritterColnodeAnimNode(c, idx);
                }
            }
        }
        MBTreeSetZsortAdd(record->active, record->descriptor->zsortParam, 1);
        record->state = -1;
        record->activeFrom = zerof;
        record->activeUntil = record->descriptor->healthScale * c->health;
        {
            u8 *psys;
            s32 sfxidx = record->descriptor->sfxIndex;
            void *hdr130 = *(void **)((u8 *)c->hdr +
                            offsetof(CritterPackedType, file));
            void *sfxparam =
                c->hdr->descriptor->model;
            if (sfxidx >= 0) {
                psys = *(u8 **)((u8 *)hdr130 + offsetof(CritterFileHeader, damage)) +
                       sfxidx * 0x50;
                CritterInitSfx(hdr130, ((CritterDamageDef *)psys)->sfxIndex, sfxparam);
                CritterInitSfx(hdr130, ((CritterDamageDef *)psys)->morphTargetIndex, sfxparam);
                CritterInitSfx(hdr130, ((CritterDamageDef *)psys)->morphIndex, sfxparam);
                CritterInitSfx(hdr130, ((CritterDamageDef *)psys)->sfx, sfxparam);
                if (((CritterDamageDef *)psys)->type == 6) {
                    lbl_80344650 = 1;
                }
            }
        }
        if ((gControllerButtons & 0x10) && gGameOptions[8]) {
            record->dmgfx = DmgFxCircleAdd(
                record->active, record->descriptor->radius,
                lbl_80346470, lbl_80346470,
                record->descriptor->position, 127);
        }
    }
}

/* 0x8003F1F0 -- instantiate every auxiliary animation tree attached to a
 * critter type and link the allocated records into the instance. */
static CritterSubnode *CritterNewAnimInst(void)
{
    s32 i;
    s32 total = lbl_80344668;

    for (i = 0; i < total; i++) {
        if (((CritterSubnode *)(lbl_802411B0 + i * 0x54))->mbnode == NULL) {
            break;
        }
    }
    if (i >= 1) {
        ErrorPrintf("Too many Critter Anim Insts: %d", i);
        return NULL;
    }
    if (i == total) {
        lbl_80344668 = lbl_80344668 + 1;
    }
    return (CritterSubnode *)(lbl_802411B0 + i * 0x54);
}

void CritterAddAnimInsts(Critter *c, f32 *matrix)
{
    u8 *node;
    CritterSubnode *record;
    CritterSubnode *tail;
    void *parent;

    node = *(u8 **)((u8 *)c->hdr + offsetof(CritterPackedType, attachments));
    while (node != NULL) {
        record = CritterNewAnimInst();
        if (record != NULL) {
            if (c->subnodes != NULL) {
                tail = (CritterSubnode *)c->subnodes;
                while (tail->next != NULL) {
                    tail = tail->next;
                }
                tail->next = record;
            } else {
                c->subnodes = record;
            }
            if (*(void **)(node + offsetof(CritterAddAnim, atree)) != NULL) {
                parent = lbl_8034473C;
                if ((*(s16 *)(node + offsetof(CritterAddAnim, flags)) & 1) != 0) {
                    if (*(s8 *)(node + offsetof(CritterAddAnim, attachNodeName)) != 0) {
                        parent = AtreeFindNode(&c->colhandle,
                                               (char *)(node + offsetof(CritterAddAnim, attachNodeName)), 8);
                        if (parent == NULL) {
                            parent = c->anim;
                        }
                    } else {
                        parent = c->anim;
                    }
                }
                record->mbnode = MBNewNode(parent, matrix, 1);
                *(f32 *)((u8 *)record->mbnode + offsetof(MBObject, mat[3][0])) = *(f32 *)(node + offsetof(CritterAddAnim, offset));
                *(f32 *)((u8 *)record->mbnode + offsetof(MBObject, mat[3][1])) = *(f32 *)(node + (offsetof(CritterAddAnim, offset) + 4));
                *(f32 *)((u8 *)record->mbnode + offsetof(MBObject, mat[3][2])) = *(f32 *)(node + (offsetof(CritterAddAnim, offset) + 8));
                record->atree =
                    AtreeInit(*(void **)(node + offsetof(CritterAddAnim, atree)), record, 0, 0x800);
                MBNodeSetParent(*(void **)record->atree, record->mbnode);
            } else {
                ErrorPrintf("Bad critter anim inst: %s", (char *)(node + offsetof(CritterAddAnim, name)));
            }
        }
        node = *(u8 **)(node + offsetof(CritterAddAnim, next));
    }
}

/* 0x8003F3AC -- allocate a load slot, read the file, and build its header. */
s32 CritterLoadFile(const char *wad, const char *name)
{
    s32 idx;
    idx = lbl_80344660++;
    lbl_80241060[idx] = AllocFile(wad, name);
    CritterInitHeader(&lbl_80241070[idx], lbl_80241060[idx]);
    return idx;
}

/* 0x8003F414 -- poll the active background model request and advance it to
 * the texture/model-finalization stage. */
s32 CritterLoadDone(s32 maxBytes)
{
    char buf[36];
    u8 *desc;
    char *fmtbase;
    s32 result;
    s32 *handle;
    s32 size;
    s32 i;

    result = 0;
    fmtbase = lbl_801120E0;
    desc = (u8 *)crit_load_desc;
    if (*(s16 *)(desc + offsetof(CritterDescriptor, loadState)) == 1) {
        if (MBOX_BGLoadModelDone() != 0) {
            *(s16 *)(desc + offsetof(CritterDescriptor, loadState)) = 2;
            switch (*(s16 *)(desc + offsetof(CritterDescriptor, type))) {
            case 3:
            case 8:
                sprintf(buf, &fmtbase[416], desc, (u8 *)gWorldData + 4);
                break;
            case 7:
                for (i = 0; i < 32; i += 4) {
                    s32 *entry = *(s32 **)((u8 *)lbl_8025776C + i);
                    if (*entry == 32) {
                        sprintf(buf, &fmtbase[432], desc, (u8 *)entry + 16);
                        break;
                    }
                }
                break;
            default:
                sprintf(buf, &fmtbase[448], desc);
                break;
            }
            size = FileSize(buf, lbl_8034664C);
            if (maxBytes != 0 && size > maxBytes) {
                size = maxBytes;
            }
            lbl_80344640 = StartFileRead(buf, lbl_8034664C, 0, size,
                                         *(s32 *)(desc + offsetof(CritterDescriptor, model)),
                                         (void *)CritterBGLoadFile);
        }
    } else {
        handle = lbl_80344640;
        if (handle != NULL) {
            if (*(handle += 4) != 0) {
                *handle = -1;
                *(s16 *)(desc + offsetof(CritterDescriptor, loadState)) = 3;
                fn_8001267C(*(s32 *)(desc + offsetof(CritterDescriptor, model)),
                            *(s16 *)(desc + offsetof(CritterDescriptor, modelIndex)), -1);
                InitTexMods(*(s32 *)(desc + offsetof(CritterDescriptor, model)),
                            *(s16 *)(desc + offsetof(CritterDescriptor, modelIndex)));
                result = 1;
            }
        } else {
            result = 1;
        }
    }
    return result;
}

/* 0x8003F5B4 -- advance a background loader unless it has finished (state 2). */
void CritterBGLoadFile(s32 *loader)
{
    if (loader[4] == 2) {
        return;
    }
    loader[1] += loader[2];
}

/* 0x8003F5D4 -- find the next unloaded critter resource and start its model
 * request.  Returns through the module globals consumed by CritterLoadDone. */
s32 CritterLoadStartNext(void)
{
    char buf[32];
    u8 *fmtbase;
    u8 *tableBase;
    s32 i;
    s32 j;
    CritterFileHeader *entry;
    CritterPackedType *sub;
    CritterDescriptor *desc;
    s32 offset;

    fmtbase = (u8 *)lbl_801120E0;
    tableBase = (u8 *)lbl_80241070;
    for (i = 0; i < lbl_80344660; i++) {
        entry = (CritterFileHeader *)(tableBase + i * 80);
        if (entry->state != 1) {
            continue;
        }
        for (j = 0; j < entry->typeCount; j++) {
            sub = &entry->types[j];
            desc = sub->descriptor;
            if (desc == NULL) {
                continue;
            }
            switch (desc->loadState) {
            case 0:
                break;
            case 1:
                switch (desc->type) {
                case 3:
                case 8:
                    sprintf(buf, (char *)&fmtbase[416], desc,
                            (u8 *)gWorldData + 4);
                    break;
                case 7:
                    for (offset = 0; offset < 32; offset += 4) {
                        s32 *e2 =
                            *(s32 **)((u8 *)lbl_8025776C + offset);
                        if (*e2 == 32) {
                            sprintf(buf, (char *)&fmtbase[432], desc,
                                    (u8 *)e2 + 16);
                            break;
                        }
                    }
                    break;
                default:
                    sprintf(buf, (char *)&fmtbase[448], desc);
                    break;
                }
                MBOX_BGLoadModelStart(buf, desc->modelIndex);
                crit_load_desc = desc;
                lbl_80344640 = NULL;
                return 1;
            case 2:
                return 1;
            case 3:
                CritterLoadFinish(sub);
                break;
            case 4:
                break;
            default:
                break;
            }
        }
        if (j == entry->typeCount) {
            entry->state = 2;
        }
    }
    return 0;
}

/* 0x8003F784 -- for every loaded type/subtype header, register each of its
 * moves via CritterAllocType. */
void CritterLoadAllTypes(s32 arg)
{
    s32 type;
    s32 sub;
    CritterFileHeader *hdr;

    for (type = 0; type < lbl_80344660; type++) {
        hdr = (CritterFileHeader *)lbl_80241070[type];
        if (hdr->state != 0) {
            for (sub = 0; sub < hdr->typeCount; sub++) {
                CritterAllocType(hdr,
                                 &((CritterPackedType *)hdr->types)[sub],
                                 arg);
            }
        }
    }
}

/* 0x8003F81C */
struct CritterHeader *CritterTypeLoaded(s32 type, s32 subtype)
{
    return gCritterHeaders[type][subtype];
}

/* 0x8003F83C -- bind one type header to its container resource and publish
 * it in the type/subtype lookup table. */
void CritterAllocType(void *hdr, void *move, s32 arg)
{
    char buf[32];
    CritterDescriptor *desc;
    u8 *fmtbase;
    s32 k;
#define M ((CritterPackedType *)move)

    M->file = (CritterFileHeader *)hdr;
    fmtbase = (u8 *)lbl_801120E0;
    desc = &((CritterDescriptor *)((CritterFileHeader *)hdr)->descriptors)
        [M->descriptorIndex];
    M->descriptor = desc;
    if (desc->modelIndex < 0) {
        switch (desc->type) {
        case 3:
        case 8:
            sprintf(buf, (char *)&fmtbase[416], desc, (u8 *)gWorldData + 4);
            break;
        case 7:
            for (k = 0; k < 8; k++) {
                s32 *e2 = lbl_8025776C[k];
                if (*e2 == 32) {
                    sprintf(buf, (char *)&fmtbase[432], desc, (u8 *)e2 + 16);
                    break;
                }
            }
            break;
        default:
            sprintf(buf, (char *)&fmtbase[448], desc);
            break;
        }
        if (arg != 0) {
            desc->modelIndex = LoadModel(buf, &desc->model, 0, -1);
            desc->loadState = 2;
            if (desc->model != NULL) {
                InitTexMods((s32)desc->model, desc->modelIndex);
                desc->loadState = 3;
            }
        } else {
            desc->modelIndex = fn_8005A1EC(buf, &desc->model);
            desc->loadState = 1;
        }
        desc->loadTick = lbl_80344664;
    }
    if (arg != 0) {
        CritterLoadFinish(M);
    } else {
        M->atree = NULL;
    }
    if (M->subtype >= 0) {
        gCritterHeaders[desc->type][M->subtype] =
            (struct CritterHeader *)M;
    }
#undef M
}

/* 0x8003F9F4 -- resolve the animation tree and named attachment nodes for a
 * type after its model resource has loaded. */
void CritterLoadFinish(CritterPackedType *header)
{
    CritterPackedType *parent;
    CritterAddAnim *attachment;
    void *atree;
    s32 index;
    f64 name[4];
    /* lint-allow-next-line FM003: measured frame slot -- deleting it moves CritterLoadFinish by 8 words at unchanged size; original local unrecovered */
    u8 unused[8];

    if (header->atree != NULL) {
        return;
    }
    if (header->parentIndex < 0) {
        sprintf((char *)name, "%s%s", header->descriptor->prefix,
                header->suffix);
        header->atree = AtreeMatch(header->descriptor->model, (char *)name, 0);
        if (header->atree == NULL) {
            ErrorPrintf("Critter can not find atree %s", (char *)name);
        }
    } else {
        parent = &header->file->types[header->parentIndex];
        header->atree = parent->atree;
        if (parent->atree == NULL) {
            FatalError("Child critter defined before parent", 0x800000);
        }
    }

    for (attachment = header->attachments; attachment != NULL;
         attachment = attachment->next) {
        attachment->atree = AtreeMatch(header->descriptor->model,
                                       attachment->name, 1);
    }

    atree = header->atree;
    index = -1;
    if (atree != NULL && header->nodeName0 != NULL &&
        header->nodeName0[0] != '\0' && header->nodeName0[1] != '\0') {
        index = AtreeFindNodeIdx(((struct atreeheader *)atree)->nodeinfo,
                                 ((struct atreeheader *)atree)->numnodes,
                                 header->nodeName0, 0x10);
    }
    header->node0Index = (s16)index;
    index = -1;
    atree = header->atree;
    if (atree != NULL && header->nodeName1 != NULL &&
        header->nodeName1[0] != '\0' && header->nodeName1[1] != '\0') {
        index = AtreeFindNodeIdx(((struct atreeheader *)atree)->nodeinfo,
                                 ((struct atreeheader *)atree)->numnodes,
                                 header->nodeName1, 0x10);
    }
    header->node1Index = (s16)index;
    index = -1;
    atree = header->atree;
    if (atree != NULL && header->nodeName2 != NULL &&
        header->nodeName2[0] != '\0' && header->nodeName2[1] != '\0') {
        index = AtreeFindNodeIdx(((struct atreeheader *)atree)->nodeinfo,
                                 ((struct atreeheader *)atree)->numnodes,
                                 header->nodeName2, 0x10);
    }
    header->node2Index = (s16)index;
}

/* 0x8003FBD0 -- initialize the move tables of every loaded type/subtype. */
void CritterInitAllMoves(void)
{
    s32 type;
    s32 sub;
    CritterFileHeader *hdr;

    for (type = 0; type < lbl_80344660; type++) {
        hdr = (CritterFileHeader *)lbl_80241070[type];
        for (sub = 0; sub < hdr->typeCount; sub++) {
            CritterInitMoves(&hdr->types[sub]);
        }
    }
}

/* 0x8003FC4C -- resolve animation/node names and every sound/particle
 * dependency referenced by a loaded type's move and collision tables. */
void CritterInitMoves(CritterPackedType *header)
{
    /* lint-allow-next-line FM003: measured frame slot -- deleting it moves CritterInitMoves by 5 words at unchanged size; original local unrecovered */
    volatile u8 unused[8];
    CritterFileHeader *container;
    void *atree;
    CritterMove *moves;
    CritterColDescriptor *colnodes;
    CritterMove *entry;
    s32 i;
    s32 index;

    atree = header->atree;
    container = header->file;
    moves = container->moves + header->moveIndex;
    colnodes = container->nodes + header->colBase;
    if (atree == NULL) {
        return;
    }
    if (header->moveCount > lbl_80344630) {
        lbl_80344630 = header->moveCount;
    }
    if (header->auxMoveCount > lbl_80344634) {
        lbl_80344634 = header->auxMoveCount;
    }
    if (header->colCount > lbl_80344638) {
        lbl_80344638 = header->colCount;
    }

    i = 0;
    while (i < header->moveCount) {
        entry = &moves[i];
        if (entry->type >= MOVE_STEPFIRST && entry->type <= MOVE_STEPLAST) {
            header->typeFlags |= 0x10000;
        }
        if (entry->seqidx >= 0) {
            goto next_move;
        }
        if (entry->anim[0] != '\0') {
            index = AtreeHeaderFindSeq(atree, entry->anim);
            entry->seqidx = (s16)index;
            if (entry->seqidx < 0) {
                ErrorPrintf(lbl_801122F0, header->descriptor->prefix,
                            entry->anim);
                entry->seqidx = 0;
            }
        }
        index = -1;
        {
            void *lookupAtree = header->atree;
        if (lookupAtree != NULL && entry->colnode != NULL &&
            entry->colnode[0] != '\0' &&
            entry->colnode[1] != '\0') {
            index = AtreeFindNodeIdx(((struct atreeheader *)lookupAtree)->nodeinfo,
                                     ((struct atreeheader *)lookupAtree)->numnodes,
                                     entry->colnode, 0x10);
        }
        }
        entry->nodeidx = (s16)index;

        index = entry->interruptAnim0;
        {
            CritterDamageDef *sfx;
            void *sfxHeader;
            sfxHeader = header->descriptor->model;
        if (index >= 0) {
            sfx = &container->damage[index];
            CritterInitSfx(container, sfx->sfxIndex, sfxHeader);
            CritterInitSfx(container, sfx->morphTargetIndex, sfxHeader);
            CritterInitSfx(container, sfx->morphIndex, sfxHeader);
            CritterInitSfx(container, sfx->sfx, sfxHeader);
            if (sfx->type == 6) {
                lbl_80344650 = 1;
            }
        }
        }
        {
            CritterDamageDef *sfx;
            void *sfxHeader;
            index = entry->interruptAnim1;
            sfxHeader = header->descriptor->model;
        if (index >= 0) {
            sfx = &container->damage[index];
            CritterInitSfx(container, sfx->sfxIndex, sfxHeader);
            CritterInitSfx(container, sfx->morphTargetIndex, sfxHeader);
            CritterInitSfx(container, sfx->morphIndex, sfxHeader);
            CritterInitSfx(container, sfx->sfx, sfxHeader);
            if (sfx->type == 6) {
                lbl_80344650 = 1;
            }
        }
        }
        CritterInitSfx(container, entry->sfx,
                       header->descriptor->model);
        CritterInitSfx(container, entry->sfx2,
                       header->descriptor->model);
next_move:
        i++;
    }

    {
    s32 colIndex = 0;
    while (colIndex < header->colCount) {
        CritterColDescriptor *colnode;
        void *lookupAtree;
        colnode = &colnodes[colIndex];
        index = -1;
        lookupAtree = header->atree;
        if (lookupAtree != NULL && colnode != NULL &&
            colnode->nodeName[0] != '\0' && colnode->nodeName[1] != '\0') {
            index = AtreeFindNodeIdx(((struct atreeheader *)lookupAtree)->nodeinfo,
                                     ((struct atreeheader *)lookupAtree)->numnodes,
                                     colnode->nodeName, 0x10);
        }
        colnode->nodeIndex = (s16)index;
        colIndex++;
    }
    }

    CritterInitSfx(container, header->sfxIndex0, header->descriptor->model);
    CritterInitSfx(container, header->sfxIndex1, header->descriptor->model);
    header->movesPtr = moves;
    header->colnodesPtr = colnodes;
    header->patternsPtr = container->patterns + header->patternIndex;
}

/* 0x8003FF98 -- lazily resolve one sound/particle descriptor and recursively
 * initialize its linked descriptor. */
void CritterInitSfx(void *file, s32 index, void *atreeHeader)
{
    CritterSfxRecord *entry;
    s32 model;
    char name[32];

    if (index < 0) {
        return;
    }
    entry = &((CritterFileHeader *)file)->sfx[index];
    if (entry->textureId < 0) {
        if ((entry->flags &
             0x0F000100) != 0) {
            if (entry->name[0] != '\0') {
                entry->textureId =
                    FindTexMod(atreeHeader,
                               entry->name,
                               NULL);
                if (entry->textureId <= 0) {
                    model = AtreeModel(atreeHeader);
                    entry->textureId =
                        MBOX_FindTexture_Sub(
                            entry->name,
                            NULL, model, model, -1);
                }
                if (entry->textureId <= 0) {
                    entry->textureId =
                        MBOX_FindTexture(
                            entry->name,
                            NULL);
                }
            } else {
                entry->textureId = -1;
            }
        } else {
            entry->textureId =
                InitCustomEffect(
                    atreeHeader,
                    entry->name,
                    entry->custom0,
                    entry->custom1);
        }
    }
    if (entry->audioId < 0) {
        if (entry->levelFmt[0] != '\0') {
            sprintf(name, entry->levelFmt,
                    gCurLevel->name[0]);
            entry->audioId =
                AudioFindSound(name, 0, 1);
        } else {
            entry->audioId = -1;
        }
    }
    CritterInitSfx(file, entry->linkIndex,
                   atreeHeader);
}
extern char lbl_8034665C[8]; /* "SFXX" */
extern char lbl_80346664[8]; /* "DAMG" */
extern char lbl_8034666C[8]; /* "MOVE" */
extern char lbl_80346674[8]; /* "PTRN" */
extern char lbl_8034667C[8]; /* "NODE" */
extern char lbl_80346684[8]; /* "DESC" */
extern char lbl_8034668C[8]; /* "TYPE" */
extern char lbl_80346694[8]; /* "ADDA" */
static inline s32 CritterWadTag(char *s)
{
    return (s[0] << 24) | (s[1] << 16) | (s[2] << 8) | s[3];
}

#define CRITTER_SFX_TAG(s) \
    (((s)[0] << 24) | ((s)[1] << 16) | ((s)[2] << 8) | (s)[3])

static inline u16 CritterSwap16(u16 v)
{
    u8 *p = (u8 *)&v;
    return (u16)(p[0] | (p[1] << 8));
}

static inline u32 CritterSwap32(u32 v)
{
    u32 r;
    u8 *s = (u8 *)&v;
    u8 *d = (u8 *)&r;
    d[0] = s[3];
    d[1] = s[2];
    d[2] = s[1];
    d[3] = s[0];
    return r;
}

static inline f32 CritterSwapF(f32 v)
{
    u32 r = CritterSwap32(*(u32 *)&v);
    return *(f32 *)&r;
}

/* 0x800400F0 -- open all CRITTER WAD sections, convert little-endian
 * serialized records in place, reset runtime-only fields, and build each
 * type's linked auxiliary-animation list. */
void CritterInitHeader(void *hdr, void *file)
{
    CritterFileHeader *header;
    s32 *wad;
    s32 swapped;
    s32 i;
    CritterAddAnim *anim;
    s32 j;
    CritterPackedType *owner;
    CritterAddAnim *tail;
    s32 typeIndex;

    header = (CritterFileHeader *)hdr;
    swapped = 0;
    if (header->state == 0) {
        wad = header->wad;
        swapped = MBSetupWad(wad, (s32)file);
        header->sfx = (CritterSfxRecord *)MBGetFromWad(wad,
                                         CRITTER_SFX_TAG(lbl_8034665C),
                                         &header->sfxCount);
        header->damage = (CritterDamageDef *)MBGetFromWad(wad,
                                            CritterWadTag(lbl_80346664),
                                            &header->damageCount);
        header->moves = (CritterMove *)MBGetFromWad(wad,
                                           CritterWadTag(lbl_8034666C),
                                           &header->moveCount);
        header->patterns = (CritterPattern *)MBGetFromWad(wad,
                                              CritterWadTag(lbl_80346674),
                                              &header->patternCount);
        header->nodes = (CritterColDescriptor *)MBGetFromWad(wad,
                                           CritterWadTag(lbl_8034667C),
                                           &header->nodeCount);
        header->descriptors = (CritterDescriptor *)MBGetFromWad(wad,
                                                 CritterWadTag(lbl_80346684),
                                                 &header->descriptorCount);
        header->types = (CritterPackedType *)MBGetFromWad(wad,
                                           CritterWadTag(lbl_8034668C),
                                           &header->typeCount);
        header->addAnims = (struct CritterAddAnim *)MBGetFromWad(wad,
                                              CritterWadTag(lbl_80346694),
                                              &header->addAnimCount);
        if (header->types == NULL) {
            FatalError("Critter Header has no types", 0x800000);
        }
        header->state = 1;
    }

    if ((u8)swapped) {
        for (i = 0; i < header->sfxCount; i++) {
            CritterSfxRecord *rec = &header->sfx[i];
            rec->custom0 = CritterSwap16(rec->custom0);
            rec->custom1 = CritterSwap16(rec->custom1);
            rec->flags = CritterSwap32(rec->flags);
            rec->linkIndex = CritterSwap32(rec->linkIndex);
            rec->textureId = CritterSwap32(rec->textureId);
            rec->audioId = CritterSwap32(rec->audioId);
            rec->life = CritterSwapF(rec->life);
            rec->rate = CritterSwapF(rec->rate);
            rec->tintColor = CritterSwap32(rec->tintColor);
            rec->scale = CritterSwapF(rec->scale);
            for (j = 0; j < 3; j++) {
                rec->color[j] = CritterSwapF(rec->color[j]);
            }
        }

        for (i = 0; i < header->damageCount; i++) {
            CritterDamageDef *def = &header->damage[i];
            def->type = CritterSwap16(def->type);
            def->behaviorFlags = CritterSwap16(def->behaviorFlags);
            def->sfxIndex = CritterSwap16(def->sfxIndex);
            def->sfx = CritterSwap16(def->sfx);
            def->morphTargetIndex = CritterSwap16(def->morphTargetIndex);
            def->morphIndex = CritterSwap16(def->morphIndex);
            def->radius = CritterSwapF(def->radius);
            def->maxDistance = CritterSwapF(def->maxDistance);
            def->minDistance = CritterSwapF(def->minDistance);
            def->yaw = CritterSwapF(def->yaw);
            def->mindp = CritterSwapF(def->mindp);
            def->pitch = CritterSwapF(def->pitch);
            def->damage = CritterSwapF(def->damage);
            def->minSpeed = CritterSwapF(def->minSpeed);
            def->maxSpeed = CritterSwapF(def->maxSpeed);
            def->gravity = CritterSwapF(def->gravity);
            def->morphSpeed = CritterSwapF(def->morphSpeed);
            def->yawSpread = CritterSwapF(def->yawSpread);
            def->flags = CritterSwap32(def->flags);
            for (j = 0; j < 3; j++) {
                def->offset[j] =
                    CritterSwapF(def->offset[j]);
            }
        }

        for (i = 0; i < header->moveCount; i++) {
            CritterMove *mv = &header->moves[i];
            mv->seqidx = CritterSwap16(mv->seqidx);
            mv->nodeidx = CritterSwap16(mv->nodeidx);
            mv->interruptAnim0 = CritterSwap16(mv->interruptAnim0);
            mv->interruptAnim1 = CritterSwap16(mv->interruptAnim1);
            mv->frameEnd = CritterSwap16(mv->frameEnd);
            mv->frameEnd2 = CritterSwap16(mv->frameEnd2);
            mv->link = CritterSwap16(mv->link);
            mv->interrupt = CritterSwap16(mv->interrupt);
            mv->sfx = CritterSwap16(mv->sfx);
            mv->sfxFrame = CritterSwap16(mv->sfxFrame);
            mv->sfx2 = CritterSwap16(mv->sfx2);
            mv->sfx2Frame = CritterSwap16(mv->sfx2Frame);
            mv->flags = CritterSwap32(mv->flags);
            mv->priority = CritterSwap32(mv->priority);
            mv->frameStart = CritterSwap32(mv->frameStart);
            mv->frameStart2 = CritterSwap32(mv->frameStart2);
            mv->framePeriod = CritterSwapF(mv->framePeriod);
            mv->cooldown = CritterSwapF(mv->cooldown);
            mv->readyDistance = CritterSwapF(mv->readyDistance);
            mv->turnRate = CritterSwapF(mv->turnRate);
            mv->holdDuration = CritterSwapF(mv->holdDuration);
            mv->type = CritterSwap32(mv->type);
            mv->target.minDistance = CritterSwapF(mv->target.minDistance);
            mv->target.maxDistance = CritterSwapF(mv->target.maxDistance);
            mv->target.yaw = CritterSwapF(mv->target.yaw);
            mv->target.minDot = CritterSwapF(mv->target.minDot);
            mv->target.minRateScale = CritterSwapF(mv->target.minRateScale);
            mv->target.maxRateScale = CritterSwapF(mv->target.maxRateScale);
            mv->target.idleGate = CritterSwapF(mv->target.idleGate);
            mv->target.maxVertical = CritterSwapF(mv->target.maxVertical);
        }

        for (i = 0; i < header->patternCount; i++) {
            CritterPattern *pat = &header->patterns[i];
            pat->flags = CritterSwap16(pat->flags);
            pat->unk12 = CritterSwap16(pat->unk12);
            pat->cooldown = CritterSwapF(pat->cooldown);
            pat->target.minDistance = CritterSwapF(pat->target.minDistance);
            pat->target.maxDistance = CritterSwapF(pat->target.maxDistance);
            pat->target.yaw = CritterSwapF(pat->target.yaw);
            pat->target.minDot = CritterSwapF(pat->target.minDot);
            pat->target.minRateScale = CritterSwapF(pat->target.minRateScale);
            pat->target.maxRateScale = CritterSwapF(pat->target.maxRateScale);
            pat->target.idleGate = CritterSwapF(pat->target.idleGate);
            pat->target.maxVertical = CritterSwapF(pat->target.maxVertical);
            for (j = 0; j < 8; j++) {
                pat->moveidx[j] = CritterSwap16(pat->moveidx[j]);
            }
        }

        for (i = 0; i < header->nodeCount; i++) {
            CritterColDescriptor *nodeDef = &header->nodes[i];
            nodeDef->flags = CritterSwap16(nodeDef->flags);
            nodeDef->sfxIndex = CritterSwap16(nodeDef->sfxIndex);
            nodeDef->nodeIndex = CritterSwap16(nodeDef->nodeIndex);
            nodeDef->zsortParam = CritterSwap16(nodeDef->zsortParam);
            nodeDef->maxTargetDistance = CritterSwapF(nodeDef->maxTargetDistance);
            nodeDef->targetScoreScale = CritterSwapF(nodeDef->targetScoreScale);
            nodeDef->radius = CritterSwapF(nodeDef->radius);
            nodeDef->damageScale = CritterSwapF(nodeDef->damageScale);
            nodeDef->healthScale = CritterSwapF(nodeDef->healthScale);
            for (j = 0; j < 3; j++) {
                nodeDef->position[j] =
                    CritterSwapF(nodeDef->position[j]);
            }
        }

        for (i = 0; i < header->descriptorCount; i++) {
            CritterDescriptor *desc = &header->descriptors[i];
            desc->type = CritterSwap16(desc->type);
            desc->loadState = CritterSwap16(desc->loadState);
            desc->loadTick = CritterSwap16(desc->loadTick);
            *(u32 *)&desc->model = CritterSwap32(*(u32 *)&desc->model);
        }

        for (i = 0; i < header->typeCount; i++) {
            CritterPackedType *type = &header->types[i];
            type->descriptorIndex = CritterSwap16(type->descriptorIndex);
            type->subtype = CritterSwap16(type->subtype);
            type->node0Index = CritterSwap16(type->node0Index);
            type->node1Index = CritterSwap16(type->node1Index);
            type->node2Index = CritterSwap16(type->node2Index);
            type->sfxIndex0 = CritterSwap16(type->sfxIndex0);
            type->sfxIndex1 = CritterSwap16(type->sfxIndex1);
            type->meterX = CritterSwap16(type->meterX);
            type->meterY = CritterSwap16(type->meterY);
            type->meterW = CritterSwap16(type->meterW);
            type->meterH = CritterSwap16(type->meterH);
            type->moveCount = CritterSwap16(type->moveCount);
            type->moveIndex = CritterSwap16(type->moveIndex);
            type->auxMoveCount = CritterSwap16(type->auxMoveCount);
            type->patternIndex = CritterSwap16(type->patternIndex);
            type->colCount = CritterSwap16(type->colCount);
            type->colBase = CritterSwap16(type->colBase);
            type->childIndex = CritterSwap16(type->childIndex);
            type->parentIndex = CritterSwap16(type->parentIndex);
            type->typeFlags = CritterSwap32(type->typeFlags);
            type->lookYawRate0 = CritterSwapF(type->lookYawRate0);
            type->lookYawRate1 = CritterSwapF(type->lookYawRate1);
            type->lookPitchRate0 = CritterSwapF(type->lookPitchRate0);
            type->lookPitchRate1 = CritterSwapF(type->lookPitchRate1);
            type->lookPitchBias0 = CritterSwapF(type->lookPitchBias0);
            type->lookPitchBias1 = CritterSwapF(type->lookPitchBias1);
            type->radius = CritterSwapF(type->radius);
            type->wallRadius = CritterSwapF(type->wallRadius);
            type->speed = CritterSwapF(type->speed);
            type->floorOffset = CritterSwapF(type->floorOffset);
            type->vertDrift = CritterSwapF(type->vertDrift);
            type->damageScale = CritterSwapF(type->damageScale);
            type->armor = CritterSwapF(type->armor);
            type->turnLimit = CritterSwapF(type->turnLimit);
            type->unkDC = CritterSwapF(type->unkDC);
            type->shieldFlags = CritterSwap32(type->shieldFlags);
            type->maxHealth = CritterSwapF(type->maxHealth);
            type->expValue = CritterSwapF(type->expValue);
            type->wakeThreshold = CritterSwapF(type->wakeThreshold);
            type->unkF0 = CritterSwapF(type->unkF0);
            *(u32 *)&type->descriptor = CritterSwap32(*(u32 *)&type->descriptor);
            *(u32 *)&type->movesPtr = CritterSwap32(*(u32 *)&type->movesPtr);
            *(u32 *)&type->patternsPtr = CritterSwap32(*(u32 *)&type->patternsPtr);
            *(u32 *)&type->colnodesPtr = CritterSwap32(*(u32 *)&type->colnodesPtr);
            *(u32 *)&type->file = CritterSwap32(*(u32 *)&type->file);
            *(u32 *)&type->atree = CritterSwap32(*(u32 *)&type->atree);
            type->target.minDistance = CritterSwapF(type->target.minDistance);
            type->target.maxDistance = CritterSwapF(type->target.maxDistance);
            type->target.yaw = CritterSwapF(type->target.yaw);
            type->target.minDot = CritterSwapF(type->target.minDot);
            type->target.minRateScale = CritterSwapF(type->target.minRateScale);
            type->target.maxRateScale = CritterSwapF(type->target.maxRateScale);
            type->target.idleGate = CritterSwapF(type->target.idleGate);
            type->target.maxVertical = CritterSwapF(type->target.maxVertical);
            for (j = 0; j < 3; j++) {
                type->defaultPos[j] =
                    CritterSwapF(type->defaultPos[j]);
                type->originOffset[j] =
                    CritterSwapF(type->originOffset[j]);
                type->unkD0[j] =
                    CritterSwapF(type->unkD0[j]);
                type->healthbarOffset[j] =
                    CritterSwapF(type->healthbarOffset[j]);
            }
        }

        for (i = 0; i < header->addAnimCount; i++) {
            CritterAddAnim *anim = &header->addAnims[i];
            anim->typeIndex = CritterSwap16(anim->typeIndex);
            anim->flags = CritterSwap16(anim->flags);
            *(u32 *)&anim->atree = CritterSwap32(*(u32 *)&anim->atree);
            *(u32 *)&anim->next = CritterSwap32(*(u32 *)&anim->next);
            for (j = 0; j < 3; j++) {
                anim->offset[j] =
                    CritterSwapF(anim->offset[j]);
            }
        }
    }

    for (i = 0; i < header->descriptorCount; i++) {
        header->descriptors[i].modelIndex = -1;
    }
    for (i = 0; i < header->typeCount; i++) {
        header->types[i].attachments = NULL;
    }
    for (i = 0; i < header->addAnimCount; i++) {
        anim = &header->addAnims[i];
        if ((typeIndex = anim->typeIndex) > header->typeCount) {
            ErrorPrintf("CRITTER: AddAnim has addto idx %d > max %d",
                        typeIndex, header->typeCount);
        } else {
            owner = &header->types[typeIndex];
            if (owner->attachments != NULL) {
                tail = owner->attachments;
                while (tail->next != NULL) {
                    tail = tail->next;
                }
                tail->next = anim;
            } else {
                owner->attachments = anim;
            }
        }
    }
}
