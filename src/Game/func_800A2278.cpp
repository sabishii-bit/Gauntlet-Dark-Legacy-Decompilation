#include "func_800A2278.h"

s32 func_800A2278(s32 arg0, s32 arg1) {
    u8* entry = lbl_80275AE0 + (u32)arg0 * STRIDE_ENTRY; // r5 = base + arg0*0x335C
    s32 idx   = READ_S32(entry, OFF_UNK_C);               // lwz r3,0xc(r5)

    // keep this after the lwz so slwi schedules at 0x14
    u32 off2  = (u32)arg1 << 1;

    u8* rec   = entry + (u32)idx * REC_SIZE;              // mulli r3,idx,0xf0 ; add r3,r5,r3 ; add r3,r3,r0

    // KEY: load to s32 'ret' directly from the s16 read, then compare 'ret'
    s32 ret   = (s32)READ_S16(rec, OFF_UNK_DEE + off2);   // lha r0,0xdee(r3)
    if (ret >= 0)                                         // cmpwi r0,0 ; mr r3,r0 ; bgelr-
        return ret;

    // fallback: materialize pointer, then lwz 0(r3)
    u8*  tbase = (u8*)lbl_80124C70;
    u32  off4  = (u32)arg1 << 2;
    u8*  ptr   = tbase + off4;                            // add r3,r0,r4
    return *(s32*)ptr;                                    // lwz r3,0(r3)
}
