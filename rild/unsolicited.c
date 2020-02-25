/****************************************************************************
  unsolicited.c
  Author : kpkang@gmail.com
*****************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "agent.h"
#include "m2mdbg.h"
#include "utils.h"
#include "clientIF.h"
#include "oemlayer.h"

#include "unsolicited.h"


/*
+QIND: "POWER",1

+CPIN: READY

+QIND: "USIM",1

+CFUN: 1

+QIND: "SMS",1

+QIND: "PB",1
====================
+QIND: "POWER",2

+QIND: "POWER",0

*/

const result_info_t urc_table[] =
{
  {"+CGREG: 1,",     10, urc_cgreg},
  {"+CPIN: ",         7, urc_cpin}, 
  {"+QIND: ",         7, urc_qind},
  {"+CMTI: \"ME\",", 12, urc_cmti},
  {"+CFUN: ",         7, urc_cfun}, 
  {"+CLIP: ",         7, urc_clip},
  {"+QIOPEN: ",       9, urc_qiopen},
  {"+QIURC: ",        8, urc_qiurc},  //  +QIURC: "recv",0,4
  {"RING",            4, urc_ring},  
  {"CONNECT",         7, NULL}, 
  {"NO CARRIER",     10, urc_no_carrier},
  {"^DSCI: ",         7,    urc_dsci },
  /*
  {"BUSY",            4, NULL},
  {"+QSTKURC: ",     10, NULL},
  {"+QCNC: ",         7, NULL}, 
  */
};



int ParseUnsolRes(char *pResult)
{
  const int  urc_max_cnt = sizeof(urc_table) / sizeof(urc_table[0]);
  int index;

  for (index = 0; index < urc_max_cnt; index++)
  {
      if (0 == strncmp(pResult, urc_table[index].strCode, urc_table[index].lenCode ))
          break;
  }

  if (urc_max_cnt == index)
      return -1;

  if (urc_table[index].result)
  {
    return urc_table[index].result(pResult, urc_table[index].lenCode);
  }
  
  return 0;
}


int urc_qiopen(char * pResult, int nIndex)
{
#ifdef SUPPORT_TCP_CMD    
  char * pToken = strrchr(pResult, ',');
  if (pToken){
    int nErrn, nSock, nPID;

    *pToken = 0;
    nErrn = atoi(++pToken);
    nSock = atoi(&pResult[ nIndex ]);

    if (nSock == 0) {
      if (0 != nErrn) {  
        // +QIOPEN: 0,563  ; socket Identity has been used
        // +QIOPEN: 0,566  ; socket connect failed 
        // +QIOPEN: 0,561  ; Open PDP context failed
        nPID = GetPidBySockId(nSock);
        TCPClosed(nPID, nErrn);
        if (561 == nErrn) {
          SendCommand(AT_QCPS, TRUE, NULL);
        }
      } else {
        nPID = GetPidBySockId(nSock);
        SetTCPState(nPID, TCP_Connected, 0);
      }
    }
  }
#endif
  return 0;
}

int urc_qiurc(char * pResult, int nIndex)
{
  //+QIURC: "recv",0,4
  //+QIURC: "closed",0
  //+QIURC: "pdpdeact",1
#ifdef SUPPORT_TCP_CMD  
  const   char strDelimit_Param[] = ",\"";

  char *pToken = strtok(&pResult[ nIndex ], strDelimit_Param);

  if (!strcmp(pToken, "recv")){
    //char  sText[1460];
    int   nSock = -1, nSize;
    char *pSize, *pSock = strtok(NULL, strDelimit_Param);
    if (pSock) pSize = strtok(NULL, strDelimit_Param); 

    if (nSock && pSize){
      nSock = atoi(pSock);
      nSize = atoi(pSize);
    }

    g_nTCPRecvLen = nSize;
    g_pidOfTCPRecv = GetPidBySockId(nSock);
  } 
  else if (!strcmp(pToken, "closed")) {
    pToken = strtok(NULL, strDelimit_Param); 
    if (pToken){
      int nSock = atoi(pToken);
      int pid = GetPidBySockId(nSock);
      TCPClosed(pid, 0);
    }
  } 
  else if (!strcmp(pToken, "pdpdeact")) {
    TCPClosed(0, ERR_TCPIP_PDP_DEACTIVATED);
  }
#endif
  return 0;
}

int urc_cgreg(char * pResult, int nIndex)
{
  char *reg_txt = strchr_nth_next(pResult, ',', 1);
  int  reg_status = REG_UNKNOWN;

  if (reg_txt)
  {
    reg_status = reg_txt[0] - '0';
    if (reg_status != REG_REGISTERED || reg_status != REG_ROAMING) {
      SendCommand(AT_QCDS, TRUE, NULL);
    }
  }
  
  if (REG_UNKNOWN != reg_status)
  {
    g_eRegistration = reg_status;
    SetRegistration(g_eRegistration);
    if (REG_REJECTED == reg_status)
    {
      SendAlertMsg(0, ERR_AUTHENTICATION);
    }
  }
  return 0;
}

int urc_cpin(char * pResult, int nIndex)
{
  char *pToken;

  //DEBUG(MSG_HIGH, "urc_cpin pResult %s\n", pResult);
  //DEBUG(MSG_HIGH, "m_nATCommandID %d, m_nUSIMState %d m_nHotSwapStat %d\n", m_nATCommandID, m_nUSIMState, g_eHotSwapStat);
  
  if (GetCurrCommand() == AT_CPIN) {
    return -1;
  }

  pToken = &pResult[ nIndex ];

  if (0 == strcmp(pToken, "READY"))
  {
#ifdef SUPPORT_SIM_HOTSWAP               
    if (g_eHotSwapStat == HOTSWAP_REMOVED){
      g_eHotSwapStat = HOTSWAP_INSERTED;
    }
#endif                
    InitModem(SIM_READY);
    g_nSimNotInsertedCnt = 0;
  }
#ifdef SUPPORT_SIM_HOTSWAP              
  else if (0 == strcmp(pToken, "NOT READY"))
  {
    if (SIM_READY == g_eSIMStatus)
    {
      g_eSIMStatus    = SIM_NOT_INSERTED;
      g_eRegistration = REG_UNKNOWN;
      SetRegistration(g_eRegistration);
      m_nStatusTimer  = TIMER_MODEM_STATUS;
      g_eHotSwapStat  = HOTSWAP_REMOVED;
      g_nSimNotInsertedCnt = 0;
    }
  }
#endif

  return 0;
}

int urc_qind(char * pResult, int nIndex)
{
  //+QIND: "RECOVERY","START"
  //+QIND: "RECOVERY","END"
  if (!strncmp(&pResult[nIndex], "\"RECOVERY\"", 10))
  {
    nIndex = nIndex + 10;
    if (strstr(&pResult[nIndex], (char *)"START")){
      g_nRecoveryCountDown = 30;
    } else if (strstr(&pResult[nIndex], (char *)"END")){
      g_nRecoveryCountDown = 10;
    }
  }
  //+QIND: "POWER",1

  else if (!strncmp(&pResult[nIndex], "\"POWER\"", 10))
  {
    char *cPwr = strrchr_nth_next(pResult, ',', 1);
    if (cPwr && '1' == *cPwr && SIM_NONE != g_eSIMStatus) {
      ResetModem(RESET_NONE_CAUSE);
    }
  }
  
  return 0;
}

int urc_cmti(char * pResult, int nIndex)
{
  int msgIndex = atoi(&pResult[ nIndex ]);

  SendCommand(AT_CMGR, FALSE, "AT+CMGR=%d\r", msgIndex);
  SendCommand(AT_CMGD_INDEX, FALSE, "AT+CMGD=%d\r", msgIndex);

  return 0;
}

int urc_cfun(char * pResult, int nIndex)
{


  return 0;
}

int urc_dsci(char * pResult, int nIndex)
{
/* MT
^DSCI: 3,1,4,0,01024313631,128     => incoming
^DSCI: 3,1,3,0,01024313631,128     => answer 
^DSCI: 3,1,6,0,01024313631,128    => hangup
*/
/* MO
^DSCI: 3,0,2,0,01024313631,129
^DSCI: 3,0,7,0,01024313631,129
^DSCI: 3,0,3,0,01024313631,129
^DSCI: 3,0,6,0,01024313631,129
*/
  char *direction = strchr_nth_next(&pResult[nIndex], ',', 1);
  char *state = strchr_nth_next(&pResult[nIndex], ',', 2);
  if (!direction || !state ) return 0;

  

  if (*direction == '1')  // MT... only interest in Caller line number
  {
    if (*state == '4')
    {
      int len = 0;
      
      char *number = strchr_nth_next(&pResult[nIndex],  ',', 4);

      if (number) DEBUG(MSG_MID, "%s number\n", number);

      if (number)
      {
        char * comma = strchr_nth(number,  ',', 1);
        if (comma) 
        {
          *comma = 0;
          len = strlen(number);
        }
      }

      if (len > 0 && len < MAX_NUMBER_LENGTH)
      {
        SendCNIPInfo(number);
      }
    }
  }
  else if (*direction == '0')  // MO only interest in remote party answer
  {
    if (*state == '3')
    {
      SetATDState(g_pidOfATD, ATD_Connected);
    }
  }

  return 0;
}

int urc_clip(char * pResult, int nIndex)
{
  char *pToken = strrchr_nth_next(pResult, ',', 1);

  if (!pToken) return 0;
   
  if (*pToken == '0')
  {
    int len;
    
    pToken = strtok(&pResult[nIndex], strDelimit);

    if (pToken)
    {
      len = strlen(pToken) - 1;
    }

    if (len > 0 && len < MAX_NUMBER_LENGTH)
    {
      SendCNIPInfo(pToken);
    }
  }
  return 0;
}

int urc_ring(char * pResult, int nIndex)
{
  SetATDState(0, ATD_Ringing);
  return 0;
}

int urc_no_carrier(char * pResult, int nIndex)
{
  SetATDState(g_pidOfATD, ATD_Hangup);
  g_pidOfATD = 0;
  return 0;
}

