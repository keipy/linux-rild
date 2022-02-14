/****************************************************************************
  agent.c
  Author : kpkang@gmail.com
*****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>

#include <linux/rtc.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <sys/stat.h>

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/un.h>

#include <time.h>
#include <pthread.h>
#include <limits.h>
#include <errno.h>
#include <linux/watchdog.h>

#include "agent.h"
#include "m2mdbg.h"
#include "utils.h"

#include "inforesponse.h"
#include "unsolicited.h"
#include "finalresult.h"
#include "oemlayer.h"
#include "cmdlist.h"

//#define _THREAD_TICK_
//#define _REPORT_RESETCAUSE_
//#define _IP_MISMATCH_CHECK_

#define _PPPD_CHILD_
#define _WVDIAL_USED_

#define AT_COMMAND_SIZE    1460
#define AT_RESPONSE_SIZE   (AT_COMMAND_SIZE+128)

/* Agent Window messages */
#define WM_USER_HANGUP     SIGHUP  /* 1  */
#define WM_USER_TIMER      SIGALRM /* 14 */
//#define WM_USER_DESTROY    SIGTERM /* 15 */
#define WM_USER_STOP       SIGSTOP /* 19, CTRL+Z, fg, bg*/
#define WM_USER_INT        SIGINT  /* 2,  CRTL+C */
#define WM_USER_QUIT       SIGQUIT /* 3,  CTRL+\ */
#define WM_USER_USER1      SIGUSR1 /* 10 */
#define WM_USER_USER2      SIGUSR2 /* 12 */ 

#define DEFAULT_KICK_TIMEOUT 16

#define WATCHDOG "/dev/watchdog"
#define PORT_OPEN_WAIT_SECS 15  /* for 15 sec */

enum {SYSTEM_PWR_NONE,SYSTEM_PWR_REBOOT, SYSTEM_PWR_OFF};

const comm_info_t cmd_table[] =
{
  {"", NULL},
  {"ATE0\r", NULL},
  {"ATA\r", NULL},
  {"AT+GMR\r", response_version},
  {"AT+GMM\r", response_model},
  {"AT+CPIN?\r", response_cpin},
  {"AT+CNUM\r", response_cnum},
  {"AT+CMGD=0,4\r", NULL},
  {"AT+CNMI=2,2,0,0,0\r", NULL},
  {"AT+CMGL=0\r", response_cmgl},

  {"AT+CMGF=0\r", NULL},
  {"AT+CGREG?\r", response_cgreg},
  {"AT+CEREG=1\r", NULL},
  {"AT+COPS?\r", response_cops},
  {"AT+CGDCONT?\r", response_cgdcont},
  {"AT+CGREG=1\r", NULL},
  {"AT+CMEE=1\r", NULL},
  {"AT+CCLK?\r", response_cclk},
  {"AT+CIMI\r", response_cimi},
  {"AT+CHUP\r", NULL},
  
  {"AT+CGSN\r", response_cgsn},
  {"AT+CFUN=1,1\r", NULL},
  {"AT+CGPADDR\r", response_cgpaddr},
  {"AT+QCSQ\r", response_qcsq},
  {"AT+QPOWD\r", NULL},
  {"AT+QSCLK?\r", NULL},
  {"AT+QSCLK=1\r", NULL},
  {"AT+QCDS\r", response_qcds},
  {"AT+QCPS?\r", response_qcps},
  {"AT+QCOTA?\r", response_qcota},
  
  {"AT+QCNC?\r", response_qcnc},
  {"AT+QCNC=1,1,1\r", NULL},
  {"AT+ICCID\r", response_iccid},
  {"AT^DSCI=1\r", NULL}, 
  {"AT+QCFG=\"band\"\r", response_band},
  {"AT+CNMI=2,2,1,0,0\r", NULL},
  {"AT+CLVL?\r", response_clvl},
  {"AT+CMVL?\r", response_cmvl},
  {"AT+QCFG=\"nwscanmode\"\r", response_nwscanmode},
  {"AT+QCFG=\"nwscanmode\",2\r", NULL},
  
  {"AT+QCFG=\"nwscanmode\",0\r", NULL},
  {"AT+QCFG=\"nwscanmode\",3\r", NULL},
  {"AT+QCFG=\"risignaltype\"\r", response_risignaltype},
  {"AT+QCFG=\"risignaltype\",\"physical\"\r", NULL},
  {"AT+QCFG=\"apready\"\r", response_apready},
  {"AT+QCFG=\"apready\",1,0,200\r", NULL},
  {"AT+QCFG=\"urc/ri/smsincoming\"\r", response_urc_ri_sms},
  {"AT+QCFG=\"urc/ri/smsincoming\",\"pulse\",360\r", NULL},
  {"AT+QURCCFG=\"urcport\"\r", response_urcport},
  {"AT+QURCCFG=\"urcport\",\"usbat\"\r", NULL},
  
  {"AT+QURCCFG=\"urcport\",\"uart1\"\r", NULL},
  {"AT+QURCCFG=\"urcport\",\"all\"\r", NULL},
  {"AT+CLCC\r", NULL},
  {"AT+QICSGP?\r", response_qicsgp},
  {"AT+QIACT?\r", response_qiact},
  {"AT+QIACT=1\r", NULL},
  {"AT+QIDEACT=1\r", NULL},
  {"AT+QISTATE?\r", response_qistate},
  {"AT+QICFG=\"dataformat\";+QICFG=\"viewmode\";+QICFG=\"transpktsize\";+QICFG=\"passiveclosed\"\r", response_qicfg},
  {"AT+QICFG=\"dataformat\",0,1\r", NULL},
  
  {"AT+QICFG=\"viewmode\",1\r", NULL},
  {"AT+QICFG=\"transpktsize\",1460\r", NULL},
  {"AT+QICFG=\"passiveclosed\",1\r", NULL},
  {"AT+QICFG=\"tcp/retranscfg\",3,20\r", NULL}, /* retransmission 3 times every 2 secs */
  {"AT+QSIMDET?\r", response_qsimdet},
  {"AT+QSIMDET=1,0\r", NULL},
  {"AT+QSIMDET=1,1\r", NULL},
  {"AT+QSIMSTAT?\r", response_qsimstat},
  {"AT+QGPIO=0,34\r", response_sim_presence},
  {"AT+QTEMP\r", response_temp},
  
  {"AT+QADC=0\r", response_adc},
  {"AT+QADC=1\r", response_adc},
  
};

char APN_NAME[100] = "notset";

DWORD argument_mask = MSG_ERROR;

byte g_nCnumEmptyCnt  = 0;
byte g_nTimeUpdated   = 0;
byte g_nSimNotInsertedCnt = 0;
byte g_nRecoveryCountDown = 0;
byte g_eSIMStatus    = SIM_NONE;  /* internal sim status */
byte g_eRegistration = REG_UNKNOWN;
byte g_eHotSwapStat  = HOTSWAP_NONE;
byte m_nCidLists = 0;

int g_nSendPDULength;
int g_nRecvMessageIndex;
int g_nRSSI, g_nRSRP, g_nRSRQ;
#if 0//def SUPPORT_VOICE_CALL
int g_nCallInfoCount = 0;
#endif

int g_pidOfSMS;
int g_pidOfATD;
#ifdef SUPPORT_TCP_CMD
int g_nTCPRecvLen = 0;
int g_pidOfTCPRecv = 0;
sock_info_t g_tcpArray[10];
#endif

proc_root_t parent_proc;

//varCmdType varCommand[2];
//#define CMD_BUF(A) varCommand[A - VAR_OFFSET].command

static DWORD mainSysTick = 0;


static  pthread_t m_hWanThread;
static  pthread_t m_hSerThread;
#ifdef _THREAD_TICK_
static  pthread_t m_hTmrThread;
#endif
static  byte m_nATCommandID;
static  byte m_nATResultTimer = 0;
static  byte m_nNoResultCount = 0;
static  byte m_nStatusTimer   = TIMER_MODEM_STATUS;

static  int m_fdSerial   = -1;

const   char strDelimit[] = " ,\"/:";
const   char strDelimit_NoSpace[] = ",\"/:";
const   char strSendPDUNoti[] = "> ";
const   char chSendPDUEnd = 0x1A;


/////////////////////////////////////////////////
/////////////////////////////////////////////////
static  int		 m_nAgentPID;
static  int		 m_nAgentQueID;
static  int    m_nDataCallPID = 0;
static  int		 m_nTCPIPQueID;


static char CMD_PORT[16];
static char DUN_PORT[16]  = "/dev/ttyUSB3";


static pthread_mutex_t s_connectmutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  s_connectcond  = PTHREAD_COND_INITIALIZER;



static void SigHandler(int sig)
{
	switch(sig) {
		case SIGQUIT:
			DEBUG(MSG_HIGH, "agent \"%s\" signal\n", "Quit");
			break;
		case SIGINT:
			DEBUG(MSG_HIGH, "agent \"%s\" signal\n", "Interrupt");
			break;
		case SIGTERM:
			DEBUG(MSG_HIGH, "agent \"%s\" signal\n", "Termination");
			break;
		case SIGABRT:
			DEBUG(MSG_HIGH, "agent \"%s\" signal\n", "Abort");
			break;
		case SIGSEGV:
      signal(SIGSEGV, SIG_IGN);
			DEBUG(MSG_HIGH, "agent \"%s\" signal\n", "Segmentation fault");
			break;
    case SIGCHLD:
      DEBUG(MSG_HIGH, "agent \"%s\" signal\n", "Child status");
      //SendMsgQue(WM_USER_NET_STAT, 0, 0); 
      return;        
	}

	// send signal error event to main loop
	SendMsgQue(WM_USER_DESTROY, 0, 0);
}

static void TmrHandler(int sig)
{
  //DEBUG(MSG_LOW, "agent ALARM signal\n");
  SendMsgQue(WM_USER_TIMER, 0, 0);
}

static void UsrHandler(int sig)
{
  if (SIGUSR1 == sig){ 
    DEBUG(MSG_ERROR, "agent USER1 signal\n");
    SendMsgQue(WM_USER_USER1, 0, 0);
  } else if (SIGUSR2 == sig){
    DEBUG(MSG_ERROR, "agent USER2 signal\n");
    SendMsgQue(WM_USER_USER2, 0, 0);
  }
}


void SigRegister(void)
{
	if(signal(SIGQUIT, SigHandler) == SIG_IGN) {
		signal(SIGQUIT, SIG_IGN);
	}
	if(signal(SIGINT, SigHandler) == SIG_IGN) {
		signal(SIGINT, SIG_IGN);
	}
	if(signal(SIGTERM, SigHandler) == SIG_IGN) {
		signal(SIGTERM, SIG_IGN);
	}
	if(signal(SIGABRT, SigHandler) == SIG_IGN) {
		signal(SIGABRT, SIG_IGN);
	}
	if(signal(SIGSEGV, SigHandler) == SIG_IGN) {
		signal(SIGSEGV, SIG_IGN);
	}
	if(signal(SIGUSR1, UsrHandler) == SIG_IGN) {
		signal(SIGUSR1, SIG_IGN);
	}
	if(signal(SIGUSR2, UsrHandler) == SIG_IGN) {
		signal(SIGUSR2, SIG_IGN);
	}
  if (argument_mask & NET_ETH_AUTO)
  {
  	if(signal(SIGCHLD, SigHandler) == SIG_IGN) {
  		signal(SIGCHLD, SIG_IGN);
  	}
	}
}

void SigDeregister(void)
{
	signal (SIGQUIT, SIG_IGN);
	signal (SIGINT,  SIG_IGN);
	signal (SIGTERM, SIG_IGN);
	signal (SIGABRT, SIG_IGN);
	signal (SIGSEGV, SIG_IGN);
	signal (SIGUSR1, SIG_IGN);
	signal (SIGUSR2, SIG_IGN);
	if (argument_mask & NET_ETH_AUTO)
	{
  	signal (SIGCHLD, SIG_IGN);
	}
}

int ParseArgs(int argc, char *argv[])
{
  int i, ret = 1;

  // rild -d /dev/ttyS4 -p -a lte.ktfwing.com -v
  for (i=1; i < argc; i++)
  {
    if (!strcmp("-d", argv[i]))
    {
      if (NULL != argv[i+1])
    	{
        strncpy(CMD_PORT, argv[++i], sizeof(CMD_PORT)-1);
    	}
    }
    else if (!strcmp("-p", argv[i]))
    {
      argument_mask |= NET_PPP_AUTO;
    }
    else if (!strcmp("-e", argv[i]))
    {
      argument_mask |= NET_ETH_AUTO;
    }
		else if (!strcmp("-a", argv[i]))
    {
      if (NULL != argv[i+1] && *argv[i+1] != '-')
    	{
    	  str_uppercase(argv[++i]);
				
    	  if (!strcmp(argv[i], "NULL"))
  	  	{
	  	  	memset(APN_NAME, 0x0, sizeof(APN_NAME));
  	  	}
				else
				{
		      strncpy(APN_NAME, argv[i], sizeof(APN_NAME)-1);
				}
    	}
    }
		else if (!strcmp("-v", argv[i]))
		{
			argument_mask |= (MSG_ERROR|MSG_HIGH|MSG_MID|MSG_LOW);
		}
  }


  if ( !strncmp(CMD_PORT, "/dev/", 5) )
  {
    if (strstr(CMD_PORT, "USB") || strstr(CMD_PORT, "ACM")) {
      argument_mask |= AT_INTF_USB;
    } else  {
      argument_mask |= AT_INTF_UART;
    }
  }
  else
  {
    DEBUG(MSG_ERROR, "dev: %s\n", CMD_PORT);
    ret = -1;
  }

	DEBUG(MSG_ERROR, "ParseArgs AT port : %s\n", CMD_PORT);

  return ret;
}


void OpenATPort(char *sPort)
{
  int ret, waitcnt = 0;
  int baudrate = 115200;

  if (AT_INTF_UART & argument_mask) {
    fd_set readfd;
    struct  timeval tv = {0, 0};
  
    m_fdSerial = OpenSerial(sPort, baudrate, 0);
      
  	FD_ZERO(&readfd);
  	FD_SET(m_fdSerial, &readfd);
    tv.tv_sec  = PORT_OPEN_WAIT_SECS;
    tv.tv_usec = 0;

  	ret = select(m_fdSerial+1, &readfd, 0, 0, &tv); // block until the data arrives
  	if (ret > 0) {
    	SendMsgQue(WM_USER_RX_CHAR, 0, 0);
    }
  }
  else
  {
    while (m_fdSerial < 0) {
      if (IsTTYAvailable(sPort)) {
  	    m_fdSerial = OpenSerial(sPort, baudrate, 1);
      } else {
        if ((++waitcnt)%PORT_OPEN_WAIT_SECS == 0) { // for 15 sec
          ModemReset();
        }
      }
      msleep(1000);
    }
  }
  
	ret = pthread_create(&m_hSerThread, NULL, (void*)SerRxThread, NULL);
	if(ret) {
		DEBUG(MSG_ERROR, "fail to create SerRxThread!\n");
	}else{
	  pthread_detach(m_hSerThread);
	}
}

void CloseATPort(void)
{
  if (IsPortOpened()){
    if (m_hSerThread) {
      pthread_cancel(m_hSerThread);
      m_hSerThread = (pthread_t)NULL;
    }

    msleep(500);
    CloseSerial(m_fdSerial);
    m_fdSerial = -1;
  }
}

static void Daemonize(void)
{
    pid_t pid;

    /* Fork off the parent process */
    pid = fork();

    /* An error occurred */
    if (pid < 0)
        exit(EXIT_FAILURE);

    /* Success: Let the parent terminate */
    if (pid > 0)
        exit(EXIT_SUCCESS);

    /* On success: The child process becomes session leader */
    if (setsid() < 0)
        exit(EXIT_FAILURE);

    /* Catch, ignore and handle signals */
    //TODO: Implement a working signal handler */
    signal(SIGCHLD, SIG_IGN);
    signal(SIGHUP, SIG_IGN);

    /* Fork off for the second time*/
    pid = fork();

    /* An error occurred */
    if (pid < 0)
        exit(EXIT_FAILURE);

    /* Success: Let the parent terminate */
    if (pid > 0)
        exit(EXIT_SUCCESS);

    /* Set new file permissions */
    umask(0);

    /* Change the working directory to the root directory */
    /* or another appropriated directory */
    chdir("/");

    /* Close all open file descriptors */
    int x;
    for (x = sysconf(_SC_OPEN_MAX); x>=0; x--)
    {
        close (x);
    }

    /* Open the log file */
    openlog ("Agent", LOG_PID, LOG_DAEMON);
}

int main(int argc, char *argv[])
{
  static char msg_buf[sizeof(Msg2Agent) + sizeof(SmsMsgT)];

  int mainLoopRun;
  int pwrDownMode;
  int recvSize, msgSize;
  Msg2Agent *msg = (Msg2Agent *)msg_buf;

	DEBUG(MSG_ERROR, "ver: %s\n", AGENT_VERSION);

  if (ParseArgs(argc, argv) < 0){
    return 0;
  }

  if (DAEMON_MODE & argument_mask) Daemonize();

  InitWatchDog();
  InitAgent();
  SigRegister();
  Init1SecTick();
  InitDataIntf();

  KICK_WATCHDOG;
  mainLoopRun = 1;
  pwrDownMode = SYSTEM_PWR_NONE;
  msgSize = sizeof(Msg2Agent) + sizeof(SmsMsgT) - sizeof(msg->u.data); 
  msgSize -= sizeof(msg->caller);
  
  while (mainLoopRun)
  {
    errno    = ENOMSG;
    recvSize = msgrcv(m_nAgentQueID, msg, msgSize, 0/*m_nAgentPID*/, 0 /* IPC_NOWAIT */);

    if (recvSize <= 0)
    {
      if (ENOMSG == errno) {
        DEBUG(MSG_ERROR, "msgrcv() ENOMSG !\n");
      } else if (EINTR != errno) {
        DEBUG(MSG_ERROR, "msgrcv() error(%d) %s\n", errno, strerror(errno));
        break;
      }        
      continue;
    }
    
    switch (msg->msgID) 
    {
      case WM_USER_RX_CHAR:
        OnRecvSerial();
        break;
          
      case WM_USER_AT_CMD:
        OnPutCommand(NONE);
        break;

      case WM_USER_TIMER:
        OnTimer();
        break;

      case WM_USER_MSG_PROC:
        OnReadMsgQue(msg->caller, msg->wparm, msg->u.data);
        break;
          
      case WM_USER_RAS_RES:
        OnRasResult(msg->wparm, msg->u.lparm);
        break;

      case WM_USER_RX_ERROR:
        OnSerialError(msg->wparm, msg->u.lparm);
        break;
#if 0        
      case WM_USER_NET_STAT :
        OnNetStatus();
        break;
#endif
        
      case WM_USER_USER1 :
        OnUserSignal(1);
        break;

      case WM_USER_USER2 :
        OnUserSignal(2);
        break;

      case WM_USER_DESTROY:

        mainLoopRun = 0;
        pwrDownMode = msg->wparm;
        break;

      default:
        DEBUG(MSG_ERROR, "msgID undefined message %d\n", msg->msgID);
        break;
    }
  }

  DeinitDataIntf();
  Deinit1SecTick();
  SigDeregister();
  DeinitAgent();
  DeinitWatchDog();

  DEBUG(MSG_ERROR, "agent terminated!!!\n");

  if (DAEMON_MODE & argument_mask) closelog();

  if (argument_mask & PWROFF_CTRL)
  {
    if (SYSTEM_PWR_REBOOT == pwrDownMode){
      ModemOff(1);
      msleep(5000);
      KICK_WATCHDOG;
      system("reboot");
    }
    else if (SYSTEM_PWR_OFF == pwrDownMode){
      ModemOff(1);
      msleep(5000);
      KICK_WATCHDOG;
      msleep(1000);
      system("poweroff");
    }
  }
  
	return EXIT_SUCCESS;
}

void InitAgent(void)
{
  struct msqid_ds ds;

  InitCmdList();

  m_nAgentPID = getpid();

retry:  
  m_nAgentQueID = msgget(MSG2SERVER_QUE_KEY, IPC_CREAT | 0666);
 
  msgctl(m_nAgentQueID, IPC_STAT, &ds);

  if (ds.msg_qnum > 0 /*|| m_nAgentQueID == 0*/ )
  {
    msgctl(m_nAgentQueID, IPC_RMID, (struct msqid_ds *)0);
    goto retry;
  }

#if defined(SUPPORT_TCP_CMD)
  retry_tcp:
    m_nTCPIPQueID = msgget(MSG2TCPSND_QUE_KEY, IPC_CREAT | 0666);
    msgctl(m_nTCPIPQueID, IPC_STAT, &ds);
  
    if (ds.msg_qnum > 0 /*|| m_nTCPIPQueID == 0*/ )
    {
      msgctl(m_nTCPIPQueID, IPC_RMID, (struct msqid_ds *)0);
      goto retry_tcp;
    }
#endif
  DEBUG(MSG_HIGH, "InitAgent(%s) QID %d TCPQID %d\r\n", AGENT_VERSION, m_nAgentQueID, m_nTCPIPQueID);  

  Initialize(m_nAgentPID, m_nAgentQueID, m_nTCPIPQueID);


#ifdef SUPPORT_MODEM_SLEEP
  MODEM_WAKEUP;
  AP_URC_READY;
#endif

  OpenATPort(CMD_PORT);

  m_nATCommandID  = NONE;
  m_nStatusTimer  = TIMER_MODEM_STATUS;
  g_nRSSI         = RSSI_INIT_VALUE;
  g_nRSRP         = RSSI_INIT_VALUE;
  g_eRegistration = REG_UNKNOWN;


	InitModem(SIM_NOT_INSERTED);
}

void DeinitAgent()
{
  DEBUG(MSG_HIGH,"DeinitAgent()\r\n");

  CloseATPort();


  Uninitialize();
  DeinitCmdList();
  
  msgctl(m_nAgentQueID, IPC_RMID, NULL);
  
#if defined(SUPPORT_TCP_CMD)
   msgctl(m_nTCPIPQueID, IPC_RMID, NULL);
#endif
}

void InitModem(int simState)
{
  if (SIM_NOT_INSERTED == simState)
  {
    g_eSIMStatus = SIM_NOT_INSERTED;
    
    SendCommand(ATE0,       FALSE, NULL);
#ifdef SUPPORT_USIM_PROVISIONING
    SendCommand(AT_QCOTA,   FALSE, NULL);
#endif
    SendCommand(AT_GMR,    FALSE, NULL);
    SendCommand(AT_CGSN,    FALSE, NULL);
    SendCommand(AT_QURCCFG, FALSE, NULL);
#ifdef SUPPORT_SIM_HOTSWAP
    SendCommand(AT_QSIMDET,   FALSE, NULL);
#endif
#ifdef SUPPORT_MODEM_SLEEP
    SendCommand(AT_QCFG_APREADY, FALSE, NULL);
    SendCommand(AT_QCFG_SMSRIURC,FALSE, NULL);
#endif
#ifdef SUPPORT_TCP_CMD
    SendCommand(AT_QICFG_RETRANS_3_20, FALSE, NULL);
    SendCommand(AT_QICFG_PKTSIZE_1460, FALSE, NULL);
#ifdef SUPPORT_TCP_HEX_CMD
    SendCommand(AT_QICFG_VIEWMODE_1, FALSE, NULL);
    SendCommand(AT_QICFG_DATAFORMAT_0_1, FALSE, NULL);
#endif
#endif // SUPPORT_TCP_CMD
		SendCommand(AT_QCFG_BAND,FALSE, NULL);
		SendCommand(AT_QCFG_NW,FALSE, NULL);
    SendCommand(AT_CGREG_1,FALSE, NULL);
    SendCommand(AT_CMEE_1, FALSE, NULL);
    SendCommand(AT_CPIN,   FALSE, NULL);
  }
  else if (SIM_READY == simState &&  g_eSIMStatus != SIM_READY)
  {
    g_eSIMStatus = SIM_READY;
    SetUSIMState(SIM_READY); 

		SendCommand(AT_CGDCONT, FALSE, NULL);

#ifdef SUPPORT_VOICE_CALL
    SendCommand(AT_DSCI_1, FALSE, NULL);
#endif
    SendCommand(AT_CMGF_0,   FALSE, NULL);
		SendCommand(AT_CMGL_0,	 FALSE, NULL);
    SendCommand(AT_CMGD_0_4, FALSE, NULL);
    SendCommand(AT_CNMI_2_2, FALSE, NULL);

    SendCommand(AT_ICCID,    FALSE, NULL);
		SendCommand(AT_CIMI,     FALSE, NULL);
    SendCommand(AT_CNUM,     FALSE, NULL);
    SendCommand(AT_QCDS,     FALSE, NULL);
#ifdef SUPPORT_MODEM_SLEEP
    SendCommand(AT_QSCLK_1,  FALSE, NULL);
#endif    
    SendCommand(AT_CCLK,     FALSE, NULL);
    SendCommand(AT_COPS,     FALSE, NULL);
		SendCommand(AT_CGPADDR,  FALSE, NULL);

    //SendAlertMsg(0, ERR_MODEM_RECOVERY_OK);
  }
}

int InitConnMgr(void)
{
  int i, ret = 0;
  
	m_nDataCallPID = fork();
	if(m_nDataCallPID < 0) {
	  m_nDataCallPID = 0;
		DEBUG(MSG_ERROR, "agent fork failed!!\n");
		return -1;
	}
	else if(m_nDataCallPID > 0) 
	{
	  DEBUG(MSG_MID, "execute ConnMgr(%d)\n", m_nDataCallPID);
	  
	  if (argument_mask & NET_PPP_AUTO) {
  		sleep(2);
  		for (i=0; i < 20; i++)
  		{
  		  sleep(1);
		    if (!IsServiceOkay()){
		      ret = -1;
		      break;
		    }
  		  
  		  if (IsWANConnected() ) {
  		    ret = 0;
  		    break;
  		  }
  		}
  		
  		if (i == 20) ret = -1;
		}
	}
	else 
	{
	  if (argument_mask & NET_PPP_AUTO) {
#ifdef _WVDIAL_USED_
			execl("/usr/bin/wvdial", "wvdial", NULL);
#else			
      execl("/usr/sbin/pppd", "pppd", "call", "hsdpa", NULL);
#endif
	  } else {
  		execl("/home/pi/quectel-CM/quectel-CM", "quectel-CM", NULL);
		}
	}

  return ret;
}

int DeinitConnMgr(void)
{
  int ret, status = 0; 

  DEBUG(MSG_HIGH, "DeinitConnMgr(%d)\n", m_nDataCallPID);  

  if (m_nDataCallPID > 0)
  {
#if 0  
    if (0 == CheckClient(m_nDataCallPID))
    {
      m_nDataCallPID = 0;
      return -1;
    }
#endif

    kill(m_nDataCallPID, SIGTERM); 

    msleep(200);
    
    // wait for quit of cldvr
    ret = waitpid(m_nDataCallPID, &status, 0); /* WNOHANG : \C0ڽ\C4 \C7\C1\B7μ\BC\BD\BA\B0\A1 \BE\F8\C0\BB \B0\E6\BF\EC \B9ٷ\CE \B8\AE\C5\CF */

    if(ret < 0 || ret != m_nDataCallPID) {
    	DEBUG(MSG_ERROR, "monitor closing failed - waitpid returns (%d), status (%d)\n", ret, status);
    }
    else {
    	DEBUG(MSG_LOW, "monitor closed\n");
    }

    if (WIFEXITED(status))
    {
      DEBUG(MSG_LOW, "OKAY!\n");
    }
    else if (WIFSIGNALED(status))
    {
      DEBUG(MSG_ERROR, "term sig %d!\n", WTERMSIG(status));
    }
    else if (WIFSTOPPED(status))
    {
      DEBUG(MSG_ERROR, "stop sig %d!\n", WSTOPSIG(status));
    }
    else if (WIFCONTINUED(status))
    {
      DEBUG(MSG_ERROR, "NOKAY!\n");
    }

    m_nDataCallPID = 0;
  }

  return ret;
}


void SendMsgQue (int msg_id, int wparm, int lparm)
{
  Msg2Agent msgAgent;
  int ret, msgSize = 0;

  msgAgent.caller= m_nAgentPID; // pid = \BB\FD\BC\BA\C7\D1 process id \C1\DF \C7ϳ\AA.
   msgAgent.msgID  = msg_id;
  msgAgent.wparm = wparm;
  msgAgent.u.lparm = lparm;
  
  msgSize = sizeof(msgAgent) - sizeof(msgAgent.caller);

  ret = msgsnd(m_nAgentQueID, &msgAgent, msgSize, 0);
  
  if (ret == -1) {
    DEBUG(MSG_ERROR,"SendMsgQue(%d) failed!\n", msg_id);
  }
}

void SendCommand(BYTE cmd_id, BOOL at_first, const char * format, ...)
{
  int  cmdLen = 0;
  char cmdBuf[AT_COMMAND_SIZE];

  if (NONE != cmd_id )
  {
    if (format) {
      va_list args;
      va_start( args, format );
      cmdLen += vsprintf(cmdBuf, format, args);
      va_end( args );
      
    }
    DEBUG(MSG_LOW,"SendCommand %d, GetCount %d\r\n", cmd_id, GetCmdCount());
    
    if (at_first){
      CmdPoolPutHead(cmd_id, (WORD)cmdLen, (BYTE *)cmdBuf);
    }else {
      CmdPoolPut(cmd_id, (WORD)cmdLen, (BYTE *)cmdBuf);
    }
    
    if (0 == m_nATResultTimer && 0 == m_nNoResultCount){
      OnPutCommand(NONE);
    }
  }
  else
  {
    if (GetCmdCount() > 0)
    {
      SendMsgQue(WM_USER_AT_CMD, 0, 0);
    }
    else
    {
      m_nATCommandID = NONE;
    }
  }
}

void ParseCodes(char chByte)
{
    static BOOL fCMGR = FALSE;
    static char pResponse[AT_RESPONSE_SIZE];
    static int  nBufferLen = 0;

#ifdef SUPPORT_TCP_CMD
    if (g_nTCPRecvLen)
    {
      pResponse[nBufferLen++] = chByte;
      if (g_nTCPRecvLen == nBufferLen) {
        int fifo = GetFifoIdByPid(g_pidOfTCPRecv);

        if (write(fifo, pResponse, nBufferLen) < 0) {
          DEBUG(MSG_ERROR, "sendto error(%d: %s)\n", errno, strerror(errno));
        } else {
          SetTCPState(g_pidOfTCPRecv, TCP_Recv, nBufferLen);
        }
        
        g_nTCPRecvLen = 0;
      }
      return;
    }
#endif

    /* x0dx0a OK x0dx0a */
    if (0x0d == chByte)
    {
        pResponse[nBufferLen] = '\0';

        if (pResponse[0] == '\0')
        {
            nBufferLen = 0;
            return ;
        }
        else if (pResponse[0] == 'A' && pResponse[1] == 'T')
        {
            DEBUG(MSG_HIGH, "=> %s\r\n", pResponse);
            nBufferLen = 0;
            pResponse[nBufferLen] = '\0';
            if (pResponse[2] != 'E')
              SendCommand(ATE0, TRUE, NULL);
            return ;
        }

        if (0 == ParseFinalRes(pResponse))
        {
            nBufferLen = 0;
            pResponse[nBufferLen] = '\0';

            //CmdPoolDelHead();
            CmdPoolDel(m_nATCommandID);
            m_nATResultTimer = 0;
            m_nNoResultCount = 0;
            SendCommand(NONE, FALSE, NULL);
            return ;
        }

        if (0 == ParseUnsolRes(pResponse))
        {
            nBufferLen = 0;
            pResponse[nBufferLen] = '\0';
            return ;
        }

        if (m_nATCommandID < ATD){
          if (cmd_table[m_nATCommandID].response)
          {
            cmd_table[m_nATCommandID].response(pResponse);
          }
        }
        else
        {
          switch (m_nATCommandID)
          {
            case AT_CMGR :
                /*
                  +CMGR: 0,,26
                  |------------------------------------------------------------------|
                  0791280102194105440BA11020900502F700002160024135856308042202008184C5
                  |------------------------------------------------------------------|
                */

                if ('+' == *pResponse)
                {
                    *(strchr(pResponse, ',')) = 0;
                    g_nRecvMessageIndex = atoi(&pResponse[ 7 ]);

                    fCMGR = TRUE;
                }
                else
                {
                  if (fCMGR)
                  {
                    char strRecvMessage[MAX_MESSAGE_LENGTH+4] = {0, };
                    char strCallerNumber[MAX_NUMBER_LENGTH+4];
                    int  msg_type = 0;
                    int  msg_len = 0;
                  
                    fCMGR = FALSE;
                    msg_len = DecodePDU(pResponse, &msg_type, strCallerNumber, strRecvMessage, NULL);
                    if (msg_len > 0)
                    {
                      DEBUG(MSG_HIGH, "MSG type %d, Len %d, Num Len %d \r\n", msg_type, msg_len, strlen(strCallerNumber));
                      SendSMSRecv(strCallerNumber, msg_type, strRecvMessage, msg_len);
                    }
                  }
                }
                break;

            case AT_CMGS_PDU :
                /*
                +CMGS: 1
                */
                if (!strncmp(pResponse, "+CMGS: ", 7))
                {
                  SetSMSState(g_pidOfSMS, SMS_Sent);
                }
                g_pidOfSMS = 0;
                break;
          }
        }
        nBufferLen = 0;
        pResponse[nBufferLen] = '\0';
    }
    else if ((0x0a != chByte) && (0x0 != chByte))
    {
        pResponse[nBufferLen] = chByte;
        nBufferLen++;

        // AT+CMGS=<length>
        // >
        if (AT_CMGS == m_nATCommandID)
        {
            if (0 == strncmp(strSendPDUNoti, pResponse, 2))
            {
                memset(pResponse, 0, sizeof(pResponse));
                nBufferLen = 0;
            	  //CmdPoolDel(AT_CMGS);
                OnPutCommand(AT_CMGS_PDU);
            }
        }
        else if (AT_QISEND == m_nATCommandID)
        {
            if (0 == strncmp(strSendPDUNoti, pResponse, 2))
            {
                memset(pResponse, 0, sizeof(pResponse));
                nBufferLen = 0;
        		  //CmdPoolDel(AT_QISEND);
                OnPutCommand(AT_QISEND_DATA);
            }
        }

        if (AT_RESPONSE_SIZE == nBufferLen)
            nBufferLen = 0;
    }

    return ;
}


void OnRecvSerial(void)
{
  char chBuffer[AT_RESPONSE_SIZE];

  if (IsPortOpened())
  {
    int i = 0;
    int rdSize = ReadSerial(m_fdSerial, chBuffer, sizeof(chBuffer));

    for (i = 0; i < rdSize; i++)
    {
      ParseCodes(chBuffer[i]);
    }
  }

  return;
}

void OnUserSignal(int sig)
{
  DEBUG(MSG_HIGH, "OnUserSignal sig %d\r\n",sig);
  if (1 == sig){
    if(signal(SIGUSR1, UsrHandler) == SIG_IGN) {
      signal(SIGUSR1, SIG_IGN);
    }
    //ProcDbugMode(1);
  }
  else if (2 == sig)
  {
    if(signal(SIGUSR2, UsrHandler) == SIG_IGN) {
      signal(SIGUSR2, SIG_IGN);
    }
    //ProcDbugMode(0);
  }
}

void OnTimer(void)
{
  DWORD nTick;
  static DWORD nLastTick = 0;
  
  mainSysTick++;

  if (0 == mainSysTick%8)
  {
    KICK_WATCHDOG;
  }
  
  nTick = GetTickCount();
  if (nTick - nLastTick < 600)
    return;
  nLastTick = nTick;

  if (IsPortOpened())
  {
    if (0 != g_nRecoveryCountDown){
      if (0 == --g_nRecoveryCountDown){
        ResetModem(RESET_NO_ATRESULT);
        goto doExit;
      }
    }
  
    if (0 != m_nATResultTimer)
    {
      if (0 == --m_nATResultTimer)
      {
        if (3 == ++m_nNoResultCount)
        {
          DEBUG(MSG_ERROR,"[E] No Response for 3 tryial!\r\n");
          ResetModem(RESET_NO_ATRESULT);
        }
        else
        {
          DEBUG(MSG_ERROR,"[E] Retry atCommand [%d]\r\n", m_nATCommandID);
          OnPutCommand(NONE); // head
        }
      }
    }
    else
    {
#if 0 //def SUPPORT_VOICE_CALL
      if (ATD_Ringing == GetATDState())
      {
        SendCommand(AT_CLCC, FALSE, NULL);
      }
#endif
      if (0 == --m_nStatusTimer)
      {
        if (SIM_READY == g_eSIMStatus)
        {
          SendCommand(AT_QCDS, FALSE, NULL);
        }
        if (REG_REGISTERED == g_eRegistration) 
        {
          if (0 == g_nTimeUpdated)
          {
            SendCommand(AT_CCLK, FALSE, NULL);
            SendCommand(AT_COPS,   FALSE, NULL);
						SendCommand(AT_CGPADDR,	 FALSE, NULL);
          }
        }
        m_nStatusTimer   = TIMER_MODEM_STATUS;
      }
    }
  }

doExit:
  return;
}

void OnPutCommand(BYTE direct_cmd)
{
  int xmitLen = 0;
  int resWaitSec = TIMER_AT_COMMAND_RESULT_WAIT;
  char xmitCommand[AT_COMMAND_SIZE+128];
  CMD_Tlv *ptlv = NULL;

  if (NONE != direct_cmd)
  {
    if (direct_cmd  < 0x80)
    {
      m_nATCommandID = direct_cmd;
      strcpy(xmitCommand, cmd_table[direct_cmd].command);
      xmitLen = strlen(cmd_table[direct_cmd].command);
    }
    else
    {
      CmdPoolGet(direct_cmd, &ptlv);
      if (ptlv) {
        xmitLen = ptlv->length;
        memcpy(xmitCommand, ptlv->value, xmitLen);
        m_nATCommandID = ptlv->tag;
      }
    }
  }
  else
  {
    CmdPoolGetHead(&ptlv);
    if (!ptlv) {
      m_nATCommandID = NONE;
      DEBUG(MSG_ERROR,"ERR: PutCommand NONE\r\n");
      return ;
    }else {
      m_nATCommandID = ptlv->tag;
    }
    
    if (m_nATCommandID  < 0x80)
    {
      strcpy(xmitCommand, cmd_table[m_nATCommandID].command);
      xmitLen = strlen(cmd_table[m_nATCommandID].command);
    }
    else
    {
      xmitLen = ptlv->length;
      memcpy(xmitCommand, ptlv->value, xmitLen);
    }
  }

  DEBUG(MSG_HIGH, "PutCommand %d,%d,%d\r\n", m_nATCommandID, mainSysTick, m_nATResultTimer);
  
  if (AT_CMGS_PDU == direct_cmd || AT_QISEND_DATA == direct_cmd)
    resWaitSec = TIMER_AT_QICLOSE_RESULT_WAIT;

  if (xmitLen > 0)
  {
    //DEBUG(MSG_ERROR,"%s\n", xmitCommand);
    if (IsPortOpened()) {
      WriteSerial(m_fdSerial, (BYTE*)xmitCommand, xmitLen);
    }
    m_nATResultTimer = resWaitSec;
  }
  
  return ;
}


DWORD GetTickCount()
{
#if 0
	struct timespec tspec;

	clock_gettime(CLOCK_MONOTONIC, &tspec);

	return (DWORD)((tspec.tv_sec * 1000) + (tspec.tv_nsec / 1000000));
#else
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (DWORD)(tv.tv_sec * 1000 + tv.tv_usec / 1000);
#endif	
}	

void OnReadMsgQue(int pid, int wparm, void *pMsgQ)
{
  
  switch (wparm)
  {
       case SYS_CONTROL:
      
          ProcSysControl(pid, *((BYTE*)pMsgQ));
          break;

       case DATA_CONNECT:
      
          DUNConnect(pid);
          break;
          
      case DATA_DISCONNECT:

          DUNDisconnect(pid);
          break;

      case SMS_SEND :
      
          OnSendSMS(pid, (SmsMsgT*)pMsgQ);
          break;

      case PROC_ATTACH :
          AddProcess(pid, *((BYTE*)pMsgQ));
          break;
      
      case PROC_DETACH :
          RemoveProcess(pid);
          break;
      
#ifdef SUPPORT_TCP_CMD
       case TCP_OPEN:  
          OnTCPOpen(pid, (TcpServerT*)pMsgQ);
          break;
       
       case TCP_SEND:
          OnTCPSend(pid, *((int*)pMsgQ));
          break;
       
       case TCP_CLOSE:
          OnTCPClose(pid);
          break;
#endif 

			case BAND_SET:
					OnBandSet((BandInfoT*)pMsgQ);
					break;
			
			case SCAN_SET:
					OnScanSet(*((BYTE*)pMsgQ));
					break;
			
			case APN_SET:
					OnApnSet((APNInfoT*)pMsgQ);
					break;
					
 #ifdef SUPPORT_VOICE_CALL
      case CALL_DIAL :
      
          OnDial(pid, (char *)pMsgQ);
          break;
      
      case CALL_ANSWER :
      
          OnAnswer(pid);
          break;
      
      case CALL_HANGUP:
      
          OnHangUp(pid);
          break;

      case CALL_VOLUME:
      
          OnSetVolume(*((BYTE*)pMsgQ));
          break;

      case CALL_MIC:
      
          OnSetMicGain(*((BYTE*)pMsgQ));
          break;

      case CALL_DTMF:
      
          OnSendDTMF(*((char*)pMsgQ));
          break;
#endif
          
  }
  
}

void OnSendDTMF(char cDtmf)
{
  
  SendCommand(AT_VTS, FALSE, "AT+VTS=%c\r", cDtmf);
}

void OnDial(int pid, char *pCallingNumber)
{
  if (REG_REGISTERED != g_eRegistration || 0 != g_pidOfATD) 
  {
    SetATDState(pid, ATD_Error);
    return ;
  }


  g_pidOfATD = pid;
  SendCommand(ATD, FALSE, "ATD%s;\r", pCallingNumber);
}

void OnAnswer(int pid)
{
  if (GetATDState() == ATD_Ringing && 0 == g_pidOfATD) {
    g_pidOfATD = pid;
    SendCommand(ATA, FALSE, NULL);
  }
}

void OnHangUp(int pid)
{
  if (GetATDState() != ATD_Idle) {
    g_pidOfATD = pid;
    SendCommand(AT_CHUP, FALSE, NULL);
  }
}

void OnSetVolume(BYTE vol)
{
  SendCommand(AT_CLVL_N, FALSE, "AT+CLVL=%d\r", vol);
}

void OnSetMicGain(BYTE vol)
{
  // sprintf(xmitCommand, "AT+QIIC=0,0x1B,0xD,2,0x%d40\r", g_nModemMicGain);
  SendCommand(AT_CMVL_N, FALSE, "AT+CMVL=%d\r", vol);
}

void OnSendSMS(int pid, SmsMsgT *sms)
{
  int pdu_length;
  char pdu_data[MAX_MESSAGE_LENGTH * 4]; 
  
  if (REG_REGISTERED != g_eRegistration || 0 != g_pidOfSMS)
  {
    SetSMSState(pid, SMS_Error);
    return ;
  }

  DEBUG(MSG_ERROR,"OnSendSMS pid %d, NUM: %s MSG: %s\r\n", pid, sms->strNumber, sms->strMessage); 

  pdu_length = EncodePDU(sms->strMessage, sms->nType, sms->strNumber, GetPhoneNumber(), pdu_data);
  if (0 == pdu_length) {
    SetSMSState(pid, SMS_Error);
    return ;
  }

	DEBUG(MSG_ERROR,"OnSendSMS PDU(%d): %s\r\n", pdu_length, pdu_data); 
  
  SendCommand(AT_CMGS, FALSE, "AT+CMGS=%d\r", pdu_length);
  SendCommand(AT_CMGS_PDU, FALSE, "%s%c", pdu_data, chSendPDUEnd);
  
  g_pidOfSMS = pid;
  SetSMSState(pid, SMS_Sending);
}


void OnApnSet(APNInfoT *p)
{

	strcpy(APN_NAME, p->apn);

	SendCommand(AT_CGDCONT_1, FALSE, "AT+CGDCONT=1,\"IPV4V6\",\"%s\"\r", APN_NAME);

}

void OnScanSet(BYTE mode)
{
	int cmd = AT_QCFG_NW_AUTO;
	
	if (mode == NET_SCAN_WCDMA)
		cmd = AT_QCFG_NW_WCDMA;
	else if (mode == NET_SCAN_LTE)
		cmd = AT_QCFG_NW_LTE;

	SendCommand(cmd, FALSE, NULL);
}

void OnBandSet(BandInfoT *p)
{

	SendCommand(AT_QCFG_BAND_X, FALSE, "AT+QCFG=\"band\",10,%x\r", p->nFDDLTE);
}


#ifdef SUPPORT_TCP_CMD  
int GetPidBySockId(int nSock)
{
  if (nSock >= sizeof(g_tcpArray)/sizeof(g_tcpArray[0])) {
    return 0;
  }
  return g_tcpArray[nSock].pid;
}

int GetSockIdByPid(int pid)
{
  int i;
  for (i=0 ;i < sizeof(g_tcpArray)/sizeof(g_tcpArray[0]) ; i++)
  {
    if (pid == g_tcpArray[i].pid)
      return i;
  }

  return -1;
}

int GetFifoIdByPid(int pid)
{
  int i;
  for (i=0 ;i < sizeof(g_tcpArray)/sizeof(g_tcpArray[0]) ; i++)
  {
    if (pid == g_tcpArray[i].pid)
      return g_tcpArray[i].fifo;
  }

  return -1;
}

// return sock_id
int BuildSockInfo(int pid)
{
  int i;
  
  for (i=0 ;i < sizeof(g_tcpArray)/sizeof(g_tcpArray[0]) ; i++)
  {
    if (0 == g_tcpArray[i].pid)
    {
      char fifo_name[64];
      sprintf(fifo_name, "/tmp/to_client_%d", pid);
      
      g_tcpArray[i].pid = pid;
      g_tcpArray[i].fifo =  open(fifo_name, O_WRONLY);
      if(g_tcpArray[i].fifo < 0)
      {
        g_tcpArray[i].pid = 0;
        DEBUG(MSG_ERROR, "RILD: TO_CLIENT: open error(%d) %s\n", errno, strerror(errno)); 
        return -1;
      }
      return i;
    }
  }
  
  return -1;
}

// return sock_id
int ClearSockInfo(int pid)
{
  int i;
  
  for (i=0 ;i < sizeof(g_tcpArray)/sizeof(g_tcpArray[0]) ; i++)
  {
    if (pid == g_tcpArray[i].pid)
    {
      g_tcpArray[i].pid = 0;
      close(g_tcpArray[i].fifo);
      return i;
    }
  }
  
  return -1;
}

void ClearAllSockInfo(void)
{
  int i;
  
  for (i=0 ;i < sizeof(g_tcpArray)/sizeof(g_tcpArray[0]) ; i++)
  {
    if (0 != g_tcpArray[i].pid)
    {
      g_tcpArray[i].pid = 0;
      close(g_tcpArray[i].fifo);
    }
  }
}

void OnTCPSend(int pid, int length)
{
  TcpDataT msg;
  int msgSize = sizeof(TcpDataT) - sizeof(msg.pid); 
  
  DEBUG(MSG_ERROR, "OnTCPSend: length(%d) \n", length);
   
  msgSize = msgrcv(m_nTCPIPQueID, &msg, msgSize, pid, IPC_NOWAIT);
  if (msgSize < 0 || length != msg.length)
  {
    DEBUG(MSG_ERROR, "OnTCPSend: no data or msgrcv error(%d) %s\n", errno, strerror(errno));
    return;
  }

  int socket_id = GetSockIdByPid(pid);

  SendCommand(AT_QISEND, FALSE, "AT+QISEND=%u,%u\r", socket_id, length);
  CmdPoolPut(AT_QISEND_DATA, (WORD)length, msg.data);
}

void OnTCPOpen(int pid, TcpServerT *srv_ptr)
{
  TcpDataT msg;

  int msgSize = sizeof(TcpDataT) - sizeof(msg.pid); 

  // buffer clear before tcp connect
  do{
    msgSize = msgrcv(m_nTCPIPQueID, &msg, msgSize, pid, IPC_NOWAIT);

    DEBUG(MSG_ERROR, "OnTCPOpen: msgSize(%d) msgrcv error(%d) %s\n", msgSize, errno, strerror(errno));
    
  } while(msgSize > 0);

  int socket_id = BuildSockInfo(pid);
  if (socket_id < 0) {
    SetTCPState(pid, TCP_Error, ERR_TCPIP_NO_RESOURCE);
    return;
  }

		
  SendCommand(AT_QIOPEN_1, FALSE, "AT+QIOPEN=1,%u,\"TCP\",\"%s\",%u,0,1\r", socket_id, 
                                        srv_ptr->addr, srv_ptr->port);
}

void OnTCPClose(int pid)
{
  TcpDataT msg;
  int msgSize = sizeof(TcpDataT) - sizeof(msg.pid); 

  // buffer clear before tcp connect
  do{
    msgSize = msgrcv(m_nTCPIPQueID, &msg, msgSize, pid, IPC_NOWAIT);
  } while(msgSize > 0);

  int  socket_id = GetSockIdByPid(pid);
  
  SendCommand(AT_QICLOSE, FALSE, "AT+QICLOSE=%u,1\r", socket_id);
}

void TCPClosed(int pid, int err)
{
  if (pid == 0)
  {
    int i;
    
    for (i=0 ;i < sizeof(g_tcpArray)/sizeof(g_tcpArray[0]) ; i++)
    {
      if (g_tcpArray[i].pid != 0)
      {
        SetTCPState(g_tcpArray[i].pid, TCP_Error, err);
        OnTCPClose(pid);
        ClearSockInfo(pid);
      }
    }
  }
  else
  {
    if (err > 0)
      SetTCPState(pid, TCP_Error, err);
    else
      SetTCPState(pid, TCP_Idle, 0);

    if (err != -1) OnTCPClose(pid);
    ClearSockInfo(pid);
  }

  g_nTCPRecvLen = 0;
}
#endif

void ResetModem(int nCause)
{
  if (nCause)
  {
    //SendAlertMsg(0, ERR_MODEM_RECOVERY);
    CloseATPort();
  }
  
  if (SMS_Idle != GetSMSState())
  {
    SetSMSState(0, SMS_Error);
  }
  g_pidOfSMS       = 0;
#ifdef SUPPORT_GPS_CMD
  if (GPS_Idle != GetGPSState())
  {
    SetGPSState(0, ATD_Error);
  }
#endif
#ifdef SUPPORT_VOICE_CALL
  if (ATD_Idle != GetATDState()){
    SetATDState(g_pidOfATD, ATD_Error);
  }
  g_pidOfATD       = 0;
  m_nCidLists      = 0;
#endif
#ifdef SUPPORT_TCP_CMD
  g_nTCPRecvLen  = 0;
  g_pidOfTCPRecv = 0;
  ClearAllSockInfo();
#endif
  m_nNoResultCount = 0;
  m_nATResultTimer = 0;
  m_nStatusTimer   = TIMER_MODEM_STATUS;
  m_nATCommandID   = NONE;
  g_nRSSI          = RSSI_INIT_VALUE;
  g_nRSRP          = RSSI_INIT_VALUE;
  g_eRegistration  = REG_UNKNOWN;
  g_eSIMStatus     = SIM_NONE;
  g_nSimNotInsertedCnt = 0;
  g_nRecoveryCountDown = 0;
  CmdPoolClear();
  ResetSharedData();
  
  if (nCause) 
  {
    Deinit1SecTick();
    ModemReset();
    OpenATPort(CMD_PORT);
    Init1SecTick();
  }

  InitModem(SIM_NOT_INSERTED);
}

void ProcSysControl(int pid, BYTE ctrl)
{

	if (ctrl == MODEM_PWROFF)
	{
		SendCommand(AT_SHDN, FALSE, NULL);
	}
	else if (ctrl == MODEM_RESET)
	{
		ResetModem(RESET_FROM_CLIENT);
	}
	else if (ctrl == MODEM_REBOOT)
	{
		SendCommand(AT_CFUN_1_1, FALSE, NULL);
	}
}

void OnNetStatus(void)
{
  DEBUG(MSG_ERROR,"OnNetStatus!\n");
  if (argument_mask & NET_ETH_AUTO) {
    pthread_mutex_lock(&s_connectmutex);
    pthread_cond_signal(&s_connectcond);
    pthread_mutex_unlock(&s_connectmutex);
  }
}

void OnRasResult(int eStat, int sStat)
{
  DEBUG(MSG_ERROR,"OnRasResult! %d\n", eStat);
  
  SetRASState(0, eStat);

	if ((RAS_Error == eStat) || (RAS_Idle == eStat))
	{
	  //RASDisconnect();
	  /*SetDataLED(_LOW);  no more used */
	  if (RAS_Error == eStat)
	  {
	    if (0 == (AT_INTF_UART & argument_mask))
	    {
  	    if (!IsTTYAvailable(CMD_PORT)){
  	      ResetModem(RESET_NO_CONNMGR);
  	    }
	    }
	  }
	  if (0 == (argument_mask & NET_INF_TASK)){
  	  if (m_hWanThread) {
    		pthread_join(m_hWanThread, NULL);
    		m_hWanThread = 0;
      }
    }
	}
	else if (RAS_Connected == eStat)
	{
	  /*SetDataLED(_HIGH); no more used */
	}
}

void OnSerialError(int err, int sub_err)
{
  if (err == -1)
  {
    ResetModem(RESET_NO_ATRESULT);
  }
}

void DUNConnect(int pid)
{
	int ret;

	if (argument_mask & NET_INF_TASK){
	  return;
	}

  if (REG_REGISTERED != g_eRegistration)
  {
    SetRASState(0, RAS_Error);
    return;
  }

  // usb check;
  if (access(DUN_PORT, F_OK) != 0)
  {
    SetRASState(0, RAS_Error);

    DEBUG(MSG_ERROR,"USB not found!\n");
    return;
  }
	
	if (!m_hWanThread)
	{
		ret = pthread_create(&m_hWanThread, NULL, (void *)DunThread, NULL);
		if(ret) {
		  SetRASState(0, RAS_Error);
			DEBUG(MSG_ERROR,"DunThread create failed!\n");
			return;
		}
	}
}

void DUNDisconnect(int pid)
{
	if (argument_mask & NET_INF_TASK){
	  return;
	}
#if 0
  if (0 != GetRASCount())
  {
    SetRASState(pid, RAS_Idle);
    return;
  }
#endif
#ifdef _PPPD_CHILD_
  if (m_nDataCallPID > 0){
    SetRASState(0, RAS_Disconnecting);
    kill(m_nDataCallPID, SIGTERM); 
  }

  // RAS_Idle will be sent in RasThread!
#else
  int ret;

  SetRASState(0, RAS_Disconnecting);
#ifdef _WVDIAL_USED_
	ret = system("ps -ef | grep wvdial | awk '{print $2}' | xargs kill -15");
#else
	ret = system("ps -ef | grep pppd | awk '{print $2}' | xargs kill -15");
#endif
	if(WEXITSTATUS(ret) != 0) {
		DEBUG(MSG_ERROR,"cannot execute kill -15 pppd\n");
	}else{
		DEBUG(MSG_HIGH, "success kill -15 pppd\n");
  }
	if(m_hWanThread) {
		pthread_join(m_hWanThread, NULL);
		m_hWanThread = 0;
	}

	SetRASState(0, RAS_Idle);
#endif
}

void *DunThread(void *lpParam)
{
	int ret, status = 0;

#ifdef _PPPD_CHILD_
	m_nDataCallPID = fork();
	if(m_nDataCallPID < 0) {
		DEBUG(MSG_ERROR, "fork failed!!\n");
		SendMsgQue(WM_USER_RAS_RES, RAS_Error, 0);
		return NULL;
	}
	else if(m_nDataCallPID > 0) 
	{
		DEBUG(MSG_ERROR, "run pppd\n");
		SendMsgQue(WM_USER_RAS_RES, RAS_Connecting, status);
		sleep(3);
		for (ret=0; ret < 20; ret++)
		{
		  sleep(1);
	    if (!IsServiceOkay()){
	      ret = 20;
	      break;
	    }
		  
		  if (IsWANConnected()) {
		    ret = 0;
		    break;
		  }
		}

		if (ret < 20){
  		SendMsgQue(WM_USER_RAS_RES, RAS_Connected, status);

    	ret = waitpid(m_nDataCallPID, &status, 0); 
    	m_nDataCallPID = 0;
      SendMsgQue(WM_USER_RAS_RES, RAS_Idle, 0);
  	}else{
  	  kill(m_nDataCallPID, SIGTERM); 
  	  m_nDataCallPID = 0;
  	  SendMsgQue(WM_USER_RAS_RES, RAS_Error, 0);
  	}
	}
	else 
	{
	
#ifdef _WVDIAL_USED_
		execl("/usr/bin/wvdial", "wvdial", NULL);
#else			
		execl("/usr/sbin/pppd", "pppd", "call", "hsdpa", NULL);
#endif
	}

#else
#ifdef _WVDIAL_USED_
	ret = system("wvdial");
#else
	ret = system("pppd call hsdpa");
#endif
	if(WEXITSTATUS(ret) != 0) {
	  DEBUG(MSG_ERROR,"cannot execute pppd!!\n");
    ret = RAS_Error;
	}else{
	  ret = RAS_Connected;
	}
	SendMsgQue(WM_USER_RAS_RES, ret, 0);
#endif

	DEBUG(MSG_ERROR, "DunThread exit %d\n", ret);
	pthread_exit(NULL);

	return NULL;
}

void InitDataIntf(void)
{
	if (!m_hWanThread)
	{
	  int ret;
	
    if (argument_mask & NET_PPP_AUTO)	  
		  ret = pthread_create(&m_hWanThread, NULL, (void *)ConnThread, NULL);
    else if (argument_mask & NET_ETH_AUTO)
		  ret = pthread_create(&m_hWanThread, NULL, (void *)ConnThread, NULL);
    else
      return;
      
		if(ret) {
		  SetRASState(0, RAS_Error);
			DEBUG(MSG_ERROR,"RAS: pthread_create failed!\n");
		} else {
		  pthread_detach(m_hWanThread);
		  SetRASState(0, RAS_Connecting);
		}
	}
}

void DeinitDataIntf(void)
{
  int ret;

  SetRASState(0, RAS_Disconnecting);

  if (argument_mask & NET_PPP_AUTO)
  {
#ifdef _WVDIAL_USED_
		ret = system("ps -ef | grep wvdial | awk '{print $2}' | xargs kill -15");
#else
  	ret = system("ps -ef | grep pppd | awk '{print $2}' | xargs kill -15");
#endif
  	if(WEXITSTATUS(ret) != 0) {
  		DEBUG(MSG_ERROR,"cannot execute kill -15 pppd\n");
  	}else{
  		DEBUG(MSG_HIGH, "success kill -15 pppd\n");
    }

	}
	else if (argument_mask & NET_ETH_AUTO)
	{
	  ret = system("ps -ef | grep quectel-CM | awk '{print $2}' | xargs kill -15");
		
		if(WEXITSTATUS(ret) != 0) {
			DEBUG(MSG_ERROR,"cannot execute kill -15 quectel-CM\n");
		}else{
			DEBUG(MSG_HIGH, "success kill -15 quectel-CM\n");
		}
	}

	if(m_hWanThread) {
		pthread_cancel(m_hWanThread);
		m_hWanThread = 0;
	}

	SetRASState(0, RAS_Idle);

}

void *ConnThread(void *lpParam)
{
  if (argument_mask & NET_PPP_AUTO)
    DunMainLoop();
  else if (argument_mask & NET_ETH_AUTO)
    EthMainLoop();

  return NULL;
}

void DunMainLoop(void)
{
  enum {RAS_OK, PORT_NO, SRV_NO };
	int ret, status = RAS_OK;

  while(1)
  {
    if (!IsTTYAvailable(DUN_PORT))
    {
      DEBUG(MSG_ERROR,"[RAS]USB not found!\n");
      sleep(3);
    }
    else if (!IsServiceOkay())
    {
      DEBUG(MSG_ERROR,"[RAS]REG not done!\n");
      sleep(3);
    }
    else
    {
      if (IsRegistered() && !IsPPPLinkUp())
      {
#ifdef _PPPD_CHILD_
        ret = InitConnMgr();

        if (ret != 0) {
          SendMsgQue(WM_USER_RAS_RES, RAS_Error, 0);

          (void) DeinitConnMgr();
        } else {
          SendMsgQue(WM_USER_RAS_RES, RAS_Connected, 0);
          
        	ret = waitpid(m_nDataCallPID, &status, 0); /* WNOHANG : \C0ڽ\C4 \C7\C1\B7μ\BC\BD\BA\B0\A1 \BE\F8\C0\BB \B0\E6\BF\EC \B9ٷ\CE \B8\AE\C5\CF */
        	
        	m_nDataCallPID = 0;
          SendMsgQue(WM_USER_RAS_RES, RAS_Idle, 0);

          sleep(5);
        }
#else 
#ifdef _WVDIAL_USED_
				ret = system("wvdial");
#else

      	ret = system("pppd call hsdpa");
#endif
      	if(WEXITSTATUS(ret) != 0) {
      	  DEBUG(MSG_ERROR,"cannot execute pppd!!\n");
          ret = RAS_Error;
      	}else{
      	  ret = RAS_Connected;
      	}
      	SendMsgQue(WM_USER_RAS_RES, ret, 0);
#endif      	
      }
      else
      {
        sleep(5);
      }
      status = RAS_OK;
    }
  }
}

void EthMainLoop(void)
{
  while(1)
  {
    if (!IsNETAvailable())
    {
      DEBUG(MSG_ERROR,"[ETH]USB not found!\n");
      sleep(3);
    }
    else if (!IsServiceOkay())
    {
      DEBUG(MSG_ERROR,"[ETH]REG not done!\n");
      sleep(3);
    }
    else
    {
      if (IsRegistered() && (0 == m_nDataCallPID))
      {
      	int ret = InitConnMgr();

      	if(ret != 0) {
          SendMsgQue(WM_USER_RAS_RES, RAS_Error, 0);
      	}else{
      	  SendMsgQue(WM_USER_RAS_RES, RAS_Connected, 0);

          pthread_mutex_lock(&s_connectmutex);
          ret = pthread_cond_wait(&s_connectcond, &s_connectmutex);
          pthread_mutex_unlock(&s_connectmutex);

          SendMsgQue(WM_USER_RAS_RES, RAS_Error, 0);
      	}

        m_nDataCallPID = 0;
      }
      
      sleep(1);
    }
  }
}


void Init1SecTick(void)
{
#ifdef _THREAD_TICK_
  // pthread_t thread;
  struct itimerval itimer;
  sigset_t sigset;
  int sig_no=0;
  int rc;

  signal(SIGALRM, SIG_IGN);

  sigemptyset(&sigset);
  sigaddset(&sigset, SIGALRM);
  sigprocmask(SIG_BLOCK, &sigset, NULL);

  pthread_sigmask(SIG_BLOCK, &sigset, NULL);

  rc = pthread_create(&m_hTmrThread,
                        NULL,
                        (void*)TickThread, 
                        NULL);

  if (rc)
    DEBUG(MSG_ERROR,"TickThread : %s\n", strerror(errno));
  else
    (void) pthread_detach(thread);

 #else
  struct sigaction si, so;
  struct itimerval ival, oval;
        
	memset (&si, 0, sizeof (si));
	memset (&so, 0, sizeof (so));
	si.sa_handler = &TmrHandler;
  sigaction (SIGALRM, &si, &so);
  
	memset (&ival, 0, sizeof (ival));
	memset (&oval, 0, sizeof (oval));
  /* Configure the timer to expire after 2 sec... */
  ival.it_value.tv_sec  = 2;
  ival.it_value.tv_usec = 0;

  /* ... and every 1 sec after that. */
  ival.it_interval.tv_sec = 1;
  ival.it_interval.tv_usec = 0;

	setitimer (ITIMER_REAL, &ival, &oval);
 #endif
}

void Deinit1SecTick(void)
{
  signal (SIGALRM, SIG_IGN);
}

#ifdef _THREAD_TICK_
void* TickThread(void* parms)
{
  struct itimerval itimer;
  sigset_t sigset;
  int sig_no = 0;
  struct timeval now;
  long time_gap_sec = 0;
  long time_gap_usec = 0;

  DEBUG(MSG_LOW, "%s() start!!\n", __func__);

  sigemptyset(&sigset);
  sigaddset(&sigset, SIGALRM);
  sigprocmask(SIG_BLOCK, &sigset, NULL);

  pthread_sigmask(SIG_UNBLOCK, &sigset, NULL);

  itimer.it_value.tv_sec     = 5;
  itimer.it_value.tv_usec    = 0;
  itimer.it_interval.tv_sec  = 1;
  itimer.it_interval.tv_usec = 0;
  
  if (setitimer(ITIMER_REAL, &itimer, NULL) != 0)
  {
    DEBUG(MSG_ERROR,"Could not start interval timer : %s", strerror(errno));
    return NULL;
  }

  while (1)
  {
    DEBUG(MSG_LOW, "sigwait ~~~~~~~~~~~~~~\n");
    if (sigwait(&sigset, &sig_no) != 0)
    {
      DEBUG(MSG_ERROR,"Failed to wait for next clock tick\n");
    }

    switch (sig_no)
    {
      case SIGALRM:
        gettimeofday(&now, NULL);
        //DEBUG(MSG_LOW, "now = %ld.%06ld, gap=%1d.%06ld\n",
        //        now.tv_sec, now.tv_usec,
        //        now.tv_sec - time_gap_sec,
        //        now.tv_sec - time_gap_usec);
        SendMsgQue(WM_USER_TIMER, 0, 0);
        break;

      default:
        DEBUG(MSG_ERROR,"unexpected signal\n");
    }
  }

  pthread_exit(NULL);

  return NULL;
}
#endif


void *SerRxThread(void *lpParam)
{
	int serfd, maxfd = 0;
	fd_set readfd, errfd;

  DEBUG(MSG_LOW, "SerRxThread enter\n");
  
  while (1)
  {

    if (m_fdSerial > 0)
    {
      serfd = m_fdSerial;
      
    	FD_ZERO(&readfd);
    	FD_SET(serfd, &readfd);
    	FD_ZERO(&errfd);
    	FD_SET(serfd, &errfd);
    	
    	maxfd = serfd + 1;

    	select(maxfd, &readfd, 0, &errfd, NULL);     // block until the data arrives

      //DEBUG(MSG_LOW, "select() ret\n");

      if (FD_ISSET(serfd, &readfd) == TRUE)
      {
        msleep(100);
        SendMsgQue(WM_USER_RX_CHAR, 0, 0);
        msleep(100);
      }
      if (FD_ISSET(serfd, &errfd) == TRUE)
      {
        DEBUG(MSG_ERROR, "SerRxThread error!\n");
        if (AT_INTF_USB & argument_mask)
        {
          if (!IsTTYAvailable(CMD_PORT)) {
            SendMsgQue(WM_USER_RX_ERROR, -1, 0);
          }
        }
        msleep(100);
      }
    }
    else
    {
      sleep(1);
    }
  }

  DEBUG(MSG_LOW, "SerRxThread exit\n");

  return NULL;
}



BOOL IsPortOpened(void)
{
  if (m_fdSerial <= 0){
    return FALSE;
  }

  return TRUE;    
}


BOOL IsServiceOkay(void)
{
  if ((SIM_READY == g_eSIMStatus) && 
      (REG_REGISTERED  == g_eRegistration) && 
      (g_nRSRP > -122))
  {
    return TRUE;
  }

	DEBUG(MSG_LOW, "IsServiceOkay reg:%d sim:%d rsrp:%d\n", g_eRegistration, g_eSIMStatus, g_nRSRP );		
  return FALSE;
}

BOOL IsRegistered(void)
{
  if (REG_REGISTERED == g_eRegistration) {
      return TRUE;
  }

  return FALSE;
}


BYTE GetCurrCommand(void)
{
  return m_nATCommandID;
}

void AddProcess(int pid, BYTE mask)
{
  int i;
  
  DEBUG(MSG_LOW, "AddProcess pid %d, mask x%x, cnt %d\n", pid, mask, parent_proc.count);
  if (parent_proc.count == 0)
  {
    parent_proc.root = malloc(sizeof(proc_node_t));
    
    parent_proc.root->pid = pid;
    parent_proc.root->event_mask = mask;
    parent_proc.root->next = NULL;
  }
  else
  {
    proc_node_t *temp_node = parent_proc.root; 
    for (i = 1; i < parent_proc.count; i++){
      temp_node = temp_node->next;
    }
    temp_node->next = malloc(sizeof(proc_node_t));
    
    temp_node->next->pid = pid;
    temp_node->next->event_mask = mask;
    temp_node->next->next = NULL;
  }

  parent_proc.count++;
}

void RemoveProcess(int pid)
{
  int i = 0;
  proc_node_t *node = parent_proc.root; 
  proc_node_t *pre_node;

  DEBUG(MSG_LOW, "RemoveProcess pid %d, cnt %d\n", pid, parent_proc.count);
  for (i = 0 ; i < parent_proc.count; i++)
  {
    if (node->pid == pid){
      break;
    }
    pre_node = node;
    node = node->next;
  }

  if (i == parent_proc.count) return;

  if (i == 0) {
    parent_proc.root = node->next;
  }
  else
    pre_node->next = node->next;


  if (node) free(node);
  parent_proc.count--;
}


void RemoveAllProc(int pid)
{
  int i;
  proc_node_t *node = parent_proc.root; 
  proc_node_t *next_node;

  for (i = 0 ; i < parent_proc.count; i++)
  {
    next_node = node->next;
    free(node);
    node = next_node;
  }
  parent_proc.count = 0;
  parent_proc.root = NULL;
}

