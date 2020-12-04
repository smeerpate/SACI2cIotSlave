#include "SACStructs.h"
#include "string.h" /* memcpy, memset */

/****************** private function prototypes *********************/
void structsInitLastSendCmd();
/********************************************************************/

/******************** private global variables **********************/
static tCtrlSendCmd sLastSendCmd;
/********************************************************************/

void structsInit()
{
    structsInitLastSendCmd();
}

void structsInitLastSendCmd()
{
    memset((void *)&sLastSendCmd, 0x00, sizeof(tCtrlSendCmd));
}

tCtrlSendCmd *setLastSendCmd(void *pSourceData)
{
    memcpy((void *)sLastSendCmd.ui8, pSourceData, sizeof(tCtrlSendCmd));
    return &sLastSendCmd;
}

tCtrlSendCmd *getLastSendCmd()
{
    return &sLastSendCmd;
}