#ifndef _AUDIO_H_
#define _AUDIO_H_

// Category: audio
// Extracted from Xbox PDB symbols
// Total types: 301
// Note: Xbox symbols - may need adaptation for GameCube

enum DSOUND_POOL_TAG
{
    DSOUND_OBJECT_POOL_TAG=1651462980,
    DSOUND_DATA_POOL_TAG=1633964868
};

enum DSOUND_ALLOCATOR_TAG
{
    DSOUND_ALLOCATOR_POOL=1819242352,
    DSOUND_ALLOCATOR_PHYS=1937336432,
    DSOUND_ALLOCATOR_SLOP=1886350451
};

struct _DSEFFECTIMAGELOC// Size=0x8 (Id=145)
{
    unsigned long dwI3DL2ReverbIndex;// Offset=0x0 Size=0x4
    unsigned long dwCrosstalkIndex;// Offset=0x4 Size=0x4
};

struct _DSFX_FLANGE_MONO_STATE// Size=0x20 (Id=178)
{
    unsigned long dwScratchOffset;// Offset=0x0 Size=0x4
    unsigned long dwScratchLength;// Offset=0x4 Size=0x4
    unsigned long dwYMemoryOffset;// Offset=0x8 Size=0x4
    unsigned long dwYMemoryLength;// Offset=0xc Size=0x4
    unsigned long dwFlags;// Offset=0x10 Size=0x4
    unsigned long dwInMixbinPtrs[2];// Offset=0x14 Size=0x8
    unsigned long dwOutMixbinPtrs[1];// Offset=0x1c Size=0x4
};

struct _DSFX_ECHO_STEREO_STATE// Size=0x24 (Id=195)
{
    unsigned long dwScratchOffset;// Offset=0x0 Size=0x4
    unsigned long dwScratchLength;// Offset=0x4 Size=0x4
    unsigned long dwYMemoryOffset;// Offset=0x8 Size=0x4
    unsigned long dwYMemoryLength;// Offset=0xc Size=0x4
    unsigned long dwFlags;// Offset=0x10 Size=0x4
    unsigned long dwInMixbinPtrs[2];// Offset=0x14 Size=0x8
    unsigned long dwOutMixbinPtrs[2];// Offset=0x1c Size=0x8
};

struct HRTFLISTENER// Size=0x70 (Id=200)
{
    struct _D3DVECTOR vNormOrient;// Offset=0x0 Size=0xc
    struct PAN3DSPEAKER aSpeakers[5];// Offset=0xc Size=0x64
};

struct _DSFX_ECHO_STEREO_PARAMS// Size=0x28 (Id=206)
{
    struct _DSFX_ECHO_STEREO_STATE State;// Offset=0x0 Size=0x24
    unsigned long dwGain;// Offset=0x24 Size=0x4
};

struct _DSFX_SPLITTER_PARAMS// Size=0x5c (Id=212)
{
    struct _DSFX_SPLITTER_STATE State;// Offset=0x0 Size=0x38
    unsigned long dwNumOutputs;// Offset=0x38 Size=0x4
    unsigned long dwGains[8];// Offset=0x3c Size=0x20
};

struct _DS3DBUFFER// Size=0x4c (Id=225)
{
    unsigned long dwSize;// Offset=0x0 Size=0x4
    struct XGVECTOR3 vPosition;// Offset=0x4 Size=0xc
    struct XGVECTOR3 vVelocity;// Offset=0x10 Size=0xc
    unsigned long dwInsideConeAngle;// Offset=0x1c Size=0x4
    unsigned long dwOutsideConeAngle;// Offset=0x20 Size=0x4
    struct XGVECTOR3 vConeOrientation;// Offset=0x24 Size=0xc
    long lConeOutsideVolume;// Offset=0x30 Size=0x4
    float flMinDistance;// Offset=0x34 Size=0x4
    float flMaxDistance;// Offset=0x38 Size=0x4
    unsigned long dwMode;// Offset=0x3c Size=0x4
    float flDistanceFactor;// Offset=0x40 Size=0x4
    float flRolloffFactor;// Offset=0x44 Size=0x4
    float flDopplerFactor;// Offset=0x48 Size=0x4
    void _DS3DBUFFER();
};

struct _DSFX_FLANGE_MONO_PARAMS// Size=0x28 (Id=244)
{
    struct _DSFX_FLANGE_MONO_STATE State;// Offset=0x0 Size=0x20
    unsigned long dwFeedback;// Offset=0x20 Size=0x4
    unsigned long dwModScale;// Offset=0x24 Size=0x4
};

struct _DSFX_OSCILLATOR_PARAMS// Size=0x38 (Id=248)
{
    struct _DSFX_OSCILLATOR_STATE State;// Offset=0x0 Size=0x24
    unsigned long dwNumOutputs;// Offset=0x24 Size=0x4
    unsigned long adwFrequency[4];// Offset=0x28 Size=0x10
};

struct _DSFX_MINIREVERB_STATE// Size=0x48 (Id=255)
{
    unsigned long dwScratchOffset;// Offset=0x0 Size=0x4
    unsigned long dwScratchLength;// Offset=0x4 Size=0x4
    unsigned long dwYMemoryOffset;// Offset=0x8 Size=0x4
    unsigned long dwYMemoryLength;// Offset=0xc Size=0x4
    unsigned long dwFlags;// Offset=0x10 Size=0x4
    unsigned long dwInMixbinPtrs[1];// Offset=0x14 Size=0x4
    unsigned long dwOutMixbinPtrs[12];// Offset=0x18 Size=0x30
};

struct AIFFSOUNDHDR// Size=0x8 (Id=258)
{
    unsigned long dwOffset;// Offset=0x0 Size=0x4
    unsigned long dwBlockSize;// Offset=0x4 Size=0x4
};

struct _DSFX_AMPMOD_STEREO_PARAMS// Size=0x28 (Id=263)
{
    struct _DSFX_AMPMOD_STEREO_STATE State;// Offset=0x0 Size=0x28
};

struct IDirectSoundStreamVtbl// Size=0x1c (Id=266)
{
    unsigned long  ( * AddRef)(struct IDirectSoundStream * );// Offset=0x0 Size=0x4
    unsigned long  ( * Release)(struct IDirectSoundStream * );// Offset=0x4 Size=0x4
    long  ( * GetInfo)(struct IDirectSoundStream * ,struct _XMEDIAINFO * );// Offset=0x8 Size=0x4
    long  ( * GetStatus)(struct IDirectSoundStream * ,unsigned long * );// Offset=0xc Size=0x4
    long  ( * Process)(struct IDirectSoundStream * ,struct _XMEDIAPACKET * ,struct _XMEDIAPACKET * );// Offset=0x10 Size=0x4
    long  ( * Discontinuity)(struct IDirectSoundStream * );// Offset=0x14 Size=0x4
    long  ( * Flush)(struct IDirectSoundStream * );// Offset=0x18 Size=0x4
};

struct HRTFVOICE// Size=0xa8 (Id=269)
{
    unsigned long dwChangeMask;// Offset=0x0 Size=0x4
    unsigned long dwMixBinValidMask;// Offset=0x4 Size=0x4
    unsigned long dwMixBinChangeMask;// Offset=0x8 Size=0x4
    long lDistanceVolume;// Offset=0xc Size=0x4
    long lConeVolume;// Offset=0x10 Size=0x4
    long lFrontVolume;// Offset=0x14 Size=0x4
    long lRearVolume;// Offset=0x18 Size=0x4
    long lDopplerPitch;// Offset=0x1c Size=0x4
    struct HRTFFILTERPAIR FilterPair;// Offset=0x20 Size=0x8
    long alMixBinVolumes[32];// Offset=0x28 Size=0x80
};

struct _DSFX_FLANGE_STEREO_STATE// Size=0x28 (Id=279)
{
    unsigned long dwScratchOffset;// Offset=0x0 Size=0x4
    unsigned long dwScratchLength;// Offset=0x4 Size=0x4
    unsigned long dwYMemoryOffset;// Offset=0x8 Size=0x4
    unsigned long dwYMemoryLength;// Offset=0xc Size=0x4
    unsigned long dwFlags;// Offset=0x10 Size=0x4
    unsigned long dwInMixbinPtrs[3];// Offset=0x14 Size=0xc
    unsigned long dwOutMixbinPtrs[2];// Offset=0x20 Size=0x8
};

struct _DSFX_I3DL2REVERB_STATE// Size=0xa8 (Id=294)
{
    unsigned long dwScratchOffset;// Offset=0x0 Size=0x4
    unsigned long dwScratchLength;// Offset=0x4 Size=0x4
    unsigned long dwYMemoryOffset;// Offset=0x8 Size=0x4
    unsigned long dwYMemoryLength;// Offset=0xc Size=0x4
    unsigned long dwFlags;// Offset=0x10 Size=0x4
    unsigned long dwInMixbinPtrs[2];// Offset=0x14 Size=0x8
    unsigned long dwOutMixbinPtrs[35];// Offset=0x1c Size=0x8c
};

struct _DSFX_FLANGE_STEREO_PARAMS// Size=0x30 (Id=305)
{
    struct _DSFX_FLANGE_STEREO_STATE State;// Offset=0x0 Size=0x28
    unsigned long dwFeedback;// Offset=0x28 Size=0x4
    unsigned long dwModScale;// Offset=0x2c Size=0x4
};

struct _DSI3DL2BUFFER// Size=0x24 (Id=308)
{
    long lDirect;// Offset=0x0 Size=0x4
    long lDirectHF;// Offset=0x4 Size=0x4
    long lRoom;// Offset=0x8 Size=0x4
    long lRoomHF;// Offset=0xc Size=0x4
    float flRoomRolloffFactor;// Offset=0x10 Size=0x4
    struct _DSI3DL2OBSTRUCTION Obstruction;// Offset=0x14 Size=0x8
    struct _DSI3DL2OCCLUSION Occlusion;// Offset=0x1c Size=0x8
};

struct _DSFX_IIR2_PARAMS// Size=0x30 (Id=316)
{
    struct _DSFX_IIR2_STATE State;// Offset=0x0 Size=0x1c
    unsigned long dwFilterB0;// Offset=0x1c Size=0x4
    unsigned long dwFilterB1;// Offset=0x20 Size=0x4
    unsigned long dwFilterB2;// Offset=0x24 Size=0x4
    unsigned long dwFilterA1;// Offset=0x28 Size=0x4
    unsigned long dwFilterA2;// Offset=0x2c Size=0x4
};

enum DSOUND_POOL_TAG
{
    DSOUND_OBJECT_POOL_TAG=1651462980,
    DSOUND_DATA_POOL_TAG=1633964868
};

struct _DSFX_OSCILLATOR_PARAMS// Size=0x38 (Id=345)
{
    struct _DSFX_OSCILLATOR_STATE State;// Offset=0x0 Size=0x24
    unsigned long dwNumOutputs;// Offset=0x24 Size=0x4
    unsigned long adwFrequency[4];// Offset=0x28 Size=0x10
};

struct _DSLFODESC// Size=0x18 (Id=348)
{
    unsigned long dwLFO;// Offset=0x0 Size=0x4
    unsigned long dwDelay;// Offset=0x4 Size=0x4
    unsigned long dwDelta;// Offset=0x8 Size=0x4
    long lPitchModulation;// Offset=0xc Size=0x4
    long lFilterCutOffRange;// Offset=0x10 Size=0x4
    long lAmplitudeModulation;// Offset=0x14 Size=0x4
};

struct _DSFX_DISTORTION_PARAMS// Size=0x48 (Id=355)
{
    struct _DSFX_DISTORTION_STATE State;// Offset=0x0 Size=0x1c
    unsigned long dwGain;// Offset=0x1c Size=0x4
    unsigned long dwPreFilterB0;// Offset=0x20 Size=0x4
    unsigned long dwPreFilterB1;// Offset=0x24 Size=0x4
    unsigned long dwPreFilterB2;// Offset=0x28 Size=0x4
    unsigned long dwPreFilterA1;// Offset=0x2c Size=0x4
    unsigned long dwPreFilterA2;// Offset=0x30 Size=0x4
    unsigned long dwPostFilterB0;// Offset=0x34 Size=0x4
    unsigned long dwPostFilterB1;// Offset=0x38 Size=0x4
    unsigned long dwPostFilterB2;// Offset=0x3c Size=0x4
    unsigned long dwPostFilterA1;// Offset=0x40 Size=0x4
    unsigned long dwPostFilterA2;// Offset=0x44 Size=0x4
};

struct _DSFX_SAMPLE_RATE_CONVERTER_PARAMS// Size=0x34 (Id=371)
{
    struct _DSFX_SAMPLE_RATE_CONVERTER_STATE State;// Offset=0x0 Size=0x1c
    unsigned long dwConversionRatio;// Offset=0x1c Size=0x4
    unsigned long dwReserved[4];// Offset=0x20 Size=0x10
    unsigned long dwScratchSampleOffset;// Offset=0x30 Size=0x4
};

struct _DSI3DL2OBSTRUCTION// Size=0x8 (Id=392)
{
    long lHFLevel;// Offset=0x0 Size=0x4
    float flLFRatio;// Offset=0x4 Size=0x4
};

struct _DSI3DL2BUFFER// Size=0x24 (Id=393)
{
    long lDirect;// Offset=0x0 Size=0x4
    long lDirectHF;// Offset=0x4 Size=0x4
    long lRoom;// Offset=0x8 Size=0x4
    long lRoomHF;// Offset=0xc Size=0x4
    float flRoomRolloffFactor;// Offset=0x10 Size=0x4
    struct _DSI3DL2OBSTRUCTION Obstruction;// Offset=0x14 Size=0x8
    struct _DSI3DL2OCCLUSION Occlusion;// Offset=0x1c Size=0x8
};

struct _DSFX_OSCILLATOR_STATE// Size=0x24 (Id=402)
{
    unsigned long dwScratchOffset;// Offset=0x0 Size=0x4
    unsigned long dwScratchLength;// Offset=0x4 Size=0x4
    unsigned long dwYMemoryOffset;// Offset=0x8 Size=0x4
    unsigned long dwYMemoryLength;// Offset=0xc Size=0x4
    unsigned long dwFlags;// Offset=0x10 Size=0x4
    unsigned long dwOutMixbinPtrs[4];// Offset=0x14 Size=0x10
};

struct _DSFX_AMPMOD_STEREO_STATE// Size=0x28 (Id=403)
{
    unsigned long dwScratchOffset;// Offset=0x0 Size=0x4
    unsigned long dwScratchLength;// Offset=0x4 Size=0x4
    unsigned long dwYMemoryOffset;// Offset=0x8 Size=0x4
    unsigned long dwYMemoryLength;// Offset=0xc Size=0x4
    unsigned long dwFlags;// Offset=0x10 Size=0x4
    unsigned long dwInMixbinPtrs[3];// Offset=0x14 Size=0xc
    unsigned long dwOutMixbinPtrs[2];// Offset=0x20 Size=0x8
};

struct _DSI3DL2OCCLUSION// Size=0x8 (Id=422)
{
    long lHFLevel;// Offset=0x0 Size=0x4
    float flLFRatio;// Offset=0x4 Size=0x4
};

struct _DSFX_I3DL2REVERB_PARAMS// Size=0x220 (Id=429)
{
    struct _DSFX_I3DL2REVERB_STATE State;// Offset=0x0 Size=0xa8
    struct _DSFX_I3DL2REVERB_DELAYLINE DelayLines[14];// Offset=0xa8 Size=0x70
    unsigned long dwReflectionsInputDelay[5];// Offset=0x118 Size=0x14
    unsigned long dwShortReverbInputDelay;// Offset=0x12c Size=0x4
    unsigned long dwLongReverbInputDelay[8];// Offset=0x130 Size=0x20
    unsigned long dwReflectionsFeedbackDelay[4];// Offset=0x150 Size=0x10
    unsigned long dwLongReverbFeedbackDelay;// Offset=0x160 Size=0x4
    unsigned long dwShortReverbInputGain[8];// Offset=0x164 Size=0x20
    unsigned long dwLongReverbInputGain;// Offset=0x184 Size=0x4
    unsigned long dwLongReverbCrossfeedGain;// Offset=0x188 Size=0x4
    unsigned long dwReflectionsOutputGain[4];// Offset=0x18c Size=0x10
    unsigned long dwShortReverbOutputGain;// Offset=0x19c Size=0x4
    unsigned long dwLongReverbOutputGain;// Offset=0x1a0 Size=0x4
    unsigned long dwChannelCount;// Offset=0x1a4 Size=0x4
    struct _DSFX_I3DL2REVERB_IIR IIR[10];// Offset=0x1a8 Size=0x78
};

struct _DSFX_ECHO_MONO_STATE// Size=0x1c (Id=433)
{
    unsigned long dwScratchOffset;// Offset=0x0 Size=0x4
    unsigned long dwScratchLength;// Offset=0x4 Size=0x4
    unsigned long dwYMemoryOffset;// Offset=0x8 Size=0x4
    unsigned long dwYMemoryLength;// Offset=0xc Size=0x4
    unsigned long dwFlags;// Offset=0x10 Size=0x4
    unsigned long dwInMixbinPtrs[1];// Offset=0x14 Size=0x4
    unsigned long dwOutMixbinPtrs[1];// Offset=0x18 Size=0x4
};

struct _DSFX_RMS_STATE// Size=0x2c (Id=437)
{
    unsigned long dwScratchOffset;// Offset=0x0 Size=0x4
    unsigned long dwScratchLength;// Offset=0x4 Size=0x4
    unsigned long dwYMemoryOffset;// Offset=0x8 Size=0x4
    unsigned long dwYMemoryLength;// Offset=0xc Size=0x4
    unsigned long dwFlags;// Offset=0x10 Size=0x4
    unsigned long dwInMixbinPtrs[6];// Offset=0x14 Size=0x18
};

struct _DSFX_SPLITTER_STATE// Size=0x38 (Id=441)
{
    unsigned long dwScratchOffset;// Offset=0x0 Size=0x4
    unsigned long dwScratchLength;// Offset=0x4 Size=0x4
    unsigned long dwYMemoryOffset;// Offset=0x8 Size=0x4
    unsigned long dwYMemoryLength;// Offset=0xc Size=0x4
    unsigned long dwFlags;// Offset=0x10 Size=0x4
    unsigned long dwInMixbinPtrs[1];// Offset=0x14 Size=0x4
    unsigned long dwOutMixbinPtrs[8];// Offset=0x18 Size=0x20
};

struct HRTFLISTENER// Size=0x70 (Id=442)
{
    struct _D3DVECTOR vNormOrient;// Offset=0x0 Size=0xc
    struct PAN3DSPEAKER aSpeakers[5];// Offset=0xc Size=0x64
};

struct _DSMIXBINVOLUMEPAIR// Size=0x8 (Id=444)
{
    unsigned long dwMixBin;// Offset=0x0 Size=0x4
    long lVolume;// Offset=0x4 Size=0x4
};

struct _DSFX_CHORUS_STEREO_PARAMS// Size=0x30 (Id=481)
{
    struct _DSFX_CHORUS_STEREO_STATE State;// Offset=0x0 Size=0x28
    unsigned long dwGain;// Offset=0x28 Size=0x4
    unsigned long dwModScale;// Offset=0x2c Size=0x4
};

struct _DS3DBUFFER// Size=0x4c (Id=483)
{
    unsigned long dwSize;// Offset=0x0 Size=0x4
    struct _D3DVECTOR vPosition;// Offset=0x4 Size=0xc
    struct _D3DVECTOR vVelocity;// Offset=0x10 Size=0xc
    unsigned long dwInsideConeAngle;// Offset=0x1c Size=0x4
    unsigned long dwOutsideConeAngle;// Offset=0x20 Size=0x4
    struct _D3DVECTOR vConeOrientation;// Offset=0x24 Size=0xc
    long lConeOutsideVolume;// Offset=0x30 Size=0x4
    float flMinDistance;// Offset=0x34 Size=0x4
    float flMaxDistance;// Offset=0x38 Size=0x4
    unsigned long dwMode;// Offset=0x3c Size=0x4
    float flDistanceFactor;// Offset=0x40 Size=0x4
    float flRolloffFactor;// Offset=0x44 Size=0x4
    float flDopplerFactor;// Offset=0x48 Size=0x4
};

enum DSOUND_ALLOCATOR_TAG
{
    DSOUND_ALLOCATOR_POOL=1819242352,
    DSOUND_ALLOCATOR_PHYS=1937336432,
    DSOUND_ALLOCATOR_SLOP=1886350451
};

struct _DSFX_MINIREVERB_STATE// Size=0x48 (Id=495)
{
    unsigned long dwScratchOffset;// Offset=0x0 Size=0x4
    unsigned long dwScratchLength;// Offset=0x4 Size=0x4
    unsigned long dwYMemoryOffset;// Offset=0x8 Size=0x4
    unsigned long dwYMemoryLength;// Offset=0xc Size=0x4
    unsigned long dwFlags;// Offset=0x10 Size=0x4
    unsigned long dwInMixbinPtrs[1];// Offset=0x14 Size=0x4
    unsigned long dwOutMixbinPtrs[12];// Offset=0x18 Size=0x30
};

struct _DSEFFECTIMAGEDESC// Size=0x28 (Id=498)
{
    unsigned long dwEffectCount;// Offset=0x0 Size=0x4
    unsigned long dwTotalScratchSize;// Offset=0x4 Size=0x4
    struct _DSEFFECTMAP aEffectMaps[1];// Offset=0x8 Size=0x20
};

struct _DSFX_SAMPLE_RATE_CONVERTER_PARAMS// Size=0x34 (Id=505)
{
    struct _DSFX_SAMPLE_RATE_CONVERTER_STATE State;// Offset=0x0 Size=0x1c
    unsigned long dwConversionRatio;// Offset=0x1c Size=0x4
    unsigned long dwReserved[4];// Offset=0x20 Size=0x10
    unsigned long dwScratchSampleOffset;// Offset=0x30 Size=0x4
};

struct _DSFX_AMPMOD_MONO_STATE// Size=0x20 (Id=514)
{
    unsigned long dwScratchOffset;// Offset=0x0 Size=0x4
    unsigned long dwScratchLength;// Offset=0x4 Size=0x4
    unsigned long dwYMemoryOffset;// Offset=0x8 Size=0x4
    unsigned long dwYMemoryLength;// Offset=0xc Size=0x4
    unsigned long dwFlags;// Offset=0x10 Size=0x4
    unsigned long dwInMixbinPtrs[2];// Offset=0x14 Size=0x8
    unsigned long dwOutMixbinPtrs[1];// Offset=0x1c Size=0x4
};

struct _DSFX_CHORUS_STEREO_STATE// Size=0x28 (Id=515)
{
    unsigned long dwScratchOffset;// Offset=0x0 Size=0x4
    unsigned long dwScratchLength;// Offset=0x4 Size=0x4
    unsigned long dwYMemoryOffset;// Offset=0x8 Size=0x4
    unsigned long dwYMemoryLength;// Offset=0xc Size=0x4
    unsigned long dwFlags;// Offset=0x10 Size=0x4
    unsigned long dwInMixbinPtrs[3];// Offset=0x14 Size=0xc
    unsigned long dwOutMixbinPtrs[2];// Offset=0x20 Size=0x8
};

struct _DSMIXBINS// Size=0x8 (Id=516)
{
    unsigned long dwMixBinCount;// Offset=0x0 Size=0x4
    struct _DSMIXBINVOLUMEPAIR * lpMixBinVolumePairs;// Offset=0x4 Size=0x4
};

struct _DSSTREAMDESC// Size=0x18 (Id=517)
{
    unsigned long dwFlags;// Offset=0x0 Size=0x4
    unsigned long dwMaxAttachedPackets;// Offset=0x4 Size=0x4
    struct tWAVEFORMATEX * lpwfxFormat;// Offset=0x8 Size=0x4
    void  ( * lpfnCallback)(void * ,void * ,unsigned long );// Offset=0xc Size=0x4
    void * lpvContext;// Offset=0x10 Size=0x4
    struct _DSMIXBINS * lpMixBins;// Offset=0x14 Size=0x4
};

struct _DSMIXBINVOLUMEPAIR// Size=0x8 (Id=530)
{
    unsigned long dwMixBin;// Offset=0x0 Size=0x4
    long lVolume;// Offset=0x4 Size=0x4
};

struct _DSFX_CHORUS_STEREO_PARAMS// Size=0x30 (Id=533)
{
    struct _DSFX_CHORUS_STEREO_STATE State;// Offset=0x0 Size=0x28
    unsigned long dwGain;// Offset=0x28 Size=0x4
    unsigned long dwModScale;// Offset=0x2c Size=0x4
};

struct _DSFX_AMPMOD_STEREO_STATE// Size=0x28 (Id=550)
{
    unsigned long dwScratchOffset;// Offset=0x0 Size=0x4
    unsigned long dwScratchLength;// Offset=0x4 Size=0x4
    unsigned long dwYMemoryOffset;// Offset=0x8 Size=0x4
    unsigned long dwYMemoryLength;// Offset=0xc Size=0x4
    unsigned long dwFlags;// Offset=0x10 Size=0x4
    unsigned long dwInMixbinPtrs[3];// Offset=0x14 Size=0xc
    unsigned long dwOutMixbinPtrs[2];// Offset=0x20 Size=0x8
};

struct _DSFX_SPLITTER_PARAMS// Size=0x5c (Id=558)
{
    struct _DSFX_SPLITTER_STATE State;// Offset=0x0 Size=0x38
    unsigned long dwNumOutputs;// Offset=0x38 Size=0x4
    unsigned long dwGains[8];// Offset=0x3c Size=0x20
};

struct _DSFX_IIR_STATE// Size=0x1c (Id=572)
{
    unsigned long dwScratchOffset;// Offset=0x0 Size=0x4
    unsigned long dwScratchLength;// Offset=0x4 Size=0x4
    unsigned long dwYMemoryOffset;// Offset=0x8 Size=0x4
    unsigned long dwYMemoryLength;// Offset=0xc Size=0x4
    unsigned long dwFlags;// Offset=0x10 Size=0x4
    unsigned long dwInMixbinPtrs[1];// Offset=0x14 Size=0x4
    unsigned long dwOutMixbinPtrs[1];// Offset=0x18 Size=0x4
};

struct HRTFFILTERPAIR// Size=0x8 (Id=580)
{
    struct FIRFILTER8 * pLeftFilter;// Offset=0x0 Size=0x4
    struct FIRFILTER8 * pRightFilter;// Offset=0x4 Size=0x4
};

struct _XSOUNDTRACK_DATA// Size=0x4c (Id=586)
{
    unsigned int uSoundtrackId;// Offset=0x0 Size=0x4
    unsigned int uSongCount;// Offset=0x4 Size=0x4
    unsigned int uSoundtrackLength;// Offset=0x8 Size=0x4
    unsigned short szName[32];// Offset=0xc Size=0x40
};

struct _DSFX_RMS_PARAMS// Size=0x60 (Id=590)
{
    struct _DSFX_RMS_STATE State;// Offset=0x0 Size=0x2c
    unsigned long dwNumMixBins;// Offset=0x2c Size=0x4
    unsigned long dwRMSValues[6];// Offset=0x30 Size=0x18
    unsigned long dwPeakValues[6];// Offset=0x48 Size=0x18
};

struct _DSI3DL2LISTENER// Size=0x30 (Id=610)
{
    long lRoom;// Offset=0x0 Size=0x4
    long lRoomHF;// Offset=0x4 Size=0x4
    float flRoomRolloffFactor;// Offset=0x8 Size=0x4
    float flDecayTime;// Offset=0xc Size=0x4
    float flDecayHFRatio;// Offset=0x10 Size=0x4
    long lReflections;// Offset=0x14 Size=0x4
    float flReflectionsDelay;// Offset=0x18 Size=0x4
    long lReverb;// Offset=0x1c Size=0x4
    float flReverbDelay;// Offset=0x20 Size=0x4
    float flDiffusion;// Offset=0x24 Size=0x4
    float flDensity;// Offset=0x28 Size=0x4
    float flHFReference;// Offset=0x2c Size=0x4
};

struct HRTFSOURCE// Size=0x1c (Id=612)
{
    struct _D3DVECTOR vNormPos;// Offset=0x0 Size=0xc
    float flMagPos;// Offset=0xc Size=0x4
    float flAzimuth;// Offset=0x10 Size=0x4
    float flElevation;// Offset=0x14 Size=0x4
    float flThetaS;// Offset=0x18 Size=0x4
};

struct _DSFX_CROSSTALK_PARAMS// Size=0x34 (Id=615)
{
    struct _DSFX_CROSSTALK_STATE State;// Offset=0x0 Size=0x34
};

struct _DSFX_RMS_STATE// Size=0x2c (Id=635)
{
    unsigned long dwScratchOffset;// Offset=0x0 Size=0x4
    unsigned long dwScratchLength;// Offset=0x4 Size=0x4
    unsigned long dwYMemoryOffset;// Offset=0x8 Size=0x4
    unsigned long dwYMemoryLength;// Offset=0xc Size=0x4
    unsigned long dwFlags;// Offset=0x10 Size=0x4
    unsigned long dwInMixbinPtrs[6];// Offset=0x14 Size=0x18
};

struct _DSFX_IIR_PARAMS// Size=0x28 (Id=647)
{
    struct _DSFX_IIR_STATE State;// Offset=0x0 Size=0x1c
    unsigned long dwDelayLength;// Offset=0x1c Size=0x4
    unsigned long dwGain;// Offset=0x20 Size=0x4
    unsigned long dwType;// Offset=0x24 Size=0x4
};

struct _DSFX_SAMPLE_RATE_CONVERTER_STATE// Size=0x1c (Id=655)
{
    unsigned long dwScratchOffset;// Offset=0x0 Size=0x4
    unsigned long dwScratchLength;// Offset=0x4 Size=0x4
    unsigned long dwYMemoryOffset;// Offset=0x8 Size=0x4
    unsigned long dwYMemoryLength;// Offset=0xc Size=0x4
    unsigned long dwFlags;// Offset=0x10 Size=0x4
    unsigned long dwInMixbinPtrs[1];// Offset=0x14 Size=0x4
    unsigned long dwOutMixbinPtrs[1];// Offset=0x18 Size=0x4
};

struct _DSFX_ECHO_MONO_PARAMS// Size=0x20 (Id=664)
{
    struct _DSFX_ECHO_MONO_STATE State;// Offset=0x0 Size=0x1c
    unsigned long dwGain;// Offset=0x1c Size=0x4
};

struct _DSFX_ECHO_STEREO_PARAMS// Size=0x28 (Id=669)
{
    struct _DSFX_ECHO_STEREO_STATE State;// Offset=0x0 Size=0x24
    unsigned long dwGain;// Offset=0x24 Size=0x4
};

struct _DSCAPS// Size=0x10 (Id=670)
{
    unsigned long dwFree2DBuffers;// Offset=0x0 Size=0x4
    unsigned long dwFree3DBuffers;// Offset=0x4 Size=0x4
    unsigned long dwFreeBufferSGEs;// Offset=0x8 Size=0x4
    unsigned long dwMemoryAllocated;// Offset=0xc Size=0x4
};

struct _DSFX_CROSSTALK_STATE// Size=0x34 (Id=675)
{
    unsigned long dwScratchOffset;// Offset=0x0 Size=0x4
    unsigned long dwScratchLength;// Offset=0x4 Size=0x4
    unsigned long dwYMemoryOffset;// Offset=0x8 Size=0x4
    unsigned long dwYMemoryLength;// Offset=0xc Size=0x4
    unsigned long dwFlags;// Offset=0x10 Size=0x4
    unsigned long dwInMixbinPtrs[4];// Offset=0x14 Size=0x10
    unsigned long dwOutMixbinPtrs[4];// Offset=0x24 Size=0x10
};

struct _DSFX_I3DL2REVERB_IIR// Size=0xc (Id=680)
{
    unsigned long dwDelay;// Offset=0x0 Size=0x4
    unsigned long dwCoefficients[2];// Offset=0x4 Size=0x8
};

struct _DSI3DL2LISTENER// Size=0x30 (Id=681)
{
    long lRoom;// Offset=0x0 Size=0x4
    long lRoomHF;// Offset=0x4 Size=0x4
    float flRoomRolloffFactor;// Offset=0x8 Size=0x4
    float flDecayTime;// Offset=0xc Size=0x4
    float flDecayHFRatio;// Offset=0x10 Size=0x4
    long lReflections;// Offset=0x14 Size=0x4
    float flReflectionsDelay;// Offset=0x18 Size=0x4
    long lReverb;// Offset=0x1c Size=0x4
    float flReverbDelay;// Offset=0x20 Size=0x4
    float flDiffusion;// Offset=0x24 Size=0x4
    float flDensity;// Offset=0x28 Size=0x4
    float flHFReference;// Offset=0x2c Size=0x4
};

struct _DSEFFECTIMAGEDESC// Size=0x28 (Id=688)
{
    unsigned long dwEffectCount;// Offset=0x0 Size=0x4
    unsigned long dwTotalScratchSize;// Offset=0x4 Size=0x4
    struct _DSEFFECTMAP aEffectMaps[1];// Offset=0x8 Size=0x20
};

struct _DSFX_AMPMOD_MONO_PARAMS// Size=0x20 (Id=689)
{
    struct _DSFX_AMPMOD_MONO_STATE State;// Offset=0x0 Size=0x20
};

struct _DSENVELOPEDESC// Size=0x28 (Id=693)
{
    unsigned long dwEG;// Offset=0x0 Size=0x4
    unsigned long dwMode;// Offset=0x4 Size=0x4
    unsigned long dwDelay;// Offset=0x8 Size=0x4
    unsigned long dwAttack;// Offset=0xc Size=0x4
    unsigned long dwHold;// Offset=0x10 Size=0x4
    unsigned long dwDecay;// Offset=0x14 Size=0x4
    unsigned long dwRelease;// Offset=0x18 Size=0x4
    unsigned long dwSustain;// Offset=0x1c Size=0x4
    long lPitchScale;// Offset=0x20 Size=0x4
    long lFilterCutOff;// Offset=0x24 Size=0x4
};

struct IDirectSoundStream// Size=0x4 (Id=712)
{
    struct IDirectSoundStreamVtbl * lpVtbl;// Offset=0x0 Size=0x4
};

struct _DSBPOSITIONNOTIFY// Size=0x8 (Id=720)
{
    unsigned long dwOffset;// Offset=0x0 Size=0x4
    void * hEventNotify;// Offset=0x4 Size=0x4
};

struct _DSFX_MINIREVERB_PARAMS// Size=0xc0 (Id=724)
{
    struct _DSFX_MINIREVERB_STATE State;// Offset=0x0 Size=0x48
    unsigned long dwDelayLineLengths[8];// Offset=0x48 Size=0x20
    unsigned long dwReflectionTaps[8];// Offset=0x68 Size=0x20
    unsigned long dwReflectionGains[8];// Offset=0x88 Size=0x20
    unsigned long dwInputIIRCoefficients[2];// Offset=0xa8 Size=0x8
    unsigned long dwInputIIRDelay;// Offset=0xb0 Size=0x4
    unsigned long dwOutputIIRCoefficients[2];// Offset=0xb4 Size=0x8
    unsigned long dwOutputIIRDelay;// Offset=0xbc Size=0x4
};

struct _DSSTREAMDESC// Size=0x18 (Id=727)
{
    unsigned long dwFlags;// Offset=0x0 Size=0x4
    unsigned long dwMaxAttachedPackets;// Offset=0x4 Size=0x4
    struct tWAVEFORMATEX * lpwfxFormat;// Offset=0x8 Size=0x4
    void  ( * lpfnCallback)(void * ,void * ,unsigned long );// Offset=0xc Size=0x4
    void * lpvContext;// Offset=0x10 Size=0x4
    struct _DSMIXBINS * lpMixBins;// Offset=0x14 Size=0x4
};

struct _DSBPOSITIONNOTIFY// Size=0x8 (Id=728)
{
    unsigned long dwOffset;// Offset=0x0 Size=0x4
    void * hEventNotify;// Offset=0x4 Size=0x4
};

struct _DSEFFECTMAP// Size=0x20 (Id=735)
{
    void * lpvCodeSegment;// Offset=0x0 Size=0x4
    unsigned long dwCodeSize;// Offset=0x4 Size=0x4
    void * lpvStateSegment;// Offset=0x8 Size=0x4
    unsigned long dwStateSize;// Offset=0xc Size=0x4
    void * lpvYMemorySegment;// Offset=0x10 Size=0x4
    unsigned long dwYMemorySize;// Offset=0x14 Size=0x4
    void * lpvScratchSegment;// Offset=0x18 Size=0x4
    unsigned long dwScratchSize;// Offset=0x1c Size=0x4
};

struct _DSFX_CHORUS_MONO_PARAMS// Size=0x28 (Id=753)
{
    struct _DSFX_CHORUS_MONO_STATE State;// Offset=0x0 Size=0x20
    unsigned long dwGain;// Offset=0x20 Size=0x4
    unsigned long dwModScale;// Offset=0x24 Size=0x4
};

struct _DSFX_AMPMOD_MONO_PARAMS// Size=0x20 (Id=767)
{
    struct _DSFX_AMPMOD_MONO_STATE State;// Offset=0x0 Size=0x20
};

struct _DSFX_ECHO_MONO_PARAMS// Size=0x20 (Id=779)
{
    struct _DSFX_ECHO_MONO_STATE State;// Offset=0x0 Size=0x1c
    unsigned long dwGain;// Offset=0x1c Size=0x4
};

struct _DSFX_I3DL2REVERB_DELAYLINE// Size=0x8 (Id=780)
{
    unsigned long dwBase;// Offset=0x0 Size=0x4
    unsigned long dwLength;// Offset=0x4 Size=0x4
};

struct _DSFX_MINIREVERB_PARAMS// Size=0xc0 (Id=788)
{
    struct _DSFX_MINIREVERB_STATE State;// Offset=0x0 Size=0x48
    unsigned long dwDelayLineLengths[8];// Offset=0x48 Size=0x20
    unsigned long dwReflectionTaps[8];// Offset=0x68 Size=0x20
    unsigned long dwReflectionGains[8];// Offset=0x88 Size=0x20
    unsigned long dwInputIIRCoefficients[2];// Offset=0xa8 Size=0x8
    unsigned long dwInputIIRDelay;// Offset=0xb0 Size=0x4
    unsigned long dwOutputIIRCoefficients[2];// Offset=0xb4 Size=0x8
    unsigned long dwOutputIIRDelay;// Offset=0xbc Size=0x4
};

struct _DSFX_CROSSTALK_PARAMS// Size=0x34 (Id=798)
{
    struct _DSFX_CROSSTALK_STATE State;// Offset=0x0 Size=0x34
};

struct _DSFX_DISTORTION_STATE// Size=0x1c (Id=799)
{
    unsigned long dwScratchOffset;// Offset=0x0 Size=0x4
    unsigned long dwScratchLength;// Offset=0x4 Size=0x4
    unsigned long dwYMemoryOffset;// Offset=0x8 Size=0x4
    unsigned long dwYMemoryLength;// Offset=0xc Size=0x4
    unsigned long dwFlags;// Offset=0x10 Size=0x4
    unsigned long dwInMixbinPtrs[1];// Offset=0x14 Size=0x4
    unsigned long dwOutMixbinPtrs[1];// Offset=0x18 Size=0x4
};

struct _DSFX_CROSSTALK_STATE// Size=0x34 (Id=803)
{
    unsigned long dwScratchOffset;// Offset=0x0 Size=0x4
    unsigned long dwScratchLength;// Offset=0x4 Size=0x4
    unsigned long dwYMemoryOffset;// Offset=0x8 Size=0x4
    unsigned long dwYMemoryLength;// Offset=0xc Size=0x4
    unsigned long dwFlags;// Offset=0x10 Size=0x4
    unsigned long dwInMixbinPtrs[4];// Offset=0x14 Size=0x10
    unsigned long dwOutMixbinPtrs[4];// Offset=0x24 Size=0x10
};

struct _DSFX_I3DL2REVERB_PARAMS// Size=0x220 (Id=820)
{
    struct _DSFX_I3DL2REVERB_STATE State;// Offset=0x0 Size=0xa8
    struct _DSFX_I3DL2REVERB_DELAYLINE DelayLines[14];// Offset=0xa8 Size=0x70
    unsigned long dwReflectionsInputDelay[5];// Offset=0x118 Size=0x14
    unsigned long dwShortReverbInputDelay;// Offset=0x12c Size=0x4
    unsigned long dwLongReverbInputDelay[8];// Offset=0x130 Size=0x20
    unsigned long dwReflectionsFeedbackDelay[4];// Offset=0x150 Size=0x10
    unsigned long dwLongReverbFeedbackDelay;// Offset=0x160 Size=0x4
    unsigned long dwShortReverbInputGain[8];// Offset=0x164 Size=0x20
    unsigned long dwLongReverbInputGain;// Offset=0x184 Size=0x4
    unsigned long dwLongReverbCrossfeedGain;// Offset=0x188 Size=0x4
    unsigned long dwReflectionsOutputGain[4];// Offset=0x18c Size=0x10
    unsigned long dwShortReverbOutputGain;// Offset=0x19c Size=0x4
    unsigned long dwLongReverbOutputGain;// Offset=0x1a0 Size=0x4
    unsigned long dwChannelCount;// Offset=0x1a4 Size=0x4
    struct _DSFX_I3DL2REVERB_IIR IIR[10];// Offset=0x1a8 Size=0x78
};

struct _DSFX_IIR2_STATE// Size=0x1c (Id=854)
{
    unsigned long dwScratchOffset;// Offset=0x0 Size=0x4
    unsigned long dwScratchLength;// Offset=0x4 Size=0x4
    unsigned long dwYMemoryOffset;// Offset=0x8 Size=0x4
    unsigned long dwYMemoryLength;// Offset=0xc Size=0x4
    unsigned long dwFlags;// Offset=0x10 Size=0x4
    unsigned long dwInMixbinPtrs[1];// Offset=0x14 Size=0x4
    unsigned long dwOutMixbinPtrs[1];// Offset=0x18 Size=0x4
};

struct _DSFX_AMPMOD_MONO_STATE// Size=0x20 (Id=862)
{
    unsigned long dwScratchOffset;// Offset=0x0 Size=0x4
    unsigned long dwScratchLength;// Offset=0x4 Size=0x4
    unsigned long dwYMemoryOffset;// Offset=0x8 Size=0x4
    unsigned long dwYMemoryLength;// Offset=0xc Size=0x4
    unsigned long dwFlags;// Offset=0x10 Size=0x4
    unsigned long dwInMixbinPtrs[2];// Offset=0x14 Size=0x8
    unsigned long dwOutMixbinPtrs[1];// Offset=0x1c Size=0x4
};

struct _DSFX_CHORUS_STEREO_STATE// Size=0x28 (Id=863)
{
    unsigned long dwScratchOffset;// Offset=0x0 Size=0x4
    unsigned long dwScratchLength;// Offset=0x4 Size=0x4
    unsigned long dwYMemoryOffset;// Offset=0x8 Size=0x4
    unsigned long dwYMemoryLength;// Offset=0xc Size=0x4
    unsigned long dwFlags;// Offset=0x10 Size=0x4
    unsigned long dwInMixbinPtrs[3];// Offset=0x14 Size=0xc
    unsigned long dwOutMixbinPtrs[2];// Offset=0x20 Size=0x8
};

struct _DSFX_ECHO_STEREO_STATE// Size=0x24 (Id=901)
{
    unsigned long dwScratchOffset;// Offset=0x0 Size=0x4
    unsigned long dwScratchLength;// Offset=0x4 Size=0x4
    unsigned long dwYMemoryOffset;// Offset=0x8 Size=0x4
    unsigned long dwYMemoryLength;// Offset=0xc Size=0x4
    unsigned long dwFlags;// Offset=0x10 Size=0x4
    unsigned long dwInMixbinPtrs[2];// Offset=0x14 Size=0x8
    unsigned long dwOutMixbinPtrs[2];// Offset=0x1c Size=0x8
};

struct _DSFX_CHORUS_MONO_STATE// Size=0x20 (Id=913)
{
    unsigned long dwScratchOffset;// Offset=0x0 Size=0x4
    unsigned long dwScratchLength;// Offset=0x4 Size=0x4
    unsigned long dwYMemoryOffset;// Offset=0x8 Size=0x4
    unsigned long dwYMemoryLength;// Offset=0xc Size=0x4
    unsigned long dwFlags;// Offset=0x10 Size=0x4
    unsigned long dwInMixbinPtrs[2];// Offset=0x14 Size=0x8
    unsigned long dwOutMixbinPtrs[1];// Offset=0x1c Size=0x4
};

struct _DSFX_CHORUS_MONO_STATE// Size=0x20 (Id=916)
{
    unsigned long dwScratchOffset;// Offset=0x0 Size=0x4
    unsigned long dwScratchLength;// Offset=0x4 Size=0x4
    unsigned long dwYMemoryOffset;// Offset=0x8 Size=0x4
    unsigned long dwYMemoryLength;// Offset=0xc Size=0x4
    unsigned long dwFlags;// Offset=0x10 Size=0x4
    unsigned long dwInMixbinPtrs[2];// Offset=0x14 Size=0x8
    unsigned long dwOutMixbinPtrs[1];// Offset=0x1c Size=0x4
};

struct _DSFX_SPLITTER_STATE// Size=0x38 (Id=921)
{
    unsigned long dwScratchOffset;// Offset=0x0 Size=0x4
    unsigned long dwScratchLength;// Offset=0x4 Size=0x4
    unsigned long dwYMemoryOffset;// Offset=0x8 Size=0x4
    unsigned long dwYMemoryLength;// Offset=0xc Size=0x4
    unsigned long dwFlags;// Offset=0x10 Size=0x4
    unsigned long dwInMixbinPtrs[1];// Offset=0x14 Size=0x4
    unsigned long dwOutMixbinPtrs[8];// Offset=0x18 Size=0x20
};

struct _DSFX_AMPMOD_STEREO_PARAMS// Size=0x28 (Id=937)
{
    struct _DSFX_AMPMOD_STEREO_STATE State;// Offset=0x0 Size=0x28
};

struct _DSFX_I3DL2REVERB_IIR// Size=0xc (Id=939)
{
    unsigned long dwDelay;// Offset=0x0 Size=0x4
    unsigned long dwCoefficients[2];// Offset=0x4 Size=0x8
};

struct _DSFILTERDESC// Size=0x18 (Id=941)
{
    unsigned long dwMode;// Offset=0x0 Size=0x4
    unsigned long dwQCoefficient;// Offset=0x4 Size=0x4
    unsigned long adwCoefficients[4];// Offset=0x8 Size=0x10
};

struct _DSFX_IIR_STATE// Size=0x1c (Id=943)
{
    unsigned long dwScratchOffset;// Offset=0x0 Size=0x4
    unsigned long dwScratchLength;// Offset=0x4 Size=0x4
    unsigned long dwYMemoryOffset;// Offset=0x8 Size=0x4
    unsigned long dwYMemoryLength;// Offset=0xc Size=0x4
    unsigned long dwFlags;// Offset=0x10 Size=0x4
    unsigned long dwInMixbinPtrs[1];// Offset=0x14 Size=0x4
    unsigned long dwOutMixbinPtrs[1];// Offset=0x18 Size=0x4
};

struct _DSFX_OSCILLATOR_STATE// Size=0x24 (Id=944)
{
    unsigned long dwScratchOffset;// Offset=0x0 Size=0x4
    unsigned long dwScratchLength;// Offset=0x4 Size=0x4
    unsigned long dwYMemoryOffset;// Offset=0x8 Size=0x4
    unsigned long dwYMemoryLength;// Offset=0xc Size=0x4
    unsigned long dwFlags;// Offset=0x10 Size=0x4
    unsigned long dwOutMixbinPtrs[4];// Offset=0x14 Size=0x10
};

struct _DS3DLISTENER// Size=0x40 (Id=954)
{
    union // Size=0x40 (Id=0)
    {
        unsigned long dwSize;// Offset=0x0 Size=0x4
        struct XGVECTOR3 vPosition;// Offset=0x4 Size=0xc
        struct XGVECTOR3 vVelocity;// Offset=0x10 Size=0xc
        struct XGVECTOR3 vOrientFront;// Offset=0x1c Size=0xc
        struct XGVECTOR3 vOrientTop;// Offset=0x28 Size=0xc
        float flDistanceFactor;// Offset=0x34 Size=0x4
        float flRolloffFactor;// Offset=0x38 Size=0x4
        float flDopplerFactor;// Offset=0x3c Size=0x4
        void _DS3DLISTENER();// Offset=0x0 Size=0x3
    };
};

struct _DSFX_I3DL2REVERB_DELAYLINE// Size=0x8 (Id=956)
{
    unsigned long dwBase;// Offset=0x0 Size=0x4
    unsigned long dwLength;// Offset=0x4 Size=0x4
};

struct _DSEFFECTMAP// Size=0x20 (Id=957)
{
    void * lpvCodeSegment;// Offset=0x0 Size=0x4
    unsigned long dwCodeSize;// Offset=0x4 Size=0x4
    void * lpvStateSegment;// Offset=0x8 Size=0x4
    unsigned long dwStateSize;// Offset=0xc Size=0x4
    void * lpvYMemorySegment;// Offset=0x10 Size=0x4
    unsigned long dwYMemorySize;// Offset=0x14 Size=0x4
    void * lpvScratchSegment;// Offset=0x18 Size=0x4
    unsigned long dwScratchSize;// Offset=0x1c Size=0x4
};

struct _DSFX_IIR2_PARAMS// Size=0x30 (Id=958)
{
    struct _DSFX_IIR2_STATE State;// Offset=0x0 Size=0x1c
    unsigned long dwFilterB0;// Offset=0x1c Size=0x4
    unsigned long dwFilterB1;// Offset=0x20 Size=0x4
    unsigned long dwFilterB2;// Offset=0x24 Size=0x4
    unsigned long dwFilterA1;// Offset=0x28 Size=0x4
    unsigned long dwFilterA2;// Offset=0x2c Size=0x4
};

struct _DSLFODESC// Size=0x18 (Id=963)
{
    unsigned long dwLFO;// Offset=0x0 Size=0x4
    unsigned long dwDelay;// Offset=0x4 Size=0x4
    unsigned long dwDelta;// Offset=0x8 Size=0x4
    long lPitchModulation;// Offset=0xc Size=0x4
    long lFilterCutOffRange;// Offset=0x10 Size=0x4
    long lAmplitudeModulation;// Offset=0x14 Size=0x4
};

struct _DSFX_RMS_PARAMS// Size=0x60 (Id=974)
{
    struct _DSFX_RMS_STATE State;// Offset=0x0 Size=0x2c
    unsigned long dwNumMixBins;// Offset=0x2c Size=0x4
    unsigned long dwRMSValues[6];// Offset=0x30 Size=0x18
    unsigned long dwPeakValues[6];// Offset=0x48 Size=0x18
};

struct AIFFSOUNDHDR// Size=0x8 (Id=986)
{
    unsigned long dwOffset;// Offset=0x0 Size=0x4
    unsigned long dwBlockSize;// Offset=0x4 Size=0x4
};

struct HRTFFILTERPAIR// Size=0x8 (Id=990)
{
    struct FIRFILTER8 * pLeftFilter;// Offset=0x0 Size=0x4
    struct FIRFILTER8 * pRightFilter;// Offset=0x4 Size=0x4
};

struct HRTFSOURCE// Size=0x1c (Id=991)
{
    struct _D3DVECTOR vNormPos;// Offset=0x0 Size=0xc
    float flMagPos;// Offset=0xc Size=0x4
    float flAzimuth;// Offset=0x10 Size=0x4
    float flElevation;// Offset=0x14 Size=0x4
    float flThetaS;// Offset=0x18 Size=0x4
};

struct HRTFVOICE// Size=0xa8 (Id=992)
{
    unsigned long dwChangeMask;// Offset=0x0 Size=0x4
    unsigned long dwMixBinValidMask;// Offset=0x4 Size=0x4
    unsigned long dwMixBinChangeMask;// Offset=0x8 Size=0x4
    long lDistanceVolume;// Offset=0xc Size=0x4
    long lConeVolume;// Offset=0x10 Size=0x4
    long lFrontVolume;// Offset=0x14 Size=0x4
    long lRearVolume;// Offset=0x18 Size=0x4
    long lDopplerPitch;// Offset=0x1c Size=0x4
    struct HRTFFILTERPAIR FilterPair;// Offset=0x20 Size=0x8
    long alMixBinVolumes[32];// Offset=0x28 Size=0x80
};

struct _DSFX_DISTORTION_STATE// Size=0x1c (Id=1009)
{
    unsigned long dwScratchOffset;// Offset=0x0 Size=0x4
    unsigned long dwScratchLength;// Offset=0x4 Size=0x4
    unsigned long dwYMemoryOffset;// Offset=0x8 Size=0x4
    unsigned long dwYMemoryLength;// Offset=0xc Size=0x4
    unsigned long dwFlags;// Offset=0x10 Size=0x4
    unsigned long dwInMixbinPtrs[1];// Offset=0x14 Size=0x4
    unsigned long dwOutMixbinPtrs[1];// Offset=0x18 Size=0x4
};

struct _DSFX_FLANGE_MONO_STATE// Size=0x20 (Id=1010)
{
    unsigned long dwScratchOffset;// Offset=0x0 Size=0x4
    unsigned long dwScratchLength;// Offset=0x4 Size=0x4
    unsigned long dwYMemoryOffset;// Offset=0x8 Size=0x4
    unsigned long dwYMemoryLength;// Offset=0xc Size=0x4
    unsigned long dwFlags;// Offset=0x10 Size=0x4
    unsigned long dwInMixbinPtrs[2];// Offset=0x14 Size=0x8
    unsigned long dwOutMixbinPtrs[1];// Offset=0x1c Size=0x4
};

struct _DS3DLISTENER// Size=0x40 (Id=1011)
{
    unsigned long dwSize;// Offset=0x0 Size=0x4
    struct _D3DVECTOR vPosition;// Offset=0x4 Size=0xc
    struct _D3DVECTOR vVelocity;// Offset=0x10 Size=0xc
    struct _D3DVECTOR vOrientFront;// Offset=0x1c Size=0xc
    struct _D3DVECTOR vOrientTop;// Offset=0x28 Size=0xc
    float flDistanceFactor;// Offset=0x34 Size=0x4
    float flRolloffFactor;// Offset=0x38 Size=0x4
    float flDopplerFactor;// Offset=0x3c Size=0x4
};

struct _DSFX_IIR_PARAMS// Size=0x28 (Id=1017)
{
    struct _DSFX_IIR_STATE State;// Offset=0x0 Size=0x1c
    unsigned long dwDelayLength;// Offset=0x1c Size=0x4
    unsigned long dwGain;// Offset=0x20 Size=0x4
    unsigned long dwType;// Offset=0x24 Size=0x4
};

struct _DSCAPS// Size=0x10 (Id=1030)
{
    unsigned long dwFree2DBuffers;// Offset=0x0 Size=0x4
    unsigned long dwFree3DBuffers;// Offset=0x4 Size=0x4
    unsigned long dwFreeBufferSGEs;// Offset=0x8 Size=0x4
    unsigned long dwMemoryAllocated;// Offset=0xc Size=0x4
};

struct _DSFX_I3DL2REVERB_STATE// Size=0xa8 (Id=1047)
{
    unsigned long dwScratchOffset;// Offset=0x0 Size=0x4
    unsigned long dwScratchLength;// Offset=0x4 Size=0x4
    unsigned long dwYMemoryOffset;// Offset=0x8 Size=0x4
    unsigned long dwYMemoryLength;// Offset=0xc Size=0x4
    unsigned long dwFlags;// Offset=0x10 Size=0x4
    unsigned long dwInMixbinPtrs[2];// Offset=0x14 Size=0x8
    unsigned long dwOutMixbinPtrs[35];// Offset=0x1c Size=0x8c
};

struct _DSBUFFERDESC// Size=0x18 (Id=1053)
{
    unsigned long dwSize;// Offset=0x0 Size=0x4
    unsigned long dwFlags;// Offset=0x4 Size=0x4
    unsigned long dwBufferBytes;// Offset=0x8 Size=0x4
    struct tWAVEFORMATEX * lpwfxFormat;// Offset=0xc Size=0x4
    struct _DSMIXBINS * lpMixBins;// Offset=0x10 Size=0x4
    unsigned long dwInputMixBin;// Offset=0x14 Size=0x4
};

struct _DSFILTERDESC// Size=0x18 (Id=1056)
{
    unsigned long dwMode;// Offset=0x0 Size=0x4
    unsigned long dwQCoefficient;// Offset=0x4 Size=0x4
    unsigned long adwCoefficients[4];// Offset=0x8 Size=0x10
};

struct _DSMIXBINS// Size=0x8 (Id=1063)
{
    unsigned long dwMixBinCount;// Offset=0x0 Size=0x4
    struct _DSMIXBINVOLUMEPAIR * lpMixBinVolumePairs;// Offset=0x4 Size=0x4
};

struct _DSFX_FLANGE_STEREO_STATE// Size=0x28 (Id=1064)
{
    unsigned long dwScratchOffset;// Offset=0x0 Size=0x4
    unsigned long dwScratchLength;// Offset=0x4 Size=0x4
    unsigned long dwYMemoryOffset;// Offset=0x8 Size=0x4
    unsigned long dwYMemoryLength;// Offset=0xc Size=0x4
    unsigned long dwFlags;// Offset=0x10 Size=0x4
    unsigned long dwInMixbinPtrs[3];// Offset=0x14 Size=0xc
    unsigned long dwOutMixbinPtrs[2];// Offset=0x20 Size=0x8
};

struct _DSBUFFERDESC// Size=0x18 (Id=1078)
{
    unsigned long dwSize;// Offset=0x0 Size=0x4
    unsigned long dwFlags;// Offset=0x4 Size=0x4
    unsigned long dwBufferBytes;// Offset=0x8 Size=0x4
    struct tWAVEFORMATEX * lpwfxFormat;// Offset=0xc Size=0x4
    struct _DSMIXBINS * lpMixBins;// Offset=0x10 Size=0x4
    unsigned long dwInputMixBin;// Offset=0x14 Size=0x4
};

struct _DSFX_DISTORTION_PARAMS// Size=0x48 (Id=1082)
{
    struct _DSFX_DISTORTION_STATE State;// Offset=0x0 Size=0x1c
    unsigned long dwGain;// Offset=0x1c Size=0x4
    unsigned long dwPreFilterB0;// Offset=0x20 Size=0x4
    unsigned long dwPreFilterB1;// Offset=0x24 Size=0x4
    unsigned long dwPreFilterB2;// Offset=0x28 Size=0x4
    unsigned long dwPreFilterA1;// Offset=0x2c Size=0x4
    unsigned long dwPreFilterA2;// Offset=0x30 Size=0x4
    unsigned long dwPostFilterB0;// Offset=0x34 Size=0x4
    unsigned long dwPostFilterB1;// Offset=0x38 Size=0x4
    unsigned long dwPostFilterB2;// Offset=0x3c Size=0x4
    unsigned long dwPostFilterA1;// Offset=0x40 Size=0x4
    unsigned long dwPostFilterA2;// Offset=0x44 Size=0x4
};

struct _DSFX_FLANGE_STEREO_PARAMS// Size=0x30 (Id=1083)
{
    struct _DSFX_FLANGE_STEREO_STATE State;// Offset=0x0 Size=0x28
    unsigned long dwFeedback;// Offset=0x28 Size=0x4
    unsigned long dwModScale;// Offset=0x2c Size=0x4
};

struct _DSI3DL2OCCLUSION// Size=0x8 (Id=1093)
{
    long lHFLevel;// Offset=0x0 Size=0x4
    float flLFRatio;// Offset=0x4 Size=0x4
};

struct _DSFX_FLANGE_MONO_PARAMS// Size=0x28 (Id=1096)
{
    struct _DSFX_FLANGE_MONO_STATE State;// Offset=0x0 Size=0x20
    unsigned long dwFeedback;// Offset=0x20 Size=0x4
    unsigned long dwModScale;// Offset=0x24 Size=0x4
};

struct _DSEFFECTIMAGELOC// Size=0x8 (Id=1097)
{
    unsigned long dwI3DL2ReverbIndex;// Offset=0x0 Size=0x4
    unsigned long dwCrosstalkIndex;// Offset=0x4 Size=0x4
};

struct _DSFX_ECHO_MONO_STATE// Size=0x1c (Id=1099)
{
    unsigned long dwScratchOffset;// Offset=0x0 Size=0x4
    unsigned long dwScratchLength;// Offset=0x4 Size=0x4
    unsigned long dwYMemoryOffset;// Offset=0x8 Size=0x4
    unsigned long dwYMemoryLength;// Offset=0xc Size=0x4
    unsigned long dwFlags;// Offset=0x10 Size=0x4
    unsigned long dwInMixbinPtrs[1];// Offset=0x14 Size=0x4
    unsigned long dwOutMixbinPtrs[1];// Offset=0x18 Size=0x4
};

struct _DSENVELOPEDESC// Size=0x28 (Id=1104)
{
    unsigned long dwEG;// Offset=0x0 Size=0x4
    unsigned long dwMode;// Offset=0x4 Size=0x4
    unsigned long dwDelay;// Offset=0x8 Size=0x4
    unsigned long dwAttack;// Offset=0xc Size=0x4
    unsigned long dwHold;// Offset=0x10 Size=0x4
    unsigned long dwDecay;// Offset=0x14 Size=0x4
    unsigned long dwRelease;// Offset=0x18 Size=0x4
    unsigned long dwSustain;// Offset=0x1c Size=0x4
    long lPitchScale;// Offset=0x20 Size=0x4
    long lFilterCutOff;// Offset=0x24 Size=0x4
};

struct _DSFX_IIR2_STATE// Size=0x1c (Id=1105)
{
    unsigned long dwScratchOffset;// Offset=0x0 Size=0x4
    unsigned long dwScratchLength;// Offset=0x4 Size=0x4
    unsigned long dwYMemoryOffset;// Offset=0x8 Size=0x4
    unsigned long dwYMemoryLength;// Offset=0xc Size=0x4
    unsigned long dwFlags;// Offset=0x10 Size=0x4
    unsigned long dwInMixbinPtrs[1];// Offset=0x14 Size=0x4
    unsigned long dwOutMixbinPtrs[1];// Offset=0x18 Size=0x4
};

struct _DSFX_CHORUS_MONO_PARAMS// Size=0x28 (Id=1109)
{
    struct _DSFX_CHORUS_MONO_STATE State;// Offset=0x0 Size=0x20
    unsigned long dwGain;// Offset=0x20 Size=0x4
    unsigned long dwModScale;// Offset=0x24 Size=0x4
};

struct _DSI3DL2OBSTRUCTION// Size=0x8 (Id=1110)
{
    long lHFLevel;// Offset=0x0 Size=0x4
    float flLFRatio;// Offset=0x4 Size=0x4
};

struct _DS3DBUFFER// Size=0x4c (Id=1220)
{
    unsigned long dwSize;// Offset=0x0 Size=0x4
    struct D3DXVECTOR3 vPosition;// Offset=0x4 Size=0xc
    struct D3DXVECTOR3 vVelocity;// Offset=0x10 Size=0xc
    unsigned long dwInsideConeAngle;// Offset=0x1c Size=0x4
    unsigned long dwOutsideConeAngle;// Offset=0x20 Size=0x4
    struct D3DXVECTOR3 vConeOrientation;// Offset=0x24 Size=0xc
    long lConeOutsideVolume;// Offset=0x30 Size=0x4
    float flMinDistance;// Offset=0x34 Size=0x4
    float flMaxDistance;// Offset=0x38 Size=0x4
    unsigned long dwMode;// Offset=0x3c Size=0x4
    float flDistanceFactor;// Offset=0x40 Size=0x4
    float flRolloffFactor;// Offset=0x44 Size=0x4
    float flDopplerFactor;// Offset=0x48 Size=0x4
    void _DS3DBUFFER();
};

struct IDirectSoundStream : public XMediaObject// Size=0x4 (Id=1274)
{
    unsigned long AddRef();
    unsigned long Release();
    long GetInfo(struct _XMEDIAINFO * );
    long GetStatus(unsigned long * );
    long Process(struct _XMEDIAPACKET * ,struct _XMEDIAPACKET * );
    long Discontinuity();
    long Flush();
    long QueryInterface(struct _GUID & ,void ** );
    long SetFormat(struct tWAVEFORMATEX * );
    long SetFrequency(unsigned long );
    long SetVolume(long );
    long SetPitch(long );
    long SetLFO(struct _DSLFODESC * );
    long SetEG(struct _DSENVELOPEDESC * );
    long SetFilter(struct _DSFILTERDESC * );
    long SetHeadroom(unsigned long );
    long SetOutputBuffer(struct IDirectSoundBuffer * );
    long SetMixBins(struct _DSMIXBINS * );
    long SetMixBinVolumes(struct _DSMIXBINS * );
    long SetAllParameters(struct _DS3DBUFFER * ,unsigned long );
    long SetConeAngles(unsigned long ,unsigned long ,unsigned long );
    long SetConeOrientation(float ,float ,float ,unsigned long );
    long SetConeOutsideVolume(long ,unsigned long );
    long SetMaxDistance(float ,unsigned long );
    long SetMinDistance(float ,unsigned long );
    long SetMode(unsigned long ,unsigned long );
    long SetPosition(float ,float ,float ,unsigned long );
    long SetVelocity(float ,float ,float ,unsigned long );
    long SetDistanceFactor(float ,unsigned long );
    long SetDopplerFactor(float ,unsigned long );
    long SetRolloffFactor(float ,unsigned long );
    long SetI3DL2Source(struct _DSI3DL2BUFFER * ,unsigned long );
    union // Size=0x5 (Id=0)
    {
        long Pause(unsigned long );// Offset=0x0 Size=0x5
        long FlushEx(long long ,unsigned long );
        void IDirectSoundStream(struct IDirectSoundStream & );
        void IDirectSoundStream();// Offset=0x0 Size=0x3
    };
};

struct IDirectSoundStream : public XMediaObject// Size=0x4 (Id=1275)
{
    unsigned long AddRef();
    unsigned long Release();
    long GetInfo(struct _XMEDIAINFO * );
    long GetStatus(unsigned long * );
    long Process(struct _XMEDIAPACKET * ,struct _XMEDIAPACKET * );
    long Discontinuity();
    long Flush();
    long QueryInterface(struct _GUID & ,void ** );
    long SetFormat(struct tWAVEFORMATEX * );
    long SetFrequency(unsigned long );
    long SetVolume(long );
    long SetPitch(long );
    long SetLFO(struct _DSLFODESC * );
    long SetEG(struct _DSENVELOPEDESC * );
    long SetFilter(struct _DSFILTERDESC * );
    long SetHeadroom(unsigned long );
    long SetOutputBuffer(struct IDirectSoundBuffer * );
    long SetMixBins(struct _DSMIXBINS * );
    long SetMixBinVolumes(struct _DSMIXBINS * );
    long SetAllParameters(struct _DS3DBUFFER * ,unsigned long );
    long SetConeAngles(unsigned long ,unsigned long ,unsigned long );
    long SetConeOrientation(float ,float ,float ,unsigned long );
    long SetConeOutsideVolume(long ,unsigned long );
    long SetMaxDistance(float ,unsigned long );
    long SetMinDistance(float ,unsigned long );
    long SetMode(unsigned long ,unsigned long );
    long SetPosition(float ,float ,float ,unsigned long );
    long SetVelocity(float ,float ,float ,unsigned long );
    long SetDistanceFactor(float ,unsigned long );
    long SetDopplerFactor(float ,unsigned long );
    long SetRolloffFactor(float ,unsigned long );
    long SetI3DL2Source(struct _DSI3DL2BUFFER * ,unsigned long );
    union // Size=0x5 (Id=0)
    {
        long Pause(unsigned long );// Offset=0x0 Size=0x5
        long FlushEx(long long ,unsigned long );
        void IDirectSoundStream(struct IDirectSoundStream & );
        void IDirectSoundStream();// Offset=0x0 Size=0x3
    };
};

struct _DS3DLISTENER// Size=0x40 (Id=1346)
{
    union // Size=0x40 (Id=0)
    {
        unsigned long dwSize;// Offset=0x0 Size=0x4
        struct D3DXVECTOR3 vPosition;// Offset=0x4 Size=0xc
        struct D3DXVECTOR3 vVelocity;// Offset=0x10 Size=0xc
        struct D3DXVECTOR3 vOrientFront;// Offset=0x1c Size=0xc
        struct D3DXVECTOR3 vOrientTop;// Offset=0x28 Size=0xc
        float flDistanceFactor;// Offset=0x34 Size=0x4
        float flRolloffFactor;// Offset=0x38 Size=0x4
        float flDopplerFactor;// Offset=0x3c Size=0x4
        void _DS3DLISTENER();// Offset=0x0 Size=0x3
    };
};

struct IDirectSound// Size=0x1 (Id=1360)
{
    long QueryInterface(struct _GUID & ,void ** );
    union // Size=0x5 (Id=0)
    {
        unsigned long AddRef();// Offset=0x0 Size=0x5
        unsigned long Release();// Offset=0x0 Size=0x5
        long GetCaps(struct _DSCAPS * );
        long CreateSoundBuffer(struct _DSBUFFERDESC * ,struct IDirectSoundBuffer ** ,struct IUnknown * );
        long CreateSoundStream(struct _DSSTREAMDESC * ,struct IDirectSoundStream ** ,struct IUnknown * );
        long GetSpeakerConfig(unsigned long * );
        long SetCooperativeLevel(struct HWND__ * ,unsigned long );
        long Compact();
        long DownloadEffectsImage(void * ,unsigned long ,struct _DSEFFECTIMAGELOC * ,struct _DSEFFECTIMAGEDESC ** );
        long GetEffectData(unsigned long ,unsigned long ,void * ,unsigned long );
        long SetEffectData(unsigned long ,unsigned long ,void * ,unsigned long ,unsigned long );
        long CommitEffectData();
        long EnableHeadphones(int );
        long SetMixBinHeadroom(unsigned long ,unsigned long );
        long SetAllParameters(struct _DS3DLISTENER * ,unsigned long );
        long SetDistanceFactor(float ,unsigned long );
        long SetDopplerFactor(float ,unsigned long );
        long SetOrientation(float ,float ,float ,float ,float ,float ,unsigned long );
        long SetPosition(float ,float ,float ,unsigned long );
        long SetRolloffFactor(float ,unsigned long );
        long SetVelocity(float ,float ,float ,unsigned long );
        long SetI3DL2Listener(struct _DSI3DL2LISTENER * ,unsigned long );
        long CommitDeferredSettings();
        long GetTime(long long * );// Offset=0x0 Size=0x5
    };
};

struct IDirectSoundBuffer// Size=0x1 (Id=1374)
{
    long QueryInterface(struct _GUID & ,void ** );
    unsigned long AddRef();
    union // Size=0x5 (Id=0)
    {
        unsigned long Release();// Offset=0x0 Size=0x5
        long SetFormat(struct tWAVEFORMATEX * );
        long SetFrequency(unsigned long );
        long SetVolume(long );
        long SetPitch(long );
        long SetLFO(struct _DSLFODESC * );
        long SetEG(struct _DSENVELOPEDESC * );
        long SetFilter(struct _DSFILTERDESC * );
        long SetHeadroom(unsigned long );
        long SetOutputBuffer(struct IDirectSoundBuffer * );
        long SetMixBins(struct _DSMIXBINS * );
        long SetMixBinVolumes(struct _DSMIXBINS * );
        long SetAllParameters(struct _DS3DBUFFER * ,unsigned long );
        long SetConeAngles(unsigned long ,unsigned long ,unsigned long );
        long SetConeOrientation(float ,float ,float ,unsigned long );
        long SetConeOutsideVolume(long ,unsigned long );
        long SetMaxDistance(float ,unsigned long );
        long SetMinDistance(float ,unsigned long );
        long SetMode(unsigned long ,unsigned long );
        long SetPosition(float ,float ,float ,unsigned long );
        long SetVelocity(float ,float ,float ,unsigned long );
        long SetDistanceFactor(float ,unsigned long );
        long SetDopplerFactor(float ,unsigned long );
        long SetRolloffFactor(float ,unsigned long );
        long SetI3DL2Source(struct _DSI3DL2BUFFER * ,unsigned long );
        long Play(unsigned long ,unsigned long ,unsigned long );// Offset=0x0 Size=0x5
        long PlayEx(long long ,unsigned long );
        long Stop();// Offset=0x0 Size=0x5
        long StopEx(long long ,unsigned long );
        long SetPlayRegion(unsigned long ,unsigned long );
        long SetLoopRegion(unsigned long ,unsigned long );
        long GetStatus(unsigned long * );// Offset=0x0 Size=0x5
        long GetCurrentPosition(unsigned long * ,unsigned long * );
        long SetCurrentPosition(unsigned long );
        long SetBufferData(void * ,unsigned long );// Offset=0x0 Size=0x5
    };
    long Lock(unsigned long ,unsigned long ,void ** ,unsigned long * ,void ** ,unsigned long * ,unsigned long );
    long Unlock(void * ,unsigned long ,void * ,unsigned long );
    long Restore();
    long SetNotificationPositions(unsigned long ,struct _DSBPOSITIONNOTIFY * );
};

struct AUDIO_ALLOC_MEMORY// Size=0x10 (Id=1673)
{
    unsigned long uSize;// Offset=0x0 Size=0x4
    unsigned long uAlignMask;// Offset=0x4 Size=0x4
    unsigned long pLinearAddress;// Offset=0x8 Size=0x4
    unsigned long uRef;// Offset=0xc Size=0x4
};

struct AUDIO_FREE_MEMORY// Size=0x8 (Id=1691)
{
    unsigned long pLinearAddress;// Offset=0x0 Size=0x4
    unsigned long uRef;// Offset=0x4 Size=0x4
};

struct AUDIO_INIT_DEVICE// Size=0x1c (Id=1854)
{
    unsigned long pResList;// Offset=0x0 Size=0x4
    unsigned char uRevisionID;// Offset=0x4 Size=0x1
    unsigned char __align0[3];// Offset=0x5 Size=0x3
    unsigned long pDevObj;// Offset=0x8 Size=0x4
    void  ( * pISRFn)(unsigned long ,unsigned long ,unsigned long );// Offset=0xc Size=0x4
    unsigned long pServiceContext;// Offset=0x10 Size=0x4
    union INTR_MASK IntrMask;// Offset=0x14 Size=0x4
    unsigned long uDeviceRef;// Offset=0x18 Size=0x4
};

struct CODEC_AUDIO_CREATE// Size=0x8 (Id=1857)
{
    unsigned long pioBaseMixer;// Offset=0x0 Size=0x4
    unsigned long pioBaseAci;// Offset=0x4 Size=0x4
};

struct AudioDec// Size=0x5c (Id=2075)
{
    int state;// Offset=0x0 Size=0x4
    struct SpuStreamHeader sshd;// Offset=0x4 Size=0x20
    struct SpuStreamBody ssbd;// Offset=0x24 Size=0x8
    int hdrCount;// Offset=0x2c Size=0x4
    unsigned char * data;// Offset=0x30 Size=0x4
    int put;// Offset=0x34 Size=0x4
    int count;// Offset=0x38 Size=0x4
    int size;// Offset=0x3c Size=0x4
    int totalBytes;// Offset=0x40 Size=0x4
    int iopBuff;// Offset=0x44 Size=0x4
    int iopBuffSize;// Offset=0x48 Size=0x4
    int iopLastPos;// Offset=0x4c Size=0x4
    int iopPausePos;// Offset=0x50 Size=0x4
    int totalBytesSent;// Offset=0x54 Size=0x4
    int iopZero;// Offset=0x58 Size=0x4
};

struct AUDIO_SLIDER// Size=0x18 (Id=2270)
{
    int vol;// Offset=0x0 Size=0x4
    struct blitinst * left;// Offset=0x4 Size=0x4
    struct blitinst * bar_gray;// Offset=0x8 Size=0x4
    struct blitinst * bar_pink;// Offset=0xc Size=0x4
    struct blitinst * marker;// Offset=0x10 Size=0x4
    struct blitinst * right;// Offset=0x14 Size=0x4
};

struct _PLAY_AUDIO// Size=0xa (Id=2362)
{
    unsigned char OperationCode;// Offset=0x0 Size=0x1
    struct // Size=0x1 (Id=0)
    {
        unsigned char Reserved1:5;// Offset=0x1 Size=0x1 BitOffset=0x0 BitSize=0x5
        unsigned char LogicalUnitNumber:3;// Offset=0x1 Size=0x1 BitOffset=0x5 BitSize=0x3
    };
    unsigned char StartingBlockAddress[4];// Offset=0x2 Size=0x4
    unsigned char Reserved2;// Offset=0x6 Size=0x1
    unsigned char PlayLength[2];// Offset=0x7 Size=0x2
    unsigned char Control;// Offset=0x9 Size=0x1
};

struct _PLAY_AUDIO_MSF// Size=0xa (Id=2371)
{
    unsigned char OperationCode;// Offset=0x0 Size=0x1
    struct // Size=0x1 (Id=0)
    {
        unsigned char Reserved1:5;// Offset=0x1 Size=0x1 BitOffset=0x0 BitSize=0x5
        unsigned char LogicalUnitNumber:3;// Offset=0x1 Size=0x1 BitOffset=0x5 BitSize=0x3
    };
    unsigned char Reserved2;// Offset=0x2 Size=0x1
    unsigned char StartingM;// Offset=0x3 Size=0x1
    unsigned char StartingS;// Offset=0x4 Size=0x1
    unsigned char StartingF;// Offset=0x5 Size=0x1
    unsigned char EndingM;// Offset=0x6 Size=0x1
    unsigned char EndingS;// Offset=0x7 Size=0x1
    unsigned char EndingF;// Offset=0x8 Size=0x1
    unsigned char Control;// Offset=0x9 Size=0x1
};

enum DirectSound::__unnamed
{
    MCPX_DEFERREDCMD_BUFFER_RELEASERESOURCES=1,
    MCPX_DEFERREDCMD_BUFFER_POSITIONDELTA=2,
    MCPX_DEFERREDCMD_BUFFER_PLAY=3,
    MCPX_DEFERREDCMD_BUFFER_STOP=4,
    MCPX_DEFERREDCMD_BUFFER_COUNT=5
};

enum DirectSound::AC97CHANNELTYPE
{
    AC97_CHANNELTYPE_ANALOG=0,
    AC97_CHANNELTYPE_DIGITAL=1,
    AC97_CHANNELTYPE_COUNT=2
};

enum DirectSound::__unnamed
{
    MCPX_GPOUTPUT_BOOTSOUND=0,
    MCPX_GPOUTPUT_COUNT=1
};

enum DirectSound::__unnamed
{
    MCPX_EPOUTPUT_AC97_ANALOG=0,
    MCPX_EPOUTPUT_AC97_DIGITAL=1,
    MCPX_EPOUTPUT_COUNT=2
};

enum DirectSound::__unnamed
{
    MCPX_MEM_GPOUTPUT=0,
    MCPX_MEM_EPOUTPUT=1,
    MCPX_MEM_MAGICWRITE=2,
    MCPX_MEM_VOICE=3,
    MCPX_MEM_NOTIFIERS=4,
    MCPX_MEM_INPUTSGE=5,
    MCPX_MEM_INPUTPRD=6,
    MCPX_MEM_HRTFTARGET=7,
    MCPX_MEM_HRTFCURRENT=8,
    MCPX_MEM_GPOUTPUTSGE=9,
    MCPX_MEM_EPOUTPUTSGE=10,
    MCPX_MEM_GPMULTIPASS=11,
    MCPX_MEM_GPSCRATCH=12,
    MCPX_MEM_GPSCRATCHSGE=13,
    MCPX_MEM_EPSCRATCH=14,
    MCPX_MEM_EPSCRATCHSGE=15,
    MCPX_MEM_COUNT=16
};

enum DirectSound::__unnamed
{
    MCPX_VOICELIST_2D=0,
    MCPX_VOICELIST_3D=1,
    MCPX_VOICELIST_MP=2,
    MCPX_VOICELIST_COUNT=3,
    MCPX_VOICELIST_INVALID=255
};

enum DirectSound::__unnamed
{
    MCPX_NOTIFIER_SSLA_DONE=0,
    MCPX_NOTIFIER_SSLB_DONE=1,
    MCPX_NOTIFIER_SSLA_GETPOS=2,
    MCPX_NOTIFIER_VOICE_OFF=3,
    MCPX_NOTIFIER_COUNT=4
};

enum DirectSound::__unnamed
{
    MCPX_DEFERREDCMD_STREAM_RELEASERESOURCES=1,
    MCPX_DEFERREDCMD_STREAM_COMPLETEPACKETS=2,
    MCPX_DEFERREDCMD_STREAM_FLUSH=3,
    MCPX_DEFERREDCMD_STREAM_STOP=4,
    MCPX_DEFERREDCMD_STREAM_COUNT=5
};

enum DirectSound::__unnamed
{
    DS3DALG_FULL_HRTF=0,
    DS3DALG_LIGHT_HRTF=1,
    DS3DALG_PAN=2,
    DS3DALG_COUNT=3
};

enum DirectSound::__unnamed
{
    MCPX_DEFERREDCMD_VOICE_CHECKSTUCK=0,
    MCPX_DEFERREDCMD_VOICE_COUNT=1
};

struct DirectSound::PAN3DSPEAKER// Size=0x14 (Id=2594)
{
    union // Size=0xc (Id=0)
    {
        struct _D3DVECTOR vSpeakerPos;// Offset=0x0 Size=0xc
        struct _VECTOR4 v4SpeakerPos;// Offset=0x0 Size=0x10
    };
    unsigned long dwMixBin;// Offset=0x10 Size=0x4
};

struct DirectSound::DSWAVEFORMAT// Size=0xc (Id=2595)
{
    unsigned short wFormatTag;// Offset=0x0 Size=0x2
    unsigned char nChannels;// Offset=0x2 Size=0x1
    unsigned char wBitsPerSample;// Offset=0x3 Size=0x1
    unsigned long nSamplesPerSec;// Offset=0x4 Size=0x4
    unsigned long nBlockAlign;// Offset=0x8 Size=0x4
};

struct DirectSound::HRTFLISTENER// Size=0x70 (Id=2596)
{
    struct _D3DVECTOR vNormOrient;// Offset=0x0 Size=0xc
    struct DirectSound::PAN3DSPEAKER aSpeakers[5];// Offset=0xc Size=0x64
};

struct DirectSound::FIRFILTER8// Size=0x20 (Id=2597)
{
    unsigned char Coeff[31];// Offset=0x0 Size=0x1f
    unsigned char Delay;// Offset=0x1f Size=0x1
};

struct DirectSound::HRTFFILTERPAIR// Size=0x8 (Id=2598)
{
    struct DirectSound::FIRFILTER8 * pLeftFilter;// Offset=0x0 Size=0x4
    struct DirectSound::FIRFILTER8 * pRightFilter;// Offset=0x4 Size=0x4
};

class DirectSound::CMcpxDspImage// Size=0x8 (Id=2599)
{
    union // Size=0xe (Id=0)
    {
        void CMcpxDspImage();// Offset=0x0 Size=0xa
        void Initialize();// Offset=0x0 Size=0xe
        void * GetLoader();// Offset=0x0 Size=0x3
        unsigned long GetLoaderSize();// Offset=0x0 Size=0x4
        void * m_pLoader;// Offset=0x0 Size=0x4
        unsigned long m_uLoaderSize;// Offset=0x4 Size=0x4
    };
};

struct DirectSound::_MCPX_DSP_MBOX::__unnamed// Size=0x2 (Id=2600)
{
    unsigned char write;// Offset=0x0 Size=0x1
    unsigned char read;// Offset=0x1 Size=0x1
};

union DirectSound::_MCPX_DSP_MBOX// Size=0x2 (Id=2601)
{
    struct DirectSound::_MCPX_DSP_MBOX::__unnamed Ptr;// Offset=0x0 Size=0x2
    unsigned short uVal;// Offset=0x0 Size=0x2
};

class DirectSound::CMcpxBuffer : public DirectSound::CMcpxVoiceClient// Size=0x148 (Id=2602)
{
    union // Size=0x17f (Id=0)
    {
        unsigned char __align0[144];// Offset=0x0 Size=0x90
        class DirectSound::CDirectSoundBufferSettings * m_pSettings;// Offset=0x90 Size=0x4
        struct DirectSound::SGEHEAPRUNMARKER * m_pSgeHeapEntry;// Offset=0x94 Size=0x4
        struct DirectSound::MCPX_DEFERRED_COMMAND m_aDeferredCommands[5];// Offset=0x98 Size=0xa0
        unsigned long m_dwCachedPlayCursor;// Offset=0x138 Size=0x4
        unsigned long m_dwLastNotifyPosition;// Offset=0x13c Size=0x4
        unsigned long m_dwNextNotifyIndex;// Offset=0x140 Size=0x4
        unsigned long m_dwBufferBase;// Offset=0x144 Size=0x4
        void CMcpxBuffer(class DirectSound::CMcpxBuffer & );
        void CMcpxBuffer(class DirectSound::CMcpxAPU * ,class DirectSound::CDirectSoundBufferSettings * );// Offset=0x0 Size=0x5b
        void ~CMcpxBuffer();// Offset=0x0 Size=0x3d
        long Initialize();// Offset=0x0 Size=0x24
        long Play(long long ,unsigned long );// Offset=0x0 Size=0x39
        long Play(unsigned long );// Offset=0x0 Size=0x7c
        long Stop(long long ,unsigned long );// Offset=0x0 Size=0x39
        long Stop(unsigned long );// Offset=0x0 Size=0x90
        long GetStatus(unsigned long * );// Offset=0x0 Size=0x49
        long SetBufferData();// Offset=0x0 Size=0x1a
        long ReleaseBufferData(int );// Offset=0x0 Size=0x31
        long SetPlayRegion();// Offset=0x0 Size=0x4b
        long SetLoopRegion();// Offset=0x0 Size=0x1e
        long GetCurrentPosition(unsigned long * ,unsigned long * );// Offset=0x0 Size=0xd9
        long SetCurrentPosition(unsigned long );// Offset=0x0 Size=0xf4
        long SetNotificationPositions();// Offset=0x0 Size=0x54
        long AllocateBufferResources();// Offset=0x0 Size=0x4e
        void ReleaseBufferResources();// Offset=0x0 Size=0x4d
        void PlayFromCurrent(unsigned long );// Offset=0x0 Size=0x141
        void PlayFromPosition(unsigned long ,unsigned long );// Offset=0x0 Size=0x17f
        long MapInputBuffer();// Offset=0x0 Size=0x68
        void MapEffectsBuffer();// Offset=0x0 Size=0x52
        void MapBuffer(unsigned long );// Offset=0x0 Size=0x72
        void MapBuffer();// Offset=0x0 Size=0x12
        void UnmapBuffer();// Offset=0x0 Size=0x46
        int ScheduleDeferredCommand(unsigned long ,long long ,unsigned long );// Offset=0x0 Size=0x2f
        void RemoveDeferredCommand(unsigned long );// Offset=0x0 Size=0x1a
        void ServiceDeferredCommand(unsigned long ,unsigned long );// Offset=0x0 Size=0x41
        void OnPositionDelta();// Offset=0x0 Size=0x9f
        void NotifyToPosition(unsigned long ,int );// Offset=0x0 Size=0x61
        void NotifyStop();// Offset=0x0 Size=0x3a
        int ServiceVoiceInterrupt();// Offset=0x0 Size=0x47
        void OnDeferredTerminate();// Offset=0x0 Size=0x1b
        void * __vecDelDtor(unsigned int );
    };
};

class DirectSound::CMcpxBuffer : public DirectSound::CMcpxVoiceClient// Size=0x148 (Id=2603)
{
    union // Size=0x17f (Id=0)
    {
        unsigned char __align0[144];// Offset=0x0 Size=0x90
        class DirectSound::CDirectSoundBufferSettings * m_pSettings;// Offset=0x90 Size=0x4
        struct DirectSound::SGEHEAPRUNMARKER * m_pSgeHeapEntry;// Offset=0x94 Size=0x4
        struct DirectSound::MCPX_DEFERRED_COMMAND m_aDeferredCommands[5];// Offset=0x98 Size=0xa0
        unsigned long m_dwCachedPlayCursor;// Offset=0x138 Size=0x4
        unsigned long m_dwLastNotifyPosition;// Offset=0x13c Size=0x4
        unsigned long m_dwNextNotifyIndex;// Offset=0x140 Size=0x4
        unsigned long m_dwBufferBase;// Offset=0x144 Size=0x4
        void CMcpxBuffer(class DirectSound::CMcpxBuffer & );
        void CMcpxBuffer(class DirectSound::CMcpxAPU * ,class DirectSound::CDirectSoundBufferSettings * );// Offset=0x0 Size=0x5b
        void ~CMcpxBuffer();// Offset=0x0 Size=0x3d
        long Initialize();// Offset=0x0 Size=0x24
        long Play(long long ,unsigned long );// Offset=0x0 Size=0x39
        long Play(unsigned long );// Offset=0x0 Size=0x7c
        long Stop(long long ,unsigned long );// Offset=0x0 Size=0x39
        long Stop(unsigned long );// Offset=0x0 Size=0x90
        long GetStatus(unsigned long * );// Offset=0x0 Size=0x49
        long SetBufferData();// Offset=0x0 Size=0x1a
        long ReleaseBufferData(int );// Offset=0x0 Size=0x31
        long SetPlayRegion();// Offset=0x0 Size=0x4b
        long SetLoopRegion();// Offset=0x0 Size=0x1e
        long GetCurrentPosition(unsigned long * ,unsigned long * );// Offset=0x0 Size=0xd9
        long SetCurrentPosition(unsigned long );// Offset=0x0 Size=0xf4
        long SetNotificationPositions();// Offset=0x0 Size=0x54
        long AllocateBufferResources();// Offset=0x0 Size=0x4e
        void ReleaseBufferResources();// Offset=0x0 Size=0x4d
        void PlayFromCurrent(unsigned long );// Offset=0x0 Size=0x141
        void PlayFromPosition(unsigned long ,unsigned long );// Offset=0x0 Size=0x17f
        long MapInputBuffer();// Offset=0x0 Size=0x68
        void MapEffectsBuffer();// Offset=0x0 Size=0x52
        void MapBuffer(unsigned long );// Offset=0x0 Size=0x72
        void MapBuffer();// Offset=0x0 Size=0x12
        void UnmapBuffer();// Offset=0x0 Size=0x46
        int ScheduleDeferredCommand(unsigned long ,long long ,unsigned long );// Offset=0x0 Size=0x2f
        void RemoveDeferredCommand(unsigned long );// Offset=0x0 Size=0x1a
        void ServiceDeferredCommand(unsigned long ,unsigned long );// Offset=0x0 Size=0x41
        void OnPositionDelta();// Offset=0x0 Size=0x9f
        void NotifyToPosition(unsigned long ,int );// Offset=0x0 Size=0x61
        void NotifyStop();// Offset=0x0 Size=0x3a
        int ServiceVoiceInterrupt();// Offset=0x0 Size=0x47
        void OnDeferredTerminate();// Offset=0x0 Size=0x1b
        void * __vecDelDtor(unsigned int );
    };
};

enum DirectSound::CLightHrtfSource::__unnamed
{
    DS3DALG=1
};

class DirectSound::CLightHrtfSource// Size=0x1 (Id=2605)
{
    enum __unnamed
    {
        DS3DALG=1
    };
    union // Size=0xd8 (Id=0)
    {
        void Calculate3d(unsigned long ,class DirectSound::CHrtfSource * );// Offset=0x0 Size=0x46
        void GetHrtfFilterPair(class DirectSound::CHrtfSource * );// Offset=0x0 Size=0x95
        void CalcNormPos(class DirectSound::CHrtfSource * );// Offset=0x0 Size=0x44
        void CalcLeftRightGains(class DirectSound::CHrtfSource * );// Offset=0x0 Size=0xd8
        void CalcFrontRearGains(class DirectSound::CHrtfSource * );// Offset=0x0 Size=0xd8
    };
};

class DirectSound::CMcpxDspScratchQ// Size=0x1c (Id=2606)
{
    public void CMcpxDspScratchQ(unsigned long ,unsigned char ,class DirectSound::CMcpxCore * ,class DirectSound::CMcpxDspScratchDma * );
    public unsigned char Start();
    public void Stop();
    public int Read(unsigned long * ,unsigned long * );
    public int Write(unsigned long ,unsigned long ,unsigned long );
    public unsigned long GetSize();
    public unsigned long GetOffset();
    private unsigned char GetAvailableSlots();
    private void Incr(unsigned char * );
    private unsigned long m_uBaseOffset;// Offset=0x0 Size=0x4
    private unsigned long m_uRegOffset;// Offset=0x4 Size=0x4
    private union DirectSound::_MCPX_DSP_MBOX m_MailBox;// Offset=0x8 Size=0x2
    private unsigned char __align0[2];// Offset=0xa Size=0x2
    private class DirectSound::CMcpxCore * m_pApu;// Offset=0xc Size=0x4
    private class DirectSound::CMcpxDspScratchDma * m_pScratchDma;// Offset=0x10 Size=0x4
    private unsigned long m_uStartFlag;// Offset=0x14 Size=0x4
    private unsigned char m_uWrapIndex;// Offset=0x18 Size=0x1
};

class DirectSound::CIrql// Size=0x8 (Id=2607)
{
    union // Size=0x33 (Id=0)
    {
        unsigned char m_irql;// Offset=0x0 Size=0x1
        unsigned char __align0[3];// Offset=0x1 Size=0x3
        int m_fRaised;// Offset=0x4 Size=0x4
        void CIrql();// Offset=0x0 Size=0x7
        void Raise();// Offset=0x0 Size=0x33
        void Lower();// Offset=0x0 Size=0x17
    };
};

enum DirectSound::CFullHrtfSource::__unnamed
{
    DS3DALG=0
};

class DirectSound::CFullHrtfSource// Size=0x1 (Id=2609)
{
    enum __unnamed
    {
        DS3DALG=0
    };
    union // Size=0x143 (Id=0)
    {
        void Calculate3d(unsigned long ,class DirectSound::CHrtfSource * );// Offset=0x0 Size=0x5c
        void GetHrtfFilterPair(class DirectSound::CHrtfSource * );// Offset=0x0 Size=0x137
        void CalcNormPos(class DirectSound::CHrtfSource * );// Offset=0x0 Size=0x53
        void CalcLeftRightGains(class DirectSound::CHrtfSource * );// Offset=0x0 Size=0x143
        void CalcDistanceVolume(class DirectSound::CHrtfSource * );// Offset=0x0 Size=0x7b
        void CalcDirection(class DirectSound::CHrtfSource * );// Offset=0x0 Size=0x9b
        void CalcConeVolume(class DirectSound::CHrtfSource * );// Offset=0x0 Size=0x9a
        void CalcFrontRearGains(class DirectSound::CHrtfSource * );// Offset=0x0 Size=0xf0
        void CalcDoppler(class DirectSound::CHrtfSource * );// Offset=0x0 Size=0xa8
    };
};

enum DirectSound::MCPX_FE_STATE
{
    MCPX_FE_STATE_HALTED=0,
    MCPX_FE_STATE_FREE_RUNNING=1,
    MCPX_FE_STATE_ISO=2,
    MCPX_FE_STATE_NON_ISO=3
};

struct DirectSound::MCPX_VOICE_VOLUME// Size=0x24 (Id=2612)
{
    unsigned long TarVOLA[3];// Offset=0x0 Size=0xc
    unsigned long TarVOLB[3];// Offset=0xc Size=0xc
    unsigned long TarVOLC[3];// Offset=0x18 Size=0xc
};

class DirectSound::CAc97Device// Size=0x3c (Id=2613)
{
    union // Size=0xdf (Id=0)
    {
        const struct tWAVEFORMATEX m_wfxFormat;// Offset=0x0 Size=0x12
        const unsigned long m_dwAc97RegisterBase;// Offset=0x0 Size=0x4
        const unsigned long m_dwAciRegisterBase;// Offset=0x0 Size=0x4
        class DirectSound::CAc97Device * m_pDevice;// Offset=0x0 Size=0x4
        class DirectSound::CAc97Channel * m_apChannels[2];// Offset=0x0 Size=0x8
        unsigned long m_dwFlags;// Offset=0x8 Size=0x4
        struct _KINTERRUPT m_Interrupt;// Offset=0x0 Size=0x70
        struct _HAL_SHUTDOWN_REGISTRATION m_HalShutdownData;// Offset=0xc Size=0x10
        struct _KDPC m_dpc;// Offset=0x1c Size=0x1c
        unsigned char m_abPendingBufferCompletions[2][2];// Offset=0x38 Size=0x4
        void CAc97Device();// Offset=0x0 Size=0x8
        void ~CAc97Device();// Offset=0x0 Size=0xd
        long Initialize(unsigned long );// Offset=0x0 Size=0xdf
        void Terminate();// Offset=0x0 Size=0x6f
        long CreateChannel(enum DirectSound::AC97CHANNELTYPE ,class DirectSound::CAc97Channel ** );// Offset=0x0 Size=0x68
        void ReleaseChannel(class DirectSound::CAc97Channel * );// Offset=0x0 Size=0xe
        void ReleaseChannel(enum DirectSound::AC97CHANNELTYPE );// Offset=0x0 Size=0x3f
        int CodecReady();// Offset=0x0 Size=0x50
        long PowerUp();// Offset=0x0 Size=0x18
        unsigned long ServiceAciInterrupt();// Offset=0x0 Size=0xd4
        void ServiceAciInterruptDpc();// Offset=0x0 Size=0x59
        void SynchronizeAciInterrupt();// Offset=0x0 Size=0x14
        unsigned long GetInterruptStatus();// Offset=0x0 Size=0x9
        int AcquireCodecSemaphore();// Offset=0x0 Size=0x2d
        unsigned char PeekRegister8(unsigned long );// Offset=0x0 Size=0x9
        void PokeRegister8(unsigned long ,unsigned char );// Offset=0x0 Size=0xd
        unsigned short PeekRegister16(unsigned long );// Offset=0x0 Size=0xa
        void PokeRegister16(unsigned long ,unsigned short );// Offset=0x0 Size=0xf
        unsigned long PeekRegister32(unsigned long );// Offset=0x0 Size=0x9
        void PokeRegister32(unsigned long ,unsigned long );// Offset=0x0 Size=0xd
        unsigned char PeekAciRegister8(unsigned long );// Offset=0x0 Size=0xd
        void PokeAciRegister8(unsigned long ,unsigned char );// Offset=0x0 Size=0x11
        unsigned short PeekAciRegister16(unsigned long );// Offset=0x0 Size=0xe
        void PokeAciRegister16(unsigned long ,unsigned short );// Offset=0x0 Size=0x13
        unsigned long PeekAciRegister32(unsigned long );// Offset=0x0 Size=0xd
        void PokeAciRegister32(unsigned long ,unsigned long );// Offset=0x0 Size=0x11
        int PeekAc97Register(enum AC97REGISTER ,unsigned short * );// Offset=0x0 Size=0x37
        int PokeAc97Register(enum AC97REGISTER ,unsigned short );// Offset=0x0 Size=0x1d
        int VerifyPokeAc97Register(enum AC97REGISTER ,unsigned short );// Offset=0x0 Size=0x31
        void WaitRegisterRetry();// Offset=0x0 Size=0x9
        unsigned char AciInterruptServiceRoutine(struct _KINTERRUPT * ,void * );// Offset=0x0 Size=0x11
        void AciInterruptDpcHandler(struct _KDPC * ,void * ,void * ,void * );// Offset=0x0 Size=0xc
        void AciShutdownNotifier(struct _HAL_SHUTDOWN_REGISTRATION * );// Offset=0x0 Size=0xf
        unsigned char AciSynchronizationRoutine(void * );// Offset=0x0 Size=0xe
        void * __vecDelDtor(unsigned int );
    };
};

class DirectSound::CFileMediaObject : public XFileMediaObject, public CRefCount, protected DirectSound::CStdFileStream// Size=0x14 (Id=2614)
{
    public void CFileMediaObject(class DirectSound::CFileMediaObject & );
    union // Size=0x9d (Id=0)
    {
        void CFileMediaObject();// Offset=0x0 Size=0x2a
        void ~CFileMediaObject();// Offset=0x0 Size=0x33
        long Initialize(void * );// Offset=0x0 Size=0x15
        long Initialize(char * ,unsigned long ,unsigned long ,unsigned long ,unsigned long );// Offset=0x0 Size=0x21
        unsigned long AddRef();// Offset=0x0 Size=0xa
        unsigned long Release();// Offset=0x0 Size=0xa
        long GetInfo(struct _XMEDIAINFO * );// Offset=0x0 Size=0x18
        long GetStatus(unsigned long * );// Offset=0x0 Size=0xf
        long Process(struct _XMEDIAPACKET * ,struct _XMEDIAPACKET * );// Offset=0x0 Size=0x9d
        long Discontinuity();// Offset=0x0 Size=0x5
        long Flush();// Offset=0x0 Size=0x12
        long Seek(long ,unsigned long ,unsigned long * );// Offset=0x0 Size=0x1b
        long GetLength(unsigned long * );// Offset=0x0 Size=0x13
        void __local_vftable_ctor_closure();
        void * __vecDelDtor(unsigned int );
    };
};

class DirectSound::CFileMediaObject : public XFileMediaObject, public CRefCount, protected DirectSound::CStdFileStream// Size=0x14 (Id=2615)
{
    public void CFileMediaObject(class DirectSound::CFileMediaObject & );
    union // Size=0x9d (Id=0)
    {
        void CFileMediaObject();// Offset=0x0 Size=0x2a
        void ~CFileMediaObject();// Offset=0x0 Size=0x33
        long Initialize(void * );// Offset=0x0 Size=0x15
        long Initialize(char * ,unsigned long ,unsigned long ,unsigned long ,unsigned long );// Offset=0x0 Size=0x21
        unsigned long AddRef();// Offset=0x0 Size=0xa
        unsigned long Release();// Offset=0x0 Size=0xa
        long GetInfo(struct _XMEDIAINFO * );// Offset=0x0 Size=0x18
        long GetStatus(unsigned long * );// Offset=0x0 Size=0xf
        long Process(struct _XMEDIAPACKET * ,struct _XMEDIAPACKET * );// Offset=0x0 Size=0x9d
        long Discontinuity();// Offset=0x0 Size=0x5
        long Flush();// Offset=0x0 Size=0x12
        long Seek(long ,unsigned long ,unsigned long * );// Offset=0x0 Size=0x1b
        long GetLength(unsigned long * );// Offset=0x0 Size=0x13
        void __local_vftable_ctor_closure();
        void * __vecDelDtor(unsigned int );
    };
};

class DirectSound::CDirectSoundStream : public IDirectSoundStream, public DirectSound::CDirectSoundVoice// Size=0x28 (Id=2616)
{
    union // Size=0xaa (Id=0)
    {
        unsigned char __align0[32];// Offset=0x0 Size=0x20
        class DirectSound::CDirectSoundStreamSettings * m_pSettings;// Offset=0x20 Size=0x4
        class DirectSound::CMcpxStream * m_pStream;// Offset=0x24 Size=0x4
        void CDirectSoundStream(class DirectSound::CDirectSoundStream & );
        void CDirectSoundStream(class DirectSound::CDirectSound * );// Offset=0x0 Size=0x25
        void ~CDirectSoundStream();// Offset=0x0 Size=0x41
        long Initialize(struct _DSSTREAMDESC * );// Offset=0x0 Size=0xaa
        unsigned long AddRef();// Offset=0x0 Size=0xa
        unsigned long Release();// Offset=0x0 Size=0xa
        long GetInfo(struct _XMEDIAINFO * );// Offset=0x0 Size=0x67
        long GetStatus(unsigned long * );// Offset=0x0 Size=0x51
        long Process(struct _XMEDIAPACKET * ,struct _XMEDIAPACKET * );// Offset=0x0 Size=0x77
        long Discontinuity();// Offset=0x0 Size=0x4d
        long Flush();// Offset=0x0 Size=0x4d
        long SetFormat(struct tWAVEFORMATEX * );// Offset=0x0 Size=0x52
        long SetFrequency(unsigned long );// Offset=0x0 Size=0x52
        long SetPitch(long );// Offset=0x0 Size=0x52
        long SetVolume(long );// Offset=0x0 Size=0x52
        long SetLFO(struct _DSLFODESC * );// Offset=0x0 Size=0x52
        long SetEG(struct _DSENVELOPEDESC * );// Offset=0x0 Size=0x52
        long SetFilter(struct _DSFILTERDESC * );// Offset=0x0 Size=0x52
        long SetHeadroom(unsigned long );// Offset=0x0 Size=0x52
        long SetOutputBuffer(struct IDirectSoundBuffer * );// Offset=0x0 Size=0x52
        long SetMixBins(struct _DSMIXBINS * );// Offset=0x0 Size=0x52
        long SetMixBinVolumes(struct _DSMIXBINS * );// Offset=0x0 Size=0x52
        long Pause(unsigned long );// Offset=0x0 Size=0x51
        long FlushEx(long long ,unsigned long );// Offset=0x0 Size=0x66
        long SetAllParameters(struct _DS3DBUFFER * ,unsigned long );// Offset=0x0 Size=0x56
        long SetConeAngles(unsigned long ,unsigned long ,unsigned long );// Offset=0x0 Size=0x5a
        long SetConeOrientation(float ,float ,float ,unsigned long );// Offset=0x0 Size=0x6b
        long SetConeOutsideVolume(long ,unsigned long );// Offset=0x0 Size=0x56
        long SetMaxDistance(float ,unsigned long );// Offset=0x0 Size=0x5a
        long SetMinDistance(float ,unsigned long );// Offset=0x0 Size=0x5a
        long SetMode(unsigned long ,unsigned long );// Offset=0x0 Size=0x56
        long SetPosition(float ,float ,float ,unsigned long );// Offset=0x0 Size=0x6b
        long SetVelocity(float ,float ,float ,unsigned long );// Offset=0x0 Size=0x6b
        long SetDistanceFactor(float ,unsigned long );// Offset=0x0 Size=0x5a
        long SetDopplerFactor(float ,unsigned long );// Offset=0x0 Size=0x5a
        long SetRolloffFactor(float ,unsigned long );// Offset=0x0 Size=0x5a
        long SetI3DL2Source(struct _DSI3DL2BUFFER * ,unsigned long );// Offset=0x0 Size=0x56
        void * __vecDelDtor(unsigned int );
    };
};

class DirectSound::CDirectSoundStream : public IDirectSoundStream, public DirectSound::CDirectSoundVoice// Size=0x28 (Id=2617)
{
    union // Size=0xaa (Id=0)
    {
        unsigned char __align0[32];// Offset=0x0 Size=0x20
        class DirectSound::CDirectSoundStreamSettings * m_pSettings;// Offset=0x20 Size=0x4
        class DirectSound::CMcpxStream * m_pStream;// Offset=0x24 Size=0x4
        void CDirectSoundStream(class DirectSound::CDirectSoundStream & );
        void CDirectSoundStream(class DirectSound::CDirectSound * );// Offset=0x0 Size=0x25
        void ~CDirectSoundStream();// Offset=0x0 Size=0x41
        long Initialize(struct _DSSTREAMDESC * );// Offset=0x0 Size=0xaa
        unsigned long AddRef();// Offset=0x0 Size=0xa
        unsigned long Release();// Offset=0x0 Size=0xa
        long GetInfo(struct _XMEDIAINFO * );// Offset=0x0 Size=0x67
        long GetStatus(unsigned long * );// Offset=0x0 Size=0x51
        long Process(struct _XMEDIAPACKET * ,struct _XMEDIAPACKET * );// Offset=0x0 Size=0x77
        long Discontinuity();// Offset=0x0 Size=0x4d
        long Flush();// Offset=0x0 Size=0x4d
        long SetFormat(struct tWAVEFORMATEX * );// Offset=0x0 Size=0x52
        long SetFrequency(unsigned long );// Offset=0x0 Size=0x52
        long SetPitch(long );// Offset=0x0 Size=0x52
        long SetVolume(long );// Offset=0x0 Size=0x52
        long SetLFO(struct _DSLFODESC * );// Offset=0x0 Size=0x52
        long SetEG(struct _DSENVELOPEDESC * );// Offset=0x0 Size=0x52
        long SetFilter(struct _DSFILTERDESC * );// Offset=0x0 Size=0x52
        long SetHeadroom(unsigned long );// Offset=0x0 Size=0x52
        long SetOutputBuffer(struct IDirectSoundBuffer * );// Offset=0x0 Size=0x52
        long SetMixBins(struct _DSMIXBINS * );// Offset=0x0 Size=0x52
        long SetMixBinVolumes(struct _DSMIXBINS * );// Offset=0x0 Size=0x52
        long Pause(unsigned long );// Offset=0x0 Size=0x51
        long FlushEx(long long ,unsigned long );// Offset=0x0 Size=0x66
        long SetAllParameters(struct _DS3DBUFFER * ,unsigned long );// Offset=0x0 Size=0x56
        long SetConeAngles(unsigned long ,unsigned long ,unsigned long );// Offset=0x0 Size=0x5a
        long SetConeOrientation(float ,float ,float ,unsigned long );// Offset=0x0 Size=0x6b
        long SetConeOutsideVolume(long ,unsigned long );// Offset=0x0 Size=0x56
        long SetMaxDistance(float ,unsigned long );// Offset=0x0 Size=0x5a
        long SetMinDistance(float ,unsigned long );// Offset=0x0 Size=0x5a
        long SetMode(unsigned long ,unsigned long );// Offset=0x0 Size=0x56
        long SetPosition(float ,float ,float ,unsigned long );// Offset=0x0 Size=0x6b
        long SetVelocity(float ,float ,float ,unsigned long );// Offset=0x0 Size=0x6b
        long SetDistanceFactor(float ,unsigned long );// Offset=0x0 Size=0x5a
        long SetDopplerFactor(float ,unsigned long );// Offset=0x0 Size=0x5a
        long SetRolloffFactor(float ,unsigned long );// Offset=0x0 Size=0x5a
        long SetI3DL2Source(struct _DSI3DL2BUFFER * ,unsigned long );// Offset=0x0 Size=0x56
        void * __vecDelDtor(unsigned int );
    };
};

struct DirectSound::_MCPX_HW_NOTIFICATION// Size=0x10 (Id=2618)
{
    unsigned long GSCNT;// Offset=0x0 Size=0x4
    unsigned long CurrentOffset;// Offset=0x4 Size=0x4
    unsigned long Zero;// Offset=0x8 Size=0x4
    unsigned char Res0;// Offset=0xc Size=0x1
    unsigned char SamplesAvailable;// Offset=0xd Size=0x1
    unsigned char EnvelopeActive;// Offset=0xe Size=0x1
    unsigned char Status;// Offset=0xf Size=0x1
};

class DirectSound::CDirectSoundBufferSettings : public DirectSound::CDirectSoundVoiceSettings// Size=0xdc (Id=2619)
{
    union // Size=0xdc (Id=0)
    {
        const struct tWAVEFORMATEX m_wfxMixDest;// Offset=0x0 Size=0x12
        unsigned char __align0[166];// Offset=0x12 Size=0xa6
        void * m_pvBufferData;// Offset=0xb8 Size=0x4
        unsigned long m_dwBufferSize;// Offset=0xbc Size=0x4
        unsigned long m_dwPlayStart;// Offset=0xc0 Size=0x4
        unsigned long m_dwPlayLength;// Offset=0xc4 Size=0x4
        unsigned long m_dwLoopStart;// Offset=0xc8 Size=0x4
        unsigned long m_dwLoopLength;// Offset=0xcc Size=0x4
        unsigned long m_dwInputMixBin;// Offset=0xd0 Size=0x4
        struct _DSBPOSITIONNOTIFY * m_paNotifies;// Offset=0xd4 Size=0x4
        unsigned long m_dwNotifyCount;// Offset=0xd8 Size=0x4
        void CDirectSoundBufferSettings(class DirectSound::CDirectSoundBufferSettings & );
        void CDirectSoundBufferSettings();// Offset=0x0 Size=0x10
        void ~CDirectSoundBufferSettings();// Offset=0x0 Size=0x25
        long Initialize(struct _DSBUFFERDESC * );// Offset=0x0 Size=0x65
        long SetBufferData(void * ,unsigned long );// Offset=0x0 Size=0x8c
        void SetPlayRegion(unsigned long ,unsigned long );// Offset=0x0 Size=0x24
        void SetLoopRegion(unsigned long ,unsigned long );// Offset=0x0 Size=0x17
        long SetNotificationPositions(unsigned long ,struct _DSBPOSITIONNOTIFY * );// Offset=0x0 Size=0xd5
        void __local_vftable_ctor_closure();
        void * __vecDelDtor(unsigned int );
    };
};

class DirectSound::CDirectSoundBufferSettings : public DirectSound::CDirectSoundVoiceSettings// Size=0xdc (Id=2620)
{
    union // Size=0xdc (Id=0)
    {
        const struct tWAVEFORMATEX m_wfxMixDest;// Offset=0x0 Size=0x12
        unsigned char __align0[166];// Offset=0x12 Size=0xa6
        void * m_pvBufferData;// Offset=0xb8 Size=0x4
        unsigned long m_dwBufferSize;// Offset=0xbc Size=0x4
        unsigned long m_dwPlayStart;// Offset=0xc0 Size=0x4
        unsigned long m_dwPlayLength;// Offset=0xc4 Size=0x4
        unsigned long m_dwLoopStart;// Offset=0xc8 Size=0x4
        unsigned long m_dwLoopLength;// Offset=0xcc Size=0x4
        unsigned long m_dwInputMixBin;// Offset=0xd0 Size=0x4
        struct _DSBPOSITIONNOTIFY * m_paNotifies;// Offset=0xd4 Size=0x4
        unsigned long m_dwNotifyCount;// Offset=0xd8 Size=0x4
        void CDirectSoundBufferSettings(class DirectSound::CDirectSoundBufferSettings & );
        void CDirectSoundBufferSettings();// Offset=0x0 Size=0x10
        void ~CDirectSoundBufferSettings();// Offset=0x0 Size=0x25
        long Initialize(struct _DSBUFFERDESC * );// Offset=0x0 Size=0x65
        long SetBufferData(void * ,unsigned long );// Offset=0x0 Size=0x8c
        void SetPlayRegion(unsigned long ,unsigned long );// Offset=0x0 Size=0x24
        void SetLoopRegion(unsigned long ,unsigned long );// Offset=0x0 Size=0x17
        long SetNotificationPositions(unsigned long ,struct _DSBPOSITIONNOTIFY * );// Offset=0x0 Size=0xd5
        void __local_vftable_ctor_closure();
        void * __vecDelDtor(unsigned int );
    };
};

enum DirectSound::CI3dl2Listener::__unnamed
{
    MainDelayLineID=0,
    ReflectionDelayLineID=4,
    ShortReverbDelayLineID=8,
    LongReverbDelayLineID=12
};

enum DirectSound::CI3dl2Listener::__unnamed
{
    InputIIR=0,
    MainDelayLineLongReverbIIR=1,
    ShortReverbIIR=4,
    LongReverbIIR=8
};

class DirectSound::CI3dl2Listener// Size=0x224 (Id=2623)
{
    union // Size=0x317 (Id=0)
    {
        struct _DSI3DL2LISTENER & m_I3dl2Params;// Offset=0x0 Size=0x4
        struct _DSFX_I3DL2REVERB_PARAMS m_I3dl2Data;// Offset=0x4 Size=0x220
        const float m_flScale23;// Offset=0x0 Size=0x4
        const float m_flScale16;// Offset=0x0 Size=0x4
        const unsigned long m_dwSamplesPerSec;// Offset=0x0 Size=0x4
        float m_aflReflectionData[4][5];// Offset=0x0 Size=0x50
        float m_aflShortReverbInputFactor[4][2];// Offset=0x0 Size=0x20
        float m_aflLongReverbInputDelay[2][4];// Offset=0x0 Size=0x20
        float m_aflShortReverbFeedbackDelay[4];// Offset=0x0 Size=0x10
        enum __unnamed
        {
            MainDelayLineID=0,
            ReflectionDelayLineID=4,
            ShortReverbDelayLineID=8,
            LongReverbDelayLineID=12
        };
        enum __unnamed
        {
            InputIIR=0,
            MainDelayLineLongReverbIIR=1,
            ShortReverbIIR=4,
            LongReverbIIR=8
        };
        void CI3dl2Listener(struct _DSI3DL2LISTENER & );// Offset=0x0 Size=0xb
        void Initialize();// Offset=0x0 Size=0x37
        void CalculateI3dl2();// Offset=0x0 Size=0x317
        void SetSize(float );// Offset=0x0 Size=0x85
        void SetInputFilter(long ,float );// Offset=0x0 Size=0x5f
        void SetReflectionsGain(float );// Offset=0x0 Size=0x33
        void SetReflectionsDelay(float );// Offset=0x0 Size=0x1c
        void SetReverbGain(float );// Offset=0x0 Size=0x35
        void SetReverbDelay(float );// Offset=0x0 Size=0x90
        void SetDecayTimes(float ,float ,float );// Offset=0x0 Size=0xd7
        void SetDiffusion(float );// Offset=0x0 Size=0x77
        void SetDecayFilter(struct _DSFX_I3DL2REVERB_IIR * ,unsigned long ,float ,float ,float );// Offset=0x0 Size=0xab
        void Get1PoleLoPass(long ,long ,float ,float ,float * ,float * );// Offset=0x0 Size=0xbf
        void Get1PoleLoPass(long ,long ,float ,float ,int * ,int * );// Offset=0x0 Size=0x57
    };
};

struct DirectSound::HRTFSOURCE// Size=0x1c (Id=2624)
{
    struct _D3DVECTOR vNormPos;// Offset=0x0 Size=0xc
    float flMagPos;// Offset=0xc Size=0x4
    float flAzimuth;// Offset=0x10 Size=0x4
    float flElevation;// Offset=0x14 Size=0x4
    float flThetaS;// Offset=0x18 Size=0x4
};

struct DirectSound::MCPX_DEFERRED_COMMAND// Size=0x20 (Id=2625)
{
    struct _LIST_ENTRY leListEntry;// Offset=0x0 Size=0x8
    unsigned long dwFlags;// Offset=0x8 Size=0x4
    class DirectSound::CMcpxVoiceClient * pVoice;// Offset=0xc Size=0x4
    unsigned long dwCommand;// Offset=0x10 Size=0x4
    unsigned long dwContext;// Offset=0x14 Size=0x4
    long long rtTimestamp;// Offset=0x18 Size=0x8
};

struct DirectSound::MCPX_VOICE_REGCACHE// Size=0x30 (Id=2626)
{
    unsigned long CfgFMT;// Offset=0x0 Size=0x4
    unsigned long CfgMISC;// Offset=0x4 Size=0x4
    unsigned long CfgENV0;// Offset=0x8 Size=0x4
    unsigned long CfgENVA;// Offset=0xc Size=0x4
    unsigned long CfgENV1;// Offset=0x10 Size=0x4
    unsigned long CfgENVF;// Offset=0x14 Size=0x4
    unsigned long CfgLFODLY;// Offset=0x18 Size=0x4
    unsigned long LFOENV;// Offset=0x1c Size=0x4
    unsigned long LFOMOD;// Offset=0x20 Size=0x4
    unsigned long TarFCA;// Offset=0x24 Size=0x4
    unsigned long TarFCB;// Offset=0x28 Size=0x4
    unsigned long VoiceOn;// Offset=0x2c Size=0x4
};

struct DirectSound::MCPX_ALLOC_CONTEXT// Size=0x10 (Id=2627)
{
    void * VirtualAddress;// Offset=0x0 Size=0x4
    unsigned long PhysicalAddress;// Offset=0x4 Size=0x4
    unsigned long Size;// Offset=0x8 Size=0x4
    int fOwned;// Offset=0xc Size=0x4
};

class DirectSound::CAutoIrql : public DirectSound::CIrql// Size=0x8 (Id=2628)
{
    union // Size=0x10 (Id=0)
    {
        void CAutoIrql();// Offset=0x0 Size=0x10
        void ~CAutoIrql();// Offset=0x0 Size=0x5
        void * __vecDelDtor(unsigned int );
    };
};

class DirectSound::CDirectSoundVoice : public CRefCount// Size=0x1c (Id=2629)
{
    union // Size=0x137 (Id=0)
    {
        unsigned char __align0[8];// Offset=0x0 Size=0x8
        class DirectSound::CDirectSound * m_pDirectSound;// Offset=0x8 Size=0x4
        class DirectSound::CMcpxVoiceClient * m_pVoice;// Offset=0xc Size=0x4
        class DirectSound::CDirectSoundVoiceSettings * m_pSettings;// Offset=0x10 Size=0x4
        struct _LIST_ENTRY m_le3dVoice;// Offset=0x14 Size=0x8
        void CDirectSoundVoice(class DirectSound::CDirectSoundVoice & );
        void CDirectSoundVoice(class DirectSound::CDirectSound * );// Offset=0x0 Size=0x2a
        void ~CDirectSoundVoice();// Offset=0x0 Size=0x4d
        void Initialize(class DirectSound::CMcpxVoiceClient * ,class DirectSound::CDirectSoundVoiceSettings * );// Offset=0x0 Size=0x3f
        long SetFormat(struct tWAVEFORMATEX * );// Offset=0x0 Size=0x47
        long SetVolume(long );// Offset=0x0 Size=0x1c
        long SetFrequency(unsigned long );// Offset=0x0 Size=0x24
        long SetPitch(long );// Offset=0x0 Size=0x19
        long SetLFO(struct _DSLFODESC * );// Offset=0x0 Size=0x13
        long SetEG(struct _DSENVELOPEDESC * );// Offset=0x0 Size=0x13
        long SetFilter(struct _DSFILTERDESC * );// Offset=0x0 Size=0x13
        long SetHeadroom(unsigned long );// Offset=0x0 Size=0x23
        long SetOutputBuffer(struct IDirectSoundBuffer * );// Offset=0x0 Size=0x54
        long SetMixBins(struct _DSMIXBINS * );// Offset=0x0 Size=0x1d
        long SetMixBinVolumes(struct _DSMIXBINS * );// Offset=0x0 Size=0x1d
        long SetAllParameters(struct _DS3DBUFFER * ,unsigned long );// Offset=0x0 Size=0x137
        long SetConeAngles(unsigned long ,unsigned long ,unsigned long );// Offset=0x0 Size=0x43
        long SetConeOrientation(float ,float ,float ,unsigned long );// Offset=0x0 Size=0x52
        long SetConeOutsideVolume(long ,unsigned long );// Offset=0x0 Size=0x33
        long SetMaxDistance(float ,unsigned long );// Offset=0x0 Size=0x33
        long SetMinDistance(float ,unsigned long );// Offset=0x0 Size=0x33
        long SetMode(unsigned long ,unsigned long );// Offset=0x0 Size=0x26
        long SetPosition(float ,float ,float ,unsigned long );// Offset=0x0 Size=0x52
        long SetVelocity(float ,float ,float ,unsigned long );// Offset=0x0 Size=0x52
        long SetDistanceFactor(float ,unsigned long );// Offset=0x0 Size=0x33
        long SetDopplerFactor(float ,unsigned long );// Offset=0x0 Size=0x33
        long SetRolloffFactor(float ,unsigned long );// Offset=0x0 Size=0x33
        long SetI3DL2Source(struct _DSI3DL2BUFFER * ,unsigned long );// Offset=0x0 Size=0xaf
        long CommitDeferredSettings();// Offset=0x0 Size=0x39
        void * __vecDelDtor(unsigned int );
    };
};

class DirectSound::CDirectSoundVoice : public CRefCount// Size=0x1c (Id=2630)
{
    union // Size=0x137 (Id=0)
    {
        unsigned char __align0[8];// Offset=0x0 Size=0x8
        class DirectSound::CDirectSound * m_pDirectSound;// Offset=0x8 Size=0x4
        class DirectSound::CMcpxVoiceClient * m_pVoice;// Offset=0xc Size=0x4
        class DirectSound::CDirectSoundVoiceSettings * m_pSettings;// Offset=0x10 Size=0x4
        struct _LIST_ENTRY m_le3dVoice;// Offset=0x14 Size=0x8
        void CDirectSoundVoice(class DirectSound::CDirectSoundVoice & );
        void CDirectSoundVoice(class DirectSound::CDirectSound * );// Offset=0x0 Size=0x2a
        void ~CDirectSoundVoice();// Offset=0x0 Size=0x4d
        void Initialize(class DirectSound::CMcpxVoiceClient * ,class DirectSound::CDirectSoundVoiceSettings * );// Offset=0x0 Size=0x3f
        long SetFormat(struct tWAVEFORMATEX * );// Offset=0x0 Size=0x47
        long SetVolume(long );// Offset=0x0 Size=0x1c
        long SetFrequency(unsigned long );// Offset=0x0 Size=0x24
        long SetPitch(long );// Offset=0x0 Size=0x19
        long SetLFO(struct _DSLFODESC * );// Offset=0x0 Size=0x13
        long SetEG(struct _DSENVELOPEDESC * );// Offset=0x0 Size=0x13
        long SetFilter(struct _DSFILTERDESC * );// Offset=0x0 Size=0x13
        long SetHeadroom(unsigned long );// Offset=0x0 Size=0x23
        long SetOutputBuffer(struct IDirectSoundBuffer * );// Offset=0x0 Size=0x54
        long SetMixBins(struct _DSMIXBINS * );// Offset=0x0 Size=0x1d
        long SetMixBinVolumes(struct _DSMIXBINS * );// Offset=0x0 Size=0x1d
        long SetAllParameters(struct _DS3DBUFFER * ,unsigned long );// Offset=0x0 Size=0x137
        long SetConeAngles(unsigned long ,unsigned long ,unsigned long );// Offset=0x0 Size=0x43
        long SetConeOrientation(float ,float ,float ,unsigned long );// Offset=0x0 Size=0x52
        long SetConeOutsideVolume(long ,unsigned long );// Offset=0x0 Size=0x33
        long SetMaxDistance(float ,unsigned long );// Offset=0x0 Size=0x33
        long SetMinDistance(float ,unsigned long );// Offset=0x0 Size=0x33
        long SetMode(unsigned long ,unsigned long );// Offset=0x0 Size=0x26
        long SetPosition(float ,float ,float ,unsigned long );// Offset=0x0 Size=0x52
        long SetVelocity(float ,float ,float ,unsigned long );// Offset=0x0 Size=0x52
        long SetDistanceFactor(float ,unsigned long );// Offset=0x0 Size=0x33
        long SetDopplerFactor(float ,unsigned long );// Offset=0x0 Size=0x33
        long SetRolloffFactor(float ,unsigned long );// Offset=0x0 Size=0x33
        long SetI3DL2Source(struct _DSI3DL2BUFFER * ,unsigned long );// Offset=0x0 Size=0xaf
        long CommitDeferredSettings();// Offset=0x0 Size=0x39
        void * __vecDelDtor(unsigned int );
    };
};

struct DirectSound::MCPX_SSL_DESC// Size=0x10 (Id=2631)
{
    struct _LIST_ENTRY lstPackets;// Offset=0x0 Size=0x8
    unsigned long dwPrdCount;// Offset=0x8 Size=0x4
    unsigned long dwBytesMapped;// Offset=0xc Size=0x4
};

class DirectSound::CWaveFile// Size=0x40 (Id=2632)
{
    union // Size=0x1ef (Id=0)
    {
        class DirectSound::CStdFileStream m_Stream;// Offset=0x0 Size=0x8
        class DirectSound::CRiffChunk m_ParentChunk;// Offset=0x8 Size=0x18
        class DirectSound::CRiffChunk m_DataChunk;// Offset=0x20 Size=0x18
        unsigned long m_dwFileType;// Offset=0x38 Size=0x4
        struct tWAVEFORMATEX * m_pwfxFormat;// Offset=0x3c Size=0x4
        void CWaveFile();// Offset=0x0 Size=0x24
        void ~CWaveFile();// Offset=0x0 Size=0x10
        long Open(char * ,void * );// Offset=0x0 Size=0x1ef
        long Open(void * );
        long Open(char * );
        void Close();// Offset=0x0 Size=0x1b
        long GetFormat(struct tWAVEFORMATEX ** );
        long GetFormat(struct tWAVEFORMATEX * ,unsigned long ,unsigned long * );// Offset=0x0 Size=0x62
        long ReadSample(unsigned long ,void * ,unsigned long ,unsigned long * );// Offset=0x0 Size=0x49
        void ConvertAiffPcm(void * ,unsigned long ,unsigned long );// Offset=0x0 Size=0x3c
        void ConvertAiffPcm(void * ,unsigned long );// Offset=0x0 Size=0x18
        long GetFileType(unsigned long * );
        long GetDataOffset(unsigned long * );
        long GetDuration(unsigned long * );
        long GetLoopRegion(unsigned long * ,unsigned long * );// Offset=0x0 Size=0x96
        long GetWaveFormat(struct tWAVEFORMATEX * ,unsigned long ,unsigned long * );// Offset=0x0 Size=0x8e
        long GetAiffFormat(struct tWAVEFORMATEX * ,unsigned long ,unsigned long * );// Offset=0x0 Size=0x155
        long GetWaveLoopRegion(unsigned long * ,unsigned long * );// Offset=0x0 Size=0x9e
        long GetAiffLoopRegion(unsigned long * ,unsigned long * );// Offset=0x0 Size=0x1cf
        void * __vecDelDtor(unsigned int );
    };
};

class DirectSound::CDirectSoundBuffer : public IDirectSoundBuffer, public DirectSound::CDirectSoundVoice// Size=0x24 (Id=2633)
{
    union // Size=0xe8 (Id=0)
    {
        unsigned char __align0[28];// Offset=0x0 Size=0x1c
        class DirectSound::CDirectSoundBufferSettings * m_pSettings;// Offset=0x1c Size=0x4
        class DirectSound::CMcpxBuffer * m_pBuffer;// Offset=0x20 Size=0x4
        void CDirectSoundBuffer(class DirectSound::CDirectSoundBuffer & );
        void CDirectSoundBuffer(class DirectSound::CDirectSound * );// Offset=0x0 Size=0x18
        void ~CDirectSoundBuffer();// Offset=0x0 Size=0x33
        long Initialize(struct _DSBUFFERDESC * );// Offset=0x0 Size=0xe8
        unsigned long AddRef();// Offset=0x0 Size=0x47
        unsigned long Release();// Offset=0x0 Size=0x4a
        long SetFormat(struct tWAVEFORMATEX * );// Offset=0x0 Size=0x4e
        long Play(unsigned long ,unsigned long ,unsigned long );// Offset=0x0 Size=0x51
        long PlayEx(long long ,unsigned long );// Offset=0x0 Size=0x59
        long Stop();// Offset=0x0 Size=0x4f
        long StopEx(long long ,unsigned long );// Offset=0x0 Size=0x59
        long SetPlayRegion(unsigned long ,unsigned long );// Offset=0x0 Size=0x80
        long SetLoopRegion(unsigned long ,unsigned long );// Offset=0x0 Size=0x85
        long GetStatus(unsigned long * );// Offset=0x0 Size=0x51
        long GetCurrentPosition(unsigned long * ,unsigned long * );// Offset=0x0 Size=0x55
        long SetCurrentPosition(unsigned long );// Offset=0x0 Size=0x51
        long SetBufferData(void * ,unsigned long );// Offset=0x0 Size=0xae
        long Lock(unsigned long ,unsigned long ,void ** ,unsigned long * ,void ** ,unsigned long * ,unsigned long );// Offset=0x0 Size=0xd6
        long SetFrequency(unsigned long );// Offset=0x0 Size=0x4e
        long SetVolume(long );// Offset=0x0 Size=0x4e
        long SetPitch(long );// Offset=0x0 Size=0x4e
        long SetLFO(struct _DSLFODESC * );// Offset=0x0 Size=0x4e
        long SetEG(struct _DSENVELOPEDESC * );// Offset=0x0 Size=0x4e
        long SetFilter(struct _DSFILTERDESC * );// Offset=0x0 Size=0x4e
        long SetHeadroom(unsigned long );// Offset=0x0 Size=0x4e
        long SetOutputBuffer(struct IDirectSoundBuffer * );// Offset=0x0 Size=0x4e
        long SetMixBins(struct _DSMIXBINS * );// Offset=0x0 Size=0x4e
        long SetMixBinVolumes(struct _DSMIXBINS * );// Offset=0x0 Size=0x4e
        long SetAllParameters(struct _DS3DBUFFER * ,unsigned long );// Offset=0x0 Size=0x52
        long SetConeAngles(unsigned long ,unsigned long ,unsigned long );// Offset=0x0 Size=0x56
        long SetConeOrientation(float ,float ,float ,unsigned long );// Offset=0x0 Size=0x67
        long SetConeOutsideVolume(long ,unsigned long );// Offset=0x0 Size=0x52
        long SetMaxDistance(float ,unsigned long );// Offset=0x0 Size=0x56
        long SetMinDistance(float ,unsigned long );// Offset=0x0 Size=0x56
        long SetMode(unsigned long ,unsigned long );// Offset=0x0 Size=0x52
        long SetPosition(float ,float ,float ,unsigned long );// Offset=0x0 Size=0x67
        long SetVelocity(float ,float ,float ,unsigned long );// Offset=0x0 Size=0x67
        long SetDistanceFactor(float ,unsigned long );// Offset=0x0 Size=0x56
        long SetDopplerFactor(float ,unsigned long );// Offset=0x0 Size=0x56
        long SetRolloffFactor(float ,unsigned long );// Offset=0x0 Size=0x56
        long SetI3DL2Source(struct _DSI3DL2BUFFER * ,unsigned long );// Offset=0x0 Size=0x52
        long SetNotificationPositions(unsigned long ,struct _DSBPOSITIONNOTIFY * );// Offset=0x0 Size=0x65
        void * __vecDelDtor(unsigned int );
    };
};

class DirectSound::CDirectSoundBuffer : public IDirectSoundBuffer, public DirectSound::CDirectSoundVoice// Size=0x24 (Id=2634)
{
    union // Size=0xe8 (Id=0)
    {
        unsigned char __align0[28];// Offset=0x0 Size=0x1c
        class DirectSound::CDirectSoundBufferSettings * m_pSettings;// Offset=0x1c Size=0x4
        class DirectSound::CMcpxBuffer * m_pBuffer;// Offset=0x20 Size=0x4
        void CDirectSoundBuffer(class DirectSound::CDirectSoundBuffer & );
        void CDirectSoundBuffer(class DirectSound::CDirectSound * );// Offset=0x0 Size=0x18
        void ~CDirectSoundBuffer();// Offset=0x0 Size=0x33
        long Initialize(struct _DSBUFFERDESC * );// Offset=0x0 Size=0xe8
        unsigned long AddRef();// Offset=0x0 Size=0x47
        unsigned long Release();// Offset=0x0 Size=0x4a
        long SetFormat(struct tWAVEFORMATEX * );// Offset=0x0 Size=0x4e
        long Play(unsigned long ,unsigned long ,unsigned long );// Offset=0x0 Size=0x51
        long PlayEx(long long ,unsigned long );// Offset=0x0 Size=0x59
        long Stop();// Offset=0x0 Size=0x4f
        long StopEx(long long ,unsigned long );// Offset=0x0 Size=0x59
        long SetPlayRegion(unsigned long ,unsigned long );// Offset=0x0 Size=0x80
        long SetLoopRegion(unsigned long ,unsigned long );// Offset=0x0 Size=0x85
        long GetStatus(unsigned long * );// Offset=0x0 Size=0x51
        long GetCurrentPosition(unsigned long * ,unsigned long * );// Offset=0x0 Size=0x55
        long SetCurrentPosition(unsigned long );// Offset=0x0 Size=0x51
        long SetBufferData(void * ,unsigned long );// Offset=0x0 Size=0xae
        long Lock(unsigned long ,unsigned long ,void ** ,unsigned long * ,void ** ,unsigned long * ,unsigned long );// Offset=0x0 Size=0xd6
        long SetFrequency(unsigned long );// Offset=0x0 Size=0x4e
        long SetVolume(long );// Offset=0x0 Size=0x4e
        long SetPitch(long );// Offset=0x0 Size=0x4e
        long SetLFO(struct _DSLFODESC * );// Offset=0x0 Size=0x4e
        long SetEG(struct _DSENVELOPEDESC * );// Offset=0x0 Size=0x4e
        long SetFilter(struct _DSFILTERDESC * );// Offset=0x0 Size=0x4e
        long SetHeadroom(unsigned long );// Offset=0x0 Size=0x4e
        long SetOutputBuffer(struct IDirectSoundBuffer * );// Offset=0x0 Size=0x4e
        long SetMixBins(struct _DSMIXBINS * );// Offset=0x0 Size=0x4e
        long SetMixBinVolumes(struct _DSMIXBINS * );// Offset=0x0 Size=0x4e
        long SetAllParameters(struct _DS3DBUFFER * ,unsigned long );// Offset=0x0 Size=0x52
        long SetConeAngles(unsigned long ,unsigned long ,unsigned long );// Offset=0x0 Size=0x56
        long SetConeOrientation(float ,float ,float ,unsigned long );// Offset=0x0 Size=0x67
        long SetConeOutsideVolume(long ,unsigned long );// Offset=0x0 Size=0x52
        long SetMaxDistance(float ,unsigned long );// Offset=0x0 Size=0x56
        long SetMinDistance(float ,unsigned long );// Offset=0x0 Size=0x56
        long SetMode(unsigned long ,unsigned long );// Offset=0x0 Size=0x52
        long SetPosition(float ,float ,float ,unsigned long );// Offset=0x0 Size=0x67
        long SetVelocity(float ,float ,float ,unsigned long );// Offset=0x0 Size=0x67
        long SetDistanceFactor(float ,unsigned long );// Offset=0x0 Size=0x56
        long SetDopplerFactor(float ,unsigned long );// Offset=0x0 Size=0x56
        long SetRolloffFactor(float ,unsigned long );// Offset=0x0 Size=0x56
        long SetI3DL2Source(struct _DSI3DL2BUFFER * ,unsigned long );// Offset=0x0 Size=0x52
        long SetNotificationPositions(unsigned long ,struct _DSBPOSITIONNOTIFY * );// Offset=0x0 Size=0x65
        void * __vecDelDtor(unsigned int );
    };
};

class DirectSound::CAutoFpState : private DirectSound::CFpState// Size=0x1 (Id=2635)
{
    union // Size=0xc (Id=0)
    {
        void CAutoFpState();// Offset=0x0 Size=0xc
        void ~CAutoFpState();// Offset=0x0 Size=0x5
        void * __vecDelDtor(unsigned int );
    };
};

class DirectSound::CFpState// Size=0x1 (Id=2636)
{
    union // Size=0x25 (Id=0)
    {
        unsigned long m_dwRefCount;// Offset=0x0 Size=0x4
        struct _KFLOATING_SAVE m_fps;// Offset=0x0 Size=0x20
        void Save();// Offset=0x0 Size=0x25
        void Restore();// Offset=0x0 Size=0x1e
    };
};

class DirectSound::CDirectSoundSettings : public CRefCount// Size=0xa8 (Id=2637)
{
    union // Size=0xa8 (Id=0)
    {
        unsigned char __align0[8];// Offset=0x0 Size=0x8
        unsigned long m_dwSpeakerConfig;// Offset=0x8 Size=0x4
        struct _DSEFFECTIMAGELOC m_EffectLocations;// Offset=0xc Size=0x8
        unsigned char m_abMixBinHeadroom[32];// Offset=0x14 Size=0x20
        struct DS3DLISTENERPARAMS m_3dParams;// Offset=0x34 Size=0x74
        void CDirectSoundSettings(class DirectSound::CDirectSoundSettings & );
        void CDirectSoundSettings();// Offset=0x0 Size=0x70
        void ~CDirectSoundSettings();// Offset=0x0 Size=0x7
        void SetEffectImageLocations(struct _DSEFFECTIMAGELOC * );// Offset=0x0 Size=0x22
        void __local_vftable_ctor_closure();
        void * __vecDelDtor(unsigned int );
    };
};

class DirectSound::CDirectSoundSettings : public CRefCount// Size=0xa8 (Id=2638)
{
    union // Size=0xa8 (Id=0)
    {
        unsigned char __align0[8];// Offset=0x0 Size=0x8
        unsigned long m_dwSpeakerConfig;// Offset=0x8 Size=0x4
        struct _DSEFFECTIMAGELOC m_EffectLocations;// Offset=0xc Size=0x8
        unsigned char m_abMixBinHeadroom[32];// Offset=0x14 Size=0x20
        struct DS3DLISTENERPARAMS m_3dParams;// Offset=0x34 Size=0x74
        void CDirectSoundSettings(class DirectSound::CDirectSoundSettings & );
        void CDirectSoundSettings();// Offset=0x0 Size=0x70
        void ~CDirectSoundSettings();// Offset=0x0 Size=0x7
        void SetEffectImageLocations(struct _DSEFFECTIMAGELOC * );// Offset=0x0 Size=0x22
        void __local_vftable_ctor_closure();
        void * __vecDelDtor(unsigned int );
    };
};

enum DirectSound::AC97CHANNELTYPE
{
    AC97_CHANNELTYPE_ANALOG=0,
    AC97_CHANNELTYPE_DIGITAL=1,
    AC97_CHANNELTYPE_COUNT=2
};

class DirectSound::CAc97Channel// Size=0x30 (Id=2640)
{
    union // Size=0xa2 (Id=0)
    {
        unsigned long m_adwRegisterOffsets[2];// Offset=0x0 Size=0x8
        const unsigned long m_dwSilenceSize;// Offset=0x0 Size=0x4
        const enum DirectSound::AC97CHANNELTYPE m_nChannelType;// Offset=0x0 Size=0x4
        unsigned long m_dwPosition;// Offset=0x4 Size=0x4
        class DirectSound::CAc97Device * m_pDevice;// Offset=0x8 Size=0x4
        unsigned long m_dwFlags;// Offset=0xc Size=0x4
        void  ( * m_pfnCallback)(void * );// Offset=0x10 Size=0x4
        void * m_pvCallbackContext;// Offset=0x14 Size=0x4
        struct ACIPRD * m_pPrdl;// Offset=0x18 Size=0x4
        unsigned long m_dwPrdlAddress;// Offset=0x1c Size=0x4
        void * m_pvSilence;// Offset=0x20 Size=0x4
        unsigned char m_bPrdCount;// Offset=0x24 Size=0x1
        unsigned char m_bCurrentPrd;// Offset=0x25 Size=0x1
        unsigned char __align0[2];// Offset=0x26 Size=0x2
        unsigned long m_dwMode;// Offset=0x28 Size=0x4
        unsigned long m_dwLastPosition;// Offset=0x2c Size=0x4
        void CAc97Channel(class DirectSound::CAc97Device * ,enum DirectSound::AC97CHANNELTYPE );// Offset=0x0 Size=0x19
        void ~CAc97Channel();// Offset=0x0 Size=0x5
        long Initialize(unsigned long ,void  ( * )(void * ),void * );// Offset=0x0 Size=0xa2
        void Terminate();// Offset=0x0 Size=0x4e
        void Release();// Offset=0x0 Size=0xb
        unsigned long GetFreePacketCount();
        void AttachPacket(unsigned long ,unsigned long );// Offset=0x0 Size=0x91
        void AttachPacket(void * ,unsigned long );// Offset=0x0 Size=0x1d
        void AttachBuffer(unsigned long ,unsigned long ,unsigned long );// Offset=0x0 Size=0x46
        void AttachBuffer(void * ,unsigned long ,unsigned long );
        void Run(unsigned long );// Offset=0x0 Size=0x5a
        void Pause();// Offset=0x0 Size=0x2f
        void Flush(int );// Offset=0x0 Size=0x4e
        void Discontinuity();// Offset=0x0 Size=0x38
        void Reset();// Offset=0x0 Size=0x72
        unsigned long GetPosition();// Offset=0x0 Size=0x8c
        void SetMode(unsigned long );// Offset=0x0 Size=0x31
        void SetPosition(unsigned long );// Offset=0x0 Size=0x2e
        void ServiceInterrupt();// Offset=0x0 Size=0x57
        void SetPrdIndeces(unsigned char ,unsigned char ,int ,int );// Offset=0x0 Size=0x90
        unsigned char PeekAciRegister8(unsigned long );// Offset=0x0 Size=0x17
        void PokeAciRegister8(unsigned long ,unsigned char );// Offset=0x0 Size=0x1b
        unsigned short PeekAciRegister16(unsigned long );// Offset=0x0 Size=0x18
        void PokeAciRegister16(unsigned long ,unsigned short );// Offset=0x0 Size=0x1d
        unsigned long PeekAciRegister32(unsigned long );
        void PokeAciRegister32(unsigned long ,unsigned long );// Offset=0x0 Size=0x1b
        void * __vecDelDtor(unsigned int );
    };
};

struct DirectSound::_MCPX_HW_NOTIFICATION// Size=0x10 (Id=2641)
{
    unsigned long GSCNT;// Offset=0x0 Size=0x4
    unsigned long CurrentOffset;// Offset=0x4 Size=0x4
    unsigned long Zero;// Offset=0x8 Size=0x4
    unsigned char Res0;// Offset=0xc Size=0x1
    unsigned char SamplesAvailable;// Offset=0xd Size=0x1
    unsigned char EnvelopeActive;// Offset=0xe Size=0x1
    unsigned char Status;// Offset=0xf Size=0x1
};

class DirectSound::CMcpxCore// Size=0x50 (Id=2642)
{
    union // Size=0x183 (Id=0)
    {
        unsigned long m_adwGPOutputBufferSizes[1];// Offset=0x0 Size=0x4
        unsigned long m_adwEPOutputBufferSizes[2];// Offset=0x0 Size=0x8
        struct DirectSound::MCPX_ALLOC_CONTEXT m_ctxMemory[16];// Offset=0x0 Size=0x100
        class DirectSound::CDirectSoundSettings * m_pSettings;// Offset=0x4 Size=0x4
        class DirectSound::CMcpxSlopMemoryHeap * m_pSlopMemoryHeap;// Offset=0x8 Size=0x4
        class DirectSound::CMcpxGPDspManager * m_pGpDspManager;// Offset=0xc Size=0x4
        class DirectSound::CMcpxEPDspManager * m_pEpDspManager;// Offset=0x10 Size=0x4
        class DirectSound::CAc97Device m_Ac97;// Offset=0x14 Size=0x3c
        void CMcpxCore(class DirectSound::CMcpxCore & );
        void CMcpxCore(class DirectSound::CDirectSoundSettings * );// Offset=0x0 Size=0x23
        void ~CMcpxCore();// Offset=0x0 Size=0xa5
        long Initialize();// Offset=0x0 Size=0x5a
        int IdleVoiceProcessor(int );// Offset=0x0 Size=0xe1
        unsigned long GetPhysicalMemoryProperties(void * ,unsigned long ,unsigned long * ,unsigned long * );// Offset=0x0 Size=0x77
        unsigned long MapTransfer(void ** ,unsigned long * ,unsigned long * );// Offset=0x0 Size=0x3d
        long AllocateContext(struct DirectSound::MCPX_ALLOC_CONTEXT * ,unsigned long ,unsigned long );// Offset=0x0 Size=0x3f
        void Reset();// Offset=0x0 Size=0x80
        void SetInterruptState(int );// Offset=0x0 Size=0x37
        void SetPrivLockState(int );// Offset=0x0 Size=0x37
        void SetFrontEndState(enum DirectSound::MCPX_FE_STATE );// Offset=0x0 Size=0x5b
        void SetSetupEngineState(enum DirectSound::MCPX_SE_STATE );// Offset=0x0 Size=0x62
        void ResetGlobalCounters();// Offset=0x0 Size=0x7a
        long AllocateApuMemory();// Offset=0x0 Size=0x180
        void SetupFrontEndProcessor();// Offset=0x0 Size=0xcb
        void SetupVoiceProcessor();// Offset=0x0 Size=0xd1
        void SetupDSPs();// Offset=0x0 Size=0x1c
        void SetupGlobalProcessor();// Offset=0x0 Size=0x129
        void SetupEncodeProcessor();// Offset=0x0 Size=0x183
        long SetupAc97();// Offset=0x0 Size=0xb9
        void * __vecDelDtor(unsigned int );
    };
};

class DirectSound::CMcpxCore// Size=0x50 (Id=2643)
{
    union // Size=0x183 (Id=0)
    {
        unsigned long m_adwGPOutputBufferSizes[1];// Offset=0x0 Size=0x4
        unsigned long m_adwEPOutputBufferSizes[2];// Offset=0x0 Size=0x8
        struct DirectSound::MCPX_ALLOC_CONTEXT m_ctxMemory[16];// Offset=0x0 Size=0x100
        class DirectSound::CDirectSoundSettings * m_pSettings;// Offset=0x4 Size=0x4
        class DirectSound::CMcpxSlopMemoryHeap * m_pSlopMemoryHeap;// Offset=0x8 Size=0x4
        class DirectSound::CMcpxGPDspManager * m_pGpDspManager;// Offset=0xc Size=0x4
        class DirectSound::CMcpxEPDspManager * m_pEpDspManager;// Offset=0x10 Size=0x4
        class DirectSound::CAc97Device m_Ac97;// Offset=0x14 Size=0x3c
        void CMcpxCore(class DirectSound::CMcpxCore & );
        void CMcpxCore(class DirectSound::CDirectSoundSettings * );// Offset=0x0 Size=0x23
        void ~CMcpxCore();// Offset=0x0 Size=0xa5
        long Initialize();// Offset=0x0 Size=0x5a
        int IdleVoiceProcessor(int );// Offset=0x0 Size=0xe1
        unsigned long GetPhysicalMemoryProperties(void * ,unsigned long ,unsigned long * ,unsigned long * );// Offset=0x0 Size=0x77
        unsigned long MapTransfer(void ** ,unsigned long * ,unsigned long * );// Offset=0x0 Size=0x3d
        long AllocateContext(struct DirectSound::MCPX_ALLOC_CONTEXT * ,unsigned long ,unsigned long );// Offset=0x0 Size=0x3f
        void Reset();// Offset=0x0 Size=0x80
        void SetInterruptState(int );// Offset=0x0 Size=0x37
        void SetPrivLockState(int );// Offset=0x0 Size=0x37
        void SetFrontEndState(enum DirectSound::MCPX_FE_STATE );// Offset=0x0 Size=0x5b
        void SetSetupEngineState(enum DirectSound::MCPX_SE_STATE );// Offset=0x0 Size=0x62
        void ResetGlobalCounters();// Offset=0x0 Size=0x7a
        long AllocateApuMemory();// Offset=0x0 Size=0x180
        void SetupFrontEndProcessor();// Offset=0x0 Size=0xcb
        void SetupVoiceProcessor();// Offset=0x0 Size=0xd1
        void SetupDSPs();// Offset=0x0 Size=0x1c
        void SetupGlobalProcessor();// Offset=0x0 Size=0x129
        void SetupEncodeProcessor();// Offset=0x0 Size=0x183
        long SetupAc97();// Offset=0x0 Size=0xb9
        void * __vecDelDtor(unsigned int );
    };
};

struct DirectSound::DOLBY_CONFIG_TABLE// Size=0x60 (Id=2644)
{
    unsigned long table_size;// Offset=0x0 Size=0x4
    unsigned long do_surround_encode;// Offset=0x4 Size=0x4
    unsigned long do_game_encode;// Offset=0x8 Size=0x4
    unsigned long do_downmix_encode;// Offset=0xc Size=0x4
    unsigned long pcm_sample_rate_code;// Offset=0x10 Size=0x4
    unsigned long input_ch_config;// Offset=0x14 Size=0x4
    unsigned long lfe_present;// Offset=0x18 Size=0x4
    unsigned long ac3_dialnorm;// Offset=0x1c Size=0x4
    unsigned long ac3_bandwidth;// Offset=0x20 Size=0x4
    unsigned long channel_lpf_enabled;// Offset=0x24 Size=0x4
    unsigned long lfe_lpf_enabled;// Offset=0x28 Size=0x4
    unsigned long dc_hpf_enabled;// Offset=0x2c Size=0x4
    unsigned long dynrng_exists;// Offset=0x30 Size=0x4
    unsigned long ac3_dynrng_code;// Offset=0x34 Size=0x4
    unsigned long compr_exists;// Offset=0x38 Size=0x4
    unsigned long ac3_compr_code;// Offset=0x3c Size=0x4
    unsigned long surround_gain_enabled;// Offset=0x40 Size=0x4
    unsigned long surround_gain;// Offset=0x44 Size=0x4
    unsigned long surround_mode;// Offset=0x48 Size=0x4
    unsigned long reserved_1;// Offset=0x4c Size=0x4
    unsigned long reserved_2;// Offset=0x50 Size=0x4
    unsigned long reserved_3;// Offset=0x54 Size=0x4
    unsigned long reserved_4;// Offset=0x58 Size=0x4
    unsigned long reserved_5;// Offset=0x5c Size=0x4
};

enum DirectSound::MCPX_SE_STATE
{
    MCPX_SE_STATE_OFF=0,
    MCPX_SE_STATE_AC_SYNC=1,
    MCPX_SE_STATE_SW=2,
    MCPX_SE_STATE_FREE_RUNNING=3,
    MCPX_SE_STATE_ISO=4,
    MCPX_SE_STATE_NON_ISO=5
};

class DirectSound::CAutoLock// Size=0x4 (Id=2646)
{
    union // Size=0x11 (Id=0)
    {
        int m_fLocked;// Offset=0x0 Size=0x4
        void CAutoLock();// Offset=0x0 Size=0x11
        void ~CAutoLock();// Offset=0x0 Size=0x11
    };
    public void * __vecDelDtor(unsigned int );
};

class DirectSound::CMcpxGPDspManager// Size=0x3c (Id=2647)
{
    union // Size=0x175 (Id=0)
    {
        void CMcpxGPDspManager();// Offset=0x0 Size=0x1f
        void ~CMcpxGPDspManager();// Offset=0x0 Size=0x41
        void Initialize();// Offset=0x0 Size=0x75
        void * GetScratchPage(unsigned long );
        void AC3SetOutputBuffer(struct DirectSound::MCPX_ALLOC_CONTEXT * ,unsigned long );// Offset=0x0 Size=0x14
        void SetMultipassBuffer(struct DirectSound::MCPX_ALLOC_CONTEXT * ,unsigned long );// Offset=0x0 Size=0x17
        long DownloadEffectsImage(void * ,unsigned long ,struct _DSEFFECTIMAGEDESC ** );// Offset=0x0 Size=0x99
        long SetEffectData(unsigned long ,unsigned long ,void * ,unsigned long ,unsigned long );// Offset=0x0 Size=0xa9
        long GetEffectData(unsigned long ,unsigned long ,void * ,unsigned long );// Offset=0x0 Size=0x3a
        long GetEffectMap(unsigned long ,struct _DSEFFECTMAP ** );// Offset=0x0 Size=0x27
        void CommitChanges(unsigned long ,unsigned long );// Offset=0x0 Size=0x65
        void RestoreCommandBlock();// Offset=0x0 Size=0x22
        class DirectSound::CMcpxDspScratchDma * GetScratchDma();
        long ParseEffectImageInfo(void * );// Offset=0x0 Size=0x175
        unsigned long m_uAC3BufferOffset;// Offset=0x0 Size=0x4
        unsigned long m_uMultipassBufferOffset;// Offset=0x4 Size=0x4
        class DirectSound::CMcpxDspScratchDma * m_pScratchDma;// Offset=0x8 Size=0x4
        class DirectSound::CMcpxDspImage * m_pDspImage;// Offset=0xc Size=0x4
        unsigned long m_uPMemOffset;// Offset=0x10 Size=0x4
        unsigned long m_uPMemMaxSize;// Offset=0x14 Size=0x4
        unsigned long m_dwCurrentLowestScratchOffset;// Offset=0x18 Size=0x4
        unsigned long m_dwStateSizeToCommit;// Offset=0x1c Size=0x4
        struct _DSEFFECTIMAGEDESC * m_pFxDescriptor;// Offset=0x20 Size=0x4
        struct HOST_TO_DSP_COMMANDBLOCK m_InitialCmdBlock;// Offset=0x24 Size=0x18
        void * __vecDelDtor(unsigned int );
    };
};

union DirectSound::R_INTR// Size=0x8 (Id=2648)
{
    union // Size=0x4 (Id=0)
    {
        struct // Size=0x4 (Id=0)
        {
            unsigned long General:1;// Offset=0x0 Size=0x4 BitOffset=0x0 BitSize=0x1
            unsigned long DeltaWarning:1;// Offset=0x0 Size=0x4 BitOffset=0x1 BitSize=0x1
            unsigned long RetriggerEvent:1;// Offset=0x0 Size=0x4 BitOffset=0x2 BitSize=0x1
            unsigned long DeltaPanic:1;// Offset=0x0 Size=0x4 BitOffset=0x3 BitSize=0x1
            unsigned long FETrap:1;// Offset=0x0 Size=0x4 BitOffset=0x4 BitSize=0x1
            unsigned long FENotify:1;// Offset=0x0 Size=0x4 BitOffset=0x5 BitSize=0x1
            unsigned long FEVoice:1;// Offset=0x0 Size=0x4 BitOffset=0x6 BitSize=0x1
            unsigned long FEMethodOverFlow:1;// Offset=0x0 Size=0x4 BitOffset=0x7 BitSize=0x1
            unsigned long GPMailbox:1;// Offset=0x0 Size=0x4 BitOffset=0x8 BitSize=0x1
            unsigned long GPNotify:1;// Offset=0x0 Size=0x4 BitOffset=0x9 BitSize=0x1
            unsigned long EPMailbox:1;// Offset=0x0 Size=0x4 BitOffset=0xa BitSize=0x1
            unsigned long EPNotify:1;// Offset=0x0 Size=0x4 BitOffset=0xb BitSize=0x1
        };
        unsigned long uValue;// Offset=0x0 Size=0x4
    };
};

class DirectSound::CStdFileStream// Size=0x8 (Id=2649)
{
    union // Size=0x3e (Id=0)
    {
        void * m_hFile;// Offset=0x0 Size=0x4
        unsigned long m_dwFlags;// Offset=0x4 Size=0x4
        void CStdFileStream();// Offset=0x0 Size=0xa
        void ~CStdFileStream();// Offset=0x0 Size=0x5
        long Open(char * ,unsigned long ,unsigned long ,unsigned long ,unsigned long );// Offset=0x0 Size=0x39
        void Attach(void * );// Offset=0x0 Size=0xd
        void Close();// Offset=0x0 Size=0x2b
        long Read(void * ,unsigned long ,unsigned long * );// Offset=0x0 Size=0x3e
        long Write(void * ,unsigned long ,unsigned long * );// Offset=0x0 Size=0x3e
        long Seek(long ,unsigned long ,unsigned long * );// Offset=0x0 Size=0x2f
        long GetLength(unsigned long * );// Offset=0x0 Size=0x21
        void * operator void *();
        void * __vecDelDtor(unsigned int );
    };
};

struct DirectSound::PAN3DSPEAKER// Size=0x14 (Id=2650)
{
    union // Size=0xc (Id=0)
    {
        struct _D3DVECTOR vSpeakerPos;// Offset=0x0 Size=0xc
        struct _VECTOR4 v4SpeakerPos;// Offset=0x0 Size=0x10
    };
    unsigned long dwMixBin;// Offset=0x10 Size=0x4
};

class DirectSound::CHrtfListener// Size=0x7c (Id=2651)
{
    union // Size=0x79 (Id=0)
    {
        const struct _D3DVECTOR m_vDefaultNormOrient;// Offset=0x0 Size=0xc
        const struct _D3DVECTOR m_vDefaultNormFrontOrient;// Offset=0x0 Size=0xc
        struct DirectSound::PAN3DSPEAKER m_aDefaultSpeakers[5];// Offset=0x0 Size=0x64
        struct _D3DMATRIX * m_pTransformMatrix;// Offset=0x0 Size=0x4
        struct _DS3DLISTENER & m_3dParams;// Offset=0x4 Size=0x4
        struct DirectSound::HRTFLISTENER m_3dData;// Offset=0x8 Size=0x70
        unsigned char m_fSurround;// Offset=0x78 Size=0x1
        void CHrtfListener(struct _DS3DLISTENER & );// Offset=0x0 Size=0x3b
        void ~CHrtfListener();// Offset=0x0 Size=0x14
        void Calculate3d(unsigned long );// Offset=0x0 Size=0xdd
    };
    public void * __vecDelDtor(unsigned int );
};

struct DirectSound::SLOPRUNMARKER// Size=0x10 (Id=2652)
{
    struct _LIST_ENTRY leListEntry;// Offset=0x0 Size=0x8
    unsigned long nLength;// Offset=0x8 Size=0x4
    union // Size=0x4 (Id=0)
    {
        unsigned long dwSignature;// Offset=0xc Size=0x4
        int fAllocated;// Offset=0xc Size=0x4
    };
};

struct DirectSound::HRTFVOICE// Size=0xa8 (Id=2653)
{
    unsigned long dwChangeMask;// Offset=0x0 Size=0x4
    unsigned long dwMixBinValidMask;// Offset=0x4 Size=0x4
    unsigned long dwMixBinChangeMask;// Offset=0x8 Size=0x4
    long lDistanceVolume;// Offset=0xc Size=0x4
    long lConeVolume;// Offset=0x10 Size=0x4
    long lFrontVolume;// Offset=0x14 Size=0x4
    long lRearVolume;// Offset=0x18 Size=0x4
    long lDopplerPitch;// Offset=0x1c Size=0x4
    struct DirectSound::HRTFFILTERPAIR FilterPair;// Offset=0x20 Size=0x8
    long alMixBinVolumes[32];// Offset=0x28 Size=0x80
};

class DirectSound::CDirectSoundPerformanceMonitor// Size=0x1 (Id=2654)
{
    union // Size=0x1 (Id=0)
    {
        void RegisterCounters();// Offset=0x0 Size=0x1
        void UnregisterCounters();// Offset=0x0 Size=0x1
    };
};

class DirectSound::CImaAdpcmCodec// Size=0x28 (Id=2655)
{
    union // Size=0x157 (Id=0)
    {
        unsigned char __align0[4];// Offset=0x0 Size=0x4
        struct xbox_adpcmwaveformat_tag m_wfxEncode;// Offset=0x4 Size=0x14
        int m_fEncoder;// Offset=0x18 Size=0x4
        short m_asNextStep[16];// Offset=0x0 Size=0x20
        short m_asStep[89];// Offset=0x0 Size=0xb2
        int m_nStepIndexL;// Offset=0x1c Size=0x4
        int m_nStepIndexR;// Offset=0x20 Size=0x4
        int  ( * m_pfnConvert)(unsigned char * ,unsigned char * ,unsigned int ,unsigned int ,unsigned int ,int * ,int * );// Offset=0x24 Size=0x4
        void CImaAdpcmCodec(class DirectSound::CImaAdpcmCodec & );
        void CImaAdpcmCodec();// Offset=0x0 Size=0x9
        void ~CImaAdpcmCodec();// Offset=0x0 Size=0x7
        int Initialize(struct xbox_adpcmwaveformat_tag * ,int );// Offset=0x0 Size=0x49
        unsigned short GetEncodeAlignment();// Offset=0x0 Size=0x5
        unsigned short GetDecodeAlignment();// Offset=0x0 Size=0x15
        unsigned short GetSourceAlignment();
        unsigned short GetDestinationAlignment();
        int Convert(void * ,void * ,unsigned int );// Offset=0x0 Size=0x24
        void Reset();// Offset=0x0 Size=0x9
        void CreatePcmFormat(unsigned short ,unsigned long ,struct tWAVEFORMATEX * );// Offset=0x0 Size=0x3f
        void CreateImaAdpcmFormat(unsigned short ,unsigned long ,unsigned short ,struct xbox_adpcmwaveformat_tag * );// Offset=0x0 Size=0x4e
        int IsValidPcmFormat(struct tWAVEFORMATEX * );// Offset=0x0 Size=0x4e
        int IsValidImaAdpcmFormat(struct xbox_adpcmwaveformat_tag * );// Offset=0x0 Size=0x46
        unsigned short CalculateEncodeAlignment(unsigned short ,unsigned short );// Offset=0x0 Size=0x2a
        int EncodeM16(unsigned char * ,unsigned char * ,unsigned int ,unsigned int ,unsigned int ,int * ,int * );// Offset=0x0 Size=0xd7
        int EncodeS16(unsigned char * ,unsigned char * ,unsigned int ,unsigned int ,unsigned int ,int * ,int * );// Offset=0x0 Size=0x146
        int DecodeM16(unsigned char * ,unsigned char * ,unsigned int ,unsigned int ,unsigned int ,int * ,int * );// Offset=0x0 Size=0xf8
        int DecodeS16(unsigned char * ,unsigned char * ,unsigned int ,unsigned int ,unsigned int ,int * ,int * );// Offset=0x0 Size=0x157
        int EncodeSample(int ,int * ,int );// Offset=0x0 Size=0x67
        int DecodeSample(int ,int ,int );// Offset=0x0 Size=0x5b
        int NextStepIndex(int ,int );// Offset=0x0 Size=0x23
        int ValidStepIndex(int );// Offset=0x0 Size=0x13
        void __local_vftable_ctor_closure();
        void * __vecDelDtor(unsigned int );
    };
};

class DirectSound::CImaAdpcmCodec// Size=0x28 (Id=2656)
{
    union // Size=0x157 (Id=0)
    {
        unsigned char __align0[4];// Offset=0x0 Size=0x4
        struct xbox_adpcmwaveformat_tag m_wfxEncode;// Offset=0x4 Size=0x14
        int m_fEncoder;// Offset=0x18 Size=0x4
        short m_asNextStep[16];// Offset=0x0 Size=0x20
        short m_asStep[89];// Offset=0x0 Size=0xb2
        int m_nStepIndexL;// Offset=0x1c Size=0x4
        int m_nStepIndexR;// Offset=0x20 Size=0x4
        int  ( * m_pfnConvert)(unsigned char * ,unsigned char * ,unsigned int ,unsigned int ,unsigned int ,int * ,int * );// Offset=0x24 Size=0x4
        void CImaAdpcmCodec(class DirectSound::CImaAdpcmCodec & );
        void CImaAdpcmCodec();// Offset=0x0 Size=0x9
        void ~CImaAdpcmCodec();// Offset=0x0 Size=0x7
        int Initialize(struct xbox_adpcmwaveformat_tag * ,int );// Offset=0x0 Size=0x49
        unsigned short GetEncodeAlignment();// Offset=0x0 Size=0x5
        unsigned short GetDecodeAlignment();// Offset=0x0 Size=0x15
        unsigned short GetSourceAlignment();
        unsigned short GetDestinationAlignment();
        int Convert(void * ,void * ,unsigned int );// Offset=0x0 Size=0x24
        void Reset();// Offset=0x0 Size=0x9
        void CreatePcmFormat(unsigned short ,unsigned long ,struct tWAVEFORMATEX * );// Offset=0x0 Size=0x3f
        void CreateImaAdpcmFormat(unsigned short ,unsigned long ,unsigned short ,struct xbox_adpcmwaveformat_tag * );// Offset=0x0 Size=0x4e
        int IsValidPcmFormat(struct tWAVEFORMATEX * );// Offset=0x0 Size=0x4e
        int IsValidImaAdpcmFormat(struct xbox_adpcmwaveformat_tag * );// Offset=0x0 Size=0x46
        unsigned short CalculateEncodeAlignment(unsigned short ,unsigned short );// Offset=0x0 Size=0x2a
        int EncodeM16(unsigned char * ,unsigned char * ,unsigned int ,unsigned int ,unsigned int ,int * ,int * );// Offset=0x0 Size=0xd7
        int EncodeS16(unsigned char * ,unsigned char * ,unsigned int ,unsigned int ,unsigned int ,int * ,int * );// Offset=0x0 Size=0x146
        int DecodeM16(unsigned char * ,unsigned char * ,unsigned int ,unsigned int ,unsigned int ,int * ,int * );// Offset=0x0 Size=0xf8
        int DecodeS16(unsigned char * ,unsigned char * ,unsigned int ,unsigned int ,unsigned int ,int * ,int * );// Offset=0x0 Size=0x157
        int EncodeSample(int ,int * ,int );// Offset=0x0 Size=0x67
        int DecodeSample(int ,int ,int );// Offset=0x0 Size=0x5b
        int NextStepIndex(int ,int );// Offset=0x0 Size=0x23
        int ValidStepIndex(int );// Offset=0x0 Size=0x13
        void __local_vftable_ctor_closure();
        void * __vecDelDtor(unsigned int );
    };
};

class DirectSound::CMcpxDspScratchDma// Size=0x28 (Id=2657)
{
    union // Size=0xd8 (Id=0)
    {
        void CMcpxDspScratchDma(int );// Offset=0x0 Size=0x1a
        void ~CMcpxDspScratchDma();// Offset=0x0 Size=0x2e
        void Initialize(unsigned long );// Offset=0x0 Size=0xb3
        void Copy(unsigned long ,void * ,unsigned long );// Offset=0x0 Size=0x28
        void Put(unsigned long ,unsigned long );// Offset=0x0 Size=0x16
        void AddPages(struct DirectSound::MCPX_ALLOC_CONTEXT * ,unsigned long ,unsigned long * );// Offset=0x0 Size=0x52
        void * GetScratchTableLinAddr();
        void * GetScratchSpaceLinAddr();// Offset=0x0 Size=0x6
        void * GetFxScratchSpaceLinAddr();// Offset=0x0 Size=0x4
        unsigned long GetScratchSpacePhysAddr();// Offset=0x0 Size=0x7
        unsigned long GetScratchSpaceSize();
        long AdjustFxScratch(unsigned long );// Offset=0x0 Size=0xd8
        unsigned long m_dwMaxPages;// Offset=0x0 Size=0x4
        unsigned long m_dwReservedPages;// Offset=0x4 Size=0x4
        int m_fGpScratch;// Offset=0x8 Size=0x4
        struct DirectSound::MCPX_ALLOC_CONTEXT * m_pSgeTableContext;// Offset=0xc Size=0x4
        struct DirectSound::MCPX_ALLOC_CONTEXT * m_pDmaBufferContext;// Offset=0x10 Size=0x4
        struct DirectSound::MCPX_ALLOC_CONTEXT m_ctxFxScratch;// Offset=0x14 Size=0x10
        unsigned long m_uRegOffsetValidPages;// Offset=0x24 Size=0x4
        void * __vecDelDtor(unsigned int );
    };
};

enum DirectSound::CPan3dSource::__unnamed
{
    DS3DALG=2
};

class DirectSound::CPan3dSource// Size=0x1 (Id=2659)
{
    enum __unnamed
    {
        DS3DALG=2
    };
    union // Size=0x3b (Id=0)
    {
        void Calculate3d(unsigned long ,class DirectSound::CHrtfSource * );// Offset=0x0 Size=0x3b
        void GetHrtfFilterPair(class DirectSound::CHrtfSource * );// Offset=0x0 Size=0x3
        void CalcPan(class DirectSound::CHrtfSource * );// Offset=0x0 Size=0xfd
    };
};

class DirectSound::CMcpxNotifier// Size=0x8 (Id=2660)
{
    union // Size=0x20 (Id=0)
    {
        struct DirectSound::_MCPX_HW_NOTIFICATION * m_paNotifier;// Offset=0x0 Size=0x4
        unsigned long m_dwNotifierCount;// Offset=0x4 Size=0x4
        void CMcpxNotifier();// Offset=0x0 Size=0xa
        void Initialize(unsigned long ,unsigned long );// Offset=0x0 Size=0x1e
        void Free();// Offset=0x0 Size=0x8
        int GetStatus(unsigned long );// Offset=0x0 Size=0x1e
        void SetStatus(unsigned long ,int );// Offset=0x0 Size=0x20
        void Reset();// Offset=0x0 Size=0x1c
    };
};

struct DirectSound::I3DL2SOURCE// Size=0x14 (Id=2661)
{
    unsigned long dwChangeMask;// Offset=0x0 Size=0x4
    long lDirect;// Offset=0x4 Size=0x4
    long lSource;// Offset=0x8 Size=0x4
    int nDirectIir;// Offset=0xc Size=0x4
    int nReverbIir;// Offset=0x10 Size=0x4
};

class DirectSound::CDirectSoundVoiceSettings : public CRefCount// Size=0xb8 (Id=2662)
{
    union // Size=0xba (Id=0)
    {
        unsigned char __align0[8];// Offset=0x0 Size=0x8
        unsigned long m_dwFlags;// Offset=0x8 Size=0x4
        struct DirectSound::DSWAVEFORMAT m_fmt;// Offset=0xc Size=0xc
        long m_lPitch;// Offset=0x18 Size=0x4
        long m_lVolume;// Offset=0x1c Size=0x4
        unsigned long m_dwHeadroom;// Offset=0x20 Size=0x4
        unsigned long m_dwMixBinCount;// Offset=0x24 Size=0x4
        unsigned char m_abMixBins[8];// Offset=0x28 Size=0x8
        long m_alMixBinVolumes[32];// Offset=0x30 Size=0x80
        class DirectSound::CDirectSoundBuffer * m_pMixinBuffer;// Offset=0xb0 Size=0x4
        struct DS3DSOURCEPARAMS * m_p3dParams;// Offset=0xb4 Size=0x4
        void CDirectSoundVoiceSettings(class DirectSound::CDirectSoundVoiceSettings & );
        void CDirectSoundVoiceSettings();// Offset=0x0 Size=0x10
        void ~CDirectSoundVoiceSettings();// Offset=0x0 Size=0x38
        long Initialize(unsigned long ,struct tWAVEFORMATEX * ,struct _DSMIXBINS * );// Offset=0x0 Size=0xba
        int SetFormat(struct tWAVEFORMATEX * ,int );// Offset=0x0 Size=0x7c
        void SetVolume(long );// Offset=0x0 Size=0xd
        void SetMixBinVolumes(struct _DSMIXBINS * );// Offset=0x0 Size=0x25
        void SetHeadroom(unsigned long );// Offset=0x0 Size=0x12
        void SetMixBins(struct _DSMIXBINS * );// Offset=0x0 Size=0x7a
        void SetOutputBuffer(class DirectSound::CDirectSoundBuffer * );// Offset=0x0 Size=0x47
        void __local_vftable_ctor_closure();
        void * __vecDelDtor(unsigned int );
    };
};

class DirectSound::CDirectSoundVoiceSettings : public CRefCount// Size=0xb8 (Id=2663)
{
    union // Size=0xba (Id=0)
    {
        unsigned char __align0[8];// Offset=0x0 Size=0x8
        unsigned long m_dwFlags;// Offset=0x8 Size=0x4
        struct DirectSound::DSWAVEFORMAT m_fmt;// Offset=0xc Size=0xc
        long m_lPitch;// Offset=0x18 Size=0x4
        long m_lVolume;// Offset=0x1c Size=0x4
        unsigned long m_dwHeadroom;// Offset=0x20 Size=0x4
        unsigned long m_dwMixBinCount;// Offset=0x24 Size=0x4
        unsigned char m_abMixBins[8];// Offset=0x28 Size=0x8
        long m_alMixBinVolumes[32];// Offset=0x30 Size=0x80
        class DirectSound::CDirectSoundBuffer * m_pMixinBuffer;// Offset=0xb0 Size=0x4
        struct DS3DSOURCEPARAMS * m_p3dParams;// Offset=0xb4 Size=0x4
        void CDirectSoundVoiceSettings(class DirectSound::CDirectSoundVoiceSettings & );
        void CDirectSoundVoiceSettings();// Offset=0x0 Size=0x10
        void ~CDirectSoundVoiceSettings();// Offset=0x0 Size=0x38
        long Initialize(unsigned long ,struct tWAVEFORMATEX * ,struct _DSMIXBINS * );// Offset=0x0 Size=0xba
        int SetFormat(struct tWAVEFORMATEX * ,int );// Offset=0x0 Size=0x7c
        void SetVolume(long );// Offset=0x0 Size=0xd
        void SetMixBinVolumes(struct _DSMIXBINS * );// Offset=0x0 Size=0x25
        void SetHeadroom(unsigned long );// Offset=0x0 Size=0x12
        void SetMixBins(struct _DSMIXBINS * );// Offset=0x0 Size=0x7a
        void SetOutputBuffer(class DirectSound::CDirectSoundBuffer * );// Offset=0x0 Size=0x47
        void __local_vftable_ctor_closure();
        void * __vecDelDtor(unsigned int );
    };
};

struct DirectSound::AC97PACKET// Size=0x20 (Id=2664)
{
    struct _LIST_ENTRY leListEntry;// Offset=0x0 Size=0x8
    struct _XMEDIAPACKET xmp;// Offset=0x8 Size=0x18
};

class DirectSound::CMcpxVoiceClient : public CRefCount// Size=0x90 (Id=2665)
{
    union // Size=0x1e7 (Id=0)
    {
        unsigned long m_dwStuckVoiceCount;// Offset=0x0 Size=0x4
        unsigned char __align0[4];// Offset=0x4 Size=0x4
        class DirectSound::CMcpxAPU * m_pMcpxApu;// Offset=0x8 Size=0x4
        unsigned short m_ahVoices[3];// Offset=0xc Size=0x6
        unsigned short m_dwStatus;// Offset=0x12 Size=0x2
        struct DirectSound::MCPX_VOICE_REGCACHE m_RegCache;// Offset=0x14 Size=0x30
        struct _LIST_ENTRY m_lstSourceVoices;// Offset=0x44 Size=0x8
        struct _LIST_ENTRY m_leActiveVoice;// Offset=0x4c Size=0x8
        struct _LIST_ENTRY m_lePendingInactiveVoice;// Offset=0x54 Size=0x8
        struct _LIST_ENTRY m_leSourceVoice;// Offset=0x5c Size=0x8
        unsigned char m_bVoiceCount;// Offset=0x64 Size=0x1
        unsigned char m_bVoiceList;// Offset=0x65 Size=0x1
        unsigned char m_bAvailable3dFilter;// Offset=0x66 Size=0x1
        unsigned char m_bAlign0;// Offset=0x67 Size=0x1
        class DirectSound::CMcpxVoiceNotifier m_Notifier;// Offset=0x68 Size=0x8
        class DirectSound::CHrtfSource * m_pHrtfSource;// Offset=0x70 Size=0x4
        class DirectSound::CI3dl2Source * m_pI3dl2Source;// Offset=0x74 Size=0x4
        class DirectSound::CDirectSoundVoiceSettings * m_pSettings;// Offset=0x78 Size=0x4
        unsigned long m_dw3dMode;// Offset=0x7c Size=0x4
        long long m_rtVoiceOff;// Offset=0x80 Size=0x8
        unsigned long m_dwIgnoredTraps;// Offset=0x88 Size=0x4
        void CMcpxVoiceClient(class DirectSound::CMcpxVoiceClient & );
        void CMcpxVoiceClient(class DirectSound::CMcpxAPU * ,class DirectSound::CDirectSoundVoiceSettings * );// Offset=0x0 Size=0x68
        void ~CMcpxVoiceClient();// Offset=0x0 Size=0x5a
        long Initialize(int );// Offset=0x0 Size=0xfe
        long SetFormat();// Offset=0x0 Size=0x113
        long SetMixBins();// Offset=0x0 Size=0xc1
        long SetVolume();// Offset=0x0 Size=0x98
        long SetPitch();// Offset=0x0 Size=0x92
        long SetLFO(struct _DSLFODESC * );// Offset=0x0 Size=0x12f
        long SetEG(struct _DSENVELOPEDESC * );// Offset=0x0 Size=0x192
        long SetFilter(struct _DSFILTERDESC * );// Offset=0x0 Size=0x14d
        long ConnectVoice();// Offset=0x0 Size=0x10d
        long DisconnectVoice();// Offset=0x0 Size=0xf6
        unsigned long Commit3dSettings();// Offset=0x0 Size=0xd8
        void ActivateVoice();// Offset=0x0 Size=0x1b4
        void DeactivateVoice(int );// Offset=0x0 Size=0x124
        void ReleaseVoice();// Offset=0x0 Size=0x84
        void PauseVoice(unsigned long );// Offset=0x0 Size=0xa7
        void WaitForVoiceOff();// Offset=0x0 Size=0x1c
        void CheckStuckVoice();// Offset=0x0 Size=0x5a
        void ConvertMixBinValues(unsigned long * ,unsigned long * );// Offset=0x0 Size=0xb1
        void ConvertVolumeValues(struct DirectSound::MCPX_VOICE_VOLUME * );// Offset=0x0 Size=0x1e7
        void ConvertPitchValue(unsigned long * );// Offset=0x0 Size=0x82
        unsigned long GetVoiceCfgFMT(unsigned long );// Offset=0x0 Size=0x12
        long AllocateVoiceResources();// Offset=0x0 Size=0x17c
        void ReleaseVoiceResources();// Offset=0x0 Size=0x88
        void Apply3dSettings(unsigned long ,unsigned long ,unsigned long );// Offset=0x0 Size=0x4d
        void LoadHrtfFilter();// Offset=0x0 Size=0x105
        unsigned long BytesToSamples(unsigned long );// Offset=0x0 Size=0x19
        unsigned long SamplesToBytes(unsigned long );// Offset=0x0 Size=0x21
        unsigned long GetSslPosition();// Offset=0x0 Size=0x8b
        class DirectSound::CMcpxBuffer * GetSubMixDestination();// Offset=0x0 Size=0x12
        int ServiceVoiceInterrupt();
        int ScheduleDeferredCommand(unsigned long ,long long ,unsigned long );
        void RemoveDeferredCommand(unsigned long );
        void ServiceDeferredCommand(unsigned long ,unsigned long );
        void GetAntecedentVoice(unsigned char * ,class DirectSound::CMcpxVoiceClient ** );// Offset=0x0 Size=0x63
        void RemoveIdleVoice(int );// Offset=0x0 Size=0x1e7
        void RemoveStuckVoice();// Offset=0x0 Size=0x57
        void * __vecDelDtor(unsigned int );
    };
};

class DirectSound::CMcpxVoiceClient : public CRefCount// Size=0x90 (Id=2666)
{
    union // Size=0x1e7 (Id=0)
    {
        unsigned long m_dwStuckVoiceCount;// Offset=0x0 Size=0x4
        unsigned char __align0[4];// Offset=0x4 Size=0x4
        class DirectSound::CMcpxAPU * m_pMcpxApu;// Offset=0x8 Size=0x4
        unsigned short m_ahVoices[3];// Offset=0xc Size=0x6
        unsigned short m_dwStatus;// Offset=0x12 Size=0x2
        struct DirectSound::MCPX_VOICE_REGCACHE m_RegCache;// Offset=0x14 Size=0x30
        struct _LIST_ENTRY m_lstSourceVoices;// Offset=0x44 Size=0x8
        struct _LIST_ENTRY m_leActiveVoice;// Offset=0x4c Size=0x8
        struct _LIST_ENTRY m_lePendingInactiveVoice;// Offset=0x54 Size=0x8
        struct _LIST_ENTRY m_leSourceVoice;// Offset=0x5c Size=0x8
        unsigned char m_bVoiceCount;// Offset=0x64 Size=0x1
        unsigned char m_bVoiceList;// Offset=0x65 Size=0x1
        unsigned char m_bAvailable3dFilter;// Offset=0x66 Size=0x1
        unsigned char m_bAlign0;// Offset=0x67 Size=0x1
        class DirectSound::CMcpxVoiceNotifier m_Notifier;// Offset=0x68 Size=0x8
        class DirectSound::CHrtfSource * m_pHrtfSource;// Offset=0x70 Size=0x4
        class DirectSound::CI3dl2Source * m_pI3dl2Source;// Offset=0x74 Size=0x4
        class DirectSound::CDirectSoundVoiceSettings * m_pSettings;// Offset=0x78 Size=0x4
        unsigned long m_dw3dMode;// Offset=0x7c Size=0x4
        long long m_rtVoiceOff;// Offset=0x80 Size=0x8
        unsigned long m_dwIgnoredTraps;// Offset=0x88 Size=0x4
        void CMcpxVoiceClient(class DirectSound::CMcpxVoiceClient & );
        void CMcpxVoiceClient(class DirectSound::CMcpxAPU * ,class DirectSound::CDirectSoundVoiceSettings * );// Offset=0x0 Size=0x68
        void ~CMcpxVoiceClient();// Offset=0x0 Size=0x5a
        long Initialize(int );// Offset=0x0 Size=0xfe
        long SetFormat();// Offset=0x0 Size=0x113
        long SetMixBins();// Offset=0x0 Size=0xc1
        long SetVolume();// Offset=0x0 Size=0x98
        long SetPitch();// Offset=0x0 Size=0x92
        long SetLFO(struct _DSLFODESC * );// Offset=0x0 Size=0x12f
        long SetEG(struct _DSENVELOPEDESC * );// Offset=0x0 Size=0x192
        long SetFilter(struct _DSFILTERDESC * );// Offset=0x0 Size=0x14d
        long ConnectVoice();// Offset=0x0 Size=0x10d
        long DisconnectVoice();// Offset=0x0 Size=0xf6
        unsigned long Commit3dSettings();// Offset=0x0 Size=0xd8
        void ActivateVoice();// Offset=0x0 Size=0x1b4
        void DeactivateVoice(int );// Offset=0x0 Size=0x124
        void ReleaseVoice();// Offset=0x0 Size=0x84
        void PauseVoice(unsigned long );// Offset=0x0 Size=0xa7
        void WaitForVoiceOff();// Offset=0x0 Size=0x1c
        void CheckStuckVoice();// Offset=0x0 Size=0x5a
        void ConvertMixBinValues(unsigned long * ,unsigned long * );// Offset=0x0 Size=0xb1
        void ConvertVolumeValues(struct DirectSound::MCPX_VOICE_VOLUME * );// Offset=0x0 Size=0x1e7
        void ConvertPitchValue(unsigned long * );// Offset=0x0 Size=0x82
        unsigned long GetVoiceCfgFMT(unsigned long );// Offset=0x0 Size=0x12
        long AllocateVoiceResources();// Offset=0x0 Size=0x17c
        void ReleaseVoiceResources();// Offset=0x0 Size=0x88
        void Apply3dSettings(unsigned long ,unsigned long ,unsigned long );// Offset=0x0 Size=0x4d
        void LoadHrtfFilter();// Offset=0x0 Size=0x105
        unsigned long BytesToSamples(unsigned long );// Offset=0x0 Size=0x19
        unsigned long SamplesToBytes(unsigned long );// Offset=0x0 Size=0x21
        unsigned long GetSslPosition();// Offset=0x0 Size=0x8b
        class DirectSound::CMcpxBuffer * GetSubMixDestination();// Offset=0x0 Size=0x12
        int ServiceVoiceInterrupt();
        int ScheduleDeferredCommand(unsigned long ,long long ,unsigned long );
        void RemoveDeferredCommand(unsigned long );
        void ServiceDeferredCommand(unsigned long ,unsigned long );
        void GetAntecedentVoice(unsigned char * ,class DirectSound::CMcpxVoiceClient ** );// Offset=0x0 Size=0x63
        void RemoveIdleVoice(int );// Offset=0x0 Size=0x1e7
        void RemoveStuckVoice();// Offset=0x0 Size=0x57
        void * __vecDelDtor(unsigned int );
    };
};

class DirectSound::CMcpxEPDspManager// Size=0xc (Id=2667)
{
    union // Size=0x127 (Id=0)
    {
        void CMcpxEPDspManager();// Offset=0x0 Size=0xd
        void ~CMcpxEPDspManager();// Offset=0x0 Size=0x1d
        void Initialize(class DirectSound::CMcpxGPDspManager * );// Offset=0x0 Size=0x127
        void AC3SetAnalogOutput(unsigned long ,unsigned long );// Offset=0x0 Size=0x28
        void AC3SetDigitalOutput(unsigned long ,unsigned long );// Offset=0x0 Size=0x45
        void * GetScratchSpaceAddr();
        struct DirectSound::DOLBY_CONFIG_TABLE * GetDolbyConfigTable();// Offset=0x0 Size=0x4
        unsigned long AC3GetTotalScratchSize();// Offset=0x0 Size=0x6
        void AC3GetSuperExec(void ** ,unsigned long * ,unsigned long * );// Offset=0x0 Size=0x1e
        void AC3GetLoader(void ** ,unsigned long * ,unsigned long * );// Offset=0x0 Size=0x21
        void AC3GetProgram(unsigned long ,void ** ,unsigned long * );// Offset=0x0 Size=0x9e
        void * AC3GetInitialConfigTable();// Offset=0x0 Size=0x6
        unsigned long AC3GetLoaderTableBase();// Offset=0x0 Size=0x6
        unsigned long AC3GetProgramBase();// Offset=0x0 Size=0x6
        unsigned long AC3GetMaxPrograms();// Offset=0x0 Size=0x4
        unsigned long AC3GetHeapSize();// Offset=0x0 Size=0x6
        void AC3StartGpInput(unsigned long );// Offset=0x0 Size=0x77
        class DirectSound::CMcpxDspScratchDma * m_pScratchDma;// Offset=0x0 Size=0x4
        class DirectSound::CMcpxGPDspManager * m_pGlobalProc;// Offset=0x4 Size=0x4
        unsigned char * m_pConfigTable;// Offset=0x8 Size=0x4
        void * __vecDelDtor(unsigned int );
    };
};

class DirectSound::CHrtfListener// Size=0x7c (Id=2668)
{
    union // Size=0x79 (Id=0)
    {
        const struct _D3DVECTOR m_vDefaultNormOrient;// Offset=0x0 Size=0xc
        const struct _D3DVECTOR m_vDefaultNormFrontOrient;// Offset=0x0 Size=0xc
        struct DirectSound::PAN3DSPEAKER m_aDefaultSpeakers[5];// Offset=0x0 Size=0x64
        struct _D3DMATRIX * m_pTransformMatrix;// Offset=0x0 Size=0x4
        struct _DS3DLISTENER & m_3dParams;// Offset=0x4 Size=0x4
        struct DirectSound::HRTFLISTENER m_3dData;// Offset=0x8 Size=0x70
        unsigned char m_fSurround;// Offset=0x78 Size=0x1
        void CHrtfListener(struct _DS3DLISTENER & );// Offset=0x0 Size=0x3b
        void ~CHrtfListener();// Offset=0x0 Size=0x14
        void Calculate3d(unsigned long );// Offset=0x0 Size=0xdd
    };
    public void * __vecDelDtor(unsigned int );
};

struct DirectSound::HRTFSOURCE// Size=0x1c (Id=2669)
{
    struct _D3DVECTOR vNormPos;// Offset=0x0 Size=0xc
    float flMagPos;// Offset=0xc Size=0x4
    float flAzimuth;// Offset=0x10 Size=0x4
    float flElevation;// Offset=0x14 Size=0x4
    float flThetaS;// Offset=0x18 Size=0x4
};

struct DirectSound::HRTFVOICE// Size=0xa8 (Id=2670)
{
    unsigned long dwChangeMask;// Offset=0x0 Size=0x4
    unsigned long dwMixBinValidMask;// Offset=0x4 Size=0x4
    unsigned long dwMixBinChangeMask;// Offset=0x8 Size=0x4
    long lDistanceVolume;// Offset=0xc Size=0x4
    long lConeVolume;// Offset=0x10 Size=0x4
    long lFrontVolume;// Offset=0x14 Size=0x4
    long lRearVolume;// Offset=0x18 Size=0x4
    long lDopplerPitch;// Offset=0x1c Size=0x4
    struct DirectSound::HRTFFILTERPAIR FilterPair;// Offset=0x20 Size=0x8
    long alMixBinVolumes[32];// Offset=0x28 Size=0x80
};

class DirectSound::CHrtfSource// Size=0xcc (Id=2671)
{
    union // Size=0xcc (Id=0)
    {
        struct DirectSound::HRTFSOURCE m_3dData;// Offset=0x0 Size=0x1c
        struct DirectSound::HRTFVOICE m_3dVoiceData;// Offset=0x1c Size=0xa8
        class DirectSound::CHrtfListener & m_Listener;// Offset=0xc4 Size=0x4
        struct _DS3DBUFFER & m_3dParams;// Offset=0xc8 Size=0x4
        unsigned int m_nAlgorithm;// Offset=0x0 Size=0x4
        const struct DirectSound::HRTFSOURCE m_Default3dData;// Offset=0x0 Size=0x1c
        const struct DirectSound::HRTFVOICE m_Default3dVoiceData;// Offset=0x0 Size=0xa8
        void  ( * m_pfnCalculate)(unsigned long ,class DirectSound::CHrtfSource * );// Offset=0x0 Size=0x4
        void  ( * m_pfnGetFilterPair)(class DirectSound::CHrtfSource * );// Offset=0x0 Size=0x4
        void CHrtfSource(class DirectSound::CHrtfListener & ,struct _DS3DBUFFER & );// Offset=0x0 Size=0x36
        void SetAlgorithm_FullHrtf();// Offset=0x0 Size=0x1c
        void SetAlgorithm_LightHrtf();// Offset=0x0 Size=0x1f
        void SetAlgorithm_Pan();
        int IsValidAlgorithm();// Offset=0x0 Size=0xd
        unsigned int GetAlgorithm();// Offset=0x0 Size=0x6
        void Calculate3d(unsigned long );// Offset=0x0 Size=0x16
        void GetHrtfFilterPair();// Offset=0x0 Size=0x8
    };
};

struct DirectSound::FIRFILTER8// Size=0x20 (Id=2672)
{
    unsigned char Coeff[31];// Offset=0x0 Size=0x1f
    unsigned char Delay;// Offset=0x1f Size=0x1
};

class DirectSound::CWaveFileMediaObject : public XWaveFileMediaObject, public CRefCount, protected DirectSound::CWaveFile// Size=0x50 (Id=2673)
{
    union // Size=0x76 (Id=0)
    {
        unsigned char __align0[76];// Offset=0x0 Size=0x4c
        unsigned long m_dwReadOffset;// Offset=0x4c Size=0x4
        void CWaveFileMediaObject(class DirectSound::CWaveFileMediaObject & );
        void CWaveFileMediaObject();// Offset=0x0 Size=0x2e
        void ~CWaveFileMediaObject();// Offset=0x0 Size=0x33
        long Initialize(void * );// Offset=0x0 Size=0x15
        long Initialize(char * );// Offset=0x0 Size=0x15
        unsigned long AddRef();// Offset=0x0 Size=0xa
        unsigned long Release();// Offset=0x0 Size=0xa
        long GetInfo(struct _XMEDIAINFO * );// Offset=0x0 Size=0x25
        long GetStatus(unsigned long * );// Offset=0x0 Size=0xf
        long Process(struct _XMEDIAPACKET * ,struct _XMEDIAPACKET * );// Offset=0x0 Size=0x76
        long Discontinuity();// Offset=0x0 Size=0x5
        long Flush();// Offset=0x0 Size=0x12
        long Seek(long ,unsigned long ,unsigned long * );// Offset=0x0 Size=0x36
        long GetLength(unsigned long * );// Offset=0x0 Size=0x12
        long GetFormat(struct tWAVEFORMATEX ** );// Offset=0x0 Size=0x12
        long GetLoopRegion(unsigned long * ,unsigned long * );// Offset=0x0 Size=0x17
        void __local_vftable_ctor_closure();
        void * __vecDelDtor(unsigned int );
    };
};

class DirectSound::CWaveFileMediaObject : public XWaveFileMediaObject, public CRefCount, protected DirectSound::CWaveFile// Size=0x50 (Id=2674)
{
    union // Size=0x76 (Id=0)
    {
        unsigned char __align0[76];// Offset=0x0 Size=0x4c
        unsigned long m_dwReadOffset;// Offset=0x4c Size=0x4
        void CWaveFileMediaObject(class DirectSound::CWaveFileMediaObject & );
        void CWaveFileMediaObject();// Offset=0x0 Size=0x2e
        void ~CWaveFileMediaObject();// Offset=0x0 Size=0x33
        long Initialize(void * );// Offset=0x0 Size=0x15
        long Initialize(char * );// Offset=0x0 Size=0x15
        unsigned long AddRef();// Offset=0x0 Size=0xa
        unsigned long Release();// Offset=0x0 Size=0xa
        long GetInfo(struct _XMEDIAINFO * );// Offset=0x0 Size=0x25
        long GetStatus(unsigned long * );// Offset=0x0 Size=0xf
        long Process(struct _XMEDIAPACKET * ,struct _XMEDIAPACKET * );// Offset=0x0 Size=0x76
        long Discontinuity();// Offset=0x0 Size=0x5
        long Flush();// Offset=0x0 Size=0x12
        long Seek(long ,unsigned long ,unsigned long * );// Offset=0x0 Size=0x36
        long GetLength(unsigned long * );// Offset=0x0 Size=0x12
        long GetFormat(struct tWAVEFORMATEX ** );// Offset=0x0 Size=0x12
        long GetLoopRegion(unsigned long * ,unsigned long * );// Offset=0x0 Size=0x17
        void __local_vftable_ctor_closure();
        void * __vecDelDtor(unsigned int );
    };
};

class DirectSound::CMcpxStream : public DirectSound::CMcpxVoiceClient// Size=0x188 (Id=2675)
{
    union // Size=0x188 (Id=0)
    {
        unsigned char __align0[144];// Offset=0x0 Size=0x90
        class DirectSound::CDirectSoundStreamSettings * m_pSettings;// Offset=0x90 Size=0x4
        unsigned long m_dwPrdControl;// Offset=0x94 Size=0x4
        struct DirectSound::MCPX_SSL_DESC m_aSslDesc[2];// Offset=0x98 Size=0x20
        struct _LIST_ENTRY m_lstPending;// Offset=0xb8 Size=0x8
        struct _LIST_ENTRY m_lstAvailable;// Offset=0xc0 Size=0x8
        struct _LIST_ENTRY m_lstCompleted;// Offset=0xc8 Size=0x8
        struct DirectSound::MCPX_PACKET_CONTEXT * m_paPacketContexts;// Offset=0xd0 Size=0x4
        struct DirectSound::MCPX_PACKET_CONTEXT * m_pWorkingPacket;// Offset=0xd4 Size=0x4
        unsigned long m_dwWorkingPacketOffset;// Offset=0xd8 Size=0x4
        unsigned char __align1[4];// Offset=0xdc Size=0x4
        struct DirectSound::MCPX_DEFERRED_COMMAND m_aDeferredCommands[5];// Offset=0xe0 Size=0xa0
        unsigned long m_dwFirstMappedSslIndex;// Offset=0x180 Size=0x4
        unsigned long m_dwMappedSslCount;// Offset=0x184 Size=0x4
        void CMcpxStream(class DirectSound::CMcpxStream & );
        void CMcpxStream(class DirectSound::CMcpxAPU * ,class DirectSound::CDirectSoundStreamSettings * );// Offset=0x0 Size=0xac
        void ~CMcpxStream();// Offset=0x0 Size=0x5b
        long Initialize();// Offset=0x0 Size=0xa7
        long Pause(unsigned long );// Offset=0x0 Size=0x47
        long Stop(long long ,unsigned long );// Offset=0x0 Size=0x39
        long Stop(unsigned long );// Offset=0x0 Size=0x62
        long GetStatus(unsigned long * );// Offset=0x0 Size=0x78
        int HasPendingData();// Offset=0x0 Size=0x18
        long SetFormat();// Offset=0x0 Size=0xca
        unsigned long GetLowWatermark();// Offset=0x0 Size=0x23
        long SubmitPacket(struct _XMEDIAPACKET & );// Offset=0x0 Size=0xc1
        long Discontinuity();// Offset=0x0 Size=0x25
        long Flush();// Offset=0x0 Size=0xd3
        long AllocateStreamResources();// Offset=0x0 Size=0x5
        void ReleaseStreamResources();// Offset=0x0 Size=0x40
        unsigned long Process();// Offset=0x0 Size=0xae
        int MapPackets(unsigned long );// Offset=0x0 Size=0x67
        unsigned long MapPacket(unsigned long ,struct DirectSound::MCPX_PACKET_CONTEXT * );// Offset=0x0 Size=0x163
        void CommitSsl(unsigned long );// Offset=0x0 Size=0xd5
        void CompleteSsl(unsigned long ,unsigned long );// Offset=0x0 Size=0xc3
        unsigned long GetSslBase(unsigned long ,unsigned long );// Offset=0x0 Size=0x16
        void CompletePackets(struct _LIST_ENTRY * ,unsigned long );// Offset=0x0 Size=0x76
        void CompleteDeferredPackets();// Offset=0x0 Size=0x28
        void CompletePacket(struct DirectSound::MCPX_PACKET_CONTEXT * ,unsigned long );// Offset=0x0 Size=0x4c
        void OnDeferredFlush();// Offset=0x0 Size=0xe
        int ServiceVoiceInterrupt();// Offset=0x0 Size=0x6c
        int ScheduleDeferredCommand(unsigned long ,long long ,unsigned long );// Offset=0x0 Size=0x2d
        void RemoveDeferredCommand(unsigned long );// Offset=0x0 Size=0x18
        void ServiceDeferredCommand(unsigned long ,unsigned long );// Offset=0x0 Size=0x3d
        void OnEndOfStream();// Offset=0x0 Size=0x2d
        void * __vecDelDtor(unsigned int );
    };
};

class DirectSound::CMcpxStream : public DirectSound::CMcpxVoiceClient// Size=0x188 (Id=2676)
{
    union // Size=0x188 (Id=0)
    {
        unsigned char __align0[144];// Offset=0x0 Size=0x90
        class DirectSound::CDirectSoundStreamSettings * m_pSettings;// Offset=0x90 Size=0x4
        unsigned long m_dwPrdControl;// Offset=0x94 Size=0x4
        struct DirectSound::MCPX_SSL_DESC m_aSslDesc[2];// Offset=0x98 Size=0x20
        struct _LIST_ENTRY m_lstPending;// Offset=0xb8 Size=0x8
        struct _LIST_ENTRY m_lstAvailable;// Offset=0xc0 Size=0x8
        struct _LIST_ENTRY m_lstCompleted;// Offset=0xc8 Size=0x8
        struct DirectSound::MCPX_PACKET_CONTEXT * m_paPacketContexts;// Offset=0xd0 Size=0x4
        struct DirectSound::MCPX_PACKET_CONTEXT * m_pWorkingPacket;// Offset=0xd4 Size=0x4
        unsigned long m_dwWorkingPacketOffset;// Offset=0xd8 Size=0x4
        unsigned char __align1[4];// Offset=0xdc Size=0x4
        struct DirectSound::MCPX_DEFERRED_COMMAND m_aDeferredCommands[5];// Offset=0xe0 Size=0xa0
        unsigned long m_dwFirstMappedSslIndex;// Offset=0x180 Size=0x4
        unsigned long m_dwMappedSslCount;// Offset=0x184 Size=0x4
        void CMcpxStream(class DirectSound::CMcpxStream & );
        void CMcpxStream(class DirectSound::CMcpxAPU * ,class DirectSound::CDirectSoundStreamSettings * );// Offset=0x0 Size=0xac
        void ~CMcpxStream();// Offset=0x0 Size=0x5b
        long Initialize();// Offset=0x0 Size=0xa7
        long Pause(unsigned long );// Offset=0x0 Size=0x47
        long Stop(long long ,unsigned long );// Offset=0x0 Size=0x39
        long Stop(unsigned long );// Offset=0x0 Size=0x62
        long GetStatus(unsigned long * );// Offset=0x0 Size=0x78
        int HasPendingData();// Offset=0x0 Size=0x18
        long SetFormat();// Offset=0x0 Size=0xca
        unsigned long GetLowWatermark();// Offset=0x0 Size=0x23
        long SubmitPacket(struct _XMEDIAPACKET & );// Offset=0x0 Size=0xc1
        long Discontinuity();// Offset=0x0 Size=0x25
        long Flush();// Offset=0x0 Size=0xd3
        long AllocateStreamResources();// Offset=0x0 Size=0x5
        void ReleaseStreamResources();// Offset=0x0 Size=0x40
        unsigned long Process();// Offset=0x0 Size=0xae
        int MapPackets(unsigned long );// Offset=0x0 Size=0x67
        unsigned long MapPacket(unsigned long ,struct DirectSound::MCPX_PACKET_CONTEXT * );// Offset=0x0 Size=0x163
        void CommitSsl(unsigned long );// Offset=0x0 Size=0xd5
        void CompleteSsl(unsigned long ,unsigned long );// Offset=0x0 Size=0xc3
        unsigned long GetSslBase(unsigned long ,unsigned long );// Offset=0x0 Size=0x16
        void CompletePackets(struct _LIST_ENTRY * ,unsigned long );// Offset=0x0 Size=0x76
        void CompleteDeferredPackets();// Offset=0x0 Size=0x28
        void CompletePacket(struct DirectSound::MCPX_PACKET_CONTEXT * ,unsigned long );// Offset=0x0 Size=0x4c
        void OnDeferredFlush();// Offset=0x0 Size=0xe
        int ServiceVoiceInterrupt();// Offset=0x0 Size=0x6c
        int ScheduleDeferredCommand(unsigned long ,long long ,unsigned long );// Offset=0x0 Size=0x2d
        void RemoveDeferredCommand(unsigned long );// Offset=0x0 Size=0x18
        void ServiceDeferredCommand(unsigned long ,unsigned long );// Offset=0x0 Size=0x3d
        void OnEndOfStream();// Offset=0x0 Size=0x2d
        void * __vecDelDtor(unsigned int );
    };
};

class DirectSound::CDirectSound : public IDirectSound, public CRefCount// Size=0x28 (Id=2677)
{
    union // Size=0x129 (Id=0)
    {
        class DirectSound::CDirectSound * m_pDirectSound;// Offset=0x0 Size=0x4
        unsigned char __align0[4];// Offset=0x4 Size=0x4
        class DirectSound::CDirectSoundSettings * m_pSettings;// Offset=0x8 Size=0x4
        class DirectSound::CMcpxAPU * m_pDevice;// Offset=0xc Size=0x4
        struct _LIST_ENTRY m_lst3dVoices;// Offset=0x10 Size=0x8
        struct IDirectSoundBuffer * m_apKeepAliveBuffers[4];// Offset=0x18 Size=0x10
        void CDirectSound(class DirectSound::CDirectSound & );
        void CDirectSound();// Offset=0x0 Size=0x1d
        void ~CDirectSound();// Offset=0x0 Size=0x3a
        long Initialize();// Offset=0x0 Size=0x129
        unsigned long AddRef();// Offset=0x0 Size=0x47
        unsigned long Release();// Offset=0x0 Size=0xb6
        long GetCaps(struct _DSCAPS * );// Offset=0x0 Size=0x70
        long CreateSoundBuffer(struct _DSBUFFERDESC * ,struct IDirectSoundBuffer ** ,struct IUnknown * );// Offset=0x0 Size=0x9c
        long CreateSoundStream(struct _DSSTREAMDESC * ,struct IDirectSoundStream ** ,struct IUnknown * );// Offset=0x0 Size=0x91
        long GetSpeakerConfig(unsigned long * );// Offset=0x0 Size=0x51
        long DownloadEffectsImage(void * ,unsigned long ,struct _DSEFFECTIMAGELOC * ,struct _DSEFFECTIMAGEDESC ** );// Offset=0x0 Size=0x64
        long GetEffectData(unsigned long ,unsigned long ,void * ,unsigned long );// Offset=0x0 Size=0x5f
        long SetEffectData(unsigned long ,unsigned long ,void * ,unsigned long ,unsigned long );// Offset=0x0 Size=0x62
        long CommitEffectData();// Offset=0x0 Size=0x50
        long EnableHeadphones(int );// Offset=0x0 Size=0x9b
        long SetMixBinHeadroom(unsigned long ,unsigned long );// Offset=0x0 Size=0x5f
        long SetAllParameters(struct _DS3DLISTENER * ,unsigned long );// Offset=0x0 Size=0xe6
        long SetDistanceFactor(float ,unsigned long );// Offset=0x0 Size=0x62
        long SetDopplerFactor(float ,unsigned long );// Offset=0x0 Size=0x62
        long SetOrientation(float ,float ,float ,float ,float ,float ,unsigned long );// Offset=0x0 Size=0x92
        long SetPosition(float ,float ,float ,unsigned long );// Offset=0x0 Size=0x76
        long SetRolloffFactor(float ,unsigned long );// Offset=0x0 Size=0x62
        long SetVelocity(float ,float ,float ,unsigned long );// Offset=0x0 Size=0x76
        long SetI3DL2Listener(struct _DSI3DL2LISTENER * ,unsigned long );// Offset=0x0 Size=0xfc
        long CommitDeferredSettings();// Offset=0x0 Size=0x7c
        long GetTime(long long * );// Offset=0x0 Size=0xf
        void DoWork();// Offset=0x0 Size=0x31
        void Force3dRecalc(unsigned long );
        void __local_vftable_ctor_closure();
        void * __vecDelDtor(unsigned int );
    };
};

class DirectSound::CDirectSound : public IDirectSound, public CRefCount// Size=0x28 (Id=2678)
{
    union // Size=0x129 (Id=0)
    {
        class DirectSound::CDirectSound * m_pDirectSound;// Offset=0x0 Size=0x4
        unsigned char __align0[4];// Offset=0x4 Size=0x4
        class DirectSound::CDirectSoundSettings * m_pSettings;// Offset=0x8 Size=0x4
        class DirectSound::CMcpxAPU * m_pDevice;// Offset=0xc Size=0x4
        struct _LIST_ENTRY m_lst3dVoices;// Offset=0x10 Size=0x8
        struct IDirectSoundBuffer * m_apKeepAliveBuffers[4];// Offset=0x18 Size=0x10
        void CDirectSound(class DirectSound::CDirectSound & );
        void CDirectSound();// Offset=0x0 Size=0x1d
        void ~CDirectSound();// Offset=0x0 Size=0x3a
        long Initialize();// Offset=0x0 Size=0x129
        unsigned long AddRef();// Offset=0x0 Size=0x47
        unsigned long Release();// Offset=0x0 Size=0xb6
        long GetCaps(struct _DSCAPS * );// Offset=0x0 Size=0x70
        long CreateSoundBuffer(struct _DSBUFFERDESC * ,struct IDirectSoundBuffer ** ,struct IUnknown * );// Offset=0x0 Size=0x9c
        long CreateSoundStream(struct _DSSTREAMDESC * ,struct IDirectSoundStream ** ,struct IUnknown * );// Offset=0x0 Size=0x91
        long GetSpeakerConfig(unsigned long * );// Offset=0x0 Size=0x51
        long DownloadEffectsImage(void * ,unsigned long ,struct _DSEFFECTIMAGELOC * ,struct _DSEFFECTIMAGEDESC ** );// Offset=0x0 Size=0x64
        long GetEffectData(unsigned long ,unsigned long ,void * ,unsigned long );// Offset=0x0 Size=0x5f
        long SetEffectData(unsigned long ,unsigned long ,void * ,unsigned long ,unsigned long );// Offset=0x0 Size=0x62
        long CommitEffectData();// Offset=0x0 Size=0x50
        long EnableHeadphones(int );// Offset=0x0 Size=0x9b
        long SetMixBinHeadroom(unsigned long ,unsigned long );// Offset=0x0 Size=0x5f
        long SetAllParameters(struct _DS3DLISTENER * ,unsigned long );// Offset=0x0 Size=0xe6
        long SetDistanceFactor(float ,unsigned long );// Offset=0x0 Size=0x62
        long SetDopplerFactor(float ,unsigned long );// Offset=0x0 Size=0x62
        long SetOrientation(float ,float ,float ,float ,float ,float ,unsigned long );// Offset=0x0 Size=0x92
        long SetPosition(float ,float ,float ,unsigned long );// Offset=0x0 Size=0x76
        long SetRolloffFactor(float ,unsigned long );// Offset=0x0 Size=0x62
        long SetVelocity(float ,float ,float ,unsigned long );// Offset=0x0 Size=0x76
        long SetI3DL2Listener(struct _DSI3DL2LISTENER * ,unsigned long );// Offset=0x0 Size=0xfc
        long CommitDeferredSettings();// Offset=0x0 Size=0x7c
        long GetTime(long long * );// Offset=0x0 Size=0xf
        void DoWork();// Offset=0x0 Size=0x31
        void Force3dRecalc(unsigned long );
        void __local_vftable_ctor_closure();
        void * __vecDelDtor(unsigned int );
    };
};

class DirectSound::CDirectSoundStreamSettings : public DirectSound::CDirectSoundVoiceSettings// Size=0xc4 (Id=2679)
{
    union // Size=0xc4 (Id=0)
    {
        unsigned char __align0[184];// Offset=0x0 Size=0xb8
        unsigned long m_dwMaxAttachedPackets;// Offset=0xb8 Size=0x4
        void  ( * m_pfnCallback)(void * ,void * ,unsigned long );// Offset=0xbc Size=0x4
        void * m_pvContext;// Offset=0xc0 Size=0x4
        void CDirectSoundStreamSettings(class DirectSound::CDirectSoundStreamSettings & );
        void CDirectSoundStreamSettings();// Offset=0x0 Size=0x10
        void ~CDirectSoundStreamSettings();// Offset=0x0 Size=0xb
        long Initialize(struct _DSSTREAMDESC * );// Offset=0x0 Size=0x39
        void __local_vftable_ctor_closure();
        void * __vecDelDtor(unsigned int );
    };
};

class DirectSound::CDirectSoundStreamSettings : public DirectSound::CDirectSoundVoiceSettings// Size=0xc4 (Id=2680)
{
    union // Size=0xc4 (Id=0)
    {
        unsigned char __align0[184];// Offset=0x0 Size=0xb8
        unsigned long m_dwMaxAttachedPackets;// Offset=0xb8 Size=0x4
        void  ( * m_pfnCallback)(void * ,void * ,unsigned long );// Offset=0xbc Size=0x4
        void * m_pvContext;// Offset=0xc0 Size=0x4
        void CDirectSoundStreamSettings(class DirectSound::CDirectSoundStreamSettings & );
        void CDirectSoundStreamSettings();// Offset=0x0 Size=0x10
        void ~CDirectSoundStreamSettings();// Offset=0x0 Size=0xb
        long Initialize(struct _DSSTREAMDESC * );// Offset=0x0 Size=0x39
        void __local_vftable_ctor_closure();
        void * __vecDelDtor(unsigned int );
    };
};

class DirectSound::CMcpxVoiceNotifier : public DirectSound::CMcpxNotifier// Size=0x8 (Id=2681)
{
    union // Size=0x17 (Id=0)
    {
        void Initialize(unsigned short );// Offset=0x0 Size=0x17
        void CMcpxVoiceNotifier();// Offset=0x0 Size=0xa
    };
};

struct DirectSound::MCPX_PACKET_CONTEXT// Size=0x30 (Id=2682)
{
    struct _LIST_ENTRY leListEntry;// Offset=0x0 Size=0x8
    struct _XMEDIAPACKET xmpPacket;// Offset=0x8 Size=0x18
    unsigned long dwCompletedSize;// Offset=0x20 Size=0x4
    unsigned long dwStatus;// Offset=0x24 Size=0x4
    long long rtTimestamp;// Offset=0x28 Size=0x8
};

class DirectSound::CMcpxSlopMemoryHeap : public CRefCount// Size=0x1c (Id=2685)
{
    union // Size=0xdb (Id=0)
    {
        const unsigned long m_dwUsageThreshold;// Offset=0x0 Size=0x4
        unsigned long & m_dwAvailable;// Offset=0x0 Size=0x4
        unsigned long & m_dwUsed;// Offset=0x0 Size=0x4
        unsigned char __align0[4];// Offset=0x4 Size=0x4
        struct _LIST_ENTRY m_lstEntries;// Offset=0x8 Size=0x8
        struct _LIST_ENTRY m_lstRuns;// Offset=0x10 Size=0x8
        struct DirectSound::SLOPRUNMARKER * m_pLargestFreeRunMarker;// Offset=0x18 Size=0x4
        void CMcpxSlopMemoryHeap(class DirectSound::CMcpxSlopMemoryHeap & );
        void CMcpxSlopMemoryHeap();// Offset=0x0 Size=0x25
        void ~CMcpxSlopMemoryHeap();// Offset=0x0 Size=0x61
        int AddRun(void * ,unsigned long ,unsigned long );// Offset=0x0 Size=0x84
        void * Alloc(unsigned long );// Offset=0x0 Size=0xdb
        void Free(void * );// Offset=0x0 Size=0x99
        struct DirectSound::SLOPRUNMARKER * CoalesceRuns(struct DirectSound::SLOPRUNMARKER * ,struct DirectSound::SLOPRUNMARKER * );// Offset=0x0 Size=0x1b
        struct DirectSound::SLOPRUNMARKER * CreateMarker(void * ,unsigned long ,struct _LIST_ENTRY * );// Offset=0x0 Size=0x27
        void __local_vftable_ctor_closure();
        void * __vecDelDtor(unsigned int );
    };
};

class DirectSound::CMcpxSlopMemoryHeap : public CRefCount// Size=0x1c (Id=2686)
{
    union // Size=0xdb (Id=0)
    {
        const unsigned long m_dwUsageThreshold;// Offset=0x0 Size=0x4
        unsigned long & m_dwAvailable;// Offset=0x0 Size=0x4
        unsigned long & m_dwUsed;// Offset=0x0 Size=0x4
        unsigned char __align0[4];// Offset=0x4 Size=0x4
        struct _LIST_ENTRY m_lstEntries;// Offset=0x8 Size=0x8
        struct _LIST_ENTRY m_lstRuns;// Offset=0x10 Size=0x8
        struct DirectSound::SLOPRUNMARKER * m_pLargestFreeRunMarker;// Offset=0x18 Size=0x4
        void CMcpxSlopMemoryHeap(class DirectSound::CMcpxSlopMemoryHeap & );
        void CMcpxSlopMemoryHeap();// Offset=0x0 Size=0x25
        void ~CMcpxSlopMemoryHeap();// Offset=0x0 Size=0x61
        int AddRun(void * ,unsigned long ,unsigned long );// Offset=0x0 Size=0x84
        void * Alloc(unsigned long );// Offset=0x0 Size=0xdb
        void Free(void * );// Offset=0x0 Size=0x99
        struct DirectSound::SLOPRUNMARKER * CoalesceRuns(struct DirectSound::SLOPRUNMARKER * ,struct DirectSound::SLOPRUNMARKER * );// Offset=0x0 Size=0x1b
        struct DirectSound::SLOPRUNMARKER * CreateMarker(void * ,unsigned long ,struct _LIST_ENTRY * );// Offset=0x0 Size=0x27
        void __local_vftable_ctor_closure();
        void * __vecDelDtor(unsigned int );
    };
};

struct DirectSound::I3DL2SOURCE// Size=0x14 (Id=2687)
{
    unsigned long dwChangeMask;// Offset=0x0 Size=0x4
    long lDirect;// Offset=0x4 Size=0x4
    long lSource;// Offset=0x8 Size=0x4
    int nDirectIir;// Offset=0xc Size=0x4
    int nReverbIir;// Offset=0x10 Size=0x4
};

class DirectSound::CI3dl2Listener// Size=0x224 (Id=2688)
{
    union // Size=0x317 (Id=0)
    {
        struct _DSI3DL2LISTENER & m_I3dl2Params;// Offset=0x0 Size=0x4
        struct _DSFX_I3DL2REVERB_PARAMS m_I3dl2Data;// Offset=0x4 Size=0x220
        const float m_flScale23;// Offset=0x0 Size=0x4
        const float m_flScale16;// Offset=0x0 Size=0x4
        const unsigned long m_dwSamplesPerSec;// Offset=0x0 Size=0x4
        float m_aflReflectionData[4][5];// Offset=0x0 Size=0x50
        float m_aflShortReverbInputFactor[4][2];// Offset=0x0 Size=0x20
        float m_aflLongReverbInputDelay[2][4];// Offset=0x0 Size=0x20
        float m_aflShortReverbFeedbackDelay[4];// Offset=0x0 Size=0x10
        enum __unnamed
        {
            MainDelayLineID=0,
            ReflectionDelayLineID=4,
            ShortReverbDelayLineID=8,
            LongReverbDelayLineID=12
        };
        enum __unnamed
        {
            InputIIR=0,
            MainDelayLineLongReverbIIR=1,
            ShortReverbIIR=4,
            LongReverbIIR=8
        };
        void CI3dl2Listener(struct _DSI3DL2LISTENER & );// Offset=0x0 Size=0xb
        void Initialize();// Offset=0x0 Size=0x37
        void CalculateI3dl2();// Offset=0x0 Size=0x317
        void SetSize(float );// Offset=0x0 Size=0x85
        void SetInputFilter(long ,float );// Offset=0x0 Size=0x5f
        void SetReflectionsGain(float );// Offset=0x0 Size=0x33
        void SetReflectionsDelay(float );// Offset=0x0 Size=0x1c
        void SetReverbGain(float );// Offset=0x0 Size=0x35
        void SetReverbDelay(float );// Offset=0x0 Size=0x90
        void SetDecayTimes(float ,float ,float );// Offset=0x0 Size=0xd7
        void SetDiffusion(float );// Offset=0x0 Size=0x77
        void SetDecayFilter(struct _DSFX_I3DL2REVERB_IIR * ,unsigned long ,float ,float ,float );// Offset=0x0 Size=0xab
        void Get1PoleLoPass(long ,long ,float ,float ,float * ,float * );// Offset=0x0 Size=0xbf
        void Get1PoleLoPass(long ,long ,float ,float ,int * ,int * );// Offset=0x0 Size=0x57
    };
};

class DirectSound::CI3dl2Source// Size=0x1c (Id=2689)
{
    union // Size=0x155 (Id=0)
    {
        struct DirectSound::I3DL2SOURCE m_I3dl2Data;// Offset=0x0 Size=0x14
        const struct DirectSound::I3DL2SOURCE m_DefaultI3dl2Data;// Offset=0x0 Size=0x14
        class DirectSound::CI3dl2Listener & m_Listener;// Offset=0x14 Size=0x4
        struct _DSI3DL2BUFFER & m_I3dl2Params;// Offset=0x18 Size=0x4
        void CI3dl2Source(class DirectSound::CI3dl2Listener & ,struct _DSI3DL2BUFFER & );// Offset=0x0 Size=0x23
        void CalculateI3dl2(float );// Offset=0x0 Size=0x155
        int Get1PoleLoPass(long ,long ,float ,float );// Offset=0x0 Size=0x8e
    };
};

class DirectSound::CRiffChunk// Size=0x18 (Id=2690)
{
    union // Size=0xaf (Id=0)
    {
        class DirectSound::CRiffChunk * m_pParentChunk;// Offset=0x0 Size=0x4
        class DirectSound::CStdFileStream * m_pStream;// Offset=0x4 Size=0x4
        unsigned long m_dwChunkId;// Offset=0x8 Size=0x4
        unsigned long m_dwDataOffset;// Offset=0xc Size=0x4
        unsigned long m_dwDataSize;// Offset=0x10 Size=0x4
        unsigned long m_dwFlags;// Offset=0x14 Size=0x4
        void CRiffChunk();// Offset=0x0 Size=0x16
        void ~CRiffChunk();// Offset=0x0 Size=0x1
        long Open(class DirectSound::CRiffChunk * ,class DirectSound::CStdFileStream * ,unsigned long );// Offset=0x0 Size=0xaf
        long Read(unsigned long ,void * ,unsigned long ,unsigned long * );// Offset=0x0 Size=0x5d
        unsigned long GetChunkId();
        unsigned long GetDataOffset();
        unsigned long GetDataSize();// Offset=0x0 Size=0x4
        int IsValid();
        void * __vecDelDtor(unsigned int );
    };
};

class DirectSound::CAc97MediaObject : public XAc97MediaObject, public CRefCount// Size=0x30 (Id=2691)
{
    union // Size=0x113 (Id=0)
    {
        unsigned long m_dwGlobalRefCount;// Offset=0x0 Size=0x4
        class DirectSound::CAc97Device * m_pDevice;// Offset=0x0 Size=0x4
        unsigned char __align0[8];// Offset=0x4 Size=0x8
        class DirectSound::CAc97Channel * m_pChannel;// Offset=0xc Size=0x4
        struct _LIST_ENTRY m_lstPending;// Offset=0x10 Size=0x8
        struct _LIST_ENTRY m_lstFree;// Offset=0x18 Size=0x8
        void  ( * m_pfnCallback)(void * ,void * ,unsigned long );// Offset=0x20 Size=0x4
        void * m_pvCallbackContext;// Offset=0x24 Size=0x4
        struct DirectSound::AC97PACKET * m_paPackets;// Offset=0x28 Size=0x4
        unsigned long m_dwStatus;// Offset=0x2c Size=0x4
        void CAc97MediaObject(class DirectSound::CAc97MediaObject & );
        void CAc97MediaObject();// Offset=0x0 Size=0x2e
        void ~CAc97MediaObject();// Offset=0x0 Size=0x78
        long Initialize(unsigned long ,void  ( * )(void * ,void * ,unsigned long ),void * );// Offset=0x0 Size=0x113
        unsigned long AddRef();// Offset=0x0 Size=0xa
        unsigned long Release();// Offset=0x0 Size=0xa
        long GetInfo(struct _XMEDIAINFO * );// Offset=0x0 Size=0x59
        long GetStatus(unsigned long * );// Offset=0x0 Size=0x48
        long Process(struct _XMEDIAPACKET * ,struct _XMEDIAPACKET * );// Offset=0x0 Size=0xdb
        long Discontinuity();// Offset=0x0 Size=0x5
        long Flush();// Offset=0x0 Size=0x7e
        long SetMode(unsigned long );// Offset=0x0 Size=0x6d
        long GetCurrentPosition(unsigned long * );// Offset=0x0 Size=0x4f
        void CompletePendingPackets();// Offset=0x0 Size=0x48
        void CompletePacket(struct DirectSound::AC97PACKET * ,unsigned long );// Offset=0x0 Size=0x39
        void InterruptCallback(void * );// Offset=0x0 Size=0xc
        void __local_vftable_ctor_closure();
        void * __vecDelDtor(unsigned int );
    };
};

class DirectSound::CAc97MediaObject : public XAc97MediaObject, public CRefCount// Size=0x30 (Id=2692)
{
    union // Size=0x113 (Id=0)
    {
        unsigned long m_dwGlobalRefCount;// Offset=0x0 Size=0x4
        class DirectSound::CAc97Device * m_pDevice;// Offset=0x0 Size=0x4
        unsigned char __align0[8];// Offset=0x4 Size=0x8
        class DirectSound::CAc97Channel * m_pChannel;// Offset=0xc Size=0x4
        struct _LIST_ENTRY m_lstPending;// Offset=0x10 Size=0x8
        struct _LIST_ENTRY m_lstFree;// Offset=0x18 Size=0x8
        void  ( * m_pfnCallback)(void * ,void * ,unsigned long );// Offset=0x20 Size=0x4
        void * m_pvCallbackContext;// Offset=0x24 Size=0x4
        struct DirectSound::AC97PACKET * m_paPackets;// Offset=0x28 Size=0x4
        unsigned long m_dwStatus;// Offset=0x2c Size=0x4
        void CAc97MediaObject(class DirectSound::CAc97MediaObject & );
        void CAc97MediaObject();// Offset=0x0 Size=0x2e
        void ~CAc97MediaObject();// Offset=0x0 Size=0x78
        long Initialize(unsigned long ,void  ( * )(void * ,void * ,unsigned long ),void * );// Offset=0x0 Size=0x113
        unsigned long AddRef();// Offset=0x0 Size=0xa
        unsigned long Release();// Offset=0x0 Size=0xa
        long GetInfo(struct _XMEDIAINFO * );// Offset=0x0 Size=0x59
        long GetStatus(unsigned long * );// Offset=0x0 Size=0x48
        long Process(struct _XMEDIAPACKET * ,struct _XMEDIAPACKET * );// Offset=0x0 Size=0xdb
        long Discontinuity();// Offset=0x0 Size=0x5
        long Flush();// Offset=0x0 Size=0x7e
        long SetMode(unsigned long );// Offset=0x0 Size=0x6d
        long GetCurrentPosition(unsigned long * );// Offset=0x0 Size=0x4f
        void CompletePendingPackets();// Offset=0x0 Size=0x48
        void CompletePacket(struct DirectSound::AC97PACKET * ,unsigned long );// Offset=0x0 Size=0x39
        void InterruptCallback(void * );// Offset=0x0 Size=0xc
        void __local_vftable_ctor_closure();
        void * __vecDelDtor(unsigned int );
    };
};

class DirectSound::CMcpxAPU : public CRefCount, public DirectSound::CMcpxCore, protected DirectSound::CHrtfListener, protected DirectSound::CI3dl2Listener// Size=0x7e0 (Id=2693)
{
    union // Size=0x7dc (Id=0)
    {
        unsigned char __align0[760];// Offset=0x0 Size=0x2f8
        unsigned short m_wFree2dVoiceCount;// Offset=0x2f8 Size=0x2
        unsigned short m_wFree3dVoiceCount;// Offset=0x2fa Size=0x2
        struct _LIST_ENTRY m_lst3dVoices;// Offset=0x2fc Size=0x8
        class DirectSound::CMcpxBufferSgeHeap m_SgeHeap;// Offset=0x304 Size=0x20
        unsigned long m_dwState;// Offset=0x324 Size=0x4
        unsigned long m_dwVoiceMapLock;// Offset=0x328 Size=0x4
        class DirectSound::CMcpxVoiceClient * m_apVoiceMap[256];// Offset=0x32c Size=0x400
        struct _LIST_ENTRY m_alstActiveVoices[3];// Offset=0x72c Size=0x18
        struct _LIST_ENTRY m_lstPendingInactiveVoices;// Offset=0x744 Size=0x8
        struct _LIST_ENTRY m_lstDeferredCommandsHigh;// Offset=0x74c Size=0x8
        struct _LIST_ENTRY m_lstDeferredCommandsLow;// Offset=0x754 Size=0x8
        union DirectSound::R_INTR m_arInterruptStatus[2];// Offset=0x75c Size=0x10
        struct _KDPC m_dpcInterrupt;// Offset=0x76c Size=0x1c
        struct _KINTERRUPT m_Interrupt;// Offset=0x0 Size=0x70
        unsigned char __align1[1816];// Offset=0x70 Size=0x718
        struct _KTIMER m_tmrDeferredCommandsHigh;// Offset=0x788 Size=0x28
        struct _KDPC m_dpcDeferredCommandsHigh;// Offset=0x7b0 Size=0x1c
        struct _HAL_SHUTDOWN_REGISTRATION m_HalShutdownData;// Offset=0x7cc Size=0x10
        unsigned long m_dwDeltaPanicCount;// Offset=0x0 Size=0x4
        unsigned long m_dwDeltaWarningCount;// Offset=0x0 Size=0x4
        void CMcpxAPU(class DirectSound::CMcpxAPU & );
        void CMcpxAPU(class DirectSound::CDirectSoundSettings * );// Offset=0x0 Size=0xab
        void ~CMcpxAPU();// Offset=0x0 Size=0xac
        long Initialize();// Offset=0x0 Size=0x12c
        long GetCaps(unsigned long * ,unsigned long * ,unsigned long * );// Offset=0x0 Size=0x37
        unsigned long Commit3dSettings();// Offset=0x0 Size=0xb6
        long DownloadEffectsImage(void * ,unsigned long ,struct _DSEFFECTIMAGEDESC ** );// Offset=0x0 Size=0x2f
        long SetEffectData(unsigned long ,unsigned long ,void * ,unsigned long ,unsigned long );// Offset=0x0 Size=0xc
        long GetEffectData(unsigned long ,unsigned long ,void * ,unsigned long );// Offset=0x0 Size=0x8
        long CommitEffectData();// Offset=0x0 Size=0xf
        long SetHrtfHeadroom(unsigned long );// Offset=0x0 Size=0x37
        long SetMixBinHeadroom(unsigned long );// Offset=0x0 Size=0x45
        long SetSpeakerConfig();// Offset=0x0 Size=0x94
        long AllocateVoices(class DirectSound::CMcpxVoiceClient * );// Offset=0x0 Size=0x9a
        void FreeVoices(class DirectSound::CMcpxVoiceClient * );// Offset=0x0 Size=0x73
        void BlockIdleHandler();// Offset=0x0 Size=0x8
        void UnblockIdleHandler();// Offset=0x0 Size=0x8
        int ScheduleDeferredCommand(struct DirectSound::MCPX_DEFERRED_COMMAND * );// Offset=0x0 Size=0x47
        void RemoveDeferredCommand(struct DirectSound::MCPX_DEFERRED_COMMAND * );// Offset=0x0 Size=0x44
        void DoWork();// Offset=0x0 Size=0x5
        unsigned long AllocateVoice(unsigned long * ,unsigned long );
        void FreeVoice(unsigned long ,unsigned long * ,unsigned long );
        void Terminate();// Offset=0x0 Size=0x63
        int ServiceApuInterrupt();// Offset=0x0 Size=0x55
        void ServiceApuInterruptDpc();// Offset=0x0 Size=0x42
        void ServiceVoiceInterrupt();// Offset=0x0 Size=0x79
        void WaitForMagicWrite();// Offset=0x0 Size=0x63
        void HandleFETrap();// Offset=0x0 Size=0x47
        void HandleDeltaWarning();// Offset=0x0 Size=0x1a
        void HandleDeltaPanic();// Offset=0x0 Size=0x5e
        void HandleSoftwareMethod(unsigned long ,unsigned long );// Offset=0x0 Size=0x16
        void HandleIdleVoice(unsigned long );// Offset=0x0 Size=0x66
        int ScheduleDeferredCommandHigh(struct DirectSound::MCPX_DEFERRED_COMMAND * );// Offset=0x0 Size=0xd6
        int ScheduleDeferredCommandLow(struct DirectSound::MCPX_DEFERRED_COMMAND * );// Offset=0x0 Size=0x25
        void ServiceDeferredCommandsHigh();// Offset=0x0 Size=0x82
        void ServiceDeferredCommandsLow();// Offset=0x0 Size=0x95
        void RemoveDeferredCommandHigh(struct DirectSound::MCPX_DEFERRED_COMMAND * );// Offset=0x0 Size=0x62
        void RemoveDeferredCommandLow(struct DirectSound::MCPX_DEFERRED_COMMAND * );// Offset=0x0 Size=0x5
        void ScheduleApuInterruptDpc();// Offset=0x0 Size=0x12
        void GetInterruptStatus();
        unsigned char ApuInterruptServiceRoutine(struct _KINTERRUPT * ,void * );// Offset=0x0 Size=0xc
        void ApuInterruptDpcRoutine(struct _KDPC * ,void * ,void * ,void * );// Offset=0x0 Size=0xc
        unsigned char GetInterruptStatusCallback(void * );// Offset=0x0 Size=0x2c
        void DeferredCommandDpcRoutine(struct _KDPC * ,void * ,void * ,void * );// Offset=0x0 Size=0xc
        void ApuShutdownNotifier(struct _HAL_SHUTDOWN_REGISTRATION * );// Offset=0x0 Size=0x1c
        void * __vecDelDtor(unsigned int );
    };
};

class DirectSound::CMcpxAPU : public CRefCount, public DirectSound::CMcpxCore, protected DirectSound::CHrtfListener, protected DirectSound::CI3dl2Listener// Size=0x7e0 (Id=2694)
{
    union // Size=0x7dc (Id=0)
    {
        unsigned char __align0[760];// Offset=0x0 Size=0x2f8
        unsigned short m_wFree2dVoiceCount;// Offset=0x2f8 Size=0x2
        unsigned short m_wFree3dVoiceCount;// Offset=0x2fa Size=0x2
        struct _LIST_ENTRY m_lst3dVoices;// Offset=0x2fc Size=0x8
        class DirectSound::CMcpxBufferSgeHeap m_SgeHeap;// Offset=0x304 Size=0x20
        unsigned long m_dwState;// Offset=0x324 Size=0x4
        unsigned long m_dwVoiceMapLock;// Offset=0x328 Size=0x4
        class DirectSound::CMcpxVoiceClient * m_apVoiceMap[256];// Offset=0x32c Size=0x400
        struct _LIST_ENTRY m_alstActiveVoices[3];// Offset=0x72c Size=0x18
        struct _LIST_ENTRY m_lstPendingInactiveVoices;// Offset=0x744 Size=0x8
        struct _LIST_ENTRY m_lstDeferredCommandsHigh;// Offset=0x74c Size=0x8
        struct _LIST_ENTRY m_lstDeferredCommandsLow;// Offset=0x754 Size=0x8
        union DirectSound::R_INTR m_arInterruptStatus[2];// Offset=0x75c Size=0x10
        struct _KDPC m_dpcInterrupt;// Offset=0x76c Size=0x1c
        struct _KINTERRUPT m_Interrupt;// Offset=0x0 Size=0x70
        unsigned char __align1[1816];// Offset=0x70 Size=0x718
        struct _KTIMER m_tmrDeferredCommandsHigh;// Offset=0x788 Size=0x28
        struct _KDPC m_dpcDeferredCommandsHigh;// Offset=0x7b0 Size=0x1c
        struct _HAL_SHUTDOWN_REGISTRATION m_HalShutdownData;// Offset=0x7cc Size=0x10
        unsigned long m_dwDeltaPanicCount;// Offset=0x0 Size=0x4
        unsigned long m_dwDeltaWarningCount;// Offset=0x0 Size=0x4
        void CMcpxAPU(class DirectSound::CMcpxAPU & );
        void CMcpxAPU(class DirectSound::CDirectSoundSettings * );// Offset=0x0 Size=0xab
        void ~CMcpxAPU();// Offset=0x0 Size=0xac
        long Initialize();// Offset=0x0 Size=0x12c
        long GetCaps(unsigned long * ,unsigned long * ,unsigned long * );// Offset=0x0 Size=0x37
        unsigned long Commit3dSettings();// Offset=0x0 Size=0xb6
        long DownloadEffectsImage(void * ,unsigned long ,struct _DSEFFECTIMAGEDESC ** );// Offset=0x0 Size=0x2f
        long SetEffectData(unsigned long ,unsigned long ,void * ,unsigned long ,unsigned long );// Offset=0x0 Size=0xc
        long GetEffectData(unsigned long ,unsigned long ,void * ,unsigned long );// Offset=0x0 Size=0x8
        long CommitEffectData();// Offset=0x0 Size=0xf
        long SetHrtfHeadroom(unsigned long );// Offset=0x0 Size=0x37
        long SetMixBinHeadroom(unsigned long );// Offset=0x0 Size=0x45
        long SetSpeakerConfig();// Offset=0x0 Size=0x94
        long AllocateVoices(class DirectSound::CMcpxVoiceClient * );// Offset=0x0 Size=0x9a
        void FreeVoices(class DirectSound::CMcpxVoiceClient * );// Offset=0x0 Size=0x73
        void BlockIdleHandler();// Offset=0x0 Size=0x8
        void UnblockIdleHandler();// Offset=0x0 Size=0x8
        int ScheduleDeferredCommand(struct DirectSound::MCPX_DEFERRED_COMMAND * );// Offset=0x0 Size=0x47
        void RemoveDeferredCommand(struct DirectSound::MCPX_DEFERRED_COMMAND * );// Offset=0x0 Size=0x44
        void DoWork();// Offset=0x0 Size=0x5
        unsigned long AllocateVoice(unsigned long * ,unsigned long );
        void FreeVoice(unsigned long ,unsigned long * ,unsigned long );
        void Terminate();// Offset=0x0 Size=0x63
        int ServiceApuInterrupt();// Offset=0x0 Size=0x55
        void ServiceApuInterruptDpc();// Offset=0x0 Size=0x42
        void ServiceVoiceInterrupt();// Offset=0x0 Size=0x79
        void WaitForMagicWrite();// Offset=0x0 Size=0x63
        void HandleFETrap();// Offset=0x0 Size=0x47
        void HandleDeltaWarning();// Offset=0x0 Size=0x1a
        void HandleDeltaPanic();// Offset=0x0 Size=0x5e
        void HandleSoftwareMethod(unsigned long ,unsigned long );// Offset=0x0 Size=0x16
        void HandleIdleVoice(unsigned long );// Offset=0x0 Size=0x66
        int ScheduleDeferredCommandHigh(struct DirectSound::MCPX_DEFERRED_COMMAND * );// Offset=0x0 Size=0xd6
        int ScheduleDeferredCommandLow(struct DirectSound::MCPX_DEFERRED_COMMAND * );// Offset=0x0 Size=0x25
        void ServiceDeferredCommandsHigh();// Offset=0x0 Size=0x82
        void ServiceDeferredCommandsLow();// Offset=0x0 Size=0x95
        void RemoveDeferredCommandHigh(struct DirectSound::MCPX_DEFERRED_COMMAND * );// Offset=0x0 Size=0x62
        void RemoveDeferredCommandLow(struct DirectSound::MCPX_DEFERRED_COMMAND * );// Offset=0x0 Size=0x5
        void ScheduleApuInterruptDpc();// Offset=0x0 Size=0x12
        void GetInterruptStatus();
        unsigned char ApuInterruptServiceRoutine(struct _KINTERRUPT * ,void * );// Offset=0x0 Size=0xc
        void ApuInterruptDpcRoutine(struct _KDPC * ,void * ,void * ,void * );// Offset=0x0 Size=0xc
        unsigned char GetInterruptStatusCallback(void * );// Offset=0x0 Size=0x2c
        void DeferredCommandDpcRoutine(struct _KDPC * ,void * ,void * ,void * );// Offset=0x0 Size=0xc
        void ApuShutdownNotifier(struct _HAL_SHUTDOWN_REGISTRATION * );// Offset=0x0 Size=0x1c
        void * __vecDelDtor(unsigned int );
    };
};

enum DirectSound::__unnamed
{
    WAVELDR_FILETYPE_WAVE=0,
    WAVELDR_FILETYPE_AIFF=1
};

enum DirectSound::__unnamed
{
    WAVELDR_FOURCC_RIFF=1179011410,
    WAVELDR_FOURCC_WAVE=1163280727,
    WAVELDR_FOURCC_FORMAT=544501094,
    WAVELDR_FOURCC_DATA=1635017060,
    WAVELDR_FOURCC_WAVE_SAMPLE=1886221175
};

enum DirectSound::__unnamed
{
    WAVELDR_FOURCC_FORM=1297239878,
    WAVELDR_FOURCC_AIFF=1179011393,
    WAVELDR_FOURCC_AIFFC=1128679745,
    WAVELDR_FOURCC_AIFF_VERSION=1380275794,
    WAVELDR_FOURCC_COMM=1296912195,
    WAVELDR_FOURCC_SOUND=1145983827,
    WAVELDR_FOURCC_NONE=1162760014,
    WAVELDR_FOURCC_INSTRUMENT=1414745673,
    WAVELDR_FOURCC_MARKER=1263681869
};

struct DirectSound::AIFFSOUNDHDR// Size=0x8 (Id=2702)
{
    unsigned long dwOffset;// Offset=0x0 Size=0x4
    unsigned long dwBlockSize;// Offset=0x4 Size=0x4
};

struct DirectSound::WAVESAMPLE_LOOP// Size=0x10 (Id=2703)
{
    unsigned long dwSize;// Offset=0x0 Size=0x4
    unsigned long dwLoopType;// Offset=0x4 Size=0x4
    unsigned long dwLoopStart;// Offset=0x8 Size=0x4
    unsigned long dwLoopLength;// Offset=0xc Size=0x4
};

struct DirectSound::AIFFFORMAT// Size=0x14 (Id=2704)
{
    unsigned short nChannels;// Offset=0x0 Size=0x2
    unsigned long dwSampleCount;// Offset=0x2 Size=0x4
    unsigned short wBitsPerSample;// Offset=0x6 Size=0x2
    unsigned short wFrequencyExponent;// Offset=0x8 Size=0x2
    unsigned long dwFrequencyMantissa;// Offset=0xa Size=0x4
    unsigned short wReserved;// Offset=0xe Size=0x2
    unsigned long dwCompression;// Offset=0x10 Size=0x4
};

struct DirectSound::AIFFLOOP// Size=0x6 (Id=2705)
{
    unsigned short wPlayMode;// Offset=0x0 Size=0x2
    unsigned short wStartMarker;// Offset=0x2 Size=0x2
    unsigned short wEndMarker;// Offset=0x4 Size=0x2
};

struct DirectSound::RIFFHEADER// Size=0x8 (Id=2706)
{
    unsigned long dwChunkId;// Offset=0x0 Size=0x4
    unsigned long dwDataSize;// Offset=0x4 Size=0x4
};

struct DirectSound::AIFFMARKERHDR// Size=0x2 (Id=2707)
{
    unsigned short wMarkerCount;// Offset=0x0 Size=0x2
};

struct DirectSound::AIFFINSTRUMENT// Size=0x14 (Id=2708)
{
    unsigned char bBaseNote;// Offset=0x0 Size=0x1
    unsigned char bDetune;// Offset=0x1 Size=0x1
    unsigned char bLowNote;// Offset=0x2 Size=0x1
    unsigned char bHighNote;// Offset=0x3 Size=0x1
    unsigned char bLowVelocity;// Offset=0x4 Size=0x1
    unsigned char bHighVelocity;// Offset=0x5 Size=0x1
    short nGain;// Offset=0x6 Size=0x2
    struct DirectSound::AIFFLOOP SustainLoop;// Offset=0x8 Size=0x6
    struct DirectSound::AIFFLOOP ReleaseLoop;// Offset=0xe Size=0x6
};

struct DirectSound::WAVESAMPLE// Size=0x14 (Id=2709)
{
    unsigned long dwSize;// Offset=0x0 Size=0x4
    unsigned short wUnityNote;// Offset=0x4 Size=0x2
    short nFineTune;// Offset=0x6 Size=0x2
    long lGain;// Offset=0x8 Size=0x4
    unsigned long dwOptions;// Offset=0xc Size=0x4
    unsigned long dwSampleLoops;// Offset=0x10 Size=0x4
    struct DirectSound::WAVESAMPLE_LOOP aLoops[0];
};

struct DirectSound::AIFFMARKER// Size=0x8 (Id=2710)
{
    unsigned short wMarkerId;// Offset=0x0 Size=0x2
    unsigned long dwPosition;// Offset=0x2 Size=0x4
    unsigned char bNameLength;// Offset=0x6 Size=0x1
    char szMarkerName[1];// Offset=0x7 Size=0x1
};

union DirectSound::MCP1_PRD::__unnamed// Size=0x4 (Id=2711)
{
    struct // Size=0x4 (Id=0)
    {
        unsigned long Length:16;// Offset=0x0 Size=0x4 BitOffset=0x0 BitSize=0x10
        unsigned long ContSize:2;// Offset=0x0 Size=0x4 BitOffset=0x10 BitSize=0x2
        unsigned long Samples:5;// Offset=0x0 Size=0x4 BitOffset=0x12 BitSize=0x5
        unsigned long Stereo:1;// Offset=0x0 Size=0x4 BitOffset=0x17 BitSize=0x1
        unsigned long Owner:1;// Offset=0x0 Size=0x4 BitOffset=0x18 BitSize=0x1
        unsigned long Intr:1;// Offset=0x0 Size=0x4 BitOffset=0x1f BitSize=0x1
    };
    unsigned long uValue;// Offset=0x0 Size=0x4
};

struct DirectSound::MCP1_PRD// Size=0x8 (Id=2712)
{
    unsigned long uAddr;// Offset=0x0 Size=0x4
    union DirectSound::MCP1_PRD::__unnamed Control;// Offset=0x4 Size=0x4
};

struct DirectSound::DOLBY_LOADER_TABLE::__unnamed// Size=0x8 (Id=2713)
{
    unsigned long ptr;// Offset=0x0 Size=0x4
    unsigned long size;// Offset=0x4 Size=0x4
};

struct DirectSound::DOLBY_LOADER_TABLE// Size=0x78 (Id=2714)
{
    unsigned long tableSize;// Offset=0x0 Size=0x4
    unsigned long maxProgs;// Offset=0x4 Size=0x4
    struct DirectSound::DOLBY_LOADER_TABLE::__unnamed prog[6];// Offset=0x8 Size=0x30
    unsigned long pcm_ptr;// Offset=0x38 Size=0x4
    unsigned long pcm_size;// Offset=0x3c Size=0x4
    unsigned long ltrt_ptr;// Offset=0x40 Size=0x4
    unsigned long ltrt_size;// Offset=0x44 Size=0x4
    unsigned long ac3_ptr;// Offset=0x48 Size=0x4
    unsigned long ac3_size;// Offset=0x4c Size=0x4
    unsigned long config_ptr;// Offset=0x50 Size=0x4
    unsigned long config_size;// Offset=0x54 Size=0x4
    unsigned long pingpong_offset;// Offset=0x58 Size=0x4
    unsigned long reserved1;// Offset=0x5c Size=0x4
    unsigned long ac3_zero_fill;// Offset=0x60 Size=0x4
    unsigned long reserved2;// Offset=0x64 Size=0x4
    unsigned long ac3_preamble;// Offset=0x68 Size=0x4
    unsigned long reserved3;// Offset=0x6c Size=0x4
    unsigned long heap_ptr;// Offset=0x70 Size=0x4
    unsigned long heap_size;// Offset=0x74 Size=0x4
};

struct DirectSound::SLOPMEMENTRY// Size=0xc (Id=2715)
{
    struct _LIST_ENTRY leListEntry;// Offset=0x0 Size=0x8
    void * pvBaseAddress;// Offset=0x8 Size=0x4
};

union DirectSound::R_FE_CONTROL// Size=0x4 (Id=2730)
{
    struct // Size=0x4 (Id=0)
    {
        unsigned long NotifyISO:1;// Offset=0x0 Size=0x4 BitOffset=0x0 BitSize=0x1
        unsigned long ReadISO:1;// Offset=0x0 Size=0x4 BitOffset=0x1 BitSize=0x1
        unsigned long WriteISO:1;// Offset=0x0 Size=0x4 BitOffset=0x2 BitSize=0x1
        unsigned long TrapOnNotifier:1;// Offset=0x0 Size=0x4 BitOffset=0x3 BitSize=0x1
        unsigned long Lock:1;// Offset=0x0 Size=0x4 BitOffset=0x4 BitSize=0x1
        unsigned long Mode:3;// Offset=0x0 Size=0x4 BitOffset=0x5 BitSize=0x3
        unsigned long TrapReason:4;// Offset=0x0 Size=0x4 BitOffset=0x8 BitSize=0x4
        unsigned long PIOClass:1;// Offset=0x0 Size=0x4 BitOffset=0xc BitSize=0x1
        unsigned long EnableLock:1;// Offset=0x0 Size=0x4 BitOffset=0xd BitSize=0x1
        unsigned long MethodOrigin:1;// Offset=0x0 Size=0x4 BitOffset=0xe BitSize=0x1
        unsigned long ValidSESSL:1;// Offset=0x0 Size=0x4 BitOffset=0xf BitSize=0x1
        unsigned long ValidSESGE:1;// Offset=0x0 Size=0x4 BitOffset=0x10 BitSize=0x1
        unsigned long ValidGPSGE:1;// Offset=0x0 Size=0x4 BitOffset=0x11 BitSize=0x1
    };
    unsigned long uValue;// Offset=0x0 Size=0x4
};

struct DirectSound::HRTFFILTERPAIR// Size=0x8 (Id=2731)
{
    struct DirectSound::FIRFILTER8 * pLeftFilter;// Offset=0x0 Size=0x4
    struct DirectSound::FIRFILTER8 * pRightFilter;// Offset=0x4 Size=0x4
};

struct DirectSound::CMcpxVoiceClient::RemoveIdleVoice::__l2::__unnamed// Size=0xc (Id=2732)
{
    unsigned long TVL;// Offset=0x0 Size=0x4
    unsigned long CVL;// Offset=0x4 Size=0x4
    unsigned long NVL;// Offset=0x8 Size=0x4
};

struct DirectSound::CMcpxVoiceClient::RemoveIdleVoice::__l2::__unnamed// Size=0xc (Id=2733)
{
    unsigned long TVL;// Offset=0x0 Size=0x4
    unsigned long CVL;// Offset=0x4 Size=0x4
    unsigned long NVL;// Offset=0x8 Size=0x4
};

struct DirectSound::WORD0::__unnamed// Size=0x4 (Id=2734)
{
    struct // Size=0x4 (Id=0)
    {
        unsigned long nextCommand:14;// Offset=0x0 Size=0x4 BitOffset=0x0 BitSize=0xe
        unsigned long EOL:1;// Offset=0x0 Size=0x4 BitOffset=0xe BitSize=0x1
    };
};

union DirectSound::WORD0// Size=0x4 (Id=2735)
{
    struct DirectSound::WORD0::__unnamed field;// Offset=0x0 Size=0x4
    unsigned long uValue;// Offset=0x0 Size=0x4
};

struct DirectSound::WORD1::__unnamed// Size=0x4 (Id=2736)
{
    struct // Size=0x4 (Id=0)
    {
        unsigned long interleave:1;// Offset=0x0 Size=0x4 BitOffset=0x0 BitSize=0x1
        unsigned long dspToSys:1;// Offset=0x0 Size=0x4 BitOffset=0x1 BitSize=0x1
        unsigned long IOC:2;// Offset=0x0 Size=0x4 BitOffset=0x2 BitSize=0x2
        unsigned long smOffWrBack:1;// Offset=0x0 Size=0x4 BitOffset=0x4 BitSize=0x1
        unsigned long smBufId:4;// Offset=0x0 Size=0x4 BitOffset=0x5 BitSize=0x4
        unsigned long iso:1;// Offset=0x0 Size=0x4 BitOffset=0x9 BitSize=0x1
        unsigned long smDataFormat:3;// Offset=0x0 Size=0x4 BitOffset=0xa BitSize=0x3
        unsigned long increment:11;// Offset=0x0 Size=0x4 BitOffset=0xe BitSize=0xb
    };
};

union DirectSound::WORD1// Size=0x4 (Id=2737)
{
    struct DirectSound::WORD1::__unnamed field;// Offset=0x0 Size=0x4
    unsigned long uValue;// Offset=0x0 Size=0x4
};

union DirectSound::R_SE_CONTROL// Size=0x4 (Id=2738)
{
    struct // Size=0x4 (Id=0)
    {
        unsigned long ReadISO:1;// Offset=0x0 Size=0x4 BitOffset=0x0 BitSize=0x1
        unsigned long WriteISO:1;// Offset=0x0 Size=0x4 BitOffset=0x1 BitSize=0x1
        unsigned long SampleReadISO:1;// Offset=0x0 Size=0x4 BitOffset=0x2 BitSize=0x1
        unsigned long GSCUpdate:2;// Offset=0x0 Size=0x4 BitOffset=0x3 BitSize=0x2
        unsigned long DeltaWarn:1;// Offset=0x0 Size=0x4 BitOffset=0x5 BitSize=0x1
        unsigned long Retriggered:1;// Offset=0x0 Size=0x4 BitOffset=0x6 BitSize=0x1
        unsigned long DeltaPanic:1;// Offset=0x0 Size=0x4 BitOffset=0x7 BitSize=0x1
    };
    unsigned long uValue;// Offset=0x0 Size=0x4
};

union DirectSound::R_GP_RESET// Size=0x4 (Id=2739)
{
    struct // Size=0x4 (Id=0)
    {
        unsigned long Global:1;// Offset=0x0 Size=0x4 BitOffset=0x0 BitSize=0x1
        unsigned long DSP:1;// Offset=0x0 Size=0x4 BitOffset=0x1 BitSize=0x1
        unsigned long NMI:1;// Offset=0x0 Size=0x4 BitOffset=0x2 BitSize=0x1
        unsigned long Abort:1;// Offset=0x0 Size=0x4 BitOffset=0x3 BitSize=0x1
    };
    unsigned long uValue;// Offset=0x0 Size=0x4
};

union DirectSound::R_GPDMA_CONFIG// Size=0x4 (Id=2740)
{
    struct // Size=0x4 (Id=0)
    {
        unsigned long Start:1;// Offset=0x0 Size=0x4 BitOffset=0x0 BitSize=0x1
        unsigned long Ready:1;// Offset=0x0 Size=0x4 BitOffset=0x1 BitSize=0x1
        unsigned long ReqIOC:1;// Offset=0x0 Size=0x4 BitOffset=0x2 BitSize=0x1
        unsigned long ReqEOL:1;// Offset=0x0 Size=0x4 BitOffset=0x3 BitSize=0x1
        unsigned long ReqErr:1;// Offset=0x0 Size=0x4 BitOffset=0x4 BitSize=0x1
    };
    unsigned long uValue;// Offset=0x0 Size=0x4
};

union DirectSound::R_GP_CONTROL// Size=0x4 (Id=2741)
{
    struct // Size=0x4 (Id=0)
    {
        unsigned long Idle:1;// Offset=0x0 Size=0x4 BitOffset=0x0 BitSize=0x1
        unsigned long Stopped:1;// Offset=0x0 Size=0x4 BitOffset=0x1 BitSize=0x1
        unsigned long EnableStop:1;// Offset=0x0 Size=0x4 BitOffset=0x2 BitSize=0x1
        unsigned long IntrNotify:1;// Offset=0x0 Size=0x4 BitOffset=0x3 BitSize=0x1
    };
    unsigned long uValue;// Offset=0x0 Size=0x4
};

struct DirectSound::WORD2::__unnamed// Size=0x4 (Id=2742)
{
    struct // Size=0x4 (Id=0)
    {
        unsigned long count0:4;// Offset=0x0 Size=0x4 BitOffset=0x0 BitSize=0x4
        unsigned long count1:10;// Offset=0x0 Size=0x4 BitOffset=0x4 BitSize=0xa
    };
};

union DirectSound::WORD2// Size=0x4 (Id=2743)
{
    struct DirectSound::WORD2::__unnamed field;// Offset=0x0 Size=0x4
    unsigned long uValue;// Offset=0x0 Size=0x4
};

struct DirectSound::DSP_CONTROL// Size=0x10 (Id=2744)
{
    union DirectSound::WORD0 w0;// Offset=0x0 Size=0x4
    union DirectSound::WORD1 w1;// Offset=0x4 Size=0x4
    union DirectSound::WORD2 w2;// Offset=0x8 Size=0x4
    union DirectSound::WORD3 w3;// Offset=0xc Size=0x4
};

struct DirectSound::WORD3::__unnamed// Size=0x4 (Id=2745)
{
    struct // Size=0x4 (Id=0)
    {
        unsigned long dspStart:14;// Offset=0x0 Size=0x4 BitOffset=0x0 BitSize=0xe
        unsigned long nul:1;// Offset=0x0 Size=0x4 BitOffset=0xe BitSize=0x1
    };
};

union DirectSound::WORD3// Size=0x4 (Id=2746)
{
    struct DirectSound::WORD3::__unnamed field;// Offset=0x0 Size=0x4
    unsigned long uValue;// Offset=0x0 Size=0x4
};

struct DirectSound::CMcpxCore::AllocateApuMemory::__l2::MCPX_ALLOC_CTX// Size=0x8 (Id=2747)
{
    unsigned long Size;// Offset=0x0 Size=0x4
    unsigned long Alignment;// Offset=0x4 Size=0x4
};

struct DirectSound::CMcpxCore::AllocateApuMemory::__l2::MCPX_ALLOC_CTX// Size=0x0 (Id=2748)
{
};

struct CAudioObject// Size=0x430 (Id=2757)
{
    float m_qstQuantStep;// Offset=0x0 Size=0x4
    long m_iPacketCurr;// Offset=0x4 Size=0x4
    long m_cBitPackedFrameSize;// Offset=0x8 Size=0x4
    long m_cBitPacketHeader;// Offset=0xc Size=0x4
    long m_cdwPacketHeader;// Offset=0x10 Size=0x4
    long m_cBitPacketHeaderFractionDw;// Offset=0x14 Size=0x4
    long m_cBitPacketLength;// Offset=0x18 Size=0x4
    long m_cRunOfZeros;// Offset=0x1c Size=0x4
    short m_iLevel;// Offset=0x20 Size=0x2
    unsigned char __align0[2];// Offset=0x22 Size=0x2
    long m_iSign;// Offset=0x24 Size=0x4
    long m_iHighCutOffCurr;// Offset=0x28 Size=0x4
    long m_iNextBarkIndex;// Offset=0x2c Size=0x4
    long m_fNoiseSub;// Offset=0x30 Size=0x4
    float m_fltBitsPerSample;// Offset=0x34 Size=0x4
    float m_fltWeightedBitsPerSample;// Offset=0x38 Size=0x4
    long m_iMaxEscSize;// Offset=0x3c Size=0x4
    long m_iMaxEscLevel;// Offset=0x40 Size=0x4
    long m_iVersion;// Offset=0x44 Size=0x4
    enum Status m_codecStatus;// Offset=0x48 Size=0x4
    long m_fSeekAdjustment;// Offset=0x4c Size=0x4
    long m_fPacketLossAdj;// Offset=0x50 Size=0x4
    long m_iSamplingRate;// Offset=0x54 Size=0x4
    unsigned short m_cChannel;// Offset=0x58 Size=0x2
    unsigned char __align1[2];// Offset=0x5a Size=0x2
    unsigned long m_nBytePerSample;// Offset=0x5c Size=0x4
    long m_cSubband;// Offset=0x60 Size=0x4
    long m_fAllowSuperFrame;// Offset=0x64 Size=0x4
    long m_fAllowSubFrame;// Offset=0x68 Size=0x4
    long m_fV5Lpc;// Offset=0x6c Size=0x4
    long m_iCurrSubFrame;// Offset=0x70 Size=0x4
    short m_iCurrReconCoef;// Offset=0x74 Size=0x2
    unsigned char __align2[2];// Offset=0x76 Size=0x2
    long m_fHeaderReset;// Offset=0x78 Size=0x4
    long m_iSubFrameSizeWithUpdate;// Offset=0x7c Size=0x4
    long m_iMaxSubFrameDiv;// Offset=0x80 Size=0x4
    long m_cMinSubFrameSample;// Offset=0x84 Size=0x4
    long m_cMinSubFrameSampleHalf;// Offset=0x88 Size=0x4
    long m_cMinSubFrameSampleQuad;// Offset=0x8c Size=0x4
    long m_cPossibleWinSize;// Offset=0x90 Size=0x4
    long m_iIncr;// Offset=0x94 Size=0x4
    long m_cSubFrameSample;// Offset=0x98 Size=0x4
    long m_cSubFrameSampleHalf;// Offset=0x9c Size=0x4
    long m_cSubFrameSampleQuad;// Offset=0xa0 Size=0x4
    struct SubFrameConfigInfo m_subfrmconfigPrev;// Offset=0xa4 Size=0xd0
    struct SubFrameConfigInfo m_subfrmconfigCurr;// Offset=0x174 Size=0xd0
    struct SubFrameConfigInfo m_subfrmconfigNext;// Offset=0x244 Size=0xd0
    long m_cBitsSubbandMax;// Offset=0x314 Size=0x4
    long m_cFrameSample;// Offset=0x318 Size=0x4
    long m_cFrameSampleHalf;// Offset=0x31c Size=0x4
    long m_cFrameSampleQuad;// Offset=0x320 Size=0x4
    long m_cLowCutOff;// Offset=0x324 Size=0x4
    long m_cHighCutOff;// Offset=0x328 Size=0x4
    long m_cLowCutOffLong;// Offset=0x32c Size=0x4
    long m_cHighCutOffLong;// Offset=0x330 Size=0x4
    long m_iWeightingMode;// Offset=0x334 Size=0x4
    enum StereoMode m_stereoMode;// Offset=0x338 Size=0x4
    long m_iEntropyMode;// Offset=0x33c Size=0x4
    float m_fltDitherLevel;// Offset=0x340 Size=0x4
    long m_iQuantStepSize;// Offset=0x344 Size=0x4
    float m_fltFlatenFactor;// Offset=0x348 Size=0x4
    float m_fltDctScale;// Offset=0x34c Size=0x4
    long m_cValidBarkBand;// Offset=0x350 Size=0x4
    long * m_rgiBarkIndex;// Offset=0x354 Size=0x4
    float m_fltSinRampUpStart;// Offset=0x358 Size=0x4
    float m_fltCosRampUpStart;// Offset=0x35c Size=0x4
    float m_fltSinRampUpPrior;// Offset=0x360 Size=0x4
    float m_fltCosRampUpPrior;// Offset=0x364 Size=0x4
    float m_fltSinRampUpStep;// Offset=0x368 Size=0x4
    float m_fltSinRampDownStart;// Offset=0x36c Size=0x4
    float m_fltCosRampDownStart;// Offset=0x370 Size=0x4
    float m_fltSinRampDownPrior;// Offset=0x374 Size=0x4
    float m_fltCosRampDownPrior;// Offset=0x378 Size=0x4
    float m_fltSinRampDownStep;// Offset=0x37c Size=0x4
    long m_iSizePrev;// Offset=0x380 Size=0x4
    long m_iSizeCurr;// Offset=0x384 Size=0x4
    long m_iSizeNext;// Offset=0x388 Size=0x4
    long m_iCoefRecurQ1;// Offset=0x38c Size=0x4
    long m_iCoefRecurQ2;// Offset=0x390 Size=0x4
    long m_iCoefRecurQ3;// Offset=0x394 Size=0x4
    long m_iCoefRecurQ4;// Offset=0x398 Size=0x4
    short * m_rgiCoefQ;// Offset=0x39c Size=0x4
    struct PerChannelInfo * m_rgpcinfo;// Offset=0x3a0 Size=0x4
    long * m_rgiCoefReconOrig;// Offset=0x3a4 Size=0x4
    long * m_rgiMaskQ;// Offset=0x3a8 Size=0x4
    long * m_rgcValidBarkBand;// Offset=0x3ac Size=0x4
    long * m_rgiBarkIndexOrig;// Offset=0x3b0 Size=0x4
    short * m_piPrevOutput;// Offset=0x3b4 Size=0x4
    long m_iDiscardSilence;// Offset=0x3b8 Size=0x4
    float m_fltFirstNoiseFreq;// Offset=0x3bc Size=0x4
    long m_iFirstNoiseBand;// Offset=0x3c0 Size=0x4
    long m_iFirstNoiseIndex;// Offset=0x3c4 Size=0x4
    long m_iNoisePeakIgnoreBand;// Offset=0x3c8 Size=0x4
    long * m_rgiFirstNoiseBand;// Offset=0x3cc Size=0x4
    unsigned char * m_rgbBandNotCoded;// Offset=0x3d0 Size=0x4
    float * m_rgffltSqrtBWRatio;// Offset=0x3d4 Size=0x4
    long * m_rgiNoisePower;// Offset=0x3d8 Size=0x4
    float * m_rgfltBandWeight;// Offset=0x3dc Size=0x4
    float * m_rgfltWeightFactor;// Offset=0x3e0 Size=0x4
    unsigned long * m_rguiWeightFactor;// Offset=0x3e4 Size=0x4
    unsigned long m_iFrameNumber;// Offset=0x3e8 Size=0x4
    long  ( * aupfnInverseQuantize)(struct CAudioObject * ,struct PerChannelInfo * ,long * );// Offset=0x3ec Size=0x4
    float  ( * aupfnCalcSqrtBWRatio)(struct PerChannelInfo * ,const long ,const long );// Offset=0x3f0 Size=0x4
    long  ( * prvpfnInverseTransformMono)(struct CAudioObject * ,struct PerChannelInfo * ,short * ,short * ,short * ,long );// Offset=0x3f4 Size=0x4
    long  ( * aupfnGetNextRun)(void * ,struct PerChannelInfo * ,long * );// Offset=0x3f8 Size=0x4
    long  ( * aupfnReconstruct)(struct CAudioObject * ,short * ,short * ,long );// Offset=0x3fc Size=0x4
    long  ( * aupfnDctIV)(struct CAudioObject * ,float * ,float ,unsigned long * );// Offset=0x400 Size=0x4
    void  ( * aupfnFFT)(float * ,long ,enum FftDirection );// Offset=0x404 Size=0x4
    struct tagRandState m_tRandState;// Offset=0x408 Size=0x8
    float * m_piSinForRecon2048;// Offset=0x410 Size=0x4
    float * m_piSinForRecon1024;// Offset=0x414 Size=0x4
    float * m_piSinForRecon512;// Offset=0x418 Size=0x4
    float * m_piSinForRecon256;// Offset=0x41c Size=0x4
    float * m_piSinForRecon128;// Offset=0x420 Size=0x4
    float * m_piSinForRecon64;// Offset=0x424 Size=0x4
    float * m_piSinForRecon;// Offset=0x428 Size=0x4
    float * m_piSinForSaveHistory;// Offset=0x42c Size=0x4
};

struct CAudioObjectDecoder// Size=0xa0 (Id=2768)
{
    struct CAudioObject * pau;// Offset=0x0 Size=0x4
    long m_fPacketLoss;// Offset=0x4 Size=0x4
    short m_cFrmInPacket;// Offset=0x8 Size=0x2
    unsigned char __align0[2];// Offset=0xa Size=0x2
    unsigned char * m_pbSrcCurr;// Offset=0xc Size=0x4
    unsigned short m_cbSrcCurrLength;// Offset=0x10 Size=0x2
    unsigned char __align1[2];// Offset=0x12 Size=0x2
    enum DecodeStatus m_decsts;// Offset=0x14 Size=0x4
    enum SubFrmDecodeStatus m_subfrmdecsts;// Offset=0x18 Size=0x4
    enum HdrDecodeStatus m_hdrdecsts;// Offset=0x1c Size=0x4
    enum RunLevelStatus m_rlsts;// Offset=0x20 Size=0x4
    short m_iChannel;// Offset=0x24 Size=0x2
    short m_iBand;// Offset=0x26 Size=0x2
    long m_fNoMoreData;// Offset=0x28 Size=0x4
    long m_fLastSubFrame;// Offset=0x2c Size=0x4
    struct CWMAInputBitStream m_ibstrm;// Offset=0x30 Size=0x38
    short * m_rgiRunEntry44ssQb;// Offset=0x68 Size=0x4
    short * m_rgiLevelEntry44ssQb;// Offset=0x6c Size=0x4
    short * m_rgiRunEntry44smQb;// Offset=0x70 Size=0x4
    short * m_rgiLevelEntry44smQb;// Offset=0x74 Size=0x4
    short * m_rgiRunEntry44ssOb;// Offset=0x78 Size=0x4
    short * m_rgiLevelEntry44ssOb;// Offset=0x7c Size=0x4
    short * m_rgiRunEntry44smOb;// Offset=0x80 Size=0x4
    short * m_rgiLevelEntry44smOb;// Offset=0x84 Size=0x4
    short * m_rgiRunEntry16ssOb;// Offset=0x88 Size=0x4
    short * m_rgiLevelEntry16ssOb;// Offset=0x8c Size=0x4
    short * m_rgiRunEntry16smOb;// Offset=0x90 Size=0x4
    short * m_rgiLevelEntry16smOb;// Offset=0x94 Size=0x4
    long  ( * m_pfnDecodeSubFrame)(struct CAudioObjectDecoder * ,long * );// Offset=0x98 Size=0x4
    long  ( * m_pfnDecodeCoefficient)(struct CAudioObjectDecoder * ,struct PerChannelInfo * ,long * );// Offset=0x9c Size=0x4
};

struct CAudioObjectDecoder// Size=0xa0 (Id=2773)
{
    struct CAudioObject * pau;// Offset=0x0 Size=0x4
    long m_fPacketLoss;// Offset=0x4 Size=0x4
    short m_cFrmInPacket;// Offset=0x8 Size=0x2
    unsigned char __align0[2];// Offset=0xa Size=0x2
    unsigned char * m_pbSrcCurr;// Offset=0xc Size=0x4
    unsigned short m_cbSrcCurrLength;// Offset=0x10 Size=0x2
    unsigned char __align1[2];// Offset=0x12 Size=0x2
    enum DecodeStatus m_decsts;// Offset=0x14 Size=0x4
    enum SubFrmDecodeStatus m_subfrmdecsts;// Offset=0x18 Size=0x4
    enum HdrDecodeStatus m_hdrdecsts;// Offset=0x1c Size=0x4
    enum RunLevelStatus m_rlsts;// Offset=0x20 Size=0x4
    short m_iChannel;// Offset=0x24 Size=0x2
    short m_iBand;// Offset=0x26 Size=0x2
    long m_fNoMoreData;// Offset=0x28 Size=0x4
    long m_fLastSubFrame;// Offset=0x2c Size=0x4
    struct CWMAInputBitStream m_ibstrm;// Offset=0x30 Size=0x38
    short * m_rgiRunEntry44ssQb;// Offset=0x68 Size=0x4
    short * m_rgiLevelEntry44ssQb;// Offset=0x6c Size=0x4
    short * m_rgiRunEntry44smQb;// Offset=0x70 Size=0x4
    short * m_rgiLevelEntry44smQb;// Offset=0x74 Size=0x4
    short * m_rgiRunEntry44ssOb;// Offset=0x78 Size=0x4
    short * m_rgiLevelEntry44ssOb;// Offset=0x7c Size=0x4
    short * m_rgiRunEntry44smOb;// Offset=0x80 Size=0x4
    short * m_rgiLevelEntry44smOb;// Offset=0x84 Size=0x4
    short * m_rgiRunEntry16ssOb;// Offset=0x88 Size=0x4
    short * m_rgiLevelEntry16ssOb;// Offset=0x8c Size=0x4
    short * m_rgiRunEntry16smOb;// Offset=0x90 Size=0x4
    short * m_rgiLevelEntry16smOb;// Offset=0x94 Size=0x4
    int  ( * m_pfnDecodeSubFrame)(struct CAudioObjectDecoder * ,long * );// Offset=0x98 Size=0x4
    int  ( * m_pfnDecodeCoefficient)(struct CAudioObjectDecoder * ,struct PerChannelInfo * ,long * );// Offset=0x9c Size=0x4
};

struct CAudioObject// Size=0x430 (Id=2774)
{
    float m_qstQuantStep;// Offset=0x0 Size=0x4
    long m_iPacketCurr;// Offset=0x4 Size=0x4
    long m_cBitPackedFrameSize;// Offset=0x8 Size=0x4
    long m_cBitPacketHeader;// Offset=0xc Size=0x4
    long m_cdwPacketHeader;// Offset=0x10 Size=0x4
    long m_cBitPacketHeaderFractionDw;// Offset=0x14 Size=0x4
    long m_cBitPacketLength;// Offset=0x18 Size=0x4
    long m_cRunOfZeros;// Offset=0x1c Size=0x4
    short m_iLevel;// Offset=0x20 Size=0x2
    unsigned char __align0[2];// Offset=0x22 Size=0x2
    long m_iSign;// Offset=0x24 Size=0x4
    long m_iHighCutOffCurr;// Offset=0x28 Size=0x4
    long m_iNextBarkIndex;// Offset=0x2c Size=0x4
    long m_fNoiseSub;// Offset=0x30 Size=0x4
    float m_fltBitsPerSample;// Offset=0x34 Size=0x4
    float m_fltWeightedBitsPerSample;// Offset=0x38 Size=0x4
    long m_iMaxEscSize;// Offset=0x3c Size=0x4
    long m_iMaxEscLevel;// Offset=0x40 Size=0x4
    long m_iVersion;// Offset=0x44 Size=0x4
    enum Status m_codecStatus;// Offset=0x48 Size=0x4
    long m_fSeekAdjustment;// Offset=0x4c Size=0x4
    long m_fPacketLossAdj;// Offset=0x50 Size=0x4
    long m_iSamplingRate;// Offset=0x54 Size=0x4
    unsigned short m_cChannel;// Offset=0x58 Size=0x2
    unsigned char __align1[2];// Offset=0x5a Size=0x2
    unsigned long m_nBytePerSample;// Offset=0x5c Size=0x4
    long m_cSubband;// Offset=0x60 Size=0x4
    long m_fAllowSuperFrame;// Offset=0x64 Size=0x4
    long m_fAllowSubFrame;// Offset=0x68 Size=0x4
    long m_fV5Lpc;// Offset=0x6c Size=0x4
    long m_iCurrSubFrame;// Offset=0x70 Size=0x4
    short m_iCurrReconCoef;// Offset=0x74 Size=0x2
    unsigned char __align2[2];// Offset=0x76 Size=0x2
    long m_fHeaderReset;// Offset=0x78 Size=0x4
    long m_iSubFrameSizeWithUpdate;// Offset=0x7c Size=0x4
    long m_iMaxSubFrameDiv;// Offset=0x80 Size=0x4
    long m_cMinSubFrameSample;// Offset=0x84 Size=0x4
    long m_cMinSubFrameSampleHalf;// Offset=0x88 Size=0x4
    long m_cMinSubFrameSampleQuad;// Offset=0x8c Size=0x4
    long m_cPossibleWinSize;// Offset=0x90 Size=0x4
    long m_iIncr;// Offset=0x94 Size=0x4
    long m_cSubFrameSample;// Offset=0x98 Size=0x4
    long m_cSubFrameSampleHalf;// Offset=0x9c Size=0x4
    long m_cSubFrameSampleQuad;// Offset=0xa0 Size=0x4
    struct SubFrameConfigInfo m_subfrmconfigPrev;// Offset=0xa4 Size=0xd0
    struct SubFrameConfigInfo m_subfrmconfigCurr;// Offset=0x174 Size=0xd0
    struct SubFrameConfigInfo m_subfrmconfigNext;// Offset=0x244 Size=0xd0
    long m_cBitsSubbandMax;// Offset=0x314 Size=0x4
    long m_cFrameSample;// Offset=0x318 Size=0x4
    long m_cFrameSampleHalf;// Offset=0x31c Size=0x4
    long m_cFrameSampleQuad;// Offset=0x320 Size=0x4
    long m_cLowCutOff;// Offset=0x324 Size=0x4
    long m_cHighCutOff;// Offset=0x328 Size=0x4
    long m_cLowCutOffLong;// Offset=0x32c Size=0x4
    long m_cHighCutOffLong;// Offset=0x330 Size=0x4
    long m_iWeightingMode;// Offset=0x334 Size=0x4
    enum StereoMode m_stereoMode;// Offset=0x338 Size=0x4
    long m_iEntropyMode;// Offset=0x33c Size=0x4
    float m_fltDitherLevel;// Offset=0x340 Size=0x4
    long m_iQuantStepSize;// Offset=0x344 Size=0x4
    float m_fltFlatenFactor;// Offset=0x348 Size=0x4
    float m_fltDctScale;// Offset=0x34c Size=0x4
    long m_cValidBarkBand;// Offset=0x350 Size=0x4
    long * m_rgiBarkIndex;// Offset=0x354 Size=0x4
    float m_fltSinRampUpStart;// Offset=0x358 Size=0x4
    float m_fltCosRampUpStart;// Offset=0x35c Size=0x4
    float m_fltSinRampUpPrior;// Offset=0x360 Size=0x4
    float m_fltCosRampUpPrior;// Offset=0x364 Size=0x4
    float m_fltSinRampUpStep;// Offset=0x368 Size=0x4
    float m_fltSinRampDownStart;// Offset=0x36c Size=0x4
    float m_fltCosRampDownStart;// Offset=0x370 Size=0x4
    float m_fltSinRampDownPrior;// Offset=0x374 Size=0x4
    float m_fltCosRampDownPrior;// Offset=0x378 Size=0x4
    float m_fltSinRampDownStep;// Offset=0x37c Size=0x4
    long m_iSizePrev;// Offset=0x380 Size=0x4
    long m_iSizeCurr;// Offset=0x384 Size=0x4
    long m_iSizeNext;// Offset=0x388 Size=0x4
    long m_iCoefRecurQ1;// Offset=0x38c Size=0x4
    long m_iCoefRecurQ2;// Offset=0x390 Size=0x4
    long m_iCoefRecurQ3;// Offset=0x394 Size=0x4
    long m_iCoefRecurQ4;// Offset=0x398 Size=0x4
    short * m_rgiCoefQ;// Offset=0x39c Size=0x4
    struct PerChannelInfo * m_rgpcinfo;// Offset=0x3a0 Size=0x4
    long * m_rgiCoefReconOrig;// Offset=0x3a4 Size=0x4
    long * m_rgiMaskQ;// Offset=0x3a8 Size=0x4
    long * m_rgcValidBarkBand;// Offset=0x3ac Size=0x4
    long * m_rgiBarkIndexOrig;// Offset=0x3b0 Size=0x4
    short * m_piPrevOutput;// Offset=0x3b4 Size=0x4
    long m_iDiscardSilence;// Offset=0x3b8 Size=0x4
    float m_fltFirstNoiseFreq;// Offset=0x3bc Size=0x4
    long m_iFirstNoiseBand;// Offset=0x3c0 Size=0x4
    long m_iFirstNoiseIndex;// Offset=0x3c4 Size=0x4
    long m_iNoisePeakIgnoreBand;// Offset=0x3c8 Size=0x4
    long * m_rgiFirstNoiseBand;// Offset=0x3cc Size=0x4
    unsigned char * m_rgbBandNotCoded;// Offset=0x3d0 Size=0x4
    float * m_rgffltSqrtBWRatio;// Offset=0x3d4 Size=0x4
    long * m_rgiNoisePower;// Offset=0x3d8 Size=0x4
    float * m_rgfltBandWeight;// Offset=0x3dc Size=0x4
    float * m_rgfltWeightFactor;// Offset=0x3e0 Size=0x4
    unsigned long * m_rguiWeightFactor;// Offset=0x3e4 Size=0x4
    unsigned long m_iFrameNumber;// Offset=0x3e8 Size=0x4
    int  ( * aupfnInverseQuantize)(struct CAudioObject * ,struct PerChannelInfo * ,long * );// Offset=0x3ec Size=0x4
    float  ( * aupfnCalcSqrtBWRatio)(struct PerChannelInfo * ,const long ,const long );// Offset=0x3f0 Size=0x4
    int  ( * prvpfnInverseTransformMono)(struct CAudioObject * ,struct PerChannelInfo * ,short * ,short * ,short * ,long );// Offset=0x3f4 Size=0x4
    int  ( * aupfnGetNextRun)(void * ,struct PerChannelInfo * ,long * );// Offset=0x3f8 Size=0x4
    int  ( * aupfnReconstruct)(struct CAudioObject * ,short * ,short * ,long );// Offset=0x3fc Size=0x4
    int  ( * aupfnDctIV)(struct CAudioObject * ,float * ,float ,unsigned long * );// Offset=0x400 Size=0x4
    void  ( * aupfnFFT)(float * ,long ,enum FftDirection );// Offset=0x404 Size=0x4
    struct tagRandState m_tRandState;// Offset=0x408 Size=0x8
    float * m_piSinForRecon2048;// Offset=0x410 Size=0x4
    float * m_piSinForRecon1024;// Offset=0x414 Size=0x4
    float * m_piSinForRecon512;// Offset=0x418 Size=0x4
    float * m_piSinForRecon256;// Offset=0x41c Size=0x4
    float * m_piSinForRecon128;// Offset=0x420 Size=0x4
    float * m_piSinForRecon64;// Offset=0x424 Size=0x4
    float * m_piSinForRecon;// Offset=0x428 Size=0x4
    float * m_piSinForSaveHistory;// Offset=0x42c Size=0x4
};

struct tAudioStreamInfo// Size=0x350 (Id=2805)
{
    unsigned short nVersion;// Offset=0x0 Size=0x2
    unsigned short wFormatTag;// Offset=0x2 Size=0x2
    unsigned long nSamplesPerSec;// Offset=0x4 Size=0x4
    unsigned long nAvgBytesPerSec;// Offset=0x8 Size=0x4
    unsigned long nBlockAlign;// Offset=0xc Size=0x4
    unsigned short nChannels;// Offset=0x10 Size=0x2
    unsigned char __align0[2];// Offset=0x12 Size=0x2
    unsigned long nSamplesPerBlock;// Offset=0x14 Size=0x4
    unsigned short nEncodeOpt;// Offset=0x18 Size=0x2
    unsigned char __align1[2];// Offset=0x1a Size=0x2
    long nBitsPerSample;// Offset=0x1c Size=0x4
    unsigned short wValidBitsPerSample;// Offset=0x20 Size=0x2
    unsigned char __align2[2];// Offset=0x22 Size=0x2
    unsigned long dwChannelMask;// Offset=0x24 Size=0x4
    struct _GUID_WMC SubFormat;// Offset=0x28 Size=0x10
    unsigned short wOriginalBitDepth;// Offset=0x38 Size=0x2
    unsigned short wStreamId;// Offset=0x3a Size=0x2
    unsigned long cbAudioSize;// Offset=0x3c Size=0x4
    void * hMSA;// Offset=0x40 Size=0x4
    long bTobeDecoded;// Offset=0x44 Size=0x4
    long bIsDecodable;// Offset=0x48 Size=0x4
    long bWantOutput;// Offset=0x4c Size=0x4
    int wmar;// Offset=0x50 Size=0x4
    long bTimeToChange;// Offset=0x54 Size=0x4
    long bTimeToChangex;// Offset=0x58 Size=0x4
    long bFirstTime;// Offset=0x5c Size=0x4
    unsigned char bAudioBuffer[512];// Offset=0x60 Size=0x200
    unsigned char * pbAudioBuffer;// Offset=0x260 Size=0x4
    unsigned long dwFrameSize;// Offset=0x264 Size=0x4
    unsigned long dwAudioBufSize;// Offset=0x268 Size=0x4
    unsigned long dwAudioBufCurOffset;// Offset=0x26c Size=0x4
    unsigned long cbNbFramesAudBuf;// Offset=0x270 Size=0x4
    unsigned long dwAudioBufDecoded;// Offset=0x274 Size=0x4
    unsigned long dwAudioPayloadSize;// Offset=0x278 Size=0x4
    unsigned char bBlockStart;// Offset=0x27c Size=0x1
    unsigned char __align3[3];// Offset=0x27d Size=0x3
    unsigned long dwBlockLeft;// Offset=0x280 Size=0x4
    unsigned long dwPayloadLeft;// Offset=0x284 Size=0x4
    unsigned long dwAudPayloadPresTime;// Offset=0x288 Size=0x4
    unsigned char __align4[4];// Offset=0x28c Size=0x4
    float dwAudioTimeStamp;// Offset=0x290 Size=0x8
    unsigned char * pStoreFrameStartPointer;// Offset=0x298 Size=0x4
    unsigned char * pStoreNextFrameStartPointer;// Offset=0x29c Size=0x4
    unsigned char * pDecodeFrameStartPointer;// Offset=0x2a0 Size=0x4
    unsigned char * pDecodeNextFrameStartPointer;// Offset=0x2a4 Size=0x4
    long bBufferIncreased;// Offset=0x2a8 Size=0x4
    long bStopReading;// Offset=0x2ac Size=0x4
    long bGotCompOutput;// Offset=0x2b0 Size=0x4
    long bOutputisReady;// Offset=0x2b4 Size=0x4
    unsigned long long cbPacketOffset;// Offset=0x2b8 Size=0x8
    unsigned long long cbCurrentPacketOffset;// Offset=0x2c0 Size=0x8
    unsigned long long cbNextPacketOffset;// Offset=0x2c8 Size=0x8
    struct tPACKET_PARSE_INFO_EX ppex;// Offset=0x2d0 Size=0x42
    struct tPAYLOAD_MAP_ENTRY_EX payload;// Offset=0x312 Size=0x27
    unsigned char __align5[3];// Offset=0x339 Size=0x3
    unsigned long iPayload;// Offset=0x33c Size=0x4
    unsigned short wPayStart;// Offset=0x340 Size=0x2
    unsigned char __align6[2];// Offset=0x342 Size=0x2
    unsigned long dwPayloadOffset;// Offset=0x344 Size=0x4
    enum tagWMCParseState parse_state;// Offset=0x348 Size=0x4
    long bPayloadGiven;// Offset=0x34c Size=0x4
};

struct strAudioStreamInfo_WMC// Size=0x20 (Id=2810)
{
    unsigned short u16FormatTag;// Offset=0x0 Size=0x2
    unsigned char __align0[2];// Offset=0x2 Size=0x2
    unsigned long u32BitsPerSecond;// Offset=0x4 Size=0x4
    unsigned short u16BitsPerSample;// Offset=0x8 Size=0x2
    unsigned short u16SamplesPerSecond;// Offset=0xa Size=0x2
    unsigned short u16NumChannels;// Offset=0xc Size=0x2
    unsigned char __align1[2];// Offset=0xe Size=0x2
    unsigned long u32BlockSize;// Offset=0x10 Size=0x4
    unsigned short u16ValidBitsPerSample;// Offset=0x14 Size=0x2
    unsigned char __align2[2];// Offset=0x16 Size=0x2
    unsigned long u32ChannelMask;// Offset=0x18 Size=0x4
    unsigned short u16StreamId;// Offset=0x1c Size=0x2
};

struct tAsfXAcmAudioErrorMaskingData// Size=0x8 (Id=2818)
{
    unsigned char span;// Offset=0x0 Size=0x1
    unsigned short virtualPacketLen;// Offset=0x1 Size=0x2
    unsigned short virtualChunkLen;// Offset=0x3 Size=0x2
    unsigned short silenceLen;// Offset=0x5 Size=0x2
    unsigned char silence[1];// Offset=0x7 Size=0x1
};

struct _DS3DLISTENER// Size=0x40 (Id=3140)
{
    union // Size=0x40 (Id=0)
    {
        unsigned long dwSize;// Offset=0x0 Size=0x4
        struct XGVECTOR3 vPosition;// Offset=0x4 Size=0xc
        struct XGVECTOR3 vVelocity;// Offset=0x10 Size=0xc
        struct XGVECTOR3 vOrientFront;// Offset=0x1c Size=0xc
        struct XGVECTOR3 vOrientTop;// Offset=0x28 Size=0xc
        float flDistanceFactor;// Offset=0x34 Size=0x4
        float flRolloffFactor;// Offset=0x38 Size=0x4
        float flDopplerFactor;// Offset=0x3c Size=0x4
        void _DS3DLISTENER();// Offset=0x0 Size=0x3
    };
};

struct _DS3DBUFFER// Size=0x4c (Id=3141)
{
    unsigned long dwSize;// Offset=0x0 Size=0x4
    struct XGVECTOR3 vPosition;// Offset=0x4 Size=0xc
    struct XGVECTOR3 vVelocity;// Offset=0x10 Size=0xc
    unsigned long dwInsideConeAngle;// Offset=0x1c Size=0x4
    unsigned long dwOutsideConeAngle;// Offset=0x20 Size=0x4
    struct XGVECTOR3 vConeOrientation;// Offset=0x24 Size=0xc
    long lConeOutsideVolume;// Offset=0x30 Size=0x4
    float flMinDistance;// Offset=0x34 Size=0x4
    float flMaxDistance;// Offset=0x38 Size=0x4
    unsigned long dwMode;// Offset=0x3c Size=0x4
    float flDistanceFactor;// Offset=0x40 Size=0x4
    float flRolloffFactor;// Offset=0x44 Size=0x4
    float flDopplerFactor;// Offset=0x48 Size=0x4
    void _DS3DBUFFER();
};

struct audio_data// Size=0x3c (Id=3270)
{
    char bank[16];// Offset=0x0 Size=0x10
    short entersnd;// Offset=0x10 Size=0x2
    short hitsnd;// Offset=0x12 Size=0x2
    int namesnd;// Offset=0x14 Size=0x4
    char stream[16];// Offset=0x18 Size=0x10
    short nareas;// Offset=0x28 Size=0x2
    short stereo;// Offset=0x2a Size=0x2
    short nparts[8];// Offset=0x2c Size=0x10
};

struct sound_data// Size=0x18 (Id=3271)
{
    char desc[16];// Offset=0x0 Size=0x10
    int idx;// Offset=0x10 Size=0x4
    short vol;// Offset=0x14 Size=0x2
    short pri;// Offset=0x16 Size=0x2
};

struct sounddata// Size=0x14 (Id=3331)
{
    float radius;// Offset=0x0 Size=0x4
    int sound;// Offset=0x4 Size=0x4
    struct mbnode * parent;// Offset=0x8 Size=0x4
    short musicarea;// Offset=0xc Size=0x2
    short active;// Offset=0xe Size=0x2
    short fade;// Offset=0x10 Size=0x2
    short flags;// Offset=0x12 Size=0x2
};

struct soundinfo// Size=0xc (Id=3362)
{
    float radius;// Offset=0x0 Size=0x4
    int musicarea;// Offset=0x4 Size=0x4
    short fade;// Offset=0x8 Size=0x2
    short flags;// Offset=0xa Size=0x2
};

struct s_audsound// Size=0x1c (Id=3420)
{
    char name[16];// Offset=0x0 Size=0x10
    unsigned int idx;// Offset=0x10 Size=0x4
    float len;// Offset=0x14 Size=0x4
    float time;// Offset=0x18 Size=0x4
};

struct audiotrackdata// Size=0x14 (Id=3427)
{
    int idx;// Offset=0x0 Size=0x4
    int pri;// Offset=0x4 Size=0x4
    float endtime;// Offset=0x8 Size=0x4
    float * pos;// Offset=0xc Size=0x4
    int tid;// Offset=0x10 Size=0x4
};


#endif // _AUDIO_H_
