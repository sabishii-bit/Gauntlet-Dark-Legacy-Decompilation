/*
 * ml_error.c - Midway error/log subsystem (ML_ERROR.OBJ).
 *
 * The game-level error logging + fatal-error path: bulletproof_printf (formats
 * via vsprintf, echoes to stdout AND appends to a logfile via sce* file API),
 * ErrorPrintf (accumulates '--> ERROR:' lines), FatalErrorf (formats + prints the
 * '========== FATAL ERROR ===========' banner then halts via pbFatalErrorf), and
 * the on-screen error dump (MBSetFontColor/MBDrawText). Strings: 'MATH ERROR',
 * '>>> BREAKPOINT! <<<', ' VERSION %s'.
 *
 * Range 0x800BC2EC..0x800BC8D8 (8 fns). Owns .sbss globals 0x80344EF0..0x80344F04.
 * FatalError/bulletproof_printf/ErrorPrintf were named by prior sessions; sits
 * ABOVE the low-level pb error TU (PB_ERROR.OBJ @0x800C1174 which it calls into).
 * Names from Xbox shell3D PDB (ML_ERROR.OBJ). cflags_demo, C++ exceptions on.
 *
 * Status: NonMatching. All eight bodies are reconstructed; byte matching is
 * tracked function-by-function while section ownership is recovered.
 */

#include "types.h"
#include "__va_arg.h"

typedef struct MLErrorScreen {
    u8 pad00[0x24];
    s32 height;
} MLErrorScreen;

typedef struct MLErrorWinGlobals {
    u8 pad00[0x10];
    MLErrorScreen* screen;
} MLErrorWinGlobals;

extern char lbl_80126A98[];
extern char lbl_80127C00[];
extern const char lbl_801160E8[];
extern const char lbl_80116150[];
extern const char lbl_80116160[];
extern char lbl_802C38C8[];
extern char lbl_802C3B08[];
extern const char lbl_80348D58[2];
extern const char lbl_80348D5C[4];

extern s32 lbl_80343ED0;
extern s32 gErrorCode;
extern u8 lbl_80344EF0;
extern u8 lbl_80344EF1;
extern u8 lbl_80344EF2;
extern s32 lbl_80344EF4;
extern s32 lbl_80344EF8;
extern s32 lbl_80344EFC;
extern s32 gDiag_F00;
extern s32 lbl_80344F04;
extern s32 dbgTextFlagA;
extern s32 dbgTextFlagB;
extern MLErrorWinGlobals* gWinGlobals;

int vsprintf(char* dst, const char* format, va_list args);
int sprintf(char* dst, const char* format, ...);
int printf(const char* format, ...);
u32 strlen(const char* text);
char* strcat(char* dst, const char* src);
int sceOpen(const char* path, int flags, ...);
int sceLseek(int fd, int offset, int whence);
int sceWrite(int fd, const void* data, int length);
int sceClose(int fd);
void fn_800AF1B8(s32 value);
void fn_800AF1C0(s32 value);
void ClearMemLocks(void);
void fn_800C1174();
void fn_800C1498(void);
u32 MBSetFontColor(u32 color);
void MBDrawText(s32 x, s32 y, char* text);

void fn_800BC52C(void);
void fn_800BC7FC(s32 print);

/* 0x800BC2EC */
void bulletproof_printf(const char* format, ...)
{
    char* base;
    va_list args;

    base = lbl_802C38C8;
    if (lbl_80344EF8 != 0) {
        fn_800AF1B8(lbl_80344EF8);
    }

    va_start(args, format);
    vsprintf(base + 0x280, format, args);
    base[0xA7F] = 0;
    printf(base + 0x280);

    if (lbl_80127C00[0] != 0) {
        lbl_80343ED0 = sceOpen(lbl_80127C00, 2);
    }
    if (lbl_80343ED0 >= 0) {
        sceLseek(lbl_80343ED0, 0, 2);
        sceWrite(lbl_80343ED0, base + 0x280, strlen(base + 0x280));
        sceClose(lbl_80343ED0);
    }

    if (lbl_80344EF8 != 0) {
        fn_800AF1C0(lbl_80344EF8);
    }
}

/* 0x800BC418 */
void fn_800BC418(s32 startLine, s32 count)
{
    MLErrorWinGlobals* globals;
    s32 i;

    globals = gWinGlobals;
    if (lbl_80344EF4 == 0) {
        lbl_80344EF4 = 1;
        for (i = 0; i < 64; i++) {
            lbl_802C3B08[i] = ' ';
        }
        lbl_802C3B08[63] = 0;
        gDiag_F00 = 0;
        fn_800BC52C();
        lbl_80344EFC = 8;
        ClearMemLocks();
    }

    if (startLine >= 0) {
        lbl_80344F04 = lbl_80344EFC + startLine;
    } else {
        lbl_80344F04 = globals->screen->height / 8 - 2;
    }
    if (count > 0) {
        lbl_80344EFC = count;
    }
}

/* 0x800BC4E4 */
void fn_800BC4E4(void)
{
    s32 i;

    gDiag_F00 = 0;
    for (i = 0; i < 8; i++) {
        lbl_802C38C8[i * 64] = 0;
    }
    lbl_80344EF0 = 0;
    lbl_80344EF1 = 0;
    lbl_80344EF2 = 0;
    lbl_80344EFC = 8;
}

/* 0x800BC52C */
void fn_800BC52C(void)
{
    s32 i;

    for (i = 0; i < 8; i++) {
        lbl_802C38C8[i * 64] = 0;
    }
    lbl_80344EF0 = 0;
    lbl_80344EF1 = 0;
    lbl_80344EF2 = 0;
}

/* 0x800BC568 */
void FatalError(char* text, s32 errorCode)
{
    gErrorCode = errorCode;
    fn_800C1174(text, errorCode);
    fn_800C1498();
}

/* 0x800BC590 */
void FatalErrorf(const char* format, ...)
{
    va_list args;
    char version[32];
    const char* strings;
    char* error;
    u8 index;
    s32 limit;
    s32 next;

    strings = lbl_801160E8;
    index = lbl_80344EF1;
    limit = lbl_80344EFC;
    next = index + 1;
    lbl_80344EF1 = next;
    if ((u8)next >= limit) {
        lbl_80344EF1 = 0;
    }
    if (lbl_80344EF2 < limit) {
        lbl_80344EF2++;
    } else {
        lbl_80344EF0 = lbl_80344EF1;
    }

    error = lbl_802C38C8 + index * 64;
    va_start(args, format);
    vsprintf(error, format, args);
    error[63] = 0;
    fn_800BC7FC(1);
    sprintf(version, strings + 12, lbl_80126A98);
    strcat(error, version);
    fn_800C1174(error);
    bulletproof_printf(strings + 24);
    bulletproof_printf(error);
    bulletproof_printf(strings + 64);
    fn_800C1498();
}

/* 0x800BC6E0 */
void ErrorPrintf(const char* format, ...)
{
    char* error;
    u8 index;
    s32 limit;
    s32 next;
    va_list args;

    index = lbl_80344EF1;
    limit = lbl_80344EFC;
    next = index + 1;
    lbl_80344EF1 = next;
    if ((u8)next >= limit) {
        lbl_80344EF1 = 0;
    }
    if (lbl_80344EF2 < limit) {
        lbl_80344EF2++;
    } else {
        lbl_80344EF0 = lbl_80344EF1;
    }

    error = lbl_802C38C8 + index * 64;
    va_start(args, format);
    vsprintf(error, format, args);
    error[63] = 0;
    bulletproof_printf(lbl_80116150);
    bulletproof_printf(error);
    bulletproof_printf(lbl_80348D58);
    gDiag_F00 = 1;
}

/* 0x800BC7FC */
void fn_800BC7FC(s32 print)
{
    s32 i;
    s32 index;
    s32 row;

    row = lbl_80344F04 - (lbl_80344EF2 - 1);
    dbgTextFlagA = 1;
    dbgTextFlagB = 1;
    MBSetFontColor(0x00FF0000);
    if (print != 0) {
        bulletproof_printf(lbl_80116160);
    }

    index = lbl_80344EF0;
    for (i = 0; i < lbl_80344EF2; i++) {
        if (print != 0) {
            bulletproof_printf(lbl_80348D5C, lbl_802C38C8 + index * 64);
        }
        MBDrawText(2, row << 3, lbl_802C38C8 + index * 64);
        index++;
        if (index >= lbl_80344EFC) {
            index = 0;
        }
        row++;
    }
    dbgTextFlagA = 0;
    dbgTextFlagB = 0;
}
