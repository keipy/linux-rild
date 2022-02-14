/****************************************************************************
  utils.c
  Author : kpkang@gmail.com
*****************************************************************************/


#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <sys/msg.h>
#include <ctype.h>
#include <time.h>
#include <pthread.h>
#include <limits.h>
#include <errno.h>

#include "m2mdbg.h"
#include "utils.h"

/*-----------------------------------------------------------------------------
                       Time Translation Tables

  The following tables are used for making conversions between
  Julian dates and number of seconds.
-----------------------------------------------------------------------------*/

#define SEC_FROM_1970_TO_1980 (315532800UL+399600UL)  // 399600 : 4일 + 15시간 차(Navicall)

/* This is the number of days in a leap year set.
   A leap year set includes 1 leap year, and 3 normal years. */
#define TIME_JUL_QUAD_YEAR        (366+(3*365))

/* The year_tab table is used for determining the number of days which
   have elapsed since the start of each year of a leap year set. It has
   1 extra entry which is used when trying to find a 'bracketing' year.
   The search is for a day >= year_tab[i] and day < year_tab[i+1]. */

static const unsigned short year_tab[] = {
  0,                              /* Year 0 (leap year) */
  366,                            /* Year 1             */
  366+365,                        /* Year 2             */
  366+365+365,                    /* Year 3             */
  366+365+365+365                 /* Bracket year       */
};


/* The norm_month_tab table holds the number of cumulative days that have
   elapsed as of the end of each month during a non-leap year. */

static const unsigned short norm_month_tab[] = {
  0,                                    /* --- */
  31,                                   /* Jan */
  31+28,                                /* Feb */
  31+28+31,                             /* Mar */
  31+28+31+30,                          /* Apr */
  31+28+31+30+31,                       /* May */
  31+28+31+30+31+30,                    /* Jun */
  31+28+31+30+31+30+31,                 /* Jul */
  31+28+31+30+31+30+31+31,              /* Aug */
  31+28+31+30+31+30+31+31+30,           /* Sep */
  31+28+31+30+31+30+31+31+30+31,        /* Oct */
  31+28+31+30+31+30+31+31+30+31+30,     /* Nov */
  31+28+31+30+31+30+31+31+30+31+30+31   /* Dec */
};

/* The leap_month_tab table holds the number of cumulative days that have
   elapsed as of the end of each month during a leap year. */

static const unsigned short leap_month_tab[] = {
  0,                                    /* --- */
  31,                                   /* Jan */
  31+29,                                /* Feb */
  31+29+31,                             /* Mar */
  31+29+31+30,                          /* Apr */
  31+29+31+30+31,                       /* May */
  31+29+31+30+31+30,                    /* Jun */
  31+29+31+30+31+30+31,                 /* Jul */
  31+29+31+30+31+30+31+31,              /* Aug */
  31+29+31+30+31+30+31+31+30,           /* Sep */
  31+29+31+30+31+30+31+31+30+31,        /* Oct */
  31+29+31+30+31+30+31+31+30+31+30,     /* Nov */
  31+29+31+30+31+30+31+31+30+31+30+31   /* Dec */
};

/* The day_offset table holds the number of days to offset
   as of the end of each year. */

static const unsigned short day_offset[] = {
  1,                                    /* Year 0 (leap year) */
  1+2,                                  /* Year 1             */
  1+2+1,                                /* Year 2             */
  1+2+1+1                               /* Year 3             */
};

/*-----------------------------------------------------------------------------
  Date conversion constants
-----------------------------------------------------------------------------*/

/* 5 days (duration between Jan 1 and Jan 6), expressed as seconds. */

#define TIME_JUL_OFFSET_S         432000UL


/* This is the year upon which all time values used by the Clock Services
** are based.  NOTE:  The user base day (GPS) is Jan 6 1980, but the internal
** base date is Jan 1 1980 to simplify calculations */

#define TIME_JUL_BASE_YEAR        1980

///////////
//////////
#define UPPER(chr) if(chr >= 'a' && chr < 'z')\
  chr = (chr - 0x20);

//////////
///////////
/*=============================================================================

FUNCTION axtoi

DESCRIPTION
  This procedure converts a pdu string to  int   

DEPENDENCIES
  None

RETURN VALUE

SIDE EFFECTS
  None

=============================================================================*/

int axtoi( const char *hexStg )
{
  int n = 0;         // position in string
  int m = 0;         // position in digit[] to shift
  int count;         // loop index
  int intValue = 0;  // integer value of hex string
  int digit[5];      // hold values to convert
  while (n < 4)
  {
     if (hexStg[n]=='\0')
        break;
     if (hexStg[n] > 0x29 && hexStg[n] < 0x3A ) //if 0 to 9
        digit[n] = hexStg[n] & 0x0f;            //convert to int
     else if (hexStg[n] > 0x40 && hexStg[n] < 0x47) //if A to F
        digit[n] = (hexStg[n] & 0x0f) + 9;      //convert to int
     else if (hexStg[n] > 0x60 && hexStg[n] < 0x67) //if a to f
        digit[n] = (hexStg[n] & 0x0f) + 9;      //convert to int
     else break;
    n++;
  }
  count = n;
  m = n - 1;
  n = 0;
  while(n < count)
  {
     intValue = intValue | (digit[n] << (m << 2));
     m--;   // adjust the position to set
     n++;   // next digit to process
  }
  return (intValue);
}

/*===========================================================================

FUNCTION DIV4X2

DESCRIPTION
  This procedure divides a specified 32 bit unsigned dividend by a
  specified 16 bit unsigned divisor. Both the quotient and remainder
  are returned.

DEPENDENCIES
  None

RETURN VALUE
  The quotient of dividend / divisor.

SIDE EFFECTS
  None

===========================================================================*/
unsigned int div4x2
(
  unsigned int dividend,       /* Dividend, note dword     */
  unsigned short  divisor,         /* Divisor                  */
  unsigned short  *rem_ptr    /* Pointer to the remainder */
)
{
  *rem_ptr = (unsigned short) (dividend % divisor);

  return (dividend / divisor);

} /* END div4x2 */

/*=============================================================================

FUNCTION TIME_JUL_FROM_SECS

DESCRIPTION
  This procedure converts a specified number of elapsed seconds   
  since the base date to its equivalent Julian date and time.     

DEPENDENCIES
  None

RETURN VALUE
  The specified Julian date record is filled in with equivalent date/time,
  and returned into the area pointed to by julian_ptr.

SIDE EFFECTS
  None

=============================================================================*/

void time_jul_from_secs
(
  /* Number of seconds since base date */
  unsigned int                             secs,

  /* OUT: Pointer to Julian date record */
  time_julian_type                *julian
)
{
  /* Loop index */
  unsigned int /* fast */       i;

  /* Days since beginning of year or quad-year */
  unsigned short                           days;

  /* Quad years since base date */
  unsigned int /* fast */       quad_years;

  /* Leap-year or non-leap year month table */
  const unsigned short                    *month_table;

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /* Add number of seconds from Jan 1 to Jan 6 from input seconds
     in order to have number of seconds since Jan 1, 1980 for calculation */

  secs += TIME_JUL_OFFSET_S;


  /* Divide elapsed seconds by 60: remainder is seconds of Julian date;
     quotient is number of elapsed minutes. */

  secs = div4x2 ( secs, 60, &julian->second );


  /* Divide elapsed minutes by 60: remainder is minutes of Julian date;
     quotient is elapsed hours. */

  secs = div4x2 ( secs, 60, &julian->minute );


  /* Divide elapsed hours by 24: remainder is hours of Julian date;
     quotient is elapsed days. */

  secs = div4x2 ( secs, 24, &julian->hour );


  /* Now things get messier. We have number of elapsed days. The 1st thing
     we do is compute how many leap year sets have gone by. We multiply
     this value by 4 (since there are 4 years in a leap year set) and add
     in the base year. This gives us a partial year value. */

  quad_years = div4x2( secs, TIME_JUL_QUAD_YEAR, &days );

  julian->year = TIME_JUL_BASE_YEAR + (4 * quad_years);


  /* Now we use the year_tab to figure out which year of the leap year
     set we are in. */

  for ( i = 0; days >= year_tab[ i + 1 ]; i++ )
  {
    /* No-op. Table seach. */
  }

  /* Subtract out days prior to current year. */
  days -= year_tab[ i ];
  
  /* Use search index to arrive at current year. */
  julian->year += i;  


  /* Take the day-of-week offset for the number of quad-years, add in
     the day-of-week offset for the year in a quad-year, add in the number
     of days into this year. */

  julian->day_of_week =
        (day_offset[3] * quad_years + day_offset[i] + days) % 7;


  /* Now we know year, hour, minute and second. We also know how many days
     we are into the current year. From this, we can compute day & month. */


  /* Use leap_month_tab in leap years, and norm_month_tab in other years */

  month_table = (i == 0) ? leap_month_tab : norm_month_tab;


  /* Search month-table to compute month */

  for ( i = 0; days >= month_table[ i + 1 ]; i++ )
  {
    /* No-op. Table seach. */
  }


  /* Compute & store day of month. */
  julian->day = days - month_table[ i ] + 1;  

  /* Store month. */
  julian->month = i + 1;


} /* time_jul_from_secs */


/*=============================================================================

FUNCTION TIME_JUL_TO_SECS

DESCRIPTION
  This procedure converts a specified Julian date and time to an  
  equivalent number of elapsed seconds since the base date.    

DEPENDENCIES
  None

RETURN VALUE
  Number of elapsed seconds since base date.       

SIDE EFFECTS
  None

=============================================================================*/

unsigned int time_jul_to_secs
(
  /* Pointer to Julian date record */
  const time_julian_type          *julian
)
{
  /* Time in various units (days, hours, minutes, and finally seconds) */
  unsigned int                          time;

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /* First, compute number of days contained in the last whole leap
     year set. */

  time = ( (julian->year - TIME_JUL_BASE_YEAR) / 4L ) * TIME_JUL_QUAD_YEAR;


  /* Now, add in days since last leap year to start of this month. */

  if ( (julian->year & 0x3) == 0 )
  {
    /* If this is a leap year, add in only days since the beginning of the
       year to start of this month. */

    time += leap_month_tab[ julian->month - 1 ];
  }
  else
  {
    /* If this is not a leap year, add in days since last leap year plus
       days since start of this year to start of this month. */

    time += year_tab[ julian->year & 0x3 ];

    time += norm_month_tab[ julian->month - 1 ];
  }

  /* Add in days in current month. */
  time += julian->day - 1;

  /* Add in elapsed hours, minutes, and seconds  */
  time = time * 24  +  julian->hour;
  time = time * 60  +  julian->minute;
  time = time * 60  +  julian->second;


  /* Subtract number of seconds from base (GPS) date of 6 Jan 1980 to
     calculation base date of 1 Jan 1980 */

  time -= TIME_JUL_OFFSET_S;


  /* Return elapsed seconds. */
  return time;

} /* time_jul_to_secs */


void msg_dump(char *pTitle, void *vpData, int ncbData)
{
	int i, move=48;	//48=3*16
	char aTitle[72], aDump[72], ch[4];
	char *pStr;

	//title display ----------------------------------------------------------------
	memset(aTitle, 0x00, sizeof(aTitle));
	sprintf(aTitle, "[%s(%d)]", pTitle, ncbData);
	printf("%s", aTitle);
	for(i=strlen(aTitle); i<67; i++)		printf("-");
	printf("\n");

	//contents display ------------------------------------------------------------
	memset(aDump, 0x00, sizeof(aDump));

	pStr = aDump;
	memset(pStr, 0x20, 68);	//[48][3][16]=67

	for(i=1; i<=ncbData; i++)
	{
		sprintf(ch, "%02X ", ((unsigned char*)vpData)[i-1]);
		memcpy(pStr, ch, 3);

		if((i%16) == 0)	*(pStr+3) = ':';	//delimiter

		pStr += (move+3);

		if(((char*)vpData)[i-1]<0x20 || ((char*)vpData)[i-1]>0x7e)
			*pStr = '^';
		else
			*pStr = ((char*)vpData)[i-1];

		*(pStr+1) = '\0';
		pStr -= move;
 		move -= 2;

		if((i%16) == 0)
		{
			printf("%s\n", aDump);
			pStr = aDump;
			memset(pStr, 0x20, 68);		//[48][3][16]=67
			move = 48;
		}
	}
	if((i%16) != 1)		printf("%s\n", aDump);
	printf("-------------------------------------------------------------------\n");
}

int xtoi(char *xs)
{
    int nResult = 0;
    int nHexLen;
    int i, xv, fact;

    nHexLen = (int)strlen(xs);
    if (0 >= nHexLen)
        return 0;

    if (8 < nHexLen)
        return 0;

    fact = 1;

    for (i = nHexLen - 1; 0 <= i ;i--)
    {
        // The function isxdigit() returns non-zero if its argument is a hexadecimal digit (i.e. A-F, a-f, or 0-9).
        // Otherwise, zero is returned.
        if (isxdigit(xs[i]))
        {
            if ('A' <= xs[i])
            {
                xv = (xs[i] - 'A') + 10;
            }
            else if ('a' <= xs[i])
            {
                xv = (xs[i] - 'a') + 10;
            }
            else
            {
                xv = xs[i] - '0';
            }

            nResult += (xv * fact);
            fact *= 16;
        }
        else
            return 0;
    }

    return nResult;
}

byte Hex2Dec(char *pHex)
{
  int nHigh = (0x41 <= pHex[0]) ? pHex[0] - 0x37 : pHex[0] - 0x30;
  int nLow  = (0x41 <= pHex[1]) ? pHex[1] - 0x37 : pHex[1] - 0x30;

  return (nHigh << 4) | nLow;
}

void Hex2Bcd(char *pHex, char *pBcd)
{
  char nHexHigh;
  char nHexLow;
  int  nLength;
  int  nIndex;

  strcpy(pBcd, pHex);

  nLength = (int)strlen(pBcd);

  if(nLength % 2)
  {
    pBcd[nLength]     = 'F';
    pBcd[nLength + 1] = '\0';

    nLength++;
  }

  for(nIndex = 0; nIndex < nLength; nIndex += 2)
  {
    nHexHigh = pBcd[nIndex];
    nHexLow  = pBcd[nIndex + 1];

    pBcd[nIndex]     = nHexLow;
    pBcd[nIndex + 1] = nHexHigh;
  }
}

void Bcd2Hex(char *pBcd, char *pHex, int nLength)
{
  char nHexHigh;
  char nHexLow;
  int  nIndex;

  nLength *= 2;

  strncpy(pHex, pBcd, nLength);

  for(nIndex = 0; nIndex < nLength; nIndex += 2)
  {
    nHexHigh = pHex[nIndex];
    nHexLow  = pHex[nIndex + 1];

    pHex[nIndex]     = nHexLow;
    pHex[nIndex + 1] = nHexHigh;
  }

  if('F' == pHex[nLength - 1])
  {
    pHex[nLength - 1] = '\0';
  }
}

char *strchr_nth(char *text, char ch, int nth)
{
  int count = 0;

  for( ; 0 != *text; text++)
  {
    if(*text == ch)
    {
      count++;

      if(count == nth)
      {
        return text;
      }
    }
  }

  return NULL;
}

char *strchr_nth_next(char *text, char ch, int nth)
{
  int count = 0;

  for( ; 0 != *text; text++)
  {
    if(*text == ch)
    {
      count++;

      if(count == nth)
      {
        return text+1;
      }
    }
  }

  return NULL;
}

char *strrchr_nth(char *text, char ch, int nth)
{
  int count = 0;
  int len = strlen(text);

  for(len=len-1; len >= 0; len--)
  {
    if(text[len] == ch)
    {
      count++;

      if(count == nth)
      {
        return text+len;
      }
    }
  }

  return NULL;
}

char *strrchr_nth_next(char *text, char ch, int nth)
{
  int count = 0;
  int len = strlen(text);

  for(len=len-1; len >= 0; len--)
  {
    if(text[len] == ch)
    {
      count++;

      if(count == nth)
      {
        return text+len+1;
      }
    }
  }

  return NULL;
}

void str_uppercase(char *s)
{
  int i;
	for (i = 0; s[i]!='\0'; i++) {
		 if(s[i] >= 'a' && s[i] <= 'z') {
				s[i] = s[i] - 32;
		 }
	}
}


// start from 1st
int get_nth_parameter(char *text, int nth, char* parm, int parm_sz)
{
  int parm_len = 0;
  char *p_tmp;

	if (parm_sz > 0) parm_sz -=1;
	else return -1;

  if (nth == 0) return -1;

  if (nth == 1)
  {
    p_tmp = strchr(text, ':');
    if (p_tmp) {
      p_tmp++;
      if (*p_tmp == ' ') p_tmp++;
    }
    else return -1;
  }
  else{
    p_tmp = strchr_nth(text, ',', nth-1);
    if (p_tmp) {
      p_tmp++;
    }
    else return -1;
  }

  while(1)
  {
    if(*p_tmp == ',' || *p_tmp == '\r'  || *p_tmp == '\n' || *p_tmp == 0)
    {
      break;
    }

    if (parm_sz > parm_len)
	    parm[parm_len++] = *p_tmp;
		else
			break;
    p_tmp++;
  }

  parm[parm_len] = 0;

  return parm_len;
}

void remove_quote(char *text)
{
  int i, len = strlen(text);

  if (len < 2 || *text != '"') return;

  for (i = 0; i < len-2; i++)
  {
    text[i] = text[i+1];
  }
  text[i] = 0;
}




int pdu2text(char *pInPDU, char *pOutBuf)
{
  int i, nHigh, nLow;
  int pduLen = strlen(pInPDU);

  if (pduLen%2 > 0){
    return 0;
  }

  for(i = 0; i < pduLen; i += 2)
  {
    nHigh = (0x41 <= pInPDU[i]) ? pInPDU[i] - 0x37 : pInPDU[i] - 0x30;
    nLow  = (0x41 <= pInPDU[i+1]) ? pInPDU[i+1] - 0x37 : pInPDU[i+1] - 0x30;

    pOutBuf[i / 2] = ((nHigh << 4) | nLow);
  }

  return pduLen/2;
}

void msleep(int ms)
{
  int i;
  for (i=0; i < ms; i++){
    usleep(1000); // 1ms
  }
}

BOOL IsTTYAvailable(char *sDev)
{
  char *strTemp;
  char devPath[64];

  strTemp = strrchr(sDev, '/');
  if (strTemp) strTemp++;
  else return FALSE;

  sprintf(devPath, "/sys/class/tty/%s", strTemp);
	if (access(devPath, F_OK) != 0) {
		return FALSE;    
	}
	
  return TRUE;    
}

BOOL IsNETAvailable(void)
{
	if (access("/sys/class/net/wwan0", F_OK) != 0) {
		return FALSE;    
	}
	
  return TRUE;    
}

BOOL IsPPPLinkUp(void)
{
  FILE *fp = NULL;
  char ppp_stat[64] = {'\0'};
    
  fp = fopen("/sys/class/net/ppp0/operstate", "r");///sys/class/net/ppp0/operstate
  if (!fp)
  {
    return FALSE;
  }
  
  fgets(ppp_stat, sizeof(ppp_stat)-1, fp);
  fclose(fp);
  if ( strncmp(ppp_stat, "up", 2u) && strncmp(ppp_stat, "unknown", 7u) )
  {
    return FALSE;
  }

  return TRUE;
}

int GetProcID(char *strName)
{
	int  ret = -1;
	FILE *fp = NULL;
	char outBuf[256];

	sprintf(outBuf, "pidof %s", strName);
	
  fp = popen(outBuf, "r");
  
  if(!fp) {
      DEBUG(MSG_ERROR,"GetProcID(): popen() Failed .. !\n");
      return -1; 
  }

  while(TRUE)
  {
    memset(outBuf, 0x0, sizeof(outBuf));
    
		if (!fgets(outBuf, sizeof(outBuf)-1, fp))
			break;

    if (strlen(outBuf) > 0){
      ret = atoi(outBuf);
      break;
    }
	}
	
  pclose(fp);
  fp = NULL;

  DEBUG(MSG_HIGH, "%s: %d\n", strName, ret);
  return ret;
}

int CheckPPPd(void)
{
	int  ret = 0;
	FILE *fp = NULL;
	char sBuf[256];
	
  fp = popen("ps | grep pppd | grep -v grep | awk '{print $4}'", "r");
  
  if(!fp) {
      DEBUG(MSG_ERROR,"CheckPPPd(): popen() Failed .. !\n");
      return -1; 
  }  

  while(TRUE)
  {
		if (!fgets(sBuf, sizeof(sBuf), fp))
			break;
		if(strlen(sBuf) < 4 )
			continue;
		
		if (!strncmp("pppd", sBuf, 4)){
		  ret = 1;
		  break;;
		}
	}
	
  pclose(fp);
  fp = NULL;

  return ret;
}

/*
Name:   connMgr
State:  Z (zombie)
=========================================
Name:   connMgr
State:  S (sleeping)
*/
int GetProcState(int nPID, char *sResult)
{
	int  i, ret = 0;
	FILE *fp = NULL;
	char sBuf[256];

  sprintf(sBuf, "/proc/%d/status", nPID);
    
  fp = fopen(sBuf, "r");
  if (!fp)
  {
    return -1;
  }

  for (i=0; i < 4; i++){
    memset(sBuf, 0x0, sizeof(sBuf));
		if (!fgets(sBuf, sizeof(sBuf), fp))
			break;
    sBuf[sizeof(sBuf) -1] = 0;

		if (!strncmp("State", sBuf, 5)){
		  strcpy(sResult, sBuf);
		  ret = 1;
		  break;;
		}
  }

  return ret;
}

BOOL IsWANConnected(void)
{
  struct ifaddrs *ifaddr, *ifa;
  int family, s, n;
  char host[NI_MAXHOST];
	char interface_name[16];
	int interface_family;

  if (argument_mask & NET_ETH_AUTO)
	  strcpy(interface_name, "wwan0");
	else
	  strcpy(interface_name, "ppp0");

  if (getifaddrs(&ifaddr) == -1)
  {
     DEBUG(MSG_ERROR, "getifaddrs return error!");
     return FALSE;
  }

  /* Walk through linked list, maintaining head pointer so we can free list later */
  for (ifa = ifaddr, n = 0; ifa != NULL; ifa = ifa->ifa_next, n++)
  {
      if (ifa->ifa_addr == NULL)
         continue;

      family = ifa->ifa_addr->sa_family;

      /* Display interface name and family (including symbolic
      * form of the latter for the common families)
      */
      DEBUG(MSG_ERROR, "%-8s %s (%d)",
            ifa->ifa_name,
            (family == AF_PACKET) ? "AF_PACKET" :
            (family == AF_INET) ? "AF_INET" :
            (family == AF_INET6) ? "AF_INET6" : "???",
            family);

      /* Find the "rmnet_data0" network adapter */
      if (strncmp(ifa->ifa_name, interface_name, strlen(interface_name)) == 0)
      {
          s = getnameinfo(ifa->ifa_addr, 
                        (family == AF_INET) ? sizeof(struct sockaddr_in) :
                        sizeof(struct sockaddr_in6),
                        host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
          if (s != 0)
          {
             DEBUG(MSG_ERROR,"getnameinfo() failed: %s", gai_strerror(s));
          }
          else
          {
            interface_family = family;
            DEBUG(MSG_ERROR,"< Retrived IP, address:%s >", host);
            break;
          }
      }
      else
      {
        DEBUG(MSG_ERROR,"< Not find RMNET >"); 
      }
  }

  if (AF_UNSPEC != interface_family){
    return TRUE;
  }

  return FALSE;
}

int GetIpAddress(IPAddrT *ip_addr)
{
  #include <net/if.h>

	int skfd;
	struct ifreq ifr;

  memset(ip_addr, 0x0, sizeof(IPAddrT));
  
	skfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (skfd < 0) 
	{
	  DEBUG(MSG_ERROR,"IP] socket open error!\n");
	  return -1;
  }
  
  if (argument_mask & NET_ETH_AUTO)
	  strcpy(ifr.ifr_name, "wwan0");
	else
	  strcpy(ifr.ifr_name, "ppp0");

	ifr.ifr_addr.sa_family = AF_INET;
	if (ioctl(skfd, SIOCGIFADDR, &ifr) == 0) {
		memcpy(ip_addr, &(ifr.ifr_addr.sa_data[2]), sizeof(IPAddrT));
    DEBUG(MSG_ERROR, "IP] socket IP %u.%u.%u.%u\n", ip_addr->addr.ipv4.digit[0], ip_addr->addr.ipv4.digit[1], ip_addr->addr.ipv4.digit[2], 
    ip_addr->addr.ipv4.digit[3]); 		
	}
	else{
	  DEBUG(MSG_ERROR,"IP] socket ioctl error\n");
  }
	close(skfd);
	return 0;
}


int SetLocalTime(struct tm *tm_cclk, int tz, int sync_rtc)
{
  struct tm tm_current;
  time_t t_current;  

  // julian time
  DWORD julian_secs;
  time_julian_type julian;

  julian.year = (unsigned short)tm_cclk->tm_year;
  julian.month = (unsigned short)tm_cclk->tm_mon;
  julian.day = (unsigned short)tm_cclk->tm_mday;
  julian.hour = (unsigned short)tm_cclk->tm_hour;
  julian.minute = (unsigned short)tm_cclk->tm_min;
  julian.second = (unsigned short)tm_cclk->tm_sec;
  julian_secs = time_jul_to_secs(&julian);
  julian_secs += (tz*900);
  time_jul_from_secs(julian_secs, &julian);
  //////////////////
  
  tm_current.tm_year = julian.year-1900;
  tm_current.tm_mon  = julian.month-1;
  tm_current.tm_mday = julian.day;
  tm_current.tm_hour = julian.hour;
  tm_current.tm_min  = julian.minute;
  tm_current.tm_sec  = julian.second;
  
  t_current = mktime(&tm_current);
  stime(&t_current);

  if (sync_rtc) system("/sbin/hwclock --systohc");

  return 0;
}

