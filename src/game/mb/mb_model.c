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
extern int   StartFileRead(const char* name, void* buf, int a, int b, int c, int d);
extern int   FileSize(const char* dir, const char* name);
extern int   MLMReadFile(const char* name, void* buf, int size);
extern void* AllocMem(int size);
extern void* GetMemBase(void);
extern int   BytesFree(void);
extern void  LockMem(void);
extern void  FreeUnlockedMem(int slot);
extern void  FatalError(const char* fmt, ...);
extern void  ErrorPrintf(const char* fmt, ...);
extern void  bulletproof_printf(const char* fmt, ...);
extern int   MBNewObject(int idx, int a, int b, int c);
extern int   MBSetObject(void* def, int idx);
extern void  MBInitPsys(void);
extern u32   pbGetCPUTime(void);
extern void  pbSetTime(int t);

extern int   fn_800C7214(void* p);          /* MB file/texture post-load */
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

extern u8   lbl_802A5CF0[0x1C];  /* background-load context block */
extern s32  lbl_802A5D0C[4];     /* per-slot model-count lock points */
extern u8   lbl_802A5D1C[0x49C]; /* MBOX_LoadModelFixed scratch/header buf */

extern const char lbl_80115DA8[]; /* "objects.ngc" */
extern const char lbl_80115DB4[]; /* "textures.ngc" (+ pooled MBOX-loading log strings) */
extern const char lbl_80115F24[]; /* pooled model-error strings ("bad version", "> max models") */
extern const char lbl_80348C28[]; /* "static" */
extern const char lbl_80348C30[4]; /* "???" */

/* forward declarations (GCN emit order = reverse Xbox source order) */
static void  BGLoadTextures(void* rq);
static void  BGLoadObjects(void* rq);
int          MBOX_LoadModelFixed(const char* dir, int a, int b, int c, int type);
static int   SetupModel(void* model, const char* name, int slot);
int          MBOX_AllocModelMem(int objSize, int texSize, const char* dir);
int          MBOX_FindTexture_Sub(const char* name, int p2, int lo, int hi, int flag);
static int   texcmp(const void* a, const void* b);
static int   texidxcmp(const void* a, const void* b);
int          MBOX_ReallyFindObject(const char* name, int a, int b, int create);
static int   objcmp(const void* a, const void* b);

/* ---- 0x800B7758 : finish a pending background model load ---- */
int MBOX_BGLoadModelDone(void) {
    u8* g = gWinGlobals;
    u32 t;

    if (lbl_80344E88 < 0) {
        return 0;
    }
    StartFileRead(lbl_80115DA8, (void*)&lbl_802A5CF0, 0, 0, 0, 0);
    bulletproof_printf(lbl_80115DB4,
                       lbl_80344E88, g, gClockFrameNumber);
    SetupModel((void*)g, lbl_80115DA8, lbl_80344E88);
    t = pbGetCPUTime();

    StartFileRead(lbl_80115DB4, (void*)&lbl_802A5CF0, 0, 0, 0, 0);
    bulletproof_printf(lbl_80115DB4,
                       lbl_80344E88, g, gClockFrameNumber);
    fn_800C7214((void*)g);
    (void)t;

    lbl_80344E88 = -1;
    return 1;
}

/* ---- 0x800B79AC : begin the next background model load ---- */
int MBOX_BGLoadModelStart(int slot) {
    u8* g = gWinGlobals;
    char* names = (char*)g;

    pbSetTime(0);
    if (lbl_80344E88 != 0) {
        FatalError(lbl_80115F24);
    }
    FileSize((const char*)g, lbl_80115DA8);
    FileSize((const char*)g, lbl_80115DB4);
    MBOX_AllocModelMem(0, 0, (const char*)g);
    bulletproof_printf(lbl_80115DB4, slot, g);

    lbl_80344E88 = slot;
    *(const char**)(names + 0x2cc) = lbl_80115DA8;
    *(const char**)(names + 0x2d0) = lbl_80115DA8;
    *(const char**)(names + 0x2d4) = lbl_80115DA8;
    *(const char**)(names + 0x2d8) = lbl_80115DA8;
    *(const char**)(names + 0x2dc) = lbl_80115DA8;
    *(u32*)(names + 0x2e0) = pbGetCPUTime();
    strncpy(names, lbl_80115DA8, 0x40);
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
    return MBOX_LoadModelFixed(dir, 0, 0, 0, -1);
}

/* ---- 0x800B7B54 : load a single model file into a fixed slot ---- */
int MBOX_LoadModelFixed(const char* dir, int a, int b, int c, int type) {
    u8* g = gWinGlobals;
    void* buf;
    int r;

    FileSize(dir, lbl_80115DA8);
    FileSize(dir, lbl_80115DB4);
    buf = (void*)(u32)MBOX_AllocModelMem(0, 0, dir);
    strncpy((char*)&lbl_802A5D1C, dir, 0x1f);
    if (strcmp(dir, lbl_80348C28) == 0) {
        MLMReadFile(dir, buf, 0);
        bulletproof_printf(lbl_80115DB4, a, dir);
        r = SetupModel(buf, dir, a);
        if (r == 2) {
            ErrorPrintf(lbl_80115F24, a, dir);
            fn_800C7214(buf);
        }
    } else {
        MLMReadFile(dir, buf, 0);
        ErrorPrintf(lbl_80115F24, a, dir);
        fn_800C7214(buf);
    }
    (void)b;
    (void)c;
    (void)type;
    (void)g;
    return a;
}

/* ---- 0x800B7D44 : parse/verify a loaded ngc model block (giant) ---- */
static int SetupModel(void* model, const char* name, int slot) {
    /* NonMatching skeleton: the real body is a ~0xAB8 inlined ngc-format
     * parser that walks the model header, validates the version word, and
     * builds the GX display-list / vertex data. Parked. */
    u8* g = gWinGlobals;
    char nm[0x40];
    strncpy(nm, name, 0x3f);
    ErrorPrintf(lbl_80115F24,
                slot, name, 0, 0);
    (void)model;
    (void)g;
    return 0;
}

/* ---- 0x800B87FC : compute+allocate model memory for a named model ---- */
int MBOX_AllocModel(const char* dir) {
    int objSize = FileSize(dir, lbl_80115DA8);
    int texSize = FileSize(dir, lbl_80115DB4);
    return MBOX_AllocModelMem(objSize, texSize, dir);
}

/* ---- 0x800B8854 : allocate a model-memory block from the MLM arena ---- */
int MBOX_AllocModelMem(int objSize, int texSize, const char* dir) {
    u8* g = gWinGlobals;
    void* base;
    int size = objSize + texSize;

    if (strcmp(dir, lbl_80348C28) == 0) {
        size = objSize;
    }
    if (BytesFree() < size) {
        FatalError(lbl_80115F24);
    }
    base = GetMemBase();
    AllocMem(size);
    mlmMemUsed = mlmMemUsed + size;
    bulletproof_printf(lbl_80115DB4, size);
    (void)g;
    (void)base;
    return size;
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
    int slotOffset;
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
    slotOffset = slotIndex * 16;
    for (; slotIndex <= b; slotIndex++, slotOffset += 16) {
        slot = *(u8**)(win + 48) + slotOffset;
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
        slotOffset = 0;
        for (; slotIndex < lbl_80344E8C; slotIndex++, slotOffset += 16) {
            slot = *(u8**)(win + 48) + slotOffset;
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
