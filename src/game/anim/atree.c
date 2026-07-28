/*
 * atree.c -- GCN ATREE.OBJ: animation-tree construction / playback.
 *
 * Real names are from the Xbox build's ATREE.OBJ (shell3D.pdb).  The GCN
 * .text lays functions out in reverse source order (DoTexMods lowest,
 * AnimDataNodeNew highest).
 *
 * Status: 18 functions byte-exact (objdiff 100%); the lookup/teardown/pool
 * families are complete.  The animation-evaluation chain (fn_8001101C..
 * fn_80011334, fn_80012F9C, AtreeNodeInit) and the two construction giants
 * (fn_80011DCC, fn_8001267C) carry the demo float-ABI passthrough and are
 * left as documented stubs pending a dedicated pass.
 *
 * .text       0x80010A4C..0x800137BC
 * extab       0x80005590..0x80005678
 * extabindex  0x800088A8..0x80008A04
 */

#include "types.h"

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

/* -- animdata: playback record in the AnimData pool (stride 0xA0) -- */
typedef struct animdata {
    /* 0x00 */ u8 _pad0[4];
    /* 0x04 */ s32 inuse;
    /* 0x08 */ u8 _pad8[0x98];
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

/* -- animinfo: atree playback state (graphics.h Id=3256, 0x38) -- */
typedef struct animinfo {
    /* 0x00 */ atreeseq* seqheader;
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

/* ================= external helpers ================= */
extern int strcmp(const char* a, const char* b);
extern int strncmp(const char* a, const char* b, u32 n);
extern void ErrorPrintf(char* fmt, ...);
extern void FatalError(char* msg, int code);
extern void* AllocMem(u32 size);
extern void DoTexModSub(TEXMOD* tm);
extern void InitTexMod(TEXMOD* tm, int texidx);
extern int strncpy(char* dst, const char* src, u32 n);
extern void* MBRemoveNode(void* obj, int flag);
extern void MBNodeSetParent(void* child, void* parent);
extern void* fn_800E8090(void* p, int a, int b);

/* intra-TU forward declarations (address-order names retained) */
anode* fn_800132F0(anode* node);
anode* fn_8001331C(anode* node, anode* list);
void fn_80011750(anode* node);
void fn_800119DC(anode* node);
void fn_80011A74(anode* node);
anode* fn_800117EC(anode* node, int keep, anode* root);
void fn_80012F9C();
void fn_80011134(f32 frame, void* node, int a, int b, int c, int d);

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
} atreelist_save;

static s32 atree_handles[24];
static u8 atree_scroll[24][16];
static void* whichatree[24];

extern u32 lbl_80343ef0;

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
            for (i = 0; i < seq->ntexmods; i++) {
                tm = (TEXMOD*)((char*)seq->texmods + i * 0x58);
                InitTexMod(tm, texidx);
            }
        }
    }
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
    seqs = ai->seqheader;
    for (i = 0; i < ai->numseqs; i++) {
        if (strcmp((char*)seqs + i * 0x30, name) == 0) {
            return i;
        }
    }
    return -1;
}

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

/* ---------------- node/animdata pools ---------------- */

void AtreeListInit(void)
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

void AtreeInitLists(int nnodes, int ndata)
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
    AtreeListInit();
}

/* ---------------- atree-list slot save / restore ---------------- */

void fn_80010DF4(int slot)
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
    AtreeListInit();
}

void fn_80010E84(int slot)
{
    atreelist_save.natreelists[slot] = natreelists;
    atreelist_save.nodelist[slot] = AtreeNodeList;
    atreelist_save.datalist[slot] = AnimDataList;
}

/* fn_8001101C: per-object animation dispatch (evaluates texmod sequences via
 * fn_8000F184 / DoTexModSeqSub, then recurses into fn_80011334).  DEFERRED --
 * demo float-ABI passthrough (f1..f8 forwarded); needs a dedicated pass. */
STUB(0x8001101C, fn_8001101C)

void fn_80011104(void* node, int a, int c)
{
    fn_80011134(0.0f, node, a, 0, c, 1);
}

/* fn_80011134: animation-tree evaluation main (InitAnim / WorldVector / texmod
 * seq, recurses fn_80011334).  DEFERRED -- demo float-ABI. */
void fn_80011134(f32 frame, void* node, int a, int b, int c, int d) {}

/* fn_80011334: recursive animation-tree walk/render (DoObjAnimation,
 * MBTreeSet/ClearFlags, fn_8000F2D8).  DEFERRED -- demo float-ABI. */
STUB(0x80011334, fn_80011334)

/* ---------------- tree traversal / teardown ---------------- */

anode* AtreeFindMbidxNode(anode* node, int mbidx)
{
    anode* found;
    anode* child;

    if (node == NULL) {
        return NULL;
    }
    for (; node != NULL; node = node->next) {
        if (node->obj != NULL && *(u32*)((char*)node->obj + 0x6C) == (u32)mbidx) {
            return node;
        }
        child = node->child;
        if (child != NULL) {
            for (; child != NULL; child = child->next) {
                if ((child->obj != NULL &&
                     (found = child, *(u32*)((char*)child->obj + 0x6C) == (u32)mbidx)) ||
                    (child->child != NULL &&
                     (found = AtreeFindMbidxNode(child->child, mbidx), found != NULL))) {
                    goto done;
                }
            }
            found = NULL;
        done:
            if (found != NULL) {
                return found;
            }
        }
    }
    return NULL;
}

void AtreeDelete(anode** proot)
{
    anode* root;
    anode* node;
    anode* nxt;

    root = *proot;
    node = root;
    while (node != NULL) {
        nxt = node->next;
        root = fn_800117EC(node, 1, root);
        node = nxt;
    }
    *proot = NULL;
}

void fn_80011628(anode* node, anode* newparent, anode* root, int reparent)
{
    anode* parent;
    anode* prev;

    parent = node->parent;
    if (parent == NULL || parent != newparent) {
        if (parent != NULL && parent->child == node) {
            parent->child = node->next;
        } else {
            prev = fn_8001331C(node, root);
            prev->next = node->next;
        }
        node->next = NULL;
        node->parent = newparent;
        if (newparent != NULL) {
            if (newparent->child == NULL) {
                newparent->child = node;
            } else {
                fn_800132F0(newparent->child)->next = node;
            }
        }
        if (reparent != 0 && node->obj != NULL) {
            MBNodeSetParent(node->obj, newparent->obj);
        }
    }
}

void fn_800116EC(atree* tree)
{
    anode* node;

    for (node = tree->root; node != NULL; node = node->next) {
        fn_80011750(node->child);
        if (node->type == 4 && node->anim != NULL) {
            node->anim = MBRemoveNode(node->anim, 0);
        }
    }
}

#pragma dont_inline on
void fn_80011750(anode* node)
{
    anode* child;

    for (; node != NULL; node = node->next) {
        for (child = node->child; child != NULL; child = child->next) {
            fn_80011750(child->child);
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

anode* fn_800117EC(anode* node, int keep, anode* root)
{
    anode* c1;
    anode* c2;
    anode* last;
    anode* p;
    anode* parent;
    anode* prev;

    if (node == NULL) {
        return root;
    }
    if (keep != 0) {
        for (c1 = node->child; c1 != NULL; c1 = c1->next) {
            c2 = c1->child;
            if (c2 != NULL) {
                for (; c2 != NULL; c2 = c2->next) {
                    if (c2->child != NULL) {
                        fn_800119DC(c2->child);
                    }
                    fn_80011A74(c2);
                }
            }
            fn_80011A74(c1);
        }
    }
    parent = node->parent;
    if (parent == NULL || parent->child != node) {
        if (node == root) {
            if (keep == 0 && (root = node->child) != NULL) {
                root->parent = NULL;
                last = node->child;
                for (p = last->next; p != NULL && p != last; p = p->next) {
                    last = p;
                }
                last->next = node->next;
            } else {
                root = node->next;
            }
        } else {
            prev = fn_8001331C(node, root);
            if (prev != NULL) {
                if (keep == 0 && node->child != NULL) {
                    prev->next = node->child;
                    node->child->parent = prev->parent;
                    last = node->child;
                    for (p = last->next; p != NULL && p != last; p = p->next) {
                        last = p;
                    }
                    last->next = node->next;
                } else {
                    prev->next = node->next;
                }
            }
        }
    } else if (keep == 0 && node->child != NULL) {
        parent->child = node->child;
        node->child->parent = parent;
        last = node->child;
        for (p = last->next; p != NULL && p != last; p = p->next) {
            last = p;
        }
        last->next = node->next;
    } else {
        parent->child = node->next;
    }
    fn_80011A74(node);
    return root;
}

void fn_800119DC(anode* node)
{
    anode* c1;
    anode* c2;

    for (; node != NULL; node = node->next) {
        c1 = node->child;
        if (c1 != NULL) {
            for (; c1 != NULL; c1 = c1->next) {
                c2 = c1->child;
                if (c2 != NULL) {
                    for (; c2 != NULL; c2 = c2->next) {
                        if (c2->child != NULL) {
                            fn_800119DC(c2->child);
                        }
                        fn_80011A74(c2);
                    }
                }
                fn_80011A74(c1);
            }
        }
        fn_80011A74(node);
    }
}

void fn_80011A74(anode* node)
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
            if (node->anim != NULL &&
                *(void**)((char*)node->anim + 0x74) == node->obj &&
                *(char*)((char*)node->anim + 0x52) == 14) {
                MBRemoveNode(node->anim, 0);
            }
            break;
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

int fn_80011B6C(void* bank)
{
    int i;

    for (i = 0; i < natreelists; i++) {
        if (whichatree[i] == bank) {
            return atree_handles[i];
        }
    }
    return -1;
}

/* fn_80011BBC: AtreeMatch variant with UV-scroll registry remap (whichatree /
 * atree_scroll lookup, then dispatches fn_80012F9C).  DEFERRED -- demo
 * float-ABI + registry. */
STUB(0x80011BBC, fn_80011BBC)

/* fn_80011DCC / fn_8001267C: GIANT atree construction pair (~0x8B0 / ~0x8FC):
 * build the runtime tree from a def (InitOAnimList, SetupAnimHeader,
 * AtreeNodeInit, node/animdata allocation).  DEFERRED -- semantic skeletons
 * only; require a dedicated float-ABI reversing pass. */
STUB(0x80011DCC, fn_80011DCC)
STUB(0x8001267C, fn_8001267C)

/* ---------------- playback dispatch wrappers ---------------- */

void fn_80012F78(void* node, int a, int b, uint c)
{
    fn_80012F9C(node, a, b, c, 1);
}

/* fn_80012F9C: recursive anim-tree instantiation (~0x2D0): resolves node names
 * (AtreeNodeInit, MBOX_ReallyFindObject), builds children.  DEFERRED. */
void fn_80012F9C() {}

/* ---------------- sibling-ring helpers ---------------- */

void fn_8001326C(anode* node, anode* parent, anode* root)
{
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
    if (parent->child == NULL) {
        parent->child = node;
        return;
    }
    last = parent->child;
    for (p = last->next; p != NULL && p != parent->child; p = p->next) {
        last = p;
    }
    last->next = node;
}

anode* fn_800132F0(anode* node)
{
    anode* last;
    anode* p;

    last = node;
    for (p = node->next; p != NULL && p != node; p = p->next) {
        last = p;
    }
    return last;
}

anode* fn_8001331C(anode* node, anode* list)
{
    anode* p;
    anode* c;
    anode* nxt;

    p = node->parent;
    if (p == NULL) {
        while (list != NULL) {
            nxt = list->next;
            if (nxt == NULL) {
                return NULL;
            }
            if (nxt == node) {
                return list;
            }
            list = nxt;
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

anode* fn_80013390(int count)
{
    int i;
    int start;
    int remain;
    int need;
    s32 off;
    anode* result;

    if (count < 1) {
        result = NULL;
    } else {
        remain = AtreeNumNodes - AtreeNodeFirstFree;
        off = AtreeNodeFirstFree * 0x28;
        start = AtreeNodeFirstFree;
        i = AtreeNodeFirstFree;
        if (AtreeNodeFirstFree < AtreeNumNodes) {
            do {
                if (*(s32*)((char*)AtreeNodeList + off + 0x20) == -1) {
                    if ((i + 1) - start >= count) {
                        break;
                    }
                } else {
                    start = i + 1;
                }
                i++;
                off += 0x28;
                remain--;
            } while (remain != 0);
        }
        need = start + count;
        if (need > AnodeMax) {
            ErrorPrintf("Attempt to add > %d Atree nodes", i + count, AtreeNumNodes,
                        AtreeNodeList, start, i, count);
            result = NULL;
        } else {
            if (AtreeNumNodes <= need) {
                AtreeNumNodes = need;
                if (AtreeNodePeak < need) {
                    AtreeNodePeak = need;
                }
            }
            if (start == AtreeNodeFirstFree) {
                AtreeNodeFirstFree += count;
            }
            result = (anode*)((char*)AtreeNodeList + start * 0x28);
        }
    }
    return result;
}

/* AtreeNodeInit: create the MBObject for one anode and wire its animdata
 * (MBNewObject / MBOX_NewObject / InitAnimData / AnimDataNodeNew, ~0x264).
 * DEFERRED -- demo float-ABI; semantic skeleton only. */
STUB(0x80013480, AtreeNodeInit)

animdata* AnimDataNodeNew(void)
{
    int i;
    int remain;
    s32 off;
    animdata* list;

    list = AnimDataList;
    i = AnimDataFirstFree;
    off = i * 0xA0;
    remain = AnimDataNum - i;
    if (i < AnimDataNum) {
        do {
            if (*(s32*)((char*)list + off + 4) == 0) {
                break;
            }
            i++;
            off += 0xA0;
            remain--;
        } while (remain != 0);
    }
    if (i >= AnimDataNum) {
        if (AnimDataNum >= AnimDataMax) {
            lbl_80343ef0 = 0xFFFF00;
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
