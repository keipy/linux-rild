/****************************************************************************
  ril.c
  Author : kpkang@gmail.com
*****************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <signal.h>
#include <fcntl.h>

#include <errno.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <errno.h>


#include "common.h"

#define  RILLIB_VERSION "1.00"


static SharedData *s_pSharedData  = NULL;
static int         s_queueID  = 0;
static int         s_sharedID = 0;
static int         s_lastErrCode = 0;
static int         s_fifoToClient= 0;

////////////////////////////////////////////////////////////////////////////
static void _attach() __attribute__((constructor));
static void _detach() __attribute__((destructor)) ;

int WriteMsgQueue(int caller_pid, int wparm, void* lpBuffer, int buffSize);


void _attach()
{
  int shm_id;
  
  shm_id = shmget((key_t)SHARED_MEMORY_KEY, sizeof(SharedData), 0666 | IPC_CREAT);
  if(shm_id == -1){
      perror("shmget failed : ");
      return;
  }

  s_pSharedData = (SharedData *)shmat(shm_id, NULL, 0);
  if((void *)s_pSharedData ==(void *)-1){
      perror("shmat failed : ");
      return;
  }
  s_sharedID = shm_id;
  
  printf("### _attach(%d) ###\r\n", shm_id);
}

void _detach()
{
  int i, pid, shm_id;
  struct shmid_ds ds;
  Msg2Client msg;
  int msg_size = sizeof(msg) - sizeof(msg.rcvPID);
  
  pid    = getpid();
  shm_id = s_sharedID;

  if (s_fifoToClient > 0) {
    char fifo_name[64] = {0, };
    sprintf(fifo_name, "/tmp/to_client_%d", pid);
    close(s_fifoToClient); 
    unlink(fifo_name);
  }

  if (s_queueID > 0)
  {
    WriteMsgQueue(pid, PROC_DETACH, NULL, 0);
    //usleep(1000);

    for (i =  0; i < 0x400; i++) {
      if (msgrcv(s_queueID, &msg, msg_size, pid, NOWAIT_EVENT|MSG_NOERROR) < 0)
        break;
    }
  }

  printf("### _detach(%d) ###\r\n", pid);
  
  shmdt(s_pSharedData);
  shmctl(shm_id, IPC_STAT, &ds);  

  if (0 == ds.shm_nattch)
  {
    shmctl(shm_id, IPC_RMID, NULL);  
  }
}

void SetLastError(int nErr)
{
  s_lastErrCode = nErr;
}


int GetLastError(void)
{
  return s_lastErrCode;
}

int WriteMsgQueue(int caller_pid, int wparm, void* lpBuffer, int buffSize)
{
  int rtn, msg_size;
  char msg_buf[sizeof(Msg2Agent) + sizeof(SmsMsgT)];
  Msg2Agent *p_msg = (Msg2Agent *)msg_buf;

  if (buffSize > sizeof(p_msg->u.data))
  {
    msg_size = sizeof(Msg2Agent) + buffSize -sizeof(p_msg->u.data);
  }
  else
  {
    msg_size = sizeof(Msg2Agent);
  }
  
  p_msg->caller = caller_pid; 
  p_msg->msgID = WM_USER_MSG_PROC;
  p_msg->wparm = wparm;

  if (buffSize > 0) {
    memcpy(p_msg->u.data, lpBuffer, buffSize);
  }
  
  msg_size -= sizeof(p_msg->caller);
  rtn = msgsnd(s_pSharedData->agentQue, p_msg, msg_size, 0);

  if (rtn == -1) {
    SetLastError(ERR_MSG_QUE_SENT_FAILED);
    return -1;
  }

  return 0;
}

int SetEventMask(int mask)
{
  int que_id;
  byte bMask = (byte)mask;

  if (0 != s_queueID) {
    
    return WriteMsgQueue(getpid(), PROC_UPDATE, &bMask, sizeof(bMask));;
  }
  
  que_id = msgget(MSG2CLIENT_QUE_KEY, IPC_CREAT | 0444 /*0666*/ );  // read only
  if(que_id == -1){
      perror("msgget failed : ");
      return 0;
  }
  s_queueID = que_id;

  return WriteMsgQueue(getpid(), PROC_ATTACH, &bMask, sizeof(bMask));
}

int WaitEvent(Msg2Client *msg_ptr, int msg_size, int wait_flag)
{
  return msgrcv(s_queueID, msg_ptr, msg_size, getpid(), wait_flag);
}

int DataConnect()
{
  printf("DataConnect: %d\r\n", s_pSharedData->eRASState);
  
  if (RAS_Disconnecting == s_pSharedData->eRASState)
  {
    SetLastError(ERR_SERVICE_START_HANG);
    return -1;
  }
  if (RAS_Connected == s_pSharedData->eRASState)
  {
    return RAS_Connected;
  }
  
  if (!WriteMsgQueue(getpid(), DATA_CONNECT,  NULL, 0))
  {
    return RAS_Connecting;
  }
  
  return -1;
}

int DataDisconnect()
{
  printf("DataDisconnect: %d\r\n", s_pSharedData->eRASState);
  
  if (RAS_Connecting == s_pSharedData->eRASState)
  {
    SetLastError(ERR_SERVICE_START_HANG);
    return -1;
  }
  
  if (RAS_Idle == s_pSharedData->eRASState || RAS_Error == s_pSharedData->eRASState)
  {
    return RAS_Idle;
  }

  if (!WriteMsgQueue(getpid(), DATA_DISCONNECT, NULL, 0))
  {
    return RAS_Disconnecting;
  }

  return -1;
}


int Dial(char *pCallingNumber)
{

  if (ATD_Idle != s_pSharedData->eATDState)
  {
    SetLastError(ERR_SERVICE_ALREADY_RUNNING);
    return -1;
  }

  if (strlen(pCallingNumber) > MAX_NUMBER_LENGTH)
  {
    SetLastError(ERR_INVALID_PARAMETER);
    return -1;
  }
  
  DialInfoT msg;
  strcpy(msg.strNumber, pCallingNumber);

  if (!WriteMsgQueue(getpid(), CALL_DIAL, &msg, sizeof(msg)))
  {
    return 0;
  }

  return -1;
}

int Answer()
{
  if (ATD_Ringing != s_pSharedData->eATDState && ATD_Cnip != s_pSharedData->eATDState)
  {
    SetLastError(ERR_SERVICE_DOES_NOT_EXIST);
    return -1;
  }
  
  if (!WriteMsgQueue(getpid(), CALL_ANSWER,  NULL, 0))
  {
    return 0;
  }

  return -1;
}


int HangUp()
{

  if (ATD_Idle == s_pSharedData->eATDState) 
  {
    SetLastError(ERR_SERVICE_DOES_NOT_EXIST);
    return -1;
  }
  
  if (!WriteMsgQueue( getpid(), CALL_HANGUP, NULL, 0))
  {
    return 0;
  }

  return -1;
}

int SendDTMF(char cDtmf)
{
  if (ATD_Connected != s_pSharedData->eATDState) 
  {
    SetLastError(ERR_SERVICE_DOES_NOT_EXIST);
    return -1;
  }

  if ((cDtmf >= '0' && cDtmf <= '9') || (cDtmf == '*') || (cDtmf == '#'))
  {
    if (!WriteMsgQueue(0, CALL_DTMF, &cDtmf, sizeof(cDtmf)))
    {
      //Sleep(10);
      return 0;
    }
  }
  else
  {
    SetLastError(ERR_INVALID_PARAMETER);
    return -1;
  }
  
  return -1;
}

int SendSMS(char *pNumber, char *pMessage, int nType)
{
  SmsMsgT msg;;

  if (SMS_Idle != s_pSharedData->eSMSState) 
  {
    SetLastError(ERR_SERVICE_ALREADY_RUNNING);
    return -1;
  }

 
  msg.nType = nType;
  strcpy(msg.strNumber, pNumber);

  memset(msg.strMessage, 0x0, sizeof(msg.strMessage));
 
  if (MESSAGE_TYPE_ASCII == nType)
  {
    if (strlen(pMessage) > MAX_MESSAGE_LENGTH)
    {
      SetLastError(ERR_INVALID_PARAMETER);
      return -1;
    }
    strcpy(msg.strMessage, pMessage);
  }
  else if (MESSAGE_TYPE_UCS2 == nType)
  {
    int i;
    
    for (i = 0; i < MAX_MESSAGE_LENGTH/2;i++)
    {
      if (pMessage[2*i] == 0 && pMessage[2*i+1] == 0) 
        break;

      msg.strMessage[2*i] = pMessage[2*i];
      msg.strMessage[2*i+1] = pMessage[2*i+1];
    }

    if (i == MAX_MESSAGE_LENGTH/2)
    {
      SetLastError(ERR_INVALID_PARAMETER);
      return -1;
    }
  }
  else{
    SetLastError(ERR_INVALID_PARAMETER);
    return -1;
  }
  

  if (!WriteMsgQueue(getpid(), SMS_SEND, &msg, sizeof(msg)))
  {
    return 0;
  }
  
  return -1;
}


int SetSpeakerVolume(int nVol)
{
  byte bVol = (byte)nVol;
  if (nVol < 0 || nVol > 5)
  {
    SetLastError(ERR_INVALID_PARAMETER);
    return -1;
  }

  return WriteMsgQueue(0 , CALL_VOLUME, &bVol, sizeof(bVol));
}

int SetMicVolume(int nGain)
{
  byte bGain = (byte)nGain;
  if (nGain < 0 || nGain > 7)
  {
    SetLastError(ERR_INVALID_PARAMETER);
    return -1;
  }

  return WriteMsgQueue(0, CALL_MIC, &bGain, sizeof(bGain));
}


int TCPOpen(char *addr, WORD port)
{
  int pid = getpid();
  char fifo_name[64] = {0, };

  if (s_fifoToClient > 0){
    close(s_fifoToClient);
    return WriteMsgQueue(pid, TCP_CLOSE, NULL, 0);
  }

  sprintf(fifo_name, "/tmp/to_client_%d", pid);
  
  unlink(fifo_name);
  if(mkfifo(fifo_name, 0666) == -1)
  {
    SetLastError(ERR_FIFO_CREATE_ERROR);
    printf("TO_CLIENT: mkfifo fail\n");
    return -1;
  }
  
  // fifo for read
  s_fifoToClient = open(fifo_name, O_RDWR | O_NONBLOCK);
  if(s_fifoToClient < 0)
  {
    printf("TO_CLIENT: open error(%d) %s\n", errno, strerror(errno)); 
    SetLastError(ERR_FIFO_CREATE_ERROR);
    return FALSE;
  } 

  TcpServerT msg;
  strncpy(msg.addr, addr, sizeof(msg.addr));
  msg.port = port;
  
  if (!WriteMsgQueue(pid, TCP_OPEN, &msg, sizeof(TcpServerT)))
  {
    return 0;
  }

  if (s_fifoToClient > 0) {
    close(s_fifoToClient); 
    s_fifoToClient = -1;
    unlink(fifo_name);
  }

  return -1;
}

int TCPSend(BYTE *data, int len)
{
  int pid = getpid();

  if (len > MAX_TCP_DATA_LENGTH) {
    SetLastError(ERR_INVALID_PARAMETER);
    return -1;
  }
  
  if ( !WriteMsgQueue(pid, TCP_SEND, &len, sizeof(len)) )
  {
    TcpDataT msg;
    int ret, msg_size = 0;

    msg.pid = pid;
    msg.length = len;
    memcpy(msg.data, data, len);
    msg_size = sizeof(msg) - sizeof(msg.pid);
    ret = msgsnd(s_pSharedData->tcpipQue, &msg, msg_size, IPC_NOWAIT);
    if (ret < 0) {
      SetLastError(ERR_MSG_QUE_SENT_FAILED);
    
      printf("TCPSend: msgsnd error(%d) %s\n", errno, strerror(errno)); 
      return -1;    
    }

    return 0;
  }

  return -1;
}

int TCPRecv(BYTE *data, int len)
{
  return read(s_fifoToClient, data, len);
}

int TCPClose()
{
  int pid = getpid();
  char fifo_name[64] = {0, };

  sprintf(fifo_name, "/tmp/to_client_%d", pid);

  if (s_fifoToClient > 0) {
    close(s_fifoToClient); 
    s_fifoToClient = -1;
    unlink(fifo_name);
  }

  return WriteMsgQueue(pid, TCP_CLOSE, NULL, 0);
}

int TCPStatus()
{
  return WriteMsgQueue(getpid(), TCP_STATUS, NULL, 0);
}

int SetBands(int wcdma, int lte)
{
  BandInfoT msg;
  msg.nFDDLTE = lte;
	msg.nWCDMA = wcdma;

  return WriteMsgQueue(getpid(), BAND_SET, &msg, sizeof(msg));
}

int SetAPN(char *apn)
{
  APNInfoT msg;

	if (strlen(apn) >= sizeof(msg.apn)) {
		SetLastError(ERR_INVALID_PARAMETER);
		return -1;
	}
	
  msg.nPdnType = PDN_IPV4V6;
	strcpy(msg.apn, apn);

	return WriteMsgQueue(getpid(), APN_SET, &msg, sizeof(msg));
}

int SetNwScan(int mode)
{
	byte bMode = (byte)mode;

	return WriteMsgQueue(getpid(), SCAN_SET, &bMode, sizeof(bMode));
}

int SystemControl(int nCtrl)
{
  byte bCtrl = (byte)nCtrl;

  return WriteMsgQueue(getpid(), SYS_CONTROL, &bCtrl, sizeof(bCtrl));
}

char *GetPhoneNumber(void)
{
  return s_pSharedData->simInfo.strPhoneNumber;
}

int GetDataState(void)
{
  return s_pSharedData->eRASState;
}

char *GetNetworkName(void)
{
  return s_pSharedData->modemInfo.strNetworkName;
}

char *GetModemVersion(void)
{
  return s_pSharedData->modemInfo.strModemVersion;
}

char *GetSerialNumber(void)
{
  return s_pSharedData->modemInfo.strSerialNumber;
}

int GetRegistration(void)
{
  return s_pSharedData->nRegistration;
}

int GetRSSI(void)
{
  return s_pSharedData->nRSSI;
}

int GetRSRP(void)
{
  return s_pSharedData->nRSRP;
}

int GetRSRQ(void)
{
  return s_pSharedData->nRSRQ;
}


int GetRadioTech(void)
{
  return s_pSharedData->nRadioTech;
}

int GetIPAddress(IPAddrT *ip_ptr)
{

	if (s_pSharedData->modemIP.ip_version == IP_VER_NONE) 
		return -1;

  memcpy(ip_ptr, &(s_pSharedData->modemIP), sizeof(IPAddrT));
	return 0;
}

char *GetRilVersion(void)
{
  return s_pSharedData->strAgentVer;
}


int GetModemVolume(void)
{
  return s_pSharedData->nModemVolume;
}

int GetModemMicGain(void)
{
  return s_pSharedData->nModemMicGain;
}


int GetSIMStatus(void)
{
  return s_pSharedData->eSIMState;
}

char* GetSIMID(void)
{
  return s_pSharedData->simInfo.strICCID;
}

char *GetAPN(void)
{
	return s_pSharedData->modemInfo.strAPN;
}

int GetLTEBands(void)
{
	return s_pSharedData->nLTEBands;
}

int GetWCDMABands(void)
{
	return s_pSharedData->nWCDMABands;
}

int GetNWScanMode(void)
{
	return s_pSharedData->nNwScanMode;
}


