#ifndef _UTIL_H_
#define _UTIL_H_

#include <time.h>
/*-----------------------------------------------------------------------------
  Julian time structure
-----------------------------------------------------------------------------*/

typedef struct
{
  /* Year [1980..2100] */
  unsigned short                          year;

  /* Month of year [1..12] */
  unsigned short                          month;

  /* Day of month [1..31] */
  unsigned short                          day;

  /* Hour of day [0..23] */
  unsigned short                          hour;

  /* Minute of hour [0..59] */
  unsigned short                          minute;

  /* Second of minute [0..59] */
  unsigned short                          second;

  /* Day of the week [0..6] [Monday .. Sunday] */
  unsigned short                          day_of_week;
}
time_julian_type;

int axtoi( const char *hexStg );

unsigned int div4x2
(
  unsigned int dividend,       /* Dividend, note dword     */
  unsigned short  divisor,         /* Divisor                  */
  unsigned short  *rem_ptr    /* Pointer to the remainder */
);

void time_jul_from_secs
(
  /* Number of seconds since base date */
  unsigned int                             secs,

  /* OUT: Pointer to Julian date record */
  time_julian_type                *julian
);

unsigned int time_jul_to_secs
(
  /* Pointer to Julian date record */
  const time_julian_type          *julian
);

void msg_dump(char *pTitle, void *vpData, int ncbData);
int xtoi(char *xs);
byte Hex2Dec(char *pHex);
void Hex2Bcd(char *pHex, char *pBcd);
void Bcd2Hex(char *pBcd, char *pHex, int nLength);

char *strchr_nth(char *text, char ch, int nth);
char *strchr_nth_next(char *text, char ch, int nth);
char *strrchr_nth(char *text, char ch, int nth);
char *strrchr_nth_next(char *text, char ch, int nth);
void str_uppercase(char *s);
int get_nth_parameter(char *text, int nth, char* parm, int parm_sz);
void remove_quote(char *text);


#define GET_NTH_PARAM(A,B,C) get_nth_parameter(A,B,C,sizeof(C))


int pdu2text(char *pInPDU, char *pOutBuf);
void msleep(int ms);
BOOL IsTTYAvailable(char*);
BOOL IsNETAvailable(void);
BOOL IsPPPLinkUp(void);
BOOL IsWANConnected(void);

int GetProcID(char *strName);
int CheckPPPd(void);
int GetProcState(int nPID, char *sResult);
int  GetIpAddress(IPAddrT *ip_addr);
int SetLocalTime(struct tm *tm_cclk, int tz, int sync_rtc);

#endif
