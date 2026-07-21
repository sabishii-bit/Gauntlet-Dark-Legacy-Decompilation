#include "types.h"

volatile u16 __VIRegs[] : 0xCC002000;

static u16 FontEncode = 0xFFFF;

u16 OSGetFontEncode(void) {
    if (FontEncode <= 1) {
        return FontEncode;
    }

    switch (*(s32*)0x800000CC) {
    case 0: /* VI_NTSC */
        FontEncode = (u16)((__VIRegs[55] & 0x0002) ? 1 : 0);
        break;
    case 1: /* VI_PAL */
    case 2: /* VI_MPAL */
    case 3: /* VI_DEBUG */
    case 4: /* VI_DEBUG_PAL */
    case 5: /* VI_EURGB60 */
    default:
        FontEncode = 0;
        break;
    }

    return FontEncode;
}
