/* pb_texture.c -- Midway "bulletproof" texture / TLUT management TU.
 *
 * Xbox counterpart: pb_texture.obj (shell3D.pdb). On Xbox this is the D3D
 * texture-cache layer (pbResetTexture/pbSetupTexture/pbSetTexture/...); the
 * GameCube build replaces the D3D calls with GX texture-object setup and adds
 * a hardware TLUT (palette) region manager (GXInitTlutRegion / GXLoadTlut /
 * GXInitTexObjCI / GXInitTexObjTlut). Function order and the TLUT helpers are
 * GCN-specific, so most internal helpers keep fn_ names; the confidently
 * mapped ones use their shell3D.pdb / behavioural names.
 *
 * .text 0x800C6AD4-0x800C79E4 (19 functions). Compiled -Cpp_exceptions on
 * (cflags_demo): every LR-saving function carries an extab/extabindex entry.
 *
 * NonMatching: the small accessors are reconstructed faithfully; the large
 * texture/TLUT walkers are best-effort structural reconstructions.
 */

#include "types.h"

/* ------------------------------------------------------------------ */
/* GX texture / TLUT SDK (already-matched gx lib -- treated as extern) */
/* ------------------------------------------------------------------ */
typedef u8 GXBool;
void GXInitTlutRegion(void* region, u32 tmem_addr, u32 tlut_size);
void GXSetTlutRegionCallback(void* cb);
void GXInitTlutObj(void* tlut, void* data, u32 fmt, u16 n_entries);
void GXInitTexObj(void* obj, void* data, u16 w, u16 h, u32 fmt, u32 ws, u32 wt,
                  GXBool mipmap);
void GXInitTexObjCI(void* obj, void* data, u16 w, u16 h, u32 fmt, u32 ws, u32 wt,
                    GXBool mipmap, u32 tlut_name);
void GXInitTexObjTlut(void* obj, u32 tlut_name);
void GXInitTexObjUserData(void* obj, void* user);
void GXLoadTlut(void* tlut, u32 tlut_name);
void DCFlushRange(void* start, u32 len);

/* ------------------------------------------------------------------ */
/* game / MSL runtime externs                                          */
/* ------------------------------------------------------------------ */
void* memset(void* dst, int c, u32 len);
long long __shl2i(int hi, int lo, int shift);
void* AllocMem32(u32 size);
void pbSetTexture(void* texObj);                     /* pb_objregs.c */
extern void fn_800C6AB4(int);                        /* pb_objregs.c */
extern void* MBRomTexPtr(int);                       /* pb model helper */
extern void FatalErrorf(char* fmt, ...); /* dbg printf */

/* ------------------------------------------------------------------ */
/* module structs (offsets verified against the disassembly)           */
/* ------------------------------------------------------------------ */

/* Per-texture context selected as gWinGlobals->cur2; default = sDefaultCtx. */
typedef struct PbTexCtx {
    u8  _pad00[0x2b0];
    s32 unk2b0;   /* 0x2b0 : reset to -1 */
    s32 unk2b4;   /* 0x2b4 : get/set via fn_800C7864 / fn_800C7874 */
    s32 unk2b8;   /* 0x2b8 */
    s32 unk2bc;   /* 0x2bc */
    s32 unk2c0;   /* 0x2c0 */
} PbTexCtx;

/* *gWinGlobals view used by this TU (the window/model manager block). */
typedef struct PbTexMgr {
    u8       _pad00[0x30];
    void*    tbl;    /* 0x30 : model/texture descriptor table */
    u8       _pad34[0x0c];
    PbTexCtx* cur2;  /* 0x40 : current per-texture context */
} PbTexMgr;
extern PbTexMgr* gWinGlobals;

/* ------------------------------------------------------------------ */
/* module data (owned by dtk auto splits -- referenced as extern here) */
/* ------------------------------------------------------------------ */

/* TLUT region manager: 0x30-byte header, then GXTlutRegionObj[] at +0x30,
 * an s32 handle table at +0x320, and per-region bookkeeping at +0x52c/+0x580.
 * dtk splits it into lbl_802C7438 (header) + lbl_802C7468 (the +0x30 body). */
typedef struct PbTlutMgrView {
    u8  _00[0x30];           /* 0x000 : per-class use counts / header */
    u8  regions[0x2f][0x10]; /* 0x030 : GXTlutRegionObj[] */
    s32 handles[0x2f];       /* 0x320 : per-region texture handle */
    u8  _3dc[0x150];         /* 0x3dc */
    u32 owners[0x15];        /* 0x52c : owning texture entry per lightmap */
    u16 keys[0x15];          /* 0x580 : lightmap TLUT keys */
} PbTlutMgrView;
extern PbTlutMgrView lbl_802C7438; /* TLUT manager base (0x802C7438) */
extern u8 lbl_802C7468[];   /* &mgr.regions[0]     (0x802C7468) */

extern u8 lbl_802C7A08[];   /* default PbTexCtx     (0x802C7A08, 0x2C8) */
extern u8 lbl_802C7CD0[];   /* scratch struct       (0x802C7CD0, 0x28) */
extern s32 lbl_802A5D0C[4]; /* per-arg start table  (0x802A5D0C) */

/* sdata2 TLUT-count config (each an 8-byte pair) */
extern const s32 lbl_80348FC8[2];  /* [0] = first region index */
extern const s32 lbl_80348FD0[2];  /* [0] = count1, [1] = count2 */
extern const s32 lbl_80348FD8[2];
extern const s32 lbl_80348FE0[2];

/* .data texture-format lookup, 0x68 bytes (stride 0x4 entries) */
extern u8 lbl_801283C0[];

/* TLUT region allocation bitmasks + state (.sbss) */
extern u64 lbl_803450D8;   /* 64-bit residency mask (lo half = 0x803450DC) */
extern u64 lbl_803450E0;   /* 64-bit lock mask      (lo half = 0x803450E4) */
extern s32 lbl_803450E8;
extern s32 lbl_803450EC;
extern s32 lbl_80345108;
extern s32 lbl_8034510C;
extern s32 lbl_80345110;
extern s32 lbl_80345114;
extern s32 lbl_80344E8C;

/* "current texture" shadow registers, reset to -1 by pbResetTextures */
extern s32 lbl_80343F70;
extern s32 lbl_80343F74;
extern s32 lbl_80343F78;
extern s32 lbl_80343F7C;
extern s32 lbl_80343F80;
extern s32 lbl_80343F84;
extern s32 lbl_80343F88;
extern s32 lbl_80343F8C;

/* debug string */
extern char lbl_80116AC0[];  /* "Lightmaps > %dK, %d/%d: %s" */

/* per-size-class TLUT region u64 bitmasks (.data, 8 bytes per class) */
extern const u64 lbl_80116AB0[];

/* ================================================================== */

/* GX callback: map a tlut_name to its GXTlutRegionObj (stride 0x10). */
static void* sTlutRegionCallback(u32 name);

/* Initialise all hardware TLUT regions and register the region callback. */
void pbInitTlutRegions(void) {
    s32 i;
    u32 addr = 0xc0000;
    u8* mgr = (u8*)&lbl_802C7438;
    u8* slot;

    for (i = lbl_80348FC8[0]; i < lbl_80348FD0[0]; i++) {
        slot = mgr + i * 0x10;
        GXInitTlutRegion(slot + 0x30, addr, 1);
        addr += 0x200;
    }
    for (; i < lbl_80348FD0[1]; i++) {
        slot = mgr + i * 0x10;
        GXInitTlutRegion(slot + 0x30, addr, 0x10);
        addr += 0x2000;
    }
    GXSetTlutRegionCallback(sTlutRegionCallback);
    for (i = 0; i < 0x2f; i++) {
        *(s32*)((slot = mgr + i * 4) + 0x320) = -1;
    }
}

static void* sTlutRegionCallback(u32 name) {
    return lbl_802C7468 + name * 0x10;
}

/* TLUT-region allocator: bump the per-region use counts, and if this size
 * class is not resident allocate the least-used region slot and remap it.
 * Returns the region index (a texture handle). GCN-only. */
int fn_800C6BB4(u8 sizeClass, s32 handle) {
    u8* mgr = (u8*)&lbl_802C7438;
    s32 first = lbl_80348FC8[sizeClass];
    s32 last  = lbl_80348FD0[sizeClass];
    u64 held;
    s32 i;

    for (i = first; i < last; i++) {
        if (mgr[i] < 0xff)
            mgr[i]++;
    }

    held = lbl_803450D8;
    if ((lbl_80116AB0[sizeClass] & held) != lbl_80116AB0[sizeClass]) {
        s32 z = 0;
        while (first < last) {
            if ((held & __shl2i(0, 1, first)) == z)
                break;
            first++;
        }
        {
            u8* q;
            lbl_803450D8 |= __shl2i(0, 1, first);
            q = mgr + first * 4;
            mgr[first] = z;
            *(s32*)(q += 0x320) = handle;
            return first;
        }
    }
    {
        s32 j = first + 1;
        s32 best = mgr[first];
        u8* p = mgr + j;
        u64 locks = lbl_803450E0;
        s32 z2 = 0;
        u8* hp;
        u32 old;
        s32 hi;
        u32 lo;

        for (; j < last; j++, p++) {
            if ((locks & __shl2i(0, 1, j)) == z2) {
                if (*p > best) {
                    best = *p;
                    first = j;
                }
            }
        }
        hp = mgr + first * 4;
        old = *(s32*)(hp += 0x320);
        hi = (s16)(old >> 16);
        lo = old & 0xFFFF;
        if (hi != -1) {
            u8* desc = *(u8**)((u8*)gWinGlobals->tbl + hi * 16 + 4);
            u8* t = *(u8**)(desc + 0x80) + (s16)lo * 48;
            t[44] = -1;
            t[45] = -1;
        } else {
            *(s32*)(mgr + (s16)lo * 16 + 0x5B8) = -1;
        }
        mgr[first] = 0;
        *(s32*)hp = handle;
        return first;
    }
}

/* Build the GX texture objects for model `id`'s texture list. */
void pbSetupTextures(s32 id) {
    u8* lookup;
    u8* desc;
    u8* obj;
    s32 i;
    u32 width;
    u32 height;
    u8* data;
    u8* texObjs;
    s32 bitsPerPixel;
    s32 texOffset;
    s32 objOffset;
    s32 formatOffset;
    s32 format;
    s32 paletteEntries;
    u8 unused[12];

    lookup = lbl_801283C0;
    i = 0;
    texOffset = 0;
    objOffset = 0;
    desc = *(u8**)((u8*)gWinGlobals->tbl + id * 0x10 + 4);
    texObjs = *(u8**)(desc + 0x80);

    while ((u32)i < *(u32*)(desc + 0x48)) {
        u8* tex;
        u8* texBase;
        s32 previousOffset;
        u32 currentData;
        u32* previous;
        s32 j;
        u8 found;

        obj = texObjs + objOffset;
        *(s8*)(obj + 0x2C) = -1;
        found = 0;
        *(s8*)(obj + 0x2D) = -1;
        tex = (texBase = *(u8**)(desc + 0x58)) + texOffset;

        if ((*(u16*)(tex + 2) & 0x100) == 0) {
            j = 0;
            previousOffset = 0;
            while ((u32)j < (u32)i && !found) {
                previous = *(u32**)(desc + 0x58);
                if ((currentData = *(u32*)(tex + 4)) ==
                    (previous = (u32*)((u8*)previous + previousOffset))[1]) {
                    s32 paletteIndex = *(s16*)((u8*)previous + 0xE);
                    found = 1;
                    *(u16*)(tex + 0xE) = paletteIndex;
                }
                j++;
                previousOffset += 0x10;
            }

            if (!found) {
                s32 type;
                s32 subtype;

                *(u16*)(tex + 0xE) = (s16)i;
                type = *(u8*)tex >> 4;
                width = *(u16*)(tex + 0xA);
                height = *(u16*)(tex + 0xC);
                data = *(u8**)(tex + 4);
                subtype = *(u8*)tex & 7;

                if ((type & 8) != 0) {
                    if (type == 8) {
                        format = 0x13;
                    } else {
                        format = 0x14;
                    }
                } else {
                    if (type > 0) {
                        s32 loadedFormat;

                        formatOffset = type * 4;
                        {
                            u8* entry = lookup;
                            entry += formatOffset;
                            loadedFormat = *(s32*)(entry + 0x28);
                        }
                        paletteEntries = 0x100;
                        if ((format = loadedFormat, loadedFormat == 0x14)) {
                            paletteEntries = 0x10;
                        }
                        DCFlushRange(data, paletteEntries << 1);
                        GXInitTlutObj(obj + 0x20, data, 2,
                                     (u16)paletteEntries);
                        {
                            u8* entry = lookup;
                            entry += formatOffset;
                            data += *(s32*)(entry + 0x3C);
                        }
                    } else {
                        u8* subtypeEntry = lookup;
                        subtypeEntry += subtype * 4;
                        format = *(s32*)(subtypeEntry + 0x50);
                    }
                }

                switch (format) {
                case 0x14:
                    bitsPerPixel = 4;
                    break;
                case 0x13:
                case 0x1B:
                    bitsPerPixel = 8;
                    break;
                case 2:
                    bitsPerPixel = 0x10;
                    break;
                }

                {
                    s32 imageBits = width * height;
                    s32 imageBytes;
                    imageBits = bitsPerPixel * imageBits;
                    imageBytes = imageBits / 8;
                    DCFlushRange(data, imageBytes);
                }
                if (bitsPerPixel <= 8) {
                    s32 paletteClass;
                    paletteClass = 0;
                    if (bitsPerPixel == 8) {
                        paletteClass = 1;
                    }
                    GXInitTexObjCI(obj, data, (u16)width, (u16)height,
                                   lbl_80348FD8[paletteClass], 1, 1, 0,
                                   lbl_80348FE0[paletteClass]);
                } else {
                    GXInitTexObj(obj, data, (u16)width, (u16)height, 5,
                                 1, 1, 0);
                }
                GXInitTexObjUserData(obj, (void*)-1);
            }
        }
        i++;
        texOffset += 0x10;
        objOffset += 0x30;
    }
}

/* Reset every model's per-texture flag buffer and the current-texture
 * shadow registers (pbResetTextures on Xbox). */
void pbResetTextures(void) {
    PbTexMgr* g = gWinGlobals;
    s32 i;

    for (i = 0; i < *(s32*)g->tbl; i++) {
        u8* e = (u8*)g->tbl + i * 0x10;
        u8** pp = (u8**)(e + 0x4);
        if (*(s32*)(e + 0x10) == 0) {
            u8* p = *pp;
            memset(*(void**)(p + 0x78), 0, (*(u32*)(p + 0x48) + 7) & ~7);
        }
    }
    lbl_80343F78 = -1;
    lbl_80343F7C = -1;
    lbl_80343F88 = -1;
    lbl_80343F8C = -1;
    lbl_80343F80 = -1;
    lbl_80343F84 = -1;
}

/* empty helper (kept: retains its slot in the module) */
void fn_800C70C4(void) {}

/* Free the TLUT regions held by model `id` and clear the residency masks. */
void fn_800C70C8(s32 id) {
    u8* e4 = (u8*)gWinGlobals->tbl + id * 0x10 + 0x4;
    u8* desc;
    u8* tex;
    s32 region;
    s32 i;

    if (*(s32*)(e4 + 0xc) != 0)
        return;
    desc = *(u8**)e4;
    tex = *(u8**)(desc + 0x80);
    if (tex == 0)
        return;

    for (i = 0; i < *(u32*)(desc + 0x48); i++) {
        if ((region = (s8)tex[i * 0x30 + 0x2d]) != -1) {
            u64 bit = __shl2i(0, 1, region);
            lbl_803450D8 &= ~bit;
            lbl_803450E0 &= ~bit;
            lbl_802C7438.handles[region] = -1;
        }
    }
    for (i = 0; i < 0x15; i++) {
        if (lbl_802C7438.owners[i] >= (u32)tex &&
            lbl_802C7438.owners[i] <= (u32)(tex + (*(s32*)(desc + 0x48) - 1) * 0x30)) {
            lbl_802C7438.owners[i] = 0;
            lbl_802C7438.keys[i] = 0xFFFF;
        }
    }
}

/* Initialise the texture entries for model `id`, then set them up. */
void fn_800C7214(s32 id) {
    u8* mgr = gWinGlobals->tbl;
    u8* e = mgr + id * 0x10 + 0x4;
    u8* desc = *(u8**)e;
    u8* t;
    s32 shift;
    s32 i;

    if (*(s32*)(e + 0xc) > 7)
        return;

    shift = *(s32*)(desc + 0x70);
    for (i = 0; i < *(u32*)(desc + 0x48); i++) {
        t = *(u8**)(desc + 0x58) + i * 0x10;
        *(s16*)(t + 0xe) = -1;
        t[0x1] = (s8)id;
        if (*(u16*)(t + 0xa) < 1 || *(u16*)(t + 0xc) < 1 ||
            (*(u16*)(t + 0x2) & 0x100)) {
            *(u16*)(t + 0x2) |= 0x100;
            *(s32*)(t + 0x4) = 0;
        } else {
            *(s32*)(t + 0x4) = shift + *(s32*)(t + 0x4);
        }
    }
    pbSetupTextures(id);
}

/* Load lightmap TLUTs for every model, reporting overflow via the debug
 * printf ("Lightmaps > %dK, %d/%d: %s"). */
void fn_800C72DC(void) {
    s32 m;
    s32 loaded = 0;
    PbTexMgr* wg = gWinGlobals;

    lbl_80345110 = 1;
    for (m = 0; m < *(s32*)wg->tbl; m++) {
        u8* e = (u8*)wg->tbl + m * 0x10;
        u8** ep = (u8**)(e + 0x4);
        s32 t;
        s32 base;
        if (*(s32*)(e + 0x10) != 0)
            continue;
        if (*(u16*)(*ep + 0x7e) == 0)
            continue;
        base = *(u16*)(*ep + 0x7c);
        for (t = 0; t < *(u16*)(*ep + 0x7e); t++) {
            if (fn_800C7558((m << 16) | (u16)(base + t)) == 0) {
                u8* tb = (u8*)wg->tbl + 0x4;
                FatalErrorf(lbl_80116AC0, 0x200, t + 1, *(u16*)(*ep + 0x7e),
                            *(void**)(m * 0x10 + tb));
            }
            loaded++;
        }
    }
    if (loaded == 0)
        fn_800C7558(0);
    lbl_80345110 = 0;
}

/* Allocate + load a palette TLUT into a hardware region (GCN-only). */
void fn_800C73E0(void) {
    u8* mgr = (u8*)&lbl_802C7438;
    s32 i;

    if (lbl_80345114 == 1) {
        DCFlushRange((void*)lbl_803450E8, 4);
        GXInitTlutObj(mgr + 0x5AC, (void*)lbl_803450E8, 2, 16);
        DCFlushRange((void*)lbl_803450EC, 4);
        GXInitTlutObj(mgr + 0x5BC, (void*)lbl_803450EC, 2, 0x100);
        lbl_80345114 = 2;
    }
    if (lbl_80345114 == 2) {
        s32* cur;
        s32 region;
        u64 bit;
        if (*(cur = (s32*)(mgr + 0x5B8)) == -1) {
            *cur = fn_800C6BB4(0, 0xFFFF0000);
            region = *cur;
            bit = __shl2i(0, 1, region);
            lbl_803450E0 |= bit;
            lbl_80345108 = lbl_80345108 + 1;
            GXLoadTlut(mgr + 0x5AC, region);
        }
    }
    if (lbl_80345114 == 0) {
        lbl_803450E8 = (s32)AllocMem32(0x20);
        lbl_803450EC = (s32)AllocMem32(0x200);
        for (i = 0; i < 16; i++) {
            ((u16*)lbl_803450E8)[i] = (i * 2048) | 0xFFF;
        }
        for (i = 0; i < 256; i++) {
            ((u16*)lbl_803450EC)[i] = (i * 128) | 0xFFF;
        }
        *(s32*)(mgr + 0x5C8) = -1;
        *(s32*)(mgr + 0x5B8) = -1;
        lbl_80345114 = 1;
    }
}

/* Ensure a texture's TLUT is resident, loading it if necessary, and bind it
 * to the texture object (GXInitTexObjTlut). Returns non-zero on success. */
typedef struct PbRomTexture {
    u8 format;
    s8 model;
    u16 flags;
    u8 _pad04[10];
    s16 index;
} PbRomTexture;

typedef struct PbTextureObject {
    u8 _pad00[0x20];
    u8 tlutObject[0x0C];
    s8 region;
    s8 paletteRegion;
    u8 _pad2E[2];
} PbTextureObject;

int fn_800C7558(s32 key) {
    PbTexMgr* globals;
    u8* modelDesc;
    PbRomTexture* rom;
    PbTextureObject* texture;
    u8* manager;
    u8* special;
    s32 model;
    s32 texnum;
    s32 format;
    u8 which;
    u8 newLoad;
    s32 region;
    s32 i;
    u8 pathPad[8];

    model = (u32)key >> 16;
    texnum = (u16)key;
    manager = (u8*)&lbl_802C7438;
    which = 1;
    globals = gWinGlobals;
    modelDesc = *(u8**)((u8*)globals->tbl + model * 0x10 + 4);
    rom = (PbRomTexture*)(*(u8**)(modelDesc + 0x58) + texnum * 0x10);
    if (*(s32*)((u8*)globals->tbl + model * 0x10 + 0x10) != 0 ||
        (rom->flags & 0x100) != 0) {
        model = 0;
        texnum = 0;
        rom = (PbRomTexture*)MBRomTexPtr(0);
    }
    if (lbl_80345110 == 0) {
        u8* used = *(u8**)(*(u8**)((u8*)globals->tbl + model * 0x10 + 4) +
                              0x78);
        used[texnum] |= 1 << lbl_80343F78;
    }

    format = rom->format >> 4;
    if ((format & 8) == 0 && format <= 0) {
        return 1;
    }
    modelDesc =
        *(u8**)((u8*)globals->tbl + (model = rom->model) * 0x10 + 4);
    texture = (PbTextureObject*)(*(u8**)(modelDesc + 0x80) +
                                 (texnum = rom->index) * 0x30);
    if ((format & 8) != 0) {
        u8* specialBase;

        newLoad = 1;
        region = 1;
        if (format != 8) {
            region = 0;
            which = 0;
        }
        specialBase = manager + region * 0x10;
        special = specialBase + 0x5AC;
        if (*(s32*)(specialBase + 0x5B8) == -1) {
            if (which == 0) {
                lbl_80345108++;
            } else {
                lbl_8034510C++;
            }
            *(s32*)(special + 0x0C) =
                fn_800C6BB4(which, 0xFFFF0000 | (u16)region);
            GXLoadTlut(special, *(s32*)(special + 0x0C));
            newLoad = 0;
        }
        if (texture->region != *(s32*)(special + 0x0C)) {
            GXInitTexObjTlut(texture, *(s32*)(special + 0x0C));
            texture->region = (s8)*(s32*)(special + 0x0C);
        }
    } else {
        newLoad = 1;
        if (*(s32*)(lbl_801283C0 + format * 4) == 0x14) {
            which = 0;
        }
        if (texture->paletteRegion == -1) {
            if (which == 0) {
                lbl_80345108++;
            } else {
                lbl_8034510C++;
            }
            region = fn_800C6BB4(which,
                                  (u16)texnum | (model << 16));
            texture->region = (s8)region;
            texture->paletteRegion = (s8)region;
            GXLoadTlut(texture->tlutObject, texture->paletteRegion);
            GXInitTexObjTlut(texture, texture->paletteRegion);
            newLoad = 0;
        }
    }
    if (newLoad != 0) {
        s32 first = lbl_80348FC8[which];
        s32 last = lbl_80348FD0[which];
        for (i = first; i < last; i++) {
            if (manager[i] < 0xFF) {
                manager[i]++;
            }
        }
        manager[texture->region] = 0;
    }
    return 1;
}

/* Return the 1-based index (1..2) of the first flag bit set in
 * (mask & desc->flags[idx]), or 0 if none. */
typedef struct TEXDESCENT {
    /* 0x0 */ u32 key;
    /* 0x4 */ u8* desc;
    /* 0x8 */ u32 unk8;
    /* 0xC */ u32 unkC;
} TEXDESCENT; /* 0x10-stride entry of the tbl */

int fn_800C780C(s32 id, s32 idx, u32 mask) {
    TEXDESCENT* t = (TEXDESCENT*)gWinGlobals->tbl;
    u8* p = t[id].desc;
    u8* flags = *(u8**)(p + 0x78);
    s32 i;
    u32 bit;

    mask &= flags[idx];
    if (mask != 0) {
        for (i = 0, bit = 1; i < 2; i++, bit <<= 1) {
            if (mask & bit)
                return i + 1;
        }
    }
    return 0;
}

/* set gWinGlobals->cur2->unk2b4 */
void fn_800C7864(s32 v) {
    gWinGlobals->cur2->unk2b4 = v;
}

/* get gWinGlobals->cur2->unk2b4 */
s32 fn_800C7874(void) {
    return gWinGlobals->cur2->unk2b4;
}

/* Reset the frame texture state: clear the two counters, wipe the scratch
 * struct, seed the model cursor from a per-arg table, then walk the models. */
void fn_800C7884(s32 arg) {
    PbTexMgr* g = gWinGlobals;
    lbl_80345108 = 0;
    lbl_8034510C = 0;
    memset(lbl_802C7CD0, 0, 0x24);
    lbl_80344E8C = lbl_802A5D0C[arg];
    while (*(s32*)g->tbl > lbl_80344E8C) {
        fn_800C70C8(lbl_80344E8C);
        lbl_80344E8C++;
    }
}

/* copy the two shadow registers out to caller-supplied slots */
void fn_800C7914(s32* a, s32* b) {
    *a = lbl_80343F70;
    *b = lbl_80343F74;
}

/* thin forwarder to pbSetTexture (framed: pbSetTexture may throw) */
void fn_800C7928(void* texObj) {
    pbSetTexture(texObj);
}

/* Bind a default per-texture context if none, and mark it dirty (unk2b0=-1). */
void fn_800C7948(void) {
    PbTexMgr* g = gWinGlobals;
    if (g->cur2 == 0)
        g->cur2 = (PbTexCtx*)lbl_802C7A08;
    g->cur2->unk2b0 = -1;
}

/* Initialise the texture subsystem: install the default context + defaults,
 * init the TLUT regions, and reset the object registers. */
void pbInitTexture(void) {
    PbTexMgr* g = gWinGlobals;

    g->cur2 = (PbTexCtx*)lbl_802C7A08;
    g->cur2->unk2b0 = -1;
    g->cur2->unk2b4 = 0;
    g->cur2->unk2b8 = 0;
    g->cur2->unk2bc = 5;
    g->cur2->unk2c0 = 1;
    pbInitTlutRegions();
    fn_800C6AB4(0);
}
