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
        }else if (11 == nIndex) {
          g_nRSRQ = atoi(pToken); // rsrq
        }
        else if (10 == nIndex) {
          g_nRSRP = atoi(pToken); // rsrp
        }
      }
      else if (NET_WCDMA == nCurrRoamNet)
      {
        if (11 == nIndex){ // 3G rssi
          g_nRSSI = atoi(pToken);
        } else if (14 == nIndex) {
          g_nRSRQ = atoi(pToken); // ecio
        }
        else if (13 == nIndex) {
          g_nRSRP = atoi(pToken); // rscp
        }
      }

      pToken = strtok(NULL, strDelimit);
  }

  SetRFState(g_nRSSI, g_nRSRP, g_nRSRQ);
  
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
	SetModemInfo(pResponse, UPDATE_SIM_IMSI);

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
			SetModemInfo(apn, UPDATE_APN);

      if (strcmp(APN_NAME, "notset") && strcmp(APN_NAME, apn))  
      {
				SendCommand(AT_CGDCONT_1, FALSE, "AT+CGDCONT=%u,\"IPV4V6\",\"%s\"\r", cid, APN_NAME);
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
  //+QCOTA: 0
  //01234567
  //AT+QCOTA=1 // auto mode
  return 0;
}

int response_iccid(char * pResponse)
{
	char iccid[MAX_ICCID_LENGTH];
	
	if (GET_NTH_PARAM(pResponse, 1, iccid) > 0){
	  SetModemInfo(iccid, UPDATE_SIM_ICCID);
	}

  return 0;
}
int response_qcnc(char * pResponse)
{
  return 0;
}
int response_cgpaddr(char * pResponse)
{
  int  nIndex;
	IPAddrT ipAddr;
	char ip_address[128];
	char cid_str[8];
	char *token;

  // +CGPADDR: 1,"223.52.248.107"
  // +CGPADDR: 16,"0.0.0.0"
  // +CGPADDR: 1,32.1.2.216.19.176.70.170.0.0.0.0.243.10.178.1
  // +CGPADDR: 1,,"32.1.2.216.19.176.70.170.0.0.0.0.243.10.178.1"

  if (GET_NTH_PARAM(pResponse, 1, cid_str) > 0)
  {
    int cid = atoi(cid_str);
    
    if (1 != cid)
      return 0 ;
  }

  token = strchr_nth(pResponse, '.', 15);

  if (token)
  { 
  	// IPV6
    if (GET_NTH_PARAM(pResponse, 2, ip_address) == 0) 
  	{
			if (GET_NTH_PARAM(pResponse, 3, ip_address) == 0) 
			{
				return 0 ;
			}
  	}

    remove_quote(ip_address);
    
    for (nIndex = 0, token = strtok(ip_address, "."); nIndex < 16 && token != NULL; nIndex++) {

      if (nIndex%2 == 0) {
				ipAddr.addr.ipv6.seg[nIndex/2] = (unsigned short)(atoi(token) << 8);
      }else{
				ipAddr.addr.ipv6.seg[nIndex/2] += (unsigned short)atoi(token);
      }

      token = strtok(NULL, ".");
    }

    ipAddr.ip_version = IP_VER_6;
		SetMdmIPAddr(&ipAddr);
  }
  else
  { 
		// IPV4
    if (GET_NTH_PARAM(pResponse, 2, ip_address) > 0) 
    {
      remove_quote(ip_address);
			ipAddr.ip_version = IP_VER_4;
			
	    for (nIndex = 0, token = strtok(ip_address, "."); NULL != token; nIndex++)
	    {
        ipAddr.addr.ipv4.digit[nIndex] = (unsigned char)atoi(token);
        token = strtok(NULL, ".");
	    }
	    
	    SetMdmIPAddr(&ipAddr);
    }
  }

	return 0 ;
}

int response_band(char * pResponse)
{
  char *pToken = NULL;

  //+QCFG: "band",0x10,0x40000015,0x0
  //01234567
  //+QCFG: "band",0x10,0x40000014,0x0 
  int i, wBand, fddBand, tddBand;
  
  pToken = strtok(&pResponse[7], strDelimit);
  for (i = 0, wBand = 0, fddBand = 0; NULL != pToken; i++)
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
          fddBand = xtoi(&pToken[2]);
      }
			
      else if (3 == i){
        if ('x' == pToken[1])
          tddBand = xtoi(&pToken[2]);
      }

      pToken = strtok(NULL, strDelimit);
  }


  DEBUG(MSG_HIGH, "BAND wBand : 0x%08x, lBand : 0x%08x tdBand : 0x%08x\r\n", wBand, fddBand, tddBand);
	SetLTEBands(fddBand);
	SetWCDMABands(wBand);

             
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
  int nwScanMode = NET_SCAN_AUTO;
  
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
	SetNWScanMode(nwScanMode);

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
        ResetModem(RESET_USIM_FAULT);
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
  char token[8];
  if (GET_NTH_PARAM(pResponse, 1, token) > 0)
	{
		if(1 == atoi(token))
		{
			if (GET_NTH_PARAM(pResponse, 2, token) > 0)
			{
				int level = atoi(token );
				
				if (AT_ADC_0 == GetCurrCommand())
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
  char token[8];
  if (GET_NTH_PARAM(pResponse, 1, token) > 0)
  {
    int temp = atoi(token);
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
  char token[8];
  if (GET_NTH_PARAM(pResponse, 1, token) > 0)
  {
    int sim_presence = atoi(token);
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
		remove_quote(net_name);
		SetModemInfo(net_name, UPDATE_NETWORKNAME);
	}
	
  return 0;
}

int response_cmgl(char *pResponse)
{
	/*
		+CMGL: 0,0,,32
		|------------------------------------------------------------------------------|
		0791280102194105440BA11091041852F00084218013414441630D0A22080B811091041852F0A4B2
		|------------------------------------------------------------------------------|
		+CMGL: 1,0,,32
		|------------------------------------------------------------------------------|
		0791280102194105440BA11091041852F00084218013414451630D0A22080B811091041852F0A4B2
		|------------------------------------------------------------------------------|
	*/

	if (*pResponse >= '0' &&  *pResponse <= '9')
	{
		char strRecvMessage[MAX_MESSAGE_LENGTH+4] = {0,};
		char strCallerNumber[MAX_NUMBER_LENGTH+4];
		int  msg_type = 0;
		int  msg_len = 0;
		
		msg_len = DecodePDU(pResponse, &msg_type, strCallerNumber, strRecvMessage, NULL);
		if (msg_len > 0)
		{
			DEBUG(MSG_HIGH, "MSG type %d, Len %d, Num Len %d \r\n", msg_type, msg_len, strlen(strCallerNumber));
			SendSMSRecv(strCallerNumber, msg_type, strRecvMessage, msg_len);
		}
	}

	return 0;
}
