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

STUB(0x8000E994, SetupAnimHeader)

s32 AnimDone(void* anim)
{
    if ((*(u16*)((u8*)anim + 0x36) & 0xFF) != 0) {
        return 1;
    }
    return 0;
}

STUB(0x8000EB70, AnimateTree)
s32 InitAnim(f32 time, animinfo* info, s32 seq, s32 frame, s32 active) {}
STUB(0x8000EF18, CalcAnimInfo)
/* SetupAnimHeader @0x8000F184 -- snap an animinfo to sequence `seq`, frame
 * interpolated across [lo,hi], and cache the animheader's first three words. */
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
    if ((f32)(info->numframes - 1) < info->frame) {
        info->frame = (f32)(info->numframes - 1);
    }
    if (info->frame < lbl_803457B4) {
        info->frame = lbl_803457B4;
    }
    info->transtime = lbl_803457B4;
    info->setpanim = 1;
    lbl_803441B8 = ((u32*)info->animheader)[0];
    lbl_803441B4 = ((u32*)info->animheader)[1];
    lbl_803441B0 = ((u32*)info->animheader)[2];
    return 1;
}

STUB(0x8000F2D8, DoAnimation)

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
