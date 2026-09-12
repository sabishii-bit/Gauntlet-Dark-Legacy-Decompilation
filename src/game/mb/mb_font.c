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
 * Native GC 1.2.5 (-O4, -Cpp_exceptions on, -str reuse,readonly).
 *
 * Status: NonMatching. All 24 bodies are reconstructed; MBRenderText and
 * MBNewFont retain native instruction differences. The independent BSS arrays
 * below cover 0x8029E3C8..0x802A4B30. Unresolved Xbox names remain fn_.
 */
#include "types.h"
#include "game/mb_font.h"

/* --- MB_FONT current-font + message state (0x80344E10.. sdata block).
 *     Referenced by lbl_ address to stay byte-identical against the DOL. --- */
extern s32 lbl_80344E10;      /* font_count */
extern s32 lbl_80344E14;      /* current_font_index */
extern s32 lbl_80344E18;      /* live textbuf_count */
extern s32 lbl_80344E20;      /* live message_count */
extern f32 lbl_80344E4C;      /* font z */
extern s32 lbl_80344E50;      /* font flags */
extern u32 lbl_80344E54;      /* font colour (packed AARRGGBB, MB half-range) */
extern f32 lbl_80344E58;      /* font scale y */
extern f32 lbl_80344E5C;      /* font scale x */
extern f32 lbl_80344E60;      /* font space scale y */
extern f32 lbl_80344E64;      /* font space scale x */
extern s32 lbl_80343EB0;      /* message_lock_level */
extern s32 lbl_80344E1C;      /* saved textbuf_count (hide/restore) */
extern s32 lbl_80344E24;      /* saved message_count (hide/restore) */
extern s32 lbl_80344E28;      /* fonts-changed flag */

typedef struct MBFont MBFont;

/* Independent MB_FONT arrays, in their GameCube BSS order. The PDB records
 * corroborate the bounds; the GC queue admits 499 of the 500 message slots.
 * The font-pointer table is also read by the save and credits screens. */
static s32 mbfont_space[35];
static s32 lbl_8029E454[8];        /* lock_font_count */
static char lbl_8029E474[4096];    /* textbuf */
static s32 lbl_8029F474[8];        /* lock_textbuf_count */
static MBTextMsg lbl_8029F494[500]; /* messages */
static s32 lbl_802A4A84[8];        /* lock_message_count */
MBFont* lbl_802A4AA4[35];          /* fonts */

/* GC letter record: 36-byte stride/copy in MBNewFont. The same record is
 * MBRenderText's local at SP+32..SP+67 (next live local at SP+68), not a
 * 32-byte record plus anonymous padding. Xbox blitinst_letter (PDB 0x3AFE)
 * corroborates all field extents, including the trailing glyph dimensions.
 * x/y are signed here because GC's glow adjustment loads them with lha;
 * the width/height fields are loaded with lhz. */
typedef struct MBBlitCell {
    u32 flags;     /* 0x00 */
    u32 tex;       /* 0x04 */
    s16 x;         /* 0x08 */
    s16 y;         /* 0x0A */
    s32 depth;     /* 0x0C */
    u16 width;     /* 0x10 */
    u16 height;    /* 0x12 */
    u16 s_left;    /* 0x14 */
    u16 s_right;   /* 0x16 */
    u16 t_top;     /* 0x18 */
    u16 t_bot;     /* 0x1A */
    u32 color;     /* 0x1C */
    u16 w;         /* 0x20: original glyph width */
    u16 h;         /* 0x22: original glyph height */
} MBBlitCell;

struct MBFont {
    s32 height;   /* 0x0 */
    MBBlitCell* cells; /* 0x4  maxCode+1 blit cells, 36B each */
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

/* ==== internal helpers ==== */

/* 0x800B5AD8 - return the selected font's pixel height. */
int MBFontHeight(int idx)
{
    if (idx < 0) idx = lbl_80344E14;
    return lbl_802A4AA4[idx]->height;
}

/* 0x800B5B00 - pixel width of a string in the current font. */
int MBFontStringWidth(const char* s)
{
    int width = 0;
    const char* str = s;
    int x;
    MBFont* font;
    int ch;

    if (lbl_80344E14 < 0) {
        lbl_80344E14 = 0;
    }
    font = lbl_802A4AA4[lbl_80344E14];

    while (*(u8*)str != 0) {
        ch = *(u8*)str;
        switch (ch) {
        case '*':
            if ((u8)str[1] < 'A' || (u8)str[1] > 'Z') {
                goto add_width;
            }
            {
                str++;
                x = (s32)(lbl_80344E5C * (f32)lbl_802A4AA4[lbl_80344E14]->height);
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

        mbBlitCalcX((u8*)&font->cells[ch], &x, 0);
        x = (s32)((f32)x * lbl_80344E5C);
        if (x == 0 && ch == ' ') {
            x = (s32)(lbl_80344E5C * (f32)mbfont_space[lbl_80344E14]);
        }

    add_width:
        width += x;
        str++;
    }
    return width;
}

/* ==== message lock / save-restore stack ==== */

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

/* ==== rasteriser ==== */

extern s32 lbl_80344E2C;
extern s32 lbl_80344E30;
extern s32 lbl_80344E34;
extern s32 lbl_80344E38;
extern s32 lbl_80344E3C;
extern s32 lbl_80344E40;
extern s32 lbl_80344E44;
extern s32 lbl_80344E48;
extern u8* gWinGlobals;
extern f32 lbl_80343EB4;
extern f64 lbl_80348B48;
extern f64 lbl_80348B50;
extern f32 lbl_80348B58;
extern f64 lbl_80348B60;
extern f32 lbl_80348B68;
extern f64 lbl_80348B70;

/* The rasteriser uses the same letter record as the stored glyph cells.
 * Stored cells carry w/h glyph metrics; the renderer copies tex through
 * t_bot only. */
typedef MBBlitCell MBBlitEnt;

/* 0x800B5DEC - MBRenderText : rasterise queued messages through the pb blit
 * pipeline in two layer passes (flag-8 messages render on the second). */
void MBRenderText(void)
{
    u8* wg = gWinGlobals;
    s32 layer = 2;
    MBBlitEnt e;
    u8 unused[20];
    MBBlitEnt* ent = &e;
    u8* pRec;
    s32 sx2;
    s32 sy2;
    s32 baseY;
    s32 spaceW;
    u32 white;
    s32 sx;
    s32 sy;
    s32 i;
    s32 x;
    s32 y;
    s32 c;
    s32 extra;
    s32 doClip;
    s32 t;
    f32 clipX;
    f32 clipY;
    f32* wp;
    char* text;
    s32 hb;
    MBFont* font;
    s32 glyph;
    s32 glyphValue;
    MBTextMsg* msg;
    u32 ch;

    if (lbl_80344E28) {
        lbl_80344E28 = 0;
        return;
    }
    wp = *(f32**)(wg + 0x38);
    sx = (s32)(0.5 + lbl_80343EB4 * wp[0]);
    sy = (s32)(0.5 + lbl_80343EB4 * wp[1]);
    mbBlitGetPage();
    pRec = (u8*)&e.tex;
    sx2 = sx << 1;
    sy2 = sy << 1;
    white = 0x80808080;
    do {
        for (i = 0; i < lbl_80344E20; i++) {
            msg = &lbl_8029F494[i];
            if (msg->flags & 1) {
                continue;
            }
            if (msg->flags & 8) {
                if (layer != 0) {
                    layer = 1;
                    continue;
                }
            } else {
                if (layer == 0) {
                    continue;
                }
            }
            t = msg->font;
            baseY = msg->y;
            x = msg->x;
            font = lbl_802A4AA4[t];
            clipX = msg->xspace;
            clipY = msg->yspace;
            if (font == NULL) {
                continue;
            }
            ent->color = msg->color;
            ent->flags = msg->flags;
            {
                s32 u = 1;
                if (1.0f == msg->xspace && 1.0 == msg->yspace) {
                    u = 0;
                }
                doClip = u ? 1 : 0;
            }
            spaceW = (s32)(msg->xscale * (f32)mbfont_space[t]);
            text = msg->text;
            while ((ch = *(u8*)text) != 0) {
                y = baseY;
                c = ch;
                hb = -1;
                extra = 0;
                if ((s32)ch == 0x2a) {
                    switch ((u8)text[1]) {
                    case 'X':
                        hb = lbl_80344E48;
                        break;
                    case 'T':
                        hb = lbl_80344E44;
                        break;
                    case 'O':
                        hb = lbl_80344E3C;
                        break;
                    case 'S':
                        hb = lbl_80344E40;
                        break;
                    case 'L':
                        hb = lbl_80344E38;
                        break;
                    case 'R':
                        hb = lbl_80344E34;
                        break;
                    case 'U':
                        hb = lbl_80344E30;
                        break;
                    case 'D':
                        hb = lbl_80344E2C;
                        break;
                    default:
                        if (c < font->count) {
                            glyph = font->cells[c].w;
                        } else {
                            glyph = 0;
                        }
                        break;
                    }
                    if (hb <= 0 && glyph == 0) {
                        goto next_char;
                    }
                } else {
                    if (font->flags & 1) {
                        if (c >= 0x80) {
                            extra = c - 0x80;
                            c = *(u8*)++text;
                        } else if (c >= '0' && c <= '9') {
                        } else if (c == '.') {
                            c = 0x3a;
                        } else if (c == '-') {
                            c = 0x3b;
                        } else {
                            goto next_char;
                        }
                        glyph = font->cells[c].w;
                    } else {
                        if (c == 0) {
                            goto next_char;
                        }
                        if (c >= font->count) {
                            goto next_char;
                        }
                        glyphValue = font->cells[c].w;
                        glyph = glyphValue;
                        if (glyphValue == 0) {
                            if (c == 0x20) {
                                x += spaceW;
                            }
                            goto next_char;
                        }
                    }
                }
                memcpy(pRec, (u8*)&font->cells[c].tex, 0x18);
                if (hb > 0) {
                    text++;
                    glyph = font->height + 4;
                    if (msg->flags & 0x4000) {
                        x += (s32)((f32)glyph * msg->xscale);
                        goto next_char;
                    }
                    y -= 2;
                    mbInitBlitEntry(ent, hb, 0);
                    mbBlitProject(ent, glyph, glyph);
                    mbBlitSetupVerts(ent, 0.0f, 1.0f, 0.0f, 1.0f);
                    ent->color = white;
                    glyph -= 2;
                } else {
                    if (msg->seq >= 0) {
                        mbInitBlitEntry(ent, msg->seq, 0);
                    } else if (extra > 0) {
                        mbInitBlitEntry(ent, -1, extra);
                    }
                }
                wp = *(f32**)(wg + 0x38);
                ent->x =
                    (s16)(s32)(0.5 + (f32)x * wp[0]);
                wp = *(f32**)(wg + 0x38);
                ent->y =
                    (s16)(s32)(0.5 + (f32)y * wp[1]);
                ent->depth = (s32)(32.0 * msg->z);
                if (doClip) {
                    mbBlitCalcClip(ent, clipX, clipY);
                }
                if (msg->flags & 0x4000) {
                    ent->x -= sx;
                    ent->y -= sy;
                    ent->width += sx2;
                    ent->height += sy2;
                }
                x += (s32)((f32)glyph * msg->xscale);
                DrawBlit(ent);
                if (hb > 0) {
                    ent->color = msg->color;
                }
            next_char:
                text++;
            }
        }
    } while (--layer == 0);
    mbBlitSetPage();
}

/* ==== current-font attribute setters ==== */

/* 0x800B6358 - MBSetFont : select current font, return previous index */
int MBSetFont(int idx)
{
    int old = lbl_80344E14;
    lbl_80344E14 = idx;
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

/* ==== font registration ==== */

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

extern MBTexHdr* MBOX_FindTexture_Err(char* name, MBTexHdr** out, s32 err);

/* 0x800B66E8 - MBNewFont : register a font.  Finds the texture, sizes the
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
    MBBlitCell* c;
    int i;
    MBGlyphDef* g;
    int maxCode;
    u8* dst;
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
    fnt->cells = (MBBlitCell*)AllocMem(size);
    fnt->count = maxCode;
    memset(fnt->cells, 0, size);
    if (def->flags & 0x100) {
        fnt->flags |= 1;
    }
    fnt->height = def->flags & 0xFF;
    g = def->glyphs;
    for (i = 0; i < nglyphs && (cc = g->code) != 0; i++, g++) {
        dst = (u8*)&fnt->cells[cc];
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
    mbfont_space[lbl_80344E10] = space;
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
static inline void MBResetFontAttrs(void)
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

/* 0x800B69DC - rescale every live glyph cell after a render-window change
 * (Xbox: MBFontUpdateWindow). */
void MBFontUpdateWindow(f32 scaleX, f32 scaleY)
{
    u8 unused[8];
    s32 fontIndex;

    for (fontIndex = 0; fontIndex < lbl_80344E10; fontIndex++) {
        s32 cellIndex;
        MBFont* font = lbl_802A4AA4[fontIndex];

        for (cellIndex = 0; cellIndex < font->count; cellIndex++) {
            u8* cell = (u8*)&font->cells[cellIndex];

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

/* 0x800B6C04 - MBLockFonts : save current font_count at a font-lock level */
void MBLockFonts(int level)
{
    lbl_8029E454[level] = lbl_80344E10;
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
