#include "types.h"
#include "dolphin/si.h"

int OSDisableInterrupts(void);
void OSRestoreInterrupts(int level);
void OSReport(const char* msg, ...);
u32 VIGetTvFormat(void);

volatile u16 __VIRegs[] : 0xCC002000;

#define VI_NTSC 0
#define VI_PAL 1
#define VI_MPAL 2
#define VI_EURGB60 5

static u32 SamplingRate = 0;

typedef struct XY {
    /* 0x0 */ u16 line;
    /* 0x2 */ u8 count;
} XY;

static XY XYNTSC[12] = {
    { 0x00F6, 0x02 }, { 0x000F, 0x12 }, { 0x001E, 0x09 }, { 0x002C, 0x06 },
    { 0x0034, 0x05 }, { 0x0041, 0x04 }, { 0x0057, 0x03 }, { 0x0057, 0x03 },
    { 0x0057, 0x03 }, { 0x0083, 0x02 }, { 0x0083, 0x02 }, { 0x0083, 0x02 },
};

static XY XYPAL[12] = {
    { 0x0128, 0x02 }, { 0x000F, 0x15 }, { 0x001D, 0x0B }, { 0x002D, 0x07 },
    { 0x0034, 0x06 }, { 0x003F, 0x05 }, { 0x004E, 0x04 }, { 0x0068, 0x03 },
    { 0x0068, 0x03 }, { 0x0068, 0x03 }, { 0x0068, 0x03 }, { 0x009C, 0x02 },
};

void SISetSamplingRate(u32 msec) {
    u32 tv;
    XY* xy;
    s32 x2;
    BOOL enabled;

    if (msec > 11) {
        msec = 11;
    }

    enabled = OSDisableInterrupts();
    SamplingRate = msec;

    tv = VIGetTvFormat();
    switch (tv) {
    case VI_NTSC:
    case VI_MPAL:
    case VI_EURGB60:
        xy = XYNTSC;
        break;
    case VI_PAL:
        xy = XYPAL;
        break;
    default:
        OSReport("SISetSamplingRate: unknown TV format. Use default.");
        msec = 0;
        xy = XYNTSC;
        break;
    }

    if (__VIRegs[54] & 1) {
        x2 = 2;
    } else {
        x2 = 1;
    }

    SISetXY(x2 * xy[msec].line, xy[msec].count);
    OSRestoreInterrupts(enabled);
}

void SIRefreshSamplingRate(void) {
    SISetSamplingRate(SamplingRate);
}
