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
STUB(0x8000F184, AnimateTreeFrame)
STUB(0x8000F2D8, DoAnimation)
STUB(0x8000F534, CalcAnimation)

#undef STUB
