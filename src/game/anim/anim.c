/*
 * anim.c -- GCN ANIM.OBJ scaffold.
 *
 * The front seam is AnimInit calling anim_play's InitAnimInvDeltaTable.  The
 * back seam is the ZeroAnimData body at 0x8000F628, which starts anim_play.
 *
 * .text       0x8000E8E8..0x8000F628
 * extab       0x80005548..0x80005580
 * extabindex  0x8000883C..0x80008890
 */

#include "types.h"
#include "game/enemy.h"

extern f32 lbl_803441A8;
extern const f32 lbl_803457B0;
extern const f32 lbl_803457B4;
extern const f32 lbl_803457B8;
void InitAnimInvDeltaTable(void);
s32 InitAnim(f32 time, animinfo* info, s32 seq, s32 frame, s32 active);
s32 GetAnimAngXYZVal();                /* 0x8000F7F4 */
void InterpPYR();                      /* 0x8000F788 */
void InterpXYZ();                      /* 0x8000F74C */
extern f64 lbl_803457E8;
extern u32 lbl_803441B8, lbl_803441B4, lbl_803441B0;
void ErrorPrintf(char* fmt, ...);      /* 0x800BC6E0 */
extern f32 gClockTime;                 /* 0x80344584 */
extern f64 lbl_803457C0;
extern char lbl_80110758[];
f32 floorf(f32 x);                     /* 0x800EAA1C */
f32 fabsf(f32 x);
extern f64 lbl_803457D0, lbl_803457D8, lbl_803457E0;
void FatalErrorf(char* fmt, ...);      /* 0x800BC590 */
extern char lbl_80110730[];
s32 CalcAnimInfo(animinfo* info);
void CopyMat3(void* src, void* dst);   /* 0x800BE8C8 */
void ZeroAnimData(int* node);          /* 0x8000F628 */
void CreateRYPMatrix();                /* 0x800BD154 */
void CreatePYRMatrix();                /* 0x800BD254 */
s32 CalcAnimation();                   /* 0x8000F534 */
extern u8 gIdentityMatrix[];

void AnimInit(void)
{
    lbl_803441A8 = lbl_803457B0;
    InitAnimInvDeltaTable();
}

#define STUB(address, name) void name(void) {}

void InitAnimInfo(animinfo* info, u8 flags)
{
    info->animseq = 0;
    info->numframes = 0;
    info->setpanim = 1;
    info->flags = flags;
    info->transfrac = lbl_803457B4;
    info->frame = lbl_803457B4;
    info->animseq0 = 0;
    info->active = 0;
    info->starttime = lbl_803457B4;
    info->transtime = lbl_803457B4;
    info->animscale = lbl_803457B8;
    info->seqscale = lbl_803457B8;
    info->atime = lbl_803457B4;
    info->repeat = 0;
    info->stage = 0;
    if (info->animheader != NULL) {
        InitAnim(lbl_803457B4, info, 0, 0, 1);
    }
}

/* SetupAnimHeader @0x8000E994 -- byte-swap the animheader's first 7 words
 * (loaded little-endian), then relocate its 5 section pointers to absolute
 * addresses in `dst` (in place when dst is NULL). */
int* SetupAnimHeader(int* hdr, int* dst)
{
#define SWAP(i)                                                               \
    {                                                                         \
        union {                                                               \
            u32 w;                                                            \
            u8 b[4];                                                          \
        } s, d;                                                               \
        s.w = hdr[i];                                                         \
        d.b[0] = s.b[3];                                                      \
        d.b[1] = s.b[2];                                                      \
        d.b[2] = s.b[1];                                                      \
        d.b[3] = s.b[0];                                                      \
        hdr[i] = d.w;                                                         \
    }
    SWAP(0);
    SWAP(1);
    SWAP(2);
    SWAP(3);
    SWAP(4);
    SWAP(5);
    SWAP(6);
#undef SWAP
    if (dst == NULL) {
        dst = hdr;
    }
    dst[0] = hdr[0] + (int)hdr;
    dst[1] = hdr[1] + (int)hdr;
    dst[2] = hdr[2] + (int)hdr;
    dst[3] = hdr[3] + (int)hdr;
    dst[4] = hdr[4] + (int)hdr;
    dst[5] = hdr[5];
    dst[6] = hdr[6];
    return dst;
}

s32 AnimDone(void* anim)
{
    if ((*(u16*)((u8*)anim + 0x36) & 0xFF) != 0) {
        return 1;
    }
    return 0;
}

/* AnimateTree @0x8000EB70 -- per-frame drive of one animinfo: advance it, then
 * (re)start sequence `seq` if the `mode` policy calls for it, and report a bit
 * mask of what happened (1=restarted, 2=looped/finished, 4=holding, 8=event). */
u32 AnimateTree(f32 time, animinfo* info, s32 seq, s32 frame, s32 mode)
{
    u32 result = 0;
    s32 restart = 0;
    s32 status;
    s32 curseq;
    s32 done;
    s16 rep;

    lbl_803441B8 = ((u32*)info->animheader)[0];
    lbl_803441B4 = ((u32*)info->animheader)[1];
    lbl_803441B0 = ((u32*)info->animheader)[2];
    status = CalcAnimInfo(info);
    curseq = info->animseq;
    done = (info->stage & 0xFF) == 0xFF;
    rep = info->repeat;
    if (mode == 2) {
        if (done || seq != curseq) {
            restart = 1;
        }
    } else if (mode < 2) {
        if (mode == 0) {
            if (done && seq != curseq) {
                restart = 1;
            }
        } else if (mode >= 0) {
            if (done) {
                restart = 1;
            }
        } else {
            restart = 1;
        }
    } else {
        restart = 1;
    }
    if (restart) {
        s32 initret;
        if (!done ||
            (f64)info->starttime < (f64)info->atime - lbl_803457C0) {
            info->starttime = (f32)((f64)info->atime - lbl_803457C0);
        }
        initret = InitAnim(time, info, seq, frame, 1);
        if (initret < 1) {
            FatalErrorf(lbl_80110730, initret, seq, info->numseqs);
        }
        if ((*(u16*)((u8*)info->seqheader + curseq * 0x30 + 0x26) & 1) != 0) {
            result = 8;
        }
        result |= 1;
    }
    if (((result & 1) != 0 || status == 0xF) &&
        (status = CalcAnimInfo(info)) == 0xF) {
        CalcAnimInfo(info);
    }
    if (result != 0 && (done || (info->stage & 0xFF) == 0xFF)) {
        result |= 2;
    }
    if ((rep != 0 || seq == curseq) && done) {
        result |= 4;
    }
    return result;
}
/* InitAnim @0x8000ED70 -- start playback of sequence `seq` on `info`, seeding
 * scale, frame, start/trans times.  Returns 0 if the tree has no sequences. */
s32 InitAnim(f32 time, animinfo* info, s32 seq, s32 frame, s32 active)
{
    u8* s;
    s16 nf;
    u16 rep;
    f32 sc;

    if (seq >= info->numseqs) {
        ErrorPrintf(lbl_80110758, seq, info->numseqs - 1);
        if (info->numseqs < 1) {
            return 0;
        }
        seq = 0;
    }
    info->atime = gClockTime;
    info->active = (s16)active;
    s = (u8*)info->seqheader + seq * 0x30;
    nf = *(s16*)(s + 0x20);
    rep = *(u16*)(s + 0x24);
    if (*(s16*)(s + 0x22) < 1) {
        info->seqscale = info->animscale;
    } else {
        info->seqscale = (f32)(lbl_803457C0 * (f64)(f32)*(s16*)(s + 0x22) *
                               (f64)info->animscale);
    }
    if (nf < frame) {
        frame = 0;
    }
    sc = info->seqscale * lbl_803441A8;
    info->frame = (f32)frame;
    if (time == lbl_803457B4) {
        info->starttime = -(info->frame * sc - info->atime);
        info->transtime = lbl_803457B4;
    } else {
        info->starttime =
            (f32)(-(f64)(info->frame * sc - info->atime) - lbl_803457C0);
        info->transtime =
            (f32)((f64)(f32)((f64)info->atime + time) - lbl_803457C0);
    }
    info->setpanim = 1;
    info->transfrac = lbl_803457B4;
    info->animseq0 = info->animseq;
    info->animseq = seq;
    info->stage &= 0xf000;
    info->repeat = rep;
    info->numframes = nf;
    return 1;
}
/* CalcAnimInfo @0x8000EF18 -- advance one animinfo along the game clock:
 * step/quantize the frame, honour looping/one-shot, and cross-fade the
 * transition fraction while the trans window is open. */
s32 CalcAnimInfo(animinfo* info)
{
    f64 sc;
    f64 at;
    f64 inv;
    f64 t;
    f64 fl;
    s32 nf;

    if (info == NULL) {
        return 0;
    }
    info->atime = gClockTime;
    if (info->setpanim == 2) {
        info->setpanim = 0;
    }
    if ((info->stage & 0xFF) == 0xFF) {
        if ((info->stage & 0x100) == 0) {
            return 0;
        }
        info->stage = info->stage & 0xFF00;
    }
    if (info->active == 0) {
        return 0;
    }
    nf = *(s16*)((u8*)info->seqheader + info->animseq * 0x30 + 0x20);
    if (nf == 0) {
        info->active = 0;
        info->stage |= 0xFF;
        info->starttime = info->atime;
        return 0;
    }
    sc = (f64)(info->seqscale * lbl_803441A8);
    at = (f64)info->atime;
    inv = lbl_803457D0 / sc;
    if ((f64)info->transtime > at) {
        info->transfrac = (info->atime - info->starttime) /
                          (info->transtime - info->starttime);
        return 0;
    }
    if (info->animseq0 != info->animseq) {
        info->starttime = -(f32)((f64)info->frame * sc - at);
        info->transtime = lbl_803457B4;
        info->transfrac = lbl_803457B4;
        info->animseq0 = info->animseq;
        info->setpanim = 1;
    }
    t = (info->atime - info->starttime) * (f32)inv;
    fl = floorf((f32)(lbl_803457D8 + t));
    if ((info->flags & 2) == 0 ||
        (f64)fabsf((f32)(t - fl)) < lbl_803457E0 || sc < (f64)lbl_803441A8) {
        t = fl;
    }
    if (lbl_803457D8 + (f32)(nf - 1) <= t) {
        info->stage |= 0xFF;
        info->frame = (f32)(nf - 1);
        if (info->repeat != 0) {
            info->starttime = info->atime;
            info->stage |= 0x100;
            info->setpanim = 1;
            return 15;
        }
        info->stage &= 0xFEFF;
        info->transfrac = lbl_803457B4;
        return 0;
    }
    if (t < lbl_803457B4) {
        t = lbl_803457B4;
    }
    info->frame = (f32)t;
    return 1;
}
/* SetupAnimHeader @0x8000F184 -- snap an animinfo to sequence `seq`, frame
 * interpolated across [lo,hi], and cache the animheader's first three words. */
#pragma opt_lifetimes off
#pragma opt_propagation off
s32 AnimateTreeFrame(f32 time, animinfo* info, s32 seq, s32 lo, s32 hi)
{
    if (seq >= info->numseqs) {
        return 0;
    }
    if (seq != info->animseq) {
        info->starttime = info->atime;
        info->animseq = seq;
    }
    info->numframes =
        *(s16*)((u8*)info->seqheader + info->animseq * 0x30 + 0x20);
    info->transfrac = lbl_803457B4;
    if (hi > lo) {
        info->frame = time * (f32)(hi - lo) + (f32)lo;
    } else {
        info->frame = (f32)lo;
    }
    if (info->frame > (f32)(info->numframes - 1)) {
        info->frame = (f32)(info->numframes - 1);
    }
    time = info->frame;
    if (time < lbl_803457B4) {
        info->frame = lbl_803457B4;
    }
    info->transtime = lbl_803457B4;
    info->setpanim = 1;
    lbl_803441B8 = ((u32*)info->animheader)[0];
    lbl_803441B4 = ((u32*)info->animheader)[1];
    lbl_803441B0 = ((u32*)info->animheader)[2];
    return 1;
}
#pragma opt_lifetimes reset
#pragma opt_propagation reset

/* DoAnimation @0x8000F2D8 -- evaluate one tree node's animation for this
 * frame: locate the (byte-swapped) key data, blend the pose, and build the
 * output transform (or the identity when the node has no animation). */
u32 DoAnimation(int* node, animinfo* info, f32* outmtx, s32* outrot,
                f32* posoff)
{
    u16* key;
    void* data;
    u16 raw;
    u32 flags;
    u32 next;
    union {
        u32 w;
        u8 b[4];
    } so, sd;
    s32 pose[12];

    if (*node == 0) {
        return 0;
    }
    key = (u16*)(*node + info->animseq * 8);
    so.w = *(u32*)(key + 2);
    sd.b[0] = so.b[3];
    sd.b[1] = so.b[2];
    sd.b[2] = so.b[1];
    sd.b[3] = so.b[0];
    data = (void*)(((s32*)info->animheader)[3] + sd.w);
    raw = *key;
    flags = (u16)(((raw & 0xFF) << 8) | (raw >> 8));
    next = (u16)(((key[1] & 0xFF) << 8) | (key[1] >> 8));
    if (info->setpanim != 0) {
        *(u16*)((u8*)node + 8) = 0xFFFF;
        *(u16*)((u8*)node + 10) = 0;
        info->setpanim = 2;
    }
    if ((raw & 0xF) == 0 && (raw >> 8) == 0) {
        if (outmtx != NULL) {
            CopyMat3(gIdentityMatrix, outmtx);
            if (posoff == NULL) {
                outmtx[0xC] = lbl_803457B4;
                outmtx[0xD] = lbl_803457B4;
                outmtx[0xE] = lbl_803457B4;
            } else {
                outmtx[0xC] = posoff[0];
                outmtx[0xD] = posoff[1];
                outmtx[0xE] = posoff[2];
            }
        }
        ZeroAnimData(node);
    } else if (CalcAnimation(node, pose, data, info, flags, next) != 0) {
        if (outmtx != NULL) {
            if ((raw & 0x80) == 0) {
                CreateRYPMatrix(outmtx, pose, data, info, flags, next);
            } else {
                CreatePYRMatrix(outmtx, pose, data, info, flags, next);
            }
            if (posoff == NULL) {
                outmtx[0xC] = *(f32*)&pose[4];
                outmtx[0xD] = *(f32*)&pose[5];
                outmtx[0xE] = *(f32*)&pose[6];
            } else {
                outmtx[0xC] = *(f32*)&pose[4] + posoff[0];
                outmtx[0xD] = *(f32*)&pose[5] + posoff[1];
                outmtx[0xE] = *(f32*)&pose[6] + posoff[2];
            }
            if (outrot != NULL) {
                outrot[0] = pose[8];
                outrot[1] = pose[9];
                outrot[2] = pose[10];
            }
        }
        if (info->transfrac == lbl_803457B4) {
            node[0xC] = pose[0];
            node[0xD] = pose[1];
            node[0xE] = pose[2];
            node[0x18] = pose[4];
            node[0x19] = pose[5];
            node[0x1A] = pose[6];
            node[0x24] = pose[8];
            node[0x25] = pose[9];
            node[0x26] = pose[10];
        }
    }
    return flags;
}

/* CalcAnimation @0x8000F534 -- evaluate the atree pose, blending the three
 * transform channels by transfrac, then mirror if the seq's flag bit is set. */
s32 CalcAnimation(u8* mtx, u8* pose, u32* p3, animinfo* info, u32 p5, s32 p6)
{
    f32* pyr = (f32*)(pose + 0x10);
    f32* xyz = (f32*)(pose + 0x20);
    f32 tf = info->transfrac;
    s32 ok = GetAnimAngXYZVal(info->frame, mtx, pose, p3, p5, p6,
                              info->numframes);
    if (ok != 0) {
        if (lbl_803457E8 != (f64)tf) {
            InterpPYR(tf, mtx + 0x30, pose, pose);
            InterpXYZ(tf, mtx + 0x60, pyr, pyr);
            InterpXYZ(tf, mtx + 0x90, xyz, xyz);
        }
        if ((info->flags & 1) != 0) {
            *(f32*)(pose + 4) = -*(f32*)(pose + 4);
            *(f32*)(pose + 8) = -*(f32*)(pose + 8);
            *pyr = -*pyr;
            *xyz = -*xyz;
        }
    }
    return ok;
}

#undef STUB
