#ifndef SACI2CDRIVER_H
#define SACI2CDRIVER_H

#include <stdint.h>

typedef union
{
    struct
    {
        uint32_t data           : 8; // 0,1,2,3,4,5,6,7
        uint32_t oe             : 1; // 8
        uint32_t ue             : 1; // 9
        uint32_t reserved       : 6; // 10,11,12,13,14,15
        uint32_t txBusy         : 1; // 16
        uint32_t rxFe           : 1; // 17
        uint32_t txFf           : 1; // 18
        uint32_t rxFf           : 1; // 19
        uint32_t txFe           : 1; // 20
        uint32_t rxBusy         : 1; // 21
        uint32_t txFLevel       : 5; // 22,23,24,25,26
        uint32_t rxFLevel       : 5; // 27,28,29,30,31
    };
    uint32_t u32;
} tSpiBcsPeriphRegDR;


typedef union
{
    struct
    {
        uint32_t oe             : 1; // 0
        uint32_t ue             : 1; // 1
        uint32_t reserved       : 30; // 2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,...31
    };
    uint32_t u32;
} tSpiBcsPeriphRegRSR;


typedef union
{
    struct
    {
        uint32_t address        : 7; // 0,1,2,3,4,5,6
        uint32_t reserved       : 25; // 7,8,9,10,11,12,13,14,15,16,17,...31
    };
    uint32_t u32;
} tSpiBcsPeriphRegSLV;


typedef union
{
    struct
    {
        uint32_t en             : 1; // 0
        uint32_t spi            : 1; // 1
        uint32_t i2c            : 1; // 2
        uint32_t cPha           : 1; // 3
        uint32_t cPol           : 1; // 4
        uint32_t enStat         : 1; // 5
        uint32_t enCtrl         : 1; // 6
        uint32_t brk            : 1; // 7
        uint32_t txe            : 1; // 8
        uint32_t rxe            : 1; // 9
        uint32_t invRxf         : 1; // 10
        uint32_t testFifo       : 1; // 11
        uint32_t hostCtrlEn     : 1; // 12
        uint32_t invTxf         : 1; // 13
        uint32_t reserved       : 18; // 14,15,16,17,...31
    };
    uint32_t u32;
} tSpiBcsPeriphRegCR;


typedef union
{
    struct
    {
        uint32_t txBusy         : 1; // 0
        uint32_t rxFe           : 1; // 1
        uint32_t txFf           : 1; // 2
        uint32_t rxFf           : 1; // 3
        uint32_t txFe           : 1; // 4
        uint32_t rxBusy         : 1; // 5
        uint32_t txFLevel       : 5; // 6,7,8,9,10
        uint32_t rxFLevel       : 5; // 11,12,13,14,15
        uint32_t reserved       : 16; // 16,17,18,19,20,21,22,23,24,25,26...31
    };
    uint32_t u32;
} tSpiBcsPeriphRegFR;


typedef union
{
    struct
    {
        uint32_t rxIm           : 1; // 0
        uint32_t txIm           : 1; // 1
        uint32_t beIm           : 1; // 2
        uint32_t oeIm           : 1; // 3
        uint32_t reserved       : 28; // 4,5,6,7,8,9,10,11,12,13,14,15,16,17,...31
    };
    uint32_t u32;
} tSpiBcsPeriphRegIMSC;


typedef union
{
    struct
    {
        uint32_t rxRis          : 1; // 0
        uint32_t txRis          : 1; // 1
        uint32_t beRis          : 1; // 2
        uint32_t oeRis          : 1; // 3
        uint32_t reserved       : 28; // 4,5,6,7,8,9,10,11,12,13,14,15,16,17,...31
    };
    uint32_t u32;
} tSpiBcsPeriphRegRIS;


typedef union
{
    struct
    {
        uint32_t rxMis          : 1; // 0
        uint32_t txMis          : 1; // 1
        uint32_t beMis          : 1; // 2
        uint32_t oeMis          : 1; // 3
        uint32_t reserved       : 28; // 4,5,6,7,8,9,10,11,12,13,14,15,16,17,...31
    };
    uint32_t u32;
} tSpiBcsPeriphRegMIS;


typedef union
{
    struct
    {
        uint32_t rxIc           : 1; // 0
        uint32_t txIc           : 1; // 1
        uint32_t beIc           : 1; // 2
        uint32_t oeIc           : 1; // 3
        uint32_t reserved       : 28; // 4,5,6,7,8,9,10,11,12,13,14,15,16,17,...31
    };
    uint32_t u32;
} tSpiBcsPeriphRegICR;


typedef union
{
    struct
    {
        uint32_t data           : 8; // 0,1,2,3,4,5,6,7
        uint32_t reserved       : 24; // 8,9,10,11,12,13,14,15,16,17,...31
    };
    uint32_t u32;
} tSpiBcsPeriphRegTDR;


typedef union
{
    struct
    {
        uint32_t data           : 8; // 0,1,2,3,4,5,6,7
        uint32_t reserved       : 24; // 8,9,10,11,12,13,14,15,16,17,...31
    };
    uint32_t u32;
} tSpiBcsPeriphRegGPUSTAT;


typedef union
{
    struct
    {
        uint32_t data           : 8; // 0,1,2,3,4,5,6,7
        uint32_t reserved       : 24; // 8,9,10,11,12,13,14,15,16,17,...31
    };
    uint32_t u32;
} tSpiBcsPeriphRegHCTRL;


typedef union
{
    struct
    {
        uint32_t data           : 8; // 0,1,2,3,4,5,6,7
        uint32_t reserved       : 24; // 8,9,10,11,12,13,14,15,16,17,...31
    };
    uint32_t u32;
} tSpiBcsPeriphRegDEBUG1;


typedef union
{
    struct
    {
        uint32_t data           : 8; // 0,1,2,3,4,5,6,7
        uint32_t reserved       : 24; // 8,9,10,11,12,13,14,15,16,17,...31
    };
    uint32_t u32;
} tSpiBcsPeriphRegDEBUG2;

#endif