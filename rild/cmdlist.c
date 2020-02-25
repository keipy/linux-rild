/****************************************************************************
  cmdlist.c
  Author : kpkang@gmail.com
*****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "agent.h"
#include "m2mdbg.h"
#include "utils.h"
#include "clientIF.h"

#include "cmdlist.h"

static CMD_TagPool cmd_pool;

static pthread_mutex_t s_cmdQmutex;
//static  CList m_listATCommand;

void CmdPoolClear()
{
  CMD_Tlv *tlv;
  CMD_Tlv *temp;
  CMD_TagPool *pool = &cmd_pool;

  pthread_mutex_lock(&s_cmdQmutex);
  for (tlv = pool->head; tlv != NULL; tlv = temp)
  {
    temp  = tlv->next;
    free(tlv);
  }

  pool->count = 0;
  pool->head = NULL;
  pthread_mutex_unlock(&s_cmdQmutex);
}

int CmdPoolGet(BYTE tag, CMD_Tlv **pptlv)
{
  int rv = 0;
  CMD_Tlv *tlv;
  CMD_TagPool *pool = &cmd_pool;

  pthread_mutex_lock(&s_cmdQmutex);
  for (tlv = pool->head; tlv != NULL; tlv = tlv->next)
  {
    if (tag == tlv->tag)
    {
      if (rv == 0) {
        *pptlv = tlv;
      }
      rv++;

      break;   // first one found and return
    }
  }
  pthread_mutex_unlock(&s_cmdQmutex);
  
  if (rv == 0)
    *pptlv = NULL;
  
  return rv;
}

int CmdPoolGetHead(CMD_Tlv **pptlv)
{
  int rv = 0;
  pthread_mutex_lock(&s_cmdQmutex);
  *pptlv = cmd_pool.head;
  rv = cmd_pool.count;
  pthread_mutex_unlock(&s_cmdQmutex);
  
  return rv;
}


// no need to check duplicate !!!
int CmdPoolPut(BYTE tag, unsigned short size, unsigned char* data)
{
  CMD_Tlv *tlv;
	CMD_TagPool *pool = &cmd_pool;

  tlv = (CMD_Tlv *)malloc(sizeof(CMD_Tlv)+size);

	if (!tlv) {
    return -1;
	}

  pthread_mutex_lock(&s_cmdQmutex);
  tlv->next = NULL;
  tlv->tag = tag;
  tlv->length = size;
  if (size > 0)
    memcpy(tlv->value, data, size);

  if (!pool->head)
  {
    pool->head = tlv;
  }
  else
  {
	  CMD_Tlv *last = pool->head;
    while (last->next)
	  {
      last = last->next;
	  }

    last->next = tlv;
  }

  pool->count++;
  pthread_mutex_unlock(&s_cmdQmutex);
	return 0;
}

// no need to check duplicate !!!
int CmdPoolPutHead(BYTE tag, unsigned short size, unsigned char* data)
{
  CMD_Tlv *tlv;
	CMD_TagPool *pool = &cmd_pool;
  CMD_Tlv *head = pool->head;

  tlv = (CMD_Tlv *)malloc(sizeof(CMD_Tlv)+size);

	if (!tlv) {
    return -1;
	}
  pthread_mutex_lock(&s_cmdQmutex);
  tlv->next = NULL;
  tlv->tag = tag;
  tlv->length = size;
  if (size > 0)
    memcpy(tlv->value, data, size);

  pool->head = tlv;
  tlv->next = head;

  pool->count++;
  pthread_mutex_unlock(&s_cmdQmutex);
	return 0;
}


int CmdPoolDel(BYTE tag){
	int delete_count = 0;
	CMD_Tlv *prev = NULL;
  CMD_Tlv *tlv = NULL;
  CMD_Tlv *rem = NULL;
  CMD_TagPool *pool = &cmd_pool;
  
  pthread_mutex_lock(&s_cmdQmutex);
  
  tlv = pool->head;

  if (tlv)
  {
    if (tlv->tag == tag) {
      rem = tlv;
      pool->head = tlv->next;
    }else {
      while(tlv->next) {
        prev = tlv;
        tlv = tlv->next;
        if (tlv->tag == tag) {
          rem = tlv;
          prev->next = tlv->next;
          break;
        }
      }
    }

    if (rem) {
      
      free(rem );
      
      pool->count--;
      delete_count++;
    }
  }
  else
  {
    pool->count = 0;
  }
#if 0
  while (tlv)
  {
    if (tag == tlv->tag)
    {
      CMD_Tlv *rem = tlv;
      if (tlv == pool->head){
        pool->head = tlv->next;
        tlv = pool->head;
      }else{
        prev->next = tlv->next;
        tlv = prev->next;
      }
      free(rem );

      pool->count--;
      delete_count++;

      break;   // first one removed and return
    }
    else
    {
      prev = tlv;
      tlv = tlv->next;
    }
  }
#endif
  pthread_mutex_unlock(&s_cmdQmutex);
	return delete_count;
}

int CmdPoolDelHead(void){
  CMD_Tlv *tlv = NULL;
  CMD_TagPool *pool = &cmd_pool;
  
  pthread_mutex_lock(&s_cmdQmutex);
  tlv = pool->head;
  if (tlv)
  {
    pool->count--;
    pool->head = tlv->next;
    free(tlv);
  }
  else
  {
    pool->count = 0;
  }
  
  pthread_mutex_unlock(&s_cmdQmutex);
	return 0;
}

int GetCmdCount(void)
{
  int count = 0;
  
  pthread_mutex_lock(&s_cmdQmutex);
  count = cmd_pool.count;
  pthread_mutex_unlock(&s_cmdQmutex);

  return count;
}

void InitCmdList(void)
{
  pthread_mutex_init(&s_cmdQmutex, NULL);
  CmdPoolClear();
}

void DeinitCmdList(void)
{
  CmdPoolClear();
  pthread_mutex_destroy(&s_cmdQmutex);
}
