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

//#include <pigpio.h>
#include "stdio.h"
#include <stdlib.h>
#include "string.h" /* memcpy, memset */
#include "unistd.h"
#include <stdarg.h>
#include <stdbool.h>
#include <signal.h>
#include <errno.h>
#include <stdint.h>

#include "SACRPiIotSlave.h"
#include "SACServerComms.h"
#include "SACPrintUtils.h"
#include "SACStructs.h"
#include "SACSerial.h"


#define USESERIAL   1

/********************** Globals *********************/
uint32_t uSleepMicrosec = 1000; // number of micro seconds to sleep if no i2c transaction received. 32bytes take about 3.2ms to transmit.
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
void copyDeckedReplyToSerialTxBuffer(uint8_t bCmdCode, uint8_t bErrorCode);
void detachI2cSlave();
void attachI2cSlave();
void closeSlave();
void SIGHandler(int signum);
/****************************************************/


/****************** Implementation ******************/
uint8_t slave_init()
{
    int iResult = serialInit();
    if(iResult < 0)
    {
        printf("[ERROR] (%s) %s: Error while initializing serial port. Return code = %i.\n", printTimestamp(), __func__, iResult);
        exit(1);
    }
    else
    {
        printf("[INFO] (%s) %s: Initialized serial port.\n", printTimestamp(), __func__);
    }
    return iResult;
}

void runSlave()
{
    int iStatus = 0;
    iStatus = slave_init(); // TODO: Might be a problem when pigpio deaman 
    if(iStatus >= 0)
    {
        // Successfully opened serial port
        printf("[INFO] (%s) %s: Successfully opened serial port. Status = %i.\n", printTimestamp(), __func__, iStatus);
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
            // TODO: don't retry to read data forever if no fale descriptor
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
            sState = S_IDLE;
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
                    copyDeckedReplyToSerialTxBuffer(0x02, bErrorResponse);
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
            serialTransmitTxBuffer(&sSerialTransfer);
            sState = S_IDLE;
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
            
            sState = S_BUILDRESPONSE;
            break;
            
        default:
            sState = S_IDLE;
            break;
    }
    
}

float getTickSec()
{
    return 0;//((float)gpioTick() * 1.0e-6); 
}


void copyDeckedReplyToSerialTxBuffer(uint8_t bCmdCode, uint8_t bErrorCode)
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
        sSerialTransfer.txBuf[i] = pDeckedReply->ui8[i];
    }
    
    sSerialTransfer.txCnt = STRUCTS_DECKEDREPLYTOTALSIZE;
    
    printf("[INFO] (%s) %s: Filled serial Tx buffer with %d bytes\n", printTimestamp(), __func__, sSerialTransfer.txCnt);
    printf("\t#(%f) Bytes (HEX): %s\n", getTickSec(), printBytesAsHexString((uint32_t)sSerialTransfer.txBuf, sSerialTransfer.txCnt, true, ", "));
}

void closeSlave()
{
    printf("[INFO] (%s) %s: Closing serial port.\n", printTimestamp(), __func__);
    serialTerminate();
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