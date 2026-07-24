/* Internal inflate structures and macros, condensed from zlib 1.0.4's
 * zutil.h, inftrees.h, infblock.h, infcodes.h and infutil.h.
 */

#ifndef _INFPRIVATE_H
#define _INFPRIVATE_H

#include "zlib/zlib.h"

#define ZALLOC(strm, items, size) (*((strm)->zalloc))((strm)->opaque, (items), (size))
#define ZFREE(strm, addr) (*((strm)->zfree))((strm)->opaque, (voidpf) (addr))

#define zmemcpy memcpy
void* memcpy(void* dst, const void* src, u32 n);

typedef uLong (*check_func)(uLong check, const Bytef* buf, uInt len);

/* ---------- inftrees.h ---------- */

typedef struct inflate_huft_s inflate_huft;

struct inflate_huft_s {
    union {
        struct {
            Byte Exop;  /* number of extra bits or operation */
            Byte Bits;  /* number of bits in this code or subcode */
        } what;
        Bytef* pad;     /* pad structure to a power of 2 */
    } word;
    union {
        uInt Base;           /* literal, length base, or distance base */
        inflate_huft* Next;  /* pointer to next level of table */
    } more;
};

int inflate_trees_bits(uIntf* c, uIntf* bb, inflate_huft** tb, z_streamp z);
int inflate_trees_dynamic(uInt nl, uInt nd, uIntf* c, uIntf* bl, uIntf* bd,
                          inflate_huft** tl, inflate_huft** td, z_streamp z);
int inflate_trees_fixed(uIntf* bl, uIntf* bd, inflate_huft** tl, inflate_huft** td);
int inflate_trees_free(inflate_huft* t, z_streamp z);

/* ---------- infcodes.h ---------- */

struct inflate_codes_state;
typedef struct inflate_codes_state inflate_codes_statef;

struct inflate_blocks_state;
typedef struct inflate_blocks_state inflate_blocks_statef;

inflate_codes_statef* inflate_codes_new(uInt bl, uInt bd, inflate_huft* tl, inflate_huft* td,
                                        z_streamp z);
int inflate_codes(inflate_blocks_statef* s, z_streamp z, int r);
void inflate_codes_free(inflate_codes_statef* c, z_streamp z);

/* ---------- infblock.h ---------- */

inflate_blocks_statef* inflate_blocks_new(z_streamp z, check_func c, uInt w);
int inflate_blocks(inflate_blocks_statef* s, z_streamp z, int r);
uLong inflate_blocks_reset(inflate_blocks_statef* s, z_streamp z, uLongf* c);
int inflate_blocks_free(inflate_blocks_statef* s, z_streamp z, uLongf* c);

/* ---------- inffast.h ---------- */

int inflate_fast(uInt bl, uInt bd, inflate_huft* tl, inflate_huft* td,
                 inflate_blocks_statef* s, z_streamp z);

/* ---------- infutil.h ---------- */

typedef enum {
    TYPE,   /* get type bits (3, including end bit) */
    LENS,   /* get lengths for stored */
    STORED, /* processing stored block */
    TABLE,  /* get table lengths */
    BTREE,  /* get bit lengths tree for a dynamic block */
    DTREE,  /* get length, distance trees for a dynamic block */
    CODES,  /* processing fixed or dynamic block */
    DRY,    /* output remaining window bytes */
    DONEB,  /* finished last block, done */
    BADB    /* got a data error--stuck here */
} inflate_block_mode;

struct inflate_blocks_state {
    /* mode */
    inflate_block_mode mode; /* current inflate_block mode */

    /* mode dependent information */
    union {
        uInt left; /* if STORED, bytes left to copy */
        struct {
            uInt table;         /* table lengths (14 bits) */
            uInt index;         /* index into blens (or border) */
            uIntf* blens;       /* bit lengths of codes */
            uInt bb;            /* bit length tree depth */
            inflate_huft* tb;   /* bit length decoding tree */
        } trees; /* if DTREE, decoding info for trees */
        struct {
            inflate_huft* tl;
            inflate_huft* td; /* trees to free */
            inflate_codes_statef* codes;
        } decode; /* if CODES, current state */
    } sub;        /* submode */
    uInt last;    /* true if this block is the last block */

    /* mode independent information */
    uInt bitk;          /* bits in bit buffer */
    uLong bitb;         /* bit buffer */
    Bytef* window;      /* sliding window */
    Bytef* end;         /* one byte after sliding window */
    Bytef* read;        /* window read pointer */
    Bytef* write;       /* window write pointer */
    check_func checkfn; /* check function */
    uLong check;        /* check on output */
};

/* defines for inflate input/output */
/*   update pointers and return */
#define UPDBITS      \
    {                \
        s->bitb = b; \
        s->bitk = k; \
    }
#define UPDIN                            \
    {                                    \
        z->avail_in = n;                 \
        z->total_in += p - z->next_in;   \
        z->next_in = p;                  \
    }
#define UPDOUT        \
    {                 \
        s->write = q; \
    }
#define UPDATE \
    {          \
        UPDBITS UPDIN UPDOUT \
    }
#define LEAVE                            \
    {                                    \
        UPDATE return inflate_flush(s, z, r); \
    }
/*   get bytes and bits */
#define LOADIN           \
    {                    \
        p = z->next_in;  \
        n = z->avail_in; \
        b = s->bitb;     \
        k = s->bitk;     \
    }
#define NEEDBYTE     \
    {                \
        if (n)       \
            r = Z_OK; \
        else         \
            LEAVE    \
    }
#define NEXTBYTE (n--, *p++)
#define NEEDBITS(j)                          \
    {                                        \
        while (k < (j)) {                    \
            NEEDBYTE;                        \
            b |= ((uLong) NEXTBYTE) << k;    \
            k += 8;                          \
        }                                    \
    }
#define DUMPBITS(j) \
    {               \
        b >>= (j);  \
        k -= (j);   \
    }
/*   output bytes */
#define WAVAIL (uInt) (q < s->read ? s->read - q - 1 : s->end - q)
#define LOADOUT            \
    {                      \
        q = s->write;      \
        m = (uInt) WAVAIL; \
    }
#define WRAP                                     \
    {                                            \
        if (q == s->end && s->read != s->window) { \
            q = s->window;                       \
            m = (uInt) WAVAIL;                   \
        }                                        \
    }
#define FLUSH                          \
    {                                  \
        UPDOUT r = inflate_flush(s, z, r); \
        LOADOUT                        \
    }
#define NEEDOUT                  \
    {                            \
        if (m == 0) {            \
            WRAP if (m == 0)     \
            {                    \
                FLUSH WRAP if (m == 0) LEAVE \
            }                    \
        }                        \
        r = Z_OK;                \
    }
#define OUTBYTE(a)          \
    {                       \
        *q++ = (Byte) (a);  \
        m--;                \
    }
/*   load local pointers */
#define LOAD \
    {        \
        LOADIN LOADOUT \
    }

/* masks for lower bits */
extern uInt inflate_mask[17];

/* copy as much as possible from the sliding window to the output area */
int inflate_flush(inflate_blocks_statef* s, z_streamp z, int r);

#endif /* _INFPRIVATE_H */
