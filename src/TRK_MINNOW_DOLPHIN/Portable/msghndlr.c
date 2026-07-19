#include "TRK_MINNOW_DOLPHIN/MetroTRK/Portable/msghndlr.h"
#include "TRK_MINNOW_DOLPHIN/MetroTRK/Portable/msgbuf.h"
#include "TRK_MINNOW_DOLPHIN/MetroTRK/Portable/msg.h"
#include "TRK_MINNOW_DOLPHIN/MetroTRK/Portable/nubevent.h"

void TRKMessageIntoReply(TRKBuffer* b, MessageCommandID commandId, DSReplyError replyError) {
    TRKResetBuffer(b, TRUE);

    if (b->position < kMessageBufferSize) {
        b->data[b->position++] = commandId;
        b->length++;
    }

    if (b->position < kMessageBufferSize) {
        b->data[b->position++] = replyError;
        b->length++;
    }
}

static DSError TRKSendACK(TRKBuffer* b) {
    DSError error;
    s32 i = 3;

    while (i > 0) {
        error = TRKMessageSend(b);
        i--;

        if (error == DS_NoError) {
            break;
        }
    }

    return error;
}

DSError TRKStandardACK(TRKBuffer* b, MessageCommandID commandId, DSReplyError replyError) {
    TRKMessageIntoReply(b, commandId, replyError);
    return TRKSendACK(b);
}

DSError TRKDoUnsupported(TRKBuffer* b) {
    return TRKStandardACK(b, DSMSG_ReplyACK, DSREPLY_UnsupportedCommandError);
}

DSError TRKDoConnect(TRKBuffer* b) {
    return TRKStandardACK(b, DSMSG_ReplyACK, DSREPLY_NoError);
}

DSError TRKDoDisconnect(TRKBuffer* b) {
    DSError error = TRKStandardACK(b, DSMSG_ReplyACK, DSREPLY_NoError);
    TRKEvent event;

    if (error == DS_NoError) {
        TRKConstructEvent(&event, 1);
        TRKPostEvent(&event);
    }

    return error;
}

DSError TRKDoReset(TRKBuffer* b) {
    TRKStandardACK(b, DSMSG_ReplyACK, DSREPLY_NoError);
    __TRK_reset();
    return DS_NoError;
}

DSError TRKDoVersions(TRKBuffer* buf) {
    DSVersions version;
    DSError error;

    if (buf->length != 1) {
        return TRKStandardACK(buf, DSMSG_ReplyACK, DSREPLY_PacketSizeError);
    }

    TRKMessageIntoReply(buf, DSMSG_ReplyACK, DSREPLY_NoError);
    error = TRKTargetVersions(&version);

    if (error == DS_NoError) {
        error = TRKAppendBuffer1_ui8(buf, version.kernelMajor);
    }

    if (error == DS_NoError) {
        error = TRKAppendBuffer1_ui8(buf, version.kernelMinor);
    }

    if (error == DS_NoError) {
        error = TRKAppendBuffer1_ui8(buf, version.protocolMajor);
    }

    if (error == DS_NoError) {
        error = TRKAppendBuffer1_ui8(buf, version.protocolMinor);
    }

    if (error != DS_NoError) {
        return TRKStandardACK(buf, DSMSG_ReplyACK, DSREPLY_CWDSError);
    }

    return TRKSendACK(buf);
}

DSError TRKDoSupportMask(TRKBuffer* buf) {
    DSSupportMask mask;
    DSError error;

    if (buf->length != 1) {
        return TRKStandardACK(buf, DSMSG_ReplyACK, DSREPLY_PacketSizeError);
    }

    TRKMessageIntoReply(buf, DSMSG_ReplyACK, DSREPLY_NoError);

    error = TRKTargetSupportMask(&mask);

    if (error == DS_NoError) {
        error = TRKAppendBuffer(buf, &mask, sizeof(DSSupportMask));
    }

    if (error == DS_NoError) {
        error = TRKAppendBuffer1_ui8(buf, 2);
    }

    if (error != DS_NoError) {
        return TRKStandardACK(buf, DSMSG_ReplyACK, DSREPLY_CWDSError);
    }

    return TRKSendACK(buf);
}

DSError TRKDoCPUType(TRKBuffer* buf) {
    DSError error;
    DSCPUType type;

    if (buf->length != 1) {
        return TRKStandardACK(buf, DSMSG_ReplyACK, DSREPLY_PacketSizeError);
    }

    TRKMessageIntoReply(buf, DSMSG_ReplyACK, DSREPLY_NoError);
    error = TRKTargetCPUType(&type);

    if (error == DS_NoError) {
        error = TRKAppendBuffer1_ui8(buf, type.cpuMajor);
    }

    if (error == DS_NoError) {
        error = TRKAppendBuffer1_ui8(buf, type.cpuMinor);
    }

    if (error == DS_NoError) {
        error = TRKAppendBuffer1_ui8(buf, type.bigEndian);
    }

    if (error == DS_NoError) {
        error = TRKAppendBuffer1_ui8(buf, type.defaultTypeSize);
    }

    if (error == DS_NoError) {
        error = TRKAppendBuffer1_ui8(buf, type.fpTypeSize);
    }

    if (error == DS_NoError) {
        error = TRKAppendBuffer1_ui8(buf, type.extended1TypeSize);
    }

    if (error == DS_NoError) {
        error = TRKAppendBuffer1_ui8(buf, type.extended2TypeSize);
    }

    if (error != DS_NoError) {
        return TRKStandardACK(buf, DSMSG_ReplyACK, DSREPLY_CWDSError);
    }

    return TRKSendACK(buf);
}

DSError TRKDoReadMemory(TRKBuffer* buf) {
    u8 buffer[0x800] ATTRIBUTE_ALIGN(32);
    size_t sp10;
    u32 spC;
    u16 spA;
    u8 sp9;
    u8 sp8;
    DSReplyError reply;
    DSError error;

    if (buf->length != 8) {
        return TRKStandardACK(buf, DSMSG_ReplyACK, DSREPLY_PacketSizeError);
    }

    TRKSetBufferPosition(buf, 0);

    error = TRKReadBuffer1_ui8(buf, &sp8);

    if (error == DS_NoError) {
        error = TRKReadBuffer1_ui8(buf, &sp9);
    }

    if (error == DS_NoError) {
        error = TRKReadBuffer1_ui16(buf, &spA);
    }

    if (error == DS_NoError) {
        error = TRKReadBuffer1_ui32(buf, &spC);
    }

    if (sp9 & 2) {
        return TRKStandardACK(buf, DSMSG_ReplyACK, DSREPLY_UnsupportedOptionError);
    }

    if (spA > sizeof(buffer)) {
        return TRKStandardACK(buf, DSMSG_ReplyACK, DSREPLY_ParameterError);
    }

    TRKMessageIntoReply(buf, DSMSG_ReplyACK, DSREPLY_NoError);
    if (error == DS_NoError) {
        sp10 = spA;
        error = TRKTargetAccessMemory(buffer, spC, &sp10,
                                      sp9 & 8 ? MEMACCESS_UserMemory : MEMACCESS_DebuggerMemory, 1);
        spA = sp10;

        if (error == DS_NoError) {
            error = TRKAppendBuffer1_ui16(buf, spA);
        }

        if (error == DS_NoError) {
            error = TRKAppendBuffer(buf, buffer, sp10);
        }
    }

    if (error != DS_NoError) {
        switch (error) {
        case DS_CWDSException:
            reply = DSREPLY_CWDSException;
            break;
        case DS_InvalidMemory:
            reply = DSREPLY_InvalidMemoryRange;
            break;
        default:
            reply = DSREPLY_CWDSError;
            break;
        }

        return TRKStandardACK(buf, DSMSG_ReplyACK, reply);
    }

    return TRKSendACK(buf);
}

DSError TRKDoWriteMemory(TRKBuffer* b) {
    u8 buffer[0x800] ATTRIBUTE_ALIGN(32);
    size_t sp10;
    u32 spC;
    u16 spA;
    u8 sp9;
    u8 sp8;
    DSReplyError reply;
    DSError error;

    if (b->length <= 8) {
        return TRKStandardACK(b, DSMSG_ReplyACK, DSREPLY_PacketSizeError);
    }

    TRKSetBufferPosition(b, 0);

    error = TRKReadBuffer1_ui8(b, &sp8);

    if (error == DS_NoError) {
        error = TRKReadBuffer1_ui8(b, &sp9);
    }

    if (error == DS_NoError) {
        error = TRKReadBuffer1_ui16(b, &spA);
    }

    if (error == DS_NoError) {
        error = TRKReadBuffer1_ui32(b, &spC);
    }

    if (sp9 & 2) {
        return TRKStandardACK(b, DSMSG_ReplyACK, DSREPLY_UnsupportedOptionError);
    }

    if (b->length != spA + 8 || spA > sizeof(buffer)) {
        return TRKStandardACK(b, DSMSG_ReplyACK, DSREPLY_ParameterError);
    }

    if (error == DS_NoError) {
        sp10 = spA;
        error = TRKReadBuffer(b, buffer, sp10);

        if (error == DS_NoError) {
            error = TRKTargetAccessMemory(buffer, spC, &sp10,
                                          sp9 & 8 ? MEMACCESS_UserMemory : MEMACCESS_DebuggerMemory, 0);
        }

        spA = sp10;
    }

    if (error == DS_NoError) {
        TRKMessageIntoReply(b, DSMSG_ReplyACK, DSREPLY_NoError);
    }

    if (error == DS_NoError) {
        error = TRKAppendBuffer1_ui16(b, spA);
    }

    if (error != DS_NoError) {
        switch (error) {
        case DS_CWDSException:
            reply = DSREPLY_CWDSException;
            break;
        case DS_InvalidMemory:
            reply = DSREPLY_InvalidMemoryRange;
            break;
        default:
            reply = DSREPLY_CWDSError;
            break;
        }

        return TRKStandardACK(b, DSMSG_ReplyACK, reply);
    }

    return TRKSendACK(b);
}

DSError TRKDoReadRegisters(TRKBuffer* b) {
    size_t sp10;
    u16 spC;
    u16 spA;
    u8 sp9;
    u8 sp8;
    DSError error;
    DSReplyError reply;

    if (b->length != 6) {
        return TRKStandardACK(b, DSMSG_ReplyACK, DSREPLY_PacketSizeError);
    }

    TRKSetBufferPosition(b, 0);

    error = TRKReadBuffer1_ui8(b, &sp8);

    if (error == DS_NoError) {
        error = TRKReadBuffer1_ui8(b, &sp9);
    }

    if (error == DS_NoError) {
        error = TRKReadBuffer1_ui16(b, &spA);
    }

    if (error == DS_NoError) {
        error = TRKReadBuffer1_ui16(b, &spC);
    }

    if (!TRKTargetStopped()) {
        return TRKStandardACK(b, DSMSG_ReplyACK, DSREPLY_NotStopped);
    }

    if (spA > spC) {
        return TRKStandardACK(b, DSMSG_ReplyACK, DSREPLY_InvalidRegisterRange);
    }

    if (error == DS_NoError) {
        TRKMessageIntoReply(b, DSMSG_ReplyACK, DSREPLY_NoError);
    }

    switch (sp9 & 7) {
    case 0:
        error = TRKTargetAccessDefault((u32)spA, (u32)spC, b, &sp10, 1);
        break;
    case 1:
        error = TRKTargetAccessFP((u32)spA, (u32)spC, b, &sp10, 1);
        break;
    case 2:
        error = TRKTargetAccessExtended1((u32)spA, (u32)spC, b, &sp10, 1);
        break;
    case 3:
        error = TRKTargetAccessExtended2((u32)spA, (u32)spC, b, &sp10, 1);
        break;
    default:
        error = DS_UnsupportedError;
        break;
    }

    if (error != DS_NoError) {
        switch (error) {
        case DS_UnsupportedError:
            reply = DSREPLY_UnsupportedOptionError;
            break;
        case DS_InvalidRegister:
            reply = DSREPLY_InvalidRegisterRange;
            break;
        case DS_CWDSException:
            reply = DSREPLY_CWDSException;
            break;
        default:
            reply = DSREPLY_CWDSError;
            break;
        }

        return TRKStandardACK(b, DSMSG_ReplyACK, reply);
    }

    return TRKSendACK(b);
}

DSError TRKDoWriteRegisters(TRKBuffer* b) {
    size_t sp10;
    u16 spC;
    u16 spA;
    u8 sp9;
    u8 sp8;
    DSError error;
    DSReplyError reply;

    if (b->length <= 6) {
        return TRKStandardACK(b, DSMSG_ReplyACK, DSREPLY_PacketSizeError);
    }

    TRKSetBufferPosition(b, 0);

    error = TRKReadBuffer1_ui8(b, &sp8);

    if (error == DS_NoError) {
        error = TRKReadBuffer1_ui8(b, &sp9);
    }

    if (error == DS_NoError) {
        error = TRKReadBuffer1_ui16(b, &spA);
    }

    if (error == DS_NoError) {
        error = TRKReadBuffer1_ui16(b, &spC);
    }

    if (!TRKTargetStopped()) {
        return TRKStandardACK(b, DSMSG_ReplyACK, DSREPLY_NotStopped);
    }

    if (spA > spC) {
        return TRKStandardACK(b, DSMSG_ReplyACK, DSREPLY_InvalidRegisterRange);
    }

    switch (sp9) {
    case 0:
        error = TRKTargetAccessDefault((u32)spA, (u32)spC, b, &sp10, 0);
        break;
    case 1:
        error = TRKTargetAccessFP((u32)spA, (u32)spC, b, &sp10, 0);
        break;
    case 2:
        error = TRKTargetAccessExtended1((u32)spA, (u32)spC, b, &sp10, 0);
        break;
    case 3:
        error = TRKTargetAccessExtended2((u32)spA, (u32)spC, b, &sp10, 0);
        break;
    default:
        error = DS_UnsupportedError;
        break;
    }

    if (error == DS_NoError) {
        TRKMessageIntoReply(b, DSMSG_ReplyACK, DSREPLY_NoError);
    }

    if (error != DS_NoError) {
        switch (error) {
        case DS_UnsupportedError:
            reply = DSREPLY_UnsupportedOptionError;
            break;
        case DS_InvalidRegister:
            reply = DSREPLY_InvalidRegisterRange;
            break;
        case DS_MessageBufferReadError:
            reply = DSREPLY_PacketSizeError;
            break;
        case DS_CWDSException:
            reply = DSREPLY_CWDSException;
            break;
        default:
            reply = DSREPLY_CWDSError;
            break;
        }

        return TRKStandardACK(b, DSMSG_ReplyACK, reply);
    }

    return TRKSendACK(b);
}

DSError TRKDoFlushCache(TRKBuffer* b) {
    u32 sp10;
    u32 spC;
    u8 sp9;
    u8 sp8;
    DSReplyError reply;
    DSError error;

    if (b->length != 0xA) {
        return TRKStandardACK(b, DSMSG_ReplyACK, DSREPLY_PacketSizeError);
    }

    TRKSetBufferPosition(b, 0);
    error = TRKReadBuffer1_ui8(b, &sp8);

    if (error == DS_NoError) {
        error = TRKReadBuffer1_ui8(b, &sp9);
    }

    if (error == DS_NoError) {
        error = TRKReadBuffer1_ui32(b, &spC);
    }

    if (error == DS_NoError) {
        error = TRKReadBuffer1_ui32(b, &sp10);
    }

    if (!TRKTargetStopped()) {
        return TRKStandardACK(b, DSMSG_ReplyACK, DSREPLY_NotStopped);
    }

    if (spC > sp10) {
        return TRKStandardACK(b, DSMSG_ReplyACK, DSREPLY_InvalidMemoryRange);
    }

    if (error == DS_NoError) {
        error = TRKTargetFlushCache(sp9, spC, sp10);
    }

    if (error == DS_NoError) {
        TRKMessageIntoReply(b, DSMSG_ReplyACK, DSREPLY_NoError);
    }

    if (error != DS_NoError) {
        switch (error) {
        case DS_UnsupportedError:
            reply = DSREPLY_UnsupportedOptionError;
            break;
        default:
            reply = DSREPLY_CWDSError;
            break;
        }

        return TRKStandardACK(b, DSMSG_ReplyACK, reply);
    }

    return TRKSendACK(b);
}

DSError TRKDoContinue(TRKBuffer* b) {
    DSError result;

    if (!TRKTargetStopped()) {
        result = TRKStandardACK(b, DSMSG_ReplyACK, DSREPLY_NotStopped);
    } else {
        result = TRKStandardACK(b, DSMSG_ReplyACK, DSREPLY_NoError);

        if (result == DS_NoError) {
            result = TRKTargetContinue();
        }
    }

    return result;
}

DSError TRKDoStep(TRKBuffer* b) {
    DSError error;
    size_t sp10;
    u32 spC;
    u8 spA;
    u8 sp9;
    u8 sp8;
    u32 pc;

    if (b->length < 3) {
        return TRKStandardACK(b, DSMSG_ReplyACK, DSREPLY_PacketSizeError);
    }

    TRKSetBufferPosition(b, 0);

    error = TRKReadBuffer1_ui8(b, &sp8);

    if (error == DS_NoError) {
        error = TRKReadBuffer1_ui8(b, &sp9);
    }

    if (!TRKTargetStopped()) {
        return TRKStandardACK(b, DSMSG_ReplyACK, DSREPLY_NotStopped);
    }

    switch (sp9) {
    case DSSTEP_IntoCount:
    case DSSTEP_OverCount:
        if (error == DS_NoError) {
            TRKReadBuffer1_ui8(b, &spA);
        }

        if (spA < 1) {
            return TRKStandardACK(b, DSMSG_ReplyACK, DSREPLY_ParameterError);
        }
        break;
    case DSSTEP_IntoRange:
    case DSSTEP_OverRange:
        if (b->length != 0xA) {
            return TRKStandardACK(b, DSMSG_ReplyACK, DSREPLY_PacketSizeError);
        }

        if (error == DS_NoError) {
            error = TRKReadBuffer1_ui32(b, &spC);
        }

        if (error == DS_NoError) {
            TRKReadBuffer1_ui32(b, &sp10);
        }

        pc = TRKTargetGetPC();
        if (pc < spC || pc > sp10) {
            return TRKStandardACK(b, DSMSG_ReplyACK, DSREPLY_ParameterError);
        }
        break;
    default:
        return TRKStandardACK(b, DSMSG_ReplyACK, DSREPLY_UnsupportedOptionError);
    }

    error = TRKStandardACK(b, DSMSG_ReplyACK, DSREPLY_NoError);

    if (error == DS_NoError) {
        switch (sp9) {
        case DSSTEP_IntoCount:
        case DSSTEP_OverCount:
            error = TRKTargetSingleStep(spA, sp9 == DSSTEP_OverCount);
            break;
        case DSSTEP_IntoRange:
        case DSSTEP_OverRange:
            error = TRKTargetStepOutOfRange(spC, sp10, sp9 == DSSTEP_OverRange);
            break;
        default:
            break;
        }
    }

    return error;
}

DSError TRKDoStop(TRKBuffer* b) {
    DSError result;

    if (TRKTargetStop() == DS_NoError) {
        result = TRKStandardACK(b, DSMSG_ReplyACK, DSREPLY_NoError);
    } else {
        result = TRKStandardACK(b, DSMSG_ReplyACK, DSREPLY_Error);
    }

    return result;
}
