/****************************************************************************
  oemlayer.c
  Author : kpkang@gmail.com
*****************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "agent.h"
#include "m2mdbg.h"
#include "utils.h"
#include "clientIF.h"

#include "oemlayer.h"

#define RTC_DEVICE	    "/dev/rtc0"

#define MODEM_SLEEP   write (m_fdGpio/*fd*/, "Q1", 2)
#define MODEM_WAKEUP  write (m_fdGpio/*fd*/, "Q0", 2)

#define AP_URC_READY  write (m_fdGpio/*fd*/, "S1", 2)
#define AP_NOT_READY  write (m_fdGpio/*fd*/, "S0", 2)

void KickWatchDog()
{
}



void VBusOnOff(int vbusOn)
{
#if 0
	int fd = open ("/dev/piodrv", O_RDWR);

	if(fd < 0)
	{
		DEBUG(MSG_ERROR, "VBusOnOff fail !! -- [%d]\n", fd);
		exit(1);
	}

  if (vbusOn)
  {
  	write (m_fdGpio/*fd*/, "P1", 2);
	}
	else
	{
	  write (m_fdGpio/*fd*/, "P0", 2);
	}

	close(fd);
  
#endif
}


void ModemOn(int vbusOn)
{
#if 0
	int fd = open ("/dev/piodrv", O_RDWR);
	
	if(fd < 0)
	{
		DEBUG(MSG_ERROR, "ModemOn fail !! -- [%d]\n", fd);
		exit(1);
	}
  
  write (m_fdGpio/*fd*/, "T1", 2);
	msleep(1000);

  write (m_fdGpio/*fd*/, "R1", 2);
  msleep(200);
  write (m_fdGpio/*fd*/, "R0", 2);

  if (vbusOn)
  {
  	write (m_fdGpio/*fd*/, "P1", 2);
	}

	close(fd);
#endif
}


void ModemOff(int doDereg)
{
#if 0
	int fd = open ("/dev/piodrv", O_RDWR);
  
	if(fd < 0)
	{
		DEBUG(MSG_ERROR, "ModemOn fail !! -- [%d]\n", fd);
		exit(1);
	}
	write (m_fdGpio/*fd*/, "P0", 2);

  if (doDereg)
  {
    write (m_fdGpio/*fd*/, "R1", 2);
    msleep(1200);
  	write (m_fdGpio/*fd*/, "R0", 2);
    msleep(3000);
  }

  write (m_fdGpio/*fd*/, "T0", 2);

  close(fd);
#endif
}

void ModemReset(void)
{
  KICK_WATCHDOG;
  ModemOff(0); 
  msleep(2800);
  if (argument_mask & AT_INTF_UART)
    ModemOn(0);
  else
    ModemOn(1);
  msleep(3000);
}


void InitWatchDog()
{
}


void DeinitWatchDog()
{
}

