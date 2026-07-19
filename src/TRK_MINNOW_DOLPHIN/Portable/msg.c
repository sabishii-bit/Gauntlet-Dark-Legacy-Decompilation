#include "trk.h"

DSError TRK_WriteUARTN(const void* data, u32 length);

DSError TRKMessageSend(TRKBuffer* msg) {
    return TRK_WriteUARTN(msg->data, msg->length);
}
