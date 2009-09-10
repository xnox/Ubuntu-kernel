/* Plz read readme file for Software License information */
#ifndef __EEPROM_H__
#define __EEPROM_H__





#ifdef RTMP_USB_SUPPORT
/*************************************************************************
  *	Public function declarations for usb-based prom chipset
  ************************************************************************/
NTSTATUS RTUSBReadEEPROM16(
	IN	PRTMP_ADAPTER	pAd,
	IN	USHORT			offset,
	OUT	PUSHORT			pData);


NTSTATUS RTUSBWriteEEPROM16(
	IN RTMP_ADAPTER *pAd, 
	IN USHORT offset, 
	IN USHORT value);

#endif // RTMP_USB_SUPPORT //



#ifdef RT30xx
#ifdef RTMP_EFUSE_SUPPORT
int rtmp_ee_efuse_read16(
	IN RTMP_ADAPTER *pAd, 
	IN USHORT Offset,
	OUT USHORT *pValue);

int rtmp_ee_efuse_write16(
	IN RTMP_ADAPTER *pAd, 
	IN USHORT Offset, 
	IN USHORT data);
#endif // RTMP_EFUSE_SUPPORT //
#endif // RT30xx //

/*************************************************************************
  *	Public function declarations for prom operation callback functions setting
  ************************************************************************/
INT RtmpChipOpsEepromHook(
	IN RTMP_ADAPTER *pAd,
	IN INT			infType);

#endif // __EEPROM_H__ //
