#ifndef GAME_MB_FONT_H
#define GAME_MB_FONT_H

#include "types.h"

/* Queued text message: MBDrawText initializes 44-byte records and returns
 * their address. Shared with text/HUD consumers; not a sprite blit record.
 * Xbox FontMessage corroborates the layout, including its AltTex at +0x26.
 * Keep the established GC field names used by mb_font.c and gamemain.c. */
typedef struct MBTextMsg {
    u32 flags;    /* 0x00: 0x02000000 marks the record; bit 0 hides it */
    s32 x;        /* 0x04 */
    s32 y;        /* 0x08 */
    f32 z;        /* 0x0C */
    char* text;   /* 0x10: copied text in the message character buffer */
    f32 xspace;   /* 0x14 */
    f32 xscale;   /* 0x18 */
    f32 yspace;   /* 0x1C */
    f32 yscale;   /* 0x20 */
    s16 font;     /* 0x24 */
    s16 seq;      /* 0x26: alternate texture index; -1 uses the font */
    u32 color;    /* 0x28 */
} MBTextMsg;

#endif
