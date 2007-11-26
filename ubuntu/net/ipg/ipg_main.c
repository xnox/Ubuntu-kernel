/*PCI_DEVICE_ID_IP1000
 *
 * ipg.c
 *
 * IC Plus IP1000 Gigabit Ethernet Adapter Linux Driver v2.01   
 * by IC Plus Corp. 2003                     
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
 * 0.1  11/8/99  Initial revision work begins.
 *
 * 0.2  11/12/99  Basic operation achieved, continuing work.
 *
 * 0.3  11/19/99  MAC Loop Back for sync problem testing.
 *
 * 0.4  12/22/99  ioctl for diagnotic program 'hunter' support.
 *
 * 0.5  4/13/00   Updates to
 *
 * 0.6  6/14/00   Slight correction to handling TFDDONE, and
 *                preservation of PHYCTRL polarity bits.
 *
 * 0.7  7/27/00   Modifications to accomodate triple speed
 *                autonegotiation. Also change to ioctl routine
 *                to handle unknown PHY address.
 *
 * 0.8  8/11/00   Added change_mtu function.
 *
 * 0.9  8/15/00   Corrected autonegotiation resolution.
 *
 * 0.10 8/30/00   Changed constants to use IPG in place
 *                of RIO. Also, removed most of debug
 *                code in preparation for production release.
 *
 * 0.11 8/31/00   Utilize 64 bit data types where appropriate.
 *
 * 0.12 9/1/00    Move some constants to include file and utilize
 *                RxDMAInt register.
 *
 * 0.13 10/31/00  Several minor modifications to improve stability.
 *
 * 0.14 11/28/00  Added call to nic_tx_free if TFD not available.
 *
 * 0.15 12/5/00   Corrected problem with receive errors, always set
 *                receive buffer address to NULL. Release RX buffers
 *                on errors.
 *
 * 0.16 12/20/00  Corrected autoneg resolution issue, must detect
 *                speed via PHYCTRL register. Also, perform only 1
 *                loop in the nic_txcleanup routine.
 *
 * 0.17 2/7/01    Changed all references of ST2021 to IPG.
 *                When next TFD not available, return -ENOMEM instead
 *                of 0. Removed references to RUBICON.
 *
 * 0.18 2/14/01   Corrected problem when unexpected jumbo frames are
 *                received (now dropped properly.) Changed
 *                "DROP_ON_ERRORS" breaking out Ethernet errors and
 *                TCP/IP errors serparately. Corrected Gigabit
 *                copper PAUSE autonegotiation.
 *
 * 0.19 2/22/01   Changed interrupt handling of RFD_LIST_END,
 *                INT_REQUESTED, and RX_DMA_COMPLETE. Masked off
 *                RMON statistics and unused MIB statistics.
 *                Make sure *all* statistics are accounted for
 *                (either masked or read in get_stats) to avoid
 *                perpetual UpdateStats interrupt from causing
 *                driver to crash.
 *
 * 0.20 3/2/01    Corrected error in nic_stop. Need to set
 *                TxBuff[] = NULL after freeing TxBuff and
 *                RxBuff[] = NULL after freeing RxBuff.
 *
 * 0.21 3/5/01    Correct 10/100Mbit PAUSE autonegotiation.
 *
 * 0.22 3/16/01   Used TxDMAIndicate for 100/1000Mbps modes. Add
 *                "TFD unavailable" and "RFD list end" counters
 *                to assist with performance measurement. Added
 *                check for maxtfdcnt != 0 to while loop within
 *                txcleanup.
 *
 * 0.23 3/22/01   Set the CurrentTxFrameID to 1 upon detecting
 *                a TxDMAComplete to reduce the number of TxDMAComplete.
 *                Also, indicate IP/TCP/UDP checksum is unneseccary
 *                if IPG indicates checksum validates.
 *
 * 0.24 3/23/01   Changed the txfree routine, eliminating the margin
 *                between the last freed TFD and the current TFD.
 *
 * 0.25 4/3/01    Corrected errors in config_autoneg to deal with
 *                fiber flag properly.
 *
 * 0.26 5/1/01    Port for operation with Linux 2.2 or 2.4 kernel.
 *
 * 0.27 5/22/01   Cleaned up some extraneous comments.
 *
 * 0.28 6/20/01   Added auto IP, TCP, and UDP checksum addition
 *                on transmit based on compilation option.
 *
 * 0.29 7/26/01   Comment out #include <asm/spinlock.h> from ipg.h
 *                for compatibility with RedHat 7.1. Unkown reason.
 *
 * 0.30 8/10/01   Added debug message to each function, print function
 *                name when entered. Added DEBUGCTRL register bit 5 for
 *                Rx DMA Poll Now bug work around. Added ifdef IPG_DEBUG
 *                flags to IPG_TFDlistunabvail and IPG_RFDlistend
 *                counters. Removed clearing of sp->stat struct from
 *                nic_open and added check in get_stats to make sure
 *                NIC is initialized before reading statistic registers.
 *                Corrected erroneous MACCTRL setting for Fiber based
 *                10/100 boards. Corrected storage of phyctrlpolarity
 *                variable.
 *
 * 0.31 8/13/01   Incorporate STI or TMI fiber based NIC detection.
 *                Corrected problem with _pciremove_linux2_4 routine.
 *                Corrected setting of IP/TCP/UDP checksumming on receive
 *                and transmit.
 *
 * 0.32 8/15/01   Changed the tmi_fiber_detect routine.
 *
 * 0.33 8/16/01   Changed PHY reset method in nic_open routine. Added
 *                a chip reset in nic_stop to shut down the IPG.
 *
 * 0.34 9/5/01    Corrected some misuage of dev_kfree_skb.
 *
 * 0.35 10/30/01  Unmap register space (IO or memory) in the nic_stop
 *                routine instead of in the cleanup or remove routines.
 *                Corrects driver up/down/up problem when using IO
 *                register mapping.
 *
 * 0.36 10/31/01  Modify the constant IPG_FRAMESBETWEENTXDMACOMPLETES
 *		  from 0x10 to 1.
 * 0.37 11/05/03  Modify the IPG_PHY_1000BASETCONTROL
 *				  in IP1000A this register is without 1000BPS Ability by default
 *				  so enable 1000BPS ability before PHY RESET/RESTART_AN
 * 0.38 11/05/03  update ipg_config_autoneg routine
 * 0.39 11/05/03  add Vendor_ID=13F0/Device_ID=1023 into support_cards
 * 2.05 10/16/04  Remove IPG_IE_RFD_LIST_END for pass SmartBit test.
 *                (see 20041019Jesse_For_SmartBit.)
 * 2.06 10/27/04 Support for kernel 2.6.x
 * 2.06a 11/03/04 remove some compile warring message.
 * 2.09b 06/03/05 Support 4k jumbo  (IC Plus, Jesse)
 * 2.09d 06/22/05 Support 10k jumbo, more than 4k will using copy mode (IC Plus, Jesse)
 */
#define JUMBO_FRAME_4k_ONLY
enum {
	netdev_io_size = 128
};

#include "ipg.h"
#define DRV_NAME	"ipg"
/* notify kernel that this software is under GNU Public License */
#if ( LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,10) )
    MODULE_LICENSE("GPL");
#endif

/* Function prototypes. */
u16     read_phy_register(IPG_DEVICE_TYPE *ipg_ethernet_device,
                          int phy_address, int phy_register);
//int     ipg_reset(u32 baseaddr, u32 resetflags); //JES20040127EEPROM: change type of param1
int     ipg_reset(IPG_DEVICE_TYPE *ipg_ethernet_device, u32 resetflags);
int     ipg_io_config(IPG_DEVICE_TYPE *ipg_ethernet_device);
static  irqreturn_t  ipg_interrupt_handler(int ipg_irq, void *device_instance);

void    ipg_nic_txcleanup(IPG_DEVICE_TYPE *ipg_ethernet_device);
void	ipg_nic_txfree(IPG_DEVICE_TYPE *ipg_ethernet_device);
int     ipg_nic_open(IPG_DEVICE_TYPE *ipg_ethernet_device);
int     ipg_nic_stop(IPG_DEVICE_TYPE *ipg_ethernet_device);
int     ipg_nic_hard_start_xmit(struct sk_buff *skb,
                                   IPG_DEVICE_TYPE *ipg_ethernet_device);
IPG_STATS_TYPE* ipg_nic_get_stats(IPG_DEVICE_TYPE
                                        *ipg_ethernet_device);
void ipg_nic_set_multicast_list(IPG_DEVICE_TYPE *dev);
int ipg_nic_init(IPG_DEVICE_TYPE *ipg_ethernet_device);
int ipg_nic_rx(IPG_DEVICE_TYPE *ipg_ethernet_device);
int ipg_nic_rxrestore(IPG_DEVICE_TYPE *ipg_ethernet_device);
int init_rfdlist(IPG_DEVICE_TYPE *ipg_ethernet_device);
int init_tfdlist(IPG_DEVICE_TYPE *ipg_ethernet_device);
int ipg_get_rxbuff(IPG_DEVICE_TYPE *ipg_ethernet_device, int rfd);
int ipg_sti_fiber_detect(IPG_DEVICE_TYPE *ipg_ethernet_device);
int ipg_tmi_fiber_detect(IPG_DEVICE_TYPE *ipg_ethernet_device,
                            int phyaddr);
int ipg_config_autoneg(IPG_DEVICE_TYPE *ipg_ethernet_device);
unsigned ether_crc_le(int length, unsigned char *data);
int ipg_nic_do_ioctl(IPG_DEVICE_TYPE *ipg_ethernet_device,
                        struct ifreq *req, int cmd);
int ipg_nic_change_mtu(IPG_DEVICE_TYPE *ipg_ethernet_device,
                          int new_mtu);

#ifdef IPG_LINUX2_2
int ipg_pcibussearch_linux2_2(void);
#endif

#ifdef IPG_LINUX2_4
int ipg_pciprobe_linux2_4(struct pci_dev*, const struct pci_device_id*);
void ipg_pciremove_linux2_4(struct pci_dev*);
#endif

void bSetPhyDefaultParam(unsigned char Rev,
		IPG_DEVICE_TYPE *ipg_ethernet_device,int phy_address);

//JES20040127EEPROM:Add three new function
int read_eeprom(IPG_DEVICE_TYPE *ipg_ethernet_device, int eep_addr);
void Set_LED_Mode(IPG_DEVICE_TYPE *ipg_ethernet_device);
void Set_PHYSet(IPG_DEVICE_TYPE *ipg_ethernet_device);
/* End function prototypes. */

#ifdef IPG_DEBUG
void ipg_dump_rfdlist(IPG_DEVICE_TYPE *ipg_ethernet_device)
{
	u32				baseaddr;
	int				i;
	u32				offset;
	struct ipg_nic_private	*sp = (struct ipg_nic_private *)
					      ipg_ethernet_device->priv;

	IPG_DEBUG_MSG("_dump_rfdlist\n");

	baseaddr = ipg_ethernet_device->base_addr;

	printk(KERN_INFO "CurrentRFD         = %2.2x\n", sp->CurrentRFD);
	printk(KERN_INFO "LastRestoredRxBuff = %2.2x\n",
	       sp->LastRestoredRxBuff);
#ifdef IPG_LINUX2_2
	printk(KERN_INFO "RFDList start address = %16.16lx\n",
	       (unsigned long int)(IPG_HOST2BUS_MAP(sp->RFDList)));
#endif
#ifdef IPG_LINUX2_4
	printk(KERN_INFO "RFDList start address = %16.16lx\n",
	       (unsigned long int)(sp->RFDListDMAhandle));
#endif
	printk(KERN_INFO "RFDListPtr register   = %8.8x%8.8x\n",
	       IPG_READ_RFDLISTPTR1(baseaddr),
	       IPG_READ_RFDLISTPTR0(baseaddr));

	for(i=0;i<IPG_RFDLIST_LENGTH;i++)
	{
		offset = (u32)(&sp->RFDList[i].RFDNextPtr)
		         - (u32)(sp->RFDList);
		printk(KERN_INFO "%2.2x %4.4x RFDNextPtr = %16.16lx\n", i,
		       offset, (unsigned long int) sp->RFDList[i].RFDNextPtr);
		offset = (u32)(&sp->RFDList[i].RFS)
		         - (u32)(sp->RFDList);
		printk(KERN_INFO "%2.2x %4.4x RFS        = %16.16lx\n", i,
		       offset, (unsigned long int)sp->RFDList[i].RFS);
		offset = (u32)(&sp->RFDList[i].FragInfo)
		         - (u32)(sp->RFDList);
		printk(KERN_INFO "%2.2x %4.4x FragInfo   = %16.16lx\n", i,
		       offset, (unsigned long int)sp->RFDList[i].FragInfo);
	}

	return;
}

void ipg_dump_tfdlist(IPG_DEVICE_TYPE	*ipg_ethernet_device)
{
	u32				baseaddr;
	int				i;
	u32				offset;
	struct ipg_nic_private	*sp = (struct ipg_nic_private *)
					      ipg_ethernet_device->priv;

	IPG_DEBUG_MSG("_dump_tfdlist\n");

	baseaddr = ipg_ethernet_device->base_addr;

	printk(KERN_INFO "CurrentTFD         = %2.2x\n", sp->CurrentTFD);
	printk(KERN_INFO "LastFreedTxBuff = %2.2x\n",
	       sp->LastFreedTxBuff);
#ifdef IPG_LINUX2_2
	printk(KERN_INFO "TFDList start address = %16.16lx\n",
	       (unsigned long int)(IPG_HOST2BUS_MAP(sp->TFDList)));
#endif
#ifdef IPG_LINUX2_4
	printk(KERN_INFO "TFDList start address = %16.16lx\n",
	       (unsigned long int)(sp->TFDListDMAhandle));
#endif
	printk(KERN_INFO "TFDListPtr register   = %8.8x%8.8x\n",
	       IPG_READ_TFDLISTPTR1(baseaddr),
	       IPG_READ_TFDLISTPTR0(baseaddr));

	for(i=0;i<IPG_TFDLIST_LENGTH;i++)
	{
		offset = (u32)(&sp->TFDList[i].TFDNextPtr)
		         - (u32)(sp->TFDList);
		printk(KERN_INFO "%2.2x %4.4x TFDNextPtr = %16.16lx\n", i,
		       offset, (unsigned long int)sp->TFDList[i].TFDNextPtr);

		offset = (u32)(&sp->TFDList[i].TFC)
		         - (u32)(sp->TFDList);
		printk(KERN_INFO "%2.2x %4.4x TFC        = %16.16lx\n", i,
		       offset, (unsigned long int)sp->TFDList[i].TFC);
		offset = (u32)(&sp->TFDList[i].FragInfo)
		         - (u32)(sp->TFDList);
		printk(KERN_INFO "%2.2x %4.4x FragInfo   = %16.16lx\n", i,
		       offset, (unsigned long int)sp->TFDList[i].FragInfo);
	}

	return;
}
#else /* Not in debug mode. */
#endif

void send_three_state(u32 baseaddr, u8 phyctrlpolarity)
{
	IPG_WRITE_PHYCTRL(baseaddr, IPG_PC_MGMTCLK_LO |
							(IPG_PC_MGMTDATA & 0) |
		                     IPG_PC_MGMTDIR | phyctrlpolarity);

	mdelay(IPG_PC_PHYCTRLWAIT);

	IPG_WRITE_PHYCTRL(baseaddr, IPG_PC_MGMTCLK_HI |
		                     (IPG_PC_MGMTDATA & 0) |
		                     IPG_PC_MGMTDIR | phyctrlpolarity);

	mdelay(IPG_PC_PHYCTRLWAIT);
	return;
}

void send_end(u32 baseaddr, u8 phyctrlpolarity)
{
	IPG_WRITE_PHYCTRL(baseaddr, IPG_PC_MGMTCLK_LO |
							(IPG_PC_MGMTDATA & 0) |
		                     IPG_PC_MGMTDIR | phyctrlpolarity);
	return;
}


u16 read_phy_bit(u32 baseaddr, u8 phyctrlpolarity)
{   u16 bit_data;
	IPG_WRITE_PHYCTRL(baseaddr, IPG_PC_MGMTCLK_LO |
		                     phyctrlpolarity);

	mdelay(IPG_PC_PHYCTRLWAIT);

	bit_data=((IPG_READ_PHYCTRL(baseaddr) & IPG_PC_MGMTDATA) >> 1) & 1;

	IPG_WRITE_PHYCTRL(baseaddr, IPG_PC_MGMTCLK_HI |
		                     phyctrlpolarity);

	mdelay(IPG_PC_PHYCTRLWAIT);
	return bit_data;
}

u16	read_phy_register(IPG_DEVICE_TYPE *ipg_ethernet_device,
	                  int phy_address, int phy_register)
{
	/* Read a register from the Physical Layer device located
	 * on the IPG NIC, using the IPG PHYCTRL register.
	 */

	u32	baseaddr;
	int	i;
	int	j;
	int	fieldlen[8];
	u32	field[8];
	u8	databit;
        u8      phyctrlpolarity;

	IPG_DEBUG_MSG("read_phy_register\n");

	baseaddr = ipg_ethernet_device->base_addr;

	/* The GMII mangement frame structure for a read is as follows:
	 *
	 * |Preamble|st|op|phyad|regad|ta|      data      |idle|
	 * |< 32 1s>|01|10|AAAAA|RRRRR|z0|DDDDDDDDDDDDDDDD|z   |
	 *
	 * <32 1s> = 32 consecutive logic 1 values
	 * A = bit of Physical Layer device address (MSB first)
	 * R = bit of register address (MSB first)
	 * z = High impedance state
	 * D = bit of read data (MSB first)
	 *
	 * Transmission order is 'Preamble' field first, bits transmitted
	 * left to right (first to last).
	 */

	field[0]    = GMII_PREAMBLE;
	fieldlen[0] = 32;		/* Preamble */
	field[1]    = GMII_ST;
	fieldlen[1] = 2;		/* ST */
	field[2]    = GMII_READ;
	fieldlen[2] = 2;		/* OP */
	field[3]    = phy_address;
	fieldlen[3] = 5;		/* PHYAD */
	field[4]    = phy_register;
	fieldlen[4] = 5;		/* REGAD */
	field[5]    = 0x0000;
	fieldlen[5] = 2;		/* TA */
	field[6]    = 0x0000;
	fieldlen[6] = 16;		/* DATA */
	field[7]    = 0x0000;
	fieldlen[7] = 1;		/* IDLE */

        /* Store the polarity values of PHYCTRL. */
        phyctrlpolarity = IPG_READ_PHYCTRL(baseaddr) &
                          (IPG_PC_DUPLEX_POLARITY |
	                   IPG_PC_LINK_POLARITY);

	/* Create the Preamble, ST, OP, PHYAD, and REGAD field. */
	for(j=0; j<5; j++)
	for(i=0; i<fieldlen[j]; i++)
	{
		/* For each variable length field, the MSB must be
		 * transmitted first. Rotate through the field bits,
		 * starting with the MSB, and move each bit into the
		 * the 1st (2^1) bit position (this is the bit position
		 * corresponding to the MgmtData bit of the PhyCtrl
		 * register for the IPG).
		 *
		 * Example: ST = 01;
		 *
		 *          First write a '0' to bit 1 of the PhyCtrl
		 *          register, then write a '1' to bit 1 of the
		 *          PhyCtrl register.
		 *
		 * To do this, right shift the MSB of ST by the value:
		 * [field length - 1 - #ST bits already written]
		 * then left shift this result by 1.
		 */
		databit = (field[j] >> (fieldlen[j] - 1 - i)) << 1;

		IPG_WRITE_PHYCTRL(baseaddr, IPG_PC_MGMTCLK_LO |
		                     (IPG_PC_MGMTDATA & databit) |
		                     IPG_PC_MGMTDIR | phyctrlpolarity);

		mdelay(IPG_PC_PHYCTRLWAIT);

		IPG_WRITE_PHYCTRL(baseaddr, IPG_PC_MGMTCLK_HI |
		                     (IPG_PC_MGMTDATA & databit) |
		                     IPG_PC_MGMTDIR | phyctrlpolarity);

		mdelay(IPG_PC_PHYCTRLWAIT);
	}

	send_three_state(baseaddr, phyctrlpolarity);

	read_phy_bit(baseaddr, phyctrlpolarity);

	/* For a read cycle, the bits for the next two fields (TA and
	 * DATA) are driven by the PHY (the IPG reads these bits).
	 */
//	for(j=6; j<8; j++)
	for(i=0; i<fieldlen[6]; i++)
	{
		field[6] |= (read_phy_bit(baseaddr, phyctrlpolarity) << (fieldlen[6]- 1 - i));

	}

	send_three_state(baseaddr, phyctrlpolarity);
	send_three_state(baseaddr, phyctrlpolarity);
	send_three_state(baseaddr, phyctrlpolarity);
	send_end(baseaddr, phyctrlpolarity);

	/* Return the value of the DATA field. */
	return field[6];
}

void	write_phy_register(IPG_DEVICE_TYPE *ipg_ethernet_device,
	                  int phy_address, int phy_register, u16 writeval)
{
	/* Write to a register from the Physical Layer device located
	 * on the IPG NIC, using the IPG PHYCTRL register.
	 */

	u32	baseaddr;
	int	i;
	int	j;
	int	fieldlen[8];
	u32	field[8];
	u8	databit;
	u8	phyctrlpolarity;

	IPG_DEBUG_MSG("write_phy_register\n");

	baseaddr = ipg_ethernet_device->base_addr;

	/* The GMII mangement frame structure for a read is as follows:
	 *
	 * |Preamble|st|op|phyad|regad|ta|      data      |idle|
	 * |< 32 1s>|01|10|AAAAA|RRRRR|z0|DDDDDDDDDDDDDDDD|z   |
	 *
	 * <32 1s> = 32 consecutive logic 1 values
	 * A = bit of Physical Layer device address (MSB first)
	 * R = bit of register address (MSB first)
	 * z = High impedance state
	 * D = bit of write data (MSB first)
	 *
	 * Transmission order is 'Preamble' field first, bits transmitted
	 * left to right (first to last).
	 */

	field[0]    = GMII_PREAMBLE;
	fieldlen[0] = 32;		/* Preamble */
	field[1]    = GMII_ST;
	fieldlen[1] = 2;		/* ST */
	field[2]    = GMII_WRITE;
	fieldlen[2] = 2;		/* OP */
	field[3]    = phy_address;
	fieldlen[3] = 5;		/* PHYAD */
	field[4]    = phy_register;
	fieldlen[4] = 5;		/* REGAD */
	field[5]    = 0x0002;
	fieldlen[5] = 2;		/* TA */
	field[6]    = writeval;
	fieldlen[6] = 16;		/* DATA */
	field[7]    = 0x0000;
	fieldlen[7] = 1;		/* IDLE */

        /* Store the polarity values of PHYCTRL. */
        phyctrlpolarity = IPG_READ_PHYCTRL(baseaddr) &
                          (IPG_PC_DUPLEX_POLARITY |
	                   IPG_PC_LINK_POLARITY);

	/* Create the Preamble, ST, OP, PHYAD, and REGAD field. */
	for(j=0; j<7; j++)
	for(i=0; i<fieldlen[j]; i++)
	{
		/* For each variable length field, the MSB must be
		 * transmitted first. Rotate through the field bits,
		 * starting with the MSB, and move each bit into the
		 * the 1st (2^1) bit position (this is the bit position
		 * corresponding to the MgmtData bit of the PhyCtrl
		 * register for the IPG).
		 *
		 * Example: ST = 01;
		 *
		 *          First write a '0' to bit 1 of the PhyCtrl
		 *          register, then write a '1' to bit 1 of the
		 *          PhyCtrl register.
		 *
		 * To do this, right shift the MSB of ST by the value:
		 * [field length - 1 - #ST bits already written]
		 * then left shift this result by 1.
		 */
		databit = (field[j] >> (fieldlen[j] - 1 - i)) << 1;

		IPG_WRITE_PHYCTRL(baseaddr, IPG_PC_MGMTCLK_LO |
		                     (IPG_PC_MGMTDATA & databit) |
		                     IPG_PC_MGMTDIR | phyctrlpolarity);

		mdelay(IPG_PC_PHYCTRLWAIT);

		IPG_WRITE_PHYCTRL(baseaddr, IPG_PC_MGMTCLK_HI |
		                     (IPG_PC_MGMTDATA & databit) |
		                     IPG_PC_MGMTDIR | phyctrlpolarity);

		mdelay(IPG_PC_PHYCTRLWAIT);
	}

	/* The last cycle is a tri-state, so read from the PHY.
	 */
	for(j=7; j<8; j++)
	for(i=0; i<fieldlen[j]; i++)
	{
		IPG_WRITE_PHYCTRL(baseaddr, IPG_PC_MGMTCLK_LO |
		                     phyctrlpolarity);

		mdelay(IPG_PC_PHYCTRLWAIT);

		field[j] |= ((IPG_READ_PHYCTRL(baseaddr) &
		             IPG_PC_MGMTDATA) >> 1)
		            << (fieldlen[j] - 1 - i);

		IPG_WRITE_PHYCTRL(baseaddr, IPG_PC_MGMTCLK_HI |
		                     phyctrlpolarity);

		mdelay(IPG_PC_PHYCTRLWAIT);

	}

	return;
}
//int     ipg_reset(u32 baseaddr, u32 resetflags) //JES20040127EEPROM: change type of param1
int	ipg_reset(IPG_DEVICE_TYPE *ipg_ethernet_device, u32 resetflags)
{
	/* Assert functional resets via the IPG AsicCtrl
	 * register as specified by the 'resetflags' input
	 * parameter.
	 */
        u32	baseaddr;//JES20040127EEPROM:	
	int timeout_count;
	baseaddr = ipg_ethernet_device->base_addr;//JES20040127EEPROM:

	IPG_DEBUG_MSG("_reset\n");

	IPG_WRITE_ASICCTRL(baseaddr, IPG_READ_ASICCTRL(baseaddr) |
	               resetflags);

	/* Wait until IPG reset is complete. */
	timeout_count = 0;

	/* Delay added to account for problem with 10Mbps reset. */
	mdelay(IPG_AC_RESETWAIT);

	while (IPG_AC_RESET_BUSY & IPG_READ_ASICCTRL(baseaddr))
	{
		mdelay(IPG_AC_RESETWAIT);
		timeout_count++;
		if (timeout_count > IPG_AC_RESET_TIMEOUT)
			return -ETIME;
	}
   /* Set LED Mode in Asic Control JES20040127EEPROM */
   Set_LED_Mode(ipg_ethernet_device);
        
   /* Set PHYSet Register Value JES20040127EEPROM */
	Set_PHYSet(ipg_ethernet_device);
	return 0;
}

int	ipg_sti_fiber_detect(IPG_DEVICE_TYPE *ipg_ethernet_device)
{
	/* Determine if NIC is fiber based by reading the PhyMedia
	 * bit in the AsicCtrl register.
	 */

	u32				asicctrl;
	u32				baseaddr;

	IPG_DEBUG_MSG("_sti_fiber_detect\n");

	baseaddr = ipg_ethernet_device->base_addr;
	asicctrl = IPG_READ_ASICCTRL(baseaddr);

        if (asicctrl & IPG_AC_PHY_MEDIA)
	{
		/* Fiber NIC. */
		return 1;
	} else
	{
		/* Not a fiber NIC. */
		return 0;
	}
}

int	ipg_tmi_fiber_detect(IPG_DEVICE_TYPE *ipg_ethernet_device,
	                        int phyaddr)
{
	/* Determine if NIC is fiber based by reading the ID register
	 * of the PHY and the GMII address.
	 */

	u16				phyid;
	u32				baseaddr;

	IPG_DEBUG_MSG("_tmi_fiber_detect\n");

	baseaddr = ipg_ethernet_device->base_addr;
	phyid = read_phy_register(ipg_ethernet_device,
	                          phyaddr, GMII_PHY_ID_1);

	IPG_DEBUG_MSG("PHY ID = %x\n", phyid);

	/* We conclude the mode is fiber if the GMII address
	 * is 0x1 and the PHY ID is 0x0000.
	 */
        if ((phyaddr == 0x1) && (phyid == 0x0000))
	{
		/* Fiber NIC. */
		return 1;
	} else
	{
		/* Not a fiber NIC. */
		return 0;
	}
}

int	ipg_find_phyaddr(IPG_DEVICE_TYPE *ipg_ethernet_device)
{
	/* Find the GMII PHY address. */

	int	i;
	int	phyaddr;
	u32	status;

        for(i=0;i<32;i++)
        {
                /* Search for the correct PHY address among 32 possible. */
                phyaddr = (IPG_NIC_PHY_ADDRESS + i) % 32;

				/* 10/22/03 Grace change verify from GMII_PHY_STATUS to
				   GMII_PHY_ID1
				 */

                status = read_phy_register(ipg_ethernet_device,
                                           phyaddr, GMII_PHY_STATUS);

 				// if (status != 0xFFFF)
                if ((status != 0xFFFF) && (status != 0))
                      return phyaddr;

                /*----------------------------------------------------
                status = read_phy_register(ipg_ethernet_device,
                						   phyaddr, GMII_PHY_ID_1);
				if (status == 0x243) {
					printk("PHY Addr = %x\n", phyaddr);
					return phyaddr;
				}
				-------------------------------------------------*/
        }

	return -1;
}

#ifdef NOTGRACE
int	ipg_config_autoneg(IPG_DEVICE_TYPE *ipg_ethernet_device)
{
	/* Configure IPG based on result of IEEE 802.3 PHY
	 * auto-negotiation.
	 */

        int                             phyaddr = 0;
	u8				phyctrl;
	u32				asicctrl;
	u32				baseaddr;
        u16                             status = 0;
	u16				advertisement;
	u16				linkpartner_ability;
	u16				gigadvertisement;
	u16				giglinkpartner_ability;
        u16                             techabilities;
        int                             fiber;
        int                             gig;
        int                             fullduplex;
        int                             txflowcontrol;
        int                             rxflowcontrol;
	struct ipg_nic_private	*sp = (struct ipg_nic_private *)
					      ipg_ethernet_device->priv;

	IPG_DEBUG_MSG("_config_autoneg\n");

	baseaddr = ipg_ethernet_device->base_addr;
	asicctrl = IPG_READ_ASICCTRL(baseaddr);
    phyctrl = IPG_READ_PHYCTRL(baseaddr);

	/* Set flags for use in resolving auto-negotation, assuming
	 * non-1000Mbps, half duplex, no flow control.
	 */
	fiber = 0;
	fullduplex = 0;
	txflowcontrol = 0;
	rxflowcontrol = 0;
	gig = 0;

	/* To accomodate a problem in 10Mbps operation,
	 * set a global flag if PHY running in 10Mbps mode.
	 */
 	sp->tenmbpsmode = 0;

	printk("Link speed = ");

	/* Determine actual speed of operation. */
	switch (phyctrl & IPG_PC_LINK_SPEED)
	{
		case IPG_PC_LINK_SPEED_10MBPS :
			printk("10Mbps.\n");
                	printk(KERN_INFO "%s: 10Mbps operational mode enabled.\n",ipg_ethernet_device->name);
	 		sp->tenmbpsmode = 1;
			break;
		case IPG_PC_LINK_SPEED_100MBPS :
			printk("100Mbps.\n");
			break;
		case IPG_PC_LINK_SPEED_1000MBPS :
			printk("1000Mbps.\n");
			gig = 1;
			break;
		default : printk("undefined!\n");
	}

#ifndef IPG_TMI_FIBER_DETECT
	fiber = ipg_sti_fiber_detect(ipg_ethernet_device);

        /* Determine if auto-negotiation resolution is necessary.
	 * First check for fiber based media 10/100 media.
	 */
        if ((fiber == 1) && (asicctrl &
	    (IPG_AC_PHY_SPEED10 | IPG_AC_PHY_SPEED100)))
        {
                printk(KERN_INFO "%s: Fiber based PHY, setting full duplex, no flow control.\n", ipg_ethernet_device->name);
                return -EILSEQ;
                IPG_WRITE_MACCTRL(baseaddr, (IPG_READ_MACCTRL(baseaddr) |
                                                IPG_MC_DUPLEX_SELECT_FD) &
                                     ~IPG_MC_TX_FLOW_CONTROL_ENABLE &
                                     ~IPG_MC_RX_FLOW_CONTROL_ENABLE);

                return 0;
        }
#endif

        /* Determine if PHY is auto-negotiation capable. */
	phyaddr = ipg_find_phyaddr(ipg_ethernet_device);

        if (phyaddr == -1)
        {
                printk(KERN_INFO "%s: Error on read to GMII/MII Status register.\n",ipg_ethernet_device->name);
                return -EILSEQ;
        }

	IPG_DEBUG_MSG("GMII/MII PHY address = %x\n", phyaddr);

        status = read_phy_register(ipg_ethernet_device,
                                   phyaddr, GMII_PHY_STATUS);

	printk("PHYStatus = %x \n", status);
        if ((status & GMII_PHY_STATUS_AUTONEG_ABILITY) == 0)
        {
                printk(KERN_INFO "%s: Error PHY unable to perform auto-negotiation.\n",
			ipg_ethernet_device->name);
                return -EILSEQ;
        }

	advertisement = read_phy_register(ipg_ethernet_device, phyaddr,
	                                  GMII_PHY_AUTONEGADVERTISEMENT);
	linkpartner_ability = read_phy_register(ipg_ethernet_device, phyaddr,
	                                        GMII_PHY_AUTONEGLINKPARTABILITY);

	printk("PHYadvertisement=%x LinkPartner=%x \n",advertisement,linkpartner_ability);
	if ((advertisement == 0xFFFF) || (linkpartner_ability == 0xFFFF))
	{
		printk(KERN_INFO "%s: Error on read to GMII/MII registers 4 and/or 5.\n", ipg_ethernet_device->name);
		return -EILSEQ;
	}

#ifdef IPG_TMI_FIBER_DETECT
	fiber = ipg_tmi_fiber_detect(ipg_ethernet_device, phyaddr);
#endif

        /* Resolve full/half duplex if 1000BASE-X. */
	if ((gig == 1) && (fiber == 1))
        {
		/* Compare the full duplex bits in the GMII registers
		 * for the local device, and the link partner. If these
		 * bits are logic 1 in both registers, configure the
		 * IPG for full duplex operation.
		 */
		if ((advertisement & GMII_PHY_ADV_FULL_DUPLEX) ==
		    (linkpartner_ability & GMII_PHY_ADV_FULL_DUPLEX))
		{
			fullduplex = 1;

			/* In 1000BASE-X using IPG's internal PCS
			 * layer, so write to the GMII duplex bit.
			 */
			write_phy_register(ipg_ethernet_device,
			                   phyaddr,
			                   GMII_PHY_CONTROL,
			                   read_phy_register(ipg_ethernet_device,
			                   phyaddr,
			                   GMII_PHY_CONTROL) |
			                   GMII_PHY_CONTROL_FULL_DUPLEX);

		} else
		{
			fullduplex = 0;

			/* In 1000BASE-X using IPG's internal PCS
			 * layer, so write to the GMII duplex bit.
			 */
			write_phy_register(ipg_ethernet_device,
			                   phyaddr,
			                   GMII_PHY_CONTROL,
			                   read_phy_register(ipg_ethernet_device,
			                   phyaddr,
			                   GMII_PHY_CONTROL) &
			                   ~GMII_PHY_CONTROL_FULL_DUPLEX);
		}
	}

        /* Resolve full/half duplex if 1000BASE-T. */
	if ((gig == 1) && (fiber == 0))
        {
		/* Read the 1000BASE-T "Control" and "Status"
		 * registers which represent the advertised and
		 * link partner abilities exchanged via next page
		 * transfers.
		 */
		gigadvertisement = read_phy_register(ipg_ethernet_device,
		                                     phyaddr,
	                                         GMII_PHY_1000BASETCONTROL);
		giglinkpartner_ability = read_phy_register(ipg_ethernet_device,
		                                           phyaddr,
	                                          GMII_PHY_1000BASETSTATUS);

		/* Compare the full duplex bits in the 1000BASE-T GMII
		 * registers for the local device, and the link partner.
		 * If these bits are logic 1 in both registers, configure
		 * the IPG for full duplex operation.
		 */
		if ((gigadvertisement & GMII_PHY_1000BASETCONTROL_FULL_DUPLEX) &&
		    (giglinkpartner_ability & GMII_PHY_1000BASETSTATUS_FULL_DUPLEX))
		{
			fullduplex = 1;
		} else
		{
			fullduplex = 0;
		}
	}

        /* Resolve full/half duplex for 10/100BASE-T. */
	if (gig == 0)
	{
	        /* Autonegotiation Priority Resolution algorithm, as defined in
	       	 * IEEE 802.3 Annex 28B.
	       	 */
	        if (((advertisement & MII_PHY_SELECTORFIELD) ==
	             MII_PHY_SELECTOR_IEEE8023) &&
	            ((linkpartner_ability & MII_PHY_SELECTORFIELD) ==
	             MII_PHY_SELECTOR_IEEE8023))
	        {
	                techabilities = (advertisement & linkpartner_ability &
	                                 MII_PHY_TECHABILITYFIELD);

	                fullduplex = 0;

	                /* 10BASE-TX half duplex is lowest priority. */
	                if (techabilities & MII_PHY_TECHABILITY_10BT)
	                {
	                        fullduplex = 0;
	                }

	                if (techabilities & MII_PHY_TECHABILITY_10BTFD)
	                {
	                        fullduplex = 1;
	                }

	                if (techabilities & MII_PHY_TECHABILITY_100BTX)
	                {
	                        fullduplex = 0;
	                }

	                if (techabilities & MII_PHY_TECHABILITY_100BT4)
	                {
	                        fullduplex = 0;
	                }

	                /* 100BASE-TX half duplex is highest priority. */ //Sorbica full duplex ?
	                if (techabilities & MII_PHY_TECHABILITY_100BTXFD)
	                {
	                        fullduplex = 1;
	                }

	                if (fullduplex == 1)
	                {
	                        /* If in full duplex mode, determine if PAUSE
	                         * functionality is supported by the local
				 * device, and the link partner.
	                         */
	                        if (techabilities & MII_PHY_TECHABILITY_PAUSE)
	                        {
					txflowcontrol = 1;
					rxflowcontrol = 1;
	                        }
	                        else
	                        {
					txflowcontrol = 0;
					rxflowcontrol = 0;
	                        }
	                }
	        }
	}

	/* If in 1000Mbps, fiber, and full duplex mode, resolve
	 * 1000BASE-X PAUSE capabilities. */
	if ((fullduplex == 1) && (fiber == 1) && (gig == 1))
	{
		/* In full duplex mode, resolve PAUSE
		 * functionality.
		 */
		switch(((advertisement & GMII_PHY_ADV_PAUSE) >> 5) |
		       ((linkpartner_ability & GMII_PHY_ADV_PAUSE) >> 7))
		{
			case 0x7 :
			txflowcontrol = 1;
			rxflowcontrol = 0;
			break;

			case 0xA : case 0xB: case 0xE: case 0xF:
			txflowcontrol = 1;
			rxflowcontrol = 1;
			break;

			case 0xD :
			txflowcontrol = 0;
			rxflowcontrol = 1;
			break;

			default :
			txflowcontrol = 0;
			rxflowcontrol = 0;
		}
	}

	/* If in 1000Mbps, non-fiber, full duplex mode, resolve
	 * 1000BASE-T PAUSE capabilities. */
	if ((fullduplex == 1) && (fiber == 0) && (gig == 1))
	{
		/* Make sure the PHY is advertising we are PAUSE
		 * capable.
		 */
		if (!(advertisement & (MII_PHY_TECHABILITY_PAUSE |
		                       MII_PHY_TECHABILITY_ASM_DIR)))
		{
			/* PAUSE is not being advertised. Advertise
			 * PAUSE and restart auto-negotiation.
			 */
			write_phy_register(ipg_ethernet_device,
		                           phyaddr,
			                   MII_PHY_AUTONEGADVERTISEMENT,
			                   (advertisement |
                                            MII_PHY_TECHABILITY_PAUSE |
			                    MII_PHY_TECHABILITY_ASM_DIR));
			write_phy_register(ipg_ethernet_device,
		                           phyaddr,
			                   MII_PHY_CONTROL,
			                   MII_PHY_CONTROL_RESTARTAN);

			return -EAGAIN;
		}

		/* In full duplex mode, resolve PAUSE
		 * functionality.
		 */
		switch(((advertisement &
		         MII_PHY_TECHABILITY_PAUSE_FIELDS) >> 0x8) |
		       ((linkpartner_ability &
		         MII_PHY_TECHABILITY_PAUSE_FIELDS) >> 0xA))
		{
			case 0x7 :
			txflowcontrol = 1;
			rxflowcontrol = 0;
			break;

			case 0xA : case 0xB: case 0xE: case 0xF:
			txflowcontrol = 1;
			rxflowcontrol = 1;
			break;

			case 0xD :
			txflowcontrol = 0;
			rxflowcontrol = 1;
			break;

			default :
			txflowcontrol = 0;
			rxflowcontrol = 0;
		}
	}

	/* If in 10/100Mbps, non-fiber, full duplex mode, assure
	 * 10/100BASE-T PAUSE capabilities are advertised. */
	if ((fullduplex == 1) && (fiber == 0) && (gig == 0))
	{
		/* Make sure the PHY is advertising we are PAUSE
		 * capable.
		 */
		if (!(advertisement & (MII_PHY_TECHABILITY_PAUSE)))
		{
			/* PAUSE is not being advertised. Advertise
			 * PAUSE and restart auto-negotiation.
			 */
			write_phy_register(ipg_ethernet_device,
		                           phyaddr,
			                   MII_PHY_AUTONEGADVERTISEMENT,
			                   (advertisement |
                                            MII_PHY_TECHABILITY_PAUSE));
			write_phy_register(ipg_ethernet_device,
		                           phyaddr,
			                   MII_PHY_CONTROL,
			                   MII_PHY_CONTROL_RESTARTAN);

			return -EAGAIN;
		}

	}

        if (fiber == 1)
	{
        	printk(KERN_INFO "%s: Fiber based PHY, ",
		       ipg_ethernet_device->name);
	} else
	{
        	printk(KERN_INFO "%s: Copper based PHY, ",
		       ipg_ethernet_device->name);
	}

	/* Configure full duplex, and flow control. */
	if (fullduplex == 1)
	{
		/* Configure IPG for full duplex operation. */
        	printk("setting full duplex, ");

		IPG_WRITE_MACCTRL(baseaddr, IPG_READ_MACCTRL(baseaddr) |
	                             IPG_MC_DUPLEX_SELECT_FD);

		if (txflowcontrol == 1)
		{
        		printk("TX flow control");
			IPG_WRITE_MACCTRL(baseaddr,
			                     IPG_READ_MACCTRL(baseaddr) |
			                     IPG_MC_TX_FLOW_CONTROL_ENABLE);
		} else
		{
        		printk("no TX flow control");
			IPG_WRITE_MACCTRL(baseaddr,
			                     IPG_READ_MACCTRL(baseaddr) &
			                     ~IPG_MC_TX_FLOW_CONTROL_ENABLE);
		}

		if (rxflowcontrol == 1)
		{
        		printk(", RX flow control.");
			IPG_WRITE_MACCTRL(baseaddr,
			                     IPG_READ_MACCTRL(baseaddr) |
			                     IPG_MC_RX_FLOW_CONTROL_ENABLE);
		} else
		{
        		printk(", no RX flow control.");
			IPG_WRITE_MACCTRL(baseaddr,
			                     IPG_READ_MACCTRL(baseaddr) &
			                     ~IPG_MC_RX_FLOW_CONTROL_ENABLE);
		}

		printk("\n");
	} else
	{
		/* Configure IPG for half duplex operation. */
	        printk("setting half duplex, no TX flow control, no RX flow control.\n");

		IPG_WRITE_MACCTRL(baseaddr, IPG_READ_MACCTRL(baseaddr) &
		                     ~IPG_MC_DUPLEX_SELECT_FD &
		                     ~IPG_MC_TX_FLOW_CONTROL_ENABLE &
		                     ~IPG_MC_RX_FLOW_CONTROL_ENABLE);
	}


	IPG_DEBUG_MSG("G/MII reg 4 (advertisement) = %4.4x\n", advertisement);
	IPG_DEBUG_MSG("G/MII reg 5 (link partner)  = %4.4x\n", linkpartner_ability);
	IPG_DEBUG_MSG("G/MII reg 9 (1000BASE-T control) = %4.4x\n", advertisement);
	IPG_DEBUG_MSG("G/MII reg 10 (1000BASE-T status) = %4.4x\n", linkpartner_ability);

	IPG_DEBUG_MSG("Auto-neg complete, MACCTRL = %8.8x\n",
	          IPG_READ_MACCTRL(baseaddr));

	return 0;
}
#else
int	ipg_config_autoneg(IPG_DEVICE_TYPE *ipg_ethernet_device)
{
	/* Configure IPG based on result of IEEE 802.3 PHY
	 * auto-negotiation.
	 */

//    int                             phyaddr = 0;
	u8				phyctrl;
	u32				asicctrl;
	u32				baseaddr;
//    u16                             status = 0;
//	u16				advertisement;
//	u16				linkpartner_ability;
//	u16				gigadvertisement;
//	u16				giglinkpartner_ability;
//        u16                             techabilities;
        int                             fiber;
        int                             gig;
        int                             fullduplex;
        int                             txflowcontrol;
        int                             rxflowcontrol;
	struct ipg_nic_private	*sp = (struct ipg_nic_private *)
					      ipg_ethernet_device->priv;

	IPG_DEBUG_MSG("_config_autoneg\n");

	baseaddr = ipg_ethernet_device->base_addr;
	asicctrl = IPG_READ_ASICCTRL(baseaddr);
        phyctrl = IPG_READ_PHYCTRL(baseaddr);

	/* Set flags for use in resolving auto-negotation, assuming
	 * non-1000Mbps, half duplex, no flow control.
	 */
	fiber = 0;
	fullduplex = 0;
	txflowcontrol = 0;
	rxflowcontrol = 0;
	gig = 0;

	/* To accomodate a problem in 10Mbps operation,
	 * set a global flag if PHY running in 10Mbps mode.
	 */
 	sp->tenmbpsmode = 0;

	printk("Link speed = ");

	/* Determine actual speed of operation. */
	switch (phyctrl & IPG_PC_LINK_SPEED)
	{
		case IPG_PC_LINK_SPEED_10MBPS :
			printk("10Mbps.\n");
                	printk(KERN_INFO "%s: 10Mbps operational mode enabled.\n",ipg_ethernet_device->name);
	 		sp->tenmbpsmode = 1;
			break;
		case IPG_PC_LINK_SPEED_100MBPS :
			printk("100Mbps.\n");
			break;
		case IPG_PC_LINK_SPEED_1000MBPS :
			printk("1000Mbps.\n");
			gig = 1;
			break;
		default : printk("undefined!\n");
				  return 0;
	}

	if ( phyctrl & IPG_PC_DUPLEX_STATUS)
	{
		fullduplex = 1;
		txflowcontrol = 1;
		rxflowcontrol = 1;
	}
	else
	{
		fullduplex = 0;
		txflowcontrol = 0;
		rxflowcontrol = 0;
	}


	/* Configure full duplex, and flow control. */
	if (fullduplex == 1)
	{
		/* Configure IPG for full duplex operation. */
        printk("setting full duplex, ");

		IPG_WRITE_MACCTRL(baseaddr, IPG_READ_MACCTRL(baseaddr) |
	                             IPG_MC_DUPLEX_SELECT_FD);

		if (txflowcontrol == 1)
		{
        	printk("TX flow control");
			IPG_WRITE_MACCTRL(baseaddr,
			                     IPG_READ_MACCTRL(baseaddr) |
			                     IPG_MC_TX_FLOW_CONTROL_ENABLE);
		} else
		{
        	printk("no TX flow control");
			IPG_WRITE_MACCTRL(baseaddr,
			                     IPG_READ_MACCTRL(baseaddr) &
			                     ~IPG_MC_TX_FLOW_CONTROL_ENABLE);
		}

		if (rxflowcontrol == 1)
		{
        		printk(", RX flow control.");
			IPG_WRITE_MACCTRL(baseaddr,
			                     IPG_READ_MACCTRL(baseaddr) |
			                     IPG_MC_RX_FLOW_CONTROL_ENABLE);
		} else
		{
        		printk(", no RX flow control.");
			IPG_WRITE_MACCTRL(baseaddr,
			                     IPG_READ_MACCTRL(baseaddr) &
			                     ~IPG_MC_RX_FLOW_CONTROL_ENABLE);
		}

		printk("\n");
	} else
	{
		/* Configure IPG for half duplex operation. */
	        printk("setting half duplex, no TX flow control, no RX flow control.\n");

		IPG_WRITE_MACCTRL(baseaddr, IPG_READ_MACCTRL(baseaddr) &
		                     ~IPG_MC_DUPLEX_SELECT_FD &
		                     ~IPG_MC_TX_FLOW_CONTROL_ENABLE &
		                     ~IPG_MC_RX_FLOW_CONTROL_ENABLE);
	}
	return 0;
}

#endif

int	ipg_io_config(IPG_DEVICE_TYPE *ipg_ethernet_device)
{
	/* Initialize the IPG I/O registers. */

	u32		baseaddr;
	u32		origmacctrl;
	u32		restoremacctrl;

	IPG_DEBUG_MSG("_io_config\n");

	baseaddr = ipg_ethernet_device->base_addr;

	/* Save the original value of MACCTRL. */
	origmacctrl = IPG_READ_MACCTRL(baseaddr);

	/* Establish a vlaue to restore MACCTRL when done. */
	restoremacctrl = origmacctrl;

	/* Enable statistics gathering. */
	restoremacctrl |= IPG_MC_STATISTICS_ENABLE;

        /* Based on compilation option, determine if FCS is to be
         * stripped on receive frames by IPG.
         */
        if (!(IPG_STRIP_FCS_ON_RX))
        {
		restoremacctrl |= IPG_MC_RCV_FCS;
        }

	/* Determine if transmitter and/or receiver are
	 * enabled so we may restore MACCTRL correctly.
	 */
	if (origmacctrl & IPG_MC_TX_ENABLED)
	{
		restoremacctrl |= IPG_MC_TX_ENABLE;
	}

	if (origmacctrl & IPG_MC_RX_ENABLED)
	{
		restoremacctrl |= IPG_MC_RX_ENABLE;
	}

	/* Transmitter and receiver must be disabled before setting
	 * IFSSelect.
	 */
	IPG_WRITE_MACCTRL(baseaddr, origmacctrl & (IPG_MC_RX_DISABLE |
	                     IPG_MC_TX_DISABLE));

	/* Now that transmitter and receiver are disabled, write
	 * to IFSSelect.
	 */
	IPG_WRITE_MACCTRL(baseaddr, origmacctrl & IPG_MC_IFS_96BIT);

	/* Set RECEIVEMODE register. */
	ipg_nic_set_multicast_list(ipg_ethernet_device);

	IPG_WRITE_MAXFRAMESIZE(baseaddr, IPG_MAX_RXFRAME_SIZE);
	IPG_WRITE_RXEARLYTHRESH(baseaddr, IPG_RXEARLYTHRESH_VALUE);
	IPG_WRITE_TXSTARTTHRESH(baseaddr, IPG_TXSTARTTHRESH_VALUE);
	IPG_WRITE_RXDMAINTCTRL(baseaddr, (IPG_RI_RSVD_MASK &
	                              ((IPG_RI_RXFRAME_COUNT &
	                                IPG_RXFRAME_COUNT) |
	                               (IPG_RI_PRIORITY_THRESH &
	                                (IPG_PRIORITY_THRESH << 12)) |
	                               (IPG_RI_RXDMAWAIT_TIME &
	                                (IPG_RXDMAWAIT_TIME << 16)))));
	IPG_WRITE_RXDMAPOLLPERIOD(baseaddr, IPG_RXDMAPOLLPERIOD_VALUE);
	IPG_WRITE_RXDMAURGENTTHRESH(baseaddr,
	                               IPG_RXDMAURGENTTHRESH_VALUE);
	IPG_WRITE_RXDMABURSTTHRESH(baseaddr,
	                              IPG_RXDMABURSTTHRESH_VALUE);
	IPG_WRITE_TXDMAPOLLPERIOD(baseaddr,
	                             IPG_TXDMAPOLLPERIOD_VALUE);
	IPG_WRITE_TXDMAURGENTTHRESH(baseaddr,
	                               IPG_TXDMAURGENTTHRESH_VALUE);
	IPG_WRITE_TXDMABURSTTHRESH(baseaddr,
	                              IPG_TXDMABURSTTHRESH_VALUE);
	IPG_WRITE_INTENABLE(baseaddr, IPG_IE_HOST_ERROR |
	                          IPG_IE_TX_DMA_COMPLETE |
	                          IPG_IE_TX_COMPLETE |
	                          IPG_IE_INT_REQUESTED |
	                          IPG_IE_UPDATE_STATS |
	                          IPG_IE_LINK_EVENT |
	                          IPG_IE_RX_DMA_COMPLETE |
	                          //IPG_IE_RFD_LIST_END |   //20041019Jesse_For_SmartBit: remove
	                          IPG_IE_RX_DMA_PRIORITY);

	IPG_WRITE_FLOWONTHRESH(baseaddr, IPG_FLOWONTHRESH_VALUE);
	IPG_WRITE_FLOWOFFTHRESH(baseaddr, IPG_FLOWOFFTHRESH_VALUE);

	/* IPG multi-frag frame bug workaround.
	 * Per silicon revision B3 eratta.
	 */
	IPG_WRITE_DEBUGCTRL(baseaddr,
	                       IPG_READ_DEBUGCTRL(baseaddr) | 0x0200);

	/* IPG TX poll now bug workaround.
	 * Per silicon revision B3 eratta.
	 */
	IPG_WRITE_DEBUGCTRL(baseaddr,
	                       IPG_READ_DEBUGCTRL(baseaddr) | 0x0010);

	/* IPG RX poll now bug workaround.
	 * Per silicon revision B3 eratta.
	 */
	IPG_WRITE_DEBUGCTRL(baseaddr,
	                       IPG_READ_DEBUGCTRL(baseaddr) | 0x0020);

	/* Now restore MACCTRL to original setting. */
	IPG_WRITE_MACCTRL(baseaddr, restoremacctrl);

	/* Disable unused RMON statistics. */
	IPG_WRITE_RMONSTATISTICSMASK(baseaddr, IPG_RZ_ALL);

	/* Disable unused MIB statistics. */
	IPG_WRITE_STATISTICSMASK(baseaddr,
	                     IPG_SM_MACCONTROLFRAMESXMTD |
	                     IPG_SM_BCSTOCTETXMTOK_BCSTFRAMESXMTDOK |
	                     IPG_SM_MCSTOCTETXMTOK_MCSTFRAMESXMTDOK |
	                     IPG_SM_MACCONTROLFRAMESRCVD |
	                     IPG_SM_BCSTOCTETRCVDOK_BCSTFRAMESRCVDOK |
	                     IPG_SM_TXJUMBOFRAMES |
	                     IPG_SM_UDPCHECKSUMERRORS |
	                     IPG_SM_IPCHECKSUMERRORS |
	                     IPG_SM_TCPCHECKSUMERRORS |
	                     IPG_SM_RXJUMBOFRAMES);

	return 0;
}

static  irqreturn_t  ipg_interrupt_handler(int ipg_irq, void *device_instance)
{
	int				error;
	u32				baseaddr;
	u16				intstatusackword;
        IPG_DEVICE_TYPE		*ipg_ethernet_device =
		                        (IPG_DEVICE_TYPE *)device_instance;

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)
        irqreturn_t intr_handled= IRQ_HANDLED;//IRQ_NONE;
#endif

#ifdef IPG_DEBUG
	struct ipg_nic_private	*sp = (struct ipg_nic_private *)
					      ipg_ethernet_device->priv;
#endif

	IPG_DEBUG_MSG("_interrupt_handler\n");

	baseaddr = ipg_ethernet_device->base_addr;

#ifdef IPG_LINUX2_2
        /*
         * The following code fragment was authored by Donald Becker.
         */
#if defined(__i386__)
                /* A lock to prevent simultaneous entry bug on Intel SMP machines. */
                if (test_and_set_bit(0, (void*)&ipg_ethernet_device->interrupt)) {
                        printk(KERN_ERR "%s: SMP simultaneous entry of an interrupt handler.\n",
                               ipg_ethernet_device->name);
                        ipg_ethernet_device->interrupt = 0;     /* Avoid halting machine. */
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)
                        return intr_handled;
#else
                        return;
#endif

                }
#else
                if (ipg_ethernet_device->interrupt) {
                        printk(KERN_ERR "%s: Re-entering the interrupt handler.\n",
                               ipg_ethernet_device->name);
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)
                        return intr_handled;
#else
                        return;
#endif
                }
                ipg_ethernet_device->interrupt = 1;
#endif
        /*
         * End of code fragment authored by Donald Becker.
         */

        /* Setting ipg_ethernet_device->interrupt = 1
         * indicates to higher network layers an
         * Ethernet NIC interrupt is being serviced.
         */
#endif

#ifdef CONFIG_NET_IPG_JUMBO_FRAME
	ipg_nic_rxrestore(ipg_ethernet_device);
#endif
	/* Get interrupt source information, and acknowledge
	 * some (i.e. TxDMAComplete, RxDMAComplete, RxEarly,
	 * IntRequested, MacControlFrame, LinkEvent) interrupts
	 * if issued. Also, all IPG interrupts are disabled by
	 * reading IntStatusAck.
	 */
	intstatusackword = IPG_READ_INTSTATUSACK(baseaddr);

	IPG_DEBUG_MSG("IntStatusAck = %4.4x\n", intstatusackword);

	/* If RFDListEnd interrupt, restore all used RFDs. */
	if (intstatusackword & IPG_IS_RFD_LIST_END)
	{
		IPG_DEBUG_MSG("RFDListEnd Interrupt.\n");

		/* The RFD list end indicates an RFD was encountered
		 * with a 0 NextPtr, or with an RFDDone bit set to 1
		 * (indicating the RFD is not read for use by the
		 * IPG.) Try to restore all RFDs.
		 */
		ipg_nic_rxrestore(ipg_ethernet_device);

#ifdef IPG_DEBUG
		/* Increment the RFDlistendCount counter. */
		sp->RFDlistendCount++;
#endif
	}

	/* If RFDListEnd, RxDMAPriority, RxDMAComplete, or
	 * IntRequested interrupt, process received frames. */
	if ((intstatusackword & IPG_IS_RX_DMA_PRIORITY) ||
	    (intstatusackword & IPG_IS_RFD_LIST_END) ||
	    (intstatusackword & IPG_IS_RX_DMA_COMPLETE) ||
	    (intstatusackword & IPG_IS_INT_REQUESTED))
	{

#ifdef IPG_DEBUG
	/* Increment the RFD list checked counter if interrupted
	 * only to check the RFD list. */
	if (intstatusackword & (~(IPG_IS_RX_DMA_PRIORITY |
	    IPG_IS_RFD_LIST_END | IPG_IS_RX_DMA_COMPLETE |
	    IPG_IS_INT_REQUESTED) &
	                          (IPG_IS_HOST_ERROR |
	                          IPG_IS_TX_DMA_COMPLETE |
	                          IPG_IS_TX_COMPLETE |
	                          IPG_IS_UPDATE_STATS |
	                          IPG_IS_LINK_EVENT)))

	{
		sp->RFDListCheckedCount++;
	}
#endif

		ipg_nic_rx(ipg_ethernet_device);
	}

	/* If TxDMAComplete interrupt, free used TFDs. */
	if (intstatusackword & IPG_IS_TX_DMA_COMPLETE)
	{
		/* Free used TFDs. */
		ipg_nic_txfree(ipg_ethernet_device);
	}

	/* TxComplete interrupts indicate one of numerous actions.
	 * Determine what action to take based on TXSTATUS register.
	 */
	if (intstatusackword & IPG_IS_TX_COMPLETE)
	{
		ipg_nic_txcleanup(ipg_ethernet_device);
	}

	/* If UpdateStats interrupt, update Linux Ethernet statistics */
	if (intstatusackword & IPG_IS_UPDATE_STATS)
	{
		ipg_nic_get_stats(ipg_ethernet_device);
	}

	/* If HostError interrupt, reset IPG. */
	if (intstatusackword & IPG_IS_HOST_ERROR)
	{
		IPG_DDEBUG_MSG("HostError Interrupt\n");

		IPG_DDEBUG_MSG("DMACtrl = %8.8x\n",
		 IPG_READ_DMACTRL(baseaddr));

		/* Acknowledge HostError interrupt by resetting
		 * IPG DMA and HOST.
		 */
		ipg_reset(ipg_ethernet_device,
		            IPG_AC_GLOBAL_RESET |
		            IPG_AC_HOST |
		            IPG_AC_DMA);

		error = ipg_io_config(ipg_ethernet_device);
		if (error < 0)
		{
                	printk(KERN_INFO "%s: Cannot recover from PCI error.\n",
			       ipg_ethernet_device->name);
		}

		init_rfdlist(ipg_ethernet_device);

		init_tfdlist(ipg_ethernet_device);
	}

	/* If LinkEvent interrupt, resolve autonegotiation. */
	if (intstatusackword & IPG_IS_LINK_EVENT)
	{
		if (ipg_config_autoneg(ipg_ethernet_device) < 0)
			printk(KERN_INFO "%s: Auto-negotiation error.\n",
			       ipg_ethernet_device->name);

	}

	/* If MACCtrlFrame interrupt, do nothing. */
	if (intstatusackword & IPG_IS_MAC_CTRL_FRAME)
	{
		IPG_DEBUG_MSG("MACCtrlFrame interrupt.\n");
	}

	/* If RxComplete interrupt, do nothing. */
	if (intstatusackword & IPG_IS_RX_COMPLETE)
	{
		IPG_DEBUG_MSG("RxComplete interrupt.\n");
	}

	/* If RxEarly interrupt, do nothing. */
	if (intstatusackword & IPG_IS_RX_EARLY)
	{
		IPG_DEBUG_MSG("RxEarly interrupt.\n");
	}

	/* Re-enable IPG interrupts. */
	IPG_WRITE_INTENABLE(baseaddr, IPG_IE_HOST_ERROR |
	                          IPG_IE_TX_DMA_COMPLETE |
	                          IPG_IE_TX_COMPLETE |
	                          IPG_IE_INT_REQUESTED |
	                          IPG_IE_UPDATE_STATS |
	                          IPG_IE_LINK_EVENT |
	                          IPG_IE_RX_DMA_COMPLETE |
	                          //IPG_IE_RFD_LIST_END | //20041019Jesse_For_SmartBit: remove
	                          IPG_IE_RX_DMA_PRIORITY);

        /* Indicate to higher network layers the Ethernet NIC
         * interrupt servicing is complete.
         */

#ifdef IPG_LINUX2_2
        /*
         * The following code fragment was authored by Donald Becker.
         */
#if defined(__i386__)
                clear_bit(0, (void*)&ipg_ethernet_device->interrupt);
#else
                ipg_ethernet_device->interrupt = 0;
#endif
        /*
         * End of code fragment authored by Donald Becker.
         */
#endif

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)
                        return intr_handled;
#else
                        return;
#endif
}

void ipg_nic_txcleanup(IPG_DEVICE_TYPE *ipg_ethernet_device)
{
	/* For TxComplete interrupts, free all transmit
	 * buffers which have already been transfered via DMA
	 * to the IPG.
	 */

	int				maxtfdcount;
	u32				baseaddr;
	u32				txstatusdword;
	struct ipg_nic_private	*sp = (struct ipg_nic_private *)
					      ipg_ethernet_device->priv;

	IPG_DEBUG_MSG("_nic_txcleanup\n");

	baseaddr = ipg_ethernet_device->base_addr;
	maxtfdcount = IPG_TFDLIST_LENGTH;

	do
	{
		/* Reading the TXSTATUS register clears the
		 * TX_COMPLETE interrupt.
		 */
		txstatusdword = IPG_READ_TXSTATUS(baseaddr);

		IPG_DEBUG_MSG("TxStatus = %8.8x\n",txstatusdword);

		/* Check for Transmit errors. Error bits only valid if
		 * TX_COMPLETE bit in the TXSTATUS register is a 1.
		 */
		if (txstatusdword & IPG_TS_TX_COMPLETE)
		{

			/* If in 10Mbps mode, indicate transmit is ready. */
			if (sp->tenmbpsmode)
			{
				spin_lock(&sp->lock);
				IPG_TX_NOTBUSY(ipg_ethernet_device);
				spin_unlock(&sp->lock);
			}

			/* Transmit error, increment stat counters. */
			if (txstatusdword & IPG_TS_TX_ERROR)
			{
				IPG_DEBUG_MSG("Transmit error.\n");
				sp->stats.tx_errors++;
			}

			/* Late collision, re-enable transmitter. */
			if (txstatusdword & IPG_TS_LATE_COLLISION)
			{
				IPG_DEBUG_MSG("Late collision on transmit.\n");
				IPG_WRITE_MACCTRL(baseaddr,
				  IPG_READ_MACCTRL(baseaddr) |
				  IPG_MC_TX_ENABLE);
			}

			/* Maximum collisions, re-enable transmitter. */
			if (txstatusdword & IPG_TS_TX_MAX_COLL)
			{
				IPG_DEBUG_MSG("Maximum collisions on transmit.\n");

				IPG_WRITE_MACCTRL(baseaddr,
				  IPG_READ_MACCTRL(baseaddr) |
				  IPG_MC_TX_ENABLE);
			}

			/* Transmit underrun, reset and re-enable
			 * transmitter.
			 */
			if (txstatusdword & IPG_TS_TX_UNDERRUN)
			{
				IPG_DEBUG_MSG("Transmitter underrun.\n");
				sp->stats.tx_fifo_errors++;
				ipg_reset(ipg_ethernet_device, IPG_AC_TX_RESET |
				                       IPG_AC_DMA |
				                       IPG_AC_NETWORK);

				/* Re-configure after DMA reset. */
				if ((ipg_io_config(ipg_ethernet_device) < 0) ||
				    (init_tfdlist(ipg_ethernet_device) < 0))
				{
					printk(KERN_INFO "%s: Error during re-configuration.\n",
				               ipg_ethernet_device->name);
				}

				IPG_WRITE_MACCTRL(baseaddr,
				  IPG_READ_MACCTRL(baseaddr) |
				  IPG_MC_TX_ENABLE);
			}
		}
		else
			break;

		maxtfdcount--;

	}
	while(maxtfdcount != 0);

	ipg_nic_txfree(ipg_ethernet_device);

	return;
}

void ipg_nic_txfree(IPG_DEVICE_TYPE *ipg_ethernet_device)
{
        /* Free all transmit buffers which have already been transfered
         * via DMA to the IPG.
         */

        int                             NextToFree;
        int                             maxtfdcount;
	struct ipg_nic_private	*sp = (struct ipg_nic_private *)
					      ipg_ethernet_device->priv;

	IPG_DEBUG_MSG("_nic_txfree\n");

	maxtfdcount = IPG_TFDLIST_LENGTH;

	/* Set the CurrentTxFrameID to skip the next
	 * TxDMACompleteInterrupt.
	 */
	sp->CurrentTxFrameID = 1;

	do
	{
		/* Calculate next TFD to release. */
		NextToFree = (sp->LastFreedTxBuff + 1) % IPG_TFDLIST_LENGTH;

		IPG_DEBUG_MSG("TFC = %16.16lx\n", (unsigned long int)
		                 sp->TFDList[NextToFree].TFC);

		/* Look at each TFD's TFC field beginning
		 * at the last freed TFD up to the current TFD.
		 * If the TFDDone bit is set, free the associated
		 * buffer.
		 */
		if ((le64_to_cpu(sp->TFDList[NextToFree].TFC) &
		     IPG_TFC_TFDDONE) &&
		    (NextToFree != sp->CurrentTFD))
		{
			/* Free the transmit buffer. */
			if (sp->TxBuff[NextToFree] != NULL)
			{
#ifdef IPG_LINUX2_4
				pci_unmap_single(sp->ipg_pci_device,
		                 sp->TxBuffDMAhandle[NextToFree].dmahandle,
				 sp->TxBuffDMAhandle[NextToFree].len,
				 PCI_DMA_TODEVICE);
#endif

				IPG_DEV_KFREE_SKB(sp->TxBuff[NextToFree]);

				sp->TxBuff[NextToFree] = NULL;
			}

			sp->LastFreedTxBuff = NextToFree;
		}
                else
                        break;

                maxtfdcount--;

        } while(maxtfdcount != 0);

	return;
}

int	ipg_nic_open(IPG_DEVICE_TYPE *ipg_ethernet_device)
{
	/* The IPG NIC Ethernet interface is opened when activated
	 * by ifconfig.
	 */

	int				phyaddr = 0;
	int				error = 0;
	int				i;
	u32				baseaddr;
	u8					revisionid=0;
	struct ipg_nic_private	*sp = (struct ipg_nic_private *)
					      ipg_ethernet_device->priv;

        IPG_DEBUG_MSG("_nic_open\n");



#ifdef CONFIG_NET_IPG_IO
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
	/* Check for I/O port address conflicts, return error
	 * if requested range is already in use.
	 */
	if ((error = check_region(IPG_PCI_RESOURCE_START(
	                          sp->ipg_pci_device, 0) &
	                          IPG_PIB_IOBASEADDRESS,
	                          IPG_IO_REG_RANGE)) < 0)
	{
		printk(KERN_INFO "%s: Error registering I/O ports.\n",
		       ipg_ethernet_device->name);
		return error;
	}

	/* Requested range is free, reserve that range for
	 * use by the IPG.
	 */
	request_region(IPG_PCI_RESOURCE_START(sp->ipg_pci_device, 0) &
	               IPG_PIB_IOBASEADDRESS,
	               IPG_IO_REG_RANGE,
	               IPG_DRIVER_NAME);

	/* Use I/O space to access IPG registers. */
	ipg_ethernet_device->base_addr =
		(IPG_PCI_RESOURCE_START(sp->ipg_pci_device, 0) &
		 IPG_PIB_IOBASEADDRESS);
#endif //LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)

#else /* Not using I/O space. */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
	/* Use memory space to access IPG registers. ioremap
	 * is used to map high-memory PCI buffer address space
	 * to user address space.
	 */
	ipg_ethernet_device->base_addr = (unsigned long)
	  (ioremap(IPG_PCI_RESOURCE_START(sp->ipg_pci_device, 1) &
	  IPG_PMB_MEMBASEADDRESS, IPG_MEM_REG_RANGE));
#endif //LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
#ifdef IPG_LINUX2_4
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
	ipg_ethernet_device->mem_start = pci_resource_start(
	                                     sp->ipg_pci_device, 1);
	ipg_ethernet_device->mem_end = ipg_ethernet_device->mem_start + IPG_MEM_REG_RANGE;
#endif //LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)	                                   
#endif

#endif

	baseaddr = ipg_ethernet_device->base_addr;

	/* Reset all functions within the IPG. Do not assert
	 * RST_OUT as not compatible with some PHYs.
	 */
	i = IPG_AC_GLOBAL_RESET | IPG_AC_RX_RESET |
	    IPG_AC_TX_RESET | IPG_AC_DMA |
	    IPG_AC_FIFO | IPG_AC_NETWORK |
	    IPG_AC_HOST | IPG_AC_AUTO_INIT;
   /* Read/Write and Reset EEPROM Value Jesse20040128EEPROM_VALUE */
	/* Read LED Mode Configuration from EEPROM */
	sp->LED_Mode=read_eeprom(ipg_ethernet_device, 6);
	
	error = ipg_reset(ipg_ethernet_device, i);
	if (error < 0)
	{
		return error;
	}

	/* Reset PHY. */
	phyaddr = ipg_find_phyaddr(ipg_ethernet_device);

	if (phyaddr != -1)
	{   u16 mii_phyctrl, mii_1000cr;
	        mii_1000cr = read_phy_register(ipg_ethernet_device,
			                   phyaddr, GMII_PHY_1000BASETCONTROL);
	        write_phy_register(ipg_ethernet_device, phyaddr,
			        			GMII_PHY_1000BASETCONTROL,
			        			mii_1000cr |
								GMII_PHY_1000BASETCONTROL_FULL_DUPLEX |
								GMII_PHY_1000BASETCONTROL_HALF_DUPLEX |
								GMII_PHY_1000BASETCONTROL_PreferMaster);

	        mii_phyctrl = read_phy_register(ipg_ethernet_device,
						phyaddr, GMII_PHY_CONTROL);
			/* Set default phyparam*/
			pci_read_config_byte(sp->ipg_pci_device,PCI_REVISION_ID,&revisionid);
			bSetPhyDefaultParam(revisionid,ipg_ethernet_device,phyaddr);
			
			/* reset Phy*/			
			write_phy_register(ipg_ethernet_device,
		                   	phyaddr, GMII_PHY_CONTROL,
				   			(mii_phyctrl | GMII_PHY_CONTROL_RESET |
				    		MII_PHY_CONTROL_RESTARTAN));

	}

        /* Intitalize lock variable before requesting interrupt. */
    sp->lock = (spinlock_t) SPIN_LOCK_UNLOCKED;

	/* Check for interrupt line conflicts, and request interrupt
	 * line for IPG.
	 *
	 * IMPORTANT: Disable IPG interrupts prior to registering
	 *            IRQ.
	 */
	IPG_WRITE_INTENABLE(baseaddr, 0x0000);

	/* Register the interrupt line to be used by the IPG within
	 * the Linux system.
	 */
	if ((error = request_irq(sp->ipg_pci_device->irq,
	                         &ipg_interrupt_handler,
	                         SA_SHIRQ,
	                         ipg_ethernet_device->name,
	                         ipg_ethernet_device)) < 0)
	{
		printk(KERN_INFO "%s: Error when requesting interrupt.\n",
                               ipg_ethernet_device->name);
		return error;
	}

	ipg_ethernet_device->irq = sp->ipg_pci_device->irq;

#ifdef IPG_LINUX2_2
	/* Reserve memory for RFD list which must lie on an 8 byte
	 * boundary. So, allocated memory must be adjusted to account
	 * for this alignment.
	 */
	sp->RFDList = kmalloc((sizeof(struct RFD) * IPG_RFDLIST_LENGTH) +
		              IPG_DMALIST_ALIGN_PAD, GFP_KERNEL);

	if (sp->RFDList != NULL)
	{
		/* Adjust the start address of the allocated
		 * memory to assure it begins on an 8 byte
		 * boundary.
		 */
		sp->RFDList = (void *)(((long)(sp->RFDList) +
	                       IPG_DMALIST_ALIGN_PAD) &
		               ~IPG_DMALIST_ALIGN_PAD);
	}

	/* Reserve memory for TFD list which must lie on an 8 byte
	 * boundary. So, allocated memory must be adjusted to account
	 * for this alignment.
	 */
	sp->TFDList = kmalloc((sizeof(struct TFD) * IPG_TFDLIST_LENGTH) +
		              IPG_DMALIST_ALIGN_PAD, GFP_KERNEL);

	if (sp->TFDList != NULL)
	{
		/* Adjust the start address of the allocated
		 * memory to assure it begins on an 8 byte
		 * boundary.
		 */
		sp->TFDList = (void *)(((long)(sp->TFDList) +
	                       IPG_DMALIST_ALIGN_PAD) &
		               ~IPG_DMALIST_ALIGN_PAD);
	}
#endif

#ifdef IPG_LINUX2_4
	sp->RFDList = pci_alloc_consistent(sp->ipg_pci_device,
	                                   (sizeof(struct RFD) *
	                                   IPG_RFDLIST_LENGTH),
	                                   &sp->RFDListDMAhandle);

	sp->TFDList = pci_alloc_consistent(sp->ipg_pci_device,
	                                   (sizeof(struct TFD) *
	                                   IPG_TFDLIST_LENGTH),
	                                   &sp->TFDListDMAhandle);
#endif

	if ((sp->RFDList == NULL) || (sp->TFDList == NULL))
	{
		printk(KERN_INFO "%s: No memory available for IP1000 RFD and/or TFD lists.\n", ipg_ethernet_device->name);
		return -ENOMEM;
	}

	error = init_rfdlist(ipg_ethernet_device);
	if (error < 0)
	{
		printk(KERN_INFO "%s: Error during configuration.\n",
                               ipg_ethernet_device->name);
		return error;
	}

	error = init_tfdlist(ipg_ethernet_device);
	if (error < 0)
	{
		printk(KERN_INFO "%s: Error during configuration.\n",
                               ipg_ethernet_device->name);
		return error;
	}
	
	/* Read MAC Address from EERPOM Jesse20040128EEPROM_VALUE */
	sp->StationAddr0=read_eeprom(ipg_ethernet_device, 16);
	sp->StationAddr1=read_eeprom(ipg_ethernet_device, 17);
	sp->StationAddr2=read_eeprom(ipg_ethernet_device, 18);
	/* Write MAC Address to Station Address */
	IPG_WRITE_STATIONADDRESS0(baseaddr,sp->StationAddr0);
	IPG_WRITE_STATIONADDRESS1(baseaddr,sp->StationAddr1);
	IPG_WRITE_STATIONADDRESS2(baseaddr,sp->StationAddr2);

	/* Set station address in ethernet_device structure. */
	ipg_ethernet_device->dev_addr[0] =
	  IPG_READ_STATIONADDRESS0(baseaddr) & 0x00FF;
	ipg_ethernet_device->dev_addr[1] =
	  (IPG_READ_STATIONADDRESS0(baseaddr) & 0xFF00) >> 8;
	ipg_ethernet_device->dev_addr[2] =
	  IPG_READ_STATIONADDRESS1(baseaddr) & 0x00FF;
	ipg_ethernet_device->dev_addr[3] =
	  (IPG_READ_STATIONADDRESS1(baseaddr) & 0xFF00) >> 8;
	ipg_ethernet_device->dev_addr[4] =
	  IPG_READ_STATIONADDRESS2(baseaddr) & 0x00FF;
	ipg_ethernet_device->dev_addr[5] =
	  (IPG_READ_STATIONADDRESS2(baseaddr) & 0xFF00) >> 8;

	/* Configure IPG I/O registers. */
	error = ipg_io_config(ipg_ethernet_device);
	if (error < 0)
	{
		printk(KERN_INFO "%s: Error during configuration.\n",
                               ipg_ethernet_device->name);
		return error;
	}

	/* Resolve autonegotiation. */
	if (ipg_config_autoneg(ipg_ethernet_device) < 0)
	{
		printk(KERN_INFO "%s: Auto-negotiation error.\n",
		       ipg_ethernet_device->name);
	}

#ifdef CONFIG_NET_IPG_JUMBO_FRAME
    /* initialize JUMBO Frame control variable */
    sp->Jumbo.FoundStart=0;
    sp->Jumbo.CurrentSize=0;
    sp->Jumbo.skb=0;
    ipg_ethernet_device->mtu = IPG_TXFRAG_SIZE;
#endif

	/* Enable transmit and receive operation of the IPG. */
	IPG_WRITE_MACCTRL(baseaddr, IPG_READ_MACCTRL(baseaddr) |
	                     IPG_MC_RX_ENABLE | IPG_MC_TX_ENABLE);

#ifdef IPG_LINUX2_2
	ipg_ethernet_device->interrupt = 0;
	ipg_ethernet_device->start = 1;
	clear_bit(0,(void*)&ipg_ethernet_device->tbusy);
#endif

#ifdef IPG_LINUX2_4
	netif_start_queue(ipg_ethernet_device);
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
	/* Increment the usage count for this IPG driver module. */
	MOD_INC_USE_COUNT;
#endif

	return 0;
}

int init_rfdlist(IPG_DEVICE_TYPE *ipg_ethernet_device)
{
	/* Initialize the RFDList. */

	int				i;
	u32				baseaddr;
	struct ipg_nic_private	*sp = (struct ipg_nic_private *)
					      ipg_ethernet_device->priv;

	IPG_DEBUG_MSG("_init_rfdlist\n");

	baseaddr = ipg_ethernet_device->base_addr;

	/* Clear the receive buffer not ready flag. */
	sp->RxBuffNotReady = 0;

	for(i=0; i<IPG_RFDLIST_LENGTH; i++)
	{
		/* Free any allocated receive buffers. */
#ifdef IPG_LINUX2_4
		pci_unmap_single(sp->ipg_pci_device,
                                 sp->RxBuffDMAhandle[i].dmahandle,
		                 sp->RxBuffDMAhandle[i].len,
		                 PCI_DMA_FROMDEVICE);
#endif
		if (sp->RxBuff[i] != NULL)
			IPG_DEV_KFREE_SKB(sp->RxBuff[i]);
		sp->RxBuff[i] = NULL;

		/* Clear out the RFS field. */
	 	sp->RFDList[i].RFS = 0x0000000000000000;

		if (ipg_get_rxbuff(ipg_ethernet_device, i) < 0)
		{
			/* A receive buffer was not ready, break the
			 * RFD list here and set the receive buffer
			 * not ready flag.
			 */
			sp->RxBuffNotReady = 1;

			IPG_DEBUG_MSG("Cannot allocate Rx buffer.\n");

			/* Just in case we cannot allocate a single RFD.
			 * Should not occur.
			 */
			if (i == 0)
			{
				printk(KERN_ERR "%s: No memory available for RFD list.\n",
				       ipg_ethernet_device->name);
				return -ENOMEM;
			}
		}

		/* Set up RFDs to point to each other. A ring structure. */
#ifdef IPG_LINUX2_2
	 	sp->RFDList[i].RFDNextPtr = cpu_to_le64(
		                                 IPG_HOST2BUS_MAP(
		                                 &sp->RFDList[(i + 1) %
		                                 IPG_RFDLIST_LENGTH]));
#endif

#ifdef IPG_LINUX2_4
	 	sp->RFDList[i].RFDNextPtr = cpu_to_le64(
		                            sp->RFDListDMAhandle +
		                            ((sizeof(struct RFD)) *
		                             ((i + 1) %
		                              IPG_RFDLIST_LENGTH)));
#endif
	}

	sp->CurrentRFD = 0;
	sp->LastRestoredRxBuff = i - 1;

	/* Write the location of the RFDList to the IPG. */
#ifdef IPG_LINUX2_2
	IPG_WRITE_RFDLISTPTR0(baseaddr,
	  (u32)(IPG_HOST2BUS_MAP(sp->RFDList)));
	IPG_WRITE_RFDLISTPTR1(baseaddr, 0x00000000);
#endif

#ifdef IPG_LINUX2_4
	IPG_WRITE_RFDLISTPTR0(baseaddr, (u32)(sp->RFDListDMAhandle));
	IPG_WRITE_RFDLISTPTR1(baseaddr, 0x00000000);
#endif

	return 0;
}

int init_tfdlist(IPG_DEVICE_TYPE *ipg_ethernet_device)
{
	/* Initialize TFDList. */

	int				i;
	u32				baseaddr;
	struct ipg_nic_private	*sp = (struct ipg_nic_private *)
					      ipg_ethernet_device->priv;

	IPG_DEBUG_MSG("_init_tfdlist\n");

	baseaddr = ipg_ethernet_device->base_addr;

	for(i=0; i<IPG_TFDLIST_LENGTH; i++)
	{
#ifdef IPG_LINUX2_2
	 	sp->TFDList[i].TFDNextPtr = cpu_to_le64(
		                                 IPG_HOST2BUS_MAP(
		                                 &sp->TFDList[(i + 1) %
		                                 IPG_TFDLIST_LENGTH]));
#endif

#ifdef IPG_LINUX2_4
	 	sp->TFDList[i].TFDNextPtr = cpu_to_le64(
		                            sp->TFDListDMAhandle +
		                            ((sizeof(struct TFD)) *
		                             ((i + 1) %
		                              IPG_TFDLIST_LENGTH)));
#endif

	 	sp->TFDList[i].TFC = cpu_to_le64(IPG_TFC_TFDDONE);
		if (sp->TxBuff[i] != NULL)
			IPG_DEV_KFREE_SKB(sp->TxBuff[i]);
		sp->TxBuff[i] = NULL;
	}

	sp->CurrentTFD = IPG_TFDLIST_LENGTH - 1;
	sp->CurrentTxFrameID = 0;
	sp->LastFreedTxBuff = IPG_TFDLIST_LENGTH - 1;

	/* Write the location of the TFDList to the IPG. */
#ifdef IPG_LINUX2_2
	IPG_WRITE_TFDLISTPTR0(baseaddr, (u32)(IPG_HOST2BUS_MAP(
	                                         sp->TFDList)));
	IPG_WRITE_TFDLISTPTR1(baseaddr, 0x00000000);
#endif

#ifdef IPG_LINUX2_4
	IPG_DDEBUG_MSG("Starting TFDListPtr = %8.8x\n",
	  (u32)(sp->TFDListDMAhandle));
	IPG_WRITE_TFDLISTPTR0(baseaddr, (u32)(sp->TFDListDMAhandle));
	IPG_WRITE_TFDLISTPTR1(baseaddr, 0x00000000);
#endif

	return 0;
}

int	ipg_get_rxbuff(IPG_DEVICE_TYPE *ipg_ethernet_device, int rfd)
{
	/* Create a receive buffer within system memory and update
	 * NIC private structure appropriately.
	 */
	u64				rxfragsize;
	struct sk_buff			*skb;
	struct ipg_nic_private	*sp = (struct ipg_nic_private *)
					      ipg_ethernet_device->priv;

	IPG_DEBUG_MSG("_get_rxbuff\n");

	/* Allocate memory buffers for receive frames. Pad by
	 * 2 to account for IP field alignment.
	 */
	skb = dev_alloc_skb(IPG_RXSUPPORT_SIZE + 2);

	if (skb == NULL)
	{
		sp->RxBuff[rfd] = NULL;
		return -ENOMEM;
	}

	/* Adjust the data start location within the buffer to
	 * align IP address field to a 16 byte boundary.
	 */
	skb_reserve(skb, 2);

	/* Associate the receive buffer with the IPG NIC. */
	skb->dev = ipg_ethernet_device;

	/* Save the address of the sk_buff structure. */
	sp->RxBuff[rfd] = skb;

#ifdef IPG_LINUX2_2
	/* The sk_buff struct "data" field holds the address
	 * of the beginning of valid octets. Assign this address
	 * to the fragement address field of the RFD.
	 */
 	sp->RFDList[rfd].FragInfo = cpu_to_le64(
	                                 IPG_HOST2BUS_MAP(skb->data));
#endif

#ifdef IPG_LINUX2_4
	sp->RxBuffDMAhandle[rfd].len = IPG_RXSUPPORT_SIZE;
	sp->RxBuffDMAhandle[rfd].dmahandle = pci_map_single(
	                                      sp->ipg_pci_device,
	                                      skb->data, IPG_RXSUPPORT_SIZE,
	                                      PCI_DMA_FROMDEVICE);
	sp->RFDList[rfd].FragInfo = cpu_to_le64(
	                                sp->RxBuffDMAhandle[rfd].dmahandle);
#endif

	/* Set the RFD fragment length. */
	rxfragsize = IPG_RXFRAG_SIZE;
 	sp->RFDList[rfd].FragInfo |= cpu_to_le64((rxfragsize << 48) &
	                                         IPG_RFI_FRAGLEN);

	return 0;
}

int	ipg_nic_stop(IPG_DEVICE_TYPE *ipg_ethernet_device)
{
	/* Release resources requested by driver open function. */

	int	i;
	int	error;
	u32	baseaddr;
	struct ipg_nic_private	*sp = (struct ipg_nic_private *)
					      ipg_ethernet_device->priv;

	IPG_DEBUG_MSG("_nic_stop\n");
	
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)
   netif_stop_queue(ipg_ethernet_device);
   //netif_msg_ifdown(sp);
#endif

	baseaddr = ipg_ethernet_device->base_addr;

	IPG_DDEBUG_MSG("TFDunavailCount = %i\n", sp->TFDunavailCount);
	IPG_DDEBUG_MSG("RFDlistendCount = %i\n", sp->RFDlistendCount);
	IPG_DDEBUG_MSG("RFDListCheckedCount = %i\n", sp->RFDListCheckedCount);
	IPG_DDEBUG_MSG("EmptyRFDListCount = %i\n", sp->EmptyRFDListCount);
	IPG_DUMPTFDLIST(ipg_ethernet_device);

	/* Reset all functions within the IPG to shut down the
	 IP1000* .
	 */
	i = IPG_AC_GLOBAL_RESET | IPG_AC_RX_RESET |
	    IPG_AC_TX_RESET | IPG_AC_DMA |
	    IPG_AC_FIFO | IPG_AC_NETWORK |
	    IPG_AC_HOST | IPG_AC_AUTO_INIT;

	error = ipg_reset(ipg_ethernet_device, i);
	if (error < 0)
	{
		return error;
	}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
	/* Decrement the usage count for this IPG driver module. */
	MOD_DEC_USE_COUNT;
#endif

	/* Free all receive buffers. */
	for(i=0; i<IPG_RFDLIST_LENGTH; i++)
	{
#ifdef IPG_LINUX2_4
		pci_unmap_single(sp->ipg_pci_device,
                                 sp->RxBuffDMAhandle[i].dmahandle,
		                 sp->RxBuffDMAhandle[i].len,
		                 PCI_DMA_FROMDEVICE);
#endif
		if (sp->RxBuff[i] != NULL)
			IPG_DEV_KFREE_SKB(sp->RxBuff[i]);
		sp->RxBuff[i] = NULL;
	}

	/* Free all transmit buffers. */
	for(i=0; i<IPG_TFDLIST_LENGTH; i++)
	{
		if (sp->TxBuff[i] != NULL)
			IPG_DEV_KFREE_SKB(sp->TxBuff[i]);
		sp->TxBuff[i] = NULL;
	}

#ifdef IPG_LINUX2_2
	ipg_ethernet_device->start = 0;
	set_bit(0,(void*)&ipg_ethernet_device->tbusy);

	/* Free memory associated with the RFDList. */
	kfree(sp->RFDList);

	/* Free memory associated with the TFDList. */
	kfree(sp->TFDList);
#endif

#ifdef IPG_LINUX2_4
	netif_stop_queue(ipg_ethernet_device);

	/* Free memory associated with the RFDList. */
	pci_free_consistent(sp->ipg_pci_device,
                            (sizeof(struct RFD) *
                            IPG_RFDLIST_LENGTH),
	                    sp->RFDList, sp->RFDListDMAhandle);

	/* Free memory associated with the TFDList. */
	pci_free_consistent(sp->ipg_pci_device,
                            (sizeof(struct TFD) *
                            IPG_TFDLIST_LENGTH),
	                    sp->TFDList, sp->TFDListDMAhandle);
#endif

	/* Release interrupt line. */
	free_irq(ipg_ethernet_device->irq, ipg_ethernet_device);
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)
         return 0;
#endif

#ifdef CONFIG_NET_IPG_IO

	/* Release I/O range reserved for IPG registers. */
	release_region(ipg_ethernet_device->base_addr,
	               IPG_IO_REG_RANGE);

#else /* Not using I/O space. */

	/* Unmap memory used for IPG registers. */

	/* The following line produces strange results unless
	 * unregister_netdev precedes it.
	 */
	iounmap((void *)ipg_ethernet_device->base_addr);

#endif

	return 0;
}

int	ipg_nic_hard_start_xmit(struct sk_buff *skb,
	                          IPG_DEVICE_TYPE *ipg_ethernet_device)
{
	/* Transmit an Ethernet frame. */

	u64				fraglen;
	u64				vlanvid;
	u64				vlancfi;
	u64				vlanuserpriority;
        unsigned long                   flags;
	int				NextTFD;
	u32				baseaddr;
	struct ipg_nic_private	*sp = (struct ipg_nic_private *)
					      ipg_ethernet_device->priv;

        IPG_DDEBUG_MSG("_nic_hard_start_xmit\n");

	baseaddr = ipg_ethernet_device->base_addr;

#ifdef IPG_LINUX2_2
	/* If IPG NIC is already busy, return error. */
	if (test_and_set_bit(0, (void*)&ipg_ethernet_device->tbusy) != 0)
	{
                IPG_DEBUG_MSG("Transmit busy!\n");

		return -EBUSY;
	}
#endif

        /* Disable interrupts. */
        spin_lock_irqsave(&sp->lock, flags);

#ifdef IPG_LINUX2_4
	/* If in 10Mbps mode, stop the transmit queue so
	 * no more transmit frames are accepted.
	 */
	if (sp->tenmbpsmode)
	{
		netif_stop_queue(ipg_ethernet_device);
	}
#endif

	/* Next TFD is found by incrementing the CurrentTFD
	 * counter, modulus the length of the TFDList.
	 */
	NextTFD = (sp->CurrentTFD + 1) % IPG_TFDLIST_LENGTH;

	/* Check for availability of next TFD. */
	if (!(le64_to_cpu(sp->TFDList[NextTFD].TFC) &
	      IPG_TFC_TFDDONE) || (NextTFD == sp->LastFreedTxBuff))
	{
		IPG_DEBUG_MSG("Next TFD not available.\n");

		/* Attempt to free any used TFDs. */
		ipg_nic_txfree(ipg_ethernet_device);

#ifdef IPG_LINUX2_2
                /* Transmit no longer busy. */
		clear_bit(0,(void*)&ipg_ethernet_device->tbusy);
#endif

                /* Restore interrupts. */
                spin_unlock_irqrestore(&sp->lock, flags);

#ifdef IPG_DEBUG
		/* Increment the TFDunavailCount counter. */
		sp->TFDunavailCount++;
#endif

                return -ENOMEM;
	}

#ifdef IPG_LINUX2_4
	sp->TxBuffDMAhandle[NextTFD].len = skb->len;
	sp->TxBuffDMAhandle[NextTFD].dmahandle = pci_map_single(
	                                          sp->ipg_pci_device,
	                                          skb->data, skb->len,
	                                          PCI_DMA_TODEVICE);
#endif

	/* Save the sk_buff pointer so interrupt handler can later free
	 * memory occupied by buffer.
	 */
	sp->TxBuff[NextTFD] = skb;

	/* Clear all TFC fields, except TFDDONE. */
	sp->TFDList[NextTFD].TFC = cpu_to_le64(IPG_TFC_TFDDONE);

	/* Specify the TFC field within the TFD. */
	sp->TFDList[NextTFD].
		TFC |= cpu_to_le64(IPG_TFC_WORDALIGNDISABLED |
		                   (IPG_TFC_FRAMEID &
		                    cpu_to_le64(sp->CurrentTxFrameID)) |
		                   (IPG_TFC_FRAGCOUNT & (1 << 24)));

	/* Request TxComplete interrupts at an interval defined
	 * by the constant IPG_FRAMESBETWEENTXCOMPLETES.
	 * Request TxComplete interrupt for every frame
	 * if in 10Mbps mode to accomodate problem with 10Mbps
	 * processing.
	 */
	if (sp->tenmbpsmode)
	{
		sp->TFDList[NextTFD].TFC |=
	        cpu_to_le64(IPG_TFC_TXINDICATE);
	}
	else if ((sp->CurrentTxFrameID %
	         IPG_FRAMESBETWEENTXDMACOMPLETES) == 0)
	{
		sp->TFDList[NextTFD].TFC |=
	        cpu_to_le64(IPG_TFC_TXDMAINDICATE);
	}

        /* Based on compilation option, determine if FCS is to be
         * appended to transmit frame by IPG.
         */
        if (!(IPG_APPEND_FCS_ON_TX))
        {
		sp->TFDList[NextTFD].
                        TFC |= cpu_to_le64(IPG_TFC_FCSAPPENDDISABLE);
        }

        /* Based on compilation option, determine if IP, TCP and/or
	 * UDP checksums are to be added to transmit frame by IPG.
         */
        if (IPG_ADD_IPCHECKSUM_ON_TX)
        {
		sp->TFDList[NextTFD].
                        TFC |= cpu_to_le64(IPG_TFC_IPCHECKSUMENABLE);
        }
        if (IPG_ADD_TCPCHECKSUM_ON_TX)
        {
		sp->TFDList[NextTFD].
                        TFC |= cpu_to_le64(IPG_TFC_TCPCHECKSUMENABLE);
        }
        if (IPG_ADD_UDPCHECKSUM_ON_TX)
        {
		sp->TFDList[NextTFD].
                        TFC |= cpu_to_le64(IPG_TFC_UDPCHECKSUMENABLE);
        }

        /* Based on compilation option, determine if VLAN tag info is to be
         * inserted into transmit frame by IPG.
         */
        if (IPG_INSERT_MANUAL_VLAN_TAG)
        {
		vlanvid = IPG_MANUAL_VLAN_VID;
		vlancfi = IPG_MANUAL_VLAN_CFI;
		vlanuserpriority = IPG_MANUAL_VLAN_USERPRIORITY;

		sp->TFDList[NextTFD].TFC |= cpu_to_le64(
		                            IPG_TFC_VLANTAGINSERT |
                                            (vlanvid << 32) |
			                    (vlancfi << 44) |
			                    (vlanuserpriority << 45));
        }

	/* The fragment start location within system memory is defined
	 * by the sk_buff structure's data field. The physical address
	 * of this location within the system's virtual memory space
	 * is determined using the IPG_HOST2BUS_MAP function.
	 */
#ifdef IPG_LINUX2_2
	sp->TFDList[NextTFD].FragInfo = cpu_to_le64(
	                                     IPG_HOST2BUS_MAP(skb->data));
#endif

#ifdef IPG_LINUX2_4
	sp->TFDList[NextTFD].FragInfo = cpu_to_le64(
	                                sp->TxBuffDMAhandle[NextTFD].dmahandle);
#endif

	/* The length of the fragment within system memory is defined by
	 * the sk_buff structure's len field.
	 */
	fraglen = (u16)skb->len;
	sp->TFDList[NextTFD].FragInfo |= cpu_to_le64(IPG_TFI_FRAGLEN &
		                         (fraglen << 48));

	/* Clear the TFDDone bit last to indicate the TFD is ready
	 * for transfer to the IPG.
	 */
	sp->TFDList[NextTFD].TFC &= cpu_to_le64(~IPG_TFC_TFDDONE);

	/* Record frame transmit start time (jiffies = Linux
	 * kernel current time stamp).
	 */
	ipg_ethernet_device->trans_start = jiffies;

	/* Update current TFD indicator. */
	sp->CurrentTFD = NextTFD;

	/* Calculate the new ID for the next transmit frame by
	 * incrementing the CurrentTxFrameID counter.
	 */
	sp->CurrentTxFrameID++;

	/* Force a transmit DMA poll event. */
	IPG_WRITE_DMACTRL(baseaddr, IPG_DC_TX_DMA_POLL_NOW);

        /* Restore interrupts. */
        spin_unlock_irqrestore(&sp->lock, flags);

#ifdef IPG_LINUX2_2
        /* Transmit no longer busy.
	 * If in 10Mbps mode, do not indicate transmit is free
	 * until receive TxComplete interrupt.
	 */
	if (!(sp->tenmbpsmode))
	{
		clear_bit(0,(void*)&ipg_ethernet_device->tbusy);
	}
#endif

	return 0;
}

#ifdef CONFIG_NET_IPG_JUMBO_FRAME

/* use jumboindex and jumbosize to control jumbo frame status
   initial status is jumboindex=-1 and jumbosize=0
   1. jumboindex = -1 and jumbosize=0 : previous jumbo frame has been done.
   2. jumboindex != -1 and jumbosize != 0 : jumbo frame is not over size and receiving
   3. jumboindex = -1 and jumbosize != 0 : jumbo frame is over size, already dump
                previous receiving and need to continue dumping the current one
*/
enum {NormalPacket,ErrorPacket};
enum {Frame_NoStart_NoEnd=0,Frame_WithStart=1,Frame_WithEnd=10,Frame_WithStart_WithEnd=11};
inline void ipg_nic_rx__FreeSkb(IPG_DEVICE_TYPE *ipg_ethernet_device)
{
	struct ipg_nic_private	*sp = (struct ipg_nic_private *)ipg_ethernet_device->priv;
					if (sp->RxBuff[sp->CurrentRFD] != NULL)
				{
#ifdef IPG_LINUX2_4
						pci_unmap_single(sp->ipg_pci_device,sp->RxBuffDMAhandle[sp->CurrentRFD].dmahandle,sp->RxBuffDMAhandle[sp->CurrentRFD].len,PCI_DMA_FROMDEVICE);
#endif
						IPG_DEV_KFREE_SKB(sp->RxBuff[sp->CurrentRFD]);
						sp->RxBuff[sp->CurrentRFD] = NULL;
				}		
}
inline int ipg_nic_rx__CheckFrameSEType(IPG_DEVICE_TYPE *ipg_ethernet_device)
{
	struct ipg_nic_private	*sp = (struct ipg_nic_private *)ipg_ethernet_device->priv;
	int FoundStartEnd=0;
	
	if(le64_to_cpu(sp->RFDList[sp->CurrentRFD].RFS)&IPG_RFS_FRAMESTART)FoundStartEnd+=1;
	if(le64_to_cpu(sp->RFDList[sp->CurrentRFD].RFS)&IPG_RFS_FRAMEEND)FoundStartEnd+=10;
	return FoundStartEnd; //Frame_NoStart_NoEnd=0,Frame_WithStart=1,Frame_WithEnd=10,Frame_WithStart_WithEnd=11
}
inline int ipg_nic_rx__CheckError(IPG_DEVICE_TYPE *ipg_ethernet_device)
{
	struct ipg_nic_private	*sp = (struct ipg_nic_private *)ipg_ethernet_device->priv;
	
				   if (IPG_DROP_ON_RX_ETH_ERRORS &&
		     		(le64_to_cpu(sp->RFDList[sp->CurrentRFD].RFS) &
		      		(IPG_RFS_RXFIFOOVERRUN | IPG_RFS_RXRUNTFRAME |
		       		 IPG_RFS_RXALIGNMENTERROR | IPG_RFS_RXFCSERROR |
		             IPG_RFS_RXOVERSIZEDFRAME | IPG_RFS_RXLENGTHERROR)))
			  {
					IPG_DEBUG_MSG("Rx error, RFS = %16.16lx\n",
					   (unsigned long int) sp->RFDList[sp->CurrentRFD].RFS);

					/* Increment general receive error statistic. */
					sp->stats.rx_errors++;

					/* Increment detailed receive error statistics. */
					if (le64_to_cpu(sp->RFDList[sp->CurrentRFD].RFS) & IPG_RFS_RXFIFOOVERRUN)
					{
						IPG_DEBUG_MSG("RX FIFO overrun occured.\n");

						sp->stats.rx_fifo_errors++;

						if (sp->RxBuffNotReady == 1)
						{
							/* If experience a RxFIFO overrun, and
							 * the RxBuffNotReady flag is set,
							 * assume the FIFO overran due to lack
							 * of an RFD.
							 */
							sp->stats.rx_dropped++;
						}
					}

					if (le64_to_cpu(sp->RFDList[sp->CurrentRFD].RFS) & IPG_RFS_RXRUNTFRAME)
					{
						IPG_DEBUG_MSG("RX runt occured.\n");
						sp->stats.rx_length_errors++;
					}

					/* Do nothing for IPG_RFS_RXOVERSIZEDFRAME,
					 * error count handled by a IPG statistic register.
			 		 */

					if (le64_to_cpu(sp->RFDList[sp->CurrentRFD].RFS) & IPG_RFS_RXALIGNMENTERROR)
					{
						IPG_DEBUG_MSG("RX alignment error occured.\n");
						sp->stats.rx_frame_errors++;
					}


					/* Do nothing for IPG_RFS_RXFCSERROR, error count
					 * handled by a IPG statistic register.
					 */

					/* Free the memory associated with the RX
					 * buffer since it is erroneous and we will
					 * not pass it to higher layer processes.
					 */
					if (sp->RxBuff[sp->CurrentRFD] != NULL)
					{
#ifdef IPG_LINUX2_4
						pci_unmap_single(sp->ipg_pci_device,
                                 sp->RxBuffDMAhandle[sp->CurrentRFD].dmahandle,
		    		             sp->RxBuffDMAhandle[sp->CurrentRFD].len,
		    		             PCI_DMA_FROMDEVICE);
#endif

						IPG_DEV_KFREE_SKB(sp->RxBuff[sp->CurrentRFD]);
						sp->RxBuff[sp->CurrentRFD] = NULL;
					}
				return ErrorPacket;
			  }
			  return NormalPacket;
}
int ipg_nic_rx(IPG_DEVICE_TYPE *ipg_ethernet_device)
{
	/* Transfer received Ethernet frames to higher network layers. */

	int				maxrfdcount;
	int				framelen;
	int            ThisEndFrameLen;
	u32				baseaddr;
	struct ipg_nic_private	*sp = (struct ipg_nic_private *)
					      ipg_ethernet_device->priv;
	struct sk_buff			*skb;


	IPG_DEBUG_MSG("_nic_rx\n");

	baseaddr = ipg_ethernet_device->base_addr;
	maxrfdcount = IPG_MAXRFDPROCESS_COUNT;

    while(le64_to_cpu(sp->RFDList[sp->CurrentRFD].RFS) & IPG_RFS_RFDDONE)
    {
	 	if (--maxrfdcount == 0)
	    {
	        /* There are more RFDs to process, however the
	     	 * allocated amount of RFD processing time has
			 * expired. Assert Interrupt Requested to make
			 * sure we come back to process the remaining RFDs.
	         */
		    IPG_WRITE_ASICCTRL(baseaddr,
			                   IPG_READ_ASICCTRL(baseaddr) |
		                       IPG_AC_INT_REQUEST);
		    break;
        }

		   switch(ipg_nic_rx__CheckFrameSEType(ipg_ethernet_device))
		   { // Frame in one RFD
//-----------
		   case Frame_WithStart_WithEnd: 	
		   	if(sp->Jumbo.FoundStart)
		   	{
					IPG_DEV_KFREE_SKB(sp->Jumbo.skb);
					sp->Jumbo.FoundStart=0;
    				sp->Jumbo.CurrentSize=0;
    				sp->Jumbo.skb=NULL;
    			}
		     if(ipg_nic_rx__CheckError(ipg_ethernet_device)==NormalPacket)//1: found error, 0 no error
			  { // accept this frame and send to upper layer
					skb = sp->RxBuff[sp->CurrentRFD];
					if(skb)
					{
						framelen=le64_to_cpu(sp->RFDList[sp->CurrentRFD].RFS) & IPG_RFS_RXFRAMELEN;
						if (framelen > IPG_RXFRAG_SIZE) framelen=IPG_RXFRAG_SIZE;
						skb_put(skb, framelen);
						/* Set the buffer's protocol field to Ehternet */
						skb->protocol=eth_type_trans(skb, ipg_ethernet_device);
						/* Not handle TCP/UDP/IP checksum */
						skb->ip_summed=CHECKSUM_NONE;
						netif_rx(skb);
						ipg_ethernet_device->last_rx=jiffies;
						sp->RxBuff[sp->CurrentRFD] = NULL;
					}
			  }//if(ipg_nic_rx__CheckError(ipg_ethernet_device)==0)//1: found error, 0 no error
			break;//case Frame_WithStart_WithEnd: 	
//-----------
			case Frame_WithStart: 
		     if(ipg_nic_rx__CheckError(ipg_ethernet_device)==NormalPacket)//1: found error, 0 no error
			  { // accept this frame and send to upper layer
					skb = sp->RxBuff[sp->CurrentRFD];
					if(skb)
					{
						if(sp->Jumbo.FoundStart)
						{
							IPG_DEV_KFREE_SKB(sp->Jumbo.skb);
						}
#ifdef IPG_LINUX2_4
				pci_unmap_single(sp->ipg_pci_device,
                                 sp->RxBuffDMAhandle[sp->CurrentRFD].dmahandle,
		                 sp->RxBuffDMAhandle[sp->CurrentRFD].len,
		                 PCI_DMA_FROMDEVICE);
#endif
    						sp->Jumbo.FoundStart=1;
    						sp->Jumbo.CurrentSize=IPG_RXFRAG_SIZE;    						
    						sp->Jumbo.skb=skb;
    						skb_put(sp->Jumbo.skb, IPG_RXFRAG_SIZE);
							sp->RxBuff[sp->CurrentRFD] = NULL;
							ipg_ethernet_device->last_rx=jiffies;
					}
			  }//if(ipg_nic_rx__CheckError(ipg_ethernet_device)==0)//1: found error, 0 no error
			break;//case Frame_WithStart: 
//-----------
			case Frame_WithEnd: 
		     if(ipg_nic_rx__CheckError(ipg_ethernet_device)==NormalPacket)//1: found error, 0 no error
			  { // accept this frame and send to upper layer
					skb = sp->RxBuff[sp->CurrentRFD];
					if(skb)
					{
						if(sp->Jumbo.FoundStart)
						{
							framelen=le64_to_cpu(sp->RFDList[sp->CurrentRFD].RFS) & IPG_RFS_RXFRAMELEN;
							ThisEndFrameLen=framelen-sp->Jumbo.CurrentSize;
							//if (framelen > IPG_RXFRAG_SIZE) framelen=IPG_RXFRAG_SIZE;
							if(framelen>IPG_RXSUPPORT_SIZE)
							{
								IPG_DEV_KFREE_SKB(sp->Jumbo.skb);
							}
							else
							{
								memcpy(skb_put(sp->Jumbo.skb,ThisEndFrameLen),skb->data,ThisEndFrameLen);
								/* Set the buffer's protocol field to Ehternet */
								sp->Jumbo.skb->protocol=eth_type_trans(sp->Jumbo.skb, ipg_ethernet_device);
								/* Not handle TCP/UDP/IP checksum */
								sp->Jumbo.skb->ip_summed=CHECKSUM_NONE;
								netif_rx(sp->Jumbo.skb);							
							}							
						}//"if(sp->Jumbo.FoundStart)"
						
						ipg_ethernet_device->last_rx=jiffies;
						sp->Jumbo.FoundStart=0;
    					sp->Jumbo.CurrentSize=0;
    					sp->Jumbo.skb=NULL;
    					//Free this buffer(JC-ADVANCE)
						ipg_nic_rx__FreeSkb(ipg_ethernet_device);
					}//"if(skb)"
			  }//"if(ipg_nic_rx__CheckError(ipg_ethernet_device)==0)//1: found error, 0 no error"
			  else
			  {
			  		IPG_DEV_KFREE_SKB(sp->Jumbo.skb);
					sp->Jumbo.FoundStart=0;
    				sp->Jumbo.CurrentSize=0;
    				sp->Jumbo.skb=NULL;
			  }
			break;//case Frame_WithEnd:
//-----------
			case Frame_NoStart_NoEnd: 
		     if(ipg_nic_rx__CheckError(ipg_ethernet_device)==NormalPacket)//1: found error, 0 no error
			  { // accept this frame and send to upper layer
					skb = sp->RxBuff[sp->CurrentRFD];
					if(skb)
					{
						if(sp->Jumbo.FoundStart)
						{
							//if (framelen > IPG_RXFRAG_SIZE) framelen=IPG_RXFRAG_SIZE;
							sp->Jumbo.CurrentSize+=IPG_RXFRAG_SIZE;
							if(sp->Jumbo.CurrentSize>IPG_RXSUPPORT_SIZE)
							{
								/*IPG_DEV_KFREE_SKB(sp->Jumbo.skb);
								sp->Jumbo.FoundStart=0;
    							sp->Jumbo.CurrentSize=0;
    							sp->Jumbo.skb=NULL;*/
							}
							else
							{
								memcpy(skb_put(sp->Jumbo.skb,IPG_RXFRAG_SIZE),skb->data,IPG_RXFRAG_SIZE);
							}
						}//"if(sp->Jumbo.FoundStart)"
						ipg_ethernet_device->last_rx=jiffies;
						ipg_nic_rx__FreeSkb(ipg_ethernet_device);
					}
			  }//if(ipg_nic_rx__CheckError(ipg_ethernet_device)==0)//1: found error, 0 no error
			  else
			  {
			  		IPG_DEV_KFREE_SKB(sp->Jumbo.skb);
					sp->Jumbo.FoundStart=0;
    				sp->Jumbo.CurrentSize=0;
    				sp->Jumbo.skb=NULL;
			  }
			break;//case Frame_NoStart_NoEnd:
			}//switch(ipg_nic_rx__CheckFrameSEType(ipg_ethernet_device))

	    sp->CurrentRFD = (sp->CurrentRFD+1) % IPG_RFDLIST_LENGTH;
	} /* end of while(IPG_RFS_RFDDONE)*/
	
	ipg_nic_rxrestore(ipg_ethernet_device);
	return 0;
}


#else
int ipg_nic_rx(IPG_DEVICE_TYPE *ipg_ethernet_device)
{
	/* Transfer received Ethernet frames to higher network layers. */

	int				maxrfdcount;
	int				framelen;
	u32				baseaddr;
	struct ipg_nic_private	*sp = (struct ipg_nic_private *)
					      ipg_ethernet_device->priv;
	struct sk_buff			*skb;

	IPG_DEBUG_MSG("_nic_rx\n");

	baseaddr = ipg_ethernet_device->base_addr;
	maxrfdcount = IPG_MAXRFDPROCESS_COUNT;

	while((le64_to_cpu(sp->RFDList[sp->CurrentRFD].RFS)) &
	       IPG_RFS_RFDDONE &&
	      (le64_to_cpu(sp->RFDList[sp->CurrentRFD].RFS)) &
	       IPG_RFS_FRAMESTART &&
	      (le64_to_cpu(sp->RFDList[sp->CurrentRFD].RFS)) &
	       IPG_RFS_FRAMEEND &&
	      (sp->RxBuff[sp->CurrentRFD] != NULL))
	{
                if (--maxrfdcount == 0)
                {
                        /* There are more RFDs to process, however the
			 * allocated amount of RFD processing time has
			 * expired. Assert Interrupt Requested to make
			 * sure we come back to process the remaining RFDs.
                         */
                        IPG_WRITE_ASICCTRL(baseaddr,
			                      IPG_READ_ASICCTRL(baseaddr) |
                                              IPG_AC_INT_REQUEST);
                        break;
                }

		/* Get received frame length. */
		framelen = le64_to_cpu(sp->RFDList[sp->CurrentRFD].RFS) &
	       		        IPG_RFS_RXFRAMELEN;

		/* Check for jumbo frame arrival with too small
		 * RXFRAG_SIZE.
		 */
		if (framelen > IPG_RXFRAG_SIZE)
		{
			IPG_DEBUG_MSG("RFS FrameLen > allocated fragment size.\n");

			framelen = IPG_RXFRAG_SIZE;
		}

		/* Get the received frame buffer. */
		skb = sp->RxBuff[sp->CurrentRFD];

		if ((IPG_DROP_ON_RX_ETH_ERRORS &&
		     (le64_to_cpu(sp->RFDList[sp->CurrentRFD].RFS &
		      (IPG_RFS_RXFIFOOVERRUN |
		       IPG_RFS_RXRUNTFRAME |
		       IPG_RFS_RXALIGNMENTERROR |
		       IPG_RFS_RXFCSERROR |
		       IPG_RFS_RXOVERSIZEDFRAME |
		       IPG_RFS_RXLENGTHERROR)))))
		{

			IPG_DEBUG_MSG("Rx error, RFS = %16.16lx\n", (unsigned long int) sp->RFDList[sp->CurrentRFD].RFS);

			/* Increment general receive error statistic. */
			sp->stats.rx_errors++;

			/* Increment detailed receive error statistics. */
			if (le64_to_cpu(sp->RFDList[sp->CurrentRFD].RFS &
			    IPG_RFS_RXFIFOOVERRUN))
			{
				IPG_DEBUG_MSG("RX FIFO overrun occured.\n");

				sp->stats.rx_fifo_errors++;

				if (sp->RxBuffNotReady == 1)
				{
					/* If experience a RxFIFO overrun, and
					 * the RxBuffNotReady flag is set,
					 * assume the FIFO overran due to lack
					 * of an RFD.
					 */
					sp->stats.rx_dropped++;
				}
			}

			if (le64_to_cpu(sp->RFDList[sp->CurrentRFD].RFS &
			    IPG_RFS_RXRUNTFRAME))
			{
				IPG_DEBUG_MSG("RX runt occured.\n");
				sp->stats.rx_length_errors++;
			}

			if (le64_to_cpu(sp->RFDList[sp->CurrentRFD].RFS &
			    IPG_RFS_RXOVERSIZEDFRAME));
			/* Do nothing, error count handled by a IPG
			 * statistic register.
			 */

			if (le64_to_cpu(sp->RFDList[sp->CurrentRFD].RFS &
			    IPG_RFS_RXALIGNMENTERROR))
			{
				IPG_DEBUG_MSG("RX alignment error occured.\n");
				sp->stats.rx_frame_errors++;
			}


			if (le64_to_cpu(sp->RFDList[sp->CurrentRFD].RFS &
			    IPG_RFS_RXFCSERROR));
			/* Do nothing, error count handled by a IPG
			 * statistic register.
			 */

			/* Free the memory associated with the RX
			 * buffer since it is erroneous and we will
			 * not pass it to higher layer processes.
			 */
			if (sp->RxBuff[sp->CurrentRFD] != NULL)
			{
#ifdef IPG_LINUX2_4
				pci_unmap_single(sp->ipg_pci_device,
                                 sp->RxBuffDMAhandle[sp->CurrentRFD].dmahandle,
		                 sp->RxBuffDMAhandle[sp->CurrentRFD].len,
		                 PCI_DMA_FROMDEVICE);
#endif

				IPG_DEV_KFREE_SKB(sp->RxBuff[sp->CurrentRFD]);
			}

		}
		else
		{

			/* Adjust the new buffer length to accomodate the size
			 * of the received frame.
			 */
			skb_put(skb, framelen);

			/* Set the buffer's protocol field to Ethernet. */
			skb->protocol = eth_type_trans(skb,
			                               ipg_ethernet_device);

			/* If the frame contains an IP/TCP/UDP frame,
			 * determine if upper layer must check IP/TCP/UDP
			 * checksums.
			 *
			 * NOTE: DO NOT RELY ON THE TCP/UDP CHECKSUM
			 *       VERIFICATION FOR SILICON REVISIONS B3
			 *       AND EARLIER!
			 *
			if ((le64_to_cpu(sp->RFDList[sp->CurrentRFD].RFS &
			     (IPG_RFS_TCPDETECTED |
			      IPG_RFS_UDPDETECTED |
			      IPG_RFS_IPDETECTED))) &&
			    !(le64_to_cpu(sp->RFDList[sp->CurrentRFD].RFS &
			      (IPG_RFS_TCPERROR |
			       IPG_RFS_UDPERROR |
			       IPG_RFS_IPERROR))))
			{
				* Indicate IP checksums were performed
				 * by the IPG.
				 *
				skb->ip_summed = CHECKSUM_UNNECESSARY;
			}
			else
			*/
			if (1==1)
			{
				/* The IPG encountered an error with (or
				 * there were no) IP/TCP/UDP checksums.
				 * This may or may not indicate an invalid
				 * IP/TCP/UDP frame was received. Let the
				 * upper layer decide.
				 */
				skb->ip_summed = CHECKSUM_NONE;
			}

			/* Hand off frame for higher layer processing.
			 * The function netif_rx() releases the sk_buff
			 * when processing completes.
			 */
			netif_rx(skb);

			/* Record frame receive time (jiffies = Linux
			 * kernel current time stamp).
			 */
			ipg_ethernet_device->last_rx = jiffies;
		}

		/* Assure RX buffer is not reused by IPG. */
		sp->RxBuff[sp->CurrentRFD] = NULL;

		/* Increment the current RFD counter. */
		sp->CurrentRFD = (sp->CurrentRFD + 1) % IPG_RFDLIST_LENGTH;

	}


#ifdef IPG_DEBUG
	/* Check if the RFD list contained no receive frame data. */
	if (maxrfdcount == IPG_MAXRFDPROCESS_COUNT)
	{
		sp->EmptyRFDListCount++;
	}
#endif
	while((le64_to_cpu(sp->RFDList[sp->CurrentRFD].RFS &
	       IPG_RFS_RFDDONE)) &&
	      !((le64_to_cpu(sp->RFDList[sp->CurrentRFD].RFS &
	       IPG_RFS_FRAMESTART)) &&
	      (le64_to_cpu(sp->RFDList[sp->CurrentRFD].RFS &
	       IPG_RFS_FRAMEEND))))
	{
		IPG_DEBUG_MSG("Frame requires multiple RFDs.\n");

		/* An unexpected event, additional code needed to handle
		 * properly. So for the time being, just disregard the
		 * frame.
		 */

		/* Free the memory associated with the RX
		 * buffer since it is erroneous and we will
		 * not pass it to higher layer processes.
		 */
		if (sp->RxBuff[sp->CurrentRFD] != NULL)
		{
#ifdef IPG_LINUX2_4
			pci_unmap_single(sp->ipg_pci_device,
                                sp->RxBuffDMAhandle[sp->CurrentRFD].dmahandle,
		                sp->RxBuffDMAhandle[sp->CurrentRFD].len,
		                PCI_DMA_FROMDEVICE);
#endif
			IPG_DEV_KFREE_SKB(sp->RxBuff[sp->CurrentRFD]);
		}

		/* Assure RX buffer is not reused by IPG. */
		sp->RxBuff[sp->CurrentRFD] = NULL;

		/* Increment the current RFD counter. */
		sp->CurrentRFD = (sp->CurrentRFD + 1) % IPG_RFDLIST_LENGTH;
	}

        /* Check to see if there are a minimum number of used
         * RFDs before restoring any (should improve performance.)
         */
        if (((sp->CurrentRFD > sp->LastRestoredRxBuff) &&
             ((sp->LastRestoredRxBuff + IPG_MINUSEDRFDSTOFREE) <=
              sp->CurrentRFD)) ||
            ((sp->CurrentRFD < sp->LastRestoredRxBuff) &&
             ((sp->LastRestoredRxBuff + IPG_MINUSEDRFDSTOFREE) <=
              (sp->CurrentRFD + IPG_RFDLIST_LENGTH))))
        {
		ipg_nic_rxrestore(ipg_ethernet_device);
	}

	return 0;
}
#endif
int ipg_nic_rxrestore(IPG_DEVICE_TYPE *ipg_ethernet_device)
{
	/* Restore used receive buffers. */

	int                             i;
	struct ipg_nic_private        *sp = (struct ipg_nic_private *)
					ipg_ethernet_device->priv;

	IPG_DEBUG_MSG("_nic_rxrestore\n");

	/* Assume receive buffers will be available. */
	sp->RxBuffNotReady = 0;

        while (sp->RxBuff[i = ((sp->LastRestoredRxBuff + 1) %
               IPG_RFDLIST_LENGTH)] == NULL)
	{
		/* Generate a new receive buffer to replace the
		 * current buffer (which will be released by the
		 * Linux system).
		 */
		if (ipg_get_rxbuff(ipg_ethernet_device, i) < 0)
		{
			IPG_DEBUG_MSG("Cannot allocate new Rx buffer.\n");

			/* Mark a flag indicating a receive buffer
			 * was not available. Use this flag to update
			 * the rx_dropped Linux statistic.
			 */
			sp->RxBuffNotReady = 1;

			break;
		}

		/* Reset the RFS field. */
		sp->RFDList[i].RFS = 0x0000000000000000;

		sp->LastRestoredRxBuff = i;
	}

	return 0;
}

IPG_STATS_TYPE* ipg_nic_get_stats(IPG_DEVICE_TYPE *ipg_ethernet_device)
{
	/* Provides statistical information about the IPG NIC. */

	u16				temp1;
	u16				temp2;
	u32				baseaddr;
	struct ipg_nic_private	*sp = (struct ipg_nic_private *)
					      ipg_ethernet_device->priv;

	IPG_DEBUG_MSG("_nic_get_stats\n");

	/* Check to see if the NIC has been initialized via nic_open,
	 * before trying to read statistic registers.
	 */
#ifdef IPG_LINUX2_2
	if (ipg_ethernet_device->start == 0)
	{
#endif
#ifdef IPG_LINUX2_4
	if (!test_bit(__LINK_STATE_START, &ipg_ethernet_device->state))
	{
#endif
		return &sp->stats;
	}


	baseaddr = ipg_ethernet_device->base_addr;

        sp->stats.rx_packets += IPG_READ_FRAMESRCVDOK(baseaddr);
        sp->stats.tx_packets += IPG_READ_FRAMESXMTDOK(baseaddr);
        sp->stats.rx_bytes   += IPG_READ_OCTETRCVOK(baseaddr);
        sp->stats.tx_bytes   += IPG_READ_OCTETXMTOK(baseaddr);
	temp1 = IPG_READ_FRAMESLOSTRXERRORS(baseaddr);
        sp->stats.rx_errors  += temp1;
        sp->stats.rx_missed_errors += temp1;
	temp1 = IPG_READ_SINGLECOLFRAMES(baseaddr) +
	        IPG_READ_MULTICOLFRAMES(baseaddr) +
	        IPG_READ_LATECOLLISIONS(baseaddr);
	temp2 = IPG_READ_CARRIERSENSEERRORS(baseaddr);
        sp->stats.collisions += temp1;
	sp->stats.tx_dropped += IPG_READ_FRAMESABORTXSCOLLS(baseaddr);
        sp->stats.tx_errors  += IPG_READ_FRAMESWEXDEFERRAL(baseaddr) +
	                        IPG_READ_FRAMESWDEFERREDXMT(baseaddr) +
	                        temp1 + temp2;
        sp->stats.multicast  += IPG_READ_MCSTOCTETRCVDOK(baseaddr);

        /* detailed tx_errors */
        sp->stats.tx_carrier_errors += temp2;

        /* detailed rx_errors */
	sp->stats.rx_length_errors += IPG_READ_INRANGELENGTHERRORS(baseaddr) +
	                              IPG_READ_FRAMETOOLONGERRRORS(baseaddr);
	sp->stats.rx_crc_errors += IPG_READ_FRAMECHECKSEQERRORS(baseaddr);

	/* Unutilized IPG statistic registers. */
	IPG_READ_MCSTFRAMESRCVDOK(baseaddr);

        /* Masked IPG statistic registers (need not be read to clear)
	IPG_READ_MACCONTROLFRAMESXMTDOK(baseaddr);
	IPG_READ_BCSTFRAMESXMTDOK(baseaddr);
	IPG_READ_MCSTFRAMESXMTDOK(baseaddr);
	IPG_READ_BCSTOCTETXMTOK(baseaddr);
	IPG_READ_MCSTOCTETXMTOK(baseaddr);
	IPG_READ_MACCONTROLFRAMESRCVD(baseaddr);
	IPG_READ_BCSTFRAMESRCVDOK(baseaddr);
	IPG_READ_BCSTOCTETRCVOK(baseaddr);
	IPG_READ_TXJUMBOFRAMES(baseaddr);
	IPG_READ_UDPCHECKSUMERRORS(baseaddr);
	IPG_READ_IPCHECKSUMERRORS(baseaddr);
	IPG_READ_TCPCHECKSUMERRORS(baseaddr);
	IPG_READ_RXJUMBOFRAMES(baseaddr);
	*/


	/* Unutilized RMON statistic registers. */

        /* Masked IPG statistic registers (need not be read to clear)
	IPG_READ_ETHERSTATSCOLLISIONS(baseaddr);
	IPG_READ_ETHERSTATSOCTETSTRANSMIT(baseaddr);
	IPG_READ_ETHERSTATSPKTSTRANSMIT(baseaddr);
	IPG_READ_ETHERSTATSPKTS64OCTESTSTRANSMIT(baseaddr);
	IPG_READ_ETHERSTATSPKTS65TO127OCTESTSTRANSMIT(baseaddr);
	IPG_READ_ETHERSTATSPKTS128TO255OCTESTSTRANSMIT(baseaddr);
	IPG_READ_ETHERSTATSPKTS256TO511OCTESTSTRANSMIT(baseaddr);
	IPG_READ_ETHERSTATSPKTS512TO1023OCTESTSTRANSMIT(baseaddr);
	IPG_READ_ETHERSTATSPKTS1024TO1518OCTESTSTRANSMIT(baseaddr);
	IPG_READ_ETHERSTATSCRCALIGNERRORS(baseaddr);
	IPG_READ_ETHERSTATSUNDERSIZEPKTS(baseaddr);
	IPG_READ_ETHERSTATSFRAGMENTS(baseaddr);
	IPG_READ_ETHERSTATSJABBERS(baseaddr);
	IPG_READ_ETHERSTATSOCTETS(baseaddr);
	IPG_READ_ETHERSTATSPKTS(baseaddr);
	IPG_READ_ETHERSTATSPKTS64OCTESTS(baseaddr);
	IPG_READ_ETHERSTATSPKTS65TO127OCTESTS(baseaddr);
	IPG_READ_ETHERSTATSPKTS128TO255OCTESTS(baseaddr);
	IPG_READ_ETHERSTATSPKTS256TO511OCTESTS(baseaddr);
	IPG_READ_ETHERSTATSPKTS512TO1023OCTESTS(baseaddr);
	IPG_READ_ETHERSTATSPKTS1024TO1518OCTESTS(baseaddr);
	*/

	return &sp->stats;
}

void ipg_nic_set_multicast_list(IPG_DEVICE_TYPE *ipg_ethernet_device)
{
	/* Determine and configure multicast operation and set
	 * receive mode for IPG.
	 */
	u8			receivemode;
	u32			hashtable[2];
	unsigned int		hashindex;
	u32			baseaddr;
	struct dev_mc_list	*mc_list_ptr;


	IPG_DEBUG_MSG("_nic_set_multicast_list\n");

	baseaddr = ipg_ethernet_device->base_addr;

	receivemode = IPG_RM_RECEIVEUNICAST |
	              IPG_RM_RECEIVEBROADCAST;

	if (ipg_ethernet_device->flags & IFF_PROMISC)
	{
		/* NIC to be configured in promiscuous mode. */
		receivemode = IPG_RM_RECEIVEALLFRAMES;
	}
	else if ((ipg_ethernet_device->flags & IFF_ALLMULTI) ||
	         (ipg_ethernet_device->flags & IFF_MULTICAST &
	          (ipg_ethernet_device->mc_count >
	           IPG_MULTICAST_HASHTABLE_SIZE)))
	{
		/* NIC to be configured to receive all multicast
		 * frames. */
		receivemode |= IPG_RM_RECEIVEMULTICAST;
	}
	else if (ipg_ethernet_device->flags & IFF_MULTICAST &
	    (ipg_ethernet_device->mc_count > 0))
	{
		/* NIC to be configured to receive selected
		 * multicast addresses. */
		receivemode |= IPG_RM_RECEIVEMULTICASTHASH;
	}

	/* Calculate the bits to set for the 64 bit, IPG HASHTABLE.
	 * The IPG applies a cyclic-redundancy-check (the same CRC
	 * used to calculate the frame data FCS) to the destination
	 * address all incoming multicast frames whose destination
	 * address has the multicast bit set. The least significant
	 * 6 bits of the CRC result are used as an addressing index
	 * into the hash table. If the value of the bit addressed by
	 * this index is a 1, the frame is passed to the host system.
	 */

	/* Clear hashtable. */
	hashtable[0] = 0x00000000;
	hashtable[1] = 0x00000000;

	/* Cycle through all multicast addresses to filter.*/
	for (mc_list_ptr = ipg_ethernet_device->mc_list;
	     mc_list_ptr != NULL;
	     mc_list_ptr = mc_list_ptr->next)
	{
		/* Calculate CRC result for each multicast address. */
		hashindex = ether_crc_le(ETH_ALEN, mc_list_ptr->dmi_addr);

		/* Use only the least significant 6 bits. */
		hashindex = hashindex & 0x3F;

		/* Within "hashtable", set bit number "hashindex"
		 * to a logic 1.
		 */
		set_bit(hashindex, (void*)hashtable);
	}

	/* Write the value of the hashtable, to the 4, 16 bit
	 * HASHTABLE IPG registers.
	 */
	IPG_WRITE_HASHTABLE0(baseaddr, hashtable[0]);
	IPG_WRITE_HASHTABLE1(baseaddr, hashtable[1]);

	IPG_WRITE_RECEIVEMODE(baseaddr, receivemode);

	IPG_DEBUG_MSG("ReceiveMode = %x\n",IPG_READ_RECEIVEMODE(baseaddr));

	return;
}

/*
 * The following code fragment was authored by Donald Becker.
 */

/* The little-endian AUTODIN II ethernet CRC calculations.
   A big-endian version is also available.
   This is slow but compact code.  Do not use this routine for bulk data,
   use a table-based routine instead.
   This is common code and should be moved to net/core/crc.c.
   Chips may use the upper or lower CRC bits, and may reverse and/or invert
   them.  Select the endian-ness that results in minimal calculations.
*/
unsigned const ethernet_polynomial_le = 0xedb88320U;
unsigned ether_crc_le(int length, unsigned char *data)
{
        unsigned int crc = 0xffffffff;  /* Initial value. */
        while(--length >= 0) {
                unsigned char current_octet = *data++;
                int bit;
                for (bit = 8; --bit >= 0; current_octet >>= 1) {
                        if ((crc ^ current_octet) & 1) {
                                crc >>= 1;
                                crc ^= ethernet_polynomial_le;
                        } else
                                crc >>= 1;
                }
        }
        return crc;
}
/*
 * End of code fragment authored by Donald Becker.
 */

int ipg_nic_init(IPG_DEVICE_TYPE *ipg_ethernet_device)
{
	/* Initialize IPG NIC. */

	struct ipg_nic_private	*sp = NULL;

	IPG_DEBUG_MSG("_nic_init\n");

	/* Register the IPG NIC in the list of Ethernet devices. */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
	ipg_ethernet_device = init_etherdev(ipg_ethernet_device,
	                         sizeof(struct ipg_nic_private));
#else
   ipg_ethernet_device=alloc_etherdev(sizeof(struct ipg_nic_private));
#endif

	if (ipg_ethernet_device == NULL)
	{
		printk(KERN_INFO "Could not initialize IP1000 based NIC.\n");
		return -ENODEV;
	}

	/* Reserve memory for ipg_nic_private structure. */
	sp = kmalloc(sizeof(struct ipg_nic_private),
		            GFP_KERNEL);

	if (sp == NULL)
	{
		printk(KERN_INFO "%s: No memory available for IP1000 private strucutre.\n", ipg_ethernet_device->name);
		return -ENOMEM;
	}
	else
	{
		/* Fill the allocated memory space with 0s.
		 * Essentially sets all ipg_nic_private
		 * structure fields to 0.
		 */
		memset(sp, 0, sizeof(*sp));
		ipg_ethernet_device->priv = sp;
	}

	/* Assign the new device to the list of IPG Ethernet devices. */
	sp->next_ipg_ethernet_device = root_ipg_ethernet_device;
	root_ipg_ethernet_device = ipg_ethernet_device;

	/* Declare IPG NIC functions for Ethernet device methods.
	 */
	ipg_ethernet_device->open = &ipg_nic_open;
	ipg_ethernet_device->stop = &ipg_nic_stop;
	ipg_ethernet_device->hard_start_xmit = &ipg_nic_hard_start_xmit;
	ipg_ethernet_device->get_stats = &ipg_nic_get_stats;
	ipg_ethernet_device->set_multicast_list =
	  &ipg_nic_set_multicast_list;
	ipg_ethernet_device->do_ioctl = & ipg_nic_do_ioctl;
	/* rebuild_header not defined. */
	/* hard_header not defined. */
	/* set_config not defined */
	/* set_mac_address not defined. */
	/* header_cache_bind not defined. */
	/* header_cache_update not defined. */
        ipg_ethernet_device->change_mtu = &ipg_nic_change_mtu;
#ifdef IPG_LINUX2_4
        /* ipg_ethernet_device->tx_timouet not defined. */
        /* ipg_ethernet_device->watchdog_timeo not defined. */
#endif

	return 0;
}

int ipg_nic_do_ioctl(IPG_DEVICE_TYPE *ipg_ethernet_device,
                        struct ifreq *req, int cmd)
{
	/* IOCTL commands for IPG NIC.
	 *
	 * SIOCDEVPRIVATE	nothing
	 * SIOCDEVPRIVATE+1	register read
	 *			ifr_data[0] = 0x08, 0x10, 0x20
	 *			ifr_data[1] = register offset
	 *			ifr_data[2] = value read
	 * SIOCDEVPRIVATE+2	register write
	 *			ifr_data[0] = 0x08, 0x10, 0x20
	 *			ifr_data[1] = register offset
	 *			ifr_data[2] = value to write
	 * SIOCDEVPRIVATE+3	GMII register read
	 *			ifr_data[1] = register offset
	 * SIOCDEVPRIVATE+4	GMII register write
	 *			ifr_data[1] = register offset
	 *			ifr_data[2] = value to write
	 * SIOCDEVPRIVATE+5	PCI register read
	 *			ifr_data[0] = 0x08, 0x10, 0x20
	 *			ifr_data[1] = register offset
	 *			ifr_data[2] = value read
	 * SIOCDEVPRIVATE+6	PCI register write
	 *			ifr_data[0] = 0x08, 0x10, 0x20
	 *			ifr_data[1] = register offset
	 *			ifr_data[2] = value to write
	 *
	 */

	u8				val8;
	u16				val16;
	u32				val32;
	unsigned int			*data;
        int                             phyaddr = 0;
	u32				baseaddr;
	struct ipg_nic_private	*sp = (struct ipg_nic_private *)
					      ipg_ethernet_device->priv;

	IPG_DEBUG_MSG("_nic_do_ioctl\n");

        data = (unsigned int *)&req->ifr_data;
	baseaddr = ipg_ethernet_device->base_addr;

        switch(cmd)
	{
	        case SIOCDEVPRIVATE:
		return 0;

		case SIOCDEVPRIVATE+1:
		switch(data[0])
		{
			case 0x08:
			data[2] = IPG_READ_BYTEREG(baseaddr + data[1]);
			return 0;

			case 0x10:
			data[2] = IPG_READ_WORDREG(baseaddr + data[1]);
			return 0;

			case 0x20:
			data[2] = IPG_READ_LONGREG(baseaddr + data[1]);
			return 0;

			default:
			data[2] = 0x00;
			return -EINVAL;
		}

		case SIOCDEVPRIVATE+2:
		switch(data[0])
		{
			case 0x08:
			IPG_WRITE_BYTEREG(baseaddr + data[1], data[2]);
			return 0;

			case 0x10:
			IPG_WRITE_WORDREG(baseaddr + data[1], data[2]);
			return 0;

			case 0x20:
			IPG_WRITE_LONGREG(baseaddr + data[1], data[2]);
			return 0;

			default:
			return -EINVAL;
		}

		case SIOCDEVPRIVATE+3:
        	phyaddr = ipg_find_phyaddr(ipg_ethernet_device);

		if (phyaddr == -1)
			return -EINVAL;

		data[2] = read_phy_register(ipg_ethernet_device,
		                            phyaddr, data[1]);

		return 0;

		case SIOCDEVPRIVATE+4:
        	phyaddr = ipg_find_phyaddr(ipg_ethernet_device);

		if (phyaddr == -1)
			return -EINVAL;

		write_phy_register(ipg_ethernet_device,
		                   phyaddr, data[1], (u16)data[2]);

		return 0;

		case SIOCDEVPRIVATE+5:
		switch(data[0])
		{
			case 0x08:
			pci_read_config_byte(sp->ipg_pci_device,data[1],
			                     &val8);
			data[2] = (unsigned int)val8;
			return 0;

			case 0x10:
			pci_read_config_word(sp->ipg_pci_device,data[1],
			                     &val16);
			data[2] = (unsigned int)val16;
			return 0;

			case 0x20:
			pci_read_config_dword(sp->ipg_pci_device,data[1],
			                     &val32);
			data[2] = (unsigned int)val32;
			return 0;

			default:
			data[2] = 0x00;
			return -EINVAL;
		}

		case SIOCDEVPRIVATE+6:
		switch(data[0])
		{
			case 0x08:
			pci_write_config_byte(sp->ipg_pci_device,data[1],
			                      (u8)data[2]);
			return 0;

			case 0x10:
			pci_write_config_word(sp->ipg_pci_device,data[1],
			                      (u16)data[2]);
			return 0;

			case 0x20:
			pci_write_config_dword(sp->ipg_pci_device,data[1],
			                       (u32)data[2]);
			return 0;

			default:
			return -EINVAL;
		}

		case SIOCSIFMTU:
		{
			return 0;
		}

		default:
		return -EOPNOTSUPP;
        }
}

int ipg_nic_change_mtu(IPG_DEVICE_TYPE *ipg_ethernet_device,
                          int new_mtu)
{
        /* Function to accomodate changes to Maximum Transfer Unit
         * (or MTU) of IPG NIC. Cannot use default function since
         * the default will not allow for MTU > 1500 bytes.
         */

	IPG_DEBUG_MSG("_nic_change_mtu\n");

        /* Check that the new MTU value is between 68 (14 byte header, 46
         * byte payload, 4 byte FCS) and IPG_MAX_RXFRAME_SIZE, which
         * corresponds to the MAXFRAMESIZE register in the IPG.
         */
        if ((new_mtu < 68) || (new_mtu > IPG_MAX_RXFRAME_SIZE))
        {
                return -EINVAL;
        }

        ipg_ethernet_device->mtu = new_mtu;

        return 0;
}

#ifdef IPG_LINUX2_2
int ipg_pcibussearch_linux2_2(void)
{
	/* Search for IPG based devices on the Ethernet bus.
	 * Code specific to the Linux 2.2 kernel.
	 */

	int				error;
	int				i;
	int				foundipgnic = 0;
	IPG_DEVICE_TYPE		*ipg_ethernet_device;
	struct ipg_nic_private	*sp;
	struct pci_dev			*ipg_pci_device;

	IPG_DEBUG_MSG("_pcibussearch_linux_2_2\n");

	for(i=0; ; i++)
	{
		if (nics_supported[i].vendorid == 0xFFFF)
			break;

		/* Start with the list of all PCI devices. */
		ipg_pci_device = pci_devices;

		/* Check each entry in the list of all PCI devices. */
		while (ipg_pci_device)
		{
			if ((ipg_pci_device->vendor ==
			     nics_supported[i].vendorid) &&
			    (ipg_pci_device->device ==
			     nics_supported[i].deviceid))
			{
				foundipgnic = 1;

				printk(KERN_INFO "%s found.\n",
				       nics_supported[i].NICname);
		                printk(KERN_INFO "Bus %x Slot %x\n",
				       ipg_pci_device->bus->number,
				       PCI_SLOT(ipg_pci_device->devfn));

				ipg_ethernet_device = NULL;

				/* A IPG based NIC was found on the PCI bus.
				 * Initialize the NIC.
				 */
				error = ipg_nic_init(ipg_ethernet_device);
				if (error < 0)
				{
					printk(KERN_INFO "Could not intialize IP1000 based NIC.\n");
					return error;
				}
				else
				{
					printk(KERN_INFO "Ethernet device registered as: %s\n",
					   root_ipg_ethernet_device->name);
				}

				sp = (struct ipg_nic_private *)
				     root_ipg_ethernet_device->priv;

				/* Save the pointer to the PCI device
				 * information.
				 */
				sp->ipg_pci_device = ipg_pci_device;
			}

			/* Move onto the next PCI device in the list. */
			ipg_pci_device = ipg_pci_device->next;
		}

	}

	return foundipgnic;
}
#endif

#ifdef IPG_LINUX2_4

struct	pci_device_id	pci_devices_supported[] =
{
	{PCI_VENDOR_ID_ICPLUS,
	 PCI_DEVICE_ID_IP1000,
	 PCI_ANY_ID,
	 PCI_ANY_ID,
	 0x020000,
	 0xFFFFFF,
	 0},

	{PCI_VENDOR_ID_SUNDANCE,
	 PCI_DEVICE_ID_SUNDANCE_ST2021,
	 PCI_ANY_ID,
	 PCI_ANY_ID,
	 0x020000,
	 0xFFFFFF,
	 1},

	{PCI_VENDOR_ID_SUNDANCE,
	 PCI_DEVICE_ID_TAMARACK_TC9020_9021,
	 PCI_ANY_ID,
	 PCI_ANY_ID,
	 0x020000,
	 0xFFFFFF,
	 2},

	{PCI_VENDOR_ID_DLINK,
	 PCI_DEVICE_ID_DLINK_1002,
	 PCI_ANY_ID,
	 PCI_ANY_ID,
	 0x020000,
	 0xFFFFFF,
	 3},

	{0,}
};
MODULE_DEVICE_TABLE(pci, pci_devices_supported);

/* PCI driver structure for Linux 2.4. */
struct  pci_driver      ipg_pci_driver =
{
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)	
        name: IPG_DRIVER_NAME,
        id_table: pci_devices_supported,
        probe: ipg_pciprobe_linux2_4,
        remove: ipg_pciremove_linux2_4,
#else
		  {NULL, NULL},
        IPG_DRIVER_NAME,
        pci_devices_supported,
        ipg_pciprobe_linux2_4,
        ipg_pciremove_linux2_4,
        NULL,
        NULL
#endif
};

void ipg_pciremove_linux2_4(struct pci_dev *ipg_pci_device_to_remove)
{
	/* Remove function called when a IPG device is
	 * to be shut down.
	 */

	IPG_DEVICE_TYPE		*prev_ipg_ethernet_device = NULL;
	IPG_DEVICE_TYPE		*ipg_ethernet_device = NULL;
	IPG_DEVICE_TYPE		*ipg_ethernet_device_to_remove =
	                                  NULL;
	struct ipg_nic_private	*prev_sp = NULL;
	struct ipg_nic_private	*sp = NULL;
	struct ipg_nic_private	*sp_to_remove = NULL;

	IPG_DEBUG_MSG("_pciremove_linux2_4\n");

	ipg_ethernet_device = root_ipg_ethernet_device;

	/* Move through list of Ethernet devices looking for
	 * a match.
	 */
	while (ipg_ethernet_device)
	{
		sp = (struct ipg_nic_private *)
		               (ipg_ethernet_device->priv);

		if (sp->ipg_pci_device == ipg_pci_device_to_remove)
		{
			/* Save the pointer to the previous Ethernet
			 * device.
			 */
			ipg_ethernet_device_to_remove =
			  ipg_ethernet_device;

			sp_to_remove = sp;

			break;
		}

		/* Save the "previous" device in the list. */
		prev_ipg_ethernet_device = ipg_ethernet_device;

		/* Retrieve next Ethernet device to be
		 * released.
		 */
		ipg_ethernet_device = sp->next_ipg_ethernet_device;
	}

	/* Check if there is a device to remove. */
	if (ipg_ethernet_device_to_remove == NULL)
	{
		/* There are no Ethernet devices to remove. */
		printk(KERN_INFO "A device remove request does not match with any Ethernet devices.\n");

		return;
	}

	/* Check to see if we are removing the root device in the list. */
	if (root_ipg_ethernet_device == ipg_ethernet_device_to_remove)
	{
		/* Change the root Ethernet device to the next device to be
		 * released.
		 */
		root_ipg_ethernet_device =
		  sp_to_remove->next_ipg_ethernet_device;
	}
	else if (sp_to_remove->next_ipg_ethernet_device != NULL)
		/* Check if we need to re-link the list of devices. */
	{
		/* If the "previous" Ethernet device is NULL,
		 * the device is at the head of the list, and
		 * no re-linking is needed.
		 */
		prev_sp = (struct ipg_nic_private *)
		          (prev_ipg_ethernet_device->priv);

		prev_sp->next_ipg_ethernet_device =
				sp_to_remove->next_ipg_ethernet_device;
	}

	/* Free memory associated with Ethernet device's
	 * private data structure.
	 */
	if (sp_to_remove)
	{
		kfree(sp_to_remove);
	}

	printk(KERN_INFO "Un-registering Ethernet device %s\n",
	       ipg_ethernet_device_to_remove->name);

	/* Un-register Ethernet device. */
	unregister_netdev(ipg_ethernet_device_to_remove);

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)	
#ifdef CONFIG_NET_IPG_IO
	pci_release_regions(ipg_pci_device_to_remove);
#else
	iounmap((void *)ipg_ethernet_device->base_addr);
#endif
#endif


	/* Free memory associated with Ethernet device. */
	if (ipg_ethernet_device_to_remove)
	{
		kfree(ipg_ethernet_device_to_remove);
	}
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)	
   pci_set_drvdata(ipg_pci_device_to_remove,NULL);
#endif
	return;
}

int ipg_pciprobe_linux2_4(struct pci_dev *ipg_pci_device,
                              const struct pci_device_id *id)
{
	/* Probe function called when a IPG device is found
	 * on the PCI bus.
	 */

	int				error;
	int				i;
	IPG_DEVICE_TYPE		*ipg_ethernet_device = NULL;
	struct ipg_nic_private	*sp;

	IPG_DEBUG_MSG("_pciprobe_linux2_4\n");

	/* Enable IPG PCI device in Linux system. */
	error = pci_enable_device(ipg_pci_device);
	if (error < 0)
	{
		return error;
	}

	/* Get the index for the driver description string. */
	i = id->driver_data;

	printk(KERN_INFO "%s found.\n",
	       nics_supported[i].NICname);
        printk(KERN_INFO "Bus %x Slot %x\n",
	       ipg_pci_device->bus->number,
	       PCI_SLOT(ipg_pci_device->devfn));

	/* Configure IPG PCI device within Linux system as
	 * a bus master.
	 */
	pci_set_master(ipg_pci_device);

	/* Indicate that we can supply 32 bits of address
	 * during PCI bus mastering.
	 */
	if (pci_dma_supported(ipg_pci_device, 0xFFFFFFFF) < 0)
	{
		printk(KERN_INFO "pci_dma_supported failed.\n");
		return -ENODEV;
	}
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
	/* A IPG based NIC was found on the PCI bus.
	 * Initialize the NIC.
	 */
	if ((ipg_nic_init(ipg_ethernet_device)) < 0)
	{
		printk(KERN_INFO "Could not intialize IP1000 based NIC.\n");
		return -ENODEV;
	}
	else
	{
		printk(KERN_INFO "Ethernet device registered as: %s\n",
		       root_ipg_ethernet_device->name);
	}
	sp = (struct ipg_nic_private *)root_ipg_ethernet_device->priv;
	/* Save the pointer to the PCI device information. */
	sp->ipg_pci_device = ipg_pci_device;
#else 
	if ((ipg_nic_init(ipg_ethernet_device)) < 0)
	{
		printk(KERN_INFO "Could not intialize IP1000 based NIC.\n");
		return -ENODEV;
	}
	ipg_ethernet_device=root_ipg_ethernet_device;
   SET_MODULE_OWNER(ipg_ethernet_device);
   pci_request_regions(ipg_pci_device,DRV_NAME);
//   if(pci_request_regions(ipg_pci_device,DRV_NAME)=err) goto xxx;

#ifdef CONFIG_NET_IPG_IO
	ipg_ethernet_device->base_addr = pci_resource_start(ipg_pci_device, 0)&0xffffff80;////20040826Jesse_mask_BaseAddr:Mask IOBaseAddr[bit0~6]
#else
	ipg_ethernet_device->base_addr = pci_resource_start(ipg_pci_device, 1)&0xffffff80;//20040826Jesse_mask_BaseAddr:Mask MemBaseAddr[bit0~6]
	ipg_ethernet_device->base_addr = (long) ioremap (ipg_ethernet_device->base_addr, netdev_io_size);
//	if (!ioaddr)goto err_out_res;
#endif//#ifdef CONFIG_NET_IPG_IO
	sp = (struct ipg_nic_private *)root_ipg_ethernet_device->priv;
	/* Save the pointer to the PCI device information. */
	sp->ipg_pci_device = ipg_pci_device;
	
   pci_set_drvdata(ipg_pci_device,ipg_ethernet_device);
  
   i = register_netdev(ipg_ethernet_device);
//	if (i)goto err_out_unmap_rx;

	printk(KERN_INFO "Ethernet device registered as: %s\n",
		       ipg_ethernet_device->name);
#endif//#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)


	return 0;
}
#endif //#ifdef IPG_LINUX2_4



#ifdef MODULE
/* A modularized driver, i.e. not part of mainstream Linux
 * kernel distribution.
 */
int	init_module(void)
{
	/* Initialize the IPG driver module. */

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)	
	int	foundipgnic = 0;
#endif

	IPG_DEBUG_MSG("init_module\n");
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)	
	return pci_register_driver(&ipg_pci_driver);
#else
	
        printk(KERN_INFO "%s", version);

	/* Define EXPORT_NO_SYMBOLS macro for specifying no
	 * symbols to be exported.
 	 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
	EXPORT_NO_SYMBOLS;
#endif

	/* The CONFIG_PCI macro is defined for systems which
	 * have a PCI bus. If the macro is undefined, there is no
	 * PCI bus and the ipg cannot be utilized.
	 */
#ifdef CONFIG_PCI

	printk(KERN_INFO "IPG module searching for Ethernet devices on PCI bus...\n");

#ifdef IPG_LINUX2_2
	foundipgnic = ipg_pcibussearch_linux2_2();
#endif

#ifdef IPG_LINUX2_4
	if (pci_register_driver(&ipg_pci_driver) != 0)
	{
		foundipgnic = 1;
	}
	else
	{
		foundipgnic = 0;
	}
#endif

	if (foundipgnic == 0)
	{
		printk(KERN_INFO " IP1000 based Ethernet device not found on PCI bus.\n");
		return -ENODEV;
	}

#else /* PCI not available. */
		/* PCI not supported in system. */
		printk(KERN_INFO "No PCI bus, required by IP1000.\n");
		return -ENODEV;
#endif

	printk(KERN_INFO "IPG module loaded.\n");

	return 0;
#endif
}

void	cleanup_module(void)
{
	/* Clean up system modifications made by IPG driver
	 * module.
	 */

#ifdef IPG_LINUX2_2

	IPG_DEVICE_TYPE		*next_ipg_ethernet_device;
	struct ipg_nic_private	*sp;

	IPG_DEBUG_MSG("cleanup_module\n");

	/* Unregister all IPG NICs from the list of Ethernet
	 * devices.
	 */
	while (root_ipg_ethernet_device)
	{
		sp = (struct ipg_nic_private *)
		     (root_ipg_ethernet_device->priv);

		/* Retrieve next Ethernet device to be
		 * released.
		 */
		next_ipg_ethernet_device = sp->next_ipg_ethernet_device;

		printk(KERN_INFO "Un-registering Ethernet device %s\n",
		       root_ipg_ethernet_device->name);

		/* Un-register Ethernet device. */
		unregister_netdev(root_ipg_ethernet_device);

		/* Free memory associated with Ethernet device's
		 * private data structure.
		 */
		kfree(sp);

		/* Free memory associated with Ethernet device. */
		kfree(root_ipg_ethernet_device);

		/* Move to next Ethernet device. */
		root_ipg_ethernet_device = next_ipg_ethernet_device;
	}

#endif

#ifdef IPG_LINUX2_4
	IPG_DEBUG_MSG("cleanup_module\n");

	pci_unregister_driver(&ipg_pci_driver);
#endif

	printk(KERN_INFO "IPG module unloaded.\n");
}
#else /* not MODULE */

int	ipg_nic_probe(IPG_DEVICE_TYPE *dev)
{
	int	error;

	IPG_DEBUG_MSG("_nic_probe\n");

	error = ipg_nic_init(dev);
	if (error < 0)
		return -ENODEV;
        printk(KERN_INFO "%s", version);

	return 0;
}

#endif
void bSetPhyDefaultParam(unsigned char Rev,
		IPG_DEVICE_TYPE *ipg_ethernet_device,int phy_address)
{
	unsigned short Length;
	unsigned char Revision;
	unsigned short *pPHYParam;
	unsigned short address,value;
      	
	pPHYParam = &DefaultPhyParam[0];
	Length = *pPHYParam & 0x00FF;
	Revision = (unsigned char) ((*pPHYParam) >> 8);
	pPHYParam++;
	while(Length != 0)
	{
		if(Rev == Revision)
		{
			while(Length >1)
			{
				address=*pPHYParam; 
				value=*(pPHYParam+1);
				pPHYParam+=2;
				write_phy_register(ipg_ethernet_device,phy_address,address, value);
				Length -= 4;
			}

			break;
		}
		else // advanced to next revision
		{
			pPHYParam += Length/2;
			Length = *pPHYParam & 0x00FF;
			Revision = (unsigned char) ((*pPHYParam) >> 8);
			pPHYParam++;
		}
	}
	return;
}

/*JES20040127EEPROM*/
int read_eeprom(IPG_DEVICE_TYPE *ipg_ethernet_device, int eep_addr)
{
   u32	baseaddr;
   int            i = 1000;
	baseaddr = ipg_ethernet_device->base_addr;
	
   IPG_WRITE_EEPROMCTRL(baseaddr,  IPG_EC_EEPROM_READOPCODE | (eep_addr & 0xff));
   while (i-- > 0) {
   	mdelay(10);
        if (!(IPG_READ_EEPROMCTRL(baseaddr)&IPG_EC_EEPROM_BUSY)) {
                return IPG_READ_EEPROMDATA (baseaddr);
	}
   }
   return 0;
}

/* Write EEPROM JES20040127EEPROM */
//void Eeprom_Write(unsigned int card_index, unsigned eep_addr, unsigned writedata)
void write_eeprom(IPG_DEVICE_TYPE *ipg_ethernet_device, unsigned int eep_addr, unsigned int writedata)
{
/* 
   u32	baseaddr;
   baseaddr = ipg_ethernet_device->base_addr;
   IPG_WRITE_EEPROMDATA(baseaddr, writedata );
   while ( (IPG_READ_EEPROMCTRL(baseaddr) & IPG_EC_EEPROM_BUSY ) ) {   
   }
   IPG_WRITE_EEPROMCTRL (baseaddr, 0xC0 );
   while ( (IPG_READ_EEPROMCTRL(baseaddr)&IPG_EC_EEPROM_BUSY) ) {   
   }
   IPG_WRITE_EEPROMCTRL(baseaddr, IPG_EC_EEPROM_WRITEOPCODE | (eep_addr & 0xff) );
   while ( (IPG_READ_EEPROMCTRL(baseaddr)&IPG_EC_EEPROM_BUSY) ) {   
   }
   
   return;*/
}

/* Set LED_Mode JES20040127EEPROM */
void Set_LED_Mode(IPG_DEVICE_TYPE *ipg_ethernet_device)
{
   u32 LED_Mode_Value;
   u32	baseaddr;	
   struct ipg_nic_private	*sp = (struct ipg_nic_private *)
					      ipg_ethernet_device->priv;
	baseaddr = ipg_ethernet_device->base_addr;					      
   
   LED_Mode_Value=IPG_READ_ASICCTRL(baseaddr);
   LED_Mode_Value &= ~(IPG_AC_LED_MODE_BIT_1 | IPG_AC_LED_MODE |IPG_AC_LED_SPEED);
   
   if((sp->LED_Mode & 0x03) > 1){
   	/* Write Asic Control Bit 29 */
   	LED_Mode_Value |=IPG_AC_LED_MODE_BIT_1;
   }
   if((sp->LED_Mode & 0x01) == 1){
        /* Write Asic Control Bit 14 */
        LED_Mode_Value |=IPG_AC_LED_MODE;
   }
   if((sp->LED_Mode & 0x08) == 8){
        /* Write Asic Control Bit 27 */
        LED_Mode_Value |=IPG_AC_LED_SPEED;
   }   
   IPG_WRITE_ASICCTRL(baseaddr,LED_Mode_Value);   	
   
   return;
}

/* Set PHYSet JES20040127EEPROM */
void Set_PHYSet(IPG_DEVICE_TYPE *ipg_ethernet_device)
{
   int PHYSet_Value;
   u32	baseaddr;	
   struct ipg_nic_private	*sp = (struct ipg_nic_private *)
					      ipg_ethernet_device->priv;
	baseaddr = ipg_ethernet_device->base_addr;					      
   
   PHYSet_Value=IPG_READ_PHYSET(baseaddr);
   PHYSet_Value &= ~(IPG_PS_MEM_LENB9B | IPG_PS_MEM_LEN9 |IPG_PS_NON_COMPDET);
   PHYSet_Value |= ((sp->LED_Mode & 0x70) >> 4);
   IPG_WRITE_PHYSET(baseaddr,PHYSet_Value);

   return;
}
/* end ipg.c */
