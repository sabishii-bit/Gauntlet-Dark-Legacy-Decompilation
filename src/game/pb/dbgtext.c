/* dbgtext.c -- Midway "pb" library on-screen debug-text overlay TU.
 *
 * .text 0x800BFC80-0x800C0ADC (12 functions). No PB_XXX.C assert strings
 * of its own; identified by the sdata2 pool seam (0x80348EF0/EF4) that
 * separates it from mb_window.obj at 0x800C0ADC, and by its callees
 * (vsprintf + MBSetFontColor + MBDrawSysText). Not a distinct shell3D.pdb
 * module (dbgText* names are behavioural). Compiled -Cpp_exceptions on
 * (cflags_demo): every LR-saving function carries an extab/extabindex entry.
 *
 * NonMatching: the small accessors are reconstructed faithfully; the printf
 * family and the large debug-quad renderer (fn_800C03E0) are best-effort
 * structural skeletons.
 */

#include "types.h"
#include "__va_arg.h"

/* ------------------------------------------------------------------ */
/* module state (.sdata / .sbss)                                       */
/* ------------------------------------------------------------------ */
extern s32 dbgTextActive;   /* 1 while a debug line was drawn this frame */
extern s32 dbgTextColor;    /* current text colour (default 0x00FF0000)  */
extern s32 dbgTextLine;     /* auto-advancing line counter               */
extern s32 gDbgTextOn;      /* master enable                             */
extern s32 dbgTextEnable;   /* per-frame enable                          */
extern s32 dbgTextFlagA;    /* OR 0x40000 into the drawn glyph flags     */
extern s32 dbgTextFlagB;    /* OR 0x8 into the drawn glyph flags         */

/* ------------------------------------------------------------------ */
/* externs (other TUs)                                                 */
/* ------------------------------------------------------------------ */
int vsprintf(char* str, const char* fmt, va_list ap);
s32 MBSetFontColor(s32 color);                       /* mb_font.c */
u32* MBDrawSysText(s32 x, s32 y, char* text);        /* mb_font.c */
void* MBNewTempQuad(void);                           /* mb_blit.c */
s32 mbBlitCalcWidth(void*);                          /* mb_blit.c */
void mbBlitProject(void*);                           /* mb_blit.c */
void fn_800B2940(void*);
void* dbgTextPrintfPx(s32 color, s32 x, s32 y, char* fmt, ...);
void fn_800C03E0(void);

extern void* lbl_802C45CC;   /* debug-cell array base (.data) */
extern void* lbl_802C45C0;
extern u32 lbl_80344F70;
extern u32 lbl_80344F74;
extern u32 lbl_80344F78;
extern u32 lbl_80344F7C;
extern u32 lbl_80344F80;

/* Reset the overlay state. */
void dbgTextInit(void)
{
    dbgTextActive = 0;
    dbgTextColor = 0x00FF0000;
    dbgTextLine = 0;
}

/* Formatted debug text at (cell x, cell line); line==-1 auto-advances. */
void* dbgTextPrintfCell(s32 color, s32 x, s32 line, char* fmt, ...)
{
    char buf[100];
    u32* h;
    s32 old;
    va_list ap;

    h = 0;
    if (!gDbgTextOn) {
        return h;
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
        old = MBSetFontColor(color);
        h = MBDrawSysText(x << 3, line << 3, buf);
        MBSetFontColor(old);
        if (h && dbgTextFlagA) {
            *h |= 0x40000;
        }
        if (h && dbgTextFlagB) {
            *h |= 0x8;
        }
        dbgTextActive = 1;
    } else if (dbgTextActive) {
        dbgTextActive = 0;
    }
    return h;
}

/* Formatted debug text with an explicit colour override. */
void* dbgTextPrintfCol(s32 color, s32 x, s32 line, char* fmt, ...)
{
    char buf[100];
    u32* h;
    s32 old;
    va_list ap;

    h = 0;
    if (!gDbgTextOn) {
        return h;
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
        old = MBSetFontColor(dbgTextColor);
        h = MBDrawSysText(x << 3, line << 3, buf);
        MBSetFontColor(old);
        if (h && dbgTextFlagA) {
            *h |= 0x40000;
        }
        if (h && dbgTextFlagB) {
            *h |= 0x8;
        }
        dbgTextActive = 1;
    } else if (dbgTextActive) {
        dbgTextActive = 0;
    }
    return h;
}

/* Formatted debug text at pixel coordinates. */
void* dbgTextPrintfPx(s32 color, s32 x, s32 y, char* fmt, ...)
{
    char buf[100];
    u32* h;
    s32 old;
    va_list ap;

    h = 0;
    if (!gDbgTextOn) {
        return h;
    }
    va_start(ap, fmt);
    vsprintf(buf, fmt, ap);
    old = MBSetFontColor(color);
    h = MBDrawSysText(x, y, buf);
    MBSetFontColor(old);
    if (h && dbgTextFlagA) {
        *h |= 0x40000;
    }
    if (h && dbgTextFlagB) {
        *h |= 0x8;
    }
    return h;
}

void* fn_800C008C(s32 color, s32 x, s32 y, char* fmt, ...)
{
    char buf[100];
    u32* h;
    s32 old;
    va_list ap;

    h = 0;
    if (!gDbgTextOn) {
        return h;
    }
    va_start(ap, fmt);
    vsprintf(buf, fmt, ap);
    old = MBSetFontColor(color);
    h = MBDrawSysText(x, y, buf);
    MBSetFontColor(old);
    if (h && dbgTextFlagA) {
        *h |= 0x40000;
    }
    if (h && dbgTextFlagB) {
        *h |= 0x8;
    }
    return h;
}

void* fn_800C01C0(s32 x, s32 y, char* fmt, ...)
{
    char buf[100];
    u32* h;
    s32 old;
    va_list ap;

    h = 0;
    if (!gDbgTextOn) {
        return h;
    }
    va_start(ap, fmt);
    vsprintf(buf, fmt, ap);
    old = MBSetFontColor(dbgTextColor);
    h = MBDrawSysText(x, y, buf);
    MBSetFontColor(old);
    if (h && dbgTextFlagA) {
        *h |= 0x40000;
    }
    if (h && dbgTextFlagB) {
        *h |= 0x8;
    }
    return h;
}

/* Set the overlay colour (-1 resets to red); returns the previous colour. */
s32 fn_800C02F4(s32 color)
{
    s32 old = dbgTextColor;
    if (color == -1) {
        color = 0x00FF0000;
    }
    dbgTextColor = color;
    return old;
}

/* Clear the drawn flag. */
void fn_800C0310(void)
{
    dbgTextActive = 0;
}

/* Bind a debug-cell array (base, stride, count, ...) and zero it, then zero
 * the fixed debug-cell block at lbl_802C45CC. */
void fn_800C031C(u32* base, u32 arg1, u32 arg2, s32 count)
{
    u32* cell;
    s32 i;

    lbl_80344F7C = (u32)base;
    lbl_80344F78 = arg1;
    lbl_80344F70 = arg2;
    lbl_80344F74 = count;
    for (i = 0; i < count; i++) {
        cell = (u32*)((char*)base + i * 16);
        cell[0] = 0;
        cell[1] = 0;
        cell[2] = 0;
        cell[3] = 0;
    }
    for (i = 0; i < 24; i++) {
        cell = (u32*)((char*)&lbl_802C45CC + i * 16);
        cell[0] = 0;
        cell[1] = 0;
        cell[2] = 0;
        cell[3] = 0;
    }
}

void fn_800C0394(void)
{
    fn_800C03E0();
}

/* Large debug-quad / graph renderer (parked structural stub). */
void fn_800C03E0(void)
{
    /* draws pooled debug quads via MBNewTempQuad/mbBlitProject and labels
     * them through dbgTextPrintfPx; reconstruction deferred. */
    dbgTextPrintfPx(0, 0, 0, (char*)&lbl_802C45CC);
    MBNewTempQuad();
    mbBlitCalcWidth(0);
    mbBlitProject(0);
    fn_800B2940(0);
}

void fn_800C0AA4(void)
{
    lbl_80344F80 = (u32)&lbl_802C45C0;
}
