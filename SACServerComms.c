#include "SACServerComms.h"
#include "SACPrintUtils.h"
#include "SACStructs.h"

#include "string.h" /* memcpy, memset */
#include <stdlib.h> /* atoi */
#include <sys/socket.h> /* socket, connect */
#include <netinet/in.h> /* struct sockaddr_in, struct sockaddr */
#include <netdb.h> /* struct hostent, gethostbyname */
#include <openssl/ssl.h> /* for https, if not installed: "sudo apt-get install libssl-dev" */
#include <openssl/err.h>
#include "stdio.h"
#include "unistd.h"

#define UPSTREAMBUFFERSIZE      12
#define DOWNSTREAMBUFFERSIZE    32
#define MAXSERVERREPLYLINES     64

#define ADDUSERREPLYINREQUEST   0 //1
#define USERREPLYINREQUEST      "35291f03beefdead"

#define DEVICEIDSTRINGLENGTH	100

/****************** private function prototypes *********************/
int httpSocketInit();
int httpWriteMsgToSocket(int iSocketFd, SSL *sSSLConn);
int httpReadRespFromSocket(int iSocketFd, SSL *sSSLConn);
int httpParseReplyMsg(char *sRawMessage);
char* prunePayloadFromJSON(char *sJSON);
/********************************************************************/

/******************** private global variables **********************/
char *msHttpHost;
int miHttpPortNo;
//char *msHttpMsgFmt;
struct hostent *msHttpServer;
struct sockaddr_in msHttpServerAddr;
int miHttpSocketFd;
char msHttpTxMessage[HTTPMSGMAXSIZE] = {0x00};
char msHttpRxMessage[HTTPMSGMAXSIZE] = {0x00};
SSL_CTX *sSSLContext;
uint32_t muiSeqNr = 0;
static char sDeviceId[DEVICEIDSTRINGLENGTH] = {0};
/********************************************************************/


/************** int httpSendRequest() *********************
    Sends a http request stored in
    msHttpTxMessage[HTTPMSGMAXSIZE] and sebsequently
    receives the response into 
    msHttpRxMessage[HTTPMSGMAXSIZE]
    
    Also parses the reply message payload into the global
    sCtrlDeckedReply payload field.  
************************************************************/
int httpSendRequest()
{
    /* initialize the socket */
    httpSocketInit();
    
    /* connect the socket */
    int iResult;
    iResult = connect(miHttpSocketFd, (struct sockaddr *)&msHttpServerAddr, sizeof(msHttpServerAddr));
    if (iResult < 0)
    {
        int iErrsv = errno;
        printf("[ERROR] (%s) %s: Could not connect to socket 0x%x. Socket connect error code %i.\n", printTimestamp(), __func__, miHttpSocketFd, iErrsv);
        return -1;
    }
    
    #if USESSL == 1
    // create an SSL connection and attach it to the socket
    SSL *sSSLConn = SSL_new(sSSLContext);
    SSL_set_fd(sSSLConn, miHttpSocketFd);
    ERR_clear_error(); // clear error queue
    iResult = SSL_connect(sSSLConn);
    if (iResult != 1)
    {
        int iErrsv = SSL_get_error(sSSLConn, iResult);
        printf("[ERROR] (%s) %s: Could not create SSL connection. Error code %i. Return Code %i.\n\t%s\n", printTimestamp(), __func__, iErrsv, iResult, ERR_error_string(ERR_get_error(), NULL));
        close(miHttpSocketFd);
        return -1;
    }
    #endif
    
    #if USESSL == 1
        /* send the request via SSL */
        iResult = httpWriteMsgToSocket(0, sSSLConn);
        if (iResult < 0)
        {
            printf("[ERROR] (%s) %s: Could not write to SSL socket. Return Code = %i.\n", printTimestamp(), __func__, iResult);
            close(miHttpSocketFd);
            SSL_shutdown(sSSLConn);
            return -1;
        }
        /* receive the response via SSL */
        iResult = httpReadRespFromSocket(0, sSSLConn);
        if (iResult < 0)
        {
            printf("[ERROR] (%s) %s: Could not read from SSL socket. Return Code = %i.\n", printTimestamp(), __func__, iResult);
            close(miHttpSocketFd);
            SSL_shutdown(sSSLConn);
            return -1;
        }
        SSL_shutdown(sSSLConn);
    #else
        /* send the request */
        iResult = httpWriteMsgToSocket(miHttpSocketFd, NULL);
        if (iResult < 0)
        {
            printf("[ERROR] (%s) %s: Could not write to socket. Return Code = %i.\n", printTimestamp(), __func__, iResult);
            close(miHttpSocketFd);
            return -1;
        }
        /* receive the response */
        iResult = httpReadRespFromSocket(miHttpSocketFd, NULL);
        if (iResult < 0)
        {
            printf("[ERROR] (%s) %s: Could not read from socket. Return Code = %i.\n", printTimestamp(), __func__, iResult);
            close(miHttpSocketFd);
            return -1;
        }
    #endif
    
    iResult = httpParseReplyMsg(msHttpRxMessage);
    if (iResult < 0)
    {
        printf("[ERROR] (%s) %s: Failed to parse the server\'s reply message. Return Code = %i.\n", printTimestamp(), __func__, iResult);
        close(miHttpSocketFd);
        return -1;
    }
    
    close(miHttpSocketFd);
    return 0;
}


/******************* httpSocketInit *************************

************************************************************/
int httpSocketInit()
{
    /* first what are we going to send and where are we going to send it? */
    /* send a post to:
        https://dashboard.safeandclean.be/mobile/webhook?id={device}&time={time}&seqNumber={seqNumber}&ack={ack}&data={data}
    */
    #if USESSL == 1
        miHttpPortNo = 443;
    #else
        miHttpPortNo = 80;
    #endif
    
    msHttpHost = IOT_HOST;
    
    /* create the http socket */
    miHttpSocketFd = socket(AF_INET, SOCK_STREAM, 0);
    if (miHttpSocketFd < 0)
    {
        printf("[ERROR] (%s) %s: Failed to open socket for \'%s\'\n", printTimestamp(), __func__, IOT_HOST);
        return -1;
    }
    
    /* lookup the server ip address */
    msHttpServer = gethostbyname(msHttpHost);
    if (msHttpServer == NULL) 
    {
        printf("[ERROR] (%s) %s: No such host: \'%s\'\n", printTimestamp(), __func__, msHttpHost);
        return -1;
    }
    
    /* clear and fill in the server address structure */
    memset(&msHttpServerAddr, 0, sizeof(msHttpServerAddr));
    msHttpServerAddr.sin_family = AF_INET;
    msHttpServerAddr.sin_port = htons(miHttpPortNo);
    memcpy(&msHttpServerAddr.sin_addr.s_addr, msHttpServer->h_addr, msHttpServer->h_length);
    
    printf("[INFO] (%s) %s: Initialized http socket: s_addr=0x%x, h_length=0x%x\n", printTimestamp(), __func__, msHttpServerAddr.sin_addr.s_addr, msHttpServer->h_length);
    
    return 0;
}

/************* int httpWriteMsgToSocket *********************
    Uses the buffer: 
    char msHttpTxMessage[HTTPMSGMAXSIZE]
************************************************************/
int httpWriteMsgToSocket(int iSocketFd, SSL *sSSLConn)
{
    int iBytesCurrentlyProcessed = 0;
    int iBytesToProcess = strlen(msHttpTxMessage);
    int iBytesSent = 0;
    
    do
    {
        #if USESSL == 1
            iBytesCurrentlyProcessed = SSL_write(sSSLConn, (char *)((uint32_t)msHttpTxMessage + (uint32_t)iBytesSent), iBytesToProcess - iBytesSent);
        #else
            iBytesCurrentlyProcessed = write(iSocketFd, (char *)((uint32_t)msHttpTxMessage + (uint32_t)iBytesSent), iBytesToProcess - iBytesSent);
        #endif
        if(iBytesCurrentlyProcessed < 0)
        {
            printf("[ERROR] (%s) %s: Could not write message %s to socket 0x%x. Socket write error code %i.\n", printTimestamp(), __func__, msHttpTxMessage, miHttpSocketFd, iBytesCurrentlyProcessed);
            return -1;
        }
        if(iBytesCurrentlyProcessed == 0)
        {
            break;
        }
        iBytesSent += iBytesCurrentlyProcessed;
    } while(iBytesSent < iBytesToProcess);
    
    printf("[INFO] (%s) %s: %i http request message bytes written to socket:\n"
            "******* ASCII begin *******\n"
            "%s\n"
            "******** ASCII end ********\n"
            , printTimestamp(), __func__, iBytesSent,
            msHttpTxMessage
            );
    return 0;
}

/************ int httpReadRespFromSocket ********************
    Uses the buffer: 
    char msHttpRxMessage[HTTPMSGMAXSIZE]
************************************************************/
int httpReadRespFromSocket(int iSocketFd, SSL *sSSLConn)
{
    int iBytesReceived = 0; 
    int iBytesCurrentlyProcessed = 0;
    int iBytesToProcess = sizeof(msHttpRxMessage) - 1;
    
    memset(msHttpRxMessage, 0, sizeof(msHttpRxMessage)); // clear buffer
    do
    {
        #if USESSL == 1
            iBytesCurrentlyProcessed = SSL_read(sSSLConn, (char *)((uint32_t)msHttpRxMessage + (uint32_t)iBytesReceived), iBytesToProcess - iBytesReceived);
        #else
            iBytesCurrentlyProcessed = read(iSocketFd, (char *)((uint32_t)msHttpRxMessage + (uint32_t)iBytesReceived), iBytesToProcess - iBytesReceived);
        #endif    
        if(iBytesCurrentlyProcessed < 0)
        {
            printf("[ERROR] (%s) %s: Could not read response from socket 0x%x. Socket write error code %i.\n", printTimestamp(), __func__, miHttpSocketFd, iBytesCurrentlyProcessed);
            return -1;
        }
        if(iBytesCurrentlyProcessed == 0)
        {
            break;
        }
        iBytesReceived += iBytesCurrentlyProcessed;
    } while(iBytesReceived < iBytesToProcess);
    
    if(iBytesReceived == iBytesToProcess)
    {
        printf("[ERROR] (%s) %s: Receive buffer ran out of space. Max. number of bytes: %i.\n", printTimestamp(), __func__, HTTPMSGMAXSIZE);
        return -1;
    }
    
    /* show the stuff that we have received */
    
    printf("[INFO] (%s) %s: %i http request message bytes received in socket:\n"
            "******* ASCII begin *******\n"
            "%s\n"
            "******** ASCII end ********\n"
            , printTimestamp(), __func__, iBytesReceived,
            msHttpRxMessage
            );
    return 0;
}

/******************* httpBuildRequestMsg ********************
    *) befor usage, int httpSocketInit() must be executed first.
    *) Is required for int httpSendRequest().
************************************************************/
void httpBuildRequestMsg(uint32_t I2CRxPayloadAddress, int I2CRxPayloadLength)
{
    gethostname(sDeviceId, DEVICEIDSTRINGLENGTH - 1);
    printf("[INFO] (%s) %s: Device ID is \'%s\'.\n", printTimestamp(), __func__, sDeviceId);
    
    char *pUpstreamDataString = printBytesAsHexString(I2CRxPayloadAddress, I2CRxPayloadLength, false, NULL);
    printf("[INFO] (%s) %s: Built upstream data string:\n\t\'%s\'\n", printTimestamp(), __func__, pUpstreamDataString);
    
    tServerRequest *sRequest = getLastServerRequest();
    sprintf(sRequest->host, "%s", IOT_HOST);
    sprintf(sRequest->path, "%s", IOT_PATH);
    sprintf(sRequest->deviceId, "%s", sDeviceId/*IOT_DEVICEID*/);
    //sprintf(sRequest->deviceId, "%s", sDeviceId);
    sRequest->time = printGetUnixEpochTimeAsInt();//1594998140;
    sRequest->seqNr = muiSeqNr;
    muiSeqNr += 1;
    sRequest->ack = 1;
    sRequest->data = pUpstreamDataString;
        
    sprintf(msHttpTxMessage, "GET %s?id=%s&time=%lu&seqNumber=%u&ack=%u&data=%s"
                            #if ADDUSERREPLYINREQUEST == 1
                                "&response=%s"
                            #endif
                            " HTTP/1.1\r\nHost: %s\r\n\r\n", 
        sRequest->path,           // path
        sRequest->deviceId,       // id=
        sRequest->time,           // time=
        sRequest->seqNr,          // seqNumber=
        sRequest->ack,            // ack=
        sRequest->data,           // data=
        #if ADDUSERREPLYINREQUEST == 1
            USERREPLYINREQUEST, // Add your custom reply here, only used for debugging!
        #endif
        sRequest->host           // Host:
        );
        
}

/******************* httpParseReplyMsg **********************
1) Server reply must start with "HTTP/1.1 " (first line).
2) reply paylod is two lines further than the first empty line
    (an empty line should only contain \r\n)

Example server reply:
    HTTP/1.1 200 OK\r\n
    Date: Sat, 05 Dec 2020 13:10:57 GMT\r\n
    Server: Apache/2.4.29 (Ubuntu)\r\n
    Cache-Control: max-age=0, must-revalidate, private\r\n
    Expires: Sat, 05 Dec 2020 13:10:57 GMT\r\n
    Vary: Accept-Encoding\r\n
    Transfer-Encoding: chunked\r\n
    Content-Type: text/html; charset=UTF-8\r\n
    \r\n
    10\r\n
    36301f73deadbeef\r\n
    0\r\n
    \r\n
    \r\n
************************************************************/
int httpParseReplyMsg(char *sRawMessage)
{
    //int iInitLength = strlen(sRawMessage);
    int iNTokens = 0;
	char asDelimiters[] = "\n";
    char *apLines[MAXSERVERREPLYLINES] = {NULL};
    int iReplyCode = -1;
    int iBlankLineIndex = -1;
    int iPayloadLineIndex = -1;
    
    //printf("\t# in hex: %s #\n", printBytesAsHexString((uint32_t)sRawMessage, iInitLength, true, ", "));
    
    // load the string token function
    char *pTemp = strtok(sRawMessage, asDelimiters);
    // first '\n' has now been replaced by 0x00...
    
    while(pTemp != NULL && iNTokens < MAXSERVERREPLYLINES)
	{
		// printf("\t# %s #\n", pTemp);
        apLines[iNTokens] = pTemp;
        pTemp = strtok(NULL, asDelimiters);
        iNTokens += 1;
	}
    //printf("[INFO] %s: Found %i tokens.\n", __func__, iNTokens);
    //printf("\t# in hex: %s #\n", printBytesAsHexString((uint32_t)sRawMessage, iInitLength, true, ", "));
    
    // look for the phrase "HTTP/1.1 "
    // it should be on line index 0
    if(apLines[0] != NULL)
    {
        if(strncmp(apLines[0], "HTTP/1.1 ", 9) == 0)
        {
            iReplyCode = atoi((char *)(apLines[0] + 9));
            printf("[INFO] %s: Found reply code: %i\n", __func__, iReplyCode);
        }
        else
        {
            // Incorrect header, expected "HTTP/1.1 ".
            printf("[ERROR] %s: Incorrect header, expected \'HTTP/1.1 \', got:\n\t%s\n", __func__, apLines[0]);
            memset(msHttpRxMessage, 0, sizeof(msHttpRxMessage));
            return -1;
        }
    }
    else
    {
        // No tokens ("\n") found in sever reply.
        printf("[ERROR] %s: Found no tokens in server reply.\n", __func__);
        memset(msHttpRxMessage, 0, sizeof(msHttpRxMessage));
        return -2;
    }
    
    // Check the response code (200 = ok, 204 = ok but no payload).
    if(iReplyCode != 200 && iReplyCode != 204)
    {
        // server reply code not supported
        printf("[ERROR] %s: Server reply code %i not supported.\n", __func__, iReplyCode);
        memset(msHttpRxMessage, 0, sizeof(msHttpRxMessage));
        return -(iReplyCode);
    }
    
    // look for the index in the substring array of the next blank line
    int i = 0;
    for(i=0; i<iNTokens; i+=1)
    {
        if(strcmp(apLines[i], "\r") == 0)
        {
            // Found blank line at index i
            iBlankLineIndex = i;
            printf("[INFO] %s: Found first blank line at substring index: %i\n", __func__, iBlankLineIndex);
            break;
        }
    }
    if(iBlankLineIndex < 0)
    {
        // No blank lines found in sever reply. Suspecting bad response
        printf("[ERROR] %s: Found no blank lines in server reply.\n", __func__);
        memset(msHttpRxMessage, 0, sizeof(msHttpRxMessage));
        return -3;
    }
    
    // If we asked for content (ack=1) and response code = 200, the server should return content after the first linefeed.
    if(iReplyCode == 200)
    {
        // get payload
        iPayloadLineIndex = iBlankLineIndex + 2;
        apLines[iPayloadLineIndex] = prunePayloadFromJSON(apLines[iPayloadLineIndex]);
        printf("[INFO] %s: Found payload line at substring index %i with content:\n\t%s\n", __func__, iPayloadLineIndex, apLines[iPayloadLineIndex]);
    }
    if (iReplyCode == 204)
    {
        printf("[INFO] %s: Reply code 204, server returned no payload.\n", __func__);
    }
    
    
    //printf("\t\'%s\'\n", printSplitByteStringInBytes(apLines[iPayloadLineIndex], ','));
    tCtrlDeckedReply *pReplyForController = getCtrlDeckedReply();
    if(pReplyForController == NULL)
    {
        int iNBytesParsed = printParseHexStringToBytes(apLines[iPayloadLineIndex], pReplyForController->payload, STRUCTS_DECKEDREPLYPAYLOADSIZE);
        printf("[INFO] (%s) %s: parsed %d bytes\n", printTimestamp(), __func__, iNBytesParsed);
        printf("\t# Bytes (HEX): %s\n", printBytesAsHexString((uint32_t)pReplyForController->payload, STRUCTS_DECKEDREPLYPAYLOADSIZE, true, ", "));
    }
    else
    {
        printf("[WARNING] %s: Failed to get Decked Reply with data for SAC arduino controller.\n", __func__);
    }
    
    // second line after the blank line is payload.
    // it should be 16 characters long
    
    // look for two blank lines at the end of the message
    // it should start two lines further.
    
    // clear the original message, we're done with it + it's been corrupted by strok
    memset(msHttpRxMessage, 0, sizeof(msHttpRxMessage));
    return 0;
}

/*
    Example http response from server: 
    {"SC-4GTEST":{"downlinkData":"360f1f73deadbeef"}}
    This function isolates the payload: 360f1f73deadbeef.
    returns a pointer to the start of the payload.
    put a \0 character directly after the payload
    Function is destructive.
*/
char* prunePayloadFromJSON(char *sJSON)
{
    char *pReturnPointer = 0;
    // look for the first occurance of '}'
    pReturnPointer = strchr(sJSON, '}');
    // go back 17 positions
    pReturnPointer = pReturnPointer - 17;
    // replace index 16 with \0
    *(pReturnPointer + 16) = '\0';
    
    return pReturnPointer;
}

/*********************** sslInit ****************************

************************************************************/
void sslInit()
{
    // initialize OpenSSL - do this once and stash ssl_ctx in a global var
    SSL_load_error_strings();
    SSL_library_init();
    sSSLContext = SSL_CTX_new(SSLv23_client_method());
}

/*********************** sslClose ***************************

************************************************************/
void sslClose()
{
    SSL_CTX_free(sSSLContext);
    return;
}
