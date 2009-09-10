/* Plz read readme file for Software License information */

#ifndef __VR_IKANS_H__
#define __VR_IKANS_H__

#ifndef MODULE_IKANOS
#define IKANOS_EXTERN	extern
#else
#define IKANOS_EXTERN
#endif // MODULE_IKANOS //

#ifdef IKANOS_VX_1X0
	typedef void (*IkanosWlanTxCbFuncP)(void *, void *);

	struct IKANOS_TX_INFO
	{
		struct net_device *netdev;
		IkanosWlanTxCbFuncP *fp;
	};
#endif // IKANOS_VX_1X0 //


IKANOS_EXTERN void VR_IKANOS_FP_Init(UINT8 BssNum, UINT8 *pApMac);

IKANOS_EXTERN INT32 IKANOS_DataFramesTx(struct sk_buff *pSkb,
										struct net_device *pNetDev);

IKANOS_EXTERN void IKANOS_DataFrameRx(PRTMP_ADAPTER pAd,
										void *pRxParam,
										struct sk_buff *pSkb,
										UINT32 Length);

#endif // __VR_IKANS_H__ //

/* End of vr_ikans.h */
