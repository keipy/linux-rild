#ifndef _M2MDBG_H_
#define _M2MDBG_H_

#include "ril.h"

#define MSG_ERROR    0x1
#define MSG_HIGH     0x2
#define MSG_MID      0x4
#define MSG_LOW      0x8

#define WATCHDOG_ON  0x10
#define PWROFF_CTRL  0x20
#define HOTSWAP_ENA  0x40
#define DAEMON_MODE  0x80

#define NET_PPP_AUTO  0x100
#define NET_ETH_AUTO  0x200
#define NET_INF_TASK  (NET_PPP_AUTO | NET_ETH_AUTO)

#define AT_INTF_UART 0x1000
#define AT_INTF_USB  0x2000

#define MODEL_EC21KL 0x10000

#include <syslog.h>

//#define DEBUG(m,msg...) do{ if (m & argument_mask) printf(msg); }while(0)
#define DEBUG(m,msg...) do { \
                          if (m & argument_mask) { \
                            if (DAEMON_MODE & argument_mask) syslog (LOG_NOTICE, msg); \
                            else                             printf(msg); \
                          }  \
                        } while(0) 

extern DWORD argument_mask;

#endif // _M2MDBG_H_
