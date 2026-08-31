/*
 * atree.c -- GCN ATREE.OBJ: animation-tree construction / playback.
 *
 * Real names are from the Xbox build's ATREE.OBJ (shell3D.pdb).  The GCN
 * .text lays functions out in reverse source order (DoTexMods lowest,
 * AnimDataNodeNew highest).
 *
 * Status: the lookup/teardown/pool families are translated, including the
 * player-model match wrapper at fn_80011BBC.  The core animation-evaluation
 * chain (DoAnimateTreeFrame, DoAnimateTree, and AnimateNode) is translated;
 * AtreeInitSub and AtreeNodeInit now cover the runtime construction path.
 * The WORLDPSYS fixup fn_80011DCC is byte-exact; the resource-header fixup
 * fn_8001267C remains translated but still needs a code-generation pass.
 *
 * .text       0x80010A4C..0x800137BC
 * extab       0x80005590..0x80005678
 * extabindex  0x800088A8..0x80008A04
 */

#include "types.h"
#include "game/mbobject.h"

#ifndef offsetof
#define offsetof(type, memb) ((u32) & ((type*)0)->memb)
#endif

/* -- TEXMOD: special texture-mod record (see g3d/auxanim.c) -- */
typedef struct TEXMOD {
    /* 0x00 */ s16 flag;
    /* 0x02 */ s16 scrollIdx;
    /* 0x04 */ char name1[0x20];
    /* 0x24 */ char name2[0x20];
    /* 0x44 */ s32 tex;
    /* 0x48 */ s32 src;
    /* 0x4C */ s16 frames;
    /* 0x4E */ s16 unk4e;
    /* 0x50 */ s32 rate;
    /* 0x54 */ s32 counter;
} TEXMOD; /* 0x58 */

/* -- atreeseq: one animation sequence (name + texmod list) -- */
typedef struct atreeseq {
    /* 0x00 */ char name[0x08];
    /* 0x08 */ s32 ntexmods;
    /* 0x0C */ TEXMOD* texmods;
    /* 0x10 */ u8 _pad[0x20];
} atreeseq; /* 0x30 */

/* -- anodeinfo: per-node name/description table entry -- */
typedef struct anodeinfo {
    /* 0x00 */ char name[0x3C];
} anodeinfo; /* 0x3C */

/* -- animdata: playback record in the AnimData pool (stride 0xA0).
 * Xbox PDB (graphics.h) names the record's other fields seq/used/pidx/nidx/
 * keycount/ppyr/npyr/xpyr/ppos/npos/xpos/pscale/nscale/xscale; only ppos and
 * xpos (previous/transitional position, GC-verified against AnimFixPos's
 * root-motion X/Z zeroing at +0x40/+0x48/+0x60/+0x68) are named here, and the
 * pre-existing `inuse` name (offset 0x04, PDB "used") is kept as-is rather
 * than renamed to match the PDB spelling. -- */
typedef struct animdata {
    /* 0x00 */ u8 _pad0[4];
    /* 0x04 */ s32 inuse;
    /* 0x08 */ u8 _pad8[0x38];
    /* 0x40 */ f32 ppos[4];
    /* 0x50 */ u8 _pad50[0x10];
    /* 0x60 */ f32 xpos[4];
    /* 0x70 */ u8 _pad70[0x30];
} animdata; /* 0xA0 */

/* -- anode: one animation-tree node -- */
typedef struct anode {
    /* 0x00 */ void* obj;
    /* 0x04 */ struct anode* parent;
    /* 0x08 */ struct anode* child;
    /* 0x0C */ struct anode* next;
    /* 0x10 */ f32 x;
    /* 0x14 */ f32 y;
    /* 0x18 */ f32 z;
    /* 0x1C */ void* anim;
    /* 0x20 */ s32 type;
    /* 0x24 */ f32 frame;
} anode; /* 0x28 */

typedef struct animseqdesc {
    /* 0x00 */ u8 _pad00[0x24];
    /* 0x24 */ s16 wraps;
    /* 0x26 */ u8 _pad26[2];
    /* 0x28 */ s16 ntexmods;
    /* 0x2A */ s16 flags;
    /* 0x2C */ TEXMOD* texmods;
} animseqdesc; /* 0x30 */

/* -- animinfo: atree playback state (graphics.h Id=3256, 0x38) -- */
typedef struct animinfo {
    /* 0x00 */ animseqdesc* seqheader;
    /* 0x04 */ void* animheader;
    /* 0x08 */ void* oanimheader;
    /* 0x0C */ s16 numseqs;
    /* 0x0E */ s16 animseq;
    /* 0x10 */ s16 numframes;
    /* 0x12 */ s8 setpanim;
    /* 0x13 */ u8 flags;
    /* 0x14 */ f32 transfrac;
    /* 0x18 */ f32 frame;
    /* 0x1C */ s16 animseq0;
    /* 0x1E */ s16 active;
    /* 0x20 */ f32 starttime;
    /* 0x24 */ f32 transtime;
    /* 0x28 */ f32 animscale;
    /* 0x2C */ f32 seqscale;
    /* 0x30 */ f32 atime;
    /* 0x34 */ s16 repeat;
    /* 0x36 */ u16 stage;
} animinfo; /* 0x38 */

/* -- atree: animation-tree instance (misc.h Id=2219, 0x48) -- */
typedef struct atree {
    /* 0x00 */ anode* root;
    /* 0x04 */ animinfo animinfo;
    /* 0x3C */ s32 nanodes;
    /* 0x40 */ anode* firstanode;
    /* 0x44 */ anodeinfo* anodeinfo;
} atree; /* 0x48 */

/* -- animheader: sequence-name table header (AtreeHeaderFindSeq) -- */
typedef struct animheader {
    /* 0x00 */ atreeseq* seqs;
    /* 0x04 */ u8 _pad[0x10];
    /* 0x14 */ s32 numseqs;
} animheader;

/* -- atreematch / atreeheader: name->node match table -- */
typedef struct atreematch {
    /* 0x00 */ char name[0x20];
    /* 0x20 */ s32 offset;
} atreematch; /* 0x24 */

typedef struct atreeheader {
    /* 0x00 */ s16 num;
    /* 0x04 */ atreematch* list;
} atreeheader;

/* -- one serialized node-description record (stride 0x3C) -- */
typedef struct AtreeNodeDef {
    /* 0x00 */ char name[0x14];
    /* 0x14 */ f32 audioParams[3];
    /* 0x20 */ f32 position[3];
    /* 0x2C */ s16 type;
    /* 0x2E */ s16 flags;
    /* 0x30 */ u32 treeFlags;
    /* 0x34 */ s32 dataOffset;
    /* 0x38 */ s32 parent;
} AtreeNodeDef; /* 0x3C */

/* The first three animinfo fields double as the node-data base table. */
typedef struct AtreeDataBases {
    u8* type3;
    u8* animData;
    u8* type2;
} AtreeDataBases;

/* -- one selected tree blob inside an atree resource -- */
typedef struct AtreeDefinition {
    /* 0x00 */ animseqdesc* seqheader;
    /* 0x04 */ void* animheader;
    /* 0x08 */ void* oanimheader;
    /* 0x0C */ AtreeNodeDef* nodes;
    /* 0x10 */ s32 nodeCount;
    /* 0x14 */ s32 sequenceCount;
    /* 0x18 */ char objectPrefix[0x1E];
    /* 0x36 */ s16 objectIndex;
} AtreeDefinition; /* 0x38 */

/* ================= external helpers ================= */
extern int strcmp(const char* a, const char* b);
extern int strncmp(const char* a, const char* b, u32 n);
extern int strncat(char* dst, const char* src, u32 n);
extern u32 strlen(const char* string);
extern void ErrorPrintf(char* fmt, ...);
extern void FatalError(char* msg, int code);
extern void* AllocMem(u32 size);
extern void DoTexModSub(TEXMOD* tm);
extern void InitTexMod(TEXMOD* tm, int texidx);
extern int strncpy(char* dst, const char* src, u32 n);
extern void* MBRemoveNode(void* obj, int flag);
extern void MBNodeSetParent(void* child, void* parent);
extern s32 MBOX_NewObject(const char* name, s32 arg1, s32 arg2, s32 arg3);
extern s32 MBOX_ReallyFindObject(const char* name, s32 arg1, s32 arg2,
                                s32 create);
extern void* MBNewObject(s32 object, s32 arg1, void* parent, s32 arg3);
extern void MBTreeSetFlags(void* object, u32 flags, s32 recurse);
extern void MBTreeClearFlags(void* object, u32 flags, s32 recurse);
extern void InitAnimData(u32* data, u32 frameData);
extern void InitAnimInfo(animinfo* info, u8 flags);
extern void* AudioSetListenerPos(s32* object, s32 frameData, f32* params);
extern s32 AnimateTreeFrame(f32 time, animinfo* info, s32 seq, s32 lo, s32 hi);
extern void DoTexModSeqSub(void* context, TEXMOD* texmod, s32 frame);
extern void DoObjAnimation(void* animation, s32 object, s32 sequence, s32 frame);
extern u32 DoAnimation(s32* animation, animinfo* info, f32* matrix,
                       s32* rotation, f32* position);
extern s32 AnimateTree(f32 time, animinfo* info, s32 sequence, s32 first,
                       s32 last);
extern void WorldVector(f32* source, f32* result, f32* matrix);
extern const f32 sAtreeZero;
extern const f64 sAtreeFrameRoundBias;
DECL_SECT(".sdata2") extern const char sAtreeDummyName[];

/* intra-TU forward declarations (address-order names retained) */
anode* AtreeNodeLastSibling(anode* node);
anode* AtreeNodePrevNode(anode* node, anode* list);
void AtreeRemovePsysSub(anode* node);
void AtreeRemoveNodeChild(anode* node);
void AtreeRemoveNodeSub(anode* node);
anode* AtreeRemoveNode(anode* node, int keep, anode* root);
anode* AtreeInitSub(AtreeDefinition* definition, atree* tree,
                    const char* objectPrefix, u32 treeFlags, s32 reportError);
void AtreeNodeInsert(anode* node, anode* parent, anode* root);
anode* AtreeNewNode(s32 count);
void AtreeNodeInit(anode* node, anode* parent, const char* name,
                   AtreeDataBases* bases, AtreeNodeDef* def, s32 objectIndex);
s32 DoAnimateTree(f32 frame, atree* tree, s32 sequence, s32 first, s32 last,
                  s32 recurse);
void AnimateNode(anode* node, animinfo* info, s32 recurse);
animdata* AnimDataNodeNew(void);

/* ================= file-scope state ================= */
static s32 AtreeNumNodes;
static s32 AtreeNodeFirstFree;
static anode* AtreeNodeList;
static s32 AnodeMax;
static s32 AnimDataNum;
static s32 AnimDataFirstFree;
static animdata* AnimDataList;
static s32 AnimDataMax;
static s32 AtreeNodePeak;
static s32 AnimDataPeak;
static s32 natreelists;

static struct {
    s32 natreelists[8];
    anode* nodelist[8];
    animdata* datalist[8];
} atreelist_save = {{0}, {0}, {0}};

static s32 atree_handles[24];
static u8 atree_scroll[24][16];
static void* whichatree[24];

extern u32 gErrorCode;

#define STUB(address, name) void name(void) {}

/* ---------------- texmod ops ---------------- */

void DoTexMods(atreeseq* seq)
{
    int i;
    TEXMOD* tm;

    if (seq != NULL) {
        for (i = 0; i < seq->ntexmods; i++) {
            tm = (TEXMOD*)((char*)seq->texmods + i * 0x58);
            if (tm->flag == -1) {
                DoTexModSub(tm);
            }
        }
    }
}

s32 FindTexMod(atreeseq* seq, char* name, TEXMOD** out)
{
    int i;
    TEXMOD* tm;

    if (seq != NULL) {
        for (i = 0; i < seq->ntexmods; i++) {
            tm = (TEXMOD*)((char*)seq->texmods + i * 0x58);
            if (strncmp(tm->name1, name, 0x20) == 0) {
                if (out != NULL) {
                    *out = tm;
                }
                return tm->src;
            }
        }
    }
    if (out != NULL) {
        *out = NULL;
    }
    return 0;
}

void InitTexMods(atreeseq* seq, int texidx)
{
    int i;
    TEXMOD* tm;

    if (seq != NULL) {
        if (texidx >= 0) {
            goto init;
        }
        if (texidx < 0) {
            goto done;
        }
init:
        for (i = 0; i < seq->ntexmods; i++) {
            tm = (TEXMOD*)((char*)seq->texmods + i * 0x58);
            InitTexMod(tm, texidx);
        }
    }
done:
    ;
}

/* ---------------- node/seq lookup ---------------- */

s32 AtreeFindNodeIdx(anodeinfo* info, int nnodes, char* name, int len)
{
    int i;

    if (name == NULL || *name == '\0') {
        return -1;
    }
    for (i = 0; i < nnodes; i++) {
        if (strncmp((char*)info + i * 0x3C, name, len - 1) == 0) {
            return i;
        }
    }
    ErrorPrintf("AtreeFindNodeIdx can not find node %s", name);
    return -1;
}

void* AtreeFindNode(atree* tree, char* name, int len)
{
    void* result;
    int idx;

    idx = AtreeFindNodeIdx(tree->anodeinfo, tree->nanodes, name, len);
    if (idx >= 0) {
        result = tree->firstanode[idx].obj;
    } else {
        result = NULL;
    }
    return result;
}

s32 AtreeHeaderFindSeq(animheader* hdr, char* name)
{
    int i;
    atreeseq* seqs;

    seqs = hdr->seqs;
    for (i = 0; i < hdr->numseqs; i++) {
        if (strcmp((char*)seqs + i * 0x30, name) == 0) {
            return i;
        }
    }
    return -1;
}

s32 AtreeFindSeq(atree* tree, char* name)
{
    int i;
    animinfo* ai;
    atreeseq* seqs;

    ai = &tree->animinfo;
    seqs = (atreeseq*)ai->seqheader;
    for (i = 0; i < ai->numseqs; i++) {
        if (strcmp((char*)seqs + i * 0x30, name) == 0) {
            return i;
        }
    }
    return -1;
}

/* ---------------- node/animdata pools ---------------- */

void AtreeSetEmpty(void)
{
    int i;

    if (AtreeNodeList != NULL) {
        for (i = 0; i < AnodeMax; i++) {
            AtreeNodeList[i].type = -1;
        }
    }
    if (AnimDataList != NULL) {
        for (i = 0; i < AnimDataMax; i++) {
            AnimDataList[i].inuse = 0;
        }
    }
}

void AtreeAlloc(int nnodes, int ndata)
{
    if (AtreeNodeList == NULL) {
        if (nnodes < 0) {
            nnodes = 0xE00;
        }
        AnodeMax = nnodes;
        AtreeNodeList = AllocMem(nnodes * 0x28);
    }
    if (AnimDataList == NULL) {
        if (ndata < 0) {
            ndata = 0xC00;
        }
        AnimDataMax = ndata;
        AnimDataList = AllocMem(ndata * 0xA0);
    }
    AtreeSetEmpty();
}

/* ---------------- atree-list slot save / restore ---------------- */

#pragma dont_inline on
void AtreeInitLists(int slot)
{
    int i;

    natreelists = atreelist_save.natreelists[slot];
    for (i = natreelists; i < 24; i++) {
        whichatree[i] = NULL;
    }
    AtreeNumNodes = 0;
    AtreeNodeFirstFree = 0;
    AtreeNodeList = atreelist_save.nodelist[slot];
    AnimDataNum = 0;
    AnimDataFirstFree = 0;
    AnimDataList = atreelist_save.datalist[slot];
    AtreeSetEmpty();
}
#pragma dont_inline reset

void AtreeListLock(int slot)
{
    atreelist_save.natreelists[slot] = natreelists;
    atreelist_save.nodelist[slot] = AtreeNodeList;
    atreelist_save.datalist[slot] = AnimDataList;
}

/* Source-level helper retained by the Xbox PDB and inlined by the GCN build. */
static inline void AnimFixPos(anode* root, animinfo* info)
{
    f32 translation[3];
    f32 component;
    anode* child;

    child = root->child;
    if (child != NULL) {
        WorldVector((f32*)((u8*)child->obj + 0x30), translation,
                    (f32*)root->obj);
        translation[1] = sAtreeZero;
        component = ((f32*)root->obj)[12];
        ((f32*)root->obj)[12] = component + translation[0];
        component = ((f32*)root->obj)[13];
        ((f32*)root->obj)[13] = component + translation[1];
        component = ((f32*)root->obj)[14];
        ((f32*)root->obj)[14] = component + translation[2];
        ((MBObject*)child->obj)->mat[3][0] = sAtreeZero;
        ((MBObject*)child->obj)->mat[3][2] = sAtreeZero;
        if (child->type == 1) {
            ((animdata*)child->anim)->ppos[0] = sAtreeZero;
            ((animdata*)child->anim)->ppos[2] = sAtreeZero;
            ((animdata*)child->anim)->xpos[0] = sAtreeZero;
            ((animdata*)child->anim)->xpos[2] = sAtreeZero;
        }
    }
    info->setpanim = 1;
}

/* Sequence texmod helper recovered from the Xbox ATREE symbol stream.
 * DoAnimateTreeFrame needs the in-place (macro) expansion, which reuses the
 * caller's own frame variable; DoAnimateTree needs the by-value inline. */
#define DoSeqTexModsInPlace(context, frame, seq)                            \
    {                                                                       \
        s32 i;                                                              \
        TEXMOD* texmod;                                                     \
        s32 period;                                                         \
                                                                            \
        for (i = 0; i < (seq)->ntexmods; i++) {                             \
            texmod = &(seq)->texmods[i];                                    \
            period = texmod->frames * texmod->rate;                         \
            if ((frame) > period && (seq)->wraps != 0 && period > 1) {      \
                (frame) %= period;                                          \
            }                                                               \
            DoTexModSeqSub((context), texmod, (frame));                     \
        }                                                                   \
    }

static inline void DoSeqTexMods(void* context, s32 frame, animseqdesc* seq)
{
    s32 i;
    s32 offset;
    TEXMOD* texmod;
    s32 period;

    i = 0;
    offset = i;
    while (i < seq->ntexmods) {
        texmod = (TEXMOD*)((u8*)seq->texmods + offset);
        period = texmod->frames * texmod->rate;
        if (frame > period && seq->wraps != 0 && period > 1) {
            frame %= period;
        }
        DoTexModSeqSub(context, texmod, frame);
        i++;
        offset += sizeof(TEXMOD);
    }
}

/* DoAnimateTreeFrame: evaluate a fixed animation frame, run sequence texmods,
 * and recurse through AnimateNode. */
s32 DoAnimateTreeFrame(atree* tree, s32 sequence, s32 frame, s32 recurse)
{
    animinfo* info;
    anode* root;
    animseqdesc* seq;
    s32 result;

    root = tree->root;
    info = &tree->animinfo;
    result = AnimateTreeFrame(0.0f, info, sequence, frame, frame);
    if (recurse > 0) {
        if (info->seqheader != NULL) {
            void* obj = root->obj;
            seq = &info->seqheader[sequence];
            DoSeqTexModsInPlace(obj, frame, seq);
        }
        AnimateNode(root, info, recurse);
    }
    return result;
}

s32 AnimateATree(atree* tree, s32 sequence, s32 last)
{
    return DoAnimateTree(0.0f, tree, sequence, 0, last, 1);
}

/* DoAnimateTree: animation-tree evaluation main (AnimateTree / WorldVector /
 * sequence texmods, then AnimateNode recursion). */
s32 DoAnimateTree(f32 time, atree* tree, s32 sequence, s32 first, s32 last,
                  s32 recurse)
{
    s32 result;
    animinfo* info = &tree->animinfo;
    anode* root;
    s32 frame;

    {
        u32 rootValue = (u32)tree->root;
        root = (anode*)rootValue;
        if (tree == NULL || rootValue == 0 || info == NULL) {
            return 0;
        }
    }

    result = AnimateTree(time, info, sequence, first, last);
    if ((result & 8) != 0) {
        AnimFixPos(root, info);
    }

    if (recurse > 0 && info->seqheader != NULL) {
        animseqdesc* seq;

        seq = (animseqdesc*)((u8*)info->seqheader + info->animseq * 0x30);
        if ((seq->flags & 1) != 0) {
            frame = info->numframes -
                    (s32)(sAtreeFrameRoundBias + info->frame) - 1;
        } else {
            frame = (s32)(sAtreeFrameRoundBias + info->frame);
        }
        DoSeqTexMods(root->obj, frame, seq);
        AnimateNode(root, info, recurse);
    }
    return result;
}

/* AnimateNode: recursively evaluate an animation node and all descendants. */
void AnimateNode(anode* node, animinfo* info, s32 recurse)
{
    s32 frame;
    u32 flags;
    animseqdesc* seq;

    seq = (animseqdesc*)((u8*)info->seqheader + info->animseq * 0x30);
    if ((seq->flags & 1) != 0) {
        frame = info->numframes - (s32)(sAtreeFrameRoundBias + info->frame) - 1;
    } else {
        frame = (s32)(sAtreeFrameRoundBias + info->frame);
    }

    for (; node != NULL; node = node->next) {
        switch (node->type) {
        case 1:
            if (recurse != 0) {
                flags = DoAnimation(node->anim, info, node->obj,
                                    (s32*)((u8*)node->obj + 0x40), &node->x);
            } else {
                flags = DoAnimation(node->anim, info, NULL, NULL, NULL);
            }
            if ((flags & 0x700) != 0) {
                MBTreeSetFlags(node->obj, 8, 0);
            } else {
                MBTreeClearFlags(node->obj, 8, 0);
            }
            break;
        case 2:
            DoObjAnimation(node->anim, (s32)node->obj, info->animseq, frame);
            break;
        case 3:
        {
            TEXMOD* texmod = node->anim;
            DoTexModSeqSub(node->obj, texmod, frame);
            break;
        }
        case 4:
            if ((info->flags & 4) != 0) {
                if (info->animseq == 0) {
                    MBTreeSetFlags(node->obj, 0x200000, 1);
                } else {
                    MBTreeClearFlags(node->obj, 0x200000, 1);
                }
            }
            break;
        }
        if (node->child != NULL) {
            AnimateNode(node->child, info, recurse);
        }
    }
}

/* ---------------- tree traversal / teardown ---------------- */

anode* AtreeFindMbidxNode(anode* node, int mbidx);

static inline anode* AtreeFindMbidxNodeChild(anode* node, int mbidx)
{
    anode* found;

    if (node == NULL) {
        return NULL;
    }
    while (node != NULL) {
        if (node->obj != NULL && ((MBObject*)node->obj)->index == (u32)mbidx) {
            return node;
        }
        if (node->child != NULL) {
            found = AtreeFindMbidxNode(node->child, mbidx);
            if (found != NULL) {
                return found;
            }
        }
        node = node->next;
    }
    return NULL;
}

anode* AtreeFindMbidxNode(anode* node, int mbidx)
{
    u8 unused[4];
    anode* found;

    if (node == NULL) {
        return NULL;
    }
    while (node != NULL) {
        if (node->obj != NULL && ((MBObject*)node->obj)->index == (u32)mbidx) {
            return node;
        }
        if (node->child != NULL) {
            found = AtreeFindMbidxNodeChild(node->child, mbidx);
            if (found != NULL) {
                return found;
            }
        }
        node = node->next;
    }
    return NULL;
}

void AtreeDelete(anode** proot)
{
    anode* root;
    anode* node;
    anode* nxt;

    root = node = *proot;
    while (node != NULL) {
        nxt = node->next;
        root = AtreeRemoveNode(node, 1, root);
        node = nxt;
    }
    *proot = NULL;
}

void AtreeNodeSetParent(anode* node, anode* newparent, anode* root, int reparent)
{
    anode* parent;
    anode* prev;

    parent = node->parent;
    if (parent == NULL || parent != newparent) {
        if (parent != NULL && parent->child == node) {
            parent->child = node->next;
        } else {
            prev = AtreeNodePrevNode(node, root);
            prev->next = node->next;
        }
        node->next = NULL;
        node->parent = newparent;
        if (newparent != NULL) {
            if (newparent->child == NULL) {
                newparent->child = node;
            } else {
                AtreeNodeLastSibling(newparent->child)->next = node;
            }
        }
        if (reparent != 0 && node->obj != NULL) {
            MBNodeSetParent(node->obj, newparent->obj);
        }
    }
}

void AtreeKillPsys(atree* tree)
{
    anode* node;

    for (node = tree->root; node != NULL; node = node->next) {
        AtreeRemovePsysSub(node->child);
        if (node->type == 4 && node->anim != NULL) {
            node->anim = MBRemoveNode(node->anim, 0);
        }
    }
}

#pragma dont_inline on
void AtreeRemovePsysSub(anode* node)
{
    anode* child;

    for (; node != NULL; node = node->next) {
        for (child = node->child; child != NULL; child = child->next) {
            AtreeRemovePsysSub(child->child);
            if (child->type == 4 && child->anim != NULL) {
                child->anim = MBRemoveNode(child->anim, 0);
            }
        }
        if (node->type == 4 && node->anim != NULL) {
            node->anim = MBRemoveNode(node->anim, 0);
        }
    }
}
#pragma dont_inline off

anode* AtreeRemoveNode(anode* node, int keep, anode* root)
{
    u8 unused[8];
    anode* c1;
    anode* c2;
    anode* last;
    anode* parent;
    anode* prev;

    if (node == NULL) {
        return root;
    }
    if (keep != 0) {
        for (c1 = node->child; c1 != NULL; c1 = c1->next) {
            if ((c2 = c1->child) != NULL) {
                for (; c2 != NULL; c2 = c2->next) {
                    if (c2->child != NULL) {
                        AtreeRemoveNodeChild(c2->child);
                    }
                    AtreeRemoveNodeSub(c2);
                }
            }
            AtreeRemoveNodeSub(c1);
        }
    }
    parent = node->parent;
    if (parent != NULL && parent->child == node) {
        if (keep != 0) {
            goto parent_no_child;
        }
        if (node->child != NULL) {
            goto parent_with_child;
        }
parent_no_child:
        parent->child = node->next;
        goto unlink_done;
parent_with_child:
        parent->child = node->child;
        node->child->parent = parent;
        last = node->child;
        c1 = last->next;
        c2 = last;
        while (c1 != NULL && c1 != last) {
            c2 = c1;
            c1 = c1->next;
        }
        c2->next = node->next;
        goto unlink_done;
    }
    if (node == root) {
        if (keep != 0) {
            goto root_no_child;
        }
        if ((c1 = node->child) != NULL) {
            goto root_with_child;
        }
root_no_child:
        root = node->next;
        goto unlink_done;
root_with_child:
        c1->parent = NULL;
        root = c1;
        last = node->child;
        c1 = last->next;
        c2 = last;
        while (c1 != NULL && c1 != last) {
            c2 = c1;
            c1 = c1->next;
        }
        c2->next = node->next;
        goto unlink_done;
    }
    prev = AtreeNodePrevNode(node, root);
    if (prev == NULL) {
        goto unlink_done;
    }
    if (keep != 0) {
        goto prev_no_child;
    }
    if (node->child != NULL) {
        goto prev_with_child;
    }
prev_no_child:
    prev->next = node->next;
    goto unlink_done;
prev_with_child:
    prev->next = node->child;
    node->child->parent = prev->parent;
    last = node->child;
    c1 = last->next;
    c2 = last;
    while (c1 != NULL && c1 != last) {
        c2 = c1;
        c1 = c1->next;
    }
    c2->next = node->next;
unlink_done:
    AtreeRemoveNodeSub(node);
    return root;
}

void AtreeRemoveNodeChild(anode* node)
{
    anode* c1;
    anode* c2;

    for (; node != NULL; node = node->next) {
        if ((c1 = node->child) != NULL) {
            for (; c1 != NULL; c1 = c1->next) {
                if ((c2 = c1->child) != NULL) {
                    for (; c2 != NULL; c2 = c2->next) {
                        if (c2->child != NULL) {
                            AtreeRemoveNodeChild(c2->child);
                        }
                        AtreeRemoveNodeSub(c2);
                    }
                }
                AtreeRemoveNodeSub(c1);
            }
        }
        AtreeRemoveNodeSub(node);
    }
}

void AtreeRemoveNodeSub(anode* node)
{
    int idx;
    animdata* ad;

    if (node->obj != NULL) {
        switch (node->type) {
        case 1:
            ad = (animdata*)node->anim;
            ad->inuse = 0;
            idx = ad - AnimDataList;
            if (idx >= 0 && idx < AnimDataFirstFree) {
                AnimDataFirstFree = idx;
            }
            break;
        case 4:
        {
            MBObject* animObj = (MBObject*)node->anim;
            if (node->anim != NULL && animObj->parent == node->obj &&
                animObj->type == MB_PSYS_NODE) {
                MBRemoveNode(node->anim, 0);
            }
            break;
        }
        }
        MBRemoveNode(node->obj, 0);
        node->obj = NULL;
    }
    node->type = -1;
    node->anim = NULL;
    idx = node - AtreeNodeList;
    if (idx >= 0 && idx < AtreeNodeFirstFree) {
        AtreeNodeFirstFree = idx;
    }
}

int AtreeModel(void* bank)
{
    int i;

    for (i = 0; i < natreelists; i++) {
        if (whichatree[i] == bank) {
            return atree_handles[i];
        }
    }
    return -1;
}

/* Match an animation tree, remember its per-bank scroll name, and instantiate
 * the selected tree into the caller's playback state. */
#pragma opt_propagation off
void* fn_80011BBC(atreeheader* hdr, char* name, void* state, char* scrollName,
                  u32 flags)
{
    s32 i = 0;
    u8 unused[8];

    for (; i < natreelists; i++) {
        if (whichatree[i] == hdr) {
            strncpy((char*)atree_scroll[i], scrollName, 16);
        }
    }

    if (name != NULL) {
        s32 matchIndex;
        atreematch* list;
        void* node;

        if (hdr == NULL) {
            FatalError("AtreeMatch with NULL atree", 0x804060);
        }
        list = hdr->list;
        matchIndex = 0;
        for (; matchIndex < hdr->num; matchIndex++) {
            if (strcmp(name, list[matchIndex].name) == 0) {
                node = (u8*)hdr + list[matchIndex].offset;
                goto found;
            }
        }
        ErrorPrintf("No AtreeMatch: %s", name);
        node = NULL;
found:
        {
            AtreeDefinition* def = (AtreeDefinition*)node;
            if (node == NULL) {
                return NULL;
            }
            return AtreeInitSub(def, (atree*)state, scrollName, flags, 1);
        }
    }

    return AtreeInitSub(
        (AtreeDefinition*)((u8*)hdr + hdr->list[0].offset), (atree*)state,
        scrollName, flags, 1);
}
#pragma opt_propagation reset

/* ---------------- match table ---------------- */

void* AtreeMatch(atreeheader* hdr, char* name, s32 report)
{
    int i;
    atreematch* list;

    if (hdr == NULL) {
        FatalError("AtreeMatch with NULL atree", 0x804060);
    }
    list = hdr->list;
    for (i = 0; i < hdr->num; i++) {
        if (strcmp(name, list[i].name) == 0) {
            return (char*)hdr + list[i].offset;
        }
    }
    if (report != 0) {
        ErrorPrintf("No AtreeMatch: %s", name);
    }
    return NULL;
}

extern int* SetupAnimHeader(int* hdr, int* dst);
extern void InitOAnimList(void* hdr, int arg);

static inline u16 AtreeNodeSwap16(u16 value)
{
    u8* bytes;

    bytes = (u8*)&value;
    return (u16)(bytes[0] | (bytes[1] << 8));
}

static inline u32 AtreeNodeSwap32(u32 value)
{
    u32 result;
    u8* source;
    u8* destination;

    source = (u8*)&value;
    destination = (u8*)&result;
    destination[0] = source[3];
    destination[1] = source[2];
    destination[2] = source[1];
    destination[3] = source[0];
    return result;
}

static inline f32 AtreeNodeSwapF32(f32 value)
{
    u32 result;

    result = AtreeNodeSwap32(*(u32*)&value);
    return *(f32*)&result;
}

/* byte-order fixup helpers: the atree resource is little-endian on disk. */
#define SWAP32(x) (x) = AtreeNodeSwap32(x)
#define SWAPF32(x) (x) = AtreeNodeSwapF32(x)
#define SWAP16(x) (x) = AtreeNodeSwap16(x)

typedef struct AtreeWorldPsys {
    u32 version;
    u16 preset;
    u8 id;
    u8 dummy;
    u32 flags;
    u32 flagEnables;
    u32 enables;
    s32 maxParticles;
    u32 maxDirections;
    u32 maxPositions;
    f32 emitterLifeFade[2];
    f32 particleLifeFade[2];
    u32 reserved1;
    u32 reserved2;
    f32 emitterAngle;
    s32 particleTextureCount;
    char particleTextureName[32];
    f32 emitterDirection[3];
    f32 emitterVolume[3];
    f32 emitterRate[4];
    f32 emitterRateRandom;
    f32 particleGravity;
    f32 particleDrag;
    f32 particleSpeed;
    u32 particleRgba[4];
    f32 particleWidth[4];
    f32 emitterDelay;
    f32 particleReserved1b;
    f32 particleReserved1c;
    f32 particleReserved1d;
    f32 particleReserved2[4];
    f32 particleReserved3[4];
    f32 particleReserved4[4];
    f32 particleReserved5[4];
    f32 particleReserved6[4];
    f32 particleReserved7[4];
    f32 particleReserved8a;
    f32 particleReserved8b;
    f32 particleReserved8c;
    u32 checksum;
} AtreeWorldPsys;

#define WORLD_SWAP32(field) (field) = AtreeNodeSwap32(field)
#define WORLD_SWAP16(field) (field) = AtreeNodeSwap16(field)
#define WORLD_SWAPF32(field) (field) = AtreeNodeSwapF32(field)

/* fn_80011DCC @0x80011DCC -- byte-swap one 0x138-byte atree node-definition
 * record (v8+ headers) from little-endian disk order. */
void fn_80011DCC(AtreeWorldPsys* psys)
{
    s32 i;

    WORLD_SWAP32(psys->version);
    WORLD_SWAP16(psys->preset);
    WORLD_SWAP32(psys->flags);
    WORLD_SWAP32(psys->flagEnables);
    WORLD_SWAP32(psys->enables);
    WORLD_SWAP32(psys->maxParticles);
    WORLD_SWAP32(psys->maxDirections);
    WORLD_SWAP32(psys->maxPositions);
    WORLD_SWAP32(psys->reserved1);
    WORLD_SWAP32(psys->reserved2);
    WORLD_SWAPF32(psys->emitterAngle);
    WORLD_SWAP32(psys->particleTextureCount);
    WORLD_SWAPF32(psys->emitterRateRandom);
    WORLD_SWAPF32(psys->particleGravity);
    WORLD_SWAPF32(psys->particleDrag);
    WORLD_SWAPF32(psys->particleSpeed);
    WORLD_SWAPF32(psys->emitterDelay);
    WORLD_SWAPF32(psys->particleReserved1b);
    WORLD_SWAPF32(psys->particleReserved1c);
    WORLD_SWAPF32(psys->particleReserved1d);
    WORLD_SWAPF32(psys->particleReserved8a);
    WORLD_SWAPF32(psys->particleReserved8b);
    WORLD_SWAPF32(psys->particleReserved8c);
    WORLD_SWAP32(psys->checksum);
    for (i = 0; i < 2; i++) {
        WORLD_SWAPF32(psys->emitterLifeFade[i]);
        WORLD_SWAPF32(psys->particleLifeFade[i]);
    }
    for (i = 0; i < 3; i++) {
        WORLD_SWAPF32(psys->emitterDirection[i]);
        WORLD_SWAPF32(psys->emitterVolume[i]);
    }
    for (i = 0; i < 4; i++) {
        WORLD_SWAPF32(psys->particleWidth[i]);
        WORLD_SWAP32(psys->particleRgba[i]);
        WORLD_SWAPF32(psys->emitterRate[i]);
        WORLD_SWAPF32(psys->particleReserved2[i]);
        WORLD_SWAPF32(psys->particleReserved3[i]);
        WORLD_SWAPF32(psys->particleReserved4[i]);
        WORLD_SWAPF32(psys->particleReserved5[i]);
        WORLD_SWAPF32(psys->particleReserved6[i]);
        WORLD_SWAPF32(psys->particleReserved7[i]);
    }
}

/* fn_8001267C @0x8001267C -- register/fix up one atree resource header:
 * byte-swap the header, its match list, texmod table and (v8+) node-def
 * records, rebase the internal offsets to pointers, run SetupAnimHeader /
 * InitOAnimList over each match entry, then claim an atree-list slot. */
u32 fn_8001267C(u16* hdr, s32 model, u32 slot)
{
    u8* base = (u8*)hdr;
    s32 i;
    s32 j;
    s32 off;

    SWAP16(hdr[0]);
    SWAP16(hdr[1]);
    SWAP32(*(u32*)(hdr + 2));
    SWAP32(*(u32*)(hdr + 4));
    SWAP32(*(u32*)(hdr + 6));
    if ((s16)hdr[1] >= 8) {
        SWAP32(*(u32*)(hdr + 8));
        SWAP32(*(u32*)(hdr + 10));
    }

    if (((s16)hdr[1] & 0x8000U) != 0) {
        slot = (s16)hdr[1] & 0x7FFF;
    } else {
        /* match list: name[0x20] + offset, stride 0x24 */
        if (*(u32*)(hdr + 2) != 0) {
            *(u32*)(hdr + 2) += (u32)base;
            off = 0;
            for (i = 0; i < (s16)hdr[0]; i++) {
                atreematch* m = (atreematch*)(*(u32*)(hdr + 2) + off);
                SWAP32(*(u32*)&m->offset);
                off += sizeof(atreematch);
            }
        }
        /* texmod table, stride 0x58 */
        if (*(u32*)(hdr + 6) != 0) {
            *(u32*)(hdr + 6) += (u32)base;
            for (i = 0; i < *(s32*)(hdr + 4); i++) {
                u16* tm = (u16*)(*(u32*)(hdr + 6) + i * sizeof(TEXMOD));
                SWAP16(tm[0]);
                SWAP16(tm[1]);
                SWAP32(*(u32*)(tm + offsetof(TEXMOD, tex) / sizeof(u16)));
                SWAP32(*(u32*)(tm + offsetof(TEXMOD, src) / sizeof(u16)));
                SWAP16(tm[offsetof(TEXMOD, frames) / sizeof(u16)]);
                SWAP16(tm[offsetof(TEXMOD, unk4e) / sizeof(u16)]);
                SWAP32(*(u32*)(tm + offsetof(TEXMOD, rate) / sizeof(u16)));
                SWAP32(*(u32*)(tm + offsetof(TEXMOD, counter) / sizeof(u16)));
            }
        }
        /* node-definition records, stride 0x138 (v8+ headers only) */
        if ((s16)hdr[1] >= 8 && *(u32*)(hdr + 10) != 0) {
            *(u32*)(hdr + 10) += (u32)base;
            for (i = 0; i < *(s32*)(hdr + 8); i++) {
                fn_80011DCC((AtreeWorldPsys*)(*(u32*)(hdr + 10) +
                                              i * sizeof(AtreeWorldPsys)));
            }
        }

        /* per-match-entry tree blobs */
        off = 0;
        for (i = 0; i < (s16)hdr[0]; i++) {
            s32* blob =
                (s32*)(base + *(s32*)(*(u32*)(hdr + 2) + off +
                                       offsetof(atreematch, offset)));
            s32 seqoff;
            s32 texbase;
            s32 nseqs;

            SWAP32(blob[0]);
            SWAP32(blob[1]);
            SWAP32(blob[2]);
            SWAP32(blob[3]);
            SWAP32(blob[4]);
            SWAP32(blob[5]);
            SWAP16(*(u16*)((u8*)blob + offsetof(AtreeDefinition, objectIndex)));
            blob[0] += (s32)blob;
            blob[3] += (s32)blob;

            /* sequence table, stride sizeof(animseqdesc). +0x20/+0x22/+0x26
             * are real fields absorbed into animseqdesc's _pad00/_pad26 -
             * left as bare offsets, no GC-verified name for them yet. */
            for (j = 0; j < blob[5]; j++) {
                u8* seq = (u8*)(blob[0] + j * sizeof(animseqdesc));
                SWAP16(*(u16*)(seq + 0x20));
                SWAP16(*(u16*)(seq + 0x22));
                SWAP16(*(u16*)(seq + offsetof(animseqdesc, wraps)));
                SWAP16(*(u16*)(seq + 0x26));
                SWAP16(*(u16*)(seq + offsetof(animseqdesc, ntexmods)));
                SWAP16(*(u16*)(seq + offsetof(animseqdesc, flags)));
                SWAP32(*(u32*)(seq + offsetof(animseqdesc, texmods)));
            }

            /* node-info table, stride sizeof(AtreeNodeDef) */
            for (j = 0; j < blob[4]; j++) {
                u8* ni = (u8*)(blob[3] + j * sizeof(AtreeNodeDef));
                SWAPF32(((AtreeNodeDef*)ni)->position[0]);
                SWAPF32(((AtreeNodeDef*)ni)->position[1]);
                SWAPF32(((AtreeNodeDef*)ni)->position[2]);
                SWAP16(*(u16*)(ni + offsetof(AtreeNodeDef, type)));
                SWAP16(*(u16*)(ni + offsetof(AtreeNodeDef, flags)));
                SWAP32(*(u32*)(ni + offsetof(AtreeNodeDef, treeFlags)));
                SWAP32(*(u32*)(ni + offsetof(AtreeNodeDef, dataOffset)));
                SWAP32(*(u32*)(ni + offsetof(AtreeNodeDef, parent)));
            }

            if (((AtreeDefinition*)blob)->animheader != NULL) {
                blob[1] = (s32)SetupAnimHeader(
                    (int*)((u8*)blob + blob[1]), (int*)0);
            }
            if (((AtreeDefinition*)blob)->oanimheader != NULL) {
                blob[2] += (s32)blob;
                SWAP32(*(u32*)blob[2]);
                SWAP32(*(u32*)(blob[2] + 4));
            }
            InitOAnimList((void*)blob[2], model);

            /* patch each sequence's texmod index into a pointer */
            nseqs = blob[5];
            seqoff = 0;
            texbase = *(s32*)(hdr + 6);
            {
                s32 sbase = blob[0];
                for (j = 0; j < nseqs; j++) {
                    s32* ptexmods =
                        (s32*)(sbase + seqoff + offsetof(animseqdesc, texmods));
                    seqoff += sizeof(animseqdesc);
                    *ptexmods = texbase + *ptexmods * sizeof(TEXMOD);
                }
            }
            *(s16*)((u8*)blob + offsetof(AtreeDefinition, objectIndex)) = (s16)model;
            off += sizeof(atreematch);
        }

        if ((s32)slot < 0) {
            if (natreelists < 24) {
                slot = natreelists;
                natreelists++;
            } else {
                gErrorCode = 0xFFFF80;
                FatalError("Too many Atrees\n", 0x804060);
            }
        }
        whichatree[slot] = hdr;
        atree_scroll[slot][0] = 0;
        atree_handles[slot] = model;
        hdr[1] = (u16)(slot | 0x8000);
    }
    return slot;
}

/* ---------------- playback dispatch wrappers ---------------- */

anode* AtreeInit(AtreeDefinition* definition, atree* tree,
                 const char* objectPrefix, u32 treeFlags)
{
    return AtreeInitSub(definition, tree, objectPrefix, treeFlags, 1);
}

/* Instantiate a serialized tree into one runtime atree, resolving each object
 * name and wiring the flat node array into its parent/child hierarchy. */
anode* AtreeInitSub(AtreeDefinition* definition, atree* tree,
                    const char* objectPrefix, u32 treeFlags, s32 reportError)
{
    s32 rootIndex;
    anode* nodes;
    s32 nodeOffset;
    s32 i;
    anode* root;
    anode* node;
    anode* parent;
    AtreeNodeDef* nodeDefinition;
    AtreeDataBases* bases;
    s32 definitionOffset;
    s32 objectIndex;
    s32 object;
    char name[16];

    rootIndex = -1;
    root = NULL;
    objectIndex = definition->objectIndex;

    if (definition->nodeCount > 0x200) {
        gErrorCode = 0xFFFF00;
        FatalError("> MAX NODES IN ATREE", 0x804060);
    }

    bases = (AtreeDataBases*)&tree->animinfo;
    tree->animinfo.animheader = definition->animheader;
    tree->animinfo.oanimheader = definition->oanimheader;
    tree->animinfo.seqheader = definition->seqheader;
    tree->animinfo.numseqs = (s16)definition->sequenceCount;
    InitAnimInfo(&tree->animinfo, 0);

    nodes = AtreeNewNode(definition->nodeCount);
    if (nodes == NULL) {
        if (reportError != 0) {
            gErrorCode = 0xFFFF00;
            FatalError("ERROR ADDING NEW ANODES", 0x804060);
        } else {
            return NULL;
        }
    }

    i = 0;
    definitionOffset = 0;
    nodeOffset = 0;
    while (i < definition->nodeCount) {
        node = (anode*)((u8*)nodes + nodeOffset);
        nodeDefinition =
            (AtreeNodeDef*)((u8*)definition->nodes + definitionOffset);

        if (nodeDefinition->parent >= i) {
            FatalError("NODE HAS PARENT >= NODE", 0x804060);
        }

        if (nodeDefinition->parent >= 0) {
            if (root == NULL) {
                FatalError("NODE HAS PARENT BEFORE ROOT", 0x804060);
            }
            parent = (anode*)((u8*)nodes + nodeDefinition->parent * 0x28);
        } else {
            parent = NULL;
        }

        if (nodeDefinition->name[0] != '\0') {
            object = -1;
            if (objectPrefix != NULL) {
                strncpy(name, objectPrefix, 15);
                strncat(name, nodeDefinition->name, 15 - strlen(objectPrefix));
                object = MBOX_ReallyFindObject(name, objectIndex, objectIndex,
                                               -1);
            }
            if (object < 0) {
                strncpy(name, definition->objectPrefix, 15);
                strncat(name, nodeDefinition->name,
                        15 - strlen(definition->objectPrefix));
            }
            name[15] = '\0';
        } else {
            name[0] = '\0';
        }

        if (node != NULL) {
            AtreeNodeInit(node, parent, name, bases, nodeDefinition,
                          objectIndex);
            AtreeNodeInsert(node, parent, root);
        }

        if (strcmp(nodeDefinition->name, sAtreeDummyName) == 0) {
            MBTreeSetFlags(node->obj, 1, 0);
        }
        MBTreeSetFlags(node->obj, treeFlags, 0);

        if (parent == NULL) {
            if (root == NULL) {
                rootIndex = i;
                root = node;
            } else {
                ErrorPrintf("ATREE: %s HAS MULTIPLE ROOTS: %d AND %d", name, i,
                            rootIndex);
            }
        }

        i++;
        definitionOffset += sizeof(AtreeNodeDef);
        nodeOffset += sizeof(anode);
    }

    tree->nanodes = definition->nodeCount;
    tree->anodeinfo = (anodeinfo*)definition->nodes;
    tree->firstanode = nodes;
    if (root == NULL) {
        FatalError("ATREE HAS NO ROOT", 0x804060);
    }
    return root;
}

/* ---------------- sibling-ring helpers ---------------- */

void AtreeNodeInsert(anode* node, anode* parent, anode* root)
{
    anode* child;
    anode* p;
    anode* last;

    node->parent = parent;
    if (parent == NULL) {
        if (root == NULL) {
            return;
        }
        last = root;
        for (p = root->next; p != NULL && p != root; p = p->next) {
            last = p;
        }
        last->next = node;
        return;
    }
    child = parent->child;
    if (parent->child == NULL) {
        parent->child = node;
        return;
    }
    parent = child->next;
    root = child;
    for (; parent != NULL && parent != child; parent = parent->next) {
        root = parent;
    }
    root->next = node;
}

anode* AtreeNodeLastSibling(anode* node)
{
    anode* last;
    anode* p;

    last = node;
    for (p = node->next; p != NULL && p != node; p = p->next) {
        last = p;
    }
    return last;
}

anode* AtreeNodePrevNode(anode* node, anode* list)
{
    anode* p;
    anode* c;

    p = node->parent;
    if (p == NULL) {
        while (list != NULL && list->next != NULL) {
            if (list->next == node) {
                return list;
            }
            list = list->next;
        }
        return NULL;
    }
    c = p->child;
    if (c == node) {
        return NULL;
    }
    while (c->next != node) {
        c = c->next;
    }
    return c;
}

/* ---------------- node / animdata allocation ---------------- */

anode* AtreeNewNode(s32 count)
{
    s32 first;
    s32 end;
    anode* nodes;
    s32 start;
    s32 i;
    s32 need;

    if (count < 1) {
        return NULL;
    }

    first = AtreeNodeFirstFree;
    end = AtreeNumNodes;
    i = first;
    nodes = AtreeNodeList;
    start = first;
    for (; i < end; i++) {
        if (nodes[i].type != -1) {
            start = i + 1;
        } else if ((i + 1) - start >= count) {
            break;
        }
    }

    need = start + count;
    if (need > AnodeMax) {
        ErrorPrintf("Attempt to add > %d Atree nodes", i + count);
        return NULL;
    }
    if (need >= end) {
        AtreeNumNodes = need;
        if (need > AtreeNodePeak) {
            AtreeNodePeak = need;
        }
    }
    if (start == first) {
        AtreeNodeFirstFree += count;
    }
    return &nodes[start];
}

/* AtreeNodeInit: create the MBObject for one anode and wire its animdata
 * (MBNewObject / MBOX_NewObject / InitAnimData / AnimDataNodeNew, ~0x264). */
void AtreeNodeInit(anode* node, anode* parent, const char* name,
                   AtreeDataBases* bases, AtreeNodeDef* def, s32 objectIndex)
{
    s32 object;
    s32 dataOffset;

    if ((def->flags & 1) != 0) {
        goto generic_object;
    }
    if (*name != '\0') {
        goto named_object;
    }
generic_object:
    node->obj = (void*)MBOX_NewObject(NULL, 0,
                                     (s32)(parent != NULL ? parent->obj : NULL), 0);
    if (def == NULL || def->type != 2) {
        MBTreeSetFlags(node->obj, 1, 0);
    }
    goto object_ready;
named_object:
    object = MBOX_ReallyFindObject(name, objectIndex, objectIndex, 1);
    node->obj = MBNewObject(object, 0,
                            parent != NULL ? parent->obj : NULL, 0);
object_ready:
    if (node->obj == NULL) {
        gErrorCode = 0xFFFF00;
        FatalError("AtreeNodeInit: MBNewObject returned NULL", 0x804060);
    }
    MBTreeSetFlags(node->obj, def->treeFlags, 0);

    if (def == NULL) {
        goto null_definition;
    }

    node->type = def->type;
    dataOffset = def->dataOffset;
    switch (def->type) {
    case 1:
        if (dataOffset >= 0) {
            *(s32*)&node->frame = (s32)(bases->animData + dataOffset);
            node->anim = AnimDataNodeNew();
        } else {
            *(s32*)&node->frame = 0;
            node->anim = NULL;
        }
        InitAnimData((u32*)node->anim, *(u32*)&node->frame);
        break;
    case 2:
        if (dataOffset >= 0) {
            object = (s32)(bases->type2 + dataOffset);
        } else {
            object = 0;
        }
        *(s32*)&node->frame = object;
        node->anim = (void*)*(u32*)&node->frame;
        break;
    case 3:
        *(s32*)&node->frame = (s32)(bases->type3 + dataOffset);
        node->anim = (void*)*(u32*)&node->frame;
        break;
    case 4:
        *(s32*)&node->frame = (s32)(bases->type3 + dataOffset);
        node->anim = AudioSetListenerPos((s32*)node->obj,
                                         *(s32*)&node->frame,
                                         def->audioParams);
        break;
    default:
        *(s32*)&node->frame = 0;
        break;
    }
    node->x = def->position[0];
    node->y = def->position[1];
    node->z = def->position[2];
    goto definition_ready;

null_definition:
    node->type = 0;
    *(s32*)&node->frame = 0;
    node->x = sAtreeZero;
    node->y = sAtreeZero;
    node->z = sAtreeZero;

definition_ready:
    ((MBObject*)node->obj)->mat[3][0] = node->x;
    ((MBObject*)node->obj)->mat[3][1] = node->y;
    ((MBObject*)node->obj)->mat[3][2] = node->z;
    node->parent = parent;
    node->child = NULL;
    node->next = NULL;
}

animdata* AnimDataNodeNew(void)
{
    s32 i;
    animdata* list;
    s32 end;

    i = AnimDataFirstFree;
    end = AnimDataNum;
    list = AnimDataList;
    for (; i < end; i++) {
        if (list[i].inuse == 0) {
            break;
        }
    }
    if (i >= end) {
        if (end >= AnimDataMax) {
            gErrorCode = 0xFFFF00;
            FatalError("Too Many AnimDataNodes", 0x804060);
        }
        AnimDataNum++;
        if (AnimDataNum > AnimDataPeak) {
            AnimDataPeak = AnimDataNum;
        }
    }
    AnimDataFirstFree = i + 1;
    AnimDataList[i].inuse = 1;
    return &AnimDataList[i];
}

#undef STUB
