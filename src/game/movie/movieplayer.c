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
extern void __unexpected(void* catchInfo);
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
extern s32 sceClose(s32 fd);
extern u8 sceFileExists(const char* path);
extern int sceOpen(const char* path, ...);
extern void sysAssertFailed(const char* expression, const char* file, int line);

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
extern const f32 lbl_80349394;
extern const f64 lbl_803493B0;
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
void fn_800DB82C(u32* stream, int fd, u32 offset);
u32 fn_800DACD8(int movie, u8* header);
u8 MovieDecoderInitBuffers(u32* decoder, u32 size, u32 hasAudio);
void fn_800D9F20(int audio);
u32* fn_800DBF6C(u32* self, s16 deleting);
u8 fn_800DBCCC(void* self, s32 x);
u8 fn_800DBD00(void* self, s32 x);
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

#pragma dont_inline on
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

#pragma dont_inline off

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
    int count;
    int nbits;
    u8* hdr8;
    u8* pal;
    u8* ip;

    count = ReadU16LE((u8*)param_1[6]);
    nbits = ReadF32LE((u8*)param_1[6] + 4);
    hdr8 = (u8*)(param_1[6] + 8);
    pal = hdr8 + param_1[0xB];
    ip = pal + count * 12;
    DCInvalidateRange((void*)param_6, param_1[0] * param_1[1] * 2);
    switch (param_4) {
    case 0:
        fn_800D86C8((u32)param_1, pal, count);
        break;
    case 1:
        fn_800D860C((u32)param_1, pal, count);
        break;
    case 2: {
        int i;
        u8* p;
        u8 sh1;
        u8 sh0;
        u8 sh2;
        int n;
        sh1 = 8 - *((u8*)param_1 + 0x39);
        sh0 = 8 - *((u8*)param_1 + 0x38);
        sh2 = 8 - *((u8*)param_1 + 0x37);
        n = count * 4;
        i = 0;
        p = pal;
        for (; i < n; i++) {
            fn_800DBE98((u32)param_1, p);
            *(u16*)(pal + i * 2) = (((p[0] >> sh0) << *((u8*)param_1 + 0x36))
                                | ((p[1] >> sh1) << *((u8*)param_1 + 0x35)))
                                | ((p[2] >> sh2) << *((u8*)param_1 + 0x34));
            p += 3;
        }
        break;
    }
    default:
        return -1;
    }

    {
        int dir;
        int row;

        if (*(int*)(param_5 + 8) < 0) {
            dir = -1;
            row = param_1[1] - 1;
        } else {
            row = 0;
            dir = 1;
        }
        if (count > 0x100) {
            u8 bits;
            u8 nb;
            u8* bp;
            int d8;
            int d2;
            int y;

            param_1[0] <<= 1;
            bits = *ip;
            bp = ip + 1;
            ip += (nbits + 7) / 8;
            d8 = dir * 8;
            d2 = dir * 2;
            nb = 0;
            for (y = 0; y < (int)param_1[1]; y += 2) {
                u8* dst;
                u8* dst2;
                u8* brow;
                int x;

                dst = (u8*)param_6 + (row & ~3) * param_1[0];
                dst += (row & 3) * 8;
                dst2 = dst + d8;
                brow = hdr8 + (y / 4) * param_1[10];
                x = 0;
                do {
                    int b;
                    int adv;

                    b = x >> 3;
                    if ((1 << (b & 7)) & brow[b / 8]) {
                        u32 idx;
                        u32 val;
                        u8* entry;

                        idx = *ip;
                        val = idx;
                        val |= ((bits >> nb) & 1) << 8;
                        entry = pal + val * 8;
                        *(u32*)dst = *(u32*)entry;
                        nb++;
                        ip++;
                        *(u32*)dst2 = *(u32*)(entry + 4);
                        if (nb == 8) {
                            bits = *bp;
                            nb = 0;
                            bp++;
                        }
                    }
                    adv = (x & 4) * 6 + 4;
                    x += 4;
                    dst += adv;
                    dst2 += adv;
                } while (x < (int)param_1[0]);
                row += d2;
            }
            param_1[0] = (int)param_1[0] / 2;
        } else {
            int d8;
            int d2;
            int y;

            d8 = dir * 8;
            d2 = dir * 2;
            param_1[0] <<= 1;
            for (y = 0; y < (int)param_1[1]; y += 2) {
                u8* dst;
                u8* dst2;
                u8* brow;
                int x;

                dst = (u8*)param_6 + (row & ~3) * param_1[0];
                dst += (row & 3) * 8;
                dst2 = dst + d8;
                brow = hdr8 + (y / 4) * param_1[10];
                x = 0;
                do {
                    int b;
                    int adv;

                    b = x >> 3;
                    if ((1 << (b & 7)) & brow[b / 8]) {
                        u32 idx;
                        u8* entry;

                        idx = *ip;
                        ip++;
                        entry = pal + idx * 8;
                        *(u32*)dst = *(u32*)entry;
                        *(u32*)dst2 = *(u32*)(entry + 4);
                    }
                    adv = (x & 4) * 6 + 4;
                    x += 4;
                    dst += adv;
                    dst2 += adv;
                } while (x < (int)param_1[0]);
                row += d2;
            }
            param_1[0] = (int)param_1[0] / 2;
        }
    }
    DCFlushRange((void*)param_6, param_1[0] * param_1[1] * 2);
    GXInvalidateTexAll();
    param_1[7] = param_1[7] + 1;
    return 0;
}

/* VQ tile decode variant (ReadU16LE, DCFlush/Invalidate, GXInvalidateTexAll) */
u32 fn_800D8BCC(u32* param_1, int param_2, char* param_3, int param_4, int param_5, u32 param_6) {
    int count;
    u8* pal;
    u8* ip;

    count = ReadU16LE((u8*)param_1[6]);
    pal = (u8*)(param_1[6] + 4);
    ip = pal + count * 12;
    DCInvalidateRange((void*)param_6, param_1[0] * param_1[1] * 2);
    switch (param_4) {
    case 0:
        fn_800D86C8((u32)param_1, pal, count);
        break;
    case 1:
        fn_800D860C((u32)param_1, pal, count);
        break;
    case 2: {
        int i;
        u8* p;
        u8 sh1;
        u8 sh0;
        u8 sh2;
        int n;
        sh1 = 8 - *((u8*)param_1 + 0x39);
        sh0 = 8 - *((u8*)param_1 + 0x38);
        sh2 = 8 - *((u8*)param_1 + 0x37);
        n = count * 4;
        i = 0;
        p = pal;
        for (; i < n; i++) {
            fn_800DBE98((u32)param_1, p);
            ((u16*)pal)[i] = (((p[0] >> sh0) << *((u8*)param_1 + 0x36))
                            | ((p[1] >> sh1) << *((u8*)param_1 + 0x35)))
                            | ((p[2] >> sh2) << *((u8*)param_1 + 0x34));
            p += 3;
        }
        break;
    }
    default:
        return -1;
    }

    {
        int dir;
        int row;

        if (*(int*)(param_5 + 8) < 0) {
            dir = -1;
            row = param_1[1] - 1;
        } else {
            row = 0;
            dir = 1;
        }
        if (count > 0x100) {
            u8 bits;
            u8 nb;
            u8* bp;
            int d8;
            int d2;
            int y;
            int w;

            w = param_1[0];
            bp = ip + 1;
            d8 = dir * 8;
            bits = *ip;
            ip += ((w / 2) * (int)param_1[1]) / 2 / 8;
            param_1[0] = w << 1;
            d2 = dir * 2;
            nb = 0;
            for (y = 0; y < (int)param_1[1]; y += 2) {
                u8* dst;
                u8* dst2;
                int x;

                dst = (u8*)param_6 + (row & ~3) * param_1[0];
                dst += (row & 3) * 8;
                dst2 = dst + d8;
                x = 0;
                do {
                    u32 idx;
                    u32 val;
                    u8* entry;
                    int adv;

                    idx = *ip;
                    val = idx;
                    val |= ((bits >> nb) & 1) << 8;
                    entry = pal + val * 8;
                    *(u32*)dst = *(u32*)entry;
                    nb++;
                    ip++;
                    *(u32*)dst2 = *(u32*)(entry + 4);
                    if (nb == 8) {
                        bits = *bp;
                        nb = 0;
                        bp++;
                    }
                    adv = (x & 4) * 6 + 4;
                    x += 4;
                    dst += adv;
                    dst2 += adv;
                } while (x < (int)param_1[0]);
                row += d2;
            }
            param_1[0] = (int)param_1[0] / 2;
        } else {
            int d8;
            int d2;
            int y;

            d8 = dir * 8;
            d2 = dir * 2;
            param_1[0] <<= 1;
            for (y = 0; y < (int)param_1[1]; y += 2) {
                u8* dst;
                u8* dst2;
                int x;

                dst = (u8*)param_6 + (row & ~3) * param_1[0];
                dst += (row & 3) * 8;
                dst2 = dst + d8;
                x = 0;
                do {
                    u32 idx;
                    u8* entry;
                    int adv;

                    idx = *ip;
                    ip++;
                    entry = pal + idx * 8;
                    *(u32*)dst = *(u32*)entry;
                    *(u32*)dst2 = *(u32*)(entry + 4);
                    adv = (x & 4) * 6 + 4;
                    x += 4;
                    dst += adv;
                    dst2 += adv;
                } while (x < (int)param_1[0]);
                row += d2;
            }
            param_1[0] = (int)param_1[0] / 2;
        }
    }
    DCFlushRange((void*)param_6, param_1[0] * param_1[1] * 2);
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
#pragma opt_propagation off
u32 fn_800D93D4(u32* param_1, u32 param_2, int param_3, char* param_4, int param_5, u32 param_6) {
    int iVar4;
    int iVar1;
    u8 hasAlpha;
    u8 auStack_20[8];

    iVar4 = *(int*)(param_3 + 0x14);
    iVar1 = ReadF32LE((u8*)param_4);
    if (1 < iVar1) {
        iVar4 = ReadF32LE((u8*)param_4);
        param_4 = param_4 + 8;
    }
    fn_800D9DBC((u32)auStack_20, param_4, iVar4, (u8*)param_1[6]);
    hasAlpha = (u16)ReadU16LE((u8*)(param_1[6] + 2)) != 0;
    if (hasAlpha == 0) {
        switch (*(int*)(param_5 + 0x10)) {
        case 0:
        case 3:
            if (*(u16*)(param_5 + 0xe) == 0x18) {
                return fn_800D8F28((int*)param_1, param_3, param_4, param_5, param_6);
            }
            if (*(u16*)(param_5 + 0xe) == 0x10) {
                return fn_800D87FC(param_1, param_3, param_4, 2, param_5, param_6);
            }
            break;
        case 0x59565955:
            return fn_800D87FC(param_1, param_3, param_4, 0, param_5, param_6);
        case 0x32595559:
            return fn_800D87FC(param_1, param_3, param_4, 1, param_5, param_6);
        }
    } else {
        switch (*(int*)(param_5 + 0x10)) {
        case 0:
        case 3:
            if (*(u16*)(param_5 + 0xe) == 0x18) {
                return fn_800D91B4(param_1, param_3, param_4, param_5, param_6);
            }
            if (*(u16*)(param_5 + 0xe) == 0x10) {
                return fn_800D8BCC(param_1, param_3, param_4, 2, param_5, param_6);
            }
            break;
        case 0x59565955:
            return fn_800D8BCC(param_1, param_3, param_4, 0, param_5, param_6);
        case 0x32595559:
            return fn_800D8BCC(param_1, param_3, param_4, 1, param_5, param_6);
        }
    }
    return 0xffffffff;
}
#pragma opt_propagation reset

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

    dispatch = *(void (**)(int, u32, u32))(*(u32*)(param_1 + 32) + 12);
    arg3 = *(u32*)(param_2 + 12);
    arg2 = *(u32*)(param_2 + 4);
    dispatch(param_1, arg2, arg3);
}

/* Initialize a VQ frame buffer and its 16-bit component selectors. */
u32 fn_800D96B0(u32* self, u32 unused, u8* header)
{
    s32 height;
    s32 halfPixels;
    s32 size;

    (void)unused;
    self[0] = *(u32*)(header + 4);
    height = *(s32*)(header + 8);
    self[1] = height < 0 ? (u32)-height : (u32)height;
    self[2] = *(u16*)(header + 14);
    self[10] = ((s32)self[0] + 31) / 32;
    self[11] = ((s32)self[1] / 4) * self[10];
    halfPixels = (((s32)self[0] / 2) * (s32)self[1]) / 2;
    size = halfPixels + 6146;
    size += halfPixels / 8;
    size += self[11];

    if (*(u16*)(header + 14) == 16) {
        if (*(u32*)(header + 16) == 0) {
            ((u8*)self)[52] = 0;
            ((u8*)self)[55] = 5;
            ((u8*)self)[53] = 5;
            ((u8*)self)[57] = 5;
            ((u8*)self)[54] = 10;
            ((u8*)self)[56] = 5;
        } else if (*(u32*)(header + 16) == 3) {
            ((u8*)self)[52] = fn_800DBD00(self, *(s32*)(header + 40));
            ((u8*)self)[55] = fn_800DBCCC(self, *(s32*)(header + 40));
            ((u8*)self)[53] = fn_800DBD00(self, *(s32*)(header + 44));
            ((u8*)self)[57] = fn_800DBCCC(self, *(s32*)(header + 44));
            ((u8*)self)[54] = fn_800DBD00(self, *(s32*)(header + 48));
            ((u8*)self)[56] = fn_800DBCCC(self, *(s32*)(header + 48));
        }
    }

    if (self[6] != 0) {
        gMovieAllocCount--;
        if (gMovieAllocCount == 0) {
            ResetAllocTot();
        }
    }
    self[6] = (u32)AllocHiMem((u32)size + 308, (u32)gMovieAllocCount++);
    self[7] = 0;
    return 0;
}
#ifdef __MWERKS__
#pragma optimization_level 4
#pragma peephole on
#pragma scheduling on
#endif

s32 MovieValidateFrameFormat(u32 param_1, int param_2, s32 unused) {
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

#pragma opt_propagation off
u32 fn_800D99AC(u32 a, int* src, u8* dst) {
    u32 r;

    if (dst == 0) {
        r = 56;
    } else {
        u32 width;
        u32 height;

        memcpy(dst, src, *src);
        width = *(u32*)(dst + 4);
        a = 24;
        height = *(u32*)(dst + 8);
        src = (int*)0;
        r = 0;
        *(u16*)(dst + 14) = a;
        *(u32*)(dst + 16) = (u32)src;
        *(u32*)(dst + 20) = width * height * 3;
    }
    return r;
}
#pragma opt_propagation reset

u32 fn_800D9A14(u32* param_1, u8* param_2, int param_3, u8 param_4) {
    u32 writeOffset;
    int used;
    u32 readOffset;
    int chunk;

    writeOffset = param_1[2];
    readOffset = param_1[3];
    if ((int)writeOffset >= (int)readOffset) {
        used = writeOffset - readOffset;
    } else {
        used = param_1[1] + (writeOffset - readOffset);
    }
    if (param_3 > used) {
        return 0;
    }
    if (param_4 != 0) {
        chunk = param_1[1] - readOffset;
        if (chunk > param_3) {
            chunk = param_3;
        }
        memcpy(param_2, (u8*)(*param_1 + readOffset), chunk);
        param_2 += chunk;
        param_3 -= chunk;
        param_1[3] += chunk;
        if ((int)param_1[3] == (int)param_1[1]) {
            param_1[3] = 0;
        }
        if (param_3 != 0) {
            memcpy(param_2, (u8*)*param_1, param_3);
            param_1[3] += param_3;
        }
    } else {
        chunk = param_1[1] - readOffset;
        if (chunk > param_3) {
            chunk = param_3;
        }
        memcpy(param_2, (u8*)(*param_1 + readOffset), chunk);
        param_3 -= chunk;
        param_2 += chunk;
        if (param_3 != 0) {
            memcpy(param_2, (u8*)*param_1, param_3);
        }
    }
    return 1;
}

u32 fn_800D9B48(u32* param_1, u8* param_2, int param_3) {
    u32 readOffset;
    int used;
    u32 writeOffset;
    int chunk;

    writeOffset = param_1[2];
    readOffset = param_1[3];
    if ((int)writeOffset >= (int)readOffset) {
        used = writeOffset - readOffset;
    } else {
        used = param_1[1] + (writeOffset - readOffset);
    }
    if (param_3 > (int)((param_1[1] - used) - 1)) {
        return 0;
    }
    chunk = param_1[1] - writeOffset;
    if (chunk > param_3) {
        chunk = param_3;
    }
    memcpy((u8*)(*param_1 + writeOffset), param_2, chunk);
    param_2 += chunk;
    param_3 -= chunk;
    param_1[2] += chunk;
    if ((int)param_1[2] == (int)param_1[1]) {
        param_1[2] = 0;
    }
    if (param_3 != 0) {
        memcpy((u8*)*param_1, param_2, param_3);
        param_1[2] += param_3;
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
    if (*(u32*)p != 0) {
        gMovieAllocCount--;
        if (gMovieAllocCount == 0) {
            ResetAllocTot();
        }
    }
    p[0] = 0;
    p[1] = n;
    p[0] = (int)AllocHiMem(p[1], (u32)gMovieAllocCount++);
    p[3] = 0;
    p[2] = 0;
}
#pragma dont_inline off

int* fn_800D9CF4(int* p, s16 releaseAgain) {
    if (p != 0) {
        if (*(u32*)p != 0) {
            gMovieAllocCount--;
            if (gMovieAllocCount == 0) {
                ResetAllocTot();
            }
        }
        if (releaseAgain > 0 && p != 0) {
            gMovieAllocCount--;
            if (gMovieAllocCount == 0) {
                ResetAllocTot();
            }
        }
    }
    return p;
}

#ifdef __MWERKS__
#pragma optimization_level 4
#pragma peephole on
#pragma scheduling on
#endif

#pragma dont_inline on
void fn_800D9DA4(u32* p) {
    p[1] = 0;
    p[0] = 0;
    p[3] = 0;
    p[2] = 0;
}
#pragma dont_inline off

int fn_800D9DBC(u32 param_1, char* param_2, int param_3, u8* param_4) {
    int outLen;

    (void)param_1;
    fn_800D9DF0(param_2, param_3, param_4, &outLen);
    return outLen;
}

#ifdef __MWERKS__
#pragma optimization_level 4
#pragma peephole on
#pragma scheduling on
#endif

void fn_800D9DF0(char* param_1, int param_2, u8* param_3, int* param_4) {
    u8* pbVar4;
    u8* pbVar5;
    u8* end;
    u8* limit;
    u32 uVar6;
    int iVar7;
    u8 bVar2;

    uVar6 = 1;
    pbVar4 = (u8*)(param_1 + 4);
    pbVar5 = param_3;
    end = (u8*)(param_1 + param_2);
    limit = end - 0x20;

    if (*(u8*)param_1 == 1) {
        memcpy(param_3, pbVar4, param_2 - 4);
        *param_4 = param_2 - 4;
    } else {
        while (pbVar4 != end) {
            if (uVar6 == 1) {
                bVar2 = *pbVar4;
                uVar6 = bVar2 | 0x10000;
                uVar6 |= (u32)pbVar4[1] << 8;
                pbVar4 = pbVar4 + 2;
            }
            if (pbVar4 <= limit) {
                iVar7 = 16;
            } else {
                iVar7 = 1;
            }
            while (iVar7-- != 0) {
                if ((uVar6 & 1) != 0) {
                    u32 uVar3;
                    u8* pbVar8;

                    pbVar8 = pbVar5;
                    bVar2 = *pbVar4;
                    uVar3 = pbVar4[1];
                    pbVar4 = pbVar4 + 2;
                    pbVar8 -= ((bVar2 & 0xf0) << 4) | uVar3;
                    uVar3 = bVar2 & 0xf;
                    *pbVar5 = *pbVar8;
                    pbVar5[1] = pbVar8[1];
                    bVar2 = pbVar8[2];
                    pbVar8 = pbVar8 + 3;
                    pbVar5[2] = bVar2;
                    pbVar5 = pbVar5 + 3;
                    for (; uVar3 != 0; uVar3 = uVar3 - 1) {
                        bVar2 = *pbVar8;
                        pbVar8 = pbVar8 + 1;
                        *pbVar5 = bVar2;
                        pbVar5 = pbVar5 + 1;
                    }
                } else {
                    bVar2 = *pbVar4;
                    pbVar4 = pbVar4 + 1;
                    *pbVar5 = bVar2;
                    pbVar5 = pbVar5 + 1;
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
    register u8* strm;
    register u8* self = m;
    register u32 active;

    active = *(u32*)(self + 400);
    if (active != 0) {
        AudioStreamStop();
        if ((strm = *(u8**)(self + 400)) != 0) {
            AudioStreamStop();
            __dla__FPv(*(void**)(strm + 4));
            __dl__FPv(strm);
        }
        *(u32*)(self + 400) = 0;
    }
    {
        register u8* object = self + 336;
        register void (*dispatch)(u8*);

        dispatch = *(void (**)(u8*))(*(u32*)(self + 368) + 28);
        dispatch(object);
    }
    fn_800DBA80(self + 32, *(s32*)(self + 28));
    if (*(s32*)(self + 28) != 0) {
        sceClose(*(s32*)(self + 28));
    }
    *(s32*)(self + 28) = 0;
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

    fn_800DB3D4((u32*)(movie + 0x20), *(s32*)(movie + 0x1C), 0xA000);
    if (movie[0x19] != 0) {
        if (movie[0x18] != 0) {
            s32 tag = gMovieAllocCount++;
            audio = AllocHiMem(sizeof(MovieAudioState), tag);
            if (audio != NULL) {
                audio->command = sndCmd16(0xC000);
                tag = gMovieAllocCount++;
                audio->buffer = AllocHiMem(0xC000, tag);
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
        chunk = fn_800DB36C((s32)(movie + 0x20));
        while (chunk != NULL && chunk[8] == 0) {
            fn_800DB29C((s32)(movie + 0x20));
            chunk = fn_800DB36C((s32)(movie + 0x20));
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
            register void (*decode)(u8*, u8*, s32) =
                *(void (**)(u8*, u8*, s32))(*(u32*)(movie + 0x170) + 0x18);
            decode(movie + 0x150, movie + 0x11C, 0);
        }
        fn_800DB29C((s32)(movie + 0x20));
    }
done:
    return TRUE;
}

/* movie open: sceOpen/sceRead the Gauntlet VQMovies .avi file, asserts on failure (MoviePlayer.cpp) */
s32 fn_800DA920(u8* movie, const char* name)
{
    u8 headerStorage[4128];
    u8* header;
    const char* selected;
    s32 offset;

    header = headerStorage;
    header += (32 - ((u32)header & 31)) & 31;
    *(f32*)(movie + 4) = lbl_80349390;
    *(u32*)(movie + 16) = 0;
    if (*(s32*)(movie + 28) != 0) {
        sceClose(*(s32*)(movie + 28));
        *(s32*)(movie + 28) = 0;
    }
    if (*(u8**)(movie + 400) != NULL) {
        u8* stream;

        AudioStreamStop();
        if ((stream = *(u8**)(movie + 400)) != NULL) {
            AudioStreamStop();
            __dla__FPv(*(void**)(stream + 4));
            __dl__FPv(stream);
        }
        *(u32*)(movie + 400) = 0;
    }

    selected = name;
    if (!sceFileExists(selected)) {
        char path[256] = "Gauntlet/VQMovies/@";
        static const char midwayPath[32] = "Gauntlet/VQMovies/midway.avi";
        static const char playerName[12] = "MoviePlayer";
        static const char baseName[16] = "MoviePlayerBase";
        static const char fallbackPath[32] = "Gauntlet/VQMovies/KnotVQ.avi";
        static const char sourceFile[16] = "MoviePlayer.cpp";
        u8 pathPad[4];
        char* dst;
        char* tmpBase;
        register char c;
        register s32 i = 0;
        register char* cursor;

        cursor = path;
        while (*cursor != '@') {
            i++;
            cursor++;
        }
        cursor = (char*)name;
        dst = (tmpBase = path) + i;
        while ((c = *cursor) != '\0') {
            *dst = c;
            cursor++;
            i++;
            dst++;
        }
        path[i] = '.';
        path[i + 1] = 'a';
        path[i + 2] = 'v';
        path[i + 3] = 'i';
        path[i + 4] = '\0';
        selected = path;
        if (!sceFileExists(selected)) {
            selected = fallbackPath;
            if (!sceFileExists(selected)) {
                sysAssertFailed(name, sourceFile, 192);
                return 0;
            }
        }
    }

    *(s32*)(movie + 28) = sceOpen(selected, 1);
    sceRead(*(s32*)(movie + 28), header, 4096);
    if (!(u8)fn_800DACD8((s32)movie, header)) {
        return 0;
    }

    *(f32*)(movie + 12) =
        (f32)((f64)*(u32*)(movie + 204) / (f64)*(u32*)(movie + 200));
    *(f32*)(movie + 8) = lbl_80349394 / *(f32*)(movie + 12);

    *(u32*)(movie + 20) = 0;
    for (offset = 0; offset < 4096; offset += 4) {
        if (ReadF32LE(header + offset) == 0x4B4E554A) {
            *(u32*)(movie + 20) = offset + 4;
            break;
        }
    }
    if (*(u32*)(movie + 20) != 0) {
        *(u32*)(movie + 20) = 0;
        for (; offset < 4096; offset += 4) {
            if (ReadF32LE(header + offset) == 0x69766F6D) {
                *(u32*)(movie + 20) = offset + 4;
                break;
            }
        }
    }
    if (*(u32*)(movie + 20) == 0) {
        *(u32*)(movie + 20) = 2048;
    }

    sceLseek(*(s32*)(movie + 28), *(s32*)(movie + 20), 0);
    fn_800D99AC((u32)(movie + 336), (int*)(movie + 404), movie + 124);
    *(u16*)(movie + 138) = 16;
    *(u32*)(movie + 140) = 3;
    *(u32*)(movie + 164) = 0xF800;
    *(u32*)(movie + 168) = 2016;
    *(u32*)(movie + 172) = 31;
    *(u32*)(movie + 144) = *(u32*)(movie + 128) *
                           *(u32*)(movie + 132) * 2;
    *(s32*)(movie + 132) = -*(s32*)(movie + 132);
    *(u8**)(movie + 288) = movie + 404;
    *(u8**)(movie + 296) = movie + 124;

    if (MovieValidateFrameFormat((u32)(movie + 336),
                                 (s32)(movie + 284), 0) != 0) {
        return 0;
    }
    {
        typedef void (*MovieConfigureFn)(u8*, u8*, s32);
        (*(MovieConfigureFn**)(movie + 368))[4](movie + 336, movie + 284, 0);
    }
    MovieDecoderInitBuffers((u32*)(movie + 32), 0x80000, movie[24]);
    fn_800DB82C((u32*)(movie + 32), *(s32*)(movie + 28),
                *(u32*)(movie + 20));
    *(u32*)(movie + 80) = *(u32*)(movie + 212);
    movie[25] = 1;
    movie[26] = 0;
    gMovieFrameTimeReset = 0;
    return 1;
}

/* VQ .avi header parser (ReadF32LE/ReadU16LE/ReadU32LE) */
u32 fn_800DACD8(int param_1, u8* param_2) {
    u8* q;
    int ofs;
    int strl;
    u8* p;

    *(u8*)(param_1 + 0x18) = 0;
    if (ReadF32LE(param_2) != 0x46464952 || ReadF32LE(param_2 + 8) != 0x20495641) {
        return 0;
    }
    ofs = ReadF32LE(param_2 + 0x1C) + 0x20;
    p = param_2 + ofs;
    if (ReadF32LE(p) != 0x5453494C) {
        return 0;
    }
    strl = ReadF32LE(p + 4) + 8;
    strl += ofs;
    ofs += 0x10;
    p = (q = param_2 + ofs) + 4;
    if (ReadF32LE(p) != 0x73646976) {
        return 0;
    }
    *(u32*)(param_1 + 0xB4) = ReadF32LE(p);
    *(u32*)(param_1 + 0xB8) = ReadF32LE(p + 4);
    *(u32*)(param_1 + 0xBC) = ReadF32LE(p + 8);
    *(u32*)(param_1 + 0xC0) = ReadF32LE(p + 0xC);
    *(u16*)(param_1 + 0xC4) = ReadU16LE(p + 0x10);
    *(u16*)(param_1 + 0xC6) = ReadU16LE(p + 0x12);
    *(u32*)(param_1 + 0xC8) = ReadF32LE(p + 0x14);
    *(u32*)(param_1 + 0xCC) = ReadF32LE(p + 0x18);
    *(u32*)(param_1 + 0xD0) = ReadF32LE(p + 0x1C);
    *(u32*)(param_1 + 0xD4) = ReadF32LE(p + 0x20);
    *(u32*)(param_1 + 0xD8) = ReadF32LE(p + 0x24);
    *(u32*)(param_1 + 0xDC) = ReadF32LE(p + 0x28);
    *(u32*)(param_1 + 0xE0) = ReadF32LE(p + 0x2C);
    *(u32*)(param_1 + 0xE4) = ReadF32LE(p + 0x30);
    ofs += ReadF32LE(q);
    if (ReadF32LE(param_2 + (ofs + 4)) != 0x66727473) {
        return 0;
    }
    q = param_2 + (u32)ofs + 0xC;
    *(u32*)(param_1 + 0x194) = ReadF32LE(q);
    *(u32*)(param_1 + 0x198) = ReadU32LE(q + 4);
    *(u32*)(param_1 + 0x19C) = ReadU32LE(q + 8);
    *(u16*)(param_1 + 0x1A0) = ReadU16LE(q + 0xC);
    *(u16*)(param_1 + 0x1A2) = ReadU16LE(q + 0xE);
    *(u32*)(param_1 + 0x1A4) = ReadF32LE(q + 0x10);
    *(u32*)(param_1 + 0x1A8) = ReadF32LE(q + 0x14);
    *(u32*)(param_1 + 0x1AC) = ReadU32LE(q + 0x18);
    *(u32*)(param_1 + 0x1B0) = ReadU32LE(q + 0x1C);
    *(u32*)(param_1 + 0x1B4) = ReadF32LE(q + 0x20);
    *(u32*)(param_1 + 0x1B8) = ReadF32LE(q + 0x24);
    *(u32*)(param_1 + 0x1BC) = ReadF32LE(q + 0x28);
    *(u32*)(param_1 + 0x1C0) = ReadF32LE(q + 0x2C);
    *(u32*)(param_1 + 0x1C4) = ReadF32LE(q + 0x30);
    q = param_2 + strl;
    if (ReadF32LE(q) == 0x5453494C && ReadF32LE(q + 0x14) == 0x73647561) {
        q = (u8*)((u32)q + 0x14);
        *(u8*)(param_1 + 0x18) = 1;
        *(u32*)(param_1 + 0xE8) = ReadF32LE(q);
        *(u32*)(param_1 + 0xEC) = ReadF32LE(q + 4);
        *(u32*)(param_1 + 0xF0) = ReadF32LE(q + 8);
        *(u32*)(param_1 + 0xF4) = ReadF32LE(q + 0xC);
        *(u16*)(param_1 + 0xF8) = ReadU16LE(q + 0x10);
        *(u16*)(param_1 + 0xFA) = ReadU16LE(q + 0x12);
        *(u32*)(param_1 + 0xFC) = ReadF32LE(q + 0x14);
        *(u32*)(param_1 + 0x100) = ReadF32LE(q + 0x18);
        *(u32*)(param_1 + 0x104) = ReadF32LE(q + 0x1C);
        *(u32*)(param_1 + 0x108) = ReadF32LE(q + 0x20);
        *(u32*)(param_1 + 0x10C) = ReadF32LE(q + 0x24);
        *(u32*)(param_1 + 0x110) = ReadF32LE(q + 0x28);
        *(u32*)(param_1 + 0x114) = ReadF32LE(q + 0x2C);
        *(u32*)(param_1 + 0x118) = ReadF32LE(q + 0x30);
    }
    return 1;
}

/* MoviePlayer teardown (AudioStreamStop, operator delete, dtor_800DBB94) */
u32* dtor_800DBB94(u32* self, s16 deleting);
u32* fn_800DBD30(u32* self, s16 deleting);

u32* fn_800DB008(u32* self, s16 deleting) {
    u32* stream;
    u8 unused[24];

    if (self != NULL) {
        self[0] = (u32)lbl_8012968C;
        if ((s32)self[7] != 0) {
            sceClose((s32)self[7]);
        }
        if (self[100] != 0) {
            AudioStreamStop();
            stream = (u32*)self[100];
            if (stream != NULL) {
                AudioStreamStop();
                __dla__FPv((void*)stream[1]);
                __dl__FPv(stream);
            }
        }
        fn_800DBD30(self + 0x54, -1);
        dtor_800DBB94(self + 8, -1);
        if (self != NULL) {
            self[0] = (u32)lbl_801296A4;
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

#ifdef __MWERKS__
#pragma optimization_level 4
#pragma peephole on
#pragma scheduling on
#endif

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

#ifdef __MWERKS__
#pragma optimization_level 4
#pragma peephole on
#pragma scheduling on
#endif

u32* dtor_800DB21C(u32* self, s16 deleting) {
    u8 unused[24];

    if (self != NULL) {
        self[0] = (u32)lbl_801296A4;
        if (deleting > 0 && self != NULL) {
            gMovieAllocCount--;
            if (gMovieAllocCount == 0) {
                ResetAllocTot();
            }
        }
    }
    return self;
}

void fn_800DB29C(int stream) {
    u32* self = (u32*)stream;
    u32* node = (u32*)self[20];
    u32* next;

    self[20] = node[0];
    self[5] = node[2] + node[1];
    node[0] = self[22];
    self[22] = (u32)node;
    next = (u32*)self[20];
    if (next != NULL && self[5] + next[1] >= self[6]) {
        self[5] = 0;
    }
}

#ifdef __MWERKS__
#pragma optimization_level 4
#pragma peephole on
#pragma scheduling on
#endif

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

u32* fn_800DB36C(int stream) {
    u32* self = (u32*)stream;
    u32* node = (u32*)self[20];

    if (node == NULL) {
        goto none;
    }
    if (node[8] != 0) {
        goto ready;
    }
    if (node[9] != 0) {
        goto ready;
    }
    if (node[6] != 0) {
        goto ready;
    }
none:
    return NULL;
ready:
    if (node[0] != 0) {
        goto ret;
    }
    if (self[10] == self[11]) {
        goto ret;
    }
    return NULL;
ret:
    return node;
}

#ifdef __MWERKS__
#pragma optimization_level 4
#pragma peephole on
#pragma scheduling on
#endif

/* VQ codebook/frame reader (memcpy, ReadF32LE, sceRead) */
void fn_800DB3D4(u32* stream, s32 fd, volatile u32 length) {
    u32 available;
    u32 chunkOffset;
    u32 chunkSize;
    u32 chunkEnd;
    u32 sz;
    u32 pos;
    u32 offset;
    u32 tag;
    u8* chunk;
    u32* node;
    u32* next;
    s32 len;
    u8 unused[16];

    if (stream[7] == 0 && stream[4] <= stream[6] - 0x800) {
        goto request_more;
    }

    if (stream[7] <= 0x10000) {
        memcpy((u8*)stream[0] + stream[4], (void*)stream[2], stream[7]);
    }
    stream[10] += stream[7];
    stream[4] += stream[7];
    stream[7] = 0;

    for (;;) {
        node = (u32*)stream[20];
        while (node != NULL && *node != 0) {
            node = (u32*)*node;
        }

        chunkOffset = node[2];
        chunkEnd = chunkOffset + node[1];
        if (chunkEnd >= stream[6] - 0x800) {
            available = stream[4] - chunkOffset;
            if (available) {
                if (stream[5] > stream[4]) {
                    return;
                }
                if (!(stream[5] > available)) {
                    return;
                }
                memcpy((void*)stream[0], (u8*)stream[0] + chunkOffset, available);
                stream[4] = available;
                node[2] = 0;
                goto request_more;
            }
        }

        available = stream[4];
        if (available > stream[5]) {
            if (chunkEnd >= available - 8) {
                goto request_more;
            }
        } else if (chunkOffset < available - 8) {
            if (chunkEnd >= available - 8) {
                goto request_more;
            }
        }

        node[6] = 0;
        node[4] = 0;
        node[5] = 0;
        offset = 0;
        do {
            chunk = (u8*)(node[2] + offset + (u32)stream[0]);
            tag = ReadF32LE(chunk);
            chunkSize = ReadF32LE(chunk + 4);
            if (tag == 0x5453494c) {
                offset += 0xc;
            } else {
                offset += chunkSize + 8;
                switch (tag) {
                case 0x62773130:
                    node[9] = (u32)(chunk + 8);
                    node[5] = chunkSize;
                    node[7] = stream[14];
                    stream[14] += chunkSize;
                    fn_800D9B48(stream + 15, chunk + 8, chunkSize);
                    break;
                case 0x4b4e554a:
                    node[6] = chunkSize;
                    break;
                case 0x62643030:
                case 0x63643030:
                    node[8] = (u32)(chunk + 8);
                    node[4] = chunkSize;
                    node[3] = stream[13];
                    stream[13]++;
                    break;
                default:
                    break;
                }
                offset = (offset + 1) & 0xfffffffe;
            }
        } while (offset < (sz = node[1]));

        if (stream[10] == stream[11] || stream[13] == stream[12]) {
            goto request_more;
        }
        pos = node[2] + sz;
        if (pos + 8 > stream[4]) {
            goto request_more;
        }
        if (pos >= stream[6]) {
            tag = ReadF32LE((u8*)stream[0]);
        } else {
            tag = ReadF32LE((u8*)stream[0] + pos);
        }
        switch (tag) {
        case 0x62643030:
        case 0x63643030:
        case 0x5453494c:
        case 0x4b4e554a:
        case 0x62773130:
            break;
        default:
            stream[13] = stream[12];
            return;
        }

        pos = node[1] + 4;
        if (pos + node[2] >= stream[6]) {
            chunkSize = ReadF32LE((u8*)stream[0]);
        } else {
            chunkSize = ReadF32LE((u8*)stream[0] + (node[2] + node[1]) + 4);
        }
        *node = stream[22];
        stream[22] = *(u32*)stream[22];
        next = (u32*)*node;
        next[0] = 0;
        next[2] = chunkEnd;
        chunkSize += chunkSize & 1;
        next[1] = chunkSize + 8;
        next[8] = 0;
        next[9] = 0;
        next[6] = 0;
        next[4] = 0;
        next[5] = 0;
    }

request_more:
    length = (length + 0x7ff) & 0xfffff800;
    if (stream[4] >= stream[5]) {
        if (stream[5] != 0) {
            available = stream[6] - stream[4];
        } else {
            available = (stream[6] - stream[4]) - 0x800;
        }
    } else {
        available = (stream[5] - stream[4]) - 0x800;
    }
    if ((s32)available < (s32)length) {
        length = available;
    }
    if ((s32)(stream[11] - stream[10]) < (s32)length) {
        length = stream[11] - stream[10];
    }
    if ((s32)length < 0) {
        length = 0;
    }
    length &= 0xfffff800;
    len = length;
    if (len > 0 && stream[13] < stream[12]) {
        sceRead(fd, (void*)stream[2], len);
        stream[7] = length;
    }
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
    u32* self = (u32*)dec;

    if (self[1] != 0) {
        gMovieAllocCount--;
        if (gMovieAllocCount == 0) {
            ResetAllocTot();
        }
    }
    self[1] = 0;
    self[0] = 0;
    if (self[3] != 0) {
        gMovieAllocCount--;
        if (gMovieAllocCount == 0) {
            ResetAllocTot();
        }
    }
    self[3] = 0;
    self[2] = 0;
    if (self[21] != 0) {
        gMovieAllocCount--;
        if (gMovieAllocCount == 0) {
            ResetAllocTot();
        }
    }
    self[6] = 0;
    self[5] = 0;
    self[4] = 0;
    self[8] = 0;
    self[9] = 0;
    self[22] = 0;
    self[20] = 0;
    self[21] = 0;
}

#ifdef __MWERKS__
#pragma optimization_level 4
#pragma peephole on
#pragma scheduling on
#endif

u32* dtor_800DBB94(u32* self, s16 deleting) {
    u8 unused[32];

    if (self != NULL) {
        __dla__FPv((void*)self[1]);
        self[1] = 0;
        self[0] = 0;
        __dla__FPv((void*)self[3]);
        self[3] = 0;
        self[2] = 0;
        __dla__FPv((void*)self[0x15]);
        self[6] = 0;
        self[5] = 0;
        self[4] = 0;
        self[8] = 0;
        self[9] = 0;
        self[0x16] = 0;
        self[0x14] = 0;
        self[0x15] = 0;
        fn_800D9CF4((int*)(self + 0xf), -1);
        if (deleting > 0 && self != NULL) {
            gMovieAllocCount--;
            if (gMovieAllocCount == 0) {
                ResetAllocTot();
            }
        }
    }
    return self;
}

u32* fn_800DBC64(register u32* p) {
    register u32* self = p;

    fn_800D9DA4(self + 0xf);
    self[3] = 0;
    self[2] = 0;
    self[1] = 0;
    self[0] = 0;
    self[6] = 0;
    self[5] = 0;
    self[4] = 0;
    self[8] = 0;
    self[9] = 0;
    self[0x16] = 0;
    self[0x15] = 0;
    self[0x14] = 0;
    return self;
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

/* Destroy an outer DText object (vtable lbl_801296CC) and optionally release it. */
u32* fn_800DBD30(u32* self, s16 deleting) {
    if (self != NULL) {
        self[8] = (u32)lbl_801296CC;
        if (self[12] != 0) {
            gMovieAllocCount--;
            if (gMovieAllocCount == 0) {
                ResetAllocTot();
            }
        }
        lbl_803452B8--;
        fn_800DBF6C(self, 0);
        if (deleting > 0 && self != NULL) {
            gMovieAllocCount--;
            if (gMovieAllocCount == 0) {
                ResetAllocTot();
            }
        }
    }
    return self;
}

/* Construct an outer DText object (base init + lbl_80321340 ramp, first time only). */
u32* fn_800DBE04(u32* p) {
    int i;
    DTextInitColorRamp(p);
    p[8] = (u32)lbl_801296CC;
    if (lbl_803452B8 == 0) {
        for (i = 0; i < 256; i++) {
            lbl_80321340[i] = (u8)((i * 31 + 128) / 255);
        }
    }
    lbl_803452B8++;
    p[12] = 0;
    return p;
}

#ifdef __MWERKS__
#pragma optimization_level 4
#pragma peephole on
#pragma scheduling on
#endif

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

u32* DTextInitColorRamp(u32* p) {
    int i;
    u8* ramp = gDTextColorRamp;
    p[8] = (u32)lbl_801296F0;
    if (gDTextInitCount == 0) {
        memset(ramp, 0, 256);
        memset(ramp + 512, 255, 256);
        for (i = 0; i < 256; i++) {
            gDTextBuf[i] = (u8)i;
        }
        for (i = 0; i < 32; i++) {
            ((DTextRampEntry*)(ramp + i))->value = (u8)((i * 255 + 16) / 31);
        }
    }
    gDTextInitCount++;
    p[6] = 0;
    return p;
}
