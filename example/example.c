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

enum {AUTO_TEST_NONE, AUTO_TEST_FTP, AUTO_TEST_TCP, AUTO_TEST_SLEEP};

int event_x = NOWAIT_EVENT;
int autoTest = AUTO_TEST_NONE;
int fileSz = 0;
int fileCnt = 0;
int handle = 0;
int nPID;
unsigned char recvData[2048];
unsigned char sendData[1024];


void TcpConnect(){
  IPAddrT addr;
#if 1  
  WORD  port = 8888;
  addr.digit1 = 225;
  addr.digit2 = 90;
  addr.digit3 = 109;
  addr.digit4 = 31;
#else  
//210.181.29.11
  WORD  port = 9206; // 9215
  addr.digit1 = 210;
  addr.digit2 = 181;
  addr.digit3 = 29;
  addr.digit4 = 11;
#endif  
  TCPOpen(&addr, port);
}
void TcpTransmit(){
  memset(sendData,       0x00, 64);
  memset(sendData+64,    0x20, 64);
  memset(sendData+64+64, 0x30, 64);

  
  //strcpy((char*)sendData,"0123456789");
  if ( TCPSend(sendData, 64*3) < 0)
  {
  printf("TCPSend error\r\n");
  }
  //TCPSend(handle, sendData, 10);
}
void TcpReceive(){
  int i, rcvLen;
  
  rcvLen = TCPRecv(recvData, sizeof(recvData));

  if (rcvLen > 0) {
    recvData[rcvLen] = 0;
    
    printf("TCPRecv: %d Bytes %s\r\n", rcvLen, (char*)recvData);
    for (i=0; i < rcvLen; i++){
      if (i%16 == 0) printf("\r\n");
      printf("%02X ", recvData[i]);
    }

    printf("\r\n");
  }
}

void MsgParser(int msgID, int wparm, char * lparm)
{
  if (event_x == WAIT_EVENT)
  {
    printf("############[%d]WAIT_EVENT: 0x%x 0x%x\r\n", getpid(), msgID, wparm);
  
    if (BM_TCP == msgID)
    {
      printf("TCP: 0x%x\r\n", wparm);
      if (TCP_Recv == wparm ){
        TcpReceive();
        sleep(1);
        TCPClose();
      }
      else if (TCP_Connected == wparm ){
        //sleep(1);
        TcpTransmit();
      }
      else if (TCP_Idle == wparm ){
        sleep(1);
        TcpConnect();
      }
      else if (TCP_Error == wparm ){
        sleep(1);
        TcpConnect();
      }
    }

  }
  else {
    printf("############[%d]BmsgParser: 0x%x 0x%x\r\n", getpid(), msgID, wparm);

    if (BM_TCP == msgID){
      printf("Example=> TCP: %d\r\n", wparm&0xF);
      //if (TCP_RECEIVE == wparm ){
      //  TcpReceive();
      //}
    }
  }

  if (BM_DIAL == msgID)
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
        printf("SMS: message %s\n", msg->strMessage);
      else {
        int i;
        for (i=0; i < 70; i++)
        {
          if (msg->strMessage[2*i] == 0 && 0 == msg->strMessage[2*i + 1])
          {
            break;
          }
          
          if (i%8 == 0) 
            printf("\r\n");
            
          printf("%02X%02X ", (unsigned char)msg->strMessage[2*i] , (unsigned char)msg->strMessage[2*i+1] );
        }
        
        printf("\r\n");
      }
    }
    if (SMS_Sent == wparm ) {
      printf(" SMS SENT !!! \r\n");
    }

    
   
  }

}


int main(int argc, char *argv[])
{
  int main_loop = 1;
  int i, ret, recvSize; 
  char ch;
  static char msg_buf[sizeof(Msg2Client) + sizeof(TcpDataT)];

  Msg2Client *msg = (Msg2Client *)msg_buf;

  int msgSize = sizeof(Msg2Client) - sizeof(msg->rcvPID) + sizeof(TcpDataT); 

  for (i=1; i < argc; i++)
  {
    if (!strcmp("-w", argv[i]))
    {
      event_x = WAIT_EVENT;
      break;
    }else if (!strcmp("-t", argv[i])){
      autoTest = AUTO_TEST_TCP;
      event_x = WAIT_EVENT;
      break;
    }
    else if (!strcmp("-s", argv[i])){
      autoTest = AUTO_TEST_SLEEP;
      event_x = NOWAIT_EVENT;
      break;
    }    
  }

  printf("PID %d, VER: %s\n", getpid(), GetRilVersion());
  SetEventMask(ALL_EVENT_MASK);

  if (autoTest == AUTO_TEST_TCP) {
    TcpConnect();
  } else if (autoTest == AUTO_TEST_SLEEP) {
    while(1){
      recvSize = WaitEvent(msg, msgSize, event_x);
      if (recvSize > 0) continue;

      if (0 == (++main_loop)%10) {
        //SystemControl(handle, SLEEP_SYSTEM);
      } else {
        sleep(1);
      }

      if (access("/root/exit.flg", F_OK) == 0){
        break;
      }
    }

    goto doExit;
  }
  
  while (main_loop)
  {
    recvSize = WaitEvent(msg, msgSize, event_x);

    if (WAIT_EVENT == event_x) {
    	printf("msgrcv() recvSize! %d !!\n", recvSize);
    	goto ParseEvent;
    }
    
    if (recvSize <= 0)
    {
      if (errno == ENOMSG) 
      {
        // \B8޽\C3\C1\F6 ť\BF\A1 \B8޽\C3\C1\F6\B0\A1 \BE\F8\BE\EE\BF\E4~~~
        ;//printf("msgrcv() empty!!!\n");
      }

      //printf("getchar (x,a,d,h,m,2,1,0,s) \n");
      ch = getchar();

      //printf("getchar %c\n", ch);

      if ('x' == ch) 
      {
        main_loop = 0;
      }
      else if ('0' == ch)
      {
        //DataDisconnect();
      }
      else if ('1' == ch)
      {
        //DataConnect();
      }

      else if ('2' == ch)
      {
        TcpConnect();
      }
      else if ('3' == ch)
      {
        TcpTransmit();
      }
      else if ('4' == ch){
        TcpReceive();
      }
      else if ('5' == ch)
      {
        TCPClose();
      }
      else if ('6' == ch)
      {
        printf("TCP status %d\n/", TCPStatus());
      }

     
      else if ('a' == ch)
      {
        Answer();
      }
      else if ('d' == ch)
      {
        Dial("01224314391");
      }
      else if ('h' == ch)
      {
        HangUp();
      }

      else if ('m' == ch)
      {
        SendSMS("01224314391", "Linux RIL Test", MESSAGE_TYPE_ASCII);
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


      continue;
    }
    else
    {
      printf("recvSize %d\n", recvSize);
    }

ParseEvent:

    MsgParser(msg->msgID ,msg->wparm, msg->data);


  }

doExit:

  
  return 0;
}


