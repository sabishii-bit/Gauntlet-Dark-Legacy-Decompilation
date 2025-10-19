ENTRY(__start)
_SDA_BASE_ = 0x-7FCB4500;
_SDA2_BASE_ = 0x-7FCB28E0;
PHDRS
{
    init PT_LOAD;
    extab PT_LOAD;
    extabindex PT_LOAD;
    text PT_LOAD;
    ctors PT_LOAD;
    dtors PT_LOAD;
    rodata PT_LOAD;
    data PT_LOAD;
    bss PT_LOAD;
    sdata PT_LOAD;
    data9 PT_LOAD;
}
SECTIONS
{
    .init 0x80003100 : { *(.init) } : init
    extab 0x800054E0 : { *(extab) } : extab
    extabindex 0x800087A0 : { *(extabindex) } : extabindex
    .text 0x8000CF40 : { *(.text) } : text
    .ctors 0x801106E0 : { *(.ctors) } : ctors
    .dtors 0x80110700 : { *(.dtors) } : dtors
    .rodata 0x80110720 : { *(.rodata) } : rodata
    .data 0x80118100 : { *(.data) } : data
    .bss 0x8023CA40 (NOLOAD) : { *(.bss) } : bss
    .sdata 0x80343B00 : { *(.sdata) } : sdata
    .data9 0x80345720 : { *(.data9) } : data9
}
