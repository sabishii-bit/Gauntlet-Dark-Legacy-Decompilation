/*
 * psfx.c -- GCN PSFX.OBJ scaffold.
 *
 * Player-specific effects, damage, weapon trails, and player-data loading.
 * The last body scans for the player-data files; the following one-instruction
 * body is PSX2.OBJ's LoadVU1GameLogic.
 *
 * .text       0x80089120..0x8008BC50
 * extab       0x80006BE8..0x80006C50
 * extabindex  0x8000AA2C..0x8000AAC8
 *
 * Player-data model (lbl_80282930[4], one header per player):
 *   header: s16 count @0, SfxRecord* records @4, s32 slot @0x24.
 *   SfxRecord: 0x50 bytes; u32 flags @0, s32 handle @8 (word[2]).
 */

#include "types.h"

extern u8* lbl_80282930[4];
extern void ClearCustomEffect(s32 index);

#define STUB(address, name) void name(void) {}

STUB(0x80089120, PlayerDoWeapTrail)
STUB(0x80089350, fn_80089350)
STUB(0x800898DC, fn_800898DC)
STUB(0x80089EA8, fn_80089EA8)
STUB(0x8008A0E4, fn_8008A0E4)
STUB(0x8008A34C, fn_8008A34C)

/* PlayerSfxClearData @0x8008A584 -- release the custom-effect handle of every
 * record whose flags lack the 0x0F000100 bits, marking each slot free. */
#pragma dont_inline on
void PlayerSfxClearData(u32* rec, s32 count)
{
    s32 i;
    for (i = 0; i < count; i++) {
        if ((rec[0] & 0xF000100) == 0 && (s32)rec[2] >= 0) {
            ClearCustomEffect(rec[2]);
            rec[2] = -1;
        }
        rec += 0x14;
    }
}
#pragma dont_inline off

STUB(0x8008A5F4, PlayerSfxInitData)
STUB(0x8008A678, fn_8008A678)

/* ClearAllPlyrData @0x8008A82C -- clear every player's sfx records. */
void ClearAllPlyrData(void)
{
    s32 i;
    for (i = 0; i < 4; i++) {
        u8* hdr = lbl_80282930[i];
        if (hdr != 0) {
            PlayerSfxClearData(*(u32**)(hdr + 4), *(s16*)hdr);
            *(s32*)(lbl_80282930[i] + 0x24) = 0;
        }
    }
}

/* ClearPlyrData @0x8008A898 -- clear one player's sfx records (loop inlined). */
void ClearPlyrData(s32 player)
{
    u8* hdr = lbl_80282930[player];
    s16 count = *(s16*)hdr;
    u32* rec = *(u32**)(hdr + 4);
    s32 i;
    for (i = 0; i < count; i++, rec += 0x14) {
        if ((rec[0] & 0xF000100) == 0 && (s32)rec[2] >= 0) {
            ClearCustomEffect(rec[2]);
            rec[2] = -1;
        }
    }
    *(s32*)(lbl_80282930[player] + 0x24) = 0;
}

STUB(0x8008A928, LoadPlyrData)
STUB(0x8008BAF0, LoadPdataFile)

#undef STUB
