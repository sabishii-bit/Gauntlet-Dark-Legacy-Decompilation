/* inftrees.c -- generate Huffman trees for efficient decoding
 * Copyright (C) 1995-1996 Mark Adler
 * From zlib 1.0.4. Midway reordered the functions and reused the
 * literal/length error strings for the distance tree.
 */

#include "zlib/infprivate.h"

/* simplify the use of the inflate_huft type with some defines */
#define base more.Base
#define next more.Next
#define exop word.what.Exop
#define bits word.what.Bits

char inflate_copyright[] = " inflate 1.0.4 Copyright 1995-1996 Mark Adler ";
/*
  If you use the zlib library in a product, an acknowledgment is welcome
  in the documentation of your product. If for some reason you cannot
  include such an acknowledgment, I would appreciate that you keep this
  copyright string in the executable of your product.
 */

/* Tables for deflate from PKZIP's appnote.txt. */
static uInt cplens[31] = { /* Copy lengths for literal codes 257..285 */
    3,  4,  5,  6,  7,  8,  9,  10, 11,  13,  15,  17,  19,  23, 27, 31,
    35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258, 0,  0};
/* see note #13 above about 258 */
static uInt cplext[31] = { /* Extra bits for literal codes 257..285 */
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
    3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0, 128, 128}; /* 128==invalid */
static uInt cpdist[30] = { /* Copy offsets for distance codes 0..29 */
    1,   2,   3,   4,   5,    7,    9,    13,   17,   25,   33,   49,   65,   97,  129,
    193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
static uInt cpdext[30] = { /* Extra bits for distance codes */
    0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
    7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

/* Huffman code decoding is performed using a multi-level table lookup. */

#define BMAX 15   /* maximum bit length of any code (16 for explode) */
#define N_MAX 288 /* maximum number of codes in any set */

static int huft_build(uIntf* b, uInt n, uInt s, uIntf* d, uIntf* e, inflate_huft** t, uIntf* m,
                      z_streamp zs);
static voidpf falloc(voidpf q, uInt n, uInt s);

int inflate_trees_free(inflate_huft* t, z_streamp z)
{
    inflate_huft *p, *q, *r;

    /* Reverse linked list */
    p = Z_NULL;
    q = t;
    while (q != Z_NULL) {
        r = (q - 1)->next;
        (q - 1)->next = p;
        p = q;
        q = r;
    }
    /* Go through linked list, freeing from the malloced (t[-1]) address. */
    while (p != Z_NULL) {
        q = (p - 1)->next;
        ZFREE(z, --p);
        p = q;
    }
    return Z_OK;
}

/* build fixed tables only once--keep them here */
static int fixed_built = 0;
#define FIXEDH 530 /* number of hufts used by fixed tables */
static inflate_huft fixed_mem[FIXEDH];
static uInt fixed_bl;
static uInt fixed_bd;
static inflate_huft* fixed_tl;
static inflate_huft* fixed_td;

int inflate_trees_fixed(uIntf* bl, uIntf* bd, inflate_huft** tl, inflate_huft** td)
{
    /* build fixed tables if not already (multiple overlapped executions ok) */
    if (!fixed_built) {
        int k;           /* temporary variable */
        uInt c[288];     /* length list for huft_build */
        z_stream z;      /* for falloc function */
        int f = FIXEDH;  /* number of hufts left in fixed_mem */

        /* set up fake z_stream for memory routines */
        z.zalloc = falloc;
        z.zfree = Z_NULL;
        z.opaque = (voidpf) &f;

        /* literal table */
        for (k = 0; k < 144; k++) {
            c[k] = 8;
        }
        for (; k < 256; k++) {
            c[k] = 9;
        }
        for (; k < 280; k++) {
            c[k] = 7;
        }
        for (; k < 288; k++) {
            c[k] = 8;
        }
        fixed_bl = 7;
        huft_build(c, 288, 257, cplens, cplext, &fixed_tl, &fixed_bl, &z);

        /* distance table */
        for (k = 0; k < 30; k++) {
            c[k] = 5;
        }
        fixed_bd = 5;
        huft_build(c, 30, 0, cpdist, cpdext, &fixed_td, &fixed_bd, &z);

        /* done */
        fixed_built = 1;
    }
    *bl = fixed_bl;
    *bd = fixed_bd;
    *tl = fixed_tl;
    *td = fixed_td;
    return Z_OK;
}

/* allocate function for fixed tables */
static voidpf falloc(voidpf q, uInt n, uInt s)
{
    *(intf*) q -= n + s - s; /* s-s to avoid warning */
    return (voidpf) (fixed_mem + *(intf*) q);
}

int inflate_trees_dynamic(uInt nl, uInt nd, uIntf* c, uIntf* bl, uIntf* bd, inflate_huft** tl,
                          inflate_huft** td, z_streamp z)
{
    int r;

    /* build literal/length tree */
    if ((r = huft_build(c, nl, 257, cplens, cplext, tl, bl, z)) != Z_OK) {
        if (r == Z_DATA_ERROR) {
            z->msg = (char*) "oversubscribed literal/length tree";
        } else if (r == Z_BUF_ERROR) {
            inflate_trees_free(*tl, z);
            z->msg = (char*) "incomplete literal/length tree";
            r = Z_DATA_ERROR;
        }
        return r;
    }

    /* build distance tree */
    if ((r = huft_build(c + nl, nd, 0, cpdist, cpdext, td, bd, z)) != Z_OK) {
        if (r == Z_DATA_ERROR) {
            z->msg = (char*) "oversubscribed literal/length tree";
        } else if (r == Z_BUF_ERROR) {
            inflate_trees_free(*td, z);
            z->msg = (char*) "incomplete literal/length tree";
            r = Z_DATA_ERROR;
        }
        inflate_trees_free(*tl, z);
        return r;
    }

    /* done */
    return Z_OK;
}

int inflate_trees_bits(uIntf* c, uIntf* bb, inflate_huft** tb, z_streamp z)
{
    int r;

    r = huft_build(c, 19, 19, (uIntf*) Z_NULL, (uIntf*) Z_NULL, tb, bb, z);
    if (r == Z_DATA_ERROR) {
        z->msg = (char*) "oversubscribed dynamic bit lengths tree";
    } else if (r == Z_BUF_ERROR) {
        inflate_trees_free(*tb, z);
        z->msg = (char*) "incomplete dynamic bit lengths tree";
        r = Z_DATA_ERROR;
    }
    return r;
}

/* Given a list of code lengths and a maximum table size, make a set of
   tables to decode that set of codes.  Return Z_OK on success, Z_BUF_ERROR
   if the given code set is incomplete (the tables are still built in this
   case), Z_DATA_ERROR if the input is invalid (an over-subscribed set of
   lengths), or Z_MEM_ERROR if not enough memory. */
static int huft_build(uIntf* b, uInt n, uInt s, uIntf* d, uIntf* e, inflate_huft** t, uIntf* m,
                      z_streamp zs)
{
    uInt a;                 /* counter for codes of length k */
    uInt c[BMAX + 1];       /* bit length count table */
    uInt f;                 /* i repeats in table every f entries */
    int g;                  /* maximum code length */
    int h;                  /* table level */
    register uInt i;        /* counter, current code */
    register uInt j;        /* counter */
    register int k;         /* number of bits in current code */
    int l;                  /* bits per table (returned in m) */
    register uIntf* p;      /* pointer into c[], b[], or v[] */
    inflate_huft* q;        /* points to current table */
    struct inflate_huft_s r; /* table entry for structure assignment */
    inflate_huft* u[BMAX];  /* table stack */
    uInt v[N_MAX];          /* values in order of bit length */
    register int w;         /* bits before this table == (l * h) */
    uInt x[BMAX + 1];       /* bit offsets, then code stack */
    uIntf* xp;              /* pointer into x */
    int y;                  /* number of dummy codes added */
    uInt z;                 /* number of entries in current table */

    /* Generate counts for each bit length */
    p = c;
#define C0 *p++ = 0;
#define C2 C0 C0 C0 C0
#define C4 C2 C2 C2 C2
    C4 /* clear c[]--assume BMAX+1 is 16 */
    p = b;
    i = n;
    do {
        c[*p++]++; /* assume all entries <= BMAX */
    } while (--i);
    if (c[0] == n) { /* null input--all zero length codes */
        *t = (inflate_huft*) Z_NULL;
        *m = 0;
        return Z_OK;
    }

    /* Find minimum and maximum length, bound *m by those */
    l = *m;
    for (j = 1; j <= BMAX; j++) {
        if (c[j]) {
            break;
        }
    }
    k = j; /* minimum code length */
    if ((uInt) l < j) {
        l = j;
    }
    for (i = BMAX; i; i--) {
        if (c[i]) {
            break;
        }
    }
    g = i; /* maximum code length */
    if ((uInt) l > i) {
        l = i;
    }
    *m = l;

    /* Adjust last length count to fill out codes, if needed */
    for (y = 1 << j; j < i; j++, y <<= 1) {
        if ((y -= c[j]) < 0) {
            return Z_DATA_ERROR;
        }
    }
    if ((y -= c[i]) < 0) {
        return Z_DATA_ERROR;
    }
    c[i] += y;

    /* Generate starting offsets into the value table for each length */
    x[1] = j = 0;
    p = c + 1;
    xp = x + 2;
    while (--i) { /* note that i == g from above */
        *xp++ = (j += *p++);
    }

    /* Make a table of values in order of bit lengths */
    p = b;
    i = 0;
    do {
        if ((j = *p++) != 0) {
            v[x[j]++] = i;
        }
    } while (++i < n);

    /* Generate the Huffman codes and for each, make the table entries */
    x[0] = i = 0;                  /* first Huffman code is zero */
    p = v;                         /* grab values in bit order */
    h = -1;                        /* no tables yet--level -1 */
    w = -l;                        /* bits decoded == (l * h) */
    u[0] = (inflate_huft*) Z_NULL; /* just to keep compilers happy */
    q = (inflate_huft*) Z_NULL;    /* ditto */
    z = 0;                         /* ditto */

    /* go through the bit lengths (k already is bits in shortest code) */
    for (; k <= g; k++) {
        a = c[k];
        while (a--) {
            /* here i is the Huffman code of length k bits for value *p */
            /* make tables up to required level */
            while (k > w + l) {
                h++;
                w += l; /* previous table always l bits */

                /* compute minimum size table less than or equal to l bits */
                z = (z = g - w) > (uInt) l ? l : z; /* upper limit on table size */
                if ((f = 1 << (j = k - w)) > a + 1) { /* try a k-w bit table */
                    /* too few codes for k-w bit table */
                    f -= a + 1; /* deduct codes from patterns left */
                    xp = c + k;
                    if (j < z) {
                        while (++j < z) { /* try smaller tables up to z bits */
                            if ((f <<= 1) <= *++xp) {
                                break; /* enough codes to use up j bits */
                            }
                            f -= *xp; /* else deduct codes from patterns */
                        }
                    }
                }
                z = 1 << j; /* table entries for j-bit table */

                /* allocate and link in new table */
                if ((q = (inflate_huft*) ZALLOC(zs, z + 1, sizeof(inflate_huft))) == Z_NULL) {
                    if (h) {
                        inflate_trees_free(u[0], zs);
                    }
                    return Z_MEM_ERROR; /* not enough memory */
                }
                *t = q + 1; /* link to list for huft_free() */
                *(t = &(q->next)) = Z_NULL;
                u[h] = ++q; /* table starts after link */

                /* connect to last table, if there is one */
                if (h) {
                    x[h] = i;           /* save pattern for backing up */
                    r.bits = (Byte) l;  /* bits to dump before this table */
                    r.exop = (Byte) j;  /* bits in this table */
                    r.next = q;         /* pointer to this table */
                    j = i >> (w - l);   /* (get around Turbo C bug) */
                    u[h - 1][j] = r;    /* connect to last table */
                }
            }

            /* set up table entry in r */
            r.bits = (Byte) (k - w);
            if (p >= v + n) {
                r.exop = 128 + 64; /* out of values--invalid code */
            } else if (*p < s) {
                r.exop = (Byte) (*p < 256 ? 0 : 32 + 64); /* 256 is end-of-block code */
                r.base = *p++;                            /* simple code is just the value */
            } else {
                r.exop = (Byte) (e[*p - s] + 16 + 64); /* non-simple--look up in lists */
                r.base = d[*p++ - s];
            }

            /* fill code-like entries with r */
            f = 1 << (k - w);
            for (j = i >> w; j < z; j += f) {
                q[j] = r;
            }

            /* backwards increment the k-bit code i */
            for (j = 1 << (k - 1); i & j; j >>= 1) {
                i ^= j;
            }
            i ^= j;

            /* backup over finished tables */
            while ((i & ((1 << w) - 1)) != x[h]) {
                h--; /* don't need to update q */
                w -= l;
            }
        }
    }

    /* Return Z_BUF_ERROR if we were given an incomplete table */
    return y != 0 && g != 1 ? Z_BUF_ERROR : Z_OK;
}
