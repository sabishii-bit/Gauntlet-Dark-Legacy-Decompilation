#include "TRK_MINNOW_DOLPHIN/MetroTRK/Portable/support.h"
#include "TRK_MINNOW_DOLPHIN/MetroTRK/Portable/msgbuf.h"

DSError TRKMessageSend(TRKBuffer* msg);
MessageBufferID TRKTestForPacket(void);
void TRKProcessInput(MessageBufferID bufID);

DSError TRKSuppAccessFile(u32 file_handle, u8* data, size_t* count, u8* io_result,
                          BOOL need_reply, BOOL read) {
    TRKBuffer* replyBuffer;
    int replyBufferId;
    u32 length;
    TRKBuffer* buffer;
    int bufferId;
    DSError error;
    u32 i;
    u16 replyLength;
    u8 replyIOResult;
    BOOL exit;

    if (data == NULL || *count == 0) {
        return DS_ParameterError;
    }

    exit = FALSE;
    *io_result = DS_IONoError;
    i = 0;
    error = DS_NoError;
    while (!exit && i < *count && error == DS_NoError && *io_result == 0) {
        if (*count - i > 0x800) {
            length = 0x800;
        } else {
            length = *count - i;
        }

        error = TRKGetFreeBuffer(&bufferId, &buffer);

        if (error == DS_NoError) {
            error = TRKAppendBuffer1_ui8(buffer, read ? DSMSG_ReadFile : DSMSG_WriteFile);
        }

        if (error == DS_NoError) {
            error = TRKAppendBuffer1_ui32(buffer, file_handle);
        }

        if (error == DS_NoError) {
            error = TRKAppendBuffer1_ui16(buffer, length);
        }

        if (!read && error == DS_NoError) {
            error = TRKAppendBuffer_ui8(buffer, data + i, length);
        }

        if (error == DS_NoError) {
            if (need_reply) {
                replyLength = 0;
                replyIOResult = 0;

                // The `(0, ...)` comma operator and the `read ? 5 : 5` ternary are
                // register-allocation nudges: without them mwcc colors `read` and the
                // loop index `i` into swapped saved registers. Both are semantically
                // no-ops (the 0 is discarded; both ternary arms are 5).
                error = (0, TRKRequestSend(buffer, &replyBufferId, read ? 5 : 5, 3,
                                          !(read && file_handle == 0)));

                if (error == DS_NoError) {
                    replyBuffer = TRKGetBuffer(replyBufferId);
                    TRKSetBufferPosition(replyBuffer, 2);
                }

                if (error == DS_NoError) {
                    error = TRKReadBuffer1_ui8(replyBuffer, &replyIOResult);
                }

                if (error == DS_NoError) {
                    error = TRKReadBuffer1_ui16(replyBuffer, &replyLength);
                }

                if (read && error == DS_NoError) {
                    if (replyBuffer->length != replyLength + 5) {
                        replyLength = replyBuffer->length - 5;
                        if (replyIOResult == 0) {
                            replyIOResult = 1;
                        }
                    }

                    if (replyLength <= length) {
                        error = TRKReadBuffer_ui8(replyBuffer, data + i, replyLength);
                    }
                }

                if (replyLength != length) {
                    if ((!read || replyLength >= length) && replyIOResult == 0) {
                        replyIOResult = 1;
                    }
                    length = replyLength;
                    exit = TRUE;
                }

                *io_result = replyIOResult;
                TRKReleaseBuffer(replyBufferId);
            } else {
                error = TRKMessageSend(buffer);
            }
        }

        TRKReleaseBuffer(bufferId);
        i += length;
    }

    *count = i;
    return error;
}

DSError TRKRequestSend(TRKBuffer* msgBuf, int* bufferId, u32 p1, u32 p2, int p3) {
    int error = DS_NoError;
    TRKBuffer* buffer;
    u32 timer;
    int tries;
    u8 msg_error;
    u8 msg_command;
    BOOL badReply = TRUE;

    *bufferId = -1;

    for (tries = p2 + 1; tries != 0 && *bufferId == -1 && error == DS_NoError; tries--) {
        error = TRKMessageSend(msgBuf);
        if (error == DS_NoError) {
            if (p3) {
                timer = 0;
            }

            while (TRUE) {
                do {
                    *bufferId = TRKTestForPacket();
                    if (*bufferId != -1) {
                        break;
                    }
                } while (!p3 || ++timer < 79999980);

                if (*bufferId == -1) {
                    break;
                }

                badReply = FALSE;

                buffer = TRKGetBuffer(*bufferId);
                TRKSetBufferPosition(buffer, 0);

                if ((error = TRKReadBuffer1_ui8(buffer, &msg_command)) != DS_NoError) {
                    break;
                }

                if (msg_command >= DSMSG_ReplyACK) {
                    break;
                }

                TRKProcessInput(*bufferId);
                *bufferId = -1;
            }

            if (*bufferId != -1) {
                if (buffer->length < p1) {
                    badReply = TRUE;
                }
                if (error == DS_NoError && !badReply) {
                    error = TRKReadBuffer1_ui8(buffer, &msg_error);
                }
                if (error == DS_NoError && !badReply) {
                    if (msg_command != DSMSG_ReplyACK || msg_error != DSREPLY_NoError) {
                        badReply = TRUE;
                    }
                }
                if (error != DS_NoError || badReply) {
                    TRKReleaseBuffer(*bufferId);
                    *bufferId = -1;
                }
            }
        }
    }

    if (*bufferId == -1) {
        error = DS_Error800;
    }

    return error;
}
