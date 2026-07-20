#ifndef METROTRK_PORTABLE_SUPPORT_H
#define METROTRK_PORTABLE_SUPPORT_H

#include "trk.h"

#ifdef __cplusplus
extern "C" {
#endif

DSError TRKSuppAccessFile(u32 file_handle, u8* data, size_t* count, u8* io_result,
                          BOOL need_reply, BOOL read);
DSError TRKRequestSend(TRKBuffer* msgBuf, int* bufferId, u32 p1, u32 p2, int p3);

#ifdef __cplusplus
}
#endif

#endif /* METROTRK_PORTABLE_SUPPORT_H */
