#ifndef _GRAPHICS_H_
#define _GRAPHICS_H_

// Category: graphics
// Extracted from Xbox PDB symbols
// Total types: 42
// Note: Xbox symbols - may need adaptation for GameCube

enum g3dtextureformat
{
    G3DTEX_RGB888=0,
    G3DTEX_RGBA8888=1
};

struct g3dhwtexture// Size=0x8 (Id=1954)
{
    int mNext;// Offset=0x0 Size=0x4
    void * mHWHandle;// Offset=0x4 Size=0x4
};

struct g3dtexturedescriptor// Size=0x8 (Id=1955)
{
    enum g3dtextureformat mFormat;// Offset=0x0 Size=0x4
    unsigned short mWidth;// Offset=0x4 Size=0x2
    unsigned short mHeight;// Offset=0x6 Size=0x2
};

struct nodeanim_s// Size=0x18 (Id=2020)
{
    int startNodeRotKey;// Offset=0x0 Size=0x4
    int nNodeRotKeys;// Offset=0x4 Size=0x4
    int startNodePosKey;// Offset=0x8 Size=0x4
    int nNodePosKeys;// Offset=0xc Size=0x4
    int startNodeSclKey;// Offset=0x10 Size=0x4
    int nNodeSclKeys;// Offset=0x14 Size=0x4
};

struct g3dtexturedescriptor// Size=0x8 (Id=2065)
{
    enum g3dtextureformat mFormat;// Offset=0x0 Size=0x4
    unsigned short mWidth;// Offset=0x4 Size=0x2
    unsigned short mHeight;// Offset=0x6 Size=0x2
};

struct PBGLOBAL_TEXTURE// Size=0x370 (Id=2107)
{
    struct sceGsLoadImage gs_load_image;// Offset=0x0 Size=0x60
    struct sceGsLoadImage gs_load_clut;// Offset=0x60 Size=0x60
    struct sceGsLoadImage gs_load_mipmaps[6];// Offset=0xc0 Size=0x240
    struct PBTEXENV_PACKET gs_tex_env;// Offset=0x300 Size=0x58
    struct ROMTEX * last_tex;// Offset=0x358 Size=0x4
    int mip_k;// Offset=0x35c Size=0x4
    int mip_l;// Offset=0x360 Size=0x4
    int mip_minfilter;// Offset=0x364 Size=0x4
    int mip_magfilter;// Offset=0x368 Size=0x4
};

enum PBMODELSTATE
{
    PBM_READY=0,
    PBM_LOADING_TEX=1,
    PBM_LOADING_GEOM=2,
    PBM_LOADING_BG_TEX=3,
    PBM_LOADING_BG_PTEX=4,
    PBM_LOADING_BG_GEOM=5,
    PBM_LOADING_BG=6,
    PBM_LOADING=7,
    PBM_ALLOCATED=8,
    PBM_ERROR=9,
    PBM_BAD_VERSION=10,
    PBM_LOAD_ERROR=11
};

struct PBGLOBAL_MODEL// Size=0x154 (Id=2135)
{
    int count;// Offset=0x0 Size=0x4
    struct PBMODELINFO info[21];// Offset=0x4 Size=0x150
};

struct MODELHEADER// Size=0xa0 (Id=2190)
{
    char DirName[32];// Offset=0x0 Size=0x20
    char ModelName[32];// Offset=0x20 Size=0x20
    void * Version;// Offset=0x40 Size=0x4
    unsigned int romobj_cnt;// Offset=0x44 Size=0x4
    unsigned int romtex_cnt;// Offset=0x48 Size=0x4
    unsigned int objdef_cnt;// Offset=0x4c Size=0x4
    unsigned int texdef_cnt;// Offset=0x50 Size=0x4
    struct ROMOBJECT * romobjs;// Offset=0x54 Size=0x4
    struct ROMTEX * romtexs;// Offset=0x58 Size=0x4
    struct OBJDEF * objdefs;// Offset=0x5c Size=0x4
    struct TEXDEF * texdefs;// Offset=0x60 Size=0x4
    struct SUBOBJECT * subobjs;// Offset=0x64 Size=0x4
    unsigned int * geom;// Offset=0x68 Size=0x4
    unsigned int * obj_end;// Offset=0x6c Size=0x4
    unsigned int * tex_start;// Offset=0x70 Size=0x4
    unsigned int * tex_end;// Offset=0x74 Size=0x4
    unsigned char * texbits;// Offset=0x78 Size=0x4
    unsigned short lmtex_first;// Offset=0x7c Size=0x2
    unsigned short lmtex_num;// Offset=0x7e Size=0x2
    unsigned int * texinfo;// Offset=0x80 Size=0x4
    unsigned int Unused[7];// Offset=0x84 Size=0x1c
};

struct tanimdata// Size=0x30 (Id=2217)
{
    float tpyr[4];// Offset=0x0 Size=0x10
    float tpos[4];// Offset=0x10 Size=0x10
    float tscale[4];// Offset=0x20 Size=0x10
};

enum ADDANIM_FLAG
{
    ADDANIM_ATTATCH=1
};

class Model_B : public Gob// Size=0x118 (Id=3105)
{
    union // Size=0x5b0 (Id=0)
    {
        unsigned char __align0[48];// Offset=0x0 Size=0x30
        class model_skinned * sm;// Offset=0x30 Size=0x4
        void Model_B(class Model_B & );
        void Model_B();// Offset=0x0 Size=0xc1
        void ModelBuildMatrix(class mat44 & ,class vec3 & );// Offset=0x0 Size=0xb0
        void Initialize(class model_generic * );// Offset=0x0 Size=0x17
        void SkinnedInitialize();// Offset=0x0 Size=0x473
        void SkinnedOptimizeVerts();// Offset=0x0 Size=0x22a
        void SkinnedConstructEdgeTable();// Offset=0x0 Size=0x49d
        char SkinnedAnimateModel(int ,float * ,char ,float );// Offset=0x0 Size=0x13b
        void SkinnedAnimateNode(class mat44 * ,class mat44 * ,struct nodeanim_s * ,const int ,const float );// Offset=0x0 Size=0x179
        void SkinnedAnimateNodeRot(struct nodeanim_s * ,const float ,class mat44 * );// Offset=0x0 Size=0x2e7
        void SkinnedAnimateNodePos(struct nodeanim_s * ,const float ,class mat44 * );// Offset=0x0 Size=0x154
        void SkinnedAnimateNodeScl(struct nodeanim_s * ,const float ,class mat44 * );// Offset=0x0 Size=0x123
        void SkinnedAccumulateVertex(class vec3 * ,float ,class vec4 * );// Offset=0x0 Size=0x4d
        void SkinnedBuildNormals();// Offset=0x0 Size=0x192
        char SkinnedLoad(char * );// Offset=0x0 Size=0x5b0
        void SkinnedRender(class RenderContext & ,unsigned int );
        void SkinnedDrawTriangle(class RenderContext & ,unsigned int );
        float SkinnedEdgePlaneIntersection(int ,class vec4 & ,class vec4 & );// Offset=0x0 Size=0xfb
        char readBlock(class CachedFile & ,unsigned int ,unsigned int * ,void ** ,char );// Offset=0x0 Size=0x179
        class vec3 translation;// Offset=0x34 Size=0xc
        class vec3 rotation;// Offset=0x40 Size=0xc
        class vec3 scaling;// Offset=0x4c Size=0xc
        class mat44 matrix;// Offset=0x58 Size=0x40
        class mat44 rotationMatrix;// Offset=0x98 Size=0x40
        class mat44 scalingMatrix;// Offset=0xd8 Size=0x40
        void updateMatrix();// Offset=0x0 Size=0x2f
        class vec3 & getTranslation();
        class vec3 & getRotation();
        class vec3 & getScaling();
        void setTranslation(class vec3 & );// Offset=0x0 Size=0x8
        void setRotation(class vec3 & );// Offset=0x0 Size=0xf5
        void setScaling(class vec3 & );// Offset=0x0 Size=0x62
        class mat44 & getModelMatrix();// Offset=0x0 Size=0x4
        class model_skinned * gsm;// Offset=0x0 Size=0x4
        void ~Model_B();// Offset=0x0 Size=0x5
        void __local_vftable_ctor_closure();
        void * __vecDelDtor(unsigned int );
    };
};

class Model_B : public Gob// Size=0x118 (Id=3106)
{
    union // Size=0x5b0 (Id=0)
    {
        unsigned char __align0[48];// Offset=0x0 Size=0x30
        class model_skinned * sm;// Offset=0x30 Size=0x4
        void Model_B(class Model_B & );
        void Model_B();// Offset=0x0 Size=0xc1
        void ModelBuildMatrix(class mat44 & ,class vec3 & );// Offset=0x0 Size=0xb0
        void Initialize(class model_generic * );// Offset=0x0 Size=0x17
        void SkinnedInitialize();// Offset=0x0 Size=0x473
        void SkinnedOptimizeVerts();// Offset=0x0 Size=0x22a
        void SkinnedConstructEdgeTable();// Offset=0x0 Size=0x49d
        char SkinnedAnimateModel(int ,float * ,char ,float );// Offset=0x0 Size=0x13b
        void SkinnedAnimateNode(class mat44 * ,class mat44 * ,struct nodeanim_s * ,const int ,const float );// Offset=0x0 Size=0x179
        void SkinnedAnimateNodeRot(struct nodeanim_s * ,const float ,class mat44 * );// Offset=0x0 Size=0x2e7
        void SkinnedAnimateNodePos(struct nodeanim_s * ,const float ,class mat44 * );// Offset=0x0 Size=0x154
        void SkinnedAnimateNodeScl(struct nodeanim_s * ,const float ,class mat44 * );// Offset=0x0 Size=0x123
        void SkinnedAccumulateVertex(class vec3 * ,float ,class vec4 * );// Offset=0x0 Size=0x4d
        void SkinnedBuildNormals();// Offset=0x0 Size=0x192
        char SkinnedLoad(char * );// Offset=0x0 Size=0x5b0
        void SkinnedRender(class RenderContext & ,unsigned int );
        void SkinnedDrawTriangle(class RenderContext & ,unsigned int );
        float SkinnedEdgePlaneIntersection(int ,class vec4 & ,class vec4 & );// Offset=0x0 Size=0xfb
        char readBlock(class CachedFile & ,unsigned int ,unsigned int * ,void ** ,char );// Offset=0x0 Size=0x179
        class vec3 translation;// Offset=0x34 Size=0xc
        class vec3 rotation;// Offset=0x40 Size=0xc
        class vec3 scaling;// Offset=0x4c Size=0xc
        class mat44 matrix;// Offset=0x58 Size=0x40
        class mat44 rotationMatrix;// Offset=0x98 Size=0x40
        class mat44 scalingMatrix;// Offset=0xd8 Size=0x40
        void updateMatrix();// Offset=0x0 Size=0x2f
        class vec3 & getTranslation();
        class vec3 & getRotation();
        class vec3 & getScaling();
        void setTranslation(class vec3 & );// Offset=0x0 Size=0x8
        void setRotation(class vec3 & );// Offset=0x0 Size=0xf5
        void setScaling(class vec3 & );// Offset=0x0 Size=0x62
        class mat44 & getModelMatrix();// Offset=0x0 Size=0x4
        class model_skinned * gsm;// Offset=0x0 Size=0x4
        void ~Model_B();// Offset=0x0 Size=0x5
        void __local_vftable_ctor_closure();
        void * __vecDelDtor(unsigned int );
    };
};

class model_skinned : public model_generic// Size=0x14c (Id=3109)
{
    union // Size=0x14c (Id=0)
    {
        unsigned char __align0[4];// Offset=0x0 Size=0x4
        char name[32];// Offset=0x4 Size=0x20
        unsigned int flags;// Offset=0x24 Size=0x4
        struct node_s * pNodes;// Offset=0x28 Size=0x4
        unsigned int nNodes;// Offset=0x2c Size=0x4
        class vec4 * pVerts;// Offset=0x30 Size=0x4
        class vec4 * pTransformedVerts;// Offset=0x34 Size=0x4
        unsigned int nVerts;// Offset=0x38 Size=0x4
        class vec2 * pTexCoords;// Offset=0x3c Size=0x4
        unsigned int nTexCoords;// Offset=0x40 Size=0x4
        struct tri_s * pTris;// Offset=0x44 Size=0x4
        unsigned int nTris;// Offset=0x48 Size=0x4
        struct triset_s * pTriSets;// Offset=0x4c Size=0x4
        unsigned int nTriSets;// Offset=0x50 Size=0x4
        struct vertinf_s * pVertInfluences;// Offset=0x54 Size=0x4
        unsigned int nVertInfluences;// Offset=0x58 Size=0x4
        struct texture_s * pTextures;// Offset=0x5c Size=0x4
        unsigned int nTextures;// Offset=0x60 Size=0x4
        struct noderotkey_s * pNodeRotKeys;// Offset=0x64 Size=0x4
        unsigned int nNodeRotKeys;// Offset=0x68 Size=0x4
        struct nodeposkey_s * pNodePosKeys;// Offset=0x6c Size=0x4
        unsigned int nNodePosKeys;// Offset=0x70 Size=0x4
        struct nodesclkey_s * pNodeSclKeys;// Offset=0x74 Size=0x4
        unsigned int nNodeSclKeys;// Offset=0x78 Size=0x4
        struct nodeanim_s * pNodeAnims;// Offset=0x7c Size=0x4
        unsigned int nNodeAnims;// Offset=0x80 Size=0x4
        struct modelanim_s * pModelAnims;// Offset=0x84 Size=0x4
        unsigned int nModelAnims;// Offset=0x88 Size=0x4
        unsigned int * pTriEdges;// Offset=0x8c Size=0x4
        unsigned int nEdges;// Offset=0x90 Size=0x4
        struct edge_s * pEdges;// Offset=0x94 Size=0x4
        char bHasStrips;// Offset=0x98 Size=0x1
        unsigned char __align1[3];// Offset=0x99 Size=0x3
        unsigned int nUnique;// Offset=0x9c Size=0x4
        unsigned int * pUnique;// Offset=0xa0 Size=0x4
        unsigned int nPairs;// Offset=0xa4 Size=0x4
        struct vertpair_s * pPairs;// Offset=0xa8 Size=0x4
        struct lod_s lod[4];// Offset=0xac Size=0x20
        int nSwitchModels;// Offset=0xcc Size=0x4
        struct switchmodel_s * pSwitchModels;// Offset=0xd0 Size=0x4
        int nSwitchAnims;// Offset=0xd4 Size=0x4
        struct switchnodeanim_s * pSwitchAnims;// Offset=0xd8 Size=0x4
        int nSwitchKeys;// Offset=0xdc Size=0x4
        struct switchnodekey_s * pSwitchKeys;// Offset=0xe0 Size=0x4
        unsigned int textureId;// Offset=0xe4 Size=0x4
        int cSM;// Offset=0xe8 Size=0x4
        int startNode;// Offset=0xec Size=0x4
        int totalNodes;// Offset=0xf0 Size=0x4
        int totalVerts;// Offset=0xf4 Size=0x4
        int startTri;// Offset=0xf8 Size=0x4
        int totalTris;// Offset=0xfc Size=0x4
        class vec3 min;// Offset=0x100 Size=0xc
        class vec3 max;// Offset=0x10c Size=0xc
        class mat44 * pRelMatrices;// Offset=0x118 Size=0x4
        class mat44 * pAbsMatrices;// Offset=0x11c Size=0x4
        class vec4 * pFaceNormals;// Offset=0x120 Size=0x4
        class vec4 * pVertNormals;// Offset=0x124 Size=0x4
        struct color_s * pLightVals;// Offset=0x128 Size=0x4
        class vec2 * pTexCoords2;// Offset=0x12c Size=0x4
        struct color_s * pSpecularVals;// Offset=0x130 Size=0x4
        class mat44 * pQuatMatrices;// Offset=0x134 Size=0x4
        class quat * pFromQuats;// Offset=0x138 Size=0x4
        class vec4 * pFromPos;// Offset=0x13c Size=0x4
        class quat * pToQuats;// Offset=0x140 Size=0x4
        class vec4 * pToPos;// Offset=0x144 Size=0x4
        unsigned int * pVertOutcodes;// Offset=0x148 Size=0x4
        void model_skinned(class model_skinned & );
        void model_skinned();// Offset=0x0 Size=0x1d
    };
};

class model_skinned : public model_generic// Size=0x14c (Id=3110)
{
    union // Size=0x14c (Id=0)
    {
        unsigned char __align0[4];// Offset=0x0 Size=0x4
        char name[32];// Offset=0x4 Size=0x20
        unsigned int flags;// Offset=0x24 Size=0x4
        struct node_s * pNodes;// Offset=0x28 Size=0x4
        unsigned int nNodes;// Offset=0x2c Size=0x4
        class vec4 * pVerts;// Offset=0x30 Size=0x4
        class vec4 * pTransformedVerts;// Offset=0x34 Size=0x4
        unsigned int nVerts;// Offset=0x38 Size=0x4
        class vec2 * pTexCoords;// Offset=0x3c Size=0x4
        unsigned int nTexCoords;// Offset=0x40 Size=0x4
        struct tri_s * pTris;// Offset=0x44 Size=0x4
        unsigned int nTris;// Offset=0x48 Size=0x4
        struct triset_s * pTriSets;// Offset=0x4c Size=0x4
        unsigned int nTriSets;// Offset=0x50 Size=0x4
        struct vertinf_s * pVertInfluences;// Offset=0x54 Size=0x4
        unsigned int nVertInfluences;// Offset=0x58 Size=0x4
        struct texture_s * pTextures;// Offset=0x5c Size=0x4
        unsigned int nTextures;// Offset=0x60 Size=0x4
        struct noderotkey_s * pNodeRotKeys;// Offset=0x64 Size=0x4
        unsigned int nNodeRotKeys;// Offset=0x68 Size=0x4
        struct nodeposkey_s * pNodePosKeys;// Offset=0x6c Size=0x4
        unsigned int nNodePosKeys;// Offset=0x70 Size=0x4
        struct nodesclkey_s * pNodeSclKeys;// Offset=0x74 Size=0x4
        unsigned int nNodeSclKeys;// Offset=0x78 Size=0x4
        struct nodeanim_s * pNodeAnims;// Offset=0x7c Size=0x4
        unsigned int nNodeAnims;// Offset=0x80 Size=0x4
        struct modelanim_s * pModelAnims;// Offset=0x84 Size=0x4
        unsigned int nModelAnims;// Offset=0x88 Size=0x4
        unsigned int * pTriEdges;// Offset=0x8c Size=0x4
        unsigned int nEdges;// Offset=0x90 Size=0x4
        struct edge_s * pEdges;// Offset=0x94 Size=0x4
        char bHasStrips;// Offset=0x98 Size=0x1
        unsigned char __align1[3];// Offset=0x99 Size=0x3
        unsigned int nUnique;// Offset=0x9c Size=0x4
        unsigned int * pUnique;// Offset=0xa0 Size=0x4
        unsigned int nPairs;// Offset=0xa4 Size=0x4
        struct vertpair_s * pPairs;// Offset=0xa8 Size=0x4
        struct lod_s lod[4];// Offset=0xac Size=0x20
        int nSwitchModels;// Offset=0xcc Size=0x4
        struct switchmodel_s * pSwitchModels;// Offset=0xd0 Size=0x4
        int nSwitchAnims;// Offset=0xd4 Size=0x4
        struct switchnodeanim_s * pSwitchAnims;// Offset=0xd8 Size=0x4
        int nSwitchKeys;// Offset=0xdc Size=0x4
        struct switchnodekey_s * pSwitchKeys;// Offset=0xe0 Size=0x4
        unsigned int textureId;// Offset=0xe4 Size=0x4
        int cSM;// Offset=0xe8 Size=0x4
        int startNode;// Offset=0xec Size=0x4
        int totalNodes;// Offset=0xf0 Size=0x4
        int totalVerts;// Offset=0xf4 Size=0x4
        int startTri;// Offset=0xf8 Size=0x4
        int totalTris;// Offset=0xfc Size=0x4
        class vec3 min;// Offset=0x100 Size=0xc
        class vec3 max;// Offset=0x10c Size=0xc
        class mat44 * pRelMatrices;// Offset=0x118 Size=0x4
        class mat44 * pAbsMatrices;// Offset=0x11c Size=0x4
        class vec4 * pFaceNormals;// Offset=0x120 Size=0x4
        class vec4 * pVertNormals;// Offset=0x124 Size=0x4
        struct color_s * pLightVals;// Offset=0x128 Size=0x4
        class vec2 * pTexCoords2;// Offset=0x12c Size=0x4
        struct color_s * pSpecularVals;// Offset=0x130 Size=0x4
        class mat44 * pQuatMatrices;// Offset=0x134 Size=0x4
        class quat * pFromQuats;// Offset=0x138 Size=0x4
        class vec4 * pFromPos;// Offset=0x13c Size=0x4
        class quat * pToQuats;// Offset=0x140 Size=0x4
        class vec4 * pToPos;// Offset=0x144 Size=0x4
        unsigned int * pVertOutcodes;// Offset=0x148 Size=0x4
        void model_skinned(class model_skinned & );
        void model_skinned();// Offset=0x0 Size=0x1d
    };
};

class model_generic// Size=0x4 (Id=3111)
{
    public unsigned int size;// Offset=0x0 Size=0x4
};

struct nodeanim_s// Size=0x18 (Id=3112)
{
    int startNodeRotKey;// Offset=0x0 Size=0x4
    int nNodeRotKeys;// Offset=0x4 Size=0x4
    int startNodePosKey;// Offset=0x8 Size=0x4
    int nNodePosKeys;// Offset=0xc Size=0x4
    int startNodeSclKey;// Offset=0x10 Size=0x4
    int nNodeSclKeys;// Offset=0x14 Size=0x4
};

struct g3dhwtexture// Size=0x8 (Id=3142)
{
    int mNext;// Offset=0x0 Size=0x4
    void * mHWHandle;// Offset=0x4 Size=0x4
};

struct texture_s// Size=0x40 (Id=3157)
{
    char name[32];// Offset=0x0 Size=0x20
    int width;// Offset=0x20 Size=0x4
    int height;// Offset=0x24 Size=0x4
    char bpp;// Offset=0x28 Size=0x1
    char rBits;// Offset=0x29 Size=0x1
    char gBits;// Offset=0x2a Size=0x1
    char bBits;// Offset=0x2b Size=0x1
    char aBits;// Offset=0x2c Size=0x1
    unsigned char __align0[3];// Offset=0x2d Size=0x3
    unsigned int flags;// Offset=0x30 Size=0x4
    unsigned char * pData;// Offset=0x34 Size=0x4
    int hwid;// Offset=0x38 Size=0x4
    void * pExtra;// Offset=0x3c Size=0x4
};

struct modelanim_s// Size=0x30 (Id=3164)
{
    char name[32];// Offset=0x0 Size=0x20
    float period;// Offset=0x20 Size=0x4
    int startNodeAnim;// Offset=0x24 Size=0x4
    int startEventKey;// Offset=0x28 Size=0x4
    int nEventKeys;// Offset=0x2c Size=0x4
};

struct switchmodel_s// Size=0x58 (Id=3169)
{
    int startNode;// Offset=0x0 Size=0x4
    int nNodes;// Offset=0x4 Size=0x4
    int startVert;// Offset=0x8 Size=0x4
    int nVerts;// Offset=0xc Size=0x4
    int startTexCoord;// Offset=0x10 Size=0x4
    int nTexCoords;// Offset=0x14 Size=0x4
    int startVertInf;// Offset=0x18 Size=0x4
    int nVertInf;// Offset=0x1c Size=0x4
    int startTriSet;// Offset=0x20 Size=0x4
    int nTriSets;// Offset=0x24 Size=0x4
    int startTexture;// Offset=0x28 Size=0x4
    int nTextures;// Offset=0x2c Size=0x4
    int startTri;// Offset=0x30 Size=0x4
    int nTris;// Offset=0x34 Size=0x4
    struct lod_s lod[4];// Offset=0x38 Size=0x20
};

struct switchnodeanim_s// Size=0x60 (Id=3170)
{
    char name[32];// Offset=0x0 Size=0x20
    char wavename[32];// Offset=0x20 Size=0x20
    int wavestart;// Offset=0x40 Size=0x4
    int waveend;// Offset=0x44 Size=0x4
    float period;// Offset=0x48 Size=0x4
    int startKey;// Offset=0x4c Size=0x4
    int nKeys;// Offset=0x50 Size=0x4
    unsigned int flags;// Offset=0x54 Size=0x4
    int lastKey;// Offset=0x58 Size=0x4
    void * pExtra;// Offset=0x5c Size=0x4
};

struct modeltexture_s// Size=0x38 (Id=3196)
{
    char name[32];// Offset=0x0 Size=0x20
    int width;// Offset=0x20 Size=0x4
    int height;// Offset=0x24 Size=0x4
    int bpp;// Offset=0x28 Size=0x4
    int fHasAlpha;// Offset=0x2c Size=0x4
    unsigned char * pData;// Offset=0x30 Size=0x4
    void * pExtra;// Offset=0x34 Size=0x4
};

class Model_P : public Model_B// Size=0x118 (Id=3213)
{
    union // Size=0x12 (Id=0)
    {
        void SkinnedRender(class RenderContext & ,unsigned int );// Offset=0x0 Size=0x3
        void SkinnedDrawTriangle(class RenderContext & ,unsigned int );// Offset=0x0 Size=0x3
        void Model_P(class Model_P & );
        void Model_P();// Offset=0x0 Size=0x12
        void ~Model_P();// Offset=0x0 Size=0x5
        void __local_vftable_ctor_closure();
        void * __vecDelDtor(unsigned int );
    };
};

class Model_P : public Model_B// Size=0x118 (Id=3214)
{
    union // Size=0x12 (Id=0)
    {
        void SkinnedRender(class RenderContext & ,unsigned int );// Offset=0x0 Size=0x3
        void SkinnedDrawTriangle(class RenderContext & ,unsigned int );// Offset=0x0 Size=0x3
        void Model_P(class Model_P & );
        void Model_P();// Offset=0x0 Size=0x12
        void ~Model_P();// Offset=0x0 Size=0x5
        void __local_vftable_ctor_closure();
        void * __vecDelDtor(unsigned int );
    };
};

class Model : public Model_P// Size=0x118 (Id=3224)
{
    public void Model(class Model & );
    union // Size=0x12 (Id=0)
    {
        void Model();// Offset=0x0 Size=0x12
        void ~Model();// Offset=0x0 Size=0x5
        void __local_vftable_ctor_closure();
        void * __vecDelDtor(unsigned int );
    };
};

class Model : public Model_P// Size=0x118 (Id=3225)
{
    public void Model(class Model & );
    union // Size=0x12 (Id=0)
    {
        void Model();// Offset=0x0 Size=0x12
        void ~Model();// Offset=0x0 Size=0x5
        void __local_vftable_ctor_closure();
        void * __vecDelDtor(unsigned int );
    };
};

struct ACTIONANIM// Size=0x8 (Id=3247)
{
    int animidx;// Offset=0x0 Size=0x4
    int nframes;// Offset=0x4 Size=0x4
};

struct animinfo// Size=0x38 (Id=3256)
{
    struct atreeseq * seqheader;// Offset=0x0 Size=0x4
    struct animheader * animheader;// Offset=0x4 Size=0x4
    struct objanimheader * oanimheader;// Offset=0x8 Size=0x4
    short numseqs;// Offset=0xc Size=0x2
    short animseq;// Offset=0xe Size=0x2
    short numframes;// Offset=0x10 Size=0x2
    char setpanim;// Offset=0x12 Size=0x1
    unsigned char flags;// Offset=0x13 Size=0x1
    float transfrac;// Offset=0x14 Size=0x4
    float frame;// Offset=0x18 Size=0x4
    short animseq0;// Offset=0x1c Size=0x2
    short active;// Offset=0x1e Size=0x2
    float starttime;// Offset=0x20 Size=0x4
    float transtime;// Offset=0x24 Size=0x4
    float animscale;// Offset=0x28 Size=0x4
    float seqscale;// Offset=0x2c Size=0x4
    float atime;// Offset=0x30 Size=0x4
    short repeat;// Offset=0x34 Size=0x2
    unsigned short stage;// Offset=0x36 Size=0x2
};

struct animheader// Size=0x1c (Id=3263)
{
    float * CompressAng;// Offset=0x0 Size=0x4
    float * CompressPos;// Offset=0x4 Size=0x4
    float * CompressUnit;// Offset=0x8 Size=0x4
    struct anim * blocks;// Offset=0xc Size=0x4
    struct animseqinfo * seqs;// Offset=0x10 Size=0x4
    int numseqs;// Offset=0x14 Size=0x4
    int numobjs;// Offset=0x18 Size=0x4
};

struct objanimheader// Size=0x8 (Id=3264)
{
    int oanims;// Offset=0x0 Size=0x4
    int numoanims;// Offset=0x4 Size=0x4
};

struct animdata// Size=0xa0 (Id=3275)
{
    struct animseqinfo * seq;// Offset=0x0 Size=0x4
    int used;// Offset=0x4 Size=0x4
    short pidx;// Offset=0x8 Size=0x2
    short nidx;// Offset=0xa Size=0x2
    int keycount;// Offset=0xc Size=0x4
    float ppyr[4];// Offset=0x10 Size=0x10
    float npyr[4];// Offset=0x20 Size=0x10
    float xpyr[4];// Offset=0x30 Size=0x10
    float ppos[4];// Offset=0x40 Size=0x10
    float npos[4];// Offset=0x50 Size=0x10
    float xpos[4];// Offset=0x60 Size=0x10
    float pscale[4];// Offset=0x70 Size=0x10
    float nscale[4];// Offset=0x80 Size=0x10
    float xscale[4];// Offset=0x90 Size=0x10
};

struct animseqinfo// Size=0x8 (Id=3276)
{
    unsigned short type;// Offset=0x0 Size=0x2
    unsigned short size;// Offset=0x2 Size=0x2
    unsigned int animidx;// Offset=0x4 Size=0x4
};

struct anim// Size=0x4 (Id=3280)
{
    union __unnamed x;// Offset=0x0 Size=0x4
};

struct objanim// Size=0x28 (Id=3286)
{
    char mbdesc[32];// Offset=0x0 Size=0x20
    int mbidx;// Offset=0x20 Size=0x4
    short nframes;// Offset=0x24 Size=0x2
    short startframe;// Offset=0x26 Size=0x2
};

struct worldanim// Size=0x10 (Id=3316)
{
    short wobjidx;// Offset=0x0 Size=0x2
    short nframes;// Offset=0x2 Size=0x2
    unsigned short flags;// Offset=0x4 Size=0x2
    short state;// Offset=0x6 Size=0x2
    float frame;// Offset=0x8 Size=0x4
    struct animseqinfo * seqinfo;// Offset=0xc Size=0x4
};

struct crit_animinst// Size=0x54 (Id=3319)
{
    struct atree atree;// Offset=0x0 Size=0x48
    struct mbnode * root;// Offset=0x48 Size=0x4
    int seqblockidx;// Offset=0x4c Size=0x4
    struct crit_animinst * next;// Offset=0x50 Size=0x4
};

struct modelinfo// Size=0x18 (Id=3337)
{
    struct fileinfo * objects;// Offset=0x0 Size=0x4
    struct fileinfo * textures;// Offset=0x4 Size=0x4
    struct fileinfo * lightmaps;// Offset=0x8 Size=0x4
    int state;// Offset=0xc Size=0x4
    int done;// Offset=0x10 Size=0x4
    int counter;// Offset=0x14 Size=0x4
};

struct PBMODELINFO// Size=0x10 (Id=3492)
{
    struct MODELHEADER * header;// Offset=0x0 Size=0x4
    unsigned int geo_size;// Offset=0x4 Size=0x4
    unsigned int tex_size;// Offset=0x8 Size=0x4
    enum PBMODELSTATE unready;// Offset=0xc Size=0x4
};

struct model_s// Size=0x0 (Id=3593)
{
};

struct otexture_s// Size=0x40 (Id=3601)
{
    char name[32];// Offset=0x0 Size=0x20
    int width;// Offset=0x20 Size=0x4
    int height;// Offset=0x24 Size=0x4
    char bpp;// Offset=0x28 Size=0x1
    char rBits;// Offset=0x29 Size=0x1
    char gBits;// Offset=0x2a Size=0x1
    char bBits;// Offset=0x2b Size=0x1
    char aBits;// Offset=0x2c Size=0x1
    unsigned char __align0[3];// Offset=0x2d Size=0x3
    unsigned int flags;// Offset=0x30 Size=0x4
    unsigned char * pData;// Offset=0x34 Size=0x4
    int hwid;// Offset=0x38 Size=0x4
    void * pExtra;// Offset=0x3c Size=0x4
};

struct crit_addanim// Size=0x30 (Id=3607)
{
    short addtoidx;// Offset=0x0 Size=0x2
    short flags;// Offset=0x2 Size=0x2
    struct atreeheader * atreehdr;// Offset=0x4 Size=0x4
    struct crit_addanim * next;// Offset=0x8 Size=0x4
    unsigned int dummy1;// Offset=0xc Size=0x4
    char desc[8];// Offset=0x10 Size=0x8
    char node[8];// Offset=0x18 Size=0x8
    float offset[3];// Offset=0x20 Size=0xc
    float dummy2;// Offset=0x2c Size=0x4
};


#endif // _GRAPHICS_H_
