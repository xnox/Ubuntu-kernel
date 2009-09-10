/* Plz read readme file for Software License information */

#ifndef __RTMP_IFACE_H__
#define __RTMP_IFACE_H__




#ifdef RTMP_USB_SUPPORT
#include "iface/rtmp_usb.h"
#endif // RTMP_USB_SUPPORT //


typedef struct _INF_PCI_CONFIG_
{
	unsigned long	CSRBaseAddress;     // PCI MMIO Base Address, all access will use
	unsigned int	irq_num;
}INF_PCI_CONFIG;


typedef struct _INF_USB_CONFIG_
{
	UINT8                BulkInEpAddr;		// bulk-in endpoint address
	UINT8                BulkOutEpAddr[6];	// bulk-out endpoint address
}INF_USB_CONFIG;


typedef struct _INF_RBUS_CONFIG_
{
	unsigned long		csr_addr;
	unsigned int		irq;
}INF_RBUS_CONFIG;


typedef enum _RTMP_INF_TYPE_
{	
	RTMP_DEV_INF_UNKNOWN = 0,
	RTMP_DEV_INF_PCI = 1,
	RTMP_DEV_INF_USB = 2,
	RTMP_DEV_INF_RBUS = 4,
}RTMP_INF_TYPE;


typedef union _RTMP_INF_CONFIG_{
	struct _INF_PCI_CONFIG_ 		pciConfig;
	struct _INF_USB_CONFIG_ 		usbConfig;
	struct _INF_RBUS_CONFIG_		rbusConfig;
}RTMP_INF_CONFIG;

#endif // __RTMP_IFACE_H__ //
