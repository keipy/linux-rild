/****************************************************************************
  finalresult.c
  Author : kpkang@gmail.com
*****************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "agent.h"
#include "m2mdbg.h"
#include "utils.h"
#include "clientIF.h"

#include "finalresult.h"
#include "cmdlist.h"


const result_info_t frc_table[] =
{
  {"OK",            2, frc_ok},      // 0
  {"ERROR",         5, frc_error},      // 1
  {"SEND OK",       7, frc_send_okay},      // 2
  {"SEND FAIL",     9, frc_send_fail},      // 3
  {"+CME ERROR: ", 12, frc_cme_error},      // 2
  {"+CMS ERROR: ", 12, frc_cms_error},      // 3
};



int ParseFinalRes(char *pResult)
{
  const int nMaxCount = sizeof(frc_table) / sizeof(frc_table[0]);
  int nIndex;
  
  DEBUG(MSG_HIGH, "%s\r\n", pResult);

  for (nIndex = 0; nIndex < nMaxCount; nIndex++)
  {
    if (0 == strncmp(pResult, frc_table[nIndex].strCode, frc_table[nIndex].lenCode ))
      break;
  }
  if(nMaxCount == nIndex)
      return -1;

  if (frc_table[nIndex].result)
  {
    return frc_table[nIndex].result(pResult, frc_table[nIndex].lenCode);
  }

  return 0;
}


int frc_send_okay(char * pResult, int nIndex) 
{
#ifdef SUPPORT_TCP_CMD
  CMD_Tlv *tlv = NULL;
  CmdPoolGet(AT_QISEND, &tlv);

  if (tlv) {
    char *sSock = strchr_nth_next((char*)tlv->value, '=', 1);
    int pid = GetPidBySockId((int)(*sSock - '0'));

    SetTCPState(pid, TCP_Sent, 0);
  }

  CmdPoolDel(AT_QISEND);
  //CmdPoolDel(AT_QISEND_DATA);
#endif
  return 0;
}

int frc_send_fail(char * pResult, int nIndex) 
{
#ifdef SUPPORT_TCP_CMD
  CMD_Tlv *tlv = NULL;
  CmdPoolGet(AT_QISEND, &tlv);
  
  if (tlv) {
    char *sSock = strchr_nth_next((char*)tlv->value, '=', 1);
    int pid = GetPidBySockId((int)(*sSock - '0'));
    SetTCPState(pid, TCP_Sent_Fail, 0);
  }
  CmdPoolDel(AT_QISEND);
  //CmdPoolDel(AT_QISEND_DATA);
#endif
  return 0;
}


int frc_ok(char * pResult, int nIndex)
{
  BYTE currCommand = GetCurrCommand();
  if(AT_CMGS_PDU == currCommand)
  {
    CmdPoolDel(AT_CMGS);
    //CmdPoolDel(AT_CMGS_PDU);
  }
  if (currCommand == AT_CGDCONT) 
  {
    if (0 == (m_nCidLists & 0x1))
      SendCommand(AT_CGDCONT_1, FALSE, NULL);
    //if (0 == (m_nCidLists & 0x2))
    //  SendCommand(AT_CGDCONT_2, FALSE);
  }
  else if (currCommand == AT_CNUM) 
  {
    char *cNum = GetPhoneNumber();
    if ('0' > cNum[1] || '9' < cNum[1])
    {
      if (++g_nCnumEmptyCnt == COUNT_MSISDN_NOT_FOUND)
      {
        SetUSIMState(SIM_NONE_MSISDN);
        SendAlertMsg(0, ERR_SIM_NONE_MSISDN); 
      }
      else
      {
        sleep(1);
        SendCommand(AT_CNUM, TRUE, NULL);
      }
    }
  }
  else if (currCommand == AT_QCOTA_1) 
  {
    ResetModem(RESET_QCOTA_DONE);
  }
#ifdef SUPPORT_TCP_CMD
  else if (currCommand == AT_QICLOSE) 
  {
    CMD_Tlv *tlv = NULL;
    CmdPoolGet(AT_QICLOSE, &tlv);

    if (tlv) {
      char *sSock = strchr_nth_next((char*)tlv->value, '=', 1);
      int pid = GetPidBySockId((int)(*sSock - '0'));
      DEBUG(MSG_HIGH, "*sSock %c, pid %d\r\n", *sSock, pid);
      TCPClosed(pid, -1);
    }
  }
#endif
#ifdef SUPPORT_MODEM_SLEEP            
  else if (AT_QSCLK_1 == currCommand) {
    m_sleepMode = SLEEP_ENABLE;
  }
#endif

#ifdef SUPPORT_VOICE_CALL
  else if (ATD == currCommand)
  {
    SetATDState(g_pidOfATD, ATD_Dialing);
  }
  else if (ATA == currCommand)
  {
    SetATDState(g_pidOfATD, ATD_Answer);
  }
  else if (AT_CHUP == currCommand)
  {
    SetATDState(g_pidOfATD, ATD_Hangup);
    g_pidOfATD = 0;
  }
#if 0
  else if (AT_CLCC == currCommand)
  {
    if (0 == g_nCallInfoCount)
    {
      if (ATD_Idle != GetATDState())
      {
        SetATDState(g_pidOfATD, ATD_Idle);
        g_pidOfATD = 0;
      }
    }
  }
#endif
  else if (currCommand == AT_CLVL)
  {
    // AT+CLVL=1
    CMD_Tlv *tlv = NULL;
    CmdPoolGet(AT_CLVL, &tlv);

    if (tlv) {
      char *sVal = strchr_nth_next((char*)tlv->value, '=', 1);
      int nModemVolume = (int)(*sVal - '0');
      SetModemVolume(nModemVolume);
    }
    
  }
  else if (currCommand == AT_CMVL)
  {
    // AT+CLVL=1
    CMD_Tlv *tlv = NULL;
    CmdPoolGet(AT_CMVL, &tlv);
    
    if (tlv) {
      char *sVal = strchr_nth_next((char*)tlv->value, '=', 1);
      int ModemMicGain = (int)(*sVal - '0');
      SetModemVolume(ModemMicGain);
    }
  }
#endif            
  
#ifdef SUPPORT_GPS_CMD
  else if (AT_QGPS_2 == currCommand)
  {
    SetGPSState(GPS_Running);
  }
  else if (AT_QGPS_0 == currCommand)
  {
    SetGPSState(GPS_Idle);
  }
#endif

  return 0;
}

int frc_error(char * pResult, int nIndex)
{
  BYTE currCommand = GetCurrCommand();

  DEBUG(MSG_ERROR,"ERROR: m_nATCommandID(%d)\r\n", currCommand);
  if (AT_CMGS == currCommand)
  {
    SetSMSState(g_pidOfSMS, SMS_Error); 
    g_pidOfSMS = 0;
  }
#ifdef SUPPORT_TCP_CMD    
  else if (AT_QIOPEN_1 == currCommand) {
    // AT+QIOPEN=1,x
    CMD_Tlv *tlv = NULL;
    CmdPoolGet(AT_QIOPEN_1, &tlv);

    if (tlv) {
      char *sSock = strchr_nth_next((char*)tlv->value, ',', 1);
      int pid = GetPidBySockId((int)(*sSock - '0'));
      TCPClosed(pid, ERR_TCPIP_COMMAND_OPEN);
    }
  }
#endif  

#ifdef SUPPORT_VOICE_CALL             
  else  if (ATD == currCommand)
  {
    SetATDState(g_pidOfATD, ATD_Error);
    g_pidOfATD = 0;
  }
#endif
#ifdef SUPPORT_MODEM_SLEEP    
  else if (AT_QSCLK_1 == currCommand) {
    m_sleepMode = SLEEP_DISABLE;
  }
#endif  

  return 0;
}

int frc_cme_error(char * pResult, int nIndex)
{
  int nError = atoi(&pResult[nIndex]);
  BYTE currCommand = GetCurrCommand();
  
  if (10 == nError /* SIM not inserted */) 
  {
    g_nSimNotInsertedCnt++;

    if(COUNT_SIM_NOT_INSERTED == g_nSimNotInsertedCnt)
    {
      SetUSIMState(SIM_NOT_INSERTED);
      SendAlertMsg(0, ERR_SIM_NOT_INSERTED); 
    }
    else
    {
      sleep(1);
      if (currCommand == AT_CPIN)
      {
        SendCommand(AT_CPIN, TRUE, NULL);
      }
    }
  }
  else if(14 == nError /* SIM busy */)  
  {
    sleep(1);
    if (currCommand == AT_CNUM)
    {
      SendCommand(AT_CNUM, TRUE, NULL);
    }
    else if (currCommand == AT_CPIN)
    {
      SendCommand(AT_CPIN, TRUE, NULL);
    }
  }
  else if (16 == nError /* incorrect password */)
  {
    SendAlertMsg(0, ERR_SIM_INCORRECT_PASSWORD); 
  }
  else if (11 == nError /* SIM PIN required */)
  {
    SendAlertMsg(0, ERR_SIM_PIN_REQUIRED); 
  }
  else if (12 == nError /* SIM PUK required */)
  {
    SendAlertMsg(0, ERR_SIM_PUK_REQUIRED); 
  }
  else if (13 == nError /* SIM failure */) 
  {
    DEBUG(MSG_ERROR,"SIM failure command(%d)\r\n", currCommand);
  }

  return 0;
}

int frc_cms_error(char * pResult, int nIndex)
{
  if(AT_CMGS_PDU == GetCurrCommand())
  {
    //CmdPoolDel(AT_CMGS_PDU);
    CmdPoolDel(AT_CMGS);
    SetSMSState(g_pidOfSMS, SMS_Error);
    g_pidOfSMS = 0;
  }

  return 0;
}



