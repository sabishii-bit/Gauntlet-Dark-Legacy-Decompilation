#include "types.h"

/* BTEXT.OBJ - bitmap/box text + SCROLLS message system (GameCube port).
 *
 * Text drawing on top of the G3D box-text primitives (0x800B5AD8..0x800B6B08)
 * plus the "SCROLLS<level>" message-resource loader (chunk tags
 * FONT/TEXT/TOFF/STRS/LOFF/LIST/DEFS/SDEF/LDEF).  Function names come from the
 * Xbox shell3D.pdb BTEXT.OBJ roster, mapped to PPC by strings / call-graph
 * (the PPC emit order differs from the Xbox source order, so mapping is by
 * behaviour, not position).  A handful of names are best-effort.
 *
 * Status: NonMatching.  All symbols are mapped; the leaf accessors/setters
 * match byte-for-byte.  The variadic Draw* wrappers, the big loader
 * (StringInitSub), and the whole-TU .sdata2 constant-pool ordering are future
 * work before the unit links green.
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

/* ---- TU data ----
 * Declared extern for now: this unit is NonMatching, so its own .sdata/.sbss
 * would only shift sections at link time.  When the whole TU is completed the
 * initialised globals move here (DrawStringScale=1.0, OldStringScale=1.0,
 * scroll_level_msg=-1) and the .sdata/.sbss/.sdata2 pools get claimed. */

extern f32 DrawStringScale; /* .sdata 0x80343BBC = 1.0 */
extern f32 OldStringScale;  /* .sdata 0x80343BC0 = 1.0 */
extern s32 scroll_level_msg;/* .sdata 0x80343BB8 = -1 */

extern s32 shadow_color;    /* .sbss 0x803443D8 */
extern s32 gDrawTextY;      /* .sbss 0x803443DC */
extern s32 gLineSpacing;    /* .sbss 0x803443E0 */

extern StrList gStringMsgList;    /* .bss 0x8023F3A0 - default list */
extern StrList gScrollMsgList[2]; /* .bss 0x8023F318 */

/* ---- external primitives ---- */

extern s32 fn_800B5AD8(s32 font); /* font pixel metric */
extern void fn_800B6B08(void);

/* ---- helpers ---- */

/* Select the default list, or scroll list `list` when >= 0. */
static StrList* PickList(s32 list)
{
    StrList* p = &gStringMsgList;
    if (list >= 0) {
        p = &gScrollMsgList[list];
    }
    return p;
}

/* Number of sub-lines in a scroll-list message. */
s32 ScrollTextNum(s32 list, s32 msg)
{
    StrList* p = PickList(list);
    return p->msgs[msg].count;
}

/* Number of sub-lines in a default-list message. */
s32 StringTextNum(s32 msg)
{
    return gStringMsgList.msgs[msg].count;
}

/* Number of messages in a scroll sub-list. */
s32 ScrollTextListNum(s32 list, s32 sub)
{
    return gScrollMsgList[list].lists[sub].count;
}

/* Resolve a nested-list entry to its message-line count. */
s32 GetScrollListMsg(s32 list, s32 sub, s32 idx)
{
    StrList* p = PickList(list);
    ListEnt* l = &p->lists[sub];
    s32 msg;

    if (idx < l->count) {
        msg = p->listOff[l->first + idx];
    } else {
        msg = -1;
    }
    return gScrollMsgList[list].msgs[msg].count;
}

/* Restore the text scale saved by SetDrawStringScale. */
f32 RestoreDrawStringScale(void)
{
    DrawStringScale = OldStringScale;
    return DrawStringScale;
}

/* Set the current text scale, returning the previous value. */
f32 SetDrawStringScale(f32 scale)
{
    OldStringScale = DrawStringScale;
    DrawStringScale = scale;
    return OldStringScale;
}

/* Per-message scale factor from a scroll list. */
f32 GetScrollScale(s32 list, s32 msg)
{
    StrList* p = PickList(list);
    return p->msgs[msg].scale;
}

/* Resolve a nested-list entry to its text pointer (default list). */
char* GetStringListText(s32 li, s32 sub, s32 idx, u32* fontOut)
{
    ListEnt* l = &gStringMsgList.lists[li];
    MsgEnt* e;
    s32 msg;
    s32 off;

    if (sub < l->count) {
        msg = gStringMsgList.listOff[l->first + sub];
    } else {
        msg = -1;
    }
    if (msg < 0) {
        return 0;
    }
    e = &gStringMsgList.msgs[msg];
    if (idx >= e->count) {
        return 0;
    }
    off = gStringMsgList.textOff[e->first + idx];
    if (fontOut != 0 && e->font >= 0) {
        *fontOut = gStringMsgList.fontDesc[e->font].color;
    }
    return (char*)(gStringMsgList.textData + off);
}

/* Resolve a nested-list entry to its message number (default list). */
s32 GetStringListMsg(s32 li, s32 sub)
{
    ListEnt* l = &gStringMsgList.lists[li];

    if (sub >= l->count) {
        return -1;
    }
    return gStringMsgList.listOff[l->first + sub];
}

/* Text pointer for a scroll-list message line (+ optional font out). */
char* GetScrollText(s32 list, s32 msg, s32 idx, u32* fontOut)
{
    StrList* p = PickList(list);
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

/* Text pointer for a default-list message line (+ optional font out). */
char* GetStringText(s32 msg, s32 idx, u32* fontOut)
{
    MsgEnt* e = &gStringMsgList.msgs[msg];
    s32 off;

    if (idx >= e->count) {
        return 0;
    }
    off = gStringMsgList.textOff[e->first + idx];
    if (fontOut != 0 && e->font >= 0) {
        *fontOut = gStringMsgList.fontDesc[e->font].color;
    }
    return (char*)(gStringMsgList.textData + off);
}

/* Text pointer for a message line of an explicit list (+ optional font out). */
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

/* Count newline-separated lines in a string (non-destructive, capped at 16). */
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

/* Set the drop-shadow colour used by the box-text primitives. */
void FontSetShadowColor(s32 color)
{
    shadow_color = color;
}

/* Reset per-frame text state. */
void FontEndFrame(void)
{
    fn_800B6B08();
    shadow_color = 0;
}
