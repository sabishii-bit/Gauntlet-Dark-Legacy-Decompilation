#ifndef __PPC_EABI_LINKER
#define __PPC_EABI_LINKER

#ifdef __cplusplus
extern "C" {
#endif

extern char _stack_addr[];
extern char _stack_end[];
extern char _heap_addr[];
extern char _heap_end[];

extern char _SDA_BASE_[];
extern char _SDA2_BASE_[];

typedef struct __eti_init_info {
    void* eti_start;
    void* eti_end;
    void* code_start;
    unsigned long code_size;
} __eti_init_info;

extern __eti_init_info _eti_init_info[];

#ifdef __cplusplus
}
#endif

#endif
