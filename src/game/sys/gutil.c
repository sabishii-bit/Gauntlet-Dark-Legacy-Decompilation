#include "types.h"

/* Generic intrusive list + mini string helpers (game utility TU). Names are
 * provisional. */

typedef struct GLINK {
    /* 0x0 */ void* prev;
    /* 0x4 */ void* next;
} GLINK;

typedef struct GLIST {
    /* 0x0 */ s32 offset;
    /* 0x4 */ void* head;
    /* 0x8 */ void* tail;
} GLIST;

#define NODE_LINK(node, off) ((GLINK*) ((u8*) (node) + (off)))

void listInsert(GLIST* list, void* before, void* node)
{
    s32 off = list->offset;
    GLINK* link = NODE_LINK(node, off);
    GLINK* blink;
    GLINK* plink;

    if (list->head != NULL) {
        if (before == NULL) {
            NODE_LINK(list->tail, off)->next = node;
            link->prev = list->tail;
            link->next = NULL;
            list->tail = node;
        } else {
            blink = NODE_LINK(before, off);
            if (before == list->head) {
                list->head = node;
                link->next = before;
                blink->prev = node;
            } else {
                plink = NODE_LINK(blink->prev, off);
                link->next = before;
                link->prev = blink->prev;
                blink->prev = node;
                plink->next = node;
            }
        }
    } else {
        list->tail = node;
        list->head = node;
        link->prev = NULL;
        link->next = NULL;
    }
}

void gstrcpy(char* dest, char* src)
{
    do {
        *dest = *src;
        dest++;
        src++;
    } while (*src != '\0');
}

int gstrcmp(char* a, char* b)
{
    while (1) {
        if (*a < *b) {
            return 1;
        }
        if (*a > *b) {
            return -1;
        }
        a++;
        b++;
        if (*a == '\0' || *b == '\0') {
            return 0;
        }
    }
}

int gstrlen(char* s)
{
    int len;

    len = 0;
    if (s == NULL) {
        return 0;
    }
    while (*s != '\0') {
        s++;
        len++;
    }
    return len;
}
