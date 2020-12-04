#ifndef SACRPIIOTSLAVE_H
#define SACRPIIOTSLAVE_H

#define I2CSALAVEADDRESS7       0x5F // SAC Iot i2c slave needs to be 0x5F (7bit address)
#define I2CSALAVEADDRESS        (I2CSALAVEADDRESS7 << 1) // 8 bit address including R/W bit (0)

#define IOTSTX                  '#'
#define IOTETX                  '\n'


typedef union
{
    struct
    {
        uint32_t txBusy                 : 1; // 0
        uint32_t rxFifoEmpty            : 1; // 1
        uint32_t txFifoFull             : 1; // 2
        uint32_t rxFifoFull             : 1; // 3
        uint32_t txFifoEmpty            : 1; // 4
        uint32_t rxBusy                 : 1; // 5
        uint32_t nBytesInTxFifo         : 5; // 6,7,8,9,10
        uint32_t nBytesInRxFifo         : 5; // 11,12,13,14,15
        uint32_t nBytesCopiedToTxFifo   : 5; // 16,17,18,19,20
        uint32_t reserved               : 11; // 21,22,23,24,25,26,27,28,29,30,31
    };
    int32_t i32;
} tBscStatus;

typedef struct
{
    uint8_t stx;
    uint8_t cmdCode;
} tIotCmdHeader;

typedef struct
{
    tIotCmdHeader header;
    uint8_t payloadSize;
    uint8_t payload[32];
    uint8_t enDownlink;
    uint8_t etx;
} tIotCmdSend;

typedef struct
{
    tIotCmdHeader header;
    uint8_t cmdCode;
    uint8_t spare;
    uint8_t etx;
} tIotCmdReadEnable;

typedef enum
{
    S_IDLE,
    S_PARSEIOTHEADER,
    S_FLAGERROR_UNKNOWNCMD,
    S_FLAGERROR_INVALIDSTX,
    S_PARSECMDSEND,
    S_PARSECMDREADENA,
    S_BUILDRESPONSE,
} tSmState;

#endif