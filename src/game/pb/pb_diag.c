#include "types.h"

/* GDL diagnostic / debug HUD overlay (GCN PB_DIAG.OBJ region,
 * 0x800A573C-0x800A87C8). The Xbox shell3D PDB stubs this module out
 * (pbDiagMenu/pbDiagMenuDraw are 1-byte no-ops there), so most GCN names
 * are provisional/descriptive. This module draws the audio/frame/object/
 * texture diagnostic screens and a debug menu, driven by pad input via the
 * `buttons` control-state block. pbDiagCtrlInt / pbDiagCtrlFloat adjust a
 * menu value from D-pad/stick input with key-repeat + wrap.
 *
 * Status: NonMatching. 6/15 functions reconstructed:
 *   pbDiagCtrlInt   - MATCHING (byte-exact)
 *   pbDiagCtrlFloat - MATCHING (byte-exact)
 *   pbResetDiag     - equivalent, insn-count exact; volatile-reg allocation
 *                     (buttons r5-vs-r6) differs; parked (regalloc-only).
 *   pbInitDiag      - equivalent, insn-count exact; gDiagData hoist +
 *                     sdata2 pool ordering differ; parked (regalloc/pool).
 *   pbDiagDrawMenuA - equivalent, insn-count exact; saved-reg numbering
 *                     (line/off/colorbase permute) differs; parked.
 *   pbDiagDrawMenuB - equivalent, insn-count exact; same regalloc residual.
 * The 9 larger draw functions (audio/soundrow/info/texture/texlabel/object/
 * colorbars/strrow/menu) are not yet reconstructed. */

typedef struct WinGlobals {
    u8 _pad0[0x30];
    u32* f30;
} WinGlobals;

typedef struct DiagList {
    /* 0x00 */ char* strs;      /* base of fixed-stride string rows */
    /* 0x04 */ u8 _pad4[0x10];
    /* 0x14 */ s32 count;       /* number of entries */
} DiagList;

typedef struct DiagMenu {
    /* 0x00 */ s16 count;
    /* 0x02 */ u8 _pad2[2];
    /* 0x04 */ char* strs;      /* base of 36-byte string rows */
} DiagMenu;

/* pad / control state block (PB_DIAG `buttons`, 0x8028C388) */
extern u32 buttons[];

/* window globals pointer (owned by pb_window, gWinGlobals) */
extern WinGlobals* gWinGlobals;

/* diag config block (this TU's .data, gDiagData, 0x170 bytes) */
extern f32 gDiagData[];

/* menu-B highlight table + current index */
extern s32 gDiagMenuList[];     /* gDiagMenuList */
extern s32 gDiagMenuIdx;        /* gDiagMenuIdx */

/* diag state (sdata/sbss, owned by this TU) */
extern s32 gDiagTurbo;          /* gDiagTurbo */
extern void* gDiagWhiteObj;     /* gDiagWhiteObj */
extern s32 gDiag_D4;            /* gDiag_D4 */
extern s32 gDiag_D8;            /* gDiag_D8 */
extern s32 gDiag_DC;            /* gDiag_DC */
extern s32 gDiagListSel;        /* gDiagListSel (menu-A highlight index) */
extern s32 gDiag_E0;            /* gDiag_E0 */
extern s32 gDiag_E4;            /* gDiag_E4 */
extern s32 gDiagRepeatDelay;    /* gDiagRepeatDelay */
extern s32 gDiagRepeatRate;     /* gDiagRepeatRate */
extern s32 gDiag_F0;            /* gDiag_F0 */
extern s32 gDiag_F4;            /* gDiag_F4 */
extern s32 gDiag_FC;            /* gDiag_FC */
extern s32 gDiag_D00;           /* gDiag_D00 */
extern s32 gDiag_D04;           /* gDiag_D04 */
extern s32 gDiag_D08;           /* gDiag_D08 */

/* --- text / draw primitives + subsystem init (other TUs) --- */
extern void fn_800C008C(u32 rgba, int x, int y, const char* fmt, ...);
extern void AudioStopSelect(void);
extern void AudioSelectReset(void);
extern void fn_800C0310(void);
extern void MBTreeInit(void);
extern void DebugCamInit(void); /* newcam.c: init the pb-diag debug camera */
extern void* MBOX_FindTexture(const char* name, int arg);
extern int strlen(const char* s);

void pbResetDiag(void);

void pbDiagDrawMenuA(DiagList* list) {
    int line;
    int i;
    int off;
    int start;
    int end;
    int count = list->count;

    line = 3;
    if (count < 38) {
        end = count;
        start = 0;
    } else {
        end = gDiagListSel + 19;
        if (end < 38) {
            end = 38;
        }
        if (end >= count) {
            end = count;
        }
        start = end - 38;
    }
    off = start * 48;
    for (i = start; i < end; i++) {
        if (i == gDiagListSel) {
            fn_800C008C(0x00FFFF00, 18, line, list->strs + off);
        } else {
            fn_800C008C(0x00FFFFFF, 18, line, list->strs + off);
        }
        line++;
        off += 48;
    }
}

void pbDiagDrawMenuB(DiagMenu* menu) {
    int line;
    int i;
    int off;
    int start;
    int end;
    int count = menu->count;

    line = 3;
    if (count < 38) {
        end = count;
        start = 0;
    } else {
        end = gDiagMenuList[gDiagMenuIdx] + 19;
        if (end < 38) {
            end = 38;
        }
        if (end >= count) {
            end = count;
        }
        start = end - 38;
    }
    if (menu == 0) {
        return;
    }
    if (menu->strs == 0) {
        return;
    }
    off = start * 36;
    for (i = start; i < end; i++) {
        if (i == gDiagMenuList[gDiagMenuIdx]) {
            fn_800C008C(0x00FFFF00, 1, line, menu->strs + off);
            strlen(menu->strs + off);
        } else {
            fn_800C008C(0x00FFFFFF, 1, line, menu->strs + off);
        }
        line++;
        off += 36;
    }
}

/* texture bank view: slot count at +0x48 */
typedef struct DiagTexBank {
    u8 _pad00[0x48];
    u32 nslots;             /* 0x48 */
} DiagTexBank;

extern s32 gDiag_F4;            /* declared above */
extern s32 gDiagBtns_F8[];      /* per-screen cursor table */
extern char lbl_803486F8[8];    /* "%3d %s"-style row format (sdata2) */
extern void* MBOX_GetTexDef(u32 id);

/* one label row per texture slot of bank `bank`, windowed like MenuA */
void pbDiagDrawTexLabel(DiagTexBank* tb, int bank)
{
    int line;
    u32 color;
    u32 hi;
    int i;
    int start;
    int end;
    register u32 count;
    u32 id;
    void* def;
    u8 _spare[8];


    line = 3;
    if ((count = tb->nslots) < 38) {
        end = count;
        start = 0;
    } else {
        end = gDiagBtns_F8[gDiag_F4] + 19;
        if (end < 38) {
            end = 38;
        }
        if ((u32)end >= count) {
            end = count;
        }
        start = end - 38;
    }
    hi = bank << 16;
    for (i = start; i < (s32)end; i++) {
        id = (u16)i | hi;
        if (i == gDiagBtns_F8[gDiag_F4]) {
            color = 0x00FFFF00;
        } else {
            color = 0x00FFFFFF;
        }
        def = MBOX_GetTexDef(id);
        fn_800C008C(color, 1, line, lbl_803486F8, i, def);
        line++;
    }
}

/* string-row list: count at +0x4C, row base at +0x5C, 24-byte rows */
typedef struct DiagStrRows {
    u8 _pad00[76];
    u32 count;              /* 0x4C */
    u8 _pad50[12];
    char* strs;             /* 0x5C */
} DiagStrRows;

typedef struct DiagRow {       /* 24-byte string row */
    char name[22];
    s16  val;
} DiagRow;

extern s32 gDiagBtns_B8[];      /* per-screen cursor table (B8 block) */
extern s32 gDiag_D0C;
extern char lbl_80348758[8];    /* value-suffix row format (sdata2) */

/* windowed 24-byte string rows; selected row also prints its s16 value */
void pbDiagDrawStrRow(DiagStrRows* p)
{
    int i;
    int line;
    int end;
    int start;
    u32 count;
    int len;

    line = 3;
    if ((count = p->count) < 38) {
        end = count;
        start = 0;
    } else {
        end = gDiagBtns_B8[gDiag_F4] + 19;
        if (end < 38) {
            end = 38;
        }
        if ((u32)end >= count) {
            end = count;
        }
        start = end - 38;
    }
    for (i = start; i < end; i++) {
        if (i == gDiagBtns_B8[gDiag_F4]) {
            fn_800C008C(0x00FFFF00, 1, line, ((DiagRow*)p->strs)[i].name);
            len = strlen(((DiagRow*)p->strs)[i].name) + 3;
            if (((DiagRow*)p->strs)[i].val > 0) {
                fn_800C008C(0x00FFFF00, len, line, lbl_80348758, gDiag_D0C);
            }
        } else {
            fn_800C008C(0x00FFFFFF, 1, line, ((DiagRow*)p->strs)[i].name);
        }
        line++;
    }
}

extern u32 lbl_80240FC0[];      /* pb frame/screen state block */
extern f32 gDiag_E68;           /* color-bar animation speed scale */
extern f64 lbl_803486D0;        /* E68 ramp step */
extern f32 lbl_803486B4;        /* E68 reset value */
extern f32 lbl_80348670;        /* zero */
extern f64 lbl_80348710;        /* hue speed factor */
extern f64 lbl_80348718;        /* hue speed cap test */
extern f32 lbl_80348720;        /* hue speed cap */
extern f32 lbl_80348724;        /* hue ctrl min */
extern f32 lbl_80348728;        /* hue ctrl max */
extern f64 lbl_80348730;        /* wrap high bound */
extern f64 lbl_80348738;        /* wrap full circle */
extern f64 lbl_80348740;        /* wrap low bound */
extern f64 lbl_80348748;        /* sat speed factor */
extern f32 lbl_80348750;        /* sat/val ctrl min */
extern f32 lbl_80348754;        /* sat/val ctrl max */
extern f64 lbl_80348688;        /* val speed factor */
f32 pbDiagCtrlFloat(s32 axis, s32 pad, f32 val, f32 inc, f32 min, f32 max);

/* buttons block view: color-bar HSV/RGB state at +368 */
typedef struct DiagPadView {
    u32 words[92];          /* 0x000: raw pad words (word 5 = held buttons) */
    f32 f368;               /* 0x170: hue A */
    f32 f372;               /* 0x174: hue B */
    f32 f376;               /* 0x178 */
    f32 f380;               /* 0x17C: sat */
    f32 f384;               /* 0x180: val A */
    f32 f388;               /* 0x184: val B */
} DiagPadView;

/* animate/adjust the diag color-bar HSV values from pad input */
#pragma opt_propagation off
void pbDiagDrawColorBars(void)
{
    f32* gd = gDiagData;
    DiagPadView* b = (DiagPadView*)buttons;
    f32* dst;
    f32 spd;
    f64 v;

    if (lbl_80240FC0[1] != 0) {
        gDiag_E68 = (f32)(gDiag_E68 + lbl_803486D0);
    } else {
        gDiag_E68 = lbl_803486B4;
    }
    if (b->words[5] & 0x40000) {
        f32 z = lbl_80348670;
        b->f368 = z;
        b->f372 = z;
        b->f376 = z;
        b->f380 = gd[36];
        b->f384 = gd[37];
        b->f388 = gd[38];
    } else {
        v = lbl_80348710 * gDiag_E68;
        spd = (f32)v;
        if (spd > lbl_80348718) {
            spd = lbl_80348720;
        }
        b->f368 = pbDiagCtrlFloat(1, 1, b->f368, -spd, lbl_80348724, lbl_80348728);
        v = b->f368;
        if (v > lbl_80348730) {
            v = v - lbl_80348738;
        } else if (v <= lbl_80348740) {
            v = lbl_80348738 + v;
        }
        b->f368 = (f32)v;
        dst = &b->f372;
        *dst = pbDiagCtrlFloat(0, 1, b->f372, spd, lbl_80348724, lbl_80348728);
        v = *dst;
        if (v > lbl_80348730) {
            v = v - lbl_80348738;
        } else if (v <= lbl_80348740) {
            v = lbl_80348738 + v;
        }
        *dst = (f32)v;
        b->f380 = pbDiagCtrlFloat(3, 1, b->f380, (f32)(lbl_80348748 * gDiag_E68),
                                  lbl_80348750, lbl_80348754);
        dst = &b->f384;
        *dst = pbDiagCtrlFloat(4, 1, b->f384, (f32)(lbl_80348688 * gDiag_E68),
                               lbl_80348750, lbl_80348754);
        dst = &b->f388;
        *dst = pbDiagCtrlFloat(2, 1, b->f388, (f32)(lbl_80348688 * gDiag_E68),
                               lbl_80348750, lbl_80348754);
    }
}
#pragma opt_propagation reset

#pragma opt_propagation off
void pbInitDiag(int mode) {
    f32* dp = gDiagData;
    f32* fp = (f32*)buttons;

    AudioStopSelect();
    AudioSelectReset();
    fn_800C0310();
    MBTreeInit();
    DebugCamInit();
    fp[92] = 0.0f;
    fp[93] = 0.0f;
    fp[94] = 0.0f;
    fp[95] = dp[36];
    fp[96] = dp[37];
    fp[97] = dp[38];
    gDiag_DC = 0;
    gDiag_E0 = mode;
    gDiag_E4 = -2;
    gDiagWhiteObj = MBOX_FindTexture("aaawhite", 0);
    gDiag_D4 = 0;
    pbResetDiag();
}
#pragma opt_propagation reset

void pbResetDiag(void) {
    int i;
    u32* p = buttons;

    gDiag_F0 = *gWinGlobals->f30;
    gDiag_F4 = 0;
    for (i = 0; i < 16; i++) {
        p[i + 12] = 0;
        p[i + 28] = 0;
        p[i + 44] = 0;
    }
    for (i = 0; i < 24; i++) {
        p[i + 44] = 0;
        p[i + 68] = 0;
    }
    gDiag_FC = 0;
    gDiag_D08 = 0;
    gDiagRepeatDelay = 15;
    gDiagRepeatRate = 8;
    gDiag_D04 = 0;
    gDiag_D00 = 0;
    p[98] = 0;
    p[106] = 0;
    gDiag_D8 = 0;
}

f32 pbDiagCtrlFloat(s32 axis, s32 pad, f32 val, f32 inc, f32 min, f32 max) {
    u32 up;
    u32 down;

    if (gDiagRepeatDelay != 0) {
        return val;
    }
    if (gDiagTurbo) {
        if (buttons[pad] & 0x00100000) {
            inc *= 5.0f;
        }
    }
    switch (axis) {
    case 0:
    default:
        up = 3;
        down = 12;
        break;
    case 1:
        up = 0x30;
        down = 0xC0;
        break;
    case 2:
        up = 0x08000000;
        down = 0x02000000;
        break;
    case 3:
        up = 0x04000000;
        down = 0x01000000;
        break;
    case 4:
        up = 0x00400000;
        down = 0x00800000;
        break;
    }
    if (buttons[pad] & up) {
        val += inc;
        gDiagRepeatDelay = gDiagRepeatRate;
        if (val > max) {
            val = min;
        }
    }
    if (buttons[pad] & down) {
        val -= inc;
        gDiagRepeatDelay = gDiagRepeatRate;
        if (val < min) {
            val = max;
        }
    }
    return val;
}

s32 pbDiagCtrlInt(s32 axis, s32 pad, s32 val, s32 inc, s32 min, s32 max) {
    u32 up;
    u32 down;

    if (gDiagRepeatDelay != 0) {
        return val;
    }
    if (gDiagTurbo) {
        if (buttons[pad] & 0x00100000) {
            inc *= 5;
        }
    }
    switch (axis) {
    case 0:
    default:
        up = 3;
        down = 12;
        break;
    case 1:
        up = 0x30;
        down = 0xC0;
        break;
    case 2:
        up = 0x08000000;
        down = 0x02000000;
        break;
    case 3:
        up = 0x04000000;
        down = 0x01000000;
        break;
    case 4:
        up = 0x00400000;
        down = 0x00800000;
        break;
    }
    if (buttons[pad] & down) {
        val += inc;
        gDiagRepeatDelay = gDiagRepeatRate;
        if (val >= max) {
            val = min;
        }
    }
    if (buttons[pad] & up) {
        val -= inc;
        gDiagRepeatDelay = gDiagRepeatRate;
        if (val < min) {
            val = max - 1;
        }
    }
    return val;
}
