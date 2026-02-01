#ifndef _D3D_H_
#define _D3D_H_

// Category: d3d
// Extracted from Xbox PDB symbols
// Total types: 381
// Note: Xbox symbols - may need adaptation for GameCube

enum PS_COMBINEROUTPUT
{
    PS_COMBINEROUTPUT_IDENTITY=0,
    PS_COMBINEROUTPUT_BIAS=8,
    PS_COMBINEROUTPUT_SHIFTLEFT_1=16,
    PS_COMBINEROUTPUT_SHIFTLEFT_1_BIAS=24,
    PS_COMBINEROUTPUT_SHIFTLEFT_2=32,
    PS_COMBINEROUTPUT_SHIFTRIGHT_1=48,
    PS_COMBINEROUTPUT_AB_BLUE_TO_ALPHA=128,
    PS_COMBINEROUTPUT_CD_BLUE_TO_ALPHA=64,
    PS_COMBINEROUTPUT_AB_MULTIPLY=0,
    PS_COMBINEROUTPUT_AB_DOT_PRODUCT=2,
    PS_COMBINEROUTPUT_CD_MULTIPLY=0,
    PS_COMBINEROUTPUT_CD_DOT_PRODUCT=1,
    PS_COMBINEROUTPUT_AB_CD_SUM=0,
    PS_COMBINEROUTPUT_AB_CD_MUX=4
};

enum _D3DXMESH
{
    D3DXMESH_32BIT=1,
    D3DXMESH_DONOTCLIP=2,
    D3DXMESH_POINTS=4,
    D3DXMESH_RTPATCHES=8,
    D3DXMESH_NPATCHES=16384,
    D3DXMESH_VB_SYSTEMMEM=16,
    D3DXMESH_VB_MANAGED=32,
    D3DXMESH_VB_WRITEONLY=64,
    D3DXMESH_VB_DYNAMIC=128,
    D3DXMESH_IB_SYSTEMMEM=256,
    D3DXMESH_IB_MANAGED=512,
    D3DXMESH_IB_WRITEONLY=1024,
    D3DXMESH_IB_DYNAMIC=2048,
    D3DXMESH_VB_SHARE=4096,
    D3DXMESH_USEHWONLY=8192,
    D3DXMESH_SYSTEMMEM=272,
    D3DXMESH_MANAGED=544,
    D3DXMESH_WRITEONLY=1088,
    D3DXMESH_DYNAMIC=2176
};

enum PS_GLOBALFLAGS
{
    PS_GLOBALFLAGS_NO_TEXMODE_ADJUST=0,
    PS_GLOBALFLAGS_TEXMODE_ADJUST=1
};

enum _D3DLIGHTTYPE
{
    D3DLIGHT_POINT=1,
    D3DLIGHT_SPOT=2,
    D3DLIGHT_DIRECTIONAL=3,
    D3DLIGHT_FORCE_DWORD=2147483647
};

enum _D3DXMESHSIMP
{
    D3DXMESHSIMP_VERTEX=1,
    D3DXMESHSIMP_FACE=2
};

enum _D3DSHADEMODE
{
    D3DSHADE_FLAT=7424,
    D3DSHADE_GOURAUD=7425,
    D3DSHADE_FORCE_DWORD=2147483647
};

enum _D3DFILLMODE
{
    D3DFILL_POINT=6912,
    D3DFILL_WIREFRAME=6913,
    D3DFILL_SOLID=6914,
    D3DFILL_FORCE_DWORD=2147483647
};

enum _D3DBLEND
{
    D3DBLEND_ZERO=0,
    D3DBLEND_ONE=1,
    D3DBLEND_SRCCOLOR=768,
    D3DBLEND_INVSRCCOLOR=769,
    D3DBLEND_SRCALPHA=770,
    D3DBLEND_INVSRCALPHA=771,
    D3DBLEND_DESTALPHA=772,
    D3DBLEND_INVDESTALPHA=773,
    D3DBLEND_DESTCOLOR=774,
    D3DBLEND_INVDESTCOLOR=775,
    D3DBLEND_SRCALPHASAT=776,
    D3DBLEND_CONSTANTCOLOR=32769,
    D3DBLEND_INVCONSTANTCOLOR=32770,
    D3DBLEND_CONSTANTALPHA=32771,
    D3DBLEND_INVCONSTANTALPHA=32772,
    D3DBLEND_FORCE_DWORD=2147483647
};

enum _D3DXERR
{
    D3DXERR_CANNOTMODIFYINDEXBUFFER=-2005529772,
    D3DXERR_INVALIDMESH=-2005529771,
    D3DXERR_CANNOTATTRSORT=-2005529770,
    D3DXERR_SKINNINGNOTSUPPORTED=-2005529769,
    D3DXERR_TOOMANYINFLUENCES=-2005529768,
    D3DXERR_INVALIDDATA=-2005529767
};

enum _D3DSTATEBLOCKDIRTYINDEX
{
    D3DSBD_TEXTURES=0,
    D3DSBD_PIXELSHADER=4,
    D3DSBD_VERTEXSHADER=5,
    D3DSBD_INDICES=6,
    D3DSBD_STREAMS=7,
    D3DSBD_PIXELSHADERCONSTANTS=23,
    D3DSBD_VERTEXSHADERCONSTANTS=39,
    D3DSBD_RENDERSTATES=231,
    D3DSBD_TEXTURESTATES=377,
    D3DSBD_TRANSFORMS=505,
    D3DSBD_VIEWPORT=515,
    D3DSBD_MATERIAL=516,
    D3DSBD_BACKMATERIAL=517,
    D3DSBD_MAX=518,
    D3DSBD_FORCE_DWORD=2147483647
};

enum _D3DBLENDOP
{
    D3DBLENDOP_ADD=32774,
    D3DBLENDOP_SUBTRACT=32778,
    D3DBLENDOP_REVSUBTRACT=32779,
    D3DBLENDOP_MIN=32775,
    D3DBLENDOP_MAX=32776,
    D3DBLENDOP_ADDSIGNED=61446,
    D3DBLENDOP_REVSUBTRACTSIGNED=61445,
    D3DBLENDOP_FORCE_DWORD=2147483647
};

enum _D3DTEXTUREADDRESS
{
    D3DTADDRESS_WRAP=1,
    D3DTADDRESS_MIRROR=2,
    D3DTADDRESS_CLAMP=3,
    D3DTADDRESS_BORDER=4,
    D3DTADDRESS_CLAMPTOEDGE=5,
    D3DTADDRESS_MAX=6,
    D3DTADDRESS_FORCE_DWORD=2147483647
};

enum _D3DTEXTURECOLORKEYOP
{
    D3DTCOLORKEYOP_DISABLE=0,
    D3DTCOLORKEYOP_ALPHA=1,
    D3DTCOLORKEYOP_RGBA=2,
    D3DTCOLORKEYOP_KILL=3,
    D3DTCOLORKEYOP_MAX=4,
    D3DTCOLORKEYOP_FORCE_DWORD=2147483647
};

enum _D3DTEXTUREALPHAKILL
{
    D3DTALPHAKILL_DISABLE=0,
    D3DTALPHAKILL_ENABLE=4,
    D3DTALPHAKILL_FORCE_DWORD=2147483647
};

enum _D3DCULL
{
    D3DCULL_NONE=0,
    D3DCULL_CW=2304,
    D3DCULL_CCW=2305,
    D3DCULL_FORCE_DWORD=2147483647
};

enum _D3DFRONT
{
    D3DFRONT_CW=2304,
    D3DFRONT_CCW=2305,
    D3DFRONT_FORCE_DWORD=2147483647
};

enum _D3DCMPFUNC
{
    D3DCMP_NEVER=512,
    D3DCMP_LESS=513,
    D3DCMP_EQUAL=514,
    D3DCMP_LESSEQUAL=515,
    D3DCMP_GREATER=516,
    D3DCMP_NOTEQUAL=517,
    D3DCMP_GREATEREQUAL=518,
    D3DCMP_ALWAYS=519,
    D3DCMP_FORCE_DWORD=2147483647
};

enum _D3DSTENCILOP
{
    D3DSTENCILOP_KEEP=7680,
    D3DSTENCILOP_ZERO=0,
    D3DSTENCILOP_REPLACE=7681,
    D3DSTENCILOP_INCRSAT=7682,
    D3DSTENCILOP_DECRSAT=7683,
    D3DSTENCILOP_INVERT=5386,
    D3DSTENCILOP_INCR=34055,
    D3DSTENCILOP_DECR=34056,
    D3DSTENCILOP_FORCE_DWORD=2147483647
};

enum _D3DFOGMODE
{
    D3DFOG_NONE=0,
    D3DFOG_EXP=1,
    D3DFOG_EXP2=2,
    D3DFOG_LINEAR=3,
    D3DFOG_FORCE_DWORD=2147483647
};

enum _D3DSWATHWIDTH
{
    D3DSWATH_8=0,
    D3DSWATH_16=1,
    D3DSWATH_32=2,
    D3DSWATH_64=3,
    D3DSWATH_128=4,
    D3DSWATH_OFF=15,
    D3DSWATH_FORCE_DWORD=2147483647
};

enum _D3DPALETTESIZE
{
    D3DPALETTE_256=0,
    D3DPALETTE_128=1,
    D3DPALETTE_64=2,
    D3DPALETTE_32=3,
    D3DPALETTE_MAX=4,
    D3DPALETTE_FORCE_DWORD=2147483647
};

enum _D3DLOGICOP
{
    D3DLOGICOP_NONE=0,
    D3DLOGICOP_CLEAR=5376,
    D3DLOGICOP_AND=5377,
    D3DLOGICOP_AND_REVERSE=5378,
    D3DLOGICOP_COPY=5379,
    D3DLOGICOP_AND_INVERTED=5380,
    D3DLOGICOP_NOOP=5381,
    D3DLOGICOP_XOR=5382,
    D3DLOGICOP_OR=5383,
    D3DLOGICOP_NOR=5384,
    D3DLOGICOP_EQUIV=5385,
    D3DLOGICOP_INVERT=5386,
    D3DLOGICOP_OR_REVERSE=5387,
    D3DLOGICOP_COPY_INVERTED=5388,
    D3DLOGICOP_OR_INVERTED=5389,
    D3DLOGICOP_NAND=5390,
    D3DLOGICOP_SET=5391,
    D3DLOGICOP_FORCE_DWORD=2147483647
};

enum _D3DXPARAMETERTYPE
{
    D3DXPT_DWORD=0,
    D3DXPT_FLOAT=1,
    D3DXPT_VECTOR=2,
    D3DXPT_MATRIX=3,
    D3DXPT_TEXTURE=4,
    D3DXPT_VERTEXSHADER=5,
    D3DXPT_PIXELSHADER=6,
    D3DXPT_CONSTANT=7,
    D3DXPT_FORCE_DWORD=2147483647
};

enum _D3DZBUFFERTYPE
{
    D3DZB_FALSE=0,
    D3DZB_TRUE=1,
    D3DZB_USEW=2,
    D3DZB_FORCE_DWORD=2147483647
};

enum _D3DPRIMITIVETYPE
{
    D3DPT_POINTLIST=1,
    D3DPT_LINELIST=2,
    D3DPT_LINELOOP=3,
    D3DPT_LINESTRIP=4,
    D3DPT_TRIANGLELIST=5,
    D3DPT_TRIANGLESTRIP=6,
    D3DPT_TRIANGLEFAN=7,
    D3DPT_QUADLIST=8,
    D3DPT_QUADSTRIP=9,
    D3DPT_POLYGON=10,
    D3DPT_MAX=11,
    D3DPT_FORCE_DWORD=2147483647
};

enum _D3DTRANSFORMSTATETYPE
{
    D3DTS_VIEW=0,
    D3DTS_PROJECTION=1,
    D3DTS_TEXTURE0=2,
    D3DTS_TEXTURE1=3,
    D3DTS_TEXTURE2=4,
    D3DTS_TEXTURE3=5,
    D3DTS_WORLD=6,
    D3DTS_WORLD1=7,
    D3DTS_WORLD2=8,
    D3DTS_WORLD3=9,
    D3DTS_MAX=10,
    D3DTS_FORCE_DWORD=2147483647
};

enum _D3DRENDERSTATETYPE
{
    D3DRS_PS_MIN=0,
    D3DRS_PSALPHAINPUTS0=0,
    D3DRS_PSALPHAINPUTS1=1,
    D3DRS_PSALPHAINPUTS2=2,
    D3DRS_PSALPHAINPUTS3=3,
    D3DRS_PSALPHAINPUTS4=4,
    D3DRS_PSALPHAINPUTS5=5,
    D3DRS_PSALPHAINPUTS6=6,
    D3DRS_PSALPHAINPUTS7=7,
    D3DRS_PSFINALCOMBINERINPUTSABCD=8,
    D3DRS_PSFINALCOMBINERINPUTSEFG=9,
    D3DRS_PSCONSTANT0_0=10,
    D3DRS_PSCONSTANT0_1=11,
    D3DRS_PSCONSTANT0_2=12,
    D3DRS_PSCONSTANT0_3=13,
    D3DRS_PSCONSTANT0_4=14,
    D3DRS_PSCONSTANT0_5=15,
    D3DRS_PSCONSTANT0_6=16,
    D3DRS_PSCONSTANT0_7=17,
    D3DRS_PSCONSTANT1_0=18,
    D3DRS_PSCONSTANT1_1=19,
    D3DRS_PSCONSTANT1_2=20,
    D3DRS_PSCONSTANT1_3=21,
    D3DRS_PSCONSTANT1_4=22,
    D3DRS_PSCONSTANT1_5=23,
    D3DRS_PSCONSTANT1_6=24,
    D3DRS_PSCONSTANT1_7=25,
    D3DRS_PSALPHAOUTPUTS0=26,
    D3DRS_PSALPHAOUTPUTS1=27,
    D3DRS_PSALPHAOUTPUTS2=28,
    D3DRS_PSALPHAOUTPUTS3=29,
    D3DRS_PSALPHAOUTPUTS4=30,
    D3DRS_PSALPHAOUTPUTS5=31,
    D3DRS_PSALPHAOUTPUTS6=32,
    D3DRS_PSALPHAOUTPUTS7=33,
    D3DRS_PSRGBINPUTS0=34,
    D3DRS_PSRGBINPUTS1=35,
    D3DRS_PSRGBINPUTS2=36,
    D3DRS_PSRGBINPUTS3=37,
    D3DRS_PSRGBINPUTS4=38,
    D3DRS_PSRGBINPUTS5=39,
    D3DRS_PSRGBINPUTS6=40,
    D3DRS_PSRGBINPUTS7=41,
    D3DRS_PSCOMPAREMODE=42,
    D3DRS_PSFINALCOMBINERCONSTANT0=43,
    D3DRS_PSFINALCOMBINERCONSTANT1=44,
    D3DRS_PSRGBOUTPUTS0=45,
    D3DRS_PSRGBOUTPUTS1=46,
    D3DRS_PSRGBOUTPUTS2=47,
    D3DRS_PSRGBOUTPUTS3=48,
    D3DRS_PSRGBOUTPUTS4=49,
    D3DRS_PSRGBOUTPUTS5=50,
    D3DRS_PSRGBOUTPUTS6=51,
    D3DRS_PSRGBOUTPUTS7=52,
    D3DRS_PSCOMBINERCOUNT=53,
    D3DRS_PSDOTMAPPING=55,
    D3DRS_PSINPUTTEXTURE=56,
    D3DRS_PS_MAX=57,
    D3DRS_ZFUNC=57,
    D3DRS_ALPHAFUNC=58,
    D3DRS_ALPHABLENDENABLE=59,
    D3DRS_ALPHATESTENABLE=60,
    D3DRS_ALPHAREF=61,
    D3DRS_SRCBLEND=62,
    D3DRS_DESTBLEND=63,
    D3DRS_ZWRITEENABLE=64,
    D3DRS_DITHERENABLE=65,
    D3DRS_SHADEMODE=66,
    D3DRS_COLORWRITEENABLE=67,
    D3DRS_STENCILZFAIL=68,
    D3DRS_STENCILPASS=69,
    D3DRS_STENCILFUNC=70,
    D3DRS_STENCILREF=71,
    D3DRS_STENCILMASK=72,
    D3DRS_STENCILWRITEMASK=73,
    D3DRS_BLENDOP=74,
    D3DRS_BLENDCOLOR=75,
    D3DRS_SWATHWIDTH=76,
    D3DRS_POLYGONOFFSETZSLOPESCALE=77,
    D3DRS_POLYGONOFFSETZOFFSET=78,
    D3DRS_POINTOFFSETENABLE=79,
    D3DRS_WIREFRAMEOFFSETENABLE=80,
    D3DRS_SOLIDOFFSETENABLE=81,
    D3DRS_SIMPLE_MAX=82,
    D3DRS_FOGENABLE=82,
    D3DRS_FOGTABLEMODE=83,
    D3DRS_FOGSTART=84,
    D3DRS_FOGEND=85,
    D3DRS_FOGDENSITY=86,
    D3DRS_RANGEFOGENABLE=87,
    D3DRS_WRAP0=88,
    D3DRS_WRAP1=89,
    D3DRS_WRAP2=90,
    D3DRS_WRAP3=91,
    D3DRS_LIGHTING=92,
    D3DRS_SPECULARENABLE=93,
    D3DRS_LOCALVIEWER=94,
    D3DRS_COLORVERTEX=95,
    D3DRS_BACKSPECULARMATERIALSOURCE=96,
    D3DRS_BACKDIFFUSEMATERIALSOURCE=97,
    D3DRS_BACKAMBIENTMATERIALSOURCE=98,
    D3DRS_BACKEMISSIVEMATERIALSOURCE=99,
    D3DRS_SPECULARMATERIALSOURCE=100,
    D3DRS_DIFFUSEMATERIALSOURCE=101,
    D3DRS_AMBIENTMATERIALSOURCE=102,
    D3DRS_EMISSIVEMATERIALSOURCE=103,
    D3DRS_BACKAMBIENT=104,
    D3DRS_AMBIENT=105,
    D3DRS_POINTSIZE=106,
    D3DRS_POINTSIZE_MIN=107,
    D3DRS_POINTSPRITEENABLE=108,
    D3DRS_POINTSCALEENABLE=109,
    D3DRS_POINTSCALE_A=110,
    D3DRS_POINTSCALE_B=111,
    D3DRS_POINTSCALE_C=112,
    D3DRS_POINTSIZE_MAX=113,
    D3DRS_PATCHEDGESTYLE=114,
    D3DRS_PATCHSEGMENTS=115,
    D3DRS_SWAPFILTER=116,
    D3DRS_DEFERRED_MAX=117,
    D3DRS_PSTEXTUREMODES=117,
    D3DRS_VERTEXBLEND=118,
    D3DRS_FOGCOLOR=119,
    D3DRS_FILLMODE=120,
    D3DRS_BACKFILLMODE=121,
    D3DRS_TWOSIDEDLIGHTING=122,
    D3DRS_NORMALIZENORMALS=123,
    D3DRS_ZENABLE=124,
    D3DRS_STENCILENABLE=125,
    D3DRS_STENCILFAIL=126,
    D3DRS_FRONTFACE=127,
    D3DRS_CULLMODE=128,
    D3DRS_TEXTUREFACTOR=129,
    D3DRS_ZBIAS=130,
    D3DRS_LOGICOP=131,
    D3DRS_EDGEANTIALIAS=132,
    D3DRS_MULTISAMPLEANTIALIAS=133,
    D3DRS_MULTISAMPLEMASK=134,
    D3DRS_MULTISAMPLEMODE=135,
    D3DRS_MULTISAMPLERENDERTARGETMODE=136,
    D3DRS_SHADOWFUNC=137,
    D3DRS_LINEWIDTH=138,
    D3DRS_DXT1NOISEENABLE=139,
    D3DRS_YUVENABLE=140,
    D3DRS_OCCLUSIONCULLENABLE=141,
    D3DRS_STENCILCULLENABLE=142,
    D3DRS_ROPZCMPALWAYSREAD=143,
    D3DRS_ROPZREAD=144,
    D3DRS_DONOTCULLUNCOMPRESSED=145,
    D3DRS_MAX=146,
    D3DRS_FORCE_DWORD=2147483647
};

enum _D3DMATERIALCOLORSOURCE
{
    D3DMCS_MATERIAL=0,
    D3DMCS_COLOR1=1,
    D3DMCS_COLOR2=2,
    D3DMCS_FORCE_DWORD=2147483647
};

enum _D3DTEXTURESTAGESTATETYPE
{
    D3DTSS_ADDRESSU=0,
    D3DTSS_ADDRESSV=1,
    D3DTSS_ADDRESSW=2,
    D3DTSS_MAGFILTER=3,
    D3DTSS_MINFILTER=4,
    D3DTSS_MIPFILTER=5,
    D3DTSS_MIPMAPLODBIAS=6,
    D3DTSS_MAXMIPLEVEL=7,
    D3DTSS_MAXANISOTROPY=8,
    D3DTSS_COLORKEYOP=9,
    D3DTSS_COLORSIGN=10,
    D3DTSS_ALPHAKILL=11,
    D3DTSS_DEFERRED_TEXTURE_STATE_MAX=12,
    D3DTSS_COLOROP=12,
    D3DTSS_COLORARG0=13,
    D3DTSS_COLORARG1=14,
    D3DTSS_COLORARG2=15,
    D3DTSS_ALPHAOP=16,
    D3DTSS_ALPHAARG0=17,
    D3DTSS_ALPHAARG1=18,
    D3DTSS_ALPHAARG2=19,
    D3DTSS_RESULTARG=20,
    D3DTSS_TEXTURETRANSFORMFLAGS=21,
    D3DTSS_DEFERRED_MAX=22,
    D3DTSS_BUMPENVMAT00=22,
    D3DTSS_BUMPENVMAT01=23,
    D3DTSS_BUMPENVMAT11=24,
    D3DTSS_BUMPENVMAT10=25,
    D3DTSS_BUMPENVLSCALE=26,
    D3DTSS_BUMPENVLOFFSET=27,
    D3DTSS_TEXCOORDINDEX=28,
    D3DTSS_BORDERCOLOR=29,
    D3DTSS_COLORKEYCOLOR=30,
    D3DTSS_MAX=32,
    D3DTSS_FORCE_DWORD=2147483647
};

enum _D3DTEXTUREOP
{
    D3DTOP_DISABLE=1,
    D3DTOP_SELECTARG1=2,
    D3DTOP_SELECTARG2=3,
    D3DTOP_MODULATE=4,
    D3DTOP_MODULATE2X=5,
    D3DTOP_MODULATE4X=6,
    D3DTOP_ADD=7,
    D3DTOP_ADDSIGNED=8,
    D3DTOP_ADDSIGNED2X=9,
    D3DTOP_SUBTRACT=10,
    D3DTOP_ADDSMOOTH=11,
    D3DTOP_BLENDDIFFUSEALPHA=12,
    D3DTOP_BLENDTEXTUREALPHA=14,
    D3DTOP_BLENDFACTORALPHA=15,
    D3DTOP_BLENDTEXTUREALPHAPM=16,
    D3DTOP_BLENDCURRENTALPHA=13,
    D3DTOP_PREMODULATE=17,
    D3DTOP_MODULATEALPHA_ADDCOLOR=18,
    D3DTOP_MODULATECOLOR_ADDALPHA=19,
    D3DTOP_MODULATEINVALPHA_ADDCOLOR=20,
    D3DTOP_MODULATEINVCOLOR_ADDALPHA=21,
    D3DTOP_DOTPRODUCT3=22,
    D3DTOP_MULTIPLYADD=23,
    D3DTOP_LERP=24,
    D3DTOP_BUMPENVMAP=25,
    D3DTOP_BUMPENVMAPLUMINANCE=26,
    D3DTOP_MAX=27,
    D3DTOP_FORCE_DWORD=2147483647
};

enum _D3DTEXTUREFILTERTYPE
{
    D3DTEXF_NONE=0,
    D3DTEXF_POINT=1,
    D3DTEXF_LINEAR=2,
    D3DTEXF_ANISOTROPIC=3,
    D3DTEXF_QUINCUNX=4,
    D3DTEXF_GAUSSIANCUBIC=5,
    D3DTEXF_MAX=6,
    D3DTEXF_FORCE_DWORD=2147483647
};

enum _D3DVSD_TOKENTYPE
{
    D3DVSD_TOKEN_NOP=0,
    D3DVSD_TOKEN_STREAM=1,
    D3DVSD_TOKEN_STREAMDATA=2,
    D3DVSD_TOKEN_TESSELLATOR=3,
    D3DVSD_TOKEN_CONSTMEM=4,
    D3DVSD_TOKEN_EXT=5,
    D3DVSD_TOKEN_END=7,
    D3DVSD_FORCE_DWORD=2147483647
};

enum _D3DSHADER_INSTRUCTION_OPCODE_TYPE
{
    D3DSIO_NOP=0,
    D3DSIO_MOV=1,
    D3DSIO_ADD=2,
    D3DSIO_SUB=3,
    D3DSIO_MAD=4,
    D3DSIO_MUL=5,
    D3DSIO_RCP=6,
    D3DSIO_RSQ=7,
    D3DSIO_DP3=8,
    D3DSIO_DP4=9,
    D3DSIO_MIN=10,
    D3DSIO_MAX=11,
    D3DSIO_SLT=12,
    D3DSIO_SGE=13,
    D3DSIO_EXP=14,
    D3DSIO_LOG=15,
    D3DSIO_LIT=16,
    D3DSIO_DST=17,
    D3DSIO_LRP=18,
    D3DSIO_FRC=19,
    D3DSIO_M4x4=20,
    D3DSIO_M4x3=21,
    D3DSIO_M3x4=22,
    D3DSIO_M3x3=23,
    D3DSIO_M3x2=24,
    D3DSIO_TEXCOORD=64,
    D3DSIO_TEXKILL=65,
    D3DSIO_TEX=66,
    D3DSIO_TEXBEM=67,
    D3DSIO_TEXBEML=68,
    D3DSIO_TEXREG2AR=69,
    D3DSIO_TEXREG2GB=70,
    D3DSIO_TEXM3x2PAD=71,
    D3DSIO_TEXM3x2TEX=72,
    D3DSIO_TEXM3x3PAD=73,
    D3DSIO_TEXM3x3TEX=74,
    D3DSIO_TEXM3x3DIFF=75,
    D3DSIO_TEXM3x3SPEC=76,
    D3DSIO_TEXM3x3VSPEC=77,
    D3DSIO_EXPP=78,
    D3DSIO_LOGP=79,
    D3DSIO_CND=80,
    D3DSIO_DEF=81,
    D3DSIO_DPH=256,
    D3DSIO_RCC=257,
    D3DSIO_XMMA=258,
    D3DSIO_XMMC=259,
    D3DSIO_XDM=260,
    D3DSIO_XDD=261,
    D3DSIO_XFC=262,
    D3DSIO_TEXM3x2DEPTH=263,
    D3DSIO_TEXBRDF=264,
    D3DSIO_COMMENT=65534,
    D3DSIO_END=65535,
    D3DSIO_FORCE_DWORD=2147483647
};

enum _D3DSHADER_PARAM_DSTMOD_TYPE
{
    D3DSPDM_NONE=0,
    D3DSPDM_BIAS=1048576,
    D3DSPDM_FORCE_DWORD=2147483647
};

enum _D3DSHADER_PARAM_REGISTER_TYPE
{
    D3DSPR_TEMP=0,
    D3DSPR_INPUT=268435456,
    D3DSPR_CONST=536870912,
    D3DSPR_ADDR=805306368,
    D3DSPR_TEXTURE=805306368,
    D3DSPR_RASTOUT=1073741824,
    D3DSPR_ATTROUT=1342177280,
    D3DSPR_TEXCRDOUT=1610612736,
    D3DSPR_FORCE_DWORD=2147483647
};

enum _D3DVS_RASTOUT_OFFSETS
{
    D3DSRO_POSITION=0,
    D3DSRO_FOG=1,
    D3DSRO_POINT_SIZE=2,
    D3DSRO_FORCE_DWORD=2147483647
};

enum D3DVS_ADRRESSMODE
{
    D3DVS_ADDRMODE_ABSOLUTE=0,
    D3DVS_ADDRMODE_RELATIVE=8192,
    D3DVS_ADDRMODE_FORCE_DWORD=2147483647
};

enum _D3DSHADER_PARAM_SRCMOD_TYPE
{
    D3DSPSM_NONE=0,
    D3DSPSM_NEG=16777216,
    D3DSPSM_BIAS=33554432,
    D3DSPSM_BIASNEG=50331648,
    D3DSPSM_SIGN=67108864,
    D3DSPSM_SIGNNEG=83886080,
    D3DSPSM_COMP=100663296,
    D3DSPSM_SAT=117440512,
    D3DSPSM_FORCE_DWORD=2147483647
};

enum _D3DBASISTYPE
{
    D3DBASIS_BEZIER=0,
    D3DBASIS_BSPLINE=1,
    D3DBASIS_INTERPOLATE=2,
    D3DBASIS_FORCE_DWORD=2147483647
};

enum _D3DORDERTYPE
{
    D3DORDER_LINEAR=1,
    D3DORDER_CUBIC=3,
    D3DORDER_QUINTIC=5,
    D3DORDER_FORCE_DWORD=2147483647
};

enum _D3DPATCHEDGESTYLE
{
    D3DPATCHEDGE_DISCRETE=0,
    D3DPATCHEDGE_CONTINUOUS=1,
    D3DPATCHEDGE_FORCE_DWORD=2147483647
};

enum _D3DSTATEBLOCKTYPE
{
    D3DSBT_ALL=1,
    D3DSBT_PIXELSTATE=2,
    D3DSBT_VERTEXSTATE=3,
    D3DSBT_FORCE_DWORD=2147483647
};

enum _D3DVERTEXBLENDFLAGS
{
    D3DVBF_DISABLE=0,
    D3DVBF_1WEIGHTS=1,
    D3DVBF_2WEIGHTS=3,
    D3DVBF_3WEIGHTS=5,
    D3DVBF_2WEIGHTS2MATRICES=2,
    D3DVBF_3WEIGHTS3MATRICES=4,
    D3DVBF_4WEIGHTS4MATRICES=6,
    D3DVBF_MAX=7,
    D3DVBF_FORCE_DWORD=2147483647
};

enum _D3DTEXTURETRANSFORMFLAGS
{
    D3DTTFF_DISABLE=0,
    D3DTTFF_COUNT1=1,
    D3DTTFF_COUNT2=2,
    D3DTTFF_COUNT3=3,
    D3DTTFF_COUNT4=4,
    D3DTTFF_PROJECTED=256,
    D3DTTFF_FORCE_DWORD=2147483647
};

enum _D3DDEVTYPE
{
    D3DDEVTYPE_HAL=1,
    D3DDEVTYPE_REF=2,
    D3DDEVTYPE_SW=3,
    D3DDEVTYPE_FORCE_DWORD=2147483647
};

enum _D3DMULTISAMPLEMODE
{
    D3DMULTISAMPLEMODE_1X=0,
    D3DMULTISAMPLEMODE_2X=1,
    D3DMULTISAMPLEMODE_4X=2,
    D3DMULTISAMPLEMODE_FORCE_DWORD=2147483647
};

enum _D3DXMESHOPT
{
    D3DXMESHOPT_COMPACT=1,
    D3DXMESHOPT_ATTRSORT=2,
    D3DXMESHOPT_VERTEXCACHE=4,
    D3DXMESHOPT_STRIPREORDER=8,
    D3DXMESHOPT_IGNOREVERTS=16,
    D3DXMESHOPT_SHAREVB=32
};

enum _D3DFORMAT
{
    D3DFMT_UNKNOWN=-1,
    D3DFMT_A8R8G8B8=6,
    D3DFMT_X8R8G8B8=7,
    D3DFMT_R5G6B5=5,
    D3DFMT_R6G5B5=39,
    D3DFMT_X1R5G5B5=3,
    D3DFMT_A1R5G5B5=2,
    D3DFMT_A4R4G4B4=4,
    D3DFMT_A8=25,
    D3DFMT_A8B8G8R8=58,
    D3DFMT_B8G8R8A8=59,
    D3DFMT_R4G4B4A4=57,
    D3DFMT_R5G5B5A1=56,
    D3DFMT_R8G8B8A8=60,
    D3DFMT_R8B8=41,
    D3DFMT_G8B8=40,
    D3DFMT_P8=11,
    D3DFMT_L8=0,
    D3DFMT_A8L8=26,
    D3DFMT_AL8=1,
    D3DFMT_L16=50,
    D3DFMT_V8U8=40,
    D3DFMT_L6V5U5=39,
    D3DFMT_X8L8V8U8=7,
    D3DFMT_Q8W8V8U8=58,
    D3DFMT_V16U16=51,
    D3DFMT_D16_LOCKABLE=44,
    D3DFMT_D16=44,
    D3DFMT_D24S8=42,
    D3DFMT_F16=45,
    D3DFMT_F24S8=43,
    D3DFMT_YUY2=36,
    D3DFMT_UYVY=37,
    D3DFMT_DXT1=12,
    D3DFMT_DXT2=14,
    D3DFMT_DXT3=14,
    D3DFMT_DXT4=15,
    D3DFMT_DXT5=15,
    D3DFMT_LIN_A1R5G5B5=16,
    D3DFMT_LIN_A4R4G4B4=29,
    D3DFMT_LIN_A8=31,
    D3DFMT_LIN_A8B8G8R8=63,
    D3DFMT_LIN_A8R8G8B8=18,
    D3DFMT_LIN_B8G8R8A8=64,
    D3DFMT_LIN_G8B8=23,
    D3DFMT_LIN_R4G4B4A4=62,
    D3DFMT_LIN_R5G5B5A1=61,
    D3DFMT_LIN_R5G6B5=17,
    D3DFMT_LIN_R6G5B5=55,
    D3DFMT_LIN_R8B8=22,
    D3DFMT_LIN_R8G8B8A8=65,
    D3DFMT_LIN_X1R5G5B5=28,
    D3DFMT_LIN_X8R8G8B8=30,
    D3DFMT_LIN_A8L8=32,
    D3DFMT_LIN_AL8=27,
    D3DFMT_LIN_L16=53,
    D3DFMT_LIN_L8=19,
    D3DFMT_LIN_V16U16=54,
    D3DFMT_LIN_V8U8=23,
    D3DFMT_LIN_L6V5U5=55,
    D3DFMT_LIN_X8L8V8U8=30,
    D3DFMT_LIN_Q8W8V8U8=18,
    D3DFMT_LIN_D24S8=46,
    D3DFMT_LIN_F24S8=47,
    D3DFMT_LIN_D16=48,
    D3DFMT_LIN_F16=49,
    D3DFMT_VERTEXDATA=100,
    D3DFMT_INDEX16=101,
    D3DFMT_FORCE_DWORD=2147483647
};

enum _D3DSWAPEFFECT
{
    D3DSWAPEFFECT_DISCARD=1,
    D3DSWAPEFFECT_FLIP=2,
    D3DSWAPEFFECT_COPY=3,
    D3DSWAPEFFECT_COPY_VSYNC=4,
    D3DSWAPEFFECT_FORCE_DWORD=2147483647
};

enum _D3DCALLBACKTYPE
{
    D3DCALLBACK_READ=0,
    D3DCALLBACK_WRITE=1,
    D3DCALLBACKTYPE_FORCE_DWORD=2147483647
};

enum _D3DRESOURCETYPE
{
    D3DRTYPE_NONE=0,
    D3DRTYPE_SURFACE=1,
    D3DRTYPE_VOLUME=2,
    D3DRTYPE_TEXTURE=3,
    D3DRTYPE_VOLUMETEXTURE=4,
    D3DRTYPE_CUBETEXTURE=5,
    D3DRTYPE_VERTEXBUFFER=6,
    D3DRTYPE_INDEXBUFFER=7,
    D3DRTYPE_PUSHBUFFER=8,
    D3DRTYPE_PALETTE=9,
    D3DRTYPE_FIXUP=10,
    D3DRTYPE_FORCE_DWORD=2147483647
};

enum _D3DMEMORY
{
    D3DMEM_AGP=0,
    D3DMEM_VIDEO=1
};

enum _D3DCUBEMAP_FACES
{
    D3DCUBEMAP_FACE_POSITIVE_X=0,
    D3DCUBEMAP_FACE_NEGATIVE_X=1,
    D3DCUBEMAP_FACE_POSITIVE_Y=2,
    D3DCUBEMAP_FACE_NEGATIVE_Y=3,
    D3DCUBEMAP_FACE_POSITIVE_Z=4,
    D3DCUBEMAP_FACE_NEGATIVE_Z=5,
    D3DCUBEMAP_FACE_FORCE_DWORD=2147483647
};

enum _D3DFIELDTYPE
{
    D3DFIELD_ODD=1,
    D3DFIELD_EVEN=2,
    D3DFIELD_PROGRESSIVE=3,
    D3DFIELD_FORCE_DWORD=2147483647
};

enum _D3DCOPYRECTOPERATION
{
    D3DCOPYRECT_SRCCOPY_AND=0,
    D3DCOPYRECT_ROP_AND=1,
    D3DCOPYRECT_BLEND_AND=2,
    D3DCOPYRECT_SRCCOPY=3,
    D3DCOPYRECT_SRCCOPY_PREMULT=4,
    D3DCOPYRECT_BLEND_PREMULT=5,
    D3DCOPYRECT_FORCE_DWORD=2147483647
};

enum _D3DCOPYRECTCOLORFORMAT
{
    D3DCOPYRECT_COLOR_FORMAT_DEFAULT=0,
    D3DCOPYRECT_COLOR_FORMAT_Y8=1,
    D3DCOPYRECT_COLOR_FORMAT_X1R5G5B5_Z1R5G5B5=2,
    D3DCOPYRECT_COLOR_FORMAT_X1R5G5B5_O1R5G5B5=3,
    D3DCOPYRECT_COLOR_FORMAT_R5G6B5=4,
    D3DCOPYRECT_COLOR_FORMAT_Y16=5,
    D3DCOPYRECT_COLOR_FORMAT_X8R8G8B8_Z8R8G8B8=6,
    D3DCOPYRECT_COLOR_FORMAT_X8R8G8B8_O8R8G8B8=7,
    D3DCOPYRECT_COLOR_FORMAT_X1A7R8G8B8_Z1A7R8G8B8=8,
    D3DCOPYRECT_COLOR_FORMAT_X1A7R8G8B8_O1A7R8G8B8=9,
    D3DCOPYRECT_COLOR_FORMAT_A8R8G8B8=10,
    D3DCOPYRECT_COLOR_FORMAT_Y32=11,
    D3DCOPYRECT_COLOR_FORMAT_FORCE_DWORD=2147483647
};

enum PS_TEXTUREMODES
{
    PS_TEXTUREMODES_NONE=0,
    PS_TEXTUREMODES_PROJECT2D=1,
    PS_TEXTUREMODES_PROJECT3D=2,
    PS_TEXTUREMODES_CUBEMAP=3,
    PS_TEXTUREMODES_PASSTHRU=4,
    PS_TEXTUREMODES_CLIPPLANE=5,
    PS_TEXTUREMODES_BUMPENVMAP=6,
    PS_TEXTUREMODES_BUMPENVMAP_LUM=7,
    PS_TEXTUREMODES_BRDF=8,
    PS_TEXTUREMODES_DOT_ST=9,
    PS_TEXTUREMODES_DOT_ZW=10,
    PS_TEXTUREMODES_DOT_RFLCT_DIFF=11,
    PS_TEXTUREMODES_DOT_RFLCT_SPEC=12,
    PS_TEXTUREMODES_DOT_STR_3D=13,
    PS_TEXTUREMODES_DOT_STR_CUBE=14,
    PS_TEXTUREMODES_DPNDNT_AR=15,
    PS_TEXTUREMODES_DPNDNT_GB=16,
    PS_TEXTUREMODES_DOTPRODUCT=17,
    PS_TEXTUREMODES_DOT_RFLCT_SPEC_CONST=18
};

enum PS_DOTMAPPING
{
    PS_DOTMAPPING_ZERO_TO_ONE=0,
    PS_DOTMAPPING_MINUS1_TO_1_D3D=1,
    PS_DOTMAPPING_MINUS1_TO_1_GL=2,
    PS_DOTMAPPING_MINUS1_TO_1=3,
    PS_DOTMAPPING_HILO_1=4,
    PS_DOTMAPPING_HILO_HEMISPHERE_D3D=5,
    PS_DOTMAPPING_HILO_HEMISPHERE_GL=6,
    PS_DOTMAPPING_HILO_HEMISPHERE=7
};

enum PS_COMPAREMODE
{
    PS_COMPAREMODE_S_LT=0,
    PS_COMPAREMODE_S_GE=1,
    PS_COMPAREMODE_T_LT=0,
    PS_COMPAREMODE_T_GE=2,
    PS_COMPAREMODE_R_LT=0,
    PS_COMPAREMODE_R_GE=4,
    PS_COMPAREMODE_Q_LT=0,
    PS_COMPAREMODE_Q_GE=8
};

enum PS_COMBINERCOUNTFLAGS
{
    PS_COMBINERCOUNT_MUX_LSB=0,
    PS_COMBINERCOUNT_MUX_MSB=1,
    PS_COMBINERCOUNT_SAME_C0=0,
    PS_COMBINERCOUNT_UNIQUE_C0=16,
    PS_COMBINERCOUNT_SAME_C1=0,
    PS_COMBINERCOUNT_UNIQUE_C1=256
};

enum PS_INPUTMAPPING
{
    PS_INPUTMAPPING_UNSIGNED_IDENTITY=0,
    PS_INPUTMAPPING_UNSIGNED_INVERT=32,
    PS_INPUTMAPPING_EXPAND_NORMAL=64,
    PS_INPUTMAPPING_EXPAND_NEGATE=96,
    PS_INPUTMAPPING_HALFBIAS_NORMAL=128,
    PS_INPUTMAPPING_HALFBIAS_NEGATE=160,
    PS_INPUTMAPPING_SIGNED_IDENTITY=192,
    PS_INPUTMAPPING_SIGNED_NEGATE=224
};

enum PS_REGISTER
{
    PS_REGISTER_ZERO=0,
    PS_REGISTER_DISCARD=0,
    PS_REGISTER_C0=1,
    PS_REGISTER_C1=2,
    PS_REGISTER_FOG=3,
    PS_REGISTER_V0=4,
    PS_REGISTER_V1=5,
    PS_REGISTER_T0=8,
    PS_REGISTER_T1=9,
    PS_REGISTER_T2=10,
    PS_REGISTER_T3=11,
    PS_REGISTER_R0=12,
    PS_REGISTER_R1=13,
    PS_REGISTER_V1R0_SUM=14,
    PS_REGISTER_EF_PROD=15,
    PS_REGISTER_ONE=32,
    PS_REGISTER_NEGATIVE_ONE=64,
    PS_REGISTER_ONE_HALF=160,
    PS_REGISTER_NEGATIVE_ONE_HALF=128
};

enum PS_CHANNEL
{
    PS_CHANNEL_RGB=0,
    PS_CHANNEL_BLUE=0,
    PS_CHANNEL_ALPHA=16
};

enum PS_FINALCOMBINERSETTING
{
    PS_FINALCOMBINERSETTING_CLAMP_SUM=128,
    PS_FINALCOMBINERSETTING_COMPLEMENT_V1=64,
    PS_FINALCOMBINERSETTING_COMPLEMENT_R0=32
};

struct D3DXCOLOR// Size=0x10 (Id=157)
{
    float r;// Offset=0x0 Size=0x4
    float g;// Offset=0x4 Size=0x4
    float b;// Offset=0x8 Size=0x4
    float a;// Offset=0xc Size=0x4
};

struct _D3DVECTOR// Size=0xc (Id=181)
{
    float x;// Offset=0x0 Size=0x4
    float y;// Offset=0x4 Size=0x4
    float z;// Offset=0x8 Size=0x4
};

struct _D3DLIGHT8// Size=0x68 (Id=183)
{
    enum _D3DLIGHTTYPE Type;// Offset=0x0 Size=0x4
    struct _D3DCOLORVALUE Diffuse;// Offset=0x4 Size=0x10
    struct _D3DCOLORVALUE Specular;// Offset=0x14 Size=0x10
    struct _D3DCOLORVALUE Ambient;// Offset=0x24 Size=0x10
    struct _D3DVECTOR Position;// Offset=0x34 Size=0xc
    struct _D3DVECTOR Direction;// Offset=0x40 Size=0xc
    float Range;// Offset=0x4c Size=0x4
    float Falloff;// Offset=0x50 Size=0x4
    float Attenuation0;// Offset=0x54 Size=0x4
    float Attenuation1;// Offset=0x58 Size=0x4
    float Attenuation2;// Offset=0x5c Size=0x4
    float Theta;// Offset=0x60 Size=0x4
    float Phi;// Offset=0x64 Size=0x4
};

struct ID3DXBufferVtbl// Size=0x14 (Id=184)
{
    long  ( * QueryInterface)(void * ,const struct _GUID * ,void ** );// Offset=0x0 Size=0x4
    unsigned long  ( * AddRef)(void * );// Offset=0x4 Size=0x4
    unsigned long  ( * Release)(void * );// Offset=0x8 Size=0x4
    void *  ( * GetBufferPointer)(void * );// Offset=0xc Size=0x4
    unsigned long  ( * GetBufferSize)(void * );// Offset=0x10 Size=0x4
};

struct _D3DSURFACE_DESC// Size=0x1c (Id=186)
{
    enum _D3DFORMAT Format;// Offset=0x0 Size=0x4
    enum _D3DRESOURCETYPE Type;// Offset=0x4 Size=0x4
    unsigned long Usage;// Offset=0x8 Size=0x4
    unsigned int Size;// Offset=0xc Size=0x4
    unsigned long MultiSampleType;// Offset=0x10 Size=0x4
    unsigned int Width;// Offset=0x14 Size=0x4
    unsigned int Height;// Offset=0x18 Size=0x4
};

struct _D3DLOCKED_RECT// Size=0x8 (Id=194)
{
    int Pitch;// Offset=0x0 Size=0x4
    void * pBits;// Offset=0x4 Size=0x4
};

struct D3DBaseTexture// Size=0x14 (Id=201)
{
    unsigned long Common;// Offset=0x0 Size=0x4
    unsigned long Data;// Offset=0x4 Size=0x4
    unsigned long Lock;// Offset=0x8 Size=0x4
    unsigned long Format;// Offset=0xc Size=0x4
    unsigned long Size;// Offset=0x10 Size=0x4
};

struct _D3DPRESENT_PARAMETERS_// Size=0x44 (Id=223)
{
    unsigned int BackBufferWidth;// Offset=0x0 Size=0x4
    unsigned int BackBufferHeight;// Offset=0x4 Size=0x4
    enum _D3DFORMAT BackBufferFormat;// Offset=0x8 Size=0x4
    unsigned int BackBufferCount;// Offset=0xc Size=0x4
    unsigned long MultiSampleType;// Offset=0x10 Size=0x4
    enum _D3DSWAPEFFECT SwapEffect;// Offset=0x14 Size=0x4
    struct HWND__ * hDeviceWindow;// Offset=0x18 Size=0x4
    int Windowed;// Offset=0x1c Size=0x4
    int EnableAutoDepthStencil;// Offset=0x20 Size=0x4
    enum _D3DFORMAT AutoDepthStencilFormat;// Offset=0x24 Size=0x4
    unsigned long Flags;// Offset=0x28 Size=0x4
    unsigned int FullScreen_RefreshRateInHz;// Offset=0x2c Size=0x4
    unsigned int FullScreen_PresentationInterval;// Offset=0x30 Size=0x4
    struct D3DSurface * BufferSurfaces[3];// Offset=0x34 Size=0xc
    struct D3DSurface * DepthStencilSurface;// Offset=0x40 Size=0x4
};

struct _D3DXEFFECT_DESC// Size=0xc (Id=229)
{
    unsigned int Parameters;// Offset=0x0 Size=0x4
    unsigned int Techniques;// Offset=0x4 Size=0x4
    unsigned long Usage;// Offset=0x8 Size=0x4
};

struct _D3DMATRIX// Size=0x40 (Id=259)
{
    union // Size=0x40 (Id=0)
    {
        float _11;// Offset=0x0 Size=0x4
        float _12;// Offset=0x4 Size=0x4
        float _13;// Offset=0x8 Size=0x4
        float _14;// Offset=0xc Size=0x4
        float _21;// Offset=0x10 Size=0x4
        float _22;// Offset=0x14 Size=0x4
        float _23;// Offset=0x18 Size=0x4
        float _24;// Offset=0x1c Size=0x4
        float _31;// Offset=0x20 Size=0x4
        float _32;// Offset=0x24 Size=0x4
        float _33;// Offset=0x28 Size=0x4
        float _34;// Offset=0x2c Size=0x4
        float _41;// Offset=0x30 Size=0x4
        float _42;// Offset=0x34 Size=0x4
        float _43;// Offset=0x38 Size=0x4
        float _44;// Offset=0x3c Size=0x4
        float m[4][4];// Offset=0x0 Size=0x40
    };
};

struct _D3DCOPYRECTROPSTATE// Size=0x20 (Id=267)
{
    unsigned long Rop;// Offset=0x0 Size=0x4
    unsigned long Shape;// Offset=0x4 Size=0x4
    unsigned long PatternSelect;// Offset=0x8 Size=0x4
    unsigned long MonoColor0;// Offset=0xc Size=0x4
    unsigned long MonoColor1;// Offset=0x10 Size=0x4
    unsigned long MonoPattern0;// Offset=0x14 Size=0x4
    unsigned long MonoPattern1;// Offset=0x18 Size=0x4
    unsigned long * ColorPattern;// Offset=0x1c Size=0x4
};

struct _D3DTILE// Size=0x18 (Id=274)
{
    unsigned long Flags;// Offset=0x0 Size=0x4
    void * pMemory;// Offset=0x4 Size=0x4
    unsigned long Size;// Offset=0x8 Size=0x4
    unsigned long Pitch;// Offset=0xc Size=0x4
    unsigned long ZStartTag;// Offset=0x10 Size=0x4
    unsigned long ZOffset;// Offset=0x14 Size=0x4
};

struct _D3DLOCKED_BOX// Size=0xc (Id=280)
{
    int RowPitch;// Offset=0x0 Size=0x4
    int SlicePitch;// Offset=0x4 Size=0x4
    void * pBits;// Offset=0x8 Size=0x4
};

struct ID3DXRenderToEnvMapVtbl// Size=0x2c (Id=281)
{
    long  ( * QueryInterface)(void * ,const struct _GUID * ,void ** );// Offset=0x0 Size=0x4
    unsigned long  ( * AddRef)(void * );// Offset=0x4 Size=0x4
    unsigned long  ( * Release)(void * );// Offset=0x8 Size=0x4
    long  ( * GetDevice)(void * ,struct D3DDevice ** );// Offset=0xc Size=0x4
    long  ( * GetDesc)(void * ,struct _D3DXRTE_DESC * );// Offset=0x10 Size=0x4
    long  ( * BeginCube)(void * ,struct D3DCubeTexture * );// Offset=0x14 Size=0x4
    long  ( * BeginSphere)(void * ,struct D3DTexture * );// Offset=0x18 Size=0x4
    long  ( * BeginHemisphere)(void * ,struct D3DTexture * ,struct D3DTexture * );// Offset=0x1c Size=0x4
    long  ( * BeginParabolic)(void * ,struct D3DTexture * ,struct D3DTexture * );// Offset=0x20 Size=0x4
    long  ( * Face)(void * ,enum _D3DCUBEMAP_FACES );// Offset=0x24 Size=0x4
    long  ( * End)(void * );// Offset=0x28 Size=0x4
};

struct ID3DXTechniqueVtbl// Size=0x2c (Id=293)
{
    long  ( * QueryInterface)(struct IDirectXFileBinary * ,const struct _GUID * ,void ** );// Offset=0x0 Size=0x4
    unsigned long  ( * AddRef)(struct IDirectXFileBinary * );// Offset=0x4 Size=0x4
    unsigned long  ( * Release)(struct IDirectXFileBinary * );// Offset=0x8 Size=0x4
    long  ( * GetDevice)(struct IDirectXFileBinary * ,struct D3DDevice ** );// Offset=0xc Size=0x4
    long  ( * GetDesc)(struct IDirectXFileBinary * ,struct _D3DXTECHNIQUE_DESC * );// Offset=0x10 Size=0x4
    long  ( * GetPassDesc)(struct IDirectXFileBinary * ,unsigned int ,struct _D3DXPASS_DESC * );// Offset=0x14 Size=0x4
    int  ( * IsParameterUsed)(struct IDirectXFileBinary * ,unsigned long );// Offset=0x18 Size=0x4
    long  ( * Validate)(struct IDirectXFileBinary * );// Offset=0x1c Size=0x4
    long  ( * Begin)(struct IDirectXFileBinary * ,unsigned int * );// Offset=0x20 Size=0x4
    long  ( * Pass)(struct IDirectXFileBinary * ,unsigned int );// Offset=0x24 Size=0x4
    long  ( * End)(struct IDirectXFileBinary * );// Offset=0x28 Size=0x4
};

struct _D3DCOPYRECTSTATE// Size=0x20 (Id=298)
{
    enum _D3DCOPYRECTCOLORFORMAT ColorFormat;// Offset=0x0 Size=0x4
    enum _D3DCOPYRECTOPERATION Operation;// Offset=0x4 Size=0x4
    int ColorKeyEnable;// Offset=0x8 Size=0x4
    unsigned long ColorKeyValue;// Offset=0xc Size=0x4
    unsigned long BlendAlpha;// Offset=0x10 Size=0x4
    unsigned long PremultAlpha;// Offset=0x14 Size=0x4
    unsigned long ClippingPoint;// Offset=0x18 Size=0x4
    unsigned long ClippingSize;// Offset=0x1c Size=0x4
};

struct ID3DXBaseMeshVtbl// Size=0x4c (Id=301)
{
    long  ( * QueryInterface)(struct IDirectXFileBinary * ,const struct _GUID * ,void ** );// Offset=0x0 Size=0x4
    unsigned long  ( * AddRef)(struct IDirectXFileBinary * );// Offset=0x4 Size=0x4
    unsigned long  ( * Release)(struct IDirectXFileBinary * );// Offset=0x8 Size=0x4
    long  ( * DrawSubset)(struct IDirectXFileBinary * ,unsigned long );// Offset=0xc Size=0x4
    unsigned long  ( * GetNumFaces)(struct IDirectXFileBinary * );// Offset=0x10 Size=0x4
    unsigned long  ( * GetNumVertices)(struct IDirectXFileBinary * );// Offset=0x14 Size=0x4
    unsigned long  ( * GetFVF)(struct IDirectXFileBinary * );// Offset=0x18 Size=0x4
    long  ( * GetDeclaration)(struct IDirectXFileBinary * ,unsigned long * );// Offset=0x1c Size=0x4
    unsigned long  ( * GetOptions)(struct IDirectXFileBinary * );// Offset=0x20 Size=0x4
    long  ( * GetDevice)(struct IDirectXFileBinary * ,struct D3DDevice ** );// Offset=0x24 Size=0x4
    long  ( * CloneMeshFVF)(struct IDirectXFileBinary * ,unsigned long ,unsigned long ,struct D3DDevice * ,struct ID3DXMesh ** );// Offset=0x28 Size=0x4
    long  ( * CloneMesh)(struct IDirectXFileBinary * ,unsigned long ,unsigned long * ,struct D3DDevice * ,struct ID3DXMesh ** );// Offset=0x2c Size=0x4
    long  ( * GetVertexBuffer)(struct IDirectXFileBinary * ,struct D3DVertexBuffer ** );// Offset=0x30 Size=0x4
    long  ( * GetIndexBuffer)(struct IDirectXFileBinary * ,struct D3DIndexBuffer ** );// Offset=0x34 Size=0x4
    long  ( * LockVertexBuffer)(struct IDirectXFileBinary * ,unsigned long ,unsigned char ** );// Offset=0x38 Size=0x4
    long  ( * UnlockVertexBuffer)(struct IDirectXFileBinary * );// Offset=0x3c Size=0x4
    long  ( * LockIndexBuffer)(struct IDirectXFileBinary * ,unsigned long ,unsigned char ** );// Offset=0x40 Size=0x4
    long  ( * UnlockIndexBuffer)(struct IDirectXFileBinary * );// Offset=0x44 Size=0x4
    long  ( * GetAttributeTable)(struct IDirectXFileBinary * ,struct _D3DXATTRIBUTERANGE * ,unsigned long * );// Offset=0x48 Size=0x4
};

struct ID3DXBaseMesh// Size=0x4 (Id=320)
{
    struct ID3DXBaseMeshVtbl * lpVtbl;// Offset=0x0 Size=0x4
};

struct ID3DXMesh// Size=0x4 (Id=328)
{
    struct ID3DXMeshVtbl * lpVtbl;// Offset=0x0 Size=0x4
};

struct _D3DVERTEXBUFFER_DESC// Size=0x8 (Id=332)
{
    enum _D3DFORMAT Format;// Offset=0x0 Size=0x4
    enum _D3DRESOURCETYPE Type;// Offset=0x4 Size=0x4
};

struct D3DXVECTOR2// Size=0x8 (Id=333)
{
    float x;// Offset=0x0 Size=0x4
    float y;// Offset=0x4 Size=0x4
};

struct _D3DXRTE_DESC// Size=0x10 (Id=350)
{
    unsigned int Size;// Offset=0x0 Size=0x4
    enum _D3DFORMAT Format;// Offset=0x4 Size=0x4
    int DepthStencil;// Offset=0x8 Size=0x4
    enum _D3DFORMAT DepthStencilFormat;// Offset=0xc Size=0x4
};

struct _D3DSTREAM_INPUT// Size=0xc (Id=360)
{
    struct D3DVertexBuffer * VertexBuffer;// Offset=0x0 Size=0x4
    unsigned int Stride;// Offset=0x4 Size=0x4
    unsigned int Offset;// Offset=0x8 Size=0x4
};

struct D3DIndexBuffer// Size=0xc (Id=362)
{
    unsigned long Common;// Offset=0x0 Size=0x4
    unsigned long Data;// Offset=0x4 Size=0x4
    unsigned long Lock;// Offset=0x8 Size=0x4
};

struct ID3DXSPMeshVtbl// Size=0x44 (Id=382)
{
    long  ( * QueryInterface)(struct IDirectXFileBinary * ,const struct _GUID * ,void ** );// Offset=0x0 Size=0x4
    unsigned long  ( * AddRef)(struct IDirectXFileBinary * );// Offset=0x4 Size=0x4
    unsigned long  ( * Release)(struct IDirectXFileBinary * );// Offset=0x8 Size=0x4
    unsigned long  ( * GetNumFaces)(struct IDirectXFileBinary * );// Offset=0xc Size=0x4
    unsigned long  ( * GetNumVertices)(struct IDirectXFileBinary * );// Offset=0x10 Size=0x4
    unsigned long  ( * GetFVF)(struct IDirectXFileBinary * );// Offset=0x14 Size=0x4
    long  ( * GetDeclaration)(struct IDirectXFileBinary * ,unsigned long * );// Offset=0x18 Size=0x4
    unsigned long  ( * GetOptions)(struct IDirectXFileBinary * );// Offset=0x1c Size=0x4
    long  ( * GetDevice)(struct IDirectXFileBinary * ,struct D3DDevice ** );// Offset=0x20 Size=0x4
    long  ( * CloneMeshFVF)(struct IDirectXFileBinary * ,unsigned long ,unsigned long ,struct D3DDevice * ,unsigned long * ,unsigned long * ,struct ID3DXMesh ** );// Offset=0x24 Size=0x4
    long  ( * CloneMesh)(struct IDirectXFileBinary * ,unsigned long ,unsigned long * ,struct D3DDevice * ,unsigned long * ,unsigned long * ,struct ID3DXMesh ** );// Offset=0x28 Size=0x4
    long  ( * ClonePMeshFVF)(struct IDirectXFileBinary * ,unsigned long ,unsigned long ,struct D3DDevice * ,unsigned long * ,struct ID3DXPMesh ** );// Offset=0x2c Size=0x4
    long  ( * ClonePMesh)(struct IDirectXFileBinary * ,unsigned long ,unsigned long * ,struct D3DDevice * ,unsigned long * ,struct ID3DXPMesh ** );// Offset=0x30 Size=0x4
    long  ( * ReduceFaces)(struct IDirectXFileBinary * ,unsigned long );// Offset=0x34 Size=0x4
    long  ( * ReduceVertices)(struct IDirectXFileBinary * ,unsigned long );// Offset=0x38 Size=0x4
    unsigned long  ( * GetMaxFaces)(struct IDirectXFileBinary * );// Offset=0x3c Size=0x4
    unsigned long  ( * GetMaxVertices)(struct IDirectXFileBinary * );// Offset=0x40 Size=0x4
};

struct _D3DXIMAGE_INFO// Size=0x14 (Id=388)
{
    unsigned int Width;// Offset=0x0 Size=0x4
    unsigned int Height;// Offset=0x4 Size=0x4
    unsigned int Depth;// Offset=0x8 Size=0x4
    unsigned int MipLevels;// Offset=0xc Size=0x4
    enum _D3DFORMAT Format;// Offset=0x10 Size=0x4
};

struct _D3DTRIPATCH_INFO// Size=0x10 (Id=395)
{
    unsigned int StartVertexOffset;// Offset=0x0 Size=0x4
    unsigned int NumVertices;// Offset=0x4 Size=0x4
    enum _D3DBASISTYPE Basis;// Offset=0x8 Size=0x4
    enum _D3DORDERTYPE Order;// Offset=0xc Size=0x4
};

struct D3DVolume// Size=0x18 (Id=416)
{
    unsigned long Common;// Offset=0x0 Size=0x4
    unsigned long Data;// Offset=0x4 Size=0x4
    unsigned long Lock;// Offset=0x8 Size=0x4
    unsigned long Format;// Offset=0xc Size=0x4
    unsigned long Size;// Offset=0x10 Size=0x4
    struct D3DBaseTexture * Parent;// Offset=0x14 Size=0x4
};

struct _D3DXPARAMETER_DESC// Size=0x8 (Id=418)
{
    unsigned long Name;// Offset=0x0 Size=0x4
    enum _D3DXPARAMETERTYPE Type;// Offset=0x4 Size=0x4
};

struct _D3DVIEWPORT8// Size=0x18 (Id=455)
{
    unsigned long X;// Offset=0x0 Size=0x4
    unsigned long Y;// Offset=0x4 Size=0x4
    unsigned long Width;// Offset=0x8 Size=0x4
    unsigned long Height;// Offset=0xc Size=0x4
    float MinZ;// Offset=0x10 Size=0x4
    float MaxZ;// Offset=0x14 Size=0x4
};

struct _D3DXTECHNIQUE_DESC// Size=0x8 (Id=459)
{
    unsigned long Name;// Offset=0x0 Size=0x4
    unsigned int Passes;// Offset=0x4 Size=0x4
};

struct ID3DXEffect// Size=0x4 (Id=462)
{
    struct ID3DXEffectVtbl * lpVtbl;// Offset=0x0 Size=0x4
};

struct _D3DVOLUME_DESC// Size=0x1c (Id=471)
{
    enum _D3DFORMAT Format;// Offset=0x0 Size=0x4
    enum _D3DRESOURCETYPE Type;// Offset=0x4 Size=0x4
    unsigned long Usage;// Offset=0x8 Size=0x4
    unsigned int Size;// Offset=0xc Size=0x4
    unsigned int Width;// Offset=0x10 Size=0x4
    unsigned int Height;// Offset=0x14 Size=0x4
    unsigned int Depth;// Offset=0x18 Size=0x4
};

struct _D3DDISPLAYMODE// Size=0x14 (Id=490)
{
    unsigned int Width;// Offset=0x0 Size=0x4
    unsigned int Height;// Offset=0x4 Size=0x4
    unsigned int RefreshRate;// Offset=0x8 Size=0x4
    unsigned long Flags;// Offset=0xc Size=0x4
    enum _D3DFORMAT Format;// Offset=0x10 Size=0x4
};

struct _D3DCAPS8// Size=0xd4 (Id=519)
{
    enum _D3DDEVTYPE DeviceType;// Offset=0x0 Size=0x4
    unsigned int AdapterOrdinal;// Offset=0x4 Size=0x4
    unsigned long Caps;// Offset=0x8 Size=0x4
    unsigned long Caps2;// Offset=0xc Size=0x4
    unsigned long Caps3;// Offset=0x10 Size=0x4
    unsigned long PresentationIntervals;// Offset=0x14 Size=0x4
    unsigned long CursorCaps;// Offset=0x18 Size=0x4
    unsigned long DevCaps;// Offset=0x1c Size=0x4
    unsigned long PrimitiveMiscCaps;// Offset=0x20 Size=0x4
    unsigned long RasterCaps;// Offset=0x24 Size=0x4
    unsigned long ZCmpCaps;// Offset=0x28 Size=0x4
    unsigned long SrcBlendCaps;// Offset=0x2c Size=0x4
    unsigned long DestBlendCaps;// Offset=0x30 Size=0x4
    unsigned long AlphaCmpCaps;// Offset=0x34 Size=0x4
    unsigned long ShadeCaps;// Offset=0x38 Size=0x4
    unsigned long TextureCaps;// Offset=0x3c Size=0x4
    unsigned long TextureFilterCaps;// Offset=0x40 Size=0x4
    unsigned long CubeTextureFilterCaps;// Offset=0x44 Size=0x4
    unsigned long VolumeTextureFilterCaps;// Offset=0x48 Size=0x4
    unsigned long TextureAddressCaps;// Offset=0x4c Size=0x4
    unsigned long VolumeTextureAddressCaps;// Offset=0x50 Size=0x4
    unsigned long LineCaps;// Offset=0x54 Size=0x4
    unsigned long MaxTextureWidth;// Offset=0x58 Size=0x4
    unsigned long MaxTextureHeight;// Offset=0x5c Size=0x4
    unsigned long MaxVolumeExtent;// Offset=0x60 Size=0x4
    unsigned long MaxTextureRepeat;// Offset=0x64 Size=0x4
    unsigned long MaxTextureAspectRatio;// Offset=0x68 Size=0x4
    unsigned long MaxAnisotropy;// Offset=0x6c Size=0x4
    float MaxVertexW;// Offset=0x70 Size=0x4
    float GuardBandLeft;// Offset=0x74 Size=0x4
    float GuardBandTop;// Offset=0x78 Size=0x4
    float GuardBandRight;// Offset=0x7c Size=0x4
    float GuardBandBottom;// Offset=0x80 Size=0x4
    float ExtentsAdjust;// Offset=0x84 Size=0x4
    unsigned long StencilCaps;// Offset=0x88 Size=0x4
    unsigned long FVFCaps;// Offset=0x8c Size=0x4
    unsigned long TextureOpCaps;// Offset=0x90 Size=0x4
    unsigned long MaxTextureBlendStages;// Offset=0x94 Size=0x4
    unsigned long MaxSimultaneousTextures;// Offset=0x98 Size=0x4
    unsigned long VertexProcessingCaps;// Offset=0x9c Size=0x4
    unsigned long MaxActiveLights;// Offset=0xa0 Size=0x4
    unsigned long MaxUserClipPlanes;// Offset=0xa4 Size=0x4
    unsigned long MaxVertexBlendMatrices;// Offset=0xa8 Size=0x4
    unsigned long MaxVertexBlendMatrixIndex;// Offset=0xac Size=0x4
    float MaxPointSize;// Offset=0xb0 Size=0x4
    unsigned long MaxPrimitiveCount;// Offset=0xb4 Size=0x4
    unsigned long MaxVertexIndex;// Offset=0xb8 Size=0x4
    unsigned long MaxStreams;// Offset=0xbc Size=0x4
    unsigned long MaxStreamStride;// Offset=0xc0 Size=0x4
    unsigned long VertexShaderVersion;// Offset=0xc4 Size=0x4
    unsigned long MaxVertexShaderConst;// Offset=0xc8 Size=0x4
    unsigned long PixelShaderVersion;// Offset=0xcc Size=0x4
    float MaxPixelShaderValue;// Offset=0xd0 Size=0x4
};

struct D3DXQUATERNION// Size=0x10 (Id=523)
{
    float x;// Offset=0x0 Size=0x4
    float y;// Offset=0x4 Size=0x4
    float z;// Offset=0x8 Size=0x4
    float w;// Offset=0xc Size=0x4
};

struct ID3DXSPMesh// Size=0x4 (Id=525)
{
    struct ID3DXSPMeshVtbl * lpVtbl;// Offset=0x0 Size=0x4
};

struct D3DSurface// Size=0x18 (Id=553)
{
    unsigned long Common;// Offset=0x0 Size=0x4
    unsigned long Data;// Offset=0x4 Size=0x4
    unsigned long Lock;// Offset=0x8 Size=0x4
    unsigned long Format;// Offset=0xc Size=0x4
    unsigned long Size;// Offset=0x10 Size=0x4
    struct D3DBaseTexture * Parent;// Offset=0x14 Size=0x4
};

struct _D3DSWAPDATA// Size=0x14 (Id=585)
{
    unsigned long Swap;// Offset=0x0 Size=0x4
    unsigned long SwapVBlank;// Offset=0x4 Size=0x4
    unsigned long MissedVBlanks;// Offset=0x8 Size=0x4
    unsigned long TimeUntilSwapVBlank;// Offset=0xc Size=0x4
    unsigned long TimeBetweenSwapVBlanks;// Offset=0x10 Size=0x4
};

struct ID3DXRenderToEnvMap// Size=0x4 (Id=588)
{
    struct ID3DXRenderToEnvMapVtbl * lpVtbl;// Offset=0x0 Size=0x4
};

struct _D3DXATTRIBUTEWEIGHTS// Size=0x34 (Id=597)
{
    float Position;// Offset=0x0 Size=0x4
    float Boundary;// Offset=0x4 Size=0x4
    float Normal;// Offset=0x8 Size=0x4
    float Diffuse;// Offset=0xc Size=0x4
    float Specular;// Offset=0x10 Size=0x4
    float Tex[8];// Offset=0x14 Size=0x20
};

struct _D3DDEVICE_CREATION_PARAMETERS// Size=0x10 (Id=618)
{
    unsigned int AdapterOrdinal;// Offset=0x0 Size=0x4
    enum _D3DDEVTYPE DeviceType;// Offset=0x4 Size=0x4
    struct HWND__ * hFocusWindow;// Offset=0x8 Size=0x4
    unsigned long BehaviorFlags;// Offset=0xc Size=0x4
};

struct ID3DXMatrixStack// Size=0x4 (Id=622)
{
    struct ID3DXMatrixStackVtbl * lpVtbl;// Offset=0x0 Size=0x4
};

struct ID3DXBuffer// Size=0x4 (Id=637)
{
    struct ID3DXBufferVtbl * lpVtbl;// Offset=0x0 Size=0x4
};

struct ID3DXRenderToSurface// Size=0x4 (Id=638)
{
    struct ID3DXRenderToSurfaceVtbl * lpVtbl;// Offset=0x0 Size=0x4
};

struct _D3DXPASS_DESC// Size=0x4 (Id=677)
{
    unsigned long Name;// Offset=0x0 Size=0x4
};

struct _D3DCOLORVALUE// Size=0x10 (Id=679)
{
    float r;// Offset=0x0 Size=0x4
    float g;// Offset=0x4 Size=0x4
    float b;// Offset=0x8 Size=0x4
    float a;// Offset=0xc Size=0x4
};

struct D3DPalette// Size=0xc (Id=692)
{
    unsigned long Common;// Offset=0x0 Size=0x4
    unsigned long Data;// Offset=0x4 Size=0x4
    unsigned long Lock;// Offset=0x8 Size=0x4
};

struct _D3DGAMMARAMP// Size=0x300 (Id=695)
{
    unsigned char red[256];// Offset=0x0 Size=0x100
    unsigned char green[256];// Offset=0x100 Size=0x100
    unsigned char blue[256];// Offset=0x200 Size=0x100
};

struct _D3DBOX// Size=0x18 (Id=723)
{
    unsigned int Left;// Offset=0x0 Size=0x4
    unsigned int Top;// Offset=0x4 Size=0x4
    unsigned int Right;// Offset=0x8 Size=0x4
    unsigned int Bottom;// Offset=0xc Size=0x4
    unsigned int Front;// Offset=0x10 Size=0x4
    unsigned int Back;// Offset=0x14 Size=0x4
};

struct _D3DVBLANKDATA// Size=0xc (Id=734)
{
    unsigned long VBlank;// Offset=0x0 Size=0x4
    unsigned long Swap;// Offset=0x4 Size=0x4
    unsigned long Flags;// Offset=0x8 Size=0x4
};

struct _D3DRASTER_STATUS// Size=0x8 (Id=752)
{
    int InVBlank;// Offset=0x0 Size=0x4
    unsigned int ScanLine;// Offset=0x4 Size=0x4
};

struct _D3DPixelShaderDefFile// Size=0xf4 (Id=764)
{
    unsigned long FileID;// Offset=0x0 Size=0x4
    struct _D3DPixelShaderDef Psd;// Offset=0x4 Size=0xf0
};

struct _D3DMATERIAL8// Size=0x44 (Id=809)
{
    struct _D3DCOLORVALUE Diffuse;// Offset=0x0 Size=0x10
    struct _D3DCOLORVALUE Ambient;// Offset=0x10 Size=0x10
    struct _D3DCOLORVALUE Specular;// Offset=0x20 Size=0x10
    struct _D3DCOLORVALUE Emissive;// Offset=0x30 Size=0x10
    float Power;// Offset=0x40 Size=0x4
};

struct _D3DXATTRIBUTERANGE// Size=0x14 (Id=815)
{
    unsigned long AttribId;// Offset=0x0 Size=0x4
    unsigned long FaceStart;// Offset=0x4 Size=0x4
    unsigned long FaceCount;// Offset=0x8 Size=0x4
    unsigned long VertexStart;// Offset=0xc Size=0x4
    unsigned long VertexCount;// Offset=0x10 Size=0x4
};

struct ID3DXPMesh// Size=0x4 (Id=818)
{
    struct ID3DXPMeshVtbl * lpVtbl;// Offset=0x0 Size=0x4
};

struct _D3DFIELD_STATUS// Size=0x8 (Id=835)
{
    enum _D3DFIELDTYPE Field;// Offset=0x0 Size=0x4
    unsigned int VBlankCount;// Offset=0x4 Size=0x4
};

struct _D3DXBONECOMBINATION// Size=0x18 (Id=851)
{
    unsigned long AttribId;// Offset=0x0 Size=0x4
    unsigned long FaceStart;// Offset=0x4 Size=0x4
    unsigned long FaceCount;// Offset=0x8 Size=0x4
    unsigned long VertexStart;// Offset=0xc Size=0x4
    unsigned long VertexCount;// Offset=0x10 Size=0x4
    unsigned long * BoneId;// Offset=0x14 Size=0x4
};

struct _D3DRECTPATCH_INFO// Size=0x1c (Id=867)
{
    unsigned int StartVertexOffsetWidth;// Offset=0x0 Size=0x4
    unsigned int StartVertexOffsetHeight;// Offset=0x4 Size=0x4
    unsigned int Width;// Offset=0x8 Size=0x4
    unsigned int Height;// Offset=0xc Size=0x4
    unsigned int Stride;// Offset=0x10 Size=0x4
    enum _D3DBASISTYPE Basis;// Offset=0x14 Size=0x4
    enum _D3DORDERTYPE Order;// Offset=0x18 Size=0x4
};

struct D3DXVECTOR4// Size=0x10 (Id=894)
{
    float x;// Offset=0x0 Size=0x4
    float y;// Offset=0x4 Size=0x4
    float z;// Offset=0x8 Size=0x4
    float w;// Offset=0xc Size=0x4
};

struct ID3DXMeshVtbl// Size=0x68 (Id=899)
{
    long  ( * QueryInterface)(struct IDirectXFileBinary * ,const struct _GUID * ,void ** );// Offset=0x0 Size=0x4
    unsigned long  ( * AddRef)(struct IDirectXFileBinary * );// Offset=0x4 Size=0x4
    unsigned long  ( * Release)(struct IDirectXFileBinary * );// Offset=0x8 Size=0x4
    long  ( * DrawSubset)(struct IDirectXFileBinary * ,unsigned long );// Offset=0xc Size=0x4
    unsigned long  ( * GetNumFaces)(struct IDirectXFileBinary * );// Offset=0x10 Size=0x4
    unsigned long  ( * GetNumVertices)(struct IDirectXFileBinary * );// Offset=0x14 Size=0x4
    unsigned long  ( * GetFVF)(struct IDirectXFileBinary * );// Offset=0x18 Size=0x4
    long  ( * GetDeclaration)(struct IDirectXFileBinary * ,unsigned long * );// Offset=0x1c Size=0x4
    unsigned long  ( * GetOptions)(struct IDirectXFileBinary * );// Offset=0x20 Size=0x4
    long  ( * GetDevice)(struct IDirectXFileBinary * ,struct D3DDevice ** );// Offset=0x24 Size=0x4
    long  ( * CloneMeshFVF)(struct IDirectXFileBinary * ,unsigned long ,unsigned long ,struct D3DDevice * ,struct ID3DXMesh ** );// Offset=0x28 Size=0x4
    long  ( * CloneMesh)(struct IDirectXFileBinary * ,unsigned long ,unsigned long * ,struct D3DDevice * ,struct ID3DXMesh ** );// Offset=0x2c Size=0x4
    long  ( * GetVertexBuffer)(struct IDirectXFileBinary * ,struct D3DVertexBuffer ** );// Offset=0x30 Size=0x4
    long  ( * GetIndexBuffer)(struct IDirectXFileBinary * ,struct D3DIndexBuffer ** );// Offset=0x34 Size=0x4
    long  ( * LockVertexBuffer)(struct IDirectXFileBinary * ,unsigned long ,unsigned char ** );// Offset=0x38 Size=0x4
    long  ( * UnlockVertexBuffer)(struct IDirectXFileBinary * );// Offset=0x3c Size=0x4
    long  ( * LockIndexBuffer)(struct IDirectXFileBinary * ,unsigned long ,unsigned char ** );// Offset=0x40 Size=0x4
    long  ( * UnlockIndexBuffer)(struct IDirectXFileBinary * );// Offset=0x44 Size=0x4
    long  ( * GetAttributeTable)(struct IDirectXFileBinary * ,struct _D3DXATTRIBUTERANGE * ,unsigned long * );// Offset=0x48 Size=0x4
    long  ( * LockAttributeBuffer)(struct IDirectXFileBinary * ,unsigned long ,unsigned long ** );// Offset=0x4c Size=0x4
    long  ( * UnlockAttributeBuffer)(struct IDirectXFileBinary * );// Offset=0x50 Size=0x4
    long  ( * ConvertPointRepsToAdjacency)(struct IDirectXFileBinary * ,unsigned long * ,unsigned long * );// Offset=0x54 Size=0x4
    long  ( * ConvertAdjacencyToPointReps)(struct IDirectXFileBinary * ,unsigned long * ,unsigned long * );// Offset=0x58 Size=0x4
    long  ( * GenerateAdjacency)(struct IDirectXFileBinary * ,float ,unsigned long * );// Offset=0x5c Size=0x4
    long  ( * Optimize)(struct IDirectXFileBinary * ,unsigned long ,unsigned long * ,unsigned long * ,unsigned long * ,struct ID3DXBuffer ** ,struct ID3DXMesh ** );// Offset=0x60 Size=0x4
    long  ( * OptimizeInplace)(struct IDirectXFileBinary * ,unsigned long ,unsigned long * ,unsigned long * ,unsigned long * ,struct ID3DXBuffer ** );// Offset=0x64 Size=0x4
};

struct ID3DXEffectVtbl// Size=0x5c (Id=902)
{
    long  ( * QueryInterface)(struct IDirectXFileBinary * ,const struct _GUID * ,void ** );// Offset=0x0 Size=0x4
    unsigned long  ( * AddRef)(struct IDirectXFileBinary * );// Offset=0x4 Size=0x4
    unsigned long  ( * Release)(struct IDirectXFileBinary * );// Offset=0x8 Size=0x4
    long  ( * GetDevice)(struct IDirectXFileBinary * ,struct D3DDevice ** );// Offset=0xc Size=0x4
    long  ( * GetDesc)(struct IDirectXFileBinary * ,struct _D3DXEFFECT_DESC * );// Offset=0x10 Size=0x4
    long  ( * GetParameterDesc)(struct IDirectXFileBinary * ,unsigned int ,struct _D3DXPARAMETER_DESC * );// Offset=0x14 Size=0x4
    long  ( * GetTechniqueDesc)(struct IDirectXFileBinary * ,unsigned int ,struct _D3DXTECHNIQUE_DESC * );// Offset=0x18 Size=0x4
    long  ( * SetDword)(struct IDirectXFileBinary * ,unsigned long ,unsigned long );// Offset=0x1c Size=0x4
    long  ( * GetDword)(struct IDirectXFileBinary * ,unsigned long ,unsigned long * );// Offset=0x20 Size=0x4
    long  ( * SetFloat)(struct IDirectXFileBinary * ,unsigned long ,float );// Offset=0x24 Size=0x4
    long  ( * GetFloat)(struct IDirectXFileBinary * ,unsigned long ,float * );// Offset=0x28 Size=0x4
    long  ( * SetVector)(struct IDirectXFileBinary * ,unsigned long ,struct D3DXVECTOR4 * );// Offset=0x2c Size=0x4
    long  ( * GetVector)(struct IDirectXFileBinary * ,unsigned long ,struct D3DXVECTOR4 * );// Offset=0x30 Size=0x4
    long  ( * SetMatrix)(struct IDirectXFileBinary * ,unsigned long ,struct _D3DMATRIX * );// Offset=0x34 Size=0x4
    long  ( * GetMatrix)(struct IDirectXFileBinary * ,unsigned long ,struct _D3DMATRIX * );// Offset=0x38 Size=0x4
    long  ( * SetTexture)(struct IDirectXFileBinary * ,unsigned long ,struct D3DBaseTexture * );// Offset=0x3c Size=0x4
    long  ( * GetTexture)(struct IDirectXFileBinary * ,unsigned long ,struct D3DBaseTexture ** );// Offset=0x40 Size=0x4
    long  ( * SetVertexShader)(struct IDirectXFileBinary * ,unsigned long ,unsigned long );// Offset=0x44 Size=0x4
    long  ( * GetVertexShader)(struct IDirectXFileBinary * ,unsigned long ,unsigned long * );// Offset=0x48 Size=0x4
    long  ( * SetPixelShader)(struct IDirectXFileBinary * ,unsigned long ,unsigned long );// Offset=0x4c Size=0x4
    long  ( * GetPixelShader)(struct IDirectXFileBinary * ,unsigned long ,unsigned long * );// Offset=0x50 Size=0x4
    long  ( * GetTechnique)(struct IDirectXFileBinary * ,unsigned int ,struct ID3DXTechnique ** );// Offset=0x54 Size=0x4
    long  ( * CloneEffect)(struct IDirectXFileBinary * ,struct D3DDevice * ,unsigned long ,struct ID3DXEffect ** );// Offset=0x58 Size=0x4
};

struct _D3DRECT// Size=0x10 (Id=915)
{
    long x1;// Offset=0x0 Size=0x4
    long y1;// Offset=0x4 Size=0x4
    long x2;// Offset=0x8 Size=0x4
    long y2;// Offset=0xc Size=0x4
};

struct _D3DADAPTER_IDENTIFIER8// Size=0x42c (Id=951)
{
    char Driver[512];// Offset=0x0 Size=0x200
    char Description[512];// Offset=0x200 Size=0x200
    union _LARGE_INTEGER DriverVersion;// Offset=0x400 Size=0x8
    unsigned long VendorId;// Offset=0x408 Size=0x4
    unsigned long DeviceId;// Offset=0x40c Size=0x4
    unsigned long SubSysId;// Offset=0x410 Size=0x4
    unsigned long Revision;// Offset=0x414 Size=0x4
    struct _GUID DeviceIdentifier;// Offset=0x418 Size=0x10
    unsigned long WHQLLevel;// Offset=0x428 Size=0x4
};

struct _D3DXRTS_DESC// Size=0x14 (Id=959)
{
    unsigned int Width;// Offset=0x0 Size=0x4
    unsigned int Height;// Offset=0x4 Size=0x4
    enum _D3DFORMAT Format;// Offset=0x8 Size=0x4
    int DepthStencil;// Offset=0xc Size=0x4
    enum _D3DFORMAT DepthStencilFormat;// Offset=0x10 Size=0x4
};

struct _D3DINDEXBUFFER_DESC// Size=0x8 (Id=968)
{
    enum _D3DFORMAT Format;// Offset=0x0 Size=0x4
    enum _D3DRESOURCETYPE Type;// Offset=0x4 Size=0x4
};

struct ID3DXTechnique// Size=0x4 (Id=1002)
{
    struct ID3DXTechniqueVtbl * lpVtbl;// Offset=0x0 Size=0x4
};

struct ID3DXSkinMesh// Size=0x4 (Id=1014)
{
    struct ID3DXSkinMeshVtbl * lpVtbl;// Offset=0x0 Size=0x4
};

struct D3DXMATERIAL// Size=0x48 (Id=1019)
{
    struct _D3DMATERIAL8 MatD3D;// Offset=0x0 Size=0x44
    char * pTextureFilename;// Offset=0x44 Size=0x4
};

struct _D3DPixelShaderDef// Size=0xf0 (Id=1029)
{
    unsigned long PSAlphaInputs[8];// Offset=0x0 Size=0x20
    unsigned long PSFinalCombinerInputsABCD;// Offset=0x20 Size=0x4
    unsigned long PSFinalCombinerInputsEFG;// Offset=0x24 Size=0x4
    unsigned long PSConstant0[8];// Offset=0x28 Size=0x20
    unsigned long PSConstant1[8];// Offset=0x48 Size=0x20
    unsigned long PSAlphaOutputs[8];// Offset=0x68 Size=0x20
    unsigned long PSRGBInputs[8];// Offset=0x88 Size=0x20
    unsigned long PSCompareMode;// Offset=0xa8 Size=0x4
    unsigned long PSFinalCombinerConstant0;// Offset=0xac Size=0x4
    unsigned long PSFinalCombinerConstant1;// Offset=0xb0 Size=0x4
    unsigned long PSRGBOutputs[8];// Offset=0xb4 Size=0x20
    unsigned long PSCombinerCount;// Offset=0xd4 Size=0x4
    unsigned long PSTextureModes;// Offset=0xd8 Size=0x4
    unsigned long PSDotMapping;// Offset=0xdc Size=0x4
    unsigned long PSInputTexture;// Offset=0xe0 Size=0x4
    unsigned long PSC0Mapping;// Offset=0xe4 Size=0x4
    unsigned long PSC1Mapping;// Offset=0xe8 Size=0x4
    unsigned long PSFinalCombinerConstants;// Offset=0xec Size=0x4
};

struct _D3DVIEWPORT8// Size=0x18 (Id=1032)
{
    unsigned long X;// Offset=0x0 Size=0x4
    unsigned long Y;// Offset=0x4 Size=0x4
    unsigned long Width;// Offset=0x8 Size=0x4
    unsigned long Height;// Offset=0xc Size=0x4
    float MinZ;// Offset=0x10 Size=0x4
    float MaxZ;// Offset=0x14 Size=0x4
};

struct ID3DXRenderToSurfaceVtbl// Size=0x1c (Id=1033)
{
    long  ( * QueryInterface)(void * ,const struct _GUID * ,void ** );// Offset=0x0 Size=0x4
    unsigned long  ( * AddRef)(void * );// Offset=0x4 Size=0x4
    unsigned long  ( * Release)(void * );// Offset=0x8 Size=0x4
    long  ( * GetDevice)(void * ,struct D3DDevice ** );// Offset=0xc Size=0x4
    long  ( * GetDesc)(void * ,struct _D3DXRTS_DESC * );// Offset=0x10 Size=0x4
    long  ( * BeginScene)(void * ,struct D3DSurface * ,struct _D3DVIEWPORT8 * );// Offset=0x14 Size=0x4
    long  ( * EndScene)(void * );// Offset=0x18 Size=0x4
};

struct D3DXVECTOR2// Size=0x8 (Id=1036)
{
    float x;// Offset=0x0 Size=0x4
    float y;// Offset=0x4 Size=0x4
};

struct _D3DMATRIX// Size=0x40 (Id=1037)
{
    union // Size=0x40 (Id=0)
    {
        float _11;// Offset=0x0 Size=0x4
        float _12;// Offset=0x4 Size=0x4
        float _13;// Offset=0x8 Size=0x4
        float _14;// Offset=0xc Size=0x4
        float _21;// Offset=0x10 Size=0x4
        float _22;// Offset=0x14 Size=0x4
        float _23;// Offset=0x18 Size=0x4
        float _24;// Offset=0x1c Size=0x4
        float _31;// Offset=0x20 Size=0x4
        float _32;// Offset=0x24 Size=0x4
        float _33;// Offset=0x28 Size=0x4
        float _34;// Offset=0x2c Size=0x4
        float _41;// Offset=0x30 Size=0x4
        float _42;// Offset=0x34 Size=0x4
        float _43;// Offset=0x38 Size=0x4
        float _44;// Offset=0x3c Size=0x4
        float m[4][4];// Offset=0x0 Size=0x40
    };
};

struct ID3DXSpriteVtbl// Size=0x20 (Id=1038)
{
    long  ( * QueryInterface)(void * ,const struct _GUID * ,void ** );// Offset=0x0 Size=0x4
    unsigned long  ( * AddRef)(void * );// Offset=0x4 Size=0x4
    unsigned long  ( * Release)(void * );// Offset=0x8 Size=0x4
    long  ( * GetDevice)(void * ,struct D3DDevice ** );// Offset=0xc Size=0x4
    long  ( * Begin)(void * );// Offset=0x10 Size=0x4
    long  ( * Draw)(void * ,struct D3DTexture * ,struct tagRECT * ,struct D3DXVECTOR2 * ,struct D3DXVECTOR2 * ,float ,struct D3DXVECTOR2 * ,unsigned long );// Offset=0x14 Size=0x4
    long  ( * DrawTransform)(void * ,struct D3DTexture * ,struct tagRECT * ,struct _D3DMATRIX * ,unsigned long );// Offset=0x18 Size=0x4
    long  ( * End)(void * );// Offset=0x1c Size=0x4
};

struct ID3DXSprite// Size=0x4 (Id=1042)
{
    struct ID3DXSpriteVtbl * lpVtbl;// Offset=0x0 Size=0x4
};

struct ID3DXSkinMeshVtbl// Size=0x70 (Id=1060)
{
    long  ( * QueryInterface)(struct IDirectXFileBinary * ,const struct _GUID * ,void ** );// Offset=0x0 Size=0x4
    unsigned long  ( * AddRef)(struct IDirectXFileBinary * );// Offset=0x4 Size=0x4
    unsigned long  ( * Release)(struct IDirectXFileBinary * );// Offset=0x8 Size=0x4
    unsigned long  ( * GetNumFaces)(struct IDirectXFileBinary * );// Offset=0xc Size=0x4
    unsigned long  ( * GetNumVertices)(struct IDirectXFileBinary * );// Offset=0x10 Size=0x4
    unsigned long  ( * GetFVF)(struct IDirectXFileBinary * );// Offset=0x14 Size=0x4
    long  ( * GetDeclaration)(struct IDirectXFileBinary * ,unsigned long * );// Offset=0x18 Size=0x4
    unsigned long  ( * GetOptions)(struct IDirectXFileBinary * );// Offset=0x1c Size=0x4
    long  ( * GetDevice)(struct IDirectXFileBinary * ,struct D3DDevice ** );// Offset=0x20 Size=0x4
    long  ( * GetVertexBuffer)(struct IDirectXFileBinary * ,struct D3DVertexBuffer ** );// Offset=0x24 Size=0x4
    long  ( * GetIndexBuffer)(struct IDirectXFileBinary * ,struct D3DIndexBuffer ** );// Offset=0x28 Size=0x4
    long  ( * LockVertexBuffer)(struct IDirectXFileBinary * ,unsigned long ,unsigned char ** );// Offset=0x2c Size=0x4
    long  ( * UnlockVertexBuffer)(struct IDirectXFileBinary * );// Offset=0x30 Size=0x4
    long  ( * LockIndexBuffer)(struct IDirectXFileBinary * ,unsigned long ,unsigned char ** );// Offset=0x34 Size=0x4
    long  ( * UnlockIndexBuffer)(struct IDirectXFileBinary * );// Offset=0x38 Size=0x4
    long  ( * LockAttributeBuffer)(struct IDirectXFileBinary * ,unsigned long ,unsigned long ** );// Offset=0x3c Size=0x4
    long  ( * UnlockAttributeBuffer)(struct IDirectXFileBinary * );// Offset=0x40 Size=0x4
    unsigned long  ( * GetNumBones)(struct IDirectXFileBinary * );// Offset=0x44 Size=0x4
    long  ( * GetOriginalMesh)(struct IDirectXFileBinary * ,struct ID3DXMesh ** );// Offset=0x48 Size=0x4
    long  ( * SetBoneInfluence)(struct IDirectXFileBinary * ,unsigned long ,unsigned long ,unsigned long * ,float * );// Offset=0x4c Size=0x4
    unsigned long  ( * GetNumBoneInfluences)(struct IDirectXFileBinary * ,unsigned long );// Offset=0x50 Size=0x4
    long  ( * GetBoneInfluence)(struct IDirectXFileBinary * ,unsigned long ,unsigned long * ,float * );// Offset=0x54 Size=0x4
    long  ( * GetMaxVertexInfluences)(struct IDirectXFileBinary * ,unsigned long * );// Offset=0x58 Size=0x4
    long  ( * GetMaxFaceInfluences)(struct IDirectXFileBinary * ,unsigned long * );// Offset=0x5c Size=0x4
    long  ( * ConvertToBlendedMesh)(struct IDirectXFileBinary * ,unsigned long ,const unsigned long * ,unsigned long * ,unsigned long * ,struct ID3DXBuffer ** ,struct ID3DXMesh ** );// Offset=0x60 Size=0x4
    long  ( * ConvertToIndexedBlendedMesh)(struct IDirectXFileBinary * ,unsigned long ,const unsigned long * ,unsigned long ,unsigned long * ,unsigned long * ,struct ID3DXBuffer ** ,struct ID3DXMesh ** );// Offset=0x64 Size=0x4
    long  ( * GenerateSkinnedMesh)(struct IDirectXFileBinary * ,unsigned long ,float ,const unsigned long * ,unsigned long * ,struct ID3DXMesh ** );// Offset=0x68 Size=0x4
    long  ( * UpdateSkinnedMesh)(struct IDirectXFileBinary * ,struct _D3DMATRIX * ,struct ID3DXMesh * );// Offset=0x6c Size=0x4
};

struct D3DVertexBuffer// Size=0xc (Id=1065)
{
    unsigned long Common;// Offset=0x0 Size=0x4
    unsigned long Data;// Offset=0x4 Size=0x4
    unsigned long Lock;// Offset=0x8 Size=0x4
};

struct D3DXPLANE// Size=0x10 (Id=1072)
{
    float a;// Offset=0x0 Size=0x4
    float b;// Offset=0x4 Size=0x4
    float c;// Offset=0x8 Size=0x4
    float d;// Offset=0xc Size=0x4
};

struct D3DPushBuffer// Size=0x14 (Id=1086)
{
    unsigned long Common;// Offset=0x0 Size=0x4
    unsigned long Data;// Offset=0x4 Size=0x4
    unsigned long Lock;// Offset=0x8 Size=0x4
    unsigned long Size;// Offset=0xc Size=0x4
    unsigned long AllocationSize;// Offset=0x10 Size=0x4
};

struct _D3DVECTOR// Size=0xc (Id=1090)
{
    float x;// Offset=0x0 Size=0x4
    float y;// Offset=0x4 Size=0x4
    float z;// Offset=0x8 Size=0x4
};

struct ID3DXMatrixStackVtbl// Size=0x48 (Id=1091)
{
    long  ( * QueryInterface)(struct ID3DXMatrixStack * ,const struct _GUID * ,void ** );// Offset=0x0 Size=0x4
    unsigned long  ( * AddRef)(struct ID3DXMatrixStack * );// Offset=0x4 Size=0x4
    unsigned long  ( * Release)(struct ID3DXMatrixStack * );// Offset=0x8 Size=0x4
    long  ( * Pop)(struct ID3DXMatrixStack * );// Offset=0xc Size=0x4
    long  ( * Push)(struct ID3DXMatrixStack * );// Offset=0x10 Size=0x4
    long  ( * LoadIdentity)(struct ID3DXMatrixStack * );// Offset=0x14 Size=0x4
    long  ( * LoadMatrix)(struct ID3DXMatrixStack * ,struct _D3DMATRIX * );// Offset=0x18 Size=0x4
    long  ( * MultMatrix)(struct ID3DXMatrixStack * ,struct _D3DMATRIX * );// Offset=0x1c Size=0x4
    long  ( * MultMatrixLocal)(struct ID3DXMatrixStack * ,struct _D3DMATRIX * );// Offset=0x20 Size=0x4
    long  ( * RotateAxis)(struct ID3DXMatrixStack * ,struct _D3DVECTOR * ,float );// Offset=0x24 Size=0x4
    long  ( * RotateAxisLocal)(struct ID3DXMatrixStack * ,struct _D3DVECTOR * ,float );// Offset=0x28 Size=0x4
    long  ( * RotateYawPitchRoll)(struct ID3DXMatrixStack * ,float ,float ,float );// Offset=0x2c Size=0x4
    long  ( * RotateYawPitchRollLocal)(struct ID3DXMatrixStack * ,float ,float ,float );// Offset=0x30 Size=0x4
    long  ( * Scale)(struct ID3DXMatrixStack * ,float ,float ,float );// Offset=0x34 Size=0x4
    long  ( * ScaleLocal)(struct ID3DXMatrixStack * ,float ,float ,float );// Offset=0x38 Size=0x4
    long  ( * Translate)(struct ID3DXMatrixStack * ,float ,float ,float );// Offset=0x3c Size=0x4
    long  ( * TranslateLocal)(struct ID3DXMatrixStack * ,float ,float ,float );// Offset=0x40 Size=0x4
    struct _D3DMATRIX *  ( * GetTop)(struct ID3DXMatrixStack * );// Offset=0x44 Size=0x4
};

struct ID3DXPMeshVtbl// Size=0x78 (Id=1106)
{
    long  ( * QueryInterface)(struct IDirectXFileBinary * ,const struct _GUID * ,void ** );// Offset=0x0 Size=0x4
    unsigned long  ( * AddRef)(struct IDirectXFileBinary * );// Offset=0x4 Size=0x4
    unsigned long  ( * Release)(struct IDirectXFileBinary * );// Offset=0x8 Size=0x4
    long  ( * DrawSubset)(struct IDirectXFileBinary * ,unsigned long );// Offset=0xc Size=0x4
    unsigned long  ( * GetNumFaces)(struct IDirectXFileBinary * );// Offset=0x10 Size=0x4
    unsigned long  ( * GetNumVertices)(struct IDirectXFileBinary * );// Offset=0x14 Size=0x4
    unsigned long  ( * GetFVF)(struct IDirectXFileBinary * );// Offset=0x18 Size=0x4
    long  ( * GetDeclaration)(struct IDirectXFileBinary * ,unsigned long * );// Offset=0x1c Size=0x4
    unsigned long  ( * GetOptions)(struct IDirectXFileBinary * );// Offset=0x20 Size=0x4
    long  ( * GetDevice)(struct IDirectXFileBinary * ,struct D3DDevice ** );// Offset=0x24 Size=0x4
    long  ( * CloneMeshFVF)(struct IDirectXFileBinary * ,unsigned long ,unsigned long ,struct D3DDevice * ,struct ID3DXMesh ** );// Offset=0x28 Size=0x4
    long  ( * CloneMesh)(struct IDirectXFileBinary * ,unsigned long ,unsigned long * ,struct D3DDevice * ,struct ID3DXMesh ** );// Offset=0x2c Size=0x4
    long  ( * GetVertexBuffer)(struct IDirectXFileBinary * ,struct D3DVertexBuffer ** );// Offset=0x30 Size=0x4
    long  ( * GetIndexBuffer)(struct IDirectXFileBinary * ,struct D3DIndexBuffer ** );// Offset=0x34 Size=0x4
    long  ( * LockVertexBuffer)(struct IDirectXFileBinary * ,unsigned long ,unsigned char ** );// Offset=0x38 Size=0x4
    long  ( * UnlockVertexBuffer)(struct IDirectXFileBinary * );// Offset=0x3c Size=0x4
    long  ( * LockIndexBuffer)(struct IDirectXFileBinary * ,unsigned long ,unsigned char ** );// Offset=0x40 Size=0x4
    long  ( * UnlockIndexBuffer)(struct IDirectXFileBinary * );// Offset=0x44 Size=0x4
    long  ( * GetAttributeTable)(struct IDirectXFileBinary * ,struct _D3DXATTRIBUTERANGE * ,unsigned long * );// Offset=0x48 Size=0x4
    long  ( * ClonePMeshFVF)(struct IDirectXFileBinary * ,unsigned long ,unsigned long ,struct D3DDevice * ,struct ID3DXPMesh ** );// Offset=0x4c Size=0x4
    long  ( * ClonePMesh)(struct IDirectXFileBinary * ,unsigned long ,unsigned long * ,struct D3DDevice * ,struct ID3DXPMesh ** );// Offset=0x50 Size=0x4
    long  ( * SetNumFaces)(struct IDirectXFileBinary * ,unsigned long );// Offset=0x54 Size=0x4
    long  ( * SetNumVertices)(struct IDirectXFileBinary * ,unsigned long );// Offset=0x58 Size=0x4
    unsigned long  ( * GetMaxFaces)(struct IDirectXFileBinary * );// Offset=0x5c Size=0x4
    unsigned long  ( * GetMinFaces)(struct IDirectXFileBinary * );// Offset=0x60 Size=0x4
    unsigned long  ( * GetMaxVertices)(struct IDirectXFileBinary * );// Offset=0x64 Size=0x4
    unsigned long  ( * GetMinVertices)(struct IDirectXFileBinary * );// Offset=0x68 Size=0x4
    long  ( * Save)(struct IDirectXFileBinary * ,struct IStream * ,struct D3DXMATERIAL * ,unsigned long );// Offset=0x6c Size=0x4
    long  ( * Optimize)(struct IDirectXFileBinary * ,unsigned long ,unsigned long * ,unsigned long * ,struct ID3DXBuffer ** ,struct ID3DXMesh ** );// Offset=0x70 Size=0x4
    long  ( * GetAdjacency)(struct IDirectXFileBinary * ,unsigned long * );// Offset=0x74 Size=0x4
};

struct D3DFixup// Size=0x18 (Id=1113)
{
    unsigned long Common;// Offset=0x0 Size=0x4
    unsigned long Data;// Offset=0x4 Size=0x4
    unsigned long Lock;// Offset=0x8 Size=0x4
    unsigned long Run;// Offset=0xc Size=0x4
    unsigned long Next;// Offset=0x10 Size=0x4
    unsigned long Size;// Offset=0x14 Size=0x4
};

enum D3DX::J_COLOR_SPACE
{
    JCS_UNKNOWN=0,
    JCS_GRAYSCALE=1,
    JCS_RGB=2,
    JCS_YCbCr=3,
    JCS_CMYK=4,
    JCS_YCCK=5
};

enum D3DX::J_DCT_METHOD
{
    JDCT_ISLOW=0,
    JDCT_IFAST=1,
    JDCT_FLOAT=2
};

enum D3DX::J_DITHER_MODE
{
    JDITHER_NONE=0,
    JDITHER_ORDERED=1,
    JDITHER_FS=2
};

struct _D3DCOLORVALUE// Size=0x10 (Id=1120)
{
    float r;// Offset=0x0 Size=0x4
    float g;// Offset=0x4 Size=0x4
    float b;// Offset=0x8 Size=0x4
    float a;// Offset=0xc Size=0x4
};

struct D3DXCOLOR// Size=0x10 (Id=1121)
{
    float r;// Offset=0x0 Size=0x4
    float g;// Offset=0x4 Size=0x4
    float b;// Offset=0x8 Size=0x4
    float a;// Offset=0xc Size=0x4
};

struct D3DXCOLOR// Size=0x10 (Id=1122)
{
    union // Size=0x55 (Id=0)
    {
        void D3DXCOLOR(float ,float ,float ,float );// Offset=0x0 Size=0x20
        void D3DXCOLOR(struct _D3DCOLORVALUE & );
        void D3DXCOLOR(float * );
        void D3DXCOLOR(unsigned long );// Offset=0x0 Size=0x55
        void D3DXCOLOR();// Offset=0x0 Size=0x3
        unsigned long operator unsigned long();
        float * operator float *();
        float * operator const float *();
        struct _D3DCOLORVALUE * operator struct _D3DCOLORVALUE *();
        struct _D3DCOLORVALUE * operator const struct _D3DCOLORVALUE *();
        struct _D3DCOLORVALUE & operator struct _D3DCOLORVALUE &();
        struct _D3DCOLORVALUE & operator const struct _D3DCOLORVALUE &();
        struct D3DXCOLOR & operator+=(struct D3DXCOLOR & );
        struct D3DXCOLOR & operator-=(struct D3DXCOLOR & );
        struct D3DXCOLOR & operator*=(float );
        struct D3DXCOLOR & operator/=(float );
        struct D3DXCOLOR operator+(struct D3DXCOLOR & );// Offset=0x0 Size=0x2c
        struct D3DXCOLOR operator+();
        struct D3DXCOLOR operator-(struct D3DXCOLOR & );// Offset=0x0 Size=0x2c
        struct D3DXCOLOR operator-();
        struct D3DXCOLOR operator*(float );// Offset=0x0 Size=0x2c
        struct D3DXCOLOR operator/(float );
        int operator==(struct D3DXCOLOR & );// Offset=0x0 Size=0x40
        int operator!=(struct D3DXCOLOR & );
        float r;// Offset=0x0 Size=0x4
        float g;// Offset=0x4 Size=0x4
        float b;// Offset=0x8 Size=0x4
        float a;// Offset=0xc Size=0x4
    };
};

struct D3DXVECTOR3 : public _D3DVECTOR// Size=0xc (Id=1129)
{
    void D3DXVECTOR3(float ,float ,float );
    void D3DXVECTOR3(struct _D3DVECTOR & );
    void D3DXVECTOR3(float * );
    void D3DXVECTOR3();// Offset=0x0 Size=0x3
    float * operator float *();
    float * operator const float *();
    struct D3DXVECTOR3 & operator+=(struct D3DXVECTOR3 & );
    struct D3DXVECTOR3 & operator-=(struct D3DXVECTOR3 & );
    struct D3DXVECTOR3 & operator*=(float );
    struct D3DXVECTOR3 & operator/=(float );
    struct D3DXVECTOR3 operator+(struct D3DXVECTOR3 & );
    struct D3DXVECTOR3 operator+();
    struct D3DXVECTOR3 operator-(struct D3DXVECTOR3 & );
    struct D3DXVECTOR3 operator-();
    struct D3DXVECTOR3 operator*(float );
    struct D3DXVECTOR3 operator/(float );
    int operator==(struct D3DXVECTOR3 & );
    int operator!=(struct D3DXVECTOR3 & );
};

struct D3DXVECTOR3 : public _D3DVECTOR// Size=0xc (Id=1130)
{
    void D3DXVECTOR3(float ,float ,float );
    void D3DXVECTOR3(struct _D3DVECTOR & );
    void D3DXVECTOR3(float * );
    void D3DXVECTOR3();// Offset=0x0 Size=0x3
    float * operator float *();
    float * operator const float *();
    struct D3DXVECTOR3 & operator+=(struct D3DXVECTOR3 & );
    struct D3DXVECTOR3 & operator-=(struct D3DXVECTOR3 & );
    struct D3DXVECTOR3 & operator*=(float );
    struct D3DXVECTOR3 & operator/=(float );
    struct D3DXVECTOR3 operator+(struct D3DXVECTOR3 & );
    struct D3DXVECTOR3 operator+();
    struct D3DXVECTOR3 operator-(struct D3DXVECTOR3 & );
    struct D3DXVECTOR3 operator-();
    struct D3DXVECTOR3 operator*(float );
    struct D3DXVECTOR3 operator/(float );
    int operator==(struct D3DXVECTOR3 & );
    int operator!=(struct D3DXVECTOR3 & );
};

struct D3DBaseTexture : public D3DPixelContainer// Size=0x14 (Id=1133)
{
    unsigned long GetLevelCount();// Offset=0x0 Size=0x5
};

struct ID3DXBaseMesh// Size=0x4 (Id=1161)
{
    struct ID3DXBaseMeshVtbl * lpVtbl;// Offset=0x0 Size=0x4
};

struct ID3DXBaseMesh : public IUnknown// Size=0x4 (Id=1162)
{
    long QueryInterface(struct _GUID & ,void ** );
    unsigned long AddRef();
    unsigned long Release();
    long DrawSubset(unsigned long );
    unsigned long GetNumFaces();
    unsigned long GetNumVertices();
    unsigned long GetFVF();
    long GetDeclaration(unsigned long * );
    unsigned long GetOptions();
    long GetDevice(struct D3DDevice ** );
    long CloneMeshFVF(unsigned long ,unsigned long ,struct D3DDevice * ,struct ID3DXMesh ** );
    long CloneMesh(unsigned long ,unsigned long * ,struct D3DDevice * ,struct ID3DXMesh ** );
    long GetVertexBuffer(struct D3DVertexBuffer ** );
    long GetIndexBuffer(struct D3DIndexBuffer ** );
    long LockVertexBuffer(unsigned long ,unsigned char ** );
    long UnlockVertexBuffer();
    long LockIndexBuffer(unsigned long ,unsigned char ** );
    long UnlockIndexBuffer();
    long GetAttributeTable(struct _D3DXATTRIBUTERANGE * ,unsigned long * );
    void ID3DXBaseMesh(struct ID3DXBaseMesh & );
    void ID3DXBaseMesh();
};

struct ID3DXMesh// Size=0x4 (Id=1163)
{
    struct ID3DXMeshVtbl * lpVtbl;// Offset=0x0 Size=0x4
};

struct ID3DXMesh : public ID3DXBaseMesh// Size=0x4 (Id=1164)
{
    long QueryInterface(struct _GUID & ,void ** );
    unsigned long AddRef();
    unsigned long Release();
    long DrawSubset(unsigned long );
    unsigned long GetNumFaces();
    unsigned long GetNumVertices();
    unsigned long GetFVF();
    long GetDeclaration(unsigned long * );
    unsigned long GetOptions();
    long GetDevice(struct D3DDevice ** );
    long CloneMeshFVF(unsigned long ,unsigned long ,struct D3DDevice * ,struct ID3DXMesh ** );
    long CloneMesh(unsigned long ,unsigned long * ,struct D3DDevice * ,struct ID3DXMesh ** );
    long GetVertexBuffer(struct D3DVertexBuffer ** );
    long GetIndexBuffer(struct D3DIndexBuffer ** );
    long LockVertexBuffer(unsigned long ,unsigned char ** );
    long UnlockVertexBuffer();
    long LockIndexBuffer(unsigned long ,unsigned char ** );
    long UnlockIndexBuffer();
    long GetAttributeTable(struct _D3DXATTRIBUTERANGE * ,unsigned long * );
    long LockAttributeBuffer(unsigned long ,unsigned long ** );
    long UnlockAttributeBuffer();
    long ConvertPointRepsToAdjacency(unsigned long * ,unsigned long * );
    long ConvertAdjacencyToPointReps(unsigned long * ,unsigned long * );
    long GenerateAdjacency(float ,unsigned long * );
    long Optimize(unsigned long ,unsigned long * ,unsigned long * ,unsigned long * ,struct ID3DXBuffer ** ,struct ID3DXMesh ** );
    long OptimizeInplace(unsigned long ,unsigned long * ,unsigned long * ,unsigned long * ,struct ID3DXBuffer ** );
    void ID3DXMesh(struct ID3DXMesh & );
    void ID3DXMesh();
};

struct D3DXVECTOR2// Size=0x8 (Id=1165)
{
    void D3DXVECTOR2(float ,float );
    void D3DXVECTOR2(float * );
    void D3DXVECTOR2();
    float * operator float *();
    float * operator const float *();
    struct D3DXVECTOR2 & operator+=(struct D3DXVECTOR2 & );
    struct D3DXVECTOR2 & operator-=(struct D3DXVECTOR2 & );
    struct D3DXVECTOR2 & operator*=(float );
    struct D3DXVECTOR2 & operator/=(float );
    struct D3DXVECTOR2 operator+(struct D3DXVECTOR2 & );
    struct D3DXVECTOR2 operator+();
    struct D3DXVECTOR2 operator-(struct D3DXVECTOR2 & );
    struct D3DXVECTOR2 operator-();
    struct D3DXVECTOR2 operator*(float );
    struct D3DXVECTOR2 operator/(float );
    int operator==(struct D3DXVECTOR2 & );
    int operator!=(struct D3DXVECTOR2 & );
    float x;// Offset=0x0 Size=0x4
    float y;// Offset=0x4 Size=0x4
};

struct D3DIndexBuffer : public D3DResource// Size=0xc (Id=1166)
{
    long Lock(unsigned int ,unsigned int ,unsigned char ** ,unsigned long );
    long Unlock();
    long GetDesc(struct _D3DINDEXBUFFER_DESC * );
};

struct _D3DBOX// Size=0x18 (Id=1173)
{
    unsigned int Left;// Offset=0x0 Size=0x4
    unsigned int Top;// Offset=0x4 Size=0x4
    unsigned int Right;// Offset=0x8 Size=0x4
    unsigned int Bottom;// Offset=0xc Size=0x4
    unsigned int Front;// Offset=0x10 Size=0x4
    unsigned int Back;// Offset=0x14 Size=0x4
};

struct D3DVolume : public D3DPixelContainer// Size=0x18 (Id=1174)
{
    long GetContainer(struct D3DBaseTexture ** );
    union // Size=0x1a (Id=0)
    {
        long GetDesc(struct _D3DVOLUME_DESC * );// Offset=0x0 Size=0x12
        long LockBox(struct _D3DLOCKED_BOX * ,struct _D3DBOX * ,unsigned long );// Offset=0x0 Size=0x1a
        long UnlockBox();// Offset=0x0 Size=0x5
        unsigned char __align0[15];// Offset=0x5 Size=0xf
        struct D3DBaseTexture * Parent;// Offset=0x14 Size=0x4
    };
};

struct D3DX::jpeg_decompress_struct// Size=0x1a8 (Id=1175)
{
    struct D3DX::jpeg_error_mgr * err;// Offset=0x0 Size=0x4
    struct D3DX::jpeg_memory_mgr * mem;// Offset=0x4 Size=0x4
    struct D3DX::jpeg_progress_mgr * progress;// Offset=0x8 Size=0x4
    unsigned char is_decompressor;// Offset=0xc Size=0x1
    unsigned char __align0[3];// Offset=0xd Size=0x3
    int global_state;// Offset=0x10 Size=0x4
    struct D3DX::jpeg_source_mgr * src;// Offset=0x14 Size=0x4
    unsigned int image_width;// Offset=0x18 Size=0x4
    unsigned int image_height;// Offset=0x1c Size=0x4
    int num_components;// Offset=0x20 Size=0x4
    enum D3DX::J_COLOR_SPACE jpeg_color_space;// Offset=0x24 Size=0x4
    enum D3DX::J_COLOR_SPACE out_color_space;// Offset=0x28 Size=0x4
    unsigned int scale_num;// Offset=0x2c Size=0x4
    unsigned int scale_denom;// Offset=0x30 Size=0x4
    unsigned char __align1[4];// Offset=0x34 Size=0x4
    float output_gamma;// Offset=0x38 Size=0x8
    unsigned char buffered_image;// Offset=0x40 Size=0x1
    unsigned char raw_data_out;// Offset=0x41 Size=0x1
    unsigned char __align2[2];// Offset=0x42 Size=0x2
    enum D3DX::J_DCT_METHOD dct_method;// Offset=0x44 Size=0x4
    unsigned char do_fancy_upsampling;// Offset=0x48 Size=0x1
    unsigned char do_block_smoothing;// Offset=0x49 Size=0x1
    unsigned char quantize_colors;// Offset=0x4a Size=0x1
    unsigned char __align3[1];// Offset=0x4b Size=0x1
    enum D3DX::J_DITHER_MODE dither_mode;// Offset=0x4c Size=0x4
    unsigned char two_pass_quantize;// Offset=0x50 Size=0x1
    unsigned char __align4[3];// Offset=0x51 Size=0x3
    int desired_number_of_colors;// Offset=0x54 Size=0x4
    unsigned char enable_1pass_quant;// Offset=0x58 Size=0x1
    unsigned char enable_external_quant;// Offset=0x59 Size=0x1
    unsigned char enable_2pass_quant;// Offset=0x5a Size=0x1
    unsigned char __align5[1];// Offset=0x5b Size=0x1
    unsigned int output_width;// Offset=0x5c Size=0x4
    unsigned int output_height;// Offset=0x60 Size=0x4
    int out_color_components;// Offset=0x64 Size=0x4
    int output_components;// Offset=0x68 Size=0x4
    int rec_outbuf_height;// Offset=0x6c Size=0x4
    int actual_number_of_colors;// Offset=0x70 Size=0x4
    unsigned char ** colormap;// Offset=0x74 Size=0x4
    unsigned int output_scanline;// Offset=0x78 Size=0x4
    int input_scan_number;// Offset=0x7c Size=0x4
    unsigned int input_iMCU_row;// Offset=0x80 Size=0x4
    int output_scan_number;// Offset=0x84 Size=0x4
    unsigned int output_iMCU_row;// Offset=0x88 Size=0x4
    int * coef_bits[64];// Offset=0x8c Size=0x4
    struct D3DX::JQUANT_TBL * quant_tbl_ptrs[4];// Offset=0x90 Size=0x10
    struct D3DX::JHUFF_TBL * dc_huff_tbl_ptrs[4];// Offset=0xa0 Size=0x10
    struct D3DX::JHUFF_TBL * ac_huff_tbl_ptrs[4];// Offset=0xb0 Size=0x10
    int data_precision;// Offset=0xc0 Size=0x4
    struct D3DX::jpeg_component_info * comp_info;// Offset=0xc4 Size=0x4
    unsigned char progressive_mode;// Offset=0xc8 Size=0x1
    unsigned char arith_code;// Offset=0xc9 Size=0x1
    unsigned char arith_dc_L[16];// Offset=0xca Size=0x10
    unsigned char arith_dc_U[16];// Offset=0xda Size=0x10
    unsigned char arith_ac_K[16];// Offset=0xea Size=0x10
    unsigned char __align6[2];// Offset=0xfa Size=0x2
    unsigned int restart_interval;// Offset=0xfc Size=0x4
    unsigned char saw_JFIF_marker;// Offset=0x100 Size=0x1
    unsigned char density_unit;// Offset=0x101 Size=0x1
    unsigned short X_density;// Offset=0x102 Size=0x2
    unsigned short Y_density;// Offset=0x104 Size=0x2
    unsigned char saw_Adobe_marker;// Offset=0x106 Size=0x1
    unsigned char Adobe_transform;// Offset=0x107 Size=0x1
    unsigned char CCIR601_sampling;// Offset=0x108 Size=0x1
    unsigned char __align7[3];// Offset=0x109 Size=0x3
    int max_h_samp_factor;// Offset=0x10c Size=0x4
    int max_v_samp_factor;// Offset=0x110 Size=0x4
    int min_DCT_scaled_size;// Offset=0x114 Size=0x4
    unsigned int total_iMCU_rows;// Offset=0x118 Size=0x4
    unsigned char * sample_range_limit;// Offset=0x11c Size=0x4
    int comps_in_scan;// Offset=0x120 Size=0x4
    struct D3DX::jpeg_component_info * cur_comp_info[4];// Offset=0x124 Size=0x10
    unsigned int MCUs_per_row;// Offset=0x134 Size=0x4
    unsigned int MCU_rows_in_scan;// Offset=0x138 Size=0x4
    int blocks_in_MCU;// Offset=0x13c Size=0x4
    int MCU_membership[10];// Offset=0x140 Size=0x28
    int Ss;// Offset=0x168 Size=0x4
    int Se;// Offset=0x16c Size=0x4
    int Ah;// Offset=0x170 Size=0x4
    int Al;// Offset=0x174 Size=0x4
    int unread_marker;// Offset=0x178 Size=0x4
    struct D3DX::jpeg_decomp_master * master;// Offset=0x17c Size=0x4
    struct D3DX::jpeg_d_main_controller * main;// Offset=0x180 Size=0x4
    struct D3DX::jpeg_d_coef_controller * coef;// Offset=0x184 Size=0x4
    struct D3DX::jpeg_d_post_controller * post;// Offset=0x188 Size=0x4
    struct D3DX::jpeg_input_controller * inputctl;// Offset=0x18c Size=0x4
    struct D3DX::jpeg_marker_reader * marker;// Offset=0x190 Size=0x4
    struct D3DX::jpeg_entropy_decoder * entropy;// Offset=0x194 Size=0x4
    struct D3DX::jpeg_inverse_dct * idct;// Offset=0x198 Size=0x4
    struct D3DX::jpeg_upsampler * upsample;// Offset=0x19c Size=0x4
    struct D3DX::jpeg_color_deconverter * cconvert;// Offset=0x1a0 Size=0x4
    struct D3DX::jpeg_color_quantizer * cquantize;// Offset=0x1a4 Size=0x4
};

struct D3DX::JHUFF_TBL// Size=0x112 (Id=1176)
{
    unsigned char bits[17];// Offset=0x0 Size=0x11
    unsigned char huffval[256];// Offset=0x11 Size=0x100
    unsigned char sent_table;// Offset=0x111 Size=0x1
};

struct D3DX::jpeg_scan_info// Size=0x24 (Id=1177)
{
    int comps_in_scan;// Offset=0x0 Size=0x4
    int component_index[4];// Offset=0x4 Size=0x10
    int Ss;// Offset=0x14 Size=0x4
    int Se;// Offset=0x18 Size=0x4
    int Ah;// Offset=0x1c Size=0x4
    int Al;// Offset=0x20 Size=0x4
};

struct D3DX::jpeg_common_struct// Size=0x14 (Id=1178)
{
    struct D3DX::jpeg_error_mgr * err;// Offset=0x0 Size=0x4
    struct D3DX::jpeg_memory_mgr * mem;// Offset=0x4 Size=0x4
    struct D3DX::jpeg_progress_mgr * progress;// Offset=0x8 Size=0x4
    unsigned char is_decompressor;// Offset=0xc Size=0x1
    unsigned char __align0[3];// Offset=0xd Size=0x3
    int global_state;// Offset=0x10 Size=0x4
};

struct D3DX::jpeg_memory_mgr// Size=0x30 (Id=1179)
{
    void *  ( * alloc_small)(struct D3DX::jpeg_common_struct * ,int ,unsigned int );// Offset=0x0 Size=0x4
    void *  ( * alloc_large)(struct D3DX::jpeg_common_struct * ,int ,unsigned int );// Offset=0x4 Size=0x4
    unsigned char **  ( * alloc_sarray)(struct D3DX::jpeg_common_struct * ,int ,unsigned int ,unsigned int );// Offset=0x8 Size=0x4
    short ** [64] ( * alloc_barray)(struct D3DX::jpeg_common_struct * ,int ,unsigned int ,unsigned int );// Offset=0xc Size=0x4
    struct D3DX::jvirt_sarray_control *  ( * request_virt_sarray)(struct D3DX::jpeg_common_struct * ,int ,unsigned char ,unsigned int ,unsigned int ,unsigned int );// Offset=0x10 Size=0x4
    struct D3DX::jvirt_barray_control *  ( * request_virt_barray)(struct D3DX::jpeg_common_struct * ,int ,unsigned char ,unsigned int ,unsigned int ,unsigned int );// Offset=0x14 Size=0x4
    void  ( * realize_virt_arrays)(struct D3DX::jpeg_common_struct * );// Offset=0x18 Size=0x4
    unsigned char **  ( * access_virt_sarray)(struct D3DX::jpeg_common_struct * ,struct D3DX::jvirt_sarray_control * ,unsigned int ,unsigned int ,unsigned char );// Offset=0x1c Size=0x4
    short ** [64] ( * access_virt_barray)(struct D3DX::jpeg_common_struct * ,struct D3DX::jvirt_barray_control * ,unsigned int ,unsigned int ,unsigned char );// Offset=0x20 Size=0x4
    void  ( * free_pool)(struct D3DX::jpeg_common_struct * ,int );// Offset=0x24 Size=0x4
    void  ( * self_destruct)(struct D3DX::jpeg_common_struct * );// Offset=0x28 Size=0x4
    long max_memory_to_use;// Offset=0x2c Size=0x4
};

struct D3DX::png_color_16_struct// Size=0xa (Id=1180)
{
    unsigned char index;// Offset=0x0 Size=0x1
    unsigned char __align0[1];// Offset=0x1 Size=0x1
    unsigned short red;// Offset=0x2 Size=0x2
    unsigned short green;// Offset=0x4 Size=0x2
    unsigned short blue;// Offset=0x6 Size=0x2
    unsigned short gray;// Offset=0x8 Size=0x2
};

struct D3DX::z_stream_s// Size=0x38 (Id=1181)
{
    unsigned char * next_in;// Offset=0x0 Size=0x4
    unsigned int avail_in;// Offset=0x4 Size=0x4
    unsigned long total_in;// Offset=0x8 Size=0x4
    unsigned char * next_out;// Offset=0xc Size=0x4
    unsigned int avail_out;// Offset=0x10 Size=0x4
    unsigned long total_out;// Offset=0x14 Size=0x4
    char * msg;// Offset=0x18 Size=0x4
    struct D3DX::internal_state * state;// Offset=0x1c Size=0x4
    void *  ( * zalloc)(void * ,unsigned int ,unsigned int );// Offset=0x20 Size=0x4
    void  ( * zfree)(void * ,void * );// Offset=0x24 Size=0x4
    void * opaque;// Offset=0x28 Size=0x4
    int data_type;// Offset=0x2c Size=0x4
    unsigned long adler;// Offset=0x30 Size=0x4
    unsigned long reserved;// Offset=0x34 Size=0x4
};

struct D3DX::png_color_8_struct// Size=0x5 (Id=1182)
{
    unsigned char red;// Offset=0x0 Size=0x1
    unsigned char green;// Offset=0x1 Size=0x1
    unsigned char blue;// Offset=0x2 Size=0x1
    unsigned char gray;// Offset=0x3 Size=0x1
    unsigned char alpha;// Offset=0x4 Size=0x1
};

struct D3DX::png_color_struct// Size=0x3 (Id=1183)
{
    unsigned char red;// Offset=0x0 Size=0x1
    unsigned char green;// Offset=0x1 Size=0x1
    unsigned char blue;// Offset=0x2 Size=0x1
};

struct D3DX::png_time_struct// Size=0x8 (Id=1184)
{
    unsigned short year;// Offset=0x0 Size=0x2
    unsigned char month;// Offset=0x2 Size=0x1
    unsigned char day;// Offset=0x3 Size=0x1
    unsigned char hour;// Offset=0x4 Size=0x1
    unsigned char minute;// Offset=0x5 Size=0x1
    unsigned char second;// Offset=0x6 Size=0x1
};

struct D3DX::JQUANT_TBL// Size=0x82 (Id=1185)
{
    unsigned short quantval[64];// Offset=0x0 Size=0x80
    unsigned char sent_table;// Offset=0x80 Size=0x1
};

struct D3DX::png_row_info_struct// Size=0xc (Id=1186)
{
    unsigned long width;// Offset=0x0 Size=0x4
    unsigned long rowbytes;// Offset=0x4 Size=0x4
    unsigned char color_type;// Offset=0x8 Size=0x1
    unsigned char bit_depth;// Offset=0x9 Size=0x1
    unsigned char channels;// Offset=0xa Size=0x1
    unsigned char pixel_depth;// Offset=0xb Size=0x1
};

struct D3DX::jpeg_scan_info// Size=0x24 (Id=1187)
{
    int comps_in_scan;// Offset=0x0 Size=0x4
    int component_index[4];// Offset=0x4 Size=0x10
    int Ss;// Offset=0x14 Size=0x4
    int Se;// Offset=0x18 Size=0x4
    int Ah;// Offset=0x1c Size=0x4
    int Al;// Offset=0x20 Size=0x4
};

struct D3DX::jpeg_comp_master// Size=0x0 (Id=1188)
{
};

struct D3DX::jpeg_c_main_controller// Size=0x0 (Id=1189)
{
};

struct D3DX::jpeg_c_prep_controller// Size=0x0 (Id=1190)
{
};

struct D3DX::jpeg_c_coef_controller// Size=0x0 (Id=1191)
{
};

struct D3DX::jpeg_marker_writer// Size=0x0 (Id=1192)
{
};

struct D3DX::jpeg_color_converter// Size=0x0 (Id=1193)
{
};

struct D3DX::jpeg_downsampler// Size=0x0 (Id=1194)
{
};

struct D3DX::jpeg_forward_dct// Size=0x0 (Id=1195)
{
};

struct D3DX::jpeg_entropy_encoder// Size=0x0 (Id=1196)
{
};

struct D3DX::jpeg_compress_struct// Size=0x158 (Id=1197)
{
    struct D3DX::jpeg_error_mgr * err;// Offset=0x0 Size=0x4
    struct D3DX::jpeg_memory_mgr * mem;// Offset=0x4 Size=0x4
    struct D3DX::jpeg_progress_mgr * progress;// Offset=0x8 Size=0x4
    unsigned char is_decompressor;// Offset=0xc Size=0x1
    unsigned char __align0[3];// Offset=0xd Size=0x3
    int global_state;// Offset=0x10 Size=0x4
    struct D3DX::jpeg_destination_mgr * dest;// Offset=0x14 Size=0x4
    unsigned int image_width;// Offset=0x18 Size=0x4
    unsigned int image_height;// Offset=0x1c Size=0x4
    int input_components;// Offset=0x20 Size=0x4
    enum D3DX::J_COLOR_SPACE in_color_space;// Offset=0x24 Size=0x4
    float input_gamma;// Offset=0x28 Size=0x8
    int data_precision;// Offset=0x30 Size=0x4
    int num_components;// Offset=0x34 Size=0x4
    enum D3DX::J_COLOR_SPACE jpeg_color_space;// Offset=0x38 Size=0x4
    struct D3DX::jpeg_component_info * comp_info;// Offset=0x3c Size=0x4
    struct D3DX::JQUANT_TBL * quant_tbl_ptrs[4];// Offset=0x40 Size=0x10
    struct D3DX::JHUFF_TBL * dc_huff_tbl_ptrs[4];// Offset=0x50 Size=0x10
    struct D3DX::JHUFF_TBL * ac_huff_tbl_ptrs[4];// Offset=0x60 Size=0x10
    unsigned char arith_dc_L[16];// Offset=0x70 Size=0x10
    unsigned char arith_dc_U[16];// Offset=0x80 Size=0x10
    unsigned char arith_ac_K[16];// Offset=0x90 Size=0x10
    int num_scans;// Offset=0xa0 Size=0x4
    struct D3DX::jpeg_scan_info * scan_info;// Offset=0xa4 Size=0x4
    unsigned char raw_data_in;// Offset=0xa8 Size=0x1
    unsigned char arith_code;// Offset=0xa9 Size=0x1
    unsigned char optimize_coding;// Offset=0xaa Size=0x1
    unsigned char CCIR601_sampling;// Offset=0xab Size=0x1
    int smoothing_factor;// Offset=0xac Size=0x4
    enum D3DX::J_DCT_METHOD dct_method;// Offset=0xb0 Size=0x4
    unsigned int restart_interval;// Offset=0xb4 Size=0x4
    int restart_in_rows;// Offset=0xb8 Size=0x4
    unsigned char write_JFIF_header;// Offset=0xbc Size=0x1
    unsigned char density_unit;// Offset=0xbd Size=0x1
    unsigned short X_density;// Offset=0xbe Size=0x2
    unsigned short Y_density;// Offset=0xc0 Size=0x2
    unsigned char write_Adobe_marker;// Offset=0xc2 Size=0x1
    unsigned char __align1[1];// Offset=0xc3 Size=0x1
    unsigned int next_scanline;// Offset=0xc4 Size=0x4
    unsigned char progressive_mode;// Offset=0xc8 Size=0x1
    unsigned char __align2[3];// Offset=0xc9 Size=0x3
    int max_h_samp_factor;// Offset=0xcc Size=0x4
    int max_v_samp_factor;// Offset=0xd0 Size=0x4
    unsigned int total_iMCU_rows;// Offset=0xd4 Size=0x4
    int comps_in_scan;// Offset=0xd8 Size=0x4
    struct D3DX::jpeg_component_info * cur_comp_info[4];// Offset=0xdc Size=0x10
    unsigned int MCUs_per_row;// Offset=0xec Size=0x4
    unsigned int MCU_rows_in_scan;// Offset=0xf0 Size=0x4
    int blocks_in_MCU;// Offset=0xf4 Size=0x4
    int MCU_membership[10];// Offset=0xf8 Size=0x28
    int Ss;// Offset=0x120 Size=0x4
    int Se;// Offset=0x124 Size=0x4
    int Ah;// Offset=0x128 Size=0x4
    int Al;// Offset=0x12c Size=0x4
    struct D3DX::jpeg_comp_master * master;// Offset=0x130 Size=0x4
    struct D3DX::jpeg_c_main_controller * main;// Offset=0x134 Size=0x4
    struct D3DX::jpeg_c_prep_controller * prep;// Offset=0x138 Size=0x4
    struct D3DX::jpeg_c_coef_controller * coef;// Offset=0x13c Size=0x4
    struct D3DX::jpeg_marker_writer * marker;// Offset=0x140 Size=0x4
    struct D3DX::jpeg_color_converter * cconvert;// Offset=0x144 Size=0x4
    struct D3DX::jpeg_downsampler * downsample;// Offset=0x148 Size=0x4
    struct D3DX::jpeg_forward_dct * fdct;// Offset=0x14c Size=0x4
    struct D3DX::jpeg_entropy_encoder * entropy;// Offset=0x150 Size=0x4
};

struct D3DX::png_info_struct// Size=0x40 (Id=1198)
{
    unsigned long width;// Offset=0x0 Size=0x4
    unsigned long height;// Offset=0x4 Size=0x4
    unsigned long valid;// Offset=0x8 Size=0x4
    unsigned long rowbytes;// Offset=0xc Size=0x4
    struct D3DX::png_color_struct * palette;// Offset=0x10 Size=0x4
    unsigned short num_palette;// Offset=0x14 Size=0x2
    unsigned short num_trans;// Offset=0x16 Size=0x2
    unsigned char bit_depth;// Offset=0x18 Size=0x1
    unsigned char color_type;// Offset=0x19 Size=0x1
    unsigned char compression_type;// Offset=0x1a Size=0x1
    unsigned char filter_type;// Offset=0x1b Size=0x1
    unsigned char interlace_type;// Offset=0x1c Size=0x1
    unsigned char channels;// Offset=0x1d Size=0x1
    unsigned char pixel_depth;// Offset=0x1e Size=0x1
    unsigned char spare_byte;// Offset=0x1f Size=0x1
    unsigned char signature[8];// Offset=0x20 Size=0x8
    float gamma;// Offset=0x28 Size=0x4
    unsigned char srgb_intent;// Offset=0x2c Size=0x1
    unsigned char __align0[3];// Offset=0x2d Size=0x3
    unsigned char * trans;// Offset=0x30 Size=0x4
    struct D3DX::png_color_16_struct trans_values;// Offset=0x34 Size=0xa
};

struct D3DX::png_text_struct// Size=0x10 (Id=1199)
{
    int compression;// Offset=0x0 Size=0x4
    char * key;// Offset=0x4 Size=0x4
    char * text;// Offset=0x8 Size=0x4
    unsigned int text_length;// Offset=0xc Size=0x4
};

struct D3DX::jpeg_destination_mgr// Size=0x14 (Id=1200)
{
    unsigned char * next_output_byte;// Offset=0x0 Size=0x4
    unsigned int free_in_buffer;// Offset=0x4 Size=0x4
    void  ( * init_destination)(struct D3DX::jpeg_compress_struct * );// Offset=0x8 Size=0x4
    unsigned char  ( * empty_output_buffer)(struct D3DX::jpeg_compress_struct * );// Offset=0xc Size=0x4
    void  ( * term_destination)(struct D3DX::jpeg_compress_struct * );// Offset=0x10 Size=0x4
};

struct D3DX::png_struct_def// Size=0x19c (Id=1201)
{
    int jmpbuf[16];// Offset=0x0 Size=0x40
    void  ( * error_fn)(struct D3DX::png_struct_def * ,char * );// Offset=0x40 Size=0x4
    void  ( * warning_fn)(struct D3DX::png_struct_def * ,char * );// Offset=0x44 Size=0x4
    void * error_ptr;// Offset=0x48 Size=0x4
    void  ( * write_data_fn)(struct D3DX::png_struct_def * ,unsigned char * ,unsigned int );// Offset=0x4c Size=0x4
    void  ( * read_data_fn)(struct D3DX::png_struct_def * ,unsigned char * ,unsigned int );// Offset=0x50 Size=0x4
    void * io_ptr;// Offset=0x54 Size=0x4
    unsigned long mode;// Offset=0x58 Size=0x4
    unsigned long flags;// Offset=0x5c Size=0x4
    unsigned long transformations;// Offset=0x60 Size=0x4
    struct D3DX::z_stream_s zstream;// Offset=0x64 Size=0x38
    unsigned char * zbuf;// Offset=0x9c Size=0x4
    unsigned int zbuf_size;// Offset=0xa0 Size=0x4
    int zlib_level;// Offset=0xa4 Size=0x4
    int zlib_method;// Offset=0xa8 Size=0x4
    int zlib_window_bits;// Offset=0xac Size=0x4
    int zlib_mem_level;// Offset=0xb0 Size=0x4
    int zlib_strategy;// Offset=0xb4 Size=0x4
    unsigned long width;// Offset=0xb8 Size=0x4
    unsigned long height;// Offset=0xbc Size=0x4
    unsigned long num_rows;// Offset=0xc0 Size=0x4
    unsigned long usr_width;// Offset=0xc4 Size=0x4
    unsigned long rowbytes;// Offset=0xc8 Size=0x4
    unsigned long irowbytes;// Offset=0xcc Size=0x4
    unsigned long iwidth;// Offset=0xd0 Size=0x4
    unsigned long row_number;// Offset=0xd4 Size=0x4
    unsigned char * prev_row;// Offset=0xd8 Size=0x4
    unsigned char * row_buf;// Offset=0xdc Size=0x4
    unsigned char * sub_row;// Offset=0xe0 Size=0x4
    unsigned char * up_row;// Offset=0xe4 Size=0x4
    unsigned char * avg_row;// Offset=0xe8 Size=0x4
    unsigned char * paeth_row;// Offset=0xec Size=0x4
    struct D3DX::png_row_info_struct row_info;// Offset=0xf0 Size=0xc
    unsigned long idat_size;// Offset=0xfc Size=0x4
    unsigned long crc;// Offset=0x100 Size=0x4
    struct D3DX::png_color_struct * palette;// Offset=0x104 Size=0x4
    unsigned short num_palette;// Offset=0x108 Size=0x2
    unsigned short num_trans;// Offset=0x10a Size=0x2
    unsigned char chunk_name[5];// Offset=0x10c Size=0x5
    unsigned char compression;// Offset=0x111 Size=0x1
    unsigned char filter;// Offset=0x112 Size=0x1
    unsigned char interlaced;// Offset=0x113 Size=0x1
    unsigned char pass;// Offset=0x114 Size=0x1
    unsigned char do_filter;// Offset=0x115 Size=0x1
    unsigned char color_type;// Offset=0x116 Size=0x1
    unsigned char bit_depth;// Offset=0x117 Size=0x1
    unsigned char usr_bit_depth;// Offset=0x118 Size=0x1
    unsigned char pixel_depth;// Offset=0x119 Size=0x1
    unsigned char channels;// Offset=0x11a Size=0x1
    unsigned char usr_channels;// Offset=0x11b Size=0x1
    unsigned char sig_bytes;// Offset=0x11c Size=0x1
    unsigned char __align0[1];// Offset=0x11d Size=0x1
    unsigned short filler;// Offset=0x11e Size=0x2
    void  ( * output_flush_fn)(struct D3DX::png_struct_def * );// Offset=0x120 Size=0x4
    unsigned long flush_dist;// Offset=0x124 Size=0x4
    unsigned long flush_rows;// Offset=0x128 Size=0x4
    int gamma_shift;// Offset=0x12c Size=0x4
    float gamma;// Offset=0x130 Size=0x4
    float screen_gamma;// Offset=0x134 Size=0x4
    unsigned char * gamma_table;// Offset=0x138 Size=0x4
    unsigned char * gamma_from_1;// Offset=0x13c Size=0x4
    unsigned char * gamma_to_1;// Offset=0x140 Size=0x4
    unsigned short ** gamma_16_table;// Offset=0x144 Size=0x4
    unsigned short ** gamma_16_from_1;// Offset=0x148 Size=0x4
    unsigned short ** gamma_16_to_1;// Offset=0x14c Size=0x4
    struct D3DX::png_color_8_struct sig_bit;// Offset=0x150 Size=0x5
    struct D3DX::png_color_8_struct shift;// Offset=0x155 Size=0x5
    unsigned char __align1[2];// Offset=0x15a Size=0x2
    unsigned char * trans;// Offset=0x15c Size=0x4
    struct D3DX::png_color_16_struct trans_values;// Offset=0x160 Size=0xa
    unsigned char __align2[2];// Offset=0x16a Size=0x2
    void  ( * read_row_fn)(struct D3DX::png_struct_def * ,unsigned long ,int );// Offset=0x16c Size=0x4
    void  ( * write_row_fn)(struct D3DX::png_struct_def * ,unsigned long ,int );// Offset=0x170 Size=0x4
    unsigned char * palette_lookup;// Offset=0x174 Size=0x4
    unsigned char * dither_index;// Offset=0x178 Size=0x4
    unsigned short * hist;// Offset=0x17c Size=0x4
    unsigned char heuristic_method;// Offset=0x180 Size=0x1
    unsigned char num_prev_filters;// Offset=0x181 Size=0x1
    unsigned char __align3[2];// Offset=0x182 Size=0x2
    unsigned char * prev_filters;// Offset=0x184 Size=0x4
    unsigned short * filter_weights;// Offset=0x188 Size=0x4
    unsigned short * inv_filter_weights;// Offset=0x18c Size=0x4
    unsigned short * filter_costs;// Offset=0x190 Size=0x4
    unsigned short * inv_filter_costs;// Offset=0x194 Size=0x4
    unsigned char empty_plte_permitted;// Offset=0x198 Size=0x1
};

struct D3DX::jpeg_source_mgr// Size=0x1c (Id=1202)
{
    unsigned char * next_input_byte;// Offset=0x0 Size=0x4
    unsigned int bytes_in_buffer;// Offset=0x4 Size=0x4
    void  ( * init_source)(struct D3DX::jpeg_decompress_struct * );// Offset=0x8 Size=0x4
    unsigned char  ( * fill_input_buffer)(struct D3DX::jpeg_decompress_struct * );// Offset=0xc Size=0x4
    void  ( * skip_input_data)(struct D3DX::jpeg_decompress_struct * ,long );// Offset=0x10 Size=0x4
    unsigned char  ( * resync_to_restart)(struct D3DX::jpeg_decompress_struct * ,int );// Offset=0x14 Size=0x4
    void  ( * term_source)(struct D3DX::jpeg_decompress_struct * );// Offset=0x18 Size=0x4
};

union D3DX::jpeg_error_mgr::__unnamed// Size=0x50 (Id=1203)
{
    int i[8];// Offset=0x0 Size=0x20
    char s[80];// Offset=0x0 Size=0x50
};

struct D3DX::jpeg_error_mgr// Size=0x84 (Id=1204)
{
    void  ( * error_exit)(struct D3DX::jpeg_common_struct * );// Offset=0x0 Size=0x4
    void  ( * emit_message)(struct D3DX::jpeg_common_struct * ,int );// Offset=0x4 Size=0x4
    void  ( * output_message)(struct D3DX::jpeg_common_struct * );// Offset=0x8 Size=0x4
    void  ( * format_message)(struct D3DX::jpeg_common_struct * ,char * );// Offset=0xc Size=0x4
    void  ( * reset_error_mgr)(struct D3DX::jpeg_common_struct * );// Offset=0x10 Size=0x4
    int msg_code;// Offset=0x14 Size=0x4
    union D3DX::jpeg_error_mgr::__unnamed msg_parm;// Offset=0x18 Size=0x50
    int trace_level;// Offset=0x68 Size=0x4
    long num_warnings;// Offset=0x6c Size=0x4
    char ** jpeg_message_table;// Offset=0x70 Size=0x4
    int last_jpeg_message;// Offset=0x74 Size=0x4
    char ** addon_message_table;// Offset=0x78 Size=0x4
    int first_addon_message;// Offset=0x7c Size=0x4
    int last_addon_message;// Offset=0x80 Size=0x4
};

struct D3DX::jpeg_progress_mgr// Size=0x14 (Id=1205)
{
    void  ( * progress_monitor)(struct D3DX::jpeg_common_struct * );// Offset=0x0 Size=0x4
    long pass_counter;// Offset=0x4 Size=0x4
    long pass_limit;// Offset=0x8 Size=0x4
    int completed_passes;// Offset=0xc Size=0x4
    int total_passes;// Offset=0x10 Size=0x4
};

struct D3DX::jpeg_component_info// Size=0x54 (Id=1206)
{
    int component_id;// Offset=0x0 Size=0x4
    int component_index;// Offset=0x4 Size=0x4
    int h_samp_factor;// Offset=0x8 Size=0x4
    int v_samp_factor;// Offset=0xc Size=0x4
    int quant_tbl_no;// Offset=0x10 Size=0x4
    int dc_tbl_no;// Offset=0x14 Size=0x4
    int ac_tbl_no;// Offset=0x18 Size=0x4
    unsigned int width_in_blocks;// Offset=0x1c Size=0x4
    unsigned int height_in_blocks;// Offset=0x20 Size=0x4
    int DCT_scaled_size;// Offset=0x24 Size=0x4
    unsigned int downsampled_width;// Offset=0x28 Size=0x4
    unsigned int downsampled_height;// Offset=0x2c Size=0x4
    unsigned char component_needed;// Offset=0x30 Size=0x1
    unsigned char __align0[3];// Offset=0x31 Size=0x3
    int MCU_width;// Offset=0x34 Size=0x4
    int MCU_height;// Offset=0x38 Size=0x4
    int MCU_blocks;// Offset=0x3c Size=0x4
    int MCU_sample_width;// Offset=0x40 Size=0x4
    int last_col_width;// Offset=0x44 Size=0x4
    int last_row_height;// Offset=0x48 Size=0x4
    struct D3DX::JQUANT_TBL * quant_table;// Offset=0x4c Size=0x4
    void * dct_table;// Offset=0x50 Size=0x4
};

struct D3DX::internal_state// Size=0x4 (Id=1207)
{
    int dummy;// Offset=0x0 Size=0x4
};

struct ID3DXEffect// Size=0x4 (Id=1218)
{
    struct ID3DXEffectVtbl * lpVtbl;// Offset=0x0 Size=0x4
};

struct ID3DXEffect : public IUnknown// Size=0x4 (Id=1219)
{
    long QueryInterface(struct _GUID & ,void ** );
    unsigned long AddRef();
    unsigned long Release();
    long GetDevice(struct D3DDevice ** );
    long GetDesc(struct _D3DXEFFECT_DESC * );
    long GetParameterDesc(unsigned int ,struct _D3DXPARAMETER_DESC * );
    long GetTechniqueDesc(unsigned int ,struct _D3DXTECHNIQUE_DESC * );
    long SetDword(unsigned long ,unsigned long );
    long GetDword(unsigned long ,unsigned long * );
    long SetFloat(unsigned long ,float );
    long GetFloat(unsigned long ,float * );
    long SetVector(unsigned long ,struct D3DXVECTOR4 * );
    long GetVector(unsigned long ,struct D3DXVECTOR4 * );
    long SetMatrix(unsigned long ,struct D3DXMATRIX * );
    long GetMatrix(unsigned long ,struct D3DXMATRIX * );
    long SetTexture(unsigned long ,struct D3DBaseTexture * );
    long GetTexture(unsigned long ,struct D3DBaseTexture ** );
    long SetVertexShader(unsigned long ,unsigned long );
    long GetVertexShader(unsigned long ,unsigned long * );
    long SetPixelShader(unsigned long ,unsigned long );
    long GetPixelShader(unsigned long ,unsigned long * );
    long GetTechnique(unsigned int ,struct ID3DXTechnique ** );
    long CloneEffect(struct D3DDevice * ,unsigned long ,struct ID3DXEffect ** );
    void ID3DXEffect(struct ID3DXEffect & );
    void ID3DXEffect();
};

struct D3DResource// Size=0xc (Id=1226)
{
    union // Size=0x5 (Id=0)
    {
        unsigned long AddRef();// Offset=0x0 Size=0x5
        unsigned long Release();// Offset=0x0 Size=0x5
        long GetDevice(struct D3DDevice ** );
        enum _D3DRESOURCETYPE GetType();// Offset=0x0 Size=0x5
        long SetPrivateData(struct _GUID & ,void * ,unsigned long ,unsigned long );
        long GetPrivateData(struct _GUID & ,void * ,unsigned long * );
        long FreePrivateData(struct _GUID & );
        int IsBusy();
        void BlockUntilNotBusy();
        void MoveResourceMemory(enum _D3DMEMORY );
        void Register(void * );
        unsigned long Common;// Offset=0x0 Size=0x4
        unsigned long Data;// Offset=0x4 Size=0x4
    };
    unsigned long Lock;// Offset=0x8 Size=0x4
};

struct D3DXQUATERNION// Size=0x10 (Id=1232)
{
    float x;// Offset=0x0 Size=0x4
    float y;// Offset=0x4 Size=0x4
    float z;// Offset=0x8 Size=0x4
    float w;// Offset=0xc Size=0x4
};

struct D3DXQUATERNION// Size=0x10 (Id=1233)
{
    void D3DXQUATERNION(float ,float ,float ,float );
    void D3DXQUATERNION(float * );
    void D3DXQUATERNION();
    float * operator float *();
    float * operator const float *();
    struct D3DXQUATERNION & operator+=(struct D3DXQUATERNION & );
    struct D3DXQUATERNION & operator-=(struct D3DXQUATERNION & );
    struct D3DXQUATERNION & operator*=(float );
    struct D3DXQUATERNION & operator*=(struct D3DXQUATERNION & );
    struct D3DXQUATERNION & operator/=(float );
    struct D3DXQUATERNION operator+(struct D3DXQUATERNION & );
    struct D3DXQUATERNION operator+();
    struct D3DXQUATERNION operator-(struct D3DXQUATERNION & );
    struct D3DXQUATERNION operator-();
    struct D3DXQUATERNION operator*(float );
    struct D3DXQUATERNION operator*(struct D3DXQUATERNION & );
    struct D3DXQUATERNION operator/(float );
    int operator==(struct D3DXQUATERNION & );
    int operator!=(struct D3DXQUATERNION & );
    float x;// Offset=0x0 Size=0x4
    float y;// Offset=0x4 Size=0x4
    float z;// Offset=0x8 Size=0x4
    float w;// Offset=0xc Size=0x4
};

struct ID3DXSPMesh// Size=0x4 (Id=1234)
{
    struct ID3DXSPMeshVtbl * lpVtbl;// Offset=0x0 Size=0x4
};

struct ID3DXSPMesh : public IUnknown// Size=0x4 (Id=1235)
{
    long QueryInterface(struct _GUID & ,void ** );
    unsigned long AddRef();
    unsigned long Release();
    unsigned long GetNumFaces();
    unsigned long GetNumVertices();
    unsigned long GetFVF();
    long GetDeclaration(unsigned long * );
    unsigned long GetOptions();
    long GetDevice(struct D3DDevice ** );
    long CloneMeshFVF(unsigned long ,unsigned long ,struct D3DDevice * ,unsigned long * ,unsigned long * ,struct ID3DXMesh ** );
    long CloneMesh(unsigned long ,unsigned long * ,struct D3DDevice * ,unsigned long * ,unsigned long * ,struct ID3DXMesh ** );
    long ClonePMeshFVF(unsigned long ,unsigned long ,struct D3DDevice * ,unsigned long * ,struct ID3DXPMesh ** );
    long ClonePMesh(unsigned long ,unsigned long * ,struct D3DDevice * ,unsigned long * ,struct ID3DXPMesh ** );
    long ReduceFaces(unsigned long );
    long ReduceVertices(unsigned long );
    unsigned long GetMaxFaces();
    unsigned long GetMaxVertices();
    void ID3DXSPMesh(struct ID3DXSPMesh & );
    void ID3DXSPMesh();
};

struct D3DSurface : public D3DPixelContainer// Size=0x18 (Id=1240)
{
    long GetContainer(struct D3DBaseTexture ** );
    union // Size=0x1a (Id=0)
    {
        long GetDesc(struct _D3DSURFACE_DESC * );// Offset=0x0 Size=0x12
        long LockRect(struct _D3DLOCKED_RECT * ,struct tagRECT * ,unsigned long );// Offset=0x0 Size=0x1a
        long UnlockRect();// Offset=0x0 Size=0x5
        unsigned char __align0[15];// Offset=0x5 Size=0xf
        struct D3DBaseTexture * Parent;// Offset=0x14 Size=0x4
    };
};

struct ID3DXRenderToEnvMap// Size=0x4 (Id=1246)
{
    struct ID3DXRenderToEnvMapVtbl * lpVtbl;// Offset=0x0 Size=0x4
};

struct ID3DXRenderToEnvMap : public IUnknown// Size=0x4 (Id=1247)
{
    long QueryInterface(struct _GUID & ,void ** );
    unsigned long AddRef();
    unsigned long Release();
    long GetDevice(struct D3DDevice ** );
    long GetDesc(struct _D3DXRTE_DESC * );
    long BeginCube(struct D3DCubeTexture * );
    long BeginSphere(struct D3DTexture * );
    long BeginHemisphere(struct D3DTexture * ,struct D3DTexture * );
    long BeginParabolic(struct D3DTexture * ,struct D3DTexture * );
    long Face(enum _D3DCUBEMAP_FACES );
    long End();
    void ID3DXRenderToEnvMap(struct ID3DXRenderToEnvMap & );
    void ID3DXRenderToEnvMap();
};

struct D3DXMATRIX : public _D3DMATRIX// Size=0x40 (Id=1255)
{
    void D3DXMATRIX(float ,float ,float ,float ,float ,float ,float ,float ,float ,float ,float ,float ,float ,float ,float ,float );
    void D3DXMATRIX(struct _D3DMATRIX & );
    void D3DXMATRIX(float * );
    void D3DXMATRIX();
    float operator()(unsigned int ,unsigned int );
    float & operator()(unsigned int ,unsigned int );
    float * operator float *();
    float * operator const float *();
    struct D3DXMATRIX & operator*=(float );
    struct D3DXMATRIX & operator*=(struct D3DXMATRIX & );
    struct D3DXMATRIX & operator+=(struct D3DXMATRIX & );
    struct D3DXMATRIX & operator-=(struct D3DXMATRIX & );
    struct D3DXMATRIX & operator/=(float );
    struct D3DXMATRIX operator+(struct D3DXMATRIX & );
    struct D3DXMATRIX operator+();
    struct D3DXMATRIX operator-(struct D3DXMATRIX & );
    struct D3DXMATRIX operator-();
    struct D3DXMATRIX operator*(float );
    struct D3DXMATRIX operator*(struct D3DXMATRIX & );
    struct D3DXMATRIX operator/(float );
    int operator==(struct D3DXMATRIX & );
    int operator!=(struct D3DXMATRIX & );
};

struct ID3DXMatrixStack// Size=0x4 (Id=1256)
{
    struct ID3DXMatrixStackVtbl * lpVtbl;// Offset=0x0 Size=0x4
};

struct ID3DXMatrixStack : public IUnknown// Size=0x4 (Id=1257)
{
    long QueryInterface(struct _GUID & ,void ** );
    unsigned long AddRef();
    unsigned long Release();
    long Pop();
    long Push();
    long LoadIdentity();
    long LoadMatrix(struct D3DXMATRIX * );
    long MultMatrix(struct D3DXMATRIX * );
    long MultMatrixLocal(struct D3DXMATRIX * );
    long RotateAxis(struct D3DXVECTOR3 * ,float );
    long RotateAxisLocal(struct D3DXVECTOR3 * ,float );
    long RotateYawPitchRoll(float ,float ,float );
    long RotateYawPitchRollLocal(float ,float ,float );
    long Scale(float ,float ,float );
    long ScaleLocal(float ,float ,float );
    long Translate(float ,float ,float );
    long TranslateLocal(float ,float ,float );
    struct D3DXMATRIX * GetTop();
    void ID3DXMatrixStack(struct ID3DXMatrixStack & );
    void ID3DXMatrixStack();
};

struct ID3DXBuffer// Size=0x4 (Id=1262)
{
    struct ID3DXBufferVtbl * lpVtbl;// Offset=0x0 Size=0x4
};

struct ID3DXBuffer : public IUnknown// Size=0x4 (Id=1263)
{
    long QueryInterface(struct _GUID & ,void ** );
    unsigned long AddRef();
    unsigned long Release();
    void * GetBufferPointer();
    unsigned long GetBufferSize();
    void ID3DXBuffer(struct ID3DXBuffer & );
    void ID3DXBuffer();
};

struct ID3DXRenderToSurface// Size=0x4 (Id=1264)
{
    struct ID3DXRenderToSurfaceVtbl * lpVtbl;// Offset=0x0 Size=0x4
};

struct ID3DXRenderToSurface : public IUnknown// Size=0x4 (Id=1265)
{
    long QueryInterface(struct _GUID & ,void ** );
    unsigned long AddRef();
    unsigned long Release();
    long GetDevice(struct D3DDevice ** );
    long GetDesc(struct _D3DXRTS_DESC * );
    long BeginScene(struct D3DSurface * ,struct _D3DVIEWPORT8 * );
    long EndScene();
    void ID3DXRenderToSurface(struct ID3DXRenderToSurface & );
    void ID3DXRenderToSurface();
};

struct D3DPalette : public D3DResource// Size=0xc (Id=1273)
{
    union // Size=0x19 (Id=0)
    {
        long Lock(unsigned long ** ,unsigned long );// Offset=0x0 Size=0x19
        long Unlock();// Offset=0x0 Size=0x5
        enum _D3DPALETTESIZE GetSize();
    };
};

struct D3DVolumeTexture : public D3DBaseTexture// Size=0x14 (Id=1282)
{
    union // Size=0x16 (Id=0)
    {
        long GetLevelDesc(unsigned int ,struct _D3DVOLUME_DESC * );// Offset=0x0 Size=0x16
        long GetVolumeLevel(unsigned int ,struct D3DVolume ** );// Offset=0x0 Size=0x5
        long LockBox(unsigned int ,struct _D3DLOCKED_BOX * ,struct _D3DBOX * ,unsigned long );
        long UnlockBox(unsigned int );
    };
};

struct D3DCubeTexture : public D3DBaseTexture// Size=0x14 (Id=1286)
{
    union // Size=0x16 (Id=0)
    {
        long GetLevelDesc(unsigned int ,struct _D3DSURFACE_DESC * );// Offset=0x0 Size=0x16
        long GetCubeMapSurface(enum _D3DCUBEMAP_FACES ,unsigned int ,struct D3DSurface ** );// Offset=0x0 Size=0x5
        long LockRect(enum _D3DCUBEMAP_FACES ,unsigned int ,struct _D3DLOCKED_RECT * ,struct tagRECT * ,unsigned long );
        long UnlockRect(enum _D3DCUBEMAP_FACES ,unsigned int );
    };
};

struct ID3DXPMesh// Size=0x4 (Id=1300)
{
    struct ID3DXPMeshVtbl * lpVtbl;// Offset=0x0 Size=0x4
};

struct ID3DXPMesh : public ID3DXBaseMesh// Size=0x4 (Id=1301)
{
    long QueryInterface(struct _GUID & ,void ** );
    unsigned long AddRef();
    unsigned long Release();
    long DrawSubset(unsigned long );
    unsigned long GetNumFaces();
    unsigned long GetNumVertices();
    unsigned long GetFVF();
    long GetDeclaration(unsigned long * );
    unsigned long GetOptions();
    long GetDevice(struct D3DDevice ** );
    long CloneMeshFVF(unsigned long ,unsigned long ,struct D3DDevice * ,struct ID3DXMesh ** );
    long CloneMesh(unsigned long ,unsigned long * ,struct D3DDevice * ,struct ID3DXMesh ** );
    long GetVertexBuffer(struct D3DVertexBuffer ** );
    long GetIndexBuffer(struct D3DIndexBuffer ** );
    long LockVertexBuffer(unsigned long ,unsigned char ** );
    long UnlockVertexBuffer();
    long LockIndexBuffer(unsigned long ,unsigned char ** );
    long UnlockIndexBuffer();
    long GetAttributeTable(struct _D3DXATTRIBUTERANGE * ,unsigned long * );
    long ClonePMeshFVF(unsigned long ,unsigned long ,struct D3DDevice * ,struct ID3DXPMesh ** );
    long ClonePMesh(unsigned long ,unsigned long * ,struct D3DDevice * ,struct ID3DXPMesh ** );
    long SetNumFaces(unsigned long );
    long SetNumVertices(unsigned long );
    unsigned long GetMaxFaces();
    unsigned long GetMinFaces();
    unsigned long GetMaxVertices();
    unsigned long GetMinVertices();
    long Save(struct IStream * ,struct D3DXMATERIAL * ,unsigned long );
    long Optimize(unsigned long ,unsigned long * ,unsigned long * ,struct ID3DXBuffer ** ,struct ID3DXMesh ** );
    long GetAdjacency(unsigned long * );
    void ID3DXPMesh(struct ID3DXPMesh & );
    void ID3DXPMesh();
};

struct D3DXVECTOR4// Size=0x10 (Id=1310)
{
    float x;// Offset=0x0 Size=0x4
    float y;// Offset=0x4 Size=0x4
    float z;// Offset=0x8 Size=0x4
    float w;// Offset=0xc Size=0x4
};

struct D3DXVECTOR4// Size=0x10 (Id=1311)
{
    void D3DXVECTOR4(float ,float ,float ,float );
    void D3DXVECTOR4(float * );
    void D3DXVECTOR4();
    float * operator float *();
    float * operator const float *();
    struct D3DXVECTOR4 & operator+=(struct D3DXVECTOR4 & );
    struct D3DXVECTOR4 & operator-=(struct D3DXVECTOR4 & );
    struct D3DXVECTOR4 & operator*=(float );
    struct D3DXVECTOR4 & operator/=(float );
    struct D3DXVECTOR4 operator+(struct D3DXVECTOR4 & );
    struct D3DXVECTOR4 operator+();
    struct D3DXVECTOR4 operator-(struct D3DXVECTOR4 & );
    struct D3DXVECTOR4 operator-();
    struct D3DXVECTOR4 operator*(float );
    struct D3DXVECTOR4 operator/(float );
    int operator==(struct D3DXVECTOR4 & );
    int operator!=(struct D3DXVECTOR4 & );
    float x;// Offset=0x0 Size=0x4
    float y;// Offset=0x4 Size=0x4
    float z;// Offset=0x8 Size=0x4
    float w;// Offset=0xc Size=0x4
};

struct _D3DGAMMARAMP// Size=0x300 (Id=1320)
{
    unsigned char red[256];// Offset=0x0 Size=0x100
    unsigned char green[256];// Offset=0x100 Size=0x100
    unsigned char blue[256];// Offset=0x200 Size=0x100
};

struct _D3DRECT// Size=0x10 (Id=1322)
{
    long x1;// Offset=0x0 Size=0x4
    long y1;// Offset=0x4 Size=0x4
    long x2;// Offset=0x8 Size=0x4
    long y2;// Offset=0xc Size=0x4
};

struct _D3DMATERIAL8// Size=0x44 (Id=1323)
{
    struct _D3DCOLORVALUE Diffuse;// Offset=0x0 Size=0x10
    struct _D3DCOLORVALUE Ambient;// Offset=0x10 Size=0x10
    struct _D3DCOLORVALUE Specular;// Offset=0x20 Size=0x10
    struct _D3DCOLORVALUE Emissive;// Offset=0x30 Size=0x10
    float Power;// Offset=0x40 Size=0x4
};

struct _D3DLIGHT8// Size=0x68 (Id=1324)
{
    enum _D3DLIGHTTYPE Type;// Offset=0x0 Size=0x4
    struct _D3DCOLORVALUE Diffuse;// Offset=0x4 Size=0x10
    struct _D3DCOLORVALUE Specular;// Offset=0x14 Size=0x10
    struct _D3DCOLORVALUE Ambient;// Offset=0x24 Size=0x10
    struct _D3DVECTOR Position;// Offset=0x34 Size=0xc
    struct _D3DVECTOR Direction;// Offset=0x40 Size=0xc
    float Range;// Offset=0x4c Size=0x4
    float Falloff;// Offset=0x50 Size=0x4
    float Attenuation0;// Offset=0x54 Size=0x4
    float Attenuation1;// Offset=0x58 Size=0x4
    float Attenuation2;// Offset=0x5c Size=0x4
    float Theta;// Offset=0x60 Size=0x4
    float Phi;// Offset=0x64 Size=0x4
};

struct _D3DPixelShaderDef// Size=0xf0 (Id=1325)
{
    unsigned long PSAlphaInputs[8];// Offset=0x0 Size=0x20
    unsigned long PSFinalCombinerInputsABCD;// Offset=0x20 Size=0x4
    unsigned long PSFinalCombinerInputsEFG;// Offset=0x24 Size=0x4
    unsigned long PSConstant0[8];// Offset=0x28 Size=0x20
    unsigned long PSConstant1[8];// Offset=0x48 Size=0x20
    unsigned long PSAlphaOutputs[8];// Offset=0x68 Size=0x20
    unsigned long PSRGBInputs[8];// Offset=0x88 Size=0x20
    unsigned long PSCompareMode;// Offset=0xa8 Size=0x4
    unsigned long PSFinalCombinerConstant0;// Offset=0xac Size=0x4
    unsigned long PSFinalCombinerConstant1;// Offset=0xb0 Size=0x4
    unsigned long PSRGBOutputs[8];// Offset=0xb4 Size=0x20
    unsigned long PSCombinerCount;// Offset=0xd4 Size=0x4
    unsigned long PSTextureModes;// Offset=0xd8 Size=0x4
    unsigned long PSDotMapping;// Offset=0xdc Size=0x4
    unsigned long PSInputTexture;// Offset=0xe0 Size=0x4
    unsigned long PSC0Mapping;// Offset=0xe4 Size=0x4
    unsigned long PSC1Mapping;// Offset=0xe8 Size=0x4
    unsigned long PSFinalCombinerConstants;// Offset=0xec Size=0x4
};

struct _D3DRECTPATCH_INFO// Size=0x1c (Id=1326)
{
    unsigned int StartVertexOffsetWidth;// Offset=0x0 Size=0x4
    unsigned int StartVertexOffsetHeight;// Offset=0x4 Size=0x4
    unsigned int Width;// Offset=0x8 Size=0x4
    unsigned int Height;// Offset=0xc Size=0x4
    unsigned int Stride;// Offset=0x10 Size=0x4
    enum _D3DBASISTYPE Basis;// Offset=0x14 Size=0x4
    enum _D3DORDERTYPE Order;// Offset=0x18 Size=0x4
};

struct _D3DTRIPATCH_INFO// Size=0x10 (Id=1327)
{
    unsigned int StartVertexOffset;// Offset=0x0 Size=0x4
    unsigned int NumVertices;// Offset=0x4 Size=0x4
    enum _D3DBASISTYPE Basis;// Offset=0x8 Size=0x4
    enum _D3DORDERTYPE Order;// Offset=0xc Size=0x4
};

struct _D3DSTREAM_INPUT// Size=0xc (Id=1328)
{
    struct D3DVertexBuffer * VertexBuffer;// Offset=0x0 Size=0x4
    unsigned int Stride;// Offset=0x4 Size=0x4
    unsigned int Offset;// Offset=0x8 Size=0x4
};

struct _D3DTILE// Size=0x18 (Id=1329)
{
    unsigned long Flags;// Offset=0x0 Size=0x4
    void * pMemory;// Offset=0x4 Size=0x4
    unsigned long Size;// Offset=0x8 Size=0x4
    unsigned long Pitch;// Offset=0xc Size=0x4
    unsigned long ZStartTag;// Offset=0x10 Size=0x4
    unsigned long ZOffset;// Offset=0x14 Size=0x4
};

struct _D3DCOPYRECTSTATE// Size=0x20 (Id=1330)
{
    enum _D3DCOPYRECTCOLORFORMAT ColorFormat;// Offset=0x0 Size=0x4
    enum _D3DCOPYRECTOPERATION Operation;// Offset=0x4 Size=0x4
    int ColorKeyEnable;// Offset=0x8 Size=0x4
    unsigned long ColorKeyValue;// Offset=0xc Size=0x4
    unsigned long BlendAlpha;// Offset=0x10 Size=0x4
    unsigned long PremultAlpha;// Offset=0x14 Size=0x4
    unsigned long ClippingPoint;// Offset=0x18 Size=0x4
    unsigned long ClippingSize;// Offset=0x1c Size=0x4
};

struct _D3DCOPYRECTROPSTATE// Size=0x20 (Id=1331)
{
    unsigned long Rop;// Offset=0x0 Size=0x4
    unsigned long Shape;// Offset=0x4 Size=0x4
    unsigned long PatternSelect;// Offset=0x8 Size=0x4
    unsigned long MonoColor0;// Offset=0xc Size=0x4
    unsigned long MonoColor1;// Offset=0x10 Size=0x4
    unsigned long MonoPattern0;// Offset=0x14 Size=0x4
    unsigned long MonoPattern1;// Offset=0x18 Size=0x4
    unsigned long * ColorPattern;// Offset=0x1c Size=0x4
};

struct D3DDevice// Size=0x1 (Id=1332)
{
    union // Size=0x28 (Id=0)
    {
        unsigned long AddRef();// Offset=0x0 Size=0x5
        unsigned long Release();
        long GetDirect3D(struct Direct3D ** );// Offset=0x0 Size=0xe
        long GetDeviceCaps(struct _D3DCAPS8 * );// Offset=0x0 Size=0xe
        long GetDisplayMode(struct _D3DDISPLAYMODE * );// Offset=0x0 Size=0xe
        long GetCreationParameters(struct _D3DDEVICE_CREATION_PARAMETERS * );
        long Reset(struct _D3DPRESENT_PARAMETERS_ * );
        long Present(struct tagRECT * ,struct tagRECT * ,void * ,void * );// Offset=0x0 Size=0xc
        long GetBackBuffer(int ,unsigned long ,struct D3DSurface ** );// Offset=0x0 Size=0x19
        long GetRasterStatus(struct _D3DRASTER_STATUS * );
        void SetGammaRamp(unsigned long ,struct _D3DGAMMARAMP * );
        void GetGammaRamp(struct _D3DGAMMARAMP * );
        long CreateTexture(unsigned int ,unsigned int ,unsigned int ,unsigned long ,enum _D3DFORMAT ,unsigned long ,struct D3DTexture ** );
        long CreateVolumeTexture(unsigned int ,unsigned int ,unsigned int ,unsigned int ,unsigned long ,enum _D3DFORMAT ,unsigned long ,struct D3DVolumeTexture ** );// Offset=0x0 Size=0x9
        long CreateCubeTexture(unsigned int ,unsigned int ,unsigned long ,enum _D3DFORMAT ,unsigned long ,struct D3DCubeTexture ** );// Offset=0x0 Size=0x9
        long CreateVertexBuffer(unsigned int ,unsigned long ,unsigned long ,unsigned long ,struct D3DVertexBuffer ** );// Offset=0x0 Size=0x5
        long CreateIndexBuffer(unsigned int ,unsigned long ,enum _D3DFORMAT ,unsigned long ,struct D3DIndexBuffer ** );
        long CreateRenderTarget(unsigned int ,unsigned int ,enum _D3DFORMAT ,unsigned long ,int ,struct D3DSurface ** );
        long CreateDepthStencilSurface(unsigned int ,unsigned int ,enum _D3DFORMAT ,unsigned long ,struct D3DSurface ** );
        long CreateImageSurface(unsigned int ,unsigned int ,enum _D3DFORMAT ,struct D3DSurface ** );
        long CopyRects(struct D3DSurface * ,struct tagRECT * ,unsigned int ,struct D3DSurface * ,struct tagPOINT * );
        long SetRenderTarget(struct D3DSurface * ,struct D3DSurface * );// Offset=0x0 Size=0x14
        long GetRenderTarget(struct D3DSurface ** );
        long GetDepthStencilSurface(struct D3DSurface ** );// Offset=0x0 Size=0x5
        long BeginScene();// Offset=0x0 Size=0x3
        long EndScene();// Offset=0x0 Size=0x3
        long Clear(unsigned long ,struct _D3DRECT * ,unsigned long ,unsigned long ,float ,unsigned long );// Offset=0x0 Size=0x28
        long SetTransform(enum _D3DTRANSFORMSTATETYPE ,struct _D3DMATRIX * );
        long GetTransform(enum _D3DTRANSFORMSTATETYPE ,struct _D3DMATRIX * );
        long MultiplyTransform(enum _D3DTRANSFORMSTATETYPE ,struct _D3DMATRIX * );
        long SetViewport(struct _D3DVIEWPORT8 * );// Offset=0x0 Size=0xf
        long GetViewport(struct _D3DVIEWPORT8 * );// Offset=0x0 Size=0xf
        long SetMaterial(struct _D3DMATERIAL8 * );
        long GetMaterial(struct _D3DMATERIAL8 * );
        long SetLight(unsigned long ,struct _D3DLIGHT8 * );// Offset=0x0 Size=0x5
        long GetLight(unsigned long ,struct _D3DLIGHT8 * );
        long LightEnable(unsigned long ,int );// Offset=0x0 Size=0x5
        long GetLightEnable(unsigned long ,int * );
        long SetRenderState(enum _D3DRENDERSTATETYPE ,unsigned long );
        long GetRenderState(enum _D3DRENDERSTATETYPE ,unsigned long * );
        long ApplyStateBlock(unsigned long );
        long CaptureStateBlock(unsigned long );
        long DeleteStateBlock(unsigned long );
        long CreateStateBlock(enum _D3DSTATEBLOCKTYPE ,unsigned long * );
        long GetTexture(unsigned long ,struct D3DBaseTexture ** );
        long SetTexture(unsigned long ,struct D3DBaseTexture * );// Offset=0x0 Size=0x14
        long GetTextureStageState(unsigned long ,enum _D3DTEXTURESTAGESTATETYPE ,unsigned long * );
        long SetTextureStageState(unsigned long ,enum _D3DTEXTURESTAGESTATETYPE ,unsigned long );
        long DrawPrimitive(enum _D3DPRIMITIVETYPE ,unsigned int ,unsigned int );
        long DrawIndexedPrimitive(enum _D3DPRIMITIVETYPE ,unsigned int ,unsigned int ,unsigned int ,unsigned int );
        long DrawPrimitiveUP(enum _D3DPRIMITIVETYPE ,unsigned int ,void * ,unsigned int );
        long DrawIndexedPrimitiveUP(enum _D3DPRIMITIVETYPE ,unsigned int ,unsigned int ,unsigned int ,void * ,enum _D3DFORMAT ,void * ,unsigned int );
        long CreateVertexShader(unsigned long * ,unsigned long * ,unsigned long * ,unsigned long );
        long SetVertexShader(unsigned long );// Offset=0x0 Size=0xf
        long GetVertexShader(unsigned long * );
        long DeleteVertexShader(unsigned long );
        long SetVertexShaderConstant(int ,void * ,unsigned long );
        long GetVertexShaderConstant(int ,void * ,unsigned long );
        long GetVertexShaderDeclaration(unsigned long ,void * ,unsigned long * );
        long GetVertexShaderFunction(unsigned long ,void * ,unsigned long * );
        long SetStreamSource(unsigned int ,struct D3DVertexBuffer * ,unsigned int );// Offset=0x0 Size=0x19
        long GetStreamSource(unsigned int ,struct D3DVertexBuffer ** ,unsigned int * );
        long SetIndices(struct D3DIndexBuffer * ,unsigned int );
        long GetIndices(struct D3DIndexBuffer ** ,unsigned int * );
        long CreatePixelShader(struct _D3DPixelShaderDef * ,unsigned long * );
        long SetPixelShader(unsigned long );
        long SetPixelShaderProgram(struct _D3DPixelShaderDef * );
        long GetPixelShader(unsigned long * );
        long DeletePixelShader(unsigned long );
        long SetPixelShaderConstant(unsigned long ,void * ,unsigned long );
        long GetPixelShaderConstant(unsigned long ,void * ,unsigned long );
        long GetPixelShaderFunction(unsigned long ,struct _D3DPixelShaderDef * );
        long DrawRectPatch(unsigned int ,float * ,struct _D3DRECTPATCH_INFO * );
        long DrawTriPatch(unsigned int ,float * ,struct _D3DTRIPATCH_INFO * );
        long DeletePatch(unsigned int );
        long SetShaderConstantMode(unsigned long );// Offset=0x0 Size=0xf
        long GetShaderConstantMode(unsigned long * );
        long LoadVertexShader(unsigned long ,unsigned long );
        long LoadVertexShaderProgram(unsigned long * ,unsigned long );
        long SelectVertexShader(unsigned long ,unsigned long );
        long RunVertexStateShader(unsigned long ,float * );
        long GetVertexShaderSize(unsigned long ,unsigned int * );
        long GetVertexShaderType(unsigned long ,unsigned long * );
        long DrawVertices(enum _D3DPRIMITIVETYPE ,unsigned int ,unsigned int );
        long DrawIndexedVertices(enum _D3DPRIMITIVETYPE ,unsigned int ,unsigned short * );
        long DrawVerticesUP(enum _D3DPRIMITIVETYPE ,unsigned int ,void * ,unsigned int );
        long DrawIndexedVerticesUP(enum _D3DPRIMITIVETYPE ,unsigned int ,void * ,void * ,unsigned int );
        long PrimeVertexCache(unsigned int ,unsigned short * );
        long CreatePalette(enum _D3DPALETTESIZE ,struct D3DPalette ** );
        long GetPalette(unsigned long ,struct D3DPalette ** );
        long SetPalette(unsigned long ,struct D3DPalette * );// Offset=0x0 Size=0x14
        long SetTextureStageStateNotInline(unsigned long ,enum _D3DTEXTURESTAGESTATETYPE ,unsigned long );
        long SetRenderStateNotInline(enum _D3DRENDERSTATETYPE ,unsigned long );
        long SetBackMaterial(struct _D3DMATERIAL8 * );
        long GetBackMaterial(struct _D3DMATERIAL8 * );
        long UpdateOverlay(struct D3DSurface * ,struct tagRECT * ,struct tagRECT * ,int ,unsigned long );// Offset=0x0 Size=0x23
        long EnableOverlay(int );// Offset=0x0 Size=0xf
        long EnableCC(int );
        long SendCC(int ,unsigned char ,unsigned char );
        long GetCCStatus(int * ,int * );
        long BeginVisibilityTest();
        long EndVisibilityTest(unsigned long );
        long GetVisibilityTestResult(unsigned long ,unsigned int * ,unsigned long long * );
        int GetOverlayUpdateStatus();
        long GetDisplayFieldStatus(struct _D3DFIELD_STATUS * );
        long SetVertexData2f(int ,float ,float );
        long SetVertexData4f(int ,float ,float ,float ,float );
        long SetVertexData2s(int ,short ,short );
        long SetVertexData4s(int ,short ,short ,short ,short );
        long SetVertexData4ub(int ,unsigned char ,unsigned char ,unsigned char ,unsigned char );
        long SetVertexDataColor(int ,unsigned long );
        long Begin(enum _D3DPRIMITIVETYPE );
        long End();
        long CreateFixup(unsigned int ,struct D3DFixup ** );
        long CreatePushBuffer(unsigned int ,int ,struct D3DPushBuffer ** );
        long BeginPushBuffer(struct D3DPushBuffer * );
        long EndPushBuffer();
        long RunPushBuffer(struct D3DPushBuffer * ,struct D3DFixup * );
        long GetPushBufferOffset(unsigned long * );
        long Nop();
        long GetProjectionViewportMatrix(struct _D3DMATRIX * );
        long SetModelView(struct _D3DMATRIX * ,struct _D3DMATRIX * ,struct _D3DMATRIX * );
        long GetModelView(struct _D3DMATRIX * );
        long SetVertexBlendModelView(unsigned int ,struct _D3DMATRIX * ,struct _D3DMATRIX * ,struct _D3DMATRIX * );
        long GetVertexBlendModelView(unsigned int ,struct _D3DMATRIX * ,struct _D3DMATRIX * );
        long SetVertexShaderInput(unsigned long ,unsigned int ,struct _D3DSTREAM_INPUT * );
        long GetVertexShaderInput(unsigned long * ,unsigned int * ,struct _D3DSTREAM_INPUT * );
        long SwitchTexture(unsigned long ,struct D3DBaseTexture * );
        long Suspend();
        long Resume(int );
        long SetScissors(unsigned long ,int ,struct _D3DRECT * );
        long GetScissors(unsigned long * ,int * ,struct _D3DRECT * );
        long SetTile(unsigned long ,struct _D3DTILE * );// Offset=0x0 Size=0x14
        long GetTile(unsigned long ,struct _D3DTILE * );
        unsigned long GetTileCompressionTags(unsigned long ,unsigned long );
        void SetTileCompressionTagBits(unsigned long ,unsigned long ,unsigned long * ,unsigned long );
        void GetTileCompressionTagBits(unsigned long ,unsigned long ,unsigned long * ,unsigned long );
        long BeginPush(unsigned long ,unsigned long ** );
        long EndPush(unsigned long * );
        int IsBusy();
        void BlockUntilIdle();
        void KickPushBuffer();
        void SetVerticalBlankCallback(void  ( * )(struct _D3DVBLANKDATA * ));
        void SetSwapCallback(void  ( * )(struct _D3DSWAPDATA * ));
        void BlockUntilVerticalBlank();// Offset=0x0 Size=0x5
        unsigned long InsertFence();
        int IsFencePending(unsigned long );
        void BlockOnFence(unsigned long );
        void InsertCallback(enum _D3DCALLBACKTYPE ,void  ( * )(unsigned long ),unsigned long );
        void FlushVertexCache();
        void SetFlickerFilter(unsigned long );
        void SetSoftDisplayFilter(int );
        long SetCopyRectsState(struct _D3DCOPYRECTSTATE * ,struct _D3DCOPYRECTROPSTATE * );
        long GetCopyRectsState(struct _D3DCOPYRECTSTATE * ,struct _D3DCOPYRECTROPSTATE * );
        long PersistDisplay();
        long GetPersistedSurface(struct D3DSurface ** );
        unsigned long Swap(unsigned long );
        long SetBackBufferScale(float ,float );
        long GetBackBufferScale(float * ,float * );
        long SetScreenSpaceOffset(float ,float );// Offset=0x0 Size=0x14
        long GetScreenSpaceOffset(float * ,float * );
        void SetOverscanColor(unsigned long );
        unsigned long GetOverscanColor();
        unsigned long SetDebugMarker(unsigned long );// Offset=0x0 Size=0x5
        unsigned long GetDebugMarker();
    };
};

struct ID3DXTechnique// Size=0x4 (Id=1340)
{
    struct ID3DXTechniqueVtbl * lpVtbl;// Offset=0x0 Size=0x4
};

struct ID3DXTechnique : public IUnknown// Size=0x4 (Id=1341)
{
    long QueryInterface(struct _GUID & ,void ** );
    unsigned long AddRef();
    unsigned long Release();
    long GetDevice(struct D3DDevice ** );
    long GetDesc(struct _D3DXTECHNIQUE_DESC * );
    long GetPassDesc(unsigned int ,struct _D3DXPASS_DESC * );
    int IsParameterUsed(unsigned long );
    long Validate();
    long Begin(unsigned int * );
    long Pass(unsigned int );
    long End();
    void ID3DXTechnique(struct ID3DXTechnique & );
    void ID3DXTechnique();
};

struct D3DXMATRIX : public _D3DMATRIX// Size=0x40 (Id=1342)
{
    void D3DXMATRIX(float ,float ,float ,float ,float ,float ,float ,float ,float ,float ,float ,float ,float ,float ,float ,float );
    void D3DXMATRIX(struct _D3DMATRIX & );
    void D3DXMATRIX(float * );
    void D3DXMATRIX();
    float operator()(unsigned int ,unsigned int );
    float & operator()(unsigned int ,unsigned int );
    float * operator float *();
    float * operator const float *();
    struct D3DXMATRIX & operator*=(float );
    struct D3DXMATRIX & operator*=(struct D3DXMATRIX & );
    struct D3DXMATRIX & operator+=(struct D3DXMATRIX & );
    struct D3DXMATRIX & operator-=(struct D3DXMATRIX & );
    struct D3DXMATRIX & operator/=(float );
    struct D3DXMATRIX operator+(struct D3DXMATRIX & );
    struct D3DXMATRIX operator+();
    struct D3DXMATRIX operator-(struct D3DXMATRIX & );
    struct D3DXMATRIX operator-();
    struct D3DXMATRIX operator*(float );
    struct D3DXMATRIX operator*(struct D3DXMATRIX & );
    struct D3DXMATRIX operator/(float );
    int operator==(struct D3DXMATRIX & );
    int operator!=(struct D3DXMATRIX & );
};

struct ID3DXSkinMesh// Size=0x4 (Id=1347)
{
    struct ID3DXSkinMeshVtbl * lpVtbl;// Offset=0x0 Size=0x4
};

struct ID3DXSkinMesh : public IUnknown// Size=0x4 (Id=1348)
{
    long QueryInterface(struct _GUID & ,void ** );
    unsigned long AddRef();
    unsigned long Release();
    unsigned long GetNumFaces();
    unsigned long GetNumVertices();
    unsigned long GetFVF();
    long GetDeclaration(unsigned long * );
    unsigned long GetOptions();
    long GetDevice(struct D3DDevice ** );
    long GetVertexBuffer(struct D3DVertexBuffer ** );
    long GetIndexBuffer(struct D3DIndexBuffer ** );
    long LockVertexBuffer(unsigned long ,unsigned char ** );
    long UnlockVertexBuffer();
    long LockIndexBuffer(unsigned long ,unsigned char ** );
    long UnlockIndexBuffer();
    long LockAttributeBuffer(unsigned long ,unsigned long ** );
    long UnlockAttributeBuffer();
    unsigned long GetNumBones();
    long GetOriginalMesh(struct ID3DXMesh ** );
    long SetBoneInfluence(unsigned long ,unsigned long ,unsigned long * ,float * );
    unsigned long GetNumBoneInfluences(unsigned long );
    long GetBoneInfluence(unsigned long ,unsigned long * ,float * );
    long GetMaxVertexInfluences(unsigned long * );
    long GetMaxFaceInfluences(unsigned long * );
    long ConvertToBlendedMesh(unsigned long ,const unsigned long * ,unsigned long * ,unsigned long * ,struct ID3DXBuffer ** ,struct ID3DXMesh ** );
    long ConvertToIndexedBlendedMesh(unsigned long ,const unsigned long * ,unsigned long ,unsigned long * ,unsigned long * ,struct ID3DXBuffer ** ,struct ID3DXMesh ** );
    long GenerateSkinnedMesh(unsigned long ,float ,const unsigned long * ,unsigned long * ,struct ID3DXMesh ** );
    long UpdateSkinnedMesh(struct D3DXMATRIX * ,struct ID3DXMesh * );
    void ID3DXSkinMesh(struct ID3DXSkinMesh & );
    void ID3DXSkinMesh();
};

struct D3DX_BLT// Size=0x4c (Id=1352)
{
    void * pData;// Offset=0x0 Size=0x4
    enum _D3DFORMAT Format;// Offset=0x4 Size=0x4
    unsigned int RowPitch;// Offset=0x8 Size=0x4
    unsigned int SlicePitch;// Offset=0xc Size=0x4
    struct _D3DBOX Region;// Offset=0x10 Size=0x18
    struct _D3DBOX SubRegion;// Offset=0x28 Size=0x18
    int bDither;// Offset=0x40 Size=0x4
    unsigned long ColorKey;// Offset=0x44 Size=0x4
    struct tagPALETTEENTRY * pPalette;// Offset=0x48 Size=0x4
};

struct ID3DXSprite// Size=0x4 (Id=1362)
{
    struct ID3DXSpriteVtbl * lpVtbl;// Offset=0x0 Size=0x4
};

struct ID3DXSprite : public IUnknown// Size=0x4 (Id=1363)
{
    long QueryInterface(struct _GUID & ,void ** );
    unsigned long AddRef();
    unsigned long Release();
    long GetDevice(struct D3DDevice ** );
    long Begin();
    long Draw(struct D3DTexture * ,struct tagRECT * ,struct D3DXVECTOR2 * ,struct D3DXVECTOR2 * ,float ,struct D3DXVECTOR2 * ,unsigned long );
    long DrawTransform(struct D3DTexture * ,struct tagRECT * ,struct D3DXMATRIX * ,unsigned long );
    long End();
    void ID3DXSprite(struct ID3DXSprite & );
    void ID3DXSprite();
};

struct D3DVertexBuffer : public D3DResource// Size=0xc (Id=1377)
{
    long Lock(unsigned int ,unsigned int ,unsigned char ** ,unsigned long );// Offset=0x0 Size=0x23
    long Unlock();
    long GetDesc(struct _D3DVERTEXBUFFER_DESC * );
};

struct D3DXPLANE// Size=0x10 (Id=1382)
{
    float a;// Offset=0x0 Size=0x4
    float b;// Offset=0x4 Size=0x4
    float c;// Offset=0x8 Size=0x4
    float d;// Offset=0xc Size=0x4
};

struct D3DXPLANE// Size=0x10 (Id=1383)
{
    void D3DXPLANE(float ,float ,float ,float );
    void D3DXPLANE(float * );
    void D3DXPLANE();
    float * operator float *();
    float * operator const float *();
    struct D3DXPLANE operator+();
    struct D3DXPLANE operator-();
    int operator==(struct D3DXPLANE & );
    int operator!=(struct D3DXPLANE & );
    float a;// Offset=0x0 Size=0x4
    float b;// Offset=0x4 Size=0x4
    float c;// Offset=0x8 Size=0x4
    float d;// Offset=0xc Size=0x4
};

struct D3DPushBuffer : public D3DResource// Size=0x14 (Id=1389)
{
    long Verify(int );
    long BeginFixup(struct D3DFixup * ,int );
    long EndFixup();
    long RunPushBuffer(unsigned long ,struct D3DPushBuffer * ,struct D3DFixup * );
    long SetModelView(unsigned long ,struct _D3DMATRIX * ,struct _D3DMATRIX * ,struct _D3DMATRIX * );
    long SetVertexBlendModelView(unsigned long ,unsigned int ,struct _D3DMATRIX * ,struct _D3DMATRIX * ,struct _D3DMATRIX * );
    long SetVertexShaderInput(unsigned long ,unsigned long ,unsigned int ,struct _D3DSTREAM_INPUT * );
    long SetRenderTarget(unsigned long ,struct D3DSurface * ,struct D3DSurface * );
    long SetTexture(unsigned long ,unsigned long ,struct D3DBaseTexture * );
    long SetPalette(unsigned long ,unsigned long ,struct D3DPalette * );
    long EndVisibilityTest(unsigned long ,unsigned long );
    long SetVertexShaderConstant(unsigned long ,int ,void * ,unsigned long );
    long Jump(unsigned long ,unsigned int );
    long GetSize(unsigned long * );
    unsigned char __align0[12];// Offset=0x0 Size=0xc
    unsigned long Size;// Offset=0xc Size=0x4
    unsigned long AllocationSize;// Offset=0x10 Size=0x4
};

struct D3DPixelContainer : public D3DResource// Size=0x14 (Id=1392)
{
    unsigned char __align0[12];// Offset=0x0 Size=0xc
    unsigned long Format;// Offset=0xc Size=0x4
    unsigned long Size;// Offset=0x10 Size=0x4
};

struct D3DTexture : public D3DBaseTexture// Size=0x14 (Id=1397)
{
    union // Size=0x23 (Id=0)
    {
        long GetLevelDesc(unsigned int ,struct _D3DSURFACE_DESC * );// Offset=0x0 Size=0x16
        long GetSurfaceLevel(unsigned int ,struct D3DSurface ** );// Offset=0x0 Size=0x5
        long LockRect(unsigned int ,struct _D3DLOCKED_RECT * ,struct tagRECT * ,unsigned long );// Offset=0x0 Size=0x23
        long UnlockRect(unsigned int );// Offset=0x0 Size=0x5
    };
};

struct D3DFixup : public D3DResource// Size=0x18 (Id=1398)
{
    long Reset();
    long GetSize(unsigned long * );
    long GetSpace(unsigned long * );
    unsigned char __align0[12];// Offset=0x0 Size=0xc
    unsigned long Run;// Offset=0xc Size=0x4
    unsigned long Next;// Offset=0x10 Size=0x4
    unsigned long Size;// Offset=0x14 Size=0x4
};

struct D3DDevice// Size=0x1 (Id=1411)
{
    union // Size=0x28 (Id=0)
    {
        unsigned long AddRef();// Offset=0x0 Size=0x5
        unsigned long Release();
        long GetDirect3D(struct Direct3D ** );// Offset=0x0 Size=0xe
        long GetDeviceCaps(struct _D3DCAPS8 * );// Offset=0x0 Size=0xe
        long GetDisplayMode(struct _D3DDISPLAYMODE * );// Offset=0x0 Size=0xe
        long GetCreationParameters(struct _D3DDEVICE_CREATION_PARAMETERS * );
        long Reset(struct _D3DPRESENT_PARAMETERS_ * );
        long Present(struct tagRECT * ,struct tagRECT * ,void * ,void * );// Offset=0x0 Size=0xc
        long GetBackBuffer(int ,unsigned long ,struct D3DSurface ** );// Offset=0x0 Size=0x19
        long GetRasterStatus(struct _D3DRASTER_STATUS * );
        void SetGammaRamp(unsigned long ,struct _D3DGAMMARAMP * );
        void GetGammaRamp(struct _D3DGAMMARAMP * );
        long CreateTexture(unsigned int ,unsigned int ,unsigned int ,unsigned long ,enum _D3DFORMAT ,unsigned long ,struct D3DTexture ** );
        long CreateVolumeTexture(unsigned int ,unsigned int ,unsigned int ,unsigned int ,unsigned long ,enum _D3DFORMAT ,unsigned long ,struct D3DVolumeTexture ** );// Offset=0x0 Size=0x9
        long CreateCubeTexture(unsigned int ,unsigned int ,unsigned long ,enum _D3DFORMAT ,unsigned long ,struct D3DCubeTexture ** );// Offset=0x0 Size=0x9
        long CreateVertexBuffer(unsigned int ,unsigned long ,unsigned long ,unsigned long ,struct D3DVertexBuffer ** );// Offset=0x0 Size=0x5
        long CreateIndexBuffer(unsigned int ,unsigned long ,enum _D3DFORMAT ,unsigned long ,struct D3DIndexBuffer ** );
        long CreateRenderTarget(unsigned int ,unsigned int ,enum _D3DFORMAT ,unsigned long ,int ,struct D3DSurface ** );
        long CreateDepthStencilSurface(unsigned int ,unsigned int ,enum _D3DFORMAT ,unsigned long ,struct D3DSurface ** );
        long CreateImageSurface(unsigned int ,unsigned int ,enum _D3DFORMAT ,struct D3DSurface ** );
        long CopyRects(struct D3DSurface * ,struct tagRECT * ,unsigned int ,struct D3DSurface * ,struct tagPOINT * );
        long SetRenderTarget(struct D3DSurface * ,struct D3DSurface * );// Offset=0x0 Size=0x14
        long GetRenderTarget(struct D3DSurface ** );
        long GetDepthStencilSurface(struct D3DSurface ** );// Offset=0x0 Size=0x5
        long BeginScene();// Offset=0x0 Size=0x3
        long EndScene();// Offset=0x0 Size=0x3
        long Clear(unsigned long ,struct _D3DRECT * ,unsigned long ,unsigned long ,float ,unsigned long );// Offset=0x0 Size=0x28
        long SetTransform(enum _D3DTRANSFORMSTATETYPE ,struct _D3DMATRIX * );
        long GetTransform(enum _D3DTRANSFORMSTATETYPE ,struct _D3DMATRIX * );
        long MultiplyTransform(enum _D3DTRANSFORMSTATETYPE ,struct _D3DMATRIX * );
        long SetViewport(struct _D3DVIEWPORT8 * );// Offset=0x0 Size=0xf
        long GetViewport(struct _D3DVIEWPORT8 * );// Offset=0x0 Size=0xf
        long SetMaterial(struct _D3DMATERIAL8 * );
        long GetMaterial(struct _D3DMATERIAL8 * );
        long SetLight(unsigned long ,struct _D3DLIGHT8 * );// Offset=0x0 Size=0x5
        long GetLight(unsigned long ,struct _D3DLIGHT8 * );
        long LightEnable(unsigned long ,int );// Offset=0x0 Size=0x5
        long GetLightEnable(unsigned long ,int * );
        long SetRenderState(enum _D3DRENDERSTATETYPE ,unsigned long );
        long GetRenderState(enum _D3DRENDERSTATETYPE ,unsigned long * );
        long BeginStateBlock();
        long EndStateBlock(unsigned long * );
        long ApplyStateBlock(unsigned long );
        long CaptureStateBlock(unsigned long );
        long DeleteStateBlock(unsigned long );
        long CreateStateBlock(enum _D3DSTATEBLOCKTYPE ,unsigned long * );
        long GetTexture(unsigned long ,struct D3DBaseTexture ** );
        long SetTexture(unsigned long ,struct D3DBaseTexture * );// Offset=0x0 Size=0x14
        long GetTextureStageState(unsigned long ,enum _D3DTEXTURESTAGESTATETYPE ,unsigned long * );
        long SetTextureStageState(unsigned long ,enum _D3DTEXTURESTAGESTATETYPE ,unsigned long );
        long DrawPrimitive(enum _D3DPRIMITIVETYPE ,unsigned int ,unsigned int );
        long DrawIndexedPrimitive(enum _D3DPRIMITIVETYPE ,unsigned int ,unsigned int ,unsigned int ,unsigned int );
        long DrawPrimitiveUP(enum _D3DPRIMITIVETYPE ,unsigned int ,void * ,unsigned int );
        long DrawIndexedPrimitiveUP(enum _D3DPRIMITIVETYPE ,unsigned int ,unsigned int ,unsigned int ,void * ,enum _D3DFORMAT ,void * ,unsigned int );
        long CreateVertexShader(unsigned long * ,unsigned long * ,unsigned long * ,unsigned long );
        long SetVertexShader(unsigned long );// Offset=0x0 Size=0xf
        long GetVertexShader(unsigned long * );
        long DeleteVertexShader(unsigned long );
        long SetVertexShaderConstant(int ,void * ,unsigned long );
        long GetVertexShaderConstant(int ,void * ,unsigned long );
        long GetVertexShaderDeclaration(unsigned long ,void * ,unsigned long * );
        long GetVertexShaderFunction(unsigned long ,void * ,unsigned long * );
        long SetStreamSource(unsigned int ,struct D3DVertexBuffer * ,unsigned int );// Offset=0x0 Size=0x19
        long GetStreamSource(unsigned int ,struct D3DVertexBuffer ** ,unsigned int * );
        long SetIndices(struct D3DIndexBuffer * ,unsigned int );
        long GetIndices(struct D3DIndexBuffer ** ,unsigned int * );
        long CreatePixelShader(struct _D3DPixelShaderDef * ,unsigned long * );
        long SetPixelShader(unsigned long );
        long SetPixelShaderProgram(struct _D3DPixelShaderDef * );
        long GetPixelShader(unsigned long * );
        long DeletePixelShader(unsigned long );
        long SetPixelShaderConstant(unsigned long ,void * ,unsigned long );
        long GetPixelShaderConstant(unsigned long ,void * ,unsigned long );
        long GetPixelShaderFunction(unsigned long ,struct _D3DPixelShaderDef * );
        long DrawRectPatch(unsigned int ,float * ,struct _D3DRECTPATCH_INFO * );
        long DrawTriPatch(unsigned int ,float * ,struct _D3DTRIPATCH_INFO * );
        long DeletePatch(unsigned int );
        long SetShaderConstantMode(unsigned long );// Offset=0x0 Size=0xf
        long GetShaderConstantMode(unsigned long * );
        long LoadVertexShader(unsigned long ,unsigned long );
        long LoadVertexShaderProgram(unsigned long * ,unsigned long );
        long SelectVertexShader(unsigned long ,unsigned long );
        long RunVertexStateShader(unsigned long ,float * );
        long GetVertexShaderSize(unsigned long ,unsigned int * );
        long GetVertexShaderType(unsigned long ,unsigned long * );
        long DrawVertices(enum _D3DPRIMITIVETYPE ,unsigned int ,unsigned int );
        long DrawIndexedVertices(enum _D3DPRIMITIVETYPE ,unsigned int ,unsigned short * );
        long DrawVerticesUP(enum _D3DPRIMITIVETYPE ,unsigned int ,void * ,unsigned int );
        long DrawIndexedVerticesUP(enum _D3DPRIMITIVETYPE ,unsigned int ,void * ,void * ,unsigned int );
        long PrimeVertexCache(unsigned int ,unsigned short * );
        long CreatePalette(enum _D3DPALETTESIZE ,struct D3DPalette ** );
        long GetPalette(unsigned long ,struct D3DPalette ** );
        long SetPalette(unsigned long ,struct D3DPalette * );// Offset=0x0 Size=0x14
        long SetTextureStageStateNotInline(unsigned long ,enum _D3DTEXTURESTAGESTATETYPE ,unsigned long );
        long SetRenderStateNotInline(enum _D3DRENDERSTATETYPE ,unsigned long );
        long SetBackMaterial(struct _D3DMATERIAL8 * );
        long GetBackMaterial(struct _D3DMATERIAL8 * );
        long UpdateOverlay(struct D3DSurface * ,struct tagRECT * ,struct tagRECT * ,int ,unsigned long );// Offset=0x0 Size=0x23
        long EnableOverlay(int );// Offset=0x0 Size=0xf
        long EnableCC(int );
        long SendCC(int ,unsigned char ,unsigned char );
        long GetCCStatus(int * ,int * );
        long BeginVisibilityTest();
        long EndVisibilityTest(unsigned long );
        long GetVisibilityTestResult(unsigned long ,unsigned int * ,unsigned long long * );
        int GetOverlayUpdateStatus();
        long GetDisplayFieldStatus(struct _D3DFIELD_STATUS * );
        long SetVertexData2f(int ,float ,float );
        long SetVertexData4f(int ,float ,float ,float ,float );
        long SetVertexData2s(int ,short ,short );
        long SetVertexData4s(int ,short ,short ,short ,short );
        long SetVertexData4ub(int ,unsigned char ,unsigned char ,unsigned char ,unsigned char );
        long SetVertexDataColor(int ,unsigned long );
        long Begin(enum _D3DPRIMITIVETYPE );
        long End();
        long CreateFixup(unsigned int ,struct D3DFixup ** );
        long CreatePushBuffer(unsigned int ,int ,struct D3DPushBuffer ** );
        long BeginPushBuffer(struct D3DPushBuffer * );
        long EndPushBuffer();
        long RunPushBuffer(struct D3DPushBuffer * ,struct D3DFixup * );
        long GetPushBufferOffset(unsigned long * );
        long Nop();
        long GetProjectionViewportMatrix(struct _D3DMATRIX * );
        long SetModelView(struct _D3DMATRIX * ,struct _D3DMATRIX * ,struct _D3DMATRIX * );
        long GetModelView(struct _D3DMATRIX * );
        long SetVertexBlendModelView(unsigned int ,struct _D3DMATRIX * ,struct _D3DMATRIX * ,struct _D3DMATRIX * );
        long GetVertexBlendModelView(unsigned int ,struct _D3DMATRIX * ,struct _D3DMATRIX * );
        long SetVertexShaderInput(unsigned long ,unsigned int ,struct _D3DSTREAM_INPUT * );
        long GetVertexShaderInput(unsigned long * ,unsigned int * ,struct _D3DSTREAM_INPUT * );
        long SwitchTexture(unsigned long ,struct D3DBaseTexture * );
        long Suspend();
        long Resume(int );
        long SetScissors(unsigned long ,int ,struct _D3DRECT * );
        long GetScissors(unsigned long * ,int * ,struct _D3DRECT * );
        long SetTile(unsigned long ,struct _D3DTILE * );// Offset=0x0 Size=0x14
        long GetTile(unsigned long ,struct _D3DTILE * );
        unsigned long GetTileCompressionTags(unsigned long ,unsigned long );
        void SetTileCompressionTagBits(unsigned long ,unsigned long ,unsigned long * ,unsigned long );
        void GetTileCompressionTagBits(unsigned long ,unsigned long ,unsigned long * ,unsigned long );
        long BeginPush(unsigned long ,unsigned long ** );
        long EndPush(unsigned long * );
        int IsBusy();
        void BlockUntilIdle();
        void KickPushBuffer();
        void SetVerticalBlankCallback(void  ( * )(struct _D3DVBLANKDATA * ));
        void SetSwapCallback(void  ( * )(struct _D3DSWAPDATA * ));
        void BlockUntilVerticalBlank();// Offset=0x0 Size=0x5
        unsigned long InsertFence();
        int IsFencePending(unsigned long );
        void BlockOnFence(unsigned long );
        void InsertCallback(enum _D3DCALLBACKTYPE ,void  ( * )(unsigned long ),unsigned long );
        void FlushVertexCache();
        void SetFlickerFilter(unsigned long );
        void SetSoftDisplayFilter(int );
        long SetCopyRectsState(struct _D3DCOPYRECTSTATE * ,struct _D3DCOPYRECTROPSTATE * );
        long GetCopyRectsState(struct _D3DCOPYRECTSTATE * ,struct _D3DCOPYRECTROPSTATE * );
        long PersistDisplay();
        long GetPersistedSurface(struct D3DSurface ** );
        unsigned long Swap(unsigned long );
        long SetBackBufferScale(float ,float );
        long GetBackBufferScale(float * ,float * );
        long SetScreenSpaceOffset(float ,float );// Offset=0x0 Size=0x14
        long GetScreenSpaceOffset(float * ,float * );
        void SetOverscanColor(unsigned long );
        unsigned long GetOverscanColor();
        unsigned long SetDebugMarker(unsigned long );// Offset=0x0 Size=0x5
        unsigned long GetDebugMarker();
    };
};

enum D3D::NV_PATCH_PRIMITIVE_TYPE
{
    NV_PATCH_PRIMITIVE_TSTRIP=1,
    NV_PATCH_PRIMITIVE_TFAN=2
};

enum D3D::NV_PATCH_BASIS_TYPE
{
    NV_PATCH_BASIS_BEZIER=0,
    NV_PATCH_BASIS_BSPLINE=1,
    NV_PATCH_BASIS_CATMULL_ROM=2
};

enum D3D::NV_PATCH_VERTEX_FORMAT_TYPE
{
    NV_PATCH_VERTEX_FORMAT_FLOAT_1=0,
    NV_PATCH_VERTEX_FORMAT_FLOAT_2=1,
    NV_PATCH_VERTEX_FORMAT_FLOAT_3=2,
    NV_PATCH_VERTEX_FORMAT_FLOAT_4=3,
    NV_PATCH_VERTEX_FORMAT_D3DCOLOR=4,
    NV_PATCH_VERTEX_FORMAT_UBYTE=5,
    NV_PATCH_VERTEX_FORMAT_SHORT_2=5,
    NV_PATCH_VERTEX_FORMAT_SHORT_4=7
};

enum D3D::NV_PATCH_BACKEND_TYPE
{
    NV_PATCH_BACKEND_IMMEDIATE=1,
    NV_PATCH_BACKEND_CELSIUS=2,
    NV_PATCH_BACKEND_KELVIN=3,
    NV_PATCH_BACKEND_DP2=4
};

enum D3D::__unnamed
{
    AUTONONE=0,
    AUTONORMAL=1,
    AUTOTEXCOORD=2
};

enum D3D::SubChannel
{
    SUBCH_3D=0,
    SUBCH_MEMCOPY=1,
    SUBCH_RECTCOPY=2,
    SUBCH_RECTCOPYSURFACES=3,
    SUBCH_RECTCOPYOPTIONS=4,
    SUBCH_UNUSED0=5,
    SUBCH_UNUSED1=6,
    SUBCH_UNUSED2=7
};

enum D3D::__unnamed
{
    VERTEX_ARRAY=0,
    WEIGHT_ARRAY=1,
    NORMAL_ARRAY=2,
    DIFFUSE_ARRAY=3,
    SPEC_ARRAY=4,
    TEX0_ARRAY=9,
    TEX1_ARRAY=10,
    TEX2_ARRAY=11,
    TEX3_ARRAY=12
};

enum D3D::__unnamed
{
    STREAM_NONE=-1
};

struct D3D::PixelShader// Size=0xc (Id=1614)
{
    unsigned long RefCount;// Offset=0x0 Size=0x4
    unsigned long D3DOwned;// Offset=0x4 Size=0x4
    struct _D3DPixelShaderDef * pPSDef;// Offset=0x8 Size=0x4
};

union D3D::_HWREG// Size=0x4 (Id=1615)
{
    unsigned long Reg032[1];// Offset=0x0 Size=0x4
};

struct D3D::InlineAttributeData// Size=0x8 (Id=1616)
{
    unsigned long UP_Count;// Offset=0x0 Size=0x4
    unsigned long UP_Delta;// Offset=0x4 Size=0x4
};

class D3D::CDevice : public D3DDevice// Size=0x2ae0 (Id=1619)
{
    union // Size=0x2ae0 (Id=0)
    {
        struct _XMETAL_PushBuffer m_Pusher;// Offset=0x0 Size=0x8
        unsigned long m_StateFlags;// Offset=0x8 Size=0x4
        unsigned long m_TextureCubemapAndDimension[4];// Offset=0xc Size=0x10
        unsigned long m_IndexBase;// Offset=0x1c Size=0x4
        unsigned long m_CachedIndexBase;// Offset=0x20 Size=0x4
        unsigned long m_PushBufferSize;// Offset=0x0 Size=0x4
        unsigned long m_PushSegmentSize;// Offset=0x0 Size=0x4
        unsigned char __align0[32];// Offset=0x4 Size=0x20
        unsigned long * m_pPushBase;// Offset=0x24 Size=0x4
        unsigned long * m_pPushLimit;// Offset=0x28 Size=0x4
        unsigned long * m_pKickOff;// Offset=0x2c Size=0x4
        unsigned long m_CpuTime;// Offset=0x30 Size=0x4
        unsigned long * m_pGpuTime;// Offset=0x34 Size=0x4
        unsigned long m_PusherLastSegment;// Offset=0x38 Size=0x4
        unsigned long m_PusherSegmentMask;// Offset=0x3c Size=0x4
        unsigned long m_PusherPutRunTotal;// Offset=0x40 Size=0x4
        unsigned long m_PusherLastSize;// Offset=0x44 Size=0x4
        unsigned long m_LastRunPushBufferTime;// Offset=0x48 Size=0x4
        struct D3D::Fence * m_PusherSegment;// Offset=0x4c Size=0x4
        struct D3D::Fence m_PusherFence[64];// Offset=0x50 Size=0x300
        struct D3DPushBuffer * m_pPushBufferRecordResource;// Offset=0x350 Size=0x4
        unsigned long m_PushBufferRecordWrapSize;// Offset=0x354 Size=0x4
        unsigned long * m_pPushBufferRecordSavedThreshold;// Offset=0x358 Size=0x4
        unsigned long * m_pPushBufferRecordSavedPut;// Offset=0x35c Size=0x4
        unsigned long m_TextureControl0Enabled[4];// Offset=0x360 Size=0x10
        struct D3D::PixelShader * m_pPixelShader;// Offset=0x370 Size=0x4
        unsigned long m_ShaderUsesSpecFog;// Offset=0x374 Size=0x4
        unsigned long m_ShaderAdjustsTexMode;// Offset=0x378 Size=0x4
        unsigned long m_PSShaderStageProgram;// Offset=0x37c Size=0x4
        struct D3D::VertexShader * m_pVertexShader;// Offset=0x380 Size=0x4
        unsigned long m_VertexShaderHandle;// Offset=0x384 Size=0x4
        unsigned long m_VertexShaderStart;// Offset=0x388 Size=0x4
        struct D3DIndexBuffer * m_pIndexBuffer;// Offset=0x38c Size=0x4
        struct D3D::Light * m_pLights;// Offset=0x390 Size=0x4
        unsigned long m_LightCount;// Offset=0x394 Size=0x4
        struct D3D::Light * m_pActiveLights;// Offset=0x398 Size=0x4
        void * m_ReportAllocations[16];// Offset=0x39c Size=0x40
        unsigned long m_dwOpcode;// Offset=0x3dc Size=0x4
        unsigned long m_dwSnapshot;// Offset=0x3e0 Size=0x4
        unsigned long * m_pShaderCaptureBuffer;// Offset=0x3e4 Size=0x4
        unsigned long * m_pShaderCapturePtr;// Offset=0x3e8 Size=0x4
        unsigned long m_pPixelShaderConstants[16];// Offset=0x3ec Size=0x40
        struct D3D::PixelShader m_UserPixelShader;// Offset=0x42c Size=0xc
        unsigned long m_dwDebugMarker;// Offset=0x438 Size=0x4
        union D3D::_HWREG * m_NvBase;// Offset=0x43c Size=0x4
        unsigned long m_cRefs;// Offset=0x440 Size=0x4
        float m_WNear;// Offset=0x444 Size=0x4
        float m_WFar;// Offset=0x448 Size=0x4
        float m_InverseWFar;// Offset=0x44c Size=0x4
        float m_ZScale;// Offset=0x450 Size=0x4
        unsigned long m_TexGenInverseNeeded;// Offset=0x454 Size=0x4
        float m_SuperSampleScaleX;// Offset=0x458 Size=0x4
        float m_SuperSampleScaleY;// Offset=0x45c Size=0x4
        float m_SuperSampleScale;// Offset=0x460 Size=0x4
        float m_SuperSampleLODBias;// Offset=0x464 Size=0x4
        float m_AntiAliasScaleX;// Offset=0x468 Size=0x4
        float m_AntiAliasScaleY;// Offset=0x46c Size=0x4
        struct _D3DMATRIX m_ProjectionViewportTransform;// Offset=0x470 Size=0x40
        struct _D3DMATRIX m_ModelViewTransform[4];// Offset=0x4b0 Size=0x100
        struct _D3DMATRIX m_ProjectionViewport;// Offset=0x5b0 Size=0x40
        unsigned long m_VertexShaderInputHandle;// Offset=0x5f0 Size=0x4
        unsigned long m_VertexShaderInputCount;// Offset=0x5f4 Size=0x4
        struct _D3DSTREAM_INPUT m_VertexShaderInputStream[16];// Offset=0x5f8 Size=0xc0
        unsigned long m_InlineVertexDwords;// Offset=0x6b8 Size=0x4
        unsigned long m_InlineStartOffset;// Offset=0x6bc Size=0x4
        unsigned long m_InlineDelta;// Offset=0x6c0 Size=0x4
        struct D3D::InlineAttributeData m_InlineAttributeData[16];// Offset=0x6c4 Size=0x80
        unsigned long m_InlineAttributeCount;// Offset=0x744 Size=0x4
        unsigned char __align1[8];// Offset=0x748 Size=0x8
        struct _D3DMATRIX m_Transform[10];// Offset=0x750 Size=0x280
        struct _D3DVIEWPORT8 m_Viewport;// Offset=0x9d0 Size=0x18
        float m_ScreenSpaceOffsetX;// Offset=0x9e8 Size=0x4
        float m_ScreenSpaceOffsetY;// Offset=0x9ec Size=0x4
        struct _D3DMATERIAL8 m_Material;// Offset=0x9f0 Size=0x44
        struct _D3DMATERIAL8 m_BackMaterial;// Offset=0xa34 Size=0x44
        struct D3DBaseTexture * m_Textures[4];// Offset=0xa78 Size=0x10
        struct D3DPalette * m_Palettes[4];// Offset=0xa88 Size=0x10
        float m_PixelShaderConstants[16][4];// Offset=0xa98 Size=0x100
        float m_VertexShaderConstants[192][4];// Offset=0xb98 Size=0xc00
        unsigned long m_VertexShaderProgramSlots[136][4];// Offset=0x1798 Size=0x880
        unsigned long m_ConstantMode;// Offset=0x2018 Size=0x4
        struct _D3DCOPYRECTSTATE m_CopyRectState;// Offset=0x201c Size=0x20
        struct _D3DCOPYRECTROPSTATE m_CopyRectRopState;// Offset=0x203c Size=0x20
        unsigned long m_MultiSampleType;// Offset=0x205c Size=0x4
        unsigned long m_SwapEffect;// Offset=0x2060 Size=0x4
        unsigned long m_PresentationInterval;// Offset=0x2064 Size=0x4
        unsigned long m_SwapTime[2];// Offset=0x2068 Size=0x8
        struct D3DSurface * m_pRenderTarget;// Offset=0x2070 Size=0x4
        struct D3DSurface * m_pZBuffer;// Offset=0x2074 Size=0x4
        unsigned long m_FrameBufferCount;// Offset=0x2078 Size=0x4
        struct D3DSurface * m_pFrameBuffer[3];// Offset=0x207c Size=0xc
        struct D3DSurface * m_pAutoDepthBuffer;// Offset=0x2088 Size=0x4
        struct D3DSurface m_BufferSurfaces[3];// Offset=0x208c Size=0x48
        struct D3DSurface m_DepthStencilSurface;// Offset=0x20d4 Size=0x18
        void * m_pFrameBufferBase;// Offset=0x20ec Size=0x4
        void * m_pAntiAliasBufferBase;// Offset=0x20f0 Size=0x4
        void * m_pAutoDepthBufferBase;// Offset=0x20f4 Size=0x4
        struct D3DSurface * m_pSaveRenderTarget;// Offset=0x20f8 Size=0x4
        struct D3DSurface * m_pSaveZBuffer;// Offset=0x20fc Size=0x4
        struct D3DBaseTexture * m_pSaveTexture;// Offset=0x2100 Size=0x4
        struct _D3DVIEWPORT8 m_SaveViewport;// Offset=0x2104 Size=0x18
        struct _D3DTILE m_Tile[8];// Offset=0x211c Size=0xc0
        struct _D3DRECT m_ScissorsRects[8];// Offset=0x21dc Size=0x80
        unsigned long m_ScissorsCount;// Offset=0x225c Size=0x4
        int m_ScissorsExclusive;// Offset=0x2260 Size=0x4
        struct Nv206eControl * m_pControlDma;// Offset=0x2264 Size=0x4
        class D3D::CMiniport m_Miniport;// Offset=0x2268 Size=0x85c
        unsigned long m_SwapCount;// Offset=0x2ac4 Size=0x4
        unsigned long m_ColorContextDmaInstance;// Offset=0x2ac8 Size=0x4
        unsigned long m_ZetaContextDmaInstance;// Offset=0x2acc Size=0x4
        unsigned long m_CopyContextDmaInstance;// Offset=0x2ad0 Size=0x4
        unsigned char * m_pCachedContiguousMemoryBase;// Offset=0x2ad4 Size=0x4
        struct NvNotification * m_pMemCopyNotifiers;// Offset=0x2ad8 Size=0x4
        struct NvNotification * m_pKelvinNotifiers;// Offset=0x2adc Size=0x4
        long Init(struct _D3DPRESENT_PARAMETERS_ * );// Offset=0x0 Size=0x510
        void UnInit();// Offset=0x0 Size=0x199
        long InitializeFrameBuffers(struct _D3DPRESENT_PARAMETERS_ * );// Offset=0x0 Size=0x537
        void FreeFrameBuffers();// Offset=0x0 Size=0x119
        void SetStateUP();// Offset=0x0 Size=0x13e
        void SetStateVB(unsigned long );// Offset=0x0 Size=0x191
        unsigned long * StartPush(unsigned long );// Offset=0x0 Size=0xe
        unsigned long * StartPush();// Offset=0x0 Size=0xd
        void EndPush(unsigned long * );// Offset=0x0 Size=0x9
        void StartBracket();// Offset=0x0 Size=0x8
        void EndBracket();// Offset=0x0 Size=0x1b
        void KickOff();// Offset=0x0 Size=0xe4
        long InitializePushBuffer();// Offset=0x0 Size=0xad
        void UninitializePushBuffer();// Offset=0x0 Size=0x20
        void HwPut(unsigned long * );// Offset=0x0 Size=0x15
        unsigned long * HwGet();// Offset=0x0 Size=0xf
        unsigned long * GpuGet();// Offset=0x0 Size=0x2f
        unsigned long GpuTime();// Offset=0x0 Size=0x6
        unsigned long Age(unsigned long );// Offset=0x0 Size=0xa
        int IsTimePending(unsigned long );// Offset=0x0 Size=0x19
        void RecordResourceReadPush(struct D3DResource * );// Offset=0x0 Size=0xd
        void RecordSurfaceWritePush(struct D3DResource * );// Offset=0x0 Size=0x1d
    };
};

struct D3D::NV_PATCH_FRAC_GUARD_INFO// Size=0x1c (Id=1620)
{
    struct D3D::NV_PATCH_CURVE_INFO * guardU0;// Offset=0x0 Size=0x4
    struct D3D::NV_PATCH_CURVE_INFO * guardV0;// Offset=0x4 Size=0x4
    struct D3D::NV_PATCH_CURVE_INFO * guardUCenter;// Offset=0x8 Size=0x4
    struct D3D::NV_PATCH_CURVE_INFO * guardVCenter;// Offset=0xc Size=0x4
    float * uMid;// Offset=0x10 Size=0x4
    float * vMid;// Offset=0x14 Size=0x4
    float * center;// Offset=0x18 Size=0x4
};

struct D3D::FenceEncoding// Size=0x18 (Id=1621)
{
    unsigned long m_SemaphoreCommand;// Offset=0x0 Size=0x4
    unsigned long m_Time;// Offset=0x4 Size=0x4
    union // Size=0x18 (Id=0)
    {
        unsigned long m_SetColorClearCommand1;// Offset=0x8 Size=0x4
        unsigned long m_SetColorClearArgument1;// Offset=0xc Size=0x4
        unsigned long m_SetColorClearCommand2;// Offset=0x10 Size=0x4
        unsigned long m_SetColorClearArgument2;// Offset=0x14 Size=0x4
        unsigned long m_WaitForIdleCommand;// Offset=0x8 Size=0x4
        unsigned long m_WaitForIdleArgument;// Offset=0xc Size=0x4
        unsigned long m_NoOperationCommand;// Offset=0x10 Size=0x4
        unsigned long m_FenceCommand;// Offset=0x14 Size=0x4
    };
};

struct D3D::NV_PATCH_FRAC_QUAD_GUARD_INFO// Size=0xd10 (Id=1622)
{
    struct D3D::NV_PATCH_CURVE_INFO guardU00;// Offset=0x0 Size=0x110
    struct D3D::NV_PATCH_CURVE_INFO guardU01;// Offset=0x110 Size=0x110
    struct D3D::NV_PATCH_CURVE_INFO guardU10;// Offset=0x220 Size=0x110
    struct D3D::NV_PATCH_CURVE_INFO guardU11;// Offset=0x330 Size=0x110
    struct D3D::NV_PATCH_CURVE_INFO guardV00;// Offset=0x440 Size=0x110
    struct D3D::NV_PATCH_CURVE_INFO guardV01;// Offset=0x550 Size=0x110
    struct D3D::NV_PATCH_CURVE_INFO guardV10;// Offset=0x660 Size=0x110
    struct D3D::NV_PATCH_CURVE_INFO guardV11;// Offset=0x770 Size=0x110
    struct D3D::NV_PATCH_CURVE_INFO guardUCenter0;// Offset=0x880 Size=0x110
    struct D3D::NV_PATCH_CURVE_INFO guardUCenter1;// Offset=0x990 Size=0x110
    struct D3D::NV_PATCH_CURVE_INFO guardVCenter0;// Offset=0xaa0 Size=0x110
    struct D3D::NV_PATCH_CURVE_INFO guardVCenter1;// Offset=0xbb0 Size=0x110
    float u0Mid[4];// Offset=0xcc0 Size=0x10
    float v0Mid[4];// Offset=0xcd0 Size=0x10
    float u1Mid[4];// Offset=0xce0 Size=0x10
    float v1Mid[4];// Offset=0xcf0 Size=0x10
    float center[4];// Offset=0xd00 Size=0x10
};

struct D3D::NV_PATCH_INFO::__unnamed::__unnamed// Size=0x10 (Id=1623)
{
    float nu0;// Offset=0x0 Size=0x4
    float nu1;// Offset=0x4 Size=0x4
    float nv0;// Offset=0x8 Size=0x4
    float nv1;// Offset=0xc Size=0x4
};

struct D3D::NV_PATCH_INFO::__unnamed::__unnamed// Size=0xc (Id=1624)
{
    float n1;// Offset=0x0 Size=0x4
    float n2;// Offset=0x4 Size=0x4
    float n3;// Offset=0x8 Size=0x4
};

union D3D::NV_PATCH_INFO::__unnamed// Size=0x10 (Id=1625)
{
    union // Size=0x10 (Id=0)
    {
        struct D3D::NV_PATCH_INFO::__unnamed::__unnamed tensor;// Offset=0x0 Size=0x10
        struct D3D::NV_PATCH_INFO::__unnamed::__unnamed tri;// Offset=0x0 Size=0xc
    };
};

struct D3D::NV_PATCH_INFO// Size=0x2b74 (Id=1626)
{
    unsigned int evalEnables;// Offset=0x0 Size=0x4
    unsigned int swEnables;// Offset=0x4 Size=0x4
    int nAttr;// Offset=0x8 Size=0x4
    int maxAttr;// Offset=0xc Size=0x4
    int firstAttr;// Offset=0x10 Size=0x4
    unsigned int maxOrder;// Offset=0x14 Size=0x4
    int maxSwatch;// Offset=0x18 Size=0x4
    int nSwatchU;// Offset=0x1c Size=0x4
    int nSwatchV;// Offset=0x20 Size=0x4
    int fracSwatchU;// Offset=0x24 Size=0x4
    int fracSwatchV;// Offset=0x28 Size=0x4
    int swatchFlags;// Offset=0x2c Size=0x4
    unsigned int flags;// Offset=0x30 Size=0x4
    int flipT;// Offset=0x34 Size=0x4
    int flipU;// Offset=0x38 Size=0x4
    int flipV;// Offset=0x3c Size=0x4
    int flipUV;// Offset=0x40 Size=0x4
    int flipTUV;// Offset=0x44 Size=0x4
    unsigned char * cachedPB;// Offset=0x48 Size=0x4
    unsigned long cachedPBSize;// Offset=0x4c Size=0x4
    unsigned long cachedPBCounter;// Offset=0x50 Size=0x4
    int reverse;// Offset=0x54 Size=0x4
    union D3D::NV_PATCH_INFO::__unnamed tess;// Offset=0x58 Size=0x10
    union D3D::NV_PATCH_INFO::__unnamed originaltess;// Offset=0x68 Size=0x10
    enum D3D::NV_PATCH_BASIS_TYPE basis;// Offset=0x78 Size=0x4
    int srcNormal;// Offset=0x7c Size=0x4
    int dstNormal;// Offset=0x80 Size=0x4
    int rational;// Offset=0x84 Size=0x4
    float startu;// Offset=0x88 Size=0x4
    float endu;// Offset=0x8c Size=0x4
    float startv;// Offset=0x90 Size=0x4
    float endv;// Offset=0x94 Size=0x4
    int srcUV[8];// Offset=0x98 Size=0x20
    int dstUV[8];// Offset=0xb8 Size=0x20
    struct D3D::NV_PATCH_MAP_INFO maps[16];// Offset=0xd8 Size=0x280
    enum D3D::NV_PATCH_BACKEND_TYPE backendType;// Offset=0x358 Size=0x4
    unsigned char * buffer;// Offset=0x35c Size=0x4
    unsigned int bufferLength;// Offset=0x360 Size=0x4
    int vertexSize;// Offset=0x364 Size=0x4
    void * context;// Offset=0x368 Size=0x4
    struct D3D::NV_PATCH_ALLOC_CACHE * pCache[10];// Offset=0x36c Size=0x28
    unsigned int retVal;// Offset=0x394 Size=0x4
    float * normalPatch;// Offset=0x398 Size=0x4
    float * UVPatch;// Offset=0x39c Size=0x4
    float * pSwatchCorner[16][2][2];// Offset=0x3a0 Size=0x100
    float gridCorner[16][2][2][4];// Offset=0x4a0 Size=0x400
    int setGridCorner;// Offset=0x8a0 Size=0x4
    struct D3D::NV_PATCH_CURVE_INFO tempCurve;// Offset=0x8a4 Size=0x110
    struct D3D::FDMatrix tempMatrix;// Offset=0x9b4 Size=0x1010
    struct D3D::FDMatrix reparam;// Offset=0x19c4 Size=0x1010
    struct D3D::FDMatrix * reduceTri[16];// Offset=0x29d4 Size=0x40
    int bytesGuardCurve;// Offset=0x2a14 Size=0x4
    int bytesGuardCurveAllAttr;// Offset=0x2a18 Size=0x4
    struct D3D::NV_PATCH_QUAD_INFO * quadInfo;// Offset=0x2a1c Size=0x4
    void * pScratchBase;// Offset=0x2a20 Size=0x4
    struct D3D::FDMatrix * ppMatrixSetSS0[16];// Offset=0x2a24 Size=0x40
    struct D3D::NV_PATCH_CURVE_INFO * tempVBegin[16];// Offset=0x2a64 Size=0x40
    struct D3D::NV_PATCH_CURVE_INFO * tempVEnd[16];// Offset=0x2aa4 Size=0x40
    struct D3D::NV_PATCH_CURVE_INFO * guardSetUInnerFrac[16];// Offset=0x2ae4 Size=0x40
    struct D3D::NV_PATCH_CURVE_INFO * guardSetVInnerFrac[16];// Offset=0x2b24 Size=0x40
    struct D3D::NV_PATCH_CURVE_INFO ** ppGuardSetUInnerFrac1[1][16];// Offset=0x2b64 Size=0x4
    struct D3D::NV_PATCH_CURVE_INFO ** ppGuardSetVInnerFrac1[1][16];// Offset=0x2b68 Size=0x4
    struct D3D::NV_PATCH_CURVE_INFO *** ppGuardSetUInnerFrac[16];// Offset=0x2b6c Size=0x4
    struct D3D::NV_PATCH_CURVE_INFO *** ppGuardSetVInnerFrac[16];// Offset=0x2b70 Size=0x4
};

struct D3D::NV_PATCH_CURVE_INFO// Size=0x110 (Id=1627)
{
    int order;// Offset=0x0 Size=0x4
    int pad[3];// Offset=0x4 Size=0xc
    float coeffs[16][4];// Offset=0x10 Size=0x100
};

struct D3D::NV_PATCH_QUAD_INFO// Size=0x898 (Id=1628)
{
    struct D3D::NV_PATCH_CURVE_INFO * pU0[16];// Offset=0x0 Size=0x40
    struct D3D::NV_PATCH_CURVE_INFO * pU1[16];// Offset=0x40 Size=0x40
    struct D3D::NV_PATCH_CURVE_INFO * pV0[16];// Offset=0x80 Size=0x40
    struct D3D::NV_PATCH_CURVE_INFO * pV1[16];// Offset=0xc0 Size=0x40
    struct D3D::NV_PATCH_CURVE_INFO * pUInner[16];// Offset=0x100 Size=0x40
    struct D3D::NV_PATCH_CURVE_INFO * pVInner[16];// Offset=0x140 Size=0x40
    struct D3D::NV_PATCH_CORNER_INFO cornAttr[16];// Offset=0x180 Size=0x400
    float * pCorners[16][2][2];// Offset=0x580 Size=0x100
    int nu0;// Offset=0x680 Size=0x4
    int nu1;// Offset=0x684 Size=0x4
    int nv0;// Offset=0x688 Size=0x4
    int nv1;// Offset=0x68c Size=0x4
    int uMaxSegs;// Offset=0x690 Size=0x4
    int vMaxSegs;// Offset=0x694 Size=0x4
    int uMinSegs;// Offset=0x698 Size=0x4
    int vMinSegs;// Offset=0x69c Size=0x4
    int needUInner;// Offset=0x6a0 Size=0x4
    int needVInner;// Offset=0x6a4 Size=0x4
    int stitchTop;// Offset=0x6a8 Size=0x4
    int stitchRight;// Offset=0x6ac Size=0x4
    int stitchBottom;// Offset=0x6b0 Size=0x4
    int stitchLeft;// Offset=0x6b4 Size=0x4
    int stitchUBegin;// Offset=0x6b8 Size=0x4
    int stitchUEnd;// Offset=0x6bc Size=0x4
    int stitchVBegin;// Offset=0x6c0 Size=0x4
    int stitchVEnd;// Offset=0x6c4 Size=0x4
    int u0Dir;// Offset=0x6c8 Size=0x4
    int v0Dir;// Offset=0x6cc Size=0x4
    int u1Dir;// Offset=0x6d0 Size=0x4
    int v1Dir;// Offset=0x6d4 Size=0x4
    int uMaxDir;// Offset=0x6d8 Size=0x4
    int vMaxDir;// Offset=0x6dc Size=0x4
    float du0;// Offset=0x6e0 Size=0x4
    float du1;// Offset=0x6e4 Size=0x4
    float dv0;// Offset=0x6e8 Size=0x4
    float dv1;// Offset=0x6ec Size=0x4
    float duMax;// Offset=0x6f0 Size=0x4
    float dvMax;// Offset=0x6f4 Size=0x4
    struct D3D::NV_PATCH_CURVE_INFO ** pSwatchUBegin[16];// Offset=0x6f8 Size=0x4
    struct D3D::NV_PATCH_CURVE_INFO ** pSwatchUEnd[16];// Offset=0x6fc Size=0x4
    struct D3D::NV_PATCH_CURVE_INFO ** pSwatchVBegin[16];// Offset=0x700 Size=0x4
    struct D3D::NV_PATCH_CURVE_INFO ** pSwatchVEnd[16];// Offset=0x704 Size=0x4
    struct D3D::NV_PATCH_CURVE_INFO ** pCurvesTop[16];// Offset=0x708 Size=0x4
    struct D3D::NV_PATCH_CURVE_INFO ** pCurvesBot[16];// Offset=0x70c Size=0x4
    struct D3D::NV_PATCH_EVAL_OUTPUT * pOut1;// Offset=0x710 Size=0x4
    struct D3D::NV_PATCH_EVAL_OUTPUT * pOut2;// Offset=0x714 Size=0x4
    struct D3D::FDMatrix * m00[16];// Offset=0x718 Size=0x40
    struct D3D::FDMatrix * m01[16];// Offset=0x758 Size=0x40
    struct D3D::FDMatrix * m10[16];// Offset=0x798 Size=0x40
    struct D3D::FDMatrix * m11[16];// Offset=0x7d8 Size=0x40
    struct D3D::NV_PATCH_FRAC_QUAD_GUARD_INFO * guardQF[16];// Offset=0x818 Size=0x40
    struct D3D::NV_PATCH_FRAC_TRI_GUARD_INFO * guardTF[16];// Offset=0x858 Size=0x40
};

class D3D::CMiniport// Size=0x85c (Id=1629)
{
    union // Size=0x9df (Id=0)
    {
        struct GENERALINFO// Size=0x1c (Id=18483)
        {
            unsigned long ChipId;// Offset=0x0 Size=0x4
            unsigned long VideoRamSize;// Offset=0x4 Size=0x4
            unsigned long VideoRamType;// Offset=0x8 Size=0x4
            unsigned long ChipIntrEn0;// Offset=0xc Size=0x4
            unsigned long MpVIPSlavePresent;// Offset=0x10 Size=0x4
            unsigned long CrystalFreq;// Offset=0x14 Size=0x4
            unsigned long MaskRevision;// Offset=0x18 Size=0x4
        };
        struct DACINFO// Size=0x3c (Id=18491)
        {
            unsigned long MClk;// Offset=0x0 Size=0x4
            unsigned long VClk;// Offset=0x4 Size=0x4
            unsigned long NVClk;// Offset=0x8 Size=0x4
            unsigned long MPllM;// Offset=0xc Size=0x4
            unsigned long MPllN;// Offset=0x10 Size=0x4
            unsigned long MPllO;// Offset=0x14 Size=0x4
            unsigned long MPllP;// Offset=0x18 Size=0x4
            unsigned long VPllM;// Offset=0x1c Size=0x4
            unsigned long VPllN;// Offset=0x20 Size=0x4
            unsigned long VPllO;// Offset=0x24 Size=0x4
            unsigned long VPllP;// Offset=0x28 Size=0x4
            unsigned long NVPllM;// Offset=0x2c Size=0x4
            unsigned long NVPllN;// Offset=0x30 Size=0x4
            unsigned long NVPllO;// Offset=0x34 Size=0x4
            unsigned long NVPllP;// Offset=0x38 Size=0x4
        };
        struct HALINFO// Size=0x5c (Id=18507)
        {
            long FifoChID;// Offset=0x0 Size=0x4
            unsigned long FifoMode;// Offset=0x4 Size=0x4
            int FifoInUse;// Offset=0x8 Size=0x4
            unsigned long FifoInstance;// Offset=0xc Size=0x4
            unsigned long FifoAllocCount;// Offset=0x10 Size=0x4
            long FifoCacheDepth;// Offset=0x14 Size=0x4
            unsigned long FifoObjectCount;// Offset=0x18 Size=0x4
            unsigned long FifoIntrEn0;// Offset=0x1c Size=0x4
            unsigned long FifoRetryCount;// Offset=0x20 Size=0x4
            unsigned long FifoUserBase;// Offset=0x24 Size=0x4
            unsigned long FifoContextAddr1;// Offset=0x28 Size=0x4
            unsigned long FifoContextAddr2;// Offset=0x2c Size=0x4
            unsigned long HashTableAddr;// Offset=0x30 Size=0x4
            unsigned long GrChID;// Offset=0x34 Size=0x4
            unsigned long GrCtxTable[2];// Offset=0x38 Size=0x8
            unsigned long GrCtxTableBase;// Offset=0x40 Size=0x4
            unsigned long GrCurrentObjects3d[2];// Offset=0x44 Size=0x8
            unsigned long FbSave0;// Offset=0x4c Size=0x4
            unsigned long FbSave1;// Offset=0x50 Size=0x4
            unsigned long McSave;// Offset=0x54 Size=0x4
            unsigned long McSaveIntrEn0;// Offset=0x58 Size=0x4
        };
        struct VIDEOMODETIMING// Size=0x40 (Id=18529)
        {
            unsigned long HorizontalVisible;// Offset=0x0 Size=0x4
            unsigned long VerticalVisible;// Offset=0x4 Size=0x4
            unsigned long Refresh;// Offset=0x8 Size=0x4
            unsigned long HorizontalTotal;// Offset=0xc Size=0x4
            unsigned long HorizontalBlankStart;// Offset=0x10 Size=0x4
            unsigned long HorizontalRetraceStart;// Offset=0x14 Size=0x4
            unsigned long HorizontalRetraceEnd;// Offset=0x18 Size=0x4
            unsigned long HorizontalBlankEnd;// Offset=0x1c Size=0x4
            unsigned long VerticalTotal;// Offset=0x20 Size=0x4
            unsigned long VerticalBlankStart;// Offset=0x24 Size=0x4
            unsigned long VerticalRetraceStart;// Offset=0x28 Size=0x4
            unsigned long VerticalRetraceEnd;// Offset=0x2c Size=0x4
            unsigned long VerticalBlankEnd;// Offset=0x30 Size=0x4
            unsigned long PixelClock;// Offset=0x34 Size=0x4
            unsigned long HorizontalSyncPolarity;// Offset=0x38 Size=0x4
            unsigned long VerticalSyncPolarity;// Offset=0x3c Size=0x4
        };
        struct HW_HASHENTRY// Size=0x8 (Id=18546)
        {
            unsigned long ht_ObjectHandle;// Offset=0x0 Size=0x4
            unsigned long ht_Context;// Offset=0x4 Size=0x4
        };
        struct OBJECTINFO// Size=0x10 (Id=18549)
        {
            unsigned long Handle;// Offset=0x0 Size=0x4
            unsigned short SubChannel;// Offset=0x4 Size=0x2
            unsigned short Engine;// Offset=0x6 Size=0x2
            unsigned long ClassNum;// Offset=0x8 Size=0x4
            unsigned long Instance;// Offset=0xc Size=0x4
            void Init();// Offset=0x0 Size=0xe
        };
        struct VBLANKFLIPS// Size=0x8 (Id=18561)
        {
            int Pending;// Offset=0x0 Size=0x4
            unsigned long Offset;// Offset=0x4 Size=0x4
        };
        void * m_RegisterBase;// Offset=0x0 Size=0x4
        unsigned long m_SurfacePitch;// Offset=0x4 Size=0x4
        unsigned long m_DisplayMode;// Offset=0x8 Size=0x4
        unsigned long m_Format;// Offset=0xc Size=0x4
        void * m_InstMem;// Offset=0x10 Size=0x4
        struct _KINTERRUPT m_InterruptObject;// Offset=0x14 Size=0x70
        struct _KDPC m_Dpc;// Offset=0x84 Size=0x1c
        int m_InterruptsEnabled;// Offset=0xa0 Size=0x4
        int m_UnhandledVBI;// Offset=0xa4 Size=0x4
        struct D3D::CMiniport::GENERALINFO m_GenInfo;// Offset=0xa8 Size=0x1c
        struct D3D::CMiniport::DACINFO m_DacInfo;// Offset=0xc4 Size=0x3c
        struct D3D::CMiniport::HALINFO m_HalInfo;// Offset=0x100 Size=0x5c
        struct D3D::CMiniport::VIDEOMODETIMING m_VideoModeTiming;// Offset=0x15c Size=0x40
        unsigned long m_VideoModeDepth;// Offset=0x19c Size=0x4
        unsigned long m_FreeInstAddr;// Offset=0x1a0 Size=0x4
        struct _HAL_SHUTDOWN_REGISTRATION m_ShutdownRegistration;// Offset=0x1a4 Size=0x10
        struct D3D::CMiniport::VBLANKFLIPS m_VBlankFlips[2];// Offset=0x1b4 Size=0x10
        void  ( * m_pSwapCallback)(struct _D3DSWAPDATA * );// Offset=0x1c4 Size=0x4
        void  ( * m_pVerticalBlankCallback)(struct _D3DVBLANKDATA * );// Offset=0x1c8 Size=0x4
        struct _KEVENT m_VerticalBlankEvent;// Offset=0x1cc Size=0x10
        struct _KEVENT m_BusyBlockEvent;// Offset=0x1dc Size=0x10
        unsigned long m_CurrentAvInfo;// Offset=0x1ec Size=0x4
        int m_FirstFlip;// Offset=0x1f0 Size=0x4
        unsigned long m_VBlankFlipCount;// Offset=0x1f4 Size=0x4
        unsigned long m_VBlankCount;// Offset=0x1f8 Size=0x4
        unsigned long m_VBlankCountAtLastFlip;// Offset=0x1fc Size=0x4
        unsigned long m_VBlanksBetweenFlips;// Offset=0x200 Size=0x4
        unsigned long m_FlipCount;// Offset=0x204 Size=0x4
        int m_OrImmediate;// Offset=0x208 Size=0x4
        unsigned long m_IsOddField;// Offset=0x20c Size=0x4
        unsigned long m_TimeBetweenVBlanks;// Offset=0x210 Size=0x4
        unsigned long m_TimeOfLastVBlank;// Offset=0x214 Size=0x4
        unsigned long m_TimeOfLastFlip;// Offset=0x218 Size=0x4
        struct _D3DGAMMARAMP m_GammaRamp[2];// Offset=0x21c Size=0x600
        int m_GammaUpdated[2];// Offset=0x81c Size=0x8
        unsigned long m_GammaCurrentIndex;// Offset=0x824 Size=0x4
        unsigned long m_OverlayVBlank;// Offset=0x828 Size=0x4
        unsigned long m_DebugRegister[11];// Offset=0x82c Size=0x2c
        struct PUSHBUFFERFIXUPINFO// Size=0x10 (Id=18605)
        {
            unsigned long * pFixupData;// Offset=0x0 Size=0x4
            unsigned char * pStart;// Offset=0x4 Size=0x4
            unsigned long ReturnOffset;// Offset=0x8 Size=0x4
            unsigned long * ReturnAddress;// Offset=0xc Size=0x4
        };
        unsigned char __align0[2120];// Offset=0x10 Size=0x848
        unsigned long m_PusherGetRunTotal;// Offset=0x858 Size=0x4
        int InitHardware();// Offset=0x0 Size=0x162
        unsigned long GetPresentFlagsFromAvInfo(unsigned long );// Offset=0x0 Size=0x40
        unsigned long GetDisplayCapabilities();// Offset=0x0 Size=0x1f
        void SetVideoMode(unsigned long ,unsigned long ,unsigned long ,unsigned long ,enum _D3DFORMAT ,unsigned long ,unsigned long );// Offset=0x0 Size=0x1d2
        int InitDMAChannel(unsigned long ,struct D3D::CMiniport::OBJECTINFO * ,struct D3D::CMiniport::OBJECTINFO * ,unsigned long ,void ** );// Offset=0x0 Size=0x63
        int BindToChannel(struct D3D::CMiniport::OBJECTINFO * );// Offset=0x0 Size=0x34
        int CreateGrObject(unsigned long ,unsigned long ,struct D3D::CMiniport::OBJECTINFO * );// Offset=0x0 Size=0x90
        int CreateCtxDmaObject(unsigned long ,unsigned long ,void * ,unsigned long ,struct D3D::CMiniport::OBJECTINFO * );// Offset=0x0 Size=0xcf
        unsigned long SetDmaRange(unsigned long ,struct D3DSurface * );// Offset=0x0 Size=0x5
        int CreateTile(unsigned long ,unsigned long ,unsigned long ,unsigned long ,unsigned long ,unsigned long ,unsigned long );// Offset=0x0 Size=0x1ee
        int DestroyTile(unsigned long ,unsigned long );// Offset=0x0 Size=0xc2
        void ShutdownEngines();// Offset=0x0 Size=0x1a6
        void DacProgramGammaRamp(struct _D3DGAMMARAMP * );// Offset=0x0 Size=0x41
        int IsFlipPending();// Offset=0x0 Size=0x11
        void DacProgramVideoStart(unsigned long );// Offset=0x0 Size=0x82
        void DisableInterrupts(void * );// Offset=0x0 Size=0x11
        void EnableInterrupts(void * );// Offset=0x0 Size=0x13
        unsigned long GetRefreshRate();// Offset=0x0 Size=0x16
        unsigned long GetTime();// Offset=0x0 Size=0x3
        unsigned char Isr(struct _KINTERRUPT * ,void * );// Offset=0x0 Size=0x11a
        void Dpc(struct _KDPC * ,void * ,void * ,void * );// Offset=0x0 Size=0xa5
        void ShutdownNotification(struct _HAL_SHUTDOWN_REGISTRATION * );// Offset=0x0 Size=0x3d
        void TilingUpdateIdle(unsigned long * );// Offset=0x0 Size=0x6b
        void FixupPushBuffer(struct D3D::CMiniport::PUSHBUFFERFIXUPINFO * ,unsigned long );// Offset=0x0 Size=0x79
        int InitEngines();// Offset=0x0 Size=0x10a
        int LoadEngines();// Offset=0x0 Size=0x139
        int MapRegisters();// Offset=0x0 Size=0x2b
        int GetGeneralInfo();// Offset=0x0 Size=0x41
        void InitGammaRamp(unsigned long );// Offset=0x0 Size=0x40
        void SoftwareMethod(unsigned long ,unsigned long );// Offset=0x0 Size=0x268
        void SetupPaletteAndGamma();
        unsigned long ReserveInstMem(unsigned long );// Offset=0x0 Size=0x13
        void GetAddressInfo(void * ,void ** ,unsigned long * ,int );// Offset=0x0 Size=0x5a
        unsigned long ServiceGrInterrupt();// Offset=0x0 Size=0xe8
        unsigned long VBlank();// Offset=0x0 Size=0x191
        void VBlankFlip(unsigned long ,unsigned long );// Offset=0x0 Size=0x75
        unsigned long ServiceFifoInterrupt();// Offset=0x0 Size=0x173
        unsigned long ServiceMediaPortInterrupt();// Offset=0x0 Size=0x3
        unsigned long ServiceVideoInterrupt();// Offset=0x0 Size=0x1a
        void HalMcControlInit();// Offset=0x0 Size=0x167
        void HalFbControlInit();// Offset=0x0 Size=0xfe
        void HalVideoControlInit();// Offset=0x0 Size=0x36
        void HalMpControlInit();// Offset=0x0 Size=0xe5
        void HalGrControlInit();// Offset=0x0 Size=0x30
        void HalGrControlLoad();// Offset=0x0 Size=0x293
        void HalGrIdle();// Offset=0x0 Size=0x3f
        void HalGrLoadChannelContext(unsigned long );// Offset=0x0 Size=0x123
        void HalGrUnloadChannelContext(unsigned long );// Offset=0x0 Size=0x39
        void HalGrInitObjectContext(unsigned long ,unsigned long );// Offset=0x0 Size=0x75
        void HalGrInit3d();// Offset=0x0 Size=0x9df
        void HalFifoControlInit();// Offset=0x0 Size=0x71
        void HalFifoControlLoad();// Offset=0x0 Size=0x81
        void HalFifoContextSwitch(unsigned long );// Offset=0x0 Size=0x1fd
        void HalFifoAllocDMA(unsigned long ,unsigned long ,unsigned long ,struct D3D::CMiniport::OBJECTINFO * );// Offset=0x0 Size=0x18d
        void HalFifoHashAdd(unsigned long ,unsigned long ,unsigned long ,unsigned long ,unsigned long );// Offset=0x0 Size=0x39
        void HalDacControlInit();// Offset=0x0 Size=0xcf
        void HalDacLoad();// Offset=0x0 Size=0x12
        void HalDacUnload();// Offset=0x0 Size=0x17
        void HalDacProgramMClk();// Offset=0x0 Size=0x14a
        void HalDacProgramNVClk();// Offset=0x0 Size=0x145
        void HalDacProgramPClk();// Offset=0x0 Size=0xaa
        void DumpClocks();// Offset=0x0 Size=0x1
        void GrDone();// Offset=0x0 Size=0xd
        void TmrDelay(unsigned long );// Offset=0x0 Size=0x17
        unsigned char UnlockCRTC();
        void RestoreCRTCLock(unsigned char );
        unsigned char ReadCRTCLock();
        int IsOddField();// Offset=0x0 Size=0x7
    };
};

struct D3D::CMiniport::PUSHBUFFERFIXUPINFO// Size=0x10 (Id=1630)
{
    unsigned long * pFixupData;// Offset=0x0 Size=0x4
    unsigned char * pStart;// Offset=0x4 Size=0x4
    unsigned long ReturnOffset;// Offset=0x8 Size=0x4
    unsigned long * ReturnAddress;// Offset=0xc Size=0x4
};

struct D3D::CMiniport::VBLANKFLIPS// Size=0x8 (Id=1631)
{
    int Pending;// Offset=0x0 Size=0x4
    unsigned long Offset;// Offset=0x4 Size=0x4
};

struct D3D::CMiniport::OBJECTINFO// Size=0x10 (Id=1632)
{
    union // Size=0x10 (Id=0)
    {
        unsigned long Handle;// Offset=0x0 Size=0x4
        unsigned short SubChannel;// Offset=0x4 Size=0x2
        unsigned short Engine;// Offset=0x6 Size=0x2
        unsigned long ClassNum;// Offset=0x8 Size=0x4
        unsigned long Instance;// Offset=0xc Size=0x4
        void Init();// Offset=0x0 Size=0xe
    };
};

struct D3D::CMiniport::HW_HASHENTRY// Size=0x8 (Id=1633)
{
    unsigned long ht_ObjectHandle;// Offset=0x0 Size=0x4
    unsigned long ht_Context;// Offset=0x4 Size=0x4
};

struct D3D::CMiniport::VIDEOMODETIMING// Size=0x40 (Id=1634)
{
    unsigned long HorizontalVisible;// Offset=0x0 Size=0x4
    unsigned long VerticalVisible;// Offset=0x4 Size=0x4
    unsigned long Refresh;// Offset=0x8 Size=0x4
    unsigned long HorizontalTotal;// Offset=0xc Size=0x4
    unsigned long HorizontalBlankStart;// Offset=0x10 Size=0x4
    unsigned long HorizontalRetraceStart;// Offset=0x14 Size=0x4
    unsigned long HorizontalRetraceEnd;// Offset=0x18 Size=0x4
    unsigned long HorizontalBlankEnd;// Offset=0x1c Size=0x4
    unsigned long VerticalTotal;// Offset=0x20 Size=0x4
    unsigned long VerticalBlankStart;// Offset=0x24 Size=0x4
    unsigned long VerticalRetraceStart;// Offset=0x28 Size=0x4
    unsigned long VerticalRetraceEnd;// Offset=0x2c Size=0x4
    unsigned long VerticalBlankEnd;// Offset=0x30 Size=0x4
    unsigned long PixelClock;// Offset=0x34 Size=0x4
    unsigned long HorizontalSyncPolarity;// Offset=0x38 Size=0x4
    unsigned long VerticalSyncPolarity;// Offset=0x3c Size=0x4
};

struct D3D::CMiniport::HALINFO// Size=0x5c (Id=1635)
{
    long FifoChID;// Offset=0x0 Size=0x4
    unsigned long FifoMode;// Offset=0x4 Size=0x4
    int FifoInUse;// Offset=0x8 Size=0x4
    unsigned long FifoInstance;// Offset=0xc Size=0x4
    unsigned long FifoAllocCount;// Offset=0x10 Size=0x4
    long FifoCacheDepth;// Offset=0x14 Size=0x4
    unsigned long FifoObjectCount;// Offset=0x18 Size=0x4
    unsigned long FifoIntrEn0;// Offset=0x1c Size=0x4
    unsigned long FifoRetryCount;// Offset=0x20 Size=0x4
    unsigned long FifoUserBase;// Offset=0x24 Size=0x4
    unsigned long FifoContextAddr1;// Offset=0x28 Size=0x4
    unsigned long FifoContextAddr2;// Offset=0x2c Size=0x4
    unsigned long HashTableAddr;// Offset=0x30 Size=0x4
    unsigned long GrChID;// Offset=0x34 Size=0x4
    unsigned long GrCtxTable[2];// Offset=0x38 Size=0x8
    unsigned long GrCtxTableBase;// Offset=0x40 Size=0x4
    unsigned long GrCurrentObjects3d[2];// Offset=0x44 Size=0x8
    unsigned long FbSave0;// Offset=0x4c Size=0x4
    unsigned long FbSave1;// Offset=0x50 Size=0x4
    unsigned long McSave;// Offset=0x54 Size=0x4
    unsigned long McSaveIntrEn0;// Offset=0x58 Size=0x4
};

struct D3D::CMiniport::DACINFO// Size=0x3c (Id=1636)
{
    unsigned long MClk;// Offset=0x0 Size=0x4
    unsigned long VClk;// Offset=0x4 Size=0x4
    unsigned long NVClk;// Offset=0x8 Size=0x4
    unsigned long MPllM;// Offset=0xc Size=0x4
    unsigned long MPllN;// Offset=0x10 Size=0x4
    unsigned long MPllO;// Offset=0x14 Size=0x4
    unsigned long MPllP;// Offset=0x18 Size=0x4
    unsigned long VPllM;// Offset=0x1c Size=0x4
    unsigned long VPllN;// Offset=0x20 Size=0x4
    unsigned long VPllO;// Offset=0x24 Size=0x4
    unsigned long VPllP;// Offset=0x28 Size=0x4
    unsigned long NVPllM;// Offset=0x2c Size=0x4
    unsigned long NVPllN;// Offset=0x30 Size=0x4
    unsigned long NVPllO;// Offset=0x34 Size=0x4
    unsigned long NVPllP;// Offset=0x38 Size=0x4
};

struct D3D::CMiniport::GENERALINFO// Size=0x1c (Id=1637)
{
    unsigned long ChipId;// Offset=0x0 Size=0x4
    unsigned long VideoRamSize;// Offset=0x4 Size=0x4
    unsigned long VideoRamType;// Offset=0x8 Size=0x4
    unsigned long ChipIntrEn0;// Offset=0xc Size=0x4
    unsigned long MpVIPSlavePresent;// Offset=0x10 Size=0x4
    unsigned long CrystalFreq;// Offset=0x14 Size=0x4
    unsigned long MaskRevision;// Offset=0x18 Size=0x4
};

struct D3D::VGATBL// Size=0x42 (Id=1638)
{
    union // Size=0x19 (Id=0)
    {
        struct SEQ// Size=0x4 (Id=19033)
        {
            unsigned char ClockMode;// Offset=0x0 Size=0x1
            unsigned char MapMask;// Offset=0x1 Size=0x1
            unsigned char FontAddr;// Offset=0x2 Size=0x1
            unsigned char MemMode;// Offset=0x3 Size=0x1
        };
        struct CRTC// Size=0x19 (Id=19038)
        {
            unsigned char HTotal;// Offset=0x0 Size=0x1
            unsigned char HDispEnd;// Offset=0x1 Size=0x1
            unsigned char HBlankS;// Offset=0x2 Size=0x1
            unsigned char HBlankE;// Offset=0x3 Size=0x1
            unsigned char HSyncS;// Offset=0x4 Size=0x1
            unsigned char HSyncE;// Offset=0x5 Size=0x1
            unsigned char VTotal;// Offset=0x6 Size=0x1
            unsigned char Overflow;// Offset=0x7 Size=0x1
            unsigned char PresetRowScan;// Offset=0x8 Size=0x1
            unsigned char CellHeight;// Offset=0x9 Size=0x1
            unsigned char CursorS;// Offset=0xa Size=0x1
            unsigned char CursorE;// Offset=0xb Size=0x1
            unsigned char RegenSHigh;// Offset=0xc Size=0x1
            unsigned char RegenSLow;// Offset=0xd Size=0x1
            unsigned char CursorPosHigh;// Offset=0xe Size=0x1
            unsigned char CursorPosLow;// Offset=0xf Size=0x1
            unsigned char VSyncS;// Offset=0x10 Size=0x1
            unsigned char VSyncE;// Offset=0x11 Size=0x1
            unsigned char VDispE;// Offset=0x12 Size=0x1
            unsigned char RowOffset;// Offset=0x13 Size=0x1
            unsigned char ULineRow;// Offset=0x14 Size=0x1
            unsigned char VBlankS;// Offset=0x15 Size=0x1
            unsigned char VBlandE;// Offset=0x16 Size=0x1
            unsigned char Mode;// Offset=0x17 Size=0x1
            unsigned char LineCompare;// Offset=0x18 Size=0x1
        };
        struct GR// Size=0x9 (Id=19064)
        {
            unsigned char SetReset;// Offset=0x0 Size=0x1
            unsigned char EnableSetReset;// Offset=0x1 Size=0x1
            unsigned char ColorCmp;// Offset=0x2 Size=0x1
            unsigned char ROP;// Offset=0x3 Size=0x1
            unsigned char ReadMap;// Offset=0x4 Size=0x1
            unsigned char Mode;// Offset=0x5 Size=0x1
            unsigned char Misc;// Offset=0x6 Size=0x1
            unsigned char CDC;// Offset=0x7 Size=0x1
            unsigned char BitMask;// Offset=0x8 Size=0x1
        };
        unsigned char columns;// Offset=0x0 Size=0x1
        unsigned char rows;// Offset=0x1 Size=0x1
        unsigned char char_height;// Offset=0x2 Size=0x1
        unsigned char __align0[1];// Offset=0x3 Size=0x1
        unsigned short RegenLenght;// Offset=0x4 Size=0x2
        struct D3D::VGATBL::SEQ seq_regs;// Offset=0x6 Size=0x4
        unsigned char PT_Misc;// Offset=0xa Size=0x1
        struct D3D::VGATBL::CRTC crtc_regs;// Offset=0xb Size=0x19
    };
    unsigned char PT_ATC[20];// Offset=0x24 Size=0x14
    struct D3D::VGATBL::GR gr_regs;// Offset=0x38 Size=0x9
};

struct D3D::VGATBL::GR// Size=0x9 (Id=1639)
{
    unsigned char SetReset;// Offset=0x0 Size=0x1
    unsigned char EnableSetReset;// Offset=0x1 Size=0x1
    unsigned char ColorCmp;// Offset=0x2 Size=0x1
    unsigned char ROP;// Offset=0x3 Size=0x1
    unsigned char ReadMap;// Offset=0x4 Size=0x1
    unsigned char Mode;// Offset=0x5 Size=0x1
    unsigned char Misc;// Offset=0x6 Size=0x1
    unsigned char CDC;// Offset=0x7 Size=0x1
    unsigned char BitMask;// Offset=0x8 Size=0x1
};

struct D3D::VGATBL::CRTC// Size=0x19 (Id=1640)
{
    unsigned char HTotal;// Offset=0x0 Size=0x1
    unsigned char HDispEnd;// Offset=0x1 Size=0x1
    unsigned char HBlankS;// Offset=0x2 Size=0x1
    unsigned char HBlankE;// Offset=0x3 Size=0x1
    unsigned char HSyncS;// Offset=0x4 Size=0x1
    unsigned char HSyncE;// Offset=0x5 Size=0x1
    unsigned char VTotal;// Offset=0x6 Size=0x1
    unsigned char Overflow;// Offset=0x7 Size=0x1
    unsigned char PresetRowScan;// Offset=0x8 Size=0x1
    unsigned char CellHeight;// Offset=0x9 Size=0x1
    unsigned char CursorS;// Offset=0xa Size=0x1
    unsigned char CursorE;// Offset=0xb Size=0x1
    unsigned char RegenSHigh;// Offset=0xc Size=0x1
    unsigned char RegenSLow;// Offset=0xd Size=0x1
    unsigned char CursorPosHigh;// Offset=0xe Size=0x1
    unsigned char CursorPosLow;// Offset=0xf Size=0x1
    unsigned char VSyncS;// Offset=0x10 Size=0x1
    unsigned char VSyncE;// Offset=0x11 Size=0x1
    unsigned char VDispE;// Offset=0x12 Size=0x1
    unsigned char RowOffset;// Offset=0x13 Size=0x1
    unsigned char ULineRow;// Offset=0x14 Size=0x1
    unsigned char VBlankS;// Offset=0x15 Size=0x1
    unsigned char VBlandE;// Offset=0x16 Size=0x1
    unsigned char Mode;// Offset=0x17 Size=0x1
    unsigned char LineCompare;// Offset=0x18 Size=0x1
};

struct D3D::VGATBL::SEQ// Size=0x4 (Id=1641)
{
    unsigned char ClockMode;// Offset=0x0 Size=0x1
    unsigned char MapMask;// Offset=0x1 Size=0x1
    unsigned char FontAddr;// Offset=0x2 Size=0x1
    unsigned char MemMode;// Offset=0x3 Size=0x1
};

class D3D::CEnum : public Direct3D// Size=0x1 (Id=1642)
{
    public enum _D3DFORMAT MapUnknownFormat(unsigned int ,unsigned long ,enum _D3DFORMAT ,enum _D3DDEVTYPE ,enum _D3DFORMAT );
};

class D3D::CEnum : public Direct3D// Size=0x1 (Id=1643)
{
    public enum _D3DFORMAT MapUnknownFormat(unsigned int ,unsigned long ,enum _D3DFORMAT ,enum _D3DDEVTYPE ,enum _D3DFORMAT );
};

class D3D::CPatch// Size=0x2bf0 (Id=1644)
{
    private unsigned long m_dwFlags;// Offset=0x0 Size=0x4
    private unsigned long m_dwHandle;// Offset=0x4 Size=0x4
    private unsigned long m_subpatchcount;// Offset=0x8 Size=0x4
    private unsigned long m_dwHeight;// Offset=0xc Size=0x4
    private unsigned long m_dwWidth;// Offset=0x10 Size=0x4
    private unsigned long m_dwOrder;// Offset=0x14 Size=0x4
    private unsigned long m_dwStride;// Offset=0x18 Size=0x4
    private unsigned long m_dwVertexOffset;// Offset=0x1c Size=0x4
    private enum D3D::NV_PATCH_VERTEX_FORMAT_TYPE m_dwTypes[16];// Offset=0x20 Size=0x40
    private struct D3D::FDMatrix ** m00;// Offset=0x60 Size=0x4
    private struct D3D::FDMatrix ** m10;// Offset=0x64 Size=0x4
    private struct D3D::FDMatrix ** m01;// Offset=0x68 Size=0x4
    private struct D3D::FDMatrix ** m11;// Offset=0x6c Size=0x4
    private struct D3D::NV_PATCH_FRAC_QUAD_GUARD_INFO ** guardQF;// Offset=0x70 Size=0x4
    private struct D3D::NV_PATCH_QUAD_INFO ** cachedQuadInfo;// Offset=0x74 Size=0x4
    private struct D3D::NV_PATCH_FRAC_TRI_GUARD_INFO ** guardTF;// Offset=0x78 Size=0x4
    private struct D3D::NV_PATCH_INFO m_PatchData;// Offset=0x7c Size=0x2b74
    private void setStream(unsigned int ,void * ,unsigned long ,unsigned long ,enum D3D::NV_PATCH_VERTEX_FORMAT_TYPE ,unsigned int );
    public int Init(unsigned long );
    public void UnInit();
    public void getCachedData(unsigned long );
    public void saveCachedData(unsigned long );
    public void freeCached();
    public int allocCached(unsigned long );
    public void setupStreams();
    public void copyDataToScratch(unsigned long ,unsigned long );
    public void copyDataToScratchTri(unsigned long ,unsigned long );
    public void convertBsplineToBezier();
    public void convertCatmullRomToBezier();
    public void setBasis(enum D3D::NV_PATCH_BASIS_TYPE );
    public unsigned int getBasis();
    public struct D3D::NV_PATCH_INFO * getInfo();
    public void setTessMode(unsigned int );
    public void setPrimMode(unsigned int );
    public void setTessellation(float * ,int );
    public void setOriginalTessellation(float * ,int );
    public void prepareBuffer(long );
    public void setVertexOffset(unsigned long );
    public void setAutoNormal(unsigned int ,unsigned int );
    public void setAutoUV(unsigned int ,unsigned int ,int );
    public void setDimension(unsigned int ,unsigned int );
    public unsigned int getWidth();
    public unsigned int getHeight();
    public void setOrder(unsigned long );
    public unsigned long getOrder();
    public void setStride(unsigned long );
    public unsigned long getStride();
    public void setTextureOffsets(float ,float ,float ,float );
    public float getSeg(unsigned int );
    public void setCalc(unsigned int );
    public unsigned int getCalc();
    public void setVertexStride(int );
};

struct D3D::VertexShader// Size=0x118 (Id=1645)
{
    unsigned long RefCount;// Offset=0x0 Size=0x4
    unsigned long Flags;// Offset=0x4 Size=0x4
    unsigned long ProgramSize;// Offset=0x8 Size=0x4
    unsigned long ProgramAndConstantsDwords;// Offset=0xc Size=0x4
    unsigned long Dimensionality;// Offset=0x10 Size=0x4
    struct D3D::VertexShaderSlot Slot[16];// Offset=0x14 Size=0x100
    unsigned long ProgramAndConstants[1];// Offset=0x114 Size=0x4
};

struct D3D::NV_PATCH_EVAL_OUTPUT// Size=0x2b00 (Id=1646)
{
    float vertexAttribs[43][16][4];// Offset=0x0 Size=0x2b00
};

struct D3D::NV_PATCH_STATIC_DATA// Size=0xe4 (Id=1647)
{
    unsigned int RetessFDtag[2][2];// Offset=0x0 Size=0x10
    struct D3D::FDMatrix * RetessFDCache[2];// Offset=0x10 Size=0x8
    int RetessFDMRU;// Offset=0x18 Size=0x4
    struct D3D::NV_PATCH_ALLOC_CACHE pCache[10];// Offset=0x1c Size=0xc8
};

struct D3D::Fence// Size=0xc (Id=1648)
{
    unsigned long Time;// Offset=0x0 Size=0x4
    struct D3D::FenceEncoding * pEncoding;// Offset=0x4 Size=0x4
    unsigned long RunTotal;// Offset=0x8 Size=0x4
};

struct D3D::Stream// Size=0xc (Id=1649)
{
    unsigned long Stride;// Offset=0x0 Size=0x4
    unsigned long Offset;// Offset=0x4 Size=0x4
    struct D3DVertexBuffer * pVertexBuffer;// Offset=0x8 Size=0x4
};

struct D3D::Light// Size=0x90 (Id=1650)
{
    struct _D3DLIGHT8 Light8;// Offset=0x0 Size=0x68
    unsigned long Flags;// Offset=0x68 Size=0x4
    struct _D3DVECTOR Direction;// Offset=0x6c Size=0xc
    float Falloff_L;// Offset=0x78 Size=0x4
    float Falloff_M;// Offset=0x7c Size=0x4
    float Falloff_N;// Offset=0x80 Size=0x4
    float Scale;// Offset=0x84 Size=0x4
    float W;// Offset=0x88 Size=0x4
    struct D3D::Light * pNext;// Offset=0x8c Size=0x4
};

struct D3D::FDMatrix// Size=0x1010 (Id=1651)
{
    int rows;// Offset=0x0 Size=0x4
    int columns;// Offset=0x4 Size=0x4
    int pad[2];// Offset=0x8 Size=0x8
    float data[16][16][4];// Offset=0x10 Size=0x1000
};

struct D3D::PAGE_ZERO// Size=0x4 (Id=1652)
{
    unsigned long m_PushBufferJump;// Offset=0x0 Size=0x4
};

struct D3D::NV_PATCH_ALLOC_CACHE// Size=0x14 (Id=1653)
{
    void * pBaseCache;// Offset=0x0 Size=0x4
    int bytesAllocCache;// Offset=0x4 Size=0x4
    int countFreeCheck;// Offset=0x8 Size=0x4
    int inUse;// Offset=0xc Size=0x4
    void * contextCache;// Offset=0x10 Size=0x4
};

struct D3D::NV_PATCH_ALLOC_CONTROL// Size=0x1c (Id=1654)
{
    void * context;// Offset=0x0 Size=0x4
    void * pBase;// Offset=0x4 Size=0x4
    void * pAvail;// Offset=0x8 Size=0x4
    void * pAllocatedBase;// Offset=0xc Size=0x4
    int bytesAlloc;// Offset=0x10 Size=0x4
    int bytesUsed;// Offset=0x14 Size=0x4
    struct D3D::NV_PATCH_ALLOC_CACHE * pCache;// Offset=0x18 Size=0x4
};

struct D3D::NV_PATCH_FRAC_TRI_GUARD_INFO// Size=0x9d0 (Id=1655)
{
    struct D3D::NV_PATCH_CURVE_INFO guard[3][2];// Offset=0x0 Size=0x660
    struct D3D::NV_PATCH_CURVE_INFO guardCenter[3];// Offset=0x660 Size=0x330
    float mid[3][4];// Offset=0x990 Size=0x30
    float center[4];// Offset=0x9c0 Size=0x10
};

struct D3D::NV_PATCH_CORNER_INFO// Size=0x40 (Id=1656)
{
    float corners[2][2][4];// Offset=0x0 Size=0x40
};

struct D3D::NV_PATCH_MAP_INFO// Size=0x28 (Id=1657)
{
    unsigned int uorder;// Offset=0x0 Size=0x4
    unsigned int vorder;// Offset=0x4 Size=0x4
    float * rawData;// Offset=0x8 Size=0x4
    int stride;// Offset=0xc Size=0x4
    int pitch;// Offset=0x10 Size=0x4
    int ufStride;// Offset=0x14 Size=0x4
    int vfStride;// Offset=0x18 Size=0x4
    enum D3D::NV_PATCH_VERTEX_FORMAT_TYPE maptype;// Offset=0x1c Size=0x4
    enum D3D::NV_PATCH_VERTEX_FORMAT_TYPE Originaltype;// Offset=0x20 Size=0x4
    unsigned int dirtyFlags;// Offset=0x24 Size=0x4
};

struct D3D::DISPLAYMODE// Size=0xc (Id=1658)
{
    unsigned long AvInfo;// Offset=0x0 Size=0x4
    unsigned short Width;// Offset=0x4 Size=0x2
    unsigned short Height;// Offset=0x6 Size=0x2
    unsigned long DisplayMode;// Offset=0x8 Size=0x4
};

struct D3D::VertexShaderSlot// Size=0x10 (Id=1659)
{
    unsigned long StreamIndex;// Offset=0x0 Size=0x4
    unsigned long Offset;// Offset=0x4 Size=0x4
    unsigned long SizeAndType;// Offset=0x8 Size=0x4
    unsigned char Flags;// Offset=0xc Size=0x1
    unsigned char Source;// Offset=0xd Size=0x1
};

enum _D3DPERF_APICounters
{
    API_DIRECT3D_CHECKDEPTHSTENCILMATCH=0,
    API_DIRECT3D_CHECKDEVICEFORMAT=1,
    API_DIRECT3D_CHECKDEVICEMULTISAMPLETYPE=2,
    API_DIRECT3D_CHECKDEVICETYPE=3,
    API_DIRECT3D_CREATEDEVICE=4,
    API_DIRECT3D_ENUMADAPTERMODES=5,
    API_DIRECT3D_GETADAPTERDISPLAYMODE=6,
    API_DIRECT3D_GETADAPTERIDENTIFIER=7,
    API_DIRECT3D_GETADAPTERMODECOUNT=8,
    API_DIRECT3D_GETDEVICECAPS=9,
    API_DIRECT3D_SETPUSHBUFFERSIZE=10,
    API_D3DDEVICE_APPLYSTATEBLOCK=11,
    API_D3DDEVICE_BEGIN=12,
    API_D3DDEVICE_BEGINPUSH=13,
    API_D3DDEVICE_BEGINPUSHBUFFER=14,
    API_D3DDEVICE_BEGINSTATEBLOCK=15,
    API_D3DDEVICE_BEGINVISIBILITYTEST=16,
    API_D3DDEVICE_BLOCKONFENCE=17,
    API_D3DDEVICE_BLOCKUNTILIDLE=18,
    API_D3DDEVICE_BLOCKUNTILVERTICALBLANK=19,
    API_D3DDEVICE_CAPTURESTATEBLOCK=20,
    API_D3DDEVICE_CLEAR=21,
    API_D3DDEVICE_COPYRECTS=22,
    API_D3DDEVICE_CREATECUBETEXTURE=23,
    API_D3DDEVICE_CREATEDEPTHSTENCILSURFACE=24,
    API_D3DDEVICE_CREATEFIXUP=25,
    API_D3DDEVICE_CREATEIMAGESURFACE=26,
    API_D3DDEVICE_CREATEINDEXBUFFER=27,
    API_D3DDEVICE_CREATEPALETTE=28,
    API_D3DDEVICE_CREATEPIXELSHADER=29,
    API_D3DDEVICE_CREATEPUSHBUFFER=30,
    API_D3DDEVICE_CREATERENDERTARGET=31,
    API_D3DDEVICE_CREATESTATEBLOCK=32,
    API_D3DDEVICE_CREATETEXTURE=33,
    API_D3DDEVICE_CREATEVERTEXBUFFER=34,
    API_D3DDEVICE_CREATEVERTEXSHADER=35,
    API_D3DDEVICE_CREATEVOLUMETEXTURE=36,
    API_D3DDEVICE_DELETEPATCH=37,
    API_D3DDEVICE_DELETEPIXELSHADER=38,
    API_D3DDEVICE_DELETESTATEBLOCK=39,
    API_D3DDEVICE_DELETEVERTEXSHADER=40,
    API_D3DDEVICE_DRAWINDEXEDVERTICES=41,
    API_D3DDEVICE_DRAWINDEXEDVERTICESUP=42,
    API_D3DDEVICE_DRAWRECTPATCH=43,
    API_D3DDEVICE_DRAWTRIPATCH=44,
    API_D3DDEVICE_DRAWVERTICES=45,
    API_D3DDEVICE_DRAWVERTICESUP=46,
    API_D3DDEVICE_ENABLECC=47,
    API_D3DDEVICE_ENABLEOVERLAY=48,
    API_D3DDEVICE_END=49,
    API_D3DDEVICE_ENDPUSH=50,
    API_D3DDEVICE_ENDPUSHBUFFER=51,
    API_D3DDEVICE_ENDSTATEBLOCK=52,
    API_D3DDEVICE_ENDVISIBILITYTEST=53,
    API_D3DDEVICE_FLUSHVERTEXCACHE=54,
    API_D3DDEVICE_GETBACKBUFFER=55,
    API_D3DDEVICE_GETBACKBUFFERSCALE=56,
    API_D3DDEVICE_GETBACKMATERIAL=57,
    API_D3DDEVICE_GETCCSTATUS=58,
    API_D3DDEVICE_GETCREATIONPARAMETERS=59,
    API_D3DDEVICE_GETDEPTHSTENCILSURFACE=60,
    API_D3DDEVICE_GETDEVICECAPS=61,
    API_D3DDEVICE_GETDIRECT3D=62,
    API_D3DDEVICE_GETDISPLAYFIELDSTATUS=63,
    API_D3DDEVICE_GETDISPLAYMODE=64,
    API_D3DDEVICE_GETGAMMARAMP=65,
    API_D3DDEVICE_GETINDICES=66,
    API_D3DDEVICE_GETLIGHT=67,
    API_D3DDEVICE_GETLIGHTENABLE=68,
    API_D3DDEVICE_GETMATERIAL=69,
    API_D3DDEVICE_GETMODELVIEW=70,
    API_D3DDEVICE_GETOVERLAYUPDATESTATUS=71,
    API_D3DDEVICE_GETPALETTE=72,
    API_D3DDEVICE_GETPIXELSHADER=73,
    API_D3DDEVICE_GETPIXELSHADERCONSTANT=74,
    API_D3DDEVICE_GETPIXELSHADERFUNCTION=75,
    API_D3DDEVICE_GETPROJECTIONVIEWPORTMATRIX=76,
    API_D3DDEVICE_GETPUSHBUFFEROFFSET=77,
    API_D3DDEVICE_GETRASTERSTATUS=78,
    API_D3DDEVICE_GETRENDERSTATE=79,
    API_D3DDEVICE_GETRENDERTARGET=80,
    API_D3DDEVICE_GETSCISSORS=81,
    API_D3DDEVICE_GETSCREENSPACEOFFSET=82,
    API_D3DDEVICE_GETSHADERCONSTANTMODE=83,
    API_D3DDEVICE_GETSTREAMSOURCE=84,
    API_D3DDEVICE_GETTEXTURE=85,
    API_D3DDEVICE_GETTEXTURESTAGESTATE=86,
    API_D3DDEVICE_GETTILE=87,
    API_D3DDEVICE_GETTILECOMPRESSIONTAGS=88,
    API_D3DDEVICE_GETTRANSFORM=89,
    API_D3DDEVICE_GETVERTEXBLENDMODELVIEW=90,
    API_D3DDEVICE_GETVERTEXSHADER=91,
    API_D3DDEVICE_GETVERTEXSHADERCONSTANT=92,
    API_D3DDEVICE_GETVERTEXSHADERDECLARATION=93,
    API_D3DDEVICE_GETVERTEXSHADERFUNCTION=94,
    API_D3DDEVICE_GETVERTEXSHADERINPUT=95,
    API_D3DDEVICE_GETVERTEXSHADERSIZE=96,
    API_D3DDEVICE_GETVERTEXSHADERTYPE=97,
    API_D3DDEVICE_GETVIEWPORT=98,
    API_D3DDEVICE_GETVISIBILITYTESTRESULT=99,
    API_D3DDEVICE_INSERTCALLBACK=100,
    API_D3DDEVICE_INSERTFENCE=101,
    API_D3DDEVICE_ISBUSY=102,
    API_D3DDEVICE_ISFENCEPENDING=103,
    API_D3DDEVICE_KICKPUSHBUFFER=104,
    API_D3DDEVICE_LIGHTENABLE=105,
    API_D3DDEVICE_LOADVERTEXSHADER=106,
    API_D3DDEVICE_LOADVERTEXSHADERPROGRAM=107,
    API_D3DDEVICE_MULTIPLYTRANSFORM=108,
    API_D3DDEVICE_NOP=109,
    API_D3DDEVICE_PERSISTDISPLAY=110,
    API_D3DDEVICE_PRESENT=111,
    API_D3DDEVICE_PRIMEVERTEXCACHE=112,
    API_D3DDEVICE_READVERTEXSHADERCONSTANT=113,
    API_D3DDEVICE_RESET=114,
    API_D3DDEVICE_RESUME=115,
    API_D3DDEVICE_RUNPUSHBUFFER=116,
    API_D3DDEVICE_RUNVERTEXSTATESHADER=117,
    API_D3DDEVICE_SELECTVERTEXSHADER=118,
    API_D3DDEVICE_SENDCC=119,
    API_D3DDEVICE_SETBACKBUFFERSCALE=120,
    API_D3DDEVICE_SETBACKMATERIAL=121,
    API_D3DDEVICE_SETGAMMARAMP=122,
    API_D3DDEVICE_SETINDICES=123,
    API_D3DDEVICE_SETLIGHT=124,
    API_D3DDEVICE_SETMATERIAL=125,
    API_D3DDEVICE_SETMODELVIEW=126,
    API_D3DDEVICE_SETPALETTE=127,
    API_D3DDEVICE_SETPIXELSHADER=128,
    API_D3DDEVICE_SETPIXELSHADERCONSTANT=129,
    API_D3DDEVICE_SETPIXELSHADERPROGRAM=130,
    API_D3DDEVICE_SETRENDERSTATE_BACKFILLMODE=131,
    API_D3DDEVICE_SETRENDERSTATE_CULLMODE=132,
    API_D3DDEVICE_SETRENDERSTATE_DONOTCULLUNCOMPRESSED=133,
    API_D3DDEVICE_SETRENDERSTATE_DXT1NOISEENABLE=134,
    API_D3DDEVICE_SETRENDERSTATE_EDGEANTIALIAS=135,
    API_D3DDEVICE_SETRENDERSTATE_FILLMODE=136,
    API_D3DDEVICE_SETRENDERSTATE_FOGCOLOR=137,
    API_D3DDEVICE_SETRENDERSTATE_FRONTFACE=138,
    API_D3DDEVICE_SETRENDERSTATE_LINEWIDTH=139,
    API_D3DDEVICE_SETRENDERSTATE_LOGICOP=140,
    API_D3DDEVICE_SETRENDERSTATE_MULTISAMPLEANTIALIAS=141,
    API_D3DDEVICE_SETRENDERSTATE_MULTISAMPLEMASK=142,
    API_D3DDEVICE_SETRENDERSTATE_MULTISAMPLEMODE=143,
    API_D3DDEVICE_SETRENDERSTATE_MULTISAMPLERENDERTARGETMODE=144,
    API_D3DDEVICE_SETRENDERSTATE_NORMALIZENORMALS=145,
    API_D3DDEVICE_SETRENDERSTATE_OCCLUSIONCULLENABLE=146,
    API_D3DDEVICE_SETRENDERSTATE_PARAMETERCHECK=147,
    API_D3DDEVICE_SETRENDERSTATE_PSTEXTUREMODES=148,
    API_D3DDEVICE_SETRENDERSTATE_ROPZCMPALWAYSREAD=149,
    API_D3DDEVICE_SETRENDERSTATE_ROPZREAD=150,
    API_D3DDEVICE_SETRENDERSTATE_SHADOWFUNC=151,
    API_D3DDEVICE_SETRENDERSTATE_SIMPLE=152,
    API_D3DDEVICE_SETRENDERSTATE_STENCILCULLENABLE=153,
    API_D3DDEVICE_SETRENDERSTATE_STENCILENABLE=154,
    API_D3DDEVICE_SETRENDERSTATE_STENCILFAIL=155,
    API_D3DDEVICE_SETRENDERSTATE_TEXTUREFACTOR=156,
    API_D3DDEVICE_SETRENDERSTATE_TWOSIDEDLIGHTING=157,
    API_D3DDEVICE_SETRENDERSTATE_VERTEXBLEND=158,
    API_D3DDEVICE_SETRENDERSTATE_YUVENABLE=159,
    API_D3DDEVICE_SETRENDERSTATE_ZBIAS=160,
    API_D3DDEVICE_SETRENDERSTATE_ZENABLE=161,
    API_D3DDEVICE_SETRENDERSTATENOTINLINE=162,
    API_D3DDEVICE_SETRENDERTARGET=163,
    API_D3DDEVICE_SETSCISSORS=164,
    API_D3DDEVICE_SETSCREENSPACEOFFSET=165,
    API_D3DDEVICE_SETSHADERCONSTANTMODE=166,
    API_D3DDEVICE_SETSTREAMSOURCE=167,
    API_D3DDEVICE_SETSWAPCALLBACK=168,
    API_D3DDEVICE_SETTEXTURE=169,
    API_D3DDEVICE_SETTEXTURESTAGESTATENOTINLINE=170,
    API_D3DDEVICE_SETTEXTURESTATE_BORDERCOLOR=171,
    API_D3DDEVICE_SETTEXTURESTATE_BUMPENV=172,
    API_D3DDEVICE_SETTEXTURESTATE_COLORKEYCOLOR=173,
    API_D3DDEVICE_SETTEXTURESTATE_PARAMETERCHECK=174,
    API_D3DDEVICE_SETTEXTURESTATE_TEXCOORDINDEX=175,
    API_D3DDEVICE_SETTILE=176,
    API_D3DDEVICE_SETTRANSFORM=177,
    API_D3DDEVICE_SETVERTEXBLENDMODELVIEW=178,
    API_D3DDEVICE_SETVERTEXDATA2F=179,
    API_D3DDEVICE_SETVERTEXDATA2S=180,
    API_D3DDEVICE_SETVERTEXDATA4F=181,
    API_D3DDEVICE_SETVERTEXDATA4S=182,
    API_D3DDEVICE_SETVERTEXDATA4UB=183,
    API_D3DDEVICE_SETVERTEXDATACOLOR=184,
    API_D3DDEVICE_SETVERTEXSHADER=185,
    API_D3DDEVICE_SETVERTEXSHADERCONSTANT=186,
    API_D3DDEVICE_SETVERTEXSHADERINPUT=187,
    API_D3DDEVICE_SETVERTICALBLANKCALLBACK=188,
    API_D3DDEVICE_SETVIEWPORT=189,
    API_D3DDEVICE_SUSPEND=190,
    API_D3DDEVICE_SWITCHTEXTURE=191,
    API_D3DDEVICE_UPDATEOVERLAY=192,
    API_D3DRESOURCE_ADDREF=193,
    API_D3DRESOURCE_BLOCKUNTILNOTBUSY=194,
    API_D3DRESOURCE_FREEPRIVATEDATA=195,
    API_D3DRESOURCE_GETDEVICE=196,
    API_D3DRESOURCE_GETPRIVATEDATA=197,
    API_D3DRESOURCE_GETTYPE=198,
    API_D3DRESOURCE_ISBUSY=199,
    API_D3DRESOURCE_REGISTER=200,
    API_D3DRESOURCE_RELEASE=201,
    API_D3DRESOURCE_SETPRIVATEDATA=202,
    API_D3DBASETEXTURE_GETLEVELCOUNT=203,
    API_D3DTEXTURE_GETLEVELDESC=204,
    API_D3DTEXTURE_GETSURFACELEVEL=205,
    API_D3DTEXTURE_LOCKRECT=206,
    API_D3DCUBETEXTURE_GETCUBEMAPSURFACE=207,
    API_D3DCUBETEXTURE_GETLEVELDESC=208,
    API_D3DCUBETEXTURE_LOCKRECT=209,
    API_D3DVOLUMETEXURE_GETLEVELDESC=210,
    API_D3DVOLUMETEXURE_GETVOLUMELEVEL=211,
    API_D3DVOLUMETEXURE_LOCKBOX=212,
    API_D3DVERTEXBUFFER_GETDESC=213,
    API_D3DVERTEXBUFFER_LOCK=214,
    API_D3DINDEXBUFFER_GETDESC=215,
    API_D3DVOLUME_GETCONTAINER=216,
    API_D3DVOLUME_GETDESC=217,
    API_D3DVOLUME_LOCKBOX=218,
    API_D3DSURFACE_GETCONTAINER=219,
    API_D3DSURFACE_GETDESC=220,
    API_D3DSURFACE_LOCKRECT=221,
    API_D3DPALETTE_GETSIZE=222,
    API_D3DPALETTE_LOCK=223,
    API_D3DPUSHBUFFER_BEGINFIXUP=224,
    API_D3DPUSHBUFFER_ENDFIXUP=225,
    API_D3DPUSHBUFFER_ENDVISIBILITYTEST=226,
    API_D3DPUSHBUFFER_JUMP=227,
    API_D3DPUSHBUFFER_RUNPUSHBUFFER=228,
    API_D3DPUSHBUFFER_SETMODELVIEW=229,
    API_D3DPUSHBUFFER_SETPALETTE=230,
    API_D3DPUSHBUFFER_SETRENDERTARGET=231,
    API_D3DPUSHBUFFER_SETTEXTURE=232,
    API_D3DPUSHBUFFER_SETVERTEXBLENDMODELVIEW=233,
    API_D3DPUSHBUFFER_SETVERTEXSHADERCONSTANT=234,
    API_D3DPUSHBUFFER_SETVERTEXSHADERINPUT=235,
    API_D3DPUSHBUFFER_VERIFY=236,
    API_D3DFIXUP_GETSIZE=237,
    API_D3DFIXUP_GETSPACE=238,
    API_D3DFIXUP_RESET=239,
    D3DAPI_MAX=240,
    D3DAPI_FORCE_LONG=-1
};

enum _D3DPERFEventsType
{
    D3DPERFEvent_Header=0,
    D3DPERFEvent_DrawVerticesUP=1,
    D3DPERFEvent_DrawIndexedVerticesUP=2,
    D3DPERFEvent_DrawVertices=3,
    D3DPERFEvent_DrawIndexedVertices=4,
    D3DPERFEvent_BeginEnd=5,
    D3DPERFEvent_RunPushBuffer=6,
    D3DPERFEvent_VBlank=7,
    D3DPERFEvent_Kickoff=8,
    D3DPERFEvent_Present=9,
    D3DPERFEvent_BlockUntilIdle=10,
    D3DPERFEvent_BlockOnFence=11,
    D3DPERFEvent_PushBufferWait=12,
    D3DPERFEvent_ObjectLockWait=13,
    D3DPERFEvent_User0=21,
    D3DPERFEvent_User1=22,
    D3DPERFEvent_User2=23,
    D3DPERFEvent_User3=24,
    D3DPERFEvent_User4=25,
    D3DPERFEvent_User5=26,
    D3DPERFEvent_User6=27,
    D3DPERFEvent_User7=28,
    D3DPERFEvent_User8=29,
    D3DPERFEvent_User9=30,
    D3DPERFEvent_User10=31,
    D3DPERFEvent_Max=32
};

enum _D3DPERF_PerformanceCounters
{
    PERF_VERTICES=0,
    PERF_SETTRANSFORM_WORLD=1,
    PERF_SETTRANSFORM_VIEW=2,
    PERF_SETTRANSFORM_PROJECTION=3,
    PERF_SETTRANSFORM_TEXTURE=4,
    PERF_PUSHBUFFER_SEGMENTS=5,
    PERF_PUSHBUFFER_WAITS=6,
    PERF_OBJECTLOCK_WAITS=7,
    PERF_PRESENT_WAITS=8,
    PERF_D3DDEVICE_BLOCKUNTILIDLE_WAITS=9,
    PERF_D3DDEVICE_BLOCKUNTILVERTICALBLANK_WAITS=10,
    PERF_D3DDEVICE_BLOCKONFENCE_WAITS=11,
    PERF_CPUSPINDURINGWAIT_TIME=12,
    PERF_UNSWIZZLING_TEXTURE_LOCKS=13,
    PERF_PUSHBUFFER_JUMPTOBEGINNING=14,
    PERF_RUNPUSHBUFFER_BYTES=15,
    PERF_REDUNDANT_SETRENDERSTATE=16,
    PERF_REDUNDANT_SETTEXTURESTATE=17,
    PERF_REDUNDANT_SETVERTEXSHADER=18,
    PERF_REDUNDANT_SETTRANSFORM=19,
    PERF_REDUNDANT_SETTEXTURE=20,
    PERF_REDUNDANT_SETPALETTE=21,
    PERF_REDUNDANT_SETSTREAMSOURCE=22,
    PERF_SETSTATE_POINTPARAMS=23,
    PERF_SETSTATE_COMBINERS=24,
    PERF_SETSTATE_TEXTURESTATE=25,
    PERF_SETSTATE_SPECFOGCOMBINER=26,
    PERF_SETSTATE_TEXTRANSFORM=27,
    PERF_SETSTATE_LIGHTS=28,
    PERF_SETSTATE_TRANSFORM=29,
    D3DPERF_MAX=30,
    D3DPERF_FORCE_LONG=-1
};

struct _D3DLIGHT8// Size=0x68 (Id=2315)
{
    enum _D3DLIGHTTYPE Type;// Offset=0x0 Size=0x4
    struct _D3DCOLORVALUE Diffuse;// Offset=0x4 Size=0x10
    struct _D3DCOLORVALUE Specular;// Offset=0x14 Size=0x10
    struct _D3DCOLORVALUE Ambient;// Offset=0x24 Size=0x10
    struct _D3DVECTOR Position;// Offset=0x34 Size=0xc
    struct _D3DVECTOR Direction;// Offset=0x40 Size=0xc
    float Range;// Offset=0x4c Size=0x4
    float Falloff;// Offset=0x50 Size=0x4
    float Attenuation0;// Offset=0x54 Size=0x4
    float Attenuation1;// Offset=0x58 Size=0x4
    float Attenuation2;// Offset=0x5c Size=0x4
    float Theta;// Offset=0x60 Size=0x4
    float Phi;// Offset=0x64 Size=0x4
};

struct D3D::Light// Size=0x90 (Id=2501)
{
    struct _D3DLIGHT8 Light8;// Offset=0x0 Size=0x68
    unsigned long Flags;// Offset=0x68 Size=0x4
    struct _D3DVECTOR Direction;// Offset=0x6c Size=0xc
    float Falloff_L;// Offset=0x78 Size=0x4
    float Falloff_M;// Offset=0x7c Size=0x4
    float Falloff_N;// Offset=0x80 Size=0x4
    float Scale;// Offset=0x84 Size=0x4
    float W;// Offset=0x88 Size=0x4
    struct D3D::Light * pNext;// Offset=0x8c Size=0x4
};

class D3D::D3D::CMiniport// Size=0x85c (Id=2505)
{
    union // Size=0x858 (Id=0)
    {
        struct GENERALINFO// Size=0x1c (Id=29179)
        {
            unsigned long ChipId;// Offset=0x0 Size=0x4
            unsigned long VideoRamSize;// Offset=0x4 Size=0x4
            unsigned long VideoRamType;// Offset=0x8 Size=0x4
            unsigned long ChipIntrEn0;// Offset=0xc Size=0x4
            unsigned long MpVIPSlavePresent;// Offset=0x10 Size=0x4
            unsigned long CrystalFreq;// Offset=0x14 Size=0x4
            unsigned long MaskRevision;// Offset=0x18 Size=0x4
        };
        struct DACINFO// Size=0x3c (Id=29187)
        {
            unsigned long MClk;// Offset=0x0 Size=0x4
            unsigned long VClk;// Offset=0x4 Size=0x4
            unsigned long NVClk;// Offset=0x8 Size=0x4
            unsigned long MPllM;// Offset=0xc Size=0x4
            unsigned long MPllN;// Offset=0x10 Size=0x4
            unsigned long MPllO;// Offset=0x14 Size=0x4
            unsigned long MPllP;// Offset=0x18 Size=0x4
            unsigned long VPllM;// Offset=0x1c Size=0x4
            unsigned long VPllN;// Offset=0x20 Size=0x4
            unsigned long VPllO;// Offset=0x24 Size=0x4
            unsigned long VPllP;// Offset=0x28 Size=0x4
            unsigned long NVPllM;// Offset=0x2c Size=0x4
            unsigned long NVPllN;// Offset=0x30 Size=0x4
            unsigned long NVPllO;// Offset=0x34 Size=0x4
            unsigned long NVPllP;// Offset=0x38 Size=0x4
        };
        struct HALINFO// Size=0x5c (Id=29203)
        {
            long FifoChID;// Offset=0x0 Size=0x4
            unsigned long FifoMode;// Offset=0x4 Size=0x4
            int FifoInUse;// Offset=0x8 Size=0x4
            unsigned long FifoInstance;// Offset=0xc Size=0x4
            unsigned long FifoAllocCount;// Offset=0x10 Size=0x4
            long FifoCacheDepth;// Offset=0x14 Size=0x4
            unsigned long FifoObjectCount;// Offset=0x18 Size=0x4
            unsigned long FifoIntrEn0;// Offset=0x1c Size=0x4
            unsigned long FifoRetryCount;// Offset=0x20 Size=0x4
            unsigned long FifoUserBase;// Offset=0x24 Size=0x4
            unsigned long FifoContextAddr1;// Offset=0x28 Size=0x4
            unsigned long FifoContextAddr2;// Offset=0x2c Size=0x4
            unsigned long HashTableAddr;// Offset=0x30 Size=0x4
            unsigned long GrChID;// Offset=0x34 Size=0x4
            unsigned long GrCtxTable[2];// Offset=0x38 Size=0x8
            unsigned long GrCtxTableBase;// Offset=0x40 Size=0x4
            unsigned long GrCurrentObjects3d[2];// Offset=0x44 Size=0x8
            unsigned long FbSave0;// Offset=0x4c Size=0x4
            unsigned long FbSave1;// Offset=0x50 Size=0x4
            unsigned long McSave;// Offset=0x54 Size=0x4
            unsigned long McSaveIntrEn0;// Offset=0x58 Size=0x4
        };
        struct VIDEOMODETIMING// Size=0x40 (Id=29225)
        {
            unsigned long HorizontalVisible;// Offset=0x0 Size=0x4
            unsigned long VerticalVisible;// Offset=0x4 Size=0x4
            unsigned long Refresh;// Offset=0x8 Size=0x4
            unsigned long HorizontalTotal;// Offset=0xc Size=0x4
            unsigned long HorizontalBlankStart;// Offset=0x10 Size=0x4
            unsigned long HorizontalRetraceStart;// Offset=0x14 Size=0x4
            unsigned long HorizontalRetraceEnd;// Offset=0x18 Size=0x4
            unsigned long HorizontalBlankEnd;// Offset=0x1c Size=0x4
            unsigned long VerticalTotal;// Offset=0x20 Size=0x4
            unsigned long VerticalBlankStart;// Offset=0x24 Size=0x4
            unsigned long VerticalRetraceStart;// Offset=0x28 Size=0x4
            unsigned long VerticalRetraceEnd;// Offset=0x2c Size=0x4
            unsigned long VerticalBlankEnd;// Offset=0x30 Size=0x4
            unsigned long PixelClock;// Offset=0x34 Size=0x4
            unsigned long HorizontalSyncPolarity;// Offset=0x38 Size=0x4
            unsigned long VerticalSyncPolarity;// Offset=0x3c Size=0x4
        };
        struct HW_HASHENTRY// Size=0x8 (Id=29242)
        {
            unsigned long ht_ObjectHandle;// Offset=0x0 Size=0x4
            unsigned long ht_Context;// Offset=0x4 Size=0x4
        };
        struct OBJECTINFO// Size=0x10 (Id=29245)
        {
            unsigned long Handle;// Offset=0x0 Size=0x4
            unsigned short SubChannel;// Offset=0x4 Size=0x2
            unsigned short Engine;// Offset=0x6 Size=0x2
            unsigned long ClassNum;// Offset=0x8 Size=0x4
            unsigned long Instance;// Offset=0xc Size=0x4
            void Init();
        };
        struct VBLANKFLIPS// Size=0x8 (Id=29253)
        {
            int Pending;// Offset=0x0 Size=0x4
            unsigned long Offset;// Offset=0x4 Size=0x4
        };
        void * m_RegisterBase;// Offset=0x0 Size=0x4
        unsigned long m_SurfacePitch;// Offset=0x4 Size=0x4
        unsigned long m_DisplayMode;// Offset=0x8 Size=0x4
        unsigned long m_Format;// Offset=0xc Size=0x4
        void * m_InstMem;// Offset=0x10 Size=0x4
        struct _KINTERRUPT m_InterruptObject;// Offset=0x14 Size=0x70
        struct _KDPC m_Dpc;// Offset=0x84 Size=0x1c
        int m_InterruptsEnabled;// Offset=0xa0 Size=0x4
        int m_UnhandledVBI;// Offset=0xa4 Size=0x4
        struct D3D::D3D::CMiniport::GENERALINFO m_GenInfo;// Offset=0xa8 Size=0x1c
        struct D3D::D3D::CMiniport::DACINFO m_DacInfo;// Offset=0xc4 Size=0x3c
        struct D3D::D3D::CMiniport::HALINFO m_HalInfo;// Offset=0x100 Size=0x5c
        struct D3D::D3D::CMiniport::VIDEOMODETIMING m_VideoModeTiming;// Offset=0x15c Size=0x40
        unsigned long m_VideoModeDepth;// Offset=0x19c Size=0x4
        unsigned long m_FreeInstAddr;// Offset=0x1a0 Size=0x4
        struct _HAL_SHUTDOWN_REGISTRATION m_ShutdownRegistration;// Offset=0x1a4 Size=0x10
        struct D3D::D3D::CMiniport::VBLANKFLIPS m_VBlankFlips[2];// Offset=0x1b4 Size=0x10
        void  ( * m_pSwapCallback)(struct _D3DSWAPDATA * );// Offset=0x1c4 Size=0x4
        void  ( * m_pVerticalBlankCallback)(struct _D3DVBLANKDATA * );// Offset=0x1c8 Size=0x4
        struct _KEVENT m_VerticalBlankEvent;// Offset=0x1cc Size=0x10
        struct _KEVENT m_BusyBlockEvent;// Offset=0x1dc Size=0x10
        unsigned long m_CurrentAvInfo;// Offset=0x1ec Size=0x4
        int m_FirstFlip;// Offset=0x1f0 Size=0x4
        unsigned long m_VBlankFlipCount;// Offset=0x1f4 Size=0x4
        unsigned long m_VBlankCount;// Offset=0x1f8 Size=0x4
        unsigned long m_VBlankCountAtLastFlip;// Offset=0x1fc Size=0x4
        unsigned long m_VBlanksBetweenFlips;// Offset=0x200 Size=0x4
        unsigned long m_FlipCount;// Offset=0x204 Size=0x4
        int m_OrImmediate;// Offset=0x208 Size=0x4
        unsigned long m_IsOddField;// Offset=0x20c Size=0x4
        unsigned long m_TimeBetweenVBlanks;// Offset=0x210 Size=0x4
        unsigned long m_TimeOfLastVBlank;// Offset=0x214 Size=0x4
        unsigned long m_TimeOfLastFlip;// Offset=0x218 Size=0x4
        struct _D3DGAMMARAMP m_GammaRamp[2];// Offset=0x21c Size=0x600
        int m_GammaUpdated[2];// Offset=0x81c Size=0x8
        unsigned long m_GammaCurrentIndex;// Offset=0x824 Size=0x4
        unsigned long m_OverlayVBlank;// Offset=0x828 Size=0x4
        unsigned long m_DebugRegister[11];// Offset=0x82c Size=0x2c
        struct PUSHBUFFERFIXUPINFO// Size=0x10 (Id=29295)
        {
            unsigned long * pFixupData;// Offset=0x0 Size=0x4
            unsigned char * pStart;// Offset=0x4 Size=0x4
            unsigned long ReturnOffset;// Offset=0x8 Size=0x4
            unsigned long * ReturnAddress;// Offset=0xc Size=0x4
        };
        unsigned char __align0[2120];// Offset=0x10 Size=0x848
    };
    public unsigned long m_PusherGetRunTotal;// Offset=0x858 Size=0x4
    public int InitHardware();
    public unsigned long GetPresentFlagsFromAvInfo(unsigned long );
    public unsigned long GetDisplayCapabilities();
    public void SetVideoMode(unsigned long ,unsigned long ,unsigned long ,unsigned long ,enum _D3DFORMAT ,unsigned long ,unsigned long );
    public int InitDMAChannel(unsigned long ,struct D3D::D3D::CMiniport::OBJECTINFO * ,struct D3D::D3D::CMiniport::OBJECTINFO * ,unsigned long ,void ** );
    public int BindToChannel(struct D3D::D3D::CMiniport::OBJECTINFO * );
    public int CreateGrObject(unsigned long ,unsigned long ,struct D3D::D3D::CMiniport::OBJECTINFO * );
    public int CreateCtxDmaObject(unsigned long ,unsigned long ,void * ,unsigned long ,struct D3D::D3D::CMiniport::OBJECTINFO * );
    public unsigned long SetDmaRange(unsigned long ,struct D3DSurface * );
    public int CreateTile(unsigned long ,unsigned long ,unsigned long ,unsigned long ,unsigned long ,unsigned long ,unsigned long );
    public int DestroyTile(unsigned long ,unsigned long );
    public void ShutdownEngines();
    public void DacProgramGammaRamp(struct _D3DGAMMARAMP * );
    public int IsFlipPending();
    public void DacProgramVideoStart(unsigned long );
    public void DisableInterrupts(void * );
    public void EnableInterrupts(void * );
    public unsigned long GetRefreshRate();
    public unsigned long GetTime();
    public unsigned char Isr(struct _KINTERRUPT * ,void * );
    public void Dpc(struct _KDPC * ,void * ,void * ,void * );
    public void ShutdownNotification(struct _HAL_SHUTDOWN_REGISTRATION * );
    public void TilingUpdateIdle(unsigned long * );
    public void FixupPushBuffer(struct D3D::D3D::CMiniport::PUSHBUFFERFIXUPINFO * ,unsigned long );
    private int InitEngines();
    private int LoadEngines();
    private int MapRegisters();
    private int GetGeneralInfo();
    private void InitGammaRamp(unsigned long );
    private void SoftwareMethod(unsigned long ,unsigned long );
    private void SetupPaletteAndGamma();
    private unsigned long ReserveInstMem(unsigned long );
    private void GetAddressInfo(void * ,void ** ,unsigned long * ,int );
    private unsigned long ServiceGrInterrupt();
    private unsigned long VBlank();
    private void VBlankFlip(unsigned long ,unsigned long );
    private unsigned long ServiceFifoInterrupt();
    private unsigned long ServiceMediaPortInterrupt();
    private unsigned long ServiceVideoInterrupt();
    private void HalMcControlInit();
    private void HalFbControlInit();
    private void HalVideoControlInit();
    private void HalMpControlInit();
    private void HalGrControlInit();
    private void HalGrControlLoad();
    private void HalGrIdle();
    private void HalGrLoadChannelContext(unsigned long );
    private void HalGrUnloadChannelContext(unsigned long );
    private void HalGrInitObjectContext(unsigned long ,unsigned long );
    private void HalGrInit3d();
    private void HalFifoControlInit();
    private void HalFifoControlLoad();
    private void HalFifoContextSwitch(unsigned long );
    private void HalFifoAllocDMA(unsigned long ,unsigned long ,unsigned long ,struct D3D::D3D::CMiniport::OBJECTINFO * );
    private void HalFifoHashAdd(unsigned long ,unsigned long ,unsigned long ,unsigned long ,unsigned long );
    private void HalDacControlInit();
    private void HalDacLoad();
    private void HalDacUnload();
    private void HalDacProgramMClk();
    private void HalDacProgramNVClk();
    private void HalDacProgramPClk();
    private void DumpClocks();
    private void GrDone();
    private void TmrDelay(unsigned long );
    private unsigned char UnlockCRTC();
    private void RestoreCRTCLock(unsigned char );
    private unsigned char ReadCRTCLock();
    public int IsOddField();
};

struct D3D::D3D::CMiniport::OBJECTINFO// Size=0x10 (Id=2506)
{
    unsigned long Handle;// Offset=0x0 Size=0x4
    unsigned short SubChannel;// Offset=0x4 Size=0x2
    unsigned short Engine;// Offset=0x6 Size=0x2
    unsigned long ClassNum;// Offset=0x8 Size=0x4
    unsigned long Instance;// Offset=0xc Size=0x4
    void Init();
};

struct D3D::D3D::CMiniport::GENERALINFO// Size=0x1c (Id=2507)
{
    unsigned long ChipId;// Offset=0x0 Size=0x4
    unsigned long VideoRamSize;// Offset=0x4 Size=0x4
    unsigned long VideoRamType;// Offset=0x8 Size=0x4
    unsigned long ChipIntrEn0;// Offset=0xc Size=0x4
    unsigned long MpVIPSlavePresent;// Offset=0x10 Size=0x4
    unsigned long CrystalFreq;// Offset=0x14 Size=0x4
    unsigned long MaskRevision;// Offset=0x18 Size=0x4
};

struct D3D::D3D::CMiniport::DACINFO// Size=0x3c (Id=2508)
{
    unsigned long MClk;// Offset=0x0 Size=0x4
    unsigned long VClk;// Offset=0x4 Size=0x4
    unsigned long NVClk;// Offset=0x8 Size=0x4
    unsigned long MPllM;// Offset=0xc Size=0x4
    unsigned long MPllN;// Offset=0x10 Size=0x4
    unsigned long MPllO;// Offset=0x14 Size=0x4
    unsigned long MPllP;// Offset=0x18 Size=0x4
    unsigned long VPllM;// Offset=0x1c Size=0x4
    unsigned long VPllN;// Offset=0x20 Size=0x4
    unsigned long VPllO;// Offset=0x24 Size=0x4
    unsigned long VPllP;// Offset=0x28 Size=0x4
    unsigned long NVPllM;// Offset=0x2c Size=0x4
    unsigned long NVPllN;// Offset=0x30 Size=0x4
    unsigned long NVPllO;// Offset=0x34 Size=0x4
    unsigned long NVPllP;// Offset=0x38 Size=0x4
};

struct D3D::D3D::CMiniport::HALINFO// Size=0x5c (Id=2509)
{
    long FifoChID;// Offset=0x0 Size=0x4
    unsigned long FifoMode;// Offset=0x4 Size=0x4
    int FifoInUse;// Offset=0x8 Size=0x4
    unsigned long FifoInstance;// Offset=0xc Size=0x4
    unsigned long FifoAllocCount;// Offset=0x10 Size=0x4
    long FifoCacheDepth;// Offset=0x14 Size=0x4
    unsigned long FifoObjectCount;// Offset=0x18 Size=0x4
    unsigned long FifoIntrEn0;// Offset=0x1c Size=0x4
    unsigned long FifoRetryCount;// Offset=0x20 Size=0x4
    unsigned long FifoUserBase;// Offset=0x24 Size=0x4
    unsigned long FifoContextAddr1;// Offset=0x28 Size=0x4
    unsigned long FifoContextAddr2;// Offset=0x2c Size=0x4
    unsigned long HashTableAddr;// Offset=0x30 Size=0x4
    unsigned long GrChID;// Offset=0x34 Size=0x4
    unsigned long GrCtxTable[2];// Offset=0x38 Size=0x8
    unsigned long GrCtxTableBase;// Offset=0x40 Size=0x4
    unsigned long GrCurrentObjects3d[2];// Offset=0x44 Size=0x8
    unsigned long FbSave0;// Offset=0x4c Size=0x4
    unsigned long FbSave1;// Offset=0x50 Size=0x4
    unsigned long McSave;// Offset=0x54 Size=0x4
    unsigned long McSaveIntrEn0;// Offset=0x58 Size=0x4
};

struct D3D::D3D::CMiniport::VIDEOMODETIMING// Size=0x40 (Id=2510)
{
    unsigned long HorizontalVisible;// Offset=0x0 Size=0x4
    unsigned long VerticalVisible;// Offset=0x4 Size=0x4
    unsigned long Refresh;// Offset=0x8 Size=0x4
    unsigned long HorizontalTotal;// Offset=0xc Size=0x4
    unsigned long HorizontalBlankStart;// Offset=0x10 Size=0x4
    unsigned long HorizontalRetraceStart;// Offset=0x14 Size=0x4
    unsigned long HorizontalRetraceEnd;// Offset=0x18 Size=0x4
    unsigned long HorizontalBlankEnd;// Offset=0x1c Size=0x4
    unsigned long VerticalTotal;// Offset=0x20 Size=0x4
    unsigned long VerticalBlankStart;// Offset=0x24 Size=0x4
    unsigned long VerticalRetraceStart;// Offset=0x28 Size=0x4
    unsigned long VerticalRetraceEnd;// Offset=0x2c Size=0x4
    unsigned long VerticalBlankEnd;// Offset=0x30 Size=0x4
    unsigned long PixelClock;// Offset=0x34 Size=0x4
    unsigned long HorizontalSyncPolarity;// Offset=0x38 Size=0x4
    unsigned long VerticalSyncPolarity;// Offset=0x3c Size=0x4
};

struct D3D::D3D::CMiniport::HW_HASHENTRY// Size=0x8 (Id=2511)
{
    unsigned long ht_ObjectHandle;// Offset=0x0 Size=0x4
    unsigned long ht_Context;// Offset=0x4 Size=0x4
};

struct D3D::D3D::CMiniport::VBLANKFLIPS// Size=0x8 (Id=2512)
{
    int Pending;// Offset=0x0 Size=0x4
    unsigned long Offset;// Offset=0x4 Size=0x4
};

struct D3D::D3D::CMiniport::PUSHBUFFERFIXUPINFO// Size=0x10 (Id=2513)
{
    unsigned long * pFixupData;// Offset=0x0 Size=0x4
    unsigned char * pStart;// Offset=0x4 Size=0x4
    unsigned long ReturnOffset;// Offset=0x8 Size=0x4
    unsigned long * ReturnAddress;// Offset=0xc Size=0x4
};

struct D3D::DISPLAYMODE// Size=0xc (Id=2514)
{
    unsigned long AvInfo;// Offset=0x0 Size=0x4
    unsigned short Width;// Offset=0x4 Size=0x2
    unsigned short Height;// Offset=0x6 Size=0x2
    unsigned long DisplayMode;// Offset=0x8 Size=0x4
};

struct D3D::SwapSavedState// Size=0x228 (Id=2515)
{
    struct D3D::PixelShader * pPixelShader;// Offset=0x0 Size=0x4
    unsigned long VertexShaderHandle;// Offset=0x4 Size=0x4
    unsigned long Stage1ColorOp;// Offset=0x8 Size=0x4
    unsigned long PSTextureModes;// Offset=0xc Size=0x4
    unsigned long RenderState[18];// Offset=0x10 Size=0x48
    unsigned long TextureState[11];// Offset=0x58 Size=0x2c
    unsigned long PixelShaderState[57];// Offset=0x84 Size=0xe4
    unsigned long VertexShaders[48];// Offset=0x168 Size=0xc0
};

struct D3D::PrivateDataNode// Size=0x24 (Id=2516)
{
    struct D3D::PrivateDataNode * pNext;// Offset=0x0 Size=0x4
    void * pObject;// Offset=0x4 Size=0x4
    struct _GUID guid;// Offset=0x8 Size=0x10
    unsigned long size;// Offset=0x18 Size=0x4
    unsigned long flags;// Offset=0x1c Size=0x4
    union // Size=0x4 (Id=0)
    {
        struct IUnknown * pUnknown;// Offset=0x20 Size=0x4
        unsigned char Data[1];// Offset=0x20 Size=0x1
    };
};

struct D3DX::png_dsort_struct// Size=0x8 (Id=2517)
{
    struct D3DX::png_dsort_struct * next;// Offset=0x0 Size=0x4
    unsigned char left;// Offset=0x4 Size=0x1
    unsigned char right;// Offset=0x5 Size=0x1
};

enum D3DX::J_MESSAGE_CODE
{
    JMSG_NOMESSAGE=0,
    JERR_ARITH_NOTIMPL=1,
    JERR_BAD_ALIGN_TYPE=2,
    JERR_BAD_ALLOC_CHUNK=3,
    JERR_BAD_BUFFER_MODE=4,
    JERR_BAD_COMPONENT_ID=5,
    JERR_BAD_DCTSIZE=6,
    JERR_BAD_IN_COLORSPACE=7,
    JERR_BAD_J_COLORSPACE=8,
    JERR_BAD_LENGTH=9,
    JERR_BAD_LIB_VERSION=10,
    JERR_BAD_MCU_SIZE=11,
    JERR_BAD_POOL_ID=12,
    JERR_BAD_PRECISION=13,
    JERR_BAD_PROGRESSION=14,
    JERR_BAD_PROG_SCRIPT=15,
    JERR_BAD_SAMPLING=16,
    JERR_BAD_SCAN_SCRIPT=17,
    JERR_BAD_STATE=18,
    JERR_BAD_STRUCT_SIZE=19,
    JERR_BAD_VIRTUAL_ACCESS=20,
    JERR_BUFFER_SIZE=21,
    JERR_CANT_SUSPEND=22,
    JERR_CCIR601_NOTIMPL=23,
    JERR_COMPONENT_COUNT=24,
    JERR_CONVERSION_NOTIMPL=25,
    JERR_DAC_INDEX=26,
    JERR_DAC_VALUE=27,
    JERR_DHT_COUNTS=28,
    JERR_DHT_INDEX=29,
    JERR_DQT_INDEX=30,
    JERR_EMPTY_IMAGE=31,
    JERR_EMS_READ=32,
    JERR_EMS_WRITE=33,
    JERR_EOI_EXPECTED=34,
    JERR_FILE_READ=35,
    JERR_FILE_WRITE=36,
    JERR_FRACT_SAMPLE_NOTIMPL=37,
    JERR_HUFF_CLEN_OVERFLOW=38,
    JERR_HUFF_MISSING_CODE=39,
    JERR_IMAGE_TOO_BIG=40,
    JERR_INPUT_EMPTY=41,
    JERR_INPUT_EOF=42,
    JERR_MISMATCHED_QUANT_TABLE=43,
    JERR_MISSING_DATA=44,
    JERR_MODE_CHANGE=45,
    JERR_NOTIMPL=46,
    JERR_NOT_COMPILED=47,
    JERR_NO_BACKING_STORE=48,
    JERR_NO_HUFF_TABLE=49,
    JERR_NO_IMAGE=50,
    JERR_NO_QUANT_TABLE=51,
    JERR_NO_SOI=52,
    JERR_OUT_OF_MEMORY=53,
    JERR_QUANT_COMPONENTS=54,
    JERR_QUANT_FEW_COLORS=55,
    JERR_QUANT_MANY_COLORS=56,
    JERR_SOF_DUPLICATE=57,
    JERR_SOF_NO_SOS=58,
    JERR_SOF_UNSUPPORTED=59,
    JERR_SOI_DUPLICATE=60,
    JERR_SOS_NO_SOF=61,
    JERR_TFILE_CREATE=62,
    JERR_TFILE_READ=63,
    JERR_TFILE_SEEK=64,
    JERR_TFILE_WRITE=65,
    JERR_TOO_LITTLE_DATA=66,
    JERR_UNKNOWN_MARKER=67,
    JERR_VIRTUAL_BUG=68,
    JERR_WIDTH_OVERFLOW=69,
    JERR_XMS_READ=70,
    JERR_XMS_WRITE=71,
    JMSG_COPYRIGHT=72,
    JMSG_VERSION=73,
    JTRC_16BIT_TABLES=74,
    JTRC_ADOBE=75,
    JTRC_APP0=76,
    JTRC_APP14=77,
    JTRC_DAC=78,
    JTRC_DHT=79,
    JTRC_DQT=80,
    JTRC_DRI=81,
    JTRC_EMS_CLOSE=82,
    JTRC_EMS_OPEN=83,
    JTRC_EOI=84,
    JTRC_HUFFBITS=85,
    JTRC_JFIF=86,
    JTRC_JFIF_BADTHUMBNAILSIZE=87,
    JTRC_JFIF_MINOR=88,
    JTRC_JFIF_THUMBNAIL=89,
    JTRC_MISC_MARKER=90,
    JTRC_PARMLESS_MARKER=91,
    JTRC_QUANTVALS=92,
    JTRC_QUANT_3_NCOLORS=93,
    JTRC_QUANT_NCOLORS=94,
    JTRC_QUANT_SELECTED=95,
    JTRC_RECOVERY_ACTION=96,
    JTRC_RST=97,
    JTRC_SMOOTH_NOTIMPL=98,
    JTRC_SOF=99,
    JTRC_SOF_COMPONENT=100,
    JTRC_SOI=101,
    JTRC_SOS=102,
    JTRC_SOS_COMPONENT=103,
    JTRC_SOS_PARAMS=104,
    JTRC_TFILE_CLOSE=105,
    JTRC_TFILE_OPEN=106,
    JTRC_UNKNOWN_IDS=107,
    JTRC_XMS_CLOSE=108,
    JTRC_XMS_OPEN=109,
    JWRN_ADOBE_XFORM=110,
    JWRN_BOGUS_PROGRESSION=111,
    JWRN_EXTRANEOUS_DATA=112,
    JWRN_HIT_MARKER=113,
    JWRN_HUFF_BAD_CODE=114,
    JWRN_JFIF_MAJOR=115,
    JWRN_JPEG_EOF=116,
    JWRN_MUST_RESYNC=117,
    JWRN_NOT_SEQUENTIAL=118,
    JWRN_TOO_MUCH_DATA=119,
    JMSG_LASTMSGCODE=120
};

struct D3DX::jpeg_entropy_decoder// Size=0x8 (Id=2519)
{
    void  ( * start_pass)(struct D3DX::jpeg_decompress_struct * );// Offset=0x0 Size=0x4
    unsigned char  ( * decode_mcu)(struct D3DX::jpeg_decompress_struct * ,short ** [64]);// Offset=0x4 Size=0x4
};

struct D3DX::jpeg_upsampler// Size=0xc (Id=2520)
{
    void  ( * start_pass)(struct D3DX::jpeg_decompress_struct * );// Offset=0x0 Size=0x4
    void  ( * upsample)(struct D3DX::jpeg_decompress_struct * ,unsigned char *** ,unsigned int * ,unsigned int ,unsigned char ** ,unsigned int * ,unsigned int );// Offset=0x4 Size=0x4
    unsigned char need_context_rows;// Offset=0x8 Size=0x1
};

enum D3DX::J_BUF_MODE
{
    JBUF_PASS_THRU=0,
    JBUF_SAVE_SOURCE=1,
    JBUF_CRANK_DEST=2,
    JBUF_SAVE_AND_PASS=3
};

struct D3DX::jpeg_marker_reader// Size=0x5c (Id=2522)
{
    void  ( * reset_marker_reader)(struct D3DX::jpeg_decompress_struct * );// Offset=0x0 Size=0x4
    int  ( * read_markers)(struct D3DX::jpeg_decompress_struct * );// Offset=0x4 Size=0x4
    unsigned char  ( * read_restart_marker)(struct D3DX::jpeg_decompress_struct * );// Offset=0x8 Size=0x4
    unsigned char  ( * process_COM)(struct D3DX::jpeg_decompress_struct * );// Offset=0xc Size=0x4
    unsigned char  ( * process_APPn)[16];// Offset=0x10 Size=0x40
    unsigned char saw_SOI;// Offset=0x50 Size=0x1
    unsigned char saw_SOF;// Offset=0x51 Size=0x1
    unsigned char __align0[2];// Offset=0x52 Size=0x2
    int next_restart_num;// Offset=0x54 Size=0x4
    unsigned int discarded_bytes;// Offset=0x58 Size=0x4
};

struct D3DX::box// Size=0x20 (Id=2523)
{
    int c0min;// Offset=0x0 Size=0x4
    int c0max;// Offset=0x4 Size=0x4
    int c1min;// Offset=0x8 Size=0x4
    int c1max;// Offset=0xc Size=0x4
    int c2min;// Offset=0x10 Size=0x4
    int c2max;// Offset=0x14 Size=0x4
    long volume;// Offset=0x18 Size=0x4
    long colorcount;// Offset=0x1c Size=0x4
};

struct D3DX::jpeg_decomp_master// Size=0xc (Id=2524)
{
    void  ( * prepare_for_output_pass)(struct D3DX::jpeg_decompress_struct * );// Offset=0x0 Size=0x4
    void  ( * finish_output_pass)(struct D3DX::jpeg_decompress_struct * );// Offset=0x4 Size=0x4
    unsigned char is_dummy_pass;// Offset=0x8 Size=0x1
};

struct D3DX::jpeg_color_deconverter// Size=0x8 (Id=2525)
{
    void  ( * start_pass)(struct D3DX::jpeg_decompress_struct * );// Offset=0x0 Size=0x4
    void  ( * color_convert)(struct D3DX::jpeg_decompress_struct * ,unsigned char *** ,unsigned int ,unsigned char ** ,int );// Offset=0x4 Size=0x4
};

struct D3DX::jpeg_color_quantizer// Size=0x10 (Id=2526)
{
    void  ( * start_pass)(struct D3DX::jpeg_decompress_struct * ,unsigned char );// Offset=0x0 Size=0x4
    void  ( * color_quantize)(struct D3DX::jpeg_decompress_struct * ,unsigned char ** ,unsigned char ** ,int );// Offset=0x4 Size=0x4
    void  ( * finish_pass)(struct D3DX::jpeg_decompress_struct * );// Offset=0x8 Size=0x4
    void  ( * new_color_map)(struct D3DX::jpeg_decompress_struct * );// Offset=0xc Size=0x4
};

struct D3DX::jpeg_inverse_dct// Size=0x2c (Id=2527)
{
    void  ( * start_pass)(struct D3DX::jpeg_decompress_struct * );// Offset=0x0 Size=0x4
    void  ( * inverse_DCT)[10];// Offset=0x4 Size=0x28
};

struct D3DX::jpeg_d_main_controller// Size=0x8 (Id=2528)
{
    void  ( * start_pass)(struct D3DX::jpeg_decompress_struct * ,enum D3DX::J_BUF_MODE );// Offset=0x0 Size=0x4
    void  ( * process_data)(struct D3DX::jpeg_decompress_struct * ,unsigned char ** ,unsigned int * ,unsigned int );// Offset=0x4 Size=0x4
};

struct D3DX::jpeg_d_coef_controller// Size=0x14 (Id=2529)
{
    void  ( * start_input_pass)(struct D3DX::jpeg_decompress_struct * );// Offset=0x0 Size=0x4
    int  ( * consume_data)(struct D3DX::jpeg_decompress_struct * );// Offset=0x4 Size=0x4
    void  ( * start_output_pass)(struct D3DX::jpeg_decompress_struct * );// Offset=0x8 Size=0x4
    int  ( * decompress_data)(struct D3DX::jpeg_decompress_struct * ,unsigned char *** );// Offset=0xc Size=0x4
    struct D3DX::jvirt_barray_control ** coef_arrays;// Offset=0x10 Size=0x4
};

struct D3DX::my_cquantizer// Size=0x2c (Id=2530)
{
    struct D3DX::jpeg_color_quantizer pub;// Offset=0x0 Size=0x10
    unsigned char ** sv_colormap;// Offset=0x10 Size=0x4
    int desired;// Offset=0x14 Size=0x4
    unsigned short ** histogram[32];// Offset=0x18 Size=0x4
    unsigned char needs_zeroed;// Offset=0x1c Size=0x1
    unsigned char __align0[3];// Offset=0x1d Size=0x3
    short * fserrors;// Offset=0x20 Size=0x4
    unsigned char on_odd_row;// Offset=0x24 Size=0x1
    unsigned char __align1[3];// Offset=0x25 Size=0x3
    int * error_limiter;// Offset=0x28 Size=0x4
};

struct D3DX::jpeg_input_controller// Size=0x14 (Id=2531)
{
    int  ( * consume_input)(struct D3DX::jpeg_decompress_struct * );// Offset=0x0 Size=0x4
    void  ( * reset_input_controller)(struct D3DX::jpeg_decompress_struct * );// Offset=0x4 Size=0x4
    void  ( * start_input_pass)(struct D3DX::jpeg_decompress_struct * );// Offset=0x8 Size=0x4
    void  ( * finish_input_pass)(struct D3DX::jpeg_decompress_struct * );// Offset=0xc Size=0x4
    unsigned char has_multiple_scans;// Offset=0x10 Size=0x1
    unsigned char eoi_reached;// Offset=0x11 Size=0x1
};

struct D3DX::jpeg_d_post_controller// Size=0x8 (Id=2532)
{
    void  ( * start_pass)(struct D3DX::jpeg_decompress_struct * ,enum D3DX::J_BUF_MODE );// Offset=0x0 Size=0x4
    void  ( * post_process_data)(struct D3DX::jpeg_decompress_struct * ,unsigned char *** ,unsigned int * ,unsigned int ,unsigned char ** ,unsigned int * ,unsigned int );// Offset=0x4 Size=0x4
};

struct D3DX::my_cquantizer// Size=0x58 (Id=2533)
{
    struct D3DX::jpeg_color_quantizer pub;// Offset=0x0 Size=0x10
    unsigned char ** sv_colormap;// Offset=0x10 Size=0x4
    int sv_actual;// Offset=0x14 Size=0x4
    unsigned char ** colorindex;// Offset=0x18 Size=0x4
    unsigned char is_padded;// Offset=0x1c Size=0x1
    unsigned char __align0[3];// Offset=0x1d Size=0x3
    int Ncolors[4];// Offset=0x20 Size=0x10
    int row_index;// Offset=0x30 Size=0x4
    int * odither[4][16];// Offset=0x34 Size=0x10
    short * fserrors[4];// Offset=0x44 Size=0x10
    unsigned char on_odd_row;// Offset=0x54 Size=0x1
};

struct D3DX::backing_store_struct// Size=0x50 (Id=2534)
{
    void  ( * read_backing_store)(struct D3DX::jpeg_common_struct * ,struct D3DX::backing_store_struct * ,void * ,long ,long );// Offset=0x0 Size=0x4
    void  ( * write_backing_store)(struct D3DX::jpeg_common_struct * ,struct D3DX::backing_store_struct * ,void * ,long ,long );// Offset=0x4 Size=0x4
    void  ( * close_backing_store)(struct D3DX::jpeg_common_struct * ,struct D3DX::backing_store_struct * );// Offset=0x8 Size=0x4
    struct _iobuf * temp_file;// Offset=0xc Size=0x4
    char temp_name[64];// Offset=0x10 Size=0x40
};

struct D3DX::my_memory_mgr// Size=0x50 (Id=2535)
{
    struct D3DX::jpeg_memory_mgr pub;// Offset=0x0 Size=0x30
    union D3DX::small_pool_struct * small_list[2];// Offset=0x30 Size=0x8
    union D3DX::large_pool_struct * large_list[2];// Offset=0x38 Size=0x8
    struct D3DX::jvirt_sarray_control * virt_sarray_list;// Offset=0x40 Size=0x4
    struct D3DX::jvirt_barray_control * virt_barray_list;// Offset=0x44 Size=0x4
    long total_space_allocated;// Offset=0x48 Size=0x4
    unsigned int last_rowsperchunk;// Offset=0x4c Size=0x4
};

struct D3DX::large_pool_struct::__unnamed// Size=0xc (Id=2536)
{
    union D3DX::large_pool_struct * next;// Offset=0x0 Size=0x4
    unsigned int bytes_used;// Offset=0x4 Size=0x4
    unsigned int bytes_left;// Offset=0x8 Size=0x4
};

union D3DX::large_pool_struct// Size=0x10 (Id=2537)
{
    union // Size=0xc (Id=0)
    {
        struct D3DX::large_pool_struct::__unnamed hdr;// Offset=0x0 Size=0xc
        float dummy;// Offset=0x0 Size=0x8
    };
};

struct D3DX::small_pool_struct::__unnamed// Size=0xc (Id=2538)
{
    union D3DX::small_pool_struct * next;// Offset=0x0 Size=0x4
    unsigned int bytes_used;// Offset=0x4 Size=0x4
    unsigned int bytes_left;// Offset=0x8 Size=0x4
};

union D3DX::small_pool_struct// Size=0x10 (Id=2539)
{
    union // Size=0xc (Id=0)
    {
        struct D3DX::small_pool_struct::__unnamed hdr;// Offset=0x0 Size=0xc
        float dummy;// Offset=0x0 Size=0x8
    };
};

struct D3DX::jvirt_barray_control// Size=0x78 (Id=2540)
{
    short ** mem_buffer[64];// Offset=0x0 Size=0x4
    unsigned int rows_in_array;// Offset=0x4 Size=0x4
    unsigned int blocksperrow;// Offset=0x8 Size=0x4
    unsigned int maxaccess;// Offset=0xc Size=0x4
    unsigned int rows_in_mem;// Offset=0x10 Size=0x4
    unsigned int rowsperchunk;// Offset=0x14 Size=0x4
    unsigned int cur_start_row;// Offset=0x18 Size=0x4
    unsigned int first_undef_row;// Offset=0x1c Size=0x4
    unsigned char pre_zero;// Offset=0x20 Size=0x1
    unsigned char dirty;// Offset=0x21 Size=0x1
    unsigned char b_s_open;// Offset=0x22 Size=0x1
    unsigned char __align0[1];// Offset=0x23 Size=0x1
    struct D3DX::jvirt_barray_control * next;// Offset=0x24 Size=0x4
    struct D3DX::backing_store_struct b_s_info;// Offset=0x28 Size=0x50
};

struct D3DX::jvirt_sarray_control// Size=0x78 (Id=2541)
{
    unsigned char ** mem_buffer;// Offset=0x0 Size=0x4
    unsigned int rows_in_array;// Offset=0x4 Size=0x4
    unsigned int samplesperrow;// Offset=0x8 Size=0x4
    unsigned int maxaccess;// Offset=0xc Size=0x4
    unsigned int rows_in_mem;// Offset=0x10 Size=0x4
    unsigned int rowsperchunk;// Offset=0x14 Size=0x4
    unsigned int cur_start_row;// Offset=0x18 Size=0x4
    unsigned int first_undef_row;// Offset=0x1c Size=0x4
    unsigned char pre_zero;// Offset=0x20 Size=0x1
    unsigned char dirty;// Offset=0x21 Size=0x1
    unsigned char b_s_open;// Offset=0x22 Size=0x1
    unsigned char __align0[1];// Offset=0x23 Size=0x1
    struct D3DX::jvirt_sarray_control * next;// Offset=0x24 Size=0x4
    struct D3DX::backing_store_struct b_s_info;// Offset=0x28 Size=0x50
};

struct D3DX::my_upsampler// Size=0xa0 (Id=2542)
{
    struct D3DX::jpeg_upsampler pub;// Offset=0x0 Size=0xc
    unsigned char ** color_buf[10];// Offset=0xc Size=0x28
    void  ( * methods)[10];// Offset=0x34 Size=0x28
    int next_row_out;// Offset=0x5c Size=0x4
    unsigned int rows_to_go;// Offset=0x60 Size=0x4
    int rowgroup_height[10];// Offset=0x64 Size=0x28
    unsigned char h_expand[10];// Offset=0x8c Size=0xa
    unsigned char v_expand[10];// Offset=0x96 Size=0xa
};

struct D3DX::my_post_controller// Size=0x1c (Id=2543)
{
    struct D3DX::jpeg_d_post_controller pub;// Offset=0x0 Size=0x8
    struct D3DX::jvirt_sarray_control * whole_image;// Offset=0x8 Size=0x4
    unsigned char ** buffer;// Offset=0xc Size=0x4
    unsigned int strip_height;// Offset=0x10 Size=0x4
    unsigned int starting_row;// Offset=0x14 Size=0x4
    unsigned int next_row;// Offset=0x18 Size=0x4
};

struct D3DX::savable_state// Size=0x14 (Id=2544)
{
    unsigned int EOBRUN;// Offset=0x0 Size=0x4
    int last_dc_val[4];// Offset=0x4 Size=0x10
};

struct D3DX::bitread_working_state// Size=0x28 (Id=2545)
{
    unsigned char * next_input_byte;// Offset=0x0 Size=0x4
    unsigned int bytes_in_buffer;// Offset=0x4 Size=0x4
    int unread_marker;// Offset=0x8 Size=0x4
    long get_buffer;// Offset=0xc Size=0x4
    long long get_buffer_64;// Offset=0x10 Size=0x8
    int bits_left;// Offset=0x18 Size=0x4
    struct D3DX::jpeg_decompress_struct * cinfo;// Offset=0x1c Size=0x4
    unsigned char * printed_eod_ptr;// Offset=0x20 Size=0x4
};

struct D3DX::bitread_perm_state// Size=0x18 (Id=2546)
{
    long long get_buffer_64;// Offset=0x0 Size=0x8
    long get_buffer;// Offset=0x8 Size=0x4
    int bits_left;// Offset=0xc Size=0x4
    unsigned char printed_eod;// Offset=0x10 Size=0x1
};

struct D3DX::phuff_entropy_decoder// Size=0x50 (Id=2547)
{
    struct D3DX::jpeg_entropy_decoder pub;// Offset=0x0 Size=0x8
    struct D3DX::bitread_perm_state bitstate;// Offset=0x8 Size=0x18
    struct D3DX::savable_state saved;// Offset=0x20 Size=0x10
    unsigned char __align0[4];// Offset=0x30 Size=0x4
    unsigned int restarts_to_go;// Offset=0x34 Size=0x4
    struct D3DX::d_derived_tbl * derived_tbls[4];// Offset=0x38 Size=0x10
    struct D3DX::d_derived_tbl * ac_derived_tbl;// Offset=0x48 Size=0x4
};

struct D3DX::d_derived_tbl// Size=0x5d4 (Id=2548)
{
    long mincode[17];// Offset=0x0 Size=0x44
    long maxcode[18];// Offset=0x44 Size=0x48
    int valptr[17];// Offset=0x8c Size=0x44
    struct D3DX::JHUFF_TBL * pub;// Offset=0xd0 Size=0x4
    int look_nbits[256];// Offset=0xd4 Size=0x400
    unsigned char look_sym[256];// Offset=0x4d4 Size=0x100
};

struct D3DX::my_upsampler// Size=0x30 (Id=2549)
{
    struct D3DX::jpeg_upsampler pub;// Offset=0x0 Size=0xc
    void  ( * upmethod)(struct D3DX::jpeg_decompress_struct * ,unsigned char *** ,unsigned int ,unsigned char ** );// Offset=0xc Size=0x4
    int * Cr_r_tab;// Offset=0x10 Size=0x4
    int * Cb_b_tab;// Offset=0x14 Size=0x4
    long * Cr_g_tab;// Offset=0x18 Size=0x4
    long * Cb_g_tab;// Offset=0x1c Size=0x4
    unsigned char * spare_row;// Offset=0x20 Size=0x4
    unsigned char spare_full;// Offset=0x24 Size=0x1
    unsigned char __align0[3];// Offset=0x25 Size=0x3
    unsigned int out_row_width;// Offset=0x28 Size=0x4
    unsigned int rows_to_go;// Offset=0x2c Size=0x4
};

struct D3DX::my_decomp_master// Size=0x1c (Id=2550)
{
    struct D3DX::jpeg_decomp_master pub;// Offset=0x0 Size=0xc
    int pass_number;// Offset=0xc Size=0x4
    unsigned char using_merged_upsample;// Offset=0x10 Size=0x1
    unsigned char __align0[3];// Offset=0x11 Size=0x3
    struct D3DX::jpeg_color_quantizer * quantizer_1pass;// Offset=0x14 Size=0x4
    struct D3DX::jpeg_color_quantizer * quantizer_2pass;// Offset=0x18 Size=0x4
};

enum D3DX::JPEG_MARKER
{
    M_SOF0=192,
    M_SOF1=193,
    M_SOF2=194,
    M_SOF3=195,
    M_SOF5=197,
    M_SOF6=198,
    M_SOF7=199,
    M_JPG=200,
    M_SOF9=201,
    M_SOF10=202,
    M_SOF11=203,
    M_SOF13=205,
    M_SOF14=206,
    M_SOF15=207,
    M_DHT=196,
    M_DAC=204,
    M_RST0=208,
    M_RST1=209,
    M_RST2=210,
    M_RST3=211,
    M_RST4=212,
    M_RST5=213,
    M_RST6=214,
    M_RST7=215,
    M_SOI=216,
    M_EOI=217,
    M_SOS=218,
    M_DQT=219,
    M_DNL=220,
    M_DRI=221,
    M_DHP=222,
    M_EXP=223,
    M_APP0=224,
    M_APP1=225,
    M_APP2=226,
    M_APP3=227,
    M_APP4=228,
    M_APP5=229,
    M_APP6=230,
    M_APP7=231,
    M_APP8=232,
    M_APP9=233,
    M_APP10=234,
    M_APP11=235,
    M_APP12=236,
    M_APP13=237,
    M_APP14=238,
    M_APP15=239,
    M_JPG0=240,
    M_JPG13=253,
    M_COM=254,
    M_TEM=1,
    M_ERROR=256
};

struct D3DX::my_main_controller// Size=0x50 (Id=2552)
{
    struct D3DX::jpeg_d_main_controller pub;// Offset=0x0 Size=0x8
    unsigned char ** buffer[10];// Offset=0x8 Size=0x28
    unsigned char buffer_full;// Offset=0x30 Size=0x1
    unsigned char __align0[3];// Offset=0x31 Size=0x3
    unsigned int rowgroup_ctr;// Offset=0x34 Size=0x4
    unsigned char *** xbuffer[2];// Offset=0x38 Size=0x8
    int whichptr;// Offset=0x40 Size=0x4
    int context_state;// Offset=0x44 Size=0x4
    unsigned int rowgroups_avail;// Offset=0x48 Size=0x4
    unsigned int iMCU_row_ctr;// Offset=0x4c Size=0x4
};

struct D3DX::my_input_controller// Size=0x18 (Id=2553)
{
    struct D3DX::jpeg_input_controller pub;// Offset=0x0 Size=0x14
    unsigned char inheaders;// Offset=0x14 Size=0x1
};

struct D3DX::savable_state// Size=0x10 (Id=2554)
{
    int last_dc_val[4];// Offset=0x0 Size=0x10
};

struct D3DX::huff_entropy_decoder// Size=0x58 (Id=2555)
{
    struct D3DX::jpeg_entropy_decoder pub;// Offset=0x0 Size=0x8
    struct D3DX::bitread_perm_state bitstate;// Offset=0x8 Size=0x18
    struct D3DX::savable_state saved;// Offset=0x20 Size=0x10
    unsigned int restarts_to_go;// Offset=0x30 Size=0x4
    struct D3DX::d_derived_tbl * dc_derived_tbls[4];// Offset=0x34 Size=0x10
    struct D3DX::d_derived_tbl * ac_derived_tbls[4];// Offset=0x44 Size=0x10
};

struct D3DX::my_idct_controller// Size=0x54 (Id=2556)
{
    struct D3DX::jpeg_inverse_dct pub;// Offset=0x0 Size=0x2c
    int cur_method[10];// Offset=0x2c Size=0x28
};

struct D3DX::my_color_deconverter// Size=0x18 (Id=2557)
{
    struct D3DX::jpeg_color_deconverter pub;// Offset=0x0 Size=0x8
    int * Cr_r_tab;// Offset=0x8 Size=0x4
    int * Cb_b_tab;// Offset=0xc Size=0x4
    long * Cr_g_tab;// Offset=0x10 Size=0x4
    long * Cb_g_tab;// Offset=0x14 Size=0x4
};

struct D3DX::my_coef_controller// Size=0x74 (Id=2558)
{
    struct D3DX::jpeg_d_coef_controller pub;// Offset=0x0 Size=0x14
    unsigned int MCU_ctr;// Offset=0x14 Size=0x4
    int MCU_vert_offset;// Offset=0x18 Size=0x4
    int MCU_rows_per_iMCU_row;// Offset=0x1c Size=0x4
    short * MCU_buffer[10][64];// Offset=0x20 Size=0x28
    struct D3DX::jvirt_barray_control * whole_image[10];// Offset=0x48 Size=0x28
    int * coef_bits_latch;// Offset=0x70 Size=0x4
};

struct d3dx_jpeg_source_mgr : public D3DX::jpeg_source_mgr// Size=0x24 (Id=2570)
{
    unsigned char __align0[28];// Offset=0x0 Size=0x1c
    void * pvData;// Offset=0x1c Size=0x4
    unsigned long cbData;// Offset=0x20 Size=0x4
};

struct d3dx_jpeg_error_mgr : public D3DX::jpeg_error_mgr// Size=0xc4 (Id=2571)
{
    unsigned char __align0[132];// Offset=0x0 Size=0x84
    int setjmp_buffer[16];// Offset=0x84 Size=0x40
};

struct d3dx_tga_header// Size=0x12 (Id=2572)
{
    unsigned char cbId;// Offset=0x0 Size=0x1
    unsigned char bColorMap;// Offset=0x1 Size=0x1
    unsigned char bImageType;// Offset=0x2 Size=0x1
    unsigned short wColorMapIndex;// Offset=0x3 Size=0x2
    unsigned short wColorMapLength;// Offset=0x5 Size=0x2
    unsigned char bColorMapBits;// Offset=0x7 Size=0x1
    unsigned short wXOrigin;// Offset=0x8 Size=0x2
    unsigned short wYOrigin;// Offset=0xa Size=0x2
    unsigned short wWidth;// Offset=0xc Size=0x2
    unsigned short wHeight;// Offset=0xe Size=0x2
    unsigned char bBits;// Offset=0x10 Size=0x1
    unsigned char bFlags;// Offset=0x11 Size=0x1
};

struct d3dx_png_io// Size=0x8 (Id=2573)
{
    void * pv;// Offset=0x0 Size=0x4
    unsigned long cb;// Offset=0x4 Size=0x4
};

struct _D3DDISPLAYMODE// Size=0x14 (Id=3124)
{
    unsigned int Width;// Offset=0x0 Size=0x4
    unsigned int Height;// Offset=0x4 Size=0x4
    unsigned int RefreshRate;// Offset=0x8 Size=0x4
    unsigned long Flags;// Offset=0xc Size=0x4
    enum _D3DFORMAT Format;// Offset=0x10 Size=0x4
};

struct _D3DCAPS8// Size=0xd4 (Id=3125)
{
    enum _D3DDEVTYPE DeviceType;// Offset=0x0 Size=0x4
    unsigned int AdapterOrdinal;// Offset=0x4 Size=0x4
    unsigned long Caps;// Offset=0x8 Size=0x4
    unsigned long Caps2;// Offset=0xc Size=0x4
    unsigned long Caps3;// Offset=0x10 Size=0x4
    unsigned long PresentationIntervals;// Offset=0x14 Size=0x4
    unsigned long CursorCaps;// Offset=0x18 Size=0x4
    unsigned long DevCaps;// Offset=0x1c Size=0x4
    unsigned long PrimitiveMiscCaps;// Offset=0x20 Size=0x4
    unsigned long RasterCaps;// Offset=0x24 Size=0x4
    unsigned long ZCmpCaps;// Offset=0x28 Size=0x4
    unsigned long SrcBlendCaps;// Offset=0x2c Size=0x4
    unsigned long DestBlendCaps;// Offset=0x30 Size=0x4
    unsigned long AlphaCmpCaps;// Offset=0x34 Size=0x4
    unsigned long ShadeCaps;// Offset=0x38 Size=0x4
    unsigned long TextureCaps;// Offset=0x3c Size=0x4
    unsigned long TextureFilterCaps;// Offset=0x40 Size=0x4
    unsigned long CubeTextureFilterCaps;// Offset=0x44 Size=0x4
    unsigned long VolumeTextureFilterCaps;// Offset=0x48 Size=0x4
    unsigned long TextureAddressCaps;// Offset=0x4c Size=0x4
    unsigned long VolumeTextureAddressCaps;// Offset=0x50 Size=0x4
    unsigned long LineCaps;// Offset=0x54 Size=0x4
    unsigned long MaxTextureWidth;// Offset=0x58 Size=0x4
    unsigned long MaxTextureHeight;// Offset=0x5c Size=0x4
    unsigned long MaxVolumeExtent;// Offset=0x60 Size=0x4
    unsigned long MaxTextureRepeat;// Offset=0x64 Size=0x4
    unsigned long MaxTextureAspectRatio;// Offset=0x68 Size=0x4
    unsigned long MaxAnisotropy;// Offset=0x6c Size=0x4
    float MaxVertexW;// Offset=0x70 Size=0x4
    float GuardBandLeft;// Offset=0x74 Size=0x4
    float GuardBandTop;// Offset=0x78 Size=0x4
    float GuardBandRight;// Offset=0x7c Size=0x4
    float GuardBandBottom;// Offset=0x80 Size=0x4
    float ExtentsAdjust;// Offset=0x84 Size=0x4
    unsigned long StencilCaps;// Offset=0x88 Size=0x4
    unsigned long FVFCaps;// Offset=0x8c Size=0x4
    unsigned long TextureOpCaps;// Offset=0x90 Size=0x4
    unsigned long MaxTextureBlendStages;// Offset=0x94 Size=0x4
    unsigned long MaxSimultaneousTextures;// Offset=0x98 Size=0x4
    unsigned long VertexProcessingCaps;// Offset=0x9c Size=0x4
    unsigned long MaxActiveLights;// Offset=0xa0 Size=0x4
    unsigned long MaxUserClipPlanes;// Offset=0xa4 Size=0x4
    unsigned long MaxVertexBlendMatrices;// Offset=0xa8 Size=0x4
    unsigned long MaxVertexBlendMatrixIndex;// Offset=0xac Size=0x4
    float MaxPointSize;// Offset=0xb0 Size=0x4
    unsigned long MaxPrimitiveCount;// Offset=0xb4 Size=0x4
    unsigned long MaxVertexIndex;// Offset=0xb8 Size=0x4
    unsigned long MaxStreams;// Offset=0xbc Size=0x4
    unsigned long MaxStreamStride;// Offset=0xc0 Size=0x4
    unsigned long VertexShaderVersion;// Offset=0xc4 Size=0x4
    unsigned long MaxVertexShaderConst;// Offset=0xc8 Size=0x4
    unsigned long PixelShaderVersion;// Offset=0xcc Size=0x4
    float MaxPixelShaderValue;// Offset=0xd0 Size=0x4
};

struct _D3DPRESENT_PARAMETERS_// Size=0x44 (Id=3126)
{
    unsigned int BackBufferWidth;// Offset=0x0 Size=0x4
    unsigned int BackBufferHeight;// Offset=0x4 Size=0x4
    enum _D3DFORMAT BackBufferFormat;// Offset=0x8 Size=0x4
    unsigned int BackBufferCount;// Offset=0xc Size=0x4
    unsigned long MultiSampleType;// Offset=0x10 Size=0x4
    enum _D3DSWAPEFFECT SwapEffect;// Offset=0x14 Size=0x4
    struct HWND__ * hDeviceWindow;// Offset=0x18 Size=0x4
    int Windowed;// Offset=0x1c Size=0x4
    int EnableAutoDepthStencil;// Offset=0x20 Size=0x4
    enum _D3DFORMAT AutoDepthStencilFormat;// Offset=0x24 Size=0x4
    unsigned long Flags;// Offset=0x28 Size=0x4
    unsigned int FullScreen_RefreshRateInHz;// Offset=0x2c Size=0x4
    unsigned int FullScreen_PresentationInterval;// Offset=0x30 Size=0x4
    struct D3DSurface * BufferSurfaces[3];// Offset=0x34 Size=0xc
    struct D3DSurface * DepthStencilSurface;// Offset=0x40 Size=0x4
};

struct D3DDevice// Size=0x1 (Id=3127)
{
    union // Size=0x2e2 (Id=0)
    {
        unsigned long AddRef();// Offset=0x0 Size=0x5
        unsigned long Release();
        long GetDirect3D(struct Direct3D ** );// Offset=0x0 Size=0xe
        long GetDeviceCaps(struct _D3DCAPS8 * );// Offset=0x0 Size=0xe
        long GetDisplayMode(struct _D3DDISPLAYMODE * );// Offset=0x0 Size=0xe
        long GetCreationParameters(struct _D3DDEVICE_CREATION_PARAMETERS * );
        long Reset(struct _D3DPRESENT_PARAMETERS_ * );
        long Present(struct tagRECT * ,struct tagRECT * ,void * ,void * );// Offset=0x0 Size=0xc
        long GetBackBuffer(int ,unsigned long ,struct D3DSurface ** );// Offset=0x0 Size=0x19
        long GetRasterStatus(struct _D3DRASTER_STATUS * );
        void SetGammaRamp(unsigned long ,struct _D3DGAMMARAMP * );
        void GetGammaRamp(struct _D3DGAMMARAMP * );
        long CreateTexture(unsigned int ,unsigned int ,unsigned int ,unsigned long ,enum _D3DFORMAT ,unsigned long ,struct D3DTexture ** );// Offset=0x0 Size=0x5
        long CreateVolumeTexture(unsigned int ,unsigned int ,unsigned int ,unsigned int ,unsigned long ,enum _D3DFORMAT ,unsigned long ,struct D3DVolumeTexture ** );
        long CreateCubeTexture(unsigned int ,unsigned int ,unsigned long ,enum _D3DFORMAT ,unsigned long ,struct D3DCubeTexture ** );
        long CreateVertexBuffer(unsigned int ,unsigned long ,unsigned long ,unsigned long ,struct D3DVertexBuffer ** );// Offset=0x0 Size=0x5
        long CreateIndexBuffer(unsigned int ,unsigned long ,enum _D3DFORMAT ,unsigned long ,struct D3DIndexBuffer ** );
        long CreateRenderTarget(unsigned int ,unsigned int ,enum _D3DFORMAT ,unsigned long ,int ,struct D3DSurface ** );
        long CreateDepthStencilSurface(unsigned int ,unsigned int ,enum _D3DFORMAT ,unsigned long ,struct D3DSurface ** );
        long CreateImageSurface(unsigned int ,unsigned int ,enum _D3DFORMAT ,struct D3DSurface ** );
        long CopyRects(struct D3DSurface * ,struct tagRECT * ,unsigned int ,struct D3DSurface * ,struct tagPOINT * );
        long SetRenderTarget(struct D3DSurface * ,struct D3DSurface * );// Offset=0x0 Size=0x14
        long GetRenderTarget(struct D3DSurface ** );
        long GetDepthStencilSurface(struct D3DSurface ** );// Offset=0x0 Size=0x5
        long BeginScene();// Offset=0x0 Size=0x3
        long EndScene();// Offset=0x0 Size=0x3
        long Clear(unsigned long ,struct _D3DRECT * ,unsigned long ,unsigned long ,float ,unsigned long );// Offset=0x0 Size=0x28
        long SetTransform(enum _D3DTRANSFORMSTATETYPE ,struct _D3DMATRIX * );// Offset=0x0 Size=0x14
        long GetTransform(enum _D3DTRANSFORMSTATETYPE ,struct _D3DMATRIX * );
        long MultiplyTransform(enum _D3DTRANSFORMSTATETYPE ,struct _D3DMATRIX * );
        long SetViewport(struct _D3DVIEWPORT8 * );// Offset=0x0 Size=0xf
        long GetViewport(struct _D3DVIEWPORT8 * );// Offset=0x0 Size=0xf
        long SetMaterial(struct _D3DMATERIAL8 * );
        long GetMaterial(struct _D3DMATERIAL8 * );
        long SetLight(unsigned long ,struct _D3DLIGHT8 * );// Offset=0x0 Size=0x5
        long GetLight(unsigned long ,struct _D3DLIGHT8 * );
        long LightEnable(unsigned long ,int );// Offset=0x0 Size=0x5
        long GetLightEnable(unsigned long ,int * );
        long SetRenderState(enum _D3DRENDERSTATETYPE ,unsigned long );// Offset=0x0 Size=0x2e2
        long GetRenderState(enum _D3DRENDERSTATETYPE ,unsigned long * );
        long ApplyStateBlock(unsigned long );
        long CaptureStateBlock(unsigned long );
        long DeleteStateBlock(unsigned long );
        long CreateStateBlock(enum _D3DSTATEBLOCKTYPE ,unsigned long * );
        long GetTexture(unsigned long ,struct D3DBaseTexture ** );
        long SetTexture(unsigned long ,struct D3DBaseTexture * );// Offset=0x0 Size=0x14
        long GetTextureStageState(unsigned long ,enum _D3DTEXTURESTAGESTATETYPE ,unsigned long * );
        long SetTextureStageState(unsigned long ,enum _D3DTEXTURESTAGESTATETYPE ,unsigned long );// Offset=0x0 Size=0x100
        long DrawPrimitive(enum _D3DPRIMITIVETYPE ,unsigned int ,unsigned int );// Offset=0x0 Size=0x28
        long DrawIndexedPrimitive(enum _D3DPRIMITIVETYPE ,unsigned int ,unsigned int ,unsigned int ,unsigned int );
        long DrawPrimitiveUP(enum _D3DPRIMITIVETYPE ,unsigned int ,void * ,unsigned int );
        long DrawIndexedPrimitiveUP(enum _D3DPRIMITIVETYPE ,unsigned int ,unsigned int ,unsigned int ,void * ,enum _D3DFORMAT ,void * ,unsigned int );
        long CreateVertexShader(unsigned long * ,unsigned long * ,unsigned long * ,unsigned long );
        long SetVertexShader(unsigned long );// Offset=0x0 Size=0xf
        long GetVertexShader(unsigned long * );
        long DeleteVertexShader(unsigned long );
        long SetVertexShaderConstant(int ,void * ,unsigned long );
        long GetVertexShaderConstant(int ,void * ,unsigned long );
        long GetVertexShaderDeclaration(unsigned long ,void * ,unsigned long * );
        long GetVertexShaderFunction(unsigned long ,void * ,unsigned long * );
        long SetStreamSource(unsigned int ,struct D3DVertexBuffer * ,unsigned int );// Offset=0x0 Size=0x19
        long GetStreamSource(unsigned int ,struct D3DVertexBuffer ** ,unsigned int * );
        long SetIndices(struct D3DIndexBuffer * ,unsigned int );
        long GetIndices(struct D3DIndexBuffer ** ,unsigned int * );
        long CreatePixelShader(struct _D3DPixelShaderDef * ,unsigned long * );
        long SetPixelShader(unsigned long );
        long SetPixelShaderProgram(struct _D3DPixelShaderDef * );
        long GetPixelShader(unsigned long * );
        long DeletePixelShader(unsigned long );
        long SetPixelShaderConstant(unsigned long ,void * ,unsigned long );
        long GetPixelShaderConstant(unsigned long ,void * ,unsigned long );
        long GetPixelShaderFunction(unsigned long ,struct _D3DPixelShaderDef * );
        long DrawRectPatch(unsigned int ,float * ,struct _D3DRECTPATCH_INFO * );
        long DrawTriPatch(unsigned int ,float * ,struct _D3DTRIPATCH_INFO * );
        long DeletePatch(unsigned int );
        long SetShaderConstantMode(unsigned long );// Offset=0x0 Size=0xf
        long GetShaderConstantMode(unsigned long * );
        long LoadVertexShader(unsigned long ,unsigned long );
        long LoadVertexShaderProgram(unsigned long * ,unsigned long );
        long SelectVertexShader(unsigned long ,unsigned long );
        long RunVertexStateShader(unsigned long ,float * );
        long GetVertexShaderSize(unsigned long ,unsigned int * );
        long GetVertexShaderType(unsigned long ,unsigned long * );
        long DrawVertices(enum _D3DPRIMITIVETYPE ,unsigned int ,unsigned int );
        long DrawIndexedVertices(enum _D3DPRIMITIVETYPE ,unsigned int ,unsigned short * );
        long DrawVerticesUP(enum _D3DPRIMITIVETYPE ,unsigned int ,void * ,unsigned int );
        long DrawIndexedVerticesUP(enum _D3DPRIMITIVETYPE ,unsigned int ,void * ,void * ,unsigned int );
        long PrimeVertexCache(unsigned int ,unsigned short * );
        long CreatePalette(enum _D3DPALETTESIZE ,struct D3DPalette ** );// Offset=0x0 Size=0x5
        long GetPalette(unsigned long ,struct D3DPalette ** );
        long SetPalette(unsigned long ,struct D3DPalette * );// Offset=0x0 Size=0x14
        long SetTextureStageStateNotInline(unsigned long ,enum _D3DTEXTURESTAGESTATETYPE ,unsigned long );
        long SetRenderStateNotInline(enum _D3DRENDERSTATETYPE ,unsigned long );
        long SetBackMaterial(struct _D3DMATERIAL8 * );
        long GetBackMaterial(struct _D3DMATERIAL8 * );
        long UpdateOverlay(struct D3DSurface * ,struct tagRECT * ,struct tagRECT * ,int ,unsigned long );// Offset=0x0 Size=0x23
        long EnableOverlay(int );// Offset=0x0 Size=0xf
        long EnableCC(int );
        long SendCC(int ,unsigned char ,unsigned char );
        long GetCCStatus(int * ,int * );
        long BeginVisibilityTest();
        long EndVisibilityTest(unsigned long );
        long GetVisibilityTestResult(unsigned long ,unsigned int * ,unsigned long long * );
        int GetOverlayUpdateStatus();
        long GetDisplayFieldStatus(struct _D3DFIELD_STATUS * );
        long SetVertexData2f(int ,float ,float );
        long SetVertexData4f(int ,float ,float ,float ,float );
        long SetVertexData2s(int ,short ,short );
        long SetVertexData4s(int ,short ,short ,short ,short );
        long SetVertexData4ub(int ,unsigned char ,unsigned char ,unsigned char ,unsigned char );
        long SetVertexDataColor(int ,unsigned long );
        long Begin(enum _D3DPRIMITIVETYPE );
        long End();
        long CreateFixup(unsigned int ,struct D3DFixup ** );
        long CreatePushBuffer(unsigned int ,int ,struct D3DPushBuffer ** );
        long BeginPushBuffer(struct D3DPushBuffer * );
        long EndPushBuffer();
        long RunPushBuffer(struct D3DPushBuffer * ,struct D3DFixup * );
        long GetPushBufferOffset(unsigned long * );
        long Nop();
        long GetProjectionViewportMatrix(struct _D3DMATRIX * );
        long SetModelView(struct _D3DMATRIX * ,struct _D3DMATRIX * ,struct _D3DMATRIX * );
        long GetModelView(struct _D3DMATRIX * );
        long SetVertexBlendModelView(unsigned int ,struct _D3DMATRIX * ,struct _D3DMATRIX * ,struct _D3DMATRIX * );
        long GetVertexBlendModelView(unsigned int ,struct _D3DMATRIX * ,struct _D3DMATRIX * );
        long SetVertexShaderInput(unsigned long ,unsigned int ,struct _D3DSTREAM_INPUT * );
        long GetVertexShaderInput(unsigned long * ,unsigned int * ,struct _D3DSTREAM_INPUT * );
        long SwitchTexture(unsigned long ,struct D3DBaseTexture * );
        long Suspend();
        long Resume(int );
        long SetScissors(unsigned long ,int ,struct _D3DRECT * );
        long GetScissors(unsigned long * ,int * ,struct _D3DRECT * );
        long SetTile(unsigned long ,struct _D3DTILE * );// Offset=0x0 Size=0x14
        long GetTile(unsigned long ,struct _D3DTILE * );
        unsigned long GetTileCompressionTags(unsigned long ,unsigned long );
        void SetTileCompressionTagBits(unsigned long ,unsigned long ,unsigned long * ,unsigned long );
        void GetTileCompressionTagBits(unsigned long ,unsigned long ,unsigned long * ,unsigned long );
        long BeginPush(unsigned long ,unsigned long ** );
        long EndPush(unsigned long * );
        int IsBusy();
        void BlockUntilIdle();
        void KickPushBuffer();
        void SetVerticalBlankCallback(void  ( * )(struct _D3DVBLANKDATA * ));
        void SetSwapCallback(void  ( * )(struct _D3DSWAPDATA * ));
        void BlockUntilVerticalBlank();// Offset=0x0 Size=0x5
        unsigned long InsertFence();
        int IsFencePending(unsigned long );
        void BlockOnFence(unsigned long );
        void InsertCallback(enum _D3DCALLBACKTYPE ,void  ( * )(unsigned long ),unsigned long );
        void FlushVertexCache();
        void SetFlickerFilter(unsigned long );
        void SetSoftDisplayFilter(int );
        long SetCopyRectsState(struct _D3DCOPYRECTSTATE * ,struct _D3DCOPYRECTROPSTATE * );
        long GetCopyRectsState(struct _D3DCOPYRECTSTATE * ,struct _D3DCOPYRECTROPSTATE * );
        long PersistDisplay();
        long GetPersistedSurface(struct D3DSurface ** );
        unsigned long Swap(unsigned long );
        long SetBackBufferScale(float ,float );
        long GetBackBufferScale(float * ,float * );
        long SetScreenSpaceOffset(float ,float );// Offset=0x0 Size=0x14
        long GetScreenSpaceOffset(float * ,float * );
        void SetOverscanColor(unsigned long );
        unsigned long GetOverscanColor();
        unsigned long SetDebugMarker(unsigned long );// Offset=0x0 Size=0x5
        unsigned long GetDebugMarker();
    };
};

struct _D3DDEVICE_CREATION_PARAMETERS// Size=0x10 (Id=3128)
{
    unsigned int AdapterOrdinal;// Offset=0x0 Size=0x4
    enum _D3DDEVTYPE DeviceType;// Offset=0x4 Size=0x4
    struct HWND__ * hFocusWindow;// Offset=0x8 Size=0x4
    unsigned long BehaviorFlags;// Offset=0xc Size=0x4
};

struct D3DCubeTexture : public D3DBaseTexture// Size=0x14 (Id=3129)
{
    long GetLevelDesc(unsigned int ,struct _D3DSURFACE_DESC * );// Offset=0x0 Size=0x16
    long GetCubeMapSurface(enum _D3DCUBEMAP_FACES ,unsigned int ,struct D3DSurface ** );
    long LockRect(enum _D3DCUBEMAP_FACES ,unsigned int ,struct _D3DLOCKED_RECT * ,struct tagRECT * ,unsigned long );
    long UnlockRect(enum _D3DCUBEMAP_FACES ,unsigned int );
};

struct _D3DRECTPATCH_INFO// Size=0x1c (Id=3130)
{
    unsigned int StartVertexOffsetWidth;// Offset=0x0 Size=0x4
    unsigned int StartVertexOffsetHeight;// Offset=0x4 Size=0x4
    unsigned int Width;// Offset=0x8 Size=0x4
    unsigned int Height;// Offset=0xc Size=0x4
    unsigned int Stride;// Offset=0x10 Size=0x4
    enum _D3DBASISTYPE Basis;// Offset=0x14 Size=0x4
    enum _D3DORDERTYPE Order;// Offset=0x18 Size=0x4
};

struct _D3DTRIPATCH_INFO// Size=0x10 (Id=3131)
{
    unsigned int StartVertexOffset;// Offset=0x0 Size=0x4
    unsigned int NumVertices;// Offset=0x4 Size=0x4
    enum _D3DBASISTYPE Basis;// Offset=0x8 Size=0x4
    enum _D3DORDERTYPE Order;// Offset=0xc Size=0x4
};

struct D3DPalette : public D3DResource// Size=0xc (Id=3132)
{
    union // Size=0x19 (Id=0)
    {
        long Lock(unsigned long ** ,unsigned long );// Offset=0x0 Size=0x19
        long Unlock();// Offset=0x0 Size=0x5
        enum _D3DPALETTESIZE GetSize();
    };
};

struct _D3DFIELD_STATUS// Size=0x8 (Id=3133)
{
    enum _D3DFIELDTYPE Field;// Offset=0x0 Size=0x4
    unsigned int VBlankCount;// Offset=0x4 Size=0x4
};

struct _D3DCOPYRECTSTATE// Size=0x20 (Id=3134)
{
    enum _D3DCOPYRECTCOLORFORMAT ColorFormat;// Offset=0x0 Size=0x4
    enum _D3DCOPYRECTOPERATION Operation;// Offset=0x4 Size=0x4
    int ColorKeyEnable;// Offset=0x8 Size=0x4
    unsigned long ColorKeyValue;// Offset=0xc Size=0x4
    unsigned long BlendAlpha;// Offset=0x10 Size=0x4
    unsigned long PremultAlpha;// Offset=0x14 Size=0x4
    unsigned long ClippingPoint;// Offset=0x18 Size=0x4
    unsigned long ClippingSize;// Offset=0x1c Size=0x4
};

struct _D3DSURFACE_DESC// Size=0x1c (Id=3135)
{
    enum _D3DFORMAT Format;// Offset=0x0 Size=0x4
    enum _D3DRESOURCETYPE Type;// Offset=0x4 Size=0x4
    unsigned long Usage;// Offset=0x8 Size=0x4
    unsigned int Size;// Offset=0xc Size=0x4
    unsigned long MultiSampleType;// Offset=0x10 Size=0x4
    unsigned int Width;// Offset=0x14 Size=0x4
    unsigned int Height;// Offset=0x18 Size=0x4
};

struct _D3DVOLUME_DESC// Size=0x1c (Id=3136)
{
    enum _D3DFORMAT Format;// Offset=0x0 Size=0x4
    enum _D3DRESOURCETYPE Type;// Offset=0x4 Size=0x4
    unsigned long Usage;// Offset=0x8 Size=0x4
    unsigned int Size;// Offset=0xc Size=0x4
    unsigned int Width;// Offset=0x10 Size=0x4
    unsigned int Height;// Offset=0x14 Size=0x4
    unsigned int Depth;// Offset=0x18 Size=0x4
};

struct D3DResource// Size=0xc (Id=3137)
{
    union // Size=0x5 (Id=0)
    {
        unsigned long AddRef();// Offset=0x0 Size=0x5
        unsigned long Release();// Offset=0x0 Size=0x5
        long GetDevice(struct D3DDevice ** );
        enum _D3DRESOURCETYPE GetType();
        long SetPrivateData(struct _GUID & ,void * ,unsigned long ,unsigned long );
        long GetPrivateData(struct _GUID & ,void * ,unsigned long * );
        long FreePrivateData(struct _GUID & );
        int IsBusy();
        void BlockUntilNotBusy();
        void MoveResourceMemory(enum _D3DMEMORY );
        void Register(void * );
        unsigned long Common;// Offset=0x0 Size=0x4
        unsigned long Data;// Offset=0x4 Size=0x4
    };
    unsigned long Lock;// Offset=0x8 Size=0x4
};

struct _D3DVERTEXBUFFER_DESC// Size=0x8 (Id=3138)
{
    enum _D3DFORMAT Format;// Offset=0x0 Size=0x4
    enum _D3DRESOURCETYPE Type;// Offset=0x4 Size=0x4
};

struct _D3DINDEXBUFFER_DESC// Size=0x8 (Id=3139)
{
    enum _D3DFORMAT Format;// Offset=0x0 Size=0x4
    enum _D3DRESOURCETYPE Type;// Offset=0x4 Size=0x4
};


#endif // _D3D_H_
