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
#include "SACSerial.h"


#define USESERIAL   1

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
volatile bsc_xfer_t sI2cTransfer; // i2c transfer struct
volatile tBscStatus sI2cStatus;
tSmState sState = S_IDLE;

volatile serial_xfer_t sSerialTransfer;
/****************************************************/


/******************** Prototypes ********************/
uint8_t slave_init();
void runSlave();
void listeningTask();
void closeSlave();
float getTickSec();
int getControlBits(int address, bool open, bool rxEnable);
void copyDeckedReplyToI2cTxBuffer(uint8_t bCmdCode, uint8_t bErrorCode);
void detachI2cSlave();
void attachI2cSlave();
void closeSlave();
void SIGHandler(int signum);
/****************************************************/


/****************** Implementation ******************/
uint8_t slave_init()
{
    serialInit();
    
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
    // Disable pull ups and pull downs on GPIO18 and GPIO19
    printf("[INFO] (%s) %s: Disabling GPIO pull-ups/downs.\n", printTimestamp(), __func__);
    gpioSetPullUpDown(18, PI_PUD_OFF);
    gpioSetPullUpDown(19, PI_PUD_OFF);
    // Set ALT function ALT3 on GPIO18 and GPIO19
    printf("[INFO] (%s) %s: Setting GPIO 18 & 19 to ALT3 (SPI/BSC slave).\n", printTimestamp(), __func__);
    if (gpioGetMode(18) != PI_ALT3)
    {
       gpioSetMode(18, PI_ALT3);  // set GPIO17 to ALT0
    }
    if (gpioGetMode(19) != PI_ALT3)
    {
       gpioSetMode(19, PI_ALT3);  // set GPIO17 to ALT0
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
    static uint8_t bErrorResponse = I2CERRORCODE_OK;   
    tCtrlSendCmd *pLastSendCommand = getLastSendCmd(); // get the address of the last saved send command
    tCtrlReadEnaCmd *pLastReadEnaCommand = getLastReadEnaCmd(); // get the address of the last saved read enable command
    int iResult = 0;
    
    
    // Protocol state machine.
    // update the transfer struct and get the peripheral status.
    switch (sState)
    {
        case S_IDLE:
            iResult = serialFlushFifoToRxBuffer(&sSerialTransfer);
            if(iResult == -1)
            {
                printf("[WARNING] (%s) %s:(S_IDLE) No file discriptor for serial port.\n", printTimestamp(), __func__);
            }
            sSerialTransfer.txCnt = 0; // set the fifo pointer to 0
            if(sSerialTransfer.rxCnt == 0)
            {
                // No new data available or busy with incoming data.
                sState = S_IDLE;
                usleep(uSleepMicrosec);
            }
            else
            {
                printf("[INFO] (%s) %s:(S_IDLE) Received %d bytes\n", printTimestamp(), __func__, sSerialTransfer.rxCnt);
                printf("\t#(%f) Bytes (HEX): %s\n", getTickSec(), printBytesAsHexString((uint32_t)sSerialTransfer.rxBuf, sSerialTransfer.rxCnt, true, ", "));
                sState = S_PARSEIOTHEADER;
            }
            break;
            
            
            
        case S_PARSEIOTHEADER:
            printf("[INFO] (%s) %s:(S_PARSEIOTHEADER) Parsing IoT header...\n", printTimestamp(), __func__);
            tIotCmdHeader* pIotCmdHeader = (tIotCmdHeader*)sSerialTransfer.rxBuf;
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
                        sState = S_FLAGERROR_UNKNOWNCMD;
                        break;
                }
            }
            else
            {
                // invalid stx
                sState = S_FLAGERROR_INVALIDSTX;
            }
            break;
        
        case S_FLAGERROR_UNKNOWNCMD:
            printf("[ERROR] (%s) %s:(S_FLAGERROR_UNKNOWNCMD) Unknown cmdCode\n", printTimestamp(), __func__);
            bErrorResponse = I2CERRORCODE_UNKNOWNCMD; // flag error
            // Discard received data reset Rx count
            sSerialTransfer.rxCnt = 0;
            sState = S_IDLE;
            break;
            
        case S_FLAGERROR_INVALIDSTX:
            printf("[ERROR] (%s) %s:(S_FLAGERROR_INVALIDSTX) Invalid start of transmission (STX) code\n", printTimestamp(), __func__);
            bErrorResponse = I2CERRORCODE_CMDPROCESSING; // flag error
            sSerialTransfer.rxCnt = 0;
            sState = S_IDLE;
            break;
            
        case S_FLAGERROR_INVALIDETX:
            printf("[ERROR] (%s) %s:(S_FLAGERROR_INVALIDETX) Invalid end of transmission (ETX) code\n", printTimestamp(), __func__);
            bErrorResponse = I2CERRORCODE_CMDPROCESSING; // flag error
            // Discard received data reset Rx count
            sSerialTransfer.rxCnt = 0;
            sState = S_IDLE;
            break;
            
        case S_PARSECMDSEND:
            pLastSendCommand = setLastSendCmd((void *)&sSerialTransfer.rxBuf[0]);
            printf("[INFO] (%s) %s:(S_PARSECMDSEND) IoT send command: payload size = %i, payload at 0x%x, ETX = 0x%x\n", printTimestamp(), __func__, pLastSendCommand->payloadSize, (uint32_t)pLastSendCommand->payload, pLastSendCommand->endTag);
            if (pLastSendCommand->endTag == IOT_FRMENDTAG)
            {
                // received correct ETX
                bErrorResponse = I2CERRORCODE_OK;
                //sState = S_DISSABLEI2CPERIPH;
                sState = S_SENDHTTPREQUEST;
            }
            else
            {
                // received incorrect ETX
                sState = S_FLAGERROR_INVALIDETX;
            }
            break;
            
           
        case S_PARSECMDREADENA:
            pLastReadEnaCommand = setLastReadEnaCmd((void *)&sSerialTransfer.rxBuf[0]);
            printf("[INFO] (%s) %s:(S_PARSECMDREADENA) IoT read enable command: ETX = 0x%x\n", printTimestamp(), __func__, pLastReadEnaCommand->endTag);
            //sState = S_BUILDRESPONSE;
            sState = S_IDLE; // testje-------------------------------------<<<<<<<<<<<<<<<<<<<<<<<<<<<!!!!!!!!!!!
            break;
            

        case S_BUILDRESPONSE:
            printf("[INFO] (%s) %s:(S_BUILDRESPONSE) Building response for downlink indicator code 0x%02x (error code = 0x%02x)\n", printTimestamp(), __func__, pLastSendCommand->downlinkIndicator, bErrorResponse);
            switch(pLastSendCommand->downlinkIndicator)
            {
                case 0x00:
                    // Master (i.e. SAC controller) did not ask for downlink data via i2c. Only send Error code.
                    sSerialTransfer.txBuf[0] = IOT_FRMSTARTTAG;
                    sSerialTransfer.txBuf[1] = 0x02;
                    sSerialTransfer.txBuf[2] = bErrorResponse;
                    sSerialTransfer.txBuf[3] = 0x00; // payload size = 0
                    sSerialTransfer.txBuf[4] = IOT_FRMENDTAG;
                    sSerialTransfer.txCnt = 5;
                    break;
                case 0x01:
                    // Master (i.e. SAC controller) DID ask for downlink data via i2c.
                    copyDeckedReplyToI2cTxBuffer(0x02, bErrorResponse);
                    break;
                default:
                    // unknown response code
                    printf("[ERROR] (%s) %s:(S_BUILDRESPONSE) Invalid downlink indicator code 0x%02x\n", printTimestamp(), __func__, pLastSendCommand->downlinkIndicator);
                    sSerialTransfer.txBuf[0] = IOT_FRMSTARTTAG;
                    sSerialTransfer.txBuf[1] = 0x02;
                    sSerialTransfer.txBuf[2] = I2CERRORCODE_INVALIDCMD;
                    sSerialTransfer.txBuf[3] = 0x00; // payload size = 0
                    sSerialTransfer.txBuf[4] = IOT_FRMENDTAG;
                    sSerialTransfer.txCnt = 5;
                    break;
            }
            sState = S_IDLE;
            //sState = S_ENABLEI2CPERIPH; // testje-------------------------------------<<<<<<<<<<<<<<<<<<<<<<<<<<<!!!!!!!!!!!
            break;
        
        case S_DISSABLEI2CPERIPH:
            // tell master to back off
            printf("[INFO] (%s) %s:(S_DISSABLEI2CPERIPH) Detaching I2C slave peripheral...\n", printTimestamp(), __func__);
            //detachI2cSlave();
            sState = S_SENDHTTPREQUEST;
            break;
            
        case S_SENDHTTPREQUEST:
            // try to send http request with payload
            printf("[INFO] (%s) %s:(S_SENDHTTPREQUEST) Sending HTTP request.\n", printTimestamp(), __func__);
            httpBuildRequestMsg((uint32_t)pLastSendCommand->payload, pLastSendCommand->payloadSize - 1); // -1 since payloadsize includes the read request byte
            if(httpSendRequest() < 0)
            // This might take a while ...
            {
                bErrorResponse = I2CERRORCODE_SERVERUNREACH;
            }
            
            //sState = S_ENABLEI2CPERIPH;
            sState = S_BUILDRESPONSE; // testje-------------------------------------<<<<<<<<<<<<<<<<<<<<<<<<<<<!!!!!!!!!!!
            break;
            
        case S_ENABLEI2CPERIPH:
            // tell master were back
            printf("[INFO] (%s) %s:(S_ENABLEI2CPERIPH) Re-attaching I2C slave peripheral...\n", printTimestamp(), __func__);
            //attachI2cSlave();
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
        
    // int iRE = 0;
    // if(rxEnable)
    // {
        // iRE = (1 << 9);
    // }
    // else
    // {
        // iRE = (0 << 9);
    // }
    
    // if(open)
        // flags = /*RE:*/ iRE | /*TE:*/ (1 << 8) | /*I2:*/ (1 << 2) | /*EN:*/ (1 << 0);
    // else // Close/Abort
        // flags = /*BK:*/ (1 << 7) | /*I2:*/ (0 << 2) | /*EN:*/ (0 << 0);

    return (address << 16 /*= to the start of significant bits*/) | flags;
}

void copyDeckedReplyToI2cTxBuffer(uint8_t bCmdCode, uint8_t bErrorCode)
{
    tCtrlDeckedReply *pDeckedReply = getCtrlDeckedReply(); // get the address of the last saved decked reply
    // build header and footer around decked reply payload
    pDeckedReply->startTag = IOT_FRMSTARTTAG;
    pDeckedReply->cmdCode = bCmdCode;
    pDeckedReply->errorCode = bErrorCode;
    pDeckedReply->payloadSize = STRUCTS_DECKEDREPLYPAYLOADSIZE;
    pDeckedReply->endTag = IOT_FRMENDTAG;
    // payload was filled after a call to httpSendRequest();
    
    int i;
    for(i=0; i<STRUCTS_DECKEDREPLYTOTALSIZE; i += 1)
    {
        sI2cTransfer.txBuf[i] = pDeckedReply->ui8[i];
    }
    
    sI2cTransfer.txCnt = STRUCTS_DECKEDREPLYTOTALSIZE;
    
    printf("[INFO] (%s) %s: Filled i2c Tx buffer with %d bytes\n", printTimestamp(), __func__, sI2cTransfer.txCnt);
    printf("\t#(%f) Bytes (HEX): %s\n", getTickSec(), printBytesAsHexString((uint32_t)sI2cTransfer.txBuf, sI2cTransfer.txCnt, true, ", "));
}

void detachI2cSlave()
{
    if (gpioGetMode(18) != PI_INPUT)
    {
       gpioSetMode(18, PI_INPUT);  // set GPIO17 to ALT0
    }
    if (gpioGetMode(19) != PI_INPUT)
    {
       gpioSetMode(19, PI_INPUT);  // set GPIO17 to ALT0
    }
    // abort I2C and clear filo's
    /*
    22 21 20 19 18 17 16 15 14 13 12 11 10 09 08 07 06 05 04 03 02 01 00
    a  a  a  a  a  a  a  -  -  IT HC TF IR RE TE BK EC ES PL PH I2 SP EN
    */
    sI2cTransfer.control = (I2CSALAVEADDRESS7 << 16) | 1<<9 | 1<<8 | 1<<7 | 1<<2 | 1<<0;
    sI2cStatus.i32 = bscXfer(&sI2cTransfer);
    if(sI2cStatus.i32 == -1)
    {
        printf("[WARNING] (%s) %s:(S_IDLE) Detected i2c slave timeout.\n", printTimestamp(), __func__);
    }
}

void attachI2cSlave()
{
    if (gpioGetMode(18) != PI_ALT3)
    {
       gpioSetMode(18, PI_ALT3);  // set GPIO17 to ALT0
    }
    if (gpioGetMode(19) != PI_ALT3)
    {
       gpioSetMode(19, PI_ALT3);  // set GPIO17 to ALT0
    }
}

void closeSlave()
{
    printf("[INFO] (%s) %s: Closing serial port.\n", printTimestamp(), __func__);
    serialTerminate();
    
    //gpioInitialise();
    //sI2cTransfer.control = getControlBits(I2CSALAVEADDRESS7, false, false);
    //bscXfer(&sI2cTransfer);
    //printf("[INFO] (%s) %s: Closed slave.\n", printTimestamp(), __func__);
    gpioTerminate();
    printf("[INFO] (%s) %s: Terminated GPIOs.\n", printTimestamp(), __func__);
}

void SIGHandler(int signum)
{
    printf("[INFO] (%s) %s: got signal number %d.\n", printTimestamp(), __func__, signum);
    closeSlave();
    exit(0);
}
/*************************************************************************************************/

/*************************** main ***************************
    Program entry point
************************************************************/
int main(int argc, char* argv[]){
    signal(SIGINT, SIGHandler);
    signal(SIGCONT, SIGHandler);
    signal(SIGABRT, SIGHandler);
    signal(SIGALRM, SIGHandler);
    signal(SIGBUS, SIGHandler);
    signal(SIGCHLD, SIGHandler);
    
    signal(SIGFPE, SIGHandler);
    signal(SIGHUP, SIGHandler);
    signal(SIGILL, SIGHandler);
    signal(SIGKILL, SIGHandler);
    
    signal(SIGPWR, SIGHandler);
    signal(SIGQUIT, SIGHandler);
    signal(SIGSEGV, SIGHandler);
    signal(SIGSTOP, SIGHandler);
    
    signal(SIGSYS, SIGHandler);
    signal(SIGTERM, SIGHandler);
    signal(SIGXCPU, SIGHandler);
    signal(SIGXFSZ, SIGHandler);
    
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