/*
 *
 * ipg.h
 *
 * Include file for Gigabit Ethernet device driver for Network
 * Interface Cards (NICs) utilizing the Tamarack Microelectronics
 * Inc. IPG Gigabit or Triple Speed Ethernet Media Access
 * Controller.
 *
 * Craig Rich
 * Sundance Technology, Inc.
 * 1485 Saratoga Avenue
 * Suite 200
 * San Jose, CA 95129
 * 408 873 4117
 * www.sundanceti.com
 * craig_rich@sundanceti.com
 *
 * Rev  Date     Description
 * --------------------------------------------------------------
 * 0.1  11/8/99  Initial revision work begins
 *
 * 0.2  12/1/99  Minor update to modversions.h inclusion.
 *
 * 0.3  12/30/99 Updates to fully comply with IPG spec.
 *
 * 0.4  4/24/00  Updates to allow for removal of FCS generation
 *               and verification.
 * 0.5  8/15/00  Updates for MII PHY registers and fields.
 *
 * 0.6  8/31/00  Updates to change to using 64 bit data types
 *
 * 0.7  10/31/00 Added DDEBUG_MSG to allow for easy activation of
 *               individual DEBUG_MSGs.
 *
 * 0.8  11/06/00 Changed LastFreedRxBuff to LastRestoredRxBuff for
 *               clarity.
 *
 * 0.9  11/10/00 Changed Sundance DeviceID to 0x9020
 *
 * 0.10 2/14/01  Changed "DROP_ON_ERRORS", breaking out Ethernet from
 *               TCP/IP errors.
 *
 * 0.11 3/16/01  Changed "IPG_FRAMESBETWEENTXCOMPLETES" to
 *               "IPG_FRAMESBETWEENTXDMACOMPLETES" since will
 *               be using TxDMAIndicate instead of TxIndicate to
 *               improve performance. Added TFDunavailCount and
 *               RFDlistendCount to aid in performance improvement.
 *
 * 0.12 3/22/01  Removed IPG_DROP_ON_RX_TCPIP_ERRORS.
 *
 * 0.13 3/23/01  Removed IPG_TXQUEUE_MARGIN.
 *
 * 0.14 3/30/01  Broke out sections into multiple files and added
 *               OS version specific detection and settings.
 */


/*
 * Linux header utilization:
 *
 * version.h	For Linux kernel version detection.
 *
 * module.h	For modularized driver support.
 *
 * kernel.h     For 'printk'.
 *
 * pci.h        PCI support, including ID, VENDOR, and CLASS
 *              standard definitions; PCI specific structures,
 *              including pci_dev struct.
 *
 * ioport.h     I/O ports, check_region, request_region,
 *              release_region.
 *
 * errno.h      Standard error numbers, e.g. ENODEV.
 *
 * asm/io.h     For reading/writing I/O ports, and for virt_to_bus
 *		function.
 *
 * delay.h      For milisecond delays.
 *
 * types.h      For specific typedefs (i.e. u32, u16, u8).
 *
 * netdevice.h  For device structure needed for network support.
 *
 * etherdevice.h	For ethernet device support.
 *
 * init.h       For __initfunc.
 *
 * skbuff.h	Socket buffer (skbuff) definition.
 *
 * asm/bitops.h	For test_and_set_bit, clear_bit functions.
 *
 * asm/spinlock.h For spin_lock_irqsave, spin_lock_irqrestore functions.
 *
 */


#include <linux/version.h>
#include <linux/utsrelease.h>
#include <linux/module.h>

#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,3,0)) &&  defined(MODVERSIONS))
	#include <linux/modversions.h>
#endif

#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/ioport.h>
#include <linux/errno.h>
#include <asm/io.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/init.h>
#include <linux/skbuff.h>
#include <linux/version.h>
#include <asm/bitops.h>
/*#include <asm/spinlock.h>*/

#define DrvVer "2.09d"

const char *version =
"ipg : "DrvVer" Written by Craig Rich, www.sundanceti.com\n";

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,1,0))
	char kernel_version[] = UTS_RELEASE;
#endif

#if (defined(MODULE) && (LINUX_VERSION_CODE > KERNEL_VERSION(2,1,15)))
	MODULE_AUTHOR("IC Plus Corp. 2003                                           ");
   MODULE_DESCRIPTION("IC Plus IP1000 Gigabit Ethernet Adapter Linux Driver "DrvVer);
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,4,0))
	#define IPG_LINUX2_2
	#undef IPG_LINUX2_4

	#define IPG_DEV_KFREE_SKB(skb) dev_kfree_skb(skb)

	#define IPG_HOST2BUS_MAP(addr) virt_to_bus(addr)

	#define IPG_PCI_RESOURCE_START(dev, bar) dev->base_address[bar]

	#define IPG_TX_NOTBUSY(dev) clear_bit(0,(void*)&dev->tbusy)
	#define IPG_IS_TX_BUSY(dev) test_and_set_bit(0, (void*)&dev->tbusy)

	#define IPG_DEVICE_TYPE struct device
	#define IPG_STATS_TYPE struct enet_statistics
#else
	#undef IPG_LINUX2_2
	#define IPG_LINUX2_4

	#define IPG_DEV_KFREE_SKB(skb) dev_kfree_skb_irq(skb)

	#define IPG_HOST2BUS_MAP(addr) addr

	#define IPG_PCI_RESOURCE_START(dev, bar) dev->resource[bar].start

	#define IPG_TX_NOTBUSY(dev) netif_wake_queue(dev)
	#define IPG_IS_TX_BUSY(dev) netif_queue_stopped(dev)

	#define IPG_DEVICE_TYPE struct net_device
	#define IPG_STATS_TYPE struct net_device_stats
#endif

/* Order of the following includes is important, the following must
 * appear first:
 * ipg_constants.h
 * ipg_tune.h
 */
#include "ipg_constants.h"
#include "ipg_tune.h"
#include "ipg_macros.h"
#include "ipg_structs.h"
#include "PhyParam.h"

/* end ipg.h */
