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
#include "dolphin/pad.h"
#include "dolphin/gx/GXVert.h"

/* Built as C++ so the -Cpp_exceptions on in cflags_demo actually reaches the
 * front end (the .c extension selected the C compiler, where the flag is
 * inert). Everything keeps C linkage: the target names every function in this
 * TU unmangled (PlayVQMovie, MovieDecoderInitBuffers, DTextInitColorRamp,
 * and the operator-delete pair __dl__FPv/__dla__FPv, which mb_blit.c
 * references with a C declaration), so mangling any of them would break both
 * the cross-TU link and the name-keyed diff pairing. */
extern "C" {

#ifndef offsetof
#define offsetof(type, memb) ((u32) & ((type*)0)->memb)
#endif

typedef struct MovieGXColor {
    u8 r, g, b, a;
} MovieGXColor;

typedef struct MovieGXTexObj {
    u32 data[8];
} MovieGXTexObj;

/* Fields at 0x30+ inferred solely from raw-offset usage in MovieDecodePalette
 * and fn_800D8BCC's case-2 block (no Xbox PDB struct for this GC-specific
 * VQ decode state was found; names are file-local, not authoritative). They
 * describe a per-channel bit-count/shift pair used to pack a 3-byte palette
 * RGB triple into a 16-bit pal[] entry: out = (p[0]>>(8-redBits))<<redShift
 * | (p[1]>>(8-greenBits))<<greenShift | (p[2]>>(8-blueBits))<<blueShift. */
typedef struct MovieDecodeState {
    /* 0x00 */ s32 width;
    /* 0x04 */ s32 height;
    /* 0x08 */ u32 _08[4];
    /* 0x18 */ u8* chunk;
    /* 0x1C */ u32 frame;
    /* 0x20 */ u32 _20[2];
    /* 0x28 */ s32 maskStride;
    /* 0x2C */ s32 paletteOffset;
    /* 0x30 */ u32 _30;
    /* 0x34 */ u8 blueShift;
    /* 0x35 */ u8 greenShift;
    /* 0x36 */ u8 redShift;
    /* 0x37 */ u8 blueBits;
    /* 0x38 */ u8 redBits;
    /* 0x39 */ u8 greenBits;
} MovieDecodeState;

typedef struct MovieDecodeCall {
    /* 0x00 */ u32 flags;
    /* 0x04 */ s32 context;
    /* 0x08 */ char* chunk;
    /* 0x0C */ s32 bitmap;
    /* 0x10 */ u8* destination;
} MovieDecodeCall;

/* Standard Windows BITMAPINFOHEADER (public AVI/BMP format, not a GDL/Xbox
 * name) - the .avi container's per-stream video format record. MovieDecodeCall
 * .context/.bitmap both point at one of these (target/source format);
 * verified by offset against fn_800D93D4, fn_800D96B0, fn_800D87FC,
 * fn_800D8BCC and MovieValidateFrameFormat, which all read the identical
 * width/height/bitCount/compression displacements. When compression==3
 * (BI_BITFIELDS) and size==40, three DWORD channel masks follow the header
 * at 0x28/0x2C/0x30 (a common practical extension, not part of the official
 * 40-byte struct) - used by fn_800D96B0 to derive the palette shift/bit
 * fields. */
typedef struct MovieBitmapHeader {
    /* 0x00 */ u32 size;
    /* 0x04 */ s32 width;
    /* 0x08 */ s32 height;
    /* 0x0C */ u16 planes;
    /* 0x0E */ u16 bitCount;
    /* 0x10 */ s32 compression;
    /* 0x14 */ u32 sizeImage;
    /* 0x18 */ u32 xPelsPerMeter;
    /* 0x1C */ u32 yPelsPerMeter;
    /* 0x20 */ u32 clrUsed;
    /* 0x24 */ u32 clrImportant;
    /* 0x28 */ u32 redMask;
    /* 0x2C */ u32 greenMask;
    /* 0x30 */ u32 blueMask;
} MovieBitmapHeader;

/* File-local circular byte-ring: fn_800D9C5C allocates buffer/size and zeros
 * writePos/readPos, fn_800D9A14 consumes from readPos, fn_800D9B48 produces
 * at writePos, fn_800D9DA4/fn_800D9CF4 release it. Not a GC-verified name -
 * offsets verified purely from this TU's own usage. */
typedef struct MovieRingBuffer {
    u8* buffer;
    u32 size;
    u32 writePos;
    u32 readPos;
} MovieRingBuffer;

/* File-local RIFF chunk-list node, 0x28 bytes (matches the 0x2800-byte pool
 * of 256 nodes allocated by MovieDecoderInitBuffers). Filled by fn_800DB3D4
 * while walking .avi '00db'/'00dc' (video) and '01wb' (audio) sub-chunks;
 * consumed by fn_800DB36C/fn_800DB29C. Not a GC-verified name. */
typedef struct MovieChunkNode {
    /* 0x00 */ struct MovieChunkNode* next;
    /* 0x04 */ u32 totalSize;       /* RIFF LIST payload size + 8, word-aligned */
    /* 0x08 */ u32 dataOffset;      /* byte offset of this chunk's data in stream.buffer */
    /* 0x0C */ u32 videoFrameIndex; /* stream.videoFrameCount snapshot at '00db'/'00dc' */
    /* 0x10 */ u32 videoSize;       /* '00db'/'00dc' payload size */
    /* 0x14 */ u32 audioSize;       /* '01wb' payload size */
    /* 0x18 */ u32 junkSize;        /* 'JUNK' payload size */
    /* 0x1C */ u32 audioByteOffset; /* stream.audioBytesProduced snapshot at '01wb' */
    /* 0x20 */ u8* videoData;
    /* 0x24 */ u8* audioData;
} MovieChunkNode;

/* File-local .avi RIFF read-ahead stream state, embedded at movie+0x20
 * (verified via fn_800DA6A4's fn_800DB3D4((u32*)(movie+0x20), ...) call and
 * PlayVQMovie's MovieDecoderInitBuffers((u32*)(movie+32), ...)). Owns a main
 * double-buffered read window (buffer/rawBuffer + stagingBuffer/rawStaging),
 * an embedded MovieRingBuffer at 0x3C for demuxed audio bytes, and a
 * MovieChunkNode free-list/pool for demuxed RIFF chunks. Not a GC-verified
 * name - offsets verified purely from this TU's own usage across
 * MovieDecoderInitBuffers, fn_800DB82C, fn_800DB3D4, fn_800DB29C,
 * fn_800DB36C, fn_800DB2F4 and the dtor cluster (__dt__11MoviePlayerFv/fn_800DB0F8/
 * __dt__15MoviePlayerBaseFv). self[8]/self[9] are always zeroed but never read in this
 * TU - left as unknown padding. self+0x4C (word 19) is written once (a byte
 * flag in fn_800DB2F4) with no corroborating read - left raw there. */
typedef struct MovieChunkStream {
    /* 0x00 */ u8* buffer;            /* aligned view of rawBuffer, main read window */
    /* 0x04 */ u8* rawBuffer;         /* AllocHiMem'd block backing buffer */
    /* 0x08 */ u8* stagingBuffer;     /* aligned view of rawStaging, read-ahead landing pad */
    /* 0x0C */ u8* rawStaging;        /* AllocHiMem'd block (0x10020 bytes) backing stagingBuffer */
    /* 0x10 */ u32 writePos;          /* bytes consumed into buffer so far */
    /* 0x14 */ u32 highWater;         /* compared against writePos when reclaiming space */
    /* 0x18 */ u32 bufferSize;        /* total allocated size of buffer */
    /* 0x1C */ u32 pendingLength;     /* bytes just landed in stagingBuffer, awaiting copy-in */
    /* 0x20 */ u32 _20;
    /* 0x24 */ u32 _24;
    /* 0x28 */ u32 filePos;           /* bytes consumed from the source file so far */
    /* 0x2C */ u32 fileSize;          /* total source file size (sceLseek SEEK_END) */
    /* 0x30 */ u32 videoFrameLimit;   /* total video frame count */
    /* 0x34 */ u32 videoFrameCount;   /* video frames demuxed so far */
    /* 0x38 */ u32 audioBytesProduced;/* cumulative demuxed audio byte offset */
    /* 0x3C */ MovieRingBuffer audio; /* demuxed-audio ring buffer, fed by fn_800D9B48 */
    /* 0x4C */ u8 _4C;                /* byte flag (fn_800DB2F4/fn_800DB82C); no read evidence */
    /* 0x4D */ u8 _4D_pad[3];
    /* 0x50 */ MovieChunkNode* activeNode;
    /* 0x54 */ u8* nodePoolRaw;       /* AllocHiMem'd block (0x2800 bytes = 256 nodes) */
    /* 0x58 */ MovieChunkNode* freeListHead;
} MovieChunkStream;

/* Codec / VQCodec -- the movie decoder's two-level class pair.  Both names are
 * RECOVERED, not invented: the GameCube build carries CodeWarrior RTTI, and the
 * vtable -> RTTI -> name chain (validated against the known-named control
 * __RTTI__Q23std9exception) reads { name = 0x803493C8 -> "Codec", base = NULL }
 * at .sdata 0x80344010 and { name = 0x803493C0 -> "VQCodec", base -> Codec's
 * record } at 0x80344018.  Codec's vtable object is __vt__5Codec (its dtor slot
 * at +8 = 0x801296F8); VQCodec's is __vt__7VQCodec.
 *
 * The inheritance is confirmed independently of the RTTI base descriptor: the
 * target's VQCodec destructor ends with `addi r3,r29,0 / li r4,0 / bl
 * __dt__5CodecFv` -- an in-charge=0 base-subobject destructor call on the same
 * `this`, which is what MWCC synthesises for a base at offset 0.
 *
 * LAYOUT, read off the two destructors' teardown displacements:
 *   Codec   +0x18  pointer released with `delete` (target: `lwz r0,24(this)`
 *                  feeding the inlined operator delete's null test)
 *           +0x20  vptr -- target stores it with `stw r0,32(this)` in BOTH
 *                  destructors, which is why the vptr sits at 0x20, not at 0
 *   VQCodec +0x30  its own released pointer (target: `lwz r0,48(this)`)
 * Codec therefore ends at 0x24 and VQCodec's own members start there, which is
 * exactly what puts the second pointer at 0x30.  (The previous file-local
 * reconstruction spelled the gap `u32 _24[2]`, which placed that pointer at
 * 0x2C -- one word low against the target's `lwz r0,48`; corrected here.) */
#pragma cplusplus on
class Codec {
public:
    /* 0x00 */ u32 _00[6];
    /* 0x18 */ u8* alloc;
    /* 0x1C */ u32 _1C;
    /* 0x20 */ u32* vtable;

    ~Codec();
};

class VQCodec : public Codec {
public:
    /* 0x24 */ u32 _24[3];
    /* 0x30 */ u8* alloc2;

    ~VQCodec();
};
#pragma cplusplus off

/* File-local streaming-audio pump state, AllocHiMem'd by PlayVQMovie and
 * stashed at movie+0x190; fn_800D9F20 (per-frame adsPoll/sndCmd17 pump) and
 * PlayVQMovie's own audio setup/teardown are its only readers/writers.
 * Not a GC-verified name - offsets verified purely from this TU's own
 * usage. */
typedef struct MovieAudioState {
    s32 command;
    u8* buffer;
    u32 remaining;
    u32 offset;
    u32 requestSize;
    u8 active;
    u8 _pad[3];
} MovieAudioState;

/* File-local top-level VQ movie-player record, 0x1D0 bytes - the size is not
 * inferred, it is PlayVQMovie's own __construct_new_array(movie, ctor, dtor,
 * 464, 1) element-size argument (and its AllocHiMem(472) = 464 + the 8-byte
 * array cookie).
 *
 * One record, four base spellings: gMovieStreamState holds the single
 * instance, and fn_800DA6A4's `movie`, fn_800DA60C's `self` and fn_800DACD8's
 * `param_1` are all that same pointer - proven directly by fn_800DA920's
 * `fn_800DACD8((s32)movie, header)` call, and independently by the bases
 * agreeing on every shared displacement (0x1C fd, 0x20 stream, 0x150 decoder,
 * 0x170 decoder vtable, 0x190 audio, 0x198/0x19C source width/height).
 *
 * Each embedded sub-record is separately cross-verified:
 *   0x020 stream      `(MovieChunkStream*)(movie + 0x20)` is already cast at
 *                     six sites; fn_800DA920 copies videoStreamHeader+0x20
 *                     into stream.videoFrameLimit (movie+80 <- movie+212).
 *   0x07C outFormat   fn_800D99AC's `dst` (= movie+124) reads +4 width and
 *                     +8 height and writes +0xE bitCount, +0x10 compression,
 *                     +0x14 sizeImage; fn_800DA920 then overwrites bitCount
 *                     = 16, compression = 3 (BI_BITFIELDS) and the three
 *                     masks at +0x28/+0x2C/+0x30 with 0xF800/0x07E0/0x001F.
 *   0x0B4/0x0E8       two 0x34-byte offset-preserving copies of the .avi
 *                     'strh' stream-header blocks (video, then audio) made by
 *                     fn_800DACD8. +0x20 is the stream length - video frame
 *                     count (copied into stream.videoFrameLimit) and audio
 *                     byte count (read by fn_800D9F20 and PlayVQMovie). The
 *                     u16 pair at +0x10/+0x12 does NOT agree with the public
 *                     AVISTREAMHEADER layout, which puts wPriority/wLanguage
 *                     at +0x0C/+0x0E, so NO field names are adopted for these
 *                     two blocks - they stay untyped word arrays.
 *   0x11C decodeCall  the MovieDecodeCall passed to the decoder's virtual
 *                     decode()/configure() and to MovieValidateFrameFormat;
 *                     fn_800DA920 wires its .context/.bitmap to &fileFormat
 *                     and &outFormat (movie+288 <- movie+404, movie+296 <-
 *                     movie+124).
 *   0x150 decoder     the MovieDTextOuter decoder object - __dt__11MoviePlayerFv tears
 *                     it down as `(MovieDTextOuter*)(self + 0x54)` on a u32*
 *                     self, i.e. byte 0x150. Its vtable member sits at +0x20
 *                     (fn_800DBE04's `p[8] = __vt__7VQCodec`), so decoderVtable
 *                     at 0x170 is that same word - exactly the MovieCloseVTable
 *                     pointer at `self + 368` that the teardown path
 *                     calls close() through, and the same word fn_800DA6A4
 *                     fetches its decode() entry from. The region is kept as
 *                     opaque words rather than an embedded MovieDTextOuter on
 *                     purpose: that struct's declared extent is unsettled (its
 *                     `_24[2]` puts ownsAlloc at 0x2C while its comment claims
 *                     0x30), and MovieState must not inherit that uncertainty.
 *   0x194 fileFormat  MovieBitmapHeader; all 14 fields line up with
 *                     fn_800DACD8's 'strf' parse, including the u16 pair at
 *                     +0x0C/+0x0E (0x1A0/0x1A2) whose widths the raw code
 *                     itself confirms.
 *
 * Offset 0x000 is the vtable, per __dt__11MoviePlayerFv's `self[0] = __vt__11MoviePlayer` and
 * PlayVQMovie's MovieStreamInterface view. No Xbox PDB authority exists for
 * this record - the Xbox build's MOVIE.OBJ is an unrelated cutscene subsystem
 * (InitMovie/ServeMovie/KillMovie) and the PDB type stream carries nothing for
 * the VQ .avi player - so every name below is file-local and derived from this
 * TU's own dataflow, not GC-verified. Bytes with no observed access stay
 * unkNNN. */
typedef struct MovieState {
    /* 0x000 */ u32 vtable;
    /* 0x004 */ f32 playTime;         /* += elapsed each update */
    /* 0x008 */ f32 frameInterval;    /* playTime / frameInterval = frame */
    /* 0x00C */ f32 frameRate;        /* videoStreamHeader rate / scale */
    /* 0x010 */ u32 frameIndex;
    /* 0x014 */ u32 dataOffset;       /* 'movi' payload offset, sceLseek target */
    /* 0x018 */ u8 hasAudio;          /* set by fn_800DACD8 on an 'auds' list */
    /* 0x019 */ u8 needsPrime;
    /* 0x01A */ u8 stopped;
    /* 0x01B */ u8 unk01B;
    /* 0x01C */ s32 fd;               /* sceOpen/sceRead/sceLseek/sceClose */
    /* 0x020 */ MovieChunkStream stream;
    /* 0x07C */ MovieBitmapHeader outFormat;
    /* 0x0B0 */ u32 unk0B0;
    /* 0x0B4 */ u8 videoStreamHeader[0x34];
    /* 0x0E8 */ u8 audioStreamHeader[0x34];
    /* 0x11C */ MovieDecodeCall decodeCall;
    /* 0x130 */ u32 unk130[8];
    /* 0x150 */ u32 decoder[8];       /* MovieDTextOuter base; opaque here */
    /* 0x170 */ u32 decoderVtable;    /* == MovieDTextOuter.vtable (+0x20) */
    /* 0x174 */ u32 unk174[7];
    /* 0x190 */ MovieAudioState* audio;
    /* 0x194 */ MovieBitmapHeader fileFormat;
    /* 0x1C8 */ u32 unk1C8[2];
} MovieState;

extern void GXInitTexObj(MovieGXTexObj* obj, void* data, u16 width, u16 height,
                         u32 format, u32 wrapS, u32 wrapT, u8 mipmap);
extern void GXLoadTexObj(MovieGXTexObj* obj, u8 map);
extern void GXSetChanMatColor(s32 channel, MovieGXColor color);
extern void GXSetZMode(u8 compareEnable, u32 compare, u8 updateEnable);
extern void GXBegin(u32 primitive, u32 format, u16 vertices);

#define MOVIE_GX_TRUE 1
#define MOVIE_GX_GEQUAL 6
#define MOVIE_GX_TRIANGLES 0x90
#define MOVIE_GX_VTXFMT0 0
#define MOVIE_GX_COLOR0A0 4
#define MOVIE_GX_TEXMAP0 0
#define MOVIE_GX_TF_RGB565 4
#define MOVIE_GX_CLAMP 1

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
extern void SetMultiPassTextureParams(s32 stages);
extern void SetCullMode(s32 mode);
extern void SetPerspectiveMode(s32 mode);
extern void SetViewportHeight(f32 height);
extern void SetVertexFormat(s32 format);
extern void fn_800C6AB4(s32 mode);
extern PADStatus* G3DGetPadStatusBuffer(void);
extern void sysResetService(void);
extern u32 pbPulseTime(void);
extern void DEMOSwapBuffers(void);
extern void DEMODoneRender(void);

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
extern u32 __vt__15MoviePlayerBase[];
extern u32 __vt__11MoviePlayer[];
extern u32 __vt__7VQCodec[];
extern u32 __vt__5Codec[];
extern u32 lbl_803452B8;
extern u32 gDTextInitCount;
extern u8 lbl_80321340[];
extern MovieGXTexObj lbl_80321308;
extern u8 gMovieAudioCallback[];
extern u8 gDTextColorRamp[];
extern u8* gDTextBuf;
extern u8 lbl_80344A5D;
extern s32 sSeconds;

/* --- sdata2 float pool (movie YUV->RGB matrix coeffs) --- */
extern const f32 lbl_80349390;
extern const f32 lbl_80349394;
extern const f32 lbl_80349398;
extern const f32 lbl_8034939C;
extern const f32 lbl_803493A0;
extern const f64 lbl_803493A8;
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
MovieChunkStream* fn_800DBC64(MovieChunkStream* p);
/* C++ region: Codec/VQCodec are class types, which the TU's C-parsed regions
 * cannot name (MWCC keeps `#pragma cplusplus off` regions on the C front end). */
#pragma cplusplus on
VQCodec* fn_800DBE04(u32* p);
Codec* DTextInitColorRamp(Codec* p);
#pragma cplusplus off
void fn_800D9DF0(char* src, int len, u8* dst, int* outlen);
u32 fn_800D93D4(u32* p1, u32 p2, int p3, char* p4, int p5, u8* p6);
u32 fn_800D87FC(MovieDecodeState* p1, int p3, char* p4, int mode, int p5, u8* p6);
u32 fn_800D8BCC(MovieDecodeState* p1, int p3, char* p4, int mode, int p5, u8* p6);
u32 fn_800D8F28(MovieDecodeState* p1, int p3, char* p4, int p5, u8* p6);
u32 fn_800D91B4(MovieDecodeState* p1, int p3, char* p4, int p5, u8* p6);
u32 fn_800D9A14(MovieRingBuffer* p1, u8* p2, int p3, u8 p4);
int fn_800D9DBC(u32 param_1, char* param_2, int param_3, u8* param_4);
void fn_800DBE98(void* param_1, u8* param_2);
int fn_800DB2F4(MovieChunkStream* param_1, u8* param_2, u32 param_3, u32 param_4);
void fn_800DB3D4(MovieChunkStream* stream, s32 fd, u32 length);
void fn_800DB29C(MovieChunkStream* stream);
MovieChunkNode* fn_800DB36C(MovieChunkStream* stream);
void fn_800DB82C(MovieChunkStream* stream, int fd, u32 offset);
void fn_800DA60C(u8* movie);
u32 fn_800DA6A4(u8* movie, u32 decodeFrame, f32 elapsed);
s32 fn_800DA920(u8* movie, const char* name);
u32 fn_800DACD8(int movie, u8* header);
u8 MovieDecoderInitBuffers(MovieChunkStream* decoder, u32 size, u32 hasAudio);
void fn_800D9F20(MovieAudioState* audio);
u32* __dt__11MoviePlayerFv(u32* self, s16 deleting);
u32* fn_800DB0F8(u32* self);
u8 fn_800DBCCC(void* self, s32 x);
u8 fn_800DBD00(void* self, s32 x);
extern u32 __cvt_fp2unsigned(f64 value);
extern s32 sndCmd16(s32 size);
extern s32 sndCmdA(s32 volume, s32 arg1, s32 arg2, void* callback);
extern s32 lbl_80343B4C;
extern u8 gMovieFrameTimeReset;
extern void* __construct_new_array(void* block, void* ctor, void* dtor,
                                   u32 size, u32 count);
extern void __destroy_new_array(void* block, void* dtor);

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
    u8* out;
    int i;
    int cnt;
    cnt = param_3 << 1;
    out = param_2;
    for (i = 0; i < cnt; i++) {
        int c1 = (int)((u32)param_2[1] * 224) / 512;
        int c4 = (int)((u32)param_2[4] * 224) / 512;
        int c2 = (int)((u32)param_2[2] * 224) / 512;
        int c5 = (int)((u32)param_2[5] * 224) / 512;
        int c0 = (int)((u32)param_2[0] * 224) / 256;
        int c3 = (int)((u32)param_2[3] * 224) / 256;
        u32 value;

        c4 += 8;
        value = c1 + 8;
        value += c4;
        c2 += c5;
        c2 += 16;
        *(u32*)(out + i * 4) =
            (value << 8 | (u32)c2 << 24)
          | ((u32)(c0 + 16) | (u32)(c3 + 16) << 16);
        param_2 += 6;
    }
}

void fn_800D86C8(u32 param_1, u8* param_2, int param_3) {
    u8* out;
    int i;
    int cnt;
    cnt = param_3 << 1;
    out = param_2;
    for (i = 0; i < cnt; i++) {
        int c1 = (int)((u32)param_2[1] * 224) / 512;
        int c4 = (int)((u32)param_2[4] * 224) / 512;
        int c2 = (int)((u32)param_2[2] * 224) / 512;
        int c5 = (int)((u32)param_2[5] * 224) / 512;
        int c0 = (int)((u32)param_2[0] * 224) / 256;
        int c3 = (int)((u32)param_2[3] * 224) / 256;
        u32 value;

        value = c1 + 8;
        value += c4 + 8;
        value |= (u32)(c2 + c5 + 16) << 16
               | ((u32)(c3 + 16) << 24 | (u32)(c0 + 16) << 8);
        *(u32*)(out + i * 4) = value;
        param_2 += 6;
    }
}

#pragma dont_inline off

/* Release one movie allocation and clear the owning slot. */
s32 fn_800D8784(MovieDecodeState* state) {
    if (state->chunk != 0) {
        gMovieAllocCount--;
        if (gMovieAllocCount == 0) {
            ResetAllocTot();
        }
    }
    state->chunk = 0;
    return 0;
}

static inline void MovieDecodePalette(MovieDecodeState* state, u8* pal, int count)
{
    int i;
    u8* p;
    u8 sh1;
    u8 sh0;
    u8 sh2;
    int n;

    sh1 = 8 - state->greenBits;
    sh0 = 8 - state->redBits;
    sh2 = 8 - state->blueBits;
    n = count * 4;
    i = 0;
    p = pal;
    for (; i < n; i++) {
        fn_800DBE98(state, p);
        ((u16*)pal)[i] = (((p[0] >> sh0) << state->redShift)
                        | ((p[1] >> sh1) << state->greenShift))
                        | ((p[2] >> sh2) << state->blueShift);
        p += 3;
    }
}

/* VQ texture/tile decode into a GX tex obj (ReadU16LE/ReadF32LE, DCFlush/Invalidate, GXInvalidateTexAll) */
u32 fn_800D87FC(MovieDecodeState* state, int param_2, char* param_3, int param_4, int param_5, u8* param_6) {
    int count;
    int nbits;
    u8* hdr8;
    u8* pal;
    u8* ip;

    count = ReadU16LE(state->chunk);
    nbits = ReadF32LE(state->chunk + 4);
    hdr8 = state->chunk + 8;
    pal = hdr8 + state->paletteOffset;
    ip = pal + count * 12;
    DCInvalidateRange((void*)param_6, state->width * state->height * 2);
    switch (param_4) {
    case 0:
        fn_800D86C8((u32)state, pal, count);
        break;
    case 1:
        fn_800D860C((u32)state, pal, count);
        break;
    case 2:
        MovieDecodePalette(state, pal, count);
        break;
    default:
        return -1;
    }

    {
        int dir;
        int row;

        if (*(int*)(param_5 + offsetof(MovieBitmapHeader, height)) < 0) {
            dir = -1;
            row = state->height - 1;
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

            bits = *ip;
            state->width <<= 1;
            bp = ip + 1;
            ip += (nbits + 7) / 8;
            d8 = dir * 8;
            d2 = dir * 2;
            nb = 0;
            for (y = 0; y < state->height; y += 2) {
                u8* dst;
                u8* dst2;
                u8* brow;
                int x;

                dst = (u8*)param_6 + (row & ~3) * state->width;
                dst += (row & 3) * 8;
                dst2 = dst + d8;
                brow = hdr8 + (y / 4) * state->maskStride;
                x = 0;
                do {
                    int b;
                    int adv;

                    b = x >> 3;
                    if ((1 << (b & 7)) & brow[b / 8]) {
                        u32 idx;
                        u16 val;
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
                } while (x < state->width);
                row += d2;
            }
            state->width = state->width / 2;
        } else {
            int d8;
            int d2;
            int y;

            d8 = dir * 8;
            d2 = dir * 2;
            state->width <<= 1;
            for (y = 0; y < state->height; y += 2) {
                u8* dst;
                u8* dst2;
                u8* brow;
                int x;

                dst = (u8*)param_6 + (row & ~3) * state->width;
                dst += (row & 3) * 8;
                dst2 = dst + d8;
                brow = hdr8 + (y / 4) * state->maskStride;
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
                } while (x < state->width);
                row += d2;
            }
            state->width = state->width / 2;
        }
    }
    DCFlushRange((void*)param_6, state->width * state->height * 2);
    GXInvalidateTexAll();
    state->frame = state->frame + 1;
    return 0;
}

/* VQ tile decode variant (ReadU16LE, DCFlush/Invalidate, GXInvalidateTexAll) */
u32 fn_800D8BCC(MovieDecodeState* state, int param_2, char* param_3, int param_4, int param_5, u8* param_6) {
    int count;
    u8* pal;
    u8* ip;

    count = ReadU16LE(state->chunk);
    pal = state->chunk + 4;
    ip = pal + count * 12;
    DCInvalidateRange((void*)param_6, state->width * state->height * 2);
    switch (param_4) {
    case 0:
        fn_800D86C8((u32)state, pal, count);
        break;
    case 1:
        fn_800D860C((u32)state, pal, count);
        break;
    case 2: {
        int i;
        int off;
        u8* p;
        u8 sh1;
        u8 sh0;
        u8 sh2;
        int n;
        sh1 = 8 - state->greenBits;
        i = 0;
        sh0 = 8 - state->redBits;
        p = pal;
        sh2 = 8 - state->blueBits;
        off = i;
        n = count * 4;
        for (; i < n; i++) {
            fn_800DBE98(state, p);
            *(u16*)(pal + off) =
                (((p[0] >> sh0) << state->redShift)
                 | ((p[1] >> sh1) << state->greenShift))
                | ((p[2] >> sh2) << state->blueShift);
            p += 3;
            off += 2;
        }
        break;
    }
    default:
        return -1;
    }

    {
        int dir;
        int row;

        if (*(int*)(param_5 + offsetof(MovieBitmapHeader, height)) < 0) {
            dir = -1;
            row = state->height - 1;
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

            w = state->width;
            bp = ip + 1;
            d8 = dir * 8;
            bits = *ip;
            ip += ((w / 2) * state->height) / 2 / 8;
            state->width = w << 1;
            d2 = dir * 2;
            nb = 0;
            for (y = 0; y < state->height; y += 2) {
                u8* dst;
                u8* dst2;
                int x;

                dst = (u8*)param_6 + (row & ~3) * state->width;
                dst += (row & 3) * 8;
                dst2 = dst + d8;
                x = 0;
                do {
                    u8 idx;
                    u32 val;
                    u8* entry;
                    int adv;

                    idx = *ip;
                    val = idx;
                    val = (val & 0xFF) | (((bits >> nb) & 1) << 8);
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
                } while (x < state->width);
                row += d2;
            }
            state->width = state->width / 2;
        } else {
            int d8;
            int d2;
            int y;

            d8 = dir * 8;
            d2 = dir * 2;
            state->width <<= 1;
            for (y = 0; y < state->height; y += 2) {
                u8* dst;
                u8* dst2;
                int x;

                dst = (u8*)param_6 + (row & ~3) * state->width;
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
                } while (x < state->width);
                row += d2;
            }
            state->width = state->width / 2;
        }
    }
    DCFlushRange((void*)param_6, state->width * state->height * 2);
    GXInvalidateTexAll();
    state->frame = state->frame + 1;
    return 0;
}

/* VQ chunk -> buffer copy (ReadU16LE/ReadF32LE, memcpy) */
u32 fn_800D8F28(MovieDecodeState* state, int unused1, char* unused2,
                 int bitmap, u8* dst) {
    int count;
    int nbits;
    u8* masks;
    u8* palette;
    u8* indices;
    int i;
    int offset;
    int rowStride;

    count = ReadU16LE(state->chunk);
    nbits = ReadF32LE(state->chunk + 4);
    masks = state->chunk + 8;
    palette = masks + state->paletteOffset;
    indices = palette + count * 12;
    i = 0;
    offset = 0;
    for (; i < count; i++, offset += 12) {
        u8* entry = palette + offset;
        fn_800DBE98(state, entry);
        fn_800DBE98(state, entry + 3);
        fn_800DBE98(state, entry + 6);
        fn_800DBE98(state, entry + 9);
    }

    if (*(int*)(bitmap + 8) < 0) {
        dst += state->width * (state->height - 1) * 3;
        rowStride = -state->width * 3;
    } else {
        rowStride = state->width * 3;
    }

    if (count > 256) {
        s32 bits;
        u8* bitBytes;
        u8 bitCount;
        int y;

        bits = *indices;
        bitBytes = indices;
        indices += (nbits + 7) / 8;
        bitCount = 0;
        y = 0;
        bitBytes++;
        for (; y < state->height; y += 2) {
            u8* maskRow = masks + (y / 4) * state->maskStride;
            int x = 0;
            for (; x < state->width; x += 2) {
                int b = x >> 2;
                if ((1 << (b & 7)) & maskRow[b / 8]) {
                    u8* entry;

                    entry = palette +
                            ((((bits >> bitCount) & 1U) << 8) | *indices) *
                                12;
                    indices++;
                    memcpy(dst, entry, 6);
                    memcpy(dst + rowStride, entry + 6, 6);
                    bitCount++;
                    if (bitCount == 8) {
                        bits = *bitBytes;
                        bitCount = 0;
                        bitBytes++;
                    }
                }
                dst += 6;
            }
            dst += rowStride;
        }
    } else {
        int y = 0;
        for (; y < state->height; y += 2) {
            u8* maskRow = masks + (y / 4) * state->maskStride;
            int x = 0;
            int one = 1;
            for (; x < state->width; x += 2) {
                int b = x >> 2;
                if ((one << (b & 7)) & maskRow[b >> 3]) {
                    u8* entry = palette + *indices * 12;
                    indices++;
                    memcpy(dst, entry, 6);
                    memcpy(dst + rowStride, entry + 6, 6);
                }
                dst += 6;
            }
            dst += rowStride;
        }
    }
    state->frame++;
    return 0;
}

/* VQ chunk -> buffer copy (ReadU16LE, memcpy) */
u32 fn_800D91B4(MovieDecodeState* state, int unused1, char* unused2,
                 int bitmap, u8* dst) {
    int count;
    u8* palette;
    int rowStride;
    u8* indices;
    int i;
    int offset;

    count = ReadU16LE(state->chunk);
    palette = state->chunk + 4;
    indices = palette + count * 12;
    i = 0;
    offset = 0;
    for (; i < count; i++, offset += 12) {
        u8* entry = palette + offset;
        fn_800DBE98(state, entry);
        fn_800DBE98(state, entry + 3);
        fn_800DBE98(state, entry + 6);
        fn_800DBE98(state, entry + 9);
    }

    if (*(int*)(bitmap + 8) < 0) {
        dst += state->width * (state->height - 1) * 3;
        rowStride = -state->width * 3;
    } else {
        rowStride = state->width * 3;
    }

    if (count > 256) {
        s32 bits;
        u8* bitBytes;
        u8 bitCount;
        int y;

        bits = *indices;
        bitBytes = indices;
        indices += (state->width / 2 * state->height / 2) / 8;
        bitCount = 0;
        y = 0;
        bitBytes++;
        for (; y < state->height; y += 2) {
            int x = 0;
            for (; x < state->width; x += 2) {
                u8* entry;

                entry = palette +
                        ((((bits >> bitCount) & 1U) << 8) | *indices) * 12;
                indices++;
                memcpy(dst, entry, 6);
                memcpy(dst + rowStride, entry + 6, 6);
                bitCount++;
                if (bitCount == 8) {
                    bits = *bitBytes;
                    bitCount = 0;
                    bitBytes++;
                }
                dst += 6;
            }
            dst += rowStride;
        }
    } else {
        int y = 0;
        for (; y < state->height; y += 2) {
            int x = 0;
            for (; x < state->width; x += 2) {
                u8* entry = palette + *indices * 12;
                indices++;
                memcpy(dst, entry, 6);
                memcpy(dst + rowStride, entry + 6, 6);
                dst += 6;
            }
            dst += rowStride;
        }
    }
    state->frame++;
    return 0;
}

/* VQ frame parser: dispatches the fn_800D87FC/8BCC/8F28/91B4 decoders */
#pragma opt_propagation off
u32 fn_800D93D4(u32* param_1, u32 param_2, int param_3, char* param_4, int param_5, u8* param_6) {
    int iVar4;
    int iVar1;
    u8 hasAlpha;
    u8 auStack_20[8];

    iVar4 = *(int*)(param_3 + offsetof(MovieBitmapHeader, sizeImage));
    iVar1 = ReadF32LE((u8*)param_4);
    if (1 < iVar1) {
        iVar4 = ReadF32LE((u8*)param_4);
        param_4 = param_4 + 8;
    }
    fn_800D9DBC((u32)auStack_20, param_4, iVar4, (u8*)param_1[6]);
    hasAlpha = (u16)ReadU16LE((u8*)(param_1[6] + 2)) != 0;
    if (hasAlpha == 0) {
        switch (*(int*)(param_5 + offsetof(MovieBitmapHeader, compression))) {
        case 0:
        case 3:
            if (*(u16*)(param_5 + offsetof(MovieBitmapHeader, bitCount)) == 0x18) {
                return fn_800D8F28((MovieDecodeState*)param_1, param_3,
                                    param_4, param_5, param_6);
            }
            if (*(u16*)(param_5 + offsetof(MovieBitmapHeader, bitCount)) == 0x10) {
                return fn_800D87FC((MovieDecodeState*)param_1, param_3, param_4, 2, param_5, param_6);
            }
            break;
        case 0x59565955:
            return fn_800D87FC((MovieDecodeState*)param_1, param_3, param_4, 0, param_5, param_6);
        case 0x32595559:
            return fn_800D87FC((MovieDecodeState*)param_1, param_3, param_4, 1, param_5, param_6);
        }
    } else {
        switch (*(int*)(param_5 + offsetof(MovieBitmapHeader, compression))) {
        case 0:
        case 3:
            if (*(u16*)(param_5 + offsetof(MovieBitmapHeader, bitCount)) == 0x18) {
                return fn_800D91B4((MovieDecodeState*)param_1, param_3,
                                    param_4, param_5, param_6);
            }
            if (*(u16*)(param_5 + offsetof(MovieBitmapHeader, bitCount)) == 0x10) {
                return fn_800D8BCC((MovieDecodeState*)param_1, param_3, param_4, 2, param_5, param_6);
            }
            break;
        case 0x59565955:
            return fn_800D8BCC((MovieDecodeState*)param_1, param_3, param_4, 0, param_5, param_6);
        case 0x32595559:
            return fn_800D8BCC((MovieDecodeState*)param_1, param_3, param_4, 1, param_5, param_6);
        }
    }
    return 0xffffffff;
}
#pragma opt_propagation reset

void fn_800D9614(u32* param_1, MovieDecodeCall* param_2) {
    u32 arg1 = param_2->context;
    u32 arg3 = param_2->bitmap;
    char* arg2 = param_2->chunk;
    u8* arg4 = param_2->destination;

    fn_800D93D4(param_1, param_2->flags, arg1, arg2, arg3, arg4);
}

void fn_800D9648(u32* param_1, MovieDecodeCall* param_2) {
    u32 arg1 = param_2->context;
    u32 arg3 = param_2->bitmap;
    char* arg2 = param_2->chunk;
    u8* arg4 = param_2->destination;

    fn_800D93D4(param_1, param_2->flags, arg1, arg2, arg3, arg4);
}

void fn_800D967C(register int param_1, register MovieDecodeCall* param_2) {
    register u32 arg3;
    register u32 arg2;
    register void (*dispatch)(int, u32, u32);

    dispatch = *(void (**)(int, u32, u32))(*(u32*)(param_1 + 32) + 12);
    arg3 = param_2->bitmap;
    arg2 = param_2->context;
    dispatch(param_1, arg2, arg3);
}

/* Initialize a VQ frame buffer and its 16-bit component selectors.
 * header's BI_BITFIELDS masks feed the shift/bit fields in reversed order
 * (mask0->blueShift/Bits, mask1->greenShift/Bits, mask2->redShift/Bits) -
 * preserved faithfully from the original raw-offset wiring, not asserted as
 * a literal R/G/B channel identity. */
u32 fn_800D96B0(MovieDecodeState* self, u32 unused, u8* header)
{
    s32 height;
    s32 halfPixels;
    s32 size;

    (void)unused;
    self->width = *(u32*)(header + offsetof(MovieBitmapHeader, width));
    height = *(s32*)(header + offsetof(MovieBitmapHeader, height));
    self->height = height < 0 ? (u32)-height : (u32)height;
    self->_08[0] = *(u16*)(header + offsetof(MovieBitmapHeader, bitCount));
    self->maskStride = ((s32)self->width + 31) / 32;
    self->paletteOffset = ((s32)self->height / 4) * self->maskStride;
    halfPixels = (((s32)self->width / 2) * (s32)self->height) / 2;
    size = halfPixels + 6146;
    size += halfPixels / 8;
    size += self->paletteOffset;

    if (*(u16*)(header + offsetof(MovieBitmapHeader, bitCount)) == 16) {
        if (*(u32*)(header + offsetof(MovieBitmapHeader, compression)) == 0) {
            self->blueShift = 0;
            self->blueBits = 5;
            self->greenShift = 5;
            self->greenBits = 5;
            self->redShift = 10;
            self->redBits = 5;
        } else if (*(u32*)(header + offsetof(MovieBitmapHeader, compression)) == 3) {
            self->blueShift = fn_800DBD00(self, *(s32*)(header + offsetof(MovieBitmapHeader, redMask)));
            self->blueBits = fn_800DBCCC(self, *(s32*)(header + offsetof(MovieBitmapHeader, redMask)));
            self->greenShift = fn_800DBD00(self, *(s32*)(header + offsetof(MovieBitmapHeader, greenMask)));
            self->greenBits = fn_800DBCCC(self, *(s32*)(header + offsetof(MovieBitmapHeader, greenMask)));
            self->redShift = fn_800DBD00(self, *(s32*)(header + offsetof(MovieBitmapHeader, blueMask)));
            self->redBits = fn_800DBCCC(self, *(s32*)(header + offsetof(MovieBitmapHeader, blueMask)));
        }
    }

    if (self->chunk != 0) {
        gMovieAllocCount--;
        if (gMovieAllocCount == 0) {
            ResetAllocTot();
        }
    }
    self->chunk = (u8*)AllocHiMem((u32)size + 308, (u32)gMovieAllocCount++);
    self->frame = 0;
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

    iVar3 = *(int*)(param_2 + offsetof(MovieDecodeCall, context));
    iVar2 = *(u32*)(param_2 + offsetof(MovieDecodeCall, bitmap));
    uVar1 = *(s32*)(iVar3 + offsetof(MovieBitmapHeader, width));
    uVar4 = *(s32*)(iVar3 + offsetof(MovieBitmapHeader, height));
    if (uVar1 % 4 != 0 || uVar4 % 4 != 0) {
        return 0xfffffffe;
    }
    if (*(s32*)(iVar3 + offsetof(MovieBitmapHeader, compression)) != 0x5644564d ||
        *(u16*)(iVar3 + offsetof(MovieBitmapHeader, bitCount)) != 0x18) {
        return 0xfffffffe;
    }
    if (iVar2 == 0) {
        return 0;
    }
    inputWidth = *(s32*)(iVar2 + offsetof(MovieBitmapHeader, width));
    inputHeight = *(s32*)(iVar2 + offsetof(MovieBitmapHeader, height));
    if (inputWidth != uVar1 ||
        (inputHeight != uVar4 && inputHeight != -uVar4)) {
        return 0xfffffffe;
    }
    switch (*(s32*)(iVar2 + offsetof(MovieBitmapHeader, compression))) {
    case 0:
        if (*(u16*)(iVar2 + offsetof(MovieBitmapHeader, bitCount)) != 0x18 &&
            *(u16*)(iVar2 + offsetof(MovieBitmapHeader, bitCount)) != 0x10) {
            return 0xfffffffe;
        }
        break;
    case 3:
        if (*(u16*)(iVar2 + offsetof(MovieBitmapHeader, bitCount)) != 0x10) {
            return 0xfffffffe;
        }
        break;
    case 0x32595559:
    case 0x59565955:
        if (*(u16*)(iVar2 + offsetof(MovieBitmapHeader, bitCount)) != 0x10) {
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

u32 fn_800D9A14(MovieRingBuffer* param_1, u8* param_2, int param_3, u8 param_4) {
    u32 writeOffset;
    int used;
    u32 readOffset;
    int chunk;

    writeOffset = param_1->writePos;
    readOffset = param_1->readPos;
    if ((int)writeOffset >= (int)readOffset) {
        used = writeOffset - readOffset;
    } else {
        used = param_1->size + (writeOffset - readOffset);
    }
    if (param_3 > used) {
        return 0;
    }
    if (param_4 != 0) {
        chunk = param_1->size - readOffset;
        if (chunk > param_3) {
            chunk = param_3;
        }
        memcpy(param_2, param_1->buffer + readOffset, chunk);
        param_2 += chunk;
        param_3 -= chunk;
        param_1->readPos += chunk;
        if ((int)param_1->readPos == (int)param_1->size) {
            param_1->readPos = 0;
        }
        if (param_3 != 0) {
            memcpy(param_2, param_1->buffer, param_3);
            param_1->readPos += param_3;
        }
    } else {
        chunk = param_1->size - readOffset;
        if (chunk > param_3) {
            chunk = param_3;
        }
        memcpy(param_2, param_1->buffer + readOffset, chunk);
        param_3 -= chunk;
        param_2 += chunk;
        if (param_3 != 0) {
            memcpy(param_2, param_1->buffer, param_3);
        }
    }
    return 1;
}

u32 fn_800D9B48(MovieRingBuffer* param_1, u8* param_2, int param_3) {
    u32 readOffset;
    int used;
    u32 writeOffset;
    int chunk;

    writeOffset = param_1->writePos;
    readOffset = param_1->readPos;
    if ((int)writeOffset >= (int)readOffset) {
        used = writeOffset - readOffset;
    } else {
        used = param_1->size + (writeOffset - readOffset);
    }
    if (param_3 > (int)((param_1->size - used) - 1)) {
        return 0;
    }
    chunk = param_1->size - writeOffset;
    if (chunk > param_3) {
        chunk = param_3;
    }
    memcpy(param_1->buffer + writeOffset, param_2, chunk);
    param_2 += chunk;
    param_3 -= chunk;
    param_1->writePos += chunk;
    if ((int)param_1->writePos == (int)param_1->size) {
        param_1->writePos = 0;
    }
    if (param_3 != 0) {
        memcpy(param_1->buffer, param_2, param_3);
        param_1->writePos += param_3;
    }
    return 1;
}

#pragma dont_inline on
int fn_800D9C34(MovieRingBuffer* p) {
    int hi = p->writePos;
    int lo = p->readPos;
    if (hi >= lo) {
        return hi - lo;
    }
    return (int)p->size + (hi - lo);
}
#pragma dont_inline off

#pragma dont_inline on
void fn_800D9C5C(MovieRingBuffer* p, int n) {
    if (p->buffer != 0) {
        gMovieAllocCount--;
        if (gMovieAllocCount == 0) {
            ResetAllocTot();
        }
    }
    p->buffer = 0;
    p->size = n;
    p->buffer = (u8*)AllocHiMem(p->size, (u32)gMovieAllocCount++);
    p->readPos = 0;
    p->writePos = 0;
}
#pragma dont_inline off

/* Kept out-of-line: the target calls this from dtor_800DBB94 (`addi r3,r28,60`
 * / `li r4,-1` / `bl`), and it is that destructor's only call site, so
 * suppressing the auto-inline here reproduces the call without affecting any
 * other function. */
#pragma dont_inline on
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
#pragma dont_inline off

#ifdef __MWERKS__
#pragma optimization_level 4
#pragma peephole on
#pragma scheduling on
#endif

#pragma dont_inline on
void fn_800D9DA4(MovieRingBuffer* p) {
    p->size = 0;
    p->buffer = 0;
    p->readPos = 0;
    p->writePos = 0;
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
void fn_800D9F20(MovieAudioState* audio) {
    u32 uVar1;
    u32 uVar2;
    u32 requestSize;
    u32 requestOffset;
    u8* requestData;

    if (audio->active != 0) {
        adsPoll();
        if (audio->remaining != 0) {
            uVar1 = sndCmd17(
                ((int)audio->buffer + audio->requestSize) -
                    audio->remaining,
                audio->remaining);
            audio->remaining = audio->remaining - uVar1;
        }
        if (audio->remaining == 0) {
            uVar2 = *(int*)(gMovieStreamState + offsetof(MovieState, audioStreamHeader[0x20])) - audio->offset;
            if (0xc000 < uVar2) {
                uVar2 = 0xc000;
            }
            audio->requestSize = uVar2;
            requestSize = audio->requestSize;
            requestOffset = audio->offset;
            requestData = audio->buffer;
            if ((u8)fn_800DB2F4((MovieChunkStream*)(gMovieStreamState + 0x20), requestData,
                                requestOffset, requestSize)) {
                audio->remaining = audio->requestSize;
                audio->offset = audio->offset + audio->requestSize;
            }
        }
    }
}

/* 0x800D9FEC top-level VQ movie playback loop: sets up GX/TEV, decodes+presents each frame (DEMODoneRender/DEMOSwapBuffers), polls pads (G3DGetPadStatusBuffer) to allow skipping, pumps audio (adsPoll/sndCmd17). Xbox: PlayVQMovie. Called by test_movies. */
#define MOVIE_SETUP_DRAW_STATE()                                             \
    {                                                                        \
        MovieGXColor color;                                                  \
        MovieGXColor passColor;                                              \
                                                                             \
        color.r = 0xff;                                                      \
        color.g = 0xff;                                                      \
        color.b = 0xff;                                                      \
        color.a = 0xff;                                                      \
        SetMultiPassTextureParams(0);                                        \
        SetCullMode(0);                                                      \
        SetPerspectiveMode(0);                                               \
        SetViewportHeight(lbl_80349394);                                     \
        SetVertexFormat(2);                                                  \
        fn_800C6AB4(1);                                                      \
        GXInvalidateTexAll();                                                \
        GXLoadTexObj(textureObject, MOVIE_GX_TEXMAP0);                       \
        passColor = color;                                                   \
        GXSetChanMatColor(MOVIE_GX_COLOR0A0, passColor);                     \
        GXSetZMode(MOVIE_GX_TRUE, MOVIE_GX_GEQUAL, MOVIE_GX_TRUE);           \
    }

#pragma cplusplus on
class MovieStreamInterface {
public:
    virtual void method0();
    virtual s32 open(const char* name);
    virtual u8 update(u8* texture, f32 elapsed);
    virtual void close();
};

extern "C" void PlayVQMovie(const char* name) throw()
{
    u8* movie;
    MovieGXTexObj* textureObject = &lbl_80321308;
    u8* texture;
    s32* dimensions;
    s32 height;
    s32 paddedHeight;
    s32 scan;
    s32 oneBits;
    s32 shifts;
    s32 textureBytes;
    s32 stopCount = 0;
    u32 lastTime;
    u32 now;
    s32 i;
    f32 texV;
    f32 elapsed;
    PADStatus* pads;
    u8 unusedHigh[40];
    PADStatus previousPads[4];
    u8 unusedLow[4];

    movie = (u8*)AllocHiMem(472, (u32)gMovieAllocCount++);
    gMovieStreamState = movie = (u8*)__construct_new_array(
        movie, (void*)fn_800DB0F8, (void*)__dt__11MoviePlayerFv, 464, 1);
    ((MovieStreamInterface*)gMovieStreamState)->open(name);

    dimensions = (s32*)(gMovieStreamState + 408);
    if (dimensions[0] != 512) {
        ((MovieStreamInterface*)gMovieStreamState)->close();
        __destroy_new_array(gMovieStreamState, (void*)__dt__11MoviePlayerFv);
        ResetAllocTot();
        return;
    }

    height = *(s32*)(gMovieStreamState + offsetof(MovieState, fileFormat.height));
    oneBits = 0;
    shifts = 0;
    scan = height;
    while (scan != 0) {
        shifts++;
        if ((scan & 1) != 0) {
            oneBits++;
        }
        scan >>= 1;
    }
    paddedHeight = height;
    if (oneBits > 1) {
        paddedHeight = 1 << shifts;
    }

    textureBytes = dimensions[0] * paddedHeight;
    texV = (f32)height / (f32)paddedHeight;
    textureBytes *= 2;
    texture = (u8*)AllocHiMem((u32)textureBytes, (u32)gMovieAllocCount++);
    memset(texture, 0, (u32)textureBytes);
    GXInitTexObj(textureObject, texture,
                 (u16)*(s32*)(gMovieStreamState + offsetof(MovieState, fileFormat.width)),
                 (u16)paddedHeight, MOVIE_GX_TF_RGB565, MOVIE_GX_CLAMP,
                 MOVIE_GX_CLAMP, 0);
    MOVIE_SETUP_DRAW_STATE();

    pads = G3DGetPadStatusBuffer();
    for (i = 0; i < 4; i++) {
        previousPads[i] = pads[i];
    }
    pbPulseTime();
    lastTime = sSeconds;
    elapsed = lbl_80349390;
    while (stopCount < 3 &&
           ((MovieStreamInterface*)gMovieStreamState)->update(texture,
                                                              elapsed) != 0) {
        if (stopCount != 0) {
            stopCount++;
        }

        if (lbl_80344A5D != 0) {
            MOVIE_SETUP_DRAW_STATE();
            lbl_80344A5D = 0;
        }

        sysResetService();
        for (i = 0; i < 4; i++) {
            if (pads[i].err == 0 && previousPads[i].err == 0 &&
                pads[i].button != 0 && previousPads[i].button == 0) {
                stopCount++;
                MOVIE_SETUP_DRAW_STATE();
            }
            previousPads[i] = pads[i];
        }

        DCFlushRange(texture, (u32)textureBytes);
        DEMOSwapBuffers();
        GXBegin(MOVIE_GX_TRIANGLES, MOVIE_GX_VTXFMT0, 6);
        GXPosition3f32(lbl_80349398, lbl_80349398, lbl_8034939C);
        GXTexCoord2f32(lbl_80349390, texV);
        GXPosition3f32(lbl_80349394, lbl_80349398, lbl_8034939C);
        GXTexCoord2f32(lbl_80349394, texV);
        GXPosition3f32(lbl_80349394, lbl_80349394, lbl_8034939C);
        GXTexCoord2f32(lbl_80349394, lbl_80349390);
        GXPosition3f32(lbl_80349394, lbl_80349394, lbl_8034939C);
        GXTexCoord2f32(lbl_80349394, lbl_80349390);
        GXPosition3f32(lbl_80349398, lbl_80349394, lbl_8034939C);
        GXTexCoord2f32(lbl_80349390, lbl_80349390);
        GXPosition3f32(lbl_80349398, lbl_80349398, lbl_8034939C);
        GXTexCoord2f32(lbl_80349390, texV);
        DEMODoneRender();
        GXSetZMode(MOVIE_GX_TRUE, MOVIE_GX_GEQUAL, MOVIE_GX_TRUE);

        {
            MovieAudioState* audio =
                *(MovieAudioState**)(gMovieStreamState + offsetof(MovieState, audio));

            if (audio->active != 0) {
                adsPoll();
                if (audio->remaining != 0) {
                    audio->remaining -= sndCmd17(
                        (s32)(audio->buffer + audio->requestSize) -
                            audio->remaining,
                        audio->remaining);
                }
                if (audio->remaining == 0) {
                    u32 request =
                        *(u32*)(gMovieStreamState + offsetof(MovieState, audioStreamHeader[0x20])) - audio->offset;
                    u8* requestData;
                    u32 requestOffset;
                    u32 requestSize;

                    if (request > 0xc000) {
                        request = 0xc000;
                    }
                    audio->requestSize = request;
                    requestSize = audio->requestSize;
                    requestOffset = audio->offset;
                    requestData = audio->buffer;
                    if ((u8)fn_800DB2F4((MovieChunkStream*)(gMovieStreamState + 32),
                                        requestData,
                                        requestOffset,
                                        requestSize) != 0) {
                        audio->remaining = audio->requestSize;
                        audio->offset += audio->requestSize;
                    }
                }
            }
        }

        pbPulseTime();
        now = sSeconds;
        elapsed = (f32)(now - lastTime) / lbl_803493A0;
        lastTime = now;
    }

    ((MovieStreamInterface*)gMovieStreamState)->close();
    if (texture != NULL) {
        gMovieAllocCount--;
        if (gMovieAllocCount == 0) {
            ResetAllocTot();
        }
    }
    __destroy_new_array(gMovieStreamState, (void*)__dt__11MoviePlayerFv);
    ResetAllocTot();
}
#pragma cplusplus off

#undef MOVIE_SETUP_DRAW_STATE

/* movie close/cleanup (AudioStreamStop, operator delete, sceClose) */
extern void AudioStreamStop(void);
extern s32 sceClose(s32 fd);
void fn_800DBA80(u8* dec, s32 fd);
void __dl__FPv(void* p);
void __dla__FPv(void* p);

typedef struct MovieCloseVTable {
    u8 pad[28];
    void (*close)(u8*);
} MovieCloseVTable;

#pragma dont_inline on
void fn_800DA60C(register u8* m)
{
    register u8* strm;
    register u8* self = m;
    register u32 active;

    active = *(u32*)(self + offsetof(MovieState, audio));
    if (active != 0) {
        AudioStreamStop();
        if ((strm = *(u8**)(self + offsetof(MovieState, audio))) != 0) {
            AudioStreamStop();
            __dla__FPv(*(void**)(strm + 4));
            __dl__FPv(strm);
        }
        *(u32*)(self + offsetof(MovieState, audio)) = 0;
    }
    (*(MovieCloseVTable**)(self + offsetof(MovieState, decoderVtable)))->close(self + 336);
    fn_800DBA80(self + 32, *(s32*)(self + offsetof(MovieState, fd)));
    if (*(s32*)(self + offsetof(MovieState, fd)) != 0) {
        sceClose(*(s32*)(self + offsetof(MovieState, fd)));
    }
    *(s32*)(self + offsetof(MovieState, fd)) = 0;
}
#pragma dont_inline off

/* Advance the VQ stream by one presentation interval and prime its audio. */
u32 fn_800DA6A4(register u8* movie, register u32 decodeFrame, f32 elapsed)
{
    u8 unused[24];
    register MovieAudioState* audio;
    u32 frame;
    register u32* chunk;

    if (*(s32*)(movie + offsetof(MovieState, fd)) == 0 || movie[0x1A] != 0) {
        return FALSE;
    }
    if (gMovieFrameTimeReset != 0) {
        elapsed = lbl_80349390;
        gMovieFrameTimeReset = 0;
    }
    if (elapsed > lbl_803493B8) {
        elapsed = lbl_803493BC;
    }

    fn_800DB3D4((MovieChunkStream*)(movie + 0x20), *(s32*)(movie + offsetof(MovieState, fd)), 0xA000);
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
            *(MovieAudioState**)(movie + offsetof(MovieState, audio)) = audio;
            audio = *(MovieAudioState**)(movie + offsetof(MovieState, audio));
            audio->active = (u8)fn_800DB2F4((MovieChunkStream*)(gMovieStreamState + 0x20), audio->buffer, 0, 0xC000);
            if (audio->active != 0) {
                audio->offset = 0xC000;
                audio->remaining = 0xC000;
                audio->remaining -= sndCmd17((s32)audio->buffer, 0x6000);
                audio->remaining -= sndCmd17((s32)(audio->buffer + 0x6000), 0x6000);
                fn_800D9F20(audio);
                sndCmdA(lbl_80343B4C, 0, 1, gMovieAudioCallback);
            }
        }
        movie[0x19] = 0;
    } else {
        *(f32*)(movie + offsetof(MovieState, playTime)) += elapsed;
    }

    frame = __cvt_fp2unsigned(*(f32*)(movie + offsetof(MovieState, playTime)) / *(f32*)(movie + offsetof(MovieState, frameInterval)));
    if (frame <= *(u32*)(movie + offsetof(MovieState, frameIndex))) {
        goto done;
    }
    {
        ++*(u32*)(movie + offsetof(MovieState, frameIndex));
        chunk = (u32*)fn_800DB36C((MovieChunkStream*)(movie + 0x20));
        while (chunk != NULL && chunk[8] == 0) {
            fn_800DB29C((MovieChunkStream*)(movie + 0x20));
            chunk = (u32*)fn_800DB36C((MovieChunkStream*)(movie + 0x20));
        }
        if (chunk == NULL) {
            return *(u32*)(movie + offsetof(MovieState, frameIndex)) < *(u32*)(movie + offsetof(MovieState, videoStreamHeader[0x20]));
        }
        if (decodeFrame == 0) {
            fn_800DB29C((MovieChunkStream*)(movie + 0x20));
            return TRUE;
        }
        *(u32*)(movie + offsetof(MovieState, fileFormat.sizeImage)) = chunk[4];
        *(u32*)(movie + offsetof(MovieState, decodeCall.chunk)) = chunk[8];
        *(s32*)(movie + offsetof(MovieState, decodeCall.destination)) = decodeFrame;
        {
            register void (*decode)(u8*, u8*, s32) =
                *(void (**)(u8*, u8*, s32))(*(u32*)(movie + offsetof(MovieState, decoderVtable)) + 0x18);
            decode(movie + 0x150, movie + 0x11C, 0);
        }
        fn_800DB29C((MovieChunkStream*)(movie + 0x20));
    }
done:
    return TRUE;
}

/* movie open: sceOpen/sceRead the Gauntlet VQMovies .avi file, asserts on failure (MoviePlayer.cpp) */
#pragma cplusplus on
#pragma dont_inline on
extern "C" s32 fn_800DA920(u8* movie, const char* name)
{
    u8 headerStorage[4128];
    u8* header;
    const char* selected;
    s32 offset;

    header = headerStorage;
    header += (32 - ((u32)header & 31)) & 31;
    *(f32*)(movie + offsetof(MovieState, playTime)) = lbl_80349390;
    *(u32*)(movie + offsetof(MovieState, frameIndex)) = 0;
    if (*(s32*)(movie + offsetof(MovieState, fd)) != 0) {
        sceClose(*(s32*)(movie + offsetof(MovieState, fd)));
        *(s32*)(movie + offsetof(MovieState, fd)) = 0;
    }
    if (*(u8**)(movie + offsetof(MovieState, audio)) != NULL) {
        u8* stream;

        AudioStreamStop();
        if ((stream = *(u8**)(movie + offsetof(MovieState, audio))) != NULL) {
            AudioStreamStop();
            __dla__FPv(*(void**)(stream + 4));
            __dl__FPv(stream);
        }
        *(u32*)(movie + offsetof(MovieState, audio)) = 0;
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

    *(s32*)(movie + offsetof(MovieState, fd)) = sceOpen(selected, 1);
    sceRead(*(s32*)(movie + offsetof(MovieState, fd)), header, 4096);
    if (!(u8)fn_800DACD8((s32)movie, header)) {
        return 0;
    }

    *(f32*)(movie + offsetof(MovieState, frameRate)) =
        (f32)((f64)*(u32*)(movie + offsetof(MovieState, videoStreamHeader[0x18])) / (f64)*(u32*)(movie + offsetof(MovieState, videoStreamHeader[0x14])));
    *(f32*)(movie + offsetof(MovieState, frameInterval)) = lbl_80349394 / *(f32*)(movie + offsetof(MovieState, frameRate));

    *(u32*)(movie + offsetof(MovieState, dataOffset)) = 0;
    for (offset = 0; offset < 4096; offset += 4) {
        if (ReadF32LE(header + offset) == 0x4B4E554A) {
            *(u32*)(movie + offsetof(MovieState, dataOffset)) = offset + 4;
            break;
        }
    }
    if (*(u32*)(movie + offsetof(MovieState, dataOffset)) != 0) {
        *(u32*)(movie + offsetof(MovieState, dataOffset)) = 0;
        for (; offset < 4096; offset += 4) {
            if (ReadF32LE(header + offset) == 0x69766F6D) {
                *(u32*)(movie + offsetof(MovieState, dataOffset)) = offset + 4;
                break;
            }
        }
    }
    if (*(u32*)(movie + offsetof(MovieState, dataOffset)) == 0) {
        *(u32*)(movie + offsetof(MovieState, dataOffset)) = 2048;
    }

    sceLseek(*(s32*)(movie + offsetof(MovieState, fd)), *(s32*)(movie + offsetof(MovieState, dataOffset)), 0);
    fn_800D99AC((u32)(movie + 336), (int*)(movie + 404), movie + 124);
    *(u16*)(movie + offsetof(MovieState, outFormat.bitCount)) = 16;
    *(u32*)(movie + offsetof(MovieState, outFormat.compression)) = 3;
    *(u32*)(movie + offsetof(MovieState, outFormat.redMask)) = 0xF800;
    *(u32*)(movie + offsetof(MovieState, outFormat.greenMask)) = 2016;
    *(u32*)(movie + offsetof(MovieState, outFormat.blueMask)) = 31;
    *(u32*)(movie + offsetof(MovieState, outFormat.sizeImage)) = *(u32*)(movie + offsetof(MovieState, outFormat.width)) *
                           *(u32*)(movie + offsetof(MovieState, outFormat.height)) * 2;
    *(s32*)(movie + offsetof(MovieState, outFormat.height)) = -*(s32*)(movie + offsetof(MovieState, outFormat.height));
    *(u8**)(movie + offsetof(MovieState, decodeCall.context)) = movie + 404;
    *(u8**)(movie + offsetof(MovieState, decodeCall.bitmap)) = movie + 124;

    if (MovieValidateFrameFormat((u32)(movie + 336),
                                 (s32)(movie + 284), 0) != 0) {
        return 0;
    }
    {
        class MovieConfigureBase {
        private:
            u8 pad[32];
        };
        class MovieConfigureObject : public MovieConfigureBase {
        public:
            virtual void method0();
            virtual void method1();
            virtual void configure(u8*, s32);
        };
        ((MovieConfigureObject*)(movie + 336))->configure(movie + 284, 0);
    }
    MovieDecoderInitBuffers((MovieChunkStream*)(movie + 32), 0x80000, movie[24]);
    fn_800DB82C((MovieChunkStream*)(movie + 32), *(s32*)(movie + offsetof(MovieState, fd)),
                *(u32*)(movie + offsetof(MovieState, dataOffset)));
    *(u32*)(movie + offsetof(MovieState, stream.videoFrameLimit)) = *(u32*)(movie + offsetof(MovieState, videoStreamHeader[0x20]));
    movie[25] = 1;
    movie[26] = 0;
    gMovieFrameTimeReset = 0;
    return 1;
}
#pragma dont_inline off
#pragma cplusplus off

/* VQ .avi header parser (ReadF32LE/ReadU16LE/ReadU32LE) */
u32 fn_800DACD8(int param_1, u8* param_2) {
    u8* q;
    int ofs;
    int strl;
    u8* p;

    /* hasAudio. Left as a bare literal deliberately: this is the function's
     * FIRST statement, and the offsetof form regresses it (+1 addi, real
     * 76 -> 119) while the identical conversion of the very same field at
     * line 2009 is byte-neutral. Operand-order swap A/B'd identically. */
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
    *(u32*)(param_1 + offsetof(MovieState, videoStreamHeader[0x00])) = ReadF32LE(p);
    *(u32*)(param_1 + offsetof(MovieState, videoStreamHeader[0x04])) = ReadF32LE(p + 4);
    *(u32*)(param_1 + offsetof(MovieState, videoStreamHeader[0x08])) = ReadF32LE(p + 8);
    *(u32*)(param_1 + offsetof(MovieState, videoStreamHeader[0x0C])) = ReadF32LE(p + 0xC);
    *(u16*)(param_1 + offsetof(MovieState, videoStreamHeader[0x10])) = ReadU16LE(p + 0x10);
    *(u16*)(param_1 + offsetof(MovieState, videoStreamHeader[0x12])) = ReadU16LE(p + 0x12);
    *(u32*)(param_1 + offsetof(MovieState, videoStreamHeader[0x14])) = ReadF32LE(p + 0x14);
    *(u32*)(param_1 + offsetof(MovieState, videoStreamHeader[0x18])) = ReadF32LE(p + 0x18);
    *(u32*)(param_1 + offsetof(MovieState, videoStreamHeader[0x1C])) = ReadF32LE(p + 0x1C);
    *(u32*)(param_1 + offsetof(MovieState, videoStreamHeader[0x20])) = ReadF32LE(p + 0x20);
    *(u32*)(param_1 + offsetof(MovieState, videoStreamHeader[0x24])) = ReadF32LE(p + 0x24);
    *(u32*)(param_1 + offsetof(MovieState, videoStreamHeader[0x28])) = ReadF32LE(p + 0x28);
    *(u32*)(param_1 + offsetof(MovieState, videoStreamHeader[0x2C])) = ReadF32LE(p + 0x2C);
    *(u32*)(param_1 + offsetof(MovieState, videoStreamHeader[0x30])) = ReadF32LE(p + 0x30);
    ofs += ReadF32LE(q);
    if (ReadF32LE(param_2 + ofs + 4) != 0x66727473) {
        return 0;
    }
    q = param_2 + (u32)ofs + 0xC;
    *(u32*)(param_1 + offsetof(MovieState, fileFormat.size)) = ReadF32LE(q);
    *(u32*)(param_1 + offsetof(MovieState, fileFormat.width)) = ReadU32LE(q + 4);
    *(u32*)(param_1 + offsetof(MovieState, fileFormat.height)) = ReadU32LE(q + 8);
    *(u16*)(param_1 + offsetof(MovieState, fileFormat.planes)) = ReadU16LE(q + 0xC);
    *(u16*)(param_1 + offsetof(MovieState, fileFormat.bitCount)) = ReadU16LE(q + 0xE);
    *(u32*)(param_1 + offsetof(MovieState, fileFormat.compression)) = ReadF32LE(q + 0x10);
    *(u32*)(param_1 + offsetof(MovieState, fileFormat.sizeImage)) = ReadF32LE(q + 0x14);
    *(u32*)(param_1 + offsetof(MovieState, fileFormat.xPelsPerMeter)) = ReadU32LE(q + 0x18);
    *(u32*)(param_1 + offsetof(MovieState, fileFormat.yPelsPerMeter)) = ReadU32LE(q + 0x1C);
    *(u32*)(param_1 + offsetof(MovieState, fileFormat.clrUsed)) = ReadF32LE(q + 0x20);
    *(u32*)(param_1 + offsetof(MovieState, fileFormat.clrImportant)) = ReadF32LE(q + 0x24);
    *(u32*)(param_1 + offsetof(MovieState, fileFormat.redMask)) = ReadF32LE(q + 0x28);
    *(u32*)(param_1 + offsetof(MovieState, fileFormat.greenMask)) = ReadF32LE(q + 0x2C);
    *(u32*)(param_1 + offsetof(MovieState, fileFormat.blueMask)) = ReadF32LE(q + 0x30);
    q = param_2 + strl;
    if (ReadF32LE(q) == 0x5453494C && ReadF32LE(q + 0x14) == 0x73647561) {
        q = (u8*)((u32)q + 0x14);
        *(u8*)(param_1 + offsetof(MovieState, hasAudio)) = 1;
        *(u32*)(param_1 + offsetof(MovieState, audioStreamHeader[0x00])) = ReadF32LE(q);
        *(u32*)(param_1 + offsetof(MovieState, audioStreamHeader[0x04])) = ReadF32LE(q + 4);
        *(u32*)(param_1 + offsetof(MovieState, audioStreamHeader[0x08])) = ReadF32LE(q + 8);
        *(u32*)(param_1 + offsetof(MovieState, audioStreamHeader[0x0C])) = ReadF32LE(q + 0xC);
        *(u16*)(param_1 + offsetof(MovieState, audioStreamHeader[0x10])) = ReadU16LE(q + 0x10);
        *(u16*)(param_1 + offsetof(MovieState, audioStreamHeader[0x12])) = ReadU16LE(q + 0x12);
        *(u32*)(param_1 + offsetof(MovieState, audioStreamHeader[0x14])) = ReadF32LE(q + 0x14);
        *(u32*)(param_1 + offsetof(MovieState, audioStreamHeader[0x18])) = ReadF32LE(q + 0x18);
        *(u32*)(param_1 + offsetof(MovieState, audioStreamHeader[0x1C])) = ReadF32LE(q + 0x1C);
        *(u32*)(param_1 + offsetof(MovieState, audioStreamHeader[0x20])) = ReadF32LE(q + 0x20);
        *(u32*)(param_1 + offsetof(MovieState, audioStreamHeader[0x24])) = ReadF32LE(q + 0x24);
        *(u32*)(param_1 + offsetof(MovieState, audioStreamHeader[0x28])) = ReadF32LE(q + 0x28);
        *(u32*)(param_1 + offsetof(MovieState, audioStreamHeader[0x2C])) = ReadF32LE(q + 0x2C);
        *(u32*)(param_1 + offsetof(MovieState, audioStreamHeader[0x30])) = ReadF32LE(q + 0x30);
    }
    return 1;
}

/* MoviePlayer teardown (AudioStreamStop, operator delete, dtor_800DBB94) */
/* C++ region: the exception specification must match the definition's. */
#pragma cplusplus on
MovieChunkStream* dtor_800DBB94(MovieChunkStream* self, s16 deleting) throw();

/* MoviePlayer -- the concrete movie player.  RTTI record .sdata 0x80344008 =
 * { name 0x80117648 -> "MoviePlayer", base -> MoviePlayerBase }; vtable
 * __vt__11MoviePlayer, whose first virtual slot is this destructor.  Non-virtual
 * spelling and no exception specification, per the same recipe as the
 * Codec/VQCodec pair above.
 *
 * TWO DELIBERATE DEVIATIONS from the class the RTTI describes.  Both are forced
 * by this TU's split, both are byte-identical to the target, and both are
 * recorded as reconstruction debt rather than hidden:
 *
 * (1) Spelled WITHOUT the `: public MoviePlayerBase` inheritance the RTTI
 *     proves, with the base teardown written out by hand as the trailing
 *     `if (self != NULL) self[0] = __vt__15MoviePlayerBase`.  The target INLINES the base
 *     destructor at that point, and MWCC only inlines a base whose body it has
 *     already seen -- but __dt__15MoviePlayerBaseFv must stay defined further
 *     down, because the target's function order fixes its own .text slot at
 *     0x800DB21C.  Moving that body into the class to make it inlinable would
 *     stop the out-of-line copy being emitted at all (this TU's split owns no
 *     .data, so the vtable that would otherwise odr-use it is linked from
 *     original bytes), trading one exact function for another.
 *
 * (2) `operator delete` is a CLASS member, defined inline, and it is what the
 *     compiler-synthesised deleting branch calls.  This is the mechanism the
 *     target's own layout points to: that branch's allocation-counter code is
 *     INLINED here, yet the global operator-delete pair is emitted AFTER this
 *     function (__dla__FPv 0x800DB15C, __dl__FPv 0x800DB1BC), and MWCC will not
 *     inline a global operator delete it has not yet seen -- measured.  An
 *     inline class-scope operator delete satisfies both constraints at once: it
 *     is available for inlining here, and being inline and never odr-used it
 *     emits no symbol of its own, leaving the global pair's addresses intact. */
class MoviePlayer {
public:
    u32 words[116];

    void operator delete(void* p) throw() {
        if (p != NULL) {
            gMovieAllocCount--;
            if (gMovieAllocCount == 0) {
                ResetAllocTot();
            }
        }
    }

    ~MoviePlayer();
};

MoviePlayer::~MoviePlayer() {
    u32* self = (u32*)this;
    u32* stream;

    self[0] = (u32)__vt__11MoviePlayer;
    if ((s32)self[7] != 0) {
        sceClose((s32)self[7]);
    }
    if (self[100] != 0) {
        AudioStreamStop();
        /* Assignment-in-condition, not a separate statement: the split form
         * loads through r0 and copies (`lwz r0,400 / cmplwi / mr r30,r0`),
         * where the target loads straight into the callee-saved home
         * (`lwz r30,400(r28) / cmplwi r30,0`). */
        if ((stream = (u32*)self[100]) != NULL) {
            AudioStreamStop();
            __dla__FPv((void*)stream[1]);
            __dl__FPv(stream);
        }
    }
    ((VQCodec*)(self + 0x54))->~VQCodec();
    dtor_800DBB94((MovieChunkStream*)(self + 8), -1);
    if (self != NULL) {
        self[0] = (u32)__vt__15MoviePlayerBase;
    }
}
#pragma cplusplus off

u32* fn_800DB0F8(u32* volatile p) {
    u32* self = p;

    self[0] = (u32)__vt__15MoviePlayerBase;
    self[0] = (u32)__vt__11MoviePlayer;
    fn_800DBC64((MovieChunkStream*)(self + 8));
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

/* The real C++ global operator-delete pair. Written as genuine operators (not
 * as C functions spelled with their mangled names): MWCC mangles them to
 * exactly the target's __dla__FPv / __dl__FPv, and only the operator form gets
 * the empty-exception-specification EH scaffolding (r31 frame pointer, the
 * __unexpected island, frame 48) that the target's 24-instruction bodies
 * carry. Defining operator delete HERE -- before the destructor below -- is
 * also what lets MWCC inline it into that destructor's compiler-synthesised
 * deleting branch, which is how the target's dtor body is shaped. */
#pragma cplusplus on

#pragma dont_inline on
void operator delete[](void* p) throw() {
    if (p != NULL) {
        gMovieAllocCount--;
        if (gMovieAllocCount == 0) {
            ResetAllocTot();
        }
    }
}
#pragma dont_inline off

void operator delete(void* p) throw() {
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

/* MoviePlayerBase -- the movie player's abstract base class.  The name is not
 * invented: the GameCube build carries CodeWarrior RTTI, and the base class's
 * vtable __vt__15MoviePlayerBase begins with a pointer to the RTTI record at .sdata
 * 0x80344000, whose layout is validated against the known-named control
 * __RTTI__Q23std9exception (0x80344048 -> "std::exception").  That record
 * reads { name = 0x80117654 -> "MoviePlayerBase", base = NULL }, and the same
 * vtable's only virtual slot is this destructor at 0x800DB21C.  (The derived
 * class resolves the same way: vtable __vt__11MoviePlayer -> RTTI 0x80344008 ->
 * { "MoviePlayer", base -> MoviePlayerBase }, dtor 0x800DB008.)
 *
 * Declared with a NON-virtual destructor on purpose.  CodeWarrior gives every
 * destructor -- virtual or not -- the hidden `short` in-charge parameter and
 * the compiler-synthesised deleting branch, so the non-virtual spelling
 * reproduces the target's body exactly while NOT emitting a __vt__ object into
 * this TU (the real vtable lives in unsplit .data and is linked from the
 * original bytes).  The destructor must also NOT carry `throw()`: an empty
 * exception specification on the destructor itself adds a SECOND __unexpected
 * island and grows the frame 48 -> 72.  Measured against the target: this form
 * is instruction-for-instruction identical, including the `mr r3,r30` that
 * lands inside the epilogue between `lwz r0,52(r31)` and `lmw r30,40(r12)` --
 * the compiler-synthesised `return this` that only a real destructor emits. */
class MoviePlayerBase {
public:
    ~MoviePlayerBase();
};

MoviePlayerBase::~MoviePlayerBase() {
    *(u32*)this = (u32)__vt__15MoviePlayerBase;
}
#pragma cplusplus off

void fn_800DB29C(MovieChunkStream* self) {
    MovieChunkNode* node = self->activeNode;
    MovieChunkNode* next;

    self->activeNode = node->next;
    self->highWater = node->dataOffset + node->totalSize;
    node->next = self->freeListHead;
    self->freeListHead = node;
    next = self->activeNode;
    if (next != NULL && self->highWater + next->totalSize >= self->bufferSize) {
        self->highWater = 0;
    }
}

#ifdef __MWERKS__
#pragma optimization_level 4
#pragma peephole on
#pragma scheduling on
#endif

int fn_800DB2F4(MovieChunkStream* param_1, u8* param_2, u32 param_3, u32 param_4) {
    int iVar1;
    int ret;
    u8 unused[8];
    iVar1 = fn_800D9C34(&param_1->audio);
    if (iVar1 < (int)param_4) {
        memset(param_2, 0, param_4);
        ret = 0;
    } else {
        param_1->_4C = 1;
        fn_800D9A14(&param_1->audio, param_2, param_4, param_1->_4C);
        ret = 1;
    }
    return ret;
}

MovieChunkNode* fn_800DB36C(MovieChunkStream* self) {
    MovieChunkNode* node = self->activeNode;

    if (node == NULL) {
        goto none;
    }
    if (node->videoData != 0) {
        goto ready;
    }
    if (node->audioData != 0) {
        goto ready;
    }
    if (node->junkSize != 0) {
        goto ready;
    }
none:
    return NULL;
ready:
    if (node->next != 0) {
        goto ret;
    }
    if (self->filePos == self->fileSize) {
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
void fn_800DB3D4(MovieChunkStream* stream, s32 fd, volatile u32 length) {
    u32 available;
    u32 chunkOffset;
    u32 chunkSize;
    u32 chunkEnd;
    u32 sz;
    u32 pos;
    u32 offset;
    u32 tag;
    u8* chunk;
    MovieChunkNode* node;
    MovieChunkNode* next;
    s32 len;
    u8 unused[16];

    if (stream->pendingLength == 0 && stream->writePos <= stream->bufferSize - 0x800) {
        goto request_more;
    }

    if (stream->pendingLength <= 0x10000) {
        memcpy(stream->buffer + stream->writePos, (void*)stream->stagingBuffer, stream->pendingLength);
    }
    stream->filePos += stream->pendingLength;
    stream->writePos += stream->pendingLength;
    stream->pendingLength = 0;

    for (;;) {
        node = stream->activeNode;
        while (node != NULL && node->next != NULL) {
            node = node->next;
        }

        chunkOffset = node->dataOffset;
        chunkEnd = chunkOffset + node->totalSize;
        if (chunkEnd >= stream->bufferSize - 0x800) {
            available = stream->writePos - chunkOffset;
            if (available) {
                if (stream->highWater > stream->writePos) {
                    return;
                }
                if (stream->highWater > available) {
                    memcpy((void*)stream->buffer, stream->buffer + chunkOffset, available);
                    stream->writePos = available;
                    node->dataOffset = 0;
                    goto request_more;
                }
                return;
            }
        }

        available = stream->writePos;
        if (available > stream->highWater) {
            if (chunkEnd >= available - 8) {
                goto request_more;
            }
        } else if (chunkOffset < available - 8) {
            if (chunkEnd >= available - 8) {
                goto request_more;
            }
        }

        node->junkSize = 0;
        node->videoSize = 0;
        node->audioSize = 0;
        offset = 0;
        do {
            chunk = (u8*)(node->dataOffset + offset + (u32)stream->buffer);
            tag = ReadF32LE(chunk);
            chunkSize = ReadF32LE(chunk + 4);
            if (tag == 0x5453494c) {
                offset += 0xc;
            } else {
                offset += chunkSize + 8;
                switch (tag) {
                case 0x62773130:
                    node->audioData = chunk + 8;
                    node->audioSize = chunkSize;
                    node->audioByteOffset = stream->audioBytesProduced;
                    stream->audioBytesProduced += chunkSize;
                    fn_800D9B48(&stream->audio, chunk + 8, chunkSize);
                    break;
                case 0x4b4e554a:
                    node->junkSize = chunkSize;
                    break;
                case 0x62643030:
                case 0x63643030:
                    node->videoData = chunk + 8;
                    node->videoSize = chunkSize;
                    node->videoFrameIndex = stream->videoFrameCount;
                    stream->videoFrameCount++;
                    break;
                default:
                    break;
                }
                offset = (offset + 1) & 0xfffffffe;
            }
        } while (offset < (sz = node->totalSize));

        if (stream->filePos == stream->fileSize || stream->videoFrameCount == stream->videoFrameLimit) {
            goto request_more;
        }
        pos = node->dataOffset + sz;
        if (pos + 8 > stream->writePos) {
            goto request_more;
        }
        if (pos >= stream->bufferSize) {
            tag = ReadF32LE(stream->buffer);
        } else {
            tag = ReadF32LE(stream->buffer + pos);
        }
        switch (tag) {
        case 0x62643030:
        case 0x63643030:
        case 0x5453494c:
        case 0x4b4e554a:
        case 0x62773130:
            break;
        default:
            stream->videoFrameCount = stream->videoFrameLimit;
            return;
        }

        pos = node->totalSize + 4;
        if (pos + node->dataOffset >= stream->bufferSize) {
            chunkSize = ReadF32LE(stream->buffer);
        } else {
            chunkSize = ReadF32LE(stream->buffer + (node->dataOffset + node->totalSize) + 4);
        }
        node->next = stream->freeListHead;
        stream->freeListHead = stream->freeListHead->next;
        next = node->next;
        next->next = NULL;
        next->dataOffset = chunkEnd;
        chunkSize += chunkSize & 1;
        next->totalSize = chunkSize + 8;
        next->videoData = NULL;
        next->audioData = NULL;
        next->junkSize = 0;
        next->videoSize = 0;
        next->audioSize = 0;
    }

request_more:
    length = (length + 0x7ff) & 0xfffff800;
    if (stream->writePos >= stream->highWater) {
        if (stream->highWater != 0) {
            available = stream->bufferSize - stream->writePos;
        } else {
            available = (stream->bufferSize - stream->writePos) - 0x800;
        }
    } else {
        available = (stream->highWater - stream->writePos) - 0x800;
    }
    if ((s32)available < (s32)length) {
        length = available;
    }
    if ((s32)(stream->fileSize - stream->filePos) < (s32)length) {
        length = stream->fileSize - stream->filePos;
    }
    if ((s32)length < 0) {
        length = 0;
    }
    length &= 0xfffff800;
    len = length;
    if (len > 0 && stream->videoFrameCount < stream->videoFrameLimit) {
        sceRead(fd, (void*)stream->stagingBuffer, len);
        stream->pendingLength = length;
    }
}

void fn_800DB82C(MovieChunkStream* param_1, int param_2, u32 param_3) {
    int iVar2;

    param_1->fileSize = sceLseek(param_2, 0, 2);
    sceLseek(param_2, param_3, 0);
    param_1->filePos = param_3;
    param_1->pendingLength = (param_1->bufferSize - 0x2000) & 0xfffff800;
    sceRead(param_2, param_1->buffer, param_1->pendingLength);
    param_1->activeNode = param_1->freeListHead;
    param_1->freeListHead = (MovieChunkNode*)*(u32*)param_1->freeListHead;
    param_1->activeNode->next = 0;
    param_1->activeNode->dataOffset = 0;
    iVar2 = ReadF32LE(param_1->buffer + 4);
    param_1->activeNode->totalSize = iVar2 + 8;
    param_1->activeNode->totalSize += param_1->activeNode->totalSize & 1;
    param_1->activeNode->videoData = 0;
    param_1->activeNode->audioData = 0;
    param_1->activeNode->junkSize = 0;
    param_1->videoFrameCount = 0;
    param_1->_4C = 0;
}

u8 MovieDecoderInitBuffers(MovieChunkStream* param_1, u32 param_2, u32 param_3) {
    int iVar3;
    int iVar4;
    u8 unused[24];

    __dla__FPv(param_1->rawBuffer);
    param_1->rawBuffer = 0;
    param_1->buffer = 0;
    __dla__FPv(param_1->rawStaging);
    param_1->rawStaging = 0;
    param_1->stagingBuffer = 0;
    __dla__FPv(param_1->nodePoolRaw);
    param_1->bufferSize = 0;
    param_1->highWater = 0;
    param_1->writePos = 0;
    param_1->_20 = 0;
    param_1->_24 = 0;
    param_1->freeListHead = 0;
    param_1->activeNode = 0;
    param_1->nodePoolRaw = 0;
    if ((param_3 & 0xff) != 0) {
        fn_800D9C5C(&param_1->audio, 0x40000);
    }
    param_1->bufferSize = param_2 & 0xfffff800;
    iVar3 = param_1->bufferSize;
    gMovieAllocCount++;
    param_1->rawBuffer = (u8*)AllocHiMem(iVar3 + 0x20, iVar3);
    iVar3 = gMovieAllocCount;
    gMovieAllocCount++;
    param_1->rawStaging = (u8*)AllocHiMem(0x10020, iVar3);
    param_1->buffer = (u8*)((u32)param_1->rawBuffer + 0x20 & 0xffffffe0);
    param_1->stagingBuffer = (u8*)((u32)param_1->rawStaging + 0x20 & 0xffffffe0);
    iVar3 = gMovieAllocCount;
    gMovieAllocCount++;
    param_1->nodePoolRaw = (u8*)AllocHiMem(0x2800, iVar3);
    param_1->freeListHead = (MovieChunkNode*)param_1->nodePoolRaw;
    for (iVar4 = 0; iVar4 < 255; iVar4++) {
        *(u32*)(param_1->nodePoolRaw + iVar4 * 0x28) = (u32)param_1->nodePoolRaw + (iVar4 + 1) * 0x28;
    }
    *(u32*)(param_1->nodePoolRaw + iVar4 * 0x28) = 0;
    return param_1->buffer != 0;
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

/* Same C++ EH shape as __dt__15MoviePlayerBaseFv: parsed as C++ for the empty exception
 * specification, which is what emits the __unexpected edge and the r31
 * frame-pointer prologue. */
#pragma cplusplus on
MovieChunkStream* dtor_800DBB94(MovieChunkStream* self, s16 deleting) throw() {
    if (self != NULL) {
        __dla__FPv(self->rawBuffer);
        self->rawBuffer = 0;
        self->buffer = 0;
        __dla__FPv(self->rawStaging);
        self->rawStaging = 0;
        self->stagingBuffer = 0;
        __dla__FPv(self->nodePoolRaw);
        self->bufferSize = 0;
        self->highWater = 0;
        self->writePos = 0;
        self->_20 = 0;
        self->_24 = 0;
        self->freeListHead = 0;
        self->activeNode = 0;
        self->nodePoolRaw = 0;
        fn_800D9CF4((int*)&self->audio, -1);
        if (deleting > 0 && self != NULL) {
            gMovieAllocCount--;
            if (gMovieAllocCount == 0) {
                ResetAllocTot();
            }
        }
    }
    return self;
}
#pragma cplusplus off

MovieChunkStream* fn_800DBC64(register MovieChunkStream* p) {
    register MovieChunkStream* self = p;

    fn_800D9DA4(&self->audio);
    self->rawStaging = 0;
    self->stagingBuffer = 0;
    self->rawBuffer = 0;
    self->buffer = 0;
    self->bufferSize = 0;
    self->highWater = 0;
    self->writePos = 0;
    self->_20 = 0;
    self->_24 = 0;
    self->freeListHead = 0;
    self->nodePoolRaw = 0;
    self->activeNode = 0;
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

/* VQCodec::~VQCodec (vtable __vt__7VQCodec).  Written as a real C++ destructor,
 * NON-virtual and with NO exception specification, per the measured recipe:
 * CodeWarrior gives every destructor the hidden `short` in-charge parameter and
 * synthesises both the leading `this == NULL` test and the trailing
 * `if (in_charge > 0) operator delete(this)` branch, so neither is written
 * here -- writing either yields it twice.  The non-virtual spelling reproduces
 * the target's stream exactly while NOT emitting a __vt__ object into this TU,
 * which matters because movieplayer's split owns only .text/extab/extabindex:
 * every vtable here lives in unsplit .data and is linked from original bytes.
 *
 * The base `Codec::~Codec(this, 0)` call is synthesised too -- that is the
 * target's `addi r3,r29,0 / li r4,0 / bl __dt__5CodecFv`.  Codec::~Codec is
 * defined BELOW this function, which is what keeps it out of line; MWCC will
 * inline a base destructor defined earlier and collapse the call away. */
#pragma cplusplus on
VQCodec::~VQCodec() {
    vtable = __vt__7VQCodec;
    delete alloc2;
    lbl_803452B8--;
}

/* Construct an outer DText object (base init + lbl_80321340 ramp, first time only). */
VQCodec* fn_800DBE04(u32* p) {
    int i;
    DTextInitColorRamp((Codec*)p);
    p[8] = (u32)__vt__7VQCodec;
    if (lbl_803452B8 == 0) {
        for (i = 0; i < 256; i++) {
            lbl_80321340[i] = (u8)((i * 31 + 128) / 255);
        }
    }
    lbl_803452B8++;
    p[12] = 0;
    return (VQCodec*)p;
}
#pragma cplusplus off

#ifdef __MWERKS__
#pragma optimization_level 4
#pragma peephole on
#pragma scheduling on
#endif

/* DText glyph blit (uses gDTextBuf + movie sdata2 pool) */
void fn_800DBE98(void* param_1, u8* param_2) {
    u8 unused[16];
    f32 fVar1;
    f32 fVar2;
    f32 fVar3;
    f32 fVar4;

    fVar4 = lbl_803493D8;
    fVar1 = (f32)(u32)param_2[0];
    fVar3 = (f32)(u32)param_2[1] - fVar4;
    fVar2 = (f32)(u32)param_2[2] - fVar4;
    param_2[2] = gDTextBuf[(int)(lbl_803493DC + (lbl_803493E0 * fVar2 + fVar1))];
    param_2[1] = gDTextBuf[(int)(lbl_803493DC + (fVar1 - lbl_803493E4 * fVar3 - lbl_803493E8 * fVar2))];
    param_2[0] = gDTextBuf[(int)(lbl_803493DC + (lbl_803493EC * fVar3 + fVar1))];
}

/* Codec::~Codec (vtable __vt__5Codec, dtor slot 0x801296F8).  Same recipe as
 * VQCodec::~VQCodec above: non-virtual, no exception specification, and neither
 * the leading null test nor the trailing deleting branch written by hand.
 *
 * Both `delete` expressions in this pair inline the TU's global
 * `operator delete` -- which is defined earlier in the file WITH `throw()` --
 * and each inlined copy contributes exactly one `__unexpected` island.  That is
 * the arithmetic behind this function's two islands (at r31+44 for the member
 * release, r31+16 for the synthesised deleting branch) and its frame of 88. */
#pragma cplusplus on
Codec::~Codec() {
    vtable = __vt__5Codec;
    gDTextInitCount--;
    delete alloc;
}
#pragma cplusplus off

/* 0x800DC034 init the DText debug-overlay 256-entry colour ramp (gDTextColorRamp/gDTextBuf) */
typedef struct DTextRampEntry {
    u8 _pad[768];
    u8 value;
} DTextRampEntry;

#pragma cplusplus on
Codec* DTextInitColorRamp(Codec* p) {
    int i;
    u8* ramp = gDTextColorRamp;
    p->vtable = __vt__5Codec;
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
    p->alloc = NULL;
    return p;
}
#pragma cplusplus off

} /* extern "C" */
