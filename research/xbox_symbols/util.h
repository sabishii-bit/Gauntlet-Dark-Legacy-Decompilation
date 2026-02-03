#ifndef _UTIL_H_
#define _UTIL_H_

// Category: util
// Extracted from Xbox PDB symbols
// Total types: 11
// Note: Xbox symbols - may need adaptation for GameCube

struct _UNICODE_STRING// Size=0x8 (Id=146)
{
    unsigned short Length;// Offset=0x0 Size=0x2
    unsigned short MaximumLength;// Offset=0x2 Size=0x2
    unsigned short * Buffer;// Offset=0x4 Size=0x4
};

struct _GUID// Size=0x10 (Id=148)
{
    unsigned long Data1;// Offset=0x0 Size=0x4
    unsigned short Data2;// Offset=0x4 Size=0x2
    unsigned short Data3;// Offset=0x6 Size=0x2
    unsigned char Data4[8];// Offset=0x8 Size=0x8
};

struct _GUID// Size=0x10 (Id=158)
{
    unsigned long Data1;// Offset=0x0 Size=0x4
    unsigned short Data2;// Offset=0x4 Size=0x2
    unsigned short Data3;// Offset=0x6 Size=0x2

    
    unsigned char Data4[8];// Offset=0x8 Size=0x8
};

struct _STRING64// Size=0x10 (Id=185)
{
    unsigned short Length;// Offset=0x0 Size=0x2
    unsigned short MaximumLength;// Offset=0x2 Size=0x2
    unsigned char __align0[4];// Offset=0x4 Size=0x4
    unsigned long long Buffer;// Offset=0x8 Size=0x8
};

struct _CPTABLEINFO// Size=0x2c (Id=188)
{
    unsigned short CodePage;// Offset=0x0 Size=0x2
    unsigned short MaximumCharacterSize;// Offset=0x2 Size=0x2
    unsigned short DefaultChar;// Offset=0x4 Size=0x2
    unsigned short UniDefaultChar;// Offset=0x6 Size=0x2
    unsigned short TransDefaultChar;// Offset=0x8 Size=0x2
    unsigned short TransUniDefaultChar;// Offset=0xa Size=0x2
    unsigned short DBCSCodePage;// Offset=0xc Size=0x2
    unsigned char LeadByte[12];// Offset=0xe Size=0xc
    unsigned char __align0[2];// Offset=0x1a Size=0x2
    unsigned short * MultiByteTable;// Offset=0x1c Size=0x4
    void * WideCharTable;// Offset=0x20 Size=0x4
    unsigned short * DBCSRanges;// Offset=0x24 Size=0x4
    unsigned short * DBCSOffsets;// Offset=0x28 Size=0x4
};

struct _STRING// Size=0x8 (Id=215)
{
    unsigned short Length;// Offset=0x0 Size=0x2
    unsigned short MaximumLength;// Offset=0x2 Size=0x2
    char * Buffer;// Offset=0x4 Size=0x4
};

struct _UNICODE_PREFIX_TABLE_ENTRY// Size=0x1c (Id=241)
{
    short NodeTypeCode;// Offset=0x0 Size=0x2
    short NameLength;// Offset=0x2 Size=0x2
    struct _UNICODE_PREFIX_TABLE_ENTRY * NextPrefixTree;// Offset=0x4 Size=0x4
    struct _UNICODE_PREFIX_TABLE_ENTRY * CaseMatch;// Offset=0x8 Size=0x4
    struct _RTL_SPLAY_LINKS Links;// Offset=0xc Size=0xc
    struct _UNICODE_STRING * Prefix;// Offset=0x18 Size=0x4
};

struct _STRING32// Size=0x8 (Id=529)
{
    unsigned short Length;// Offset=0x0 Size=0x2
    unsigned short MaximumLength;// Offset=0x2 Size=0x2
    unsigned long Buffer;// Offset=0x4 Size=0x4
};

struct _UNICODE_PREFIX_TABLE// Size=0xc (Id=804)
{
    short NodeTypeCode;// Offset=0x0 Size=0x2
    short NameLength;// Offset=0x2 Size=0x2
    struct _UNICODE_PREFIX_TABLE_ENTRY * NextPrefixTree;// Offset=0x4 Size=0x4
    struct _UNICODE_PREFIX_TABLE_ENTRY * LastNextEntry;// Offset=0x8 Size=0x4
};

struct _UNICODE_STRING// Size=0x8 (Id=817)
{
    unsigned short Length;// Offset=0x0 Size=0x2
    unsigned short MaximumLength;// Offset=0x2 Size=0x2
    unsigned short * Buffer;// Offset=0x4 Size=0x4
};

struct _GUID_WMC// Size=0x10 (Id=2781)
{
    unsigned long Data1;// Offset=0x0 Size=0x4
    unsigned short Data2;// Offset=0x4 Size=0x2
    unsigned short Data3;// Offset=0x6 Size=0x2
    unsigned char Data4[8];// Offset=0x8 Size=0x8
};


#endif // _UTIL_H_
