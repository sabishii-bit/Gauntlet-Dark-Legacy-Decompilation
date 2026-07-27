/*
 * mb_struct.c - MB rom-texture descriptor management (mb_struct.obj).
 *
 * The MB library's ROM-texture table accessors: MBRomTexPtr returns a pointer
 * to a texture descriptor entry (indexed via the model-manager's texture table,
 * asserting texidx >= 0 with 'MBRomTexPtr: texidx < 0' via FatalError), and the
 * Set/Copy helpers move 16-byte descriptors between slots. fn_800B9E4C is a
 * small init that zeroes the descriptor context at 0x802C29B8.
 *
 * Range 0x800B9E4C..0x800BA084 (4 fns), between mb_objects.c (ends 0x800B9E4C)
 * and mb_tree.c (starts 0x800BA084). Names from the Xbox shell3D PDB
 * (mb_struct.obj: MBRomTexPtr/MBGetRomTexture/MBSetRomTexture/MBCopyTexture;
 * MBGetRomTexture appears inlined on GC). cflags_demo, C++ exceptions on.
 *
 * Status: NonMatching wired skeleton (stubs). Full bodies not reconstructed.
 */

#include "types.h"

typedef struct MBRomTexture {
    u16 field0;
    u16 field2;
    u32 word4;
    u16 field8;
    u16 fieldA;
    u32 wordC;
} MBRomTexture;

typedef struct MBRomTexturePageData {
    u8 _pad00[0x58];
    MBRomTexture* textures;
} MBRomTexturePageData;

typedef struct MBRomTexturePage {
    u32 unk00;
    MBRomTexturePageData* data;
    u32 unk08;
    u32 unk0C;
} MBRomTexturePage;

typedef struct MBStructGlobals {
    u8 _pad00[0x30];
    MBRomTexturePage* texturePages;
} MBStructGlobals;

extern MBStructGlobals* gWinGlobals;
extern u32 lbl_802C29B8[];
extern void fn_800C70C4(s32 texidx);
extern void FatalError(const char* message, u32 code);

static inline MBRomTexture* romTexPtr(s32 texidx, MBStructGlobals* globals)
{
    MBRomTexturePage* page;
    s32 pageOffset;

    if (texidx < 0) {
        FatalError("MBRomTexPtr: texidx < 0", 0x800000);
    }
    pageOffset = (texidx >> 16) << 4;
    page = globals->texturePages;
    page = (MBRomTexturePage*) ((u8*) page + pageOffset);
    return &page->data->textures[(u32) texidx & 0xFFFF];
}

/* 0x800B9E4C */
void fn_800B9E4C(void)
{
    lbl_802C29B8[16] = 0;
    lbl_802C29B8[17] = 0;
    lbl_802C29B8[18] = 0;
    lbl_802C29B8[19] = 0;
    lbl_802C29B8[20] = 0;
    lbl_802C29B8[21] = 0;
    lbl_802C29B8[22] = 0;
    lbl_802C29B8[23] = 0;
    lbl_802C29B8[25] = 0;
    lbl_802C29B8[24] = 0;
    lbl_802C29B8[26] = 0;
    lbl_802C29B8[27] = 0;
    lbl_802C29B8[4] = 0;
    lbl_802C29B8[5] = 0;
    lbl_802C29B8[6] = 0;
    lbl_802C29B8[7] = 0;
    lbl_802C29B8[8] = 0;
    lbl_802C29B8[9] = 0;
    lbl_802C29B8[10] = 0;
    lbl_802C29B8[11] = 0;
    lbl_802C29B8[13] = 0;
    lbl_802C29B8[14] = 0;
    lbl_802C29B8[15] = 0;
    lbl_802C29B8[12] = 0;
}

MBRomTexture* MBRomTexPtr(s32 texidx);

/* 0x800B9EBC */
void MBCopyTexture(s32 sourceIndex, s32 destinationIndex)
{
    MBRomTexture* source;
    MBRomTexture* destination;
    u8 unused[16];

    fn_800C70C4(destinationIndex);
    source = romTexPtr(sourceIndex, gWinGlobals);
    destination = romTexPtr(destinationIndex, gWinGlobals);
    *destination = *source;
}

/* 0x800B9F88 */
void MBSetRomTexture(s32 texidx, volatile MBRomTexture* texture)
{
    u8 unused[8];
    register u16 keep8;
    register u16 keep2;
    MBRomTexture* destination;
    MBStructGlobals* globals;

    fn_800C70C4(texidx);
    globals = gWinGlobals;
    destination = romTexPtr(texidx, globals);
    keep2 = destination->field2;
    keep8 = destination->field8;
    *destination = *texture;
    destination->field2 = keep2;
    destination->field8 = keep8;
}

/* 0x800BA024 */
MBRomTexture* MBRomTexPtr(s32 texidx)
{
    u8 unused[8];

    return romTexPtr(texidx, gWinGlobals);
}
