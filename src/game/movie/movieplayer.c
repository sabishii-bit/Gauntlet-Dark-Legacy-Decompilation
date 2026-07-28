/*
 * MoviePlayer.cpp - GameCube VQ (vector-quantised) full-motion-video player.
 *
 * Text 0x800D860C-0x800DC180.  Sits between the g3dpad.c pad layer
 * (0xD7CFC-0xD860C) and cardutil.c (0xDC180).  Xbox module: wmvplayer.obj
 * (class CWMVPlayer, hardware WMV); the GameCube port reuses the source but
 * replaces WMV with a software VQ codec that decodes each frame into a GX
 * texture and presents it through the DEMO framebuffer, streaming the audio
 * track through the adstream/dcs layer.  Class names in the binary are
 * "MoviePlayer" / "MoviePlayerBase"; the assert file string is
 * "MoviePlayer.cpp" @0x80117684.
 *
 * String anchors (.rodata): "Gauntlet/VQMovies/@" @0x80117528,
 * "Gauntlet/VQMovies/midway.avi" @0x80117628, "MoviePlayer" @0x80117648,
 * "MoviePlayerBase" @0x80117654, "Gauntlet/VQMovies/KnotVQ.avi" @0x80117664,
 * "MoviePlayer.cpp" @0x80117684.
 *
 * sdata2 pool 0x80349390-0x803493F0 (shared by the movie fns and the embedded
 * DText debug-overlay helpers).  The little-endian file readers
 * (ReadU16LE/ReadU32LE/ReadF32LE, Xbox pb_objregs.obj
 * _2/_4/f32Little2NativeEndian) are TU-local here, used only to parse the
 * PC-endian .avi container.  PlayVQMovie (fn_800D9FEC) is the public entry,
 * called by test_movies (0x80049210 region) via the 0x800BF1A8/0x800BF208
 * movie API.
 *
 * NonMatching: reconstruction scaffold - the ~48 VQ-decode / GX-present /
 * file-stream bodies are GameCube-specific and not reconstructed; boundaries,
 * names and roles were established by scouting (extabindex grouping + sdata2
 * pool seam + string/global ownership).  Extracted bytes link from the DOL.
 * C++ in the original; written as C stubs (the mangled operator-delete names
 * are valid C identifiers) so the unit compiles for the NonMatching link.
 */
#include "types.h"

/* --- externs: allocator + alloc-balance counter (ml_mem.c) --- */
extern s32 lbl_803452AC;
extern void ResetAllocTot(void);
extern void* AllocHiMem(u32 size);
extern void* memcpy(void* dst, const void* src, u32 n);
extern void* memset(void* dst, int c, u32 n);

/* --- vtables (.data) + subsystem refcounts (.sbss) + colour ramps (.bss) --- */
extern u32 lbl_801296A4[];
extern u32 lbl_8012968C[];
extern u32 lbl_801296CC[];
extern u32 lbl_801296F0[];
extern u32 lbl_803452B8;
extern u32 gDTextInitCount;
extern u8 lbl_80321340[];
extern u8 gDTextColorRamp[];
extern u8* gDTextBuf;

/* --- sdata2 float pool (movie YUV->RGB matrix coeffs) --- */
extern const f32 lbl_803493D8;
extern const f32 lbl_803493DC;
extern const f32 lbl_803493E0;
extern const f32 lbl_803493E4;
extern const f32 lbl_803493E8;
extern const f32 lbl_803493EC;

/* --- forward decls for intra-TU calls --- */
u32* fn_800DBC64(u32* p);
u32* fn_800DBE04(u32* p);
u32* DTextInitColorRamp(u32* p);
void fn_800D9DF0(char* src, int len, u8* dst, int* outlen);
u32 fn_800D93D4(u32* p1, u32 p2, int p3, char* p4, int p5, u32 p6);
u32 fn_800D87FC(u32* p1, int p3, char* p4, int mode, int p5, u32 p6);
u32 fn_800D8BCC(u32* p1, int p3, char* p4, int mode, int p5, u32 p6);
u32 fn_800D8F28(int* p1, int p3, char* p4, int p5, u32 p6);
u32 fn_800D91B4(u32* p1, int p3, char* p4, int p5, u32 p6);
u32 fn_800D9A14(u32* p1, u8* p2, int p3, u8 p4);

/* --- little-endian container readers (parse the PC-format .avi header) ---
 * Defined at file-end in the original (callers see only a prototype), so they
 * are never auto-inlined; dont_inline reproduces that here. */
#pragma dont_inline on
u16 ReadU16LE(u8* p) {
    union { u8 b[2]; u16 h; } u;
    u.b[0] = p[1];
    u.b[1] = p[0];
    return u.h;
}

u32 ReadU32LE(u8* p) {
    union { u8 b[4]; u32 w; } u;
    u.b[0] = p[3];
    u.b[1] = p[2];
    u.b[2] = p[1];
    u.b[3] = p[0];
    return u.w;
}

u32 ReadF32LE(u8* p) {
    union { u8 b[4]; u32 w; } u;
    u.b[0] = p[3];
    u.b[1] = p[2];
    u.b[2] = p[1];
    u.b[3] = p[0];
    return u.w;
}
#pragma dont_inline off

/* --- VQ decode / GX present / file-stream bodies (parked NonMatching) --- */

void fn_800D860C(u32 param_1, u8* param_2, int param_3) {
    u8* p;
    int i;
    int cnt;
    cnt = param_3 << 1;
    i = 0;
    p = param_2;
    if (cnt <= 0) {
        return;
    }
    do {
        /* structural: YUV->RGBA pack; +8+8 term-A reassociation is compiler-internal */
        *(u32*)(param_2 + i) =
            ((int)((u32)p[1] * 224) / 512 + (int)((u32)p[4] * 224) / 512 + 16) * 0x100
          | ((int)((u32)p[5] * 224) / 512 + (int)((u32)p[2] * 224) / 512 + 16) * 0x1000000
          | (u32)((int)((u32)p[0] * 224) / 256 + 16)
          | ((int)((u32)p[3] * 224) / 256 + 16) * 0x10000;
        p += 6;
        i += 4;
        cnt--;
    } while (cnt != 0);
}

void fn_800D86C8(u32 param_1, u8* param_2, int param_3) {
    u8* p;
    int i;
    int cnt;
    cnt = param_3 << 1;
    i = 0;
    p = param_2;
    if (cnt <= 0) {
        return;
    }
    do {
        /* structural: YUV->RGBA pack (byte-swapped lanes vs fn_800D860C) */
        *(u32*)(param_2 + i) =
            (u32)((int)((u32)p[1] * 224) / 512 + (int)((u32)p[4] * 224) / 512 + 16)
          | ((int)((u32)p[5] * 224) / 512 + (int)((u32)p[2] * 224) / 512 + 16) * 0x10000
          | ((int)((u32)p[0] * 224) / 256 + 16) * 0x100
          | ((int)((u32)p[3] * 224) / 256 + 16) * 0x1000000;
        p += 6;
        i += 4;
        cnt--;
    } while (cnt != 0);
}

/* PARKED: C++ throw() landing-pad (bl __unexpected) - unreproducible in .c compile */
void fn_800D8784(void) {
}

/* VQ texture/tile decode into a GX tex obj (ReadU16LE/ReadF32LE, DCFlush/Invalidate, GXInvalidateTexAll) */
u32 fn_800D87FC(u32* p1, int p3, char* p4, int mode, int p5, u32 p6) {
    return 0;
}

/* VQ tile decode variant (ReadU16LE, DCFlush/Invalidate, GXInvalidateTexAll) */
u32 fn_800D8BCC(u32* p1, int p3, char* p4, int mode, int p5, u32 p6) {
    return 0;
}

/* VQ chunk -> buffer copy (ReadU16LE/ReadF32LE, memcpy) */
u32 fn_800D8F28(int* p1, int p3, char* p4, int p5, u32 p6) {
    return 0;
}

/* VQ chunk -> buffer copy (ReadU16LE, memcpy) */
u32 fn_800D91B4(u32* p1, int p3, char* p4, int p5, u32 p6) {
    return 0;
}

/* VQ frame parser: dispatches the fn_800D87FC/8BCC/8F28/91B4 decoders */
u32 fn_800D93D4(u32* param_1, u32 param_2, int param_3, char* param_4, int param_5, u32 param_6) {
    int iVar4;
    int iVar1;
    short sVar3;
    u8 auStack_20[8];

    iVar4 = *(int*)(param_3 + 0x14);
    iVar1 = ReadF32LE((u8*)param_4);
    if (1 < iVar1) {
        iVar4 = ReadF32LE((u8*)param_4);
        param_4 = param_4 + 8;
    }
    fn_800D9DBC((u32)auStack_20, param_4, iVar4, (u8*)param_1[6]);
    sVar3 = ReadU16LE((u8*)(param_1[6] + 2));
    if (sVar3 == 0) {
        switch (*(int*)(param_5 + 0x10)) {
        case 0x32595559:
            return fn_800D87FC(param_1, param_3, param_4, 1, param_5, param_6);
        case 0:
        case 3:
            if (*(short*)(param_5 + 0xe) == 0x18) {
                return fn_800D8F28((int*)param_1, param_3, param_4, param_5, param_6);
            }
            if (*(short*)(param_5 + 0xe) == 0x10) {
                return fn_800D87FC(param_1, param_3, param_4, 2, param_5, param_6);
            }
            break;
        case 0x59565955:
            return fn_800D87FC(param_1, param_3, param_4, 0, param_5, param_6);
        }
    } else {
        switch (*(int*)(param_5 + 0x10)) {
        case 0x32595559:
            return fn_800D8BCC(param_1, param_3, param_4, 1, param_5, param_6);
        case 0:
        case 3:
            if (*(short*)(param_5 + 0xe) == 0x18) {
                return fn_800D91B4(param_1, param_3, param_4, param_5, param_6);
            }
            if (*(short*)(param_5 + 0xe) == 0x10) {
                return fn_800D8BCC(param_1, param_3, param_4, 2, param_5, param_6);
            }
            break;
        case 0x59565955:
            return fn_800D8BCC(param_1, param_3, param_4, 0, param_5, param_6);
        }
    }
    return 0xffffffff;
}

void fn_800D9614(u32* param_1, u32* param_2) {
    fn_800D93D4(param_1, param_2[0], param_2[1], (char*)param_2[2], param_2[3], param_2[4]);
}

void fn_800D9648(u32* param_1, u32* param_2) {
    fn_800D93D4(param_1, param_2[0], param_2[1], (char*)param_2[2], param_2[3], param_2[4]);
}

void fn_800D967C(int param_1, int param_2) {
    (**(void(***)(int, u32, u32))(*(int*)(param_1 + 0x20) + 0xc))
        (param_1, *(u32*)(param_2 + 4), *(u32*)(param_2 + 0xc));
}

/* allocator (AllocHiMem + ResetAllocTot/__unexpected) */
void fn_800D96B0(void) {
}

void fn_800D9874(void) {
}

u32 fn_800D99AC(u32 a, int* src, u8* dst) {
    u32 r;
    if (dst == 0) {
        r = 56;
    } else {
        int wh;
        memcpy(dst, src, *src);
        wh = *(int*)(dst + 4) * *(int*)(dst + 8);
        r = 0;
        *(u16*)(dst + 0xe) = 24;
        *(u32*)(dst + 0x10) = 0;
        *(int*)(dst + 0x14) = wh * 3;
    }
    return r;
}

u32 fn_800D9A14(u32* param_1, u8* param_2, int param_3, u8 param_4) {
    u32 uVar1;
    int iVar2;
    u32 uVar4;
    int iVar5;

    uVar1 = param_1[2];
    uVar4 = param_1[3];
    if ((int)uVar1 >= (int)uVar4) {
        iVar2 = uVar1 - uVar4;
    } else {
        iVar2 = param_1[1] + (uVar1 - uVar4);
    }
    if (param_3 > iVar2) {
        return 0;
    }
    if (param_4 != 0) {
        iVar2 = param_1[1] - uVar4;
        if (iVar2 > param_3) {
            iVar2 = param_3;
        }
        memcpy(param_2, (u8*)(*param_1 + uVar4), iVar2);
        iVar5 = param_3 - iVar2;
        param_1[3] = param_1[3] + iVar2;
        if (param_1[3] == param_1[1]) {
            param_1[3] = 0;
        }
        if (iVar5 != 0) {
            memcpy(param_2 + iVar2, (u8*)*param_1, iVar5);
            param_1[3] = param_1[3] + iVar5;
        }
    } else {
        iVar2 = param_1[1] - uVar4;
        if (iVar2 > param_3) {
            iVar2 = param_3;
        }
        memcpy(param_2, (u8*)(*param_1 + uVar4), iVar2);
        if (param_3 - iVar2 != 0) {
            memcpy(param_2 + iVar2, (u8*)*param_1, param_3 - iVar2);
        }
    }
    return 1;
}

u32 fn_800D9B48(u32* param_1, u8* param_2, int param_3) {
    u32 uVar1;
    int iVar2;
    u32 uVar4;
    int iVar5;

    uVar4 = param_1[2];
    uVar1 = param_1[3];
    if ((int)uVar4 >= (int)uVar1) {
        iVar2 = uVar4 - uVar1;
    } else {
        iVar2 = param_1[1] + (uVar4 - uVar1);
    }
    if (param_3 > (int)((param_1[1] - iVar2) - 1)) {
        return 0;
    }
    iVar2 = param_1[1] - uVar4;
    if (iVar2 > param_3) {
        iVar2 = param_3;
    }
    memcpy((u8*)(*param_1 + uVar4), param_2, iVar2);
    iVar5 = param_3 - iVar2;
    param_1[2] = param_1[2] + iVar2;
    if (param_1[2] == param_1[1]) {
        param_1[2] = 0;
    }
    if (iVar5 != 0) {
        memcpy((u8*)*param_1, param_2 + iVar2, iVar5);
        param_1[2] = param_1[2] + iVar5;
    }
    return 1;
}

#pragma dont_inline on
int fn_800D9C34(int p) {
    int hi = *(int*)(p + 8);
    int lo = *(int*)(p + 0xc);
    if (hi >= lo) {
        return hi - lo;
    }
    return *(int*)(p + 4) + (hi - lo);
}
#pragma dont_inline off

void fn_800D9C5C(void) {
}

void fn_800D9CF4(void) {
}

#pragma dont_inline on
void fn_800D9DA4(u32* p) {
    p[1] = 0;
    p[0] = 0;
    p[3] = 0;
    p[2] = 0;
}
#pragma dont_inline off

int fn_800D9DBC(u32 param_1, char* param_2, int param_3, u8* param_4) {
    int local[2];
    fn_800D9DF0(param_2, param_3, param_4, local);
    return local[0];
}

void fn_800D9DF0(char* param_1, int param_2, u8* param_3, int* param_4) {
    u32 uVar6;
    u8 bVar2;
    u32 uVar3;
    u8* pbVar4;
    u8* pbVar5;
    u8* pbVar8;
    u8* pbVar9;
    u8* end;
    u8* limit;
    int iVar7;

    pbVar4 = (u8*)(param_1 + 4);
    pbVar5 = param_3;
    end = (u8*)(param_1 + param_2);
    limit = end - 0x20;
    uVar6 = 1;
    if (*(u8*)param_1 == 1) {
        memcpy(param_3, pbVar4, param_2 - 4);
        *param_4 = param_2 - 4;
    } else {
        while (pbVar4 != end) {
            if (uVar6 == 1) {
                bVar2 = *pbVar4;
                pbVar8 = pbVar4 + 1;
                pbVar4 = pbVar4 + 2;
                uVar6 = bVar2 | 0x10000 | ((u32)*pbVar8 << 8);
            }
            if (pbVar4 <= limit) {
                iVar7 = 16;
            } else {
                iVar7 = 1;
            }
            while (iVar7-- != 0) {
                if ((uVar6 & 1) == 0) {
                    bVar2 = *pbVar4;
                    pbVar4 = pbVar4 + 1;
                    *pbVar5 = bVar2;
                    pbVar5 = pbVar5 + 1;
                } else {
                    bVar2 = *pbVar4;
                    pbVar8 = pbVar4 + 1;
                    pbVar4 = pbVar4 + 2;
                    pbVar8 = pbVar5 - (((bVar2 & 0xf0) << 4) | *pbVar8);
                    uVar3 = bVar2 & 0xf;
                    *pbVar5 = *pbVar8;
                    pbVar5[1] = pbVar8[1];
                    pbVar9 = pbVar8 + 3;
                    pbVar5[2] = pbVar8[2];
                    pbVar5 = pbVar5 + 3;
                    for (; uVar3 != 0; uVar3 = uVar3 - 1) {
                        bVar2 = *pbVar9;
                        pbVar9 = pbVar9 + 1;
                        *pbVar5 = bVar2;
                        pbVar5 = pbVar5 + 1;
                    }
                }
                uVar6 = uVar6 >> 1;
            }
        }
        *param_4 = (int)pbVar5 - (int)param_3;
    }
}

/* per-frame audio pump during playback (adsPoll, sndCmd17) */
void fn_800D9F20(void) {
}

/* 0x800D9FEC top-level VQ movie playback loop: sets up GX/TEV, decodes+presents each frame (DEMODoneRender/DEMOSwapBuffers), polls pads (G3DGetPadStatusBuffer) to allow skipping, pumps audio (adsPoll/sndCmd17). Xbox: PlayVQMovie. Called by test_movies. */
void PlayVQMovie(void) {
}

/* movie close/cleanup (AudioStreamStop, operator delete, sceClose) */
void fn_800DA60C(void) {
}

/* movie start: stream audio setup (sndCmd16/sndCmd17) */
void fn_800DA6A4(void) {
}

/* movie open: sceOpen/sceRead the Gauntlet VQMovies .avi file, asserts on failure (MoviePlayer.cpp) */
void fn_800DA920(void) {
}

/* VQ .avi header parser (ReadF32LE/ReadU16LE/ReadU32LE) */
u32 fn_800DACD8(int param_1, u8* param_2) {
    int iVar1;
    u32 uVar2;
    int iVar3;
    int iVar4;
    u16 uVar6;
    int iVar5;
    u8* puVar7;

    *(u8*)(param_1 + 0x18) = 0;
    iVar1 = ReadF32LE(param_2);
    if (iVar1 == 0x46464952 && (iVar1 = ReadF32LE(param_2 + 8), iVar1 == 0x20495641)) {
        iVar1 = ReadF32LE(param_2 + 0x1c);
        iVar3 = ReadF32LE(param_2 + iVar1 + 0x20);
        if (iVar3 == 0x5453494c) {
            iVar3 = ReadF32LE(param_2 + iVar1 + 0x20 + 4);
            puVar7 = param_2 + iVar1 + 0x30;
            iVar4 = ReadF32LE(puVar7 + 4);
            if (iVar4 == 0x73646976) {
                *(u32*)(param_1 + 0xb4) = ReadF32LE(puVar7 + 4);
                *(u32*)(param_1 + 0xb8) = ReadF32LE(puVar7 + 8);
                *(u32*)(param_1 + 0xbc) = ReadF32LE(puVar7 + 0xc);
                *(u32*)(param_1 + 0xc0) = ReadF32LE(puVar7 + 0x10);
                *(u16*)(param_1 + 0xc4) = ReadU16LE(puVar7 + 0x14);
                *(u16*)(param_1 + 0xc6) = ReadU16LE(puVar7 + 0x16);
                *(u32*)(param_1 + 200) = ReadF32LE(puVar7 + 0x18);
                *(u32*)(param_1 + 0xcc) = ReadF32LE(puVar7 + 0x1c);
                *(u32*)(param_1 + 0xd0) = ReadF32LE(puVar7 + 0x20);
                *(u32*)(param_1 + 0xd4) = ReadF32LE(puVar7 + 0x24);
                *(u32*)(param_1 + 0xd8) = ReadF32LE(puVar7 + 0x28);
                *(u32*)(param_1 + 0xdc) = ReadF32LE(puVar7 + 0x2c);
                *(u32*)(param_1 + 0xe0) = ReadF32LE(puVar7 + 0x30);
                *(u32*)(param_1 + 0xe4) = ReadF32LE(puVar7 + 0x34);
                iVar4 = ReadF32LE(puVar7);
                iVar4 = iVar1 + 0x30 + iVar4;
                iVar5 = ReadF32LE(param_2 + iVar4 + 4);
                if (iVar5 == 0x66727473) {
                    *(u32*)(param_1 + 0x194) = ReadF32LE(param_2 + iVar4 + 0xc);
                    *(u32*)(param_1 + 0x198) = ReadU32LE(param_2 + iVar4 + 0x10);
                    *(u32*)(param_1 + 0x19c) = ReadU32LE(param_2 + iVar4 + 0x14);
                    *(u16*)(param_1 + 0x1a0) = ReadU16LE(param_2 + iVar4 + 0x18);
                    *(u16*)(param_1 + 0x1a2) = ReadU16LE(param_2 + iVar4 + 0x1a);
                    *(u32*)(param_1 + 0x1a4) = ReadF32LE(param_2 + iVar4 + 0x1c);
                    *(u32*)(param_1 + 0x1a8) = ReadF32LE(param_2 + iVar4 + 0x20);
                    *(u32*)(param_1 + 0x1ac) = ReadU32LE(param_2 + iVar4 + 0x24);
                    *(u32*)(param_1 + 0x1b0) = ReadU32LE(param_2 + iVar4 + 0x28);
                    *(u32*)(param_1 + 0x1b4) = ReadF32LE(param_2 + iVar4 + 0x2c);
                    *(u32*)(param_1 + 0x1b8) = ReadF32LE(param_2 + iVar4 + 0x30);
                    *(u32*)(param_1 + 0x1bc) = ReadF32LE(param_2 + iVar4 + 0x34);
                    *(u32*)(param_1 + 0x1c0) = ReadF32LE(param_2 + iVar4 + 0x38);
                    *(u32*)(param_1 + 0x1c4) = ReadF32LE(param_2 + iVar4 + 0x3c);
                    puVar7 = param_2 + iVar1 + 0x20 + iVar3 + 8;
                    iVar1 = ReadF32LE(puVar7);
                    if (iVar1 == 0x5453494c && (iVar1 = ReadF32LE(puVar7 + 0x14), iVar1 == 0x73647561)) {
                        *(u8*)(param_1 + 0x18) = 1;
                        *(u32*)(param_1 + 0xe8) = ReadF32LE(puVar7 + 0x14);
                        *(u32*)(param_1 + 0xec) = ReadF32LE(puVar7 + 0x18);
                        *(u32*)(param_1 + 0xf0) = ReadF32LE(puVar7 + 0x1c);
                        *(u32*)(param_1 + 0xf4) = ReadF32LE(puVar7 + 0x20);
                        *(u16*)(param_1 + 0xf8) = ReadU16LE(puVar7 + 0x24);
                        *(u16*)(param_1 + 0xfa) = ReadU16LE(puVar7 + 0x26);
                        *(u32*)(param_1 + 0xfc) = ReadF32LE(puVar7 + 0x28);
                        *(u32*)(param_1 + 0x100) = ReadF32LE(puVar7 + 0x2c);
                        *(u32*)(param_1 + 0x104) = ReadF32LE(puVar7 + 0x30);
                        *(u32*)(param_1 + 0x108) = ReadF32LE(puVar7 + 0x34);
                        *(u32*)(param_1 + 0x10c) = ReadF32LE(puVar7 + 0x38);
                        *(u32*)(param_1 + 0x110) = ReadF32LE(puVar7 + 0x3c);
                        *(u32*)(param_1 + 0x114) = ReadF32LE(puVar7 + 0x40);
                        *(u32*)(param_1 + 0x118) = ReadF32LE(puVar7 + 0x44);
                    }
                    uVar2 = 1;
                } else {
                    uVar2 = 0;
                }
            } else {
                uVar2 = 0;
            }
        } else {
            uVar2 = 0;
        }
    } else {
        uVar2 = 0;
    }
    return uVar2;
}

/* MoviePlayer teardown (AudioStreamStop, operator delete, dtor_800DBB94) */
void fn_800DB008(void) {
}

u32* fn_800DB0F8(u32* p) {
    p[0] = (u32)lbl_801296A4;
    p[0] = (u32)lbl_8012968C;
    fn_800DBC64(p + 8);
    fn_800DBE04(p + 0x54);
    p[7] = 0;
    p[100] = 0;
    return p;
}

/* operator delete[] (weak, emitted into this TU) */
void __dla__FPv(void) {
}

/* operator delete (weak, emitted into this TU) */
void __dl__FPv(void) {
}

void dtor_800DB21C(void) {
}

void fn_800DB29C(int p) {
    u32* n = *(u32**)(p + 0x50);
    *(u32*)(p + 0x50) = n[0];
    *(u32*)(p + 0x14) = n[2] + n[1];
    n[0] = *(u32*)(p + 0x58);
    *(u32**)(p + 0x58) = n;
    if (*(u32*)(p + 0x50) == 0) {
        return;
    }
    if ((u32)(*(int*)(p + 0x14) + *(int*)(*(int*)(p + 0x50) + 4)) < *(u32*)(p + 0x18)) {
        return;
    }
    *(u32*)(p + 0x14) = 0;
}

int fn_800DB2F4(int param_1, u8* param_2, u32 param_3, u32 param_4) {
    int iVar1;
    int ret;
    u8 unused[8];
    iVar1 = fn_800D9C34(param_1 + 0x3c);
    if (iVar1 < (int)param_4) {
        memset(param_2, 0, param_4);
        ret = 0;
    } else {
        *(u8*)(param_1 + 0x4c) = 1;
        fn_800D9A14((u32*)(param_1 + 0x3c), param_2, param_4, *(char*)(param_1 + 0x4c));
        ret = 1;
    }
    return ret;
}

u32* fn_800DB36C(int p) {
    u32* q = *(u32**)(p + 0x50);
    if (q == 0 || (q[8] == 0 && q[9] == 0 && q[6] == 0)) {
        return 0;
    }
    if (*q == 0 && *(u32*)(p + 0x28) != *(u32*)(p + 0x2c)) {
        return 0;
    }
    return q;
}

/* VQ codebook/frame reader (memcpy, ReadF32LE, sceRead) */
void fn_800DB3D4(void) {
}

void fn_800DB82C(void) {
}

void fn_800DB91C(void) {
}

void fn_800DBA80(void) {
}

void dtor_800DBB94(void) {
}

u32* fn_800DBC64(u32* p) {
    fn_800D9DA4(p + 0xf);
    p[3] = 0;
    p[2] = 0;
    p[1] = 0;
    p[0] = 0;
    p[6] = 0;
    p[5] = 0;
    p[4] = 0;
    p[8] = 0;
    p[9] = 0;
    p[0x16] = 0;
    p[0x15] = 0;
    p[0x14] = 0;
    return p;
}

u8 fn_800DBCCC(void* self, s32 x) {
    u32 n;
    if (x == 0) {
        return 0;
    }
    for (n = 0; x != 0; x = x >> 1) {
        n += x & 1;
    }
    return n;
}

u8 fn_800DBD00(void* self, s32 x) {
    u32 c;
    if (x == 0) {
        return 0;
    }
    for (c = 0; (x & 1) == 0; x = x >> 1) {
        c++;
    }
    return c;
}

void fn_800DBD30(void) {
}

u32* fn_800DBE04(u32* p) {
    DTextInitColorRamp(p);
    p[8] = (u32)lbl_801296CC;
    if (lbl_803452B8 == 0) {
        int i;
        int val;
        for (i = 0, val = 0; i < 256; i++, val += 31) {
            lbl_80321340[i] = (val + 128) / 255;
        }
    }
    lbl_803452B8++;
    p[12] = 0;
    return p;
}

/* DText glyph blit (uses gDTextBuf + movie sdata2 pool) */
void fn_800DBE98(u32 param_1, u8* param_2) {
    f32 fVar1;
    f32 fVar2;
    f32 fVar3;
    f32 fVar4;

    fVar4 = lbl_803493DC;
    fVar1 = (f32)(u32)param_2[0];
    fVar2 = (f32)(u32)param_2[2] - lbl_803493D8;
    fVar3 = (f32)(u32)param_2[1] - lbl_803493D8;
    param_2[2] = gDTextBuf[(int)(lbl_803493DC + lbl_803493E0 * fVar2 + fVar1)];
    param_2[1] = gDTextBuf[(int)(fVar4 + -(lbl_803493E8 * fVar2 - -(lbl_803493E4 * fVar3 - fVar1)))];
    param_2[0] = gDTextBuf[(int)(fVar4 + lbl_803493EC * fVar3 + fVar1)];
}

/* DText allocate/init buffer (gDTextInitCount) */
void fn_800DBF6C(void) {
}

/* 0x800DC034 init the DText debug-overlay 256-entry colour ramp (gDTextColorRamp/gDTextBuf) */
u32* DTextInitColorRamp(u32* p) {
    p[8] = (u32)lbl_801296F0;
    if (gDTextInitCount == 0) {
        int i;
        int val;
        memset(gDTextColorRamp, 0, 256);
        memset(gDTextColorRamp + 512, 255, 256);
        for (i = 0; i < 256; i++) {
            gDTextBuf[i] = i;
        }
        for (i = 0, val = 0; i < 32; i++, val += 255) {
            gDTextColorRamp[768 + i] = (val + 16) / 31;
        }
    }
    gDTextInitCount++;
    p[6] = 0;
    return p;
}
