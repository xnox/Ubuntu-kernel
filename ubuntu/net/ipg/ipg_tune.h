/*
 *
 * ipg_tune.h
 *
 * Include file with tunable driver parameters for Gigabit Ethernet
 * device driver for Network Interface Cards (NICs) utilizing the
 * Tamarack Microelectronics Inc. IPG Gigabit or Triple Speed
 * Ethernet Media Access Controller.
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
 * 0.1  3/30/01  New file created from original ipg.h
 *
 * 0.2  5/22/01  Added PCI_DEVICE_ID_TAMARACK_TC9020_9021_ALT.
 *
 * 0.3  6/20/01  Added IPG_ADD_IPCHECKSUM_ON_TX,
 *               IPG_ADD_TCPCHECKSUM_ON_TX, and
 *               IPG_ADD_UDPCHECKSUM_ON_TX.
 *
 * 0.4  8/11/01  Added comments about avoiding use of TCP/UDP
 *               checksums for silicon revs B3 and earlier.
 *
 * 0.5  10/30/01 Optimized numerous settings.
 */

/* Define PCI vendor and device IDs if not already
 * defined in pci.h.
 */

#ifndef PCI_VENDOR_ID_ICPLUS
#  define       PCI_VENDOR_ID_ICPLUS            0x13F0
#endif
#ifndef PCI_DEVICE_ID_IP1000
#  define       PCI_DEVICE_ID_IP1000		    0x1023
#endif
#ifndef PCI_VENDOR_ID_SUNDANCE
#  define       PCI_VENDOR_ID_SUNDANCE          0x13F0
#endif
#ifndef PCI_DEVICE_ID_SUNDANCE_IPG
#  define       PCI_DEVICE_ID_SUNDANCE_ST2021   0x2021
#endif
#ifndef PCI_DEVICE_ID_TAMARACK_TC9020_9021_ALT
#  define       PCI_DEVICE_ID_TAMARACK_TC9020_9021_ALT 0x9021
#endif
#ifndef PCI_DEVICE_ID_TAMARACK_TC9020_9021
#  define       PCI_DEVICE_ID_TAMARACK_TC9020_9021 0x1021
#endif
#ifndef PCI_VENDOR_ID_DLINK
#  define       PCI_VENDOR_ID_DLINK             0x1186
#endif
#ifndef PCI_DEVICE_ID_DLINK_1002
#  define       PCI_DEVICE_ID_DLINK_1002        0x1002
#endif
#ifndef PCI_DEVICE_ID_DLINK_IP1000A
#  define PCI_DEVICE_ID_DLINK_IP1000A		0x4020
#endif

/* Miscellaneous Constants. */
#define   TRUE  1
#define   FALSE 0

/* Assign IPG_APPEND_FCS_ON_TX > 0 for auto FCS append on TX. */
#define         IPG_APPEND_FCS_ON_TX         TRUE

/* Assign IPG_APPEND_FCS_ON_TX > 0 for auto FCS strip on RX. */
#define         IPG_STRIP_FCS_ON_RX          TRUE

/* Assign IPG_DROP_ON_RX_ETH_ERRORS > 0 to drop RX frames with
 * Ethernet errors.
 */
#define         IPG_DROP_ON_RX_ETH_ERRORS    TRUE

/* Assign IPG_INSERT_MANUAL_VLAN_TAG > 0 to insert VLAN tags manually
 * (via TFC).
 */
#define		IPG_INSERT_MANUAL_VLAN_TAG   FALSE

/* Assign IPG_ADD_IPCHECKSUM_ON_TX > 0 for auto IP checksum on TX. */
#define         IPG_ADD_IPCHECKSUM_ON_TX     FALSE

/* Assign IPG_ADD_TCPCHECKSUM_ON_TX > 0 for auto TCP checksum on TX.
 * DO NOT USE FOR SILICON REVISIONS B3 AND EARLIER.
 */
#define         IPG_ADD_TCPCHECKSUM_ON_TX    FALSE

/* Assign IPG_ADD_UDPCHECKSUM_ON_TX > 0 for auto UDP checksum on TX.
 * DO NOT USE FOR SILICON REVISIONS B3 AND EARLIER.
 */
#define         IPG_ADD_UDPCHECKSUM_ON_TX    FALSE

/* If inserting VLAN tags manually, assign the IPG_MANUAL_VLAN_xx
 * constants as desired.
 */
#define		IPG_MANUAL_VLAN_VID		0xABC
#define		IPG_MANUAL_VLAN_CFI		0x1
#define		IPG_MANUAL_VLAN_USERPRIORITY 0x5

#define         IPG_IO_REG_RANGE		0xFF
#define         IPG_MEM_REG_RANGE		0x154
#define         IPG_DRIVER_NAME		"Sundance Technology IPG Triple-Speed Ethernet"
#define         IPG_NIC_PHY_ADDRESS          0x01
#define		IPG_DMALIST_ALIGN_PAD	0x07
#define		IPG_MULTICAST_HASHTABLE_SIZE	0x40

/* Number of miliseconds to wait after issuing a software reset.
 * 0x05 <= IPG_AC_RESETWAIT to account for proper 10Mbps operation.
 */
#define         IPG_AC_RESETWAIT             0x05

/* Number of IPG_AC_RESETWAIT timeperiods before declaring timeout. */
#define         IPG_AC_RESET_TIMEOUT         0x0A

/* Minimum number of miliseconds used to toggle MDC clock during
 * MII/GMII register access.
 */
#define         IPG_PC_PHYCTRLWAIT           0x01

#define		IPG_TFDLIST_LENGTH		0x100

/* Number of frames between TxDMAComplete interrupt.
 * 0 < IPG_FRAMESBETWEENTXDMACOMPLETES <= IPG_TFDLIST_LENGTH
 */
#define		IPG_FRAMESBETWEENTXDMACOMPLETES 0x1

#ifdef CONFIG_NET_IPG_JUMBO_FRAME

# ifdef CONFIG_NET_IPG_JUMBO_FRAME_SIZE_2K
# define JUMBO_FRAME_SIZE 2048
# define __IPG_RXFRAG_SIZE 2048
# else
#  ifdef CONFIG_NET_IPG_JUMBO_FRAME_SIZE_3K
#  define JUMBO_FRAME_SIZE 3072
#  define __IPG_RXFRAG_SIZE 3072
#  else
#   ifdef CONFIG_NET_IPG_JUMBO_FRAME_SIZE_4K
#   define JUMBO_FRAME_SIZE 4096
#   define __IPG_RXFRAG_SIZE 4088
#   else
#    ifdef CONFIG_NET_IPG_JUMBO_FRAME_SIZE_5K
#    define JUMBO_FRAME_SIZE 5120
#    define __IPG_RXFRAG_SIZE 4088
#    else
#     ifdef CONFIG_NET_IPG_JUMBO_FRAME_SIZE_6K
#     define JUMBO_FRAME_SIZE 6144
#     define __IPG_RXFRAG_SIZE 4088
#     else
#      ifdef CONFIG_NET_IPG_JUMBO_FRAME_SIZE_7K
#      define JUMBO_FRAME_SIZE 7168
#      define __IPG_RXFRAG_SIZE 4088
#      else
#       ifdef CONFIG_NET_IPG_JUMBO_FRAME_SIZE_8K
#       define JUMBO_FRAME_SIZE 8192
#       define __IPG_RXFRAG_SIZE 4088
#       else
#        ifdef CONFIG_NET_IPG_JUMBO_FRAME_SIZE_9K
#        define JUMBO_FRAME_SIZE 9216
#        define __IPG_RXFRAG_SIZE 4088
#        else
#         ifdef CONFIG_NET_IPG_JUMBO_FRAME_SIZE_10K
#         define JUMBO_FRAME_SIZE 10240
#         define __IPG_RXFRAG_SIZE 4088
#         else
#         define JUMBO_FRAME_SIZE 4096
#         endif 
#        endif 
#       endif 
#      endif 
#     endif 
#    endif 
#   endif 
#  endif 
# endif 
#endif

/* Size of allocated received buffers. Nominally 0x0600.
 * Define larger if expecting jumbo frames.
 */
#ifdef CONFIG_NET_IPG_JUMBO_FRAME
//IPG_TXFRAG_SIZE must <= 0x2b00, or TX will crash
#define		IPG_TXFRAG_SIZE		JUMBO_FRAME_SIZE
#endif


/* Size of allocated received buffers. Nominally 0x0600.
 * Define larger if expecting jumbo frames.
 */
#ifdef CONFIG_NET_IPG_JUMBO_FRAME
//4088=4096-8
#define		IPG_RXFRAG_SIZE		__IPG_RXFRAG_SIZE
#define     IPG_RXSUPPORT_SIZE   IPG_MAX_RXFRAME_SIZE
#else
#define		IPG_RXFRAG_SIZE		0x0600
#define     IPG_RXSUPPORT_SIZE   IPG_RXFRAG_SIZE
#endif

/* IPG_MAX_RXFRAME_SIZE <= IPG_RXFRAG_SIZE */
#ifdef CONFIG_NET_IPG_JUMBO_FRAME
#define		IPG_MAX_RXFRAME_SIZE		JUMBO_FRAME_SIZE
#else
#define		IPG_MAX_RXFRAME_SIZE		0x0600
#endif

#define		IPG_RFDLIST_LENGTH		0x100

/* Maximum number of RFDs to process per interrupt.
 * 1 < IPG_MAXRFDPROCESS_COUNT < IPG_RFDLIST_LENGTH
 */
#define		IPG_MAXRFDPROCESS_COUNT	0x80

/* Minimum margin between last freed RFD, and current RFD.
 * 1 < IPG_MINUSEDRFDSTOFREE < IPG_RFDLIST_LENGTH
 */
#define		IPG_MINUSEDRFDSTOFREE	0x80

/* Specify priority threshhold for a RxDMAPriority interrupt. */
#define		IPG_PRIORITY_THRESH		0x07

/* Specify the number of receive frames transferred via DMA
 * before a RX interrupt is issued.
 */
#define		IPG_RXFRAME_COUNT		0x08

/* specify the jumbo frame maximum size
 * per unit is 0x600 (the RxBuffer size that one RFD can carry)
 */
#define     MAX_JUMBOSIZE	        0x8   // max is 12K

/* Specify the maximum amount of time (in 64ns increments) to wait
 * before issuing a RX interrupt if number of frames received
 * is less than IPG_RXFRAME_COUNT.
 *
 * Value	Time
 * -------------------
 * 0x3D09	~1ms
 * 0x061A	~100us
 * 0x009C	~10us
 * 0x000F	~1us
 */
#define		IPG_RXDMAWAIT_TIME		0x009C

/* Key register values loaded at driver start up. */

/* TXDMAPollPeriod is specified in 320ns increments.
 *
 * Value	Time
 * ---------------------
 * 0x00-0x01	320ns
 * 0x03		~1us
 * 0x1F		~10us
 * 0xFF		~82us
 */
#define		IPG_TXDMAPOLLPERIOD_VALUE	0x26
#define		IPG_TXSTARTTHRESH_VALUE	0x0FFF

/* TxDMAUrgentThresh specifies the minimum amount of
 * data in the transmit FIFO before asserting an
 * urgent transmit DMA request.
 *
 * Value	Min TxFIFO occupied space before urgent TX request
 * ---------------------------------------------------------------
 * 0x00-0x04	128 bytes (1024 bits)
 * 0x27		1248 bytes (~10000 bits)
 * 0x30		1536 bytes (12288 bits)
 * 0xFF		8192 bytes (65535 bits)
 */
#define		IPG_TXDMAURGENTTHRESH_VALUE	0x04

/* TxDMABurstThresh specifies the minimum amount of
 * free space in the transmit FIFO before asserting an
 * transmit DMA request.
 *
 * Value	Min TxFIFO free space before TX request
 * ----------------------------------------------------
 * 0x00-0x08	256 bytes
 * 0x30		1536 bytes
 * 0xFF		8192 bytes
 */
#define		IPG_TXDMABURSTTHRESH_VALUE	0x30

/* RXDMAPollPeriod is specified in 320ns increments.
 *
 * Value	Time
 * ---------------------
 * 0x00-0x01	320ns
 * 0x03		~1us
 * 0x1F		~10us
 * 0xFF		~82us
 */
#define		IPG_RXDMAPOLLPERIOD_VALUE	0x01
#define		IPG_RXEARLYTHRESH_VALUE	0x07FF

/* RxDMAUrgentThresh specifies the minimum amount of
 * free space within the receive FIFO before asserting
 * a urgent receive DMA request.
 *
 * Value	Min RxFIFO free space before urgent RX request
 * ---------------------------------------------------------------
 * 0x00-0x04	128 bytes (1024 bits)
 * 0x27		1248 bytes (~10000 bits)
 * 0x30		1536 bytes (12288 bits)
 * 0xFF		8192 bytes (65535 bits)
 */
#define		IPG_RXDMAURGENTTHRESH_VALUE	0x30

/* RxDMABurstThresh specifies the minimum amount of
 * occupied space within the receive FIFO before asserting
 * a receive DMA request.
 *
 * Value	Min TxFIFO free space before TX request
 * ----------------------------------------------------
 * 0x00-0x08	256 bytes
 * 0x30		1536 bytes
 * 0xFF		8192 bytes
 */
#define		IPG_RXDMABURSTTHRESH_VALUE	0x30

/* FlowOnThresh specifies the maximum amount of occupied
 * space in the receive FIFO before a PAUSE frame with
 * maximum pause time transmitted.
 *
 * Value	Max RxFIFO occupied space before PAUSE
 * ---------------------------------------------------
 * 0x0000	0 bytes
 * 0x0740	29,696 bytes
 * 0x07FF	32,752 bytes
 */
#define		IPG_FLOWONTHRESH_VALUE	0x0740

/* FlowOffThresh specifies the minimum amount of occupied
 * space in the receive FIFO before a PAUSE frame with
 * zero pause time is transmitted.
 *
 * Value	Max RxFIFO occupied space before PAUSE
 * ---------------------------------------------------
 * 0x0000	0 bytes
 * 0x00BF	3056 bytes
 * 0x07FF	32,752 bytes
 */
#define		IPG_FLOWOFFTHRESH_VALUE	0x00BF

/* end ipg_tune.h */
