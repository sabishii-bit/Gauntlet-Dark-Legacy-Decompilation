#include "types.h"
#include "game/dcs.h"

/*
 * Sound / message-dispatch client (early game TU, text 0x8004229C-0x800433EC).
 *
 * A set of small "screen/query" helpers that marshal a 32-byte parameter
 * block and dispatch a numeric command to the audio/resource server via
 * dcsHandleRequest.  Also owns a deferred-callback list and the sound-test AX
 * voice bank (14 voices).  Names describe observed behaviour; exact Midway
 * identifiers are unconfirmed.
 *
 * NonMatching: the original reserves a small unused stack local in many of
 * these functions (reconstructed here as `volatile s32 _fpad[N]`).  A few
 * residuals remain: the deferred-callback loop (sndSysFlush/sndSysClear/
 * sndCmdD/sndCmd1) differs by one address-fold instruction, while sndSysInit
 * differs by register choice.
 * These are register/frame-allocation-only residuals; logic is verified.
 */

void* memset(void* dst, int val, u32 n);
char* strncpy(char* dst, const char* src, u32 n);
u32 strlen(const char* s);

/* command dispatch to the sound/resource server: id + in/out param blocks */
s32 dcsHandleRequest(s32 id, void* in, void* out);   /* server message dispatch */
void HealthMeterInit(void);                      /* HealthMeterInit */
s32 adsPoll(void);                               /* ADSTREAM per-frame poll (adsPoll) */

/* GameCube audio (sndvoice.c / AX / AR) */
void sndVoiceStart(s32 voice);
void sndVoiceStop(s32 voice);
void sndVoiceUpdateAll(void);
void sndVoiceSetParams(s32 voice, s32 a, s32 b, s32 c, s32 d, s32 e, s32 f, s32 g);
s32 AXAcquireVoice(s32 prio, s32 cb, s32 ctx);
void AXRegisterCallback(void* cb);
void* ARGetBaseAddress(void);

typedef struct Node {
    /* 0x0 */ u32 unk0;
    /* 0x4 */ u32 unk4;
    /* 0x8 */ u32 unk8;
    /* 0xC */ void (*cb)(struct Node*);
} Node;

typedef struct SndState {
    /* 0x000 */ char name[0x400];
    /* 0x400 */ u8 _pad400[0x28];
    /* 0x428 */ Node* nodes[0x20];
    /* 0x4A8 */ s32 defer[0x100];
    /* 0x8A8 */ s32 msgbuf[0x100];
    /* 0xCA8 */ s32 out[8];
    /* 0xCC8 */ s32 in[8];
    /* 0xCE8 */ s32 voice[14];
} SndState;

extern SndState gSndState;
extern s32 sVoice[14];   /* sVoice */
#define g gSndState

/* small state (sdata/sbss) */
extern u8  sSndActive;      /* sSndActive */
extern s32 sBankLock;       /* sBankLock */
extern s32 sMode;           /* sMode */
extern Node* sHeldNode;     /* sHeldNode */
extern Node* sFrameCb;      /* sFrameCb */
extern s32 sCount1;         /* sCount1 */
extern s32 sCount2;         /* sCount2 */
extern s32 sInSync;         /* sInSync */
extern s32 sFramePend;      /* sFramePend */
extern s32 sConfig;         /* sConfig */
extern s32 sPending;        /* sPending */
extern s32 sReset;          /* sReset */
extern s32 sFlags;          /* sFlags */
extern long long gControllerButtons; /* 64-bit word whose low half aliases sFlags */

/*
 * The init state is a single aggregate the compiler anchors on: sndSysInit
 * reaches every member as base+offset from gBig (at 0x80240FD0).
 */
typedef struct BigState {
    /* 0x00000 */ f32 f0[0x14];
    /* 0x00050 */ u8 _p50[0x40];
    /* 0x00090 */ s32 arr90[4];
    /* 0x000A0 */ u8 arrA0[4][0x50];
    /* 0x001E0 */ u8 blk1E0[0x54];
    /* 0x00234 */ u8 blk234[0xAE00];
    /* 0x0B034 */ s32 arrB034[9][6];
    /* 0x0B10C */ u8 _end[4];
} BigState;
extern BigState gBig;   /* gBig */

extern s32 gBossDead;   /* gBossDead */
extern s32 lbl_8034466C;
extern s32 lbl_80344668;
extern u16 lbl_80344664;
extern s32 lbl_80344660;
extern s32 lbl_8034465C;
extern s32 lbl_80344658;
extern s32 lbl_80344654;
extern s32 lbl_80344650;
extern f32 lbl_8034464C;
extern f32 lbl_80346470;
extern s32 lbl_80240E30[];

/* nodes[0x20]/defer and msgbuf are separate bss objects (dtk-labelled) when
 * accessed as top-level arrays (folded own-symbol address). */
extern s32 lbl_8024C508[];  /* nodes[0x20] @0x8024C508 (== gSndState+0x428) */
extern s32 lbl_8024C988[];  /* msgbuf[0x100] @0x8024C988 (== gSndState+0x8A8) */

s32 sndSysFlush(void);         /* sndSysFlush */
void sndSysClear(void);        /* sndSysClear */
void sndSysSync(void);         /* sndSysSync */
void sndTestAXCallback(void);  /* sndTestAXCallback */

/* 0x8004229C */
void sndSysInit(void)
{
    BigState* big = &gBig;
    s32 i, j;

    for (i = 0; i < 9; i++) {
        for (j = 0; j < 6; j++) {
            big->arrB034[i][j] = 0;
        }
    }
    lbl_8034466C = 0;
    memset(big->blk234, 0, 0xAE00);
    lbl_80344668 = 0;
    memset(big->blk1E0, 0, 0x54);
    lbl_80344664 = 0;
    lbl_80344660 = 0;
    for (i = 0; i < 4; i++) {
        *(s32*)(big->arrA0[i]) = 0;
        big->arr90[i] = 0;
    }
    lbl_8034465C = 0;
    lbl_80344658 = 0;
    lbl_80344654 = -1;
    lbl_80344650 = 0;
    lbl_8034464C = lbl_80346470;
    gBossDead = 0;
    HealthMeterInit();
}

/* 0x80042394 */
void sndSysStub0(void) {}

/* 0x80042398 */
void sndSysStub1(void) {}

/* 0x8004239C */
s32 sndSysFrameCallback(void)
{
    s32 pend = sConfig & 1;

    if (sFramePend != 0 && pend == 0) {
        if (sFrameCb != 0 && sFrameCb->cb != 0) {
            sFrameCb->cb(sFrameCb);
            sFrameCb = 0;
        }
    }
    sFramePend = pend;
    return pend;
}

/* 0x8004240C */
void sndSysSetBit1(s32 v)
{
    s32 b = v ? 2 : 0;
    sConfig = (sConfig & 1) | b;
}

/* 0x80042434 */
void sndSysSetBit0(s32 v)
{
    s32 b = v ? 1 : 0;
    sConfig = (sConfig & 2) | b;
}

/* 0x8004245C  cmd 0x17 */
s32 sndCmd17(s32 a, s32 b)
{
    SndState* s = &g;
    volatile s32 _fpad[2];

    memset(s->in, 0, 0x20);
    s->in[0] = a;
    s->in[1] = b;
    if (sMode != 0) {
        return b;
    }
    sndSysSync();
    if (sMode != 0) {
        memset(s->out, 0, 0x20);
    } else {
        sndSysFlush();
        dcsHandleRequest(0x17, s->in, s->out);
    }
    return s->out[0];
}

/* 0x800424F8  cmd 0x16 */
s32 sndCmd16(s32 a)
{
    SndState* s = &g;

    memset(s->in, 0, 0x20);
    if (a <= 0) {
        return -1;
    }
    s->in[0] = a;
    sndSysSync();
    if (sMode != 0) {
        memset(s->out, 0, 0x20);
    } else {
        sndSysFlush();
        dcsHandleRequest(0x16, s->in, s->out);
    }
    return s->out[0];
}

/* 0x80042588  register a deferred slot */
s32 sndDeferSlot(s32 a)
{
    lbl_8024C988[sCount2++] = 0x55ab;
    lbl_8024C988[sCount2++] = a | 0xffff0000;
    sndSysFlush();
    return 0;
}

/* 0x800425EC  cmd 0xb */
s32 sndCmdB(void)
{
    SndState* s = &g;

    memset(s->in, 0, 0x20);
    sndSysSync();
    if (sMode != 0) {
        memset(s->out, 0, 0x20);
    } else {
        sndSysFlush();
        dcsHandleRequest(0xb, s->in, s->out);
    }
    return 0;
}

/* 0x80042664  cmd 0xa, arms a frame callback */
s32 sndCmdA(s32 a, s32 b, s32 c, Node* cb)
{
    SndState* s = &g;
    volatile s32 _fpad[2];

    memset(s->in, 0, 0x20);
    if (cb == 0) {
        return -3;
    }
    s->in[0] = a;
    s->in[1] = b;
    s->in[2] = c;
    sndSysSync();
    if (sMode != 0) {
        memset(s->out, 0, 0x20);
    } else {
        sndSysFlush();
        dcsHandleRequest(0xa, s->in, s->out);
    }
    {
        s32 r = s->out[0];
        sFrameCb = cb;
        return r;
    }
}

/* 0x8004270C  cmd 0x8, by name */
s32 sndCmd8(char* name, s32 b, s32 c)
{
    SndState* s;
    s32 rv = -4;

    memset(g.in, 0, 0x20);
    s = &g;
    if (name == 0) {
        rv = -1;
    } else if (sBankLock == 0) {
        strncpy(s->name, name, 0x3ff);
        s->in[0] = (s32)s;
        s->in[1] = strlen(s->name) + 1;
        s->in[2] = b;
        s->in[3] = c;
        sndSysSync();
        if (sMode != 0) {
            memset(s->out, 0, 0x20);
        } else {
            sndSysFlush();
            dcsHandleRequest(0x8, s->in, s->out);
        }
        rv = s->out[0];
    }
    return rv;
}

/* 0x800427E0  cmd 0xc */
s32 sndCmdC(void)
{
    SndState* s = &g;
    volatile s32 _fpad[2];

    memset(s->in, 0, 0x20);
    sndSysSync();
    if (sMode != 0) {
        memset(s->out, 0, 0x20);
    } else {
        sndSysFlush();
        dcsHandleRequest(0xc, s->in, s->out);
    }
    return s->out[0];
}

/* 0x80042858  cmd 0xd, then clear pending callbacks */
s32 sndCmdD(void)
{
    SndState* s = &g;
    volatile s32 _fpad[2];

    memset(s->in, 0, 0x20);
    sndSysSync();
    if (sMode != 0) {
        memset(s->out, 0, 0x20);
    } else {
        sndSysFlush();
        dcsHandleRequest(0xd, s->in, s->out);
    }
    memset(s->msgbuf, 0, 0x400);
    sCount2 = 0;
    while (sCount1-- > 0) {
        Node* node;
        SndState* entry = (SndState*)((u8*)s + sCount1 * 4);
        node = entry->nodes[0];
        entry->nodes[0] = 0;
        node->unk4 = 0;
        node->unk8 = 0;
        node->cb(node);
    }
    sCount1 = 0;
    memset(s->defer, 0, 0x400);
    return 0;
}

/* 0x80042940  register callback pair */
s32 sndRegisterPair(s32* items, s32 count, s32 tag)
{
    s32 i;

    if (sCount2 >= 0x100) {
        return 0;
    }
    if (sCount1 >= 0x20) {
        return 0;
    }
    for (i = 0; i < count; i++) {
        lbl_8024C988[sCount2++] = items[i];
    }
    lbl_8024C508[sCount1++] = tag;
    sndSysFlush();
    return count;
}

/* 0x800429F4 */
s32 sndRegisterList(s32* items, s32 count)
{
    volatile s32 _fpad[2];
    s32 i;

    for (i = 0; i < count; i++) {
        lbl_8024C988[sCount2++] = items[i];
    }
    sndSysFlush();
    return count;
}

/* 0x80042A5C  cmd 0x4, by name */
#ifdef __MWERKS__
asm s32 sndCmd4(char* name, s32 b, s32 c, s32* outp)
{
    nofralloc
    mflr r0
    lis r7, gSndState@ha
    stw r0, 4(r1)
    stwu r1, -56(r1)
    stmw r27, 36(r1)
    addi r31, r7, gSndState@l
    addi r27, r3, 0
    addi r28, r4, 0
    addi r29, r5, 0
    addi r30, r6, 0
    lwz r0, sHeldNode(r0)
    cmplwi r0, 0
    beq sndCmd4_available
    li r3, -1
    b sndCmd4_done
sndCmd4_available:
    addi r3, r31, 3272
    li r4, 0
    li r5, 32
    bl memset
    addi r3, r31, 0
    addi r4, r27, 0
    li r5, 1023
    bl strncpy
    stw r31, 3272(r31)
    mr r3, r31
    bl strlen
    addi r0, r3, 1
    stw r0, 3276(r31)
    stw r28, 3280(r31)
    stw r29, 3284(r31)
    bl sndSysSync
    lwz r0, sMode(r0)
    cmpwi r0, 0
    beq sndCmd4_dispatch
    addi r3, r31, 3240
    li r4, 0
    li r5, 32
    bl memset
    b sndCmd4_result
sndCmd4_dispatch:
    bl sndSysFlush
    addi r4, r31, 3272
    addi r5, r31, 3240
    li r3, 4
    bl dcsHandleRequest
sndCmd4_result:
    lwz r0, 3240(r31)
    cmpwi r0, 0
    mr r3, r0
    blt sndCmd4_done
    lwz r0, 3244(r31)
    stw r0, 4(r30)
    stw r30, sHeldNode(r0)
sndCmd4_done:
    lmw r27, 36(r1)
    lwz r0, 60(r1)
    addi r1, r1, 56
    mtlr r0
    blr
}
#else
s32 sndCmd4(char* name, s32 b, s32 c, s32* outp)
{
    SndState* s = &g;
    volatile s32 _fpad[2];

    if (sHeldNode != 0) {
        return -1;
    }
    memset(s->in, 0, 0x20);
    strncpy(s->name, name, 0x3ff);
    s->in[0] = (s32)s;
    s->in[1] = strlen(s->name) + 1;
    s->in[2] = b;
    s->in[3] = c;
    sndSysSync();
    if (sMode != 0) {
        memset(s->out, 0, 0x20);
    } else {
        sndSysFlush();
        dcsHandleRequest(0x4, s->in, s->out);
    }
    {
        s32 r = s->out[0];
        if (r >= 0) {
            outp[1] = s->out[1];
            sHeldNode = (Node*)outp;
        }
        return r;
    }
}
#endif

#ifdef __MWERKS__
#pragma optimization_level 4
#pragma peephole on
#pragma scheduling on
#endif

/* 0x80042B3C  cmd 0x7 */
s32 sndCmd7(s32 a, u16* w1, u16* w2)
{
    SndState* s = &g;

    memset(s->in, 0, 0x20);
    s->in[0] = a;
    if (sMode == 0) {
        s32 r;
        sndSysSync();
        if (sMode != 0) {
            memset(s->out, 0, 0x20);
        } else {
            sndSysFlush();
            dcsHandleRequest(0x7, s->in, s->out);
        }
        r = s->out[0];
        *w1 = s->out[1];
        *w2 = s->out[2];
        return r;
    }
    *w1 = 0;
    *w2 = 0;
    return -1;
}

/* 0x80042BF4  cmd 0x6 */
s32 sndCmd6(void)
{
    SndState* s = &g;

    memset(s->in, 0, 0x20);
    sndSysSync();
    if (sMode != 0) {
        memset(s->out, 0, 0x20);
    } else {
        sndSysFlush();
        dcsHandleRequest(0x6, s->in, s->out);
    }
    return 0;
}

/* 0x80042C6C  cmd 0x18 */
s32 sndCmd18(s32 a)
{
    SndState* s = &g;

    memset(s->in, 0, 0x20);
    s->in[0] = a;
    if (sMode == 0) {
        sndSysSync();
        if (sMode != 0) {
            memset(s->out, 0, 0x20);
        } else {
            sndSysFlush();
            dcsHandleRequest(0x18, s->in, s->out);
        }
        return s->out[0];
    }
    return 0;
}

/* 0x80042D00  cmd 0x1, then clear pending callbacks */
s32 sndCmd1(void)
{
    SndState* s = &g;
    volatile s32 _fpad[2];

    sPending = -2;
    memset(s->in, 0, 0x20);
    s->in[0] = 1;
    sndSysSync();
    if (sMode != 0) {
        memset(s->out, 0, 0x20);
    } else {
        sndSysFlush();
        dcsHandleRequest(0x1, s->in, s->out);
    }
    memset(s->msgbuf, 0, 0x400);
    sCount2 = 0;
    while (sCount1-- > 0) {
        Node* node;
        SndState* entry = (SndState*)((u8*)s + sCount1 * 4);
        node = entry->nodes[0];
        entry->nodes[0] = 0;
        node->unk4 = 0;
        node->unk8 = 0;
        node->cb(node);
    }
    sCount1 = 0;
    memset(s->defer, 0, 0x400);
    return 0;
}

/* 0x80042DF8  per-frame poll / cmd 0x2 */
s32 sndSysUpdate(void)
{
    SndState* s = &g;
    volatile s32 _fpad[2];

    adsPoll();
    if (sReset != 0) {
        Node* held = sHeldNode;
        sHeldNode = 0;
        if (sReset < 0) {
            held->unk4 = 2;
            held->unk8 = 0;
        } else {
            held->unk4 = 0;
            held->unk8 = sReset;
        }
        sReset = 0;
        held->cb(held);
    }
    if (sPending > 0) {
        s->msgbuf[sCount2++] = 0x55af;
        s->msgbuf[sCount2++] = lbl_80240E30[1];
        if (sndSysFlush() == 0) {
            memset(s->in, 0, 0x20);
            sndSysSync();
            if (sMode != 0) {
                memset(s->out, 0, 0x20);
            } else {
                sndSysFlush();
                dcsHandleRequest(0x2, s->in, s->out);
            }
        }
    }
    return sPending;
}

/* 0x80042F18  cmd 0x3 */
s32 sndCmd3(s32 a)
{
    SndState* s = &g;

    memset(s->in, 0, 0x20);
    s->in[0] = a;
    sndSysSync();
    if (sMode != 0) {
        memset(s->out, 0, 0x20);
    } else {
        sndSysFlush();
        dcsHandleRequest(0x3, s->in, s->out);
    }
    return 0;
}

/* 0x80042F98 */
void sndTestStartAll(void)
{
    s32 i;

    if (sSndActive != 0) {
        for (i = 0; i < 14; i++) {
            sndVoiceStart(sVoice[i]);
        }
    }
}

/* 0x80042FF4 */
void sndTestStopAll(void)
{
    s32 i;

    if (sSndActive != 0) {
        for (i = 0; i < 14; i++) {
            sndVoiceStop(sVoice[i]);
        }
    }
}

/* 0x80043050 */
s32 sndTestAcquire(void)
{
    SndState* s = &g;
    volatile s32 _fpad[2];

    s32 i;

    if (sSndActive == 0) {
        sSndActive = 1;
        ARGetBaseAddress();
        for (i = 0; i < 14; i++) {
            s32 voice = AXAcquireVoice(0x1f, 0, 0);
            SndState* entry = (SndState*)((u8*)s + i * 4);

            entry->voice[0] = voice;
            sndVoiceSetParams(entry->voice[0], 0, 0, -0x3e8, -0x3e8,
                              0x40, 0x40, 0);
        }
        AXRegisterCallback(sndTestAXCallback);
    }
    sReset = 0;
    sPending = 0;
    memset(s->in, 0, 0x20);
    s->in[1] = 0x40000;
    s->in[2] = 0x18000;
    s->in[3] = 0x100;
    sndSysSync();
    if (sMode != 0) {
        memset(s->out, 0, 0x20);
    } else {
        sndSysFlush();
        dcsHandleRequest(0x13, s->in, s->out);
    }
    return 0;
}

/* 0x80043168 */
void sndTestAXCallback(void)
{
    sndVoiceUpdateAll();
    dcsServiceQueue();
}

/* 0x8004318C  flush the deferred callback list */
s32 sndSysFlush(void)
{
    SndState* s = &g;
    s32 ret = 0;
    s32 i;
    s32 n;
    s32* e = s->defer;
    volatile s32 _fpad[2];

    if (sCount2 > 0) {
        s->msgbuf[sCount2] = 0;
        dcsHandleRequest(0x11, s->msgbuf, e);
        sCount2 = 0;
        ret = 1;
        n = e[0];
        e++;
        for (i = 0; i < n; i++) {
            Node* node;
            SndState* entry = (SndState*)((u8*)s + i * 4);
            node = entry->nodes[0];
            entry->nodes[0] = 0;
            node->unk4 = e[1];
            node->unk8 = e[2];
            node->cb(node);
            e += 3;
        }
        sCount1 = 0;
    }
    return ret;
}

/* 0x80043250  clear the deferred callback list */
#pragma dont_inline on
void sndSysClear(void)
{
    SndState* s = &g;

    memset(s->msgbuf, 0, 0x400);
    sCount2 = 0;
    while (sCount1-- > 0) {
        Node* node;
        SndState* entry = (SndState*)((u8*)s + sCount1 * 4);
        node = entry->nodes[0];
        entry->nodes[0] = 0;
        node->unk4 = 0;
        node->unk8 = 0;
        node->cb(node);
    }
    sCount1 = 0;
    memset(s->defer, 0, 0x400);
}
#pragma dont_inline off

/* 0x800432EC  synchronise: emit begin/end commands (0xc, 0xd) */
#pragma opt_strength_reduction off
void sndSysSync(void)
{
    SndState* s = &g;
    volatile s32 _fpad[4];
    s32 want = gControllerButtons & 0x20;

    if (sInSync != 0) {
        return;
    }
    if (want != 0 && sMode == 0) {
        sInSync = 1;
        /* command 0xc */
        memset(s->in, 0, 0x20);
        sndSysSync();
        if (sMode != 0) {
            memset(s->out, 0, 0x20);
        } else {
            sndSysFlush();
            dcsHandleRequest(0xc, s->in, s->out);
        }
        /* command 0xd */
        memset(s->in, 0, 0x20);
        sndSysSync();
        if (sMode != 0) {
            memset(s->out, 0, 0x20);
        } else {
            sndSysFlush();
            dcsHandleRequest(0xd, s->in, s->out);
        }
        sndSysClear();
        sInSync = 0;
    }
    sMode = want;
}
#pragma opt_strength_reduction reset
