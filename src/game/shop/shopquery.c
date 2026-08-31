#include "types.h"
#include "game/player.h"

#define offsetof(type, memb) ((u32) & ((type*)0)->memb)

/* Small shop/player item-query helpers (leaf functions from the SHOP-region
 * source, sitting just before the shop-screen draw code). Names describe
 * observed behaviour; exact Midway identifiers are unconfirmed. */

extern struct {
    /* 0x0 */ s32 unk0;
    /* 0x4 */ s32 unk4;
} lbl_80122F8C[];

extern Player gPlayers[4]; /* gPlayerRecords[4], stride 0x335C */

/* Per-item shop entry record, stride 80, held in shop.c's lbl_80344C14 table
 * (shop.wad "ITEM" chunk); shop.c carries the same view as DSItem. Field names
 * from the Xbox PDB `shop_item` (research/xbox_symbols/misc.h Id=3505,
 * Size=0x50), whose size equals this record's independently verified stride and
 * whose price@0x48 shop.c had already recovered under the same name. Only the
 * fields this TU dereferences are named. */
typedef struct {
    /* 0x00 */ s8 blit[32];
    /* 0x20 */ s8 desc[32];
    /* 0x40 */ f32 scale;
    /* 0x44 */ s32 type;
    /* 0x48 */ s32 price;
    /* 0x4C */ s32 amount;
} DSItem; /* size 0x50 = 80 */

s32 ItemDefValid(u8* p)
{
    s32 idx = *(s32*)(p + offsetof(DSItem, type));

    if (lbl_80122F8C[idx].unk0 != 0 && lbl_80122F8C[idx].unk4 != 0) {
        return 1;
    }
    return 0;
}

s32 PlayerItemState(s32 pidx, u8* cursor)
{
    s32 st = *(s32*)(cursor + offsetof(DSItem, type));
    Player* pl = &gPlayers[pidx];

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
    if (pl->item_body_lo != 0) {
        return st;
    }
    goto ret;

body_hi:
    if (pl->item_body_hi != 0) {
        return st;
    }

ret:
    return -1;
}
