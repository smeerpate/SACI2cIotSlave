#include "SACServerComms.h"
#include "SACPrintUtils.h"

#include "string.h" /* memcpy, memset */
#include <sys/socket.h> /* socket, connect */
#include <netinet/in.h> /* struct sockaddr_in, struct sockaddr */
#include <netdb.h> /* struct hostent, gethostbyname */
#include <openssl/ssl.h> /* for https, if not installed: "sudo apt-get install libssl-dev" */
#include <openssl/err.h>
#include "stdio.h"
#include "unistd.h"

#define UPSTREAMBUFFERSIZE      12
#define DOWNSTREAMBUFFERSIZE    32

/****************** private function prototypes *********************/
int httpSocketInit();
int httpWriteMsgToSocket(int iSocketFd, SSL *sSSLConn);
int httpReadRespFromSocket(int iSocketFd, SSL *sSSLConn);
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
/********************************************************************/


/************** int httpSendRequest() *********************
    Sends a http request stored in
    msHttpTxMessage[HTTPMSGMAXSIZE] and sebsequently
    receives the response into 
    msHttpRxMessage[HTTPMSGMAXSIZE]
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
        return -1;
    }
    #endif
    
    #if USESSL == 1
        /* send the request via SSL */
        httpWriteMsgToSocket(0, sSSLConn);
        /* receive the response via SSL */
        httpReadRespFromSocket(0, sSSLConn);
        SSL_shutdown(sSSLConn);
    #else
        /* send the request */
        httpWriteMsgToSocket(miHttpSocketFd, NULL);
        /* receive the response */
        httpReadRespFromSocket(miHttpSocketFd, NULL);
    #endif
    
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
    
    msHttpHost = "dashboard.safeandclean.be";
    
    /* create the http socket */
    miHttpSocketFd = socket(AF_INET, SOCK_STREAM, 0);
    if (miHttpSocketFd < 0)
    {
        printf("[ERROR] (%s) %s: Failed to open socket\n", printTimestamp(), __func__);
        return -1;
    }
    
    /* lookup the server ip address */
    msHttpServer = gethostbyname(msHttpHost);
    if (msHttpServer == NULL) 
    {
        printf("[ERROR] (%s) %s: No such host: %s\n", printTimestamp(), __func__, msHttpHost);
        return -1;
    }
    
    /* clear and fill in the server address structure */
    memset(&msHttpServerAddr, 0, sizeof(msHttpServerAddr));
    msHttpServerAddr.sin_family = AF_INET;
    msHttpServerAddr.sin_port = htons(miHttpPortNo);
    memcpy(&msHttpServerAddr.sin_addr.s_addr, msHttpServer->h_addr, msHttpServer->h_length);
    
    printf("[INFO] (%s) %s: Initialized http socket: s_addr=0x%x, h_addr=%s, h_length=0x%x\n", printTimestamp(), __func__, msHttpServerAddr.sin_addr.s_addr, msHttpServer->h_addr, msHttpServer->h_length);
    
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

/******************* httpBuildRequestMsg *******************
    *) befor usage, int httpSocketInit() must be executed first.
    *) Is required for int httpSendRequest().
************************************************************/
void httpBuildRequestMsg(uint32_t I2CRxPayloadAddress, int I2CRxPayloadLength)
{   
    char *pUpstreamDataString = printBytesAsHexString(I2CRxPayloadAddress, I2CRxPayloadLength, false, NULL);
    printf("[INFO] (%s) %s: Built upstream data string:\n\t\'%s\'\n", printTimestamp(), __func__, pUpstreamDataString);
        
    sprintf(msHttpTxMessage, "GET %s?id=%s&time=%s&seqNumber=%s&ack=%s&data=%s HTTP/1.1\r\nHost: %s\r\n\r\n", 
        "/mobile/webhook",              // path
        "SC-4GTEST",                    // id=
        "1594998140",                   // time=
        "207",                          // seqNumber=
        "1",                            // ack=
        pUpstreamDataString,            // data=
        "dashboard.safeandclean.be"     // Host:
        );
        
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
    return;
}