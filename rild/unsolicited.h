#ifndef __UNSOLICITED_H
#define __UNSOLICITED_H

#include "common.h"

int ParseUnsolRes(char *pResult);

int urc_cgreg(char * pResult, int nIndex);
int urc_cpin(char * pResult, int nIndex);
int urc_qind(char * pResult, int nIndex);
int urc_cmti(char * pResult, int nIndex);
int urc_cfun(char * pResult, int nIndex);
int urc_qiopen(char * pResult, int nIndex);
int urc_qiurc(char * pResult, int nIndex);
int urc_clip(char * pResult, int nIndex);
int urc_ring(char * pResult, int nIndex);
int urc_no_carrier(char * pResult, int nIndex);
int urc_dsci(char * pResult, int nIndex);


#endif
