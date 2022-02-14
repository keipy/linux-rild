#pragma once

#include "sms.h"
#include "serialport.h"


#define TIMER_AT_CMGL_0_RESULT_WAIT     9 // sec
#define TIMER_AT_QICLOSE_RESULT_WAIT    10 // 10 sec
#define TIMER_AT_COMMAND_RESULT_WAIT    3  
#define TIMER_MODEM_STATUS              5  // every 5 sec

#define MAX_CMD_FIFO_SZ             128

#define WM_USER_DESTROY     (WM_USER+256)
#define WM_USER_AT_CMD		  (WM_USER+257)
#define WM_USER_RX_CHAR   	(WM_USER+259)	
#define WM_USER_RX_ERROR  	(WM_USER+261)
#define WM_USER_RAS_RES    	(WM_USER+262)
#define WM_USER_NET_STAT    (WM_USER+265)

#define COUNT_SIM_NOT_INSERTED 10
#define COUNT_MSISDN_NOT_FOUND 10

#define SKT_DEFAULT_APN "lte-internet.sktelecom.com"
#define KT_DEFAULT_APN  "privatelte.ktfwing.com"

enum
{
    NONE = 0,
    ATE0,
    ATA,
    AT_GMR,
    AT_GMM,
    AT_CPIN,
    AT_CNUM,
    AT_CMGD_0_4,
    AT_CNMI_2_2,
    AT_CMGL_0,
    
    AT_CMGF_0,  // 10
    AT_CGREG,
    AT_CEREG_1,
    AT_COPS,
    AT_CGDCONT,
    AT_CGREG_1 , 
    AT_CMEE_1,
    AT_CCLK,
    AT_CIMI,
    AT_CHUP,
    
    AT_CGSN,  // 20
    AT_CFUN_1_1,
    AT_CGPADDR, 
    AT_QCSQ,
    AT_SHDN,
    AT_QSCLK,
    AT_QSCLK_1,
    AT_QCDS,
    AT_QCPS,
    AT_QCOTA,
    
    AT_QCNC,    // 30
    AT_QCNC_1_1_1,
    AT_ICCID, 
    AT_DSCI_1,
    AT_QCFG_BAND,
    AT_CNMI_2_2_1,
    AT_CLVL,
    AT_CMVL,
    AT_QCFG_NW,   
    AT_QCFG_NW_WCDMA,
    
    AT_QCFG_NW_AUTO, // 40
    AT_QCFG_NW_LTE,  
    AT_QCFG_RI,
    AT_QCFG_RI_PHY,
    AT_QCFG_APREADY,
    AT_QCFG_APREADY_1,
    AT_QCFG_SMSRIURC,
    AT_QCFG_SMSRIURC_200,
    AT_QURCCFG,
    AT_QURCCFG_USB, 
    
    AT_QURCCFG_UART, // 50
    AT_QURCCFG_ALL, 
    AT_CLCC,
    AT_QICSGP,   
    AT_QIACT,
    AT_QIACT_1,
    AT_QIDEACT_1,
    AT_QISTATE,
    AT_QICFG_READ,
    AT_QICFG_DATAFORMAT_0_1,
    
    AT_QICFG_VIEWMODE_1,   // 60
    AT_QICFG_PKTSIZE_1460,  
    AT_QICFG_PASSIVECLOSED_1,
    AT_QICFG_RETRANS_3_20,
    AT_QSIMDET,
    AT_QSIMDET_1_0,
    AT_QSIMDET_1_1,
    AT_QSIMSTAT,
    AT_SIM_PRESENCE,
    AT_TEMP,

    AT_ADC_0,  // 70
    AT_ADC_1,  
    
    
////////////////////////////////////////////////////////////
    ATD = 128,
    AT_CLVL_N,
    AT_CMVL_N,
    AT_VTS,   
    
    AT_CMGS,
    AT_CMGS_PDU,
    AT_CMGR,
    AT_CMGD_INDEX,

    AT_QIOPEN_1,
    AT_QISEND,
    AT_QISEND_DATA,
    AT_QICLOSE,

    AT_QIDNSGIP,
    AT_QCFG_BAND_X,
    AT_CGDCONT_1
};



enum
{
  RESET_NONE_CAUSE = 0x0,
  RESET_NO_CONNMGR = 0x1,
  RESET_NO_ATRESULT = 0x2,
  RESET_IP_MISMATCH = 0x4,
  RESET_QCOTA_DONE  = 0x8,
  RESET_USIM_FAULT  = 0x10,
  RESET_FROM_CLIENT = 0x20,
};

enum 
{
  HOTSWAP_NONE = 0, 
  HOTSWAP_REMOVED, 
  HOTSWAP_INSERTED
};

typedef struct
{
  int        wYear;
  int        wMonth;
  int        wDay;
  int        wHour;
  int        wMinute;
  int        wSecond;
  int        wMilliseconds;
  int        wDayOfWeek;
} SYSTEMTIME;


struct proc_node{
  int pid;
  BYTE event_mask;
  BYTE reserved;
  struct proc_node *next;
};

typedef struct proc_node proc_node_t;

typedef struct {
    int count;
    proc_node_t * root;
} proc_root_t;


typedef struct {
    int pid;
    int fifo;
} sock_info_t;


//typedef void (*parse_func_t)(char chByte);


typedef struct
{
  const char  *command;
  int (*response)(char * pResponse);
}comm_info_t;

typedef struct
{
  const char  *strCode;
  int lenCode;
  int (*result)(char * pResult, int nIndex);
}result_info_t;


extern DWORD argument_mask;

extern byte g_nCnumEmptyCnt ;
extern byte g_nTimeUpdated  ;
extern byte g_nSimNotInsertedCnt;
extern byte g_nRecoveryCountDown;
extern byte g_eSIMStatus   ;  /* internal sim status */
extern byte g_eRegistration;
extern byte g_eHotSwapStat ;
extern byte m_nCidLists ;

extern int g_nSendPDULength;
extern int g_nRecvMessageIndex;
extern int g_nRSSI, g_nRSRP, g_nRSRQ;
#if 0//def SUPPORT_VOICE_CALL
extern int g_nCallInfoCount;
#endif

extern int g_pidOfSMS;
extern int g_pidOfATD;
#ifdef SUPPORT_TCP_CMD
extern int g_nTCPRecvLen;
extern int g_pidOfTCPRecv;
#endif

extern char APN_NAME[];
extern proc_root_t parent_proc;

extern const   char strDelimit[];
extern const   char strDelimit_NoSpace[];

void  OnTimer(void);
void  OnRecvSerial(void);
void  OnPutCommand(BYTE);
void  OnReadMsgQue(int , int , void *);

void InitAgent(void);
void DeinitAgent(void);
void InitModem(int simState);
void Init1SecTick(void);
void Deinit1SecTick(void);
int  InitConnMgr(void);
int  DeinitConnMgr(void);

void ResetModem(int nCause);
void ProcSysControl(int pid, BYTE ctrl);

DWORD GetTickCount(void);
void  SendMsgQue (int msg_id, int wparm, int lparm);

void OnSendDTMF(char cDtmf);
void OnDial(int, char *);
void OnAnswer(int);
void OnHangUp(int);
void OnSendSMS(int pid, SmsMsgT *sms);

void OnTCPOpen(int pid, TcpServerT *srv_ptr);
void OnTCPClose(int pid);
void OnTCPSend(int pid, int length);
void OnSetVolume(BYTE vol);
void OnSetMicGain(BYTE vol);
void OnNetStatus(void);
void OnUserSignal(int sig);
void OnRasResult(int eStat,int sStat);
void OnSerialError(int err, int sub_err);
void OnApnSet(APNInfoT *p);
void OnScanSet(BYTE mode);
void OnBandSet(BandInfoT *p);

void DUNConnect(int pid);
void DUNDisconnect(int pid);

void InitDataIntf(void);
void DeinitDataIntf(void);
void *DunThread(void *lpParam);
void *ConnThread(void *lpParam);
void DunMainLoop(void);
void EthMainLoop(void);

void *SerRxThread(void *lpParam);

BOOL IsServiceOkay(void);
BOOL IsPortOpened(void);
BOOL IsRegistered(void);

void SendCommand(BYTE cmd_id, BOOL at_first, const char * format, ...);
BYTE GetCurrCommand(void);
void AddProcess(int pid, BYTE mask);
void RemoveProcess(int pid);
void RemoveAllProc(int pid);

#ifdef SUPPORT_TCP_CMD  
void TcpOpen(int pid, char *addr, WORD port);
void TcpClose(int pid);
void TCPClosed(int pid, int err);

int GetPidBySockId(int nSock);
int GetSockIdByPid(int pid);
int GetFifoIdByPid(int pid);
#endif
