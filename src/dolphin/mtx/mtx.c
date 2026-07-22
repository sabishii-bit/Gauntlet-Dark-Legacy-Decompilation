#include "types.h"
#include "dolphin/mtx.h"

#define qr0 0

static f32 Unit01[2] = { 0.0F, 1.0F };

void C_MTXIdentity(Mtx m) {
    m[0][0] = 1;
    m[0][1] = 0;
    m[0][2] = 0;
    m[0][3] = 0;
    m[1][0] = 0;
    m[1][1] = 1;
    m[1][2] = 0;
    m[1][3] = 0;
    m[2][0] = 0;
    m[2][1] = 0;
    m[2][2] = 1;
    m[2][3] = 0;
}

void PSMTXIdentity(register Mtx m) {
    register f32 c_zero = 0.0F;
    register f32 c_one = 1.0F;
    register f32 c_01;
    register f32 c_10;

    asm {
        psq_st c_zero, 8(m), 0, qr0
        ps_merge01 c_01, c_zero, c_one
        psq_st c_zero, 24(m), 0, qr0
        ps_merge10 c_10, c_one, c_zero
        psq_st c_zero, 32(m), 0, qr0
        psq_st c_01, 16(m), 0, qr0
        psq_st c_10, 0(m), 0, qr0
        psq_st c_10, 40(m), 0, qr0
    }
}

asm void PSMTXCopy(register Mtx src, register Mtx dst) {
    psq_l f0, 0(src), 0, qr0
    psq_st f0, 0(dst), 0, qr0
    psq_l f1, 8(src), 0, qr0
    psq_st f1, 8(dst), 0, qr0
    psq_l f2, 16(src), 0, qr0
    psq_st f2, 16(dst), 0, qr0
    psq_l f3, 24(src), 0, qr0
    psq_st f3, 24(dst), 0, qr0
    psq_l f4, 32(src), 0, qr0
    psq_st f4, 32(dst), 0, qr0
    psq_l f5, 40(src), 0, qr0
    psq_st f5, 40(dst), 0, qr0
}

asm void PSMTXConcat(register Mtx mA, register Mtx mB, register Mtx mAB) {
    nofralloc
    stwu r1, -64(r1)
    psq_l f0, 0(mA), 0, qr0
    stfd f14, 8(r1)
    psq_l f6, 0(mB), 0, qr0
    lis r6, Unit01@ha
    psq_l f7, 8(mB), 0, qr0
    stfd f15, 16(r1)
    addi r6, r6, Unit01@l
    stfd f31, 40(r1)
    psq_l f8, 16(mB), 0, qr0
    ps_muls0 f12, f6, f0
    psq_l f2, 16(mA), 0, qr0
    ps_muls0 f13, f7, f0
    psq_l f31, 0(r6), 0, qr0
    ps_muls0 f14, f6, f2
    psq_l f9, 24(mB), 0, qr0
    ps_muls0 f15, f7, f2
    psq_l f1, 8(mA), 0, qr0
    ps_madds1 f12, f8, f0, f12
    psq_l f3, 24(mA), 0, qr0
    ps_madds1 f14, f8, f2, f14
    psq_l f10, 32(mB), 0, qr0
    ps_madds1 f13, f9, f0, f13
    psq_l f11, 40(mB), 0, qr0
    ps_madds1 f15, f9, f2, f15
    psq_l f4, 32(mA), 0, qr0
    psq_l f5, 40(mA), 0, qr0
    ps_madds0 f12, f10, f1, f12
    ps_madds0 f13, f11, f1, f13
    ps_madds0 f14, f10, f3, f14
    ps_madds0 f15, f11, f3, f15
    psq_st f12, 0(mAB), 0, qr0
    ps_muls0 f2, f6, f4
    ps_madds1 f13, f31, f1, f13
    ps_muls0 f0, f7, f4
    psq_st f14, 16(mAB), 0, qr0
    ps_madds1 f15, f31, f3, f15
    psq_st f13, 8(mAB), 0, qr0
    ps_madds1 f2, f8, f4, f2
    ps_madds1 f0, f9, f4, f0
    ps_madds0 f2, f10, f5, f2
    lfd f14, 8(r1)
    psq_st f15, 24(mAB), 0, qr0
    ps_madds0 f0, f11, f5, f0
    psq_st f2, 32(mAB), 0, qr0
    ps_madds1 f0, f31, f5, f0
    lfd f15, 16(r1)
    psq_st f0, 40(mAB), 0, qr0
    lfd f31, 40(r1)
    addi r1, r1, 64
    blr
}

void PSMTXTranspose(register Mtx src, register Mtx xPose) {
    register f32 c_zero = 0.0F;
    register f32 row0a;
    register f32 row1a;
    register f32 row0b;
    register f32 row1b;
    register f32 trns0;
    register f32 trns1;
    register f32 trns2;

    asm {
        psq_l row0a, 0(src), 0, qr0
    }
    xPose[2][3] = c_zero;
    asm {
        psq_l row1a, 16(src), 0, qr0
        ps_merge00 trns0, row0a, row1a
        psq_l row0b, 8(src), 1, qr0
        ps_merge11 trns1, row0a, row1a
        psq_l row1b, 24(src), 1, qr0
        psq_st trns0, 0(xPose), 0, qr0
        psq_l row0a, 32(src), 0, qr0
        ps_merge00 trns2, row0b, row1b
        psq_st trns1, 16(xPose), 0, qr0
        ps_merge00 trns0, row0a, c_zero
        psq_st trns2, 32(xPose), 0, qr0
        ps_merge10 trns1, row0a, c_zero
        psq_st trns0, 8(xPose), 0, qr0
    }
    row0b = src[2][2];
    asm {
        psq_st trns1, 24(xPose), 0, qr0
    }
    xPose[2][2] = row0b;
}
