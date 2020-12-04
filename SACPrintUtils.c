#include "SACPrintUtils.h"

#include "string.h" /* memcpy, memset */
#include <time.h>

static char msTimestampBuffer[TIMESTAMPBUFFERSIZE] = {0x00};
time_t sRawTime;

/*
    Makes use of and overwrites the msTimestampBuffer.
*/
char* printTimestamp()
{
    memset((void *)msTimestampBuffer, 0x00, TIMESTAMPBUFFERSIZE);
    struct tm* tm_info;    
    sRawTime = time(NULL);
    
    tm_info = localtime(&sRawTime);
    strftime(msTimestampBuffer, 26, "%Y-%m-%d %H:%M:%S", tm_info);
    return msTimestampBuffer;
}