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

/* 0x800D5260  no-op hook called on list-verify failure */
void listVerifyHook(void) {
}

/* 0x800D5264  memlock acquire stub (returns 1) */
void dcsMemLock(void) {
}

/* 0x800D526C  memlock release stub (returns 1) */
void dcsMemUnlock(void) {
}

/* 0x800D5274  memlock try stub (returns 1) */
void dcsMemTryLock(void) {
}

/* 0x800D527C  return current lock owner */
void dcsMemLockOwner(void) {
}

/* 0x800D5284  tag a lock table slot + timestamp */
void dcsMemLockTag(void) {
}

/* 0x800D52C4  create/register a pool block */
void pool_new(void) {
}

/* 0x800D5390  coalesce free blocks (qsort) */
void pool_garbage_collect(void) {
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
void pool_init(void) {
}

/* 0x800D5F94  insert a node (verified) */
void list_insert(void) {
}

/* 0x800D603C  remove a node (verified) */
void list_remove(void) {
}

/* 0x800D60B8  append a node (verified) */
void list_append(void) {
}

/* 0x800D6130  validate a node ("LIST Bad node") */
void list_verify(void) {
}

/* 0x800D621C  tiny pool accessor */
void pool_query(void) {
}

