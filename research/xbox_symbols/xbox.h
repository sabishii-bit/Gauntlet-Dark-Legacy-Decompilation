#ifndef _XBOX_H_
#define _XBOX_H_

// Category: xbox
// Extracted from Xbox PDB symbols
// Total types: 45
// Note: Xbox symbols - may need adaptation for GameCube

enum AC97REGISTER
{
    AC97REG_RESET=0,
    AC97REG_FRONT_VOLUME=1,
    AC97REG_HEADPHONE_VOLUME=2,
    AC97REG_MONOOUT_VOLUME=3,
    AC97REG_MASTER_TONE=4,
    AC97REG_PCBEEP_VOLUME=5,
    AC97REG_PHONE_VOLUME=6,
    AC97REG_MIC_VOLUME=7,
    AC97REG_LINE_IN_VOLUME=8,
    AC97REG_CD_VOLUME=9,
    AC97REG_VIDEO_VOLUME=10,
    AC97REG_AUX_VOLUME=11,
    AC97REG_PCM_OUT_VOLUME=12,
    AC97REG_RECORD_SELECT=13,
    AC97REG_RECORD_GAIN=14,
    AC97REG_RECORD_GAIN_MIC=15,
    AC97REG_GENERAL=16,
    AC97REG_3D_CTRL=17,
    AC97REG_RESERVED0=18,
    AC97REG_POWERDOWN=19,
    AC97REG_EXT_AUDIO_ID=20,
    AC97REG_EXT_AUDIO_CTRL=21,
    AC97REG_FRONT_RATE=22,
    AC97REG_SURR_RATE=23,
    AC97REG_LFE_RATE=24,
    AC97REG_LR_RATE=25,
    AC97REG_MIC_RATE=26,
    AC97REG_6CH_VOL_CLFE=27,
    AC97REG_6CH_VOL_SURR=28,
    AC97REG_RESERVED1=29,
    AC97REG_EXT_MODEM_ID=30,
    AC97REG_EXT_MODEM_CTRL=31,
    AC97REG_LINE1_RATE=32,
    AC97REG_LINE2_RATE=33,
    AC97REG_HANDSET_RATE=34,
    AC97REG_LINE1_LEVEL=35,
    AC97REG_LINE2_LEVEL=36,
    AC97REG_HANDSET_LEVEL=37,
    AC97REG_GPIO_CONFIG=38,
    AC97REG_GPIO_POLARITY=39,
    AC97REG_GPIO_STICKY=40,
    AC97REG_GPIO_WAKEUP=41,
    AC97REG_GPIO_STATUS=42,
    AC97REG_MISC_MODEM_CTRL=43,
    AC97REG_RESERVED2=44,
    AC97REG_VENDOR_RESERVED0=45,
    AC97REG_VENDOR_RESERVED1=46,
    AC97REG_VENDOR_RESERVED2=47,
    AC97REG_VENDOR_RESERVED3=48,
    AC97REG_VENDOR_RESERVED4=49,
    AC97REG_VENDOR_RESERVED5=50,
    AC97REG_VENDOR_RESERVED6=51,
    AC97REG_VENDOR_RESERVED7=52,
    AC97REG_VENDOR_RESERVED8=53,
    AC97REG_VENDOR_RESERVED9=54,
    AC97REG_VENDOR_RESERVED10=55,
    AC97REG_VENDOR_RESERVED11=56,
    AC97REG_VENDOR_RESERVED12=57,
    AC97REG_VENDOR_RESERVED13=58,
    AC97REG_VENDOR_RESERVED14=59,
    AC97REG_VENDOR_RESERVED15=60,
    AC97REG_VENDOR_RESERVED16=61,
    AC97REG_VENDOR_ID1=62,
    AC97REG_VENDOR_ID2=63,
    AC97REG_INVALID=64
};

enum AC97CHANNELTYPE
{
    AC97_CHANNELTYPE_ANALOG=0,
    AC97_CHANNELTYPE_DIGITAL=1,
    AC97_CHANNELTYPE_COUNT=2
};

enum _XC_VALUE_INDEX
{
    XC_TIMEZONE_BIAS=0,
    XC_TZ_STD_NAME=1,
    XC_TZ_STD_DATE=2,
    XC_TZ_STD_BIAS=3,
    XC_TZ_DLT_NAME=4,
    XC_TZ_DLT_DATE=5,
    XC_TZ_DLT_BIAS=6,
    XC_LANGUAGE=7,
    XC_VIDEO_FLAGS=8,
    XC_AUDIO_FLAGS=9,
    XC_PARENTAL_CONTROL_GAMES=10,
    XC_PARENTAL_CONTROL_PASSWORD=11,
    XC_PARENTAL_CONTROL_MOVIES=12,
    XC_ONLINE_IP_ADDRESS=13,
    XC_ONLINE_DNS_ADDRESS=14,
    XC_ONLINE_DEFAULT_GATEWAY_ADDRESS=15,
    XC_ONLINE_SUBNET_ADDRESS=16,
    XC_MISC_FLAGS=17,
    XC_DVD_REGION=18,
    XC_MAX_OS=255,
    XC_FACTORY_START_INDEX=256,
    XC_FACTORY_SERIAL_NUMBER=256,
    XC_FACTORY_ETHERNET_ADDR=257,
    XC_FACTORY_ONLINE_KEY=258,
    XC_FACTORY_AV_REGION=259,
    XC_FACTORY_GAME_REGION=260,
    XC_MAX_FACTORY=511,
    XC_ENCRYPTED_SECTION=65534,
    XC_MAX_ALL=65535
};

struct _XDCS_ASYNC_DOWNLOAD_REQUEST// Size=0x1c (Id=143)
{
    unsigned long dwDeviceInstance;// Offset=0x0 Size=0x4
    void * pvBuffer;// Offset=0x4 Size=0x4
    unsigned long ulOffset;// Offset=0x8 Size=0x4
    unsigned long ulLength;// Offset=0xc Size=0x4
    unsigned long ulBytesRead;// Offset=0x10 Size=0x4
    unsigned long ulStatus;// Offset=0x14 Size=0x4
    void * hCompleteEvent;// Offset=0x18 Size=0x4
};

struct MCPXVPREGSET// Size=0x4000 (Id=152)
{
    unsigned long Reserved0000[4];// Offset=0x0 Size=0x10
    unsigned long PIOFree;// Offset=0x10 Size=0x4
    unsigned long PIOInfo;// Offset=0x14 Size=0x4
    unsigned long Reserved2[10];// Offset=0x18 Size=0x28
    unsigned long DMAPut;// Offset=0x40 Size=0x4
    unsigned long DMAGet;// Offset=0x44 Size=0x4
    unsigned long Reserved0048[46];// Offset=0x48 Size=0xb8
    unsigned long NoOperation;// Offset=0x100 Size=0x4
    unsigned long Synchronize;// Offset=0x104 Size=0x4
    unsigned long SetTime;// Offset=0x108 Size=0x4
    unsigned long SetProcessorMode;// Offset=0x10c Size=0x4
    unsigned long Reserved0110[4];// Offset=0x110 Size=0x10
    unsigned long SetAntecedentVoice;// Offset=0x120 Size=0x4
    unsigned long VoiceOn;// Offset=0x124 Size=0x4
    unsigned long VoiceOff;// Offset=0x128 Size=0x4
    unsigned long VoiceRelease;// Offset=0x12c Size=0x4
    unsigned long GetVoicePosition;// Offset=0x130 Size=0x4
    unsigned long Reserved0134[3];// Offset=0x134 Size=0xc
    unsigned long VoicePause;// Offset=0x140 Size=0x4
    unsigned long Reserved0144[7];// Offset=0x144 Size=0x1c
    unsigned long SetCurrentHRTFEntry;// Offset=0x160 Size=0x4
    unsigned long Reserved0164[7];// Offset=0x164 Size=0x1c
    unsigned long SetContextDMANotify;// Offset=0x180 Size=0x4
    unsigned long Reserved0184[2];// Offset=0x184 Size=0x8
    unsigned long SetCurrentSSLContextDMA;// Offset=0x18c Size=0x4
    unsigned long SetCurrentSSL;// Offset=0x190 Size=0x4
    unsigned long Reserved0194[27];// Offset=0x194 Size=0x6c
    unsigned long SetSubMixHeadroom[32];// Offset=0x200 Size=0x80
    unsigned long SetHRTFHeadroom;// Offset=0x280 Size=0x4
    unsigned long Reserved0284[3];// Offset=0x284 Size=0xc
    unsigned long SetHRTFSubmix[4];// Offset=0x290 Size=0x10
    unsigned long SetVolumeTracking;// Offset=0x2a0 Size=0x4
    unsigned long SetPitchTracking;// Offset=0x2a4 Size=0x4
    unsigned long SetHRTFTracking;// Offset=0x2a8 Size=0x4
    unsigned long SetITDTracking;// Offset=0x2ac Size=0x4
    unsigned long SetFilterTracking;// Offset=0x2b0 Size=0x4
    unsigned long Reserved02B4[3];// Offset=0x2b4 Size=0xc
    unsigned long SetHRTFSubmixes;// Offset=0x2c0 Size=0x4
    unsigned long Reserved02C4[13];// Offset=0x2c4 Size=0x34
    unsigned long SetCurrentVoice;// Offset=0x2f8 Size=0x4
    unsigned long VoiceLock;// Offset=0x2fc Size=0x4
    unsigned long SetVoiceCfgVBIN;// Offset=0x300 Size=0x4
    unsigned long SetVoiceCfgFMT;// Offset=0x304 Size=0x4
    unsigned long SetVoiceCfgENV0;// Offset=0x308 Size=0x4
    unsigned long SetVoiceCfgENVA;// Offset=0x30c Size=0x4
    unsigned long SetVoiceCfgENV1;// Offset=0x310 Size=0x4
    unsigned long SetVoiceCfgENVF;// Offset=0x314 Size=0x4
    unsigned long SetVoiceCfgMISC;// Offset=0x318 Size=0x4
    unsigned long SetVoiceTarHRTF;// Offset=0x31c Size=0x4
    unsigned long SetVoiceSSLA;// Offset=0x320 Size=0x4
    unsigned long Reserved0324[11];// Offset=0x324 Size=0x2c
    unsigned long SetVoiceCfgLFODLY;// Offset=0x350 Size=0x4
    unsigned long Reserved0354[2];// Offset=0x354 Size=0x8
    unsigned long SetVoiceSSLB;// Offset=0x35c Size=0x4
    unsigned long SetVoiceTarVOLA;// Offset=0x360 Size=0x4
    unsigned long SetVoiceTarVOLB;// Offset=0x364 Size=0x4
    unsigned long SetVoiceTarVOLC;// Offset=0x368 Size=0x4
    unsigned long SetVoiceLFOENV;// Offset=0x36c Size=0x4
    unsigned long SetVoiceLFOMOD;// Offset=0x370 Size=0x4
    unsigned long SetVoiceTarFCA;// Offset=0x374 Size=0x4
    unsigned long SetVoiceTarFCB;// Offset=0x378 Size=0x4
    unsigned long SetVoiceTarPitch;// Offset=0x37c Size=0x4
    unsigned long Reserved0360[8];// Offset=0x380 Size=0x20
    unsigned long SetVoiceCfgBufBase;// Offset=0x3a0 Size=0x4
    unsigned long SetVoiceCfgBufLBO;// Offset=0x3a4 Size=0x4
    unsigned long Reserved03A8[11];// Offset=0x3a8 Size=0x2c
    unsigned long SetVoiceBufCBOFrac;// Offset=0x3d4 Size=0x4
    unsigned long SetVoiceBufCBO;// Offset=0x3d8 Size=0x4
    unsigned long SetVoiceCfgBufEBO;// Offset=0x3dc Size=0x4
    unsigned long Reserved03E0[8];// Offset=0x3e0 Size=0x20
    unsigned long SetHRIR[15];// Offset=0x400 Size=0x3c
    unsigned long SetHRIRX;// Offset=0x43c Size=0x4
    unsigned long Reserved0440[112];// Offset=0x440 Size=0x1c0
    struct __unnamed SetSSLSegment[64];// Offset=0x600 Size=0x200
    unsigned long SetCurrentInBufSGEContextDMA;// Offset=0x800 Size=0x4
    unsigned long SetCurrentInBufSGE;// Offset=0x804 Size=0x4
    unsigned long SetCurrentInBufSGEOffset;// Offset=0x808 Size=0x4
    unsigned long Reserved080C[509];// Offset=0x80c Size=0x7f4
    struct __unnamed SetOutBuf[4];// Offset=0x1000 Size=0x20
    unsigned long Reserved1020[504];// Offset=0x1020 Size=0x7e0
    unsigned long SetCurrentOutBufSGE;// Offset=0x1800 Size=0x4
    unsigned long SetCurrentOutBufSGEContextDMA;// Offset=0x1804 Size=0x4
    unsigned long SetOutBufSGEOffset;// Offset=0x1808 Size=0x4
    unsigned long SetSGEVoiceNumHack;// Offset=0x180c Size=0x4
    unsigned long SetSGEFullLenHack;// Offset=0x1810 Size=0x4
    unsigned long SetSGEAddrHack;// Offset=0x1814 Size=0x4
    unsigned long Reserved1818[2554];// Offset=0x1818 Size=0x27e8
};

struct _XDEVICE_PREALLOC_TYPE// Size=0x8 (Id=167)
{
    struct _XPP_DEVICE_TYPE * DeviceType;// Offset=0x0 Size=0x4
    unsigned long dwPreallocCount;// Offset=0x4 Size=0x4
};

struct _XINPUT_POLLING_PARAMETERS// Size=0x4 (Id=191)
{
    struct // Size=0x1 (Id=0)
    {
        unsigned char fAutoPoll:1;// Offset=0x0 Size=0x1 BitOffset=0x0 BitSize=0x1
        unsigned char fInterruptOut:1;// Offset=0x0 Size=0x1 BitOffset=0x1 BitSize=0x1
        unsigned char ReservedMBZ1:6;// Offset=0x0 Size=0x1 BitOffset=0x2 BitSize=0x6
    };
    unsigned char bInputInterval;// Offset=0x1 Size=0x1
    unsigned char bOutputInterval;// Offset=0x2 Size=0x1
    unsigned char ReservedMBZ2;// Offset=0x3 Size=0x1
};

struct _XBOX_HARDWARE_INFO// Size=0x8 (Id=192)
{
    unsigned long Flags;// Offset=0x0 Size=0x4
    unsigned char GpuRevision;// Offset=0x4 Size=0x1
    unsigned char McpRevision;// Offset=0x5 Size=0x1
    unsigned char reserved[2];// Offset=0x6 Size=0x2
};

struct _XINPUT_FEEDBACK// Size=0x46 (Id=193)
{
    struct _XINPUT_FEEDBACK_HEADER Header;// Offset=0x0 Size=0x42
    struct _XINPUT_RUMBLE Rumble;// Offset=0x42 Size=0x4
};

struct _XINPUT_IR_REMOTE// Size=0x4 (Id=262)
{
    unsigned short wKeyCode;// Offset=0x0 Size=0x2
    unsigned short wTimeDelta;// Offset=0x2 Size=0x2
};

struct xbox_adpcmwaveformat_tag// Size=0x14 (Id=303)
{
    struct tWAVEFORMATEX wfx;// Offset=0x0 Size=0x12
    unsigned short wSamplesPerBlock;// Offset=0x12 Size=0x2
};

struct _XINPUT_STATE// Size=0x16 (Id=335)
{
    unsigned long dwPacketNumber;// Offset=0x0 Size=0x4
    struct _XINPUT_GAMEPAD Gamepad;// Offset=0x4 Size=0x12
};

struct xbox_adpcmwaveformat_tag// Size=0x14 (Id=339)
{
    struct tWAVEFORMATEX wfx;// Offset=0x0 Size=0x12
    unsigned short wSamplesPerBlock;// Offset=0x12 Size=0x2
};

struct _XINPUT_GAMEPAD// Size=0x12 (Id=366)
{
    unsigned short wButtons;// Offset=0x0 Size=0x2
    unsigned char bAnalogButtons[8];// Offset=0x2 Size=0x8
    short sThumbLX;// Offset=0xa Size=0x2
    short sThumbLY;// Offset=0xc Size=0x2
    short sThumbRX;// Offset=0xe Size=0x2
    short sThumbRY;// Offset=0x10 Size=0x2
};

struct XMediaObjectVtbl// Size=0x1c (Id=391)
{
    unsigned long  ( * AddRef)(struct XMediaObject * );// Offset=0x0 Size=0x4
    unsigned long  ( * Release)(struct XMediaObject * );// Offset=0x4 Size=0x4
    long  ( * GetInfo)(struct XMediaObject * ,struct _XMEDIAINFO * );// Offset=0x8 Size=0x4
    long  ( * GetStatus)(struct XMediaObject * ,unsigned long * );// Offset=0xc Size=0x4
    long  ( * Process)(struct XMediaObject * ,struct _XMEDIAPACKET * ,struct _XMEDIAPACKET * );// Offset=0x10 Size=0x4
    long  ( * Discontinuity)(struct XMediaObject * );// Offset=0x14 Size=0x4
    long  ( * Flush)(struct XMediaObject * );// Offset=0x18 Size=0x4
};

struct _XINPUT_STATE_INTERNAL// Size=0x18 (Id=458)
{
    unsigned long dwPacketNumber;// Offset=0x0 Size=0x4
    union // Size=0x12 (Id=0)
    {
        struct _XINPUT_GAMEPAD Gamepad;// Offset=0x4 Size=0x12
        struct _XINPUT_IR_REMOTE IrRemote;// Offset=0x4 Size=0x4
    };
};

struct MCPXVPREGSET// Size=0x4000 (Id=491)
{
    unsigned long Reserved0000[4];// Offset=0x0 Size=0x10
    unsigned long PIOFree;// Offset=0x10 Size=0x4
    unsigned long PIOInfo;// Offset=0x14 Size=0x4
    unsigned long Reserved2[10];// Offset=0x18 Size=0x28
    unsigned long DMAPut;// Offset=0x40 Size=0x4
    unsigned long DMAGet;// Offset=0x44 Size=0x4
    unsigned long Reserved0048[46];// Offset=0x48 Size=0xb8
    unsigned long NoOperation;// Offset=0x100 Size=0x4
    unsigned long Synchronize;// Offset=0x104 Size=0x4
    unsigned long SetTime;// Offset=0x108 Size=0x4
    unsigned long SetProcessorMode;// Offset=0x10c Size=0x4
    unsigned long Reserved0110[4];// Offset=0x110 Size=0x10
    unsigned long SetAntecedentVoice;// Offset=0x120 Size=0x4
    unsigned long VoiceOn;// Offset=0x124 Size=0x4
    unsigned long VoiceOff;// Offset=0x128 Size=0x4
    unsigned long VoiceRelease;// Offset=0x12c Size=0x4
    unsigned long GetVoicePosition;// Offset=0x130 Size=0x4
    unsigned long Reserved0134[3];// Offset=0x134 Size=0xc
    unsigned long VoicePause;// Offset=0x140 Size=0x4
    unsigned long Reserved0144[7];// Offset=0x144 Size=0x1c
    unsigned long SetCurrentHRTFEntry;// Offset=0x160 Size=0x4
    unsigned long Reserved0164[7];// Offset=0x164 Size=0x1c
    unsigned long SetContextDMANotify;// Offset=0x180 Size=0x4
    unsigned long Reserved0184[2];// Offset=0x184 Size=0x8
    unsigned long SetCurrentSSLContextDMA;// Offset=0x18c Size=0x4
    unsigned long SetCurrentSSL;// Offset=0x190 Size=0x4
    unsigned long Reserved0194[27];// Offset=0x194 Size=0x6c
    unsigned long SetSubMixHeadroom[32];// Offset=0x200 Size=0x80
    unsigned long SetHRTFHeadroom;// Offset=0x280 Size=0x4
    unsigned long Reserved0284[3];// Offset=0x284 Size=0xc
    unsigned long SetHRTFSubmix[4];// Offset=0x290 Size=0x10
    unsigned long SetVolumeTracking;// Offset=0x2a0 Size=0x4
    unsigned long SetPitchTracking;// Offset=0x2a4 Size=0x4
    unsigned long SetHRTFTracking;// Offset=0x2a8 Size=0x4
    unsigned long SetITDTracking;// Offset=0x2ac Size=0x4
    unsigned long SetFilterTracking;// Offset=0x2b0 Size=0x4
    unsigned long Reserved02B4[3];// Offset=0x2b4 Size=0xc
    unsigned long SetHRTFSubmixes;// Offset=0x2c0 Size=0x4
    unsigned long Reserved02C4[13];// Offset=0x2c4 Size=0x34
    unsigned long SetCurrentVoice;// Offset=0x2f8 Size=0x4
    unsigned long VoiceLock;// Offset=0x2fc Size=0x4
    unsigned long SetVoiceCfgVBIN;// Offset=0x300 Size=0x4
    unsigned long SetVoiceCfgFMT;// Offset=0x304 Size=0x4
    unsigned long SetVoiceCfgENV0;// Offset=0x308 Size=0x4
    unsigned long SetVoiceCfgENVA;// Offset=0x30c Size=0x4
    unsigned long SetVoiceCfgENV1;// Offset=0x310 Size=0x4
    unsigned long SetVoiceCfgENVF;// Offset=0x314 Size=0x4
    unsigned long SetVoiceCfgMISC;// Offset=0x318 Size=0x4
    unsigned long SetVoiceTarHRTF;// Offset=0x31c Size=0x4
    unsigned long SetVoiceSSLA;// Offset=0x320 Size=0x4
    unsigned long Reserved0324[11];// Offset=0x324 Size=0x2c
    unsigned long SetVoiceCfgLFODLY;// Offset=0x350 Size=0x4
    unsigned long Reserved0354[2];// Offset=0x354 Size=0x8
    unsigned long SetVoiceSSLB;// Offset=0x35c Size=0x4
    unsigned long SetVoiceTarVOLA;// Offset=0x360 Size=0x4
    unsigned long SetVoiceTarVOLB;// Offset=0x364 Size=0x4
    unsigned long SetVoiceTarVOLC;// Offset=0x368 Size=0x4
    unsigned long SetVoiceLFOENV;// Offset=0x36c Size=0x4
    unsigned long SetVoiceLFOMOD;// Offset=0x370 Size=0x4
    unsigned long SetVoiceTarFCA;// Offset=0x374 Size=0x4
    unsigned long SetVoiceTarFCB;// Offset=0x378 Size=0x4
    unsigned long SetVoiceTarPitch;// Offset=0x37c Size=0x4
    unsigned long Reserved0360[8];// Offset=0x380 Size=0x20
    unsigned long SetVoiceCfgBufBase;// Offset=0x3a0 Size=0x4
    unsigned long SetVoiceCfgBufLBO;// Offset=0x3a4 Size=0x4
    unsigned long Reserved03A8[11];// Offset=0x3a8 Size=0x2c
    unsigned long SetVoiceBufCBOFrac;// Offset=0x3d4 Size=0x4
    unsigned long SetVoiceBufCBO;// Offset=0x3d8 Size=0x4
    unsigned long SetVoiceCfgBufEBO;// Offset=0x3dc Size=0x4
    unsigned long Reserved03E0[8];// Offset=0x3e0 Size=0x20
    unsigned long SetHRIR[15];// Offset=0x400 Size=0x3c
    unsigned long SetHRIRX;// Offset=0x43c Size=0x4
    unsigned long Reserved0440[112];// Offset=0x440 Size=0x1c0
    struct __unnamed SetSSLSegment[64];// Offset=0x600 Size=0x200
    unsigned long SetCurrentInBufSGEContextDMA;// Offset=0x800 Size=0x4
    unsigned long SetCurrentInBufSGE;// Offset=0x804 Size=0x4
    unsigned long SetCurrentInBufSGEOffset;// Offset=0x808 Size=0x4
    unsigned long Reserved080C[509];// Offset=0x80c Size=0x7f4
    struct __unnamed SetOutBuf[4];// Offset=0x1000 Size=0x20
    unsigned long Reserved1020[504];// Offset=0x1020 Size=0x7e0
    unsigned long SetCurrentOutBufSGE;// Offset=0x1800 Size=0x4
    unsigned long SetCurrentOutBufSGEContextDMA;// Offset=0x1804 Size=0x4
    unsigned long SetOutBufSGEOffset;// Offset=0x1808 Size=0x4
    unsigned long SetSGEVoiceNumHack;// Offset=0x180c Size=0x4
    unsigned long SetSGEFullLenHack;// Offset=0x1810 Size=0x4
    unsigned long SetSGEAddrHack;// Offset=0x1814 Size=0x4
    unsigned long Reserved1818[2554];// Offset=0x1818 Size=0x27e8
};

struct _XINPUT_CAPABILITIES// Size=0x19 (Id=513)
{
    unsigned char SubType;// Offset=0x0 Size=0x1
    unsigned short Reserved;// Offset=0x1 Size=0x2
    union __unnamed In;// Offset=0x3 Size=0x12
    union __unnamed Out;// Offset=0x15 Size=0x4
};

struct _XINPUT_FEEDBACK_HEADER// Size=0x42 (Id=522)
{
    unsigned long dwStatus;// Offset=0x0 Size=0x4
    void * hEvent;// Offset=0x4 Size=0x4
    unsigned char Reserved[58];// Offset=0x8 Size=0x3a
};

struct _XBOX_KRNL_VERSION// Size=0x8 (Id=566)
{
    unsigned short Major;// Offset=0x0 Size=0x2
    unsigned short Minor;// Offset=0x2 Size=0x2
    unsigned short Build;// Offset=0x4 Size=0x2
    unsigned short Qfe;// Offset=0x6 Size=0x2
};

struct _XDCS_DVD_CODE_INFORMATION// Size=0x6 (Id=582)
{
    unsigned short bcdVersion;// Offset=0x0 Size=0x2
    unsigned long dwCodeLength;// Offset=0x2 Size=0x4
};

struct XNetStartupParams// Size=0xb (Id=624)
{
    unsigned char cfgSizeOfStruct;// Offset=0x0 Size=0x1
    unsigned char cfgFlags;// Offset=0x1 Size=0x1
    unsigned char cfgPrivatePoolSizeInPages;// Offset=0x2 Size=0x1
    unsigned char cfgEnetReceiveQueueLength;// Offset=0x3 Size=0x1
    unsigned char cfgIpFragMaxSimultaneous;// Offset=0x4 Size=0x1
    unsigned char cfgIpFragMaxPacketDiv256;// Offset=0x5 Size=0x1
    unsigned char cfgSockMaxSockets;// Offset=0x6 Size=0x1
    unsigned char cfgSockDefaultRecvBufsizeInK;// Offset=0x7 Size=0x1
    unsigned char cfgSockDefaultSendBufsizeInK;// Offset=0x8 Size=0x1
    unsigned char cfgKeyRegMax;// Offset=0x9 Size=0x1
    unsigned char cfgSecRegMax;// Offset=0xa Size=0x1
};

struct AC97PACKET// Size=0x20 (Id=737)
{
    struct _LIST_ENTRY leListEntry;// Offset=0x0 Size=0x8
    struct _XMEDIAPACKET xmp;// Offset=0x8 Size=0x18
};

struct XMediaObject// Size=0x4 (Id=750)
{
    struct XMediaObjectVtbl * lpVtbl;// Offset=0x0 Size=0x4
};

struct _XINPUT_RUMBLE// Size=0x4 (Id=857)
{
    unsigned short wLeftMotorSpeed;// Offset=0x0 Size=0x2
    unsigned short wRightMotorSpeed;// Offset=0x2 Size=0x2
};

struct AC97PACKET// Size=0x20 (Id=949)
{
    struct _LIST_ENTRY leListEntry;// Offset=0x0 Size=0x8
    struct _XMEDIAPACKET xmp;// Offset=0x8 Size=0x18
};

union _XINPUT_CAPABILITIES::__unnamed// Size=0x12 (Id=1223)
{
    struct _XINPUT_GAMEPAD Gamepad;// Offset=0x0 Size=0x12
};

union _XINPUT_CAPABILITIES::__unnamed// Size=0x4 (Id=1224)
{
    struct _XINPUT_RUMBLE Rumble;// Offset=0x0 Size=0x4
};

struct _XINPUT_CAPABILITIES// Size=0x19 (Id=1225)
{
    unsigned char SubType;// Offset=0x0 Size=0x1
    unsigned short Reserved;// Offset=0x1 Size=0x2
    union _XINPUT_CAPABILITIES::__unnamed In;// Offset=0x3 Size=0x12
    union _XINPUT_CAPABILITIES::__unnamed Out;// Offset=0x15 Size=0x4
};

struct XMediaObject// Size=0x4 (Id=1287)
{
    unsigned long AddRef();
    unsigned long Release();
    long GetInfo(struct _XMEDIAINFO * );
    long GetStatus(unsigned long * );
    long Process(struct _XMEDIAPACKET * ,struct _XMEDIAPACKET * );
    long Discontinuity();
    long Flush();
    void XMediaObject(struct XMediaObject & );
    void XMediaObject();// Offset=0x0 Size=0x3
};

struct XMediaObject// Size=0x4 (Id=1288)
{
    unsigned long AddRef();
    unsigned long Release();
    long GetInfo(struct _XMEDIAINFO * );
    long GetStatus(unsigned long * );
    long Process(struct _XMEDIAPACKET * ,struct _XMEDIAPACKET * );
    long Discontinuity();
    long Flush();
    void XMediaObject(struct XMediaObject & );
    void XMediaObject();// Offset=0x0 Size=0x3
};

struct _XBOX_HARDWARE_INFO// Size=0x8 (Id=2389)
{
    unsigned long Flags;// Offset=0x0 Size=0x4
    unsigned char GpuRevision;// Offset=0x4 Size=0x1
    unsigned char McpRevision;// Offset=0x5 Size=0x1
    unsigned char reserved[2];// Offset=0x6 Size=0x2
};

struct _XINPUT_FEEDBACK_HEADER_INTERNAL// Size=0x3a (Id=2432)
{
    struct _XID_OPEN_DEVICE * OpenDevice;// Offset=0x0 Size=0x4
    struct _KEVENT * CompletionEvent;// Offset=0x4 Size=0x4
    union _URB Urb;// Offset=0x8 Size=0x30
    unsigned char bReportId;// Offset=0x38 Size=0x1
    unsigned char bSize;// Offset=0x39 Size=0x1
};

struct _XINPUT_FEEDBACK_INTERNAL// Size=0x42 (Id=2435)
{
    unsigned long dwStatus;// Offset=0x0 Size=0x4
    void * hEvent;// Offset=0x4 Size=0x4
    struct _XINPUT_FEEDBACK_HEADER_INTERNAL Internal;// Offset=0x8 Size=0x3a
};

struct _XINPUT_KEYBOARD// Size=0x8 (Id=2436)
{
    unsigned char Modifiers;// Offset=0x0 Size=0x1
    unsigned char Reserved;// Offset=0x1 Size=0x1
    unsigned char Keys[6];// Offset=0x2 Size=0x6
};

struct _XINPUT_KEYBOARD_LEDS// Size=0x1 (Id=2439)
{
    unsigned char LedStates;// Offset=0x0 Size=0x1
};

struct _XBOX_CACHE_DB_SECTOR// Size=0x1fc (Id=2476)
{
    unsigned long SectorBeginSignature;// Offset=0x0 Size=0x4
    unsigned long Version;// Offset=0x4 Size=0x4
    unsigned char Data[496];// Offset=0x8 Size=0x1f0
    unsigned long SectorEndSignature;// Offset=0x1f8 Size=0x4
};

struct _XBOX_TIMEZONE_DATE// Size=0x4 (Id=2484)
{
    unsigned char Month;// Offset=0x0 Size=0x1
    unsigned char Day;// Offset=0x1 Size=0x1
    unsigned char DayOfWeek;// Offset=0x2 Size=0x1
    unsigned char Hour;// Offset=0x3 Size=0x1
};

struct _XBOX_USER_SETTINGS// Size=0x60 (Id=2485)
{
    unsigned long Checksum;// Offset=0x0 Size=0x4
    long TimeZoneBias;// Offset=0x4 Size=0x4
    char TimeZoneStdName[4];// Offset=0x8 Size=0x4
    char TimeZoneDltName[4];// Offset=0xc Size=0x4
    unsigned long Reserved1[2];// Offset=0x10 Size=0x8
    struct _XBOX_TIMEZONE_DATE TimeZoneStdDate;// Offset=0x18 Size=0x4
    struct _XBOX_TIMEZONE_DATE TimeZoneDltDate;// Offset=0x1c Size=0x4
    unsigned long Reserved2[2];// Offset=0x20 Size=0x8
    long TimeZoneStdBias;// Offset=0x28 Size=0x4
    long TimeZoneDltBias;// Offset=0x2c Size=0x4
    unsigned long Language;// Offset=0x30 Size=0x4
    unsigned long VideoFlags;// Offset=0x34 Size=0x4
    unsigned long AudioFlags;// Offset=0x38 Size=0x4
    unsigned long ParentalControlGames;// Offset=0x3c Size=0x4
    unsigned long ParentalControlPassword;// Offset=0x40 Size=0x4
    unsigned long ParentalControlMovies;// Offset=0x44 Size=0x4
    unsigned long OnlineIpAddress;// Offset=0x48 Size=0x4
    unsigned long OnlineDnsAddress;// Offset=0x4c Size=0x4
    unsigned long OnlineDefaultGatewayAddress;// Offset=0x50 Size=0x4
    unsigned long OnlineSubnetMask;// Offset=0x54 Size=0x4
    unsigned long MiscFlags;// Offset=0x58 Size=0x4
    unsigned long DvdRegion;// Offset=0x5c Size=0x4
};

struct _XBOX_TIMEZONE_DATE// Size=0x4 (Id=2486)
{
    unsigned char Month;// Offset=0x0 Size=0x1
    unsigned char Day;// Offset=0x1 Size=0x1
    unsigned char DayOfWeek;// Offset=0x2 Size=0x1
    unsigned char Hour;// Offset=0x3 Size=0x1
};

class XGRAPHICS::XGInternalSwizzler<unsigned short>// Size=0x1 (Id=2576)
{
    union // Size=0xcc (Id=0)
    {
        void Swizzle1x1Column(struct XGRAPHICS::XGINTERNALSWIZZLE * );// Offset=0x0 Size=0x83
        void Unswizzle1x1Column(struct XGRAPHICS::XGINTERNALSWIZZLE * );// Offset=0x0 Size=0x83
        void Swizzle2x1(struct XGRAPHICS::XGINTERNALSWIZZLE * );// Offset=0x0 Size=0xcc
        void Unswizzle2x1(struct XGRAPHICS::XGINTERNALSWIZZLE * );// Offset=0x0 Size=0xcc
    };
};

class XGRAPHICS::XGInternalSwizzler<__int64>// Size=0x1 (Id=2577)
{
    public void Swizzle1x1Column(struct XGRAPHICS::XGINTERNALSWIZZLE * );
    public void Unswizzle1x1Column(struct XGRAPHICS::XGINTERNALSWIZZLE * );
    union // Size=0xdc (Id=0)
    {
        void Swizzle2x1(struct XGRAPHICS::XGINTERNALSWIZZLE * );// Offset=0x0 Size=0xdc
        void Unswizzle2x1(struct XGRAPHICS::XGINTERNALSWIZZLE * );// Offset=0x0 Size=0xdc
    };
};

class XGRAPHICS::XGInternalSwizzler<unsigned char>// Size=0x1 (Id=2578)
{
    union // Size=0x7b (Id=0)
    {
        void Swizzle1x1Column(struct XGRAPHICS::XGINTERNALSWIZZLE * );// Offset=0x0 Size=0x7b
        void Unswizzle1x1Column(struct XGRAPHICS::XGINTERNALSWIZZLE * );// Offset=0x0 Size=0x7b
    };
    public void Swizzle2x1(struct XGRAPHICS::XGINTERNALSWIZZLE * );
    public void Unswizzle2x1(struct XGRAPHICS::XGINTERNALSWIZZLE * );
};

struct XGRAPHICS::XGINTERNALSWIZZLE// Size=0x6c (Id=2579)
{
    union // Size=0x6c (Id=0)
    {
        void * pSource;// Offset=0x0 Size=0x4
        unsigned long Pitch;// Offset=0x4 Size=0x4
        unsigned long SLeft;// Offset=0x8 Size=0x4
        unsigned long STop;// Offset=0xc Size=0x4
        unsigned long RWidth;// Offset=0x10 Size=0x4
        unsigned long RHeight;// Offset=0x14 Size=0x4
        void * pDest;// Offset=0x18 Size=0x4
        unsigned long Width;// Offset=0x1c Size=0x4
        unsigned long Height;// Offset=0x20 Size=0x4
        unsigned long DTop;// Offset=0x24 Size=0x4
        unsigned long DLeft;// Offset=0x28 Size=0x4
        int xOff;// Offset=0x2c Size=0x4
        int yOff;// Offset=0x30 Size=0x4
        int uWidth;// Offset=0x34 Size=0x4
        int uHeight;// Offset=0x38 Size=0x4
        int bpp;// Offset=0x3c Size=0x4
        class Swizzler swiz;// Offset=0x40 Size=0x24
        unsigned long mask[2];// Offset=0x64 Size=0x8
        void XGINTERNALSWIZZLE();// Offset=0x0 Size=0xf
    };
};

class XGRAPHICS::XGInternalSwizzler<unsigned long>// Size=0x1 (Id=2580)
{
    union // Size=0xd4 (Id=0)
    {
        void Swizzle1x1Column(struct XGRAPHICS::XGINTERNALSWIZZLE * );// Offset=0x0 Size=0x81
        void Unswizzle1x1Column(struct XGRAPHICS::XGINTERNALSWIZZLE * );// Offset=0x0 Size=0x81
        void Swizzle2x1(struct XGRAPHICS::XGINTERNALSWIZZLE * );// Offset=0x0 Size=0xd4
        void Unswizzle2x1(struct XGRAPHICS::XGINTERNALSWIZZLE * );// Offset=0x0 Size=0xd4
    };
};


#endif // _XBOX_H_
