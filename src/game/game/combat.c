/*
 * combat.c -- GCN COMBAT.OBJ scaffold.
 *
 * This object sits between CAMERA.OBJ and CONTROLS.OBJ in link order.  The
 * PDB identifies the subsystem as player/enemy missile creation, collision,
 * and damage modification; platform-specific expansion prevents a safe
 * positional rename of the 44 GCN bodies, so unverified names stay address
 * based.
 *
 * .text       0x8002951C..0x8003101C
 * extab       0x80005B00..0x80005C10
 * extabindex  0x800090D0..0x80009268
 */

typedef signed int s32;
typedef unsigned int u32;
typedef signed short s16;
typedef unsigned short u16;
typedef signed char s8;
typedef unsigned char u8;
typedef float f32;
typedef double f64;

/* Lock-on / reticle table (COMBAT.OBJ local, 15 slots x 0x38). */
typedef struct CombatSlot {
    s32 active;   /* +0x00 */
    s32 id;       /* +0x04 */
    f32 x;        /* +0x08 */
    f32 y;        /* +0x0C */
    f32 z;        /* +0x10 */
    u8  _pad14[0x24];
} CombatSlot;

extern CombatSlot lbl_80240218[15];
extern u8 lbl_8023F808[]; /* combat-state region base; reticle table at +2576 */

extern s32 lbl_80344540;   /* active reticle count  */
extern s32 lbl_80344548;   /* reticle mode/state    */
extern s32 lbl_8034454C;
extern s32 lbl_80344588;
extern f32 sMusicFadeBase;
extern f32 lbl_80344584;
extern s32 InfFrame;
extern s32 lbl_80344564;
extern f32 lbl_8034458C;
extern f32 lbl_80344590;
extern f32 lbl_80344558;

extern f32 lbl_8023F864[]; /* reticle world-position vec3 array (stride 3) */

/* cross-TU references */
void CopyMat4(f32* src, f32* dst);
extern f64 __frsqrte(f64 x);
extern f64 atan2(f64 y, f64 x);
void FixAngle(f64 angle);

/* in-TU forward references */
void fn_8002C680(s32 camIdx, s32 flag);

#define STUB(address, name) void name(void) {}

STUB(0x8002951C, fn_8002951C)
STUB(0x800297A8, fn_800297A8)
STUB(0x80029E8C, fn_80029E8C)
STUB(0x8002A024, fn_8002A024)
STUB(0x8002A124, fn_8002A124)
STUB(0x8002A5C0, fn_8002A5C0)
STUB(0x8002A788, fn_8002A788)
void fn_8002A890(f32* out)
{
    f32 sum[3];
    s32 i;
    s32 k;
    s32 off;
    s32 n = lbl_8034454C;
    f32 scale;

    if (n > 0) {
        sum[0] = 0.0f;
        sum[1] = 0.0f;
        sum[2] = 0.0f;
        off = 0;
        for (i = 0; i < n; i++) {
            f32* q = lbl_8023F864 + off;
            for (k = 0; k < 3; k++) {
                sum[k] += q[k];
            }
            off += 3;
        }
        scale = (f32)(1.0 / (f64)n);
        sum[0] = sum[0] * scale;
        sum[1] = sum[1] * scale;
        sum[2] = sum[2] * scale;
        out[0] = sum[0];
        out[1] = sum[1];
        out[2] = sum[2];
    }
}

void fn_8002A96C(s32 mode)
{
    lbl_80344548 = mode;
    lbl_8034454C = 0;
}

STUB(0x8002A97C, fn_8002A97C)
STUB(0x8002ABE0, fn_8002ABE0)
STUB(0x8002B2D4, fn_8002B2D4)
STUB(0x8002B450, fn_8002B450)
STUB(0x8002B828, fn_8002B828)
STUB(0x8002C180, fn_8002C180)

void fn_8002C49C(u32 id)
{
    s32 found = 0;
    s32 i;
    CombatSlot* p;

    if (lbl_80344540 != 0) {
        p = lbl_80240218;
        for (i = 0; i < 15; i++, p++) {
            if (p->active != 0 && id == p->id) {
                p->active = 0;
                found = 1;
                p->id = 0;
                p->x = 0.0f;
                p->y = 0.0f;
                p->z = 0.0f;
                lbl_80344540--;
                break;
            }
        }
        if (found) {
            fn_8002C680(0, 0);
        }
    }
}

void fn_8002C53C(u32 id)
{
    s32 found = 0;
    s32 i;
    CombatSlot* p;
    u8* g = lbl_8023F808;

    if (lbl_80344540 >= 15) {
        return;
    }

    if (lbl_80344540 == 0) {
        p = (CombatSlot*)(g + 2576);
        for (i = 0; i < 15; i++, p++) {
            p->active = 0;
            p->id = 0;
            p->x = 0.0f;
            p->y = 0.0f;
            p->z = 0.0f;
        }
        lbl_80344540 = 0;
    }

    p = (CombatSlot*)(g + 2576);
    for (i = 0; i < 15; i++, p++) {
        if (id == p->id) {
            return;
        }
    }

    p = (CombatSlot*)(g + 2576);
    for (i = 0; i < 15; i++, p++) {
        if (p->active == 0) {
            p->active = 1;
            found = 1;
            p->id = id;
            lbl_80344540++;
            break;
        }
    }

    if (found) {
        lbl_8034454C = 0;
        lbl_80344548 = 3;
        fn_8002C680(0, lbl_80344540 == 1);
    }
}

void fn_8002C640(void)
{
    f32 v = 0.0f;
    s32 i;
    CombatSlot* p = lbl_80240218;

    for (i = 0; i < 15; i++, p++) {
        p->active = 0;
        p->id = 0;
        p->x = v;
        p->y = v;
        p->z = v;
    }
    lbl_80344540 = 0;
}

STUB(0x8002C7CC, fn_8002C7CC)
void fn_8002C8A8(f32* a, f32* b)
{
    f32 dz = a[2] - b[2];
    f32 dx = a[0] - b[0];
    f32 dy = a[1] - b[1];
    f32 len = dz * dz + dx * dx;
    f32 dist;
    f64 ang;
    volatile f32 root;
    union {
        f32 f;
        u32 i;
    } u;

    if (len > 0.0f) {
        f64 guess = __frsqrte(len);
        guess = 0.5 * guess * (3.0 - len * guess * guess);
        guess = 0.5 * guess * (3.0 - len * guess * guess);
        guess = 0.5 * guess * (3.0 - len * guess * guess);
        root = (f32)(len * (0.5 * guess * (3.0 - len * guess * guess)));
        len = root;
    }
    dist = len;
    if (len <= 0.001) {
        dist = 0.001f;
    }
    u.f = dy;
    u.i &= 0x7FFFFFFF;
    ang = atan2(u.f, dist);
    if (dy >= 0.0) {
        ang = -ang;
    }
    FixAngle(ang);
}
STUB(0x8002C9A8, fn_8002C9A8)
STUB(0x8002CF78, fn_8002CF78)
STUB(0x8002E0BC, fn_8002E0BC)
void fn_8002E328(u8* src, u8* dst)
{
    CopyMat4((f32*)(src + 4), (f32*)(dst + 4));
#define CF(o) *(f32*)(dst + (o)) = *(f32*)(src + (o))
#define CI(o) *(u32*)(dst + (o)) = *(u32*)(src + (o))
    CF(0x64); CF(0x68); CF(0x6C); CF(0x74); CF(0x78); CF(0x7C);
    CF(0x84); CF(0x88); CF(0x8C); CF(0x94); CF(0x98); CF(0x9C);
    CF(0xA4); CF(0xA8); CF(0xAC); CF(0xB4); CF(0xB8); CF(0xBC);
    CI(0xD8); CF(0xC4); CI(0xC8); CI(0xCC); CI(0xD0); CI(0xD4);
    CF(0xDC); CF(0xE0); CF(0xE4); CF(0xE8);
    CI(0xEC); CI(0xF0); CI(0xF4); CI(0xF8); CI(0xFC); CI(0x100);
    CI(0x104); CI(0x108); CI(0x10C); CI(0x110); CI(0x114); CI(0x118);
    CF(0x11C); CF(0x120); CF(0x124); CF(0x12C); CF(0x130); CF(0x134);
    CF(0x13C); CF(0x140); CF(0x144); CF(0x14C); CF(0x150); CF(0x154);
    CF(0x15C); CF(0x160); CF(0x164); CF(0x16C); CF(0x170); CF(0x174);
    CF(0x17C); CF(0x180); CF(0x184);
#undef CF
#undef CI
}
STUB(0x8002E548, fn_8002E548)
STUB(0x8002E69C, fn_8002E69C)

void fn_8002EFE8(void)
{
    lbl_80344588 = 0;
    sMusicFadeBase = 0.0f;
    lbl_80344584 = 0.0f;
    InfFrame = 0;
    lbl_80344564 = 0;
    lbl_8034458C = 0.0f;
    lbl_80344590 = 0.0f;
    lbl_80344558 = 0.0f;
}

void fn_8002F014(void)
{
    lbl_80344588 = 0;
    sMusicFadeBase = 0.0f;
    lbl_80344584 = 0.0f;
    InfFrame = 0;
    lbl_80344564 = 0;
    lbl_8034458C = 0.0f;
    lbl_80344590 = 0.0f;
    lbl_80344558 = 0.0f;
}

STUB(0x8002F040, fn_8002F040)
STUB(0x8002F2D4, fn_8002F2D4)
STUB(0x8002F44C, fn_8002F44C)
STUB(0x8002F5D8, fn_8002F5D8)

s32 fn_8002F818(s32 type)
{
    switch (type & 0xF) {
    default:
        return -1;
    case 1:
        return 2;
    case 2:
        return 1;
    case 3:
        return 0;
    case 4:
        return 3;
    }
}

STUB(0x8002F86C, fn_8002F86C)
STUB(0x8002F9C4, fn_8002F9C4)
STUB(0x8002FA70, fn_8002FA70)
STUB(0x8002FC54, fn_8002FC54)
STUB(0x800300E0, fn_800300E0)
STUB(0x80030838, fn_80030838)
STUB(0x80030AE8, fn_80030AE8)
STUB(0x80030BA0, fn_80030BA0)
STUB(0x80030C84, fn_80030C84)

#undef STUB
