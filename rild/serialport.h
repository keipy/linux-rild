#ifndef __SERIALPORT_H
#define __SERIALPORT_H

#include <pthread.h>
#include <limits.h>

#include "common.h"


int  OpenSerial(char *, int, int);
int  WriteSerial(int, BYTE *, int);
int  ReadSerial(int, char *, int);
void CloseSerial(int);

void rts_on(int);
void rts_off(int);
int  cts_check(int);
int  stty_raw(int, int, int);

#endif
