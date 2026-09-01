/*
 * mempool.c - DCS block-pool allocator + intrusive list (MEMPOOL.OBJ).
 *
 * Text 0x800D5260-0x800D621C.  A first-fit block pool carved out of an
 * AllocMem() arena, with an intrusive doubly-linked free/used list and a
 * garbage-collect (qsort-based coalesce).  All mutating ops run under an
 * semaphore/thread-owner locking through the PS2 compatibility shim and print
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
extern char lbl_80349300[8];

#define MEMPOOL_STRINGS                                                       \
    "MEMLOCK is saving your skin!\n\0\0\0"                                  \
    "DCSERROR: \0\0"                                                        \
    "No more free MemBlk's\n\0\0"                                           \
    "pool_free ran out of FREE blocks! increase pool_init() value\n\0\0\0"     \
    "pool_dispose_and_alloc IGNORING already-allocated block 0x%08x\n\0"     \
    "pool_alloc IGNORING already-allocated block 0x%08x\n\0"                 \
    "pool_alloc IGNORING 0-byte request\n\0"                                  \
    "pool_alloc failed %d bytes (%d biggest, %d total)\n\0\0"                \
    "WARNING: pool_new rounding block up to next power of 2: 0x%08x\n\0"     \
    "DISPOSE code %d unknown\n\0\0\0\0"                                     \
    "DCSFATAL: \0\0"                                                        \
    "LIST Null link\n\0"                                                     \
    "LIST Bad node 0x%08x <- 0x%08x\n\0"                                    \
    "LIST Bad node 0x%08x -> 0x%08x\n"

void mathStub1__Fv();
void* AllocMem(u32 size);
void* memset(void* dst, s32 value, u32 size);
int printf(const char* format, ...);
s32 GetThreadId(void);
void WaitSema(u32 sema);
void SignalSema(u32 sema);
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
    s32 alignmentShift;
} MemPoolLists;

extern MemListNode* lbl_8031EB00[];

void list_verify(MemList* list);
void list_insert_size(MemList* list, MemListNode* node);
void list_insert_tail(MemList* list, MemListNode* node);
void list_remove(MemList* list, MemListNode* node);

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
    owner = GetThreadId();
    if (owner != lbl_8034525C) {
        if (lbl_8034525C != 0) {
            printf(lbl_80349300);
            printf(MEMPOOL_STRINGS);
        }
        WaitSema(lbl_80345258);
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
        SignalSema(lbl_80345258);
        lbl_8034525C = 0;
        lbl_80345260 = 0;
    }
    return result;
}

/* 0x800D5390  coalesce free blocks (qsort) */
s32 pool_garbage_collect(MemPoolLists* pool,
                         s32 (*gapCallback)(MemListNode*, u32)) {
    s32 result;
    s32 count;
    MemListNode* node;
    s32 i;
    u32 currentEnd;
    MemListNode** entries = lbl_8031EB00;

    result = 1;
    count = 0;
    if ((node = pool->secondary.head) != NULL) {
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

/* 0x800D54A4  free a block (list_insert_tail) */
void pool_free(MemPoolLists* pool, MemListNode* node) {
    s32 owner;

    owner = GetThreadId();
    if (owner != lbl_8034525C) {
        if (lbl_8034525C != 0) {
            printf(lbl_80349300);
            printf(MEMPOOL_STRINGS);
        }
        WaitSema(lbl_80345258);
        lbl_8034525C = owner;
    }

    lbl_80345260++;
    list_verify(&pool->secondary);
    if (node == node->next) {
        pool->secondary.head = NULL;
    } else {
        if (pool->secondary.head == node) {
            pool->secondary.head = node->next;
        }
        node->next->prev = node->prev;
        node->prev->next = node->next;
    }
    list_verify(&pool->secondary);
    list_insert_tail(&pool->secondary, node);
    pool->secondary.head = node;

    if (--lbl_80345260 <= 0) {
        SignalSema(lbl_80345258);
        lbl_8034525C = 0;
        lbl_80345260 = 0;
    }
}

/* shared free-slot search, inlined by pool_alloc and pool_alloc_at */
static inline MemListNode* pool_new_block(void)
{
    MemListNode* node;
    s32 i;

    node = NULL;
    for (i = (s32)node; i < (s32)lbl_80345254; i++) {
        MemListNode* candidate = &((MemListNode*)lbl_80345250)[i];

        if (candidate->flags == 0) {
            node = candidate;
            break;
        }
    }
    if (node == NULL) {
        printf(MEMPOOL_STRINGS + 32);
        printf(MEMPOOL_STRINGS + 44);
    }
    return node;
}

static inline s32 pool_merge_adjacent(MemPoolLists* pool,
                                      MemListNode* candidate,
                                      MemListNode* node, s32 merged)
{
    if ((void*)(candidate->flags + candidate->key) == (void*)node->flags) {
        node->flags = candidate->flags;
        node->key += candidate->key;
        list_remove(&pool->primary, candidate);
        merged = 1;
    } else {
        if ((void*)(node->flags + node->key) == (void*)candidate->flags) {
            node->key += candidate->key;
            list_remove(&pool->primary, candidate);
            merged = 1;
        }
    }

    return merged;
}

/* 0x800D55A8  first-fit allocate (list_remove) */
MemListNode* pool_alloc(MemPoolLists* pool, MemListNode* node) {
    MemListNode* result;
    MemListNode* candidate;
    MemListNode* scan;
    s32 merged;
    s32 owner;
    s32 remaining;

    result = NULL;
    if (node->flags == 0) {
        return NULL;
    }

    owner = GetThreadId();
    if (owner != lbl_8034525C) {
        if (lbl_8034525C != 0) {
            printf(lbl_80349300);
            printf(MEMPOOL_STRINGS);
        }
        WaitSema(lbl_80345258);
        lbl_8034525C = owner;
    }
    lbl_80345260++;

    scan = pool->primary.head != NULL ? pool->primary.head->prev : NULL;
    remaining = 2;
    if (scan != NULL) {
        do {
            candidate = scan;
            if (scan != pool->primary.head) {
                scan = scan->prev;
            }
            merged = pool_merge_adjacent(pool, candidate, node, 0);

            if (merged != 0) {
                if (result == NULL) {
                    result = candidate;
                } else {
                    candidate->flags = 0;
                }
                remaining--;
            }
        } while (remaining != 0 && candidate != scan);
    }

    if (result == NULL) {
        result = pool_new_block();
    }

    if (result != NULL) {
        result->flags = node->flags;
        result->key = node->key;
        list_insert_size(&pool->primary, result);
        node->flags = 0;
        node->key = 0;
        list_verify(&pool->secondary);
        if (node == node->next) {
            pool->secondary.head = NULL;
        } else {
            if (pool->secondary.head == node) {
                pool->secondary.head = node->next;
            }
            node->next->prev = node->prev;
            node->prev->next = node->next;
        }
        list_verify(&pool->secondary);
    } else {
        printf(MEMPOOL_STRINGS + 32);
        printf(MEMPOOL_STRINGS + 68);
    }

    if (--lbl_80345260 <= 0) {
        SignalSema(lbl_80345258);
        lbl_8034525C = 0;
        lbl_80345260 = 0;
    }
    return result;
}

/* 0x800D5848  allocate at a fixed address */
s32 pool_alloc_at(MemPoolLists* pool, MemListNode* node, s32 size,
                  u32 address) {
    u32 endAddress;
    s32 result;
    u32 alignedSize;
    u32 totalSize;
    u32 lastSize;
    MemListNode* freeNode;
    MemListNode* head;
    u32 alignmentMask;
    MemListNode* remainderNode;
    s32 remainderSize;
    u32 blockSize;
    s32 owner;

    result = 0;
    totalSize = 0;
    lastSize = 0;
    if (node->flags != 0) {
        printf(MEMPOOL_STRINGS + 32);
        printf(MEMPOOL_STRINGS + 196, node->flags);
        return 0;
    }
    if (size == 0) {
        printf(MEMPOOL_STRINGS + 32);
        printf(MEMPOOL_STRINGS + 248);
        return 0;
    }

    owner = GetThreadId();
    if (owner != lbl_8034525C) {
        if (lbl_8034525C != 0) {
            printf(lbl_80349300);
            printf(MEMPOOL_STRINGS);
        }
        WaitSema(lbl_80345258);
        lbl_8034525C = owner;
    }
    lbl_80345260++;

    alignmentMask = (1 << pool->alignmentShift) - 1;
    alignedSize = (size + alignmentMask) & ~alignmentMask;
    head = pool->primary.head;
    freeNode = head;
    if (head != NULL) {
        endAddress = address + alignedSize;
        do {
            if (freeNode->flags <= address &&
                freeNode->flags + freeNode->key > endAddress) {
                u32 blockSize;

                node->key = alignedSize;
                node->flags = address;
                list_insert_tail(&pool->secondary, node);
                pool->secondary.head = node;

                list_verify(&pool->primary);
                if (freeNode == freeNode->next) {
                    pool->primary.head = NULL;
                } else {
                    if (pool->primary.head == freeNode) {
                        pool->primary.head = freeNode->next;
                    }
                    freeNode->next->prev = freeNode->prev;
                    freeNode->prev->next = freeNode->next;
                }
                list_verify(&pool->primary);

                blockSize = freeNode->key;
                if (blockSize == alignedSize) {
                    freeNode->flags = 0;
                } else if (freeNode->flags == address) {
                    freeNode->flags += alignedSize;
                    freeNode->key -= alignedSize;
                    list_insert_size(&pool->primary, freeNode);
                } else {
                    freeNode->key = address - freeNode->flags;
                    remainderSize = blockSize - alignedSize;
                    remainderSize -= freeNode->key;
                    list_insert_size(&pool->primary, freeNode);
                    if (remainderSize != 0) {
                        remainderNode = pool_new_block();
                        if (remainderNode == NULL) {
                            break;
                        }
                        remainderNode->flags = endAddress;
                        remainderNode->key = remainderSize;
                    }
                }
                result = alignedSize;
                break;
            }
            blockSize = freeNode->key;
            freeNode = freeNode->next;
            lastSize = blockSize;
            totalSize += blockSize;
        } while (freeNode != head);
    }

    if (result == 0) {
        printf(MEMPOOL_STRINGS + 32);
        printf(MEMPOOL_STRINGS + 284, alignedSize, lastSize, totalSize);
    }

    if (--lbl_80345260 <= 0) {
        SignalSema(lbl_80345258);
        lbl_8034525C = 0;
        lbl_80345260 = 0;
    }
    return result;
}

/* 0x800D5B38  dispose then reallocate */
s32 pool_dispose_and_alloc(MemPoolLists* pool, MemListNode* node, s32 size) {
    s32 owner;
    MemListNode* head;
    u32 alignmentMask;
    u32 alignedSize;
    u32 blockSize;
    s32 result;
    u32 totalSize;
    u32 lastSize;
    MemListNode* freeNode;

    result = 0;
    totalSize = 0;
    lastSize = 0;
    if (node->flags != 0) {
        printf(MEMPOOL_STRINGS + 32);
        printf(MEMPOOL_STRINGS + 196, node->flags);
        return 0;
    }
    if (size == 0) {
        printf(MEMPOOL_STRINGS + 32);
        printf(MEMPOOL_STRINGS + 248);
        return 0;
    }

    owner = GetThreadId();
    if (owner != lbl_8034525C) {
        if (lbl_8034525C != 0) {
            printf(lbl_80349300);
            printf(MEMPOOL_STRINGS);
        }
        WaitSema(lbl_80345258);
        lbl_8034525C = owner;
    }
    lbl_80345260++;

    alignmentMask = (1 << pool->alignmentShift) - 1;
    alignedSize = (size + alignmentMask) & ~alignmentMask;
    head = pool->primary.head;
    freeNode = head;
    if (head != NULL) {
        do {
            blockSize = freeNode->key;
            if (blockSize >= alignedSize) {
                node->key = alignedSize;
                node->flags = freeNode->flags;
                list_remove(&pool->primary, freeNode);
                if (freeNode->key > alignedSize) {
                    freeNode->flags += alignedSize;
                    freeNode->key -= alignedSize;
                    list_insert_size(&pool->primary, freeNode);
                } else {
                    freeNode->flags = 0;
                }
                list_insert_tail(&pool->secondary, node);
                pool->secondary.head = node;
                result = alignedSize;
                break;
            }
            freeNode = freeNode->next;
            lastSize = blockSize;
            totalSize += blockSize;
        } while (freeNode != head);
    }

    if (result == 0) {
        printf(MEMPOOL_STRINGS + 32);
        printf(MEMPOOL_STRINGS + 284, alignedSize, lastSize, totalSize);
    }

    if (--lbl_80345260 <= 0) {
        SignalSema(lbl_80345258);
        lbl_8034525C = 0;
        lbl_80345260 = 0;
    }
    return result;
}

/* 0x800D5D2C  dispose a block */
s32 pool_dispose(MemPoolLists* pool, u32 address, u32 size,
                 s32 alignment) {
    MemListNode* node;
    MemListNode* next;
    s32 owner;
    s32 result;
    MemListNode* freeNode;

    result = 0;
    owner = GetThreadId();
    if (owner != lbl_8034525C) {
        if (lbl_8034525C != 0) {
            printf(lbl_80349300);
            printf(MEMPOOL_STRINGS);
        }
        WaitSema(lbl_80345258);
        lbl_8034525C = owner;
    }
    lbl_80345260++;

    node = pool->secondary.head;
    if (node != NULL) {
        do {
            next = node->next;
            node->next = NULL;
            node->prev = NULL;
            node->flags = 0;
            node = next;
            if (node == NULL) {
                break;
            }
        } while (next != pool->secondary.head);
    }
    freeNode = NULL;
    pool->secondary.head = freeNode;

    node = pool->primary.head;
    if (node != NULL) {
        next = node->next;
        while (next != NULL && next != pool->primary.head) {
            node->flags = (u32)freeNode;
            node = next;
            next = next->next;
        }
    } else {
        freeNode = pool_new_block();
        node = freeNode;
    }

    if (node == NULL) {
        pool->primary.head = NULL;
    } else {
        node->flags = address;
        node->key = size;
        node->next = node;
        node->prev = node;
        pool->primary.head = node;
        pool->alignmentShift = 1;
        while ((1 << pool->alignmentShift) < alignment) {
            pool->alignmentShift++;
        }
        if ((1 << pool->alignmentShift) > alignment) {
            printf(lbl_80349300);
            printf(MEMPOOL_STRINGS + 336, 1 << pool->alignmentShift);
        }
        result = 1;
    }

    if (--lbl_80345260 <= 0) {
        SignalSema(lbl_80345258);
        lbl_8034525C = 0;
        lbl_80345260 = 0;
    }
    return result;
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
#pragma opt_common_subs off
void list_insert_size(MemList* list, MemListNode* node) {
    MemListNode** link;

    link = &list->head;
    list_verify(list);
    {
        MemListNode* current;
        MemListNode* head;

        head = list->head;
        if (head == NULL) {
            node->prev = node;
            node->next = node;
            list->head = node;
        } else {
            while (node->key > (current = *link)->key) {
                link = &current->next;
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
#pragma opt_common_subs reset

/* 0x800D603C  insert a node at the tail (verified) */
void list_insert_tail(MemList* list, MemListNode* node) {
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

/* 0x800D60B8  unlink a node (verified) */
void list_remove(MemList* list, MemListNode* node) {
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

    strings = MEMPOOL_STRINGS;
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

/* 0x800D621C  qsort comparator for pool_garbage_collect: order block
 * entries by start offset (flags word). Final fn of MEMPOOL.OBJ. */
s32 pool_query(const void* lhs, const void* rhs) {
    return (s32)((*(MemListNode**)lhs)->flags - (*(MemListNode**)rhs)->flags);
}
