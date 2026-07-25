/*
 * memcard.c - savegame layer over dolphin/card (Xbox: MEMCARD.OBJ).
 * GCN save files keep the PS2 serial: "BASLUS-20047save%04d" etc.
 */
#include "types.h"

int sprintf(char* dst, const char* fmt, ...);
char* strcpy(char* dst, const char* src);

/* string block: cache-transaction logs @0/40, "BASLUS-20047GameOpts"@80,
 * "BASLUS-20047DirInfo"@104, "BASLUS-20047save%04d"@124 */
extern char lbl_801131C0[];

extern s32 lbl_80344A18;   /* card state */
extern s32 lbl_80344A14;

/* fixed size of a Gauntlet save in bytes */
s32 saveFileSize(void)
{
    return 128272;
}

/* slot -2 = game options, -1 = directory info, else numbered save */
void getSaveFileName(char* dst, s32 fileNo)
{
    char* base = lbl_801131C0;

    if (fileNo == -2) {
        strcpy(dst, base + 80);
    } else if (fileNo == -1) {
        strcpy(dst, base + 104);
    } else {
        sprintf(dst, base + 124, fileNo + 1);
    }
}

/* map card state to a save result code; the result!=0 / -2 arms are
 * statically dead (result starts at 0) but present in the original */
/* PARKED: target pre-loads result=0 into r3 (beqlr exit); ours folds the
 * known-zero through the dead result!=0 arm and sinks the init (3 forms
 * tried: arg, ternary, ptr+cached-state). ~3 insn-class diffs. */
s32 fn_800689CC(void)
{
    s32 result = 0;
    s32* p = &lbl_80344A14;
    s32 state = lbl_80344A18;

    if (state == -1) {
        return result;
    }
    if (*p == 1) {
        result = (state == 3) ? 1 : -1;
        return result;
    }
    if (result != 0) {
        return result;
    }
    if (*p == 1) {
        return -2;
    }
    return -1;
}
