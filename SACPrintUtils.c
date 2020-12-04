#include "SACPrintUtils.h"

#include "string.h" /* memcpy, memset */
#include <time.h>
#include <stdio.h>

static char msTimestampBuffer[TIMESTAMPBUFFERSIZE] = {0x00};
char msGenericStringBuffer[GENERICSTRBUFFERSIZE] = {0x00};
time_t sRawTime;

/********************** printTimestamp ***********************
    Makes use of and overwrites the msTimestampBuffer.
    prints a timestamp like this:
        2020-12-04 14:13:32
************************************************************/
char* printTimestamp()
{
    memset((void *)msTimestampBuffer, 0x00, TIMESTAMPBUFFERSIZE);
    struct tm* tm_info;    
    sRawTime = time(NULL);
    
    tm_info = localtime(&sRawTime);
    strftime(msTimestampBuffer, 26, "%Y-%m-%d %H:%M:%S", tm_info);
    return msTimestampBuffer;
}

/****************** printBytesAsHexString *******************
    Makes use of and overwrites the msGenericStringBuffer.
    return a pointer to the msGenericStringBuffer.
************************************************************/
char* printBytesAsHexString(uint32_t startAddress, int length, bool addSeparator, const char * separator)
{
    char sParsedByte[GENERICSTRBUFFERSIZE] = {0x00};
    memset((void *)msGenericStringBuffer, 0x00, GENERICSTRBUFFERSIZE);
    int iBytesCurrentlyProcessed = 0;
    
    do
    {
        if(!addSeparator)
        {
            sprintf(sParsedByte, "%02x", *((char *)(startAddress + iBytesCurrentlyProcessed)));
        }
        else
        {
            sprintf(sParsedByte, "%02x%s", *((char *)(startAddress + iBytesCurrentlyProcessed)), separator);
        }
        strcat(msGenericStringBuffer, sParsedByte);
        iBytesCurrentlyProcessed += 1;
    } while(iBytesCurrentlyProcessed < length);
    
    return msGenericStringBuffer;
}