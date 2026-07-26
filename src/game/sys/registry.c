#include "types.h"

/* Named-value registry over the OS heap (string -> value with destructor
 * callbacks and refcounts). Names are provisional. */

typedef s32 OSHeapHandle;
extern volatile OSHeapHandle __OSCurrHeap;
void* OSAllocFromHeap(OSHeapHandle heap, u32 size);
void OSFreeToHeap(OSHeapHandle heap, void* ptr);

char* gstrcpy(char* dest, const char* src);
char gstrcmp(const char* a, const char* b);
u32 gstrlen(const char* s);

typedef struct REGNODE {
    /* 0x00 */ struct REGNODE* prev;
    /* 0x04 */ struct REGNODE* next;
    /* 0x08 */ void (*callback)(void*);
    /* 0x0C */ char* name;
    /* 0x10 */ u32 value;
    /* 0x14 */ u16 refCount;
} REGNODE;

typedef struct REGLIST {
    /* 0x00 */ u32 unk00;
    /* 0x04 */ REGNODE* tail;
    /* 0x08 */ REGNODE* head;
} REGLIST;

void listInsert(void* list, void* after, void* node);

u32 regAllocNode(REGNODE** node, char* name);
void regFreeNode(REGNODE** node);

REGNODE* regAdd(REGLIST* list, char* name, u32 value, void (*callback)(void*))
{
    REGNODE* node;

    node = NULL;
    if ((regAllocNode(&node, name) & 0xFF) == 0) {
        return NULL;
    }
    gstrcpy(node->name, name);
    node->value = value;
    node->callback = callback;
    node->refCount = 0;
    listInsert(&list->tail, NULL, node);
    return node;
}

u32 regAllocNode(REGNODE** node, char* name)
{
    if (*node != NULL) {
        regFreeNode(node);
    }
    *node = OSAllocFromHeap(__OSCurrHeap, sizeof(REGNODE));
    if (*node == NULL) {
        return 0;
    }
    (*node)->name = OSAllocFromHeap(__OSCurrHeap, gstrlen(name) + 1);
    if ((*node)->name == NULL) {
        return 0;
    }
    return 1;
}

/* NonMatching: 3-line branch-fold residual only. Target keeps the match test
 * unfolded (`extsb.; bne skip; b found; skip:`); MWCC folds our bare
 * `if (...==0) goto found;` to a single `beq found`. This fold is NOT the ,p
 * peephole -- under MWCC, -O4 already implies peephole, and `,p` only adds
 * -opt speed. Verified dead ends (do NOT repeat): cflags_demo (== -O4 no ,p,
 * peephole still ON) leaves it identical while keeping the other 3 exact;
 * `#pragma peephole off` folds anyway AND regresses the stw-pair prologue to
 * stmw plus the mr./extsb. record-form fusion; if/else, do{}while(0), dead
 * self-store, and inline-dup+tail-merge all either fold anyway or flip the
 * prologue to stmw. The clean goto form is the ONLY shape giving the correct
 * stw/stw prologue, and that shape inherently folds -- parked as a near-match. */
u32 regFind(REGLIST* list, char* name)
{
    REGNODE* node;
    u8 unused[8];

    node = list->head;
    if (name != NULL) {
        while (node != NULL) {
            if (gstrcmp(name, node->name) == 0) {
                goto found;
            }
            node = node->next;
        }
    }
    node = NULL;
found:
    if (node != NULL) {
        node->refCount++;
        return node->value;
    }
    return 0;
}

void regFreeNode(REGNODE** node)
{
    if (*node != NULL) {
        if ((*node)->callback != NULL) {
            (*node)->callback(&(*node)->value);
        }
        OSFreeToHeap(__OSCurrHeap, (*node)->name);
        OSFreeToHeap(__OSCurrHeap, *node);
        *node = NULL;
    }
}
