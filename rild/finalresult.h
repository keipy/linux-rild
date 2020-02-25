#ifndef __FINALRESULT_H
#define __FINALRESULT_H

#include "common.h"



int ParseFinalRes(char *pResult);


int frc_ok(char * pResult, int nIndex);
int frc_error(char * pResult, int nIndex);
int frc_cme_error(char * pResult, int nIndex);
int frc_cms_error(char * pResult, int nIndex);
int frc_send_okay(char * pResult, int nIndex);
int frc_send_fail(char * pResult, int nIndex); 



#endif
