/****************************************************************************
  inforesponse.c
  Author : kpkang@gmail.com
*****************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "agent.h"
#include "m2mdbg.h"
#include "utils.h"
#include "clientIF.h"

#include "inforesponse.h"


int response_version(char * pResponse)
{
  // EC20EQBR05A02E2G
  // 0123456789012

  // EC25JFAR02A06M4G
  // 0123456789012

  SetModemInfo(pResponse, UPDATE_MODEMVERSION);

  return 0;                
}

int response_model(char * pResponse)
{
  // EC21
  // BG96
  // BG95-M6


  return 0;                
}


int response_cnum(char * pResponse)
{
  // +CNUM: ,"01224090207",129
  // +CNUM: "Hello world","+821223883681",145
  // +CNUM: ,"+821223883681",145
  char * pComma1 = 0;
  char * pComma2 = 0;
  char * p1;
  char * p2;
  char * p3;
  pComma1 = strchr_nth(pResponse, ',', 1);
  pComma2 = strchr_nth(pResponse, ',', 2);
  p1 = pComma1 + 1;

  if (*p1 == '\"')
    p1++;
  p2 = p1;

  for (; p2 < pComma2; p2++ )
  {
    if (*p2 == '\"')
      *p2 = 0;
  }

  if ( (p3 = strstr(p1, "+82")) != 0 )
  {
    p3 += 2;
    *p3 = '0';
    p1 = p3;
  }

  //DEBUG(MSG_HIGH, "Cnum : %s , len = %d \r\n", p1, strlen(p1) );
  
  if (NULL != p1)
  {
    g_nCnumEmptyCnt = 0;
    SetModemInfo(p1, UPDATE_PHONENUMBER);
    SendAlertMsg(0, ERR_MODEM_RECOVERY_OK);
  }
  
  return 0;
}

int response_cclk(char * pResponse)
{
  int nIndex;
  int timezone = 0;
  char *pToken = NULL;

  struct tm tm_cclk;
  //+CCLK: "15/09/10,14:58:07+36"
  //0123456789

  pToken = strtok(&pResponse[7], strDelimit);

  for (nIndex = 0; NULL != pToken; nIndex++)
  {
      if (0 == nIndex)
          tm_cclk.tm_year = atoi(pToken)+2000;
      else if (1 == nIndex)
          tm_cclk.tm_mon = atoi(pToken);
      else if (2 == nIndex)
          tm_cclk.tm_mday = atoi(pToken);
      else if (3 == nIndex)
          tm_cclk.tm_hour = atoi(pToken);
      else if (4 == nIndex)
          tm_cclk.tm_min = atoi(pToken);
      else if (5 == nIndex){
          char sSec[4] = {0, };
          sSec[0] = pToken[0]; 
          sSec[1] = pToken[1];
          tm_cclk.tm_sec = atoi(sSec);

          timezone = atoi(&pToken[3]);
          
          if ('-' == pToken[2]) 
            timezone *= (-1);
      }
      pToken = strtok(NULL, strDelimit);
  }

  //DEBUG(MSG_HIGH, "%u.%u.%u-%u.%u\n", tm_cclk.tm_year, tm_cclk.tm_mon, tm_cclk.tm_mday, tm_cclk.tm_hour, tm_cclk.tm_min);

  if (tm_cclk.tm_year > 2018 && tm_cclk.tm_year < 2028)
  {
    if (SetLocalTime(&tm_cclk, timezone, 0) < 0)
    {
      DEBUG(MSG_ERROR, "[E] Fail to SetLocalTime\n");
    }
    else
    {
      g_nTimeUpdated = 1;
    }
  }
  
  return 0;
}

int response_qcds(char * pResponse)
{
  int  nIndex;
  char *pToken = NULL;

  /*
  +CSQ: 99,99
  0123456
  +CGREG: 0,0

  +QCDS: "LIMITED","3G",45005,10713,12D1C00,4,0,0,  2091,145, 0, -88,129,-88,-10,0
  +QCDS: "LIMITED","4G",45008,1550, B0D60E, 4,0,153,1202,-56,-88,-12,5,  128, 0, 0, 0
  +QCDS: "SRV","3G",45005,10713,12D1C00,4,3,0,2091,145,64,-87,129,-87,-8,508002
  0123456
  */
  int nCurrRoamNet = NET_UNKNOWN;
  pToken = strtok(&pResponse[7], strDelimit);

  for (nIndex = 0; NULL != pToken; nIndex++)
  {
      if (0 == nIndex){
          if (!strcmp("SRV", pToken)){
             g_eRegistration = REG_REGISTERED;
             SetRegistration(g_eRegistration);
          }else{
             g_eRegistration = REG_RESERVED;
             SetRegistration(g_eRegistration);
          }
      }
      else if (1 == nIndex){
          if (!strcmp("4G", pToken)){
            nCurrRoamNet = NET_LTE;
          }else{
            nCurrRoamNet = NET_WCDMA;
          }
      }
      if (NET_LTE == nCurrRoamNet)
      {
        if (9 == nIndex){ // 4g rssi
          g_nRSSI = atoi(pToken);
        }/*else if (11 == nIndex) {
          m_nEcIo = atoi(pToken); // rsrq
        } */
        else if (10 == nIndex) {
          g_nRSRP = atoi(pToken); // rsrp
        }
      }
      else if (NET_WCDMA == nCurrRoamNet)
      {
        if (11 == nIndex){ // 3G rssi
          g_nRSSI = atoi(pToken);
        }/* else if (14 == nIndex) {
          m_nEcIo = atoi(pToken); // ecio
        } */
        else if (13 == nIndex) {
          g_nRSRP = atoi(pToken); // rscp
        }
      }

      pToken = strtok(NULL, strDelimit);
  }

  SetRFState(g_nRSSI, g_nRSRP);
  
  if (nCurrRoamNet != GetRadioTech())
  {
    SetRadioTech(nCurrRoamNet);
  }
  
#ifdef SUPPORT_SIM_HOTSWAP
  if (g_eHotSwapStat != HOTSWAP_NONE) {
    if (HOTSWAP_REMOVED == g_eHotSwapStat) {
      SendCommand(AT_QSIMSTAT, FALSE, NULL);
    }
    else if (HOTSWAP_INSERTED == g_eHotSwapStat){
      if (g_eRegistration != REG_REGISTERED) {
        if (++g_nSimNotInsertedCnt > 3){
          ResetModem(RESET_USIM_FAULT);
        }
      } else {
        g_eHotSwapStat = HOTSWAP_NONE;
        g_nSimNotInsertedCnt = 0;
      }
    }
  }
#endif

  return 0;
}
int response_cimi(char * pResponse)
{
  return 0;
}

int response_cgsn(char * pResponse)
{
  // 356170060049633

  SetModemInfo(pResponse, UPDATE_SERIALNUMBER);
  return 0;
}
int response_cpin(char * pResponse)
{
  char *pToken = NULL;

  /*
    -------|---|
    +CPIN: READY
    -------|---|
    01234567
  */

  pToken = &pResponse[7];

  if(0 == strcmp(pToken, "READY"))
  {
    InitModem(SIM_READY);
    g_nSimNotInsertedCnt = 0;
  }
  else
  {
    if(0 == strcmp(pToken, "SIM PIN"))
    {
      SetUSIMState(SIM_PIN); 
    }
    else if(0 == strcmp(pToken, "SIM PUK"))
    {
      SetUSIMState(SIM_PUK); 
    }
  }
  
  return 0;                
}
int response_cgreg(char * pResponse)
{
	 return 0;
}
int response_cgdcont(char * pResponse)
{
  //+CGDCONT: 1,"IPV4V6","","0.0.0.0",0,0
  //01234567890
  {
    int cid = -1;
    char apn[128] = {0,};
		char parm[16] ;

    if (GET_NTH_PARAM(pResponse, 1, parm ) > 0) 
		{
      cid = atoi(parm);

      if (GET_NTH_PARAM(pResponse, 3, apn ) >= 0)
    	{
	    	remove_quote(apn);
				if (strlen(apn) == 0) {
					DEBUG(MSG_HIGH,"[%s] null_apn\n", __func__ );
				}
    	}
    }

		if (cid > 0) m_nCidLists |= (0x1 << (cid -1));

    if (cid == 1)
    {
      if (strcmp(APN_NAME, apn))  
      {
				SendCommand(AT_CGDCONT_X, FALSE, "AT+CGDCONT=%u,\"IPV4V6\",\"%s\"\r", cid, APN_NAME);
      }
    }
  }

  return 0;
}
int response_qcps(char * pResponse)
{
  return 0;
}
int response_qcota(char * pResponse)
{
  char *pToken = NULL;

  //+QCOTA: 0
  //01234567
  //AT+QCOTA=1 // auto mode
  pToken = strtok(&pResponse[7], strDelimit);
  if (pToken)
  {
    int otaState;
    otaState = atoi(&pToken[0]);
    if (otaState != 1)
    {
      SendCommand(AT_QCOTA_1, FALSE, NULL);
    }
  }
  return 0;
}

int response_iccid(char * pResponse)
{
  SetICCID(&pResponse[8]);

  return 0;
}
int response_qcnc(char * pResponse)
{
  return 0;
}
int response_cgpaddr(char * pResponse)
{
  int  nIndex;
  char *pToken = NULL;

  //+CGPADDR: 1,"10.207.137.130"
  //0123456789012
  if (pResponse[10] == '1' )
  {
    IPAddrT mdmIPAddr;
    
    pToken = strtok(&pResponse[12], " ,\".");
    for (nIndex = 0; NULL != pToken; nIndex++)
    {
        if (0 == nIndex)
            mdmIPAddr.digit1 = (unsigned char)atoi(pToken);
        else if (1 == nIndex)
            mdmIPAddr.digit2 = (unsigned char)atoi(pToken);
        else if (2 == nIndex)
            mdmIPAddr.digit3 = (unsigned char)atoi(pToken);
        else if (3 == nIndex)
            mdmIPAddr.digit4 = (unsigned char)atoi(pToken);

        pToken = strtok(NULL, " ,\".");
    }
        
    SetMdmIPAddr(&mdmIPAddr);
  }

  return 0;

}
int response_band(char * pResponse)
{
  char *pToken = NULL;

  //+QCFG: "band",0x10,0x40000015,0x0
  //01234567
  //+QCFG: "band",0x10,0x40000014,0x0 
  int i, wBand, lBand, tdBand;
  
  pToken = strtok(&pResponse[7], strDelimit);
  for (i = 0, wBand = 0, lBand = 0; NULL != pToken; i++)
  {
      if (0 == i){
        if (strcmp("band", pToken))
          break;
      }
      else if (1 == i){ // wcdma
        if ('x' == pToken[1])
          wBand = xtoi(&pToken[2]);
      }  
      else if (2 == i){
        if ('x' == pToken[1])
          lBand = xtoi(&pToken[2]);
      }
			
      else if (3 == i){
        if ('x' == pToken[1])
          tdBand = xtoi(&pToken[2]);
      }

      pToken = strtok(NULL, strDelimit);
  }


  DEBUG(MSG_HIGH, "BAND wBand : 0x%08x, lBand : 0x%08x tdBand : 0x%08x\r\n", wBand, lBand, tdBand);

  if (strstr(GetModemVersion(), "EC21KL"))
  {
    if ( lBand != 0x55)
    {
		  SendCommand(AT_QCFG_BAND_XX, FALSE, "AT+QCFG=\"band\",0,55,0\r");
    }
  }
             
  return 0;
}
int response_pdp_duplicate(char * pResponse)
{
  int  nIndex;
  //char *pToken = NULL;

  // +QCFG: "PDP/DuplicateChk",0

  nIndex = strlen(pResponse);
#if 1 // real
  if ((nIndex > 0) && '1' != pResponse[nIndex -1]){
    SendCommand(AT_QCFG_PDPDUP_1, FALSE, NULL);
  }
#else // test
  if ((nIndex > 0) && '0' != pResponse[nIndex -1]){
    SendCommand(AT_QCFG_PDPDUP_1, FALSE, NULL);
  }
#endif
  return 0;
}
int response_nwscanmode(char * pResponse)
{
  //int  nIndex;
  char *pToken = NULL;

  //+QCFG: "nwscanmode",2
  //01234567
  //+QCFG: "nwscanmode",0 // auto mode
  int i;
  int nwScanMode;
  
  pToken = strtok(&pResponse[7], strDelimit);
  for (i = 0; NULL != pToken; i++)
  {
      if (0 == i){
        if (strcmp("nwscanmode", pToken))
          break;
      }
      else if (1 == i){ // 2:3g, 0:auto
        nwScanMode = xtoi(&pToken[0]);
      }  

      pToken = strtok(NULL, strDelimit);
  }

	if (strstr(GetModemVersion(), "EC21KL") || strstr(GetModemVersion(), "BG9"))
  {
    if (nwScanMode != 3 )
    {
      SendCommand(AT_QCFG_NW_LTE, FALSE, NULL);
    }
  }
  else
  {
    if (nwScanMode != 0 )
    {
      SendCommand(AT_QCFG_NW_AUTO, FALSE, NULL);
    }
  }
  return 0;
}

int response_risignaltype(char * pResponse)
{
  if (!strstr(&pResponse[20], "physical"))
  {
    SendCommand(AT_QCFG_RI_PHY, FALSE, NULL);
  }

  return 0;
}

int response_apready(char * pResponse)
{
  if (!strstr(&pResponse[16], "1,0,200")){
    SendCommand(AT_QCFG_APREADY_1, FALSE, NULL);
  }

  return 0;
}

int response_urc_ri_sms(char * pResponse)
{
  if (!strstr(&pResponse[26], "\"pulse\",360")){
    SendCommand(AT_QCFG_SMSRIURC_200, FALSE, NULL);
  }

  return 0;
}

int response_urcport(char * pResponse)
{
  //+QURCCFG: "urcport","uart1"
  //+QURCCFG: "urcport","usbat"
  //012345678901234567890

  
  if (AT_INTF_UART & argument_mask){
    if (!strstr(&pResponse[20], "uart")){
      SendCommand(AT_QURCCFG_UART, FALSE, NULL);
    }
  } else {
    if (!strstr(&pResponse[20], "usbat")){
			if (strstr(GetModemVersion(), "EC21KL") )
				SendCommand(AT_QURCCFG_ALL, FALSE, NULL);
			else
      	SendCommand(AT_QURCCFG_USB, FALSE, NULL);
    }
  }
  /*
  if (!strstr(&pResponse[20], "all")){
    SendCommand(AT_QURCCFG_ALL, FALSE, NULL);
  }
  */
  return 0;
}

int response_qicsgp(char * pResponse)
{
  return 0;
}

int response_qiact(char * pResponse)
{
  return 0;
}

int response_qistate(char * pResponse)
{
  return 0;
}

int response_qicfg(char * pResponse)
{
  return 0;
}

int response_qcsq(char * pResponse)
{
  return 0;
}

int response_qsimdet(char * pResponse)
{
#ifdef SUPPORT_SIM_HOTSWAP
  //+QSIMDET: 1,0
  //+QSIMDET: 1,1
  //01234567890
  if (strcmp(&pResponse[10], "1,1")){
    SendCommand(AT_QSIMDET_1_1, TRUE, NULL);
    SendCommand(AT_W, FALSE, NULL);
  }
#endif  
  return 0;
}
int response_qsimstat(char * pResponse)
{
#ifdef SUPPORT_SIM_HOTSWAP
  //+QSIMSTAT: 0,1
  //01234567890123
  if (HOTSWAP_REMOVED == g_eHotSwapStat) {
    char * p = strrchr(pResponse, ',');
    if (p && p[1] == '1') { // inseted
      if (++g_nSimNotInsertedCnt > 2){
        ResetModem(RESET_USIM_FAULT, NULL);
      }
    }
  }
#endif  
  return 0;
}

int response_clcc(char *pResponse)
{
  /*
    -----------|----------------------
    +CLCC: 1,1,4,0,0,"01194081250",161
    -----------|-----|----------------
    012345678901234567
  
    <stat> - state of the call   ==> pResponse[11]
        0 - active 
        1 - held 
        2 - dialing (MO call) 
        3 - alerting (MO call) 
        4 - incoming (MT call) 
        5 - waiting (MT call)         
  */
#if 0//def SUPPORT_VOICE_CALL
  if (!strncmp(pResponse, "+CLCC", 5))
  {
    g_nCallInfoCount++;
  }
#endif
  return 0;
}

int response_adc(char *pResponse)
{
  // +QADC: 1,632
#if 0
  char *token = strchr_nth(pResponse, ' ', 1);

  if(NULL != token)
  {
    if(1 == atoi(token + 1))
    {
      token = strchr_nth(token, ',', 1);

      if(NULL != token)
      {
        int level = atoi(token + 1);
        if (AT_ADC_0 == GetCurrCommand() || AT_ADC_TEMP == GetCurrCommand())
          SetADC1(level);
        else if (AT_ADC_1 == GetCurrCommand())
          SetADC2(level);
      }
    }
  }
#endif
  return 0;
}

int response_temp(char *pResponse)
{
  // +QTEMP: 32,0,18
  #if 0
  char *token = strchr_nth(pResponse, ' ', 1);
  int   temp;

  if(NULL != token)
  {
    temp = atoi(token + 1);
    SetTemperature(temp);
  }
  #endif
  return 0;
}


int response_sim_presence(char *pResponse)
{
  // +QGPIO: 0
  // 012345678
#ifdef SUPPORT_SIM_HOTSWAP

  char *token = strchr_nth(pResponse, ' ', 1);
  int   sim_presence ;

  if(NULL != token)
  {
    sim_presence = atoi(token + 1);
    if (sim_presence != 1) {
      SetUSIMState(SIM_NOT_INSERTED);
    }
  }
#endif  
  return 0;
}

int response_clvl(char *pResponse)
{
  //+CLVL: 2
  char token[8];
  if (GET_NTH_PARAM(pResponse, 1, token) > 0)
  {
    int nVol = atoi(token);
    SetModemVolume(nVol);
  }
  
  return 0;
}


int response_cmvl(char *pResponse)
{
  //+CMVL: 3
  char token[8];
  if (GET_NTH_PARAM(pResponse, 1, token) > 0)
  {
    int nVol = atoi(token);
    SetMicGain(nVol);
  }
  
  return 0;
}

int response_cops(char *pResponse)
{
  //+COPS: 0,0,"SKTelecom",7
  char net_name[MAX_NETNAME_LENGTH];


	if (GET_NTH_PARAM(pResponse, 3, net_name) > 0)
	{
		SetModemInfo(net_name, UPDATE_NETWORKNAME);
	}
	
   
  return 0;
}

