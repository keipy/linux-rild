/****************************************************************************
  clientIF.c
  Author : kpkang@gmail.com
*****************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <string.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <dlfcn.h>

#include <time.h>
#include <pthread.h>
#include <limits.h>
#include <errno.h>

#include "m2mdbg.h"
#include "agent.h"
#include "clientIF.h"
#include "utils.h"

#ifdef _SHARED_MEMORY
static void *handle = NULL;
#endif
static int clientQueID = 0;
static int sharedMemID = 0;
static SharedData *g_pSharedData = NULL;


void Initialize(int pid, int qid, int tcp_qid)
{
#ifdef _SHARED_MEMORY  
  void* (*funcGetMem)(void);
  
  handle = dlopen("libril.so", RTLD_NOW);
  if ( !handle )
  { 
      DEBUG(MSG_ERROR,"fail to dlopen, %s\n", dlerror());
      return ;
  }
  
  funcGetMem = dlsym(handle, "GetSharedMemory");
  if ( dlerror() != NULL )
  {
      DEBUG(MSG_ERROR,"fail to dlsym, %s\n", dlerror());
      return ;
  }

  g_pSharedData = (void *)funcGetMem();
#else
  sharedMemID = shmget((key_t)SHARED_MEMORY_KEY, sizeof(SharedData), 0666 | IPC_CREAT);
  if(sharedMemID == -1){
      DEBUG(MSG_ERROR, "Initialize shmget error\r\n");
      perror("shmget failed : ");
      exit(0);
  }

  g_pSharedData = (SharedData *)shmat(sharedMemID, NULL, 0);
  if((void *)g_pSharedData ==(void * )-1){
      DEBUG(MSG_ERROR, "Initialize shmat error\r\n");
      perror("shmat failed : ");
      exit(0);
  }
#endif
  memset(g_pSharedData, 0x0, sizeof(SharedData));

  g_pSharedData->agentPID = pid;
  g_pSharedData->agentQue = qid;
  g_pSharedData->tcpipQue = tcp_qid;
  g_pSharedData->nRSSI    = RSSI_INIT_VALUE;
  g_pSharedData->nRSRP         = RSSI_INIT_VALUE;
  g_pSharedData->nRejectCode   = 0;
  g_pSharedData->nRadioTech    = NET_UNKNOWN;
  g_pSharedData->eSIMState     = SIM_NONE;
  strcpy(g_pSharedData->strAgentVer, AGENT_VERSION);
  memset(&(g_pSharedData->modemIP), 0x0, sizeof(IPAddrT));
  
  clientQueID = msgget(MSG2CLIENT_QUE_KEY, IPC_CREAT | 0666 /*0222*/); // write only
  if(clientQueID == -1){
      DEBUG(MSG_ERROR, "Initialize msgget error\r\n");
      perror("msgget failed : ");
      exit(0);
  }

  DEBUG(MSG_HIGH, "Initialize QID %d\r\n", clientQueID);
}

void Uninitialize(void)
{
  struct shmid_ds ds;

#ifdef _SHARED_MEMORY
  dlclose(handle);
#else
  shmdt(g_pSharedData);

  shmctl(sharedMemID, IPC_STAT, &ds);  

  if (0 == ds.shm_nattch)
  {
    shmctl(sharedMemID, IPC_RMID, NULL);  
  }
#endif  

  msgctl(clientQueID, IPC_RMID, NULL);  
}

void ResetSharedData(void)
{
  g_pSharedData->eRASState = RAS_Idle;
  g_pSharedData->eATDState = ATD_Idle;
  g_pSharedData->eSMSState = SMS_Idle;
   
  g_pSharedData->nRegistration = REG_UNKNOWN;
  g_pSharedData->nRSSI         = RSSI_INIT_VALUE;
  g_pSharedData->nRSRP         = RSSI_INIT_VALUE;
  g_pSharedData->nRadioTech    = NET_UNKNOWN;
  g_pSharedData->eSIMState     = SIM_NONE;
  g_pSharedData->nRejectCode   = 0;
  memset(&(g_pSharedData->modemIP), 0x0 , sizeof(IPAddrT));
}

BOOL SendToClient(int pid, int msgid, int wparm, void* lpBuffer, int buffSize)
{
  Msg2Client msg;
  int ret, msg_size;

  if (buffSize > 1) {
    msg_size = sizeof(Msg2Client) + buffSize -1;
    Msg2Client *p_msg = (Msg2Client *)malloc(msg_size);

    if (!p_msg) {
      DEBUG(MSG_ERROR, "SendToClient() malloc error!\n");
      return -1;
    }

    p_msg->rcvPID = pid; // pid = \BB\FD\BC\BA\C7\D1 process id \C1\DF \C7ϳ\AA.
    p_msg->msgID = msgid;
    p_msg->wparm = wparm;

    if ( buffSize > 0) memcpy(p_msg->data, lpBuffer, buffSize);

    msg_size -= sizeof(p_msg->rcvPID);
    ret = msgsnd(clientQueID, p_msg, msg_size, 0);
    free(p_msg);
    
    if (ret == -1) {
      DEBUG(MSG_ERROR, "SendToClient() fail errno %d(%s)\n", errno, strerror(errno));
      return FALSE;
    }
    
    return TRUE;
  }
  
  msg.rcvPID = pid; // pid = msg \B9\DE\C0\BB  process id \C1\DF \C7ϳ\AA.
  msg.msgID  = msgid;
  msg.wparm  = wparm;
  if (1 == buffSize) {
    msg.data[0] = *((char*)lpBuffer);
  }
  msg_size = sizeof(msg) - sizeof(msg.rcvPID);
  
  ret = msgsnd(clientQueID, &msg, msg_size, 0);

  if (ret == -1) {
    DEBUG(MSG_ERROR, "SendToClient() fail %s\n", strerror(errno));
    return FALSE;
  }

  return TRUE;
}

void SendAlertMsg(int pid, int warn)
{
  int i;
  if (pid != 0)
  {
    (void) SendToClient(pid, BM_ERROR, warn, NULL, 0);
    return;
  }

  proc_node_t *node = parent_proc.root; 
  
  for (i = 0; i < parent_proc.count; i++,node = node->next)
  {
    (void) SendToClient(pid, BM_ERROR, warn, NULL, 0);
  }
}

void SetRASState(int pid, int state)
{
  int i;
  g_pSharedData->eRASState = (state == RAS_Error) ?  RAS_Idle : state;

  if (pid != 0)
  {
    (void) SendToClient(pid, BM_DATA, state, NULL, 0);
    return;
  }

  if (state == RAS_Connecting || state == RAS_Disconnecting)
    return;
  
  proc_node_t *node = parent_proc.root; 
  
  for (i = 0; i < parent_proc.count; i++,node = node->next)
  {
    if (node->event_mask & DATA_EVENT_MASK)
    {
      SendToClient(node->pid, BM_DATA, state, NULL, 0);
    }
  }

}

void SetSMSState(int pid, int state)
{
  int i;
  if ((SMS_Error == state) || (SMS_Sent == state)) {
     g_pSharedData->eSMSState = SMS_Idle;
  } else {
    g_pSharedData->eSMSState = state;
  }
  
  if (state == SMS_Sending || state == SMS_Idle) {
    return;
  }
  
  if (pid != 0)
  {
    (void) SendToClient(pid, BM_SMS, state, NULL, 0);
    return;
  }

  proc_node_t *node = parent_proc.root; 
  
  for (i = 0; i < parent_proc.count; i++,node = node->next)
  {
    if (node->event_mask & SMS_EVENT_MASK)
    {
      SendToClient(node->pid, BM_SMS, state, NULL, 0);
    }
  }

}

void SetATDState(int pid, int state)
{
  int i;
  if ((ATD_Error == state) || (ATD_Hangup == state)) {
     g_pSharedData->eATDState = ATD_Idle;
  } else {
    g_pSharedData->eATDState = state;
  }

  if (pid != 0)
  {
    (void) SendToClient(pid,  BM_DIAL, state, NULL, 0);
    return;
  }
  
  proc_node_t *node = parent_proc.root; 
  
  for (i = 0; i < parent_proc.count; i++,node = node->next)
  {
    if (node->event_mask & DIAL_EVENT_MASK)
    {
      SendToClient(node->pid, BM_DIAL, state, NULL, 0);
    }
  }
}

void SetTCPState(int pid, int state, int err)
{
  if (!err)
    SendToClient(pid, BM_TCP, state, NULL, 0);
  else
    SendToClient(pid, BM_TCP, state, &err, sizeof(err));
}


void SetGPSState(int pid, int state)
{
}

void SendCNIPInfo(char *pNumber)
{
  int i;
  proc_node_t *node = parent_proc.root; 
  DialInfoT dial_info;

  strcpy(dial_info.strNumber, pNumber);
  
  for (i = 0; i < parent_proc.count; i++,node = node->next)
  {
    if (node->event_mask & DIAL_EVENT_MASK)
    {
      SendToClient(node->pid, BM_DIAL, ATD_Cnip, &dial_info, sizeof(dial_info));
    }
  }

}

void SendSMSRecv(char *pNumber, int type, char *pMessage, int len )
{
  int i;
  proc_node_t *node = parent_proc.root; 
  SmsMsgT sms_msg;

  sms_msg.nType = type;
  strcpy(sms_msg.strNumber, pNumber);
  memset(sms_msg.strMessage, 0x0, sizeof(sms_msg.strMessage));
  memcpy(sms_msg.strMessage, pMessage, len);
 
  for (i = 0; i < parent_proc.count; i++,node = node->next)
  {
    if (node->event_mask & SMS_EVENT_MASK)
    {
      SendToClient(node->pid, BM_SMS, SMS_Received, &sms_msg, sizeof(sms_msg));
    }
  }
}

void SetRegistration(int nReg)
{
  //DEBUG(MSG_ERROR,"SetRegistration(%d)\n", nReg);
  g_pSharedData->nRegistration = nReg;
}

void SetModemInfo(char *strInfo, int update)
{
  if (update & UPDATE_MODEMVERSION)
    strcpy(g_pSharedData->modemInfo.strModemVersion, strInfo);
  if (update & UPDATE_PHONENUMBER)
    strcpy(g_pSharedData->modemInfo.strPhoneNumber, strInfo);
  if (update & UPDATE_SERIALNUMBER)
    strcpy(g_pSharedData->modemInfo.strSerialNumber, strInfo);
  if (update & UPDATE_NETWORKNAME)
    strcpy(g_pSharedData->modemInfo.strNetworkName, strInfo);
}

void SetICCID(char *strInfo)
{
  strncpy(g_pSharedData->strICCID, strInfo, MAX_ICCID_LENGTH-1);
  g_pSharedData->strICCID[MAX_ICCID_LENGTH-1] = 0;
}

void SetModemVolume(int nVol)
{
  g_pSharedData->nModemVolume = nVol;
}

void SetMicGain(int nMic)
{
  g_pSharedData->nModemMicGain = nMic;
}

void SetRejectCode(int code)
{
  DEBUG(MSG_ERROR, "SetRejectCode(%d)\n", code);
  g_pSharedData->nRejectCode = code;
}

void SetRFState(int rssi, int rsrp)
{
  g_pSharedData->nRSSI = rssi;
  g_pSharedData->nRSRP = rsrp;
}

void SetRadioTech(int tech)
{
  g_pSharedData->nRadioTech = tech;
}

void SetUSIMState(int state)
{
  g_pSharedData->eSIMState = state;
}

int GetRASCount(void){

  return 0;
}


int CheckClient(int pid)
{
  return 0;
}

void SetMdmIPAddr(IPAddrT *addr)
{
  memcpy(&(g_pSharedData->modemIP), addr, sizeof(IPAddrT));
}

char * GetPhoneNumber(void)
{
  return g_pSharedData->modemInfo.strPhoneNumber;
}

int GetRASState(void)
{
  return g_pSharedData->eRASState;
}

int GetATDState(void)
{
  return g_pSharedData->eATDState;
}

int GetSMSState(void)
{
  return g_pSharedData->eSMSState;
}

int GetGPSState(void)
{
  return g_pSharedData->eGPSState;
}

int GetRadioTech(void)
{
  return  g_pSharedData->nRadioTech;
}

int GetUSIMState(void)
{
  return g_pSharedData->eSIMState;
}

void GetMdmIPAddr(IPAddrT *addr)
{
  memcpy(addr, &(g_pSharedData->modemIP), sizeof(IPAddrT));
}

char *GetModemVersion(void)
{
  return g_pSharedData->modemInfo.strModemVersion;
}

char *GetModemSerial(void)
{
  return g_pSharedData->modemInfo.strSerialNumber;
}

char *GetICCID(void){
  return g_pSharedData->strICCID;
}

int GetRejectCode(void)
{
  return g_pSharedData->nRejectCode;
}

