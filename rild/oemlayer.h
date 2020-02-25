/****************************************************************************
  oemlayer.h
  Author : kpkang@gmail.com
*****************************************************************************/
#ifndef __OEMLAYER_H
#define __OEMLAYER_H

#include "common.h"

#define KICK_WATCHDOG    KickWatchDog()

void VBusOnOff(int vbusOn);
void ModemOn(int vbusOn);
void ModemOff(int doDereg);
void ModemReset(void);
void KickWatchDog(void);

#endif
