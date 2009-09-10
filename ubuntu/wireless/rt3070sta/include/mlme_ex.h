/* Plz read readme file for Software License information */
#ifndef __MLME_EX_H__
#define __MLME_EX_H__

#include "mlme_ex_def.h"


VOID StateMachineInitEx(
	IN STATE_MACHINE_EX *S, 
	IN STATE_MACHINE_FUNC_EX Trans[], 
	IN ULONG StNr,
	IN ULONG MsgNr,
	IN STATE_MACHINE_FUNC_EX DefFunc, 
	IN ULONG InitState, 
	IN ULONG Base);

VOID StateMachineSetActionEx(
	IN STATE_MACHINE_EX *S, 
	IN ULONG St, 
	IN ULONG Msg, 
	IN STATE_MACHINE_FUNC_EX Func);

BOOLEAN isValidApCliIf(
	SHORT Idx);

VOID StateMachinePerformActionEx(
	IN PRTMP_ADAPTER pAd, 
	IN STATE_MACHINE_EX *S, 
	IN MLME_QUEUE_ELEM *Elem,
	USHORT Idx,
	PULONG pCurrState);

BOOLEAN MlmeEnqueueEx(
	IN	PRTMP_ADAPTER pAd,
	IN ULONG Machine, 
	IN ULONG MsgType, 
	IN ULONG MsgLen, 
	IN VOID *Msg,
	IN USHORT Idx);

VOID DropEx(
    IN PRTMP_ADAPTER pAd,
    IN MLME_QUEUE_ELEM *Elem,
	PULONG pCurrState,
	USHORT Idx);

#endif /* __MLME_EX_H__ */

