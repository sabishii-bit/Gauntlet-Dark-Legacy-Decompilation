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
extern u8 lbl_8012006C[];
extern void* lbl_80120E00[16];
extern char lbl_801142A0[];
extern char lbl_801142D4[];
extern char lbl_80347E44[];
extern char lbl_80347E4C[];
extern s32 mlmLastFileSize;
extern char lbl_80347E3C[];
extern void* InitCustomEffect();
extern s32 MBOX_FindTexture_Sub();
extern s32 AudioFindSound();
extern void* MBOX_ReallyFindObject();
extern s32* AtreeFindMbidxNode();
extern s32 sprintf(void* dst, const char* fmt, ...);
extern s32 FileExists();
extern void* AllocFile();
extern s32 MLMReadFile();
extern void FatalErrorf(const char* fmt, ...);
extern void ErrorPrintf(const char* fmt, ...);
extern void* AllocMem();
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
    i = 0;
    while (i < count) {
        if ((rec[0] & 0xF000100) == 0 && (s32)rec[2] >= 0) {
            ClearCustomEffect(rec[2]);
            rec[2] = -1;
        }
        i++;
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
    s32 secondOff;
    s32 j;

    for (i = 0, off = 0; i < count; i++, off += 0x50) {
        rec = (u32*)((u8*)records + off);
        rec[2] = -1;
        rec[3] = -1;
    }
    j = 0;
    secondOff = 0;
    while (j < count) {
        fn_8008A678(player, (u32*)((u8*)records + secondOff), param4);
        j++;
        secondOff += 0x50;
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

/* --- LoadPlyrData support ------------------------------------------------ */

/* pdata wad payload is little-endian (PS2/Xbox heritage); MBSetupWad reports
 * whether the archive needs byte-swapping and these fix each record up. */
#define SWAP16(v)                                                       \
    do {                                                                \
        u16 _t = (v);                                                   \
        (v) = (u16)((((u8*)&_t)[1] << 8) | ((u8*)&_t)[0]);              \
    } while (0)

#define SWAP32(v)                                                       \
    do {                                                                \
        u8 _t[8];                                                       \
        *(u32*)_t = (v);                                                \
        _t[4] = _t[3];                                                  \
        _t[5] = _t[2];                                                  \
        _t[6] = _t[1];                                                  \
        _t[7] = _t[0];                                                  \
        (v) = *(u32*)(_t + 4);                                          \
    } while (0)

#define SWAPF(v)                                                        \
    do {                                                                \
        f32 _in = (v);                                                  \
        f32 _out;                                                       \
        u32 _w = *(u32*)&_in;                                           \
        SWAP32(_w);                                                     \
        *(u32*)&_out = _w;                                              \
        (v) = _out;                                                     \
    } while (0)

/* 4-char wad chunk tags kept as strings in sdata2 (chars are signed) */
#define WADTAG(s) (((s)[0] << 24) | ((s)[1] << 16) | ((s)[2] << 8) | (s)[3])

/* player-data header (first chunk of the pdata wad) */
typedef struct PsfxHeader {
    /* 0x00 */ s16 count;    /* number of SfxRecords            */
    /* 0x02 */ s16 _02;
    /* 0x04 */ u8* records;  /* SfxRecord[count], 0x50 each     */
    /* 0x08 */ u8* moves;    /* third chunk rows, 0x58 each     */
    /* 0x0C */ u8 _0c[0x18];
    /* 0x24 */ s32 resolved; /* handles resolved this level     */
} PsfxHeader;

extern char lbl_80347E54[]; /* header-chunk wad tag  */
extern char lbl_80347E5C[]; /* record-chunk wad tag  */
extern char lbl_80347E64[]; /* move-chunk wad tag    */
extern u32 gControllerButtons;
extern u32 sFlags;
extern s32 fn_80055F68(s32 a, s32 b);
extern u8 MBSetupWad(void* wad, void* data);
extern void* MBGetFromWad(void* wad, s32 tag, s32* count);
extern void* memcpy(void* dst, const void* src, u32 n);
extern u8 gPlayers[];
extern void PlayerSfxInitData(s32* player, u32* records, s32 count, void* param4);

/* LoadPlyrData @0x8008A928 -- ensure player plr has class cls's pdata wad
 * loaded and parsed: reload the per-class file if needed (debug flag 0x10
 * forces a disk re-read), copy it into the player's load buffer, pull the
 * three chunks out of the wad, byte-swap every record when the archive is
 * foreign-endian, then resolve the effect/sound handles. */
void LoadPlyrData(s32 plr, s32 cls, s32 resolve)
{
    s32 wad[4];
    s32 n1;
    s32 n2;
    s32 n3;
    PsfxHeader* hdr;
    u8* p;
    s32 mode;
    s32 i;
    s32 j;
    s32 k;
    s32 off;
    u8 swapped;

    mode = 0;
    if (plr < 0) {
        return;
    }
    if (cls < 0) {
        return;
    }

    if (cls != *(s32*)(lbl_802828B0 + 0x20 + plr * 4) ||
        (*(s32*)(gPlayers + plr * 0x335C + 0xE8) != 0 && resolve != 0)) {
        if ((sFlags & 0x10) == 0 || fn_80055F68(0, -1) == 0) {
            mode = 1;
        } else {
            mode = 2;
        }
    }

    if (mode == 0) {
        /* already loaded: only (re)resolve the handles */
        if (resolve != 0) {
            hdr = (PsfxHeader*)lbl_80282930[plr];
            if (hdr->resolved == 0) {
                PlayerSfxInitData((s32*)(gPlayers + plr * 0x335C),
                                  (u32*)hdr->records, hdr->count,
                                  ((void**)player_multiple_models)[plr * 0x13 + 18]);
                hdr->resolved = 1;
            }
        }
        return;
    }

    if (mode == 2) {
        /* forced re-read of the class file from disk */
        sprintf(lbl_802828B0, "%s.wad", (char*)lbl_8012006C + cls * 4);
        if (FileExists("pdata", lbl_802828B0)) {
            if (lbl_80120E00[cls] == 0) {
                lbl_80120E00[cls] = AllocFile("pdata", lbl_802828B0);
                *(s32*)(lbl_802828B0 + 0x40 + cls * 4) = mlmLastFileSize;
            } else if (!MLMReadFile("pdata", lbl_802828B0,
                                    *(s32*)(lbl_802828B0 + 0x40 + cls * 4),
                                    lbl_80120E00[cls])) {
                FatalErrorf("pdata file %s: file on disk go too large for buffer",
                            lbl_802828B0);
            }
        } else {
            ErrorPrintf("No player data file: %s", lbl_802828B0);
            lbl_80120E00[cls] = 0;
            *(s32*)(lbl_802828B0 + 0x40 + cls * 4) = 0;
        }
    }

    if (lbl_80120E00[cls] == 0) {
        FatalErrorf("No player data file: %s", lbl_802828B0);
        return;
    }

    memcpy(*(void**)(lbl_802828B0 + 0x30 + plr * 4), lbl_80120E00[cls],
           *(s32*)(lbl_802828B0 + 0x40 + cls * 4));
    swapped = MBSetupWad(wad, *(void**)(lbl_802828B0 + 0x30 + plr * 4));

    hdr = (PsfxHeader*)MBGetFromWad(wad, WADTAG(lbl_80347E54), &n1);
    lbl_80282930[plr] = (u8*)hdr;
    hdr = (PsfxHeader*)lbl_80282930[plr];
    hdr->records = MBGetFromWad(wad, WADTAG(lbl_80347E5C), &n2);
    hdr = (PsfxHeader*)lbl_80282930[plr];
    hdr->moves = MBGetFromWad(wad, WADTAG(lbl_80347E64), &n3);
    hdr = (PsfxHeader*)lbl_80282930[plr];
    hdr->resolved = 0;
    *(s32*)(lbl_802828B0 + 0x20 + plr * 4) = cls;

    if (swapped) {
        /* header rows: 0x180 bytes each */
        off = 0;
        for (i = 0; i < n1; i++, off += 0x180) {
            p = (u8*)lbl_80282930[plr] + off;
            SWAP16(*(u16*)(p + 0x00));
            SWAP16(*(u16*)(p + 0x02));
            SWAP16(*(u16*)(p + 0x0C));
            SWAP16(*(u16*)(p + 0x0E));
            SWAP16(*(u16*)(p + 0x10));
            SWAP16(*(u16*)(p + 0x12));
            SWAP16(*(u16*)(p + 0x14));
            SWAP16(*(u16*)(p + 0x16));
            SWAP16(*(u16*)(p + 0x18));
            SWAP16(*(u16*)(p + 0x1A));
            SWAP16(*(u16*)(p + 0x1C));
            SWAP16(*(u16*)(p + 0x1E));
            SWAP16(*(u16*)(p + 0x20));
            SWAP16(*(u16*)(p + 0x22));
            SWAP32(*(u32*)(p + 0x24));
            SWAPF(*(f32*)(p + 0x28));
            SWAPF(*(f32*)(p + 0x2C));
            SWAPF(*(f32*)(p + 0x30));
            SWAPF(*(f32*)(p + 0x34));
            SWAPF(*(f32*)(p + 0x38));
            SWAPF(*(f32*)(p + 0x3C));
            SWAPF(*(f32*)(p + 0x40));
            SWAPF(*(f32*)(p + 0x44));
            SWAPF(*(f32*)(p + 0x48));
            SWAPF(*(f32*)(p + 0x4C));
            SWAPF(*(f32*)(p + 0x50));
            SWAPF(*(f32*)(p + 0x54));
            SWAPF(*(f32*)(p + 0x58));
            SWAPF(*(f32*)(p + 0x17C));
            for (k = 0; k < 3; k++) {
                SWAPF(*(f32*)(p + 0x5C + k * 4));
                SWAPF(*(f32*)(p + 0x158 + k * 4));
                SWAPF(*(f32*)(p + 0x164 + k * 4));
                SWAPF(*(f32*)(p + 0x170 + k * 4));
            }
            for (j = 0; j < 10; j++) {
                for (k = 0; k < 3; k++) {
                    SWAPF(*(f32*)(p + 0x68 + j * 0xC + k * 4));
                    SWAPF(*(f32*)(p + 0xE0 + j * 0xC + k * 4));
                }
            }
        }

        /* sfx records: 0x50 bytes each */
        off = 0;
        for (i = 0; i < n2; i++, off += 0x50) {
            p = ((PsfxHeader*)lbl_80282930[plr])->records + off;
            SWAP16(*(u16*)(p + 0x30));
            SWAP16(*(u16*)(p + 0x32));
            SWAP32(*(u32*)(p + 0x00));
            SWAP32(*(u32*)(p + 0x04));
            SWAP32(*(u32*)(p + 0x08));
            SWAP32(*(u32*)(p + 0x0C));
            SWAPF(*(f32*)(p + 0x40));
            SWAPF(*(f32*)(p + 0x44));
            SWAPF(*(f32*)(p + 0x48));
            SWAP32(*(u32*)(p + 0x4C));
            for (k = 0; k < 3; k++) {
                SWAPF(*(f32*)(p + 0x34 + k * 4));
            }
        }

        /* move rows: 0x58 bytes each */
        off = 0;
        for (i = 0; i < n3; i++, off += 0x58) {
            p = ((PsfxHeader*)lbl_80282930[plr])->moves + off;
            SWAP16(*(u16*)(p + 0x00));
            SWAP16(*(u16*)(p + 0x02));
            SWAP16(*(u16*)(p + 0x48));
            SWAP16(*(u16*)(p + 0x4A));
            SWAP16(*(u16*)(p + 0x4C));
            SWAP16(*(u16*)(p + 0x4E));
            SWAP16(*(u16*)(p + 0x50));
            SWAP16(*(u16*)(p + 0x52));
            SWAP16(*(u16*)(p + 0x54));
            SWAPF(*(f32*)(p + 0x08));
            SWAPF(*(f32*)(p + 0x0C));
            SWAPF(*(f32*)(p + 0x10));
            SWAPF(*(f32*)(p + 0x14));
            SWAPF(*(f32*)(p + 0x18));
            SWAPF(*(f32*)(p + 0x1C));
            SWAPF(*(f32*)(p + 0x20));
            SWAPF(*(f32*)(p + 0x24));
            SWAPF(*(f32*)(p + 0x28));
            SWAPF(*(f32*)(p + 0x38));
            SWAPF(*(f32*)(p + 0x3C));
            SWAPF(*(f32*)(p + 0x40));
            SWAPF(*(f32*)(p + 0x44));
            SWAP32(*(u32*)(p + 0x04));
            for (k = 0; k < 3; k++) {
                SWAPF(*(f32*)(p + 0x2C + k * 4));
            }
        }
    }

    if (resolve != 0) {
        hdr = (PsfxHeader*)lbl_80282930[plr];
        PlayerSfxInitData((s32*)(gPlayers + plr * 0x335C), (u32*)hdr->records,
                          hdr->count,
                          ((void**)player_multiple_models)[plr * 0x13 + 18]);
        hdr->resolved = 1;
    }
}
/* LoadPdataFile @0x8008BAF0 -- preflight all 16 class pdata files, retain
 * their largest size, then allocate four reusable player load buffers. */
void LoadPdataFile(void)
{
    u8 unused[8];
    u8* temp = lbl_802828B0;
    u8* sizePtr;
    s32 maxSize;
    s32 index;
    s32 offset;

    maxSize = 0;
    index = 0;
    offset = 0;
    do {
        *(void**)((u8*)lbl_80120E00 + offset) = 0;
        sprintf(temp, "%s.wad", lbl_8012006C + offset);
        if (FileExists("pdata", temp)) {
            if (*(void**)((u8*)lbl_80120E00 + offset) == 0) {
                *(void**)((u8*)lbl_80120E00 + offset) =
                    AllocFile("pdata", temp);
                sizePtr = temp + offset;
                *(u32*)(sizePtr + 0x40) = mlmLastFileSize;
            } else if (!MLMReadFile("pdata", temp,
                                    *(u32*)((sizePtr = temp + offset) + 0x40),
                                    *(void**)((u8*)lbl_80120E00 + offset))) {
                FatalErrorf("pdata file %s: file on disk go too large for buffer", temp);
            }
        } else {
            ErrorPrintf("No player data file: %s", temp);
            *(void**)((u8*)lbl_80120E00 + offset) = 0;
            sizePtr = temp + offset;
            *(u32*)(sizePtr + 0x40) = 0;
        }
        sizePtr = temp + offset;
        if (*(s32*)(sizePtr + 0x40) > maxSize) {
            maxSize = *(s32*)(sizePtr + 0x40);
        }
        index++;
        offset += 4;
    } while (index < 16);

    index = 0;
    offset = 0;
    do {
        sizePtr = temp + offset;
        *(s32*)(sizePtr + 0x20) = -1;
        *(void**)(sizePtr + 0x30) = AllocMem(maxSize);
        *(s32*)(sizePtr + 0x80) = 0;
        index++;
        offset += 4;
    } while (index < 4);
}

#undef STUB
