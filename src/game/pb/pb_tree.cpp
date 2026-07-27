/*
 * pb_tree.cpp - scene-tree traversal + per-node render dispatch (PB_TREE.OBJ).
 *
 * .text 0x800C79E4-0x800C8044 (the final TU of the pb C++ library run, between
 * pb_texture and PB_UTILS -- exactly where PB_TREE.OBJ sits in the Xbox link
 * order).  Walks the MB tree from the root (lbl_80344ECC), maintaining a
 * 64-level matrix stack, and dispatches each node by type to the MB draw
 * subsystems (objects / psys / polys / blits / text / sort queues).
 *
 * Xbox PDB roster (PB_TREE.OBJ): MBNodeSetVis, MBNodeIsVis, init_geo_traverse,
 * PushMatrix, PopMatrix, CameraFace, pbRenderNode, pbTraverseDrawObjects,
 * pbTreeTraverse; data matrix_level, matrix_stack, node_flags, view_flag,
 * skiped_nodes, alpha_tree_dist, alpha_tree_dist_add, clear_alpha_dist,
 * camfacemat.  On GC the tiny statics (init_geo_traverse/PushMatrix/PopMatrix)
 * are inlined into the two traversal fns ("PushMatrix: Too many levels" now
 * lives in pbTraverseDrawObjects), leaving 4 fns:
 *   fn_800C79E4 = pbTreeTraverse   (kept fn_ name: called by MATCHING mb_main.c)
 *   0x800C7A70  = pbTraverseDrawObjects (recursive walker, Push/Pop inlined)
 *   0x800C7C4C  = pbRenderNode     (type switch, jumptable_80128468)
 *   0x800C7EB0  = CameraFace       (billboard modes, FaceCamMat/TopFaceMat)
 *
 * node_flags (0x802C8CF8, 0x700) = per-level flag stack (64 words) followed
 * by the node-vis bitmask (the MBNodeSetVis/IsVis array); original source
 * addresses it both directly and via matrix_stack+0x1000/+0x1100 byte
 * offsets -- both styles kept (link-identical ...bss.0 relocs).
 */

typedef signed char s8;
typedef unsigned char u8;
typedef unsigned short u16;
typedef signed long s32;
typedef unsigned long u32;
typedef float f32;

class vec3 {
  public:
    f32 x, y, z;
    vec3() {}
};

class vec4 {
  public:
    f32 x, y, z, w;
    vec4() {}
};

class mat44 {
  public:
    f32 m[4][4];
    mat44() {}
};

/* g3dMath3D.cpp (C++ linkage) */
void vec4ApplyTrans(vec4& d, vec4& v, mat44& m);

/* Scene-tree node (mb_tree node; local matrix at +0, links at +0x74). */
typedef struct PBTREENODE {
    /* 0x00 */ f32 mtx[4][4]; /* local transform; row 3 = position */
    /* 0x40 */ f32 scale[3];  /* extra transform data (flags & 8) */
    /* 0x4C */ u8 _pad4C[4];
    /* 0x50 */ u16 id;        /* node index (vis bitmask bit) */
    /* 0x52 */ s8 type;       /* render dispatch type */
    /* 0x53 */ u8 _pad53[13];
    /* 0x60 */ u32 flags;
    /* 0x64 */ u8 _pad64[16];
    /* 0x74 */ struct PBTREENODE* parent;
    /* 0x78 */ struct PBTREENODE* child;
    /* 0x7C */ struct PBTREENODE* next;
} PBTREENODE;

typedef struct PBWINGLOBALS {
    /* 0x00 */ s32 unk00;
    /* 0x04 */ char* cur; /* current PBWINDOW* */
} PBWINGLOBALS;

extern "C" {

/* pb_frame / pb_texture hooks */
void fn_800C1004(void);
void fn_800C1120(s32 arg);
void fn_800C72DC(void);
void fn_800C73E0(void);

/* mb_objects.c */
void InitSortObjects(void);
s32 MBDrawObjectTest(PBTREENODE* node, mat44* mat, u32 allowDefer);
void MBSetupObject(PBTREENODE* node, mat44* mat, u32 allowDefer,
                   f32 sortOverride, f32 zadd);
void MBDrawPsysObjects(void);
void MBDrawDistObjects(void);
void MBDrawSortObjects(void);
void TopFaceMat(mat44* mat);
void FaceCamMat(mat44* mat, f32 tilt);
void QuickYawMat(mat44* mat);

/* mb_particle.c */
s32 MBDrawPsysTest(PBTREENODE* node, mat44* mat);
s32 MBTraversePsys(PBTREENODE* node, mat44* mat);

/* mb_blit.c / mb_poly.c / mb_font.c */
void MBDrawBlits(PBTREENODE* node);
void MBDrawPolyInsts(PBTREENODE* node);
void MBRenderText(void);

/* ps2/ml_fmath.c matrix helpers */
void MulMat4(mat44* a, void* b, mat44* dst);
void CopyMat4(void* src, void* dst);
void fn_800BDF48(void* pyr, void* mat, mat44* dst);
void fn_800BE030(mat44* mat, vec3* out);
void fn_800BE1E0(mat44* a, void* b, mat44* dst, f32* scale);
void fn_800BE79C(mat44* dst, mat44* a, vec3* v);

/* ps2/ml_error.c */
void FatalError(const char* text, s32 errorCode);
void ErrorPrintf(const char* format, ...);

extern PBWINGLOBALS* gWinGlobals;
extern char* lbl_80344EE8;      /* current MB window (mb_window.c) */
extern PBTREENODE* lbl_80344ECC; /* tree root (mb_tree.c) */

/* this TU's fns (definitions below, target emission order) */
void fn_800C79E4(void); /* = pbTreeTraverse */
s32 pbTraverseDrawObjects(PBTREENODE* node, PBTREENODE* stop, s32 mode);
void pbRenderNode(PBTREENODE* node);
void CameraFace(u32 flags);

/* ---- data ------------------------------------------------------------ */

/* .sdata draw-enable toggles + alpha-tree distance (forward order) */
s32 draw_blits_on = 1;
s32 draw_text_on = 1;
s32 draw_polys_on = 1;
s32 draw_psys_on = 1;
s32 draw_sortobjs_on = 1;
s32 draw_distobjs_on = 1;
f32 alpha_tree_dist = -1.0f;

/* .data default camera-facing matrix (mirrored X/Z) */
f32 camfacemat[4][4] = {
    { -1.0f, 0.0f, 0.0f, 0.0f },
    { 0.0f, 1.0f, 0.0f, 0.0f },
    { 0.0f, 0.0f, -1.0f, 0.0f },
    { 0.0f, 0.0f, 0.0f, 1.0f },
};

/* .sbss (C++ strong defs, section-allocated in declaration order) */
s32 matrix_level;          /* 0x80345120 */
s32 skiped_nodes;          /* 0x80345124 */
f32 alpha_tree_dist_add;   /* 0x80345128 */
s32 clear_alpha_dist;      /* 0x8034512C */
s32 lbl_80345130;          /* 0x80345130  cleared after each traverse */

/* .bss: matrix stack, then flag stack + vis bitmask */
mat44 matrix_stack[64];    /* 0x802C7CF8 */
u32 node_flags[0x1C0];     /* 0x802C8CF8  [0..63] per-level flags, +0x100 vis bits */

/* ---- code ------------------------------------------------------------ */

/* 0x800C79E4  pbTreeTraverse: per-frame render traversal entry.  Seeds the
 * matrix stack from the current window camera, resets the traversal state,
 * then walks the whole tree from the root. */
void fn_800C79E4(void)
{
    InitSortObjects();
    CopyMat4(lbl_80344EE8 + 228, matrix_stack);
    node_flags[0] = 1;
    matrix_level = 0;
    alpha_tree_dist = -1.0f;
    clear_alpha_dist = 0;
    fn_800C1004();
    fn_800C73E0();
    fn_800C72DC();
    fn_800C1120(0);
    pbTraverseDrawObjects(lbl_80344ECC, 0, 1);
    lbl_80345130 = 0;
    fn_800C1120(1);
}

/* 0x800C7A70  pbTraverseDrawObjects: recursive sibling-list walker with the
 * Xbox PushMatrix/PopMatrix statics inlined.  mode 1 = remember parent and
 * resume upward when the level is exhausted; mode 2 = continue from the
 * node's next sibling (upward resume).  Returns 1 when `stop` is reached. */
s32 pbTraverseDrawObjects(PBTREENODE* node, PBTREENODE* stop, s32 mode)
{
    PBTREENODE* up = 0;
    s32 l;
    char* q;

    if (mode != 0) {
        up = node->parent;
        if (mode == 2) {
            node = node->next;
        }
    }

    while (node != 0) {
        if (node == stop) {
            return 1;
        }
        if (node->flags & 2) {
            node = node->next;
            continue;
        }

        /* PushMatrix (inlined) */
        matrix_level++;
        if (matrix_level >= 64) {
            FatalError("PushMatrix: Too many levels", 0x800000);
        }
        l = matrix_level;
        *(u32*)((char*)matrix_stack + 4096 + l * 4) =
            *(u32*)((char*)matrix_stack + 4092 + l * 4);

        if (node->flags & 8) {
            fn_800BE1E0(&matrix_stack[l - 1], node, &matrix_stack[l],
                        node->scale);
            q = (char*)matrix_stack + matrix_level * 4;
            *(u32*)(q + 4096) |= 2;
        } else {
            MulMat4(&matrix_stack[l - 1], node, &matrix_stack[l]);
        }

        if (node->flags & 0x0F000000) {
            CameraFace(node->flags);
        }
        if (!(node->flags & 1)) {
            pbRenderNode(node);
        }

        if (node->child != 0) {
            if (pbTraverseDrawObjects(node->child, stop, 0)) {
                return 1;
            }
        }

        /* PopMatrix (inlined) */
        matrix_level--;
        node = node->next;
        if (clear_alpha_dist != 0) {
            alpha_tree_dist = -1.0f;
            clear_alpha_dist = 0;
        }
    }

    if (up != 0) {
        matrix_level--;
        if (pbTraverseDrawObjects(up, stop, 2)) {
            return 1;
        }
    }
    return 0;
}

/* 0x800C7C4C  pbRenderNode: dispatch one node by type.  Type 2 = object
 * (vis test / setup, alpha-tree fade, parent vis-bit marking), 3 = alpha
 * tree distance marker, 4 = clear level flag bit, 7/8/9 = flush sort /
 * dist / psys queues, 10 = poly insts, 13 = blits, 14 = particle system,
 * 15 = text. */
void pbRenderNode(PBTREENODE* node)
{
    vec4 res;
    PBTREENODE* p;
    mat44* mat = &matrix_stack[matrix_level];
    u32* fp = (u32*)((char*)matrix_stack + 4096 + matrix_level * 4);
    u32 fl = *fp;
    PBWINGLOBALS* win = gWinGlobals;

    switch (node->type) {
    case 3:
        if (!(alpha_tree_dist >= 0.0)) {
            ErrorPrintf("ALPHATREE_NODE child of ALPHATREE_NODE");
        }
        vec4ApplyTrans(res, *(vec4*)((char*)mat + 48),
                       *(mat44*)(win->cur + 704));
        alpha_tree_dist = res.z;
        alpha_tree_dist_add = 0.0f;
        clear_alpha_dist = 1;
        break;

    case 2:
        if (!MBDrawObjectTest(node, mat, fl & 1)) {
            skiped_nodes++;
            break;
        }
        if (alpha_tree_dist >= 0.0) {
            f32 unused;
            MBSetupObject(node, mat, fl & 1, unused, alpha_tree_dist_add);
            alpha_tree_dist_add -= 1.0;
        } else {
            MBSetupObject(node, mat, fl & 1, 0.0f, 0.0f);
        }
        p = node->parent;
        if (p != 0) {
            if ((p->flags & 1) || p->type == 1) {
                *(u32*)((char*)matrix_stack + 4352 + ((p->id >> 5) * 4)) |=
                    1 << (p->id & 31);
            }
        }
        break;

    case 14:
        if (!MBDrawPsysTest(node, mat)) {
            skiped_nodes++;
            break;
        }
        if (draw_psys_on != 0) {
            MBTraversePsys(node, mat);
        }
        break;

    case 7:
        if (draw_sortobjs_on != 0) {
            MBDrawSortObjects();
        }
        break;

    case 8:
        if (draw_distobjs_on != 0) {
            MBDrawDistObjects();
        }
        break;

    case 9:
        if (draw_psys_on != 0) {
            MBDrawPsysObjects();
        }
        break;

    case 10:
        if (draw_polys_on != 0) {
            MBDrawPolyInsts(node);
        }
        break;

    case 13:
        if (draw_blits_on != 0) {
            MBDrawBlits(node);
        }
        break;

    case 15:
        if (draw_text_on != 0) {
            MBRenderText();
        }
        break;

    case 4:
        *fp &= ~1;
        break;
    }
}

/* 0x800C7EB0  CameraFace: overwrite the current stack matrix with a
 * camera-facing variant selected by the node's billboard mode bits
 * (flags & 0x0F000000).  Modes 1/3/5/6/7 = FaceCamMat with fixed tilts
 * (0, -1, PI/12, PI/6, PI/4), 2 = QuickYawMat, 8 = TopFaceMat, 4 = the
 * camfacemat default (with optional scale re-apply when the level flag
 * bit 2 is set). */
void CameraFace(u32 flags)
{
    vec3 tmp;
    PBWINGLOBALS* win = gWinGlobals;
    mat44* mat = &matrix_stack[matrix_level];
    u32 fl = *(u32*)((char*)node_flags + matrix_level * 4);

    switch (flags & 0x0F000000) {
    case 0x01000000:
        FaceCamMat(mat, 0.0f);
        break;
    case 0x08000000:
        TopFaceMat(mat);
        break;
    case 0x02000000:
        QuickYawMat(mat);
        break;
    case 0x03000000:
        FaceCamMat(mat, -1.0f);
        break;
    case 0x04000000:
        if (fl & 2) {
            fn_800BE030(mat, &tmp);
            fn_800BDF48(camfacemat, win->cur + 576, mat);
            fn_800BE79C(mat, mat, &tmp);
        } else {
            fn_800BDF48(camfacemat, win->cur + 576, mat);
        }
        break;
    case 0x05000000:
        FaceCamMat(mat, 0.2617993950843811f); /* PI/12 */
        break;
    case 0x06000000:
        FaceCamMat(mat, 0.5235987901687622f); /* PI/6 */
        break;
    case 0x07000000:
        FaceCamMat(mat, 0.7853981852531433f); /* PI/4 */
        break;
    }
}

} /* extern "C" */
