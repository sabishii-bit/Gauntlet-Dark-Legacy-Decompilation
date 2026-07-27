/*
 * mempool.c - DCS block-pool allocator + intrusive list (MEMPOOL.OBJ).
 *
 * Text 0x800D5260-0x800D621C.  A first-fit block pool carved out of an
 * AllocMem() arena, with an intrusive doubly-linked free/used list and a
 * garbage-collect (qsort-based coalesce).  All mutating ops run under an
 * interrupt-lock (OSDisableInterrupts via fn_800AF1D0/B8/C0) and print
 * "MEMLOCK is saving your skin!" if entered unlocked.  Debug: "pool_alloc
 * failed %d bytes", "No more free MemBlk's", "LIST Bad node".
 *
 * The pool_alloc/pool_free/pool_dispose family names are behavioural (correct
 * module+operation family; exact 1:1 identity vs the arcade source not yet
 * pinned).  The dcsMem* lock stubs (return 1) are the GC no-op memlock API.
 *
 * NonMatching: reconstruction scaffold.
 */
#include "types.h"

extern s32 lbl_80345248;
extern u32 lbl_8031EAF0[];
extern void* lbl_80345250;
extern u32 lbl_80345254;
extern u32 lbl_80345258;
extern volatile s32 lbl_8034525C;
extern s32 lbl_80345260;
extern char lbl_801172A0[];
extern char lbl_80349300[8];

void mathStub1__Fv();
void* AllocMem(u32 size);
void* memset(void* dst, s32 value, u32 size);
int printf(const char* format, ...);
s32 fn_800AF1D0(void);
void fn_800AF1B8(u32 state);
void fn_800AF1C0(u32 state);
void qsort(void* base, u32 count, u32 width,
           s32 (*compare)(const void*, const void*));
s32 pool_query(const void* lhs, const void* rhs);

typedef struct MemListNode {
    u32 flags;
    u32 key;
    struct MemListNode* prev;
    struct MemListNode* next;
} MemListNode;

typedef struct MemList {
    MemListNode* head;
} MemList;

typedef struct MemPoolLists {
    MemList primary;
    MemList secondary;
} MemPoolLists;

extern MemListNode* lbl_8031EB00[];

void list_verify(MemList* list);

/* 0x800D5260  no-op hook called on list-verify failure */
#pragma dont_inline on
void listVerifyHook(void) {
}
#pragma dont_inline off

/* 0x800D5264  memlock acquire stub (returns 1) */
s32 dcsMemLock(void) {
    return 1;
}

/* 0x800D526C  memlock release stub (returns 1) */
s32 dcsMemUnlock(void) {
    return 1;
}

/* 0x800D5274  memlock try stub (returns 1) */
s32 dcsMemTryLock(void) {
    return 1;
}

/* 0x800D527C  return current lock owner */
s32 dcsMemLockOwner(void) {
    return lbl_80345248;
}

/* 0x800D5284  tag a lock table slot + timestamp */
void dcsMemLockTag(s32 slot, u32 tag) {
    lbl_8031EAF0[slot + 2] |= tag;
    mathStub1__Fv(slot | 0x1600);
}

/* 0x800D52C4  create/register a pool block */
u32 pool_new(MemList* list) {
    s32 owner;
    u32 result;
    MemListNode* node;

    result = 0;
    owner = fn_800AF1D0();
    if (owner != lbl_8034525C) {
        if (lbl_8034525C != 0) {
            printf(lbl_80349300);
            printf(lbl_801172A0);
        }
        fn_800AF1B8(lbl_80345258);
        lbl_8034525C = owner;
    }

    lbl_80345260++;
    if (list->head != NULL) {
        node = list->head->prev;
    } else {
        node = NULL;
    }
    if (node != NULL) {
        result = node->key;
    }

    if (--lbl_80345260 <= 0) {
        fn_800AF1C0(lbl_80345258);
        lbl_8034525C = 0;
        lbl_80345260 = 0;
    }
    return result;
}

/* 0x800D5390  coalesce free blocks (qsort) */
s32 pool_garbage_collect(MemPoolLists* pool,
                         s32 (*gapCallback)(MemListNode*, u32)) {
    MemListNode* node;
    MemListNode** entries = lbl_8031EB00;
    s32 count;
    u32 currentEnd;
    s32 i;
    s32 result;

    result = 1;
    count = 0;
    node = pool->secondary.head;
    if (node != NULL) {
        do {
            entries[count++] = node;
            node = node->next;
        } while (node != pool->secondary.head);
    }

    qsort(entries, count, sizeof(MemListNode*), pool_query);

    currentEnd = entries[0]->flags;
    node = pool->primary.head;
    if (node != NULL) {
        do {
            if (currentEnd > node->flags) {
                currentEnd = node->flags;
            }
            node = node->next;
        } while (node != pool->primary.head);
    }

    for (i = 0; i < count; i++) {
        if (entries[i]->flags > currentEnd &&
            gapCallback(entries[i], currentEnd) != 0) {
            result = 0;
            break;
        }
        currentEnd += entries[i]->key;
    }
    return result;
}

/* 0x800D54A4  free a block (list_remove) */
void pool_free(void) {
}

/* 0x800D55A8  first-fit allocate (list_append) */
void pool_alloc(void) {
}

/* 0x800D5848  allocate at a fixed address */
void pool_alloc_at(void) {
}

/* 0x800D5B38  dispose then reallocate */
void pool_dispose_and_alloc(void) {
}

/* 0x800D5D2C  dispose a block */
void pool_dispose(void) {
}

/* 0x800D5F38  carve arena from AllocMem, clear */
u32 pool_init(u32 size) {
    u32 alignedSize;

    alignedSize = size - (size & 0xF);
    lbl_80345250 = AllocMem(alignedSize);
    memset(lbl_80345250, 0, alignedSize);
    lbl_80345254 = alignedSize >> 4;
    lbl_80345258 = 0;
    return alignedSize;
}

/* 0x800D5F94  insert a node (verified) */
void list_insert(MemList* list, MemListNode* node) {
    MemListNode** link;

    link = &list->head;
    list_verify(list);
    {
        MemListNode* head;

        head = list->head;
        if (head == NULL) {
            node->prev = node;
            node->next = node;
            list->head = node;
        } else {
            while (node->key > (*link)->key) {
                link = &(*link)->next;
                if (*link == head) {
                    break;
                }
            }
            node->next = *link;
            node->prev = (*link)->prev;
            node->next->prev = node;
            node->prev->next = node;
            *link = node;
        }
    }
    list_verify(list);
}

/* 0x800D603C  remove a node (verified) */
void list_remove(MemList* list, MemListNode* node) {
    list_verify(list);
    if (list->head != NULL) {
        node->next = list->head;
        node->prev = list->head->prev;
        node->next->prev = node;
        node->prev->next = node;
        list_verify(list);
    } else {
        node->prev = node;
        node->next = node;
        list->head = node;
        list_verify(list);
    }
}

/* 0x800D60B8  append a node (verified) */
void list_append(MemList* list, MemListNode* node) {
    list_verify(list);
    if (node == node->next) {
        list->head = NULL;
    } else {
        if (list->head == node) {
            list->head = node->next;
        }
        node->next->prev = node->prev;
        node->prev->next = node->next;
    }
    list_verify(list);
}

/* 0x800D6130  validate a node ("LIST Bad node") */
void list_verify(MemList* list) {
    char* strings;
    MemListNode* node;

    strings = lbl_801172A0;
    node = list->head;
    if (node != NULL) {
        do {
            if (node->next == NULL || node->prev == NULL) {
                printf(strings + 428);
                printf(strings + 440);
                listVerifyHook();
            }
            if (node != node->prev->next) {
                printf(strings + 428);
                printf(strings + 456, node, node->prev);
                listVerifyHook();
            }
            if (node != node->next->prev) {
                printf(strings + 428);
                printf(strings + 488, node, node->next);
                listVerifyHook();
            }
            node = node->next;
        } while (node != list->head);
    }
}
