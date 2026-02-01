#ifndef _WINDOWS_GDI_H_
#define _WINDOWS_GDI_H_

// Category: windows_gdi
// Extracted from Xbox PDB symbols
// Total types: 60
// Note: Xbox symbols - may need adaptation for GameCube

enum tagSTGTY
{
    STGTY_STORAGE=1,
    STGTY_STREAM=2,
    STGTY_LOCKBYTES=3,
    STGTY_PROPERTY=4
};

enum tagSTREAM_SEEK
{
    STREAM_SEEK_SET=0,
    STREAM_SEEK_CUR=1,
    STREAM_SEEK_END=2
};

enum tagLOCKTYPE
{
    LOCK_WRITE=1,
    LOCK_EXCLUSIVE=2,
    LOCK_ONLYONCE=4
};

struct _RECTL// Size=0x10 (Id=189)
{
    long left;// Offset=0x0 Size=0x4
    long top;// Offset=0x4 Size=0x4
    long right;// Offset=0x8 Size=0x4
    long bottom;// Offset=0xc Size=0x4
};

struct HGLRC__// Size=0x4 (Id=198)
{
    int unused;// Offset=0x0 Size=0x4
};

struct tagPOINTS// Size=0x4 (Id=329)
{
    short x;// Offset=0x0 Size=0x2
    short y;// Offset=0x2 Size=0x2
};

struct _RECTL// Size=0x10 (Id=407)
{
    long left;// Offset=0x0 Size=0x4
    long top;// Offset=0x4 Size=0x4
    long right;// Offset=0x8 Size=0x4
    long bottom;// Offset=0xc Size=0x4
};

struct HWND__// Size=0x4 (Id=614)
{
    int unused;// Offset=0x0 Size=0x4
};

struct tagPALETTEENTRY// Size=0x4 (Id=620)
{
    unsigned char peRed;// Offset=0x0 Size=0x1
    unsigned char peGreen;// Offset=0x1 Size=0x1
    unsigned char peBlue;// Offset=0x2 Size=0x1
    unsigned char peFlags;// Offset=0x3 Size=0x1
};

struct tagPOINT// Size=0x8 (Id=676)
{
    long x;// Offset=0x0 Size=0x4
    long y;// Offset=0x4 Size=0x4
};

struct tagSIZE// Size=0x8 (Id=802)
{
    long cx;// Offset=0x0 Size=0x4
    long cy;// Offset=0x4 Size=0x4
};

struct tagSTATSTG// Size=0x48 (Id=870)
{
    unsigned short * pwcsName;// Offset=0x0 Size=0x4
    unsigned long type;// Offset=0x4 Size=0x4
    union _ULARGE_INTEGER cbSize;// Offset=0x8 Size=0x8
    struct _FILETIME mtime;// Offset=0x10 Size=0x8
    struct _FILETIME ctime;// Offset=0x18 Size=0x8
    struct _FILETIME atime;// Offset=0x20 Size=0x8
    unsigned long grfMode;// Offset=0x28 Size=0x4
    unsigned long grfLocksSupported;// Offset=0x2c Size=0x4
    struct _GUID clsid;// Offset=0x30 Size=0x10
    unsigned long grfStateBits;// Offset=0x40 Size=0x4
    unsigned long reserved;// Offset=0x44 Size=0x4
};

struct tagRECT// Size=0x10 (Id=1026)
{
    long left;// Offset=0x0 Size=0x4
    long top;// Offset=0x4 Size=0x4
    long right;// Offset=0x8 Size=0x4
    long bottom;// Offset=0xc Size=0x4
};

struct tagRECT// Size=0x10 (Id=1035)
{
    long left;// Offset=0x0 Size=0x4
    long top;// Offset=0x4 Size=0x4
    long right;// Offset=0x8 Size=0x4
    long bottom;// Offset=0xc Size=0x4
};

struct HDC__// Size=0x4 (Id=1089)
{
    int unused;// Offset=0x0 Size=0x4
};

struct tagPOINT// Size=0x8 (Id=1321)
{
    long x;// Offset=0x0 Size=0x4
    long y;// Offset=0x4 Size=0x4
};

struct tagPALETTEENTRY// Size=0x4 (Id=1351)
{
    unsigned char peRed;// Offset=0x0 Size=0x1
    unsigned char peGreen;// Offset=0x1 Size=0x1
    unsigned char peBlue;// Offset=0x2 Size=0x1
    unsigned char peFlags;// Offset=0x3 Size=0x1
};

struct tagBITMAPINFOHEADER// Size=0x28 (Id=2567)
{
    unsigned long biSize;// Offset=0x0 Size=0x4
    long biWidth;// Offset=0x4 Size=0x4
    long biHeight;// Offset=0x8 Size=0x4
    unsigned short biPlanes;// Offset=0xc Size=0x2
    unsigned short biBitCount;// Offset=0xe Size=0x2
    unsigned long biCompression;// Offset=0x10 Size=0x4
    unsigned long biSizeImage;// Offset=0x14 Size=0x4
    long biXPelsPerMeter;// Offset=0x18 Size=0x4
    long biYPelsPerMeter;// Offset=0x1c Size=0x4
    unsigned long biClrUsed;// Offset=0x20 Size=0x4
    unsigned long biClrImportant;// Offset=0x24 Size=0x4
};

struct tagBITMAPFILEHEADER// Size=0xe (Id=2568)
{
    unsigned short bfType;// Offset=0x0 Size=0x2
    unsigned long bfSize;// Offset=0x2 Size=0x4
    unsigned short bfReserved1;// Offset=0x6 Size=0x2
    unsigned short bfReserved2;// Offset=0x8 Size=0x2
    unsigned long bfOffBits;// Offset=0xa Size=0x4
};

struct tagRGBQUAD// Size=0x4 (Id=2569)
{
    unsigned char rgbBlue;// Offset=0x0 Size=0x1
    unsigned char rgbGreen;// Offset=0x1 Size=0x1
    unsigned char rgbRed;// Offset=0x2 Size=0x1
    unsigned char rgbReserved;// Offset=0x3 Size=0x1
};

struct tagRandState// Size=0x8 (Id=2751)
{
    long iPrior;// Offset=0x0 Size=0x4
    unsigned long uiRand;// Offset=0x4 Size=0x4
};

enum tagWMCParseState
{
    csWMC_NotValid=0,
    csWMCHeaderStart=1,
    csWMCHeaderError=2,
    csWMCNewAsfPacket=3,
    csWMCDecodePayloadStart=4,
    csWMCDecodePayload=5,
    csWMCDecodePayloadHeader=6,
    csWMCDecodeLoopStart=7,
    csWMCDecodePayloadEnd=8,
    csWMCDecodeCompressedPayload=9,
    csWMCEnd=10
};

enum tagWMVDecodeStatus
{
    WMV_Succeeded=0,
    WMV_Failed=1,
    WMV_BadMemory=2,
    WMV_NoKeyFrameDecoded=3,
    WMV_CorruptedBits=4,
    WMV_UnSupportedOutputPixelFormat=5,
    WMV_UnSupportedCompressedFormat=6,
    WMV_InValidArguments=7,
    WMV_BadSource=8,
    WMV_NoMoreOutput=9,
    WMV_EndOfFrame=10,
    WMV_BrokenFrame=11
};

enum tagMediaType_WMC
{
    Audio_WMC=0,
    Video_WMC=1,
    Binary_WMC=2
};

struct tagWMCMetaDataDescRecords// Size=0x14 (Id=2783)
{
    unsigned short wLangIdIndex;// Offset=0x0 Size=0x2
    unsigned short wStreamNumber;// Offset=0x2 Size=0x2
    unsigned short wNameLenth;// Offset=0x4 Size=0x2
    unsigned short wDataType;// Offset=0x6 Size=0x2
    unsigned long wDataLength;// Offset=0x8 Size=0x4
    unsigned short * pwName;// Offset=0xc Size=0x4
    void * pData;// Offset=0x10 Size=0x4
};

struct tagPlannedOutput_WMC// Size=0x5f8 (Id=2785)
{
    unsigned short wTotalOutput;// Offset=0x0 Size=0x2
    unsigned char __align0[2];// Offset=0x2 Size=0x2
    struct tagPlannedOutputId_WMC tPlannedId[127];// Offset=0x4 Size=0x5f4
};

struct tagWMCIndexEntries// Size=0x8 (Id=2786)
{
    unsigned long dwPacket;// Offset=0x0 Size=0x4
    unsigned short wSpan;// Offset=0x4 Size=0x2
};

struct tagWMCCodecEntry// Size=0x1c (Id=2787)
{
    enum tagMediaType_WMC m_wCodecType;// Offset=0x0 Size=0x4
    unsigned short m_wCodecNameLength;// Offset=0x4 Size=0x2
    unsigned char __align0[2];// Offset=0x6 Size=0x2
    unsigned short * m_pwCodecName;// Offset=0x8 Size=0x4
    unsigned short m_wCodecDescLength;// Offset=0xc Size=0x2
    unsigned char __align1[2];// Offset=0xe Size=0x2
    unsigned short * m_pwCodecDescription;// Offset=0x10 Size=0x4
    unsigned short m_wCodecInfoLen;// Offset=0x14 Size=0x2
    unsigned char __align2[2];// Offset=0x16 Size=0x2
    unsigned char * m_pbCodecInfo;// Offset=0x18 Size=0x4
};

struct tagWMCContentDescription// Size=0x20 (Id=2790)
{
    unsigned short uiTitle_len;// Offset=0x0 Size=0x2
    unsigned short uiAuthor_len;// Offset=0x2 Size=0x2
    unsigned short uiCopyright_len;// Offset=0x4 Size=0x2
    unsigned short uiDescription_len;// Offset=0x6 Size=0x2
    unsigned short uiRating_len;// Offset=0x8 Size=0x2
    unsigned char __align0[2];// Offset=0xa Size=0x2
    unsigned short * pchTitle;// Offset=0xc Size=0x4
    unsigned short * pchAuthor;// Offset=0x10 Size=0x4
    unsigned short * pchCopyright;// Offset=0x14 Size=0x4
    unsigned short * pchDescription;// Offset=0x18 Size=0x4
    unsigned short * pchRating;// Offset=0x1c Size=0x4
};

struct tagWMCMarkerEntry// Size=0x28 (Id=2792)
{
    unsigned long long m_qOffset;// Offset=0x0 Size=0x8
    unsigned long long m_qtime;// Offset=0x8 Size=0x8
    unsigned short m_wEntryLen;// Offset=0x10 Size=0x2
    unsigned char __align0[2];// Offset=0x12 Size=0x2
    unsigned long m_dwSendTime;// Offset=0x14 Size=0x4
    unsigned long m_dwFlags;// Offset=0x18 Size=0x4
    unsigned long m_dwDescLen;// Offset=0x1c Size=0x4
    unsigned short * m_pwDescName;// Offset=0x20 Size=0x4
};

struct tagPlannedOutputId_WMC// Size=0xc (Id=2794)
{
    unsigned short wStreamIndex;// Offset=0x0 Size=0x2
    unsigned char __align0[2];// Offset=0x2 Size=0x2
    enum tagMediaType_WMC tMediaType;// Offset=0x4 Size=0x4
    long bDone;// Offset=0x8 Size=0x4
};

struct tagStreamIdnMediaType_WMC// Size=0x8 (Id=2795)
{
    unsigned short wStreamId;// Offset=0x0 Size=0x2
    unsigned char __align0[2];// Offset=0x2 Size=0x2
    enum tagMediaType_WMC MediaType;// Offset=0x4 Size=0x4
};

struct tagWMCScriptCommand// Size=0x14 (Id=2796)
{
    unsigned short num_commands;// Offset=0x0 Size=0x2
    unsigned short num_types;// Offset=0x2 Size=0x2
    unsigned short ** type_names;// Offset=0x4 Size=0x4
    long * type_name_len;// Offset=0x8 Size=0x4
    long * command_param_len;// Offset=0xc Size=0x4
    struct _WMCCommandEntry * commands;// Offset=0x10 Size=0x4
};

struct tagWMCExtendedContentDescription// Size=0x8 (Id=2797)
{
    unsigned short cDescriptors;// Offset=0x0 Size=0x2
    unsigned char __align0[2];// Offset=0x2 Size=0x2
    struct _ECD_DESCRIPTOR * pDescriptors;// Offset=0x4 Size=0x4
};

struct tagWMCMetaDataEntry// Size=0x8 (Id=2798)
{
    unsigned short m_wDescRecordsCount;// Offset=0x0 Size=0x2
    unsigned char __align0[2];// Offset=0x2 Size=0x2
    struct tagWMCMetaDataDescRecords * pDescRec;// Offset=0x4 Size=0x4
};

struct tagWMCIndexInfo// Size=0x14 (Id=2800)
{
    unsigned short nStreamId;// Offset=0x0 Size=0x2
    unsigned char __align0[2];// Offset=0x2 Size=0x2
    unsigned long time_deltaMs;// Offset=0x4 Size=0x4
    unsigned long max_packets;// Offset=0x8 Size=0x4
    unsigned long num_entries;// Offset=0xc Size=0x4
    struct tagWMCIndexEntries * pIndexEntries;// Offset=0x10 Size=0x4
};

enum tagWMCDecodeDispRotateDegree
{
    WMC_DispRotate0=0,
    WMC_DispRotate90=1,
    WMC_DispFlip=2,
    WMC_DispRotate270=3
};

enum tagVideoFormat_WMC
{
    YUY2_WMV=0,
    UYVY_WMV=1,
    YVYU_WMV=2,
    RGB24_WMV=3,
    RGB555_WMV=4,
    RGB565_WMV=5,
    RGB32_WMV=6,
    RGB8_WMV=7,
    IYUV_WMV=8,
    I420_WMV=9,
    YVU9_WMV=10,
    WMV2_WMV=11,
    WMV1_WMV=12,
    IGNORE_VIDEO=13
};

enum tagOutputType_WMC
{
    Discard_WMC=0,
    Compressed_WMC=1,
    Decompressed_WMC=2
};

struct tagStreamIdPattern_WMC// Size=0x8 (Id=2812)
{
    unsigned short wStreamId;// Offset=0x0 Size=0x2
    unsigned char __align0[2];// Offset=0x2 Size=0x2
    enum tagOutputType_WMC tPattern;// Offset=0x4 Size=0x4
};

struct tagWMCContentDescription// Size=0x20 (Id=2813)
{
    unsigned short uiTitle_len;// Offset=0x0 Size=0x2
    unsigned short uiAuthor_len;// Offset=0x2 Size=0x2
    unsigned short uiCopyright_len;// Offset=0x4 Size=0x2
    unsigned short uiDescription_len;// Offset=0x6 Size=0x2
    unsigned short uiRating_len;// Offset=0x8 Size=0x2
    unsigned char __align0[2];// Offset=0xa Size=0x2
    unsigned short * pchTitle;// Offset=0xc Size=0x4
    unsigned short * pchAuthor;// Offset=0x10 Size=0x4
    unsigned short * pchCopyright;// Offset=0x14 Size=0x4
    unsigned short * pchDescription;// Offset=0x18 Size=0x4
    unsigned short * pchRating;// Offset=0x1c Size=0x4
};

struct tagWMCExtendedContentDescription// Size=0x8 (Id=2814)
{
    unsigned short cDescriptors;// Offset=0x0 Size=0x2
    unsigned char __align0[2];// Offset=0x2 Size=0x2
    struct _ECD_DESCRIPTOR * pDescriptors;// Offset=0x4 Size=0x4
};

struct tagWMCScriptCommand// Size=0x14 (Id=2815)
{
    unsigned short num_commands;// Offset=0x0 Size=0x2
    unsigned short num_types;// Offset=0x2 Size=0x2
    unsigned short ** type_names;// Offset=0x4 Size=0x4
    long * type_name_len;// Offset=0x8 Size=0x4
    long * command_param_len;// Offset=0xc Size=0x4
    struct _WMCCommandEntry * commands;// Offset=0x10 Size=0x4
};

struct tagStreamIdPattern_WMC// Size=0x8 (Id=2816)
{
    unsigned short wStreamId;// Offset=0x0 Size=0x2
    unsigned char __align0[2];// Offset=0x2 Size=0x2
    enum tagOutputType_WMC tPattern;// Offset=0x4 Size=0x4
};

enum tagVResultCode
{
    vrNoError=0,
    vrFail=1,
    vrNotFound=2,
    vrExists=3,
    vrEof=4,
    vrOutOfMemory=5,
    vrOutOfResource=6,
    vrOutOfBounds=7,
    vrBadParam=8,
    vrBadInput=9,
    vrIOError=10,
    vrInterrupted=11,
    vrNotSupported=12,
    vrNotImplemented=13,
    vrDropped=14,
    vr_ENDOFALLERRORS=15
};

struct tagCWMVMBMode// Size=0x8 (Id=2822)
{
    struct // Size=0x4 (Id=0)
    {
        unsigned long m_bSkip:1;// Offset=0x0 Size=0x4 BitOffset=0x0 BitSize=0x1
        unsigned long m_bCBPAllZero:1;// Offset=0x0 Size=0x4 BitOffset=0x1 BitSize=0x1
        unsigned long m_bZeroMV:1;// Offset=0x0 Size=0x4 BitOffset=0x2 BitSize=0x1
        unsigned long m_bBlkXformSwitchOn:1;// Offset=0x0 Size=0x4 BitOffset=0x3 BitSize=0x1
        unsigned long m_iMBXformMode:2;// Offset=0x0 Size=0x4 BitOffset=0x4 BitSize=0x2
        unsigned long m_iDCTTable_MB_Index:2;// Offset=0x0 Size=0x4 BitOffset=0x6 BitSize=0x2
        unsigned long m_iMVPredDirection:2;// Offset=0x0 Size=0x4 BitOffset=0x8 BitSize=0x2
        unsigned long m_dctMd:1;// Offset=0x0 Size=0x4 BitOffset=0xa BitSize=0x1
    };
    unsigned char m_rgbCodedBlockPattern;// Offset=0x4 Size=0x1
    unsigned char m_rgbDCTCoefPredPattern;// Offset=0x5 Size=0x1
};

struct tagCInputBitStream_WMV// Size=0x1c (Id=2824)
{
    unsigned char * m_pBuffer;// Offset=0x0 Size=0x4
    int m_cbBuflen;// Offset=0x4 Size=0x4
    unsigned long m_dwDot;// Offset=0x8 Size=0x4
    unsigned long m_dwBitsLeft;// Offset=0xc Size=0x4
    int m_fStreamStaus;// Offset=0x10 Size=0x4
    long m_bNotEndOfFrame;// Offset=0x14 Size=0x4
    unsigned long m_uiUserData;// Offset=0x18 Size=0x4
};

struct tagCVector// Size=0x2 (Id=2826)
{
    char x;// Offset=0x0 Size=0x1
    char y;// Offset=0x1 Size=0x1
};

struct tagYUV420Frame_WMV// Size=0x18 (Id=2827)
{
    unsigned char * m_pucYPlane;// Offset=0x0 Size=0x4
    unsigned char * m_pucUPlane;// Offset=0x4 Size=0x4
    unsigned char * m_pucVPlane;// Offset=0x8 Size=0x4
    unsigned char * m_pucYPlane_Unaligned;// Offset=0xc Size=0x4
    unsigned char * m_pucUPlane_Unaligned;// Offset=0x10 Size=0x4
    unsigned char * m_pucVPlane_Unaligned;// Offset=0x14 Size=0x4
};

struct tagWMVDecInternalMember// Size=0x39c8 (Id=2832)
{
    unsigned long m_uiUserData;// Offset=0x0 Size=0x4
    struct tagCInputBitStream_WMV * m_pInputBitstream;// Offset=0x4 Size=0x4
    struct tagCInputBitStream_WMV * m_pbitstrmIn;// Offset=0x8 Size=0x4
    unsigned long m_uiFOURCCCompressed;// Offset=0xc Size=0x4
    int m_iPostFilterLevel;// Offset=0x10 Size=0x4
    float m_fltFrameRate;// Offset=0x14 Size=0x4
    float m_fltBitRate;// Offset=0x18 Size=0x4
    int m_iFrameRate;// Offset=0x1c Size=0x4
    int m_iBitRate;// Offset=0x20 Size=0x4
    long m_fPrepared;// Offset=0x24 Size=0x4
    long m_fDecodedI;// Offset=0x28 Size=0x4
    unsigned short m_uiNumFramesLeftForOutput;// Offset=0x2c Size=0x2
    unsigned char __align0[2];// Offset=0x2e Size=0x2
    int m_iWidthSource;// Offset=0x30 Size=0x4
    int m_iHeightSource;// Offset=0x34 Size=0x4
    int m_iWidthInternal;// Offset=0x38 Size=0x4
    int m_iWidthInternalTimesMB;// Offset=0x3c Size=0x4
    int m_iHeightInternal;// Offset=0x40 Size=0x4
    int m_iWidthInternalUV;// Offset=0x44 Size=0x4
    int m_iWidthInternalUVTimesBlk;// Offset=0x48 Size=0x4
    int m_iHeightInternalUV;// Offset=0x4c Size=0x4
    long m_bSizeMBAligned;// Offset=0x50 Size=0x4
    unsigned long m_uiNumMB;// Offset=0x54 Size=0x4
    unsigned long m_uiNumMBX;// Offset=0x58 Size=0x4
    unsigned long m_uiNumMBY;// Offset=0x5c Size=0x4
    unsigned long m_uintNumMBX;// Offset=0x60 Size=0x4
    unsigned long m_uintNumMBY;// Offset=0x64 Size=0x4
    unsigned long m_uintNumMB;// Offset=0x68 Size=0x4
    unsigned long m_uiRightestMB;// Offset=0x6c Size=0x4
    long m_bMBAligned;// Offset=0x70 Size=0x4
    int m_iFrmWidthSrc;// Offset=0x74 Size=0x4
    int m_iFrmHeightSrc;// Offset=0x78 Size=0x4
    int m_iWidthYRepeatPad;// Offset=0x7c Size=0x4
    int m_iWidthUVRepeatPad;// Offset=0x80 Size=0x4
    int m_iHeightYRepeatPad;// Offset=0x84 Size=0x4
    int m_iHeightUVRepeatPad;// Offset=0x88 Size=0x4
    int m_iWidthY;// Offset=0x8c Size=0x4
    int m_iWidthYPlusExp;// Offset=0x90 Size=0x4
    int m_iHeightY;// Offset=0x94 Size=0x4
    int m_iWidthUV;// Offset=0x98 Size=0x4
    int m_iWidthUVPlusExp;// Offset=0x9c Size=0x4
    int m_iHeightUV;// Offset=0xa0 Size=0x4
    int m_iWidthPrevY;// Offset=0xa4 Size=0x4
    int m_iWidthPrevUV;// Offset=0xa8 Size=0x4
    int m_iHeightPrevY;// Offset=0xac Size=0x4
    int m_iHeightPrevUV;// Offset=0xb0 Size=0x4
    int m_iWidthPrevYXExpPlusExp;// Offset=0xb4 Size=0x4
    int m_iWidthPrevUVXExpPlusExp;// Offset=0xb8 Size=0x4
    int m_iMBSizeXWidthPrevY;// Offset=0xbc Size=0x4
    int m_iBlkSizeXWidthPrevUV;// Offset=0xc0 Size=0x4
    int m_iWidthPrevYxBlkMinusBlk;// Offset=0xc4 Size=0x4
    int m_iMVLeftBound;// Offset=0xc8 Size=0x4
    int m_iMVRightBound;// Offset=0xcc Size=0x4
    unsigned char * m_rgiClapTabDec;// Offset=0xd0 Size=0x4
    struct tagYUV420Frame_WMV * m_pfrmCurrQ;// Offset=0xd4 Size=0x4
    struct tagYUV420Frame_WMV * m_pfrmPrev;// Offset=0xd8 Size=0x4
    struct tagYUV420Frame_WMV * m_pfrmPostQ;// Offset=0xdc Size=0x4
    unsigned char * m_ppxliCurrQPlusExpY;// Offset=0xe0 Size=0x4
    unsigned char * m_ppxliCurrQPlusExpU;// Offset=0xe4 Size=0x4
    unsigned char * m_ppxliCurrQPlusExpV;// Offset=0xe8 Size=0x4
    unsigned char * m_ppxliRef0Y;// Offset=0xec Size=0x4
    unsigned char * m_ppxliRef0U;// Offset=0xf0 Size=0x4
    unsigned char * m_ppxliRef0V;// Offset=0xf4 Size=0x4
    unsigned char * m_ppxliRef0YPlusExp;// Offset=0xf8 Size=0x4
    unsigned char * m_ppxliPostQY;// Offset=0xfc Size=0x4
    unsigned char * m_ppxliPostQU;// Offset=0x100 Size=0x4
    unsigned char * m_ppxliPostQV;// Offset=0x104 Size=0x4
    unsigned char * m_ppxliPostQPlusExpY;// Offset=0x108 Size=0x4
    unsigned char * m_ppxliPostQPlusExpU;// Offset=0x10c Size=0x4
    unsigned char * m_ppxliPostQPlusExpV;// Offset=0x110 Size=0x4
    struct tagCVector * m_rgmv;// Offset=0x114 Size=0x4
    struct tagCWMVMBMode * m_rgmbmd;// Offset=0x118 Size=0x4
    struct tagCWMVMBMode * m_pmbmdZeroCBPCY;// Offset=0x11c Size=0x4
    unsigned char * m_rgchSkipPrevFrame;// Offset=0x120 Size=0x4
    long m_bIFrameDecoded;// Offset=0x124 Size=0x4
    long m_bXintra8Switch;// Offset=0x128 Size=0x4
    long m_bXformSwitch;// Offset=0x12c Size=0x4
    long m_bSKIPBIT_CODING_;// Offset=0x130 Size=0x4
    long m_bNEW_PCBPCY_TABLE;// Offset=0x134 Size=0x4
    long m_bMixedPel;// Offset=0x138 Size=0x4
    long m_bLoopFilter;// Offset=0x13c Size=0x4
    long m_bRndCtrlOn;// Offset=0x140 Size=0x4
    int m_iRndCtrl;// Offset=0x144 Size=0x4
    int m_iSliceCode;// Offset=0x148 Size=0x4
    unsigned long m_uintNumMBYSlice;// Offset=0x14c Size=0x4
    enum CodecVersion m_cvCodecVersion;// Offset=0x150 Size=0x4
    long m_bMainProfileOn;// Offset=0x154 Size=0x4
    long m_bMMXSupport;// Offset=0x158 Size=0x4
    enum tagFrameType_WMV m_tFrmType;// Offset=0x15c Size=0x4
    enum SKIPBITCODINGMODE m_SkipBitCodingMode;// Offset=0x160 Size=0x4
    long m_bXintra8;// Offset=0x164 Size=0x4
    long m_bFrmHybridMVOn;// Offset=0x168 Size=0x4
    long m_bDCTTable_MB;// Offset=0x16c Size=0x4
    long m_bDCTTable_MB_ENABLED;// Offset=0x170 Size=0x4
    long m_bDCPred_IMBInPFrame;// Offset=0x174 Size=0x4
    long m_bCODFlagOn;// Offset=0x178 Size=0x4
    int m_iMvResolution;// Offset=0x17c Size=0x4
    int m_iStepSize;// Offset=0x180 Size=0x4
    int m_iDCStepSize;// Offset=0x184 Size=0x4
    int m_iDCStepSizeC;// Offset=0x188 Size=0x4
    int m_i2DoublePlusStepSize;// Offset=0x18c Size=0x4
    int m_i2DoublePlusStepSizeNeg;// Offset=0x190 Size=0x4
    int m_iDoubleStepSize;// Offset=0x194 Size=0x4
    int m_iStepMinusStepIsEven;// Offset=0x198 Size=0x4
    long m_bStepSizeIsEven;// Offset=0x19c Size=0x4
    long m_bMBHybridMV;// Offset=0x1a0 Size=0x4
    long m_bMBXformSwitching;// Offset=0x1a4 Size=0x4
    int m_iMixedPelMV;// Offset=0x1a8 Size=0x4
    int m_iFrameXformMode;// Offset=0x1ac Size=0x4
    int m_iOffsetToTopMB;// Offset=0x1b0 Size=0x4
    unsigned char m_riReconBuf[288];// Offset=0x1b4 Size=0x120
    unsigned char m_riPixelError[288];// Offset=0x2d4 Size=0x120
    int * m_rgiCoefReconPlus1;// Offset=0x3f4 Size=0x4
    int * m_rgiCoefRecon;// Offset=0x3f8 Size=0x4
    union Buffer * m_ppxliErrorQ;// Offset=0x3fc Size=0x4
    union Buffer * m_rgiCoefReconBuf;// Offset=0x400 Size=0x4
    long m_bRotatedIdct;// Offset=0x404 Size=0x4
    unsigned char * m_pZigzagInv_I;// Offset=0x408 Size=0x4
    unsigned char * m_pHorizontalZigzagInv;// Offset=0x40c Size=0x4
    unsigned char * m_pVerticalZigzagInv;// Offset=0x410 Size=0x4
    unsigned char * m_pZigzagInvRotated_I;// Offset=0x414 Size=0x4
    unsigned char * m_pHorizontalZigzagInvRotated;// Offset=0x418 Size=0x4
    unsigned char * m_pVerticalZigzagInvRotated;// Offset=0x41c Size=0x4
    unsigned char * m_pZigzagInvRotated;// Offset=0x420 Size=0x4
    unsigned char * m_pZigzagInv;// Offset=0x424 Size=0x4
    unsigned char * m_pZigzagScanOrder;// Offset=0x428 Size=0x4
    unsigned char * m_p8x4ZigzagInv;// Offset=0x42c Size=0x4
    unsigned char * m_p4x8ZigzagInv;// Offset=0x430 Size=0x4
    unsigned char * m_p8x4ZigzagInvRotated;// Offset=0x434 Size=0x4
    unsigned char * m_p4x8ZigzagInvRotated;// Offset=0x438 Size=0x4
    unsigned char * m_p8x4ZigzagScanOrder;// Offset=0x43c Size=0x4
    unsigned char * m_p4x8ZigzagScanOrder;// Offset=0x440 Size=0x4
    int m_iNumOfQuanDctCoefForACPredPerRow;// Offset=0x444 Size=0x4
    short * m_rgiQuanCoefACPred;// Offset=0x448 Size=0x4
    short ** m_rgiQuanCoefACPredTable;// Offset=0x44c Size=0x4
    short * m_pAvgQuanDctCoefDec;// Offset=0x450 Size=0x4
    short * m_pAvgQuanDctCoefDecC;// Offset=0x454 Size=0x4
    short * m_pAvgQuanDctCoefDecLeft;// Offset=0x458 Size=0x4
    short * m_pAvgQuanDctCoefDecTop;// Offset=0x45c Size=0x4
    unsigned long m_iEscRunDiffV2V3;// Offset=0x460 Size=0x4
    int m_iDCPredCorrect;// Offset=0x464 Size=0x4
    int m_iDCTHorzFlags;// Offset=0x468 Size=0x4
    long m_bFirstEscCodeInFrame;// Offset=0x46c Size=0x4
    int m_iNUMBITS_ESC_LEVEL;// Offset=0x470 Size=0x4
    int m_iNUMBITS_ESC_RUN;// Offset=0x474 Size=0x4
    int sm_iIDCTDecCount;// Offset=0x478 Size=0x4
    unsigned long m_uiNumProcessors;// Offset=0x47c Size=0x4
    int m_iNonflatQuant;// Offset=0x480 Size=0x4
    struct t_SpatialPredictor * m_pSp;// Offset=0x484 Size=0x4
    struct t_ContextWMV * m_pContext;// Offset=0x488 Size=0x4
    struct t_AltTablesDecoder * m_pAltTables;// Offset=0x48c Size=0x4
    long m_bBMPInitialized;// Offset=0x490 Size=0x4
    unsigned long m_uiFOURCCOutput;// Offset=0x494 Size=0x4
    unsigned short m_uiBitsPerPixelOutput;// Offset=0x498 Size=0x2
    unsigned char __align1[2];// Offset=0x49a Size=0x2
    long m_bRefreshBMP;// Offset=0x49c Size=0x4
    long m_bYUVDstBMP;// Offset=0x4a0 Size=0x4
    unsigned long m_uiRedscale;// Offset=0x4a4 Size=0x4
    unsigned long m_uiGreenscale;// Offset=0x4a8 Size=0x4
    unsigned long m_uiRedmask;// Offset=0x4ac Size=0x4
    unsigned long m_uiGreenmask;// Offset=0x4b0 Size=0x4
    unsigned char m_rgDitherMap[4][4][3][256];// Offset=0x4b4 Size=0x3000
    int * m_piYscale;// Offset=0x34b4 Size=0x4
    int * m_piVtoR;// Offset=0x34b8 Size=0x4
    int * m_piUtoG;// Offset=0x34bc Size=0x4
    int * m_piVtoG;// Offset=0x34c0 Size=0x4
    int * m_piUtoB;// Offset=0x34c4 Size=0x4
    int m_iWidthBMP;// Offset=0x34c8 Size=0x4
    int m_iBMPPointerStart;// Offset=0x34cc Size=0x4
    int m_iBMPMBIncrement;// Offset=0x34d0 Size=0x4
    int m_iBMPBlkIncrement;// Offset=0x34d4 Size=0x4
    int m_iBMPMBHeightIncrement;// Offset=0x34d8 Size=0x4
    int m_iBMPBlkHeightIncrement;// Offset=0x34dc Size=0x4
    unsigned char * m_pBMPBits;// Offset=0x34e0 Size=0x4
    int m_iWidthPrevYTimes8Minus8;// Offset=0x34e4 Size=0x4
    int m_iWidthPrevUVTimes4Minus4;// Offset=0x34e8 Size=0x4
    unsigned char * m_puXMvFromIndex;// Offset=0x34ec Size=0x4
    unsigned char * m_puYMvFromIndex;// Offset=0x34f0 Size=0x4
    struct _Huffman_WMV * m_pHufMVDec;// Offset=0x34f4 Size=0x4
    struct _Huffman_WMV m_hufMVDec_Talking;// Offset=0x34f8 Size=0x28
    struct _Huffman_WMV m_hufMVDec_HghMt;// Offset=0x3520 Size=0x28
    struct _Huffman_WMV * m_pHufMVDec_Set[2];// Offset=0x3548 Size=0x8
    unsigned char * m_puMvFromIndex_Set[4];// Offset=0x3550 Size=0x10
    unsigned long m_iMVTable;// Offset=0x3560 Size=0x4
    struct _Huffman_WMV m_hufDCTDCyDec_Talking;// Offset=0x3564 Size=0x28
    struct _Huffman_WMV m_hufDCTDCcDec_Talking;// Offset=0x358c Size=0x28
    struct _Huffman_WMV m_hufDCTDCyDec_HghMt;// Offset=0x35b4 Size=0x28
    struct _Huffman_WMV m_hufDCTDCcDec_HghMt;// Offset=0x35dc Size=0x28
    unsigned long m_iIntraDCTDCTable;// Offset=0x3604 Size=0x4
    struct _Huffman_WMV * m_pHufDCTDCyDec;// Offset=0x3608 Size=0x4
    struct _Huffman_WMV * m_pHufDCTDCcDec;// Offset=0x360c Size=0x4
    struct _Huffman_WMV * m_pHufDCTDCDec_Set[4];// Offset=0x3610 Size=0x10
    struct _Huffman_WMV m_hufICBPCYDec;// Offset=0x3620 Size=0x28
    struct _Huffman_WMV m_hufPCBPCYDec;// Offset=0x3648 Size=0x28
    struct _Huffman_WMV * m_pHufNewPCBPCYDec;// Offset=0x3670 Size=0x4
    struct _Huffman_WMV m_hufPCBPCYDec_HighRate;// Offset=0x3674 Size=0x28
    struct _Huffman_WMV m_hufPCBPCYDec_MidRate;// Offset=0x369c Size=0x28
    struct _Huffman_WMV m_hufPCBPCYDec_LowRate;// Offset=0x36c4 Size=0x28
    struct _Huffman_WMV m_hufDCTACInterDec_HghMt;// Offset=0x36ec Size=0x28
    struct _Huffman_WMV m_hufDCTACIntraDec_HghMt;// Offset=0x3714 Size=0x28
    struct _Huffman_WMV m_hufDCTACInterDec_Talking;// Offset=0x373c Size=0x28
    struct _Huffman_WMV m_hufDCTACIntraDec_Talking;// Offset=0x3764 Size=0x28
    struct _Huffman_WMV m_hufDCTACInterDec_MPEG4;// Offset=0x378c Size=0x28
    struct _Huffman_WMV m_hufDCTACIntraDec_MPEG4;// Offset=0x37b4 Size=0x28
    struct _CDCTTableInfo_Dec InterDCTTableInfo_Dec_HghMt;// Offset=0x37dc Size=0x24
    struct _CDCTTableInfo_Dec IntraDCTTableInfo_Dec_HghMt;// Offset=0x3800 Size=0x24
    struct _CDCTTableInfo_Dec InterDCTTableInfo_Dec_Talking;// Offset=0x3824 Size=0x24
    struct _CDCTTableInfo_Dec IntraDCTTableInfo_Dec_Talking;// Offset=0x3848 Size=0x24
    struct _CDCTTableInfo_Dec InterDCTTableInfo_Dec_MPEG4;// Offset=0x386c Size=0x24
    struct _CDCTTableInfo_Dec IntraDCTTableInfo_Dec_MPEG4;// Offset=0x3890 Size=0x24
    struct _CDCTTableInfo_Dec * m_pInterDCTTableInfo_Dec;// Offset=0x38b4 Size=0x4
    struct _CDCTTableInfo_Dec * m_pIntraDCTTableInfo_Dec;// Offset=0x38b8 Size=0x4
    struct _CDCTTableInfo_Dec * m_pInterDCTTableInfo_Dec_Set[3];// Offset=0x38bc Size=0xc
    struct _CDCTTableInfo_Dec * m_pIntraDCTTableInfo_Dec_Set[3];// Offset=0x38c8 Size=0xc
    unsigned long m_iDCTACInterTableIndx;// Offset=0x38d4 Size=0x4
    unsigned long m_iDCTACIntraTableIndx;// Offset=0x38d8 Size=0x4
    void  ( * m_pWMVideoDecUpdateDstMB)(struct tagWMVDecInternalMember * ,unsigned char * ,unsigned char * ,unsigned char * ,unsigned char * ,int ,int ,int );// Offset=0x38dc Size=0x4
    void  ( * m_pWMVideoDecUpdateDstPartialMB)(struct tagWMVDecInternalMember * ,unsigned char * ,unsigned char * ,unsigned char * ,unsigned char * ,int ,int ,int ,int );// Offset=0x38e0 Size=0x4
    void  ( * m_pWMVideoDecUpdateDstBlk)(struct tagWMVDecInternalMember * ,unsigned char * ,unsigned char * ,unsigned char * ,unsigned char * ,int ,int ,int );// Offset=0x38e4 Size=0x4
    enum tagWMVDecodeStatus  ( * m_pDecodeI)(struct tagWMVDecInternalMember * );// Offset=0x38e8 Size=0x4
    enum tagWMVDecodeStatus  ( * m_pDecodeP)(struct tagWMVDecInternalMember * );// Offset=0x38ec Size=0x4
    enum tagWMVDecodeStatus  ( * m_pDecodeIMBAcPred)(struct tagWMVDecInternalMember * ,struct tagCWMVMBMode * ,unsigned char * ,unsigned char * ,unsigned char * ,short * ,short ** ,long ,long ,long );// Offset=0x38f0 Size=0x4
    enum tagWMVDecodeStatus  ( * m_pDecodePMB)(struct tagWMVDecInternalMember * ,struct tagCWMVMBMode * ,unsigned char * ,unsigned char * ,unsigned char * ,int ,int ,int ,int );// Offset=0x38f4 Size=0x4
    enum tagWMVDecodeStatus  ( * m_pDecodeMBOverheadOfIVOP)(struct tagWMVDecInternalMember * ,struct tagCWMVMBMode * ,int ,int );// Offset=0x38f8 Size=0x4
    enum tagWMVDecodeStatus  ( * m_pDecodeMBOverheadOfPVOP)(struct tagWMVDecInternalMember * ,struct tagCWMVMBMode * );// Offset=0x38fc Size=0x4
    enum tagWMVDecodeStatus  ( * m_pDecodeInverseInterBlockQuantize)(struct tagWMVDecInternalMember * ,struct _CDCTTableInfo_Dec * ,unsigned char * ,int );// Offset=0x3900 Size=0x4
    void  ( * m_pIntraIDCT_Dec)(unsigned char * ,int ,int * );// Offset=0x3904 Size=0x4
    void  ( * m_pInterIDCT_Dec)(union Buffer * ,union Buffer * ,int ,int );// Offset=0x3908 Size=0x4
    void  ( * m_pInter8x4IDCT_Dec)(union Buffer * ,int ,union Buffer * ,int );// Offset=0x390c Size=0x4
    void  ( * m_pInter4x8IDCT_Dec)(union Buffer * ,int ,union Buffer * ,int );// Offset=0x3910 Size=0x4
    void  ( * m_pMotionCompAndAddError)(struct tagWMVDecInternalMember * ,unsigned char * ,union Buffer * ,unsigned char * ,int ,long ,long ,int );// Offset=0x3914 Size=0x4
    void  ( * m_pMotionComp)(struct tagWMVDecInternalMember * ,unsigned char * ,unsigned char * ,int ,long ,long ,int );// Offset=0x3918 Size=0x4
    void  ( * m_pMotionCompAndAddErrorRndCtrlOn)(struct tagWMVDecInternalMember * ,unsigned char * ,union Buffer * ,unsigned char * ,int ,long ,long ,int );// Offset=0x391c Size=0x4
    void  ( * m_pMotionCompRndCtrlOn)(struct tagWMVDecInternalMember * ,unsigned char * ,unsigned char * ,int ,long ,long ,int );// Offset=0x3920 Size=0x4
    void  ( * m_pMotionCompAndAddErrorRndCtrlOff)(struct tagWMVDecInternalMember * ,unsigned char * ,union Buffer * ,unsigned char * ,int ,long ,long ,int );// Offset=0x3924 Size=0x4
    void  ( * m_pMotionCompRndCtrlOff)(struct tagWMVDecInternalMember * ,unsigned char * ,unsigned char * ,int ,long ,long ,int );// Offset=0x3928 Size=0x4
    void  ( * m_pMotionCompZero)(unsigned char * ,unsigned char * ,unsigned char * ,unsigned char * ,unsigned char * ,unsigned char * ,int ,int );// Offset=0x392c Size=0x4
    void  ( * m_pMotionCompUV)(struct tagWMVDecInternalMember * ,unsigned char * ,unsigned char * ,int ,long ,long ,int );// Offset=0x3930 Size=0x4
    void  ( * m_pMotionCompAndAddErrorUV)(struct tagWMVDecInternalMember * ,unsigned char * ,union Buffer * ,unsigned char * ,int ,long ,long ,int );// Offset=0x3934 Size=0x4
    void  ( * m_pMotionCompMixed)(struct tagWMVDecInternalMember * ,unsigned char * ,unsigned char * ,int ,long ,long ,int );// Offset=0x3938 Size=0x4
    void  ( * m_pMotionCompMixedAndAddError)(struct tagWMVDecInternalMember * ,unsigned char * ,union Buffer * ,unsigned char * ,int ,long ,long ,int );// Offset=0x393c Size=0x4
    void  ( * m_pFilterHorizontalEdge)(unsigned char * ,int ,int ,int );// Offset=0x3940 Size=0x4
    void  ( * m_pFilterVerticalEdge)(unsigned char * ,int ,int ,int );// Offset=0x3944 Size=0x4
    int  ( * m_pBlkAvgX8_MMX)(unsigned char * ,int ,int );// Offset=0x3948 Size=0x4
    unsigned long uiFCode;// Offset=0x394c Size=0x4
    int iRange;// Offset=0x3950 Size=0x4
    int iScaleFactor;// Offset=0x3954 Size=0x4
    int m_iClockRate;// Offset=0x3958 Size=0x4
    unsigned char __align2[4];// Offset=0x395c Size=0x4
    long long m_t;// Offset=0x3960 Size=0x8
    int m_iVPMBnum;// Offset=0x3968 Size=0x4
    unsigned char __align3[4];// Offset=0x396c Size=0x4
    long long m_tModuloBaseDecd;// Offset=0x3970 Size=0x8
    long long m_tModuloBaseDisp;// Offset=0x3978 Size=0x8
    long long m_tOldModuloBaseDecd;// Offset=0x3980 Size=0x8
    long long m_tOldModuloBaseDisp;// Offset=0x3988 Size=0x8
    int m_iNumBitsTimeIncr;// Offset=0x3990 Size=0x4
    long m_bResyncDisable;// Offset=0x3994 Size=0x4
    long m_bDeblockOn;// Offset=0x3998 Size=0x4
    long m_bDeringOn;// Offset=0x399c Size=0x4
    int m_bUseOldSetting;// Offset=0x39a0 Size=0x4
    long m_bRefreshDisplay_AllMB_Enable;// Offset=0x39a4 Size=0x4
    long m_bRefreshDisplay_AllMB;// Offset=0x39a8 Size=0x4
    int m_iRefreshDisplay_AllMB_Cnt;// Offset=0x39ac Size=0x4
    int m_iRefreshDisplay_AllMB_Period;// Offset=0x39b0 Size=0x4
    long m_bCopySkipMBToPostBuf;// Offset=0x39b4 Size=0x4
    long m_bDefaultColorSetting;// Offset=0x39b8 Size=0x4
    long m_bCPUQmoved;// Offset=0x39bc Size=0x4
    int m_iPostProcessMode;// Offset=0x39c0 Size=0x4
};

enum tagFrameType_WMV
{
    IVOP=0,
    PVOP=1
};

struct tagPackHuffmanCode_WMV// Size=0x4 (Id=2836)
{
    struct // Size=0x4 (Id=0)
    {
        unsigned long code:26;// Offset=0x0 Size=0x4 BitOffset=0x0 BitSize=0x1a
        unsigned long length:6;// Offset=0x0 Size=0x4 BitOffset=0x1a BitSize=0x6
    };
};

struct tagCVector// Size=0x2 (Id=2838)
{
    char x;// Offset=0x0 Size=0x1
    char y;// Offset=0x1 Size=0x1
};

struct tagCWMVMBMode// Size=0x8 (Id=2839)
{
    struct // Size=0x4 (Id=0)
    {
        unsigned long m_bSkip:1;// Offset=0x0 Size=0x4 BitOffset=0x0 BitSize=0x1
        unsigned long m_bCBPAllZero:1;// Offset=0x0 Size=0x4 BitOffset=0x1 BitSize=0x1
        unsigned long m_bZeroMV:1;// Offset=0x0 Size=0x4 BitOffset=0x2 BitSize=0x1
        unsigned long m_bBlkXformSwitchOn:1;// Offset=0x0 Size=0x4 BitOffset=0x3 BitSize=0x1
        unsigned long m_iMBXformMode:2;// Offset=0x0 Size=0x4 BitOffset=0x4 BitSize=0x2
        unsigned long m_iDCTTable_MB_Index:2;// Offset=0x0 Size=0x4 BitOffset=0x6 BitSize=0x2
        unsigned long m_iMVPredDirection:2;// Offset=0x0 Size=0x4 BitOffset=0x8 BitSize=0x2
        unsigned long m_dctMd:1;// Offset=0x0 Size=0x4 BitOffset=0xa BitSize=0x1
    };
    unsigned char m_rgbCodedBlockPattern;// Offset=0x4 Size=0x1
    unsigned char m_rgbDCTCoefPredPattern;// Offset=0x5 Size=0x1
};

struct tagEntry// Size=0xc (Id=3068)
{
    int sizeFront;// Offset=0x0 Size=0x4
    struct tagEntry * pEntryNext;// Offset=0x4 Size=0x4
    struct tagEntry * pEntryPrev;// Offset=0x8 Size=0x4
};

struct tagListHead// Size=0x8 (Id=3069)
{
    struct tagEntry * pEntryNext;// Offset=0x0 Size=0x4
    struct tagEntry * pEntryPrev;// Offset=0x4 Size=0x4
};

struct tagGroup// Size=0x204 (Id=3070)
{
    int cntEntries;// Offset=0x0 Size=0x4
    struct tagListHead listHead[64];// Offset=0x4 Size=0x200
};

struct tagRegion// Size=0x41c4 (Id=3071)
{
    int indGroupUse;// Offset=0x0 Size=0x4
    char cntRegionSize[64];// Offset=0x4 Size=0x40
    unsigned int bitvGroupHi[32];// Offset=0x44 Size=0x80
    unsigned int bitvGroupLo[32];// Offset=0xc4 Size=0x80
    struct tagGroup grpHeadList[32];// Offset=0x144 Size=0x4080
};

struct tagHeader// Size=0x14 (Id=3072)
{
    unsigned int bitvEntryHi;// Offset=0x0 Size=0x4
    unsigned int bitvEntryLo;// Offset=0x4 Size=0x4
    unsigned int bitvCommit;// Offset=0x8 Size=0x4
    void * pHeapData;// Offset=0xc Size=0x4
    struct tagRegion * pRegion;// Offset=0x10 Size=0x4
};

struct MsgPacket// Size=0x8 (Id=3521)
{
    struct MsgPacket * next;// Offset=0x0 Size=0x4
    unsigned char msgPriority;// Offset=0x4 Size=0x1
    unsigned char dummy[3];// Offset=0x5 Size=0x3
};


#endif // _WINDOWS_GDI_H_
