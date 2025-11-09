#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef signed short  s16;
typedef signed int    s32;
typedef unsigned int  u32;
typedef unsigned char u8;

#define READ_S16(base, off) (*(s16*)((u8*)(base) + (u32)(off)))
#define READ_S32(base, off) (*(s32*)((u8*)(base) + (u32)(off)))

enum {
    STRIDE_ENTRY = 0x335C,
    OFF_UNK_C    = 0x000C,
    REC_SIZE     = 0x00F0,
    OFF_UNK_DEE  = 0x0DEE,
};

extern u8  lbl_80275AE0[]; /* defined in ASM (.bss/.data/etc.) */
extern s32 lbl_80124C70[]; /* defined in ASM (.rodata/.sdata2/etc.) */

s32 func_800A2278(s32 arg0, s32 arg1);

#ifdef __cplusplus
}
#endif
