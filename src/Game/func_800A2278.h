typedef signed short  s16;
typedef signed int    s32;
typedef unsigned int  u32;
typedef unsigned char u8;

#define READ_S16(base, off) (*(s16*)((u8*)(base) + (u32)(off)))
#define READ_S32(base, off) (*(s32*)((u8*)(base) + (u32)(off)))

enum {
    STRIDE_ENTRY = 0x335C,  /* one “entry” stride inside lbl_80275AE0 */
    OFF_UNK_C    = 0x000C,  /* s32 at +0xC */
    REC_SIZE     = 0x00F0,  /* inner record size multiplied by unkC */
    OFF_UNK_DEE  = 0x0DEE,  /* base of a s16 table inside the entry */
};

extern u8  lbl_80275AE0[]; /* big array of entries */
extern s32 lbl_80124C70[]; /* fallback table */
