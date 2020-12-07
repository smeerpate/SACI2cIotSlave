#ifndef SACPRINTUTILS_H
#define SACPRINTUTILS_H

#include <stdbool.h>
#include <stdint.h>

#define TIMESTAMPBUFFERSIZE     64
#define GENERICSTRBUFFERSIZE    256

char* printTimestamp();
char* printBytesAsHexString(uint32_t startAddress, int length, bool addSeparator, const char * separator);
long unsigned int printGetUnixEpochTimeAsInt();
char* printSplitByteStringInBytes(char *sByteString, char cSeparator);
int printParseHexStringToBytes(char *sByteString, uint8_t *bDestBuffer, uint8_t bDestBufferSize);

#endif