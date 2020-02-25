#ifndef __SMS_H
#define __SMS_H

#include "clientIF.h"



int EncodePDU(/*IN*/char *inSMSTxt, /*IN*/int inType, /*IN*/char *inMtNumber, /*IN*/char *inMoNumber, /*OUT*/char *outSMSPDU);
int DecodePDU(/*IN*/char *inSMSPDU, /*OUT*/int *outType, /*OUT*/char *outNumber, /*OUT*/char *outSMSTxt) ;


int GetCharLength(unsigned char* inBuffer, int nSize);
BOOL KSC5601ToUCS2(unsigned char* inBuffer, WORD* outString, int nCharLen);

#endif
