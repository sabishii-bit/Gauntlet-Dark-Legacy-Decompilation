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
extern void* player_multiple_models[];
extern u8 lbl_802828B0[];
extern char lbl_80347E3C[];
extern void* InitCustomEffect();
extern s32 MBOX_FindTexture_Sub();
extern s32 AudioFindSound();
extern void* MBOX_ReallyFindObject();
extern s32* AtreeFindMbidxNode();
extern void sprintf();
void fn_8008A678();

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

/* PlayerSfxInitData @0x8008A5F4 -- clear every record's two handles to -1,
 * then run fn_8008A678 to resolve the effect/sound for each. */
void PlayerSfxInitData(s32* player, u32* records, s32 count, void* param4)
{
    s32 off;
    u32* rec;
    s32 i;

    for (i = 0, off = 0; i < count; i++, off += 0x50) {
        rec = (u32*)((u8*)records + off);
        rec[2] = -1;
        rec[3] = -1;
    }
    for (i = 0, off = 0; i < count; i++, off += 0x50) {
        fn_8008A678(player, (u32*)((u8*)records + off), param4);
    }
}

/* fn_8008A678 @0x8008A678 -- resolve one record's custom-effect handle (rec[2])
 * and audio/mbox handle (rec[3]) if not already set. */
void fn_8008A678(s32* player, u32* rec, void* p11, s32 p12, void* p13)
{
    if ((s32)rec[2] == -1) {
        if ((rec[0] & 0xF000100) != 0) {
            s32 alt = ((s32*)player_multiple_models)[*player * 0x13 + 4];
            rec[2] = MBOX_FindTexture_Sub((char*)(rec + 4), 0, alt, alt, 0);
            p12 = alt;
            alt = ((s32*)player_multiple_models)[*player * 0x13 + 13];
            if (rec[2] == 0) {
                rec[2] = MBOX_FindTexture_Sub((char*)(rec + 4), 0, alt, alt, 0);
                p12 = alt;
            }
            if (rec[2] == 0) {
                p12 = -1;
                rec[2] = MBOX_FindTexture_Sub((char*)(rec + 4), 0, -1, -1, 0);
            }
        } else {
            p12 = *(s16*)((u8*)rec + 0x32);
            rec[2] = (u32)InitCustomEffect(p11, (char*)(rec + 4),
                                           *(s16*)((u8*)rec + 0x30), p12);
        }
    }
    if ((s32)rec[3] == -1) {
        if ((rec[0] & 0xF000000) == 0) {
            if (*(char*)(rec + 8) == 0) {
                rec[3] = 0xFFFFFFFF;
            } else {
                rec[3] = AudioFindSound((char*)(rec + 8), 0, 1, p12);
            }
        } else if (*(char*)(rec + 8) == 0) {
            rec[3] = 0xFFFFFFFF;
        } else {
            u32* node;
            sprintf(lbl_802828B0, lbl_80347E3C, (char*)(player + 0x1B0),
                    (char*)(rec + 8));
            node = (u32*)AtreeFindMbidxNode(
                (s32*)player[0x1F],
                MBOX_ReallyFindObject(lbl_802828B0, player[0x1FD],
                                      player[0x1FD], 1, p13));
            if (node == 0) {
                rec[3] = 0xFFFFFFFF;
            } else {
                rec[3] = *node;
            }
        }
    }
}

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
