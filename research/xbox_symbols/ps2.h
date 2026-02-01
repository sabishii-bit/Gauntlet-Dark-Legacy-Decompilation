#ifndef _PS2_H_
#define _PS2_H_

// Category: ps2
// Extracted from Xbox PDB symbols
// Total types: 80
// Note: Xbox symbols - may need adaptation for GameCube

struct sceSifCmdSRData// Size=0x18 (Id=1992)
{
    struct sceSifCmdHdr chdr;// Offset=0x0 Size=0x10
    int rno;// Offset=0x10 Size=0x4
    unsigned int value;// Offset=0x14 Size=0x4
};

enum sceMpegCbType
{
    sceMpegCbError=0,
    sceMpegCbNodata=1,
    sceMpegCbStopDMA=2,
    sceMpegCbRestartDMA=3,
    sceMpegCbBackground=4,
    sceMpegCbTimeStamp=5,
    sceMpegCbStr=6
};

struct sceCdRMode// Size=0x4 (Id=2074)
{
    unsigned char trycount;// Offset=0x0 Size=0x1
    unsigned char spindlctrl;// Offset=0x1 Size=0x1
    unsigned char datapattern;// Offset=0x2 Size=0x1
    unsigned char pad;// Offset=0x3 Size=0x1
};

struct sceGsDBuff// Size=0x220 (Id=2078)
{
    struct sceGsDispEnv disp[2];// Offset=0x0 Size=0x50
    struct sceGifTag giftag0;// Offset=0x50 Size=0x10
    struct sceGsDrawEnv1 draw0;// Offset=0x60 Size=0x80
    struct sceGsClear clear0;// Offset=0xe0 Size=0x58
    struct sceGifTag giftag1;// Offset=0x138 Size=0x10
    struct sceGsDrawEnv1 draw1;// Offset=0x148 Size=0x80
    struct sceGsClear clear1;// Offset=0x1c8 Size=0x58
};

struct sceGsLoadImage// Size=0x60 (Id=2079)
{
    struct sceGifTag giftag0;// Offset=0x0 Size=0x10
    struct sceGsBitbltbuf bitbltbuf;// Offset=0x10 Size=0x8
    long bitbltbufaddr;// Offset=0x18 Size=0x4
    unsigned char __align0[4];// Offset=0x1c Size=0x4
    struct sceGsTrxpos trxpos;// Offset=0x20 Size=0x8
    long trxposaddr;// Offset=0x28 Size=0x4
    unsigned char __align1[4];// Offset=0x2c Size=0x4
    struct sceGsTrxreg trxreg;// Offset=0x30 Size=0x8
    long trxregaddr;// Offset=0x38 Size=0x4
    unsigned char __align2[4];// Offset=0x3c Size=0x4
    struct sceGsTrxdir trxdir;// Offset=0x40 Size=0x8
    long trxdiraddr;// Offset=0x48 Size=0x4
    unsigned char __align3[4];// Offset=0x4c Size=0x4
    struct sceGifTag giftag1;// Offset=0x50 Size=0x10
};

enum sceMpegStrType
{
    sceMpegStrM2V=0,
    sceMpegStrIPU=1,
    sceMpegStrPCM=2,
    sceMpegStrADPCM=3,
    sceMpegStrDATA=4
};

struct sceGifPacket// Size=0x10 (Id=2083)
{
    unsigned int * pCurrent;// Offset=0x0 Size=0x4
    unsigned int * pBase;// Offset=0x4 Size=0x4
    unsigned int * pDmaTag;// Offset=0x8 Size=0x4
    unsigned long * pGifTag;// Offset=0xc Size=0x4
};

struct sceGsDBuffDc// Size=0x320 (Id=2146)
{
    struct sceGsDispEnv disp[2];// Offset=0x0 Size=0x50
    struct sceGifTag giftag0;// Offset=0x50 Size=0x10
    struct sceGsDrawEnv1 draw01;// Offset=0x60 Size=0x80
    struct sceGsDrawEnv2 draw02;// Offset=0xe0 Size=0x80
    struct sceGsClear clear0;// Offset=0x160 Size=0x58
    struct sceGifTag giftag1;// Offset=0x1b8 Size=0x10
    struct sceGsDrawEnv1 draw11;// Offset=0x1c8 Size=0x80
    struct sceGsDrawEnv2 draw12;// Offset=0x248 Size=0x80
    struct sceGsClear clear1;// Offset=0x2c8 Size=0x58
};

struct sceGsDispEnv// Size=0x28 (Id=2150)
{
    struct tGS_PMODE pmode;// Offset=0x0 Size=0x8
    struct tGS_SMODE2 smode2;// Offset=0x8 Size=0x8
    struct tGS_DISPFB2 dispfb;// Offset=0x10 Size=0x8
    struct tGS_DISPLAY2 display;// Offset=0x18 Size=0x8
    struct tGS_BGCOLOR bgcolor;// Offset=0x20 Size=0x8
};

struct sceMcTblGetDir// Size=0x40 (Id=2279)
{
    struct __unnamed _Create;// Offset=0x0 Size=0x8
    struct __unnamed _Modify;// Offset=0x8 Size=0x8
    unsigned int FileSizeByte;// Offset=0x10 Size=0x4
    unsigned short AttrFile;// Offset=0x14 Size=0x2
    unsigned short Reserve1;// Offset=0x16 Size=0x2
    unsigned int Reserve2;// Offset=0x18 Size=0x4
    unsigned int PdaAplNo;// Offset=0x1c Size=0x4
    unsigned char EntryName[32];// Offset=0x20 Size=0x20
};

struct sceGsBitbltbuf// Size=0x8 (Id=3295)
{
    struct // Size=0x8 (Id=0)
    {
        unsigned long long SBP:14;// Offset=0x0 Size=0x8 BitOffset=0x0 BitSize=0xe
        unsigned long long pad14:2;// Offset=0x0 Size=0x8 BitOffset=0xe BitSize=0x2
        unsigned long long SBW:6;// Offset=0x0 Size=0x8 BitOffset=0x10 BitSize=0x6
        unsigned long long pad22:2;// Offset=0x0 Size=0x8 BitOffset=0x16 BitSize=0x2
        unsigned long long SPSM:6;// Offset=0x0 Size=0x8 BitOffset=0x18 BitSize=0x6
        unsigned long long pad30:2;// Offset=0x0 Size=0x8 BitOffset=0x1e BitSize=0x2
        unsigned long long DBP:14;// Offset=0x0 Size=0x8 BitOffset=0x20 BitSize=0xe
        unsigned long long pad46:2;// Offset=0x0 Size=0x8 BitOffset=0x2e BitSize=0x2
        unsigned long long DBW:6;// Offset=0x0 Size=0x8 BitOffset=0x30 BitSize=0x6
        unsigned long long pad54:2;// Offset=0x0 Size=0x8 BitOffset=0x36 BitSize=0x2
        unsigned long long DPSM:6;// Offset=0x0 Size=0x8 BitOffset=0x38 BitSize=0x6
        unsigned long long pad62:2;// Offset=0x0 Size=0x8 BitOffset=0x3e BitSize=0x2
    };
};

struct tGS_DISPLAY2// Size=0x8 (Id=3296)
{
    struct // Size=0x8 (Id=0)
    {
        unsigned int DX:12;// Offset=0x0 Size=0x4 BitOffset=0x0 BitSize=0xc
        unsigned int DY:11;// Offset=0x0 Size=0x4 BitOffset=0xc BitSize=0xb
        unsigned int MAGH:4;// Offset=0x0 Size=0x4 BitOffset=0x17 BitSize=0x4
        unsigned int MAGV:2;// Offset=0x0 Size=0x4 BitOffset=0x1b BitSize=0x2
        unsigned int p0:3;// Offset=0x0 Size=0x4 BitOffset=0x1d BitSize=0x3
        unsigned int DW:12;// Offset=0x4 Size=0x4 BitOffset=0x0 BitSize=0xc
        unsigned int DH:11;// Offset=0x4 Size=0x4 BitOffset=0xc BitSize=0xb
        unsigned int p1:9;// Offset=0x4 Size=0x4 BitOffset=0x17 BitSize=0x9
    };
};

struct sceGsDrawEnv1// Size=0x80 (Id=3301)
{
    struct sceGsFrame frame1;// Offset=0x0 Size=0x8
    unsigned long frame1addr;// Offset=0x8 Size=0x4
    unsigned char __align0[4];// Offset=0xc Size=0x4
    struct sceGsZbuf zbuf1;// Offset=0x10 Size=0x8
    long zbuf1addr;// Offset=0x18 Size=0x4
    unsigned char __align1[4];// Offset=0x1c Size=0x4
    struct sceGsXyoffset xyoffset1;// Offset=0x20 Size=0x8
    long xyoffset1addr;// Offset=0x28 Size=0x4
    unsigned char __align2[4];// Offset=0x2c Size=0x4
    struct sceGsScissor scissor1;// Offset=0x30 Size=0x8
    long scissor1addr;// Offset=0x38 Size=0x4
    unsigned char __align3[4];// Offset=0x3c Size=0x4
    struct sceGsPrmodecont prmodecont;// Offset=0x40 Size=0x8
    long prmodecontaddr;// Offset=0x48 Size=0x4
    unsigned char __align4[4];// Offset=0x4c Size=0x4
    struct sceGsColclamp colclamp;// Offset=0x50 Size=0x8
    long colclampaddr;// Offset=0x58 Size=0x4
    unsigned char __align5[4];// Offset=0x5c Size=0x4
    struct sceGsDthe dthe;// Offset=0x60 Size=0x8
    long dtheaddr;// Offset=0x68 Size=0x4
    unsigned char __align6[4];// Offset=0x6c Size=0x4
    struct sceGsTest test1;// Offset=0x70 Size=0x8
    long test1addr;// Offset=0x78 Size=0x4
};

struct sceGsFrame// Size=0x8 (Id=3302)
{
    struct // Size=0x8 (Id=0)
    {
        unsigned long long FBP:9;// Offset=0x0 Size=0x8 BitOffset=0x0 BitSize=0x9
        unsigned long long pad09:7;// Offset=0x0 Size=0x8 BitOffset=0x9 BitSize=0x7
        unsigned long long FBW:6;// Offset=0x0 Size=0x8 BitOffset=0x10 BitSize=0x6
        unsigned long long pad22:2;// Offset=0x0 Size=0x8 BitOffset=0x16 BitSize=0x2
        unsigned long long PSM:6;// Offset=0x0 Size=0x8 BitOffset=0x18 BitSize=0x6
        unsigned long long pad30:2;// Offset=0x0 Size=0x8 BitOffset=0x1e BitSize=0x2
        unsigned long long FBMSK:32;// Offset=0x0 Size=0x8 BitOffset=0x20 BitSize=0x20
    };
};

struct sceGsZbuf// Size=0x8 (Id=3303)
{
    struct // Size=0x8 (Id=0)
    {
        unsigned long long ZBP:9;// Offset=0x0 Size=0x8 BitOffset=0x0 BitSize=0x9
        unsigned long long pad09:15;// Offset=0x0 Size=0x8 BitOffset=0x9 BitSize=0xf
        unsigned long long PSM:4;// Offset=0x0 Size=0x8 BitOffset=0x18 BitSize=0x4
        unsigned long long pad28:4;// Offset=0x0 Size=0x8 BitOffset=0x1c BitSize=0x4
        unsigned long long ZMSK:1;// Offset=0x0 Size=0x8 BitOffset=0x20 BitSize=0x1
        unsigned long long pad33:31;// Offset=0x0 Size=0x8 BitOffset=0x21 BitSize=0x1f
    };
};

struct sceGsXyoffset// Size=0x8 (Id=3304)
{
    struct // Size=0x8 (Id=0)
    {
        unsigned long long OFX:16;// Offset=0x0 Size=0x8 BitOffset=0x0 BitSize=0x10
        unsigned long long pad16:16;// Offset=0x0 Size=0x8 BitOffset=0x10 BitSize=0x10
        unsigned long long OFY:16;// Offset=0x0 Size=0x8 BitOffset=0x20 BitSize=0x10
        unsigned long long pad48:16;// Offset=0x0 Size=0x8 BitOffset=0x30 BitSize=0x10
    };
};

struct sceGsScissor// Size=0x8 (Id=3305)
{
    struct // Size=0x8 (Id=0)
    {
        unsigned long long SCAX0:11;// Offset=0x0 Size=0x8 BitOffset=0x0 BitSize=0xb
        unsigned long long pad11:5;// Offset=0x0 Size=0x8 BitOffset=0xb BitSize=0x5
        unsigned long long SCAX1:11;// Offset=0x0 Size=0x8 BitOffset=0x10 BitSize=0xb
        unsigned long long pad27:5;// Offset=0x0 Size=0x8 BitOffset=0x1b BitSize=0x5
        unsigned long long SCAY0:11;// Offset=0x0 Size=0x8 BitOffset=0x20 BitSize=0xb
        unsigned long long pad43:5;// Offset=0x0 Size=0x8 BitOffset=0x2b BitSize=0x5
        unsigned long long SCAY1:11;// Offset=0x0 Size=0x8 BitOffset=0x30 BitSize=0xb
        unsigned long long pad59:5;// Offset=0x0 Size=0x8 BitOffset=0x3b BitSize=0x5
    };
};

struct sceGsPrmodecont// Size=0x8 (Id=3306)
{
    struct // Size=0x8 (Id=0)
    {
        unsigned long long AC:1;// Offset=0x0 Size=0x8 BitOffset=0x0 BitSize=0x1
        unsigned long long pad01:63;// Offset=0x0 Size=0x8 BitOffset=0x1 BitSize=0x3f
    };
};

struct sceGsColclamp// Size=0x8 (Id=3307)
{
    struct // Size=0x8 (Id=0)
    {
        unsigned long long CLAMP:1;// Offset=0x0 Size=0x8 BitOffset=0x0 BitSize=0x1
        unsigned long long pad01:63;// Offset=0x0 Size=0x8 BitOffset=0x1 BitSize=0x3f
    };
};

struct sceGsDthe// Size=0x8 (Id=3308)
{
    struct // Size=0x8 (Id=0)
    {
        unsigned long long DTHE:1;// Offset=0x0 Size=0x8 BitOffset=0x0 BitSize=0x1
        unsigned long long pad01:63;// Offset=0x0 Size=0x8 BitOffset=0x1 BitSize=0x3f
    };
};

struct sceGsTest// Size=0x8 (Id=3309)
{
    struct // Size=0x8 (Id=0)
    {
        unsigned long long ATE:1;// Offset=0x0 Size=0x8 BitOffset=0x0 BitSize=0x1
        unsigned long long ATST:3;// Offset=0x0 Size=0x8 BitOffset=0x1 BitSize=0x3
        unsigned long long AREF:8;// Offset=0x0 Size=0x8 BitOffset=0x4 BitSize=0x8
        unsigned long long AFAIL:2;// Offset=0x0 Size=0x8 BitOffset=0xc BitSize=0x2
        unsigned long long DATE:1;// Offset=0x0 Size=0x8 BitOffset=0xe BitSize=0x1
        unsigned long long DATM:1;// Offset=0x0 Size=0x8 BitOffset=0xf BitSize=0x1
        unsigned long long ZTE:1;// Offset=0x0 Size=0x8 BitOffset=0x10 BitSize=0x1
        unsigned long long ZTST:2;// Offset=0x0 Size=0x8 BitOffset=0x11 BitSize=0x2
        unsigned long long pad19:45;// Offset=0x0 Size=0x8 BitOffset=0x13 BitSize=0x2d
    };
};

struct sceGsTrxreg// Size=0x8 (Id=3318)
{
    struct // Size=0x8 (Id=0)
    {
        unsigned long long RRW:12;// Offset=0x0 Size=0x8 BitOffset=0x0 BitSize=0xc
        unsigned long long pad12:20;// Offset=0x0 Size=0x8 BitOffset=0xc BitSize=0x14
        unsigned long long RRH:12;// Offset=0x0 Size=0x8 BitOffset=0x20 BitSize=0xc
        unsigned long long pad44:20;// Offset=0x0 Size=0x8 BitOffset=0x2c BitSize=0x14
    };
};

struct tGS_PMODE// Size=0x8 (Id=3320)
{
    struct // Size=0x4 (Id=0)
    {
        unsigned int EN1:1;// Offset=0x0 Size=0x4 BitOffset=0x0 BitSize=0x1
        unsigned int EN2:1;// Offset=0x0 Size=0x4 BitOffset=0x1 BitSize=0x1
        unsigned int CRTMD:3;// Offset=0x0 Size=0x4 BitOffset=0x2 BitSize=0x3
        unsigned int MMOD:1;// Offset=0x0 Size=0x4 BitOffset=0x5 BitSize=0x1
        unsigned int AMOD:1;// Offset=0x0 Size=0x4 BitOffset=0x6 BitSize=0x1
        unsigned int SLBG:1;// Offset=0x0 Size=0x4 BitOffset=0x7 BitSize=0x1
        unsigned int ALP:8;// Offset=0x0 Size=0x4 BitOffset=0x8 BitSize=0x8
        unsigned int p0:16;// Offset=0x0 Size=0x4 BitOffset=0x10 BitSize=0x10
    };
    unsigned int p1;// Offset=0x4 Size=0x4
};

struct tGS_SMODE2// Size=0x8 (Id=3321)
{
    struct // Size=0x4 (Id=0)
    {
        unsigned int INT:1;// Offset=0x0 Size=0x4 BitOffset=0x0 BitSize=0x1
        unsigned int FFMD:1;// Offset=0x0 Size=0x4 BitOffset=0x1 BitSize=0x1
        unsigned int DPMS:2;// Offset=0x0 Size=0x4 BitOffset=0x2 BitSize=0x2
        unsigned int p0:28;// Offset=0x0 Size=0x4 BitOffset=0x4 BitSize=0x1c
    };
    unsigned int p1;// Offset=0x4 Size=0x4
};

struct tGS_DISPFB2// Size=0x8 (Id=3322)
{
    struct // Size=0x8 (Id=0)
    {
        unsigned int FBP:9;// Offset=0x0 Size=0x4 BitOffset=0x0 BitSize=0x9
        unsigned int FBW:6;// Offset=0x0 Size=0x4 BitOffset=0x9 BitSize=0x6
        unsigned int PSM:5;// Offset=0x0 Size=0x4 BitOffset=0xf BitSize=0x5
        unsigned int p0:12;// Offset=0x0 Size=0x4 BitOffset=0x14 BitSize=0xc
        unsigned int DBX:11;// Offset=0x4 Size=0x4 BitOffset=0x0 BitSize=0xb
        unsigned int DBY:11;// Offset=0x4 Size=0x4 BitOffset=0xb BitSize=0xb
        unsigned int p1:10;// Offset=0x4 Size=0x4 BitOffset=0x16 BitSize=0xa
    };
};

struct tGS_BGCOLOR// Size=0x8 (Id=3323)
{
    struct // Size=0x4 (Id=0)
    {
        unsigned int R:8;// Offset=0x0 Size=0x4 BitOffset=0x0 BitSize=0x8
        unsigned int G:8;// Offset=0x0 Size=0x4 BitOffset=0x8 BitSize=0x8
        unsigned int B:8;// Offset=0x0 Size=0x4 BitOffset=0x10 BitSize=0x8
        unsigned int p0:8;// Offset=0x0 Size=0x4 BitOffset=0x18 BitSize=0x8
    };
    unsigned int p1;// Offset=0x4 Size=0x4
};

struct sceGsAlphaEnv2// Size=0x40 (Id=3341)
{
    struct sceGsAlpha alpha2;// Offset=0x0 Size=0x8
    long alpha2addr;// Offset=0x8 Size=0x4
    unsigned char __align0[4];// Offset=0xc Size=0x4
    struct sceGsPabe pabe;// Offset=0x10 Size=0x8
    long pabeaddr;// Offset=0x18 Size=0x4
    unsigned char __align1[4];// Offset=0x1c Size=0x4
    struct sceGsTexa texa;// Offset=0x20 Size=0x8
    long texaaddr;// Offset=0x28 Size=0x4
    unsigned char __align2[4];// Offset=0x2c Size=0x4
    struct sceGsFba fba2;// Offset=0x30 Size=0x8
    long fba2addr;// Offset=0x38 Size=0x4
};

struct sceGsAlpha// Size=0x8 (Id=3342)
{
    struct // Size=0x8 (Id=0)
    {
        unsigned long long A:2;// Offset=0x0 Size=0x8 BitOffset=0x0 BitSize=0x2
        unsigned long long B:2;// Offset=0x0 Size=0x8 BitOffset=0x2 BitSize=0x2
        unsigned long long C:2;// Offset=0x0 Size=0x8 BitOffset=0x4 BitSize=0x2
        unsigned long long D:2;// Offset=0x0 Size=0x8 BitOffset=0x6 BitSize=0x2
        unsigned long long pad8:24;// Offset=0x0 Size=0x8 BitOffset=0x8 BitSize=0x18
        unsigned long long FIX:8;// Offset=0x0 Size=0x8 BitOffset=0x20 BitSize=0x8
        unsigned long long pad40:24;// Offset=0x0 Size=0x8 BitOffset=0x28 BitSize=0x18
    };
};

struct sceGsPabe// Size=0x8 (Id=3343)
{
    struct // Size=0x8 (Id=0)
    {
        unsigned long long PABE:1;// Offset=0x0 Size=0x8 BitOffset=0x0 BitSize=0x1
        unsigned long long pad01:63;// Offset=0x0 Size=0x8 BitOffset=0x1 BitSize=0x3f
    };
};

struct sceGsTexa// Size=0x8 (Id=3344)
{
    struct // Size=0x8 (Id=0)
    {
        unsigned long long TA0:8;// Offset=0x0 Size=0x8 BitOffset=0x0 BitSize=0x8
        unsigned long long pad08:7;// Offset=0x0 Size=0x8 BitOffset=0x8 BitSize=0x7
        unsigned long long AEM:1;// Offset=0x0 Size=0x8 BitOffset=0xf BitSize=0x1
        unsigned long long pad16:16;// Offset=0x0 Size=0x8 BitOffset=0x10 BitSize=0x10
        unsigned long long TA1:8;// Offset=0x0 Size=0x8 BitOffset=0x20 BitSize=0x8
        unsigned long long pad40:24;// Offset=0x0 Size=0x8 BitOffset=0x28 BitSize=0x18
    };
};

struct sceGsFba// Size=0x8 (Id=3345)
{
    struct // Size=0x8 (Id=0)
    {
        unsigned long long FBA:1;// Offset=0x0 Size=0x8 BitOffset=0x0 BitSize=0x1
        unsigned long long pad01:63;// Offset=0x0 Size=0x8 BitOffset=0x1 BitSize=0x3f
    };
};

struct sceGsTrxpos// Size=0x8 (Id=3346)
{
    struct // Size=0x8 (Id=0)
    {
        unsigned long long SSAX:11;// Offset=0x0 Size=0x8 BitOffset=0x0 BitSize=0xb
        unsigned long long pad11:5;// Offset=0x0 Size=0x8 BitOffset=0xb BitSize=0x5
        unsigned long long SSAY:11;// Offset=0x0 Size=0x8 BitOffset=0x10 BitSize=0xb
        unsigned long long pad27:5;// Offset=0x0 Size=0x8 BitOffset=0x1b BitSize=0x5
        unsigned long long DSAX:11;// Offset=0x0 Size=0x8 BitOffset=0x20 BitSize=0xb
        unsigned long long pad43:5;// Offset=0x0 Size=0x8 BitOffset=0x2b BitSize=0x5
        unsigned long long DSAY:11;// Offset=0x0 Size=0x8 BitOffset=0x30 BitSize=0xb
        unsigned long long DIR:2;// Offset=0x0 Size=0x8 BitOffset=0x3b BitSize=0x2
        unsigned long long pad61:3;// Offset=0x0 Size=0x8 BitOffset=0x3d BitSize=0x3
    };
};

struct sceGsStoreImage// Size=0x70 (Id=3367)
{
    unsigned int vifcode[4];// Offset=0x0 Size=0x10
    struct sceGifTag giftag;// Offset=0x10 Size=0x10
    struct sceGsBitbltbuf bitbltbuf;// Offset=0x20 Size=0x8
    long bitbltbufaddr;// Offset=0x28 Size=0x4
    unsigned char __align0[4];// Offset=0x2c Size=0x4
    struct sceGsTrxpos trxpos;// Offset=0x30 Size=0x8
    long trxposaddr;// Offset=0x38 Size=0x4
    unsigned char __align1[4];// Offset=0x3c Size=0x4
    struct sceGsTrxreg trxreg;// Offset=0x40 Size=0x8
    long trxregaddr;// Offset=0x48 Size=0x4
    unsigned char __align2[4];// Offset=0x4c Size=0x4
    struct sceGsFinish finish;// Offset=0x50 Size=0x8
    long finishaddr;// Offset=0x58 Size=0x4
    unsigned char __align3[4];// Offset=0x5c Size=0x4
    struct sceGsTrxdir trxdir;// Offset=0x60 Size=0x8
    long trxdiraddr;// Offset=0x68 Size=0x4
};

struct sceGifTag// Size=0x10 (Id=3368)
{
    struct // Size=0x10 (Id=0)
    {
        unsigned long long NLOOP:15;// Offset=0x0 Size=0x8 BitOffset=0x0 BitSize=0xf
        unsigned long long EOP:1;// Offset=0x0 Size=0x8 BitOffset=0xf BitSize=0x1
        unsigned long long pad16:16;// Offset=0x0 Size=0x8 BitOffset=0x10 BitSize=0x10
        unsigned long long id:14;// Offset=0x0 Size=0x8 BitOffset=0x20 BitSize=0xe
        unsigned long long PRE:1;// Offset=0x0 Size=0x8 BitOffset=0x2e BitSize=0x1
        unsigned long long PRIM:11;// Offset=0x0 Size=0x8 BitOffset=0x2f BitSize=0xb
        unsigned long long FLG:2;// Offset=0x0 Size=0x8 BitOffset=0x3a BitSize=0x2
        unsigned long long NREG:4;// Offset=0x0 Size=0x8 BitOffset=0x3c BitSize=0x4
        unsigned long long REGS0:4;// Offset=0x8 Size=0x8 BitOffset=0x0 BitSize=0x4
        unsigned long long REGS1:4;// Offset=0x8 Size=0x8 BitOffset=0x4 BitSize=0x4
        unsigned long long REGS2:4;// Offset=0x8 Size=0x8 BitOffset=0x8 BitSize=0x4
        unsigned long long REGS3:4;// Offset=0x8 Size=0x8 BitOffset=0xc BitSize=0x4
        unsigned long long REGS4:4;// Offset=0x8 Size=0x8 BitOffset=0x10 BitSize=0x4
        unsigned long long REGS5:4;// Offset=0x8 Size=0x8 BitOffset=0x14 BitSize=0x4
        unsigned long long REGS6:4;// Offset=0x8 Size=0x8 BitOffset=0x18 BitSize=0x4
        unsigned long long REGS7:4;// Offset=0x8 Size=0x8 BitOffset=0x1c BitSize=0x4
        unsigned long long REGS8:4;// Offset=0x8 Size=0x8 BitOffset=0x20 BitSize=0x4
        unsigned long long REGS9:4;// Offset=0x8 Size=0x8 BitOffset=0x24 BitSize=0x4
        unsigned long long REGS10:4;// Offset=0x8 Size=0x8 BitOffset=0x28 BitSize=0x4
        unsigned long long REGS11:4;// Offset=0x8 Size=0x8 BitOffset=0x2c BitSize=0x4
        unsigned long long REGS12:4;// Offset=0x8 Size=0x8 BitOffset=0x30 BitSize=0x4
        unsigned long long REGS13:4;// Offset=0x8 Size=0x8 BitOffset=0x34 BitSize=0x4
        unsigned long long REGS14:4;// Offset=0x8 Size=0x8 BitOffset=0x38 BitSize=0x4
        unsigned long long REGS15:4;// Offset=0x8 Size=0x8 BitOffset=0x3c BitSize=0x4
    };
};

struct sceGsFinish// Size=0x8 (Id=3369)
{
    unsigned long long pad00;// Offset=0x0 Size=0x8
};

struct sceGsTrxdir// Size=0x8 (Id=3370)
{
    struct // Size=0x8 (Id=0)
    {
        unsigned long long XDR:2;// Offset=0x0 Size=0x8 BitOffset=0x0 BitSize=0x2
        unsigned long long pad02:62;// Offset=0x0 Size=0x8 BitOffset=0x2 BitSize=0x3e
    };
};

struct sceGsAlphaEnv// Size=0x40 (Id=3371)
{
    struct sceGsAlpha alpha1;// Offset=0x0 Size=0x8
    long alpha1addr;// Offset=0x8 Size=0x4
    unsigned char __align0[4];// Offset=0xc Size=0x4
    struct sceGsPabe pabe;// Offset=0x10 Size=0x8
    long pabeaddr;// Offset=0x18 Size=0x4
    unsigned char __align1[4];// Offset=0x1c Size=0x4
    struct sceGsTexa texa;// Offset=0x20 Size=0x8
    long texaaddr;// Offset=0x28 Size=0x4
    unsigned char __align2[4];// Offset=0x2c Size=0x4
    struct sceGsFba fba1;// Offset=0x30 Size=0x8
    long fba1addr;// Offset=0x38 Size=0x4
};

struct sceGsTexEnv2// Size=0x40 (Id=3372)
{
    struct sceGsTexflush texflush;// Offset=0x0 Size=0x8
    long texflushaddr;// Offset=0x8 Size=0x4
    unsigned char __align0[4];// Offset=0xc Size=0x4
    struct sceGsTex1 tex12;// Offset=0x10 Size=0x8
    long tex12addr;// Offset=0x18 Size=0x4
    unsigned char __align1[4];// Offset=0x1c Size=0x4
    struct sceGsTex0 tex02;// Offset=0x20 Size=0x8
    long tex02addr;// Offset=0x28 Size=0x4
    unsigned char __align2[4];// Offset=0x2c Size=0x4
    struct sceGsClamp clamp2;// Offset=0x30 Size=0x8
    long clamp2addr;// Offset=0x38 Size=0x4
};

struct sceGsTexflush// Size=0x8 (Id=3373)
{
    unsigned long long pad00;// Offset=0x0 Size=0x8
};

struct sceGsTex0// Size=0x8 (Id=3374)
{
    struct // Size=0x8 (Id=0)
    {
        unsigned long long TBP0:14;// Offset=0x0 Size=0x8 BitOffset=0x0 BitSize=0xe
        unsigned long long TBW:6;// Offset=0x0 Size=0x8 BitOffset=0xe BitSize=0x6
        unsigned long long PSM:6;// Offset=0x0 Size=0x8 BitOffset=0x14 BitSize=0x6
        unsigned long long TW:4;// Offset=0x0 Size=0x8 BitOffset=0x1a BitSize=0x4
        unsigned long long TH:4;// Offset=0x0 Size=0x8 BitOffset=0x1e BitSize=0x4
        unsigned long long TCC:1;// Offset=0x0 Size=0x8 BitOffset=0x22 BitSize=0x1
        unsigned long long TFX:2;// Offset=0x0 Size=0x8 BitOffset=0x23 BitSize=0x2
        unsigned long long CBP:14;// Offset=0x0 Size=0x8 BitOffset=0x25 BitSize=0xe
        unsigned long long CPSM:4;// Offset=0x0 Size=0x8 BitOffset=0x33 BitSize=0x4
        unsigned long long CSM:1;// Offset=0x0 Size=0x8 BitOffset=0x37 BitSize=0x1
        unsigned long long CSA:5;// Offset=0x0 Size=0x8 BitOffset=0x38 BitSize=0x5
        unsigned long long CLD:3;// Offset=0x0 Size=0x8 BitOffset=0x3d BitSize=0x3
    };
};

struct sceGsClamp// Size=0x8 (Id=3375)
{
    struct // Size=0x8 (Id=0)
    {
        unsigned long long WMS:2;// Offset=0x0 Size=0x8 BitOffset=0x0 BitSize=0x2
        unsigned long long WMT:2;// Offset=0x0 Size=0x8 BitOffset=0x2 BitSize=0x2
        unsigned long long MINU:10;// Offset=0x0 Size=0x8 BitOffset=0x4 BitSize=0xa
        unsigned long long MAXU:10;// Offset=0x0 Size=0x8 BitOffset=0xe BitSize=0xa
        unsigned long long MINV:10;// Offset=0x0 Size=0x8 BitOffset=0x18 BitSize=0xa
        unsigned long long MAXV:10;// Offset=0x0 Size=0x8 BitOffset=0x22 BitSize=0xa
        unsigned long long pad44:20;// Offset=0x0 Size=0x8 BitOffset=0x2c BitSize=0x14
    };
};

struct sceGsTexEnv// Size=0x40 (Id=3376)
{
    struct sceGsTexflush texflush;// Offset=0x0 Size=0x8
    long texflushaddr;// Offset=0x8 Size=0x4
    unsigned char __align0[4];// Offset=0xc Size=0x4
    struct sceGsTex1 tex11;// Offset=0x10 Size=0x8
    long tex11addr;// Offset=0x18 Size=0x4
    unsigned char __align1[4];// Offset=0x1c Size=0x4
    struct sceGsTex0 tex01;// Offset=0x20 Size=0x8
    long tex01addr;// Offset=0x28 Size=0x4
    unsigned char __align2[4];// Offset=0x2c Size=0x4
    struct sceGsClamp clamp1;// Offset=0x30 Size=0x8
    long clamp1addr;// Offset=0x38 Size=0x4
};

struct sceGsDrawEnv2// Size=0x80 (Id=3377)
{
    struct sceGsFrame frame2;// Offset=0x0 Size=0x8
    unsigned long frame2addr;// Offset=0x8 Size=0x4
    unsigned char __align0[4];// Offset=0xc Size=0x4
    struct sceGsZbuf zbuf2;// Offset=0x10 Size=0x8
    long zbuf2addr;// Offset=0x18 Size=0x4
    unsigned char __align1[4];// Offset=0x1c Size=0x4
    struct sceGsXyoffset xyoffset2;// Offset=0x20 Size=0x8
    long xyoffset2addr;// Offset=0x28 Size=0x4
    unsigned char __align2[4];// Offset=0x2c Size=0x4
    struct sceGsScissor scissor2;// Offset=0x30 Size=0x8
    long scissor2addr;// Offset=0x38 Size=0x4
    unsigned char __align3[4];// Offset=0x3c Size=0x4
    struct sceGsPrmodecont prmodecont;// Offset=0x40 Size=0x8
    long prmodecontaddr;// Offset=0x48 Size=0x4
    unsigned char __align4[4];// Offset=0x4c Size=0x4
    struct sceGsColclamp colclamp;// Offset=0x50 Size=0x8
    long colclampaddr;// Offset=0x58 Size=0x4
    unsigned char __align5[4];// Offset=0x5c Size=0x4
    struct sceGsDthe dthe;// Offset=0x60 Size=0x8
    long dtheaddr;// Offset=0x68 Size=0x4
    unsigned char __align6[4];// Offset=0x6c Size=0x4
    struct sceGsTest test2;// Offset=0x70 Size=0x8
    long test2addr;// Offset=0x78 Size=0x4
};

struct sceGsClear// Size=0x58 (Id=3378)
{
    struct sceGsTest testa;// Offset=0x0 Size=0x8
    long testaaddr;// Offset=0x8 Size=0x4
    unsigned char __align0[4];// Offset=0xc Size=0x4
    struct sceGsPrim prim;// Offset=0x10 Size=0x8
    long primaddr;// Offset=0x18 Size=0x4
    struct sceGsRgbaq rgbaq;// Offset=0x1c Size=0x8
    long rgbaqaddr;// Offset=0x24 Size=0x4
    struct sceGsXyz xyz2a;// Offset=0x28 Size=0x8
    long xyz2aaddr;// Offset=0x30 Size=0x4
    unsigned char __align1[4];// Offset=0x34 Size=0x4
    struct sceGsXyz xyz2b;// Offset=0x38 Size=0x8
    long xyz2baddr;// Offset=0x40 Size=0x4
    unsigned char __align2[4];// Offset=0x44 Size=0x4
    struct sceGsTest testb;// Offset=0x48 Size=0x8
    long testbaddr;// Offset=0x50 Size=0x4
};

struct sceGsPrim// Size=0x8 (Id=3379)
{
    struct // Size=0x8 (Id=0)
    {
        unsigned long long PRIM:3;// Offset=0x0 Size=0x8 BitOffset=0x0 BitSize=0x3
        unsigned long long IIP:1;// Offset=0x0 Size=0x8 BitOffset=0x3 BitSize=0x1
        unsigned long long TME:1;// Offset=0x0 Size=0x8 BitOffset=0x4 BitSize=0x1
        unsigned long long FGE:1;// Offset=0x0 Size=0x8 BitOffset=0x5 BitSize=0x1
        unsigned long long ABE:1;// Offset=0x0 Size=0x8 BitOffset=0x6 BitSize=0x1
        unsigned long long AA1:1;// Offset=0x0 Size=0x8 BitOffset=0x7 BitSize=0x1
        unsigned long long FST:1;// Offset=0x0 Size=0x8 BitOffset=0x8 BitSize=0x1
        unsigned long long CTXT:1;// Offset=0x0 Size=0x8 BitOffset=0x9 BitSize=0x1
        unsigned long long FIX:1;// Offset=0x0 Size=0x8 BitOffset=0xa BitSize=0x1
        unsigned long long pad11:53;// Offset=0x0 Size=0x8 BitOffset=0xb BitSize=0x35
    };
};

struct sceGsRgbaq// Size=0x8 (Id=3380)
{
    struct // Size=0x4 (Id=0)
    {
        unsigned int R:8;// Offset=0x0 Size=0x4 BitOffset=0x0 BitSize=0x8
        unsigned int G:8;// Offset=0x0 Size=0x4 BitOffset=0x8 BitSize=0x8
        unsigned int B:8;// Offset=0x0 Size=0x4 BitOffset=0x10 BitSize=0x8
        unsigned int A:8;// Offset=0x0 Size=0x4 BitOffset=0x18 BitSize=0x8
    };
    float Q;// Offset=0x4 Size=0x4
};

struct sceGsXyz// Size=0x8 (Id=3381)
{
    struct // Size=0x8 (Id=0)
    {
        unsigned long long X:16;// Offset=0x0 Size=0x8 BitOffset=0x0 BitSize=0x10
        unsigned long long Y:16;// Offset=0x0 Size=0x8 BitOffset=0x10 BitSize=0x10
        unsigned long long Z:32;// Offset=0x0 Size=0x8 BitOffset=0x20 BitSize=0x20
    };
};

struct sceExecData// Size=0x10 (Id=3408)
{
    unsigned int epc;// Offset=0x0 Size=0x4
    unsigned int gp;// Offset=0x4 Size=0x4
    unsigned int sp;// Offset=0x8 Size=0x4
    unsigned int dummy;// Offset=0xc Size=0x4
};

struct sce_stat// Size=0x40 (Id=3418)
{
    unsigned int st_mode;// Offset=0x0 Size=0x4
    unsigned int st_attr;// Offset=0x4 Size=0x4
    unsigned int st_size;// Offset=0x8 Size=0x4
    unsigned char st_ctime[8];// Offset=0xc Size=0x8
    unsigned char st_atime[8];// Offset=0x14 Size=0x8
    unsigned char st_mtime[8];// Offset=0x1c Size=0x8
    unsigned int st_hisize;// Offset=0x24 Size=0x4
    unsigned int st_private[6];// Offset=0x28 Size=0x18
};

struct sce_dirent// Size=0x144 (Id=3423)
{
    struct sce_stat d_stat;// Offset=0x0 Size=0x40
    char d_name[256];// Offset=0x40 Size=0x100
    void * d_private;// Offset=0x140 Size=0x4
};

struct sceVif1Packet// Size=0x20 (Id=3438)
{
    unsigned int * pCurrent;// Offset=0x0 Size=0x4
    unsigned int * pBase;// Offset=0x4 Size=0x4
    unsigned int * pDmaTag;// Offset=0x8 Size=0x4
    unsigned int * pVifCode;// Offset=0xc Size=0x4
    unsigned int numlen;// Offset=0x10 Size=0x4
    unsigned long * pGifTag;// Offset=0x14 Size=0x4
    unsigned int pad12;// Offset=0x18 Size=0x4
    unsigned int pad13;// Offset=0x1c Size=0x4
};

struct sceDevGifCnd// Size=0x18 (Id=3442)
{
    unsigned int tag;// Offset=0x0 Size=0x4
    unsigned int stat;// Offset=0x4 Size=0x4
    unsigned int count;// Offset=0x8 Size=0x4
    unsigned int p3count;// Offset=0xc Size=0x4
    unsigned int p3tag;// Offset=0x10 Size=0x4
    unsigned int pad;// Offset=0x14 Size=0x4
};

struct sceDmaChan// Size=0x90 (Id=3443)
{
    struct tD_CHCR chcr;// Offset=0x0 Size=0x4
    unsigned int p0[3];// Offset=0x4 Size=0xc
    void * madr;// Offset=0x10 Size=0x4
    unsigned int p1[3];// Offset=0x14 Size=0xc
    unsigned int qwc;// Offset=0x20 Size=0x4
    unsigned int p2[3];// Offset=0x24 Size=0xc
    struct _sceDmaTag * tadr;// Offset=0x30 Size=0x4
    unsigned int p3[3];// Offset=0x34 Size=0xc
    void * as0;// Offset=0x40 Size=0x4
    unsigned int p4[3];// Offset=0x44 Size=0xc
    void * as1;// Offset=0x50 Size=0x4
    unsigned int p5[3];// Offset=0x54 Size=0xc
    unsigned int p6[4];// Offset=0x60 Size=0x10
    unsigned int p7[4];// Offset=0x70 Size=0x10
    void * sadr;// Offset=0x80 Size=0x4
    unsigned int p8[3];// Offset=0x84 Size=0xc
};

struct sceDevVif1Cnd// Size=0x44 (Id=3444)
{
    unsigned int row[4];// Offset=0x0 Size=0x10
    unsigned int col[4];// Offset=0x10 Size=0x10
    unsigned int mask;// Offset=0x20 Size=0x4
    unsigned int code;// Offset=0x24 Size=0x4
    unsigned int stat;// Offset=0x28 Size=0x4
    unsigned short itop;// Offset=0x2c Size=0x2
    unsigned short itops;// Offset=0x2e Size=0x2
    unsigned short base;// Offset=0x30 Size=0x2
    unsigned short offset;// Offset=0x32 Size=0x2
    unsigned short top;// Offset=0x34 Size=0x2
    unsigned short tops;// Offset=0x36 Size=0x2
    unsigned short mark;// Offset=0x38 Size=0x2
    unsigned short num;// Offset=0x3a Size=0x2
    unsigned char error;// Offset=0x3c Size=0x1
    unsigned char cl;// Offset=0x3d Size=0x1
    unsigned char wl;// Offset=0x3e Size=0x1
    unsigned char cmod;// Offset=0x3f Size=0x1
    unsigned char pad;// Offset=0x40 Size=0x1
};

struct sceDevVif0Cnd// Size=0x3c (Id=3445)
{
    unsigned int row[4];// Offset=0x0 Size=0x10
    unsigned int col[4];// Offset=0x10 Size=0x10
    unsigned int mask;// Offset=0x20 Size=0x4
    unsigned int code;// Offset=0x24 Size=0x4
    unsigned int stat;// Offset=0x28 Size=0x4
    unsigned short itop;// Offset=0x2c Size=0x2
    unsigned short itops;// Offset=0x2e Size=0x2
    unsigned short mark;// Offset=0x30 Size=0x2
    unsigned short num;// Offset=0x32 Size=0x2
    unsigned char error;// Offset=0x34 Size=0x1
    unsigned char cl;// Offset=0x35 Size=0x1
    unsigned char wl;// Offset=0x36 Size=0x1
    unsigned char cmod;// Offset=0x37 Size=0x1
    unsigned char pad;// Offset=0x38 Size=0x1
};

struct sceDevVu1Cnd// Size=0xbc (Id=3446)
{
    unsigned int vf[32];// Offset=0x0 Size=0x80
    unsigned int status;// Offset=0x80 Size=0x4
    unsigned int mac;// Offset=0x84 Size=0x4
    unsigned int clipping;// Offset=0x88 Size=0x4
    unsigned int r;// Offset=0x8c Size=0x4
    unsigned int i;// Offset=0x90 Size=0x4
    unsigned int q;// Offset=0x94 Size=0x4
    unsigned int p;// Offset=0x98 Size=0x4
    unsigned short vi[16];// Offset=0x9c Size=0x20
};

struct sceDevVu0Cnd// Size=0xb8 (Id=3447)
{
    unsigned int vf[32];// Offset=0x0 Size=0x80
    unsigned int status;// Offset=0x80 Size=0x4
    unsigned int mac;// Offset=0x84 Size=0x4
    unsigned int clipping;// Offset=0x88 Size=0x4
    unsigned int r;// Offset=0x8c Size=0x4
    unsigned int i;// Offset=0x90 Size=0x4
    unsigned int q;// Offset=0x94 Size=0x4
    unsigned short vi[16];// Offset=0x98 Size=0x20
};

struct sceVif0Packet// Size=0x20 (Id=3448)
{
    unsigned int * pCurrent;// Offset=0x0 Size=0x4
    unsigned int * pBase;// Offset=0x4 Size=0x4
    unsigned int * pDmaTag;// Offset=0x8 Size=0x4
    unsigned int * pVifCode;// Offset=0xc Size=0x4
    unsigned int numlen;// Offset=0x10 Size=0x4
    unsigned int pad11;// Offset=0x14 Size=0x4
    unsigned int pad12;// Offset=0x18 Size=0x4
    unsigned int pad13;// Offset=0x1c Size=0x4
};

struct sceDmaEnv// Size=0x14 (Id=3449)
{
    unsigned char sts;// Offset=0x0 Size=0x1
    unsigned char std;// Offset=0x1 Size=0x1
    unsigned char mfd;// Offset=0x2 Size=0x1
    unsigned char rcycle;// Offset=0x3 Size=0x1
    unsigned short express;// Offset=0x4 Size=0x2
    unsigned short notify;// Offset=0x6 Size=0x2
    unsigned short sqwc;// Offset=0x8 Size=0x2
    unsigned short tqwc;// Offset=0xa Size=0x2
    void * rbadr;// Offset=0xc Size=0x4
    unsigned int rbmsk;// Offset=0x10 Size=0x4
};

struct sceSifCmdHdr// Size=0x10 (Id=3472)
{
    struct // Size=0x4 (Id=0)
    {
        unsigned int psize:8;// Offset=0x0 Size=0x4 BitOffset=0x0 BitSize=0x8
        unsigned int dsize:24;// Offset=0x0 Size=0x4 BitOffset=0x8 BitSize=0x18
    };
    unsigned int daddr;// Offset=0x4 Size=0x4
    unsigned int fcode;// Offset=0x8 Size=0x4
    unsigned int opt;// Offset=0xc Size=0x4
};

struct sceSifCmdData// Size=0x8 (Id=3475)
{
    void  ( * func)(void * ,void * );// Offset=0x0 Size=0x4
    void * data;// Offset=0x4 Size=0x4
};

struct sceCdCLOCK// Size=0x8 (Id=3499)
{
    unsigned char stat;// Offset=0x0 Size=0x1
    unsigned char second;// Offset=0x1 Size=0x1
    unsigned char minute;// Offset=0x2 Size=0x1
    unsigned char hour;// Offset=0x3 Size=0x1
    unsigned char pad;// Offset=0x4 Size=0x1
    unsigned char day;// Offset=0x5 Size=0x1
    unsigned char month;// Offset=0x6 Size=0x1
    unsigned char year;// Offset=0x7 Size=0x1
};

struct sceCdlFILE// Size=0x24 (Id=3500)
{
    unsigned int lsn;// Offset=0x0 Size=0x4
    unsigned int size;// Offset=0x4 Size=0x4
    char name[16];// Offset=0x8 Size=0x10
    unsigned char date[8];// Offset=0x18 Size=0x8
    unsigned int flag;// Offset=0x20 Size=0x4
};

struct sceCdlLOCCD// Size=0x4 (Id=3501)
{
    unsigned char minute;// Offset=0x0 Size=0x1
    unsigned char second;// Offset=0x1 Size=0x1
    unsigned char sector;// Offset=0x2 Size=0x1
    unsigned char track;// Offset=0x3 Size=0x1
};

struct sceMpeg// Size=0x28 (Id=3509)
{
    int width;// Offset=0x0 Size=0x4
    int height;// Offset=0x4 Size=0x4
    int frameCount;// Offset=0x8 Size=0x4
    long pts;// Offset=0xc Size=0x4
    long dts;// Offset=0x10 Size=0x4
    unsigned long flags;// Offset=0x14 Size=0x4
    long pts2nd;// Offset=0x18 Size=0x4
    long dts2nd;// Offset=0x1c Size=0x4
    unsigned long flags2nd;// Offset=0x20 Size=0x4
    void * sys;// Offset=0x24 Size=0x4
};

struct sceIpuRGB32// Size=0x400 (Id=3510)
{
    unsigned int pix[256];// Offset=0x0 Size=0x400
};

struct sceIpuRAW8// Size=0x180 (Id=3511)
{
    unsigned char y[256];// Offset=0x0 Size=0x100
    unsigned char cb[64];// Offset=0x100 Size=0x40
    unsigned char cr[64];// Offset=0x140 Size=0x40
};

union sceMpegCbData// Size=0x18 (Id=3512)
{
    enum sceMpegCbType type;// Offset=0x0 Size=0x4
    struct sceMpegCbDataError error;// Offset=0x0 Size=0x8
    struct sceMpegCbDataTimeStamp ts;// Offset=0x0 Size=0xc
    struct sceMpegCbDataStr str;// Offset=0x0 Size=0x18
};

struct sceSdBatch// Size=0x8 (Id=3513)
{
    unsigned short func;// Offset=0x0 Size=0x2
    unsigned short entry;// Offset=0x2 Size=0x2
    unsigned int value;// Offset=0x4 Size=0x4
};

struct sceSdEffectAttr// Size=0x14 (Id=3514)
{
    int core;// Offset=0x0 Size=0x4
    int mode;// Offset=0x4 Size=0x4
    short depth_L;// Offset=0x8 Size=0x2
    short depth_R;// Offset=0xa Size=0x2
    int delay;// Offset=0xc Size=0x4
    int feedback;// Offset=0x10 Size=0x4
};

struct sceSifDmaData// Size=0x10 (Id=3517)
{
    unsigned int data;// Offset=0x0 Size=0x4
    unsigned int addr;// Offset=0x4 Size=0x4
    unsigned int size;// Offset=0x8 Size=0x4
    unsigned int mode;// Offset=0xc Size=0x4
};

struct sceMpegCbDataError// Size=0x8 (Id=3528)
{
    enum sceMpegCbType type;// Offset=0x0 Size=0x4
    char * errMessage;// Offset=0x4 Size=0x4
};

struct sceMpegCbDataTimeStamp// Size=0xc (Id=3529)
{
    enum sceMpegCbType type;// Offset=0x0 Size=0x4
    long pts;// Offset=0x4 Size=0x4
    long dts;// Offset=0x8 Size=0x4
};

struct sceMpegCbDataStr// Size=0x18 (Id=3530)
{
    enum sceMpegCbType type;// Offset=0x0 Size=0x4
    unsigned char * header;// Offset=0x4 Size=0x4
    unsigned char * data;// Offset=0x8 Size=0x4
    unsigned int len;// Offset=0xc Size=0x4
    long pts;// Offset=0x10 Size=0x4
    long dts;// Offset=0x14 Size=0x4
};

struct SpuStreamHeader// Size=0x20 (Id=3573)
{
    char id[4];// Offset=0x0 Size=0x4
    int size;// Offset=0x4 Size=0x4
    int type;// Offset=0x8 Size=0x4
    int rate;// Offset=0xc Size=0x4
    int ch;// Offset=0x10 Size=0x4
    int interSize;// Offset=0x14 Size=0x4
    int loopStart;// Offset=0x18 Size=0x4
    int loopEnd;// Offset=0x1c Size=0x4
};

struct SpuStreamBody// Size=0x8 (Id=3574)
{
    char id[4];// Offset=0x0 Size=0x4
    int size;// Offset=0x4 Size=0x4
};

struct sceGsTex1// Size=0x8 (Id=3603)
{
    struct // Size=0x8 (Id=0)
    {
        unsigned long long LCM:1;// Offset=0x0 Size=0x8 BitOffset=0x0 BitSize=0x1
        unsigned long long pad01:1;// Offset=0x0 Size=0x8 BitOffset=0x1 BitSize=0x1
        unsigned long long MXL:3;// Offset=0x0 Size=0x8 BitOffset=0x2 BitSize=0x3
        unsigned long long MMAG:1;// Offset=0x0 Size=0x8 BitOffset=0x5 BitSize=0x1
        unsigned long long MMIN:3;// Offset=0x0 Size=0x8 BitOffset=0x6 BitSize=0x3
        unsigned long long MTBA:1;// Offset=0x0 Size=0x8 BitOffset=0x9 BitSize=0x1
        unsigned long long pad10:9;// Offset=0x0 Size=0x8 BitOffset=0xa BitSize=0x9
        unsigned long long L:2;// Offset=0x0 Size=0x8 BitOffset=0x13 BitSize=0x2
        unsigned long long pad21:11;// Offset=0x0 Size=0x8 BitOffset=0x15 BitSize=0xb
        unsigned long long K:12;// Offset=0x0 Size=0x8 BitOffset=0x20 BitSize=0xc
        unsigned long long pad44:20;// Offset=0x0 Size=0x8 BitOffset=0x2c BitSize=0x14
    };
};

struct tGS_DISPFB1// Size=0x8 (Id=3604)
{
    struct // Size=0x8 (Id=0)
    {
        unsigned int FBP:9;// Offset=0x0 Size=0x4 BitOffset=0x0 BitSize=0x9
        unsigned int FBW:6;// Offset=0x0 Size=0x4 BitOffset=0x9 BitSize=0x6
        unsigned int PSM:5;// Offset=0x0 Size=0x4 BitOffset=0xf BitSize=0x5
        unsigned int p0:12;// Offset=0x0 Size=0x4 BitOffset=0x14 BitSize=0xc
        unsigned int DBX:11;// Offset=0x4 Size=0x4 BitOffset=0x0 BitSize=0xb
        unsigned int DBY:11;// Offset=0x4 Size=0x4 BitOffset=0xb BitSize=0xb
        unsigned int p1:10;// Offset=0x4 Size=0x4 BitOffset=0x16 BitSize=0xa
    };
};

struct sceIpuDmaEnv// Size=0x24 (Id=3618)
{
    unsigned int d4madr;// Offset=0x0 Size=0x4
    unsigned int d4tadr;// Offset=0x4 Size=0x4
    unsigned int d4qwc;// Offset=0x8 Size=0x4
    unsigned int d4chcr;// Offset=0xc Size=0x4
    unsigned int d3madr;// Offset=0x10 Size=0x4
    unsigned int d3qwc;// Offset=0x14 Size=0x4
    unsigned int d3chcr;// Offset=0x18 Size=0x4
    unsigned int ipubp;// Offset=0x1c Size=0x4
    unsigned int ipuctrl;// Offset=0x20 Size=0x4
};

struct tGS_DISPLAY1// Size=0x8 (Id=3621)
{
    struct // Size=0x8 (Id=0)
    {
        unsigned int DX:12;// Offset=0x0 Size=0x4 BitOffset=0x0 BitSize=0xc
        unsigned int DY:11;// Offset=0x0 Size=0x4 BitOffset=0xc BitSize=0xb
        unsigned int MAGH:4;// Offset=0x0 Size=0x4 BitOffset=0x17 BitSize=0x4
        unsigned int MAGV:2;// Offset=0x0 Size=0x4 BitOffset=0x1b BitSize=0x2
        unsigned int p0:3;// Offset=0x0 Size=0x4 BitOffset=0x1d BitSize=0x3
        unsigned int DW:12;// Offset=0x4 Size=0x4 BitOffset=0x0 BitSize=0xc
        unsigned int DH:11;// Offset=0x4 Size=0x4 BitOffset=0xc BitSize=0xb
        unsigned int p1:9;// Offset=0x4 Size=0x4 BitOffset=0x17 BitSize=0x9
    };
};


#endif // _PS2_H_
