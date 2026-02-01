#ifndef _GAME_H_
#define _GAME_H_

// Category: game
// Extracted from Xbox PDB symbols
// Total types: 52
// Note: Xbox symbols - may need adaptation for GameCube

struct SGEHEAPRUNMARKER// Size=0x14 (Id=187)
{
    struct _LIST_ENTRY leListEntry;// Offset=0x0 Size=0x8
    unsigned short nElement;// Offset=0x8 Size=0x2
    unsigned short nLength;// Offset=0xa Size=0x2
    union // Size=0x4 (Id=0)
    {
        unsigned long dwRefCount;// Offset=0xc Size=0x4
        int fAllocated;// Offset=0xc Size=0x4
    };
    void * pvBaseAddress;// Offset=0x10 Size=0x4
};

struct mmtime_tag// Size=0xc (Id=548)
{
    unsigned int wType;// Offset=0x0 Size=0x4
    union __unnamed u;// Offset=0x4 Size=0x8
};

struct SGEHEAPRUNMARKER// Size=0x14 (Id=926)
{
    struct _LIST_ENTRY leListEntry;// Offset=0x0 Size=0x8
    unsigned short nElement;// Offset=0x8 Size=0x2
    unsigned short nLength;// Offset=0xa Size=0x2
    union // Size=0x4 (Id=0)
    {
        unsigned long dwRefCount;// Offset=0xc Size=0x4
        int fAllocated;// Offset=0xc Size=0x4
    };
    void * pvBaseAddress;// Offset=0x10 Size=0x4
};

struct mmtime_tag// Size=0xc (Id=1112)
{
    unsigned int wType;// Offset=0x0 Size=0x4
    union __unnamed u;// Offset=0x4 Size=0x8
};

struct mmtime_tag::__unnamed::__unnamed// Size=0x8 (Id=1236)
{
    unsigned char hour;// Offset=0x0 Size=0x1
    unsigned char min;// Offset=0x1 Size=0x1
    unsigned char sec;// Offset=0x2 Size=0x1
    unsigned char frame;// Offset=0x3 Size=0x1
    unsigned char fps;// Offset=0x4 Size=0x1
    unsigned char dummy;// Offset=0x5 Size=0x1
    unsigned char pad[2];// Offset=0x6 Size=0x2
};

struct mmtime_tag::__unnamed::__unnamed// Size=0x4 (Id=1237)
{
    unsigned long songptrpos;// Offset=0x0 Size=0x4
};

union mmtime_tag::__unnamed// Size=0x8 (Id=1238)
{
    union // Size=0x8 (Id=0)
    {
        unsigned long ms;// Offset=0x0 Size=0x4
        unsigned long sample;// Offset=0x0 Size=0x4
        unsigned long cb;// Offset=0x0 Size=0x4
        unsigned long ticks;// Offset=0x0 Size=0x4
        struct mmtime_tag::__unnamed::__unnamed smpte;// Offset=0x0 Size=0x8
        struct mmtime_tag::__unnamed::__unnamed midi;// Offset=0x0 Size=0x4
    };
};

struct mmtime_tag// Size=0xc (Id=1239)
{
    unsigned int wType;// Offset=0x0 Size=0x4
    union mmtime_tag::__unnamed u;// Offset=0x4 Size=0x8
};

enum enemy_action_type
{
    E_READY=0,
    E_START=1,
    E_TAUNT=2,
    E_WALK=3,
    E_RUN=4,
    E_FLY=5,
    E_HOVER=6,
    E_LANDING=7,
    E_WALKTOREADY=8,
    E_READYTOWALK=9,
    E_RUNTOREADY=10,
    E_READYTORUN=11,
    E_ATTACK=12,
    E_ATTACK_R=13,
    E_ATTACK2=14,
    E_ATTACK2_R=15,
    E_ATTACK_PWR=16,
    E_ATTACK_PWR_R=17,
    E_ATTACK4=18,
    E_ATTACK4_R=19,
    E_ATTACK5=20,
    E_ATTACK5_R=21,
    E_RUNATTACK=22,
    E_RUNATTACK2=23,
    E_THROW=24,
    E_THROW2=25,
    E_THROW_FINISH=26,
    E_THROWTOREADY=27,
    E_HIT_REACT1=28,
    E_HIT_REACT2=29,
    E_HIT_REACT3=30,
    E_GETUP=31,
    E_DYING=32,
    E_NACTIONS=33
};

struct DirectSound::SGEHEAPRUNMARKER// Size=0x14 (Id=2611)
{
    struct _LIST_ENTRY leListEntry;// Offset=0x0 Size=0x8
    unsigned short nElement;// Offset=0x8 Size=0x2
    unsigned short nLength;// Offset=0xa Size=0x2
    union // Size=0x4 (Id=0)
    {
        unsigned long dwRefCount;// Offset=0xc Size=0x4
        int fAllocated;// Offset=0xc Size=0x4
    };
    void * pvBaseAddress;// Offset=0x10 Size=0x4
};

class DirectSound::CMcpxBufferSgeHeap// Size=0x20 (Id=2683)
{
    union // Size=0xcf (Id=0)
    {
        unsigned char __align0[4];// Offset=0x0 Size=0x4
        struct _LIST_ENTRY m_lstRuns;// Offset=0x4 Size=0x8
        struct _LIST_ENTRY m_lstMarkers;// Offset=0xc Size=0x8
        struct DirectSound::SGEHEAPRUNMARKER * m_paMarkers;// Offset=0x14 Size=0x4
        unsigned long m_nFreeElementCount;// Offset=0x18 Size=0x4
        struct DirectSound::SGEHEAPRUNMARKER * m_pLargestFreeRunMarker;// Offset=0x1c Size=0x4
        void CMcpxBufferSgeHeap(class DirectSound::CMcpxBufferSgeHeap & );
        void CMcpxBufferSgeHeap();// Offset=0x0 Size=0x19
        void ~CMcpxBufferSgeHeap();// Offset=0x0 Size=0x1c
        long Initialize(unsigned short );// Offset=0x0 Size=0x89
        struct DirectSound::SGEHEAPRUNMARKER * Alloc(void * ,unsigned long );// Offset=0x0 Size=0x8f
        void Free(struct DirectSound::SGEHEAPRUNMARKER * );// Offset=0x0 Size=0x4b
        unsigned long GetFreeElementCount();// Offset=0x0 Size=0x4
        struct DirectSound::SGEHEAPRUNMARKER * AllocRun(void * ,unsigned short );// Offset=0x0 Size=0xcf
        void FreeRun(struct DirectSound::SGEHEAPRUNMARKER * );// Offset=0x0 Size=0x5a
        struct DirectSound::SGEHEAPRUNMARKER * CoalesceRuns(struct DirectSound::SGEHEAPRUNMARKER * ,struct DirectSound::SGEHEAPRUNMARKER * );// Offset=0x0 Size=0x21
        struct DirectSound::SGEHEAPRUNMARKER * CreateMarker(struct DirectSound::SGEHEAPRUNMARKER * ,unsigned short ,unsigned short ,struct _LIST_ENTRY * );// Offset=0x0 Size=0x36
        void MapBuffer(unsigned long ,void * ,unsigned long );// Offset=0x0 Size=0x6d
        void UnmapBuffer(void * ,unsigned long );// Offset=0x0 Size=0x17
        void __local_vftable_ctor_closure();
        void * __vecDelDtor(unsigned int );
    };
};

class DirectSound::CMcpxBufferSgeHeap// Size=0x20 (Id=2684)
{
    union // Size=0xcf (Id=0)
    {
        unsigned char __align0[4];// Offset=0x0 Size=0x4
        struct _LIST_ENTRY m_lstRuns;// Offset=0x4 Size=0x8
        struct _LIST_ENTRY m_lstMarkers;// Offset=0xc Size=0x8
        struct DirectSound::SGEHEAPRUNMARKER * m_paMarkers;// Offset=0x14 Size=0x4
        unsigned long m_nFreeElementCount;// Offset=0x18 Size=0x4
        struct DirectSound::SGEHEAPRUNMARKER * m_pLargestFreeRunMarker;// Offset=0x1c Size=0x4
        void CMcpxBufferSgeHeap(class DirectSound::CMcpxBufferSgeHeap & );
        void CMcpxBufferSgeHeap();// Offset=0x0 Size=0x19
        void ~CMcpxBufferSgeHeap();// Offset=0x0 Size=0x1c
        long Initialize(unsigned short );// Offset=0x0 Size=0x89
        struct DirectSound::SGEHEAPRUNMARKER * Alloc(void * ,unsigned long );// Offset=0x0 Size=0x8f
        void Free(struct DirectSound::SGEHEAPRUNMARKER * );// Offset=0x0 Size=0x4b
        unsigned long GetFreeElementCount();// Offset=0x0 Size=0x4
        struct DirectSound::SGEHEAPRUNMARKER * AllocRun(void * ,unsigned short );// Offset=0x0 Size=0xcf
        void FreeRun(struct DirectSound::SGEHEAPRUNMARKER * );// Offset=0x0 Size=0x5a
        struct DirectSound::SGEHEAPRUNMARKER * CoalesceRuns(struct DirectSound::SGEHEAPRUNMARKER * ,struct DirectSound::SGEHEAPRUNMARKER * );// Offset=0x0 Size=0x21
        struct DirectSound::SGEHEAPRUNMARKER * CreateMarker(struct DirectSound::SGEHEAPRUNMARKER * ,unsigned short ,unsigned short ,struct _LIST_ENTRY * );// Offset=0x0 Size=0x36
        void MapBuffer(unsigned long ,void * ,unsigned long );// Offset=0x0 Size=0x6d
        void UnmapBuffer(void * ,unsigned long );// Offset=0x0 Size=0x17
        void __local_vftable_ctor_closure();
        void * __vecDelDtor(unsigned int );
    };
};

struct PMD// Size=0xc (Id=3080)
{
    int mdisp;// Offset=0x0 Size=0x4
    int pdisp;// Offset=0x4 Size=0x4
    int vdisp;// Offset=0x8 Size=0x4
};

struct PMD// Size=0xc (Id=3099)
{
    int mdisp;// Offset=0x0 Size=0x4
    int pdisp;// Offset=0x4 Size=0x4
    int vdisp;// Offset=0x8 Size=0x4
};

struct smplacement_t// Size=0xd4 (Id=3176)
{
    char name[32];// Offset=0x0 Size=0x20
    int libPartIndex;// Offset=0x20 Size=0x4
    class vec3 pos;// Offset=0x24 Size=0xc
    class vec3 rot;// Offset=0x30 Size=0xc
    class vec3 scale;// Offset=0x3c Size=0xc
    class mat43 matrix;// Offset=0x48 Size=0x30
    class mat43 invMatrix;// Offset=0x78 Size=0x30
    unsigned int flags;// Offset=0xa8 Size=0x4
    class vec4 * pPlcVerts;// Offset=0xac Size=0x4
    struct smtri_t * pSMTris;// Offset=0xb0 Size=0x4
    struct smplctri_t * pPlcTris;// Offset=0xb4 Size=0x4
    struct smgeoset_t * pGeosets;// Offset=0xb8 Size=0x4
    class vec3 * pVertNormals;// Offset=0xbc Size=0x4
    struct smcolor_t * pVertColors;// Offset=0xc0 Size=0x4
    struct smplane_t * pPlanes;// Offset=0xc4 Size=0x4
    int nPlanes;// Offset=0xc8 Size=0x4
    struct smtrilink_s ** ppTriLists;// Offset=0xcc Size=0x4
    int nVerts;// Offset=0xd0 Size=0x4
    void smplacement_t(struct smplacement_t & );
    void smplacement_t();
};

struct smplacement_t// Size=0xd4 (Id=3177)
{
    char name[32];// Offset=0x0 Size=0x20
    int libPartIndex;// Offset=0x20 Size=0x4
    class vec3 pos;// Offset=0x24 Size=0xc
    class vec3 rot;// Offset=0x30 Size=0xc
    class vec3 scale;// Offset=0x3c Size=0xc
    class mat43 matrix;// Offset=0x48 Size=0x30
    class mat43 invMatrix;// Offset=0x78 Size=0x30
    unsigned int flags;// Offset=0xa8 Size=0x4
    class vec4 * pPlcVerts;// Offset=0xac Size=0x4
    struct smtri_t * pSMTris;// Offset=0xb0 Size=0x4
    struct smplctri_t * pPlcTris;// Offset=0xb4 Size=0x4
    struct smgeoset_t * pGeosets;// Offset=0xb8 Size=0x4
    class vec3 * pVertNormals;// Offset=0xbc Size=0x4
    struct smcolor_t * pVertColors;// Offset=0xc0 Size=0x4
    struct smplane_t * pPlanes;// Offset=0xc4 Size=0x4
    int nPlanes;// Offset=0xc8 Size=0x4
    struct smtrilink_s ** ppTriLists;// Offset=0xcc Size=0x4
    int nVerts;// Offset=0xd0 Size=0x4
    void smplacement_t(struct smplacement_t & );
    void smplacement_t();
};

struct smcolor_t// Size=0xc (Id=3185)
{
    float r;// Offset=0x0 Size=0x4
    float g;// Offset=0x4 Size=0x4
    float b;// Offset=0x8 Size=0x4
};

struct smworld_t// Size=0x150 (Id=3197)
{
    char name[128];// Offset=0x0 Size=0x80
    char fHasDevData;// Offset=0x80 Size=0x1
    unsigned char __align0[3];// Offset=0x81 Size=0x3
    struct smplacement_t * pPlacements;// Offset=0x84 Size=0x4
    int nPlacements;// Offset=0x88 Size=0x4
    struct smlibpart_t * pLibParts;// Offset=0x8c Size=0x4
    int nLibParts;// Offset=0x90 Size=0x4
    struct otexture_s * pTextures;// Offset=0x94 Size=0x4
    int nTextures;// Offset=0x98 Size=0x4
    struct smlight_t * pLights;// Offset=0x9c Size=0x4
    int nLights;// Offset=0xa0 Size=0x4
    struct smcolor_t ambientColor;// Offset=0xa4 Size=0xc
    struct smsky_t sky;// Offset=0xb0 Size=0x5c
    struct smmodel_t * pSMModels;// Offset=0x10c Size=0x4
    int nSMModels;// Offset=0x110 Size=0x4
    int nAI;// Offset=0x114 Size=0x4
    void ** apAI;// Offset=0x118 Size=0x4
    int nEmit;// Offset=0x11c Size=0x4
    int cEmit;// Offset=0x120 Size=0x4
    int nEmitMax;// Offset=0x124 Size=0x4
    struct smemitplace_t * pEmit;// Offset=0x128 Size=0x4
    class vec3 cameraOffset;// Offset=0x12c Size=0xc
    class vec3 cameraStartPos;// Offset=0x138 Size=0xc
    class vec3 cameraTargetPos;// Offset=0x144 Size=0xc
    void smworld_t(struct smworld_t & );
    void smworld_t();
};

struct smworld_t// Size=0x150 (Id=3198)
{
    char name[128];// Offset=0x0 Size=0x80
    char fHasDevData;// Offset=0x80 Size=0x1
    unsigned char __align0[3];// Offset=0x81 Size=0x3
    struct smplacement_t * pPlacements;// Offset=0x84 Size=0x4
    int nPlacements;// Offset=0x88 Size=0x4
    struct smlibpart_t * pLibParts;// Offset=0x8c Size=0x4
    int nLibParts;// Offset=0x90 Size=0x4
    struct otexture_s * pTextures;// Offset=0x94 Size=0x4
    int nTextures;// Offset=0x98 Size=0x4
    struct smlight_t * pLights;// Offset=0x9c Size=0x4
    int nLights;// Offset=0xa0 Size=0x4
    struct smcolor_t ambientColor;// Offset=0xa4 Size=0xc
    struct smsky_t sky;// Offset=0xb0 Size=0x5c
    struct smmodel_t * pSMModels;// Offset=0x10c Size=0x4
    int nSMModels;// Offset=0x110 Size=0x4
    int nAI;// Offset=0x114 Size=0x4
    void ** apAI;// Offset=0x118 Size=0x4
    int nEmit;// Offset=0x11c Size=0x4
    int cEmit;// Offset=0x120 Size=0x4
    int nEmitMax;// Offset=0x124 Size=0x4
    struct smemitplace_t * pEmit;// Offset=0x128 Size=0x4
    class vec3 cameraOffset;// Offset=0x12c Size=0xc
    class vec3 cameraStartPos;// Offset=0x138 Size=0xc
    class vec3 cameraTargetPos;// Offset=0x144 Size=0xc
    void smworld_t(struct smworld_t & );
    void smworld_t();
};

struct smlibpart_t// Size=0x70 (Id=3199)
{
    char name[32];// Offset=0x0 Size=0x20
    class vec4 * pVerts;// Offset=0x20 Size=0x4
    int nVerts;// Offset=0x24 Size=0x4
    struct smtri_t * pSMTris;// Offset=0x28 Size=0x4
    int nTris;// Offset=0x2c Size=0x4
    class vec3 * pVertNormals;// Offset=0x30 Size=0x4
    class vec2 * pTexCoords;// Offset=0x34 Size=0x4
    class vec3 min;// Offset=0x38 Size=0xc
    class vec3 max;// Offset=0x44 Size=0xc
    int nTexCoords;// Offset=0x50 Size=0x4
    struct smgeoset_t * pGeosets;// Offset=0x54 Size=0x4
    int nGeosets;// Offset=0x58 Size=0x4
    unsigned int flags;// Offset=0x5c Size=0x4
    int listId;// Offset=0x60 Size=0x4
    int pad[3];// Offset=0x64 Size=0xc
    void smlibpart_t(struct smlibpart_t & );
    void smlibpart_t();
};

struct smlibpart_t// Size=0x70 (Id=3200)
{
    char name[32];// Offset=0x0 Size=0x20
    class vec4 * pVerts;// Offset=0x20 Size=0x4
    int nVerts;// Offset=0x24 Size=0x4
    struct smtri_t * pSMTris;// Offset=0x28 Size=0x4
    int nTris;// Offset=0x2c Size=0x4
    class vec3 * pVertNormals;// Offset=0x30 Size=0x4
    class vec2 * pTexCoords;// Offset=0x34 Size=0x4
    class vec3 min;// Offset=0x38 Size=0xc
    class vec3 max;// Offset=0x44 Size=0xc
    int nTexCoords;// Offset=0x50 Size=0x4
    struct smgeoset_t * pGeosets;// Offset=0x54 Size=0x4
    int nGeosets;// Offset=0x58 Size=0x4
    unsigned int flags;// Offset=0x5c Size=0x4
    int listId;// Offset=0x60 Size=0x4
    int pad[3];// Offset=0x64 Size=0xc
    void smlibpart_t(struct smlibpart_t & );
    void smlibpart_t();
};

struct smlight_t// Size=0x68 (Id=3201)
{
    char name[32];// Offset=0x0 Size=0x20
    class vec3 pos;// Offset=0x20 Size=0xc
    struct smcolor_t color;// Offset=0x2c Size=0xc
    float radius;// Offset=0x38 Size=0x4
    int type;// Offset=0x3c Size=0x4
    class vec3 dir;// Offset=0x40 Size=0xc
    float coneAngle;// Offset=0x4c Size=0x4
    float intensity;// Offset=0x50 Size=0x4
    unsigned int flags;// Offset=0x54 Size=0x4
    struct smlightray_t * pLightRays;// Offset=0x58 Size=0x4
    int nLightRays;// Offset=0x5c Size=0x4
    int maxLightRays;// Offset=0x60 Size=0x4
    float fallStart;// Offset=0x64 Size=0x4
    void smlight_t(struct smlight_t & );
    void smlight_t();
};

struct smlight_t// Size=0x68 (Id=3202)
{
    char name[32];// Offset=0x0 Size=0x20
    class vec3 pos;// Offset=0x20 Size=0xc
    struct smcolor_t color;// Offset=0x2c Size=0xc
    float radius;// Offset=0x38 Size=0x4
    int type;// Offset=0x3c Size=0x4
    class vec3 dir;// Offset=0x40 Size=0xc
    float coneAngle;// Offset=0x4c Size=0x4
    float intensity;// Offset=0x50 Size=0x4
    unsigned int flags;// Offset=0x54 Size=0x4
    struct smlightray_t * pLightRays;// Offset=0x58 Size=0x4
    int nLightRays;// Offset=0x5c Size=0x4
    int maxLightRays;// Offset=0x60 Size=0x4
    float fallStart;// Offset=0x64 Size=0x4
    void smlight_t(struct smlight_t & );
    void smlight_t();
};

struct smsky_t// Size=0x5c (Id=3203)
{
    int cloud0;// Offset=0x0 Size=0x4
    int cloud1;// Offset=0x4 Size=0x4
    int east;// Offset=0x8 Size=0x4
    int west;// Offset=0xc Size=0x4
    int north;// Offset=0x10 Size=0x4
    int south;// Offset=0x14 Size=0x4
    int up;// Offset=0x18 Size=0x4
    int down;// Offset=0x1c Size=0x4
    class vec3 dir0;// Offset=0x20 Size=0xc
    float speed0;// Offset=0x2c Size=0x4
    float scale0;// Offset=0x30 Size=0x4
    float alpha0;// Offset=0x34 Size=0x4
    class vec3 dir1;// Offset=0x38 Size=0xc
    float speed1;// Offset=0x44 Size=0x4
    float scale1;// Offset=0x48 Size=0x4
    float alpha1;// Offset=0x4c Size=0x4
    struct smcolor_t ambientColor;// Offset=0x50 Size=0xc
    void smsky_t(struct smsky_t & );
    void smsky_t();
};

struct smsky_t// Size=0x5c (Id=3204)
{
    int cloud0;// Offset=0x0 Size=0x4
    int cloud1;// Offset=0x4 Size=0x4
    int east;// Offset=0x8 Size=0x4
    int west;// Offset=0xc Size=0x4
    int north;// Offset=0x10 Size=0x4
    int south;// Offset=0x14 Size=0x4
    int up;// Offset=0x18 Size=0x4
    int down;// Offset=0x1c Size=0x4
    class vec3 dir0;// Offset=0x20 Size=0xc
    float speed0;// Offset=0x2c Size=0x4
    float scale0;// Offset=0x30 Size=0x4
    float alpha0;// Offset=0x34 Size=0x4
    class vec3 dir1;// Offset=0x38 Size=0xc
    float speed1;// Offset=0x44 Size=0x4
    float scale1;// Offset=0x48 Size=0x4
    float alpha1;// Offset=0x4c Size=0x4
    struct smcolor_t ambientColor;// Offset=0x50 Size=0xc
    void smsky_t(struct smsky_t & );
    void smsky_t();
};

struct smemitplace_t// Size=0x98 (Id=3205)
{
    char name[32];// Offset=0x0 Size=0x20
    unsigned int num;// Offset=0x20 Size=0x4
    unsigned int handle;// Offset=0x24 Size=0x4
    unsigned int folder;// Offset=0x28 Size=0x4
    unsigned int used;// Offset=0x2c Size=0x4
    class vec3 location;// Offset=0x30 Size=0xc
    float activeRange;// Offset=0x3c Size=0x4
    char aTName[32];// Offset=0x40 Size=0x20
    class vec3 orient;// Offset=0x60 Size=0xc
    class vec3 up;// Offset=0x6c Size=0xc
    int multiplier;// Offset=0x78 Size=0x4
    unsigned int emitFlags;// Offset=0x7c Size=0x4
    float rate;// Offset=0x80 Size=0x4
    unsigned int flags;// Offset=0x84 Size=0x4
    unsigned int pad[3];// Offset=0x88 Size=0xc
    int activeID;// Offset=0x94 Size=0x4
    void smemitplace_t(struct smemitplace_t & );
    void smemitplace_t();
};

struct smemitplace_t// Size=0x98 (Id=3206)
{
    char name[32];// Offset=0x0 Size=0x20
    unsigned int num;// Offset=0x20 Size=0x4
    unsigned int handle;// Offset=0x24 Size=0x4
    unsigned int folder;// Offset=0x28 Size=0x4
    unsigned int used;// Offset=0x2c Size=0x4
    class vec3 location;// Offset=0x30 Size=0xc
    float activeRange;// Offset=0x3c Size=0x4
    char aTName[32];// Offset=0x40 Size=0x20
    class vec3 orient;// Offset=0x60 Size=0xc
    class vec3 up;// Offset=0x6c Size=0xc
    int multiplier;// Offset=0x78 Size=0x4
    unsigned int emitFlags;// Offset=0x7c Size=0x4
    float rate;// Offset=0x80 Size=0x4
    unsigned int flags;// Offset=0x84 Size=0x4
    unsigned int pad[3];// Offset=0x88 Size=0xc
    int activeID;// Offset=0x94 Size=0x4
    void smemitplace_t(struct smemitplace_t & );
    void smemitplace_t();
};

struct smtrilink_s// Size=0x10 (Id=3207)
{
    struct smtri_t * pTri;// Offset=0x0 Size=0x4
    struct smtexture_t * pMap;// Offset=0x4 Size=0x4
    class vec4 * pVerts;// Offset=0x8 Size=0x4
    struct smtrilink_s * pNext;// Offset=0xc Size=0x4
};

struct smtri_t// Size=0x98 (Id=3208)
{
    int tex;// Offset=0x0 Size=0x4
    int vi[3];// Offset=0x4 Size=0xc
    int tc[3];// Offset=0x10 Size=0xc
    unsigned int flags;// Offset=0x1c Size=0x4
    class vec3 n;// Offset=0x20 Size=0xc
    class vec3 s;// Offset=0x2c Size=0xc
    class vec3 t;// Offset=0x38 Size=0xc
    class vec3 bp;// Offset=0x44 Size=0xc
    class vec2 stshift;// Offset=0x50 Size=0x8
    class vec3 edgeInnerNormals[3];// Offset=0x58 Size=0x24
    class vec3 min;// Offset=0x7c Size=0xc
    class vec3 max;// Offset=0x88 Size=0xc
    float backfaceConst;// Offset=0x94 Size=0x4
    void smtri_t(struct smtri_t & );
    void smtri_t();
};

struct smtri_t// Size=0x98 (Id=3209)
{
    int tex;// Offset=0x0 Size=0x4
    int vi[3];// Offset=0x4 Size=0xc
    int tc[3];// Offset=0x10 Size=0xc
    unsigned int flags;// Offset=0x1c Size=0x4
    class vec3 n;// Offset=0x20 Size=0xc
    class vec3 s;// Offset=0x2c Size=0xc
    class vec3 t;// Offset=0x38 Size=0xc
    class vec3 bp;// Offset=0x44 Size=0xc
    class vec2 stshift;// Offset=0x50 Size=0x8
    class vec3 edgeInnerNormals[3];// Offset=0x58 Size=0x24
    class vec3 min;// Offset=0x7c Size=0xc
    class vec3 max;// Offset=0x88 Size=0xc
    float backfaceConst;// Offset=0x94 Size=0x4
    void smtri_t(struct smtri_t & );
    void smtri_t();
};

struct smtexture_t// Size=0x20 (Id=3210)
{
    struct otexture_s * pTexture;// Offset=0x0 Size=0x4
    class vec2 uv[3];// Offset=0x4 Size=0x18
    struct smtexel_t * pData;// Offset=0x1c Size=0x4
    void smtexture_t(struct smtexture_t & );
    void smtexture_t();
};

struct smtexture_t// Size=0x20 (Id=3211)
{
    struct otexture_s * pTexture;// Offset=0x0 Size=0x4
    class vec2 uv[3];// Offset=0x4 Size=0x18
    struct smtexel_t * pData;// Offset=0x1c Size=0x4
    void smtexture_t(struct smtexture_t & );
    void smtexture_t();
};

struct smlightray_t// Size=0x14 (Id=3215)
{
    class vec3 end;// Offset=0x0 Size=0xc
    int placement;// Offset=0xc Size=0x4
    int face;// Offset=0x10 Size=0x4
    void smlightray_t(struct smlightray_t & );
    void smlightray_t();
};

struct smlightray_t// Size=0x14 (Id=3216)
{
    class vec3 end;// Offset=0x0 Size=0xc
    int placement;// Offset=0xc Size=0x4
    int face;// Offset=0x10 Size=0x4
    void smlightray_t(struct smlightray_t & );
    void smlightray_t();
};

struct smplctri_t// Size=0x8 (Id=3217)
{
    unsigned int flags;// Offset=0x0 Size=0x4
    struct smtexture_t * pMap;// Offset=0x4 Size=0x4
};

struct smgeoset_t// Size=0x2c (Id=3218)
{
    int nTris;// Offset=0x0 Size=0x4
    int startTri;// Offset=0x4 Size=0x4
    int texture;// Offset=0x8 Size=0x4
    int envmap;// Offset=0xc Size=0x4
    unsigned int flags;// Offset=0x10 Size=0x4
    int nStripStart;// Offset=0x14 Size=0x4
    int nStripLength;// Offset=0x18 Size=0x4
    int listId;// Offset=0x1c Size=0x4
    int pad[3];// Offset=0x20 Size=0xc
};

struct smplane_t// Size=0xcc (Id=3219)
{
    struct smtrilink_s * pTriList;// Offset=0x0 Size=0x4
    class vec3 n;// Offset=0x4 Size=0xc
    class vec3 s;// Offset=0x10 Size=0xc
    class vec3 t;// Offset=0x1c Size=0xc
    class vec3 bp;// Offset=0x28 Size=0xc
    class vec2 stshift;// Offset=0x34 Size=0x8
    class vec3 edgeInnerNormals[4];// Offset=0x3c Size=0x30
    class vec3 verts[4];// Offset=0x6c Size=0x30
    class vec2 uv[4];// Offset=0x9c Size=0x20
    int w;// Offset=0xbc Size=0x4
    int h;// Offset=0xc0 Size=0x4
    struct smtexel_t * pData;// Offset=0xc4 Size=0x4
    struct otexture_s * pTexture;// Offset=0xc8 Size=0x4
    void smplane_t(struct smplane_t & );
    void smplane_t();
};

struct smplane_t// Size=0xcc (Id=3220)
{
    struct smtrilink_s * pTriList;// Offset=0x0 Size=0x4
    class vec3 n;// Offset=0x4 Size=0xc
    class vec3 s;// Offset=0x10 Size=0xc
    class vec3 t;// Offset=0x1c Size=0xc
    class vec3 bp;// Offset=0x28 Size=0xc
    class vec2 stshift;// Offset=0x34 Size=0x8
    class vec3 edgeInnerNormals[4];// Offset=0x3c Size=0x30
    class vec3 verts[4];// Offset=0x6c Size=0x30
    class vec2 uv[4];// Offset=0x9c Size=0x20
    int w;// Offset=0xbc Size=0x4
    int h;// Offset=0xc0 Size=0x4
    struct smtexel_t * pData;// Offset=0xc4 Size=0x4
    struct otexture_s * pTexture;// Offset=0xc8 Size=0x4
    void smplane_t(struct smplane_t & );
    void smplane_t();
};

struct enemy// Size=0x394 (Id=3245)
{
    enum e_e_tpye type;// Offset=0x0 Size=0x4
    struct OBJGRP objgrp;// Offset=0x4 Size=0x68
    struct atree atree;// Offset=0x6c Size=0x48
    enum state state;// Offset=0xb4 Size=0x4
    struct E_ATTRIBUTES atts;// Offset=0xb8 Size=0x14
    int action;// Offset=0xcc Size=0x4
    int daction;// Offset=0xd0 Size=0x4
    struct ACTIONANIM actionlist[33];// Offset=0xd4 Size=0x108
    struct mbnode * shadow;// Offset=0x1dc Size=0x4
    int specialfx;// Offset=0x1e0 Size=0x4
    struct skinfx skinfx;// Offset=0x1e4 Size=0x18
    short gridnext;// Offset=0x1fc Size=0x2
    short area;// Offset=0x1fe Size=0x2
    float health;// Offset=0x200 Size=0x4
    short damage_count;// Offset=0x204 Size=0x2
    short org_lvl;// Offset=0x206 Size=0x2
    int attack_timer;// Offset=0x208 Size=0x4
    int stun_timer;// Offset=0x20c Size=0x4
    float trans[3];// Offset=0x210 Size=0xc
    float flooroffset;// Offset=0x21c Size=0x4
    float attn_offset[3];// Offset=0x220 Size=0xc
    float coll_offset[3];// Offset=0x22c Size=0xc
    float rad;// Offset=0x238 Size=0x4
    float hht;// Offset=0x23c Size=0x4
    float pyr[3];// Offset=0x240 Size=0xc
    float ang;// Offset=0x24c Size=0x4
    float angbak;// Offset=0x250 Size=0x4
    float anghit;// Offset=0x254 Size=0x4
    float genang_offset;// Offset=0x258 Size=0x4
    float pushed[3];// Offset=0x25c Size=0xc
    float pushang;// Offset=0x268 Size=0x4
    float pushmag2;// Offset=0x26c Size=0x4
    int push_cnt;// Offset=0x270 Size=0x4
    short closest;// Offset=0x274 Size=0x2
    short prev_closest;// Offset=0x276 Size=0x2
    float close_dist;// Offset=0x278 Size=0x4
    float actual_dist;// Offset=0x27c Size=0x4
    short moved;// Offset=0x280 Size=0x2
    short stopped;// Offset=0x282 Size=0x2
    int coll_pnum;// Offset=0x284 Size=0x4
    int coll_enenum;// Offset=0x288 Size=0x4
    struct item * coll_ip;// Offset=0x28c Size=0x4
    struct item * generator;// Offset=0x290 Size=0x4
    float floory;// Offset=0x294 Size=0x4
    struct worldobj * floor_wobj;// Offset=0x298 Size=0x4
    int floor_surf;// Offset=0x29c Size=0x4
    float damage;// Offset=0x2a0 Size=0x4
    int damagetype;// Offset=0x2a4 Size=0x4
    float damagedir[3];// Offset=0x2a8 Size=0xc
    float fxhittime[5];// Offset=0x2b4 Size=0x14
    int fxhitidx;// Offset=0x2c8 Size=0x4
    short attack_index;// Offset=0x2cc Size=0x2
    short attack_count;// Offset=0x2ce Size=0x2
    int attack_flag;// Offset=0x2d0 Size=0x4
    short endurance;// Offset=0x2d4 Size=0x2
    short watchdog;// Offset=0x2d6 Size=0x2
    short birth_style;// Offset=0x2d8 Size=0x2
    short visible;// Offset=0x2da Size=0x2
    short visactive;// Offset=0x2dc Size=0x2
    short recognized;// Offset=0x2de Size=0x2
    float dest[3];// Offset=0x2e0 Size=0xc
    float birth_pos[3];// Offset=0x2ec Size=0xc
    unsigned int sensor;// Offset=0x2f8 Size=0x4
    float view;// Offset=0x2fc Size=0x4
    float sight;// Offset=0x300 Size=0x4
    float xspd;// Offset=0x304 Size=0x4
    float zspd;// Offset=0x308 Size=0x4
    float prev_dir;// Offset=0x30c Size=0x4
    short algorithm;// Offset=0x310 Size=0x2
    short old_ai;// Offset=0x312 Size=0x2
    short prev_ai;// Offset=0x314 Size=0x2
    short mode1;// Offset=0x316 Size=0x2
    short mode2;// Offset=0x318 Size=0x2
    unsigned char __align0[2];// Offset=0x31a Size=0x2
    int flag1;// Offset=0x31c Size=0x4
    int flag2;// Offset=0x320 Size=0x4
    int counter1;// Offset=0x324 Size=0x4
    int counter2;// Offset=0x328 Size=0x4
    int skip_itemcol;// Offset=0x32c Size=0x4
    unsigned int ai_flags;// Offset=0x330 Size=0x4
    int prev_enemy;// Offset=0x334 Size=0x4
    int next_enemy;// Offset=0x338 Size=0x4
    int guard_mode;// Offset=0x33c Size=0x4
    int guard_closest;// Offset=0x340 Size=0x4
    float guard_dist;// Offset=0x344 Size=0x4
    int plr_ms;// Offset=0x348 Size=0x4
    int ms_idx;// Offset=0x34c Size=0x4
    int max_msidx;// Offset=0x350 Size=0x4
    int route;// Offset=0x354 Size=0x4
    int dead_end;// Offset=0x358 Size=0x4
    int count;// Offset=0x35c Size=0x4
    int lost;// Offset=0x360 Size=0x4
    short collided;// Offset=0x364 Size=0x2
    short stuck_count;// Offset=0x366 Size=0x2
    int operation_speed;// Offset=0x368 Size=0x4
    int operation_count;// Offset=0x36c Size=0x4
    int lv;// Offset=0x370 Size=0x4
    int play;// Offset=0x374 Size=0x4
    float idle_time;// Offset=0x378 Size=0x4
    float idle_secs;// Offset=0x37c Size=0x4
    float idle_frac;// Offset=0x380 Size=0x4
    struct item * gotitem;// Offset=0x384 Size=0x4
    int alpha;// Offset=0x388 Size=0x4
    float prev_frame;// Offset=0x38c Size=0x4
    int anim_done;// Offset=0x390 Size=0x4
};

struct enemy_data// Size=0x18 (Id=3268)
{
    int type;// Offset=0x0 Size=0x4
    int subtype;// Offset=0x4 Size=0x4
    char audname[8];// Offset=0x8 Size=0x8
    char desc[8];// Offset=0x10 Size=0x8
};

struct enemydata// Size=0x14 (Id=3325)
{
    short etype;// Offset=0x0 Size=0x2
    char strength;// Offset=0x2 Size=0x1
    char ai;// Offset=0x3 Size=0x1
    float ang;// Offset=0x4 Size=0x4
    unsigned int flags;// Offset=0x8 Size=0x4
    float rad;// Offset=0xc Size=0x4
    short interval;// Offset=0x10 Size=0x2
    short pickup;// Offset=0x12 Size=0x2
};

struct enemyinfo// Size=0xc (Id=3358)
{
    short strength;// Offset=0x0 Size=0x2
    short ai;// Offset=0x2 Size=0x2
    float rad;// Offset=0x4 Size=0x4
    short interval;// Offset=0x8 Size=0x2
    short dummy;// Offset=0xa Size=0x2
};

struct playermodelinfo// Size=0x4c (Id=3382)
{
    int ptype;// Offset=0x0 Size=0x4
    int color;// Offset=0x4 Size=0x4
    int level;// Offset=0x8 Size=0x4
    char * hidden;// Offset=0xc Size=0x4
    int modelidx;// Offset=0x10 Size=0x4
    int atreeidx;// Offset=0x14 Size=0x4
    int maxmodelsize;// Offset=0x18 Size=0x4
    int maxanimsize;// Offset=0x1c Size=0x4
    int maxtexturesize;// Offset=0x20 Size=0x4
    struct atreelist * atreelist;// Offset=0x24 Size=0x4
    int maxgeoanimsize;// Offset=0x28 Size=0x4
    int geoatreeidx;// Offset=0x2c Size=0x4
    struct atreelist * geoatreelist;// Offset=0x30 Size=0x4
    int sfxmodelidx;// Offset=0x34 Size=0x4
    int sfxatreeidx;// Offset=0x38 Size=0x4
    int sfxmaxmodelsize;// Offset=0x3c Size=0x4
    int sfxmaxanimsize;// Offset=0x40 Size=0x4
    int sfxmaxtexturesize;// Offset=0x44 Size=0x4
    struct atreelist * sfxatreelist;// Offset=0x48 Size=0x4
};

struct smmodel_t// Size=0x108 (Id=3594)
{
    char name[32];// Offset=0x0 Size=0x20
    struct smphysics_t physics;// Offset=0x20 Size=0x60
    float scale;// Offset=0x80 Size=0x4
    float speed;// Offset=0x84 Size=0x4
    float dir;// Offset=0x88 Size=0x4
    int aiNum;// Offset=0x8c Size=0x4
    int nWay;// Offset=0x90 Size=0x4
    struct smwaypoint_t * pWay;// Offset=0x94 Size=0x4
    void * pEnemy;// Offset=0x98 Size=0x4
    int anim;// Offset=0x9c Size=0x4
    char fPlayAnimBackwards;// Offset=0xa0 Size=0x1
    unsigned char __align0[3];// Offset=0xa1 Size=0x3
    float animStartTime;// Offset=0xa4 Size=0x4
    unsigned int animFlags;// Offset=0xa8 Size=0x4
    int prevAnim;// Offset=0xac Size=0x4
    float animSpeed;// Offset=0xb0 Size=0x4
    struct smintersection_t groundTriIntersection;// Offset=0xb4 Size=0x18
    struct smintersection_t * pGroundTriIntersection;// Offset=0xcc Size=0x4
    char fSlerping;// Offset=0xd0 Size=0x1
    unsigned char __align1[3];// Offset=0xd1 Size=0x3
    float slerpStartTime;// Offset=0xd4 Size=0x4
    float slerpEndTime;// Offset=0xd8 Size=0x4
    int slerpPrevAnim;// Offset=0xdc Size=0x4
    struct model_s * pModel;// Offset=0xe0 Size=0x4
    char fLoaded;// Offset=0xe4 Size=0x1
    unsigned char __align2[3];// Offset=0xe5 Size=0x3
    unsigned int flags;// Offset=0xe8 Size=0x4
    void * pExtra;// Offset=0xec Size=0x4
    struct smcolor_t ambientColor;// Offset=0xf0 Size=0xc
    class vec3 startPos;// Offset=0xfc Size=0xc
    void smmodel_t(struct smmodel_t & );
    void smmodel_t();
};

struct smmodel_t// Size=0x108 (Id=3595)
{
    char name[32];// Offset=0x0 Size=0x20
    struct smphysics_t physics;// Offset=0x20 Size=0x60
    float scale;// Offset=0x80 Size=0x4
    float speed;// Offset=0x84 Size=0x4
    float dir;// Offset=0x88 Size=0x4
    int aiNum;// Offset=0x8c Size=0x4
    int nWay;// Offset=0x90 Size=0x4
    struct smwaypoint_t * pWay;// Offset=0x94 Size=0x4
    void * pEnemy;// Offset=0x98 Size=0x4
    int anim;// Offset=0x9c Size=0x4
    char fPlayAnimBackwards;// Offset=0xa0 Size=0x1
    unsigned char __align0[3];// Offset=0xa1 Size=0x3
    float animStartTime;// Offset=0xa4 Size=0x4
    unsigned int animFlags;// Offset=0xa8 Size=0x4
    int prevAnim;// Offset=0xac Size=0x4
    float animSpeed;// Offset=0xb0 Size=0x4
    struct smintersection_t groundTriIntersection;// Offset=0xb4 Size=0x18
    struct smintersection_t * pGroundTriIntersection;// Offset=0xcc Size=0x4
    char fSlerping;// Offset=0xd0 Size=0x1
    unsigned char __align1[3];// Offset=0xd1 Size=0x3
    float slerpStartTime;// Offset=0xd4 Size=0x4
    float slerpEndTime;// Offset=0xd8 Size=0x4
    int slerpPrevAnim;// Offset=0xdc Size=0x4
    struct model_s * pModel;// Offset=0xe0 Size=0x4
    char fLoaded;// Offset=0xe4 Size=0x1
    unsigned char __align2[3];// Offset=0xe5 Size=0x3
    unsigned int flags;// Offset=0xe8 Size=0x4
    void * pExtra;// Offset=0xec Size=0x4
    struct smcolor_t ambientColor;// Offset=0xf0 Size=0xc
    class vec3 startPos;// Offset=0xfc Size=0xc
    void smmodel_t(struct smmodel_t & );
    void smmodel_t();
};

struct smphysics_t// Size=0x60 (Id=3596)
{
    class vec3 pos;// Offset=0x0 Size=0xc
    class vec3 prevPos;// Offset=0xc Size=0xc
    class vec3 vel;// Offset=0x18 Size=0xc
    float yaw;// Offset=0x24 Size=0x4
    float pitch;// Offset=0x28 Size=0x4
    unsigned int flags;// Offset=0x2c Size=0x4
    class mat43 matrix;// Offset=0x30 Size=0x30
    void smphysics_t(struct smphysics_t & );
    void smphysics_t();
};

struct smphysics_t// Size=0x60 (Id=3597)
{
    class vec3 pos;// Offset=0x0 Size=0xc
    class vec3 prevPos;// Offset=0xc Size=0xc
    class vec3 vel;// Offset=0x18 Size=0xc
    float yaw;// Offset=0x24 Size=0x4
    float pitch;// Offset=0x28 Size=0x4
    unsigned int flags;// Offset=0x2c Size=0x4
    class mat43 matrix;// Offset=0x30 Size=0x30
    void smphysics_t(struct smphysics_t & );
    void smphysics_t();
};

struct smwaypoint_t// Size=0x10 (Id=3598)
{
    class vec3 pos;// Offset=0x0 Size=0xc
    unsigned int flags;// Offset=0xc Size=0x4
    void smwaypoint_t(struct smwaypoint_t & );
    void smwaypoint_t();
};

struct smwaypoint_t// Size=0x10 (Id=3599)
{
    class vec3 pos;// Offset=0x0 Size=0xc
    unsigned int flags;// Offset=0xc Size=0x4
    void smwaypoint_t(struct smwaypoint_t & );
    void smwaypoint_t();
};

struct smtexel_t// Size=0x10 (Id=3600)
{
    float r;// Offset=0x0 Size=0x4
    float g;// Offset=0x4 Size=0x4
    float b;// Offset=0x8 Size=0x4
    char fWithin;// Offset=0xc Size=0x1
};

struct smintersection_t// Size=0x18 (Id=3619)
{
    float dist;// Offset=0x0 Size=0x4
    class vec3 p;// Offset=0x4 Size=0xc
    struct smplacement_t * pPlacement;// Offset=0x10 Size=0x4
    int triNum;// Offset=0x14 Size=0x4
    void smintersection_t(struct smintersection_t & );
    void smintersection_t();
};

struct smintersection_t// Size=0x18 (Id=3620)
{
    float dist;// Offset=0x0 Size=0x4
    class vec3 p;// Offset=0x4 Size=0xc
    struct smplacement_t * pPlacement;// Offset=0x10 Size=0x4
    int triNum;// Offset=0x14 Size=0x4
    void smintersection_t(struct smintersection_t & );
    void smintersection_t();
};


#endif // _GAME_H_
