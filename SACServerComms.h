#ifndef SACSERVERCOMMS_H
#define SACSERVERCOMMS_H

#include <stdbool.h>
#include <stdint.h>

#define HTTPMSGMAXSIZE          4096
#define USESSL                  1
#define IOT_FRMSTARTTAG         '#'
#define IOT_FRMENDTAG           '\n'

int httpSendRequest();
void httpBuildRequestMsg(uint32_t I2CRxPayloadAddress, int I2CRxPayloadLength);
void sslInit();
void sslClose();

#endif