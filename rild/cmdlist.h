#ifndef __CMDLIST_H
#define __CMDLIST_H


#include "common.h"

struct _CMD_Tlv{
  struct _CMD_Tlv *next;
  BYTE tag;
  WORD length;
  BYTE value[1];
};

typedef struct _CMD_Tlv CMD_Tlv;

typedef struct {
  int count;
  CMD_Tlv *head;
}CMD_TagPool;

void CmdPoolClear();

int CmdPoolGet(BYTE tag, CMD_Tlv **pptlv);
int CmdPoolGetHead(CMD_Tlv **pptlv);

int CmdPoolPut(BYTE tag, unsigned short size, unsigned char* data);
int CmdPoolPutHead(BYTE tag, unsigned short size, unsigned char* data);

int CmdPoolDel(BYTE tag);
int CmdPoolDelHead(void);

int GetCmdCount(void);
void InitCmdList(void);
void DeinitCmdList(void);

#endif
