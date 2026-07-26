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

/* window globals pointer (owned by pb_window, lbl_80344FC0) */
extern WinGlobals* gWinGlobals;

/* diag config block (this TU's .data, lbl_80126AA0, 0x170 bytes) */
extern f32 gDiagData[];

/* menu-B highlight table + current index */
extern s32 gDiagMenuList[];     /* lbl_8028C438 */
extern s32 gDiagMenuIdx;        /* lbl_80343E64 */

/* diag state (sdata/sbss, owned by this TU) */
extern s32 gDiagTurbo;          /* lbl_80343E60 */
extern void* gDiagWhiteObj;     /* lbl_80344CD0 */
extern s32 gDiag_D4;            /* lbl_80344CD4 */
extern s32 gDiag_D8;            /* lbl_80344CD8 */
extern s32 gDiag_DC;            /* lbl_80344CDC */
extern s32 gDiagListSel;        /* lbl_80344D18 (menu-A highlight index) */
extern s32 gDiag_E0;            /* lbl_80344CE0 */
extern s32 gDiag_E4;            /* lbl_80344CE4 */
extern s32 gDiagRepeatDelay;    /* lbl_80344CE8 */
extern s32 gDiagRepeatRate;     /* lbl_80344CEC */
extern s32 gDiag_F0;            /* lbl_80344CF0 */
extern s32 gDiag_F4;            /* lbl_80344CF4 */
extern s32 gDiag_FC;            /* lbl_80344CFC */
extern s32 gDiag_D00;           /* lbl_80344D00 */
extern s32 gDiag_D04;           /* lbl_80344D04 */
extern s32 gDiag_D08;           /* lbl_80344D08 */

/* --- text / draw primitives + subsystem init (other TUs) --- */
extern void fn_800C008C(u32 rgba, int x, int y, const char* fmt, ...);
extern void fn_800A110C(void);
extern void fn_800A17D4(void);
extern void fn_800C0310(void);
extern void fn_800BA820(void);
extern void fn_80070930(void);
extern void* fn_800B8B04(const char* name, int arg);
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

void pbInitDiag(int mode) {
    f32* dp = gDiagData;
    f32* fp = (f32*)buttons;

    fn_800A110C();
    fn_800A17D4();
    fn_800C0310();
    fn_800BA820();
    fn_80070930();
    fp[92] = 0.0f;
    fp[93] = 0.0f;
    fp[94] = 0.0f;
    fp[95] = dp[36];
    fp[96] = dp[37];
    fp[97] = dp[38];
    gDiag_DC = 0;
    gDiag_E0 = mode;
    gDiag_E4 = -2;
    gDiagWhiteObj = fn_800B8B04("aaawhite", 0);
    gDiag_D4 = 0;
    pbResetDiag();
}

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
