/*
    Iot protocol driver for SAC dispenser.
    Frederic Torreele, 25/11/2020.
    
    Sets up the RPi's Broadcom I2C slave peripheral (BSCSL)
    to be able to communicate with the SAC dispenser controller.
    Hardware pins on the RPi: (3.3V!!)
    GPIO18 = BSCSL_SDA
    GPIO19 = BSCSL_SCK
    
    Credits:
    - Using the pigpio library http://abyz.me.uk/rpi/pigpio
        Thank you abyz.me.uk.
    - Thank you rameyjm7 for the usefull example code.
        https://github.com/rameyjm7/rpi_i2c_slave
    - Thanks to Jerry Jeremiah for his forum answer:
        https://stackoverflow.com/questions/22077802/simple-c-example-of-doing-an-http-post-and-consuming-the-response
        
    Compile:
        gcc -Wall -pthread -o SACRPiIotSlave SACRPiIotSlave.c -lpigpio -lrt -lssl -lcrypto
*/

#include <pigpio.h>
#include "stdio.h"
#include <stdlib.h>
#include "string.h" /* memcpy, memset */
#include "unistd.h"
#include <stdarg.h>
#include <stdbool.h>
#include <signal.h>
#include <errno.h>

#include "SACRPiIotSlave.h"
#include "SACServerComms.h"
#include "SACPrintUtils.h"
#include "SACStructs.h"

/********************** Globals *********************/
uint32_t uSleepMicrosec = 1000; // number of micro seconds to sleep if no i2c transaction received. 32bytes take about 3.2ms to transmit.
/* i2c transfer struct
bsc_xfer:= a structure defining the transfer

typedef struct
{
    uint32_t control;          // Write
    int rxCnt;                 // Read only
    char rxBuf[BSC_FIFO_SIZE]; // Read only
    int txCnt;                 // Write
    char txBuf[BSC_FIFO_SIZE]; // Write
} bsc_xfer_t;
*/
bsc_xfer_t sI2cTransfer; // i2c transfer struct
volatile tBscStatus sI2cStatus;
tSmState sState = S_IDLE;
/****************************************************/


/******************** Prototypes ********************/
uint8_t slave_init();
void runSlave();
void listeningTask();
void closeSlave();
float getTickSec();
int getControlBits(int address, bool open, bool rxEnable);
void closeSlave();
void SIGHandler(int signum);
/****************************************************/


/****************** Implementation ******************/
uint8_t slave_init()
{
    int iResult = gpioInitialise();
    if(iResult < 0)
    {
        printf("[ERROR] (%s) %s: Error while initializing GPIOs. Return code = %i.\n", printTimestamp(), __func__, iResult);
        exit(1);
    }
    else
    {
        printf("[INFO] (%s) %s: Initialized GPIOs\n", printTimestamp(), __func__);
    }
    // Close old device (if any)
    sI2cTransfer.control = getControlBits(I2CSALAVEADDRESS7, false, false); // To avoid conflicts when restarting
    bscXfer(&sI2cTransfer);
    // Set I2C slave Address
    printf("[INFO] (%s) %s: Setting I2C slave address to 0x%02x\n", printTimestamp(), __func__, I2CSALAVEADDRESS7);
    sI2cTransfer.control = getControlBits(I2CSALAVEADDRESS7, true, true);
    iResult = bscXfer(&sI2cTransfer); // Should now be visible in I2C-Scanners
    return iResult;
}

void runSlave()
{
    int iStatus = 0;
    iStatus = slave_init(); // TODO: Might be a problem when pigpio deaman 
    if(iStatus >= 0)
    {
        // Successfully opened i2c slave
        printf("[INFO] (%s) %s: Successfully opened i2c slave. Status = %i.\n", printTimestamp(), __func__, iStatus);
        printf("[INFO] (%s) %s: FIFO size is %i bytes\n", printTimestamp(), __func__, BSC_FIFO_SIZE);
        sI2cTransfer.rxCnt = 0;
        sI2cStatus.txBusy = 0;
        sI2cStatus.rxBusy = 0;
        // Start listening...
        while(1)
        {
            listeningTask();
        }
    }
    else
    {
       printf("[ERROR] (%s) %s: Failed opening i2c slave. Status = %i.\n", printTimestamp(), __func__, iStatus);
       exit(1);
    }
}

void listeningTask()
{
    //uint8_t bEtx;
    static uint8_t bErrorResponse = 0x00;   
    tCtrlSendCmd *pLastSendCommand = getLastSendCmd(); // get the address of the last saved send command
    tCtrlReadEnaCmd *pLastReadEnaCommand = getLastReadEnaCmd(); // get the address of the last saved read enable command
    tCtrlDeckedReply *pDeckedReply = getCtrlDeckedReply(); // get the address of the last saved decked reply
    
    // Protocol state machine.
    // update the transfer struct and get the peripheral status.
    switch (sState)
    {
        case S_IDLE:
            sI2cStatus.i32 = bscXfer(&sI2cTransfer);
            sI2cTransfer.txCnt = 0; // set the fifo pointer to 0
            if(sI2cTransfer.rxCnt == 0 || sI2cStatus.rxBusy == 1)
            {
                // No new data available or busy with incoming data.
                sState = S_IDLE;
                usleep(uSleepMicrosec);
            }
            else
            {
                printf("[INFO] (%s) %s: Received %d bytes\n", printTimestamp(), __func__, sI2cTransfer.rxCnt);
                printf("\t#(%f) Bytes (HEX): %s\n", getTickSec(), printBytesAsHexString((uint32_t)sI2cTransfer.rxBuf, sI2cTransfer.rxCnt, true, ", "));
                sState = S_PARSEIOTHEADER;
            }
            break;
            
        case S_PARSEIOTHEADER:
            printf("[INFO] (%s) %s: Parsing IoT header...\n", printTimestamp(), __func__);
            tIotCmdHeader* pIotCmdHeader = (tIotCmdHeader*)sI2cTransfer.rxBuf;
            printf("\t#(%f) IoT header: stx=0x%02x, cmdCode=0x%02x\n", getTickSec(), pIotCmdHeader->startTag, pIotCmdHeader->cmdCode);
            if(pIotCmdHeader->startTag == IOT_FRMSTARTTAG)
            {
                // stx is correct proceed to command code
                switch(pIotCmdHeader->cmdCode)
                {
                    case 0x01:
                        sState = S_PARSECMDREADENA;
                        break;
                    case 0x02:
                        sState = S_PARSECMDSEND;
                        break;
                    default:
                        sState = S_IDLE;
                        break;
                }
            }
            else
            {
                // invalid stx
                sState = S_IDLE;
            }
            break;
        
        case S_FLAGERROR_UNKNOWNCMD:
            printf("[ERROR] (%s) %s: Unknown cmdCode\n", printTimestamp(), __func__);
            sState = S_IDLE;
            break;
            
        case S_FLAGERROR_INVALIDSTX:
            printf("[ERROR] (%s) %s: Invalid start of transmission (STX) code\n", printTimestamp(), __func__);
            sState = S_IDLE;
            break;
            
        case S_PARSECMDSEND:            
            pLastSendCommand = setLastSendCmd((void *)&sI2cTransfer.rxBuf[0]);
            printf("[INFO] (%s) %s: IoT send command: payload size = %i, payload at 0x%x, ETX = 0x%x\n", printTimestamp(), __func__, pLastSendCommand->payloadSize, (uint32_t)pLastSendCommand->payload, pLastSendCommand->endTag);
            if (pLastSendCommand->endTag == IOT_FRMENDTAG)
            {
                // received correct ETX
                bErrorResponse = 0x00;
                // tell master to back off
                sI2cTransfer.control = getControlBits(I2CSALAVEADDRESS7, true, false);
                bscXfer(&sI2cTransfer);
                // try to send http request with payload
                httpBuildRequestMsg((uint32_t)pLastSendCommand->payload, pLastSendCommand->payloadSize - 1); // -1 since payloadsize includes the read request byte
                httpSendRequest();
                // tell master were back
                sI2cTransfer.control = getControlBits(I2CSALAVEADDRESS7, true, true);
                bscXfer(&sI2cTransfer);
            }
            else
            {
                // received incorrect ETX
                bErrorResponse = 0x03; // flag invalid command error
            }
            // were done with the received data reset Rx count
            sI2cTransfer.rxCnt = 0;
            sState = S_IDLE;
            break;
            
           
        case S_PARSECMDREADENA:
            pLastReadEnaCommand = setLastReadEnaCmd((void *)&sI2cTransfer.rxBuf[0]);
            printf("[INFO] (%s) %s: IoT read enable command: ETX = 0x%x\n", printTimestamp(), __func__, pLastReadEnaCommand->endTag);
            sState = S_BUILDRESPONSE;
            break;
            

        case S_BUILDRESPONSE:
            printf("[INFO] (%s) %s: Building response for downlink indicator code 0x%02x (error code = 0x%02x)\n", printTimestamp(), __func__, pLastSendCommand->downlinkIndicator, bErrorResponse);
            switch(pLastSendCommand->downlinkIndicator)
            {
                case 0x00:
                    // Master did not ask for downlink data. Only send Error code.
                    sI2cTransfer.txBuf[0] = IOT_FRMSTARTTAG;
                    sI2cTransfer.txBuf[1] = 0x02;
                    sI2cTransfer.txBuf[2] = bErrorResponse;
                    sI2cTransfer.txBuf[3] = 0x00; // payload size = 0
                    sI2cTransfer.txBuf[4] = IOT_FRMENDTAG;
                    sI2cTransfer.txCnt = 5;
                    break;
                case 0x01:
                    // Master DID ask for downlink data.
                    // Payload was built in state S_PARSECMDSEND
                    pDeckedReply->startTag = IOT_FRMSTARTTAG;
                    pDeckedReply->cmdCode = 0x02;
                    pDeckedReply->errorCode = bErrorResponse;
                    pDeckedReply->payloadSize = STRUCTS_DECKEDREPLYPAYLOADSIZE;
                    pDeckedReply->endTag = IOT_FRMENDTAG;
                    
                    memcpy((void *)&sI2cTransfer, (void *)pDeckedReply, 5+STRUCTS_DECKEDREPLYPAYLOADSIZE);
                    sI2cTransfer.txCnt = 5+STRUCTS_DECKEDREPLYPAYLOADSIZE;
                    
                    // sI2cTransfer.txBuf[0] = IOT_FRMSTARTTAG;
                    // sI2cTransfer.txBuf[1] = 0x02;
                    // sI2cTransfer.txBuf[2] = bErrorResponse;
                    // sI2cTransfer.txBuf[3] = 0x08;
                    // sI2cTransfer.txBuf[4] = 0x36;
                    // sI2cTransfer.txBuf[5] = 0x30;
                    // sI2cTransfer.txBuf[6] = 0x1f;
                    // sI2cTransfer.txBuf[7] = 0x03;
                    // sI2cTransfer.txBuf[8] = 0xBE;
                    // sI2cTransfer.txBuf[9] = 0xEF;
                    // sI2cTransfer.txBuf[10] = 0xBE;
                    // sI2cTransfer.txBuf[11] = 0xEF;
                    // sI2cTransfer.txBuf[12] = IOT_FRMENDTAG;
                    // sI2cTransfer.txCnt = 13;
                    break;
                default:
                    // unknown response code
                    printf("[ERROR] (%s) %s: Invalid downlink indicator code 0x%02x\n", printTimestamp(), __func__, pLastSendCommand->downlinkIndicator);
                    sI2cTransfer.txBuf[0] = IOT_FRMSTARTTAG;
                    sI2cTransfer.txBuf[1] = 0x02;
                    sI2cTransfer.txBuf[2] = 0x03;
                    sI2cTransfer.txBuf[3] = 0x00; // payload size = 0
                    sI2cTransfer.txBuf[4] = IOT_FRMENDTAG;
                    sI2cTransfer.txCnt = 5;
                    break;
            }
            sI2cStatus.i32 = bscXfer(&sI2cTransfer);
            sI2cTransfer.txCnt = 0; // set the fifo pointer to 0. Important to set this so master can read the right data.
            sState = S_IDLE;
            break;
            
        default:
            sState = S_IDLE;
            break;
    }
    
}

float getTickSec()
{
    return ((float)gpioTick() * 1.0e-6); 
}


int getControlBits(int address /* 7 bit address */, bool open, bool rxEnable) {
    /*
    Excerpt from http://abyz.me.uk/rpi/pigpio/cif.html#bscXfer regarding the control bits:

    22 21 20 19 18 17 16 15 14 13 12 11 10 09 08 07 06 05 04 03 02 01 00
    a  a  a  a  a  a  a  -  -  IT HC TF IR RE TE BK EC ES PL PH I2 SP EN

    Bits 0-13 are copied unchanged to the BSC CR register. See pages 163-165 of the Broadcom 
    peripherals document for full details. 

    aaaaaaa defines the I2C slave address (only relevant in I2C mode)
    IT  invert transmit status flags
    HC  enable host control
    TF  enable test FIFO
    IR  invert receive status flags
    RE  enable receive
    TE  enable transmit
    BK  abort operation and clear FIFOs
    EC  send control register as first I2C byte
    ES  send status register as first I2C byte
    PL  set SPI polarity high
    PH  set SPI phase high
    I2  enable I2C mode
    SP  enable SPI mode
    EN  enable BSC peripheral
    */

    // Flags like this: 0b/*IT:*/0/*HC:*/0/*TF:*/0/*IR:*/0/*RE:*/0/*TE:*/0/*BK:*/0/*EC:*/0/*ES:*/0/*PL:*/0/*PH:*/0/*I2:*/0/*SP:*/0/*EN:*/0;

    int flags;
    int iEN = 0;
    int iI2 = 0;
    if(rxEnable)
    {
        iEN = (1 << 0);
        iI2 = (1 << 2);
    }
    else
    {
        iEN = (0 << 0);
        iI2 = (0 << 2);
    }
    
    if(open)
        flags = /*RE:*/ (1 << 9) | /*TE:*/ (1 << 8) | /*I2:*/ iI2 | /*EN:*/ iEN;
    else // Close/Abort
        flags = /*BK:*/ (1 << 7) | /*I2:*/ (0 << 2) | /*EN:*/ (0 << 0);

    return (address << 16 /*= to the start of significant bits*/) | flags;
}


void closeSlave()
{
    gpioInitialise();
    sI2cTransfer.control = getControlBits(I2CSALAVEADDRESS7, false, false);
    bscXfer(&sI2cTransfer);
    printf("[INFO] (%s) %s: Closed slave.\n", printTimestamp(), __func__);
    gpioTerminate();
    printf("[INFO] (%s) %s: Terminated GPIOs.\n", printTimestamp(), __func__);
}

void SIGHandler(int signum)
{
    closeSlave();
    exit(signum);
}
/*************************************************************************************************/

/*************************** main ***************************
    Program entry point
************************************************************/
int main(int argc, char* argv[]){
    signal(SIGINT, SIGHandler);
    structsInit();
    #if USESSL == 1
        sslInit();
    #endif
    runSlave();
    closeSlave();
    sslClose();
    return 0;
}

/*************************************************************************************************/