#ifndef SACSERVERCOMMS_H
#define SACSERVERCOMMS_H

#include <stdbool.h>
#include <stdint.h>

#define HTTPMSGMAXSIZE          4096
#define USESSL                  1
#define IOT_FRMSTARTTAG         '#'
#define IOT_FRMENDTAG           '\n'
#define IOT_HOST                "safeandclean.dieterprovoost.be"//"dashboard.safeandclean.be" // TODO: assert that string length is <= than STRUCTS_SERVREQ_MAXSTRSIZE
#define IOT_PATH                "/mobile/webhook" // Todo assert that string length is <= than STRUCTS_SERVREQ_MAXSTRSIZE
#define IOT_DEVICEID            "SC-4GTEST" // Todo assert that string length is <= than STRUCTS_SERVREQ_MAXSTRSIZE

int httpSendRequest();
void httpBuildRequestMsg(uint32_t I2CRxPayloadAddress, int I2CRxPayloadLength);
void sslInit();
void sslClose();

#endif