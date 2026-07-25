#include "types.h"

/* Small shop/player item-query helpers (leaf functions from the SHOP-region
 * source, sitting just before the shop-screen draw code). Names describe
 * observed behaviour; exact Midway identifiers are unconfirmed. */

extern struct {
    /* 0x0 */ s32 unk0;
    /* 0x4 */ s32 unk4;
} lbl_80122F8C[];

extern u8 lbl_80275AE0[]; /* player array, stride 0x335C */

s32 ItemDefValid(u8* p)
{
    s32 idx = *(s32*)(p + 0x44);

    if (lbl_80122F8C[idx].unk0 != 0 && lbl_80122F8C[idx].unk4 != 0) {
        return 1;
    }
    return 0;
}

s32 PlayerItemState(s32 pidx, u8* cursor)
{
    s32 st = *(s32*)(cursor + 0x44);
    u8* pl = lbl_80275AE0 + pidx * 0x335C;

    if (st == 2) {
        goto ret;
    }
    if (st >= 2) {
        goto arm_hi;
    }
    if (st >= 1) {
        goto body_lo;
    }
    goto ret;

arm_hi:
    if (st >= 4) {
        goto ret;
    }
    goto body_hi;

body_lo:
    if (*(s32*)(pl + 0x1EB8) != 0) {
        return st;
    }
    goto ret;

body_hi:
    if (*(s32*)(pl + 0x1EBC) != 0) {
        return st;
    }

ret:
    return -1;
}
