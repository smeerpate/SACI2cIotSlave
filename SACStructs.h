#ifndef SACSTRUCTS_H
#define SACSTRUCTS_H

#include <stdint.h>

#define STRUCTS_SENDCMDPAYLOADSIZE      12
#define STRUCTS_DECKEDREPLYPAYLOADSIZE  8

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
        uint8_t payload[STRUCTS_SENDCMDPAYLOADSIZE];
        uint8_t endTag;
    };
    uint8_t ui8[5+STRUCTS_SENDCMDPAYLOADSIZE];
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
    uint8_t ui8[5+STRUCTS_DECKEDREPLYPAYLOADSIZE];
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


typedef struct
{
    uint8_t expectingContent;
    int replycode;
    char replyContent[16];
} tServerReply

void structsInit();
tCtrlSendCmd *setLastSendCmd(void *pSourceData);
tCtrlReadEnaCmd *setLastReadEnaCmd(void *pSourceData);
tCtrlSendCmd *getLastSendCmd();
tCtrlReadEnaCmd *getLastReadEnaCmd();

#endif