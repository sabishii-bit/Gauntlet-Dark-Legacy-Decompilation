#ifndef __START_H
#define __START_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PAD3_BUTTON_ADDR 0x800030E4
#define EXCEPTIONMASK_ADDR 0x80000044
#define BOOTINFO2_ADDR 0x800000F4
#define OS_BI2_DEBUGFLAG_OFFSET 0xC
#define ARENAHI_ADDR 0x80000034
#define DVD_DEVICECODE_ADDR 0x800030E6

extern void InitMetroTRK();

extern int main(int argc, char* argv[]);
extern void exit(int);
extern void __init_user(void);
extern void OSInit(void);
extern void DBInit(void);
extern void __OSPSInit(void);
extern void __OSCacheInit(void);
extern void OSResetSystem(BOOL reset, u32 resetCode, BOOL forceMenu);
extern void* memset(void* dst, int val, size_t n);
extern void* memcpy(void* dst, const void* src, size_t n);

SECTION_INIT static void __check_pad3(void);
SECTION_INIT extern __declspec(weak) void __start(void);
SECTION_INIT static void __init_registers(void);
SECTION_INIT static void __init_data(void);
SECTION_INIT extern void __init_hardware(void);
SECTION_INIT extern void __flush_cache(void* addr, u32 size);

SECTION_INIT extern u8 _stack_addr[];
SECTION_INIT extern char _SDA_BASE_[];
SECTION_INIT extern char _SDA2_BASE_[];

typedef struct __rom_copy_info {
    char* rom;
    char* addr;
    uint size;
} __rom_copy_info;

SECTION_INIT extern __rom_copy_info _rom_copy_info[];

typedef struct __bss_init_info {
    char* addr;
    uint size;
} __bss_init_info;

SECTION_INIT extern __bss_init_info _bss_init_info[];

#ifdef __cplusplus
};
#endif

#endif /* __START_H */
