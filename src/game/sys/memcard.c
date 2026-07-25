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
extern s32* lbl_80344A14;

/* fixed size of a Gauntlet save in bytes */
s32 saveFileSize(void)
{
    return 128272;
}

/* slot -2 = game options, -1 = directory info, else numbered save */
void getSaveFileName(char* dst, s32 fileNo)
{
    if (fileNo == -2) {
        strcpy(dst, lbl_801131C0 + 80);
    } else if (fileNo == -1) {
        strcpy(dst, lbl_801131C0 + 104);
    } else {
        sprintf(dst, lbl_801131C0 + 124, fileNo + 1);
    }
}

/* map card state to a save result code */
s32 fn_800689CC(s32 result)
{
    if (lbl_80344A18 == -1) {
        return result;
    }
    if (*lbl_80344A14 == 1) {
        return (lbl_80344A18 == 3) ? 1 : -1;
    }
    if (result != 0) {
        return result;
    }
    if (*lbl_80344A14 == 1) {
        return -2;
    }
    return -1;
}
