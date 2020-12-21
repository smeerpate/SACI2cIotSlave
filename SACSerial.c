#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <string.h> // needed for memset

#include "SACSerial.h"
#include "SACPrintUtils.h"
#include "SACServerComms.h"


static struct termios sTio;
static int iTtyFd = 0;
static char *sPortName = "/dev/ttyACM0";


int serialInit()
{
    int iResult = 0;
    
    memset(&sTio, 0, sizeof(sTio));
    sTio.c_iflag = 0;
    sTio.c_oflag = 0;
    sTio.c_cflag = CS8 | CREAD | CLOCAL;           // 8n1, see termios.h for more information
    sTio.c_lflag = 0;
    sTio.c_cc[VMIN] = 0; // number of bytes needed for read to return (only in blocking mode?)
    sTio.c_cc[VTIME] = 0; // integer value in "deciseconds" :-)
    
    // TODO: ACM* number might change: 0, 1, ...
    iTtyFd = open(sPortName, O_RDWR);        // O_NONBLOCK might override VMIN and VTIME, so read() may return immediately.
    if(iTtyFd <= -1)
    {
        // Failed to open port.
        printf("[ERROR] (%s) %s: Failed to open serial port.\n", printTimestamp(), __func__);
        return -1;
    }
    cfsetospeed(&sTio, B115200); // output speed 115200 baud, B115200, B57600, or B9600
    cfsetispeed(&sTio, B115200); // input speed 115200 baud
    
    iResult = tcsetattr(iTtyFd, TCSANOW, &sTio); // Set attributes immediately.
    
    // Empty Rx and Tx fifo's
    iResult = tcflush(iTtyFd, TCIOFLUSH);
    
    printf("[INFO] (%s) %s: Listening on serial port %s.\n", printTimestamp(), __func__, sPortName);
    
    return 0;
}


int serialXfer(volatile serial_xfer_t *xfer)
{
    // unsigned char c;
    // int iRead;
    // int iRxBuffPointer = 0;
    
    // if(iTtyFd <= 0)
    // {
        // printf("[ERROR] (%s) %s:Failed to open serial port.\n", printTimestamp(), __func__);
        // return -1;
    // }
    
    // // Get all data from the fifo (if any) and write into RxBuffer starting from idx 0.
    // iRead = read(iTtyFd, &c, 1);
    // if(iRead > 0)
    // {
        // while(iRead > 0)
        // {
            // printf("Serial read #%d: 0x%02x\n", iRxBuffPointer, c);
            // xfer->rxBuf[iRxBuffPointer] = c;
            // iRxBuffPointer += 1; // Not a huge fan of post increment.
            // iRead = read(iTtyFd, &c, 1);
        // }
        // xfer->rxCnt = iRxBuffPointer;
    // }
    //printf("[INFO] (%s) %s:Returning to main state machine.\n", printTimestamp(), __func__);   
    return 0;
}

int serialFlushFifoToRxBuffer(volatile serial_xfer_t *xfer)
{
    unsigned char c;
    int iRead;
    int iRxBuffPointer = 0;
    
    if(iTtyFd <= 0)
    {
        printf("[ERROR] (%s) %s:Failed to open serial port.\n", printTimestamp(), __func__);
        return -1;
    }
    
    // Get all data from the fifo (if any) till next IOT_FRMENDTAG and write into RxBuffer starting from idx 0.
    iRead = read(iTtyFd, &c, 1);
    if(iRead > 0)
    {
        while(iRead > 0)
        {
            //printf("Serial read #%d: 0x%02x\n", iRxBuffPointer, c);
            xfer->rxBuf[iRxBuffPointer] = c;
            iRxBuffPointer += 1; // Not a huge fan of post increment.
            if(c == IOT_FRMENDTAG)
                break;
            iRead = read(iTtyFd, &c, 1);
        }
    }
    xfer->rxCnt = iRxBuffPointer;
    //printf("[INFO] (%s) %s:Returning to main state machine.\n", printTimestamp(), __func__);   
    return 0;
}

int serialTransmitTxBuffer(volatile serial_xfer_t *xfer)
{
    unsigned char c;
    int iTxBuffPointer = 0;
    
    while(xfer->txCnt > iTxBuffPointer)
    {
        c = xfer->txBuf[iTxBuffPointer];
        write(iTtyFd, &c, 1);
        iTxBuffPointer += 1;
    }
    return 0;
}

int serialTerminate()
{
    if(iTtyFd)
        close(iTtyFd);
        
    return 0;
}