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
IPAddrT addr;
WORD  port = 8000;


static pthread_t m_hModemThread;

void TcpConnect(){
  TCPOpen(&addr, port);
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
	printf("r : radio status\n");
	printf("i : general information\n");
	printf("n : modem information\n");
	printf("m: sms sending\n");
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
		
		else if (!strcmp("-i", argv[i])){
		  char *pToken = NULL;
			
			pToken = strtok(argv[++i], " ,\".");
			for (j = 0; NULL != pToken; j++)
			{
					if (0 == j)
							addr.digit1 = (unsigned char)atoi(pToken);
					else if (1 == j)
							addr.digit2 = (unsigned char)atoi(pToken);
					else if (2 == j)
							addr.digit3 = (unsigned char)atoi(pToken);
					else if (3 == j)
							addr.digit4 = (unsigned char)atoi(pToken);
		
					pToken = strtok(NULL, " ,\".");
			}

			 printf("ipaddr %u.%u.%u.%u\n", addr.digit1, addr.digit2, addr.digit3, addr.digit4);
		}
		else if (!strcmp("-p", argv[i])){
			port = atoi(argv[++i]);
			printf("port %u\n", port);
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
    else if ('m' == ch)
    {
      if (test_number[0] == 0)
				printf("number not defined\n");
			else
      SendSMS(test_number, "Linux RIL Test  @written by keipy@", MESSAGE_TYPE_ASCII);
    }
    else if ('r' == ch)
    {
      printf("RadioTech %d, RASStatus %d ICCID %s SIM x%x\n", GetRadioTech(), GetDataState(), GetICCID(), GetSIMStatus());
    }
    else if ('i' == ch)
    {
      printf("RSSI %d, Registration %d, RSRP %d, Operator: %s\n", GetRSSI(), GetRegistration(), GetRSRP(), GetNetworkName());
    }
    else if ('n' == ch)
    {
      printf("IMEI %s, REV %s, NUM %s\n", GetSerialNumber(), GetModemVersion(), GetPhoneNumber());
    }

  }


	modem_run = 0;

	if(m_hModemThread) {
		pthread_cancel(m_hModemThread);
		m_hModemThread = 0;
	}
  return 0;
}


