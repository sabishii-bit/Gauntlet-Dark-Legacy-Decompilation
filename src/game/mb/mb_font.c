/*
 * mb_font.c - MB text / drawtext layer (MB_FONT.OBJ).
 *
 * The text-drawing layer of the Midway "MB" graphics library: font registration
 * (MBNewFont), per-frame drawtext message queue (MBDrawText/MBDrawSysText into a
 * bounded message + character buffer -> "TOO MANY DRAWTEXT MESSAGES/CHARACTERS"),
 * message lock/save-restore stack (MBLockMessages "> max"/MBUnlockMessages), the
 * text rasteriser that submits glyph quads through the pb blit pipeline
 * (MBRenderText -> mbInitBlitEntry/mbBlitProject/mbBlitSetupVerts/mbBlitCalcClip/
 * DrawBlit), and the current-font attribute setters (font index/flags/z/colour/
 * alpha/scale, all held in the 0x80344E10.. sdata block).
 *
 * Address range 0x800B5AA8..0x800B6CDC (24 functions). TU identity is pinned by
 * the assert strings "MBLockMessages > max" (@0x80115CE8), "TOO MANY DRAWTEXT
 * MESSAGES/CHARACTERS" (@0x80115D00/D20) and "MBNewFont: MBNewBlit failed" /
 * "Too many fonts" (@0x80115D44/D60), and by its private sdata2 pool
 * (0x80348B48..0x80348B8C, disjoint from the mb_camera pool below it). Function
 * names come from the Xbox shell3D PDB (MB_FONT.OBJ).
 *
 * cflags_demo (-O4 no-peephole, -Cpp_exceptions on, -str reuse,readonly).
 *
 * Status: NonMatching. The current-font setters, string width/height helpers,
 * and lock/unlock stack are reconstructed bodies; the rasteriser and remaining
 * reset/init cluster are documented stubs (call/flow shape only). Several
 * mid-TU helpers whose Xbox name is not uniquely pinned are left as fn_.
 */
#include "types.h"

/* --- MB_FONT current-font + message state (0x80344E10.. sdata block).
 *     Referenced by lbl_ address to stay byte-identical against the DOL. --- */
extern s32 lbl_80344E10;      /* font_count */
extern s32 lbl_80344E14;      /* current_font_index */
extern s32 lbl_80344E18;      /* live message_count */
extern s32 lbl_80344E20;      /* live textbuf_count */
extern f32 lbl_80344E4C;      /* font z */
extern s32 lbl_80344E50;      /* font flags */
extern u32 lbl_80344E54;      /* font colour (packed AARRGGBB, MB half-range) */
extern f32 lbl_80344E58;      /* font scale y */
extern f32 lbl_80344E5C;      /* font scale x */
extern f32 lbl_80344E60;      /* font space scale y */
extern f32 lbl_80344E64;      /* font space scale x */
extern s32 lbl_80343EB0;      /* message_lock_level */
extern s32 lbl_80344E1C;      /* saved message_count (hide/restore) */
extern s32 lbl_80344E24;      /* saved textbuf_count (hide/restore) */
extern s32 lbl_80344E28;      /* fonts-changed flag */

extern s32 lbl_8029F474[8];   /* saved message_count per lock level */
extern s32 lbl_802A4A84[8];   /* saved textbuf_count per lock level */
extern s32 lbl_8029E454[];    /* saved font_count per font-lock level */
extern void* lbl_802A4AA4[];  /* fonts[] : per-font descriptor pointers */

/* per-drawtext message record (44 bytes) */
typedef struct MBTextMsg {
    u32 flags;    /* 0x00 (0x02000000 = marked; bit0 = hidden) */
    s32 x;        /* 0x04 */
    s32 y;        /* 0x08 */
    f32 z;        /* 0x0C */
    char* text;   /* 0x10 -> slice of the char buffer lbl_8029E474 */
    f32 xspace;   /* 0x14 */
    f32 xscale;   /* 0x18 */
    f32 yspace;   /* 0x1C */
    f32 yscale;   /* 0x20 */
    s16 font;     /* 0x24 */
    s16 seq;      /* 0x26 */
    u32 color;    /* 0x28 */
} MBTextMsg;

extern MBTextMsg lbl_8029F494[]; /* drawtext message records (44B each) */
extern char lbl_8029E474[];      /* drawtext character buffer */

typedef struct MBFont MBFont;

/* The font-space table is the first member of the module's contiguous BSS
 * state.  The live font-pointer table starts at +0x66DC. */
typedef struct MBFontState {
    s32 space[35];
    u8 _pad008C[0x6650];
    MBFont* fonts[35];
} MBFontState;

extern MBFontState mbfont_space;

struct MBFont {
    s32 height;   /* 0x0 */
    u8* cells;    /* 0x4  maxCode+1 blit entries, 36B each */
    s32 count;    /* 0x8 */
    u32 flags;    /* 0xC  bit0 = remap punctuation/extended chars */
};

extern void FatalError(const char* msg, u32 code);
extern void ErrorPrintf(const char* fmt, ...);
extern u32  strlen(const char* s);
extern char* strcpy(char* d, const char* s);
extern void* MBCreateBlit();
extern void  MBRemoveBlit();
extern void* AllocMem();
extern void* memset(void* p, int c, u32 n);
extern void* memcpy(void* d, const void* s, u32 n);
extern void  mbInitBlitEntry();
extern void  mbBlitProject();
extern void  mbBlitSetupVerts(void* e, f32 u1, f32 u2, f32 v1, f32 v2);
extern void  mbBlitCalcClip();
extern void  mbBlitGetPage();
extern void  mbBlitSetPage();
extern void  DrawBlit();
extern s32   mbBlitCalcX();

/* forward decls for internal helpers */
int MBFontStringWidth(const char* s);

/* ==== current-font attribute setters ==== */

/* 0x800B6358 - MBSetFont : select current font, return previous index */
int MBSetFont(int idx)
{
    int old = lbl_80344E14;
    lbl_80344E14 = idx;
    return old;
}

/* 0x800B63A0 - MBSetFontZ : set text depth, return previous */
f32 MBSetFontZ(f32 z)
{
    f32 old = lbl_80344E4C;
    lbl_80344E4C = z;
    return old;
}

/* 0x800B63B0 - MBSetFontFlags : set draw flags, return previous */
int MBSetFontFlags(int flags)
{
    int old = lbl_80344E50;
    lbl_80344E50 = flags;
    return old;
}

/* 0x800B6368 - MBSetFontColor : set RGB (MB half-range c/2+1 per component),
 *              keep current alpha, return previous RGB */
u32 MBSetFontColor(u32 rgb)
{
    u32 old = lbl_80344E54;
    lbl_80344E54 = old & 0xFF000000;
    lbl_80344E54 = lbl_80344E54 | (((rgb >> 1) & 0x007F7F7F) + 0x00010101);
    return old & 0x00FFFFFF;
}

/* 0x800B63C0 - MBSetFontAlpha : set alpha (MB 128-a/2 encoding), return prev */
int MBSetFontAlpha(u32 alpha)
{
    u32 old = lbl_80344E54;
    alpha >>= 1;
    lbl_80344E54 = old & 0x00FFFFFF;
    old = 128 - (old >> 24);
    lbl_80344E54 = lbl_80344E54 | ((128 - alpha) << 24);
    return old << 1;
}

/* 0x800B63F4 - MBSetFontScale : set glyph x/y scale (positive only) */
void MBSetFontScale(f32 sx, f32 sy)
{
    if (sx > 0.0f) lbl_80344E5C = sx;
    if (sy > 0.0f) lbl_80344E58 = sy;
}

/* 0x800B6418 - MBSetFontScaleSpace : set inter-glyph spacing + glyph scale */
void MBSetFontScaleSpace(f32 sx, f32 sy)
{
    if (sx > 0.0f) { lbl_80344E64 = sx; lbl_80344E5C = sx; }
    if (sy > 0.0f) { lbl_80344E60 = sy; lbl_80344E58 = sy; }
}

/* 0x800B5AA8 - MBFontMsgSetAlpha : set a queued message's alpha, return prev */
int MBFontMsgSetAlpha(MBTextMsg* m, u32 alpha)
{
    u32 old = m->color;
    alpha = 128 - (alpha >> 1);
    m->color = old & 0x00FFFFFF;
    old = (old >> 23) & 0x1FE;
    m->color = m->color | (alpha << 24);
    return old;
}

/* ==== message lock / save-restore stack ==== */

/* 0x800B5D20 - MBLockMessages : push current drawtext counts at a lock level */
void MBLockMessages(int level)
{
    if (level >= 8) {
        FatalError("MBLockMessages > max", 0x00800000);
    }
    lbl_8029F474[level] = lbl_80344E18;
    lbl_802A4A84[level] = lbl_80344E20;
    lbl_80343EB0 = level;
}

/* 0x800B5CCC - MBUnlockMessages : pop back to a lock level (or clear if empty) */
void MBUnlockMessages(int level)
{
    lbl_80343EB0 = level - 1;
    if (lbl_80343EB0 < 0) {
        lbl_80344E20 = 0;
        lbl_80344E18 = 0;
    } else {
        lbl_80344E18 = lbl_8029F474[lbl_80343EB0];
        lbl_80344E20 = lbl_802A4A84[lbl_80343EB0];
    }
}

/* 0x800B6C04 - MBLockFonts : save current font_count at a font-lock level */
void MBLockFonts(int level)
{
    lbl_8029E454[level] = lbl_80344E10;
}

/* ==== internal helpers ==== */

/* 0x800B5AD8 - return the selected font's pixel height. */
int MBFontHeight(int idx)
{
    if (idx < 0) idx = lbl_80344E14;
    return *(s32*)lbl_802A4AA4[idx];
}

/* 0x800B5B00 - pixel width of a string in the current font. */
int MBFontStringWidth(const char* s)
{
    MBFontState* state = &mbfont_space;
    int width = 0;
    const char* str = s;
    int x;
    MBFont* font;
    int ch;

    if (lbl_80344E14 < 0) {
        lbl_80344E14 = 0;
    }
    font = state->fonts[lbl_80344E14];

    while (*(u8*)str != 0) {
        ch = *(u8*)str;
        switch (ch) {
        case '*':
            if ((u8)str[1] < 'A' || (u8)str[1] > 'Z') {
                goto add_width;
            }
            {
                MBFont* specialFont;

                str++;
                specialFont = (MBFont*)((u8*)state + lbl_80344E14 * 4);
                specialFont = *(MBFont**)((u8*)specialFont + 0x66dc);
                x = (s32)(lbl_80344E5C * (f32)specialFont->height);
            }
            goto add_width;
        default:
            break;
        }

        if (font->flags & 1) {
            if (ch >= 128) {
                ch = *(u8*)++str;
            } else if (ch >= '0' && ch <= '9') {
                /* Numeric glyphs are already in the remapped range. */
            } else if (ch == '.') {
                ch = 58;
            } else if (ch == '-') {
                ch = 59;
            } else {
                goto add_width;
            }
        }

        mbBlitCalcX(font->cells + ch * 36, &x, 0);
        x = (s32)((f32)x * lbl_80344E5C);
        if (x == 0 && ch == ' ') {
            x = (s32)(lbl_80344E5C * (f32)state->space[lbl_80344E14]);
        }

    add_width:
        width += x;
        str++;
    }
    return width;
}

/* ==== drawtext queue ==== */

/* 0x800B6444 - MBDrawSysText : queue a string in the system font (font 0,
 * unit scale), silently dropping it when out of bounds or buffer-full. */
MBTextMsg* MBDrawSysText(int x, int y, const char* s)
{
    int len;
    int total;
    MBTextMsg* m;

    if (lbl_80344E20 >= 499) {
        return 0;
    }
    len = strlen(s) + 1;
    total = lbl_80344E18 + len;
    if (total >= 4095) {
        return 0;
    }
    if (x < 0) {
        x = -x - (MBFontStringWidth(s) >> 1);
    }
    if (y < 0 || y >= 384) {
        return 0;
    }
    if (x < 0 || x >= 512) {
        return 0;
    }
    m = &lbl_8029F494[lbl_80344E20++];
    m->x = x;
    m->y = y;
    m->z = lbl_80344E4C;
    m->font = 0;
    m->yspace = 1.0f;
    m->xspace = 1.0f;
    m->yscale = 1.0f;
    m->xscale = 1.0f;
    m->text = lbl_8029E474 + lbl_80344E18;
    m->color = lbl_80344E54;
    m->flags = lbl_80344E50;
    m->seq = -1;
    strcpy(m->text, s);
    lbl_80344E18 += len;
    return m;
}

/* 0x800B6588 - MBDrawText : queue a string at (x, y).  Negative x centres
 * the string (half its pixel width left of -x).  Bounds-check the message
 * and character buffers, then fill the next message record from the
 * current-font state and copy the text into the character buffer. */
MBTextMsg* MBDrawText(int x, int y, const char* s)
{
    int len;
    MBTextMsg* m;

    if (lbl_80344E20 >= 499) {
        ErrorPrintf("TOO MANY DRAWTEXT MESSAGES: %d", lbl_80344E20);
        return 0;
    }
    len = strlen(s) + 1;
    if (lbl_80344E18 + len >= 4095) {
        ErrorPrintf("TOO MANY DRAWTEXT CHARACTERS: %d",
                    lbl_80344E18 + len);
        return 0;
    }
    if (x < 0) {
        x = -x - (MBFontStringWidth(s) >> 1);
    }
    if (y < 0 || y >= 384) {
        return 0;
    }
    m = &lbl_8029F494[lbl_80344E20++];
    m->x = x;
    m->y = y;
    m->z = lbl_80344E4C;
    m->font = lbl_80344E14;
    m->xspace = lbl_80344E64;
    m->yspace = lbl_80344E60;
    m->xscale = lbl_80344E5C;
    m->yscale = lbl_80344E58;
    m->text = lbl_8029E474 + lbl_80344E18;
    m->color = lbl_80344E54;
    m->flags = lbl_80344E50;
    m->seq = -1;
    strcpy(m->text, s);
    lbl_80344E18 += len;
    return m;
}

/* ==== rasteriser ==== */

/* 0x800B5DEC - MBRenderText : rasterise queued messages through the pb blit
 * pipeline. NonMatching stub: documented call shape only. */
void MBRenderText(void)
{
    mbBlitGetPage();
    mbInitBlitEntry();
    mbBlitProject();
    mbBlitSetupVerts((void*)0, 0.0f, 0.0f, 0.0f, 0.0f);
    mbBlitCalcClip();
    DrawBlit();
    mbBlitSetPage();
}

/* ==== font registration ==== */

/* 0x800B66E8 - MBNewFont : register a font (MBCreateBlit); "Too many fonts" /
 * "MBNewFont: MBNewBlit failed". NonMatching stub. */
typedef struct MBGlyphDef {   /* one entry of the caller's glyph table (16B) */
    s32 code;   /* 0x0  glyph index in the font (0 terminates) */
    s32 w;      /* 0x4  pixel width */
    s32 u;      /* 0x8  texture x */
    s32 v;      /* 0xC  texture y */
} MBGlyphDef;

typedef struct MBFontDef {    /* MBNewFont input descriptor */
    char* texname;      /* 0x0 */
    u32 flags;          /* 0x4  low byte = glyph height; 0x100 = additive */
    MBGlyphDef* glyphs; /* 0x8 */
} MBFontDef;

typedef struct MBTexHdr { u8 _p[32]; u16 w; u16 h; } MBTexHdr;
typedef struct MBBlitCell { u8 _p[32]; u16 w; u16 h; } MBBlitCell;

extern MBTexHdr* MBOX_FindTexture_Err(char* name, MBTexHdr** out, s32 err);

/* 0x800B65F4 - MBNewFont : register a font.  Finds the texture, sizes the
 * glyph-cell table from the highest glyph code, builds one projected blit
 * entry per glyph (u/v from the texture dimensions) and stores the font in
 * the font table.  Returns the new font index. */
int MBNewFont(MBFontDef* def, int space, int nglyphs, int perRow)
{
    MBTexHdr* tex = 0;
    f32 su = 0.125f;
    f32 sv = su;
    MBFont* fnt;
    void* blit;
    MBGlyphDef* g;
    u8* dst;
    MBBlitCell* c;
    int i;
    int maxCode;
    int size;
    int cc;

    blit = MBCreateBlit(0, MBOX_FindTexture_Err(def->texname, &tex, 1),
                        0, 0, -1, -1);
    if (blit == 0) {
        FatalError("MBNewFont: MBNewBlit failed", 0x800000);
    }
    if (tex != 0) {
        su = 1.0 / tex->w;
        sv = 1.0 / tex->h;
    }
    maxCode = 0;
    g = def->glyphs;
    for (i = 0; i < nglyphs && (cc = g->code) != 0; i++, g++) {
        if (cc > maxCode) {
            maxCode = cc;
        }
    }
    fnt = (MBFont*)AllocMem(16);
    memset(fnt, 0, 16);
    maxCode++;
    size = maxCode * 36;
    fnt->cells = (u8*)AllocMem(size);
    fnt->count = maxCode;
    memset(fnt->cells, 0, size);
    if (def->flags & 0x100) {
        fnt->flags |= 1;
    }
    fnt->height = def->flags & 0xFF;
    g = def->glyphs;
    for (i = 0; i < nglyphs && (cc = g->code) != 0; i++, g++) {
        dst = fnt->cells + cc * 36;
        memcpy(dst, blit, 36);
        mbInitBlitEntry(dst, -1, i / perRow);
        mbBlitProject(dst, g->w, fnt->height);
        c = (MBBlitCell*)dst;
        c->w = g->w;
        c->h = fnt->height;
        mbBlitSetupVerts(dst, g->u * su, su * (f32)(g->u + g->w),
                          g->v * sv, sv * (f32)(g->v + fnt->height));
    }
    MBRemoveBlit(blit);
    if (lbl_80344E14 < 0) {
        lbl_80344E14 = lbl_80344E10;
    }
    mbfont_space.space[lbl_80344E10] = space;
    lbl_802A4AA4[lbl_80344E10] = fnt;
    lbl_80344E10 = lbl_80344E10 + 1;
    if (lbl_80344E10 >= 35) {
        FatalError("Too many fonts", 0x800000);
    }
    return lbl_80344E10 - 1;
}

/* ==== reset / init / query cluster ==== */

/* Shared current-font attribute reset (static, auto-inlined into the four
 * resetters below; the standalone copy is deadstripped by mwld). */
static void MBResetFontAttrs(void)
{
    lbl_80344E20 = 0;
    lbl_80344E18 = 0;
    lbl_80344E14 = -1;
    lbl_80344E60 = 1.0f;
    lbl_80344E64 = 1.0f;
    lbl_80344E58 = 1.0f;
    lbl_80344E5C = 1.0f;
    lbl_80344E50 = 0;
    lbl_80344E4C = 0.25f;
    lbl_80344E54 = 0x80808080;
}

/* 0x800B5D90 - stash the live message/textbuf counts, then restore the
 * lock-level snapshot (or clear when unlocked). */
void fn_800B5D90(void)
{
    int lock = lbl_80343EB0;

    lbl_80344E1C = lbl_80344E18;
    lbl_80344E24 = lbl_80344E20;
    if (lock < 0) {
        lbl_80344E20 = 0;
        lbl_80344E18 = 0;
        return;
    }
    lbl_80344E18 = lbl_8029F474[lock];
    lbl_80344E20 = lbl_802A4A84[lock];
}

/* 0x800B69DC - rescale every live glyph cell after a render-window change
 * (Xbox: MBFontUpdateWindow). */
void MBFontUpdateWindow(f32 scaleX, f32 scaleY)
{
    u8 unused[8];
    s32 fontIndex;

    for (fontIndex = 0; fontIndex < lbl_80344E10; fontIndex++) {
        s32 cellIndex;
        MBFont* font = (MBFont*)lbl_802A4AA4[fontIndex];

        for (cellIndex = 0; cellIndex < font->count; cellIndex++) {
            u8* cell = font->cells + cellIndex * 36;

            if (*(u16*)(cell + 16) != 0 && (*(u32*)cell & 0x40) == 0) {
                *(u16*)(cell + 8) = (u16)((f32)*(u16*)(cell + 8) * scaleX);
                *(u16*)(cell + 10) = (u16)((f32)*(u16*)(cell + 10) * scaleY);
                *(u16*)(cell + 16) = (u16)((f32)*(u16*)(cell + 16) * scaleX);
                if ((*(u32*)cell & 0x100) == 0) {
                    *(u16*)(cell + 18) = (u16)((f32)*(u16*)(cell + 18) * scaleY);
                }
            }
        }
    }
}

/* 0x800B6B08 - full font reset: drop all fonts + lock saves, mark changed. */
void MBInitFonts(void)
{
    int i;

    lbl_80344E10 = 0;
    for (i = 0; i < 8; i++) {
        lbl_8029E454[i] = 0;
    }
    lbl_80344E28 = 1;
    MBResetFontAttrs();
}

/* 0x800B6B80 - flag pass over the live messages: any marked message
 * (0x02000000) is hidden (bit 0). */
void MBHideMarkedMessages(void)
{
    int i;

    for (i = 0; i < lbl_80344E20; i++) {
        if (lbl_8029F494[i].flags & 0x02000000) {
            lbl_8029F494[i].flags |= 1;
        }
    }
}

/* 0x800B6BC0 - reset the current-font attributes only. */
void MBResetFonts(void)
{
    MBResetFontAttrs();
}

/* 0x800B6C20 - unlock fonts to `level`: restore the saved font count (mark
 * changed if it differs), reset attributes and drop the message lock. */
void MBResetUnlockedFonts(int level)
{
    if (lbl_80344E10 != lbl_8029E454[level]) {
        lbl_80344E10 = lbl_8029E454[level];
        lbl_80344E28 = 1;
    }
    MBResetFontAttrs();
    lbl_80343EB0 = -1;
}

/* 0x800B6C94 - full attribute reset + message unlock. */
void MBResetFontData(void)
{
    MBResetFontAttrs();
    lbl_80343EB0 = -1;
}
