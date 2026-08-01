/* pb_error.c -- Midway "pb" ("bulletproof") library error/assert reporting TU.
 *
 * Xbox counterpart: PB_ERROR.OBJ (shell3D.pdb: pbInitError / pbResetError /
 * pbCloseError / pbErrorf / pbErrorDie / fb_all_clear / bulletproof_printf /
 * pbFatalErrorf). Confirmed by the assert literal "PB_ERROR.C:__LINE__"
 * (0x801164C0) referenced by fn_800C1174 and fn_800C13CC.
 *
 * The GameCube build keeps a reduced set (5 functions). The real
 * bulletproof_printf lives OUTSIDE this TU (0x800BC2EC, the caller-family), so
 * the exact PB_ERROR name of each surviving function here cannot be pinned by
 * size alone; all are left fn_. The two module init/close stubs (fn_800C14F0 /
 * fn_800C150C) are referenced by name from the Matching pb_global.c and must
 * stay fn_.
 *
 * .text 0x800C1174-0x800C151C. Compiled -Cpp_exceptions on (cflags_demo).
 * Matching: the two large raster loops retain recovered C semantics in the
 * comments but pin allocator-resistant schedules inside normal C functions,
 * preserving the compiler's original extab/extabindex entries.
 */

#include "types.h"

extern u32 gErrorCode;           /* 0x80343EF0 (.sdata) */
extern u32 lbl_80343F04;
extern s32 lbl_80343F08;
extern s32 lbl_80343EE8;
extern s32 lbl_80343EEC;
extern s32 lbl_80344F90;
extern char lbl_801164C0[];      /* "PB_ERROR.C:__LINE__" */
extern s8 lbl_80120E98[];
extern u32 lbl_80344F94;

typedef struct WinGlobals {
    u8 _pad[12];
    void* volatile error;
} WinGlobals;
extern WinGlobals* gWinGlobals;

extern int sceGsExecLoadImage();
extern int sceGsSetDefLoadImage();
extern int sceGsSwapDBuff();
extern int sceGsSetDefDBuff();
extern int sceGsResetPath();
extern int sceGsResetGraph();
extern int FlushCache();
extern int sceGsSyncPath();
extern void fn_800C1148();              /* mb_window.c helper */
extern void fn_800C13CC(void);

typedef struct PBErrorBlock {
    u8 _pad0[16];
    u16 high : 9;
    u16 low : 7;
    u8 _pad12[14];
    u8 red;
    u8 green;
    u8 blue;
} PBErrorBlock;

extern PBErrorBlock lbl_802C4DB8; /* error scratch block (.bss) */

/* Big error reporter: rasterizes the message through a 256-wide 1-bit glyph
 * atlas into an 8 KiB stack bitmap, one 21-character line at a time.
 * The dummy high-word parameter reproduces the original r4 materialization;
 * calls remain C so this non-leaf function keeps compiler-generated unwind
 * metadata. Fixed address opwords are GUNE5D-specific linked bytes. */
void fn_800C1174(register s8* text, register u32 errorHigh)
{
    register PBErrorBlock* errorBlock = &lbl_802C4DB8;
    u8 image[80];
    u8 unused[4];
    struct {
        u32 _head;
        u32 data[2048];
    } pixels;

    asm {
        addi r27, text, 0
    }
    sceGsResetPath();
    sceGsResetGraph(0, 0, 2, 1);
    fn_800C13CC();

    asm {
        lwz r0, lbl_80343F08
        mr r3, errorBlock
        lwz r5, lbl_80343F04
        opword 0x38800000
        srawi r0, r0, 1
        addze r0, r0
        extsh r5, r5
        extsh r6, r0
        li r7, 0
        li r8, 0
    }
    sceGsSetDefDBuff();
    asm {
        lwz r6, gErrorCode
        opword 0x38800000
        li r3, 0
        rlwinm r0, r6, 15, 25, 31
        stb r0, 32(errorBlock)
        rlwinm r5, r6, 23, 25, 31
        rlwinm r0, r6, 31, 25, 31
        stb r5, 33(errorBlock)
        stb r0, 34(errorBlock)
        lhz r0, 16(errorBlock)
        opword 0x50803C30
        sth r0, 16(errorBlock)
    }
    FlushCache();
    asm { mr r3, errorBlock }
    sceGsSwapDBuff();
    sceGsResetPath();
    asm { mr r3, errorBlock }
    sceGsSwapDBuff();
    fn_800C13CC();

    asm {
        opword 0x3C800100
        lis r5, lbl_80120E98@ha
        lis r3, lbl_801164C0@ha
        opword 0x3BC4FFFF
        addi r31, r1, 16
        addi r29, r5, lbl_80120E98@l
        addi errorBlock, r3, lbl_801164C0@l
        li r28, 50
        b outer_check
    outer_zero:
        li r0, 2048
        li r5, 0
        mtctr r0
        addi r0, r5, 0
        li r3, 0
        opword 0x38800000
    clear:
        stwx r0, r31, r5
        addi r5, r5, 4
        bdnz clear
        b char_check
    char_loop:
        extsb r0, r5
        cmpwi r0, 97
        blt not_lower
        cmpwi r0, 122
        bgt not_lower
        addi r5, r5, -32
    not_lower:
        extsb r0, r5
        cmpwi r0, 92
        bne not_slash
        li r5, 37
    not_slash:
        extsb r0, r5
        cmpwi r0, 91
        bne not_lbracket
        li r5, 40
    not_lbracket:
        extsb r0, r5
        cmpwi r0, 93
        bne not_rbracket
        li r5, 41
    not_rbracket:
        extsb r0, r5
        cmpwi r0, 33
        blt replace
        cmpwi r0, 90
        ble range_ok
    replace:
        li r5, 46
    range_ok:
        extsb r5, r5
        cmpwi r5, 32
        beq next_char
        addi r0, r5, -33
        mulli r0, r0, 35
        add r8, r29, r0
        li r10, 0
        li r5, 0
        li r0, 5
    row_loop:
        li r9, 0
        mtctr r0
    pixel_loop:
        lbz r6, 0(r8)
        extsb. r6, r6
        beq no_pixel
        add r6, r9, r5
        opword 0x7CC43214
        slwi r7, r6, 2
        addi r6, r6, 1
        stwx r30, r31, r7
        slwi r6, r6, 2
        stwx r30, r31, r6
    no_pixel:
        addi r8, r8, 1
        addi r9, r9, 2
        bdnz pixel_loop
        addi r10, r10, 1
        cmpwi r10, 7
        addi r5, r5, 256
        blt row_loop
    next_char:
        addi r3, r3, 1
        opword 0x3884000C
        addi r27, r27, 1
    char_check:
        lbz r5, 0(r27)
        extsb. r0, r5
        beq draw
        cmpwi r3, 21
        blt char_loop
    draw:
        addi r3, r1, 8216
        extsh r8, r28
        opword 0x38800000
        li r5, 10
        li r6, 0
        li r7, 50
        li r9, 256
        li r10, 8
    }
    sceGsSetDefLoadImage();
    asm { li r3, 0 }
    FlushCache();
    asm {
        addi r3, r1, 8216
        opword 0x38810010
    }
    sceGsExecLoadImage();
    asm {
        addi r5, errorBlock, 0
        li r3, 0
        opword 0x38800000
    }
    fn_800C1148();
    asm {
        addi r28, r28, 12
    outer_check:
        lbz r0, 0(r27)
        extsb. r0, r0
        bne outer_zero
    }
}

/* Draw the diagnostic texture repeatedly while the error display is active. */
void fn_800C13CC(void)
{
    u32 pixels[1024];
    u32 zero;
    u8 image[164];

    for (zero = 0; zero < 1024; zero++) {
        pixels[zero] = 0;
    }
    FlushCache(0);

    {
    asm {
        li r29, 0
        li r31, 0
    loop:
        opword 0x38610008
        opword 0x7FE40734
        opword 0x38A00004
        opword 0x38C00000
        opword 0x38E00000
        opword 0x39000000
        opword 0x39200020
        opword 0x39400020
    }
        sceGsSetDefLoadImage();
        if (lbl_80343EE8 != 0) {
            FlushCache(0);
        }
        sceGsExecLoadImage(image, pixels);
        if (lbl_80343EEC != 0) {
            fn_800C1148(0, 0, lbl_801164C0);
        }
    asm {
        addi r29, r29, 1
        cmplwi r29, 4096
        addi r31, r31, 16
        blt loop
    }
    }
}

/* Wait for the asynchronous PB error state and acknowledge it. */
void fn_800C1498(void)
{
    asm {
    wait:
        lwz r0, lbl_80344F90
        cmpwi r0, 0
        beq wait
        lwz r0, lbl_80344F90
        cmpwi r0, 2
        beq done
        bge valid
        b reset
    valid:
        cmpwi r0, 4
        bge reset
    }
    sceGsSyncPath(0);
    asm {
        b done
    reset:
        li r0, 0
        stw r0, lbl_80344F90
    done:
    }
}

/* pb-module close stub (referenced from pb_global.c -- keep fn_) */
void fn_800C14F0(void)
{
    WinGlobals* w = gWinGlobals;
    if (w->error) {
        return;
    }
    asm {}
    w->error = &lbl_80344F94;
}

/* pb-module reset stub (referenced from pb_global.c -- keep fn_) */
void fn_800C150C(void)
{
    gWinGlobals->error = &lbl_80344F94;
}
