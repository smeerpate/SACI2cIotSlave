#include "SACStructs.h"
#include "string.h" /* memcpy, memset */

/****************** private function prototypes *********************/
void structsInitLastSendCmd();
void structsInitLastReadEnaCmd();
void structsInitLastServerReply();
void structsInitLastServerRequest();
/********************************************************************/

/******************** private global variables **********************/
static tCtrlSendCmd sLastSendCmd;
static tCtrlReadEnaCmd sLastReadEnaCmd;
static tServerRequest sLastServerRequest;
static tServerReply sLastServerReply;
/********************************************************************/

void structsInit()
{
    structsInitLastSendCmd();
    structsInitLastReadEnaCmd();
    structsInitLastServerReply();
    structsInitLastServerRequest();
}

/******** Initializers **********/
void structsInitLastSendCmd()
{
    memset((void *)&sLastSendCmd, 0x00, sizeof(tCtrlSendCmd));
}

void structsInitLastReadEnaCmd()
{
    memset((void *)&sLastReadEnaCmd, 0x00, sizeof(tCtrlReadEnaCmd));
}

void structsInitLastServerReply()
{
    memset((void *)&sLastServerReply, 0x00, sizeof(tServerReply));
}

void structsInitLastServerRequest()
{
    memset((void *)&sLastServerRequest, 0x00, sizeof(tServerRequest));
}
/********************************/

/*********** Setters ************/
tCtrlSendCmd *setLastSendCmd(void *pSourceData)
{
    memcpy((void *)sLastSendCmd.ui8, pSourceData, sizeof(tCtrlSendCmd));
    return &sLastSendCmd;
}

tCtrlReadEnaCmd *setLastReadEnaCmd(void *pSourceData)
{
    memcpy((void *)sLastReadEnaCmd.ui8, pSourceData, sizeof(tCtrlReadEnaCmd));
    return &sLastReadEnaCmd;
}
/********************************/

/*********** Getters ************/
tCtrlSendCmd *getLastSendCmd()
{
    return &sLastSendCmd;
}

tCtrlReadEnaCmd *getLastReadEnaCmd()
{
    return &sLastReadEnaCmd;
}

tServerReply *getLastServerReply()
{
    return &sLastServerReply;
}

tServerRequest *getLastServerRequest()
{
    return &sLastServerRequest;
}
/********************************/