#include "game/gutil.h"

/* Named-value registry over the OS heap (string -> value with destructor
 * callbacks and refcounts). Names are provisional. */

typedef s32 OSHeapHandle;
extern volatile OSHeapHandle __OSCurrHeap;
void* OSAllocFromHeap(OSHeapHandle heap, u32 size);
void OSFreeToHeap(OSHeapHandle heap, void* ptr);

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
    /* 0x04 */ GLIST nodes;
} REGLIST;

/* regAdd passes root + 4 to listInsert: link offset, head and tail live at
 * root + 4/+8/+12. The leading word's purpose is not yet known. */

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
    listInsert(&list->nodes, NULL, node);
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

/* Native reconstruction remains NonMatching: the target has bne/b after
 * gstrcmp, while this source folds it to beq and branches after the loop.
 * The old eight-byte reservation below also has unrecovered provenance. */
u32 regFind(REGLIST* list, char* name)
{
    REGNODE* node;
    u8 unused[8];

    node = list->nodes.head;
    if (name != NULL) {
        while (node != NULL) {
            if (gstrcmp(name, node->name) == 0) {
                break;
            }
            node = node->next;
        }
    } else {
        node = NULL;
    }
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
