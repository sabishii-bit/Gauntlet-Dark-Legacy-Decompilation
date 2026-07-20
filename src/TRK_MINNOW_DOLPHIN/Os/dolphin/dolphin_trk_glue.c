#include "trk.h"

#define HARDWARE_NDEV 1

typedef struct OSContext {
    /* 0x000 */ u32 gpr[32];
    /* 0x080 */ u32 cr;
    /* 0x084 */ u32 lr;
    /* 0x088 */ u32 ctr;
    /* 0x08C */ u32 xer;
    /* 0x090 */ f64 fpr[32];
    /* 0x190 */ u32 fpscr_pad;
    /* 0x194 */ u32 fpscr;
    /* 0x198 */ u32 srr0;
    /* 0x19C */ u32 srr1;
    /* 0x1A0 */ u16 mode;
    /* 0x1A2 */ u16 state;
    /* 0x1A4 */ u32 gqr[8];
    /* 0x1C4 */ u32 psf_pad;
    /* 0x1C8 */ f64 psf[32];
} OSContext;

typedef int (*DBCommInitFunc)(void*, void*);
typedef int (*DBCommFunc)(void);
typedef int (*DBCommReadFunc)(void*, int);
typedef int (*DBCommWriteFunc)(const void*, int);

typedef struct DBCommTable {
    /* 0x00 */ DBCommInitFunc initialize_func;
    /* 0x04 */ DBCommFunc initinterrupts_func;
    /* 0x08 */ DBCommFunc peek_func;
    /* 0x0C */ DBCommReadFunc read_func;
    /* 0x10 */ DBCommWriteFunc write_func;
    /* 0x14 */ DBCommFunc open_func;
    /* 0x18 */ DBCommFunc close_func;
} DBCommTable;

void TRKInterruptHandler(void);
void OSEnableScheduler(void);
void OSReport(const char* msg, ...);
int Hu_IsStub(void);
int AMC_IsStub(void);

void DBInitComm(void);
void DBInitInterrupts(void);
void DBQueryData(void);
void DBRead(void);
void DBWrite(void);
void DBOpen(void);
void DBClose(void);
void EXI2_Init(void);
void EXI2_EnableInterrupts(void);
void EXI2_Poll(void);
void EXI2_ReadN(void);
void EXI2_WriteN(void);
void EXI2_Reserve(void);
void EXI2_Unreserve(void);

static DBCommTable gDBCommTable = { NULL, NULL, NULL, NULL, NULL, NULL, NULL };

ASM static void TRKLoadContext(OSContext* ctx, register u32 val) {
#ifdef __MWERKS__ // clang-format off
    nofralloc

    lwz r0, 0(r3)
    lwz r1, 4(r3)
    lwz r2, 8(r3)
    lhz r5, 0x1a2(r3)
    rlwinm. r6, r5, 0, 0x1e, 0x1e
    beq loadNonVolatile
    rlwinm r5, r5, 0, 0x1f, 0x1d
    sth r5, 0x1a2(r3)
    lmw r5, 0x14(r3)
    b afterLoad
loadNonVolatile:
    lmw r13, 0x34(r3)
afterLoad:
    mr r31, r3
    mr r3, r4
    lwz r4, 0x80(r31)
    mtcrf 0xff, r4
    lwz r4, 0x84(r31)
    mtlr r4
    lwz r4, 0x88(r31)
    mtctr r4
    lwz r4, 0x8c(r31)
    mtxer r4
    mfmsr r4
    rlwinm r4, r4, 0, 0x11, 0xf
    rlwinm r4, r4, 0, 0x1f, 0x1d
    mtmsr r4
    mtsprg 1, r2
    lwz r4, 0xc(r31)
    mtsprg 2, r4
    lwz r4, 0x10(r31)
    mtsprg 3, r4
    lwz r2, 0x198(r31)
    lwz r4, 0x19c(r31)
    lwz r31, 0x7c(r31)
    b TRKInterruptHandler
#endif // clang-format on
}

static void TRKEXICallBack(short r3, OSContext* ctx) {
    OSEnableScheduler();
    TRKLoadContext(ctx, 0x500);
}

int InitMetroTRKCommTable(int hwId) {
    int isStub;

    if (hwId == HARDWARE_NDEV) {
        isStub = Hu_IsStub();
        gDBCommTable.initialize_func = (DBCommInitFunc)DBInitComm;
        gDBCommTable.initinterrupts_func = (DBCommFunc)DBInitInterrupts;
        gDBCommTable.peek_func = (DBCommFunc)DBQueryData;
        gDBCommTable.read_func = (DBCommReadFunc)DBRead;
        gDBCommTable.write_func = (DBCommWriteFunc)DBWrite;
        gDBCommTable.open_func = (DBCommFunc)DBOpen;
        gDBCommTable.close_func = (DBCommFunc)DBClose;
        return isStub;
    } else {
        isStub = AMC_IsStub();
        gDBCommTable.initialize_func = (DBCommInitFunc)EXI2_Init;
        gDBCommTable.initinterrupts_func = (DBCommFunc)EXI2_EnableInterrupts;
        gDBCommTable.peek_func = (DBCommFunc)EXI2_Poll;
        gDBCommTable.read_func = (DBCommReadFunc)EXI2_ReadN;
        gDBCommTable.write_func = (DBCommWriteFunc)EXI2_WriteN;
        gDBCommTable.open_func = (DBCommFunc)EXI2_Reserve;
        gDBCommTable.close_func = (DBCommFunc)EXI2_Unreserve;
        return isStub;
    }
}

void TRKUARTInterruptHandler(void) {}

DSError TRKInitializeIntDrivenUART(u32 r3, u32 r4, u32 r5, void* r6) {
    gDBCommTable.initialize_func(r6, TRKEXICallBack);
    return DS_NoError;
}

void EnableEXI2Interrupts(void) {
    gDBCommTable.initinterrupts_func();
}

int TRKPollUART(void) {
    return gDBCommTable.peek_func();
}

DSError TRK_ReadUARTN(void* bytes, u32 limit) {
    int r3 = gDBCommTable.read_func(bytes, limit);
    return !r3 ? DS_NoError : -1;
}

DSError TRK_WriteUARTN(const void* bytes, u32 length) {
    int r3 = gDBCommTable.write_func(bytes, length);
    return !r3 ? DS_NoError : -1;
}

void ReserveEXI2Port(void) {
    gDBCommTable.open_func();
}

void UnreserveEXI2Port(void) {
    gDBCommTable.close_func();
}

#pragma push
#pragma peephole off
void TRK_board_display(char* str) {
    OSReport(str);
}
#pragma pop
