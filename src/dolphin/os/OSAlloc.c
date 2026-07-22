#include "types.h"

void OSReport(const char* msg, ...);

#define ALIGNMENT 32
#define HEADERSIZE 32u
#define MINOBJSIZE 64u

#define OFFSET(n, a) (((u32)(n)) & ((a)-1))

#define InRange(cell, arenaStart, arenaEnd) \
    ((u32)arenaStart <= (u32)cell) && ((u32)cell < (u32)arenaEnd)

struct Cell {
    /* 0x0 */ struct Cell* prev;
    /* 0x4 */ struct Cell* next;
    /* 0x8 */ long size;
};

struct HeapDesc {
    /* 0x0 */ long size;
    /* 0x4 */ struct Cell* free;
    /* 0x8 */ struct Cell* allocated;
};

volatile int __OSCurrHeap = -1;

static struct HeapDesc* HeapArray;
static int NumHeaps;
static void* ArenaStart;
static void* ArenaEnd;

static struct Cell* DLAddFront(struct Cell* list, struct Cell* cell);
static struct Cell* DLLookup(struct Cell* list, struct Cell* cell);
static struct Cell* DLExtract(struct Cell* list, struct Cell* cell);
static struct Cell* DLInsert(struct Cell* list, struct Cell* cell);
static int DLOverlap(struct Cell* list, void* start, void* end);
static long DLSize(struct Cell* list);

static struct Cell* DLAddFront(struct Cell* list, struct Cell* cell) {
    cell->next = list;
    cell->prev = 0;
    if (list) {
        list->prev = cell;
    }
    return cell;
}

static struct Cell* DLLookup(struct Cell* list, struct Cell* cell) {
    for (; list; list = list->next) {
        if (list == cell) {
            return list;
        }
    }
    return NULL;
}

static struct Cell* DLExtract(struct Cell* list, struct Cell* cell) {
    if (cell->next) {
        cell->next->prev = cell->prev;
    }
    if (cell->prev == NULL) {
        return cell->next;
    }
    cell->prev->next = cell->next;
    return list;
}

#pragma dont_inline on
static struct Cell* DLInsert(struct Cell* list, struct Cell* cell) {
    struct Cell* prev;
    struct Cell* next;

    for (next = list, prev = NULL; next != 0; prev = next, next = next->next) {
        if (cell <= next) {
            break;
        }
    }

    cell->next = next;
    cell->prev = prev;
    if (next) {
        next->prev = cell;
        if ((u8*)cell + cell->size == (u8*)next) {
            cell->size += next->size;
            next = next->next;
            cell->next = next;
            if (next) {
                next->prev = cell;
            }
        }
    }
    if (prev) {
        prev->next = cell;
        if ((u8*)prev + prev->size == (u8*)cell) {
            prev->size += cell->size;
            prev->next = next;
            if (next) {
                next->prev = prev;
            }
        }
        return list;
    }
    return cell;
}
#pragma dont_inline off

static int DLOverlap(struct Cell* list, void* start, void* end) {
    struct Cell* cell = list;

    while (cell) {
        if (((start <= (void*)cell) && ((void*)cell < end)) ||
            ((start < (void*)((u8*)cell + cell->size)) &&
             ((void*)((u8*)cell + cell->size) <= end))) {
            return 1;
        }
        cell = cell->next;
    }
    return 0;
}

static long DLSize(struct Cell* list) {
    struct Cell* cell;
    long size;

    size = 0;
    cell = list;

    while (cell) {
        size += cell->size;
        cell = cell->next;
    }

    return size;
}

void* OSAllocFromHeap(int heap, unsigned long size) {
    struct HeapDesc* hd;
    struct Cell* cell;
    struct Cell* newCell;
    long leftoverSize;

    hd = &HeapArray[heap];
    size += 0x20;
    size = (size + 0x1F) & 0xFFFFFFE0;

    for (cell = hd->free; cell != NULL; cell = cell->next) {
        if ((signed)size <= (signed)cell->size) {
            break;
        }
    }

    if (cell == NULL) {
        return NULL;
    }

    leftoverSize = cell->size - size;
    if (leftoverSize < 0x40U) {
        hd->free = DLExtract(hd->free, cell);
    } else {
        cell->size = size;
        newCell = (void*)((u8*)cell + size);
        newCell->size = leftoverSize;
        newCell->prev = cell->prev;
        newCell->next = cell->next;
        if (newCell->next != NULL) {
            newCell->next->prev = newCell;
        }
        if (newCell->prev != NULL) {
            newCell->prev->next = newCell;
        } else {
            hd->free = newCell;
        }
    }

    hd->allocated = DLAddFront(hd->allocated, cell);
    return (u8*)cell + 0x20;
}

void* OSAllocFixed(void* rstart, void* rend) {
    int i;
    struct Cell* cell;
    struct Cell* newCell;
    struct HeapDesc* hd;
    void* start;
    void* end;
    void* cellEnd;

    start = (void*)((*(u32*)rstart) & ~((32) - 1));
    end = (void*)((*(u32*)rend + 0x1FU) & ~((32) - 1));

    for (i = 0; i < NumHeaps; i++) {
        hd = &HeapArray[i];
        if (hd->size >= 0) {
            if (DLOverlap(hd->allocated, start, end)) {
                return NULL;
            }
        }
    }

    for (i = 0; i < NumHeaps; i++) {
        hd = &HeapArray[i];
        if (hd->size >= 0) {
            for (cell = hd->free; cell; cell = cell->next) {
                cellEnd = ((u8*)cell + cell->size);
                if (cellEnd > start) {
                    if (end <= (void*)cell) {
                        break;
                    }
                    if ((char*)start - 0x20 <= (char*)cell && (void*)cell < end &&
                        (start <= cellEnd) && (cellEnd < (void*)((char*)end + 0x40))) {
                        if ((void*)cell < start) {
                            start = cell;
                        }
                        if (end < cellEnd) {
                            end = cellEnd;
                        }
                        hd->free = DLExtract(hd->free, cell);
                        hd->size -= cell->size;
                    } else if ((char*)start - 0x20 <= (char*)cell && (void*)cell < end) {
                        if ((void*)cell < start) {
                            start = cell;
                        }
                        newCell = (struct Cell*)end;

                        newCell->size = (s32)((char*)cellEnd - (char*)end);
                        newCell->next = cell->next;
                        if (newCell->next) {
                            newCell->next->prev = newCell;
                        }
                        newCell->prev = cell->prev;
                        if (newCell->prev) {
                            newCell->prev->next = newCell;
                        } else {
                            hd->free = newCell;
                        }
                        hd->size -= ((char*)end - (char*)cell);
                        break;
                    } else {
                        if ((start <= cellEnd) && (cellEnd < (void*)((char*)end + 0x40U))) {
                            if (end < cellEnd) {
                                end = cellEnd;
                            }
                            hd->size -= ((char*)cellEnd - (char*)start);
                            cell->size = ((char*)start - (char*)cell);
                        } else {
                            newCell = (struct Cell*)end;
                            newCell->size = ((char*)cellEnd - (char*)end);
                            newCell->next = cell->next;
                            if (newCell->next) {
                                newCell->next->prev = newCell;
                            }
                            newCell->prev = cell;
                            cell->next = newCell;
                            cell->size = ((char*)start - (char*)cell);
                            hd->size -= ((char*)end - (char*)start);
                            break;
                        }
                    }
                }
            }
        }
    }
    *(u32*)rstart = (u32)start;
    *(u32*)rend = (u32)end;
    return (void*)*(u32*)rstart;
}

void OSFreeToHeap(int heap, void* ptr) {
    struct HeapDesc* hd;
    struct Cell* cell;

    cell = (void*)((u32)ptr - 0x20);
    hd = &HeapArray[heap];

    hd->allocated = DLExtract(hd->allocated, cell);
    hd->free = DLInsert(hd->free, cell);
}

int OSSetCurrentHeap(int heap) {
    int prev;

    prev = __OSCurrHeap;
    __OSCurrHeap = heap;
    return prev;
}

void* OSInitAlloc(void* arenaStart, void* arenaEnd, int maxHeaps) {
    unsigned long arraySize;
    int i;
    struct HeapDesc* hd;

    arraySize = maxHeaps * sizeof(struct HeapDesc);
    HeapArray = arenaStart;
    NumHeaps = maxHeaps;

    for (i = 0; i < NumHeaps; i++) {
        hd = &HeapArray[i];
        hd->size = -1;
        hd->free = hd->allocated = 0;
    }
    __OSCurrHeap = -1;
    arenaStart = (void*)((u32)((char*)HeapArray + arraySize));
    arenaStart = (void*)(((u32)arenaStart + 0x1F) & 0xFFFFFFE0);
    ArenaStart = arenaStart;
    ArenaEnd = (void*)((u32)arenaEnd & 0xFFFFFFE0);
    return arenaStart;
}

int OSCreateHeap(void* start, void* end) {
    int heap;
    struct HeapDesc* hd;
    struct Cell* cell;

    start = (void*)(((u32)start + 0x1FU) & ~((32) - 1));
    end = (void*)(((u32)end) & ~((32) - 1));

    for (heap = 0; heap < NumHeaps; heap++) {
        hd = &HeapArray[heap];
        if (hd->size < 0) {
            hd->size = (u32)end - (u32)start;
            cell = start;
            cell->prev = 0;
            cell->next = 0;
            cell->size = hd->size;
            hd->free = cell;
            hd->allocated = 0;
            return heap;
        }
    }
    return -1;
}

void OSDestroyHeap(int heap) {
    struct HeapDesc* hd;

    hd = &HeapArray[heap];
    hd->size = -1;
}

#define ASSERTREPORT(line, cond) \
    if (!(cond)) {               \
        OSReport("OSCheckHeap: Failed " #cond " in %d", line); \
        return -1;               \
    }

long OSCheckHeap(int heap) {
    struct HeapDesc* hd;
    struct Cell* cell;
    long total = 0;
    long free = 0;

    ASSERTREPORT(0x37D, HeapArray);
    ASSERTREPORT(0x37E, 0 <= heap && heap < NumHeaps);
    hd = &HeapArray[heap];
    ASSERTREPORT(0x381, 0 <= hd->size);

    ASSERTREPORT(0x383, hd->allocated == NULL || hd->allocated->prev == NULL);

    for (cell = hd->allocated; cell; cell = cell->next) {
        ASSERTREPORT(0x386, InRange(cell, ArenaStart, ArenaEnd));
        ASSERTREPORT(0x387, OFFSET(cell, ALIGNMENT) == 0);
        ASSERTREPORT(0x388, cell->next == NULL || cell->next->prev == cell);
        ASSERTREPORT(0x389, MINOBJSIZE <= cell->size);
        ASSERTREPORT(0x38A, OFFSET(cell->size, ALIGNMENT) == 0);
        total += cell->size;
        ASSERTREPORT(0x38D, 0 < total && total <= hd->size);
    }

    ASSERTREPORT(0x395, hd->free == NULL || hd->free->prev == NULL);

    for (cell = hd->free; cell; cell = cell->next) {
        ASSERTREPORT(0x398, InRange(cell, ArenaStart, ArenaEnd));
        ASSERTREPORT(0x399, OFFSET(cell, ALIGNMENT) == 0);
        ASSERTREPORT(0x39A, cell->next == NULL || cell->next->prev == cell);
        ASSERTREPORT(0x39B, MINOBJSIZE <= cell->size);
        ASSERTREPORT(0x39C, OFFSET(cell->size, ALIGNMENT) == 0);
        ASSERTREPORT(0x39D, cell->next == NULL || (char*) cell + cell->size < (char*) cell->next);
        total += cell->size;
        free = (cell->size + free);
        free -= HEADERSIZE;
        ASSERTREPORT(0x3A1, 0 < total && total <= hd->size);
    }
    ASSERTREPORT(0x3A8, total == hd->size);
    return free;
}

void OSDumpHeap(int heap) {
    struct HeapDesc* hd;
    struct Cell* cell;

    OSReport("\nOSDumpHeap(%d):\n", heap);
    hd = &HeapArray[heap];
    if (hd->size < 0) {
        OSReport("--------Inactive\n");
        return;
    }
    OSReport("addr\tsize\t\tend\tprev\tnext\n");
    OSReport("--------Allocated\n");

    for (cell = hd->allocated; cell; cell = cell->next) {
        OSReport("%x\t%d\t%x\t%x\t%x\n", cell, cell->size, (char*)cell + cell->size, cell->prev,
                 cell->next);
    }
    OSReport("--------Free\n");
    for (cell = hd->free; cell; cell = cell->next) {
        OSReport("%x\t%d\t%x\t%x\t%x\n", cell, cell->size, (char*)cell + cell->size, cell->prev,
                 cell->next);
    }
}
