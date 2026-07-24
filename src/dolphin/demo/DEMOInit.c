#include "types.h"
#include "dolphin/dvd.h"
#include "dolphin/gx.h"
#include "dolphin/vi.h"

/* Dolphin demo-library DEMOInit.c, modified by the game: single framebuffer,
 * G3D pad init instead of DEMOPad, trimmed viewport/scissor (8 rows top and
 * bottom), and the created heap kept in DemoHeap. The Bypass workaround
 * functions are never called and get dead-stripped at link, but they stay in
 * the source so the string/float constant pools match. */

typedef s32 OSHeapHandle;
extern volatile OSHeapHandle __OSCurrHeap;

void OSInit(void);
void OSPanic(char* file, int line, char* msg, ...);
void OSReport(char* msg, ...);
void* OSGetArenaHi(void);
void* OSGetArenaLo(void);
void OSSetArenaHi(void* newHi);
void OSSetArenaLo(void* newLo);
void* OSInitAlloc(void* arenaStart, void* arenaEnd, int maxHeaps);
OSHeapHandle OSCreateHeap(void* start, void* end);
OSHeapHandle OSSetCurrentHeap(OSHeapHandle heap);
void* OSAllocFromHeap(OSHeapHandle heap, u32 size);
void OSFreeToHeap(OSHeapHandle heap, void* ptr);
void OSAllocFixed(void** rstart, void** rend);
long OSDumpHeap(OSHeapHandle heap);
u32 OSGetConsoleType(void);
u32 OSGetPhysicalMemSize(void);
u32 OSGetConsoleSimulatedMemSize(void);

#define OSPhysicalToCached(paddr) ((void*) ((u32) (paddr) + 0x80000000))

void DVDInit(void);
void G3DInitControlPads(void);

void DEMOInit(GXRenderModeObj* mode);
void DEMOReInit(GXRenderModeObj* mode);
void DEMODoneRender(void);
void DEMOSwapBuffers(void);
void DEMOEnableBypassWorkaround(u32 timeoutFrames);

OSHeapHandle DemoHeap = 0;
static void* DefaultFifo;
static GXFifoObj* DefaultFifoObj;
static GXRenderModeObj* rmode;

void* DemoFrameBuffer1;
void* DemoFrameBuffer2;
void* DemoCurrentBuffer;

static GXRenderModeObj rmodeobj;

static u8 DemoFirstFrame = 1;

static int BypassWorkaround;
static u32 FrameCount;
static u32 FrameMissThreshold;

static void LoadMemInfo(void);
static void __DEMOInitRenderMode(GXRenderModeObj* mode);
static void __DEMOInitMem(void);
static void __DEMOInitGX(void);
static void __DEMOInitVI(void);
static void __BypassRetraceCallback();
static void __BypassDoneRender(void);

static void LoadMemInfo(void)
{
    void* arenaHiOld;
    void* simMemEnd;
    DVDFileInfo fileInfo;
    struct MemEntry {
        void* start;
        void* end;
    }* memEntry;
    u32 transferLength;
    void* arenaLo;
    void* arenaHi;
    void* heapLo;
    u32 length;
    s32 offset;
    u32 i;
    u32 indexMax;
    char* buf[63];

    /* The pointer temporaries below serve several roles (the second heap
     * block swaps arenaLo/arenaHi, and the entry loop reuses both); this
     * exact variable assignment is what the original register allocation
     * requires. */
    OSReport("\nNow, try to find memory info file...\n\n");
    if (!DVDOpen("/meminfo.bin", &fileInfo)) {
        OSReport("\nCan't find memory info file. Use /XXX toolname/ to maximize available\n");
        OSReport("memory space. For now, we only use the first %dMB.\n",
                 OSGetConsoleSimulatedMemSize() >> 0x14);
        arenaLo = OSGetArenaLo();
        arenaHi = OSGetArenaHi();
        heapLo = OSInitAlloc(arenaLo, arenaHi, 1);
        OSSetArenaLo(heapLo);
        heapLo = (void*) (((u32) heapLo + 0x1F) & 0xFFFFFFE0);
        arenaHi = (void*) ((u32) arenaHi & 0xFFFFFFE0);
        DemoHeap = OSCreateHeap(heapLo, arenaHi);
        OSSetCurrentHeap(DemoHeap);
        OSSetArenaLo((heapLo = arenaHi));
        return;
    }
    memEntry = (void*) (((u32) buf + 0x1F) & 0xFFFFFFE0);
    arenaHiOld = OSGetArenaHi();
    simMemEnd = OSPhysicalToCached(OSGetConsoleSimulatedMemSize());
    OSSetArenaHi(OSPhysicalToCached(OSGetPhysicalMemSize()));
    arenaHi = OSGetArenaLo();
    arenaLo = OSGetArenaHi();
    arenaHi = OSInitAlloc(arenaHi, arenaLo, 1);
    OSSetArenaLo(arenaHi);
    arenaHi = (void*) (((u32) arenaHi + 0x1F) & 0xFFFFFFE0);
    arenaLo = (void*) ((u32) arenaLo & 0xFFFFFFE0);
    DemoHeap = OSCreateHeap(arenaHi, arenaLo);
    OSSetCurrentHeap(DemoHeap);
    OSSetArenaLo((arenaHi = arenaLo));
    OSAllocFixed(&arenaHiOld, &simMemEnd);
    length = fileInfo.length;
    offset = 0;
    while (length) {
        OSReport("loop\n");
        transferLength = (length < 0x20) ? length : 0x20;
        if (DVDReadPrio(&fileInfo, memEntry, (transferLength + 0x1F) & 0xFFFFFFE0, offset, 2) < 0) {
            OSPanic("DEMOInit.c", 0x499, "An error occurred when issuing read to /meminfo.bin\n");
        }
        indexMax = transferLength / 8;
        for (i = 0; i < indexMax; i++) {
            arenaLo = &memEntry[i];
            OSReport("start: 0x%08x, end: 0x%08x\n", ((struct MemEntry*) arenaLo)->start,
                     ((struct MemEntry*) arenaLo)->end);
            arenaHi = &((struct MemEntry*) arenaLo)->end;
            OSAllocFixed(&((struct MemEntry*) arenaLo)->start, (void**) arenaHi);
            OSReport("Removed 0x%08x - 0x%08x from the current heap\n",
                     ((struct MemEntry*) arenaLo)->start, (char*) *(void**) arenaHi - 1);
        }
        length -= transferLength;
        offset += transferLength;
    }
    DVDClose(&fileInfo);
    OSDumpHeap(__OSCurrHeap);
}

void DEMODoneRender(void)
{
    GXSetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
    GXSetColorUpdate(GX_TRUE);
    GXFlush();
    if (DemoFirstFrame) {
        VISetBlack(FALSE);
        VIFlush();
        DemoFirstFrame = 0;
    }
}

void DEMOSwapBuffers(void)
{
    GXDrawDone();
    VIWaitForRetrace();
    GXCopyDisp(DemoCurrentBuffer, GX_TRUE);
    GXDrawDone();
    GXInvalidateVtxCache();
}

static void __DEMOInitMem(void)
{
    void* arenaLo = OSGetArenaLo();
    void* arenaHi = OSGetArenaHi();
    u32 fbSize = ((u16) (rmode->fbWidth + 15) & 0xFFF0) * rmode->xfbHeight * 2;

    DemoFrameBuffer1 = (void*) (((u32) arenaLo + 0x1F) & 0xFFFFFFE0);
    DemoFrameBuffer2 = (void*) (((u32) DemoFrameBuffer1 + 0x1F) & 0xFFFFFFE0);
    DemoCurrentBuffer = DemoFrameBuffer2;
    arenaLo = (void*) (((u32) DemoFrameBuffer2 + fbSize + 0x1F) & 0xFFFFFFE0);
    OSSetArenaLo(arenaLo);
    if ((OSGetConsoleType() == 0x10000004) && (OSGetPhysicalMemSize() != 0x400000) &&
        (OSGetConsoleSimulatedMemSize() < 0x01800000)) {
        LoadMemInfo();
        return;
    }
    arenaLo = OSGetArenaLo();
    arenaHi = OSGetArenaHi();
    arenaLo = OSInitAlloc(arenaLo, arenaHi, 2);
    OSSetArenaLo(arenaLo);
    arenaLo = (void*) (((u32) arenaLo + 0x1F) & 0xFFFFFFE0);
    arenaHi = (void*) ((u32) arenaHi & 0xFFFFFFE0);
    DemoHeap = OSCreateHeap(arenaLo, arenaHi);
    OSSetCurrentHeap(DemoHeap);
    OSSetArenaLo((arenaLo = arenaHi));
}

static void __DEMOInitRenderMode(GXRenderModeObj* mode)
{
    if (mode != NULL) {
        rmode = mode;
        return;
    }
    switch (VIGetTvFormat()) {
    case VI_NTSC:
        rmode = &GXNtsc480IntDf;
        break;
    case VI_PAL:
        rmode = &GXPal528IntDf;
        break;
    case VI_MPAL:
        rmode = &GXMpal480IntDf;
        break;
    default:
        OSPanic("DEMOInit.c", 0x1B0, "DEMOInit: invalid TV format\n");
        break;
    }
    GXAdjustForOverscan(rmode, &rmodeobj, 0, 0x10);
    rmode = &rmodeobj;
}

static void __DEMOInitGX(void)
{
    GXSetViewport(0.0F, 8.0F, rmode->fbWidth, (f32) rmode->xfbHeight - 16.0F, 0.0F, 1.0F);
    GXSetScissor(0, 8, rmode->fbWidth, (u32) ((f32) rmode->efbHeight - 16.0F));
    GXSetDispCopySrc(0, 0, rmode->fbWidth, rmode->efbHeight);
    GXSetDispCopyDst(rmode->fbWidth, rmode->xfbHeight);
    GXSetDispCopyYScale((f32) rmode->xfbHeight / (f32) rmode->efbHeight);
    GXSetCopyFilter(rmode->aa, rmode->sample_pattern, GX_TRUE, rmode->vfilter);
    if (rmode->aa) {
        GXSetPixelFmt(GX_PF_RGB565_Z16, GX_ZC_LINEAR);
    } else {
        GXSetPixelFmt(GX_PF_RGB8_Z24, GX_ZC_LINEAR);
    }
    GXSetAlphaUpdate(GX_FALSE);
    GXSetDstAlpha(GX_FALSE, 0);
    GXCopyDisp(DemoCurrentBuffer, GX_TRUE);
    GXSetDispCopyGamma(GX_GM_1_0);
}

static void __DEMOInitVI(void)
{
    u32 nin;

    VISetNextFrameBuffer(DemoFrameBuffer1);
    DemoCurrentBuffer = DemoFrameBuffer2;
    VIFlush();
    VIWaitForRetrace();
    nin = rmode->viTVmode & 1;
    if (nin != 0) {
        VIWaitForRetrace();
    }
}

void DEMOInit(GXRenderModeObj* mode)
{
    OSInit();
    DVDInit();
    VIInit();
    G3DInitControlPads();
    __DEMOInitRenderMode(mode);
    __DEMOInitMem();
    VIConfigure(rmode);
    DefaultFifo = OSAllocFromHeap(__OSCurrHeap, 0x40000);
    DefaultFifoObj = GXInit(DefaultFifo, 0x40000);
    __DEMOInitGX();
    __DEMOInitVI();
}

void DEMOEnableBypassWorkaround(u32 timeoutFrames)
{
    BypassWorkaround = 1;
    FrameMissThreshold = timeoutFrames;
    VISetPreRetraceCallback((VIRetraceCallback) __BypassRetraceCallback);
}

static void __BypassRetraceCallback()
{
    FrameCount += 1;
}

static void __BypassDoneRender(void)
{
    int abort;

    abort = 0;
    GXCopyDisp(DemoCurrentBuffer, GX_TRUE);
    GXSetDrawSync(0xB00B);
    FrameCount = 0;
    while ((GXReadDrawSync() != 0xB00B) && (abort == 0)) {
        if (FrameCount >= FrameMissThreshold) {
            OSReport("---------WARNING : ABORTING FRAME----------\n");
            abort = 1;
            DEMOReInit(rmode);
        }
    }
    DEMOSwapBuffers();
}

void DEMOReInit(GXRenderModeObj* mode)
{
    GXFifoObj tmpobj;
    void* tmpFifo;
    GXFifoObj* realFifoObj;
    void* realFifoBase;
    u32 realFifoSize;

    tmpFifo = OSAllocFromHeap(__OSCurrHeap, 0x10000);
    realFifoObj = GXGetCPUFifo();
    realFifoBase = GXGetFifoBase(realFifoObj);
    realFifoSize = GXGetFifoSize(realFifoObj);
    GXAbortFrame();
    GXInitFifoBase(&tmpobj, tmpFifo, 0x10000);
    GXSetCPUFifo(&tmpobj);
    GXSetGPFifo(&tmpobj);
    __DEMOInitRenderMode(mode);
    DefaultFifoObj = GXInit(realFifoBase, realFifoSize);
    __DEMOInitGX();
    VIConfigure(rmode);
    __DEMOInitVI();
    OSFreeToHeap(__OSCurrHeap, tmpFifo);
}
