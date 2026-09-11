/* ML_TEXT: formatted debug text, independently separated from ML_TIMER.
 * The eight GC bodies span 0x800BFC80..0x800C031C. Xbox/PS2 ML_TEXT corroborate
 * the module, its separate scalar state and queued FontMessage handles.
 * Xbox's buffer is char[70]; its -72 stack location is alignment, not size.
 * GC's rounded frames and every instruction/binding also agree with 70.
 */
#include "types.h"
#include "game/ml_text.h"
#include "game/mb_font.h"
#include "__va_arg.h"

enum { DEBUG_TEXT_DEFAULT_COLOR = 0x00FF0000 };

/* Existing GC names retained. PDB/PS2 identify the separate ML_TEXT
 * scalars; no aggregate or alignment-filling dummy field is implied. */
s32 gDbgTextOn = 1;       /* DoPrint */
s32 dbgTextActive;       /* screen_dirty */
s32 dbgTextFlagB;        /* XYPrintPriority */
s32 dbgTextFlagA;        /* XYPrintBlackBG */
s32 gFontsInited;        /* XYPrintfUseMBFonts */
s32 dbgTextEnable;       /* DoDebug */
s32 dbgTextLine;         /* text_y */
s32 dbgTextColor;        /* text_color */

int vsprintf(char* str, const char* fmt, va_list ap);
u32 MBSetFontColor(u32 color);
MBTextMsg* MBDrawSysText(int x, int y, const char* text);

/* Reset the overlay state. */
void dbgTextInit(void)
{
    dbgTextColor = DEBUG_TEXT_DEFAULT_COLOR;
    dbgTextActive = 0;
    dbgTextLine = 0;
}

/* Formatted debug text at (cell x, cell line); line==-1 auto-advances. */
void dbgTextPrintfCell(s32 color, s32 x, s32 line, const char* fmt, ...)
{
    char buf[70];
    MBTextMsg* h;
    u32 old;
    va_list ap;

    if (!gDbgTextOn) {
        return;
    }
    if (dbgTextEnable) {
        if (line == -1) {
            line = dbgTextLine;
            dbgTextLine = line + 1;
        } else {
            dbgTextLine = line + 1;
        }
        va_start(ap, fmt);
        vsprintf(buf, fmt, ap);
        va_end(ap);
        old = MBSetFontColor(color);
        h = MBDrawSysText(x << 3, line << 3, buf);
        MBSetFontColor(old);
        if (h && dbgTextFlagA) {
            h->flags |= 0x40000;
        }
        if (h && dbgTextFlagB) {
            h->flags |= 0x8;
        }
        dbgTextActive = 1;
    } else if (dbgTextActive) {
        dbgTextActive = 0;
    }
}

/* Formatted debug text in the current colour at cell coordinates. */
void dbgTextPrintfCol(s32 x, s32 line, const char* fmt, ...)
{
    char buf[70];
    MBTextMsg* h;
    u32 old;
    va_list ap;

    if (!gDbgTextOn) {
        return;
    }
    if (dbgTextEnable) {
        if (line == -1) {
            line = dbgTextLine;
            dbgTextLine = line + 1;
        } else {
            dbgTextLine = line + 1;
        }
        va_start(ap, fmt);
        vsprintf(buf, fmt, ap);
        va_end(ap);
        old = MBSetFontColor(dbgTextColor);
        h = MBDrawSysText(x << 3, line << 3, buf);
        MBSetFontColor(old);
        if (h && dbgTextFlagA) {
            h->flags |= 0x40000;
        }
        if (h && dbgTextFlagB) {
            h->flags |= 0x8;
        }
        dbgTextActive = 1;
    } else if (dbgTextActive) {
        dbgTextActive = 0;
    }
}

/* Formatted debug text at pixel coordinates. */
void dbgTextPrintfPx(s32 color, s32 x, s32 line, const char* fmt, ...)
{
    char buf[70];
    MBTextMsg* h;
    u32 old;
    va_list ap;

    if (!gDbgTextOn) {
        return;
    }
    if (line == -1) {
        line = dbgTextLine;
        dbgTextLine = line + 1;
    } else {
        dbgTextLine = line + 1;
    }
    va_start(ap, fmt);
    vsprintf(buf, fmt, ap);
    va_end(ap);
    old = MBSetFontColor(color);
    h = MBDrawSysText(x, line, buf);
    MBSetFontColor(old);
    if (h && dbgTextFlagA) {
        h->flags |= 0x40000;
    }
    if (h && dbgTextFlagB) {
        h->flags |= 0x8;
    }
}

void fn_800C008C(s32 color, s32 x, s32 line, const char* fmt, ...)
{
    char buf[70];
    MBTextMsg* h;
    u32 old;
    va_list ap;

    if (!gDbgTextOn) {
        return;
    }
    if (line == -1) {
        line = dbgTextLine;
        dbgTextLine = line + 1;
    } else {
        dbgTextLine = line + 1;
    }
    va_start(ap, fmt);
    vsprintf(buf, fmt, ap);
    va_end(ap);
    old = MBSetFontColor(color);
    h = MBDrawSysText(x << 3, line << 3, buf);
    MBSetFontColor(old);
    if (h && dbgTextFlagA) {
        h->flags |= 0x40000;
    }
    if (h && dbgTextFlagB) {
        h->flags |= 0x8;
    }
}

void fn_800C01C0(s32 x, s32 line, const char* fmt, ...)
{
    char buf[70];
    MBTextMsg* h;
    u32 old;
    va_list ap;

    if (!gDbgTextOn) {
        return;
    }
    if (line == -1) {
        line = dbgTextLine;
        dbgTextLine = line + 1;
    } else {
        dbgTextLine = line + 1;
    }
    va_start(ap, fmt);
    vsprintf(buf, fmt, ap);
    va_end(ap);
    old = MBSetFontColor(dbgTextColor);
    h = MBDrawSysText(x << 3, line << 3, buf);
    MBSetFontColor(old);
    if (h && dbgTextFlagA) {
        h->flags |= 0x40000;
    }
    if (h && dbgTextFlagB) {
        h->flags |= 0x8;
    }
}

/* Set the overlay colour (-1 resets to red); returns the previous colour. */
s32 fn_800C02F4(s32 color)
{
    s32 old = dbgTextColor;
    if (color == -1) {
        color = DEBUG_TEXT_DEFAULT_COLOR;
    }
    dbgTextColor = color;
    return old;
}

/* Clear the drawn flag. */
void fn_800C0310(void)
{
    dbgTextActive = 0;
}
