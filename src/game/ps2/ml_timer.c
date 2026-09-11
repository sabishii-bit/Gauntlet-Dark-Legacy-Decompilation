/* ML_TIMER: profiling samples and debug graphs, separate from ML_TEXT.
 * GC text 0x800C031C..0x800C0ADC; timer data and Xbox module contributions
 * corroborate both ends. Remaining reconstruction debt is confined here,
 * not in the eight natively exact formatted-text functions in ml_text.c.
 * Timer data ownership and the renderer residual remain open. Registration
 * matches natively using the recovered, twice-used init_timersFYB helper.
 */
#include "types.h"
#include "game/ml_text.h"
#include "game/timing.h"

/* ------------------------------------------------------------------ */
/* module state (.sdata / .sbss)                                       */
/* ------------------------------------------------------------------ */
extern s32 dbgTextFlagA;    /* OR 0x40000 into the drawn glyph flags     */

/* ------------------------------------------------------------------ */
/* externs (other TUs)                                                 */
/* ------------------------------------------------------------------ */
void* MBNewTempQuad(void);                           /* mb_blit.c */
void mbBlitCalcWidth(void*, s32 x, s32 y, f32 depth); /* mb_blit.c */
void mbBlitProject(void*, s32 a, s32 c);             /* mb_blit.c */
void MBBlitSetColor(void*, u32 bright);              /* mb_blit.c */
s32 fn_800C03E0(s32 line);

/* Unrecovered pooled BSS address base, 12 bytes BEFORE TimersFYB. The two
 * exact reset routines below retain their old partial view for now; this
 * does not identify a separate timer array or an object spanning both. */
extern u32 lbl_802C45C0[];

typedef struct DbgGraphCell {
    u32 unk0;
    u32 unk4;
    u32 unk8;
    u32 acc; /* per-slot accumulator, latched by fn_800C0AA4 */
} DbgGraphCell; /* 16-byte graph slot */
extern struct MBBlit** lbl_80344F70;
extern u32 lbl_80344F74;
extern TimerDesc* lbl_80344F78;
extern TimerSample* lbl_80344F7C;
extern s32 lbl_80344F80;

/* ML_TIMER's real local helper, called twice by TimersAddList on PS2 and
 * recorded with the same pointer/count API in Xbox's PDB. Both calls are
 * inlined in GC; writing the loops directly loses the native zero-value
 * register allocation. No target-specific instruction or compiler flag. */
static inline void init_timersFYB(TimerSample* tmrs, s32 num)
{
    s32 i;
    for (i = 0; i < num; i++) {
        tmrs[i].frame = 0;
        tmrs[i].count = 0;
        tmrs[i].current = 0;
        tmrs[i].last_frame = 0;
    }
}

/* TimersAddList: register samples, descriptions and display handles; clear
 * the samples and the separate fixed platform-timer array. */
void fn_800C031C(TimerSample* base, TimerDesc* arg1, struct MBBlit** arg2, s32 count)
{
    lbl_80344F7C = base;
    lbl_80344F78 = arg1;
    lbl_80344F70 = arg2;
    lbl_80344F74 = count;
    init_timersFYB(base, count);
    init_timersFYB(lbl_802C45CC, 24);
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

extern s32 lbl_8034475C;        /* debug page/mode selector               */
extern char lbl_80116450[];     /* rodata: 8 colors + scale/fmt strings   */
extern TimerDesc lbl_80127DE8[]; /* 24 descriptors; final word is precision */
extern f32  lbl_80348EF0;       /* quad depth constant                    */
extern char lbl_80348EF4;       /* mode-5 row fmt (sdata2 string)         */

/* Large debug-quad / graph renderer: per-mode text rows + bar quads.
 * Dispatch is on global lbl_8034475C. The incoming line parameter is
 * overwritten with 20 in the retail code; return its advanced value. */
s32 fn_800C03E0(s32 line)
{
    u8 unused[96];
    TimerSample* tblA = lbl_802C45CC;
    char* fmts = lbl_80116450;
    TimerDesc* tblB = lbl_80127DE8;
    u32 shift = 10;
    u32 div;
    s32 qline;
    s32 i;
    s32 j;
    s32 k;
    void* quad;
    TimerDesc* row;
    u32 scale;

    div = tblA[4].last_frame >> 10;
    if (div == 0) {
        div = 1000000;
    }
    dbgTextFlagA = 1;
    line = 20;
    qline = 20;

    if (lbl_8034475C == 3) {
        dbgTextPrintfPx(0xFFFFFF, 240, 12, fmts + 32);
        for (i = 0; i < 74; i++) {
            s32 id = lbl_80344F78[i].level;
            u32 dv;
            u32 pct;
            u32 w;
            u32 color;
            s32 textX;
            if (id < 0) {
                goto next3;
            }
            scale = 4882;
            dv = lbl_80344F7C[i].last_frame;
            pct = dv >> 10;
            if (dv != 0 && pct == 0) {
                pct = 1;
            }
            textX = 0;
            dbgTextPrintfPx(lbl_80344F78[i].color, textX * 8, line, fmts + 64,
                            pct * 100 / div, pct);
            dbgTextPrintfPx(lbl_80344F78[i].color, (id + 11) * 8, line,
                            lbl_80344F78[i].name);
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
            color = lbl_80344F78[i].color;
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
            id = tblB[i].level;
            if (id < 0) {
                goto next2;
            }
            dv = tblA[i].last_frame;
            colorp = &tblB[i].color;
            pct = dv >> 10;
            textX = 0;
            scale = 4882;
            dbgTextPrintfPx(*colorp, textX * 8, line, fmts + 64,
                            pct * 100 / div, pct);
            dbgTextPrintfPx(*colorp, (id + 11) * 8, line,
                            tblB[i].name);
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
        {
            s32 tk;
            s32 ti;
            for (tk = 0, ti = 0; tk < 6; tk++, ti += 6) {
                quad = MBNewTempQuad();
                mbBlitCalcWidth(quad, (ti + 30) * 8, 20, lbl_80348EF0);
                mbBlitProject(quad, 2, j);
                MBBlitSetColor(quad, 0xFFFFFF);
            }
        }
    }

    if (lbl_8034475C == 5) {
        for (i = 0; i < 24; i++) {
            u32 dv;
            s32 textX;
            row = &tblB[i];
            dv = tblA[i].last_frame;
            textX = 0;
            dbgTextPrintfPx(row->color, textX * 8, line, &lbl_80348EF4,
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
        dv = tblA[4].last_frame;
        pct = dv >> 10;
        i = 4;
        bid = tblB[i].level;
        dbgTextPrintfPx(0xFFFFFF, j * 8, line, fmts + 64,
                        pct * 100 / div, pct);
        lx = (bid + 9) * 8;
        dbgTextPrintfPx(0xFFFFFF, lx, line, tblB[i].name);
        dv = tblA[5].last_frame;
        pct = dv >> 10;
        i = 5;
        dbgTextPrintfPx(0xFFFFFF, j * 8, line + 8, fmts + 64,
                        pct * 100 / div, pct);
        dbgTextPrintfPx(0xFFFFFF, lx, line + 8, tblB[i].name);
        dv = tblA[8].last_frame;
        pct = dv >> 10;
        i = 8;
        dbgTextPrintfPx(0xFFFFFF, j * 8, line + 16, fmts + 64,
                        pct * 100 / div, pct);
        dbgTextPrintfPx(0xFFFFFF, lx, line + 16, tblB[i].name);
        dv = tblA[0].last_frame;
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
            scale = 4882;
            dv = tblA[i].last_frame;
            pct = dv >> 10;
            w = pct * 48;
            if ((s32)(w / scale) > 0) {
                quad = MBNewTempQuad();
                mbBlitCalcWidth(quad, x * 8 + 1, qline + 1, lbl_80348EF0);
                mbBlitProject(quad, w / scale, 4);
                MBBlitSetColor(quad, 0x10101);
            }
            pct = pct * 48;
            color = row->color;
            if ((s32)(pct / 4882) > 0) {
                quad = MBNewTempQuad();
                mbBlitCalcWidth(quad, x * 8, qline + 2, lbl_80348EF0);
                mbBlitProject(quad, pct / scale, 4);
                MBBlitSetColor(quad, color);
            }
            line += 8;
            qline += 8;
        }
        j = qline - 20;
        {
            s32 tk;
            s32 ti;
            for (tk = 0, ti = 0; tk < 6; tk++, ti += 6) {
                quad = MBNewTempQuad();
                mbBlitCalcWidth(quad, (ti + 30) * 8, 20, lbl_80348EF0);
                mbBlitProject(quad, 2, j);
                MBBlitSetColor(quad, 0xFFFFFF);
            }
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
