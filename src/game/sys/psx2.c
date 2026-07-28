/*
 * psx2.c -- retained PS2/IOP compatibility bootstrap.
 *
 * .text       0x8008BC50..0x8008BF88
 * .rodata     0x801142F0..0x801143F8
 * .sdata2     0x80347E70..0x80347E90
 * extab       0x80006C50..0x80006C60
 * extabindex  0x8000AAC8..0x8000AAE0
 */

typedef signed int s32;
typedef unsigned int u32;
typedef unsigned char u8;

extern s32 lbl_803445EC;
extern s32 lbl_80344B48;
extern u32* lbl_80343E78;

extern void fn_800AEA54(s32);
extern s32 fn_800AEA58(void);
extern s32 fn_800AEA60(const char*);
extern s32 fn_800AEA68(void);
extern s32 fn_800AEA70(const char*, s32, void*);
extern s32 fn_800AEA78(void);
extern s32 fn_800AEA90(void);
extern void mathStub2__Fv(s32);
extern void mathStub3__Fv(s32);
extern void dcsInit(void);
extern s32 memcmp(const void*, const void*, u32);
extern s32 sprintf(char*, const char*, ...);
extern void bulletproof_printf(const char*, ...);
extern void FatalErrorf(const char*, ...);

static s32 load_irx_args(char* directory, char* name, s32 fatal,
                         s32 argLength, void* args);
static void fed_upper(char* text, char c);

void LoadVU1GameLogic(void) {
}

/* The caller supplies a legacy boot argument, but this compatibility body
 * intentionally ignores it; the original K&R-style interface accepts it. */
void init_psx2() {
    char message[64];
    u8 unused[4];
    s32 result;

    fn_800AEA54(0);
    mathStub3__Fv(0);
    mathStub2__Fv(1);
    sprintf(message, "REBOOT IOP (%s%s)", "cdrom0:\\\\", "IOPRP21.IMG");

    do {
        result = fn_800AEA60("cdrom0:\\\\IOPRP21.IMG");
    } while (result == 0);
    if (result < 0) {
        FatalErrorf("sceSifRebootIop %s%s Failed: %d", "cdrom0:\\\\",
                    "IOPRP21.IMG", result);
    }
    while (fn_800AEA58() == 0) {
    }

    fn_800AEA54(0);
    mathStub3__Fv(0);
    mathStub2__Fv(1);
    fn_800AEA90();
    fn_800AEA68();

    if (lbl_80344B48 != 0) {
        load_irx_args("cdrom0:\\\\IRX\\", "mem2MB.irx", 1, 0, 0);
        while (fn_800AEA60("cdrom0:\\\\IOPRP21.IMG") == 0) {
        }
        while (fn_800AEA58() == 0) {
        }
        fn_800AEA54(0);
        mathStub3__Fv(0);
        mathStub2__Fv(1);
        fn_800AEA90();
    }

    fn_800AEA78();
    sprintf(message, "LOAD IRX (%s)", "cdrom0:\\\\IRX\\");
    load_irx_args("cdrom0:\\\\IRX\\", "sio2man.irx", 1, 0, 0);
    if (load_irx_args("cdrom0:\\\\IRX\\", "mtapman.irx", 0, 0, 0) == 0) {
        lbl_803445EC = -1;
    }
    load_irx_args("cdrom0:\\\\IRX\\", "mcman.irx", 1, 0, 0);
    load_irx_args("cdrom0:\\\\IRX\\", "mcserv.irx", 1, 0, 0);
    load_irx_args("cdrom0:\\\\IRX\\", "padman.irx", 1, 0, 0);
    load_irx_args("cdrom0:\\\\IRX\\", "libsd.irx", 1, 0, 0);
    dcsInit();
    load_irx_args("cdrom0:\\\\IRX\\", "dcs.irx", 1, 0, 0);
    *lbl_80343E78 = 0x83;
}

static void fed_upper(char* text, char c) {
    while ((c = *text) != 0) {
        if (c >= 'a' && c <= 'z') {
            *text = c - 0x20;
        }
        text++;
    }
}

static s32 load_irx_args(char* directory, char* name, s32 fatal,
                         s32 argLength, void* args) {
    s32 tries;
    s32 result;
    s32 delay = 0x100000;
    char path[128];

    if (memcmp(directory, "cdrom", 5) == 0) {
        fed_upper(name, 0);
        sprintf(path, "%s%s;1", directory, name);
    } else {
        sprintf(path, "%s%s", directory, name);
    }

    tries = 0;
    do {
        result = fn_800AEA70(path, argLength, args);
        if (result >= 0) {
            break;
        }
        {
            s32 i;
            for (i = 0; i < delay; i++) {
            }
        }
        tries++;
    } while (tries < 100);

    if (result < 0) {
        bulletproof_printf("Can't load module: %s (%d) (%s)\n", path,
                           result, "IOPRP21.IMG");
        if (fatal != 0) {
            FatalErrorf("%s(%d)(%s)\n", path, result, "IOPRP21.IMG");
        }
    }
    return result >= 0;
}
