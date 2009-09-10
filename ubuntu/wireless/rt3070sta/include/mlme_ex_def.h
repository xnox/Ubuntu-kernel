/* Plz read readme file for Software License information */
#ifndef __MLME_EX_DEF_H__
#define __MLME_EX_DEF_H__


typedef VOID (*STATE_MACHINE_FUNC_EX)(VOID *Adaptor, MLME_QUEUE_ELEM *Elem, PULONG pCurrState, USHORT Idx);

typedef struct _STA_STATE_MACHINE_EX
{
	ULONG					Base;
	ULONG					NrState;
	ULONG					NrMsg;
	ULONG					CurrState;
	STATE_MACHINE_FUNC_EX	*TransFunc;
} STATE_MACHINE_EX, *PSTA_STATE_MACHINE_EX;

#endif // __MLME_EX_DEF_H__ //

