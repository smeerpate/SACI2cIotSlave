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
*/

#include <pigpio.h>
#include "stdio.h"
#include <stdlib.h>
#include "string.h" /* memcpy, memset */
#include "unistd.h"
#include <stdarg.h>
#include <stdbool.h>
#include <time.h>
#include <sys/socket.h> /* socket, connect */
#include <netinet/in.h> /* struct sockaddr_in, struct sockaddr */
#include <netdb.h> /* struct hostent, gethostbyname */
#include <signal.h>
#include <errno.h>

#define I2CSALAVEADDRESS7       0x5F // SAC Iot i2c slave needs to be 0x5F (7bit address)
#define I2CSALAVEADDRESS        (I2CSALAVEADDRESS7 << 1) // 8 bit address including R/W bit (0)
#define UPSTREAMBUFFERSIZE      12
#define DOWNSTREAMBUFFERSIZE    32

#define IOTSTX                  '#'
#define IOTETX                  '\n'

#define HTTPMSGMAXSIZE          512

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
uint8_t mabUpstreamDataBuffer[UPSTREAMBUFFERSIZE] = {0x00};
uint8_t mabDownstreamDataBuffer[DOWNSTREAMBUFFERSIZE] = {0x00};
time_t sRawTime;
char sTimeStamp[32] = {0x00};

char *msHttpHost;
int miHttpPortNo;
char *msHttpMsgFmt;
struct hostent *msHttpServer;
struct sockaddr_in msHttpServerAddr;
int miHttpSocketFd;
char msHttpTxMessage[HTTPMSGMAXSIZE] = {0x00};
char msHttpRxMessage[HTTPMSGMAXSIZE] = {0x00};
/****************************************************/


/******************** Prototypes ********************/
uint8_t slave_init();
void runSlave();
void listeningTask();
void closeSlave();
char* getTimestamp();
float getTickSec();
int getControlBits(int, bool);
void closeSlave();
void SIGHandler(int signum);
int httpSocketInit();
int httpSendRequest();
void httpBuildRequestMsg(uint32_t I2CRxPayloadAddress, int I2CRxPayloadLength);
/****************************************************/


/****************** Implementation ******************/
uint8_t slave_init()
{
    int iResult = gpioInitialise();
    if(iResult < 0)
    {
        printf("[ERROR] (%s) %s: Error while initializing GPIOs. Return code = %i.\n", getTimestamp(), __func__, iResult);
        exit(1);
    }
    else
    {
        printf("[INFO] (%s) %s: Initialized GPIOs\n", getTimestamp(), __func__);
    }
    // Close old device (if any)
    sI2cTransfer.control = getControlBits(I2CSALAVEADDRESS7, false); // To avoid conflicts when restarting
    bscXfer(&sI2cTransfer);
    // Set I2C slave Address
    printf("[INFO] (%s) %s: Setting I2C slave address to 0x%02x\n", getTimestamp(), __func__, I2CSALAVEADDRESS7);
    sI2cTransfer.control = getControlBits(I2CSALAVEADDRESS7, true);
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
        printf("[INFO] (%s) %s: Successfully opened i2c slave. Status = %i.\n", getTimestamp(), __func__, iStatus);
        printf("[INFO] (%s) %s: FIFO size is %i bytes\n", getTimestamp(), __func__, BSC_FIFO_SIZE);
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
       printf("[ERROR] (%s) %s: Failed opening i2c slave. Status = %i.\n", getTimestamp(), __func__, iStatus);
       exit(1);
    }
}

void listeningTask()
{
    uint8_t bPayloadSize;
    uint8_t *pPayload;
    uint8_t bEtx;
    static uint8_t bErrorResponse = 0x00;
    static uint8_t bLastDownLinkIndicatorCode = 0xFF;
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
                printf("[INFO] (%s) %s: Received %d bytes\n", getTimestamp(), __func__, sI2cTransfer.rxCnt);
                for(int i=0; i < sI2cTransfer.rxCnt; i++)
                {
                    printf("\t#(%f) Byte %d : 0x%02x\n", getTickSec(), i, sI2cTransfer.rxBuf[i]);
                }
                sState = S_PARSEIOTHEADER;
            }
            break;
            
        case S_PARSEIOTHEADER:
            printf("[INFO] (%s) %s: Parsing IoT header...\n", getTimestamp(), __func__);
            tIotCmdHeader* pIotCmdHeader = (tIotCmdHeader*)sI2cTransfer.rxBuf;
            printf("\t#(%f) IoT header: stx=0x%02x, cmdCode=0x%02x\n", getTickSec(), pIotCmdHeader->stx, pIotCmdHeader->cmdCode);
            if(pIotCmdHeader->stx == IOTSTX)
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
            printf("[ERROR] (%s) %s: Unknown cmdCode\n", getTimestamp(), __func__);
            sState = S_IDLE;
            break;
            
        case S_FLAGERROR_INVALIDSTX:
            printf("[ERROR] (%s) %s: Invalid start of transmission (STX) code\n", getTimestamp(), __func__);
            sState = S_IDLE;
            break;
            
        case S_PARSECMDSEND:
            bPayloadSize = sI2cTransfer.rxBuf[2];
            pPayload = (uint8_t *)&sI2cTransfer.rxBuf[4];
            bEtx = *(pPayload + bPayloadSize - 1);
            printf("[INFO] (%s) %s: IoT send command: payload size = %i, payload at 0x%x, ETX = 0x%x\n", getTimestamp(), __func__, bPayloadSize, (uint32_t)pPayload, bEtx);
            // save rx command status
            bLastDownLinkIndicatorCode = sI2cTransfer.rxBuf[3];
            if (bEtx == IOTETX)
            {
                // received correct ETX
                bErrorResponse = 0x00;
                // try to send http request with payload
                httpBuildRequestMsg((uint32_t)pPayload, bPayloadSize);
                httpSendRequest();
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
            bEtx = sI2cTransfer.rxBuf[3];
            printf("[INFO] (%s) %s: IoT read enable command: ETX = 0x%x\n", getTimestamp(), __func__, bEtx);
            sState = S_BUILDRESPONSE;
            break;
            

        case S_BUILDRESPONSE:
            printf("[INFO] (%s) %s: Building response for downlink indicator code 0x%02x (error code = 0x%02x)\n", getTimestamp(), __func__, bLastDownLinkIndicatorCode, bErrorResponse);
            switch(bLastDownLinkIndicatorCode)
            {
                case 0x00:
                    // Master did not ask for downlink data. Only send Error code.
                    sI2cTransfer.txBuf[0] = IOTSTX;
                    sI2cTransfer.txBuf[1] = 0x02;
                    sI2cTransfer.txBuf[2] = bErrorResponse;
                    sI2cTransfer.txBuf[3] = 0x00; // payload size = 0
                    sI2cTransfer.txBuf[4] = IOTETX;
                    sI2cTransfer.txCnt = 5;
                    break;
                case 0x01:
                    // Master DID ask for downlink data.
                    sI2cTransfer.txBuf[0] = IOTSTX;
                    sI2cTransfer.txBuf[1] = 0x02;
                    sI2cTransfer.txBuf[2] = bErrorResponse;
                    sI2cTransfer.txBuf[3] = 0x08;
                    sI2cTransfer.txBuf[4] = 0x36;
                    sI2cTransfer.txBuf[5] = 0x30;
                    sI2cTransfer.txBuf[6] = 0x1f;
                    sI2cTransfer.txBuf[7] = 0x03;
                    sI2cTransfer.txBuf[8] = 0xBE;
                    sI2cTransfer.txBuf[9] = 0xEF;
                    sI2cTransfer.txBuf[10] = 0xBE;
                    sI2cTransfer.txBuf[11] = 0xEF;
                    sI2cTransfer.txBuf[12] = IOTETX;
                    sI2cTransfer.txCnt = 13;
                    break;
                default:
                    // unknown response code
                    printf("[ERROR] (%s) %s: Invalid downlink indicator code 0x%02x\n", getTimestamp(), __func__, sI2cTransfer.rxBuf[3]);
                    sI2cTransfer.txBuf[0] = IOTSTX;
                    sI2cTransfer.txBuf[1] = 0x02;
                    sI2cTransfer.txBuf[2] = 0x03;
                    sI2cTransfer.txBuf[3] = 0x00; // payload size = 0
                    sI2cTransfer.txBuf[4] = IOTETX;
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


char* getTimestamp()
{
    
    struct tm* tm_info;    
    sRawTime = time(NULL);
    
    tm_info = localtime(&sRawTime);
    strftime(sTimeStamp, 26, "%Y-%m-%d %H:%M:%S", tm_info);
    return sTimeStamp;
}

float getTickSec()
{
    return ((float)gpioTick() * 1.0e-6); 
}


int getControlBits(int address /* 7 bit address */, bool open) {
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
    if(open)
        flags = /*RE:*/ (1 << 9) | /*TE:*/ (1 << 8) | /*I2:*/ (1 << 2) | /*EN:*/ (1 << 0);
    else // Close/Abort
        flags = /*BK:*/ (1 << 7) | /*I2:*/ (0 << 2) | /*EN:*/ (0 << 0);

    return (address << 16 /*= to the start of significant bits*/) | flags;
}


void closeSlave()
{
    gpioInitialise();
    sI2cTransfer.control = getControlBits(I2CSALAVEADDRESS7, false);
    bscXfer(&sI2cTransfer);
    printf("[INFO] (%s) %s: Closed slave.\n", getTimestamp(), __func__);
    gpioTerminate();
    printf("[INFO] (%s) %s: Terminated GPIOs.\n", getTimestamp(), __func__);
}

void SIGHandler(int signum)
{
    closeSlave();
    exit(signum);
}


int httpSocketInit()
{
    /* first what are we going to send and where are we going to send it? */
    /* send a post to:
        https://dashboard.safeandclean.be/mobile/webhook?id={device}&time={time}&seqNumber={seqNumber}&ack={ack}&data={data}
    */
    miHttpPortNo = 80;
    msHttpHost = "dashboard.safeandclean.be";
    
    /* create the http socket */
    miHttpSocketFd = socket(AF_INET, SOCK_STREAM, 0);
    if (miHttpSocketFd < 0)
    {
        printf("[ERROR] (%s) %s: Failed to open socket\n", getTimestamp(), __func__);
        return -1;
    }
    
    /* lookup the server ip address */
    msHttpServer = gethostbyname(msHttpHost);
    if (msHttpServer == NULL) 
    {
        printf("[ERROR] (%s) %s: No such host: %s\n", getTimestamp(), __func__, msHttpHost);
        return -1;
    }
    
    /* clear and fill in the server address structure */
    memset(&msHttpServerAddr, 0, sizeof(msHttpServerAddr));
    msHttpServerAddr.sin_family = AF_INET;
    msHttpServerAddr.sin_port = htons(miHttpPortNo);
    memcpy(&msHttpServerAddr.sin_addr.s_addr, msHttpServer->h_addr, msHttpServer->h_length);
    
    printf("[INFO] (%s) %s: Initialized http socket: s_addr=0x%x, h_addr=%s, h_length=0x%x\n", getTimestamp(), __func__, msHttpServerAddr.sin_addr.s_addr, msHttpServer->h_addr, msHttpServer->h_length);
    
    return 0;
}


/************** int httpSendRequest() *********************
    Uses the buffers 
    char msHttpTxMessage[HTTPMSGMAXSIZE]
    char msHttpRxMessage[HTTPMSGMAXSIZE]
************************************************************/
int httpSendRequest()
{
    int iBytesToProcess = strlen(msHttpTxMessage);
    int iBytesSent = 0;
    int iBytesReceived = 0;
    int iBytesCurrentlyProcessed;
    
    /* initialize the socket */
    httpSocketInit();
    
    /* connect the socket */
    int iResult;
    iResult = connect(miHttpSocketFd, (struct sockaddr *)&msHttpServerAddr, sizeof(msHttpServerAddr));
    if (iResult < 0)
    {
        int iErrsv = errno;
        printf("[ERROR] (%s) %s: Could not connect to socket 0x%x. Socket connect error code %i.\n", getTimestamp(), __func__, miHttpSocketFd, iErrsv);
        return -1;
    }
        
    /* send the request */
    iBytesCurrentlyProcessed = 0;
    do
    {
        iBytesCurrentlyProcessed = write(miHttpSocketFd, (char *)((uint32_t)msHttpTxMessage + (uint32_t)iBytesSent), iBytesToProcess - iBytesSent);
        if(iBytesCurrentlyProcessed < 0)
        {
            printf("[ERROR] (%s) %s: Could not write message %s to socket 0x%x. Socket write error code %i.\n", getTimestamp(), __func__, msHttpTxMessage, miHttpSocketFd, iBytesCurrentlyProcessed);
            return -1;
        }
        if(iBytesCurrentlyProcessed == 0)
        {
            break;
        }
        iBytesSent += iBytesCurrentlyProcessed;
    } while(iBytesSent < iBytesToProcess);
    
    printf("[INFO] (%s) %s: %i http request message bytes written to socket: %s\n", getTimestamp(), __func__, iBytesSent, msHttpTxMessage);
    
    /* receive the response */
    iBytesCurrentlyProcessed = 0;
    memset(msHttpRxMessage, 0, sizeof(msHttpRxMessage)); // clear buffer
    iBytesToProcess = sizeof(msHttpRxMessage) - 1;
    do
    {
        iBytesCurrentlyProcessed = read(miHttpSocketFd, (char *)((uint32_t)msHttpRxMessage + (uint32_t)iBytesReceived), iBytesToProcess - iBytesReceived);
        if(iBytesCurrentlyProcessed < 0)
        {
            printf("[ERROR] (%s) %s: Could not read response from socket 0x%x. Socket write error code %i.\n", getTimestamp(), __func__, miHttpSocketFd, iBytesCurrentlyProcessed);
            return -1;
        }
        if(iBytesCurrentlyProcessed == 0)
        {
            break;
        }
        iBytesReceived += iBytesCurrentlyProcessed;
    } while(iBytesReceived < iBytesToProcess);
    
    if(iBytesReceived == iBytesToProcess)
    {
        printf("[ERROR] (%s) %s: Receive buffer ran out of space. Max. number of bytes: %i.\n", getTimestamp(), __func__, HTTPMSGMAXSIZE);
        return -1;
    }
    
    close(miHttpSocketFd);
    return 0;
}

/******************* httpBuildRequestMsg *******************
    *) befor usage, int httpSocketInit() must be executed first.
    *) Is required for int httpSendRequest().
************************************************************/
void httpBuildRequestMsg(uint32_t I2CRxPayloadAddress, int I2CRxPayloadLength)
{
    char sUpstreamMsg[UPSTREAMBUFFERSIZE*2 + 1] = {0x00};
    char sParsedByte[3] = {0x00}; // adding '/0' character
    int iBytesCurrentlyProcessed = 0;
    
    // prepare upstream payload data
    do
    {
        sprintf(sParsedByte, "%02x", *((char *)(I2CRxPayloadAddress + iBytesCurrentlyProcessed)));
        strcat(sUpstreamMsg, sParsedByte);
        iBytesCurrentlyProcessed += 1;
    } while(iBytesCurrentlyProcessed < UPSTREAMBUFFERSIZE);
    
    msHttpMsgFmt = "GET /mobile/webhook?id=%s&time=%s&seqNumber=%s&ack=%s&data=%s HTTP/1.0\r\n\r\n";
    sprintf(msHttpTxMessage, msHttpMsgFmt, "SC-4GTEST", "1594998140", "207", "1", sUpstreamMsg);
}

int main(int argc, char* argv[]){
    signal(SIGINT, SIGHandler);
    runSlave();
    closeSlave();
    return 0;
}

/****************************************************/