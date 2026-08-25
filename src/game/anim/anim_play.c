/*
 * anim_play.c -- GCN anim_play.obj scaffold.
 *
 * Function identity is pinned by the Xbox PDB roster and direct call shape:
 * CalcAnimData is the wrapper around GetAnimAngXYZVal, while the final
 * function constructs the 256-entry inverse-delta table.
 *
 * .text       0x8000F628..0x80010A4C
 * extab       0x80005580..0x80005590
 * extabindex  0x80008890..0x800088A8
 */

#include "types.h"

extern f32 light_color[4];
extern const f32 lbl_803457F0;
extern f64 lbl_803457F8, lbl_80345800, lbl_80345808;

/*
 * Per-node anim channel state (0x9C bytes): three channel groups (PYR, XYZ,
 * scale), each holding current/previous keyframe values plus a third vector
 * used as the transition source by CalcAnimation.
 */
typedef struct AnimData {
    u32 unk00;          /* 0x00 */
    u32 unk04;          /* 0x04 */
    s16 curkey;         /* 0x08 */
    s16 prevkey;        /* 0x0A */
    s32 keynum;         /* 0x0C */
    f32 pyr[3];         /* 0x10 current PYR    */
    u32 unk1C;          /* 0x1C */
    f32 prevpyr[3];     /* 0x20 previous PYR   */
    u32 unk2C;          /* 0x2C */
    f32 unk30[3];       /* 0x30 */
    u32 unk3C;          /* 0x3C */
    f32 xyz[3];         /* 0x40 current XYZ    */
    u32 unk4C;          /* 0x4C */
    f32 prevxyz[3];     /* 0x50 previous XYZ   */
    u32 unk5C;          /* 0x5C */
    f32 unk60[3];       /* 0x60 */
    u32 unk6C;          /* 0x6C */
    f32 scale[3];       /* 0x70 current scale  */
    u32 unk7C;          /* 0x7C */
    f32 prevscale[3];   /* 0x80 previous scale */
    u32 unk8C;          /* 0x8C */
    f32 unk90[3];       /* 0x90 */
    u32 unk9C;          /* 0x9C */
} AnimData;

s32 GetAnimAngXYZVal(f32 frame, AnimData* data, f32* pose, u32* keydata, s32 flags, s32 keysize,
                     s32 numframes);

void ZeroAnimData(void* data)
{
    f32 zero = lbl_803457F0;
    f32* value = (f32*)data;

    value[12] = zero;
    value[13] = zero;
    value[14] = zero;
    value[24] = zero;
    value[25] = zero;
    value[26] = zero;
    value[36] = light_color[0];
    value[37] = light_color[1];
    value[38] = light_color[2];
    *(s16*)((u8*)data + 8) = -1;
    *(s16*)((u8*)data + 10) = 0;
}

#define STUB(address, name) void name(void) {}

/* InitAnimData @0x8000F678 -- reset one node's anim-data block to defaults:
 * clear the pose channels to zero and the scale channels to light_color. */
void InitAnimData(u32* data, u32 arg)
{
    f32* v = (f32*)data;
    f32 zero;

    data[0] = arg;
    *(s16*)(data + 2) = 0xFFFF;
    *(s16*)((u8*)data + 10) = 0;
    data[3] = 0xFFFFFFFF;
    zero = lbl_803457F0;
    v[12] = zero;
    v[13] = zero;
    v[14] = zero;
    v[8] = zero;
    v[9] = zero;
    v[10] = zero;
    v[4] = zero;
    v[5] = zero;
    v[6] = zero;
    v[24] = zero;
    v[25] = zero;
    v[26] = zero;
    v[20] = zero;
    v[21] = zero;
    v[22] = zero;
    v[16] = zero;
    v[17] = zero;
    v[18] = zero;
    v[36] = light_color[0];
    v[37] = light_color[1];
    v[38] = light_color[2];
    v[32] = light_color[0];
    v[33] = light_color[1];
    v[34] = light_color[2];
    v[28] = light_color[0];
    v[29] = light_color[1];
    v[30] = light_color[2];
}

/* CalcAnimData @0x8000F740 -- straight passthrough into GetAnimAngXYZVal. */
s32 CalcAnimData(f32 frame, AnimData* data, f32* pose, u32* keydata, s32 flags, s32 keysize,
                 s32 numframes)
{
    return GetAnimAngXYZVal(frame, data, pose, keydata, flags, keysize, numframes);
}

/* InterpXYZ @0x8000F74C -- linear-interpolate a 3-vector by `frac`. */
void InterpXYZ(f32 frac, f32* a, f32* b, f32* out)
{
    s32 i;
    for (i = 0; i < 3; i++) {
        if (a[i] != b[i]) {
            out[i] = frac * (b[i] - a[i]) + a[i];
        } else {
            out[i] = a[i];
        }
    }
}

/* InterpPYR @0x8000F788 -- interpolate a 3-angle vector, wrapping each delta
 * into the shortest arc before blending. */
void InterpPYR(f32 frac, f32* a, f32* b, f32* out)
{
    s32 i;
    for (i = 0; i < 3; i++) {
        f32 av = a[i];
        f32 bv = b[i];

        if (av != bv) {
            bv = bv - av;
            if (bv > lbl_803457F8) {
                bv = bv - lbl_80345800;
            }
            if (bv <= lbl_80345808) {
                bv = bv + lbl_80345800;
            }
            out[i] = frac * bv + av;
        } else {
            out[i] = av;
        }
    }
}
/*
 * GetAnimAngXYZVal @0x8000F7F4 -- evaluate one node's anim channels at
 * `frame`, writing the PYR/XYZ/scale triples into `pose`.  Channel records
 * are packed: a set bitmask in `flags` (low 16 bits) selects which of the
 * nine channels carry data; lbl_80118100 maps channel slot to mask bit.
 * When the 0x2000 flag is set the records after key 0 hold per-channel byte
 * indices into the shared delta tables (lbl_803441B8/B4/B0) instead of
 * absolute floats.  Xbox locals: GetPYR/GetXYZ/GetScale (+Comp variants),
 * NextKey (fn_80010904) and PrevKey (fn_80010850).
 */

extern u32 lbl_80118100[10];
extern f32* lbl_803441B8;
extern f32* lbl_803441B4;
extern f32* lbl_803441B0;
extern const f32 lbl_80345810;
extern f64 lbl_80345818, lbl_80345820, lbl_80345828, lbl_80345830;
extern f32 lbl_8023CBA0[256];

int fn_80010850(u32* bits, s32 n, s32* out);
u32 fn_80010904(u32* bits, s32 start, s32 total);

/* Byte-swap a float through memory (anim data is little-endian). */
static inline f32 SwapFloat(f32 v)
{
    u32 w;

    {
        union {
            u32 w;
            u8 b[4];
        } s, d;

        s.w = *(u32*)&v;
        *(volatile u8*)&d.b[0] = *(volatile u8*)&s.b[3];
        *(volatile u8*)&d.b[1] = *(volatile u8*)&s.b[2];
        *(volatile u8*)&d.b[2] = *(volatile u8*)&s.b[1];
        *(volatile u8*)&d.b[3] = *(volatile u8*)&s.b[0];
        w = d.w;
    }
    return *(f32*)&w;
}

static inline s32 GetPYR(f32* dst, f32* src, s32 flags, s32 n, u32* mask)
{
    s32 i;
    f32 dflt = lbl_803457F0;

    for (i = 0; i < 3; i++) {
        if ((s16)flags & mask[i]) {
            dst[i] = SwapFloat(src[n]);
            n++;
        } else {
            dst[i] = dflt;
        }
    }
    return n;
}

static inline s32 GetPYRComp(f32* dst, u8* src, s32 flags, s32 n, u32* mask)
{
    s32 i;

    for (i = 0; i < 3; i++) {
        if ((s16)flags & mask[i]) {
            dst[i] += SwapFloat(lbl_803441B8[*src]);
            src++;
            n++;
        }
    }
    return n;
}

static inline s32 GetXYZ(f32* dst, f32* src, s32 flags, s32 n, u32* mask)
{
    s32 i;
    f32 dflt = lbl_803457F0;

    for (i = 0; i < 3; i++) {
        if ((s16)flags & mask[i + 3]) {
            dst[i] = SwapFloat(src[n]);
            n++;
        } else {
            dst[i] = dflt;
        }
    }
    return n;
}

static inline s32 GetXYZComp(f32* dst, u8* src, s32 flags, s32 n, u32* mask)
{
    s32 i;

    for (i = 0; i < 3; i++) {
        if ((s16)flags & mask[i + 3]) {
            dst[i] += SwapFloat(lbl_803441B4[*src]);
            src++;
            n++;
        }
    }
    return n;
}

static inline s32 GetScale(f32* dst, f32* src, s32 flags, s32 n, u32* mask)
{
    s32 i;
    f32 dflt = lbl_80345810;

    for (i = 0; i < 3; i++) {
        if ((s16)flags & mask[i + 6]) {
            dst[i] = SwapFloat(src[n]);
            n++;
        } else {
            dst[i] = dflt;
        }
    }
    return n;
}

static inline s32 GetPYRDflt(f32* dst, f32* src, s32 flags, s32 n, u32* mask, f32 dflt)
{
    s32 i;

    for (i = 0; i < 3; i++) {
        if ((s16)flags & mask[i]) {
            dst[i] = SwapFloat(src[n]);
            n++;
        } else {
            dst[i] = dflt;
        }
    }
    return n;
}

static inline s32 GetXYZDflt(f32* dst, f32* src, s32 flags, s32 n, u32* mask, f32 dflt)
{
    s32 i;

    for (i = 0; i < 3; i++) {
        if ((s16)flags & mask[i + 3]) {
            dst[i] = SwapFloat(src[n]);
            n++;
        } else {
            dst[i] = dflt;
        }
    }
    return n;
}

static inline s32 GetScaleDflt(f32* dst, f32* src, s32 flags, s32 n, u32* mask, f32 dflt)
{
    s32 i;

    for (i = 0; i < 3; i++) {
        if ((s16)flags & mask[i + 6]) {
            dst[i] = SwapFloat(src[n]);
            n++;
        } else {
            dst[i] = dflt;
        }
    }
    return n;
}

static inline s32 GetScaleComp(f32* dst, u8* src, s32 flags, s32 n, u32* mask)
{
    s32 i;

    for (i = 0; i < 3; i++) {
        if ((s16)flags & mask[i + 6]) {
            dst[i] += SwapFloat(lbl_803441B0[*src]);
            src++;
            n++;
        }
    }
    return n;
}

s32 GetAnimAngXYZVal(f32 frame, AnimData* data, f32* pose, u32* keydata, s32 flags, s32 keysize,
                     s32 numframes)
{
    s32 curkey = data->curkey;
    s32 prevkey = data->prevkey;
    s32 keynum = data->keynum;
    u32* bits = keydata;
    u32* mask = lbl_80118100;
    f32* cpyr = data->pyr;
    f32* ppyr = data->prevpyr;
    f32* oxyz = pose + 4;
    f32* cxyz = data->xyz;
    f32* pxyz = data->prevxyz;
    f32* oscale = pose + 8;
    f32* cscale = data->scale;
    f32* pscale = data->prevscale;
    s32 comp;
    u8* rec;
    s32 n;
    s32 next;
    s32 diff;
    s32 i;
    f32 t;
    u8 unused[184];

    if (flags & 0x4000) {
        n = GetPYR(pose, (f32*)keydata, flags, 0, mask);
        n = GetXYZ(oxyz, (f32*)keydata, flags, n, mask);
        GetScale(oscale, (f32*)keydata, flags, n, mask);
        data->curkey = 0;
        return 1;
    }
    if (lbl_80345818 == frame) {
        if (curkey != 0 || prevkey != 0) {
        keydata += (numframes + 31) >> 5;
        n = GetPYR(cpyr, (f32*)keydata, flags, 0, mask);
        pose[0] = cpyr[0];
        pose[1] = cpyr[1];
        pose[2] = cpyr[2];
        ppyr[0] = cpyr[0];
        ppyr[1] = cpyr[1];
        ppyr[2] = cpyr[2];
        n = GetXYZ(cxyz, (f32*)keydata, flags, n, mask);
        oxyz[0] = cxyz[0];
        oxyz[1] = cxyz[1];
        oxyz[2] = cxyz[2];
        pxyz[0] = cxyz[0];
        pxyz[1] = cxyz[1];
        pxyz[2] = cxyz[2];
        GetScale(cscale, (f32*)keydata, flags, n, mask);
        oscale[0] = cscale[0];
        oscale[1] = cscale[1];
        oscale[2] = cscale[2];
        pscale[0] = cscale[0];
        pscale[1] = cscale[1];
        pscale[2] = cscale[2];
        data->curkey = 0;
        data->prevkey = 0;
        if (flags & 0x2000) {
            data->keynum = -1;
        } else {
            data->keynum = 0;
        }
        return 1;
        }
    } else {
    keydata += (numframes + 31) >> 5;
    if (curkey < 0) {
        n = GetPYR(cpyr, (f32*)keydata, flags, 0, mask);
        ppyr[0] = cpyr[0];
        ppyr[1] = cpyr[1];
        ppyr[2] = cpyr[2];
        n = GetXYZ(cxyz, (f32*)keydata, flags, n, mask);
        pxyz[0] = cxyz[0];
        pxyz[1] = cxyz[1];
        pxyz[2] = cxyz[2];
        GetScale(cscale, (f32*)keydata, flags, n, mask);
        pscale[0] = cscale[0];
        pscale[1] = cscale[1];
        pscale[2] = cscale[2];
        curkey = 0;
        prevkey = 0;
        if (flags & 0x2000) {
            keynum = -1;
        } else {
            keynum = 0;
        }
    }
    comp = flags & 0x2000;
    if (comp) {
        keydata += keysize;
    }
    {
        f32 one = lbl_80345810;
        f32 zero = lbl_803457F0;

        while (prevkey < frame) {
        if (prevkey > curkey) {
            cpyr[0] = ppyr[0];
            cpyr[1] = ppyr[1];
            cpyr[2] = ppyr[2];
            cxyz[0] = pxyz[0];
            cxyz[1] = pxyz[1];
            cxyz[2] = pxyz[2];
            cscale[0] = pscale[0];
            cscale[1] = pscale[1];
            cscale[2] = pscale[2];
            curkey = prevkey;
        }
        next = fn_80010904(bits, prevkey, numframes);
        if (next >= 0) {
            prevkey = next;
            keynum++;
            if (comp) {
                rec = (u8*)keydata + keynum * keysize;
                n = GetPYRComp(ppyr, rec, flags, 0, mask);
                n = GetXYZComp(pxyz, rec + n, flags, n, mask);
                GetScaleComp(pscale, rec + n, flags, n, mask);
            } else {
                n = GetPYRDflt(ppyr, (f32*)keydata + keynum * keysize, flags, 0, mask, zero);
                n = GetXYZDflt(pxyz, (f32*)keydata + keynum * keysize, flags, n, mask, zero);
                GetScaleDflt(pscale, (f32*)keydata + keynum * keysize, flags, n, mask, one);
            }
        } else {
            frame = prevkey;
        }
        }
    }
    {
        f32 one = lbl_80345810;
        f32 zero = lbl_803457F0;

        while (curkey > frame) {
        if (curkey < prevkey) {
            ppyr[0] = cpyr[0];
            ppyr[1] = cpyr[1];
            ppyr[2] = cpyr[2];
            pxyz[0] = cxyz[0];
            pxyz[1] = cxyz[1];
            pxyz[2] = cxyz[2];
            pscale[0] = cscale[0];
            pscale[1] = cscale[1];
            pscale[2] = cscale[2];
            prevkey = curkey;
        }
        next = fn_80010850(bits, curkey, &keynum);
        if (next >= 0) {
            curkey = next;
            if (comp) {
                rec = (u8*)keydata + keynum * keysize;
                n = GetPYRComp(cpyr, rec, flags, 0, mask);
                n = GetXYZComp(cxyz, rec + n, flags, n, mask);
                GetScaleComp(cscale, rec + n, flags, n, mask);
            } else {
                n = GetPYRDflt(cpyr, (f32*)keydata + keynum * keysize, flags, 0, mask, zero);
                n = GetXYZDflt(cxyz, (f32*)keydata + keynum * keysize, flags, n, mask, zero);
                GetScaleDflt(cscale, (f32*)keydata + keynum * keysize, flags, n, mask, one);
            }
        } else {
            frame = curkey;
        }
        }
    }
    if (curkey < prevkey && prevkey - frame > lbl_80345820) {
        diff = prevkey - curkey;
        if (diff < 256) {
            t = (frame - curkey) * lbl_8023CBA0[diff];
        } else {
            t = (frame - curkey) / diff;
        }
        for (i = 0; i < 3; i++) {
            f32 d = ppyr[i] - cpyr[i];

            if (d > lbl_80345828 && d < lbl_80345830) {
                pose[i] = d * t + cpyr[i];
            } else {
                pose[i] = cpyr[i];
            }
        }
        for (i = 0; i < 3; i++) {
            oxyz[i] = t * (pxyz[i] - cxyz[i]) + cxyz[i];
        }
        for (i = 0; i < 3; i++) {
            oscale[i] = t * (pscale[i] - cscale[i]) + cscale[i];
        }
    } else {
        pose[0] = ppyr[0];
        pose[1] = ppyr[1];
        pose[2] = ppyr[2];
        oxyz[0] = pxyz[0];
        oxyz[1] = pxyz[1];
        oxyz[2] = pxyz[2];
        oscale[0] = pscale[0];
        oscale[1] = pscale[1];
        oscale[2] = pscale[2];
    }
    {
        f64 lower = lbl_80345808;
        f64 cycle = lbl_80345800;
        f64 upper = lbl_803457F8;

        for (i = 0; i < 3; i++) {
        f64 v = pose[i];

        if (v > upper) {
            v -= cycle;
        } else if (v <= lower) {
            v += cycle;
        }
        pose[i] = v;
        }
    }
    data->curkey = curkey;
    data->prevkey = prevkey;
    data->keynum = keynum;
    return 1;
    }
    return 0;
}
/* fn_80010850 @0x80010850 -- scan `n` bits of a byte-swapped bitfield; return
 * the index of the highest set bit and store the total set count via `out`. */
int fn_80010850(u32* bits, s32 n, s32* out)
{
    u32 word;
    s32 last = 0;
    s32 i = 0;
    s32 count = 0;
    s32 bit = 0;
    volatile union {
        u32 w;
        u8 b[4];
    } d0, s0, d1, s1;
    u8 unused[16];

    s0.w = *bits;
    d0.b[0] = s0.b[3];
    d0.b[1] = s0.b[2];
    d0.b[2] = s0.b[1];
    d0.b[3] = s0.b[0];
    word = d0.w;
    while (1) {
        if (i >= n) {
            break;
        }
        if ((word & (1 << bit)) != 0) {
            count++;
            last = i;
        }
        bit++;
        if (bit >= 32) {
            bits++;
            s1.w = *bits;
            d1.b[0] = s1.b[3];
            d1.b[1] = s1.b[2];
            d1.b[2] = s1.b[1];
            d1.b[3] = s1.b[0];
            word = d1.w;
            bit = 0;
        }
        i++;
    }
    *out = count;
    return last;
}

/* fn_80010904 @0x80010904 -- return the index of the next set bit strictly
 * after `start` in the byte-swapped bitfield, or -1 if none within `total`. */
u32 fn_80010904(u32* bits, s32 start, s32 total)
{
    u32 word;
    u32 result;
    u32 bit;
    volatile union {
        u32 w;
        u8 b[4];
    } d0, s0, d1, s1;
    u8 unused[16];

    bit = start + 1;
    result = bit;
    if ((s32)bit >= total) {
        return 0xFFFFFFFF;
    }
    {
        while ((s32)bit >= 32) {
            bits++;
            bit -= 0x20;
            total -= 0x20;
        }
        s0.w = *bits;
        d0.b[0] = s0.b[3];
        d0.b[1] = s0.b[2];
        d0.b[2] = s0.b[1];
        d0.b[3] = s0.b[0];
        word = d0.w;
        while (1) {
            if ((word & (1 << bit)) != 0) {
                return result;
            }
            bit++;
            if ((s32)bit >= total) {
                return 0xFFFFFFFF;
            }
            if ((s32)bit >= 32) {
                bits++;
                bit -= 0x20;
                total -= 0x20;
                s1.w = *bits;
                d1.b[0] = s1.b[3];
                d1.b[1] = s1.b[2];
                d1.b[2] = s1.b[1];
                d1.b[3] = s1.b[0];
                word = d1.w;
            }
            result++;
        }
    }
}

#undef STUB

extern f32 lbl_8023CBA0[256];
extern f64 lbl_80345838;
extern f64 lbl_80345840;

void InitAnimInvDeltaTable(void)
{
    s32 i;

    lbl_8023CBA0[0] = lbl_803457F0;
    for (i = 1; i < 256; i++) {
        lbl_8023CBA0[i] = lbl_80345840 / (f32)i;
    }
}
