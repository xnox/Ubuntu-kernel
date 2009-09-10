/* Plz read readme file for Software License information */

#ifndef __RTMP_MCU_H__
#define __RTMP_MCU_H__


INT RtmpAsicEraseFirmware(
	IN PRTMP_ADAPTER pAd);

NDIS_STATUS RtmpAsicLoadFirmware(
	IN PRTMP_ADAPTER pAd);

INT RtmpAsicSendCommandToMcu(
	IN PRTMP_ADAPTER pAd,
	IN UCHAR		 Command,
	IN UCHAR		 Token,
	IN UCHAR		 Arg0,
	IN UCHAR		 Arg1);
	
#endif // __RTMP_MCU_H__ //
