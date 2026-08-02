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
void GetAnimAngXYZVal();

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

void CalcAnimData()
{
    GetAnimAngXYZVal();
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
STUB(0x8000F7F4, GetAnimAngXYZVal)
/* fn_80010850 @0x80010850 -- scan `n` bits of a byte-swapped bitfield; return
 * the index of the highest set bit and store the total set count via `out`. */
int fn_80010850(u32* bits, s32 n, s32* out)
{
    s32 last = 0;
    s32 i = 0;
    s32 count = 0;
    s32 bit = 0;
    u32 word;
    union {
        u32 w;
        u8 b[4];
    } s, d;

    s.w = *bits;
    d.b[0] = s.b[3];
    d.b[1] = s.b[2];
    d.b[2] = s.b[1];
    d.b[3] = s.b[0];
    word = d.w;
    while (i < n) {
        if ((word & (1 << bit)) != 0) {
            count++;
            last = i;
        }
        bit++;
        if (bit > 31) {
            bits++;
            s.w = *bits;
            d.b[0] = s.b[3];
            d.b[1] = s.b[2];
            d.b[2] = s.b[1];
            d.b[3] = s.b[0];
            word = d.w;
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
    u32 idx = start + 1;

    if ((s32)idx < total) {
        u32 bit = idx;
        u32 word;
        union {
            u32 w;
            u8 b[4];
        } s, d;

        if ((s32)idx >= 32) {
            u32 skip = idx >> 5;
            do {
                bits++;
                bit -= 0x20;
                total -= 0x20;
                skip--;
            } while (skip != 0);
        }
        s.w = *bits;
        d.b[0] = s.b[3];
        d.b[1] = s.b[2];
        d.b[2] = s.b[1];
        d.b[3] = s.b[0];
        word = d.w;
        while ((word & (1 << bit)) == 0) {
            u32 nextbit = bit + 1;
            if (total <= (s32)nextbit) {
                return 0xFFFFFFFF;
            }
            if ((s32)nextbit >= 32) {
                bits++;
                s.w = *bits;
                d.b[0] = s.b[3];
                d.b[1] = s.b[2];
                d.b[2] = s.b[1];
                d.b[3] = s.b[0];
                word = d.w;
                nextbit = bit - 0x1F;
                total -= 0x20;
            }
            idx++;
            bit = nextbit;
        }
    } else {
        idx = 0xFFFFFFFF;
    }
    return idx;
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
