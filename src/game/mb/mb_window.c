/*
 * mb_window.c - MB viewport/window layer (mb_window.obj).
 *
 * The MB window: a 4-entry window array (lbl_802C3228, stride 0x1A8) with a
 * current-window pointer (lbl_80344EE8, this TU's .sbss). Angles are radians;
 * MBWindowSetAng builds the tan/cotan/cos projection params, MBWindowZoom
 * derives (ang, hang) from a vertical FOV + aspect, MBWindowProject /
 * MBWindowTo3D map 3D<->screen through an ml_fmath matrix transform.
 * fn_800BBA34 is the full view setup (sizes from gWinGlobals' display info,
 * default 640x448) shared by MBWindowInit.
 *
 * Range 0x800BB804..0x800BC2EC (7 fns). PDB roster (mb_window.obj):
 * MBWindowSetAng/SetHang/Init/Zoom/Project/To3D/SetRegion. GC keeps 6 publics
 * + fn_800BBA34 (setup helper; SetHang not emitted on GC).
 * cflags_demo, C++ exceptions on.
 */

typedef signed short s16;
typedef signed int s32;
typedef unsigned char u8;
typedef unsigned int u32;
typedef float f32;

typedef struct MBCamNode {
    /* 0x00 */ f32 mat[12];   /* 3x4 camera matrix */
    /* 0x30 */ f32 pos[3];    /* camera position */
} MBCamNode;

typedef struct MBWINDOW {
    /* 0x000 */ f32 xscale;       /* px per eye-unit */
    /* 0x004 */ f32 yscale;
    /* 0x008 */ f32 xcenter;      /* px */
    /* 0x00C */ f32 ycenter;
    /* 0x010 */ f32 unk10;        /* 1.0f */
    /* 0x014 */ f32 width;
    /* 0x018 */ f32 height;
    /* 0x01C */ f32 ang;
    /* 0x020 */ f32 hang;
    /* 0x024 */ f32 tanAng;       /* tan(ang/2) */
    /* 0x028 */ f32 tanHang;
    /* 0x02C */ f32 cotAng;       /* 1/tan */
    /* 0x030 */ f32 cotHang;
    /* 0x034 */ f32 cosAng;       /* cos(ang/2) */
    /* 0x038 */ f32 cosHang;
    /* 0x03C */ f32 rect1[4];     /* {x0, w, h, y0} */
    /* 0x04C */ f32 rect2[4];     /* {x0, w, h, y0} */
    /* 0x05C */ f32 nearZ;        /* 1.0f */
    /* 0x060 */ f32 farZ;         /* 16776960.0f */
    /* 0x064 */ MBCamNode cam;    /* default camera node */
    /* 0x0A0 */ u8 unkA0[0xE4 - 0xA0];
    /* 0x0E4 */ f32 mat4[16];     /* 4x4, CopyMat4'd from gIdentityMatrix */
    /* 0x124 */ u8 unk124[0x128 - 0x124];
    /* 0x128 */ s32 unk128;
    /* 0x12C */ f32 unk12C;       /* 32.0f */
    /* 0x130 */ u8 unk130[4];
    /* 0x134 */ f32 unk134;       /* 1.0f */
    /* 0x138 */ f32 unk138;       /* -0.5f */
    /* 0x13C */ s32 index;        /* window index */
    /* 0x140 */ s32 active;
    /* 0x144 */ f32 cotHalfAng;   /* cos/sin of ang*0.5f */
    /* 0x148 */ f32 cotHalfHang;
    /* 0x14C */ f32 unk14C;       /* 1.0f */
    /* 0x150 */ u8 unk150[0x190 - 0x150];
    /* 0x190 */ f32 halfW;        /* 0.5*width */
    /* 0x194 */ f32 halfH2;       /* 0.5*height (+rect2 y0 in SetAng) */
    /* 0x198 */ f32 unk198;       /* 0.0f */
    /* 0x19C */ f32 halfW2;       /* 0.5*width */
    /* 0x1A0 */ f32 halfH;        /* 0.5*height */
    /* 0x1A4 */ f32 unk1A4;       /* = unk10 */
} MBWINDOW;

typedef struct MBDispInfo {
    u8 _pad[32];
    /* 0x20 */ s32 w;
    /* 0x24 */ s32 h;
} MBDispInfo;

typedef struct MBWinGlobals {
    u8 _pad[16];
    /* 0x10 */ MBDispInfo* disp;
} MBWinGlobals;

extern f32 sin(f32);        /* PS2-port math shim: float trig */
extern f32 cos(f32);
extern double tan(double);
extern double __atan(double);

extern void WorldVector(f32* src, f32* dst, f32* matrix);  /* ml_fmath: apply mat */
extern void BodyVector(f32* src, f32* dst, f32* matrix);   /* ml_fmath: apply mat^T */
extern void MBCameraUpdate(f32* position, f32* matrix);    /* mb_camera.c */
extern void CopyMat4(void* src, void* dst);                /* ml_fmath */
extern void MBWindowProjection(f32 angle, f32 aspect);     /* pb_window.c */
extern void MBWindowViewport(f32 x, f32 w, f32 h, f32 y);  /* pb_window.c */

extern MBWinGlobals* gWinGlobals;

MBWINDOW* lbl_80344EE8;                 /* current window (this TU's .sbss) */
extern MBWINDOW lbl_802C3228[4];        /* window array */
extern s32 lbl_802A4B30[4];             /* dirty flags; [2]/[3] cleared here */
extern s32 lbl_80343F04;                /* default screen w (640) */
extern s32 lbl_80343F0C;                /* default screen h (448) */
extern f32 lbl_80343EC8;                /* projection aspect (1.0f) */
extern f32 lbl_80127D00[];              /* default camera position */
extern f32 gIdentityMatrix[];              /* identity matrix */

MBWINDOW* fn_800BBA34(f32 ang, f32 hang);
void MBWindowSetAng(f32 ang, f32 hang);

/* 0x800BB804 - screen (s16 x,y) + depth -> 3D world point */
void MBWindowTo3D(s16* scr, MBCamNode* node, f32* out, f32 depth) {
    f32 tmp[3];
    f32 in[3];
    MBWINDOW* w = lbl_80344EE8;

    if (node == 0) {
        node = &w->cam;
    }
    in[0] = (((f32)scr[0] - w->xcenter) * depth) / w->xscale;
    in[1] = (((f32)scr[1] - w->ycenter) * depth) / w->yscale;
    in[2] = depth;
    WorldVector(in, tmp, node->mat);
    out[0] = tmp[0] + node->pos[0];
    out[1] = tmp[1] + node->pos[1];
    out[2] = tmp[2] + node->pos[2];
}

/* Shared s16 screen clamp of MBWindowProject (defined before it so
 * -inline auto folds it into both call sites). */
static s16 ClampS16(f32 s) {
    double v;

    if (s < -32767.0) {
        v = -32767.0;
    } else if (s > 32767.0) {
        v = 32767.0;
    } else {
        v = s;
    }
    return (s16)v;
}

/* 0x800BB8E8 - 3D world point -> screen (s16 x,y), optional eye-space out */
void MBWindowProject(f32* pt, MBCamNode* node, f32* outEye, s16* outScr) {
    f32 rel[3];
    u8 unusedd[4];
    f32 eye[3];
    MBWINDOW* w = lbl_80344EE8;
    f32 persp;
    f32 sx;
    f32 sy;

    rel[0] = pt[0] - node->pos[0];
    rel[1] = pt[1] - node->pos[1];
    rel[2] = pt[2] - node->pos[2];
    BodyVector(rel, eye, node->mat);

    if (eye[2] < 3.0) {
        eye[2] = 3.0f;
    }
    persp = 1.0 / eye[2];
    sx = w->xscale * (eye[0] * persp) + w->xcenter;
    sy = w->yscale * (eye[1] * persp) + w->ycenter;

    outScr[0] = ClampS16(sx);
    outScr[1] = ClampS16(sy);

    if (outEye != 0) {
        outEye[0] = eye[0];
        outEye[1] = eye[1];
        outEye[2] = eye[2];
    }
}

/* 0x800BBA34 - full view setup: sizes from the display info, rects, angles,
 * clip planes, identity transform. Returns the current window. */
MBWINDOW* fn_800BBA34(f32 ang, f32 hang) {
    MBWINDOW* w = lbl_80344EE8;
    s32 wpx;
    s32 hpx;
    f32 fw;
    f32 fh;
    u8 unused[16];

    wpx = gWinGlobals->disp->w;
    hpx = gWinGlobals->disp->h;
    if (wpx == 0 || hpx == 0) {
        wpx = lbl_80343F04;
        hpx = lbl_80343F0C;
    }
    w->width = (f32)wpx;
    w->height = (f32)hpx;
    w->halfW = 0.5 * w->width;
    w->halfH2 = 0.5 * w->height;
    w->unk198 = 0.0f;
    fw = (f32)wpx;
    fh = (f32)hpx;
    w->rect1[0] = 0.0f;
    w->rect2[0] = 0.0f;
    w->rect1[1] = fw;
    w->rect2[1] = fw;
    w->rect1[2] = fh;
    w->rect2[2] = fh;
    w->rect1[3] = 0.0f;
    w->rect2[3] = 0.0f;
    MBWindowSetAng(ang, hang);
    w->nearZ = 1.0f;
    w->farZ = 16776960.0f;
    w->unk128 = 0;
    w->unk12C = 32.0f;
    w->unk10 = 1.0f;
    w->unk134 = 1.0f;
    w->unk138 = -0.5f;
    CopyMat4(gIdentityMatrix, w->mat4);
    return w;
}

/* 0x800BBB70 - set zoom: vertical FOV -> (ang, hang) via aspect, then
 * projection. Negative zoom uses the raw display aspect.
 * PARKED residual: target materializes z=zoom as fmr f30,f31 in the then
 * branch (copy survives); ours copy-propagates - the gcontrolpads
 * rematerialization/copy class. Structure otherwise exact. */
/* fmuls operand-order escape: with both operands opaque params the inliner
 * preserves text order (MWCC canonicalizes visible-const mults const-first). */
static f32 mulf(f32 a, f32 b) {
    return a * b;
}

static double muld(double a, double b) {
    return a * b;
}

#pragma opt_lifetimes off
#pragma opt_propagation off
void MBWindowZoom(f32 zoom) {
    MBWinGlobals* g = gWinGlobals;
    MBWINDOW* w = lbl_80344EE8;
    f32 z;
    f32 aspect;
    f32 hang;

    aspect = 0.75 * (w->height / (f32)g->disp->h);
    if (zoom > 0.0f) {
        z = zoom;
        hang = 2.0 * __atan(aspect * tan(muld(z, 0.5)));
        MBWindowSetAng(z, hang);
    } else {
        z = -zoom;
        hang = 2.0 * __atan(((f32)g->disp->h / (f32)g->disp->w) * tan(muld(z, 0.5)));
        MBWindowSetAng(z, hang);
    }
    MBWindowProjection(0.31830988614222805 * (180.0 * zoom), lbl_80343EC8);
}
#pragma opt_propagation reset
#pragma opt_lifetimes reset

/* 0x800BBCA8 - set view angles (radians): tan/cotan/cos of the half angles,
 * pixel scale/center, half-angle cotans. */
void MBWindowSetAng(f32 ang, f32 hang) {
    MBWINDOW* w = lbl_80344EE8;
    f32 half;
    f32 c;
    f32 s;

    lbl_802A4B30[2] = 0;
    lbl_802A4B30[3] = 0;

    w->ang = ang;
    half = 0.5 * ang;
    c = cos(half);
    w->tanAng = sin(half) / c;
    w->cotAng = 1.0 / w->tanAng;
    w->cosAng = c;

    w->hang = hang;
    half = 0.5 * hang;
    c = cos(half);
    w->tanHang = sin(half) / c;
    w->cotHang = 1.0 / w->tanHang;
    w->cosHang = c;

    w->xcenter = 0.5 * w->width + w->rect2[0];
    w->ycenter = 0.5 * w->height + w->rect2[3];
    w->xscale = (0.5 * w->width) * w->cotAng;
    w->yscale = (0.5 * w->height) * w->cotHang;

    half = mulf(ang, 0.5f);
    s = sin(half);
    w->cotHalfAng = cos(half) / s;
    half = mulf(hang, 0.5f);
    s = sin(half);
    w->cotHalfHang = cos(half) / s;
    w->unk14C = 1.0f;

    w->halfW2 = 0.5 * w->width;
    w->halfH = 0.5 * w->height;
    w->unk1A4 = w->unk10;
    w->halfW = 0.5 * w->width;
    w->halfH2 = 0.5 * w->height + w->rect2[3];
    w->unk198 = 0.0f;
}

/* 0x800BBE88 - set the window region {x0,x1,y1,y0} (-1 = full screen edge,
 * negative = from the far edge), clamp to the display, convert to a GX
 * viewport, and refresh the rects/angles/half sizes. e = border inset. */
void MBWindowSetRegion(f32 x0, f32 x1, f32 y1, f32 y0, f32 e) {
    MBWinGlobals* g = gWinGlobals;
    MBWINDOW* w = lbl_80344EE8;
    s32 dw;
    s32 dh;
    f32 a;
    f32 b;
    f32 c;
    f32 d;
    f32 vx;
    u8 unused[16];

    if (x0 == -1.0) {
        x0 = 0.0f;
    }
    if (x1 == -1.0) {
        x1 = (f32)g->disp->w;
    } else if (x1 < 0.0f) {
        x1 = (f32)g->disp->w + x1;
    }
    if (y1 == -1.0) {
        y1 = (f32)g->disp->h;
    } else if (y1 < 0.0f) {
        y1 = (f32)g->disp->h + y1;
    }
    if (y0 == -1.0) {
        y0 = 0.0f;
    }

    a = x0;
    d = y0;
    b = x1;
    c = y1;
    if (x0 < 0.0f) {
        a = 0.0f;
    }
    if (x1 < 0.0f) {
        b = 0.0f;
    }
    if (y1 < 0.0f) {
        c = 0.0f;
    }
    if (y0 < 0.0f) {
        d = 0.0f;
    }

    dw = g->disp->w;
    if (a > (f32)dw) {
        a = (f32)dw;
    }
    if (b > (f32)dw) {
        b = (f32)dw;
    }
    dh = g->disp->h;
    if (c > (f32)dh) {
        c = (f32)dh;
    }
    if (d > (f32)dh) {
        d = (f32)dh;
    }

    if (a < b) {
        vx = a;
        b = (f32)dw - b;
    } else {
        vx = (f32)dw - a;
    }
    if (c < d) {
        d = (f32)dh - d;
    } else {
        c = (f32)dh - c;
    }
    MBWindowViewport(vx, b, c, d);

    w->rect1[0] = x0 - e;
    w->rect1[1] = x1 + e;
    w->rect1[2] = y1 + 2.0f * e;
    w->rect1[3] = y0 - 2.0f * e;
    w->rect2[0] = x0;
    w->rect2[1] = x1;
    w->rect2[2] = y1;
    w->rect2[3] = y0;
    w->width = x1 - x0;
    w->height = y1 - y0;
    MBWindowSetAng(w->ang, w->hang);

    w->halfW2 = 0.5 * w->width;
    w->halfH = 0.5 * w->height;
    w->unk1A4 = w->unk10;
    w->halfW = 0.5 * (x0 + x1);
    w->halfH2 = 0.5 * (y1 + y0);
    w->unk198 = 0.0f;
}

/* 0x800BC23C - init all 4 windows: index/active, default angles, camera. */
void MBWindowInit(void) {
    s32 i;
    u8 unused[24];

    lbl_802A4B30[2] = 0;
    lbl_802A4B30[3] = 0;
    for (i = 0; i < 4; i++) {
        lbl_80344EE8 = &lbl_802C3228[i];
        lbl_80344EE8->index = i;
        lbl_80344EE8->active = 0;
        fn_800BBA34(0.78539818525314331f, 0.58904862403869629f);
        MBCameraUpdate(lbl_80127D00, gIdentityMatrix);
    }
    lbl_80344EE8 = lbl_802C3228;
    lbl_80344EE8->active = 1;
}
