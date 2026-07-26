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
 * Status: NonMatching. The current-font setters and the lock/unlock stack are
 * reconstructed bodies; the rasteriser / drawtext / font-registration giants and
 * the reset/init cluster are documented stubs (call/flow shape only). Several
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

extern s32 lbl_8029F474[8];   /* saved message_count per lock level */
extern s32 lbl_802A4A84[8];   /* saved textbuf_count per lock level */
extern s32 lbl_8029E454[];    /* saved font_count per font-lock level */
extern void* lbl_802A4AA4[];  /* fonts[] : per-font descriptor pointers */

/* per-drawtext message record; +0x28 holds the packed colour */
typedef struct MBTextMsg {
    u8 _pad[0x28];
    u32 color;    /* 0x28 */
} MBTextMsg;

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
extern void  mbBlitSetupVerts();
extern void  mbBlitCalcClip();
extern void  mbBlitGetPage();
extern void  mbBlitSetPage();
extern void  DrawBlit();
extern s32   mbBlitCalcX();

/* forward decls for internal helpers */
static void* fn_800B5B00(void);

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
    lbl_80344E54 = old & 0x00FFFFFF;
    lbl_80344E54 = lbl_80344E54 | ((128 - (alpha >> 1)) << 24);
    return (128 - (old >> 24)) << 1;
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
    int ret = (old >> 23) & 0x1FE;
    m->color = old & 0x00FFFFFF;
    m->color = m->color | ((128 - (alpha >> 1)) << 24);
    return ret;
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

/* 0x800B5AD8 - resolve a font index (<0 = current), return its descriptor */
void* mbFontFromIndex(int idx)
{
    if (idx < 0) idx = lbl_80344E14;
    return ((void**)lbl_802A4AA4[idx])[0];
}

/* 0x800B5B00 - build/queue a glyph string blit (called by MBDrawText/SysText).
 * NonMatching stub: shape only. */
static void* fn_800B5B00(void)
{
    mbBlitCalcX();
    return (void*)0;
}

/* ==== drawtext queue ==== */

/* 0x800B6444 - MBDrawSysText : queue a system-font string. NonMatching stub. */
void MBDrawSysText(const char* s)
{
    strlen(s);
    fn_800B5B00();
    strcpy((char*)0, s);
}

/* 0x800B6588 - MBDrawText : queue a string, bounds-check the message/char
 * buffers ("TOO MANY DRAWTEXT ..."). NonMatching stub. */
void MBDrawText(const char* s)
{
    if (lbl_80344E18 >= 0) ErrorPrintf("TOO MANY DRAWTEXT MESSAGES: %d", lbl_80344E18);
    if (lbl_80344E20 >= 0) ErrorPrintf("TOO MANY DRAWTEXT CHARACTERS: %d", lbl_80344E20);
    strlen(s);
    fn_800B5B00();
    strcpy((char*)0, s);
}

/* ==== rasteriser ==== */

/* 0x800B5DEC - MBRenderText : rasterise queued messages through the pb blit
 * pipeline. NonMatching stub: documented call shape only. */
void MBRenderText(void)
{
    mbBlitGetPage();
    mbInitBlitEntry();
    mbBlitProject();
    mbBlitSetupVerts();
    mbBlitCalcClip();
    DrawBlit();
    mbBlitSetPage();
}

/* ==== font registration ==== */

/* 0x800B66E8 - MBNewFont : register a font (MBCreateBlit); "Too many fonts" /
 * "MBNewFont: MBNewBlit failed". NonMatching stub. */
int MBNewFont(void)
{
    if (lbl_80344E10 >= 3) {
        FatalError("Too many fonts", 0);
        return -1;
    }
    if (MBCreateBlit() == (void*)0) {
        FatalError("MBNewFont: MBNewBlit failed", 0);
    }
    AllocMem();
    return lbl_80344E10;
}

/* ==== reset / init / query cluster (NonMatching stubs) ==== */

/* 0x800B5D90 - MBHideMarkedMessages (tentative) */
void fn_800B5D90(void)
{
}

/* 0x800B69DC - MBInitFonts (tentative) */
void fn_800B69DC(void)
{
}

/* 0x800B6B08 - MBResetFontData (tentative) */
void fn_800B6B08(void)
{
}

/* 0x800B6B80 - MB text query (tentative) */
void fn_800B6B80(void)
{
}

/* 0x800B6BC0 - MBResetFonts (tentative) */
void fn_800B6BC0(void)
{
}

/* 0x800B6C20 - MBResetUnlockedFonts (tentative) */
void fn_800B6C20(void)
{
}

/* 0x800B6C94 - MBFontUpdateWindow (tentative) */
void fn_800B6C94(void)
{
}
