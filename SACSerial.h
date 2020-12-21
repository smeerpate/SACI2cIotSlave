#ifndef SACSERIAL_H
#define SACSERIAL_H

#include <stdint.h>

#define SERIAL_BUFFER_SIZE    128

typedef struct
{
    uint32_t control;                   // Write
    int rxCnt;                          // Read only
    char rxBuf[SERIAL_BUFFER_SIZE];     // Read only
    int txCnt;                          // Write
    char txBuf[SERIAL_BUFFER_SIZE];     // Write
} serial_xfer_t;

int serialInit();
int serialXfer(volatile serial_xfer_t *xfer);
int serialFlushFifoToRxBuffer(volatile serial_xfer_t *xfer);
int serialTransmitTxBuffer(volatile serial_xfer_t *xfer);
int serialTerminate();

#endif