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
s32 mbBlitCalcWidth(void*, s32 x, s32 y, f32 depth); /* mb_blit.c */
void mbBlitProject(void*, s32 a, s32 c);             /* mb_blit.c */
void MBBlitSetColor(void*, u32 bright);              /* mb_blit.c */
void dbgTextPrintfPx(s32 color, s32 x, s32 line, char* fmt, ...);
s32 fn_800C03E0(s32 mode);

extern u32 lbl_802C45CC[];   /* debug-cell array base (.data) */
extern u32 lbl_802C45C0[];   /* debug-graph state block (.data) */

typedef struct DbgGraphCell {
    u32 unk0;
    u32 unk4;
    u32 unk8;
    u32 acc; /* per-slot accumulator, latched by fn_800C0AA4 */
} DbgGraphCell; /* 16-byte graph slot */
extern u32 lbl_80344F70;
extern u32 lbl_80344F74;
extern u32 lbl_80344F78;
extern u32 lbl_80344F7C;
extern s32 lbl_80344F80;

/* Reset the overlay state. */
void dbgTextInit(void)
{
    dbgTextActive = 0;
    dbgTextColor = 0x00FF0000;
    dbgTextLine = 0;
}

/* Formatted debug text at (cell x, cell line); line==-1 auto-advances. */
void dbgTextPrintfCell(s32 color, s32 x, s32 line, char* fmt, ...)
{
    char buf[72];
    u32* h;
    s32 old;
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
}

/* Formatted debug text with an explicit colour override. */
void dbgTextPrintfCol(s32 x, s32 line, char* fmt, ...)
{
    char buf[72];
    u32* h;
    s32 old;
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
}

/* Formatted debug text at pixel coordinates. */
void dbgTextPrintfPx(s32 color, s32 x, s32 line, char* fmt, ...)
{
    char buf[72];
    u32* h;
    s32 old;
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
    old = MBSetFontColor(color);
    h = MBDrawSysText(x, line, buf);
    MBSetFontColor(old);
    if (h && dbgTextFlagA) {
        *h |= 0x40000;
    }
    if (h && dbgTextFlagB) {
        *h |= 0x8;
    }
}

void fn_800C008C(s32 color, s32 x, s32 line, char* fmt, ...)
{
    char buf[72];
    u32* h;
    s32 old;
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
    old = MBSetFontColor(color);
    h = MBDrawSysText(x << 3, line << 3, buf);
    MBSetFontColor(old);
    if (h && dbgTextFlagA) {
        *h |= 0x40000;
    }
    if (h && dbgTextFlagB) {
        *h |= 0x8;
    }
}

void fn_800C01C0(s32 x, s32 line, char* fmt, ...)
{
    char buf[72];
    u32* h;
    s32 old;
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
    old = MBSetFontColor(dbgTextColor);
    h = MBDrawSysText(x << 3, line << 3, buf);
    MBSetFontColor(old);
    if (h && dbgTextFlagA) {
        *h |= 0x40000;
    }
    if (h && dbgTextFlagB) {
        *h |= 0x8;
    }
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
    s32 i;
    u32* cell;

    lbl_80344F7C = (u32)base;
    lbl_80344F78 = arg1;
    lbl_80344F70 = arg2;
    lbl_80344F74 = count;
    for (i = 0; i < count; i++) {
        cell = (u32*)((char*)base + i * 16);
        cell[3] = cell[2] = cell[1] = cell[0] = 0;
    }
    for (i = 0; i < 24; i++) {
        cell = (u32*)((char*)lbl_802C45CC + i * 16);
        cell[3] = cell[2] = cell[1] = cell[0] = 0;
    }
}

void fn_800C0394(void)
{
    u32* b = lbl_802C45C0;
    if (lbl_80344F80 != 0) {
        b[98] = b[95];
        b[95] = 0;
        b[96] = 0;
    }
    fn_800C03E0(4);
}

/* Debug-table record (28-byte stride): name used as the row's fmt label. */
typedef struct DbgRow {
    /* 0x00 */ char name[16];
    /* 0x10 */ s32  id;      /* <0 = unused slot */
    /* 0x14 */ u32  color;
    /* 0x18 */ u32  _pad;
} DbgRow;

extern s32 lbl_8034475C;        /* debug page/mode selector               */
extern char lbl_80116450[];     /* rodata: 8 colors + scale/fmt strings   */
extern DbgRow lbl_80127DE8[];   /* 24-entry debug row table (.data)       */
extern f32  lbl_80348EF0;       /* quad depth constant                    */
extern char lbl_80348EF4;       /* mode-5 row fmt (sdata2 string)         */

/* Large debug-quad / graph renderer: per-mode text rows + bar quads.
 * Dispatch is on the global lbl_8034475C (the mode parameter is unused in
 * the original).  Returns the advanced line cursor. */
s32 fn_800C03E0(s32 mode)
{
    u8 unused[96];
    u32* tblA = lbl_802C45CC;
    char* fmts = lbl_80116450;
    DbgRow* tblB = lbl_80127DE8;
    u32 shift = 10;
    u32 div;
    s32 line;
    s32 qline;
    s32 i;
    s32 j;
    s32 k;
    void* quad;
    DbgRow* row;
    u32 scale;

    (void)mode;
    div = tblA[19] >> 10;
    if (div == 0) {
        div = 1000000;
    }
    dbgTextFlagA = 1;
    line = 20;
    qline = 20;

    if (lbl_8034475C == 3) {
        dbgTextPrintfPx(0xFFFFFF, 240, 12, fmts + 32);
#define DBGROW3 ((DbgRow*)((u8*)lbl_80344F78 + i * 28))
        for (i = 0; i < 74; i++) {
            s32 id = DBGROW3->id;
            u32 dv;
            u32 pct;
            u32 w;
            u32 color;
            s32 textX;
            if (id < 0) {
                goto next3;
            }
            scale = 4882;
            dv = ((u32*)lbl_80344F7C)[i * 4 + 3];
            pct = dv >> 10;
            if (dv != 0 && pct == 0) {
                pct = 1;
            }
            textX = 0;
            dbgTextPrintfPx(DBGROW3->color, textX * 8, line, fmts + 64,
                            pct * 100 / div, pct);
            dbgTextPrintfPx(DBGROW3->color, (id + 11) * 8, line,
                            DBGROW3->name);
            w = pct * 96;
            j = (s32)(w / scale);
            if (j > 0) {
                s32 x;
                quad = MBNewTempQuad();
                x = 30;
                mbBlitCalcWidth(quad, x * 8 + 1, qline + 1, lbl_80348EF0);
                mbBlitProject(quad, w / scale, 4);
                MBBlitSetColor(quad, 0x10101);
            }
            color = DBGROW3->color;
            if (j > 0) {
                s32 x;
                quad = MBNewTempQuad();
                x = 30;
                mbBlitCalcWidth(quad, x * 8, qline + 2, lbl_80348EF0);
                mbBlitProject(quad, w / scale, 4);
                MBBlitSetColor(quad, color);
            }
            line += 8;
            qline += 8;
        next3:
            ;
        }
#undef DBGROW3
        j = qline - 20;
        for (k = 0, i = 0; k < 3; k++, i += 12) {
            quad = MBNewTempQuad();
            mbBlitCalcWidth(quad, (i + 30) * 8, 20, lbl_80348EF0);
            mbBlitProject(quad, 2, j);
            MBBlitSetColor(quad, 0xFFFFFF);
        }
    }

    if (lbl_8034475C == 2) {
        dbgTextPrintfPx(0xFFFFFF, 240, line - 8, fmts + 76);
        for (i = 0; i < 24; i++) {
            u32* colorp;
            s32 id;
            u32 dv;
            u32 pct;
            u32 w;
            u32 color;
            s32 textX;
            row = &tblB[i];
            id = row->id;
            if (id < 0) {
                goto next2;
            }
            dv = tblA[i * 4 + 3];
            colorp = &row->color;
            pct = dv >> 10;
            textX = 0;
            scale = 4882;
            dbgTextPrintfPx(*colorp, textX * 8, line, fmts + 64,
                            pct * 100 / div, pct);
            dbgTextPrintfPx(*colorp, (id + 11) * 8, line,
                            row->name);
            w = pct * 48;
            j = (s32)(w / scale);
            if (j > 0) {
                s32 x;
                quad = MBNewTempQuad();
                x = 30;
                mbBlitCalcWidth(quad, x * 8 + 1, qline + 1, lbl_80348EF0);
                mbBlitProject(quad, w / scale, 4);
                MBBlitSetColor(quad, 0x10101);
            }
            color = *colorp;
            if (j > 0) {
                s32 x;
                quad = MBNewTempQuad();
                x = 30;
                mbBlitCalcWidth(quad, x * 8, qline + 2, lbl_80348EF0);
                mbBlitProject(quad, w / scale, 4);
                MBBlitSetColor(quad, color);
            }
            line += 8;
            qline += 8;
        next2:
            ;
        }
        j = qline - 20;
        for (k = 0, i = 0; k < 6; k++, i += 6) {
            quad = MBNewTempQuad();
            mbBlitCalcWidth(quad, (i + 30) * 8, 20, lbl_80348EF0);
            mbBlitProject(quad, 2, j);
            MBBlitSetColor(quad, 0xFFFFFF);
        }
    }

    if (lbl_8034475C == 5) {
        for (i = 0; i < 24; i++) {
            u32 dv;
            row = &tblB[i];
            dv = tblA[i * 4 + 3];
            dbgTextPrintfPx(row->color, 0, line, &lbl_80348EF4,
                            dv >> shift);
            line += 8;
            qline += 8;
        }
    }

    if (lbl_8034475C == 1) {
        s32 bid;
        s32 lx;
        u32 dv;
        u32 pct;
        s32 i;

        j = 0;
        dv = tblA[19];
        pct = dv >> 10;
        i = 4;
        bid = tblB[i].id;
        dbgTextPrintfPx(0xFFFFFF, j * 8, line, fmts + 64,
                        pct * 100 / div, pct);
        lx = (bid + 9) * 8;
        dbgTextPrintfPx(0xFFFFFF, lx, line, tblB[i].name);
        dv = tblA[23];
        pct = dv >> 10;
        i = 5;
        dbgTextPrintfPx(0xFFFFFF, j * 8, line + 8, fmts + 64,
                        pct * 100 / div, pct);
        dbgTextPrintfPx(0xFFFFFF, lx, line + 8, tblB[i].name);
        dv = tblA[35];
        pct = dv >> 10;
        i = 8;
        dbgTextPrintfPx(0xFFFFFF, j * 8, line + 16, fmts + 64,
                        pct * 100 / div, pct);
        dbgTextPrintfPx(0xFFFFFF, lx, line + 16, tblB[i].name);
        dv = tblA[3];
        pct = dv >> 10;
        i = 0;
        dbgTextPrintfPx(0xFFFFFF, j * 8, line + 24, fmts + 64,
                        pct * 100 / div, pct);
        dbgTextPrintfPx(0xFFFFFF, lx, line + 24, tblB[i].name);
        line += 32;
        qline += 32;
    }

    if (lbl_8034475C == 4) {
        for (i = 0; i < 2; i++) {
            u32 dv;
            u32 pct;
            u32 w;
            u32 color;
            s32 x = 30;
            row = &tblB[i];
            dv = tblA[i * 4 + 3];
            pct = dv >> 10;
            w = pct * 48;
            if ((s32)(w / 4882) > 0) {
                quad = MBNewTempQuad();
                mbBlitCalcWidth(quad, x * 8 + 1, qline + 1, lbl_80348EF0);
                mbBlitProject(quad, w / 4882, 4);
                MBBlitSetColor(quad, 0x10101);
            }
            pct = pct * 48;
            color = row->color;
            if ((s32)(pct / 4882) > 0) {
                quad = MBNewTempQuad();
                mbBlitCalcWidth(quad, x * 8, qline + 2, lbl_80348EF0);
                mbBlitProject(quad, pct / 4882, 4);
                MBBlitSetColor(quad, color);
            }
            line += 8;
            qline += 8;
        }
        j = qline - 20;
        for (k = 0, i = 0; k < 6; k++, i += 6) {
            quad = MBNewTempQuad();
            mbBlitCalcWidth(quad, (i + 30) * 8, 20, lbl_80348EF0);
            mbBlitProject(quad, 2, j);
            MBBlitSetColor(quad, 0xFFFFFF);
        }
    }

    for (i = 74; i > 0; i--) {
    }
    dbgTextFlagA = 0;
    return line;
}

/* Latch a graph slot: move its accumulator to the display field. */
void fn_800C0AA4(s32 idx)
{
    DbgGraphCell* p = (DbgGraphCell*)lbl_802C45C0;

    if (lbl_80344F80 == 0) {
        return;
    }
    p[idx + 1].unk8 = p[idx].acc;
    p[idx].acc = 0;
    p[idx + 1].unk0 = 0;
}
