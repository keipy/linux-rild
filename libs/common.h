/****************************************************************************
  common.h
  Author : kpkang@gmail.com
*****************************************************************************/
#ifndef __RESERVED_API_H
#define __RESERVED_API_H

#include "ril.h"

/* Agent Window messages */
#define WM_USER                (1024)
#define WM_USER_MSG_PROC	     (WM_USER+250)

#define SHARED_MEMORY_KEY      1234
#define MSG2SERVER_QUE_KEY     4499
#define MSG2CLIENT_QUE_KEY     4498
#define MSG2TCPSND_QUE_KEY     4497

#define MAX_MODEL_LENGTH       16
#define MAX_VERSION_LENGTH     40
#define MAX_SERIAL_LENGTH      20
#define MAX_NETNAME_LENGTH     20

#define MAX_IMSI_LENGTH        20
#define MAX_ICCID_LENGTH       24
#define MAX_NUMBER_LENGTH      20
#define MAX_APN_LENGTH         100

#define MAX_MESSAGE_LENGTH     160

#define MSGQUEUE_NAME       L"Message Queue"
#define MAX_MSG_QUEUE 20

#define DEFAULT_MODEM_VOLUME  2
#define DEFAULT_MODEM_MICGAIN 6

#define UPDATE_PHONENUMBER  0x1
#define UPDATE_SERIALNUMBER 0x2
#define UPDATE_MODEMVERSION 0x4
#define UPDATE_NETWORKNAME  0x8
#define UPDATE_MODEM_MODEL   0x10
#define UPDATE_SIM_IMSI 0x20
#define UPDATE_SIM_ICCID 0x40
#define UPDATE_APN 0x80


#define RSSI_INIT_VALUE    (-128)
#define ECIO_INIT_VALUE    (-31)


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
#endif // _WIN_TYPE_

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif

enum
{
  CALL_DIAL = 1,
  CALL_ANSWER,
  CALL_HANGUP,
  CALL_VOLUME,
  CALL_MIC,
  CALL_DTMF,

  TCP_OPEN = 20,
  TCP_SEND,
  TCP_CLOSE,
  TCP_STATUS,

  SMS_SEND = 40,
  DATA_CONNECT,
  DATA_DISCONNECT,
  GPS_START,
  GPS_STOP,
  
	APN_SET = 60,
	BAND_SET,
	SCAN_SET,
  
  SYS_CONTROL = 80,
  PROC_ATTACH,
  PROC_DETACH,
  PROC_UPDATE,
};


enum
{
  NET_UNKNOWN = 0,
  NET_WCDMA = 2,
  NET_LTE,
};

typedef struct {
  long caller;   /* message type, must be > 0 */
  int  msgID;
  int  wparm;
  union {
    char data[sizeof(int)]; 
    int  lparm;
  } u;
}Msg2Agent;

typedef struct
{
	char strModelId[MAX_MODEL_LENGTH];
  char strModemVersion[MAX_VERSION_LENGTH ];     // AT+GMR
  char strSerialNumber[MAX_SERIAL_LENGTH ];     // AT+GSN
  char strNetworkName[MAX_NETNAME_LENGTH];        // AT+COPS
	char strAPN[MAX_APN_LENGTH];			// CGDCONT
}ModemInfo;



typedef struct
{
	char strIMSI[MAX_IMSI_LENGTH]; // GSN
  char strICCID[MAX_ICCID_LENGTH];     // AT+ICCID
  char strPhoneNumber[MAX_NUMBER_LENGTH];      // AT+CNUM
}UiccInfo;



typedef struct{

  int          agentPID;
  int          agentQue;  // for client -> server
  int          tcpipQue;
  char         strAgentVer[MAX_VERSION_LENGTH];
  ModemInfo    modemInfo;
	UiccInfo     simInfo;
  IPAddrT      modemIP;
  int          nRSSI;
  int          nRSRQ;
  int          nRSRP;
  int          nRadioTech;
  int          nRegistration;
  int          eRASState ;
  int          eATDState ;
  int          eSIMState ;
  int          eSMSState ;
  int          eGPSState ;
  int          nRejectCode;
  int          nModemVolume;
  int          nModemMicGain;
	int          nWCDMABands;
	int          nLTEBands;
	int          nNwScanMode;
} SharedData;

#endif
