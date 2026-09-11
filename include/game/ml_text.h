#ifndef GAME_ML_TEXT_H
#define GAME_ML_TEXT_H

#include "types.h"

/* ML_TEXT entry points. Existing GC exports are retained until a coordinated
 * rename; in particular the color setter returns the previous signed color. */
void dbgTextInit(void);
void dbgTextPrintfCell(s32 color, s32 x, s32 line, const char* fmt, ...);
void dbgTextPrintfCol(s32 x, s32 line, const char* fmt, ...);
void dbgTextPrintfPx(s32 color, s32 x, s32 line, const char* fmt, ...);
void fn_800C008C(s32 color, s32 x, s32 line, const char* fmt, ...);
void fn_800C01C0(s32 x, s32 line, const char* fmt, ...);
s32 fn_800C02F4(s32 color);
void fn_800C0310(void);

#endif
