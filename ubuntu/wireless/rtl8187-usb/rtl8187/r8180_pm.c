/* 
   Power management interface routines. 
   Written by Mariusz Matuszek.
   This code is currently just a placeholder for later work and 
   does not do anything useful.
   
   This is part of rtl8180 OpenSource driver.
   Copyright (C) Andrea Merello 2004  <andreamrl@tiscali.it> 
   Released under the terms of GPL (General Public Licence)	
*/

#ifdef CONFIG_RTL8180_PM


#include "r8180_hw.h"
#include "r8180_pm.h"
#include "r8187.h"
int rtl8180_save_state (struct pci_dev *dev, u32 state)
{
        printk(KERN_NOTICE "r8180 save state call (state %u).\n", state);
	return(-EAGAIN);
}


int rtl8187_suspend (struct usb_interface *intf,
	pm_message_t state)
{
	struct r8180_priv *priv;
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)
	struct net_device *dev = usb_get_intfdata(intf);
#else
	//Will be patched.
	//struct net_device *dev = (struct net_device *)ptr;
#endif

	if(dev)
	{
		netif_carrier_off(dev);
		netif_stop_queue(dev);
		netif_device_detach(dev);

		// mark halt flag
		//RTMP_SET_FLAG(pAd, fRTMP_ADAPTER_HALT_IN_PROGRESS);
		//RTMP_SET_FLAG(pAd, fRTMP_ADAPTER_RADIO_OFF);

		// take down the device
		//rtl8180_close(dev);
		dev->stop(dev);
	}

	return 0;
	//	printk(KERN_NOTICE "r8180 suspend call (state %u).\n", state);
	//return(-EAGAIN);
}


int rtl8187_resume (struct usb_interface *intf)
{
	struct r8180_priv *priv;
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)
	struct net_device *dev = usb_get_intfdata(intf);
#else
	//Will be patched.
	//struct net_device *dev = (struct net_device *)ptr;
#endif

	if(dev)
	{

		netif_device_attach(dev);

		//rtl8180_open(dev) ;
		dev->open(dev);
		
		return 0;
	}	

	return 0;
	   // printk(KERN_NOTICE "r8180 resume call.\n");
	//return(-EAGAIN);
}


int rtl8180_enable_wake (struct pci_dev *dev, u32 state, int enable)
{
		
		//printk(KERN_NOTICE "r8180 enable wake call (state %u, enable %d).\n", 
	//       state, enable);
	//return(-EAGAIN);
}



#endif //CONFIG_RTL8180_PM
