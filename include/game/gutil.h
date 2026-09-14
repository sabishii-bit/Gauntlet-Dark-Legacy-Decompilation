#ifndef GAME_GUTIL_H
#define GAME_GUTIL_H

#include "types.h"

/* Intrusive links are located at offset bytes within each list node. */
typedef struct GLIST {
    /* 0x0 */ s32 offset;
    /* 0x4 */ void* head;
    /* 0x8 */ void* tail;
} GLIST;

void listInsert(GLIST* list, void* before, void* node);
void gstrcpy(char* dest, const char* src);
/* regFind narrows this result as a signed byte; the helper returns -1/0/1.
 * This shared declaration follows that GC call ABI, not a recovered header. */
char gstrcmp(const char* a, const char* b);
int gstrlen(const char* s);

#endif
