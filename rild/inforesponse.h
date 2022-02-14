
#ifndef __INFORESPONSE_H
#define __INFORESPONSE_H

#include "common.h"


int response_version(char * pResponse);
int response_model(char * pResponse);
int response_cnum(char * pResponse);
int response_cclk(char * pResponse);
int response_qcds(char * pResponse);
int response_cimi(char * pResponse);
int response_cgsn(char * pResponse);
int response_cpin(char * pResponse);
int response_cgreg(char * pResponse);
int response_cgdcont(char * pResponse);
int response_qcps(char * pResponse);
int response_qcota(char * pResponse);
int response_iccid(char * pResponse);
int response_qcnc(char * pResponse);
int response_qcsq(char * pResponse);
int response_cgpaddr(char * pResponse);
int response_band(char * pResponse);
int response_nwscanmode(char * pResponse);
int response_risignaltype(char * pResponse);
int response_apready(char * pResponse);
int response_urc_ri_sms(char * pResponse);
int response_urcport(char * pResponse);
int response_qicsgp(char * pResponse);
int response_qiact(char * pResponse);
int response_qistate(char * pResponse);
int response_qicfg(char * pResponse);
int response_qsimdet(char * pResponse);
int response_qsimstat(char * pResponse);
int response_adc(char *pResponse);
int response_temp(char *pResponse);
int response_sim_presence(char *pResponse);
int response_clvl(char *pResponse);
int response_cmvl(char *pResponse);
int response_cops(char *pResponse);
int response_cmgl(char *pResponse);

#endif

