/* Plz read readme file for Software License information */

#ifndef MODULE_MBSS

#define MBSS_EXTERN    extern

#else

#define MBSS_EXTERN

#endif // MODULE_MBSS //


/* Public function list */
MBSS_EXTERN VOID RT28xx_MBSS_Init(
	IN PRTMP_ADAPTER ad_p,
	IN PNET_DEV main_dev_p);

MBSS_EXTERN VOID RT28xx_MBSS_Close(
	IN PRTMP_ADAPTER ad_p);

MBSS_EXTERN VOID RT28xx_MBSS_Remove(
	IN PRTMP_ADAPTER ad_p);

INT MBSS_VirtualIF_Open(
	IN	PNET_DEV			dev_p);
INT MBSS_VirtualIF_Close(
	IN	PNET_DEV			dev_p);
INT MBSS_VirtualIF_PacketSend(
	IN PNDIS_PACKET			skb_p,
	IN PNET_DEV				dev_p);
INT MBSS_VirtualIF_Ioctl(
	IN PNET_DEV				dev_p, 
	IN OUT struct ifreq 	*rq_p, 
	IN INT cmd);

/* End of ap_mbss.h */
