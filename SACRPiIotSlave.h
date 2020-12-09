#ifndef SACRPIIOTSLAVE_H
#define SACRPIIOTSLAVE_H

#define I2CSALAVEADDRESS7       0x5F // SAC Iot i2c slave needs to be 0x5F (7bit address)
#define I2CSALAVEADDRESS        (I2CSALAVEADDRESS7 << 1) // 8 bit address including R/W bit (0)

#define I2CERRORCODE_OK             0x00
#define I2CERRORCODE_CMDPROCESSING  0x01
#define I2CERRORCODE_NOCMD          0x02
#define I2CERRORCODE_INVALIDCMD     0x03
#define I2CERRORCODE_UNKNOWNCMD     0x04
#define I2CERRORCODE_UNEXPECTEDPLSZ 0x05
#define I2CERRORCODE_RES            0x06
#define I2CERRORCODE_SERVERUNREACH  0x07


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