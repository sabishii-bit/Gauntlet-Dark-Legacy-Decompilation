#include "types.h"
#include "__va_arg.h"

/* BTEXT.OBJ - bitmap/box text + SCROLLS message system (GameCube port).
 *
 * Text drawing on top of the G3D box-text primitives (MBFontHeight,
 * MBSetFont*, MBDrawText, MBNewFont) plus the "SCROLLS<level>" message-resource
 * loader (chunk tags FONT/TEXT/TOFF/STRS/LOFF/LIST/DEFS/SDEF/LDEF).  Function
 * names come from the Xbox shell3D.pdb BTEXT.OBJ roster, mapped to PPC by
 * strings / call-graph.
 *
 * Status: NonMatching.  Functions are emitted in target address order.  The
 * TU-local .bss pool (font_info .. gTextFormatBuf) is defined in-unit, in
 * declaration order, so font_info anchors the section at offset 0 and the list
 * accessors reproduce the font_info-relative sibling pooling.  21/45 functions
 * are byte-exact (all the leaf accessors/setters + the GetScroll/GetString
 * resolvers + DrawNormalText/DrawTextKeepScale).  The remaining full-bodied
 * functions (Find, Sub workers, DrawTextMLines, DrawGlowText) are
 * structurally complete with register/schedule residuals (opcode streams match;
 * see PARKED.txt).  DrawText and DrawStringText now reproduce the variadic
 * FP-save ABI exactly; DrawStringTextMLines is behavior-complete with only
 * loop-register residuals.  DrawStringTextMulti, the scroll wrappers, and the
 * big loader (StringInitSub) still need full reconstruction.
 */

/* ---- message-resource structures (SCROLLS files) ---- */

typedef struct MsgEnt {   /* 0x14 - one message */
    /* 0x00 */ s32 count; /* number of sub-lines */
    /* 0x04 */ s32 first; /* first index into the text-offset table */
    /* 0x08 */ s32 font;  /* font-descriptor index, -1 = inherit */
    /* 0x0C */ f32 scale;
    /* 0x10 */ f32 shScale;
} MsgEnt;

typedef struct ListEnt {  /* 0x08 - one nested message list */
    /* 0x00 */ s32 count;
    /* 0x04 */ s32 first;
} ListEnt;

typedef struct FontDesc { /* 0x14 - one font descriptor */
    /* 0x00 */ u8 _pad[0x10];
    /* 0x10 */ u32 color;
} FontDesc;

typedef struct StrList {  /* 0x44 - a loaded SCROLLS resource */
    /* 0x00 */ u8 arc[0xC];   /* archive handle */
    /* 0x0C */ FontDesc* fontDesc; /* FONT chunk (0x14-byte descriptors) */
    /* 0x10 */ u8* textData;  /* TEXT chunk */
    /* 0x14 */ s32* textOff;  /* TOFF chunk */
    /* 0x18 */ MsgEnt* msgs;  /* STRS chunk */
    /* 0x1C */ s32* listOff;  /* LOFF chunk */
    /* 0x20 */ ListEnt* lists;/* LIST chunk */
    /* 0x24 */ u8* nameData;  /* DEFS chunk */
    /* 0x28 */ s32* nameOff;  /* SDEF chunk */
    /* 0x2C */ u8* ldef;      /* LDEF chunk */
    /* 0x30 */ s32 nFont;
    /* 0x34 */ s32 nMsg;
    /* 0x38 */ s32 nList;
    /* 0x3C */ s32 nName;
    /* 0x40 */ s32 nLdef;
} StrList;

/* Address view used where the original source addressed the whole TU pool
 * through its font_info anchor. */
typedef struct BTextPoolView {
    void* fonts[14];
    u8 workBuf[0x800];
    StrList scrollLists[2];
    StrList stringList;
    u8 formatBuf[0x404];
} BTextPoolView;

/* ---- TU-local data pool (.bss, address order pins the font_info anchor) ---- */

void* font_info[14];           /* 0x8023EAE0 - per-mode loaded font pointers */
u8 gTextWorkBuf[0x800];         /* 0x8023EB18 - line-split scratch */
StrList gScrollMsgList[2];      /* 0x8023F318 - scroll lists */
StrList gStringMsgList;         /* 0x8023F3A0 - default list */
u8 gTextFormatBuf[0x404];       /* 0x8023F3E4 - vsprintf output */

/* .sdata (initialised) */
s32 scroll_level_msg = -1;      /* 0x80343BB8 */
f32 DrawStringScale = 1.0f;     /* 0x80343BBC */
f32 OldStringScale = 1.0f;      /* 0x80343BC0 */

/* .sbss */
s32 shadow_color;               /* 0x803443D8 */
s32 gDrawTextY;                 /* 0x803443DC */
s32 gLineSpacing;               /* 0x803443E0 */

/* Glow/font config + embedded font tables live in other pools; extern here. */
extern s32 glow_text_extra;     /* 0x803443E4 */
extern s32 gScrollModes[2];     /* 0x80343BB0 */
extern u32 glow_color;          /* 0x80343BC4 */
extern s32 glow_radius;         /* 0x80343BCC */
extern s32 glow_period;         /* 0x80343BD0 */
extern s32 glow_font;           /* 0x80343BD4 */
extern s32 gFontsInited;        /* 0x80344F5C */
extern s32 pbLoad;
extern u8 gDefaultFontData[];   /* 0x80237C60 */
extern char* gFontDefs8x8[];    /* 0x80118AF8 */
extern s32 gFontDefs[];         /* 0x80118B2C */
extern char lbl_801119C4[];     /* "SCROLLS%s" format string */
extern char sDrawStringTextMultiRangeError[41];
extern char sDrawScrollTextRangeError[36];
extern char sFontFileFormat[7]; /* "%s.fnt" */
extern char sFontDirectory[6];  /* "fonts" */
extern const f64 sBTextIntBias;
extern const f32 sBTextOne;
extern const f64 sBTextZero;
extern const f64 sBTextShadowSpacing;
extern const f64 sBTextOneDouble;

/* ---- external primitives ---- */

extern s32 MBFontHeight(s32 font);
extern s32 MBFontStringWidth(u8* str);                 /* 0x800B5B00 - measure/blit one line */
extern u32 MBSetFont(u32 font);                  /* 0x800B6358 - returns previous */
extern void MBSetFontColor(u32 color);           /* 0x800B6368 */
extern u32 MBSetFontFlags(u32 flags);            /* 0x800B63B0 */
extern void MBSetFontAlpha(s32 alpha);           /* 0x800B63C0 */
extern void MBSetFontScale(f32 sx, f32 sy);      /* 0x800B63F4 */
extern void MBSetFontScaleSpace(f32 sx, f32 sy); /* 0x800B6418 */
extern void* MBDrawText(s32 x, s32 y, u8* str);  /* 0x800B6588 */
extern s32 MBNewFont(void* def, s32 space, s32 nglyphs, s32 perRow); /* 0x800B66E8 */
extern void fn_800B6B08(void);                   /* 0x800B6B08 */
extern void* strcpy(void* dst, const void* src); /* 0x800E80D4 */
extern void* strcat(void* dst, const void* src); /* 0x800E8064 */
extern s32 stricmp(const u8* a, const u8* b);    /* 0x800C80EC */
extern void* AllocFile(char* directory, char* name); /* 0x800BF7F4 */
extern void ErrorPrintf(const char* fmt, ...);   /* 0x800BC6E0 */
extern int sprintf(char* buf, const char* fmt, ...);
extern int vsprintf(char* buf, const char* fmt, va_list ap);

/* ---- helpers ---- */

s32 FixMLineText(s32* src, s32* dst, s32 lines);
s32 DrawTextSub(f32 scale, f32 shScale, s32 x, s32 y, u32 flags, u32 color, u8* str);

static inline char* GetStringTextInline(BTextPoolView* info, s32 msg, s32 idx,
                                        volatile u32* fontOut)
{
    MsgEnt* entry = &info->stringList.msgs[msg];
    s32 fontIndex;
    s32 off;

    if (idx >= entry->count) {
        return 0;
    }
    off = info->stringList.textOff[entry->first + idx];
    fontIndex = entry->font;
    if (fontIndex >= 0) {
        *fontOut = info->stringList.fontDesc[fontIndex].color;
    }
    return (char*)(info->stringList.textData + off);
}

static inline char* GetStringTextRefsInline(BTextPoolView* info,
                                            MsgEnt** msgsRef,
                                            FontDesc** fontRef,
                                            s32 msgOffset, s32 idx,
                                            volatile u32* fontOut)
{
    MsgEnt* entry = (MsgEnt*)((u8*)*msgsRef + msgOffset);
    s32 fontIndex;
    s32 off;

    if (idx >= entry->count) {
        return 0;
    }
    off = info->stringList.textOff[entry->first + idx];
    fontIndex = entry->font;
    if (fontIndex >= 0) {
        *fontOut = (*fontRef)[fontIndex].color;
    }
    return (char*)(info->stringList.textData + off);
}

static inline char* GetScrollTextInline(StrList* p, s32 msg, s32 idx,
                                        u32* fontOut)
{
    MsgEnt* e = &p->msgs[msg];
    s32 off;

    if (idx >= e->count) {
        return 0;
    }
    off = p->textOff[e->first + idx];
    if (fontOut != 0 && e->font >= 0) {
        *fontOut = p->fontDesc[e->font].color;
    }
    return (char*)(p->textData + off);
}

static inline char* GetScrollTextRequiredFontInline(StrList* p, s32 msg,
                                                    s32 idx,
                                                    volatile u32* fontOut)
{
    MsgEnt* e = &p->msgs[msg];
    s32 off;

    if (idx >= e->count) {
        return 0;
    }
    off = p->textOff[e->first + idx];
    if (e->font >= 0) {
        *fontOut = p->fontDesc[e->font].color;
    }
    return (char*)(p->textData + off);
}

/* Force the .bss pool into address order (deadstripped by mwld). */
static void btext_bss_order(void)
{
    font_info[0] = 0;
    gTextWorkBuf[0] = 0;
    gScrollMsgList[0].arc[0] = 0;
    gStringMsgList.arc[0] = 0;
    gTextFormatBuf[0] = 0;
}

static inline char* find_newline(const char* s)
{
    while (*s != '\0') {
        if (*s == '\n') {
            return (char*)s;
        }
        s++;
    }
    return 0;
}

/* ==== 0x8001EAE0 DrawGlowTextMLines ==== */
void DrawGlowTextMLines(f32 scale, s32 x, s32 y, s32* str);
void DrawGlowText(f32 scale, s32 x, s32 y, u8* str);

void DrawGlowTextMLines(f32 scale, s32 x, s32 y, s32* str)
{
    s32 lh;
    s32 n;
    s32 i;
    s32 off;
    void* lines[16];

    lh = (s32)((f32)MBFontHeight(glow_font) * scale);
    n = FixMLineText(str, (s32*)gTextWorkBuf, (s32)lines);
    if (y < 0) {
        y = -(y + (n * lh) / 2);
    }
    off = 0;
    for (i = 0; i < n; i++) {
        DrawGlowText(scale, x, y, *(u8**)((s32)lines + off));
        y += lh;
        off += 4;
    }
    gDrawTextY = y + n * lh;
}

/* ==== 0x8001EBCC DrawGlowText (glow effect; skeleton) ==== */
void DrawGlowText(f32 scale, s32 x, s32 y, u8* str)
{
    s32 span = glow_radius * 2;
    s32 phase = pbLoad - (pbLoad / (u32)(glow_period + span)) * (glow_period + span);
    u32 prevFlags;
    u32 a;
    void* q;

    if (span < phase) {
        phase = 0;
    } else if (glow_radius < phase) {
        phase = span - phase;
    }
    MBSetFontScaleSpace(scale, scale);
    prevFlags = MBSetFontFlags(0x4000);
    MBSetFont(glow_font);
    MBSetFontColor(glow_color);
    a = (glow_radius + phase * 0xFF - 1) / glow_radius;
    MBSetFontAlpha(0x7F - (s32)a / 2);
    q = MBDrawText(x, y, str);
    *(s16*)((s32)q + 0x26) = (s16)glow_text_extra;
    MBSetFontFlags(prevFlags);
    MBSetFontColor(0xFFFFFF);
    MBSetFontAlpha(0);
    MBDrawText(x, y, str);
    MBSetFontScaleSpace(1.0f, 1.0f);
    gDrawTextY = y + (s32)((f32)MBFontHeight(glow_font) * scale);
}

/* ==== 0x8001ED24 ScrollTextNum ==== */
s32 ScrollTextNum(s32 list, s32 msg)
{
    StrList* p = &gStringMsgList;
    if (list >= 0) {
        p = &gScrollMsgList[list];
    }
    return p->msgs[msg].count;
}

/* ==== 0x8001ED58 StringTextNum ==== */
s32 StringTextNum(s32 msg)
{
    return gStringMsgList.msgs[msg].count;
}

/* ==== 0x8001ED70 ScrollTextHeight ==== */
s32 StringTextHeightSub(f32 scale, StrList* p, s32 msg, s32 idx, s32 spacing);

s32 ScrollTextHeight(f32 scale, s32 list, s32 msg, s32 idx, s32 spacing)
{
    StrList* p = &gStringMsgList;
    if (list >= 0) {
        p = &gScrollMsgList[list];
    }
    if (msg < 0) {
        msg = scroll_level_msg;
    }
    if (msg < 0) {
        return 0;
    }
    return StringTextHeightSub(scale, p, msg, idx, spacing);
}

/* ==== 0x8001EDD0 StringTextHeight ==== */
s32 StringTextHeight(f32 scale, s32 msg, s32 idx, s32 spacing)
{
    return StringTextHeightSub(scale, &gStringMsgList, msg, idx, spacing);
}

/* ==== 0x8001EE04 StringTextHeightSub ==== */
s32 StringTextHeightSub(f32 scale, StrList* p, s32 msg, s32 idx, s32 spacing)
{
    MsgEnt* e = &p->msgs[msg];
    s32 fh;
    f32 lh;
    s32 total;
    s32 line;
    s32 n;
    char* s;

    if (spacing < 0) {
        spacing = gLineSpacing;
    }
    lh = (f32)(scale * (f32)e->scale);
    fh = (s32)((f32)MBFontHeight(p->fontDesc[e->font].color) * lh);
    if (idx < 0) {
        total = 0;
        for (line = 0; line < e->count; line++) {
            s = (char*)(p->textData + p->textOff[e->first + line]);
            n = 0;
            for (;;) {
                s = find_newline(s);
                if (s == 0 || n >= 0xF) {
                    break;
                }
                s++;
                n++;
            }
            total += (n + 1) * (fh + spacing);
        }
        return total;
    }
    if (idx >= e->count) {
        return 0;
    }
    s = (char*)(p->textData + p->textOff[e->first + idx]);
    n = 0;
    for (;;) {
        s = find_newline(s);
        if (s == 0 || n >= 0xF) {
            break;
        }
        s++;
        n++;
    }
    return (n + 1) * (fh + spacing);
}

/* ==== 0x8001EFC0 ScrollTextWidth ==== */
s32 StringTextWidthSub(f32 scale, StrList* p, s32 msg, s32 idx);

s32 ScrollTextWidth(f32 scale, s32 list, s32 msg, s32 idx)
{
    StrList* p = &gStringMsgList;
    if (list >= 0) {
        p = &gScrollMsgList[list];
    }
    if (msg < 0) {
        msg = scroll_level_msg;
    }
    if (msg < 0) {
        return 0;
    }
    return StringTextWidthSub(scale, p, msg, idx);
}

/* ==== 0x8001F020 StringTextWidth ==== */
s32 StringTextWidth(f32 scale, s32 msg, s32 idx)
{
    return StringTextWidthSub(scale, &gStringMsgList, msg, idx);
}

/* ==== 0x8001F050 StringTextWidthSub ==== */
s32 StringTextWidthSub(f32 scale, StrList* p, s32 msg, s32 idx)
{
    MsgEnt* e = &p->msgs[msg];
    f32 lh = (f32)(scale * (f32)e->scale);
    u32 color = p->fontDesc[e->font].color;
    s32 maxw = 0;
    s32 line;
    s32 j;
    s32 nlines;
    s32 w;
    s32 lineMax;
    u32 prev;
    u8* ls;
    s32 off;
    void* buf1[18];
    void* buf2[20];

    if (idx < 0) {
        color &= 0xff;
        for (line = 0; line < e->count; line++) {
            lineMax = 0;
            nlines = FixMLineText((s32*)(p->textData + p->textOff[e->first + line]),
                                  (s32*)gTextWorkBuf, (s32)buf1);
            off = 0;
            for (j = 0; j < nlines; j++) {
                ls = *(u8**)((s32)buf1 + off);
                prev = MBSetFont(color);
                MBSetFontScaleSpace(lh, 0.0f);
                w = MBFontStringWidth(ls);
                if (prev != color) {
                    MBSetFont(prev);
                }
                if (lineMax < w) {
                    lineMax = w;
                }
                off += 4;
            }
            if (maxw < lineMax) {
                maxw = lineMax;
            }
        }
    } else if (idx < e->count) {
        maxw = 0;
        nlines = FixMLineText((s32*)(p->textData + p->textOff[e->first + idx]),
                              (s32*)gTextWorkBuf, (s32)buf2);
        color &= 0xff;
        off = 0;
        for (j = 0; j < nlines; j++) {
            ls = *(u8**)((s32)buf2 + off);
            prev = MBSetFont(color);
            MBSetFontScaleSpace(lh, 0.0f);
            w = MBFontStringWidth(ls);
            if (prev != color) {
                MBSetFont(prev);
            }
            if (maxw < w) {
                maxw = w;
            }
            off += 4;
        }
    } else {
        maxw = 0;
    }
    return maxw;
}

/* ==== 0x8001F234 DrawStringTextMLines ==== */
s32 DrawStringTextSub(StrList* p, s32 msg, s32 x, s32 y, s32 spacing, u32 font, u32 color);
char* GetStringTextSub(StrList* p, s32 msg, s32 idx, u32* fontOut);

s32 DrawStringTextMLines(s32 x, s32 y, s32 spacing, s32 font, u32 color, s32 msg, ...)
{
    BTextPoolView* info;
    MsgEnt* entry;
    f32 scale;
    f32 shScale;
    s32 defaultFont;
    s32 fontHeight;
    s32 msgLine;
    s32 lineCount;
    s32 line;
    s32 off;
    s32* linePtr;
    u32 text;
    va_list ap;
    s32 lines[16];
    volatile u8 unused[20];

    info = (BTextPoolView*)font_info;
    entry = &info->stringList.msgs[msg];
    msgLine = 0;
    lineCount = msgLine;
    if (spacing < 0) {
        spacing = gLineSpacing;
    }
    scale = entry->scale * DrawStringScale;
    shScale = entry->shScale;
    defaultFont = info->stringList.fontDesc[entry->font].color;
    if (font < 0 || (font < 10 && defaultFont >= 10)) {
        font = defaultFont;
    }
    fontHeight = (s32)(scale * (f32)MBFontHeight(font));
    spacing += fontHeight;
    if ((y & 0x1000) != 0) {
        y &= ~0x1000;
        y -= (fontHeight + spacing * (entry->count - 1)) / 2;
    }

    info->workBuf[0x400] = 0;
    while (lineCount < entry->count) {
        if (lineCount == 16) {
            break;
        }
        text = (u32)GetStringTextSub(&info->stringList, msg, msgLine++, 0);
        if (text == 0) {
            if (lineCount == 0) {
                ErrorPrintf("DrawStringTextMLines: Msg=%d idx=%d > max", msg, msgLine);
            }
            break;
        }
        strcat(info->workBuf + 0x400, (char*)text);
        strcat(info->workBuf + 0x400, "\n");
        lineCount++;
    }

    va_start(ap, msg);
    vsprintf((char*)info->workBuf, (char*)info->workBuf + 0x400, ap);
    lineCount = FixMLineText((s32*)info->workBuf, (s32*)info->formatBuf, (s32)lines);
    line = 0;
    off = line;
    linePtr = (s32*)(u32)lines;
    while (line < lineCount) {
        DrawTextSub(scale, shScale, x, y, font, color,
                    *(u8**)((s32)linePtr + off));
        y += spacing;
        line++;
        off += 4;
    }
    gDrawTextY = y;
    return y;
}

/* ==== 0x8001F48C DrawStringTextMulti ==== */
s32 DrawStringTextMulti(s32 x, s32 y, s32 spacing, s32 font, u32 color, s32 msg)
{
    BTextPoolView* info = (BTextPoolView*)font_info;
    MsgEnt** msgsRef = &info->stringList.msgs;
    FontDesc** fontRef;
    s32 msgOffset = msg * sizeof(MsgEnt);
    MsgEnt* entry = (MsgEnt*)((u8*)*msgsRef + msgOffset);
    volatile u32 defaultFont;
    f32 scale;
    f32 shadowScale;
    s32 lineHeight;
    s32 lineStep;
    s32 idx;
    char* text;

    if (spacing < 0) {
        spacing = gLineSpacing;
    }
    scale = entry->scale * DrawStringScale;
    shadowScale = entry->shScale;
    defaultFont = (*(fontRef = &info->stringList.fontDesc))[entry->font].color;
    if (font < 0 || (font < 10 && (s32)defaultFont >= 10)) {
        font = defaultFont;
    }

    lineHeight = (s32)(scale * (f32)MBFontHeight(font));
    lineStep = spacing + lineHeight;
    if (y & 0x1000) {
        y &= ~0x1000;
        y -= (lineHeight + lineStep * (entry->count - 1)) / 2;
    }

    idx = 0;
    for (;;) {
        text = GetStringTextRefsInline(info, msgsRef, fontRef, msgOffset, idx,
                                       &defaultFont);
        if (text == NULL) {
            if (idx == 0) {
                ErrorPrintf(sDrawStringTextMultiRangeError, msg, idx);
            }
            break;
        }
        DrawTextSub(scale, shadowScale, x, y, font, color, (u8*)text);
        y += lineStep;
        idx++;
    }
    {
        u8 unused[16];
        gDrawTextY = y;
        return y;
    }
}

/* ==== 0x8001F660 ScrollTextListNum ==== */
s32 ScrollTextListNum(s32 list, s32 sub)
{
    return gScrollMsgList[list].lists[sub].count;
}

/* ==== 0x8001F680 GetScrollListMsg ==== */
s32 GetScrollListMsg(s32 list, s32 sub, s32 idx)
{
    StrList* p = &gStringMsgList;
    ListEnt* l;
    s32 msg;

    if (list >= 0) {
        p = &gScrollMsgList[list];
    }
    l = &p->lists[sub];
    msg = (idx >= l->count) ? -1 : p->listOff[l->first + idx];
    return gScrollMsgList[list].msgs[msg].count;
}

/* ==== 0x8001F6EC DrawScrollListText ==== */
s32 DrawScrollText(s32 list, s32 x, s32 y, s32 spacing, s32 font,
                   u32 color, s32 msg, s32 idx);

void DrawScrollListText(s32 list, s32 x, s32 y, s32 spacing, s32 font,
                        u32 color, s32 subList, s32 idx, s32 msgIdx)
{
    BTextPoolView* info = (BTextPoolView*)font_info;
    StrList* p = &info->stringList;
    ListEnt* sub;
    s32 msg;
    s32 resolved;

    if (list >= 0) {
        p = &info->scrollLists[list];
    }
    sub = &p->lists[subList];
    if (idx >= sub->count) {
        resolved = -1;
    } else {
        resolved = p->listOff[sub->first + idx];
    }
    msg = resolved;
    if (y < 0) {
        StrList* heightList;

        heightList = &((StrList*)info)[list];
        y = -(y + StringTextHeightSub(DrawStringScale,
                                      (StrList*)((u8*)heightList + 0x838),
                                      msg, msgIdx,
                                      spacing) / 2);
    }
    {
        u8 unused[16];
        DrawScrollText(list, x, y, spacing, font, color, msg, msgIdx);
    }
}

/* ==== 0x8001F7DC DrawScrollText ==== */
s32 DrawScrollText(s32 list, s32 x, s32 y, s32 spacing, s32 font,
                   u32 color, s32 msg, s32 idx)
{
    BTextPoolView* info = (BTextPoolView*)font_info;
    StrList* drawList = &info->stringList;
    StrList* textList;
    char* text;
    volatile u32 defaultFont;
    s32 fontIndex;

    if (list >= 0) {
        drawList = &((StrList*)info)[list];
        drawList = (StrList*)((u8*)drawList + 0x838);
    }
    if (msg < 0) {
        msg = scroll_level_msg;
    }
    if (msg < 0) {
        return 0;
    }

    textList = &info->stringList;
    if (list >= 0) {
        textList = &((StrList*)info)[list];
        textList = (StrList*)((u8*)textList + 0x838);
    }
    text = GetScrollTextRequiredFontInline(textList, msg, idx, &defaultFont);
    if (text == NULL) {
        ErrorPrintf(sDrawScrollTextRangeError, msg, idx);
        return 0;
    }
    if (font < 0 || (font < 10 && (s32)defaultFont >= 10)) {
        font = defaultFont;
    }
    strcpy(gTextFormatBuf, text);
    {
        u8 unused[4];
        return DrawStringTextSub(drawList, msg, x, y, spacing, font, color);
    }
}

/* ==== 0x8001F93C DrawStringText ==== */
s32 DrawStringText(s32 x, s32 y, u32 flags, u32 color, s32 msg, s32 idx, ...)
{
    StrList* list;
    char* text;
    volatile u32 font;
    va_list ap;
    volatile u32 unused;

    list = &((BTextPoolView*)font_info)->stringList;
    text = GetStringTextInline((BTextPoolView*)font_info, msg, idx, &font);
    if (text == 0) {
        ErrorPrintf("DrawStringText: Msg %d idx %d > msg count\n", msg, idx);
        return 0;
    }
    if ((s32)flags < 0 || ((s32)flags < 10 && (s32)font >= 10)) {
        flags = font;
    }
    va_start(ap, idx);
    vsprintf((char*)gTextFormatBuf, text, ap);
    return DrawStringTextSub(list, msg, x, y, -1, flags, color);
}

/* ==== 0x8001FAB0 DrawStringTextSub - draw a multi-line message ==== */
s32 DrawStringTextSub(StrList* p, s32 msg, s32 x, s32 y, s32 spacing, u32 font, u32 color)
{
    u32 ret;
    u8* e;
    f32 sx;
    f32 sh;
    s32 n;
    s32 i;
    s32 off;
    s32 lines[18];
    s32* lp;

    ret = 0;
    if (spacing < 0) {
        spacing = gLineSpacing;
    }
    e = (u8*)p->msgs + msg * 20;
    sx = DrawStringScale * *(f32*)(e + 12);
    sh = *(f32*)(e + 16);
    spacing += (s32)((f32)MBFontHeight(font) * sx);
    n = FixMLineText((s32*)gTextFormatBuf, (s32*)gTextWorkBuf, (s32)lines);
    lp = (s32*)(u32)lines;
    i = 0;
    off = 0;
    for (; i < n; i++, off += 4) {
        u32 r = DrawTextSub(sx, sh, x, y, font, color,
                            *(u8**)((u8*)lp + off));
        if (ret == 0) {
            ret = r;
        }
        y += spacing;
    }
    gDrawTextY = y;
    return ret;
}

/* ==== 0x8001FBCC RestoreDrawStringScale ==== */
f32 RestoreDrawStringScale(void)
{
    DrawStringScale = OldStringScale;
    return DrawStringScale;
}

/* ==== 0x8001FBDC SetDrawStringScale ==== */
f32 SetDrawStringScale(f32 scale)
{
    OldStringScale = DrawStringScale;
    DrawStringScale = scale;
    return OldStringScale;
}

/* ==== 0x8001FBF0 SetScrollLevelMsgList ==== */
s32 FindStringMessageSub(StrList* p, const u8* name);

void SetScrollLevelMsgList(s32 level, const char* suffix)
{
    u8 buf[32];
    sprintf((char*)buf, lbl_801119C4, suffix);
    scroll_level_msg = FindStringMessageSub(&gScrollMsgList[level], buf);
}

/* ==== 0x8001FC4C FindStringMessageListSub_8001FC4C ==== */
s32 FindStringMessageListSub_8001FC4C(s32 list, const u8* name)
{
    s32 i;
    s32 off;

    for (i = 0, off = 0; i < gScrollMsgList[list].nName; i++, off += 4) {
        if (stricmp(gScrollMsgList[list].nameData +
                    *(s32*)((u8*)gScrollMsgList[list].nameOff + off), name) == 0) {
            break;
        }
    }
    if (i >= gScrollMsgList[list].nName) {
        i = -1;
    }
    return i;
}

/* ==== 0x8001FCE4 FindStringMessageSub ==== */
s32 FindStringMessageSub(StrList* p, const u8* name)
{
    s32 off;
    s32 i;
    s32 nameOffset;

    for (i = 0, off = 0; i < p->nName; i++, off += 4) {
        nameOffset = *(s32*)((u8*)p->nameOff + off);
        if (stricmp(p->nameData + nameOffset, name) == 0) {
            break;
        }
    }
    if (i >= p->nName) {
        i = -1;
    }
    return i;
}

/* ==== 0x8001FD64 GetScrollScale ==== */
f32 GetScrollScale(s32 list, s32 msg)
{
    StrList* p = &gStringMsgList;
    if (list >= 0) {
        p = &gScrollMsgList[list];
    }
    return ((f32*)p->msgs)[msg * 5 + 3];
}

/* ==== 0x8001FD9C GetStringListText ==== */
static inline char* GetStringListTextInline(BTextPoolView* info, s32 msg,
                                             s32 idx, u32* fontOut)
{
    MsgEnt* e = &info->stringList.msgs[msg];
    s32 off;

    if (idx >= e->count) {
        return 0;
    }
    off = info->stringList.textOff[e->first + idx];
    if (fontOut != 0 && e->font >= 0) {
        *fontOut = info->stringList.fontDesc[e->font].color;
    }
    return (char*)(info->stringList.textData + off);
}

char* GetStringListText(s32 li, s32 sub, s32 idx, u32* fontOut)
{
    BTextPoolView* info = (BTextPoolView*)font_info;
    ListEnt* l = &info->stringList.lists[li];
    s32 msg = (sub >= l->count) ? -1 : info->stringList.listOff[l->first + sub];

    if (msg < 0) {
        return 0;
    }
    return GetStringListTextInline(info, msg, idx, fontOut);
}

/* ==== 0x8001FE50 GetStringListMsg ==== */
s32 GetStringListMsg(s32 li, s32 sub)
{
    ListEnt* l = &gStringMsgList.lists[li];

    if (sub >= l->count) {
        return -1;
    }
    return gStringMsgList.listOff[l->first + sub];
}

/* ==== 0x8001FE90 GetScrollText ==== */
char* GetScrollText(s32 list, s32 msg, s32 idx, u32* fontOut)
{
    StrList* p = &gStringMsgList;

    if (list >= 0) {
        p = &gScrollMsgList[list];
    }
    return GetScrollTextInline(p, msg, idx, fontOut);
}

/* ==== 0x8001FF1C GetStringText ==== */
char* GetStringText(s32 msg, s32 idx, u32* fontOut)
{
    BTextPoolView* info = (BTextPoolView*)font_info;
    MsgEnt* e = &info->stringList.msgs[msg];
    s32 off;

    if (idx >= e->count) {
        return 0;
    }
    off = info->stringList.textOff[e->first + idx];
    if (fontOut != 0 && e->font >= 0) {
        *fontOut = info->stringList.fontDesc[e->font].color;
    }
    return (char*)(info->stringList.textData + off);
}

/* ==== 0x8001FF8C GetStringTextSub ==== */
char* GetStringTextSub(StrList* p, s32 msg, s32 idx, u32* fontOut)
{
    MsgEnt* e = &p->msgs[msg];
    s32 off;

    if (idx >= e->count) {
        return 0;
    }
    off = p->textOff[e->first + idx];
    if (fontOut != 0 && e->font >= 0) {
        *fontOut = p->fontDesc[e->font].color;
    }
    return (char*)(p->textData + off);
}

/* ==== 0x8001FFF4 StringInitSub (SCROLLS loader; skeleton) ==== */
void* StringInitSub(s32 mode, StrList* p)
{
    (void)mode;
    return p;
}

/* ==== 0x80020764 TextHeightMLines ==== */
s32 TextHeightMLines(f32 scale, s32 font, char* str)
{
    s32 fh = MBFontHeight(font);
    s32 n = 0;

    for (;;) {
        str = find_newline(str);
        if (str == 0 || n >= 0xF) {
            break;
        }
        str++;
        n++;
    }
    return (n + 1) * (s32)((f32)(s32)fh * scale);
}

/* ==== 0x8002081C FontHeight ==== */
s32 FontHeight(f32 scale, s32 font)
{
    return (s32)((f32)MBFontHeight(font) * scale);
}

/* ==== 0x80020874 DrawNormalText ==== */
s32 DrawNormalText(f32 scale, u8* str, s32 color)
{
    s32 prev = MBSetFont(color & 0xff);
    s32 w;

    MBSetFontScaleSpace(scale, 0.0f);
    w = MBFontStringWidth(str);
    if (prev != (color & 0xff)) {
        MBSetFont(prev);
    }
    return w;
}

/* ==== 0x800208E4 DrawTextMLines ==== */

s32 DrawTextMLines(f32 scale, s32 x, s32 y, u32 font, u32 color, s32* str)
{
    s32 lh = (s32)((f32)MBFontHeight(font) * scale);
    s32 n;
    s32 i;
    s32 off;
    void* lines[17];

    n = FixMLineText(str, (s32*)gTextFormatBuf, (s32)lines);
    off = 0;
    for (i = 0; i < n; i++) {
        DrawTextSub(scale, 1.0f, x, y, font, color, *(u8**)((s32)lines + off));
        y += lh;
        off += 4;
    }
    return n * lh;
}

/* ==== 0x800209BC DrawTextKeepScale ==== */
void DrawTextKeepScale(f32 scale, s32 x, s32 y, u32 font, u32 color, u8* str)
{
    DrawTextSub(scale, 1.0f, x, y, font, color, str);
}

/* ==== 0x800209E0 DrawText (variadic; skeleton) ==== */
void DrawText(s32 x, s32 y, u32 flags, u32 color, const char* fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vsprintf((char*)gTextFormatBuf, fmt, ap);
    DrawTextSub(1.0f, 1.0f, x, y, flags, color, gTextFormatBuf);
}

/* ==== 0x80020AAC DrawTextSub (core) ==== */
s32 DrawTextSub(register f32 scale, f32 shScale, s32 x, s32 y, u32 flags,
                u32 color, u8* str)
{
    register f32 fontHeight;
    f64 shadowProduct;
    f32 shadowSpace;
    s32 font;
    s32 oldFlags;
    s32 height;
    s32 result;

    font = flags & 0xFF;
    oldFlags = 0;
    if (y > 0 && (y & 0x1000)) {
        y &= ~0x1000;
        fontHeight = (f32)MBFontHeight(flags);
        asm { fmuls fontHeight, fontHeight, scale }
        height = (s32)fontHeight;
        y -= height / 2;
    }

    if ((f64)shScale > sBTextZero) {
        shScale *= scale;
    } else if ((f64)shScale < sBTextZero) {
        shScale = -shScale;
    } else {
        shScale = sBTextOne;
    }

    MBSetFont(font);
    font = flags & 0x200;
    if (font != 0) {
        oldFlags = MBSetFontFlags(0);
        MBSetFontFlags(oldFlags | 0x100);
    }
    if (flags & 0x100) {
        MBSetFontColor(shadow_color);
        shadowProduct = sBTextShadowSpacing * scale;
        shadowSpace = (f32)shadowProduct;
        MBSetFontScaleSpace(shadowSpace, shadowSpace);
        MBSetFontScale(shScale, scale);
        MBDrawText(x - 2, y - 2, str);
    }

    MBSetFontColor(color);
    MBSetFontScaleSpace(scale, scale);
    MBSetFontScale(shScale, scale);
    result = (s32)MBDrawText(x, y, str);
    if (font != 0) {
        MBSetFontFlags(oldFlags);
    }
    if (sBTextOneDouble != (f64)shScale) {
        MBSetFontScale(sBTextOne, sBTextOne);
    }
    {
        u8 unused[8];
        return result;
    }
}

/* ==== 0x80020C3C TextMLines ==== */
s32 TextMLines(const char* s)
{
    s32 n = 0;

    for (;;) {
        s = find_newline(s);
        if (s == 0 || n >= 0xF) {
            break;
        }
        s++;
        n++;
    }
    return n + 1;
}

/* ==== 0x80020C8C FixMLineText ==== */
s32 FixMLineText(s32* src, s32* dst, s32 lines)
{
    s32* p = src;
    s32 n = 0;
    s32 off = 0;

    if (dst != 0) {
        strcpy(dst, src);
        p = dst;
    }
    for (;;) {
        if (lines != 0) {
            *(s32**)(lines + off) = p;
        }
        for (; *(char*)p != '\0'; p = (s32*)((s32)p + 1)) {
            if (*(char*)p == '\n') {
                goto found;
            }
        }
        p = 0;
found:
        if (p == 0) {
            return n + 1;
        }
        if (dst != 0) {
            *(char*)p = '\0';
        }
        if (n >= 0xF) {
            return n + 1;
        }
        p = (s32*)((s32)p + 1);
        n++;
        off += 4;
    }
}

/* ==== 0x80020D3C FontSetShadowColor ==== */
void FontSetShadowColor(s32 color)
{
    shadow_color = color;
}

/* ==== 0x80020D44 FontInitSpecial (variadic forwarder; skeleton) ==== */
void LoadFonts(s32 mode, char* suffix, s32 space);

void FontInitSpecial(void* def, s32 space)
{
    LoadFonts(0xd, def, space);
}

/* ==== 0x80020D70 FontInitDefault (skeleton) ==== */
void FontInitDefault(void)
{
    LoadFonts(0, gFontDefs8x8[0], gFontDefs[0]);
}

/* ==== 0x80020DA8 FontInit (skeleton) ==== */
void FontInit(void)
{
    s32 i;

    StringInitSub(0, &gStringMsgList);
    for (i = 0; i < 2; i++) {
        StringInitSub(gScrollModes[i], &gScrollMsgList[i]);
    }
    for (i = 1; i < 0xd; i++) {
        LoadFonts(i, gFontDefs8x8[i], gFontDefs[i]);
    }
    gFontsInited = 1;
}

/* ==== 0x80020E5C FontEndFrame ==== */
void FontEndFrame(void)
{
    fn_800B6B08();
    shadow_color = 0;
}

/* ==== 0x80020E84 LoadFonts (core loader) ==== */
void LoadFonts(s32 mode, char* suffix, s32 space)
{
    char name[132];
    u8* font;
    s32 loaded;
    s32 glyphCount;
    s32 glyphsPerRow;
    s32 offset;
    s32 i;

    sprintf(name, sFontFileFormat, suffix);
    if (mode == 0) {
        loaded = (s32)gDefaultFontData;
    } else {
        loaded = (s32)AllocFile(sFontDirectory, name);
    }
    font = (u8*)loaded;
    offset = loaded + 12;

#define SWAP_FONT_WORD(value)                                                \
    do {                                                                     \
        u8 _swap[8];                                                         \
        *(u32*)_swap = (value);                                              \
        _swap[4] = _swap[3];                                                 \
        _swap[5] = _swap[2];                                                 \
        _swap[6] = _swap[1];                                                 \
        _swap[7] = _swap[0];                                                 \
        (value) = *(u32*)(_swap + 4);                                        \
    } while (0)

    SWAP_FONT_WORD(*(u32*)(font + 4));
    SWAP_FONT_WORD(*(u32*)(font + 8));
    *(char**)font = suffix;
    *(s32*)(font + 8) = offset;

    if (mode >= 10 && mode <= 11) {
        glyphCount = 256;
        glyphsPerRow = 210;
    } else if (mode == 12) {
        glyphCount = 256;
        glyphsPerRow = 100;
    } else {
        glyphCount = 128;
        glyphsPerRow = 128;
    }

    offset = 0;
    for (i = 0; i < glyphCount; i++) {
        s32* glyph = (s32*)(*(u8**)(font + 8) + offset);

        if (glyph[0] == 0) {
            break;
        }
        offset += 16;
        SWAP_FONT_WORD(glyph[0]);
        SWAP_FONT_WORD(glyph[1]);
        SWAP_FONT_WORD(glyph[2]);
        SWAP_FONT_WORD(glyph[3]);
    }
#undef SWAP_FONT_WORD

    {
        u8 unused[48];
        MBNewFont(font, space, glyphCount, glyphsPerRow);
        font_info[mode] = font;
    }
}
