/*
 * dcsdrv.c - DCS driver front-end (DCS_PS2.OBJ aud_* layer), GameCube port.
 *
 * Text 0x800D4960-0x800D5260.  Owns driver init, the per-frame poll, and the
 * numeric command dispatcher that game code (game/audio/soundmgr.c) posts
 * sound/resource requests through.  Debug: "dcs_driver", "/gauntlet/".
 *
 * NonMatching: reconstruction scaffold.
 */
#include "types.h"
#include "game/dcs.h"

typedef struct DcsDriverState {
    void* streamFile;
    s32 streamArg0;
    s32 streamArg1;
    s32 bank;
    s32 memoryTop;
    u8 pad14[8];
    s32 initArg0;
    u8 pad20[4];
    s32 initArg1;
    s32 initArg2;
    char streamPath[240];
    char streamName[240];
} DcsDriverState;

extern DcsDriverState lbl_8031E0E0;
extern u8 lbl_8031E0EC[];
extern char lbl_80117268[];
extern u8 lbl_80345238;
extern u32 lbl_8034523C;
extern void* lbl_80345240;
extern s32 sConfig;
extern s32 sPending;
extern s32 sReset;

void mathStub1b__Fv();
void mathStub0__Fv();
u32 pool_init(u32 size);
void AdsSetVolumeDirect(void* ads, s32 volume);
void AdsKeyVoices(void* ads);
u32 AdsGetStatus(void* ads);
void AdsQuit(void);
void AdsClose(void* ads);
void AdsDelete(void* ads);
s32 AdsStart(void* ads);
int printf(const char* format, ...);
void* memset(void* dst, s32 value, u32 size);
void* memcpy(void* dst, const void* src, u32 size);
char* strcpy(char* dst, const char* src);
char* strcat(char* dst, const char* src);
u32 AdsGetMode(void* ads);
void AdsSetMode(void* ads, u32 mode);
void AdsSetVolume(void* ads, s32 volume);
void AdsInit(s32 memory, s32 block, s32 mode);
void* AdsNew(s32 size);
s32 AdsOpen(void* ads, void* desc);
void* FileBufStart(void* desc);
s32 AdsPutBuffer(void* ads, void* data, s32 size, s32 flags);
s32 adsUpdateStream(void* ads);
extern s32 lbl_80345244;
extern char lbl_80117294[];

#define DCS_DRIVER_STATE (&lbl_8031E0E0)
#define DCS_REQUEST_ARG(base, n) \
    (*(s32*)((u8*)(base) + ((n) * sizeof(s32))))

/* 0x800D4960  init driver + block pool (-> pool_init) */
void dcsInit(void)
{
    u8* state;
    s32 i;

    state = (u8*)&lbl_8031E0E0;
    lbl_80345238 = 1;
    for (i = 0; i < 2; i++) {
        mathStub1b__Fv(i | 0x980, 0x3FFF);
        mathStub1b__Fv(i | 0xA80, 0x3FFF);
    }
    pool_init(0x2000);
    *(s32*)(state + 0x0C) = 0x4000;
    *(s32*)(state + 0x10) = 0x9D8000;
    *(s32*)(state + 0x1C) = 0;
    *(s32*)(state + 0x24) = 0;
}

/* 0x800D49E4  per-frame driver tick ("dcs_driver") */
void dcsMain(void)
{
    volatile u8 scratch[16];
    char* strings;
    void* ads;
    s32 count;
    s32 i;
    s32 reset;

    (void)scratch;
    strings = lbl_80117268;
    if (lbl_80345238 != 0) {
        switch (lbl_8034523C) {
        case 1:
        case 19:
            AudioStillLoading();
            dcsBankUnload(lbl_8031E0EC);
            if (lbl_80345240 != 0) {
                AdsSetVolumeDirect(lbl_80345240, 0);
                ads = lbl_80345240;
                count = 0;
                AdsKeyVoices(ads);
                while (AdsGetStatus(ads) == 0x1000) {
                    mathStub0__Fv(1000);
                    count++;
                    if (count > 500) {
                        break;
                    }
                }
                if (count > 500) {
                    printf(strings + 12);
                    printf(strings + 24);
                    AdsQuit();
                }
                AdsClose(lbl_80345240);
                AdsDelete(lbl_80345240);
                lbl_80345240 = 0;
            }
            for (i = 0; i < 2; i++) {
                mathStub1b__Fv(i | 0x980, 0x3FFF);
                mathStub1b__Fv(i | 0xA80, 0x3FFF);
            }
            sConfig = 0;
            sPending = 1;
            break;

        case 4:
            reset = dcsBankLoad(&lbl_8031E0E0, 0);
            sReset = reset;
            if (reset > -2) {
                sPending = 1;
            }
            break;

        case 8:
            if (AdsStart(lbl_80345240) < 0) {
                AdsClose(lbl_80345240);
                AdsDelete(lbl_80345240);
                lbl_80345240 = 0;
            }
            break;

        case 12:
            if (lbl_80345240 != 0) {
                ads = lbl_80345240;
                count = 0;
                AdsKeyVoices(ads);
                while (AdsGetStatus(ads) == 0x1000) {
                    mathStub0__Fv(1000);
                    count++;
                    if (count > 500) {
                        break;
                    }
                }
                if (count > 500) {
                    printf(strings + 12);
                    printf(strings + 24);
                    AdsQuit();
                }
                AdsClose(lbl_80345240);
                AdsDelete(lbl_80345240);
                lbl_80345240 = 0;
            }
            sConfig = 0;
            break;
        }
        lbl_8034523C = 0;
    }
}

/* 0x800D4BF4  dispatch a numeric request (id,in,out) */
void dcsHandleRequest(u32 request, s32* input, s32* output)
{
    DcsDriverState* state;
    s32 resultOffset;
    u32 index;
    s32 i;
    s32 bankSize;
    s32 value;
    s32 result;
    s32 opcode;
    u32 mode;

    state = DCS_DRIVER_STATE;
    memset(output, 0, 32);

    switch (request) {
    case 19:
        state->initArg0 = input[0];
        state->initArg1 = input[2];
        state->initArg2 = input[3];

    case 1:
        dcsRequestReset();
        lbl_8034523C = request;
        dcsMain();
        break;

    case 25:
        dcsAllocReset(output + 5, output + 4, output + 3);
        output[2] = state->memoryTop - output[5];
        output[1] = 0;
        break;

    case 3:
        lbl_80345244 = !input[0];
        if (lbl_80345240 != 0) {
            mode = AdsGetMode(lbl_80345240) & 0xFF0F;
            if (lbl_80345244 != 0) {
                mode |= 0x10;
            }
            AdsSetMode(lbl_80345240, mode);
        }
        break;

    case 18:
        for (i = 0; i < 2; i++) {
            mathStub1b__Fv(i | 0x980, 0x3FFF);
            mathStub1b__Fv(i | 0xA80, 0x3FFF);
        }
        break;

    case 4:
        bankSize = input[1];
        if (bankSize >= 240) {
            bankSize = 240;
        }
        memcpy(state->streamName, (void*)input[0], bankSize);
        strcpy(state->streamPath, lbl_80117294);
        strcat(state->streamPath, state->streamName);
        state->streamFile = state->streamPath;
        state->streamArg0 = input[2];
        state->streamArg1 = input[3];
        output[0] = 0;
        output[1] = 1;
        sPending = -1;
        lbl_8034523C = request;
        dcsMain();
        break;

    case 7:
        if (dcsBankQuery(input[0], output + 1, output + 2) != 0) {
            output[0] = 0;
        } else {
            output[0] = -1;
        }
        break;

    case 24:
        AudioQueUpdate(input[0]);
        break;

    case 5:
        dcsQuePoll();
        break;

    case 6:
        AudioStillLoading();
        dcsBankUnload(&state->bank);
        break;

    case 17:
        output[0] = resultOffset = 0;
        index = 0;
        while (index < 32 && (opcode = input[index]) != 0) {
            switch (opcode) {
            case 0x55AA:
                index += 2;
                break;

            case 0x55AB:
                value = DCS_REQUEST_ARG(input, index + 1);
                if ((value & 0xFFFF0000) == 0xFFFF0000) {
                    DCS_REQUEST_ARG(input, index + 1) = value & 0xFFFF;
                    value =
                        (DCS_REQUEST_ARG(input, index + 1) * 0x1FFF) / 256;
                    AdsSetVolume(lbl_80345240, value | (value << 16));
                } else {
                    dcsChannelSetVolPan2(value >> 16, value & 0xFFFF);
                }
                index += 2;
                break;

            case 0x55AC:
                if (lbl_80345244 != 0) {
                    dcsChannelSetVolPan(
                        DCS_REQUEST_ARG(input, index + 1) >> 16, 0x7F);
                } else {
                    value = DCS_REQUEST_ARG(input, index + 1);
                    dcsChannelSetVolPan(value >> 16, value & 0xFFFF);
                }
                index += 2;
                break;

            case 0x55AD:
                index += 2;
                break;

            case 0x55AE:
                update_chinfo(DCS_REQUEST_ARG(input, index + 1) >> 16);
                index += 2;
                break;

            case 0x55D3:
                index += 2;
                break;

            case 0x55AF:
                dcsChannelPlay(DCS_REQUEST_ARG(input, index + 1));
                index += 2;
                break;

            default:
                if (lbl_80345244 != 0) {
                    input[index + 1] =
                        (input[index + 1] & 0xFFFF0000) | 0x7F;
                }
                result = dcsVoiceStart(input[index], input[index + 1],
                                       input[index + 2]);
                if (result >= 0) {
                    *(s32*)((u8*)output + resultOffset + 4) = 0;
                    *(s32*)((u8*)output + resultOffset + 8) = 0;
                    *(s32*)((u8*)output + resultOffset + 12) = (1 << result) << 16;
                } else {
                    *(s32*)((u8*)output + resultOffset + 4) = -2;
                    *(s32*)((u8*)output + resultOffset + 8) = 0;
                    *(s32*)((u8*)output + resultOffset + 12) = 0;
                }
                resultOffset += 12;
                index += 3;
                output[0]++;
                break;
            }
        }
        break;

    case 13:
        update_chinfo(0xFFFFFF);
        break;

    case 8:
        value = input[1];
        memcpy(state->streamName, (void*)input[0],
               value < 240 ? value : 240);
        strcpy(state->streamPath, lbl_80117294);
        strcat(state->streamPath, state->streamName);
        state->streamFile = state->streamPath;
        state->streamArg0 = input[2];
        state->streamArg1 = input[3];
        if (lbl_80345240 == 0) {
            AdsInit(0x9DC000, 0x2000, 2);
            lbl_80345240 = AdsNew(0x40000);
            if (lbl_80345240 != 0) {
                AdsSetVolume(lbl_80345240, 0x1FFF1FFF);
            }
        }
        if (lbl_80345240 == 0) {
            output[0] = -4;
        } else {
            result = AdsOpen(lbl_80345240, state);
            if (result >= 0) {
                lbl_8034523C = request;
                dcsMain();
                output[0] = 0;
            } else {
                AdsClose(lbl_80345240);
                AdsDelete(lbl_80345240);
                lbl_80345240 = 0;
                output[0] = -5;
            }
        }
        break;

    case 22:
        if (lbl_80345240 != 0) {
            output[0] = -4;
        } else {
            AdsInit(0x9DC000, 0x2000, 2);
            lbl_80345240 = AdsNew(input[0] << 2);
            if (lbl_80345240 != 0) {
                AdsSetVolume(lbl_80345240, 0x1FFF1FFF);
                state->streamFile = 0;
                output[0] = (s32)FileBufStart(state) + 0x40;
            } else {
                output[0] = -4;
            }
        }
        break;

    case 23:
        output[0] = 0;
        if (lbl_80345240 != 0) {
            output[0] = AdsPutBuffer(lbl_80345240, (void*)input[0], input[1], 0);
        }
        break;

    case 10:
        value = (input[0] * 0x1FFF) / 256;
        AdsSetVolume(lbl_80345240, value | (value << 16));
        result = lbl_80345244 != 0 || input[2] == 0;
        if (result != 0) {
            mode = 0x10;
        } else {
            mode = 0;
        }
        value = input[1] != 0 ? 2 : 0;
        AdsSetMode(lbl_80345240, value | mode);
        output[0] = adsUpdateStream(lbl_80345240);
        break;

    case 12:
        if (lbl_80345240 != 0) {
            AdsSetVolumeDirect(lbl_80345240, 0);
            lbl_8034523C = request;
            dcsMain();
            result = 0;
        } else {
            result = -2;
        }
        output[0] = result;
        break;

    case 9:
        value = (input[0] * 0x1FFF) / 256;
        AdsSetVolume(lbl_80345240, value | (value << 16));
        break;
    }
}
