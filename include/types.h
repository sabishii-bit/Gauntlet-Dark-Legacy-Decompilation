#ifndef TYPES_H
#define TYPES_H

typedef signed char s8;
typedef signed short s16;
typedef signed long s32;
typedef signed long long s64;
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;
typedef unsigned long long u64;

typedef volatile u8 vu8;
typedef volatile u16 vu16;
typedef volatile u32 vu32;
typedef volatile u64 vu64;
typedef volatile s8 vs8;
typedef volatile s16 vs16;
typedef volatile s32 vs32;
typedef volatile s64 vs64;

typedef float f32;
typedef double f64;
typedef volatile f32 vf32;
typedef volatile f64 vf64;

typedef int BOOL;
typedef unsigned int uint;
typedef unsigned long size_t;

#define TRUE 1
#define FALSE 0

#ifndef NULL
#define NULL 0
#endif

#ifdef __MWERKS__
#define DECL_SECT(name) __declspec(section name)
#define WEAKFUNC __declspec(weak)
#define ASM asm
#else
#define DECL_SECT(name)
#define WEAKFUNC
#define ASM
#endif

#define SECTION_INIT DECL_SECT(".init")

#ifdef __MWERKS__
#define ATTRIBUTE_ALIGN(n) __attribute__((aligned(n)))
#else
#define ATTRIBUTE_ALIGN(n)
#endif

#endif
