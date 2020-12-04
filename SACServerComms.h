#ifndef SACSERVERCOMMS_H
#define SACSERVERCOMMS_H

#include <stdbool.h>
#include <stdint.h>

#define HTTPMSGMAXSIZE          4096
#define USESSL                  1

int httpSocketInit();
int httpSendRequest();
void httpBuildRequestMsg(uint32_t I2CRxPayloadAddress, int I2CRxPayloadLength);
char* printBytesAsHexString(uint32_t startAddress, int length, bool addSeparator, const char * separator);
void sslInit();
void sslClose();

#endif