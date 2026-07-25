#include "types.h"
#include "dolphin/dvd.h"

/* Dolphin charPipeline texPalette.c, modified by the game to cache loaded
 * palettes in the file registry instead of the charPipeline fileCache. */

typedef char* Ptr;

typedef struct CLUTHeader {
    /* 0x00 */ u16 numEntries;
    /* 0x02 */ u8 unpacked;
    /* 0x03 */ u8 pad8;
    /* 0x04 */ u32 format;
    /* 0x08 */ Ptr data;
} CLUTHeader, *CLUTHeaderPtr;

typedef struct TEXHeader {
    /* 0x00 */ u16 height;
    /* 0x02 */ u16 width;
    /* 0x04 */ u32 format;
    /* 0x08 */ Ptr data;
    /* 0x0C */ u32 wrapS;
    /* 0x10 */ u32 wrapT;
    /* 0x14 */ u32 minFilter;
    /* 0x18 */ u32 magFilter;
    /* 0x1C */ f32 LODBias;
    /* 0x20 */ u8 edgeLODEnable;
    /* 0x21 */ u8 minLOD;
    /* 0x22 */ u8 maxLOD;
    /* 0x23 */ u8 unpacked;
} TEXHeader, *TEXHeaderPtr;

typedef struct TEXDescriptor {
    /* 0x00 */ TEXHeaderPtr textureHeader;
    /* 0x04 */ CLUTHeaderPtr CLUTHeader;
} TEXDescriptor, *TEXDescriptorPtr;

typedef struct TEXPalette {
    /* 0x00 */ u32 versionNumber;
    /* 0x04 */ u32 numDescriptors;
    /* 0x08 */ TEXDescriptorPtr descriptorArray;
} TEXPalette, *TEXPalettePtr;

typedef s32 OSHeapHandle;
extern volatile OSHeapHandle __OSCurrHeap;
void* OSAllocFromHeap(OSHeapHandle heap, u32 size);
void OSFreeToHeap(OSHeapHandle heap, void* ptr);
void OSPanic(char* file, int line, char* msg, ...);

#define OSRoundUp32B(x) (((u32) (x) + 31) & ~31)

typedef struct REGLIST REGLIST;
u32 regFind(REGLIST* list, char* name);
void* regAdd(REGLIST* list, char* name, u32 value, void (*callback)(void*));

extern u8 gFileCacheEnabled;
extern REGLIST gFileCacheRegistry;

static void LoadTexPalette(TEXPalettePtr* pal, char* name);
static void UnpackTexPalette(TEXPalettePtr pal);
static void TexFreeFunc(void* pal);

#define PALETTE_VERSION 0x20AF30

void TEXGetPalette(TEXPalettePtr* pal, char* name)
{
    void* p = TexFreeFunc;

    if (gFileCacheEnabled) {
        *pal = (TEXPalettePtr) regFind(&gFileCacheRegistry, name);
    }
    if (!*pal) {
        LoadTexPalette(pal, name);
        if (gFileCacheEnabled) {
            regAdd(&gFileCacheRegistry, name, (u32) *pal, p);
            regFind(&gFileCacheRegistry, name);
        }
    }
}

static void LoadTexPalette(TEXPalettePtr* pal, char* name)
{
    DVDFileInfo dfi;

    DVDOpen(name, &dfi);
    *pal = OSAllocFromHeap(__OSCurrHeap, OSRoundUp32B(dfi.length));
    DVDReadPrio(&dfi, *pal, OSRoundUp32B(dfi.length), 0, 2);
    DVDClose(&dfi);
    UnpackTexPalette(*pal);
}

static void UnpackTexPalette(TEXPalettePtr pal)
{
    u16 i;

    if (pal->versionNumber != PALETTE_VERSION) {
        OSPanic("texPalette.c", 0x53, "invalid version number for texture palette");
    }

    pal->descriptorArray = (TEXDescriptorPtr) ((Ptr) pal->descriptorArray + (u32) pal);
    for (i = 0; i < pal->numDescriptors; i++) {
        if (pal->descriptorArray[i].textureHeader) {
            pal->descriptorArray[i].textureHeader =
                (TEXHeaderPtr) ((Ptr) pal + (u32) pal->descriptorArray[i].textureHeader);
            if (!pal->descriptorArray[i].textureHeader->unpacked) {
                pal->descriptorArray[i].textureHeader->data =
                    (Ptr) pal + (u32) pal->descriptorArray[i].textureHeader->data;
                pal->descriptorArray[i].textureHeader->unpacked = TRUE;
            }
        }
        if (pal->descriptorArray[i].CLUTHeader) {
            pal->descriptorArray[i].CLUTHeader =
                (CLUTHeaderPtr) ((u8*) pal + (u32) pal->descriptorArray[i].CLUTHeader);
            if (!pal->descriptorArray[i].CLUTHeader->unpacked) {
                pal->descriptorArray[i].CLUTHeader->data =
                    (Ptr) pal + (u32) pal->descriptorArray[i].CLUTHeader->data;
                pal->descriptorArray[i].CLUTHeader->unpacked = TRUE;
            }
        }
    }
}

TEXDescriptorPtr TEXGet(TEXPalettePtr pal, u32 id)
{
    return &pal->descriptorArray[id];
}

static void TexFreeFunc(void* pal)
{
    OSFreeToHeap(__OSCurrHeap, pal);
}
