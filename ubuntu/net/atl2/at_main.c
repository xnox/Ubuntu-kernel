/*
 * Copyright(c) 2005 - 2006 Attansic Corporation. All rights reserved.
 * Copyright(c) 2006 xiong huang <xiong.huang@atheros.com>
 *
 * Derived from Intel e1000 driver
 * Copyright(c) 1999 - 2005 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59
 * Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * There are a lot of defines in here that are unused and/or have cryptic
 * names.  Please leave them alone, as they're the closest thing we have
 * to a spec from Attansic at present. *ahem* -- CHS
 */


#include "at.h"


char at_driver_name[] = "ATL2";
char at_driver_string[] = "Attansic(R) L2 Ethernet Network Driver";
#define DRV_VERSION "1.0.40.2"
char at_driver_version[] = DRV_VERSION;
char at_copyright[] = "Copyright (c) 2006 Attansic Corporation.";


/*
 * at_pci_tbl - PCI Device ID Table
 *
 * Wildcard entries (PCI_ANY_ID) should come last
 * Last entry must be all 0s
 *
 * { Vendor ID, Device ID, SubVendor ID, SubDevice ID,
 *   Class, Class Mask, private data (not used) }
 */
static struct pci_device_id at_pci_tbl[] = {
    ATTANSIC_ETHERNET_DEVICE(0x2048),
    /* required last entry */
    {0,}
};

MODULE_DEVICE_TABLE(pci, at_pci_tbl);

int at_up(struct at_adapter *adapter);
void at_down(struct at_adapter *adapter);
int at_reset(struct at_adapter *adapter);
int32_t at_setup_ring_resources(struct at_adapter *adapter);
void at_free_ring_resources(struct at_adapter *adapter);
void at_reinit_locked(struct at_adapter *adapter);

/* Local Function Prototypes */
static int at_init_module(void);
static void at_exit_module(void);
static int at_probe(struct pci_dev *pdev, const struct pci_device_id *ent);
static void __devexit at_remove(struct pci_dev *pdev);
static int at_sw_init(struct at_adapter *adapter);
static int at_open(struct net_device *netdev);
static int at_close(struct net_device *netdev);
static int at_xmit_frame(struct sk_buff *skb, struct net_device *netdev);
static struct net_device_stats * at_get_stats(struct net_device *netdev);
static int at_change_mtu(struct net_device *netdev, int new_mtu);
static void at_set_multi(struct net_device *netdev);
static int at_set_mac(struct net_device *netdev, void *p);
static int at_ioctl(struct net_device *netdev, struct ifreq *ifr, int cmd);
static void at_tx_timeout(struct net_device *dev);
static irqreturn_t at_intr(int irq, void *data);
static void at_intr_rx(struct at_adapter* adapter);
static void at_intr_tx(struct at_adapter* adapter);
void at_power_up_phy(struct at_adapter *adapter);
static void at_power_down_phy(struct at_adapter *adapter);
static void at_watchdog(unsigned long data);
static void at_phy_config(unsigned long data);
static void at_reset_task(struct work_struct *work);
static void at_link_chg_task(struct work_struct *work);
static void at_check_for_link(struct at_adapter* adapter);
void at_set_ethtool_ops(struct net_device *netdev);
static int at_check_link(struct at_adapter* adapter);
void init_ring_ptrs(struct at_adapter *adapter);
static int at_configure(struct at_adapter *adapter);

#define COPYBREAK_DEFAULT 256
static unsigned int copybreak __read_mostly = COPYBREAK_DEFAULT;
module_param(copybreak, uint, 0644);
MODULE_PARM_DESC(copybreak,
	"Maximum size of packet that is copied to a new buffer on receive");
	
	
#ifdef SIOCGMIIPHY
static int at_mii_ioctl(struct net_device *netdev, struct ifreq *ifr, int cmd);
#endif


#ifdef NETIF_F_HW_VLAN_TX
static void at_vlan_rx_register(struct net_device *netdev, struct vlan_group *grp);
static void at_vlan_rx_add_vid(struct net_device *netdev, u16 vid);
static void at_vlan_rx_kill_vid(struct net_device *netdev, u16 vid);
static void at_restore_vlan(struct at_adapter *adapter);
#endif

static int at_suspend(struct pci_dev *pdev, pm_message_t state);
#ifdef CONFIG_PM
static int at_resume(struct pci_dev *pdev);
#endif

#ifndef USE_REBOOT_NOTIFIER
static void at_shutdown(struct pci_dev *pdev);
#else
static int at_notify_reboot(struct notifier_block *nb, unsigned long event, void *p);

struct notifier_block at_notifier_reboot = {
    .notifier_call  = at_notify_reboot,
    .next       = NULL,
    .priority   = 0
};
#endif

/* Exported from other modules */

extern void at_check_options(struct at_adapter *adapter);
//extern int at_ethtool_ioctl(struct net_device *netdev, struct ifreq *ifr);
#ifdef SIOCDEVPRIVATE
extern int at_priv_ioctl(struct net_device* netdev, struct ifreq* ifr);
#endif

#ifdef CONFIG_AT_PCI_ERS
static pci_ers_result_t at_io_error_detected(struct pci_dev *pdev,
                     pci_channel_state_t state);
static pci_ers_result_t at_io_slot_reset(struct pci_dev *pdev);
static void at_io_resume(struct pci_dev *pdev);

static struct pci_error_handlers at_err_handler = {
	.error_detected = at_io_error_detected,
	.slot_reset = at_io_slot_reset,
	.resume = at_io_resume,
};
#endif

static struct pci_driver at_driver = {
    .name     = at_driver_name,
    .id_table = at_pci_tbl,
    .probe    = at_probe,
    .remove   = __devexit_p(at_remove),
    /* Power Managment Hooks */
#ifdef CONFIG_PM
    .suspend  = at_suspend,
    .resume   = at_resume,
#endif
#ifndef USE_REBOOT_NOTIFIER
	.shutdown = at_shutdown,
#endif
#ifdef CONFIG_AT_PCI_ERS
	.err_handler = &at_err_handler
#endif

};

MODULE_AUTHOR("Attansic Corporation, <xiong_huang@attansic.com>");
MODULE_DESCRIPTION("Attansic 100M Ethernet Network Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRV_VERSION);

/**
 * at_init_module - Driver Registration Routine
 *
 * at_init_module is the first routine called when the driver is
 * loaded. All it does is register with the PCI subsystem.
 **/

static int __init
at_init_module(void)
{
    int ret;
    printk(KERN_INFO "%s - version %s\n",
           at_driver_string, at_driver_version);

    printk(KERN_INFO "%s\n", at_copyright);

	ret = pci_register_driver(&at_driver);
#ifdef USE_REBOOT_NOTIFIER
	if (ret >= 0) {
		register_reboot_notifier(&at_notifier_reboot);
	}
#endif
	if (copybreak != COPYBREAK_DEFAULT) {
		if (copybreak == 0)
			printk(KERN_INFO "ATL2: copybreak disabled\n");
		else
			printk(KERN_INFO "ATL2: copybreak enabled for "
			       "packets <= %u bytes\n", copybreak);
	}
	return ret;
}

module_init(at_init_module);

/**
 * at_exit_module - Driver Exit Cleanup Routine
 *
 * at_exit_module is called just before the driver is removed
 * from memory.
 **/

static void __exit
at_exit_module(void)
{
#ifdef USE_REBOOT_NOTIFIER
	unregister_reboot_notifier(&at_notifier_reboot);
#endif
	pci_unregister_driver(&at_driver);
	
}

module_exit(at_exit_module);



static int at_request_irq(struct at_adapter *adapter)
{
	struct net_device *netdev = adapter->netdev;
	int flags, err = 0;

	flags = IRQF_SHARED;
	if ((err = request_irq(adapter->pdev->irq, &at_intr, flags,
	                       netdev->name, netdev))) {
		AT_DBG("Unable to allocate interrupt Error: %d\n", err);
	}

	return err;
}


static void at_setup_pcicmd(struct pci_dev* pdev)
{
	u16 cmd;

	pci_read_config_word(pdev, PCI_COMMAND, &cmd);
	
	if (cmd & PCI_COMMAND_INTX_DISABLE)
		cmd &= ~PCI_COMMAND_INTX_DISABLE;
	if (cmd & PCI_COMMAND_IO)
		cmd &= ~PCI_COMMAND_IO;
	if (0 == (cmd & PCI_COMMAND_MEMORY))
		cmd |= PCI_COMMAND_MEMORY;
	if (0 == (cmd & PCI_COMMAND_MASTER))
		cmd |= PCI_COMMAND_MASTER;
	pci_write_config_word(pdev, PCI_COMMAND, cmd);
	
	/* 
	 * some motherboards BIOS(PXE/EFI) driver may set PME
	 * while they transfer control to OS (Windows/Linux)
	 * so we should clear this bit before NIC work normally
	 */
	pci_write_config_dword(pdev, REG_PM_CTRLSTAT, 0);			
}




/**
 * at_probe - Device Initialization Routine
 * @pdev: PCI device information struct
 * @ent: entry in at_pci_tbl
 *
 * Returns 0 on success, negative on failure
 *
 * at_probe initializes an adapter identified by a pci_dev structure.
 * The OS initialization, configuring of the adapter private structure,
 * and a hardware reset occur.
 **/

static int __devinit
at_probe(struct pci_dev *pdev,
            const struct pci_device_id *ent)
{
    struct net_device *netdev;
    struct at_adapter *adapter;
    static int cards_found = 0;
    unsigned long mmio_start;
    int mmio_len;
    boolean_t pci_using_64 = TRUE;
    int err;

    DEBUGFUNC("at_probe !");

    if((err = pci_enable_device(pdev)))
        return err;

	if (!(err = pci_set_dma_mask(pdev, DMA_64BIT_MASK)) &&
	    !(err = pci_set_consistent_dma_mask(pdev, DMA_64BIT_MASK))) {
		pci_using_64 = TRUE; 
	} else {
		if ((err = pci_set_dma_mask(pdev, DMA_32BIT_MASK)) &&
		    (err = pci_set_consistent_dma_mask(pdev, DMA_32BIT_MASK))) {
			AT_ERR("No usable DMA configuration, aborting\n");
			goto err_dma;
		}
		pci_using_64 = FALSE;
	}		
		
   
    // Mark all PCI regions associated with PCI device 
    // pdev as being reserved by owner at_driver_name
    if((err = pci_request_regions(pdev, at_driver_name)))
       goto err_pci_reg;

    // Enables bus-mastering on the device and calls 
    // pcibios_set_master to do the needed arch specific settings
    pci_set_master(pdev);

	err = -ENOMEM;
    netdev = alloc_etherdev(sizeof(struct at_adapter));
    if(!netdev)
        goto err_alloc_etherdev;
    
    SET_MODULE_OWNER(netdev);
    SET_NETDEV_DEV(netdev, &pdev->dev);

    pci_set_drvdata(pdev, netdev);
    adapter = netdev_priv(netdev);
    adapter->netdev = netdev;
    adapter->pdev = pdev;
    adapter->hw.back = adapter;

    mmio_start = pci_resource_start(pdev, BAR_0);
    mmio_len = pci_resource_len(pdev, BAR_0);

    AT_DBG("base memory = %lx memory length = %x \n", 
        mmio_start, mmio_len);
    adapter->hw.mem_rang = (u32)mmio_len;
    adapter->hw.hw_addr = ioremap(mmio_start, mmio_len);
    if(!adapter->hw.hw_addr) {
        err = -EIO;
        goto err_ioremap;
    }
    
	at_setup_pcicmd(pdev);
	
    netdev->open = &at_open;
    netdev->stop = &at_close;
    netdev->hard_start_xmit = &at_xmit_frame;
    netdev->get_stats = &at_get_stats;
    netdev->set_multicast_list = &at_set_multi;
    netdev->set_mac_address = &at_set_mac;
    netdev->change_mtu = &at_change_mtu;
    netdev->do_ioctl = &at_ioctl;
    at_set_ethtool_ops(netdev);
    
#ifdef HAVE_TX_TIMEOUT
    netdev->tx_timeout = &at_tx_timeout;
    netdev->watchdog_timeo = 5 * HZ;
#endif
#ifdef NETIF_F_HW_VLAN_TX
    netdev->vlan_rx_register = at_vlan_rx_register;
    netdev->vlan_rx_add_vid = at_vlan_rx_add_vid;
    netdev->vlan_rx_kill_vid = at_vlan_rx_kill_vid;
#endif
	strncpy(netdev->name, pci_name(pdev), sizeof(netdev->name) - 1);
	
	
	
    netdev->mem_start = mmio_start;
    netdev->mem_end = mmio_start + mmio_len;
    //netdev->base_addr = adapter->io_base;
    adapter->bd_number = cards_found;
    adapter->pci_using_64 = pci_using_64;

    /* setup the private structure */

    if((err = at_sw_init(adapter)))
        goto err_sw_init;

    err = -EIO;
    
#ifdef NETIF_F_HW_VLAN_TX
    netdev->features |= 
               (NETIF_F_HW_VLAN_TX | 
                NETIF_F_HW_VLAN_RX );
#endif

    if(pci_using_64) {
        netdev->features |= NETIF_F_HIGHDMA;
        AT_DBG("pci using 64bit address\n");
    }
#ifdef NETIF_F_LLTX
    netdev->features |= NETIF_F_LLTX;
#endif


    /* reset the controller to 
     * put the device in a known good starting state */
    
    if (at_reset_hw(&adapter->hw)) {
        err = -EIO;
        goto err_reset;
    }

    /* copy the MAC address out of the EEPROM */

    at_read_mac_addr(&adapter->hw);
    memcpy(netdev->dev_addr, adapter->hw.mac_addr, netdev->addr_len);
#ifdef ETHTOOL_GPERMADDR
	memcpy(netdev->perm_addr, adapter->hw.mac_addr, netdev->addr_len);

	if (!is_valid_ether_addr(netdev->perm_addr)) {
#else
	if (!is_valid_ether_addr(netdev->dev_addr)) {
#endif
		AT_DBG("Invalid MAC Address\n");
		err = -EIO;
		goto err_eeprom;
	}
    AT_DBG("mac address : %02x-%02x-%02x-%02x-%02x-%02x\n",
        adapter->hw.mac_addr[0],
        adapter->hw.mac_addr[1],
        adapter->hw.mac_addr[2],
        adapter->hw.mac_addr[3],
        adapter->hw.mac_addr[4],
        adapter->hw.mac_addr[5] );

    at_check_options(adapter);
    
    /* pre-init the MAC, and setup link */

//    if ((err = at_init_hw(&adapter->hw))) {
//        err = -EIO;
//        goto err_init_hw;
//    }
    
    init_timer(&adapter->watchdog_timer);
    adapter->watchdog_timer.function = &at_watchdog;
    adapter->watchdog_timer.data = (unsigned long) adapter;
    
    init_timer(&adapter->phy_config_timer);
    adapter->phy_config_timer.function = &at_phy_config;
    adapter->phy_config_timer.data = (unsigned long) adapter;
    adapter->phy_timer_pending = FALSE;
    
    INIT_WORK(&adapter->reset_task, at_reset_task);
    INIT_WORK(&adapter->link_chg_task, at_link_chg_task);


	strcpy(netdev->name, "eth%d"); // ??
    if((err = register_netdev(netdev)))
        goto err_register;

    /* assume we have no link for now */
    netif_carrier_off(netdev);
    netif_stop_queue(netdev);
    
    
    cards_found++;

    return 0;

//err_init_hw:
err_reset:
err_register:	
err_sw_init:
err_eeprom:
	iounmap(adapter->hw.hw_addr);
err_ioremap:
	free_netdev(netdev);
err_alloc_etherdev:
	pci_release_regions(pdev);
err_pci_reg:
err_dma:
	pci_disable_device(pdev);
	return err;
}

/**
 * at_remove - Device Removal Routine
 * @pdev: PCI device information struct
 *
 * at_remove is called by the PCI subsystem to alert the driver
 * that it should release a PCI device.  The could be caused by a
 * Hot-Plug event, or because the driver is going to be removed from
 * memory.
 **/

static void __devexit
at_remove(struct pci_dev *pdev)
{
	struct net_device *netdev = pci_get_drvdata(pdev);
	struct at_adapter *adapter = netdev_priv(netdev);
	
	DEBUGFUNC("at_remove");


	/* flush_scheduled work may reschedule our watchdog task, so
	 * explicitly disable watchdog tasks from being rescheduled  */
	set_bit(__AT_DOWN, &adapter->flags);

	del_timer_sync(&adapter->watchdog_timer);
	del_timer_sync(&adapter->phy_config_timer);

	flush_scheduled_work();

	unregister_netdev(netdev);

	iounmap(adapter->hw.hw_addr);
	pci_release_regions(pdev);

	free_netdev(netdev);

	pci_disable_device(pdev);
}

#ifdef USE_REBOOT_NOTIFIER
/* only want to do this for 2.4 kernels? */
static int
at_notify_reboot(struct notifier_block *nb, unsigned long event, void *p)
{
	struct pci_dev *pdev = NULL;

	DEBUGFUNC("at_notify_reboot !");

    switch(event) {
    case SYS_DOWN:
    case SYS_HALT:
    case SYS_POWER_OFF:
        while((pdev = pci_find_device(PCI_ANY_ID, PCI_ANY_ID, pdev))) {
            if(pci_dev_driver(pdev) == &at_driver)
                at_suspend(pdev, PMSG_SUSPEND);
        }
    }
    return NOTIFY_DONE;
    
}
#endif

#ifndef USE_REBOOT_NOTIFIER
static void at_shutdown(struct pci_dev *pdev)
{
	at_suspend(pdev, PMSG_SUSPEND);
}
#endif


static int
at_suspend(struct pci_dev *pdev, pm_message_t state)
{
    struct net_device *netdev = pci_get_drvdata(pdev);
    struct at_adapter *adapter = netdev_priv(netdev);
    struct at_hw * hw = &adapter->hw;
    u16 speed, duplex;
    u32 ctrl = 0;
    u32 wufc = adapter->wol;

#ifdef CONFIG_PM
	int retval = 0;
#endif

    DEBUGFUNC("at_suspend !"); 


	netif_device_detach(netdev);

	if (netif_running(netdev)) {
		WARN_ON(test_bit(__AT_RESETTING, &adapter->flags));
		at_down(adapter);
	}

#ifdef CONFIG_PM
	retval = pci_save_state(pdev);
	if (retval)
		return retval;
#endif


    at_read_phy_reg(hw, MII_BMSR, (u16*)&ctrl);
    at_read_phy_reg(hw, MII_BMSR, (u16*)&ctrl);
    if(ctrl & BMSR_LSTATUS)
        wufc &= ~AT_WUFC_LNKC;

	if (0 != (ctrl&BMSR_LSTATUS) && 0 != wufc) {
            u32 ret_val;	
	/* get current link speed & duplex */
		ret_val = at_get_speed_and_duplex(hw, &speed, &duplex);
		if (ret_val) {
			printk(KERN_DEBUG "%s: get speed&duplex error while suspend\n", 
				at_driver_name);
			goto wol_dis;
		}
		
		/* if resume, let driver to re- setup link */
		hw->phy_configured = FALSE;
		
		ctrl = 0;
		
		/* turn on magic packet wol */
		if (wufc & AT_WUFC_MAG)
			ctrl |= (WOL_MAGIC_EN | WOL_MAGIC_PME_EN);

		/* ignore Link Chg event when Link is up */
		AT_WRITE_REG(hw, REG_WOL_CTRL, ctrl);
		
		AT_DBG("%s: suspend WOL=0x%x\n", at_driver_name, ctrl);
	
	
	
	    /* Config MAC CTRL Register */
	    ctrl = MAC_CTRL_RX_EN | MAC_CTRL_MACLP_CLK_PHY;
	    if (FULL_DUPLEX == adapter->link_duplex)    
	        ctrl |= MAC_CTRL_DUPLX;
	    ctrl |= (MAC_CTRL_ADD_CRC|MAC_CTRL_PAD);
	    ctrl |= (((u32)adapter->hw.preamble_len
	                  &MAC_CTRL_PRMLEN_MASK)<< MAC_CTRL_PRMLEN_SHIFT);
	    ctrl |= (((u32)(adapter->hw.retry_buf
                	&MAC_CTRL_HALF_LEFT_BUF_MASK)) 
                    << MAC_CTRL_HALF_LEFT_BUF_SHIFT);    
		if (wufc & AT_WUFC_MAG) {
			/* magic packet maybe Broadcast&multicast&Unicast frame */
			ctrl |= MAC_CTRL_BC_EN;
		}
		AT_DBG("%s: suspend MAC=0x%x\n", at_driver_name, ctrl);
		
    	AT_WRITE_REG(hw, REG_MAC_CTRL, ctrl);
			
		/* pcie patch */
		ctrl = AT_READ_REG(hw, REG_PCIE_PHYMISC);
		ctrl |= PCIE_PHYMISC_FORCE_RCV_DET;
		AT_WRITE_REG(hw, REG_PCIE_PHYMISC, ctrl);
	
		pci_enable_wake(pdev, pci_choose_state(pdev, state), 1);
		goto suspend_exit;
	}	
		
	if (0 == (ctrl&BMSR_LSTATUS) && 0 != (wufc&AT_WUFC_LNKC)) {
		/* link is down, so only LINK CHG WOL event enable */
		ctrl |= (WOL_LINK_CHG_EN | WOL_LINK_CHG_PME_EN);
		AT_WRITE_REG(hw, REG_WOL_CTRL, ctrl);
		AT_WRITE_REG(hw, REG_MAC_CTRL, 0);
		
		/* pcie patch */
		ctrl = AT_READ_REG(hw, REG_PCIE_PHYMISC);
		ctrl |= PCIE_PHYMISC_FORCE_RCV_DET;
		AT_WRITE_REG(hw, REG_PCIE_PHYMISC, ctrl);	
		
		pci_enable_wake(pdev, pci_choose_state(pdev, state), 1);
		
		goto suspend_exit;
	} 
	
wol_dis:
	
	/* WOL disabled */
	AT_WRITE_REG(hw, REG_WOL_CTRL, 0);
	
	/* pcie patch */
	ctrl = AT_READ_REG(hw, REG_PCIE_PHYMISC);
	ctrl |= PCIE_PHYMISC_FORCE_RCV_DET;
	AT_WRITE_REG(hw, REG_PCIE_PHYMISC, ctrl);				
	
	pci_enable_wake(pdev, pci_choose_state(pdev, state), 0);

suspend_exit:	

	if (netif_running(netdev))
		free_irq(adapter->pdev->irq, netdev);

	pci_disable_device(pdev);

	pci_set_power_state(pdev, pci_choose_state(pdev, state));

	return 0;
}


#ifdef CONFIG_PM
static int
at_resume(struct pci_dev *pdev)
{
    struct net_device *netdev = pci_get_drvdata(pdev);
    struct at_adapter *adapter = netdev_priv(netdev);
    u32 err;

    DEBUGFUNC("at_resume !");

    pci_set_power_state(pdev, PCI_D0);
    pci_restore_state(pdev);
    
    if ((err = pci_enable_device(pdev))) {
	printk(KERN_ERR "atl2: Cannot enable PCI device from suspend\n");
	return err;
    }
	
    pci_set_master(pdev);

    pci_enable_wake(pdev, PCI_D3hot, 0);
    pci_enable_wake(pdev, PCI_D3cold, 0);
	
    AT_WRITE_REG(&adapter->hw, REG_WOL_CTRL, 0);
	
    if (netif_running(netdev) && (err = at_request_irq(adapter)))
	return err;

    at_power_up_phy(adapter);
    at_reset_hw(&adapter->hw);

    if(netif_running(netdev))
        at_up(adapter);

    netif_device_attach(netdev);

    return 0;
}
#endif


#ifdef CONFIG_AT_PCI_ERS
/**
 * at_io_error_detected - called when PCI error is detected
 * @pdev: Pointer to PCI device
 * @state: The current pci connection state
 *
 * This function is called after a PCI bus error affecting
 * this device has been detected.
 */
static pci_ers_result_t at_io_error_detected(struct pci_dev *pdev, pci_channel_state_t state)
{
	struct net_device *netdev = pci_get_drvdata(pdev);
	struct at_adapter *adapter = netdev->priv;

	netif_device_detach(netdev);

	if (netif_running(netdev))
		at_down(adapter);
		
	pci_disable_device(pdev);

	/* Request a slot slot reset. */
	return PCI_ERS_RESULT_NEED_RESET;
}

/**
 * at_io_slot_reset - called after the pci bus has been reset.
 * @pdev: Pointer to PCI device
 *
 * Restart the card from scratch, as if from a cold-boot. Implementation
 * resembles the first-half of the e1000_resume routine.
 */
static pci_ers_result_t at_io_slot_reset(struct pci_dev *pdev)
{
	struct net_device *netdev = pci_get_drvdata(pdev);
	struct at_adapter *adapter = netdev->priv;

	if (pci_enable_device(pdev)) {
		printk(KERN_ERR "ATL2: Cannot re-enable PCI device after reset.\n");
		return PCI_ERS_RESULT_DISCONNECT;
	}
	pci_set_master(pdev);

	pci_enable_wake(pdev, PCI_D3hot, 0);
	pci_enable_wake(pdev, PCI_D3cold, 0);

	at_reset_hw(&adapter->hw);
	//AT_WRITE_REG(&adapter->hw, E1000_WUS, ~0);

	return PCI_ERS_RESULT_RECOVERED;
}

/**
 * at_io_resume - called when traffic can start flowing again.
 * @pdev: Pointer to PCI device
 *
 * This callback is called when the error recovery driver tells us that
 * its OK to resume normal operation. Implementation resembles the
 * second-half of the at_resume routine.
 */
static void at_io_resume(struct pci_dev *pdev)
{
	struct net_device *netdev = pci_get_drvdata(pdev);
	struct at_adapter *adapter = netdev->priv;

	if (netif_running(netdev)) {
		if (at_up(adapter)) {
			printk("ATL2: can't bring device back up after reset\n");
			return;
		}
	}

	netif_device_attach(netdev);
}
#endif /* CONFIG_AT_PCI_ERS */



/**
 * at_irq_enable - Enable default interrupt generation settings
 * @adapter: board private structure
 **/

inline void
at_irq_enable(struct at_adapter *adapter)
{
    if (likely(atomic_dec_and_test(&adapter->irq_sem))) {
		AT_WRITE_REG(&adapter->hw, REG_IMR, IMR_NORMAL_MASK);
		AT_WRITE_FLUSH(&adapter->hw);
	}
}

/**
 * at_irq_disable - Mask off interrupt generation on the NIC
 * @adapter: board private structure
 **/

inline void
at_irq_disable(struct at_adapter *adapter)
{
    atomic_inc(&adapter->irq_sem);
    AT_WRITE_REG(&adapter->hw, REG_IMR, 0);
    AT_WRITE_FLUSH(&adapter->hw);
    synchronize_irq(adapter->pdev->irq);
}

/**
 * at_sw_init - Initialize general software structures (struct at_adapter)
 * @adapter: board private structure to initialize
 *
 * at_sw_init initializes the Adapter private data structure.
 * Fields are initialized based on PCI device information and
 * OS network device settings (MTU size).
 **/

static int __devinit
at_sw_init(struct at_adapter *adapter)
{
    struct at_hw *hw = &adapter->hw;
    struct pci_dev *pdev = adapter->pdev;

    /* PCI config space info */

    hw->vendor_id = pdev->vendor;
    hw->device_id = pdev->device;
    hw->subsystem_vendor_id = pdev->subsystem_vendor;
    hw->subsystem_id = pdev->subsystem_device;

    pci_read_config_byte(pdev, PCI_REVISION_ID, &hw->revision_id);

    pci_read_config_word(pdev, PCI_COMMAND, &hw->pci_cmd_word);

    adapter->wol = 0;

    adapter->ict = 50000;  // 100ms
    
    adapter->link_speed = SPEED_0;   // hardware init
    adapter->link_duplex = FULL_DUPLEX; //

  
    hw->phy_configured = FALSE;
    hw->preamble_len = 7;
    hw->ipgt = 0x60;
    hw->min_ifg = 0x50;
    hw->ipgr1 = 0x40;
    hw->ipgr2 = 0x60;
    hw->retry_buf = 2;
    
    hw->max_retry = 0xf;
    hw->lcol = 0x37;
    hw->jam_ipg = 7;
    
    hw->fc_rxd_hi = 0;
    hw->fc_rxd_lo = 0; 
    
    hw->max_frame_size = adapter->netdev->mtu;
    
    atomic_set(&adapter->irq_sem, 1);
    spin_lock_init(&adapter->stats_lock);
    spin_lock_init(&adapter->tx_lock);
    
	set_bit(__AT_DOWN, &adapter->flags);
	
    return 0;
}

int
at_reset(struct at_adapter *adapter)
{
    int ret;
    
    if (AT_SUCCESS != (ret = at_reset_hw(&adapter->hw)))
        return ret;

    return at_init_hw(&adapter->hw);
}

/**
 * at_open - Called when a network interface is made active
 * @netdev: network interface device structure
 *
 * Returns 0 on success, negative value on failure
 *
 * The open entry point is called when a network interface is made
 * active by the system (IFF_UP).  At this point all resources needed
 * for transmit and receive operations are allocated, the interrupt
 * handler is registered with the OS, the watchdog timer is started,
 * and the stack is notified that the interface is ready.
 **/

static int
at_open(struct net_device *netdev)
{
    struct at_adapter *adapter = netdev_priv(netdev);
    int err;
    u32 val;

    DEBUGFUNC("at_open !");

    /* disallow open during test */
    if (test_bit(__AT_TESTING, &adapter->flags))
	return -EBUSY;
		
    /* allocate transmit descriptors */

    if((err = at_setup_ring_resources(adapter)))
        return err;

    at_power_up_phy(adapter);

    if((err = at_init_hw(&adapter->hw))) {
        err = -EIO;
        goto err_init_hw;
    }
	
    /* hardware has been reset, we need to reload some things */

    at_set_multi(netdev);
    init_ring_ptrs(adapter);

#ifdef NETIF_F_HW_VLAN_TX
    at_restore_vlan(adapter);
#endif

    if (at_configure(adapter)) {
        err = -EIO;
        goto err_config;
    }
   
    if ((err = at_request_irq(adapter)))
        goto err_req_irq;
        
   	clear_bit(__AT_DOWN, &adapter->flags);     
        
    mod_timer(&adapter->watchdog_timer, jiffies + 4*HZ); 
    
    val = AT_READ_REG(&adapter->hw, REG_MASTER_CTRL);
    AT_WRITE_REG(&adapter->hw, REG_MASTER_CTRL, val|MASTER_CTRL_MANUAL_INT);
    //at_check_link(adapter);    
    
    
    at_irq_enable(adapter);

    return 0;

err_init_hw:
err_req_irq:
err_config:
	at_power_down_phy(adapter);
	at_free_ring_resources(adapter);
	at_reset_hw(&adapter->hw);
	
	return err;
}

/**
 * at_close - Disables a network interface
 * @netdev: network interface device structure
 *
 * Returns 0, this is not allowed to fail
 *
 * The close entry point is called when an interface is de-activated
 * by the OS.  The hardware is still under the drivers control, but
 * needs to be disabled.  A global MAC reset is issued to stop the
 * hardware, and all transmit and receive resources are freed.
 **/

static int
at_close(struct net_device *netdev)
{
    struct at_adapter *adapter = netdev_priv(netdev);
    DEBUGFUNC("at_close!");

    WARN_ON(test_bit(__AT_RESETTING, &adapter->flags));
	
    at_down(adapter);
    at_power_down_phy(adapter);
    free_irq(adapter->pdev->irq, netdev);
    at_free_ring_resources(adapter);

    return 0;
}

/**
 * at_setup_mem_resources - allocate Tx / RX descriptor resources 
 * @adapter: board private structure
 *
 * Return 0 on success, negative on failure
 **/

int32_t
at_setup_ring_resources(struct at_adapter *adapter)
{
    struct pci_dev *pdev = adapter->pdev;
    int size;
    u8 offset = 0;

    DEBUGFUNC("at_setup_ring_resources");

    /* real ring DMA buffer */
    adapter->ring_size = size =   
	      adapter->txd_ring_size * 1   + 7         // dword align
	    + adapter->txs_ring_size * 4   + 7         // dword align
            + adapter->rxd_ring_size * 1536+ 127;    // 128bytes align
    
    adapter->ring_vir_addr = 
                    pci_alloc_consistent(pdev, size, &adapter->ring_dma);
    if (!adapter->ring_vir_addr) {
        DEBUGOUT1("pci_alloc_consistent failed, size = D%d", size);
        return -ENOMEM;
    }
 
    if (adapter->pci_using_64) { 
        // test whether HIDWORD dma buffer is not cross boundary
        if (    ((adapter->ring_dma       &0xffffffff00000000ULL)>>32)
             != (((adapter->ring_dma+size)&0xffffffff00000000ULL)>>32) ) {
            pci_free_consistent(
                     pdev, 
                     adapter->ring_size, 
                     adapter->ring_vir_addr, 
                     adapter->ring_dma);
            DEBUGOUT("memory allocated cross 32bit boundary !");
            return -ENOMEM;
        }
    }

//    DEBUGOUT("memory allocated successfully !");    
    
    memset(adapter->ring_vir_addr, 0, adapter->ring_size);

      // Init TXD Ring
      
      adapter->txd_dma = adapter->ring_dma ;
      offset = (adapter->txd_dma & 0x7) ? (8 - (adapter->txd_dma & 0x7)) : 0;
      adapter->txd_dma += offset;
      adapter->txd_ring = (tx_pkt_header_t*) (adapter->ring_vir_addr + offset);
      
      // Init TXS Ring
      
      adapter->txs_dma = adapter->txd_dma + adapter->txd_ring_size;
      offset = (adapter->txs_dma & 0x7) ? (8- (adapter->txs_dma & 0x7)) : 0;
      adapter->txs_dma += offset;
      adapter->txs_ring = (tx_pkt_status_t*) 
                (((u8*)adapter->txd_ring) + (adapter->txd_ring_size+offset));
                
      // Init RXD Ring
      adapter->rxd_dma = adapter->txs_dma + adapter->txs_ring_size*4;
      offset = (adapter->rxd_dma & 127) ? (128 - (adapter->rxd_dma & 127)) : 0;
      if (offset > 7) {
	  offset -= 8;
      } else {
	  offset += (128 - 8);
      }
      adapter->rxd_dma += offset;
      adapter->rxd_ring = (rx_desc_t*)
                (((u8*)adapter->txs_ring) + 
		    (adapter->txs_ring_size*4 + offset));


      // Read / Write Ptr Initialize:
  //      init_ring_ptrs(adapter);

    return AT_SUCCESS;
}


void
init_ring_ptrs(struct at_adapter *adapter)
{
    // Read / Write Ptr Initialize:
    adapter->txd_write_ptr = 0;
    atomic_set(&adapter->txd_read_ptr, 0);

    adapter->rxd_read_ptr = 0;
    adapter->rxd_write_ptr = 0;
    
    atomic_set(&adapter->txs_write_ptr, 0);
    adapter->txs_next_clear = 0;
}

/**
 * at_free_ring_resources - Free Tx / RX descriptor Resources
 * @adapter: board private structure
 *
 * Free all transmit software resources
 **/

void
at_free_ring_resources(struct at_adapter *adapter)
{
    struct pci_dev *pdev = adapter->pdev;
    
    DEBUGFUNC("at_free_ring_resources");

    pci_free_consistent(
         pdev, 
         adapter->ring_size,
         adapter->ring_vir_addr,
         adapter->ring_dma);
         
}


int
at_up(struct at_adapter *adapter)
{
    struct net_device *netdev = adapter->netdev;
    int err = 0;
    u32 val;

//    DEBUGFUNC("at_up !"); 

    /* hardware has been reset, we need to reload some things */

    err = at_init_hw(&adapter->hw);
    if (err) {
        err = -EIO;
        return err;
    }

    at_set_multi(netdev);
    init_ring_ptrs(adapter);

#ifdef NETIF_F_HW_VLAN_TX
    at_restore_vlan(adapter);
#endif

    if (at_configure(adapter)) {
        err = -EIO;
        goto err_up;
    }    

    clear_bit(__AT_DOWN, &adapter->flags);
	
    val = AT_READ_REG(&adapter->hw, REG_MASTER_CTRL);
    AT_WRITE_REG(&adapter->hw, REG_MASTER_CTRL, val|MASTER_CTRL_MANUAL_INT);
    //at_check_link(adapter);  
    at_irq_enable(adapter);
err_up:
    return err;
}

inline void
at_setup_mac_ctrl(struct at_adapter* adapter)
{
    u32 value;
    struct at_hw* hw = &adapter->hw;
    struct net_device* netdev = adapter->netdev;
    
    /* Config MAC CTRL Register */
    value = MAC_CTRL_TX_EN | 
	    	MAC_CTRL_RX_EN |
            MAC_CTRL_MACLP_CLK_PHY;
    // duplex
    if (FULL_DUPLEX == adapter->link_duplex)    
        value |= MAC_CTRL_DUPLX;
    // flow control
    value |= (MAC_CTRL_TX_FLOW|MAC_CTRL_RX_FLOW);

    // PAD & CRC
    value |= (MAC_CTRL_ADD_CRC|MAC_CTRL_PAD);
    // preamble length
    value |= (((u32)adapter->hw.preamble_len
                  &MAC_CTRL_PRMLEN_MASK)<< MAC_CTRL_PRMLEN_SHIFT);
    // vlan 
    if (adapter->vlgrp)     
        value |= MAC_CTRL_RMV_VLAN;
        
    // filter mode
    value |= MAC_CTRL_BC_EN;
    if (netdev->flags & IFF_PROMISC) 
        value |= MAC_CTRL_PROMIS_EN;
    else if (netdev->flags & IFF_ALLMULTI)
        value |= MAC_CTRL_MC_ALL_EN;

    // half retry buffer
    value |= (((u32)(adapter->hw.retry_buf
                &MAC_CTRL_HALF_LEFT_BUF_MASK)) 
                    << MAC_CTRL_HALF_LEFT_BUF_SHIFT);

    AT_WRITE_REG(hw, REG_MAC_CTRL, value);
}


static int
at_check_link(struct at_adapter* adapter)
{
    struct at_hw *hw = &adapter->hw;
    struct net_device * netdev = adapter->netdev;
    int ret_val;
    u16 speed, duplex, phy_data;
    int reconfig = 0;

//    DEBUGFUNC("at_check_link !");
	// MII_BMSR must read twise
    at_read_phy_reg(hw, MII_BMSR, &phy_data);
    at_read_phy_reg(hw, MII_BMSR, &phy_data);
    if (!(phy_data&BMSR_LSTATUS)) { // link down
		if (netif_carrier_ok(netdev)) { // old link state: Up
			DEBUGOUT("NIC Link is Down");
            adapter->link_speed = SPEED_0;
            netif_carrier_off(netdev);
            netif_stop_queue(netdev);
        }
        return AT_SUCCESS;  
    }
    
    // Link Up
	ret_val = at_get_speed_and_duplex(hw, &speed, &duplex);
	if (ret_val)  return ret_val;
	switch( hw->MediaType )
	{
	case MEDIA_TYPE_100M_FULL:
		if (speed  != SPEED_100 || duplex != FULL_DUPLEX)
			reconfig = 1;
		break;
	case MEDIA_TYPE_100M_HALF:
		if (speed  != SPEED_100 || duplex != HALF_DUPLEX)
			reconfig = 1;
		break;
	case MEDIA_TYPE_10M_FULL:
		if (speed != SPEED_10 || duplex != FULL_DUPLEX)
			reconfig = 1;
	        break;	
	case MEDIA_TYPE_10M_HALF:
		if (speed  != SPEED_10 || duplex != HALF_DUPLEX)
			reconfig = 1;
		break;
	}
	// link result is our setting
	if (0 == reconfig)
	{
		if (adapter->link_speed != speed ||
            adapter->link_duplex != duplex ) {
			adapter->link_speed = speed;
			adapter->link_duplex = duplex;
			at_setup_mac_ctrl(adapter); 
			printk(KERN_INFO
                   "%s: %s NIC Link is Up<%d Mbps %s>\n",
		   			at_driver_name,
                    netdev->name, adapter->link_speed,
                    adapter->link_duplex == FULL_DUPLEX ?
 					"Full Duplex" : "Half Duplex"); 
		}
		
		if (!netif_carrier_ok(netdev)) { // Link down -> Up
			netif_carrier_on(netdev);
			netif_wake_queue(netdev);
		}
		return AT_SUCCESS;
	}
	
	// change orignal link status
	if (netif_carrier_ok(netdev)) { 
		adapter->link_speed = SPEED_0;
    	netif_carrier_off(netdev);
    	netif_stop_queue(netdev);
    }
    
    if (hw->MediaType != MEDIA_TYPE_AUTO_SENSOR) {
    	switch (hw->MediaType)
    	{
    	case MEDIA_TYPE_100M_FULL:
    		phy_data = MII_CR_FULL_DUPLEX|MII_CR_SPEED_100|MII_CR_RESET;
    		break;
    	case MEDIA_TYPE_100M_HALF:
    		phy_data = MII_CR_SPEED_100|MII_CR_RESET;
    		break;
    	case MEDIA_TYPE_10M_FULL:
    		phy_data = MII_CR_FULL_DUPLEX|MII_CR_SPEED_10|MII_CR_RESET;
    		break;
    	default: // MEDIA_TYPE_10M_HALF:
    		phy_data = MII_CR_SPEED_10|MII_CR_RESET;
    		break;
    	}
    	at_write_phy_reg(hw, MII_BMCR, phy_data);
    	return AT_SUCCESS;
    }

	// auto-neg, insert timer to re-config phy
    if (!adapter->phy_timer_pending) {
		adapter->phy_timer_pending = TRUE;
		mod_timer(&adapter->phy_config_timer, jiffies + 3 * HZ);
	}

    return AT_SUCCESS;
}


/**
 * at_power_up_phy - restore link in case the phy was powered down
 * @adapter: address of board private structure
 *
 * The phy may be powered down to save power and turn off link when the
 * driver is unloaded and wake on lan is not enabled (among others)
 * *** this routine MUST be followed by a call to at_reset ***
 *
 **/

void at_power_up_phy(struct at_adapter *adapter)
{
    u16 mii_reg = 0;

    DEBUGFUNC("at_power_up_phy");

	/* Just clear the power down bit to wake the phy back up */
	/* according to the manual, the phy will retain its
	 * settings across a power-down/up cycle */
	at_read_phy_reg(&adapter->hw, MII_BMCR, &mii_reg);
	mii_reg &= ~MII_CR_POWER_DOWN;
	mii_reg |= MII_CR_RESET;
	at_write_phy_reg(&adapter->hw, MII_BMCR, mii_reg);
}


static void at_power_down_phy(struct at_adapter *adapter)
{
	/* Power down the PHY so no link is implied when interface is down *
	 * The PHY cannot be powered down if any of the following is TRUE *
	 * (a) WoL is enabled
	 */
    DEBUGFUNC("at_power_down_phy");

	if (!adapter->wol ) {
		u16 mii_reg = 0;
		at_read_phy_reg(&adapter->hw, MII_BMCR, &mii_reg);
		mii_reg |= (MII_CR_POWER_DOWN|MII_CR_RESET);
		at_write_phy_reg(&adapter->hw, MII_BMCR, mii_reg);
		mdelay(1);
	}

	return;
}



void
at_down(struct at_adapter *adapter)
{
    struct net_device *netdev = adapter->netdev;
    
//    DEBUGFUNC("at_down !");

     /* signal that we're down so the interrupt handler does not
      * reschedule our watchdog timer */
    set_bit(__AT_DOWN, &adapter->flags);

#ifdef NETIF_F_LLTX
    netif_stop_queue(netdev);
#else
    netif_tx_disable(netdev);
#endif

    /* reset MAC to disable all RX/TX */
    at_reset_hw(&adapter->hw);
    msleep(1);
	
    at_irq_disable(adapter);
	
    del_timer_sync(&adapter->watchdog_timer);
    del_timer_sync(&adapter->phy_config_timer);
    adapter->phy_timer_pending = FALSE;
	
    netif_carrier_off(netdev);
    adapter->link_speed = SPEED_0;
    adapter->link_duplex = -1;
   
//    at_reset(adapter);
}



/**
 * at_set_multi - Multicast and Promiscuous mode set
 * @netdev: network interface device structure
 *
 * The set_multi entry point is called whenever the multicast address
 * list or the network interface flags are updated.  This routine is
 * responsible for configuring the hardware for proper multicast,
 * promiscuous mode, and all-multi behavior.
 **/

static void
at_set_multi(struct net_device *netdev)
{
    struct at_adapter *adapter = netdev_priv(netdev);
    struct at_hw *hw = &adapter->hw;
    struct dev_mc_list *mc_ptr;
    u32 rctl;
    u32 hash_value;

//    DEBUGFUNC("at_set_multi !");

    /* Check for Promiscuous and All Multicast modes */

    rctl = AT_READ_REG(hw, REG_MAC_CTRL);

    if(netdev->flags & IFF_PROMISC) {
        rctl |= MAC_CTRL_PROMIS_EN;
    } else if(netdev->flags & IFF_ALLMULTI) {
        rctl |= MAC_CTRL_MC_ALL_EN;
        rctl &= ~MAC_CTRL_PROMIS_EN;
    } else {
        rctl &= ~(MAC_CTRL_PROMIS_EN | MAC_CTRL_MC_ALL_EN);
    }

    AT_WRITE_REG(hw, REG_MAC_CTRL, rctl);

    /* clear the old settings from the multicast hash table */
    AT_WRITE_REG(hw, REG_RX_HASH_TABLE, 0);
    AT_WRITE_REG_ARRAY(hw, REG_RX_HASH_TABLE, 1, 0);

    /* comoute mc addresses' hash value ,and put it into hash table */

    for(mc_ptr = netdev->mc_list; mc_ptr; mc_ptr = mc_ptr->next) {
        hash_value = at_hash_mc_addr(hw, mc_ptr->dmi_addr);
        at_hash_set(hw, hash_value);
    }
}

#ifdef NETIF_F_HW_VLAN_TX
static void
at_vlan_rx_register(struct net_device *netdev, struct vlan_group *grp)
{
    struct at_adapter *adapter = netdev_priv(netdev);
    u32 ctrl;

 //   DEBUGFUNC("at_vlan_rx_register !");    

    at_irq_disable(adapter);
    adapter->vlgrp = grp;

    if(grp) {
        /* enable VLAN tag insert/strip */

        ctrl = AT_READ_REG(&adapter->hw, REG_MAC_CTRL);
        ctrl |= MAC_CTRL_RMV_VLAN; 
        AT_WRITE_REG(&adapter->hw, REG_MAC_CTRL, ctrl);
    } else {
        /* disable VLAN tag insert/strip */

        ctrl = AT_READ_REG(&adapter->hw, REG_MAC_CTRL);
        ctrl &= ~MAC_CTRL_RMV_VLAN;
        AT_WRITE_REG(&adapter->hw, REG_MAC_CTRL, ctrl);
    }

    at_irq_enable(adapter);
}

static void
at_vlan_rx_add_vid(struct net_device *netdev, u16 vid)
{
    /* We don't do Vlan filtering */
//    DEBUGFUNC("at_vlan_rx_add_vid !");
    return ;
}

static void
at_vlan_rx_kill_vid(struct net_device *netdev, u16 vid)
{
    struct at_adapter *adapter = netdev_priv(netdev);

//    DEBUGFUNC("at_vlan_rx_kill_vid !");
    at_irq_disable(adapter);

    if (adapter->vlgrp) {
        adapter->vlgrp->vlan_devices_arrays[vid] = NULL;
    }

    at_irq_enable(adapter);

    /* We don't do Vlan filtering */

    return;
}

static void
at_restore_vlan(struct at_adapter *adapter)
{
//    DEBUGFUNC("at_restore_vlan !");
    at_vlan_rx_register(adapter->netdev, adapter->vlgrp);

    if(adapter->vlgrp) {
        u16 vid;
        for(vid = 0; vid < VLAN_GROUP_ARRAY_LEN; vid++) {
            if(!adapter->vlgrp->vlan_devices_arrays[vid])
                continue;
            at_vlan_rx_add_vid(adapter->netdev, vid);
        }
    }
}
#endif

/**
 * at_configure - Configure Transmit&Receive Unit after Reset
 * @adapter: board private structure
 *
 * Configure the Tx /Rx unit of the MAC after a reset.
 **/

static int
at_configure(struct at_adapter *adapter)
{
    struct at_hw * hw = &adapter->hw;
    u32 value;
    
//    DEBUGFUNC("at_configure !");

    // clear interrupt status
    AT_WRITE_REG(&adapter->hw, REG_ISR, 0xffffffff);

    // set MAC Address
    value = (((u32)hw->mac_addr[2]) << 24) |
            (((u32)hw->mac_addr[3]) << 16) |
            (((u32)hw->mac_addr[4]) << 8 ) |
            (((u32)hw->mac_addr[5])      ) ;
    AT_WRITE_REG(hw, REG_MAC_STA_ADDR, value);
    value = (((u32)hw->mac_addr[0]) << 8 ) |
            (((u32)hw->mac_addr[1])      ) ;
    AT_WRITE_REG(hw, (REG_MAC_STA_ADDR+4), value);

    // tx / rx ring :
    
    // HI base address
    AT_WRITE_REG(
          hw, 
          REG_DESC_BASE_ADDR_HI, 
          (u32)((adapter->ring_dma&0xffffffff00000000ULL) >>32));
    // LO base address
    AT_WRITE_REG(
          hw, 
          REG_TXD_BASE_ADDR_LO, 
          (u32)(adapter->txd_dma&0x00000000ffffffffULL));
    AT_WRITE_REG(
          hw, 
          REG_TXS_BASE_ADDR_LO, 
          (u32)(adapter->txs_dma& 0x00000000ffffffffULL));
    AT_WRITE_REG(hw, 
                 REG_RXD_BASE_ADDR_LO, 
                 (u32)(adapter->rxd_dma& 0x00000000ffffffffULL));
  
    // element count
    AT_WRITE_REGW(hw, REG_TXD_MEM_SIZE, (u16)(adapter->txd_ring_size/4));
    AT_WRITE_REGW(hw, REG_TXS_MEM_SIZE, (u16)adapter->txs_ring_size);
    AT_WRITE_REGW(hw, REG_RXD_BUF_NUM,  (u16)adapter->rxd_ring_size);
    DEBUGOUT1("txd ring size:%d, txs ring size:%d, rxd ring size:%d",
		    adapter->txd_ring_size/4,
		    adapter->txs_ring_size,
		    adapter->rxd_ring_size);
    
    /* config Internal SRAM */
/*
    AT_WRITE_REGW(hw, REG_SRAM_TXRAM_END, sram_tx_end);
    AT_WRITE_REGW(hw, REG_SRAM_TXRAM_END, sram_rx_end);    
*/
   
   
    /* config IPG/IFG */
    value = 
        (((u32)hw->ipgt&MAC_IPG_IFG_IPGT_MASK) 
              <<MAC_IPG_IFG_IPGT_SHIFT) |
        (((u32)hw->min_ifg &MAC_IPG_IFG_MIFG_MASK) 
              <<MAC_IPG_IFG_MIFG_SHIFT) |
        (((u32)hw->ipgr1&MAC_IPG_IFG_IPGR1_MASK)
              <<MAC_IPG_IFG_IPGR1_SHIFT)|
        (((u32)hw->ipgr2&MAC_IPG_IFG_IPGR2_MASK)
              <<MAC_IPG_IFG_IPGR2_SHIFT);
    AT_WRITE_REG(hw, REG_MAC_IPG_IFG, value);
//    DEBUGOUT1("init ipg/ifg with 0x%x", value);
    
    /* config  Half-Duplex Control */
    value = 
      ((u32)hw->lcol&MAC_HALF_DUPLX_CTRL_LCOL_MASK) |
      (((u32)hw->max_retry&MAC_HALF_DUPLX_CTRL_RETRY_MASK)
          <<MAC_HALF_DUPLX_CTRL_RETRY_SHIFT) |
      MAC_HALF_DUPLX_CTRL_EXC_DEF_EN   |
      (0xa<<MAC_HALF_DUPLX_CTRL_ABEBT_SHIFT) |
      (((u32)hw->jam_ipg&MAC_HALF_DUPLX_CTRL_JAMIPG_MASK)
          <<MAC_HALF_DUPLX_CTRL_JAMIPG_SHIFT);
    AT_WRITE_REG(hw, REG_MAC_HALF_DUPLX_CTRL, value);
//    DEBUGOUT1("init Half Duplex with 0x%x", value);
    
    
    /* set Interrupt Moderator Timer */
    AT_WRITE_REGW(hw, REG_IRQ_MODU_TIMER_INIT, adapter->imt);
    AT_WRITE_REG(hw, REG_MASTER_CTRL, MASTER_CTRL_ITIMER_EN);
//    DEBUGOUT1("init Irq Modurator Timer with 0x%x", adapter->imt);
    
    /* set Interrupt Clear Timer */
    AT_WRITE_REGW(hw, REG_CMBDISDMA_TIMER, adapter->ict);
//    DEBUGOUT1("init Irq Clear Timer with 0x%x", adapter->ict);
    
    /* set MTU */
    AT_WRITE_REG(hw, REG_MTU, 
		    adapter->netdev->mtu +
		    ENET_HEADER_SIZE + 
		    VLAN_SIZE +
		    ETHERNET_FCS_SIZE);
//    DEBUGOUT1("init MTU with 0x%x", hw->max_frame_size); 
   
    /* 1590 */
    AT_WRITE_REG(hw, 
		 REG_TX_CUT_THRESH,
		 0x177);
    
     /* flow control */
    AT_WRITE_REGW(hw, REG_PAUSE_ON_TH, hw->fc_rxd_hi);
    AT_WRITE_REGW(hw, REG_PAUSE_OFF_TH, hw->fc_rxd_lo);
    
    /* Init mailbox */
    AT_WRITE_REGW(hw, REG_MB_TXD_WR_IDX, (u16)adapter->txd_write_ptr);
    AT_WRITE_REGW(hw, REG_MB_RXD_RD_IDX, (u16)adapter->rxd_read_ptr);
    
    /* enable DMA read/write */
    AT_WRITE_REGB(hw, REG_DMAR, DMAR_EN);
    AT_WRITE_REGB(hw, REG_DMAW, DMAW_EN);
    
    
    value = AT_READ_REG(&adapter->hw, REG_ISR);
    if ((value&ISR_PHY_LINKDOWN) != 0) {
        value = 1; // config failed 
    } else {
        value = 0;
    }

    // clear all interrupt status
    AT_WRITE_REG(&adapter->hw, REG_ISR, 0x3fffffff);
    AT_WRITE_REG(&adapter->hw, REG_ISR, 0);
    return value;
}

/**
 * at_set_mac - Change the Ethernet Address of the NIC
 * @netdev: network interface device structure
 * @p: pointer to an address structure
 *
 * Returns 0 on success, negative on failure
 **/

static int
at_set_mac(struct net_device *netdev, void *p)
{
    struct at_adapter *adapter = netdev_priv(netdev);
    struct sockaddr *addr = p;

    DEBUGFUNC("at_set_mac !");
    
    if (!is_valid_ether_addr(addr->sa_data))
		return -EADDRNOTAVAIL;
		
    if (netif_running(netdev))
        return -EBUSY; 

    if(!is_valid_ether_addr(addr->sa_data))
        return -EADDRNOTAVAIL;

    memcpy(netdev->dev_addr, addr->sa_data, netdev->addr_len);
    memcpy(adapter->hw.mac_addr, addr->sa_data, netdev->addr_len);

    
    set_mac_addr(&adapter->hw);

    return 0;
}



/**
 * at_change_mtu - Change the Maximum Transfer Unit
 * @netdev: network interface device structure
 * @new_mtu: new value for maximum frame size
 *
 * Returns 0 on success, negative on failure
 **/

static int
at_change_mtu(struct net_device *netdev, int new_mtu)
{
	struct at_adapter *adapter = netdev_priv(netdev);
	struct at_hw* hw = &adapter->hw;
	
    DEBUGFUNC("at_change_mtu !");
    
    if ((new_mtu < 40) || (new_mtu > (ETH_DATA_LEN + VLAN_SIZE)))
        return -EINVAL;

	/* set MTU */
	if (hw->max_frame_size != new_mtu) {
		
//		while (test_and_set_bit(__AT_RESETTING, &adapter->flags))
//			msleep(1);
		
		netdev->mtu = new_mtu;		      
		
		AT_WRITE_REG(hw, REG_MTU, 
		    			new_mtu +
		    			ENET_HEADER_SIZE + 
		    			VLAN_SIZE +
		    			ETHERNET_FCS_SIZE);
		    			
//		clear_bit(__AT_RESETTING, &adapter->flags);		    			
	}

	return 0;
}



void
at_read_pci_cfg(struct at_hw *hw, u32 reg, u16 *value)
{
    struct at_adapter *adapter = hw->back;

    pci_read_config_word(adapter->pdev, reg, value);
}

void
at_write_pci_cfg(struct at_hw *hw, u32 reg, u16 *value)
{
    struct at_adapter *adapter = hw->back;

    pci_write_config_word(adapter->pdev, reg, *value);
}


/**
 * at_get_stats - Get System Network Statistics
 * @netdev: network interface device structure
 *
 * Returns the address of the device statistics structure.
 * The statistics are actually updated from the timer callback.
 **/

static struct net_device_stats *
at_get_stats(struct net_device *netdev)
{
    struct at_adapter *adapter = netdev_priv(netdev);
    u32 drop_rxd, drop_rxs;
    unsigned long flags;
    
    spin_lock_irqsave(&adapter->stats_lock, flags);
    drop_rxd = AT_READ_REG(&adapter->hw, REG_STS_RXD_OV);
    drop_rxs = AT_READ_REG(&adapter->hw, REG_STS_RXS_OV);
    adapter->net_stats.rx_over_errors += (drop_rxd+drop_rxs);

    spin_unlock_irqrestore(&adapter->stats_lock, flags);
        
    return &adapter->net_stats;
}       

/**
 * at_ioctl -
 * @netdev:
 * @ifreq:
 * @cmd:
 **/

static int
at_ioctl(struct net_device *netdev, struct ifreq *ifr, int cmd)
{
//    DEBUGFUNC("at_ioctl !");
    switch (cmd) {
#ifdef SIOCGMIIPHY
    case SIOCGMIIPHY:
    case SIOCGMIIREG:
    case SIOCSMIIREG:
        return at_mii_ioctl(netdev, ifr, cmd);
#endif

#ifdef ETHTOOL_OPS_COMPAT
	case SIOCETHTOOL:
		return at_ethtool_ioctl(ifr);
#endif

    default:
        return -EOPNOTSUPP;
    }
}


#ifdef SIOCGMIIPHY
/**
 * at_mii_ioctl -
 * @netdev:
 * @ifreq:
 * @cmd:
 **/

static int
at_mii_ioctl(struct net_device *netdev, struct ifreq *ifr, int cmd)
{
    struct at_adapter *adapter = netdev_priv(netdev);
    struct mii_ioctl_data *data = if_mii(ifr);
    unsigned long flags;
	
//    DEBUGFUNC("at_mii_ioctl !");

    switch (cmd) {
    case SIOCGMIIPHY:
        data->phy_id = 0;
        break;
    case SIOCGMIIREG:
        if (!capable(CAP_NET_ADMIN))
            return -EPERM;
        spin_lock_irqsave(&adapter->stats_lock, flags);
        if (at_read_phy_reg(&adapter->hw, data->reg_num & 0x1F, &data->val_out)) {
            spin_unlock_irqrestore(&adapter->stats_lock, flags);
            return -EIO;
        }
        spin_unlock_irqrestore(&adapter->stats_lock, flags);
        break;
    case SIOCSMIIREG:
        if (!capable(CAP_NET_ADMIN))
            return -EPERM;
        if (data->reg_num & ~(0x1F))
            return -EFAULT;
         
        spin_lock_irqsave(&adapter->stats_lock, flags);
    	DEBUGOUT1("<at_mii_ioctl> write %x %x", 
            data->reg_num, 
            data->val_in);
        if (at_write_phy_reg(&adapter->hw, data->reg_num, data->val_in)) {
            spin_unlock_irqrestore(&adapter->stats_lock, flags);
            return -EIO;
        }
        // ......
        spin_unlock_irqrestore(&adapter->stats_lock, flags);
        break;
        
    default:
        return -EOPNOTSUPP;
    }
    return AT_SUCCESS;
}

#endif

/**
 * at_tx_timeout - Respond to a Tx Hang
 * @netdev: network interface device structure
 **/

static void
at_tx_timeout(struct net_device *netdev)
{
    struct at_adapter *adapter = netdev_priv(netdev);

    DEBUGFUNC("at_tx_timeout !");

    /* Do the reset outside of interrupt context */
    schedule_work(&adapter->reset_task);
}


static void
at_reset_task(struct work_struct *work)
{
	struct at_adapter *adapter;
	adapter = container_of(work, struct at_adapter, reset_task);

	at_reinit_locked(adapter);
}


void
at_reinit_locked(struct at_adapter *adapter)
{
 
    DEBUGFUNC("at_reinit_locked !");

    WARN_ON(in_interrupt());
	while (test_and_set_bit(__AT_RESETTING, &adapter->flags))
		msleep(1);
	at_down(adapter);
	at_up(adapter);
	clear_bit(__AT_RESETTING, &adapter->flags);
}

/**
 * at_link_chg_task - deal with link change event Out of interrupt context
 * @netdev: network interface device structure
 **/
static void
at_link_chg_task(struct work_struct *work)
{
    struct at_adapter *adapter;
    unsigned long flags;

    DEBUGFUNC("at_link_chg_task !");

    adapter = container_of(work, struct at_adapter, link_chg_task);


    spin_lock_irqsave(&adapter->stats_lock, flags);
    at_check_link(adapter);
    spin_unlock_irqrestore(&adapter->stats_lock, flags);
}

static void
at_check_for_link(struct at_adapter* adapter)
{
    struct net_device *netdev = adapter->netdev;
    u16 phy_data = 0;

    DEBUGFUNC("at_check_for_link!");
    
    spin_lock(&adapter->stats_lock);
    adapter->phy_timer_pending = FALSE;
    at_read_phy_reg(&adapter->hw, MII_BMSR, &phy_data);
    at_read_phy_reg(&adapter->hw, MII_BMSR, &phy_data);
    spin_unlock(&adapter->stats_lock);
    
    DEBUGOUT1("MII_BMSR=%x <at_check_for_link>", phy_data);
    
    // notify upper layer link down ASAP
    if (!(phy_data&BMSR_LSTATUS)) { // Link Down
        if (netif_carrier_ok(netdev)) { // old link state: Up
            printk(KERN_INFO
                   "%s: %s NIC Link is Down\n",
		   			at_driver_name,
                    netdev->name );
            adapter->link_speed = SPEED_0;
            netif_carrier_off(netdev);
            netif_stop_queue(netdev);
        }
    }
    schedule_work(&adapter->link_chg_task);
}

static inline void
at_clear_phy_int(struct at_adapter* adapter)
{
    u16 phy_data;
    
    spin_lock(&adapter->stats_lock);
    at_read_phy_reg(&adapter->hw, 19, &phy_data);
	spin_unlock(&adapter->stats_lock);
}


/**
 * at_intr - Interrupt Handler
 * @irq: interrupt number
 * @data: pointer to a network interface device structure
 * @pt_regs: CPU registers structure
 **/

static irqreturn_t
at_intr(int irq, void *data)
{
    struct at_adapter *adapter = netdev_priv(data);
    struct at_hw *hw = &adapter->hw;
    u32 status;
	
    
    status = AT_READ_REG(hw, REG_ISR);
    if (0 == status)    
        return IRQ_NONE;
       
    // link event
    if (status&ISR_PHY) {
        at_clear_phy_int(adapter);
    }
    
    // clear ISR status, and Enable CMB DMA/Disable Interrupt
    AT_WRITE_REG(hw, REG_ISR, status|ISR_DIS_INT);

    
    // check if PCIE PHY Link down
   if (status&ISR_PHY_LINKDOWN) {
        DEBUGOUT1("pcie phy linkdown %x", status);
        if(netif_running(adapter->netdev)) { // reset MAC
            AT_WRITE_REG(hw, REG_ISR, 0);
            AT_WRITE_REG(hw, REG_IMR, 0);
            AT_WRITE_FLUSH(hw);
            schedule_work(&adapter->reset_task);
            return IRQ_HANDLED; 
        }
    }

    // check if DMA read/write error ?
    if (status&(ISR_DMAR_TO_RST|ISR_DMAW_TO_RST)) 
    {
        DEBUGOUT1("PCIE DMA RW error (status = 0x%x) !", status);
        //AT_WRITE_REG(&adapter->hw, REG_MASTER_CTRL, MASTER_CTRL_SOFT_RST);
        AT_WRITE_REG(hw, REG_ISR, 0);
        AT_WRITE_REG(hw, REG_IMR, 0);
        AT_WRITE_FLUSH(hw);
        schedule_work(&adapter->reset_task);
        return IRQ_HANDLED; 
    }
    
    // link event
    if (status&(ISR_PHY|ISR_MANUAL))
    {
        adapter->net_stats.tx_carrier_errors++;
        at_check_for_link(adapter);
    }
    
    
    // transmit event
    if ( status&ISR_TX_EVENT ) {
        at_intr_tx(adapter);
    }
    
    // rx exception
    if ( status& ISR_RX_EVENT ) {
        at_intr_rx(adapter);
    }
    
    // re-enable Interrupt
    AT_WRITE_REG(&adapter->hw, REG_ISR, 0);
    return IRQ_HANDLED;
}

static void
at_intr_tx(struct at_adapter* adapter)
{
    u32 txd_read_ptr;
    u32 txs_write_ptr;
    tx_pkt_status_t* txs;
    tx_pkt_header_t* txph;
    int free_hole = 0;  
    
//    DEBUGFUNC("at_intr_tx"); 

    do {
        txs_write_ptr = (u32) atomic_read(&adapter->txs_write_ptr);
        txs = adapter->txs_ring + txs_write_ptr;
        if (!txs->update) 
            break; // tx stop here
            
        free_hole = 1;
        txs->update = 0;
       	
        if (++txs_write_ptr == adapter->txs_ring_size)
            txs_write_ptr = 0;
        atomic_set(&adapter->txs_write_ptr, (int)txs_write_ptr);
                
        txd_read_ptr = (u32) atomic_read(&adapter->txd_read_ptr);
        txph = (tx_pkt_header_t*) 
                    (((u8*)adapter->txd_ring) + txd_read_ptr);
        
        if ( txph->pkt_size != txs->pkt_size) {
		
            tx_pkt_status_t* old_txs = txs;
            printk(KERN_WARNING
                    "%s: txs packet size do not coinsist with txd"
		    " txd_:0x%08x, txs_:0x%08x!\n",
		    adapter->netdev->name,
		    *(u32*)txph, *(u32*)txs);
	    	printk(KERN_WARNING
			    "txd read ptr: 0x%x\n",
			    txd_read_ptr);
	    	txs = adapter->txs_ring + txs_write_ptr;
            printk(KERN_WARNING
			    "txs-behind:0x%08x\n",
			    *(u32*)txs);
            if (txs_write_ptr < 2) {
		    txs = adapter->txs_ring + (adapter->txs_ring_size+
				               txs_write_ptr - 2);
	    } else {
		    txs = adapter->txs_ring + (txs_write_ptr - 2);
	    }
	    printk(KERN_WARNING
			    "txs-before:0x%08x\n",
			    *(u32*)txs);
	    txs = old_txs;
        }
        
        txd_read_ptr += (((u32)(txph->pkt_size)+7)& ~3);//4for TPH
        if (txd_read_ptr >= adapter->txd_ring_size)
            txd_read_ptr -= adapter->txd_ring_size;
        
        atomic_set(&adapter->txd_read_ptr, (int)txd_read_ptr);
        
        // tx statistics:
        if (txs->ok)            adapter->net_stats.tx_packets++;
        else                    adapter->net_stats.tx_errors++;
        if (txs->defer)         adapter->net_stats.collisions++;
        if (txs->abort_col)     adapter->net_stats.tx_aborted_errors++;
        if (txs->late_col)      adapter->net_stats.tx_window_errors++;
        if (txs->underun)       adapter->net_stats.tx_fifo_errors++;
    } while (1);
       
    if (free_hole) {
        if(netif_queue_stopped(adapter->netdev) && 
            netif_carrier_ok(adapter->netdev))
            netif_wake_queue(adapter->netdev);
    }
}       


static void
at_intr_rx(struct at_adapter* adapter)
{
    struct net_device *netdev = adapter->netdev;
    rx_desc_t* rxd;
    struct sk_buff* skb;
    
    
    do {
        rxd = adapter->rxd_ring+adapter->rxd_write_ptr;
        if (!rxd->status.update)
            break; // end of tx
	// clear this flag at once
        rxd->status.update = 0; 
        
        if (rxd->status.ok && rxd->status.pkt_size >= 60) {
            int rx_size = (int)(rxd->status.pkt_size-4);
            // alloc new buffer
            skb = netdev_alloc_skb(netdev, rx_size+NET_IP_ALIGN);
            if (NULL == skb) {
                printk(KERN_WARNING"%s: Memory squeeze, deferring packet.\n",
                        netdev->name);
                /* We should check that some rx space is free.
                  If not, free one and mark stats->rx_dropped++. */
                adapter->net_stats.rx_dropped++;
                break;
            }
/*           
	    if (rx_size > 1400) {
                int s,c;
		c = 0;
		printk("rx_size= %d\n", rx_size);
		for (s=0; s < 800; s++) {
		    if (0 == c) {
		        printk("%04x ", s);
		    }
		    printk("%02x ", rxd->packet[s]);
		    if (++c == 16) {
			c = 0;
			printk("\n");
		    }
		}
		printk(KERN_WARNING"\n");
	    }
*/
	    
            skb_reserve(skb, NET_IP_ALIGN);
            skb->dev = netdev;
	    	eth_copy_and_sum(
	        	skb, 
				rxd->packet,
				rx_size, 0);
	    	skb_put(skb, rx_size);
	    /*
            memcpy(skb_put(skb, rx_size), 
                    rxd->packet,
                    rx_size);
            */
            skb->protocol = eth_type_trans(skb, netdev);
#ifdef NETIF_F_HW_VLAN_TX
        
	    if(adapter->vlgrp && (rxd->status.vlan)) {
            u16 vlan_tag = 
                    (rxd->status.vtag>>4)        |
                    ((rxd->status.vtag&7) << 13) |
                    ((rxd->status.vtag&8) << 9)  ;
                DEBUGOUT1("RXD VLAN TAG<RRD>=0x%04x", rxd->status.vtag);
 	        vlan_hwaccel_rx(skb, adapter->vlgrp, vlan_tag);
            } else 
#endif
            netif_rx(skb);
            adapter->net_stats.rx_bytes += rx_size;
            adapter->net_stats.rx_packets++;
	    	netdev->last_rx = jiffies;
	    
        } else { 
            
            adapter->net_stats.rx_errors++; 
            
            if (rxd->status.ok && rxd->status.pkt_size <= 60) {
                adapter->net_stats.rx_length_errors++;
            }
            if (rxd->status.mcast)  adapter->net_stats.multicast++;
            if (rxd->status.crc)    adapter->net_stats.rx_crc_errors++;
            if (rxd->status.align)  adapter->net_stats.rx_frame_errors++;
        }
        
        // advance write ptr
        if (++adapter->rxd_write_ptr == adapter->rxd_ring_size) 
            adapter->rxd_write_ptr = 0;
    } while (1);
    
     
    // update mailbox ?
    adapter->rxd_read_ptr = adapter->rxd_write_ptr;
    AT_WRITE_REGW(&adapter->hw, REG_MB_RXD_RD_IDX, adapter->rxd_read_ptr);
}

inline 
int
TxsFreeUnit(struct at_adapter* adapter)
{
    u32 txs_write_ptr = (u32) atomic_read(&adapter->txs_write_ptr);
        
    return 
        (adapter->txs_next_clear >= txs_write_ptr) ?
        (int) (adapter->txs_ring_size - adapter->txs_next_clear 
                        + txs_write_ptr - 1) :
        (int) (txs_write_ptr - adapter->txs_next_clear - 1);    
}

inline
int
TxdFreeBytes(struct at_adapter* adapter)
{
    u32 txd_read_ptr = (u32)atomic_read(&adapter->txd_read_ptr);
    
    return (adapter->txd_write_ptr >= txd_read_ptr) ?
            (int) (adapter->txd_ring_size - adapter->txd_write_ptr 
                        + txd_read_ptr - 1):
            (int) (txd_read_ptr - adapter->txd_write_ptr - 1);
}



static int
at_xmit_frame(struct sk_buff *skb, struct net_device *netdev)
{
    struct at_adapter *adapter = netdev_priv(netdev);
    unsigned long flags;
    tx_pkt_header_t* txph;
    u32 offset, copy_len;
    int txs_unused;
    int txbuf_unused;
    
    //DEBUGFUNC("at_xmit_frame");    
    
    if (test_bit(__AT_DOWN, &adapter->flags)) {
		dev_kfree_skb_any(skb);
		return NETDEV_TX_OK;
	}
	
	if (unlikely(skb->len <= 0)) {
		dev_kfree_skb_any(skb);
		return NETDEV_TX_OK;
	}
	
#ifdef NETIF_F_LLTX
	local_irq_save(flags);
	if (!spin_trylock(&adapter->tx_lock)) {
		/* Collision - tell upper layer to requeue */
		local_irq_restore(flags);
		return NETDEV_TX_LOCKED;
	}
#else
	spin_lock_irqsave(&adapter->tx_lock, flags);
#endif
    txs_unused = TxsFreeUnit(adapter);
    txbuf_unused = TxdFreeBytes(adapter);

    if (txs_unused < 1 || skb->len > txbuf_unused) {
        // no enough resource
        netif_stop_queue(netdev);
        spin_unlock_irqrestore(&adapter->tx_lock, flags);
		DEBUGOUT("tx busy!!");
        return NETDEV_TX_BUSY;
    }

    offset = adapter->txd_write_ptr;

    txph = (tx_pkt_header_t*) 
            (((u8*)adapter->txd_ring)+offset);
    
    *(u32*)txph = 0; 
    txph->pkt_size = skb->len;

    offset += 4;
    if (offset >= adapter->txd_ring_size)
	    offset -= adapter->txd_ring_size;
    copy_len = adapter->txd_ring_size - offset;
    if (copy_len >= skb->len) {
        memcpy(((u8*)adapter->txd_ring)+offset,
					skb->data,
					skb->len);
		offset += ((u32)(skb->len+3)&~3);
    } else {
		memcpy(((u8*)adapter->txd_ring)+offset,
					skb->data,
					copy_len);
		memcpy((u8*)adapter->txd_ring,
					skb->data+copy_len,
					skb->len-copy_len);
		offset = ((u32)(skb->len-copy_len+3)&~3);
    }
    
#ifdef NETIF_F_HW_VLAN_TX
    if (adapter->vlgrp && vlan_tx_tag_present(skb)) {
        u16 vlan_tag = vlan_tx_tag_get(skb);
        vlan_tag = (vlan_tag << 4)          |
                   (vlan_tag >> 13)         |
                   ((vlan_tag >>9) & 0x8)   ;
        txph->ins_vlan = 1;
		txph->vlan = vlan_tag;
        DEBUGOUT1("TXD VLAN TAG<TPD>=%04x", vlan_tag);
    }
#endif    
    if (offset >= adapter->txd_ring_size)
	    offset -= adapter->txd_ring_size;
    	adapter->txd_write_ptr = offset;

    // clear txs before send
    adapter->txs_ring[adapter->txs_next_clear].update = 0;
    if (++adapter->txs_next_clear == adapter->txs_ring_size)
        adapter->txs_next_clear = 0;
/*
    printk("txd header: 0x%08x, txd write ptr: %08x\n", 
		    *(u32*)txph,
		    adapter->txd_write_ptr);
*/		    
    
    AT_WRITE_REGW(  &adapter->hw, 
                    REG_MB_TXD_WR_IDX, 
                    (adapter->txd_write_ptr>>2));
                    
    spin_unlock_irqrestore(&adapter->tx_lock, flags);
    
    netdev->trans_start = jiffies;
    dev_kfree_skb_any(skb);
    return NETDEV_TX_OK;
}

/**
 * at_phy_config - Timer Call-back
 * @data: pointer to netdev cast into an unsigned long
 **/

static void
at_phy_config(unsigned long data)
{
    struct at_adapter *adapter = (struct at_adapter *) data;
    struct at_hw *hw = &adapter->hw;  
    unsigned long flags;

     DEBUGFUNC("at_phy_reconfig!");
    
    spin_lock_irqsave(&adapter->stats_lock, flags);
    adapter->phy_timer_pending = FALSE;
    at_write_phy_reg(hw, MII_ADVERTISE, hw->mii_autoneg_adv_reg);
    DEBUGOUT("4 register written");
    at_write_phy_reg(hw, MII_BMCR, MII_CR_RESET|MII_CR_AUTO_NEG_EN);
    spin_unlock_irqrestore(&adapter->stats_lock, flags);
}


/**
 * at_watchdog - Timer Call-back
 * @data: pointer to netdev cast into an unsigned long
 **/

static void
at_watchdog(unsigned long data)
{
    struct at_adapter *adapter = (struct at_adapter *) data;
    u32 drop_rxd, drop_rxs;
    unsigned long flags;
    
    spin_lock_irqsave(&adapter->stats_lock, flags);
    drop_rxd = AT_READ_REG(&adapter->hw, REG_STS_RXD_OV);
    drop_rxs = AT_READ_REG(&adapter->hw, REG_STS_RXS_OV);
    adapter->net_stats.rx_over_errors += (drop_rxd+drop_rxs);

    spin_unlock_irqrestore(&adapter->stats_lock, flags);
    
    /* Reset the timer */
    mod_timer(&adapter->watchdog_timer, jiffies + 4 * HZ);
}



