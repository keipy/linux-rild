/****************************************************************************
  ril.h
  Author : kpkang@gmail.com
*****************************************************************************/
#ifndef __RILLIB_H
#define __RILLIB_H

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef _WIN_TYPE_
#define _WIN_TYPE_
typedef int           HANDLE;
typedef int           HWND;
typedef unsigned char BOOL;
typedef unsigned char BYTE;
typedef unsigned int  UINT;
typedef unsigned int  DWORD;
typedef double        DOUBLE;
typedef char          CHAR;
typedef unsigned char byte;
typedef unsigned short WORD;
#endif //_WIN_TYPE_

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif


#ifndef MAX_NUMBER_LENGTH
#define MAX_NUMBER_LENGTH      20
#endif
#ifndef MAX_MESSAGE_LENGTH
#define MAX_MESSAGE_LENGTH     160
#endif

#define WM_USER_BROADCAST 0x1000

// ERROR for GetErrorCode()
#define ERR_DEV_NOT_EXIST             (-101)
#define ERR_INVALID_HANDLE            (-102)
#define ERR_SERVICE_ALREADY_RUNNING   (-103)
#define ERR_INVALID_PARAMETER         (-104)
#define ERR_SERVICE_DOES_NOT_EXIST    (-105)
#define ERR_SERVICE_START_HANG        (-106)
#define ERR_MSG_QUE_SENT_FAILED       (-107)
#define ERR_SERVICE_STATE_ERROR       (-108)
#define ERR_SOCKET_CREATE_ERROR       (-109)
#define ERR_SOCKET_SEND_ERROR         (-110)
#define ERR_FIFO_CREATE_ERROR         (-111)
#define ERR_SERVICE_NOT_ALLOWED       (-112)
#define ERR_MEM_ALLOC_FAILED          (-113)


#define BM_DATA                 (WM_USER_BROADCAST+1)
#define BM_DIAL                 (WM_USER_BROADCAST+2)
#define BM_SMS                  (WM_USER_BROADCAST+3)
#define BM_GPS                  (WM_USER_BROADCAST+4)
#define BM_TCP                  (WM_USER_BROADCAST+5)
#define BM_ERROR                (WM_USER_BROADCAST+11)



#define MESSAGE_TYPE_ASCII    1
#define MESSAGE_TYPE_UCS2     2

#define WAIT_EVENT     0
#define NOWAIT_EVENT   IPC_NOWAIT

#define MAX_TCP_DATA_LENGTH 1460

// GPS, SMS, DATA, VOICE
#define DIAL_EVENT_MASK  0x1
#define SMS_EVENT_MASK   0x2
#define DATA_EVENT_MASK  0x4
#define GPS_EVENT_MASK   0x8
#define ALL_EVENT_MASK   0xF

enum
{
    RAS_Idle = 0,
    RAS_Connecting,
    RAS_Connected,
    RAS_Disconnecting,
    RAS_Error,

    RAS_Max = 0xFFFFFFFF
};


enum 
{
    ATD_Idle = 0,
    ATD_Dialing,
    ATD_Ringing,
    ATD_Cnip,
    ATD_Answer,
    ATD_Connected = ATD_Answer,
    ATD_Hangup,
    ATD_Error,

    ATD_Max = 0xFFFFFFFF
};

enum 
{
    SMS_Idle = 0,
    SMS_Sending,
    SMS_Sent,
    SMS_Received,
    SMS_Error,

    SMS_Max = 0xFFFFFFFF
};

enum
{
    GPS_Idle = 0,
    GPS_Running,
    GPS_Fixed,
    GPS_Error,

    GPS_Max = 0xFFFFFFFF
};

enum {
    TCP_Idle = 0,
    TCP_Connect_Fail,
    TCP_Connected, 
    TCP_Recv,
    TCP_Sent,
    TCP_Sent_Fail,
    TCP_Error, 

    TCP_Max = 0xFFFFFFFF
};


enum 
{
    ERR_NONE =   0,        
    ERR_REGISTRATION,       
    ERR_AUTHENTICATION,     
    ERR_NO_SERVICE,         
    ERR_DIAL,               
    ERR_CONNECT,            
    ERR_NO_CARRIER,         
    ERR_MESSAGE_SEND,       
    ERR_MESSAGE_RECEIVE,    
    ERR_MODEM_PORT_FAIL,    
    ERR_MODEM_RECOVERY,     
    ERR_MODEM_RECOVERY_FAIL,
    ERR_MODEM_RECOVERY_OK,  
    ERR_DATA_SRV_EXPIRED,   
    ERR_SIM_NOT_INSERTED,   
    ERR_SIM_PIN_REQUIRED,   
    ERR_SIM_PUK_REQUIRED,
    ERR_SIM_INCORRECT_PASSWORD,
    ERR_SIM_NONE_MSISDN,


    ERR_TCPIP_NO_RESOURCE  = 597,
    ERR_TCPIP_COMMAND_OPEN = 598,
    ERR_TCPIP_PDP_DEACTIVATED = 599,
};

enum {
	IP_VER_NONE = 0,
  IP_VER_4 = 1,
	IP_VER_6 = 2,
};

enum {
	NET_SCAN_AUTO = 0,
	NET_SCAN_WCDMA = 2,
	NET_SCAN_LTE = 3,
};

enum
{
  MODEM_RESET = 0,
  MODEM_REBOOT,
  MODEM_PWROFF,
};

enum
{
  PDN_IPV4 = 0,
  PDN_IPV6,
  PDN_IPV4V6,
};


typedef struct {
  long rcvPID;     /* message type, must be > 0 */
  int  msgID;    /* message data */
  int  wparm;
  char data[0];
} Msg2Client;

typedef struct
{
	unsigned char digit[4];
}IPv4_T;

typedef struct
{
	unsigned short seg[8];
}IPv6_T;


typedef struct
{
  int ip_version;
  union {
    IPv4_T ipv4; 
    IPv6_T ipv6;
  } addr;
} IPAddrT;

typedef struct
{
  char strNumber[MAX_NUMBER_LENGTH + 4];
} DialInfoT;


typedef struct
{
  int nType;
  char strNumber[MAX_NUMBER_LENGTH+4];
  char strMessage[MAX_MESSAGE_LENGTH+4];
} SmsMsgT;

typedef struct
{
  char addr[256];
  WORD port;
} TcpServerT;


typedef struct {
  long pid;   /* message type, must be > 0 */
  int length;
  BYTE data[MAX_TCP_DATA_LENGTH];
} TcpDataT;


typedef struct  
{
  int nPdnType;
  char apn[100];
} APNInfoT;

typedef struct
{
  int nWCDMA;
  int nFDDLTE;
} BandInfoT;

typedef struct  
{
  unsigned int m_valid;
  unsigned int m_year;
  unsigned int m_month;
  unsigned int m_day;
  unsigned int m_hour;
  unsigned int m_min;
  unsigned int m_sec;
  
  double m_latitude;
  double m_longitude;
  double m_altitude;
  double m_groundSpeed;
  double m_courseOverGround;
  
  unsigned int m_nSentences;
  unsigned int m_signalQuality;
  unsigned int m_satelitesInUse;
}GPSInfoT;


/*
  ModemRegistration()
  \C0\BD\BF\B5\C1\F6\BF\AA ===> REG_NO_SERVICE
*/
enum
{
  REG_UNKNOWN       = 0x0,  
  REG_REGISTERED    = 0x1,
  REG_SEARCHING     = 0x2,
  REG_REJECTED      = 0x3, /* by network */
  REG_RESERVED      = 0x4,
  REG_ROAMING       = 0x5,
};

/*
  GetSIMStatus()
  "\B9\F8ȣ\B5\EE\B7\CF \C8\C4 \BB\E7\BF\EB\C0\CC \B0\A1\B4\C9\C7մϴ\D9" ===> SIM_NONE_MSISDN
  "\B0\B3\C5\EB\C0\CC \C7ʿ\E4\C7մϴ\D9. \B0\ED\B0\B4\BC\BE\C5\CD \B6Ǵ\C2 \B4�?C1\A1\C0\B8\B7\CE \B9�?C7Ͽ\A9 \C1ֽʽÿ\E4." ===> SIM_NONE_PROVISIONING
  "USIMī\B5�?\C0νĵ\C7\C1\F6 \BEʽ\C0\B4ϴ\D9"   ===> SIM_NOT_INSERTED
*/
enum
{
  SIM_NOT_INSERTED  = 0x0,
  SIM_READY,          
  SIM_BUSY,
  SIM_FAILURE,
  SIM_PIN,
  SIM_PUK,
  SIM_NONE,
  
  SIM_NONE_MSISDN = 0x10,
};


int  DataConnect();
int  DataDisconnect();
int  TCPOpen(char *addr, WORD port);
int  TCPSend(BYTE *data, int len);
int  TCPRecv(BYTE *data, int len);
int  TCPClose();
int  TCPStatus();

int  Dial(char *pCallingNumber);
int  Answer();
int  HangUp();
int  SendDTMF(char cDtmf);
int  SendSMS(char *pNumber, char *pMessage, int nType);
int  SetSpeakerVolume(int nVol);
int  SetMicVolume(int nGain);
int  SetBands(int wcdma, int lte);
int  SetAPN(char *apn);
int  SetNwScan(int mode);
int  SystemControl(int nCtrl);


char *GetPhoneNumber(void);
char *GetNetworkName(void);
char *GetModemVersion(void);
char *GetSerialNumber(void);
char* GetSIMID(void);
char *GetRilVersion(void);
int  GetIPAddress(IPAddrT *ip_ptr);
char *GetAPN(void);

int  GetDataState(void);
int  GetRegistration(void);
int  GetRSSI(void);
int  GetRSRP(void);
int  GetRSRQ(void);
int  GetRadioTech(void);
int  GetSIMStatus(void);
int  GetLastError(void);
int  GetModemVolume(void);
int  GetModemMicGain(void);
int  GetLTEBands(void);
int  GetWCDMABands(void);
int  GetNWScanMode(void);


int  WaitEvent(Msg2Client *msg_ptr, int msg_size, int wait_flag);
int  SetEventMask(int mask);
#ifdef __cplusplus
}
#endif

#endif  // __RILLIB_H
