#include "SACPrintUtils.h"

#include "string.h" /* memcpy, memset */
#include <time.h>
#include <stdio.h>
#include <stdlib.h> /* strtol */

#define ISVALIDHEXCHAR(x) ((x>=48 && x<=57) ||  (x>=65 && x<=70) || (x>=97 && x<=102))

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

/************** printGetUnixEpochTimeAsInt ******************
************************************************************/
long unsigned int printGetUnixEpochTimeAsInt()
{
    sRawTime = time(NULL);
    return (unsigned long int)sRawTime;
}


/******************* printSplitInBytes **********************
    If the input string is "\r36301f73deadbeef" and cSeparator = ','
    then the output will be:
        "36,30,1f,73,de,ad,be,ef,"
    
    Discards all leading non hex digit characters.
************************************************************/
char* printSplitByteStringInBytes(char *sByteString, char cSeparator)
{
    bool biMightBePrefix = true;
    bool biSeparatorAdded = false;
    memset((void *)msGenericStringBuffer, 0x00, GENERICSTRBUFFERSIZE);
    int iDestPointer = 0;
    int iLength = strlen(sByteString);
    int iHexStartIndex = -1;
    int i;
    
    for(i=0; i<iLength && i<GENERICSTRBUFFERSIZE; i+=1)
    {
        char cCurrChar = sByteString[i];
        if(ISVALIDHEXCHAR(cCurrChar))
        {
            // got a valid character.
            if(biMightBePrefix)
            {
                // this must be the first valid hex character
                biMightBePrefix = false;
                iHexStartIndex = i;
            }
            msGenericStringBuffer[iDestPointer] = cCurrChar;
            if (((i-iHexStartIndex)+1)%2 == 0)
            {
                // on even offsets from the HexStartIndex, insert a separator character.
                iDestPointer += 1;
                msGenericStringBuffer[iDestPointer] = cSeparator;
                biSeparatorAdded = true;
            }
            else
            {
                biSeparatorAdded = false;
            }
            iDestPointer += 1;
        }
        else
        {
            if(!biMightBePrefix)
            {
                // this is not part of the hex string anymore, break
                break;
            }
            else
            {
                // we might be in a prefix of some sort
            }
        }
    }
    
    if(biMightBePrefix)
    {
        // We never encountered a valid hex char. Ther is no value to return
        return NULL;
    }
    else
    {
        if (!biSeparatorAdded)
        {
            // If a separator character wasn't added at the last iteration, add one now.
            //iDestPointer += 1;
            msGenericStringBuffer[iDestPointer] = cSeparator;
        }
        return msGenericStringBuffer;
    }
}

/************** printParseHexStringToBytes ******************
    e.g. input string is: "\r36301f73deadbeef"
    returns uint8_t values parsed from string:
        uint8_t[]:{0x36,0x30,0x1f,0x73,0xde,0xad,0xbe,0xef}
    with 0x36 address bDestBuffer.
    Return int value is the number of bytes that were parsed.
************************************************************/
int printParseHexStringToBytes(char *sByteString, uint8_t *bDestBuffer, uint8_t bDestBufferSize)
{
    char asDelimiter[] = ","; // only one element alowed!!
    char *sBytesCommaSeparated = printSplitByteStringInBytes(sByteString, asDelimiter[0]);
    int iNParsedBytes = 0;
    char *pEnd;
    
    // load the string token function
    char *pTemp = strtok(sBytesCommaSeparated, asDelimiter);
    // first ',' has now been replaced by 0x00...
    while((pTemp != NULL) && (bDestBufferSize > iNParsedBytes))
	{
		bDestBuffer[iNParsedBytes] = strtol (pTemp, &pEnd, 16);
        iNParsedBytes += 1;
        pTemp = strtok(NULL, asDelimiter);
	}
    return iNParsedBytes;
}