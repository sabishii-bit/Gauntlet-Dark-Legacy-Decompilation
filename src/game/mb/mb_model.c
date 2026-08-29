#include "types.h"

/* Midway "MB" model-buffer library model loader (GCN MB_MODEL.CPP TU,
 * .text 0x800B7758-0x800B90A4). This is the objects.ngc / textures.ngc
 * resource loader for the MB (model-buffer) graphics system: it reads the
 * per-world geometry ("objects") and texture ("textures") archives off the
 * disc, allocates model memory from the MLM (ml_mem) arena, sets up the
 * loaded model data (version-checked ngc blocks), and maintains the
 * object-definition and texture-definition lookup tables (binary searched
 * by name / index).
 *
 * TU identity: PDB module MB_MODEL.OBJ, plus the rodata watermark strings
 * "objects.ngc"/"textures.ngc" (0x80115DA8/0x80115DB4) and the assert text
 * lbl_80115F24 (0x80115F24). Real names taken from the
 * Xbox PDB (MBOX_* prefix). GCN .text order is the reverse of the Xbox
 * source order; seven small Xbox helpers/getters (strCopyFix,
 * MBOX_GetObject{Def,Name,Radius}, MBLoadTexturesWait, MBOX_GetTextureName,
 * MBOX_FindObject) are inlined away on GCN, leaving 24 functions.
 *
 * cflags_demo (-O4 no-peephole, -Cpp_exceptions on, -str reuse,readonly):
 * dtk attributes 20 extabindex entries here (one per LR-saving function; the
 * 4 leaf functions BGLoadObjects/BGLoadTextures/MBBackgroundLoading/texidxcmp
 * have none), confirming exceptions-on.
 *
 * Status: NonMatching, 15 of 24 functions byte-exact (BGLoadObjects,
 * BGLoadTextures, MBBackgroundLoading, MBOX_LoadModel, MBOX_AllocModel,
 * MBOX_LockModels, MBOX_ResetModels, MBOX_FindTexture, MBOX_FindTexture_Err,
 * MBOX_NewObject, MBOX_SetObject, MBOX_FindObject, objcmp, texcmp,
 * texidxcmp). Parked (bodies are behavioural skeletons pending full
 * model/texture-def struct + ngc-format reconstruction): the giant
 * SetupModel (0xAB8, inlined ngc parser), the two background/synchronous
 * file loaders (MBOX_BGLoadModelDone/Start, MBOX_LoadModelFixed), the MLM
 * allocator (MBOX_AllocModelMem), the table walkers (MBOX_FindTexture_Sub,
 * MBOX_GetTexDef, MBOX_ReallyFindObject) and MBOX_ResetUnlockedModels.
 */

/* ---- externs: sibling-TU functions (resolved via symbols.txt) ---- */
extern void* StartFileRead(const char* dir, const char* name, int flags,
                           int size, void* destination,
                           void (*callback)(void*));
extern int   FileSize(const char* dir, const char* name);
extern int   MLMReadFile(const char* dir, const char* name, void* buf, int size);
extern void* AllocMem(int size);
extern void* GetMemBase(void);
extern int   BytesFree(void);
extern void  LockMem(void);
extern void  FreeUnlockedMem(int slot);
extern void  FatalError(const char* fmt, int code);
extern void  ErrorPrintf(const char* fmt, ...);
extern void  bulletproof_printf(const char* fmt, ...);
extern int   MBNewObject(int idx, int a, int b, int c);
extern int   MBSetObject(void* def, int idx);
extern void  MBInitPsys(void);
extern u32   pbGetCPUTime(void);
extern void  pbSetTime(int t);

extern int   fn_800C7214(int slot);         /* MB file/texture post-load */
extern void  fn_800C7884(int a);            /* MB texture-region reset */
extern void  MBLockFonts(int slot);         /* MB lock helper */
extern void  MBResetUnlockedFonts(int level); /* MB unlock helper */
extern void  MBResetFonts(void);              /* MB reset helper */
extern void  MBTreeInit(void);             /* mb_objects reset */
extern void  fn_800B9E4C(void);             /* mb_objects init */
extern void  FatalErrorf(const char* fmt, ...);

extern int   strncmp(const char*, const char*, u32);
extern int   strcmp(const char*, const char*);
extern char* strncpy(char*, const char*, u32);
extern char* strcpy(char*, const char*);
extern int   toupper(int);
extern void* bsearch(const void*, const void*, u32, u32,
                     int (*)(const void*, const void*));

/* ---- MB_MODEL globals (resolved via symbols.txt / auto data objects) ---- */
extern u8*  gWinGlobals;         /* 0x80344FC0 : MB/window manager context */
extern s32  lbl_80344E88;        /* background-load state / index (-1 = idle) */
extern s32  lbl_80344E8C;        /* current model/object-def count */
extern s32  gClockFrameNumber;   /* bulletproof_printf channel arg */
extern s32  mlmMemUsed;          /* 0x80344F44 : MLM bytes used by models */

/*
 * The background loader addresses this as one 0x4C4-byte object.  The two
 * following symbols are interior linker labels at +0x1C and +0x2C.
 */
extern u8   lbl_802A5CF0[0x4C4];
extern s32  lbl_802A5D0C[4];
extern u8   lbl_802A5D1C[0x49C];

extern const char lbl_80115DA8[]; /* "objects.ngc" */
extern const char lbl_80115DB4[]; /* "textures.ngc" (+ pooled MBOX-loading log strings) */
extern const char lbl_80115F24[]; /* pooled model-error strings ("bad version", "> max models") */
extern const char lbl_80348C28[7]; /* "static" */
extern u8 lbl_80129740[];        /* static fallback model buffer */
extern const char lbl_80348C30[4]; /* "???" */

/* forward declarations (GCN emit order = reverse Xbox source order) */
static void  BGLoadTextures(void* rq);
static void  BGLoadObjects(void* rq);
int          MBOX_LoadModelFixed(const char* dir, void* buf, int a, int b, int slot);
static void  SetupModel();
int          MBOX_AllocModelMem(int objSize, int texSize, const char* dir);
int          MBOX_FindTexture_Sub(const char* name, int p2, int lo, int hi, int flag);
static int   texcmp(const void* a, const void* b);
static int   texidxcmp(const void* a, const void* b);
int          MBOX_ReallyFindObject(const char* name, int a, int b, int create);
static int   objcmp(const void* a, const void* b);

typedef struct MboxModelLoadSlot {
    u8* model;
    s32 objectSize;
    s32 textureSize;
    s32 state;
} MboxModelLoadSlot;

typedef struct MboxBackgroundLoad {
    u8* objectRead;
    u8* textureRead;
    s32 unused;
    s32 state;
    s32 done;
    u32 time;
} MboxBackgroundLoad;

typedef struct MboxBackgroundState {
    u8 unknown[28];
    s32 modelCount[4];
    char names[21][32];
    MboxBackgroundLoad loads[21];
} MboxBackgroundState;

/* ---- 0x800B7758 : finish a pending background model load ---- */
int MBOX_BGLoadModelDone(void) {
    char* strs = (char*)lbl_80115DA8;
    MboxBackgroundState* background;
    s32 index;
    u8* models;
    u8* indexed;
    MboxBackgroundLoad* load;
    MboxModelLoadSlot* model;
    s32 slot;
    u8* h;

    background = (MboxBackgroundState*)lbl_802A5CF0;
    index = lbl_80344E88;
    models = gWinGlobals;
    if (index < 0) {
        return 1;
    }
    indexed = (u8*)background + index * sizeof(MboxBackgroundLoad);
    models = *(u8**)(models + 48);
    model = (MboxModelLoadSlot*)(models + (index << 4) + 4);
    load = (MboxBackgroundLoad*)(indexed + 716);
    slot = index;
    switch (((MboxBackgroundLoad*)(indexed + 716))->state) {
    case 0: {
        char* objectName;
        s32 objectSize;
        u8* objectBuffer;

        objectBuffer = model->model;
        indexed = models;
        indexed += slot << 4;
        objectSize = *(s32*)(indexed + 8);
        objectName = (char*)background + (slot << 5);
        model->state = 2;
        load->objectRead = StartFileRead(objectName += 44, strs, 0, objectSize,
                                         objectBuffer, BGLoadObjects);
        load->state = 1;
        bulletproof_printf(strs + 28, slot, objectName, gClockFrameNumber);
        break;
    }
    case 1: {
        char* setupName;

        h = load->objectRead + 16;
        if (*(s32*)h == 0) {
            break;
        }
        *(s32*)h = -1;
        setupName = (char*)background + (slot << 5);
        setupName += 44;
        SetupModel(slot, setupName);
        load->objectRead = 0;
        load->state = 2;
        load->time = pbGetCPUTime();
        if (model->state >= 9) {
            return 1;
        }
        model->state = 4;
        break;
    }
    case 2: {
        char* textureName;
        s32 textureSize;
        s32 objectSize;
        u8* objectBuffer;

        textureSize = model->textureSize;
        objectBuffer = model->model;
        objectSize = model->objectSize;
        textureName = (char*)background + (slot << 5);
        model->state = 3;
        load->textureRead = StartFileRead(
            textureName += 44, strs + 12, 0, textureSize,
            objectBuffer + objectSize, BGLoadTextures);
        load->state = 3;
        bulletproof_printf(strs + 72, slot, textureName, gClockFrameNumber);
        break;
    }
    case 3:
        h = load->textureRead + 16;
        if (*(s32*)h == 0) {
            break;
        }
        *(s32*)h = -1;
        load->textureRead = 0;
        load->state = 4;
        load->time = pbGetCPUTime();
        fn_800C7214(slot);
        if (model->state >= 9) {
            return 1;
        }
        model->state = 0;
        break;
    case 4: {
        char* doneName;

        doneName = (char*)background + (slot << 5);
        doneName += 44;
        bulletproof_printf(strs + 120, slot, doneName,
                           gClockFrameNumber);
        lbl_80344E88 = -1;
        load->done = 1;
        return 1;
    }
    }
    return 0;
}

/* ---- 0x800B79AC : begin the next background model load ---- */
int MBOX_BGLoadModelStart(const char* dir, int slot) {
    s32 objSize;
    char* strs = (char*)lbl_80115DA8;
    u8* tbl;
    u8* g;
    u8* row;
    u8* ent;
    s32 texSize;
    s32 zero;

    tbl = lbl_802A5CF0;
    g = gWinGlobals;
    pbSetTime(0);
    if (lbl_80344E88 >= 0) {
        FatalError(strs + 160, 0x800000);
    }
    if (slot < 0) {
        objSize = FileSize(dir, strs);
        texSize = FileSize(dir, strs + 12);
        slot = MBOX_AllocModelMem(objSize, texSize, dir);
    }
    row = *(u8**)(g + 48) + (slot << 4) + 4;
    *(s32*)(row + 12) = 6;
    bulletproof_printf(strs + 200, slot, dir, *(s32*)(row + 4));
    lbl_80344E88 = slot;
    ent = tbl + slot * 24;
    zero = 0;
    *(s32*)(ent + 716) = zero;
    *(s32*)(ent + 720) = zero;
    *(s32*)(ent + 724) = zero;
    *(s32*)(ent + 728) = zero;
    *(s32*)(ent + 732) = zero;
    *(s32*)(ent + 736) = pbGetCPUTime();
    ent = tbl + slot * 32;
    strncpy((char*)(ent + 44), dir, 32);
    *(u8*)(tbl + slot * 32 + 75) = zero;
    lbl_80344E88 = slot;
    return slot;
}

/* ---- 0x800B7AC8 / 0x800B7AE8 : per-chunk read-advance callbacks ---- */
static void BGLoadTextures(void* rq) {
    s32* r = (s32*)rq;
    if (r[4] == 1) {
        return;
    }
    r[1] = r[1] + r[2];
}

static void BGLoadObjects(void* rq) {
    s32* r = (s32*)rq;
    if (r[4] == 1) {
        return;
    }
    r[1] = r[1] + r[2];
}

/* ---- 0x800B7B08 : is a background model load in progress? ---- */
int MBBackgroundLoading(void) {
    s32 n = lbl_80344E88;
    if (n >= 0) {
        return n + 1;
    }
    return 0;
}

/* ---- 0x800B7B24 : load a single model (default flags) ---- */
int MBOX_LoadModel(const char* dir) {
    return MBOX_LoadModelFixed(dir, NULL, 0, 0, -1);
}

/* ---- 0x800B7B54 : load a single model file into a fixed slot ---- */
int MBOX_LoadModelFixed(const char* dir, void* buf, int a, int b, int slot) {
    char* strs;
    u8* g;
    u8* row;
    u8* base;
    s32 objSize;
    s32 off;
    s32 got;
    u8* nameBase;
    s32 nameOff;
    u8 unused[8];

    strs = (char*)lbl_80115DA8;
    g = gWinGlobals;
    if (slot < 0) {
        objSize = FileSize(dir, strs);
        slot = MBOX_AllocModelMem(objSize, FileSize(dir, strs + 12), dir);
    }
    off = slot << 4;
    row = *(u8**)(g + 48) + off + 4;
    base = *(u8**)row;
    *(s32*)(row + 12) = 2;
    nameBase = lbl_802A5D1C;
    nameOff = slot << 5;
    strncpy((char*)(nameBase + nameOff), dir, 32);
    *(u8*)(nameBase + nameOff + 31) = 0;
    if (strcmp(dir, lbl_80348C28) != 0) {
        got = MLMReadFile(dir, strs, buf, (int)base);
        bulletproof_printf(strs + 244, slot, dir, *(s32*)(row + 4));
        if ((u32)got > (u32)*(s32*)(row + 4)) {
            ErrorPrintf(strs + 288, slot, dir, got);
        }
    }
    strncpy((char*)(base + 32), dir, 32);
    *(u8*)(base + 63) = 0;
    SetupModel(slot, dir);
    if (*(s32*)(row + 12) != 2) {
        return slot;
    }
    *(s32*)(row + 12) = 1;
    if (strcmp(dir, lbl_80348C28) == 0) {
        fn_800C7214(slot);
    } else {
        u8* row2 = *(u8**)(gWinGlobals + 48) + off + 4;
        got = MLMReadFile(dir, strs + 12, (void*)*(s32*)(row2 + 8),
                          *(s32*)(*(u8**)row2 + 112));
        if ((u32)((got + 15) & ~15) > (u32)*(s32*)(row2 + 8)) {
            ErrorPrintf(strs + 336, dir);
        }
        fn_800C7214(slot);
    }
    if (*(s32*)(row + 12) != 1) {
        return slot;
    }
    *(s32*)(row + 12) = 0;
    return slot;
}

static inline u16 ModelSwap16(u16 value)
{
    u8* bytes;

    bytes = (u8*)&value;
    return (u16)(bytes[0] | (bytes[1] << 8));
}

static inline u32 ModelSwap32(u32 value)
{
    u32 result;
    u8* source;
    u8* destination;

    source = (u8*)&value;
    destination = (u8*)&result;
    destination[0] = source[3];
    destination[1] = source[2];
    destination[2] = source[1];
    destination[3] = source[0];
    return result;
}

static inline f32 ModelSwapF32(f32 value)
{
    u32 result;

    result = ModelSwap32(*(u32*)&value);
    return *(f32*)&result;
}

#define MODEL_U16(data, offset) (*(u16*)((data) + (offset)))
#define MODEL_S16(data, offset) (*(s16*)((data) + (offset)))
#define MODEL_U32(data, offset) (*(u32*)((data) + (offset)))
#define MODEL_S32(data, offset) (*(s32*)((data) + (offset)))
#define MODEL_F32(data, offset) (*(f32*)((data) + (offset)))

/* ---- 0x800B7D44 : parse/verify a loaded ngc model block (giant) ---- */
static void SetupModel(slot, name)
int slot;
const char* name;
{
    u8* globals;
    u8* model;
    u8* records;
    u8* record;
    u8* packed;
    u8* elements;
    u8* objectRecord;
    u32 version;
    u8 swapData;
    u32 i;
    s32 j;
    s32 trimIndex;
    s32 offset;
    s32 objectOffset;
    s32 packedOffset;
    s32 elementOffset;
    s32 count;
    s32 recordIndex;

    globals = gWinGlobals;
    model = *(u8**)(*(u8**)(globals + 48) + (slot << 4) + 4);
    swapData = 1;

    MODEL_U32(model, 64) = ModelSwap32(MODEL_U32(model, 64));
    if (MODEL_U32(model, 64) != 0xF00B000D) {
        swapData = 0;
        MODEL_U32(model, 64) = ModelSwap32(MODEL_U32(model, 64));
    }

    if (swapData) {
        MODEL_U32(model, 68) = ModelSwap32(MODEL_U32(model, 68));
        MODEL_U32(model, 72) = ModelSwap32(MODEL_U32(model, 72));
        MODEL_U32(model, 76) = ModelSwap32(MODEL_U32(model, 76));
        MODEL_U32(model, 80) = ModelSwap32(MODEL_U32(model, 80));
        MODEL_U32(model, 84) = ModelSwap32(MODEL_U32(model, 84));
        MODEL_U32(model, 88) = ModelSwap32(MODEL_U32(model, 88));
        MODEL_U32(model, 92) = ModelSwap32(MODEL_U32(model, 92));
        MODEL_U32(model, 96) = ModelSwap32(MODEL_U32(model, 96));
        MODEL_U32(model, 100) = ModelSwap32(MODEL_U32(model, 100));
        MODEL_U32(model, 104) = ModelSwap32(MODEL_U32(model, 104));
        MODEL_U32(model, 108) = ModelSwap32(MODEL_U32(model, 108));
        MODEL_U32(model, 112) = ModelSwap32(MODEL_U32(model, 112));
        MODEL_U32(model, 116) = ModelSwap32(MODEL_U32(model, 116));
        MODEL_U32(model, 120) = ModelSwap32(MODEL_U32(model, 120));
        MODEL_U16(model, 124) = ModelSwap16(MODEL_U16(model, 124));
        MODEL_U16(model, 126) = ModelSwap16(MODEL_U16(model, 126));
    }

    version = MODEL_U32(model, 64);
    if (version != 0xF00B000D) {
        ErrorPrintf(lbl_80115F24, slot, model + 32, version, 0xF00B000D);
    }
    strncpy((char*)(model + 32), name, 32);
    model[63] = 0;

    MODEL_U32(model, 84) = (u32)model + MODEL_U32(model, 84);
    MODEL_U32(model, 88) = (u32)model + MODEL_U32(model, 88);
    MODEL_U32(model, 92) = (u32)model + MODEL_U32(model, 92);
    MODEL_U32(model, 96) = (u32)model + MODEL_U32(model, 96);
    MODEL_U32(model, 100) = (u32)model + MODEL_U32(model, 100);
    MODEL_U32(model, 104) = (u32)model + MODEL_U32(model, 104);
    MODEL_U32(model, 120) = (u32)model + MODEL_U32(model, 120);
    MODEL_U32(model, 108) = (u32)model + MODEL_U32(model, 108);
    MODEL_U32(model, 112) = (u32)model +
                            MODEL_U32(*(u8**)(globals + 48) + (slot << 4), 8);
    MODEL_U32(model, 116) = (u32)model +
                            MODEL_U32(*(u8**)(globals + 48) + (slot << 4), 8) +
                            MODEL_U32(*(u8**)(globals + 48) + (slot << 4), 12);

    records = (u8*)MODEL_U32(model, 88);
    if (swapData) {
        objectOffset = 0;
        for (i = 0; i < MODEL_U32(model, 68); i++, objectOffset += 64) {
            objectRecord = (u8*)MODEL_U32(model, 84) + objectOffset;
            MODEL_F32(objectRecord, 0) = ModelSwapF32(MODEL_F32(objectRecord, 0));
            MODEL_F32(objectRecord, 4) = ModelSwapF32(MODEL_F32(objectRecord, 4));
            MODEL_U32(objectRecord, 8) = ModelSwap32(MODEL_U32(objectRecord, 8));
            MODEL_U32(objectRecord, 12) = ModelSwap32(MODEL_U32(objectRecord, 12));
            MODEL_U16(objectRecord, 16) = ModelSwap16(MODEL_U16(objectRecord, 16));
            MODEL_U16(objectRecord, 18) = ModelSwap16(MODEL_U16(objectRecord, 18));
            MODEL_U16(objectRecord, 20) = ModelSwap16(MODEL_U16(objectRecord, 20));
            MODEL_U16(objectRecord, 22) = ModelSwap16(MODEL_U16(objectRecord, 22));
            MODEL_U32(objectRecord, 24) = ModelSwap32(MODEL_U32(objectRecord, 24));
            MODEL_U32(objectRecord, 28) = ModelSwap32(MODEL_U32(objectRecord, 28));
            MODEL_U32(objectRecord, 32) = ModelSwap32(MODEL_U32(objectRecord, 32));
            MODEL_U32(objectRecord, 36) = ModelSwap32(MODEL_U32(objectRecord, 36));
            MODEL_U32(objectRecord, 40) = ModelSwap32(MODEL_U32(objectRecord, 40));
            MODEL_U32(objectRecord, 44) = ModelSwap32(MODEL_U32(objectRecord, 44));
        }

        offset = 0;
        for (i = 0; i < MODEL_U32(model, 72); i++, offset += 64) {
            record = records + offset;
            MODEL_U16(record, 8) = ModelSwap16(MODEL_U16(record, 8));
            MODEL_U32(record, 12) = ModelSwap32(MODEL_U32(record, 12));
            MODEL_U16(record, 18) = ModelSwap16(MODEL_U16(record, 18));
            MODEL_U16(record, 22) = ModelSwap16(MODEL_U16(record, 22));
            MODEL_U16(record, 24) = ModelSwap16(MODEL_U16(record, 24));
        }

        offset = 0;
        packedOffset = 0;
        for (i = 0; i < MODEL_U32(model, 72); i++, offset += 64, packedOffset += 16) {
            record = records + offset;
            packed = records + packedOffset;
            packed[0] = record[0];
            MODEL_U16(packed, 2) = MODEL_U16(record, 8);
            MODEL_U32(packed, 4) = MODEL_U32(record, 12);
            MODEL_U16(packed, 8) = MODEL_U16(record, 18);
            MODEL_U16(packed, 10) = MODEL_U16(record, 22);
            MODEL_U16(packed, 12) = MODEL_U16(record, 24);
        }
        MODEL_U32(model, 128) = (u32)records + MODEL_U32(model, 72) * 16;

        offset = 0;
        for (i = 0; i < MODEL_U32(model, 76); i++, offset += 24) {
            record = (u8*)MODEL_U32(model, 92) + offset;
            MODEL_F32(record, 16) = ModelSwapF32(MODEL_F32(record, 16));
            MODEL_U16(record, 20) = ModelSwap16(MODEL_U16(record, 20));
            MODEL_U16(record, 22) = ModelSwap16(MODEL_U16(record, 22));
        }

        offset = 0;
        for (i = 0; i < MODEL_U32(model, 80); i++, offset += 36) {
            record = (u8*)MODEL_U32(model, 96) + offset;
            MODEL_U16(record, 30) = ModelSwap16(MODEL_U16(record, 30));
            MODEL_U16(record, 32) = ModelSwap16(MODEL_U16(record, 32));
            MODEL_U16(record, 34) = ModelSwap16(MODEL_U16(record, 34));
        }
    }

    count = (s32)MODEL_U32(model, 80);
    offset = (count - 1) * 36;
    for (trimIndex = count - 1; trimIndex >= 0; trimIndex--, offset -= 36) {
        if (*(s8*)((u8*)MODEL_U32(model, 96) + offset) != 0) {
            break;
        }
        MODEL_U32(model, 80)--;
    }

    offset = 0;
    for (i = 0; i < MODEL_U32(model, 68); i++, offset += 64) {
        record = (u8*)MODEL_U32(model, 84) + offset;
        MODEL_U32(record, 44) = 0;
        count = MODEL_S32(record, 12);
        if (count < 1 || MODEL_U16(record, 16) < 1 || MODEL_U32(record, 28) == 0 ||
            (count > 1 && MODEL_U32(record, 24) == 0)) {
            MODEL_S32(record, 12) = 0;
            continue;
        }

        MODEL_U32(record, 24) = (u32)model + MODEL_U32(record, 24);
        MODEL_U32(record, 28) = (u32)model + MODEL_U32(record, 28);
        if (swapData) {
            elementOffset = 0;
            for (j = 0; j < MODEL_S32(record, 12) - 1; j++, elementOffset += 8) {
                elements = (u8*)MODEL_U32(record, 24) + elementOffset;
                MODEL_U16(elements, 0) = ModelSwap16(MODEL_U16(elements, 0));
                MODEL_U16(elements, 2) = ModelSwap16(MODEL_U16(elements, 2));
                MODEL_U16(elements, 4) = ModelSwap16(MODEL_U16(elements, 4));
                MODEL_U16(elements, 6) = ModelSwap16(MODEL_U16(elements, 6));
            }
        }
    }

    offset = 0;
    for (i = 0; i < MODEL_U32(model, 76); i++, offset += 24) {
        record = (u8*)MODEL_U32(model, 92) + offset;
        recordIndex = MODEL_S16(record, 20);
        *(u8**)((u8*)MODEL_U32(model, 84) + recordIndex * 64 + 44) = record;
    }
}

/* ---- 0x800B87FC : compute+allocate model memory for a named model ---- */
int MBOX_AllocModel(const char* dir) {
    int objSize = FileSize(dir, lbl_80115DA8);
    int texSize = FileSize(dir, lbl_80115DB4);
    return MBOX_AllocModelMem(objSize, texSize, dir);
}

/* ---- 0x800B8854 : allocate a model-memory block from the MLM arena ---- */
typedef struct ModelRec {
    s32 unk0;      /* 0x00 */
    u8* base;      /* 0x04 model memory base */
    s32 objBytes;  /* 0x08 */
    s32 texBytes;  /* 0x0C */
    s32 locked;    /* 0x10 */
} ModelRec;

int MBOX_AllocModelMem(int objSize, int texSize, const char* dir) {
    char* strs = (char*)lbl_80115DA8;
    u8* g;
    s32 off;
    s32 memUsed;
    s32 slot;
    s32 texAlloc;
    s32* counter;
    u8* row;
    u8* aligned;
    s32 want;
    s32 pad;

    objSize = (objSize + 255) & ~255;
    texAlloc = (texSize + 15) & ~15;
    g = gWinGlobals;
    counter = *(s32**)(g + 48);
    slot = (*counter)++;
    off = slot << 4;
    lbl_80344E8C = **(s32**)(g + 48);
    row = *(u8**)(g + 48);
    row += off;
    *(s32*)(row + 16) = 8;
    row = *(u8**)(g + 48);
    row += off;
    *(s32*)(row + 4) = 0;
    row = *(u8**)(g + 48);
    row += off;
    *(s32*)(row + 8) = 0;
    row = *(u8**)(g + 48);
    row += off;
    *(s32*)(row + 12) = 0;
    if (strcmp(dir, lbl_80348C28) != 0) {
        want = objSize + texAlloc;
        texAlloc = BytesFree();
        memUsed = mlmMemUsed;
        if (slot >= 21) {
            FatalError(strs + 432, 0x800000);
        }
        if (want > texAlloc) {
            FatalError(strs + 464, 0x800000);
        }
        aligned = GetMemBase();
        pad = 256 - (s32)((u32)aligned & 0xFF);
        want += pad;
        aligned += pad;
        AllocMem(want);
        texAlloc = texAlloc - BytesFree() - objSize;
        row = *(u8**)(g + 48);
        row += off;
        *(u8**)(row + 4) = aligned;
    } else {
        row = *(u8**)(g + 48);
        row += off;
        *(u8**)(row + 4) = lbl_80129740;
    }
    row = *(u8**)(g + 48);
    row += off;
    *(s32*)(row + 8) = objSize;
    row = *(u8**)(g + 48);
    row += off;
    *(s32*)(row + 12) = texAlloc;
    bulletproof_printf(strs + 508, slot,
                       (dir != NULL) ? dir : strs + 496,
                       memUsed >> 10);
    bulletproof_printf(strs + 552, mlmMemUsed >> 10, objSize >> 10,
                       texAlloc >> 10);
    return slot;
}

/* ---- 0x800B89EC : lock the current model set at a slot ---- */
void MBOX_LockModels(int slot) {
    LockMem();
    MBLockFonts(slot);
    lbl_802A5D0C[slot] = lbl_80344E8C;
}

/* ---- 0x800B8A38 : free all models above the highest lock point ---- */
void MBOX_ResetUnlockedModels(int slot) {
    u8* g = gWinGlobals;

    fn_800C7884(slot);
    FreeUnlockedMem(slot);
    MBResetUnlockedFonts(slot);
    lbl_80344E8C = lbl_802A5D0C[slot];
    (*(s32**)(g + 0x30))[0] = lbl_80344E8C;
    MBTreeInit();
    lbl_80344E88 = -1;
    if (slot == 1) {
        fn_800B9E4C();
        MBInitPsys();
    }
}

/* ---- 0x800B8AB8 : reset the whole model system ---- */
void MBOX_ResetModels(void) {
    u8* g = gWinGlobals;
    MBResetFonts();
    fn_800C7884(0);
    lbl_80344E8C = 0;
    (*(s32**)(g + 0x30))[0] = 0;
    lbl_80344E88 = -1;
}

typedef struct MboxTextureArchive {
    u8 _pad00[0x50];
    u32 textureCount;
    u8 _pad54[0x0C];
    void* textureDefs;
} MboxTextureArchive;

typedef struct MboxModelSlot {
    u8 _pad00[4];
    MboxTextureArchive* archive;
    u8 _pad08[8];
    s32 locked;
} MboxModelSlot;

/* ---- 0x800B8B04 : find a texture def by name ---- */
int MBOX_FindTexture(const char* name, void** out) {
    return MBOX_FindTexture_Sub(name, (int)out, 0, lbl_80344E8C - 1, 0);
}

/* ---- 0x800B8B34 : find a texture def by name (error variant) ---- */
int MBOX_FindTexture_Err(const char* name, void** out, int flag) {
    return MBOX_FindTexture_Sub(name, (int)out, 0, lbl_80344E8C - 1, flag);
}

/* ---- 0x800B8B64 : binary-search the texture-def table by name ---- */
int MBOX_FindTexture_Sub(const char* name, int p2, int lo, int hi, int flag) {
    char* destination;
    void** out = (void**)p2;
    u8* g;
    void* result;
    s32 model;
    char key[0x24];

    destination = key;
    result = NULL;
    g = gWinGlobals;
    while (*name == ' ' || *name == '\t' || *name == '\n') {
        name++;
    }
    while (*name != '\0' && *name != ' ' && *name != '\t' && *name != '\n') {
        *destination = (char)toupper(*name);
        name++;
        destination++;
    }
    *destination = '\0';
    if (lo < 0) {
        lo = 0;
    }
    if (hi < 0 || hi >= lbl_80344E8C) {
        hi = lbl_80344E8C - 1;
    }
    model = lo;
    lo <<= 4;
    while (model <= hi) {
        MboxModelSlot* current =
            (MboxModelSlot*)(*(u8**)(g + 0x30) + lo);

        if (current->locked == 0) {
            result = bsearch(key, current->archive->textureDefs,
                             current->archive->textureCount, 0x24, texcmp);
            if (result != NULL) {
                break;
            }
        }
        model++;
        lo += 0x10;
    }
    if (result == NULL) {
        if (flag == -1) {
            return -1;
        }
        if (out != NULL) {
            *out = NULL;
        }
        return 0;
    }
    {
        s32 textureIndex = *(s16*)((u8*)result + 0x1E);
        u32 id = (u16)textureIndex;

        id |= model << 16;
        if (out != NULL) {
            *out = result;
        }
        return id;
    }
}

/* ---- 0x800B8CE8 : texture-name comparator (30 chars) ---- */
static int texcmp(const void* a, const void* b) {
    return strncmp((const char*)a, (const char*)b, 0x1e);
}

static void setTextureKey(s16* destination, s32 value) {
    *destination = (s16)value;
}

/* ---- 0x800B8D0C : find a texture def by index ---- */
#pragma opt_propagation off
void* MBOX_GetTexDef(int idx) {
    u8 unused[8];
    struct {
        u8 _pad[30];
        s16 h;
    } key;
    u16 textureIndex = idx;
    u8* g = gWinGlobals;
    s32 bank = idx >> 16;
    void* result;

    (void)unused;
    if (bank >= lbl_80344E8C) {
        result = NULL;
        goto done;
    }
    bank <<= 4;
    if (((MboxModelSlot*)(*(u8**)(g + 0x30) + bank))->locked != 0) {
        result = NULL;
        goto done;
    }
    if (((MboxModelSlot*)(*(u8**)(g + 0x30) + bank))->archive
            ->textureCount == 0) {
        result = NULL;
        goto done;
    }
    setTextureKey(&key.h, (s16)textureIndex);
    {
        MboxModelSlot* slot;

        slot = (MboxModelSlot*)(*(u8**)(g + 0x30) + bank);
        result = bsearch(&key, slot->archive->textureDefs,
                         slot->archive->textureCount, 0x24, texidxcmp);
    }
done:
    result = result != NULL ? result : (void*)lbl_80348C30;
    return result;
}
#pragma opt_propagation reset

/* ---- 0x800B8DC0 : texture-index comparator ---- */
static int texidxcmp(const void* a, const void* b) {
    return ((const s16*)a)[0xf] - ((const s16*)b)[0xf];
}

/* ---- 0x800B8DD0 : register a new object def + create an MB object ---- */
int MBOX_NewObject(const char* name, int p2, int p3, int p4) {
    int idx = MBOX_ReallyFindObject(name, -1, -1, 1);
    return MBNewObject(idx, p2, p3, p4);
}

/* ---- 0x800B8E20 : register/replace an object def + set an MB object ---- */
int MBOX_SetObject(void* def, const char* name) {
    int idx = MBOX_ReallyFindObject(name, -1, -1, 1);
    return MBSetObject(def, idx);
}

/* ---- 0x800B8E68 : find an object def by name (no create) ---- */
int MBOX_FindObject(const char* name) {
    return MBOX_ReallyFindObject(name, -1, -1, 0);
}

/* ---- 0x800B8E94 : find-or-register an object def by name ---- */
#pragma opt_propagation off
int MBOX_ReallyFindObject(const char* name, int a, int b, int create) {
    u8 unused[8];
    char nm[16];
    const char* strings;
    u8* win;
    u8* slot;
    u8* model;
    u8* found;
    int slotIndex;
    int objectIndex;

    strings = lbl_80115DA8;
    found = NULL;
    win = gWinGlobals;
    if (a < 0) {
        a = 0;
    }
    if (b < 0 || b >= lbl_80344E8C) {
        b = lbl_80344E8C - 1;
    }

    if (name == NULL || *(const s8*)name == 0) {
        strcpy(nm, strings + 580);
    } else {
        strncpy(nm, name, 16);
    }

    slotIndex = a;
    for (; slotIndex <= b; slotIndex++) {
        slot = *(u8**)(win + 48) + slotIndex * 16;
        if (*(s32*)(slot + 16) == 0) {
            model = *(u8**)(slot + 4);
            found = bsearch(nm, *(void**)(model + 92),
                            *(u32*)(model + 76), 24, objcmp);
            if (found != NULL) {
                break;
            }
        }
    }

    if (found == NULL) {
        if (create == -1) {
            return -1;
        }

        strcpy(nm, strings + 580);
        slotIndex = 0;
        for (; slotIndex < lbl_80344E8C; slotIndex++) {
            slot = *(u8**)(win + 48) + slotIndex * 16;
            if (*(s32*)(slot + 16) == 0) {
                model = *(u8**)(slot + 4);
                found = bsearch(nm, *(void**)(model + 92),
                                *(u32*)(model + 76), 24, objcmp);
                if (found != NULL) {
                    break;
                }
            }
        }
    }

    if (found == NULL) {
        FatalErrorf(strings + 592, name, nm);
    }

    slot = *(u8**)(win + 48) + slotIndex * 16;
    model = *(u8**)(slot + 4);
    if (*(u32*)(model + 76) == 0) {
        objectIndex = (found - *(u8**)(model + 92)) / 24;
    } else {
        objectIndex = *(s16*)(found + 20);
    }
    return (u16)objectIndex | (slotIndex << 16);
}
#pragma opt_propagation reset

/* ---- 0x800B9068 : object-name comparator (15 chars, null-safe) ---- */
static int objcmp(const void* a, const void* b) {
    if (a == 0 || b == 0) {
        return -1;
    }
    return strncmp((const char*)a, (const char*)b, 0xf);
}
