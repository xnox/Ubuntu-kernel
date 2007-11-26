/*
 *
 * ipg.h
 *
 * Include file with structures for Gigabit Ethernet
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
 * 0.2  5/22/01  Added PCI_DEVICE_ID_TAMARACK_TC9020_9021_ALT
 *               to nics_supported[] array.
 */

/* IPG Ethernet device structure, used for removing module. */
IPG_DEVICE_TYPE *root_ipg_ethernet_device = NULL;

/* Transmit Frame Descriptor. The IPG supports 15 fragments,
 * however Linux requires only a single fragment. Note, each
 * TFD field is 64 bits wide.
 */
struct TFD
{
	u64		TFDNextPtr;
	u64		TFC;
	u64		FragInfo;
};

/* Receive Frame Descriptor. Note, each RFD field is 64 bits wide.
 */
struct RFD
{
	u64		RFDNextPtr;
	u64		RFS;
	u64		FragInfo;
};

#ifdef IPG_LINUX2_4
struct ipg_dmabuff
{
	dma_addr_t	dmahandle;
	unsigned long	len;
};

#endif
struct SJumbo
{
	int FoundStart;
	int CurrentSize;
	struct sk_buff			*skb;
};
/* Structure of IPG NIC specific data. */
struct ipg_nic_private
{
        struct TFD    		*TFDList;
        struct RFD    		*RFDList;
#ifdef IPG_LINUX2_4
	dma_addr_t		TFDListDMAhandle;
	dma_addr_t		RFDListDMAhandle;
	struct ipg_dmabuff	TxBuffDMAhandle[IPG_TFDLIST_LENGTH];
	struct ipg_dmabuff	RxBuffDMAhandle[IPG_RFDLIST_LENGTH];
#endif
	struct sk_buff		*TxBuff[IPG_TFDLIST_LENGTH];
	struct sk_buff		*RxBuff[IPG_RFDLIST_LENGTH];
	u16			CurrentTxFrameID;
	int			CurrentTFD;
	int			LastFreedTxBuff;
	int			CurrentRFD;
// Add by Grace 2005/05/19
#ifdef CONFIG_NET_IPG_JUMBO_FRAME
    	struct SJumbo      Jumbo;
#endif
	int			LastRestoredRxBuff;
	int			RxBuffNotReady;
	struct pci_dev		*ipg_pci_device;
	IPG_STATS_TYPE	stats;
	IPG_DEVICE_TYPE	*next_ipg_ethernet_device;
        spinlock_t              lock;
	int			tenmbpsmode;
	
	/*Jesse20040128EEPROM_VALUE */
	u16         LED_Mode;
	u16         StationAddr0;   /* Station Address in EEPROM Reg 0x10 */
   u16         StationAddr1;   /* Station Address in EEPROM Reg 0x11 */        
	u16         StationAddr2;   /* Station Address in EEPROM Reg 0x12 */   

#ifdef IPG_DEBUG
	int			TFDunavailCount;
	int			RFDlistendCount;
	int			RFDListCheckedCount;
	int			EmptyRFDListCount;
#endif
};

struct	nic_id
{
	char*		NICname;
	int		vendorid;
	int		deviceid;
};

struct	nic_id	nics_supported[] =
{
	{"IC PLUS IP1000 1000/100/10 based NIC",
	 PCI_VENDOR_ID_ICPLUS,
	 PCI_DEVICE_ID_IP1000},
	{"Sundance Technology ST2021 based NIC",
     PCI_VENDOR_ID_SUNDANCE,
	 PCI_DEVICE_ID_SUNDANCE_ST2021},
	{"Tamarack Microelectronics TC9020/9021 based NIC",
	 PCI_VENDOR_ID_SUNDANCE,
	 PCI_DEVICE_ID_TAMARACK_TC9020_9021},
	{"Tamarack Microelectronics TC9020/9021 based NIC",
	 PCI_VENDOR_ID_SUNDANCE,
	 PCI_DEVICE_ID_TAMARACK_TC9020_9021_ALT},
	{"D-Link NIC",
	 PCI_VENDOR_ID_DLINK,
	 PCI_DEVICE_ID_DLINK_1002},
	{"D-Link NIC IP1000A",
	 PCI_VENDOR_ID_DLINK,
	 PCI_DEVICE_ID_DLINK_IP1000A},
	 
	{"N/A", 0xFFFF, 0}
};

/* end ipg_structs.h */
