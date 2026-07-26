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
extern void* fn_800BA024(int, int);                  /* pb model helper */
extern void fn_800BC590(char* fmt, int, int, int, int, void*); /* dbg printf */

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
extern u8 lbl_802C7438[];   /* TLUT manager base   (0x802C7438) */
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
extern s32 lbl_803450D8;
extern s32 lbl_803450DC;
extern s32 lbl_803450E0;
extern s32 lbl_803450E4;
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

/* ================================================================== */

/* GX callback: map a tlut_name to its GXTlutRegionObj (stride 0x10). */
static void* sTlutRegionCallback(u32 name);

/* Initialise all hardware TLUT regions and register the region callback. */
void pbInitTlutRegions(void) {
    s32 i;
    u32 addr = 0xc0000;

    for (i = lbl_80348FC8[0]; i < lbl_80348FD0[0]; i++) {
        GXInitTlutRegion(lbl_802C7438 + 0x30 + i * 0x10, addr, 1);
        addr += 0x200;
    }
    for (; i < lbl_80348FD0[1]; i++) {
        GXInitTlutRegion(lbl_802C7438 + 0x30 + i * 0x10, addr, 0x10);
        addr += 0x2000;
    }
    GXSetTlutRegionCallback(sTlutRegionCallback);
    for (i = 0; i < 0x2f; i++)
        *(s32*)(lbl_802C7438 + 0x320 + i * 4) = -1;
}

static void* sTlutRegionCallback(u32 name) {
    return lbl_802C7468 + name * 0x10;
}

/* TLUT-region allocator: bump the per-region use counts, and if this size
 * class is not resident allocate the least-used region slot and remap it.
 * Returns the region index (a texture handle). GCN-only. */
int fn_800C6BB4(u8 sizeClass, s32 handle) {
    u8* mgr = lbl_802C7438;
    s32 first = lbl_80348FC8[sizeClass];
    s32 last  = lbl_80348FD0[sizeClass];
    s32 i;
    s32 mask_lo, mask_hi;

    for (i = first; i < last; i++) {
        if (mgr[i] >= 0xff)
            break;
        mgr[i]++;
    }

    mask_lo = lbl_803450D8;
    mask_hi = lbl_803450DC;
    (void)mask_lo;
    (void)mask_hi;

    /* find / evict a region slot (see disassembly) */
    for (i = first; i < last; i++) {
        s64 bit = __shl2i(0, 1, i);
        if (((lbl_803450D8 & (s32)(bit >> 32)) | (lbl_803450DC & (s32)bit)) == 0) {
            lbl_803450D8 |= (s32)(bit >> 32);
            lbl_803450DC |= (s32)bit;
            mgr[i] = 0;
            *(s32*)(mgr + 0x320 + i * 4) = handle;
            return i;
        }
    }
    return first;
}

/* Build the GX texture objects for model `id`'s texture list. */
void pbSetupTextures(s32 id) {
    u8* mgr = gWinGlobals->tbl;
    u8* desc = *(u8**)(mgr + id * 0x10 + 0x4);
    u8* texObjs = *(u8**)(desc + 0x80);
    u8* texList = *(u8**)(desc + 0x58);
    s32 count = *(s32*)(desc + 0x48);
    s32 i;

    for (i = 0; i < count; i++) {
        u8* obj = texObjs + i * 0x30;
        u8* tex = texList + i * 0x10;
        obj[0x2c] = 0xff;
        obj[0x2d] = 0xff;
        /* format dispatch + GXInitTexObj / GXInitTexObjCI / GXInitTlutObj,
         * followed by GXInitTexObjUserData(obj, -1). See disassembly. */
        (void)tex;
        (void)lbl_801283C0;
        GXInitTexObjUserData(obj, (void*)-1);
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
    u8* mgr = lbl_802C7438;
    u8* desc = *(u8**)((u8*)gWinGlobals->tbl + id * 0x10 + 0x4);
    u8* tex;
    s32 i;

    if (*(s32*)(desc + 0xc) != 0)
        return;
    desc = *(u8**)desc;
    tex = *(u8**)(desc + 0x80);
    if (tex == 0)
        return;

    for (i = 0; i < *(s32*)(desc + 0x48); i++) {
        s32 region = (s8)tex[i * 0x30 + 0x2d];
        if (region != -1) {
            s64 bit = __shl2i(0, 1, region);
            lbl_803450DC &= ~(s32)bit;
            lbl_803450E4 &= ~(s32)bit;
            lbl_803450D8 &= ~(s32)(bit >> 32);
            lbl_803450E0 &= ~(s32)(bit >> 32);
            *(s32*)(mgr + 0x320 + region * 4) = -1;
        }
    }
    for (i = 0; i < 0x15; i++) {
        u8* p = mgr + i * 4;
        if (*(u32*)(p + 0x52c) >= (u32)desc &&
            *(u32*)(p + 0x52c) <= (u32)(desc + (*(s32*)(desc + 0x48) - 1) * 0x30)) {
            *(s32*)(p + 0x52c) = 0;
            *(s16*)(mgr + i * 2 + 0x580) = 0xffff;
        }
    }
}

/* Initialise the texture entries for model `id`, then set them up. */
void fn_800C7214(s32 id) {
    u8* mgr = gWinGlobals->tbl;
    u8* e = mgr + id * 0x10 + 0x4;
    u8* desc = *(u8**)e;
    s32 i;

    s32 shift;

    if (*(s32*)(e + 0xc) > 7)
        return;

    shift = *(s32*)(desc + 0x70);
    for (i = 0; i < *(u32*)(desc + 0x48); i++) {
        u8* t = *(u8**)(desc + 0x58) + i * 0x10;
        *(s16*)(t + 0xe) = -1;
        t[0x1] = (s8)id;
        if (*(u16*)(t + 0xa) >= 1 && *(u16*)(t + 0xc) >= 1) {
            if (*(u16*)(t + 0x2) & 0x100) {
                *(u16*)(t + 0x2) |= 0x100;
                *(s32*)(t + 0x4) = 0;
            } else {
                *(s32*)(t + 0x4) = shift + *(s32*)(t + 0x4);
            }
        }
    }
    pbSetupTextures(id);
}

/* Load lightmap TLUTs for every model, reporting overflow via the debug
 * printf ("Lightmaps > %dK, %d/%d: %s"). */
void fn_800C72DC(void) {
    u8* mgr = gWinGlobals->tbl;
    s32 loaded = 0;
    s32 m;

    lbl_80345110 = 1;
    for (m = 0; m < *(s32*)mgr; m++) {
        u8* e = mgr + m * 0x10;
        u8* desc;
        s32 t, n;
        if (*(s32*)(e + 0x10) != 0)
            continue;
        desc = *(u8**)(e + 0x4);
        n = *(u16*)(desc + 0x7e);
        if (n == 0)
            continue;
        for (t = 0; t < *(u16*)(desc + 0x7e); t++) {
            s32 key = (*(u16*)(desc + 0x7c) << 16) | (u16)(*(u16*)(desc + 0x7c) + t);
            if (fn_800C7558(key) == 0) {
                fn_800BC590(lbl_80116AC0, 0x200, m + 1, t + 1,
                            *(u16*)(*(u8**)(e + 0x4) + 0x7e), *(void**)(mgr + m * 0x10 + 0x4));
                loaded++;
            }
        }
    }
    if (loaded == 0)
        fn_800C7558(0);
    lbl_80345110 = 0;
}

/* Allocate + load a palette TLUT into a hardware region (GCN-only). */
int fn_800C73E0(s32 key) {
    void* buf;
    s32 region = fn_800C6BB4((u8)(key >> 8), key);
    buf = AllocMem32(0x200);
    GXInitTlutObj(lbl_802C7438 + 0x30 + region * 0x10, buf, 0, 0x100);
    GXLoadTlut(lbl_802C7438 + 0x30 + region * 0x10, region);
    (void)lbl_803450E8;
    (void)lbl_803450EC;
    (void)lbl_80345108;
    (void)lbl_80345114;
    return region;
}

/* Ensure a texture's TLUT is resident, loading it if necessary, and bind it
 * to the texture object (GXInitTexObjTlut). Returns non-zero on success. */
int fn_800C7558(s32 key) {
    void* palette = fn_800BA024(key >> 12, key);
    s32 region;
    if (palette == 0)
        return 0;
    region = fn_800C6BB4((u8)(key >> 20), key);
    GXLoadTlut(lbl_802C7438 + 0x30 + region * 0x10, region);
    GXInitTexObjTlut((void*)gWinGlobals->tbl, region);
    (void)lbl_80345108;
    (void)lbl_8034510C;
    (void)lbl_801283C0;
    return 1;
}

/* Return the 1-based index (1..2) of the first flag bit set in
 * (mask & desc->flags[idx]), or 0 if none. */
int fn_800C780C(s32 id, s32 idx, u32 mask) {
    /* NonMatching residual: MWCC 1.2.5n folds the +4 member offset into an
     * indexed lwzx; the target uses add + displacement-4 load (open wall). */
    u8* p = *(u8**)((u8*)gWinGlobals->tbl + id * 0x10 + 0x4);
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
void fn_800C7974(void) {
    gWinGlobals->cur2 = (PbTexCtx*)lbl_802C7A08;
    gWinGlobals->cur2->unk2b0 = -1;
    gWinGlobals->cur2->unk2b4 = 0;
    gWinGlobals->cur2->unk2b8 = 0;
    gWinGlobals->cur2->unk2bc = 5;
    gWinGlobals->cur2->unk2c0 = 1;
    pbInitTlutRegions();
    fn_800C6AB4(0);
}
