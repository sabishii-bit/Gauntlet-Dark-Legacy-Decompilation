#include "TRK_MINNOW_DOLPHIN/MetroTRK/Portable/msgbuf.h"
#include "TRK_MINNOW_DOLPHIN/MetroTRK/Portable/support.h"

void TRKTargetAddStopInfo(TRKBuffer* buffer);
void TRKTargetAddExceptionInfo(TRKBuffer* buffer);

DSError TRKDoNotifyStopped(u8 cmdId) {
    DSError result;
    int sp8;
    TRKBuffer* buffer;
    int spC;

    result = TRKGetFreeBuffer(&spC, &buffer);

    if (result == DS_NoError) {
        result = TRKAppendBuffer1_ui8(buffer, cmdId);

        if (result == DS_NoError) {
            if (cmdId == DSMSG_NotifyStopped) {
                TRKTargetAddStopInfo(buffer);
            } else {
                TRKTargetAddExceptionInfo(buffer);
            }
        }

        result = TRKRequestSend(buffer, &sp8, 2, 3, 1);

        if (result == DS_NoError) {
            TRKReleaseBuffer(sp8);
        }

        TRKReleaseBuffer(spC);
    }

    return result;
}
