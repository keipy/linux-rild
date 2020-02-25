#ifndef __CLIENTIF_H
#define __CLIENTIF_H

#include "common.h"

#define AGENT_VERSION "1.01"

void Initialize(int pid, int qid, int tcp_qid);
void Uninitialize(void);

void ResetSharedData(void);

void SetRegistration(int nReg);
void SetModemInfo(char *strInfo, int update);
void SetICCID(char *strInfo);

void SetRASState(int pid, int state);
void SetSMSState(int pid, int state);
void SetATDState(int pid, int state);
void SendCNIPInfo(char *pNumber);
void SendSMSRecv(char *pNumber, int type, char *pMessage, int len );
void SetTCPState(int pid, int state, int err);
void SendAlertMsg(int pid, int warn);
void SetModemVolume(int nVol);
void SetMicGain(int nMic);

char * GetPhoneNumber(void);
void SetRFState(int rssi, int rsrp);
void SetUSIMState(int state);

void SetMdmIPAddr(IPAddrT *addr);
void GetMdmIPAddr(IPAddrT *addr);


void SetRadioTech(int tech);

int  CheckClient(int pid);
int  GetRASState(void);
int  GetATDState(void);
int  GetSMSState(void);
int  GetRadioTech(void);
int  GetRASCount(void);
int  GetUSIMState(void);
void SetRejectCode(int code);
char *GetModemVersion(void);
char *GetModemSerial(void);
char *GetICCID(void);
int GetRejectCode(void);

#endif
