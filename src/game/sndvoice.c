#include "types.h"
#include "dolphin/ax.h"

int OSDisableInterrupts(void);
void OSRestoreInterrupts(int level);
void AISetStreamVolLeft(u8 vol);
void AISetStreamVolRight(u8 vol);

/* GameCube platform voice mixer (Midway) - sits over AX. No Xbox counterpart
 * module (Xbox uses DirectSound); names here are provisional. */

typedef struct SNDVOICE {
    /* 0x00 */ AXVPB* voice;
    /* 0x04 */ u32 flags;
    /* 0x08 */ s32 vol;
    /* 0x0C */ s32 auxA;
    /* 0x10 */ s32 auxB;
    /* 0x14 */ s32 volIdx;
    /* 0x18 */ s32 panIdx;
    /* 0x1C */ s32 master;
    /* 0x20 */ s32 volL;
    /* 0x24 */ s32 volR;
    /* 0x28 */ s32 panL;
    /* 0x2C */ s32 panR;
    /* 0x30 */ u16 mix[20];
} SNDVOICE;

static u16 sndDbTable[966] = {
    0x0000, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001,
    0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001,
    0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001,
    0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001,
    0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001,
    0x0001, 0x0001, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002,
    0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002,
    0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002,
    0x0002, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003,
    0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003,
    0x0003, 0x0003, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004,
    0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0005, 0x0005, 0x0005,
    0x0005, 0x0005, 0x0005, 0x0005, 0x0005, 0x0005, 0x0005, 0x0005, 0x0005, 0x0005, 0x0005, 0x0005,
    0x0005, 0x0006, 0x0006, 0x0006, 0x0006, 0x0006, 0x0006, 0x0006, 0x0006, 0x0006, 0x0006, 0x0006,
    0x0006, 0x0006, 0x0007, 0x0007, 0x0007, 0x0007, 0x0007, 0x0007, 0x0007, 0x0007, 0x0007, 0x0007,
    0x0007, 0x0007, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008,
    0x0009, 0x0009, 0x0009, 0x0009, 0x0009, 0x0009, 0x0009, 0x0009, 0x0009, 0x000A, 0x000A, 0x000A,
    0x000A, 0x000A, 0x000A, 0x000A, 0x000A, 0x000A, 0x000B, 0x000B, 0x000B, 0x000B, 0x000B, 0x000B,
    0x000B, 0x000C, 0x000C, 0x000C, 0x000C, 0x000C, 0x000C, 0x000C, 0x000D, 0x000D, 0x000D, 0x000D,
    0x000D, 0x000D, 0x000D, 0x000E, 0x000E, 0x000E, 0x000E, 0x000E, 0x000E, 0x000F, 0x000F, 0x000F,
    0x000F, 0x000F, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0011, 0x0011, 0x0011, 0x0011, 0x0011,
    0x0012, 0x0012, 0x0012, 0x0012, 0x0012, 0x0013, 0x0013, 0x0013, 0x0013, 0x0013, 0x0014, 0x0014,
    0x0014, 0x0014, 0x0015, 0x0015, 0x0015, 0x0015, 0x0016, 0x0016, 0x0016, 0x0016, 0x0017, 0x0017,
    0x0017, 0x0018, 0x0018, 0x0018, 0x0018, 0x0019, 0x0019, 0x0019, 0x001A, 0x001A, 0x001A, 0x001A,
    0x001B, 0x001B, 0x001B, 0x001C, 0x001C, 0x001C, 0x001D, 0x001D, 0x001D, 0x001E, 0x001E, 0x001E,
    0x001F, 0x001F, 0x0020, 0x0020, 0x0020, 0x0021, 0x0021, 0x0021, 0x0022, 0x0022, 0x0023, 0x0023,
    0x0023, 0x0024, 0x0024, 0x0025, 0x0025, 0x0026, 0x0026, 0x0026, 0x0027, 0x0027, 0x0028, 0x0028,
    0x0029, 0x0029, 0x002A, 0x002A, 0x002B, 0x002B, 0x002C, 0x002C, 0x002D, 0x002D, 0x002E, 0x002E,
    0x002F, 0x002F, 0x0030, 0x0031, 0x0031, 0x0032, 0x0032, 0x0033, 0x0033, 0x0034, 0x0035, 0x0035,
    0x0036, 0x0037, 0x0037, 0x0038, 0x0038, 0x0039, 0x003A, 0x003A, 0x003B, 0x003C, 0x003D, 0x003D,
    0x003E, 0x003F, 0x003F, 0x0040, 0x0041, 0x0042, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0046,
    0x0047, 0x0048, 0x0049, 0x004A, 0x004B, 0x004B, 0x004C, 0x004D, 0x004E, 0x004F, 0x0050, 0x0051,
    0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057, 0x0058, 0x0059, 0x005A, 0x005B, 0x005C, 0x005D,
    0x005E, 0x005F, 0x0060, 0x0061, 0x0062, 0x0064, 0x0065, 0x0066, 0x0067, 0x0068, 0x006A, 0x006B,
    0x006C, 0x006D, 0x006F, 0x0070, 0x0071, 0x0072, 0x0074, 0x0075, 0x0076, 0x0078, 0x0079, 0x007B,
    0x007C, 0x007E, 0x007F, 0x0080, 0x0082, 0x0083, 0x0085, 0x0087, 0x0088, 0x008A, 0x008B, 0x008D,
    0x008F, 0x0090, 0x0092, 0x0094, 0x0095, 0x0097, 0x0099, 0x009B, 0x009C, 0x009E, 0x00A0, 0x00A2,
    0x00A4, 0x00A6, 0x00A8, 0x00AA, 0x00AB, 0x00AD, 0x00AF, 0x00B2, 0x00B4, 0x00B6, 0x00B8, 0x00BA,
    0x00BC, 0x00BE, 0x00C0, 0x00C3, 0x00C5, 0x00C7, 0x00CA, 0x00CC, 0x00CE, 0x00D1, 0x00D3, 0x00D6,
    0x00D8, 0x00DB, 0x00DD, 0x00E0, 0x00E2, 0x00E5, 0x00E7, 0x00EA, 0x00ED, 0x00F0, 0x00F2, 0x00F5,
    0x00F8, 0x00FB, 0x00FE, 0x0101, 0x0104, 0x0107, 0x010A, 0x010D, 0x0110, 0x0113, 0x0116, 0x011A,
    0x011D, 0x0120, 0x0124, 0x0127, 0x012A, 0x012E, 0x0131, 0x0135, 0x0138, 0x013C, 0x0140, 0x0143,
    0x0147, 0x014B, 0x014F, 0x0153, 0x0157, 0x015B, 0x015F, 0x0163, 0x0167, 0x016B, 0x016F, 0x0173,
    0x0178, 0x017C, 0x0180, 0x0185, 0x0189, 0x018E, 0x0193, 0x0197, 0x019C, 0x01A1, 0x01A6, 0x01AB,
    0x01AF, 0x01B4, 0x01BA, 0x01BF, 0x01C4, 0x01C9, 0x01CE, 0x01D4, 0x01D9, 0x01DF, 0x01E4, 0x01EA,
    0x01EF, 0x01F5, 0x01FB, 0x0201, 0x0207, 0x020D, 0x0213, 0x0219, 0x021F, 0x0226, 0x022C, 0x0232,
    0x0239, 0x0240, 0x0246, 0x024D, 0x0254, 0x025B, 0x0262, 0x0269, 0x0270, 0x0277, 0x027E, 0x0286,
    0x028D, 0x0295, 0x029D, 0x02A4, 0x02AC, 0x02B4, 0x02BC, 0x02C4, 0x02CC, 0x02D5, 0x02DD, 0x02E6,
    0x02EE, 0x02F7, 0x0300, 0x0309, 0x0312, 0x031B, 0x0324, 0x032D, 0x0337, 0x0340, 0x034A, 0x0354,
    0x035D, 0x0367, 0x0371, 0x037C, 0x0386, 0x0390, 0x039B, 0x03A6, 0x03B1, 0x03BB, 0x03C7, 0x03D2,
    0x03DD, 0x03E9, 0x03F4, 0x0400, 0x040C, 0x0418, 0x0424, 0x0430, 0x043D, 0x0449, 0x0456, 0x0463,
    0x0470, 0x047D, 0x048A, 0x0498, 0x04A5, 0x04B3, 0x04C1, 0x04CF, 0x04DD, 0x04EC, 0x04FA, 0x0509,
    0x0518, 0x0527, 0x0536, 0x0546, 0x0555, 0x0565, 0x0575, 0x0586, 0x0596, 0x05A6, 0x05B7, 0x05C8,
    0x05D9, 0x05EB, 0x05FC, 0x060E, 0x0620, 0x0632, 0x0644, 0x0657, 0x066A, 0x067D, 0x0690, 0x06A4,
    0x06B7, 0x06CB, 0x06DF, 0x06F4, 0x0708, 0x071D, 0x0732, 0x0748, 0x075D, 0x0773, 0x0789, 0x079F,
    0x07B6, 0x07CD, 0x07E4, 0x07FB, 0x0813, 0x082B, 0x0843, 0x085C, 0x0874, 0x088E, 0x08A7, 0x08C1,
    0x08DA, 0x08F5, 0x090F, 0x092A, 0x0945, 0x0961, 0x097D, 0x0999, 0x09B5, 0x09D2, 0x09EF, 0x0A0D,
    0x0A2A, 0x0A48, 0x0A67, 0x0A86, 0x0AA5, 0x0AC5, 0x0AE5, 0x0B05, 0x0B25, 0x0B47, 0x0B68, 0x0B8A,
    0x0BAC, 0x0BCF, 0x0BF2, 0x0C15, 0x0C39, 0x0C5D, 0x0C82, 0x0CA7, 0x0CCC, 0x0CF2, 0x0D19, 0x0D3F,
    0x0D67, 0x0D8E, 0x0DB7, 0x0DDF, 0x0E08, 0x0E32, 0x0E5C, 0x0E87, 0x0EB2, 0x0EDD, 0x0F09, 0x0F36,
    0x0F63, 0x0F91, 0x0FBF, 0x0FEE, 0x101D, 0x104D, 0x107D, 0x10AE, 0x10DF, 0x1111, 0x1144, 0x1177,
    0x11AB, 0x11DF, 0x1214, 0x124A, 0x1280, 0x12B7, 0x12EE, 0x1326, 0x135F, 0x1399, 0x13D3, 0x140D,
    0x1449, 0x1485, 0x14C2, 0x14FF, 0x153E, 0x157D, 0x15BC, 0x15FD, 0x163E, 0x1680, 0x16C3, 0x1706,
    0x174A, 0x178F, 0x17D5, 0x181C, 0x1863, 0x18AC, 0x18F5, 0x193F, 0x198A, 0x19D5, 0x1A22, 0x1A6F,
    0x1ABE, 0x1B0D, 0x1B5D, 0x1BAE, 0x1C00, 0x1C53, 0x1CA7, 0x1CFC, 0x1D52, 0x1DA9, 0x1E01, 0x1E5A,
    0x1EB4, 0x1F0F, 0x1F6B, 0x1FC8, 0x2026, 0x2086, 0x20E6, 0x2148, 0x21AA, 0x220E, 0x2273, 0x22D9,
    0x2341, 0x23A9, 0x2413, 0x247E, 0x24EA, 0x2557, 0x25C6, 0x2636, 0x26A7, 0x271A, 0x278E, 0x2803,
    0x287A, 0x28F2, 0x296B, 0x29E6, 0x2A62, 0x2AE0, 0x2B5F, 0x2BDF, 0x2C61, 0x2CE5, 0x2D6A, 0x2DF1,
    0x2E79, 0x2F03, 0x2F8E, 0x301B, 0x30AA, 0x313A, 0x31CC, 0x325F, 0x32F5, 0x338C, 0x3425, 0x34BF,
    0x355B, 0x35FA, 0x369A, 0x373C, 0x37DF, 0x3885, 0x392C, 0x39D6, 0x3A81, 0x3B2F, 0x3BDE, 0x3C90,
    0x3D43, 0x3DF9, 0x3EB1, 0x3F6A, 0x4026, 0x40E5, 0x41A5, 0x4268, 0x432C, 0x43F4, 0x44BD, 0x4589,
    0x4657, 0x4727, 0x47FA, 0x48D0, 0x49A8, 0x4A82, 0x4B5F, 0x4C3E, 0x4D20, 0x4E05, 0x4EEC, 0x4FD6,
    0x50C3, 0x51B2, 0x52A4, 0x5399, 0x5491, 0x558C, 0x5689, 0x578A, 0x588D, 0x5994, 0x5A9D, 0x5BAA,
    0x5CBA, 0x5DCD, 0x5EE3, 0x5FFC, 0x6119, 0x6238, 0x635C, 0x6482, 0x65AC, 0x66D9, 0x680A, 0x693F,
    0x6A77, 0x6BB2, 0x6CF2, 0x6E35, 0x6F7B, 0x70C6, 0x7214, 0x7366, 0x74BC, 0x7616, 0x7774, 0x78D6,
    0x7A3D, 0x7BA7, 0x7D16, 0x7E88, 0x7FFF, 0x817B, 0x82FB, 0x847F, 0x8608, 0x8795, 0x8927, 0x8ABE,
    0x8C59, 0x8DF9, 0x8F9E, 0x9148, 0x92F6, 0x94AA, 0x9663, 0x9820, 0x99E3, 0x9BAB, 0x9D79, 0x9F4C,
    0xA124, 0xA302, 0xA4E5, 0xA6CE, 0xA8BC, 0xAAB0, 0xACAA, 0xAEAA, 0xB0B0, 0xB2BC, 0xB4CE, 0xB6E5,
    0xB904, 0xBB28, 0xBD53, 0xBF84, 0xC1BC, 0xC3FA, 0xC63F, 0xC88B, 0xCADD, 0xCD37, 0xCF97, 0xD1FE,
    0xD46D, 0xD6E3, 0xD960, 0xDBE4, 0xDE70, 0xE103, 0xE39E, 0xE641, 0xE8EB, 0xEB9E, 0xEE58, 0xF11B,
    0xF3E6, 0xF6B9, 0xF994, 0xFC78, 0xFF64, 0x0000,
};

static s32 sndVolCurve[128] = {
    0, 0, -1, -1, -1, -2, -2, -2,
    -3, -3, -4, -4, -4, -5, -5, -5,
    -6, -6, -7, -7, -7, -8, -8, -9,
    -9, -10, -10, -10, -11, -11, -12, -12,
    -13, -13, -14, -14, -14, -15, -15, -16,
    -16, -17, -17, -18, -18, -19, -20, -20,
    -21, -21, -22, -22, -23, -23, -24, -25,
    -25, -26, -26, -27, -28, -28, -29, -30,
    -30, -31, -32, -33, -33, -34, -35, -36,
    -36, -37, -38, -39, -40, -40, -41, -42,
    -43, -44, -45, -46, -47, -48, -49, -50,
    -51, -52, -54, -55, -56, -57, -59, -60,
    -61, -63, -64, -66, -67, -69, -71, -72,
    -74, -76, -78, -80, -83, -85, -87, -90,
    -93, -96, -99, -102, -106, -110, -115, -120,
    -126, -133, -140, -150, -163, -180, -210, -904,
};

static s32 sndPanCurve[128] = {
    -904, -210, -180, -163, -150, -140, -133, -126,
    -120, -115, -110, -106, -102, -99, -96, -93,
    -90, -87, -85, -83, -80, -78, -76, -74,
    -72, -71, -69, -67, -66, -64, -63, -61,
    -60, -59, -57, -56, -55, -54, -52, -51,
    -50, -49, -48, -47, -46, -45, -44, -43,
    -42, -41, -40, -40, -39, -38, -37, -36,
    -36, -35, -34, -33, -33, -32, -31, -30,
    -30, -29, -28, -28, -27, -26, -26, -25,
    -25, -24, -23, -23, -22, -22, -21, -21,
    -20, -20, -19, -18, -18, -17, -17, -16,
    -16, -15, -15, -14, -14, -14, -13, -13,
    -12, -12, -11, -11, -10, -10, -10, -9,
    -9, -8, -8, -7, -7, -7, -6, -6,
    -5, -5, -5, -4, -4, -4, -3, -3,
    -2, -2, -2, -1, -1, -1, 0, 0,
};

static u8 sndStreamVolTable[50] = {
    0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x02, 0x02, 0x02,
    0x02, 0x03, 0x03, 0x04, 0x04, 0x05, 0x05, 0x06, 0x07, 0x08,
    0x09, 0x0A, 0x0B, 0x0C, 0x0E, 0x10, 0x12, 0x14, 0x16, 0x19,
    0x1C, 0x20, 0x24, 0x28, 0x2D, 0x32, 0x39, 0x40, 0x47, 0x50,
    0x5A, 0x65, 0x71, 0x7F, 0x8F, 0xA0, 0xB4, 0xCA, 0xE3, 0xFF,
};

static SNDVOICE sndVoice[64];

static s32 sndStreamVolCur;
static s32 sndStreamVolTarget;
static u32 sndMixEnabled;

static u16 sndDbToMix(s32 db)
{
    if (db <= -0x388) {
        return 0;
    } else if (db >= 0x3C) {
        return 0xFF64;
    }
    return sndDbTable[db + 0x388];
}

void sndVoiceInit(void)
{
    int i;
    SNDVOICE* v;
    s32 volL;
    s32 volR;
    s32 panL;
    s32 panR;

    volL = sndVolCurve[0x40];
    volR = sndPanCurve[0x40];
    panL = sndPanCurve[0];
    panR = sndVolCurve[0];
    for (i = 0; i < 64; i++) {
        v = &sndVoice[i];
        v->flags = 0x50000000;
        v->vol = 0;
        v->auxA = -960;
        v->auxB = -960;
        v->volL = volL;
        v->volR = volR;
        v->panL = panL;
        v->panR = panR;
        v->master = 0;
        v->volIdx = 0x40;
        v->panIdx = 0x7F;
        v->mix[18] = 0;
        v->mix[16] = 0;
        v->mix[14] = 0;
        v->mix[12] = 0;
        v->mix[10] = 0;
        v->mix[8] = 0;
        v->mix[6] = 0;
        v->mix[4] = 0;
        v->mix[2] = 0;
        v->mix[0] = 0;
    }
    sndStreamVolCur = 0;
    sndStreamVolTarget = 0;
    sndMixEnabled = 1;
}

void sndVoiceSetParams(AXVPB* p, u32 flags, s32 vol, s32 auxA, s32 auxB,
                       s32 volIdx, s32 panIdx, s32 master)
{
    SNDVOICE* v = &sndVoice[p->index];
    int old;
    u16 ctrl;
    u8 unused[0xB0];

    v->voice = p;
    v->flags = flags & 7;
    v->vol = vol;
    v->auxA = auxA;
    v->auxB = auxB;
    v->volIdx = volIdx;
    v->panIdx = panIdx;
    v->master = master;
    v->volL = sndVolCurve[v->volIdx];
    v->volR = sndPanCurve[v->volIdx];
    v->panL = sndPanCurve[v->panIdx];
    v->panR = sndVolCurve[v->panIdx];

    if (v->flags & 4) {
        v->mix[0] = 0;
    } else {
        v->mix[0] = sndDbToMix(vol);
    }

    if (sndMixEnabled == 1) {
        v->mix[2] = sndDbToMix(v->master + v->volL + v->panL);
        v->mix[4] = sndDbToMix(v->master + v->volR + v->panL);
        v->mix[6] = sndDbToMix(v->master + v->panR);
        if (v->flags & 1) {
            v->mix[8] = sndDbToMix(v->auxA + v->volL + v->panL);
            v->mix[10] = sndDbToMix(v->auxA + v->volR + v->panL);
            v->mix[12] = sndDbToMix(v->auxA + v->panR - 0x3C);
        } else {
            v->mix[8] = sndDbToMix(v->master + v->auxA + v->volL + v->panL);
            v->mix[10] = sndDbToMix(v->master + v->auxA + v->volR + v->panL);
            v->mix[12] = sndDbToMix(v->master + v->auxA + v->panR - 0x3C);
        }
        if (v->flags & 2) {
            v->mix[14] = sndDbToMix(v->auxB + v->volL + v->panL);
            v->mix[16] = sndDbToMix(v->auxB + v->volR + v->panL);
            v->mix[18] = sndDbToMix(v->auxB + v->panR - 0x3C);
        } else {
            v->mix[14] = sndDbToMix(v->master + v->auxB + v->volL + v->panL);
            v->mix[16] = sndDbToMix(v->master + v->auxB + v->volR + v->panL);
            v->mix[18] = sndDbToMix(v->master + v->auxB + v->panR - 0x3C);
        }
    } else {
        v->mix[2] = sndDbToMix(v->master + v->panL);
        v->mix[4] = sndDbToMix(v->master + v->panL);
        v->mix[6] = sndDbToMix(v->master + v->panR);
        if (v->flags & 1) {
            v->mix[8] = sndDbToMix(v->auxA + v->panL);
            v->mix[10] = sndDbToMix(v->auxA + v->panL);
            v->mix[12] = sndDbToMix(v->auxA + v->panR - 0x3C);
        } else {
            v->mix[8] = sndDbToMix(v->master + v->auxA + v->panL);
            v->mix[10] = sndDbToMix(v->master + v->auxA + v->panL);
            v->mix[12] = sndDbToMix(v->master + v->auxA + v->panR - 0x3C);
        }
        if (v->flags & 2) {
            v->mix[14] = sndDbToMix(v->auxB + v->panL);
            v->mix[16] = sndDbToMix(v->auxB + v->panL);
            v->mix[18] = sndDbToMix(v->auxB + v->panR - 0x3C);
        } else {
            v->mix[14] = sndDbToMix(v->master + v->auxB + v->panL);
            v->mix[16] = sndDbToMix(v->master + v->auxB + v->panL);
            v->mix[18] = sndDbToMix(v->master + v->auxB + v->panR - 0x3C);
        }
    }

    old = OSDisableInterrupts();
    p->pb.ve.currentVolume = v->mix[0];
    p->pb.ve.currentDelta = 0;
    p->pb.mix.vL = v->mix[2];
    p->pb.mix.vDeltaL = 0;
    p->pb.mix.vR = v->mix[4];
    p->pb.mix.vDeltaR = 0;
    p->pb.mix.vAuxAL = v->mix[8];
    p->pb.mix.vDeltaAuxAL = 0;
    p->pb.mix.vAuxAR = v->mix[10];
    p->pb.mix.vDeltaAuxAR = 0;
    p->pb.mix.vAuxBL = v->mix[14];
    p->pb.mix.vDeltaAuxBL = 0;
    p->pb.mix.vAuxBR = v->mix[16];
    p->pb.mix.vDeltaAuxBR = 0;
    p->pb.mix.vAuxBS = v->mix[18];
    p->pb.mix.vDeltaAuxBS = 0;
    p->pb.mix.vS = v->mix[6];
    p->pb.mix.vDeltaS = 0;
    p->pb.mix.vAuxAS = v->mix[12];
    p->pb.mix.vDeltaAuxAS = 0;
    ctrl = 0;
    if (p->pb.mix.vAuxAL != 0 || p->pb.mix.vAuxAR != 0 ||
        p->pb.mix.vAuxAS != 0)
    {
        ctrl |= 1;
    }
    if (p->pb.mix.vAuxBL != 0 || p->pb.mix.vAuxBR != 0 ||
        p->pb.mix.vAuxBS != 0)
    {
        ctrl |= 2;
    }
    if (p->pb.mix.vS != 0 || p->pb.mix.vAuxAS != 0 || p->pb.mix.vAuxBS != 0) {
        ctrl |= 4;
    }
    p->pb.mixerCtrl = ctrl;
    p->sync |= 0x212;
    OSRestoreInterrupts(old);
}

void sndVoiceSetVolume(AXVPB* p, s32 vol)
{
    SNDVOICE* v = &sndVoice[p->index];

    if (vol < 0) {
        vol = 0;
    } else if (vol > 0x7F) {
        vol = 0x7F;
    }
    v->volIdx = vol;
    v->volL = sndVolCurve[v->volIdx];
    v->volR = sndPanCurve[v->volIdx];
    v->panL = sndPanCurve[v->panIdx];
    v->panR = sndVolCurve[v->panIdx];
    v->flags |= 0x40000000;
}

void sndVoiceSetPan(AXVPB* p, s32 pan)
{
    SNDVOICE* v = &sndVoice[p->index];

    if (pan < 0) {
        pan = 0;
    } else if (pan > 0x7F) {
        pan = 0x7F;
    }
    v->panIdx = pan;
    v->volL = sndVolCurve[v->volIdx];
    v->volR = sndPanCurve[v->volIdx];
    v->panL = sndPanCurve[v->panIdx];
    v->panR = sndVolCurve[v->panIdx];
    v->flags |= 0x40000000;
}

void sndVoiceStop(AXVPB* p)
{
    SNDVOICE* v = &sndVoice[p->index];

    v->flags |= 0x10000004;
}

void sndVoiceStart(AXVPB* p)
{
    SNDVOICE* v = &sndVoice[p->index];

    v->flags &= ~4;
    v->flags |= 0x10000000;
}

void sndVoiceSetMaster(AXVPB* p, s32 master)
{
    SNDVOICE* v = &sndVoice[p->index];

    v->master = master;
    v->flags |= 0x40000000;
}

void sndVoiceUpdateAll(void)
{
    SNDVOICE* v;
    AXVPB* p;
    int i;
    int mixDirty;
    int volDirty;
    u16 cur;
    u16 ctrl;
    u8 unused[0x78];

    for (i = 0; i < 64; i++) {
        v = &sndVoice[i];
        volDirty = 0;
        mixDirty = 0;
        p = v->voice;
        if (p != NULL) {
            if (v->flags & 0x20000000) {
                v->mix[0] = v->mix[1];
                v->flags &= ~0x20000000;
                volDirty = 1;
            }
            if (v->flags & 0x10000000) {
                if (v->flags & 4) {
                    v->mix[1] = 0;
                } else {
                    v->mix[1] = sndDbToMix(v->vol);
                }
                volDirty = 1;
                v->flags &= ~0x10000000;
                v->flags |= 0x20000000;
            }
            if (v->flags & 0x80000000) {
                mixDirty = 1;
                v->mix[2] = v->mix[3];
                v->mix[4] = v->mix[5];
                v->mix[6] = v->mix[7];
                v->mix[8] = v->mix[9];
                v->mix[10] = v->mix[11];
                v->mix[12] = v->mix[13];
                v->mix[14] = v->mix[15];
                v->mix[16] = v->mix[17];
                v->mix[18] = v->mix[19];
                v->flags &= ~0x80000000;
            }
            if (v->flags & 0x40000000) {
                if (sndMixEnabled == 1) {
                    v->mix[3] = sndDbToMix(v->master + v->volL + v->panL);
                    v->mix[5] = sndDbToMix(v->master + v->volR + v->panL);
                    v->mix[7] = sndDbToMix(v->master + v->panR - 0x3C);
                    if (v->flags & 1) {
                        v->mix[9] = sndDbToMix(v->auxA + v->volL + v->panL);
                        v->mix[11] = sndDbToMix(v->auxA + v->volR + v->panL);
                        v->mix[13] = sndDbToMix(v->auxA + v->panR - 0x3C);
                    } else {
                        v->mix[9] =
                            sndDbToMix(v->master + v->auxA + v->volL + v->panL);
                        v->mix[11] =
                            sndDbToMix(v->master + v->auxA + v->volR + v->panL);
                        v->mix[13] =
                            sndDbToMix(v->master + v->auxA + v->panR - 0x3C);
                    }
                    if (v->flags & 2) {
                        v->mix[15] = sndDbToMix(v->auxB + v->volL + v->panL);
                        v->mix[17] = sndDbToMix(v->auxB + v->volR + v->panL);
                        v->mix[19] = sndDbToMix(v->auxB + v->panR - 0x3C);
                    } else {
                        v->mix[15] =
                            sndDbToMix(v->master + v->auxB + v->volL + v->panL);
                        v->mix[17] =
                            sndDbToMix(v->master + v->auxB + v->volR + v->panL);
                        v->mix[19] =
                            sndDbToMix(v->master + v->auxB + v->panR - 0x3C);
                    }
                } else {
                    v->mix[3] = sndDbToMix(v->master + v->panL);
                    v->mix[5] = sndDbToMix(v->master + v->panL);
                    v->mix[7] = sndDbToMix(v->master + v->panR - 0x3C);
                    if (v->flags & 1) {
                        v->mix[9] = sndDbToMix(v->auxA + v->panL);
                        v->mix[11] = sndDbToMix(v->auxA + v->panL);
                        v->mix[13] = sndDbToMix(v->auxA + v->panR - 0x3C);
                    } else {
                        v->mix[9] = sndDbToMix(v->master + v->auxA + v->panL);
                        v->mix[11] = sndDbToMix(v->master + v->auxA + v->panL);
                        v->mix[13] =
                            sndDbToMix(v->master + v->auxA + v->panR - 0x3C);
                    }
                    if (v->flags & 2) {
                        v->mix[15] = sndDbToMix(v->auxB + v->panL);
                        v->mix[17] = sndDbToMix(v->auxB + v->panL);
                        v->mix[19] = sndDbToMix(v->auxB + v->panR - 0x3C);
                    } else {
                        v->mix[15] = sndDbToMix(v->master + v->auxB + v->panL);
                        v->mix[17] = sndDbToMix(v->master + v->auxB + v->panL);
                        v->mix[19] =
                            sndDbToMix(v->master + v->auxB + v->panR - 0x3C);
                    }
                }
                mixDirty = 1;
                v->flags &= ~0x40000000;
                v->flags |= 0x80000000;
            }
            if (volDirty && p != NULL) {
                cur = v->mix[0];
                p->pb.ve.currentVolume = cur;
                p->pb.ve.currentDelta = (v->mix[1] - v->mix[0]) / 0xA0;
                p->sync |= 0x200;
            }
            if (mixDirty && p != NULL) {
                p->pb.mix.vL = v->mix[2];
                p->pb.mix.vDeltaL = (v->mix[3] - v->mix[2]) / 0xA0;
                p->pb.mix.vR = v->mix[4];
                p->pb.mix.vDeltaR = (v->mix[5] - v->mix[4]) / 0xA0;
                p->pb.mix.vAuxAL = v->mix[8];
                p->pb.mix.vDeltaAuxAL = (v->mix[9] - v->mix[8]) / 0xA0;
                p->pb.mix.vAuxAR = v->mix[10];
                p->pb.mix.vDeltaAuxAR = (v->mix[11] - v->mix[10]) / 0xA0;
                p->pb.mix.vAuxBL = v->mix[14];
                p->pb.mix.vDeltaAuxBL = (v->mix[15] - v->mix[14]) / 0xA0;
                p->pb.mix.vAuxBR = v->mix[16];
                p->pb.mix.vDeltaAuxBR = (v->mix[17] - v->mix[16]) / 0xA0;
                p->pb.mix.vAuxBS = v->mix[18];
                p->pb.mix.vDeltaAuxBS = (v->mix[19] - v->mix[18]) / 0xA0;
                p->pb.mix.vS = v->mix[6];
                p->pb.mix.vDeltaS = (v->mix[7] - v->mix[6]) / 0xA0;
                ctrl = 0;
                p->pb.mix.vAuxAS = v->mix[12];
                p->pb.mix.vDeltaAuxAS = (v->mix[13] - v->mix[12]) / 0xA0;
                if ((cur = p->pb.mix.vAuxAL) != 0 ||
                    (cur = p->pb.mix.vAuxAR) != 0 ||
                    (cur = p->pb.mix.vAuxAS) != 0)
                {
                    ctrl |= 1;
                }
                if ((cur = p->pb.mix.vAuxBL) != 0 ||
                    (cur = p->pb.mix.vAuxBR) != 0 ||
                    (cur = p->pb.mix.vAuxBS) != 0)
                {
                    ctrl |= 2;
                }
                if ((cur = p->pb.mix.vS) != 0 ||
                    (cur = p->pb.mix.vAuxAS) != 0 ||
                    (cur = p->pb.mix.vAuxBS) != 0)
                {
                    ctrl |= 4;
                }
                if ((cur = p->pb.mix.vDeltaL) != 0 ||
                    (cur = p->pb.mix.vDeltaR) != 0 ||
                    (cur = p->pb.mix.vDeltaS) != 0 ||
                    (cur = p->pb.mix.vDeltaAuxAL) != 0 ||
                    (cur = p->pb.mix.vDeltaAuxAR) != 0 ||
                    (cur = p->pb.mix.vDeltaAuxAS) != 0 ||
                    (cur = p->pb.mix.vDeltaAuxBL) != 0 ||
                    (cur = p->pb.mix.vDeltaAuxBR) != 0 ||
                    (cur = p->pb.mix.vDeltaAuxBS) != 0)
                {
                    ctrl |= 8;
                }
                p->pb.mixerCtrl = ctrl;
                p->sync |= 0x12;
            }
        }
    }
    if (sndStreamVolTarget > sndStreamVolCur) {
        sndStreamVolCur += 1;
        AISetStreamVolLeft(sndStreamVolTable[sndStreamVolCur]);
        AISetStreamVolRight(sndStreamVolTable[sndStreamVolCur]);
    } else if (sndStreamVolTarget < sndStreamVolCur) {
        sndStreamVolCur -= 1;
        AISetStreamVolLeft(sndStreamVolTable[sndStreamVolCur]);
        AISetStreamVolRight(sndStreamVolTable[sndStreamVolCur]);
    }
}
