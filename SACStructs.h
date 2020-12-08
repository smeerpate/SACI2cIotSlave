#ifndef SACSTRUCTS_H
#define SACSTRUCTS_H

#include <stdint.h>

#define STRUCTS_SENDCMDPAYLOADSIZE      12
#define STRUCTS_SENDCMDTOTALSIZE        STRUCTS_SENDCMDPAYLOADSIZE + 5
#define STRUCTS_DECKEDREPLYPAYLOADSIZE  8
#define STRUCTS_DECKEDREPLYTOTALSIZE    STRUCTS_DECKEDREPLYPAYLOADSIZE + 5
#define STRUCTS_SERVREQ_MAXSTRSIZE      32

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
    uint8_t ui8[STRUCTS_SENDCMDTOTALSIZE];
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
        uint8_t payload[STRUCTS_DECKEDREPLYPAYLOADSIZE];
        uint8_t endTag;
    };
    uint8_t ui8[STRUCTS_DECKEDREPLYTOTALSIZE];
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
    char host[STRUCTS_SERVREQ_MAXSTRSIZE];
    char path[STRUCTS_SERVREQ_MAXSTRSIZE];
    char deviceId[STRUCTS_SERVREQ_MAXSTRSIZE];
    long unsigned int time;
    uint32_t seqNr;
    uint8_t ack;
    char *data; // data as hex-string e.g. "01000000001ecc36301fffff" 
} tServerRequest;

typedef struct
{
    int replycode;
    char *data; // data as hex-string e.g. "1d301f73deadbeef"
} tServerReply;

void structsInit();
tCtrlSendCmd *setLastSendCmd(void *pSourceData);
tCtrlReadEnaCmd *setLastReadEnaCmd(void *pSourceData);
tCtrlSendCmd *getLastSendCmd();
tCtrlReadEnaCmd *getLastReadEnaCmd();
tServerReply *getLastServerReply();
tServerRequest *getLastServerRequest();
tCtrlDeckedReply *getCtrlDeckedReply();

#endif