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
 * PC-endian .avi container.  PlayVQMovie (PlayVQMovie) is the public entry,
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
extern s32 gMovieAllocCount;
extern void ResetAllocTot(void);
extern void* AllocHiMem(u32 size, u32 tag);
extern void* memcpy(void* dst, const void* src, u32 n);
extern void* memset(void* dst, int c, u32 n);

/* --- GX / data-cache maintenance (present the decoded VQ texture) --- */
extern void DCInvalidateRange(void* addr, u32 nBytes);
extern void DCFlushRange(void* addr, u32 nBytes);
extern void GXInvalidateTexAll(void);

/* --- audio stream pump (soundmgr / adstream) --- */
extern void adsPoll(void);
extern s32 sndCmd17(s32 a, s32 b);
extern u8* gMovieStreamState;

/* --- PS2-shim file IO (sceLseek/sceRead the .avi container) --- */
extern int sceLseek(int fd, int offset, int whence);
extern int sceRead(int fd, void* data, int length);

/* --- vtables (.data) + subsystem refcounts (.sbss) + colour ramps (.bss) --- */
extern u32 lbl_801296A4[];
extern u32 lbl_8012968C[];
extern u32 lbl_801296CC[];
extern u32 lbl_801296F0[];
extern u32 lbl_803452B8;
extern u32 gDTextInitCount;
extern u8 lbl_80321340[];
extern u8 gMovieAudioCallback[];
extern u8 gDTextColorRamp[];
extern u8* gDTextBuf;

/* --- sdata2 float pool (movie YUV->RGB matrix coeffs) --- */
extern const f32 lbl_80349390;
extern const f32 lbl_803493B8;
extern const f32 lbl_803493BC;
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
void fn_800DBE98(u32 param_1, u8* param_2);
int fn_800DB2F4(int param_1, u8* param_2, u32 param_3, u32 param_4);
void fn_800DB3D4(u32* stream, s32 fd, u32 length);
void fn_800DB29C(int stream);
u32* fn_800DB36C(int stream);
void fn_800D9F20(int audio);
extern u32 __cvt_fp2unsigned(f64 value);
extern s32 sndCmd16(s32 size);
extern s32 sndCmdA(s32 volume, s32 arg1, s32 arg2, void* callback);
extern s32 lbl_80343B4C;
extern u8 gMovieFrameTimeReset;

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

/* Release one movie allocation and clear the owning slot. */
s32 fn_800D8784(u32* state) {
    if (state[6] != 0) {
        gMovieAllocCount--;
        if (gMovieAllocCount == 0) {
            ResetAllocTot();
        }
    }
    state[6] = 0;
    return 0;
}

/* VQ texture/tile decode into a GX tex obj (ReadU16LE/ReadF32LE, DCFlush/Invalidate, GXInvalidateTexAll) */
u32 fn_800D87FC(u32* param_1, int param_2, char* param_3, int param_4, int param_5, u32 param_6) {
    u8 bVar1;
    u8 bVar2;
    u8 bVar3;
    u16 uVar7;
    int iVar4;
    u32 uVar6;
    u32 uVar9;
    u8* pbVar8;
    u8* pbVar10;
    int iVar11;
    u32 uVar12;
    u32* puVar13;
    u32* puVar14;
    u32 uVar15;
    u32 uVar16;
    u32 uVar17;
    u32 uVar18;
    u8* pbVar19;
    int iVar20;
    int iVar21;

    uVar7 = ReadU16LE((u8*)param_1[6]);
    uVar6 = uVar7;
    iVar4 = ReadF32LE((u8*)(param_1[6] + 4));
    iVar20 = param_1[6] + 8;
    pbVar8 = (u8*)(iVar20 + param_1[0xb]);
    pbVar19 = pbVar8 + uVar6 * 0xc;
    DCInvalidateRange((void*)param_6, *param_1 * param_1[1] * 2);
    if (param_4 == 1) {
        fn_800D860C((u32)param_1, pbVar8, uVar6);
        goto present;
    } else {
        if (param_4 < 1) {
            if (-1 < param_4) {
                fn_800D86C8((u32)param_1, pbVar8, uVar6);
                goto present;
            }
        } else if (param_4 < 3) {
            bVar3 = *((u8*)param_1 + 0x39);
            bVar1 = *(u8*)(param_1 + 0xe);
            bVar2 = *((u8*)param_1 + 0x37);
            iVar11 = 0;
            pbVar10 = pbVar8;
            for (iVar21 = 0; iVar21 < (int)(uVar6 << 2); iVar21 = iVar21 + 1) {
                fn_800DBE98((u32)param_1, pbVar10);
                *(u16*)(pbVar8 + iVar11) =
                    (u16)(((u32)pbVar10[2] >> (8 - bVar2 & 0x3f)) << *(u8*)(param_1 + 0xd)) |
                    (u16)(((u32)*pbVar10 >> (8 - bVar1 & 0x3f)) << *((u8*)param_1 + 0x36)) |
                    (u16)(((u32)pbVar10[1] >> (8 - bVar3 & 0x3f)) << *((u8*)param_1 + 0x35));
                pbVar10 = pbVar10 + 3;
                iVar11 = iVar11 + 2;
            }
            goto present;
        }
        return 0xffffffff;
    }
present:
    if (*(int*)(param_5 + 8) < 0) {
        iVar11 = -1;
        uVar15 = param_1[1] - 1;
    } else {
        uVar15 = 0;
        iVar11 = 1;
    }
    if (uVar6 < 0x101) {
        *param_1 = *param_1 << 1;
        for (uVar6 = 0; (int)uVar6 < (int)param_1[1]; uVar6 = uVar6 + 2) {
            uVar12 = param_1[10];
            puVar13 = (u32*)(param_6 + (uVar15 & 0xfffffffc) * *param_1 + (uVar15 & 3) * 8);
            puVar14 = puVar13 + iVar11 * 2;
            uVar16 = 0;
            do {
                uVar9 = (int)uVar16 >> 3;
                if ((1 << (uVar9 & 7) &
                     (u32)*(u8*)(iVar20 + ((int)uVar6 >> 2) * uVar12 + ((int)uVar16 >> 6))) != 0) {
                    bVar3 = *pbVar19;
                    pbVar19 = pbVar19 + 1;
                    *puVar13 = *(u32*)(pbVar8 + (u32)bVar3 * 8);
                    *puVar14 = *(u32*)(pbVar8 + (u32)bVar3 * 8 + 4);
                }
                iVar4 = (uVar16 & 4) * 6 + 4;
                uVar16 = uVar16 + 4;
                puVar13 = (u32*)((int)puVar13 + iVar4);
                puVar14 = (u32*)((int)puVar14 + iVar4);
            } while ((int)uVar16 < (int)*param_1);
            uVar15 = uVar15 + iVar11 * 2;
        }
        uVar6 = *param_1;
        *param_1 = (int)uVar6 >> 1;
    } else {
        uVar6 = iVar4 + 7;
        uVar12 = *pbVar19;
        *param_1 = *param_1 << 1;
        pbVar10 = pbVar19 + 1;
        pbVar19 = pbVar19 + ((int)uVar6 >> 3);
        uVar6 = 0;
        for (uVar16 = 0; (int)uVar16 < (int)param_1[1]; uVar16 = uVar16 + 2) {
            uVar9 = param_1[10];
            puVar13 = (u32*)(param_6 + (uVar15 & 0xfffffffc) * *param_1 + (uVar15 & 3) * 8);
            puVar14 = puVar13 + iVar11 * 2;
            uVar17 = 0;
            do {
                uVar18 = (int)uVar17 >> 3;
                if ((1 << (uVar18 & 7) &
                     (u32)*(u8*)(iVar20 + ((int)uVar16 >> 2) * uVar9 + ((int)uVar17 >> 6))) != 0) {
                    bVar3 = *pbVar19;
                    uVar18 = uVar6 & 0x3f;
                    uVar6 = uVar6 + 1;
                    *puVar13 = *(u32*)(pbVar8 + (((int)uVar12 >> uVar18 & 1U) << 8 | (u32)bVar3) * 8);
                    pbVar19 = pbVar19 + 1;
                    *puVar14 = *(u32*)(pbVar8 + (((int)uVar12 >> uVar18 & 1U) << 8 | (u32)bVar3) * 8 + 4);
                    if ((uVar6 & 0xff) == 8) {
                        uVar12 = *pbVar10;
                        uVar6 = 0;
                        pbVar10 = pbVar10 + 1;
                    }
                }
                iVar4 = (uVar17 & 4) * 6 + 4;
                uVar17 = uVar17 + 4;
                puVar13 = (u32*)((int)puVar13 + iVar4);
                puVar14 = (u32*)((int)puVar14 + iVar4);
            } while ((int)uVar17 < (int)*param_1);
            uVar15 = uVar15 + iVar11 * 2;
        }
        uVar6 = *param_1;
        *param_1 = (int)uVar6 >> 1;
    }
    DCFlushRange((void*)param_6, *param_1 * param_1[1] * 2);
    GXInvalidateTexAll();
    param_1[7] = param_1[7] + 1;
    return 0;
}

/* VQ tile decode variant (ReadU16LE, DCFlush/Invalidate, GXInvalidateTexAll) */
u32 fn_800D8BCC(u32* param_1, int param_2, char* param_3, int param_4, int param_5, u32 param_6) {
    u8 bVar1;
    u8 bVar2;
    u8 bVar3;
    u32 uVar4;
    u16 uVar7;
    u8* pbVar6;
    u8* pbVar8;
    u32 uVar9;
    u32 uVar10;
    int iVar11;
    u32 uVar12;
    u32 uVar13;
    int iVar14;
    int iVar15;
    u32* puVar16;
    u32* puVar17;
    u8* pbVar18;

    uVar7 = ReadU16LE((u8*)param_1[6]);
    uVar9 = uVar7;
    pbVar8 = (u8*)(param_1[6] + 4);
    pbVar18 = pbVar8 + uVar9 * 0xc;
    DCInvalidateRange((void*)param_6, *param_1 * param_1[1] * 2);
    if (param_4 == 1) {
        fn_800D860C((u32)param_1, pbVar8, uVar9);
        goto present;
    } else {
        if (param_4 < 1) {
            if (-1 < param_4) {
                fn_800D86C8((u32)param_1, pbVar8, uVar9);
                goto present;
            }
        } else if (param_4 < 3) {
            bVar3 = *((u8*)param_1 + 0x39);
            bVar1 = *(u8*)(param_1 + 0xe);
            bVar2 = *((u8*)param_1 + 0x37);
            iVar14 = 0;
            pbVar6 = pbVar8;
            for (iVar11 = 0; iVar11 < (int)(uVar9 << 2); iVar11 = iVar11 + 1) {
                fn_800DBE98((u32)param_1, pbVar6);
                *(u16*)(pbVar8 + iVar14) =
                    (u16)(((u32)pbVar6[2] >> (8 - bVar2 & 0x3f)) << *(u8*)(param_1 + 0xd)) |
                    (u16)(((u32)*pbVar6 >> (8 - bVar1 & 0x3f)) << *((u8*)param_1 + 0x36)) |
                    (u16)(((u32)pbVar6[1] >> (8 - bVar3 & 0x3f)) << *((u8*)param_1 + 0x35));
                pbVar6 = pbVar6 + 3;
                iVar14 = iVar14 + 2;
            }
            goto present;
        }
        return 0xffffffff;
    }
present:
    if (*(int*)(param_5 + 8) < 0) {
        iVar14 = -1;
        uVar12 = param_1[1] - 1;
    } else {
        uVar12 = 0;
        iVar14 = 1;
    }
    if (uVar9 < 0x101) {
        *param_1 = *param_1 << 1;
        for (iVar11 = 0; iVar11 < (int)param_1[1]; iVar11 = iVar11 + 2) {
            uVar9 = 0;
            puVar16 = (u32*)(param_6 + (uVar12 & 0xfffffffc) * *param_1 + (uVar12 & 3) * 8);
            puVar17 = puVar16 + iVar14 * 2;
            do {
                bVar3 = *pbVar18;
                uVar10 = uVar9 & 4;
                uVar9 = uVar9 + 4;
                iVar15 = uVar10 * 6 + 4;
                *puVar16 = *(u32*)(pbVar8 + (u32)bVar3 * 8);
                puVar16 = (u32*)((int)puVar16 + iVar15);
                pbVar18 = pbVar18 + 1;
                *puVar17 = *(u32*)(pbVar8 + (u32)bVar3 * 8 + 4);
                puVar17 = (u32*)((int)puVar17 + iVar15);
            } while ((int)uVar9 < (int)*param_1);
            uVar12 = uVar12 + iVar14 * 2;
        }
        uVar9 = *param_1;
        *param_1 = (int)uVar9 >> 1;
    } else {
        uVar13 = *param_1;
        pbVar6 = pbVar18 + 1;
        uVar10 = *pbVar18;
        uVar9 = ((int)uVar13 >> 1) * param_1[1];
        uVar9 = (int)uVar9 >> 1;
        *param_1 = uVar13 << 1;
        pbVar18 = pbVar18 + ((int)uVar9 >> 3);
        uVar9 = 0;
        for (iVar11 = 0; iVar11 < (int)param_1[1]; iVar11 = iVar11 + 2) {
            uVar13 = 0;
            puVar16 = (u32*)(param_6 + (uVar12 & 0xfffffffc) * *param_1 + (uVar12 & 3) * 8);
            puVar17 = puVar16 + iVar14 * 2;
            do {
                bVar3 = *pbVar18;
                uVar4 = uVar9 & 0x3f;
                uVar9 = uVar9 + 1;
                *puVar16 = *(u32*)(pbVar8 + (((int)uVar10 >> uVar4 & 1U) << 8 | (u32)bVar3) * 8);
                pbVar18 = pbVar18 + 1;
                *puVar17 = *(u32*)(pbVar8 + (((int)uVar10 >> uVar4 & 1U) << 8 | (u32)bVar3) * 8 + 4);
                if ((uVar9 & 0xff) == 8) {
                    uVar10 = *pbVar6;
                    uVar9 = 0;
                    pbVar6 = pbVar6 + 1;
                }
                iVar15 = (uVar13 & 4) * 6 + 4;
                uVar13 = uVar13 + 4;
                puVar16 = (u32*)((int)puVar16 + iVar15);
                puVar17 = (u32*)((int)puVar17 + iVar15);
            } while ((int)uVar13 < (int)*param_1);
            uVar12 = uVar12 + iVar14 * 2;
        }
        uVar9 = *param_1;
        *param_1 = (int)uVar9 >> 1;
    }
    DCFlushRange((void*)param_6, *param_1 * param_1[1] * 2);
    GXInvalidateTexAll();
    param_1[7] = param_1[7] + 1;
    return 0;
}

/* VQ chunk -> buffer copy (ReadU16LE/ReadF32LE, memcpy) */
u32 fn_800D8F28(int* param_1, int param_2, char* param_3, int param_4, u32 param_5) {
    u16 uVar4;
    int iVar1;
    u32 uVar2;
    u32 uVar3;
    int iVar5;
    int iVar6;
    u32 uVar7;
    u32 uVar8;
    u8* pbVar9;
    u8* pbVar10;
    int iVar11;
    int iVar12;

    uVar4 = ReadU16LE((u8*)param_1[6]);
    uVar2 = uVar4;
    iVar1 = ReadF32LE((u8*)(param_1[6] + 4));
    iVar12 = param_1[6] + 8;
    iVar11 = iVar12 + param_1[0xb];
    pbVar9 = (u8*)(iVar11 + uVar2 * 0xc);
    iVar6 = 0;
    for (iVar5 = 0; iVar5 < (int)uVar2; iVar5 = iVar5 + 1) {
        pbVar10 = (u8*)(iVar11 + iVar6);
        fn_800DBE98((u32)param_1, pbVar10);
        fn_800DBE98((u32)param_1, pbVar10 + 3);
        fn_800DBE98((u32)param_1, pbVar10 + 6);
        fn_800DBE98((u32)param_1, pbVar10 + 9);
        iVar6 = iVar6 + 0xc;
    }
    if (*(int*)(param_4 + 8) < 0) {
        iVar5 = *param_1 * -3;
        param_5 = param_5 + *param_1 * (param_1[1] - 1) * 3;
    } else {
        iVar5 = *param_1 * 3;
    }
    if (uVar2 < 0x101) {
        for (uVar2 = 0; (int)uVar2 < param_1[1]; uVar2 = uVar2 + 2) {
            iVar1 = param_1[10];
            for (iVar6 = 0; iVar6 < *param_1; iVar6 = iVar6 + 2) {
                if ((1 << (iVar6 >> 2 & 7) &
                     (u32)*(u8*)(iVar12 + ((int)uVar2 >> 2) * iVar1 + (iVar6 >> 5))) != 0) {
                    uVar7 = iVar11 + (u32)*pbVar9 * 0xc;
                    pbVar9 = pbVar9 + 1;
                    memcpy((void*)param_5, (void*)uVar7, 6);
                    memcpy((void*)(param_5 + iVar5), (void*)(uVar7 + 6), 6);
                }
                param_5 = param_5 + 6;
            }
            param_5 = param_5 + iVar5;
        }
    } else {
        uVar2 = iVar1 + 7;
        uVar7 = *pbVar9;
        pbVar10 = pbVar9 + ((int)uVar2 >> 3);
        uVar8 = 0;
        pbVar9 = pbVar9 + 1;
        for (uVar2 = 0; (int)uVar2 < param_1[1]; uVar2 = uVar2 + 2) {
            iVar1 = param_1[10];
            for (iVar6 = 0; iVar6 < *param_1; iVar6 = iVar6 + 2) {
                uVar3 = iVar6 >> 2;
                if ((1 << (uVar3 & 7) &
                     (u32)*(u8*)(iVar12 + ((int)uVar2 >> 2) * iVar1 + (iVar6 >> 5))) != 0) {
                    uVar3 = iVar11 + (((int)uVar7 >> (uVar8 & 0x3f) & 1U) << 8 | (u32)*pbVar10) * 0xc;
                    pbVar10 = pbVar10 + 1;
                    memcpy((void*)param_5, (void*)uVar3, 6);
                    memcpy((void*)(param_5 + iVar5), (void*)(uVar3 + 6), 6);
                    uVar8 = uVar8 + 1;
                    if ((uVar8 & 0xff) == 8) {
                        uVar7 = *pbVar9;
                        uVar8 = 0;
                        pbVar9 = pbVar9 + 1;
                    }
                }
                param_5 = param_5 + 6;
            }
            param_5 = param_5 + iVar5;
        }
    }
    param_1[7] = param_1[7] + 1;
    return 0;
}

/* VQ chunk -> buffer copy (ReadU16LE, memcpy) */
u32 fn_800D91B4(u32* param_1, int param_2, char* param_3, int param_4, u32 param_5) {
    u16 uVar2;
    u32 uVar1;
    int iVar3;
    int iVar4;
    int iVar5;
    u32 uVar6;
    u32 uVar7;
    u8* pbVar8;
    u8* pbVar9;
    int iVar10;

    uVar2 = ReadU16LE((u8*)param_1[6]);
    uVar1 = uVar2;
    iVar10 = param_1[6] + 4;
    pbVar8 = (u8*)(iVar10 + uVar1 * 0xc);
    iVar3 = 0;
    for (iVar5 = 0; iVar5 < (int)uVar1; iVar5 = iVar5 + 1) {
        pbVar9 = (u8*)(iVar10 + iVar3);
        fn_800DBE98((u32)param_1, pbVar9);
        fn_800DBE98((u32)param_1, pbVar9 + 3);
        fn_800DBE98((u32)param_1, pbVar9 + 6);
        fn_800DBE98((u32)param_1, pbVar9 + 9);
        iVar3 = iVar3 + 0xc;
    }
    if (*(int*)(param_4 + 8) < 0) {
        iVar3 = *param_1 * -3;
        param_5 = param_5 + *param_1 * (param_1[1] - 1) * 3;
    } else {
        iVar3 = *param_1 * 3;
    }
    if (uVar1 < 0x101) {
        for (iVar5 = 0; iVar5 < (int)param_1[1]; iVar5 = iVar5 + 2) {
            for (iVar4 = 0; iVar4 < (int)*param_1; iVar4 = iVar4 + 2) {
                uVar1 = iVar10 + (u32)*pbVar8 * 0xc;
                pbVar8 = pbVar8 + 1;
                memcpy((void*)param_5, (void*)uVar1, 6);
                memcpy((void*)(param_5 + iVar3), (void*)(uVar1 + 6), 6);
                param_5 = param_5 + 6;
            }
            param_5 = param_5 + iVar3;
        }
    } else {
        uVar1 = *param_1;
        uVar6 = *pbVar8;
        uVar7 = 0;
        uVar1 = ((int)uVar1 >> 1) * param_1[1];
        uVar1 = (int)uVar1 >> 1;
        pbVar9 = pbVar8 + ((int)uVar1 >> 3);
        pbVar8 = pbVar8 + 1;
        for (iVar5 = 0; iVar5 < (int)param_1[1]; iVar5 = iVar5 + 2) {
            for (iVar4 = 0; iVar4 < (int)*param_1; iVar4 = iVar4 + 2) {
                uVar1 = iVar10 + (((int)uVar6 >> (uVar7 & 0x3f) & 1U) << 8 | (u32)*pbVar9) * 0xc;
                pbVar9 = pbVar9 + 1;
                memcpy((void*)param_5, (void*)uVar1, 6);
                memcpy((void*)(param_5 + iVar3), (void*)(uVar1 + 6), 6);
                uVar7 = uVar7 + 1;
                if ((uVar7 & 0xff) == 8) {
                    uVar6 = *pbVar8;
                    uVar7 = 0;
                    pbVar8 = pbVar8 + 1;
                }
                param_5 = param_5 + 6;
            }
            param_5 = param_5 + iVar3;
        }
    }
    param_1[7] = param_1[7] + 1;
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
    u32 arg1 = param_2[1];
    u32 arg3 = param_2[3];
    u32 arg2 = param_2[2];
    u32 arg4 = param_2[4];

    fn_800D93D4(param_1, param_2[0], arg1, (char*)arg2, arg3, arg4);
}

void fn_800D9648(u32* param_1, u32* param_2) {
    u32 arg1 = param_2[1];
    u32 arg3 = param_2[3];
    u32 arg2 = param_2[2];
    u32 arg4 = param_2[4];

    fn_800D93D4(param_1, param_2[0], arg1, (char*)arg2, arg3, arg4);
}

void fn_800D967C(register int param_1, register int param_2) {
    register u32 arg3;
    register u32 arg2;
    register void (*dispatch)(int, u32, u32);

    asm {
        lwz dispatch, 32(param_1)
        lwz arg3, 12(param_2)
        lwz dispatch, 12(dispatch)
        lwz arg2, 4(param_2)
    }
    dispatch(param_1, arg2, arg3);
}

/* allocator (AllocHiMem + ResetAllocTot/__unexpected) */
void fn_800D96B0(void) {
}

u32 MovieValidateFrameFormat(u32 param_1, int param_2) {
    int uVar1;
    u32 iVar2;
    int iVar3;
    int uVar4;
    int inputWidth;
    int inputHeight;

    iVar3 = *(int*)(param_2 + 4);
    iVar2 = *(u32*)(param_2 + 0xc);
    uVar1 = *(int*)(iVar3 + 4);
    uVar4 = *(int*)(iVar3 + 8);
    if (uVar1 % 4 != 0 || uVar4 % 4 != 0) {
        return 0xfffffffe;
    }
    if (*(int*)(iVar3 + 0x10) != 0x5644564d || *(u16*)(iVar3 + 0xe) != 0x18) {
        return 0xfffffffe;
    }
    if (iVar2 == 0) {
        return 0;
    }
    inputWidth = *(int*)(iVar2 + 4);
    inputHeight = *(int*)(iVar2 + 8);
    if (inputWidth != uVar1 ||
        (inputHeight != uVar4 && inputHeight != -uVar4)) {
        return 0xfffffffe;
    }
    switch (*(int*)(iVar2 + 0x10)) {
    case 0:
        if (*(u16*)(iVar2 + 0xe) != 0x18 && *(u16*)(iVar2 + 0xe) != 0x10) {
            return 0xfffffffe;
        }
        break;
    case 3:
        if (*(u16*)(iVar2 + 0xe) != 0x10) {
            return 0xfffffffe;
        }
        break;
    case 0x32595559:
    case 0x59565955:
        if (*(u16*)(iVar2 + 0xe) != 0x10) {
            return 0xfffffffe;
        }
        break;
    default:
        return 0xfffffffe;
    }
    return 0;
}

u32 fn_800D99AC(u32 a, int* src, u8* dst) {
    register u32 r;
    register u8* out = dst;

    if (out == 0) {
        r = 56;
    } else {
        memcpy(out, src, *src);
        asm {
            lwz r0,4(out)
            li r5,24
            lwz r6,8(out)
            li r4,0
            li r,0
            mullw r0,r0,r6
            sth r5,14(out)
            stw r4,16(out)
            mulli r0,r0,3
            stw r0,20(out)
        }
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

#pragma dont_inline on
void fn_800D9C5C(int* p, int n) {
    s32 tag;

    if (p[0] != 0) {
        gMovieAllocCount--;
        if (gMovieAllocCount == 0) {
            ResetAllocTot();
        }
    }
    p[0] = 0;
    p[1] = n;
    tag = gMovieAllocCount;
    gMovieAllocCount++;
    p[0] = (s32)AllocHiMem(p[1], tag);
    p[3] = 0;
    p[2] = 0;
}
#pragma dont_inline off

int* fn_800D9CF4(int* p, s16 releaseAgain) {
    if (p != NULL) {
        if (*p != 0) {
            gMovieAllocCount--;
            if (gMovieAllocCount == 0) {
                ResetAllocTot();
            }
        }
        if (releaseAgain > 0 && p != NULL) {
            gMovieAllocCount--;
            if (gMovieAllocCount == 0) {
                ResetAllocTot();
            }
        }
    }
    return p;
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
void fn_800D9F20(int param_1) {
    u32 uVar1;
    u32 uVar2;
    u32 requestSize;
    u32 requestOffset;
    u8* requestData;

    if (*(u8*)(param_1 + 0x14) != 0) {
        adsPoll();
        if (*(u32*)(param_1 + 8) != 0) {
            uVar1 = sndCmd17(
                (*(int*)(param_1 + 4) + *(int*)(param_1 + 0x10)) -
                    *(u32*)(param_1 + 8),
                *(u32*)(param_1 + 8));
            *(u32*)(param_1 + 8) = *(u32*)(param_1 + 8) - uVar1;
        }
        if (*(u32*)(param_1 + 8) == 0) {
            uVar2 = *(int*)(gMovieStreamState + 0x108) - *(int*)(param_1 + 0xc);
            if (0xc000 < uVar2) {
                uVar2 = 0xc000;
            }
            *(u32*)(param_1 + 0x10) = uVar2;
            requestSize = *(u32*)(param_1 + 0x10);
            requestOffset = *(u32*)(param_1 + 0xc);
            requestData = *(u8**)(param_1 + 4);
            if ((u8)fn_800DB2F4((int)(gMovieStreamState + 0x20), requestData,
                                requestOffset, requestSize)) {
                *(u32*)(param_1 + 8) = *(u32*)(param_1 + 0x10);
                *(int*)(param_1 + 0xc) = *(int*)(param_1 + 0xc) + *(int*)(param_1 + 0x10);
            }
        }
    }
}

/* 0x800D9FEC top-level VQ movie playback loop: sets up GX/TEV, decodes+presents each frame (DEMODoneRender/DEMOSwapBuffers), polls pads (G3DGetPadStatusBuffer) to allow skipping, pumps audio (adsPoll/sndCmd17). Xbox: PlayVQMovie. Called by test_movies. */
void PlayVQMovie(void) {
}

/* movie close/cleanup (AudioStreamStop, operator delete, sceClose) */
extern void AudioStreamStop(void);
extern s32 sceClose(s32 fd);
void fn_800DBA80(u8* dec, s32 fd);
void __dl__FPv(void* p);
void __dla__FPv(void* p);

#pragma dont_inline on
void fn_800DA60C(register u8* m)
{
    u8* strm;

    if (*(u32*)((u32)m + 400) != 0) {
        AudioStreamStop();
        if ((strm = *(u8**)(m + 400)) != 0) {
            AudioStreamStop();
            __dla__FPv(*(void**)(strm + 4));
            __dl__FPv(strm);
        }
        *(u32*)(m + 400) = 0;
    }
    {
        register u8* object = m + 336;
        register void (*dispatch)(u8*);
        asm {
            lwz dispatch, 368(m)
            lwz dispatch, 28(dispatch)
        }
        dispatch(object);
    }
    fn_800DBA80(m + 32, *(s32*)(m + 28));
    if (*(s32*)(m + 28) != 0) {
        sceClose(*(s32*)(m + 28));
    }
    *(s32*)(m + 28) = 0;
}
#pragma dont_inline off

typedef struct MovieAudioState {
    s32 command;
    u8* buffer;
    s32 remaining;
    s32 offset;
    s32 requestSize;
    u8 active;
    u8 _pad[3];
} MovieAudioState;

/* Advance the VQ stream by one presentation interval and prime its audio. */
u32 fn_800DA6A4(register u8* movie, register u32 decodeFrame, f32 elapsed)
{
    u8 unused[24];
    register u32 audioSize;
    register MovieAudioState* audio;
    u32 frame;
    register u32* chunk;

    if (*(s32*)(movie + 0x1C) == 0 || movie[0x1A] != 0) {
        return FALSE;
    }
    if (gMovieFrameTimeReset != 0) {
        elapsed = lbl_80349390;
        gMovieFrameTimeReset = 0;
    }
    if (elapsed > lbl_803493B8) {
        elapsed = lbl_803493BC;
    }

    audioSize = 0x10000;
    fn_800DB3D4((u32*)(movie + 0x20), *(s32*)(movie + 0x1C), audioSize - 0x6000);
    if (movie[0x19] != 0) {
        if (movie[0x18] != 0) {
            s32 tag = gMovieAllocCount++;
            audio = AllocHiMem(sizeof(MovieAudioState), tag);
            if (audio != NULL) {
                audio->command = sndCmd16(audioSize - 0x4000);
                tag = gMovieAllocCount++;
                audio->buffer = AllocHiMem(audioSize - 0x4000, tag);
                audio->requestSize = 0;
                audio->offset = 0;
                audio->remaining = 0;
                audio->active = 0;
            }
            *(MovieAudioState**)(movie + 0x190) = audio;
            audio = *(MovieAudioState**)(movie + 0x190);
            audio->active = (u8)fn_800DB2F4((s32)(gMovieStreamState + 0x20), audio->buffer, 0, 0xC000);
            if (audio->active != 0) {
                audio->offset = 0xC000;
                audio->remaining = 0xC000;
                audio->remaining -= sndCmd17((s32)audio->buffer, 0x6000);
                audio->remaining -= sndCmd17((s32)(audio->buffer + 0x6000), 0x6000);
                fn_800D9F20((s32)audio);
                sndCmdA(lbl_80343B4C, 0, 1, gMovieAudioCallback);
            }
        }
        movie[0x19] = 0;
    } else {
        *(f32*)(movie + 4) += elapsed;
    }

    frame = __cvt_fp2unsigned(*(f32*)(movie + 4) / *(f32*)(movie + 8));
    if (frame <= *(u32*)(movie + 0x10)) {
        goto done;
    }
    {
        ++*(u32*)(movie + 0x10);
        fn_800DB36C((s32)(movie + 0x20));
        asm { mr chunk, r3 }
        while (chunk != NULL && chunk[8] == 0) {
            fn_800DB29C((s32)(movie + 0x20));
            fn_800DB36C((s32)(movie + 0x20));
            asm { mr chunk, r3 }
        }
        if (chunk == NULL) {
            return *(u32*)(movie + 0x10) < *(u32*)(movie + 0xD4);
        }
        if (decodeFrame == 0) {
            fn_800DB29C((s32)(movie + 0x20));
            return TRUE;
        }
        *(u32*)(movie + 0x1A8) = chunk[4];
        *(u32*)(movie + 0x124) = chunk[8];
        *(s32*)(movie + 0x12C) = decodeFrame;
        {
            register void (*decode)(u8*, u8*, s32);
            asm {
                lwz decode, 0x170(movie)
                lwz decode, 0x18(decode)
            }
            decode(movie + 0x150, movie + 0x11C, 0);
        }
        fn_800DB29C((s32)(movie + 0x20));
    }
done:
    return TRUE;
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

u32* fn_800DB0F8(u32* volatile p) {
    u32* self = p;

    self[0] = (u32)lbl_801296A4;
    self[0] = (u32)lbl_8012968C;
    fn_800DBC64(self + 8);
    fn_800DBE04(self + 0x54);
    self[7] = 0;
    self[100] = 0;
    return self;
}

/* operator delete[] (weak, emitted into this TU) */
#pragma dont_inline on
void __dla__FPv(void* p) {
    if (p != NULL) {
        gMovieAllocCount--;
        if (gMovieAllocCount == 0) {
            ResetAllocTot();
        }
    }
}
#pragma dont_inline off

/* operator delete (weak, emitted into this TU) */
void __dl__FPv(void* p) {
    if (p != NULL) {
        gMovieAllocCount--;
        if (gMovieAllocCount == 0) {
            ResetAllocTot();
        }
    }
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
void fn_800DB3D4(u32* stream, s32 fd, u32 length) {
}

void fn_800DB82C(u32* param_1, int param_2, u32 param_3) {
    int iVar2;

    param_1[0xb] = sceLseek(param_2, 0, 2);
    sceLseek(param_2, param_3, 0);
    param_1[10] = param_3;
    param_1[7] = (param_1[6] - 0x2000) & 0xfffff800;
    sceRead(param_2, (void*)param_1[0], param_1[7]);
    param_1[0x14] = param_1[0x16];
    param_1[0x16] = *(u32*)param_1[0x16];
    *(u32*)param_1[0x14] = 0;
    *(u32*)(param_1[0x14] + 8) = 0;
    iVar2 = ReadF32LE((u8*)(param_1[0] + 4));
    *(int*)(param_1[0x14] + 4) = iVar2 + 8;
    *(u32*)(param_1[0x14] + 4) =
        *(u32*)(param_1[0x14] + 4) + (*(u32*)(param_1[0x14] + 4) & 1);
    *(u32*)(param_1[0x14] + 0x20) = 0;
    *(u32*)(param_1[0x14] + 0x24) = 0;
    *(u32*)(param_1[0x14] + 0x18) = 0;
    param_1[0xd] = 0;
    *(u8*)(param_1 + 0x13) = 0;
}

u8 MovieDecoderInitBuffers(u32* param_1, u32 param_2, u32 param_3) {
    int iVar3;
    int iVar4;
    u8 unused[24];

    __dla__FPv((void*)param_1[1]);
    param_1[1] = 0;
    param_1[0] = 0;
    __dla__FPv((void*)param_1[3]);
    param_1[3] = 0;
    param_1[2] = 0;
    __dla__FPv((void*)param_1[0x15]);
    param_1[6] = 0;
    param_1[5] = 0;
    param_1[4] = 0;
    param_1[8] = 0;
    param_1[9] = 0;
    param_1[0x16] = 0;
    param_1[0x14] = 0;
    param_1[0x15] = 0;
    if ((param_3 & 0xff) != 0) {
        fn_800D9C5C((int*)(param_1 + 0xf), 0x40000);
    }
    param_1[6] = param_2 & 0xfffff800;
    iVar3 = param_1[6];
    gMovieAllocCount++;
    param_1[1] = (u32)AllocHiMem(iVar3 + 0x20, iVar3);
    iVar3 = gMovieAllocCount;
    gMovieAllocCount++;
    param_1[3] = (u32)AllocHiMem(0x10020, iVar3);
    param_1[0] = param_1[1] + 0x20 & 0xffffffe0;
    param_1[2] = param_1[3] + 0x20 & 0xffffffe0;
    iVar3 = gMovieAllocCount;
    gMovieAllocCount++;
    param_1[0x15] = (u32)AllocHiMem(0x2800, iVar3);
    param_1[0x16] = param_1[0x15];
    for (iVar4 = 0; iVar4 < 255; iVar4++) {
        *(u32*)(param_1[0x15] + iVar4 * 0x28) = param_1[0x15] + (iVar4 + 1) * 0x28;
    }
    *(u32*)(param_1[0x15] + iVar4 * 0x28) = 0;
    return param_1[0] != 0;
}

void fn_800DBA80(u8* dec, s32 fd) {
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
        for (i = 0; i < 256; i++) {
            lbl_80321340[i] = (i * 31 + 128) / 255;
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

/* Destroy a DText renderer and optionally release the object itself. */
u32* fn_800DBF6C(u32* self, s16 deleting) {
    u8 unused[64];

    if (self != NULL) {
        self[8] = (u32)lbl_801296F0;
        gDTextInitCount--;
        if (self[6] != 0) {
            gMovieAllocCount--;
            if (gMovieAllocCount == 0) {
                ResetAllocTot();
            }
        }
        if (deleting > 0 && self != NULL) {
            gMovieAllocCount--;
            if (gMovieAllocCount == 0) {
                ResetAllocTot();
            }
        }
    }
    return self;
}

/* 0x800DC034 init the DText debug-overlay 256-entry colour ramp (gDTextColorRamp/gDTextBuf) */
typedef struct DTextRampEntry {
    u8 _pad[768];
    u8 value;
} DTextRampEntry;

#pragma opt_propagation off
u32* DTextInitColorRamp(u32* p) {
    DTextRampEntry* ramp;

    p[8] = (u32)lbl_801296F0;
    ramp = (DTextRampEntry*)gDTextColorRamp;
    if (gDTextInitCount == 0) {
        int i;
        DTextRampEntry* entry;
        int divisor;
        memset(ramp, 0, 256);
        memset((u8*)ramp + 512, 255, 256);
        for (i = 0; i < 256; i++) {
            gDTextBuf[i] = i;
        }
        divisor = 31;
        for (i = 0; i < 32; i++) {
            entry = (DTextRampEntry*)((u8*)ramp + i);
            entry->value = (i * 255 + 16) / divisor;
        }
    }
    gDTextInitCount++;
    p[6] = 0;
    return p;
}
#pragma opt_propagation reset
