#ifndef GAME_ML_MEM_H
#define GAME_ML_MEM_H

enum FinfoState {
    FINFO_FREE = -1,
    FINFO_IN_USE = 0,
    FINFO_READY = 1,
    FINFO_USER = 2
};

enum FileCmds {
    FILE_OPENING = 0,
    FILE_READING = 1
};

/* Xbox fileinfo field names and types, checked against StartFileRead's
 * stores and do_threaded_io's loads. The callback at +0 is only stored on
 * GC; keep the existing opaque callback API pending shared declaration
 * recovery, rather than claiming the Xbox function-pointer signature here. */
typedef struct fileinfo {
    /* 0x000 */ void* complete;
    /* 0x004 */ char* destbuf;
    /* 0x008 */ int destbufsize;
    /* 0x00C */ int bytes_read;
    /* 0x010 */ enum FinfoState done;
    /* 0x014 */ enum FileCmds cmd;
    /* 0x018 */ int file_len;
    /* 0x01C */ int fd;
    /* 0x020 */ char filename[256];
    /* 0x120 */ int compressed;
    /* 0x124 */ char* comp_buff;
    /* 0x128 */ int decomp_size;
} MLFILE; /* 0x12C */

#endif
