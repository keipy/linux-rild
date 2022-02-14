#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/signal.h>
#include <linux/input.h>
#include <fcntl.h>
#include <errno.h>
#include <dlfcn.h>
#include <time.h>
#include <pthread.h>

#include "ril.h"

unsigned char recvData[2048];
unsigned char sendData[1024];
char test_number[24];
int modem_run = 0;
char addr[256];
WORD  port = 8000;
char apn[100];


static pthread_t m_hModemThread;

void TcpConnect()
{

  TCPOpen(addr, port);
}

void TcpTransmit(){
  memset(sendData,       0x41, 64);
  memset(sendData+64,    0x20, 64);
  memset(sendData+64+64, 0x30, 64);

  if ( TCPSend(sendData, 64*3) < 0)
  {
	  printf("TCPSend error\r\n");
  }
}
void TcpReceive(){
  int i, rcvLen;
  
  rcvLen = TCPRecv(recvData, sizeof(recvData));

  if (rcvLen > 0) {
    recvData[rcvLen] = 0;
    
    printf("TCPRecv: len: %d Bytes\r\n", rcvLen);
    for (i=0; i < rcvLen; i++){
      if (i%16 == 0) printf("\r\n");
      printf("%02X ", recvData[i]);
    }

    printf("\r\n");
  }
}

void MsgParser(int msgID, int wparm, char * lparm)
{
	int i;
  printf("############[%d]BmsgParser: 0x%x 0x%x\r\n", getpid(), msgID, wparm);

	if (BM_TCP == msgID){
		printf("Example=> TCP: %d\r\n", wparm&0xF);
		if (TCP_Recv == wparm ){
			TcpReceive();
		}
		
		else if (TCP_Connected == wparm ){
			printf("TCP_Connected\r\n");
		}
		else if (TCP_Idle == wparm ){
			printf("TCP_Idle\r\n");
		}
		else if (TCP_Error == wparm ){
			printf("TCP_Error\r\n");
		}
	}
  else if (BM_DIAL == msgID)
  {
    printf("DIAL: %d\r\n", wparm);
    if (ATD_Cnip == wparm)
    {
			DialInfoT *info = (DialInfoT *)lparm;
			printf("CNIP: %s\r\n", info->strNumber);
    }
  }
  else if (BM_SMS == msgID)
  {
    printf("SMS: %d\r\n", wparm);
    
    if (SMS_Received == wparm )
    {
      SmsMsgT *msg = (SmsMsgT *)lparm;

      printf("SMS: number %s\n", msg->strNumber);
      if (msg->nType == MESSAGE_TYPE_ASCII )
    	{
        printf("SMS: message %s\n", msg->strMessage);
    	}
      else 
			{
        for (i=0; i < 70; i++)
        {
          if (msg->strMessage[2*i] == 0 && 0 == msg->strMessage[2*i + 1])
          {
            break;
          }

          printf("%02X%02X", (unsigned char)msg->strMessage[2*i+1] , (unsigned char)msg->strMessage[2*i] );
        }
        
        printf("\r\n");
      }
    }
    else if (SMS_Sent == wparm ) {
      printf(" SMS SENT !!! \r\n");
    }
  }

}

void *ModemThread(void *lpParam)
{
	int i, ret, recvSize; 
	static char msg_buf[sizeof(Msg2Client) + sizeof(TcpDataT)];
	Msg2Client *msg = (Msg2Client *)msg_buf;
  int msgSize = sizeof(Msg2Client) - sizeof(msg->rcvPID) + sizeof(TcpDataT); 

	while (modem_run)
	{
		recvSize = WaitEvent(msg, msgSize, WAIT_EVENT);
		if (errno == ENOMSG) 
		{
			printf(" ENOMSG !!! \r\n");;
		}


		if (recvSize > 0) 
		{
			MsgParser(msg->msgID ,msg->wparm, msg->data);
		}
		else
		{
		 printf(" ERR: recvSize less than zero !!! \r\n");;
		 break;
		}
	}

  return NULL;
}


void PrintUsage()
{
	printf("d : dial\n");
	printf("a : answer\n");
	printf("h : hangup\n");
	printf("0 : PPP hangup\n");
	printf("1 : PPP dialupt\n");
	printf("2 : TCP connect\n");
	printf("3 : TCP transmit\n");
	printf("5 : TCP close\n");
	printf("s: sms sending\n");
	printf("i : general information\n");
	printf("m : modem information\n");
	printf("r : radio status\n");
	printf("6 : set APN\n");
	printf("7 : set Scanmode\n");
	printf("x: help\n");
	printf("q : exit\n");
}

int main(int argc, char *argv[])
{
  int i, j, ret, main_loop = 1;
  char ch;

  for (i=1; i < argc; i++)
  {
		if (!strcmp("-n", argv[i])){
			strncpy(test_number, argv[++i], sizeof(test_number));
		}
		else if (!strcmp("-s", argv[i])){
			strncpy(addr, argv[++i], sizeof(addr));
			printf("ipaddr %s\n", addr);
		}
		else if (!strcmp("-p", argv[i])){
			port = atoi(argv[++i]);
			printf("port %u\n", port);
		}
		else if (!strcmp("-a", argv[i])){
			strncpy(apn, argv[++i], sizeof(apn));
			printf("apn %s\n", apn);
		}
  }

  printf("PID %d, VER: %s\n", getpid(), GetRilVersion());
  SetEventMask(ALL_EVENT_MASK);

	PrintUsage();
	modem_run = 1;
	ret = pthread_create(&m_hModemThread, NULL, (void *)ModemThread, NULL);
  
  while (main_loop)
  {
    ch = getchar();

		if ('x' == ch)
		{
			PrintUsage();
		}
    else if ('q' == ch) 
    {
      main_loop = 0;
    }
    else if ('0' == ch)
    {
      DataDisconnect();
    }
    else if ('1' == ch)
    {
      DataConnect();
    }
    else if ('2' == ch)
    {
      TcpConnect();
    }
    else if ('3' == ch)
    {
      TcpTransmit();
    }
    else if ('4' == ch)
		{
      TcpReceive();
    }
    else if ('5' == ch)
    {
      TCPClose();
    }
    else if ('t' == ch)
    {
      printf("TCP status %d\n", TCPStatus());
    }
    else if ('a' == ch)
    {
      Answer();
    }
    else if ('d' == ch)
    {
      if (test_number[0] == 0)
				printf("number not defined\n");
			else
      Dial(test_number);
    }
    else if ('h' == ch)
    {
      HangUp();
    }
    else if ('s' == ch)
    {
      if (test_number[0] == 0)
				printf("number not defined\n");
			else
      SendSMS(test_number, "Linux RIL Test  @written by keipy@", MESSAGE_TYPE_ASCII);
    }
    else if ('r' == ch)
    {
      printf("Reg %d, RSSI %d, RSRP %d, RSRQ %d, Operator: %s\n", GetRegistration(), GetRSSI(), GetRSRP(), GetRSRQ(), GetNetworkName());
    }
    else if ('i' == ch)
    {
      printf("RadioTech %d, RASStatus %d, LTE Bands %x, SCAN Mode %d, APN %s \n", GetRadioTech(), GetDataState(), GetLTEBands(), GetNWScanMode(), GetAPN());
    }
    else if ('m' == ch)
    {
      IPAddrT  ipAddr;
      printf("IMEI %s, VER %s, NUM %s, ICCID %s, SIM_Status x%x\n", GetSerialNumber(), GetModemVersion(), GetPhoneNumber(), GetSIMID(), GetSIMStatus());
			
			if (!GetIPAddress(&ipAddr)) {
				if (ipAddr.ip_version == IP_VER_4){
					printf("IPV4:  %d.%d.%d.%d\n", ipAddr.addr.ipv4.digit[0], ipAddr.addr.ipv4.digit[1], ipAddr.addr.ipv4.digit[2], ipAddr.addr.ipv4.digit[3]);
				}
				else {
					printf("IPV6:  %x:%x:%x:%x:%x:%x:%x:%x\n", ipAddr.addr.ipv6.seg[0], ipAddr.addr.ipv6.seg[1], ipAddr.addr.ipv6.seg[2], ipAddr.addr.ipv6.seg[3], 
																     ipAddr.addr.ipv6.seg[4], ipAddr.addr.ipv6.seg[5], ipAddr.addr.ipv6.seg[6], ipAddr.addr.ipv6.seg[7]);
				}
			}
			
    }
		else if ('6' == ch)
		{
			SetAPN(apn);
		}
		else if ('7' == ch)
		{
		  printf("SCAN: 0:AUTO, 2:WCDMA, 3:LTE => ");
			do{
				ch = getchar();
			}while (ch != '0' && ch != '2' && ch != '3');

			SetNwScan( (int)(ch -'0'));
    }

  }


	modem_run = 0;

	if(m_hModemThread) {
		pthread_cancel(m_hModemThread);
		m_hModemThread = 0;
	}
  return 0;
}


