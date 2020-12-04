#ifndef SACSTRUCTS_H
#define SACSTRUCTS_H

#include <stdint.h>

typedef struct
{
    uint8_t startTag;
    uint8_t cmdCode;
} tIotCmdHeader;

typedef union
{
    struct
    {
        uint8_t startTag;
        uint8_t cmdCode;
        uint8_t payloadSize;
        uint8_t downlinkIndicator;
        uint8_t payload[12];
        uint8_t endTag;
    };
    uint8_t ui8[17];
} tCtrlSendCmd; // contains upstream payload

typedef union
{
    struct
    {
        uint8_t startTag;
        uint8_t cmdCode;
        uint8_t payload;
        uint8_t endTag;
    };
    uint8_t ui8[4];
} tCtrlReadEnaCmd;

typedef union
{
    struct
    {
        uint8_t startTag;
        uint8_t cmdCode;
        uint8_t errorCode;
        uint8_t payloadSize;
        uint8_t payload[8];
        uint8_t endTag;
    };
    uint8_t ui8[13];
} tCtrlDeckedReply; // contains downstream payload

typedef union
{
    struct
    {
        uint8_t startTag;
        uint8_t cmdCode;
        uint8_t errorCode;
        uint8_t payloadSize;
        uint8_t endTag;
    };
    uint8_t ui8[5];
} tCtrlEmptyReply;

void structsInit();
tCtrlSendCmd *setLastSendCmd(void *pSourceData);
tCtrlSendCmd *getLastSendCmd();

#endif