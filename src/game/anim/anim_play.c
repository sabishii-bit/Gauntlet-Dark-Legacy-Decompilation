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
    f64 lo = lbl_80345808;
    f64 wrap = lbl_80345800;
    f64 hi = lbl_803457F8;
    s32 i;
    for (i = 0; i < 3; i++) {
        if (a[i] != b[i]) {
            f64 d = b[i] - a[i];
            if (d > hi) {
                d = d - wrap;
            }
            if (d <= lo) {
                d = d + wrap;
            }
            out[i] = (f32)(frac * d + (f64)a[i]);
        } else {
            out[i] = a[i];
        }
    }
}
STUB(0x8000F7F4, GetAnimAngXYZVal)
STUB(0x80010850, fn_80010850)
STUB(0x80010904, fn_80010904)

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
