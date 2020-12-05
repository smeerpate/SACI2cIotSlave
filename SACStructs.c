#include "SACStructs.h"
#include "string.h" /* memcpy, memset */

/****************** private function prototypes *********************/
void structsInitLastSendCmd();
void structsInitLastReadEnaCmd();
/********************************************************************/

/******************** private global variables **********************/
static tCtrlSendCmd sLastSendCmd;
static tCtrlReadEnaCmd sLastReadEnaCmd;
/********************************************************************/

void structsInit()
{
    structsInitLastSendCmd();
    structsInitLastReadEnaCmd();
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
/********************************/