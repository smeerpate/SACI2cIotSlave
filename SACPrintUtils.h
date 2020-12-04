#ifndef SACPRINTUTILS_H
#define SACPRINTUTILS_H

#include <stdbool.h>
#include <stdint.h>

#define TIMESTAMPBUFFERSIZE     64
#define GENERICSTRBUFFERSIZE    256

char* printTimestamp();
char* printBytesAsHexString(uint32_t startAddress, int length, bool addSeparator, const char * separator);

#endif