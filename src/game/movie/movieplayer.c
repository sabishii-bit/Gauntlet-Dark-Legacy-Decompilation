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

/* --- little-endian container readers (parse the PC-format .avi header) --- */

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

/* --- VQ decode / GX present / file-stream bodies (parked NonMatching) --- */

void fn_800D860C(void) {
}

void fn_800D86C8(void) {
}

void fn_800D8784(void) {
}

/* VQ texture/tile decode into a GX tex obj (ReadU16LE/ReadF32LE, DCFlush/Invalidate, GXInvalidateTexAll) */
void fn_800D87FC(void) {
}

/* VQ tile decode variant (ReadU16LE, DCFlush/Invalidate, GXInvalidateTexAll) */
void fn_800D8BCC(void) {
}

/* VQ chunk -> buffer copy (ReadU16LE/ReadF32LE, memcpy) */
void fn_800D8F28(void) {
}

/* VQ chunk -> buffer copy (ReadU16LE, memcpy) */
void fn_800D91B4(void) {
}

/* VQ frame parser: dispatches the fn_800D87FC/8BCC/8F28/91B4 decoders */
void fn_800D93D4(void) {
}

void fn_800D9614(void) {
}

void fn_800D9648(void) {
}

void fn_800D967C(void) {
}

/* allocator (gAlloc + ResetAllocTot/__unexpected) */
void fn_800D96B0(void) {
}

void fn_800D9874(void) {
}

void fn_800D99AC(void) {
}

void fn_800D9A14(void) {
}

void fn_800D9B48(void) {
}

void fn_800D9C34(void) {
}

void fn_800D9C5C(void) {
}

void fn_800D9CF4(void) {
}

void fn_800D9DA4(void) {
}

void fn_800D9DBC(void) {
}

void fn_800D9DF0(void) {
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
void fn_800DACD8(void) {
}

/* MoviePlayer teardown (AudioStreamStop, operator delete, dtor_800DBB94) */
void fn_800DB008(void) {
}

void fn_800DB0F8(void) {
}

/* operator delete[] (weak, emitted into this TU) */
void __dla__FPv(void) {
}

/* operator delete (weak, emitted into this TU) */
void __dl__FPv(void) {
}

void dtor_800DB21C(void) {
}

void fn_800DB29C(void) {
}

void fn_800DB2F4(void) {
}

void fn_800DB36C(void) {
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

void fn_800DBC64(void) {
}

void fn_800DBCCC(void) {
}

void fn_800DBD00(void) {
}

void fn_800DBD30(void) {
}

void fn_800DBE04(void) {
}

/* DText glyph blit (uses gDTextBuf + movie sdata2 pool) */
void fn_800DBE98(void) {
}

/* DText allocate/init buffer (gDTextInitCount) */
void fn_800DBF6C(void) {
}

/* 0x800DC034 init the DText debug-overlay 256-entry colour ramp (gDTextColorRamp/gDTextBuf) */
void DTextInitColorRamp(void) {
}

