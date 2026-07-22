#include "types.h"
#include "dolphin/mtx.h"

#define qr0 0

asm void PSMTXMultVec(register Mtx m, register Vec* src, register Vec* dst) {
    nofralloc
    psq_l f0, 0(src), 0, qr0
    psq_l f2, 0(m), 0, qr0
    psq_l f1, 8(src), 1, qr0
    ps_mul f4, f2, f0
    psq_l f3, 8(m), 0, qr0
    ps_madd f5, f3, f1, f4
    psq_l f8, 16(m), 0, qr0
    ps_sum0 f6, f5, f6, f5
    psq_l f9, 24(m), 0, qr0
    ps_mul f10, f8, f0
    psq_st f6, 0(dst), 1, qr0
    ps_madd f11, f9, f1, f10
    psq_l f2, 32(m), 0, qr0
    ps_sum0 f12, f11, f12, f11
    psq_l f3, 40(m), 0, qr0
    ps_mul f4, f2, f0
    psq_st f12, 4(dst), 1, qr0
    ps_madd f5, f3, f1, f4
    ps_sum0 f6, f5, f6, f5
    psq_st f6, 8(dst), 1, qr0
    blr
}
