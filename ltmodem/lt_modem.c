/*
 * UNOFFICIAL BUT WORKING LTMODEM DRIVER for 2.6.x Linux kernels
 *  based on original code released by Agere Systems Inc.
 */
/****************************************************************
 * File :  ltmodem.c
 *
 * Copyright (C) 1999, 2000, 2001 Lucent Technologies Inc.
 * Copyright (C) 2001, 2002, 2003 Agere Systems Inc. All rights reserved.
 *
 *
 * Description :
 *	Contains the interface functions for Linux
 *
 ****************************************************************/
/****************************************************************
MRS changes
- support for 2.2 and 2.4 kernels in same file
- support for override vendor and device id's
- fixups to remove warnings from compile
   some return valuse not supplied. various statics removed
   in NOEEPROM ifdef to be consistent with other part
   
    - PCI support for kernel 2.5 (c) Jason Hall
    - minor 2.6 simplification by Aleksey Kondratenko.
	Now it supports 2.6 only.
****************************************************************/
#include <linux/version.h>

#include <linux/module.h>
#include <linux/init.h>

#include <linux/sched.h>   // in order to get jiffies
#include <linux/param.h>   // in order to get HZ
#include <linux/pci.h>     // pci functions
#include <linux/time.h>
#include <asm/io.h>
#include "linuxif.h"

#ifndef MODULE
#error this code cannot be the part of Linux kernel
#endif

#ifndef PCI_DEVICE_ID_ATT_L56XMF
#define PCI_DEVICE_ID_ATT_L56XMF 0x0440
#endif

static char *modem_name = "Lucent Modem Controller driver";
static char *modem_version = "8.26-alk-8";
extern int eeprom_flag;
 
struct timer_list timerList;
asmlinkage int (*lt_rs_interrupt)(void);

spinlock_t modem_driver_lock = SPIN_LOCK_UNLOCKED;

#ifdef MEMSET_HACK
#undef memset
asmlinkage
void *memset(void *p, int val, size_t cnt)
{
	printk(KERN_INFO "ltmodem_memset called\n");
	return __memset(p,val,cnt);
}
#endif

/* define pci config access function */
LT_DEFINE_PCI_OP(read, byte, char *)
LT_DEFINE_PCI_OP(read, word, short *)
LT_DEFINE_PCI_OP(read, dword, int *)
LT_DEFINE_PCI_OP(write, byte, char)
LT_DEFINE_PCI_OP(write, word, short)
LT_DEFINE_PCI_OP(write, dword, int)

asmlinkage
byte Get_PCI_INTERRUPT_LINE(void)
{
	return PCI_INTERRUPT_LINE;
}

asmlinkage
byte Get_PCI_BASE_ADDRESS_1(void)
{
	return PCI_BASE_ADDRESS_1;
}

asmlinkage
byte Get_PCI_BASE_ADDRESS_2(void)
{
	return PCI_BASE_ADDRESS_2;
}

asmlinkage
dword Get_PCI_BASE_ADDRESS_IO_MASK(void)
{
	return PCI_BASE_ADDRESS_IO_MASK;
}

asmlinkage
byte Get_PCI_BASE_ADDRESS_SPACE_IO(void)
{
	return PCI_BASE_ADDRESS_SPACE_IO;
}

asmlinkage
BOOL lt_pci_present(void)
{
	return 1;
}

static
struct PCI_IDs {
	int vendor_id;
	int device_id_start;
	int device_id_last;
} pci_ids[] = {
	{0x115d, 0x0010, 0x03ff}
};

static int vendor_id = 0;
static int device_id = 0;

module_param(vendor_id, int, 0);
module_param(device_id, int, 0);

MODULE_PARM_DESC(vendor_id, "Vendor ID of the Lucent Modem e.g. vendor_id=0x11c1");
MODULE_PARM_DESC(device_id, "Device ID of the Lucent Modem e.g. device_id=0x0440");

static int Forced[4] = {-1,-1,-1,0};

static int Forced_set(const char *val, struct kernel_param *kp)
{
	get_options((char *)val, 4, (int *)Forced);
	return 0;
}
module_param_call(Forced, Forced_set, NULL, NULL, 0);

MODULE_PARM_DESC(Forced, "Forced Irq,BaseAddress,ComAddress[,NoDetect] of the Lucent Modem e.g. Forced=3,0x130,0x2f8");

static
struct pci_dev *__find_device(struct lt_pci_dev_info *lt_dev, unsigned int id, unsigned int num)
{
	int i;
	struct pci_dev *dev = pci_get_device(id,num,0);
	if (!dev || pci_enable_device(dev) < 0 || !dev->irq)
		return 0;
	lt_dev->irq = dev->irq;
	lt_dev->vendor = dev->vendor;
	lt_dev->device = dev->device;
	lt_dev->devfn = dev->devfn;
	lt_dev->bus_num = dev->bus->number;
	lt_dev->subsystem_vendor = dev->subsystem_vendor;
	lt_dev->subsystem_device = dev->subsystem_device;
	for (i=0;i<6;i++) {
		lt_dev->Base_Address[i] = dev->resource[i].start;
		if (dev->resource[i].start)
			lt_dev->Base_Address[i] |= dev->resource[i].flags & 1;
	}
	return dev;
}

static
struct pci_dev *detected_pci_dev;

asmlinkage
BOOL lt_pci_find_device(struct lt_pci_dev_info *lt_dev, unsigned int id, unsigned int num)
{
	if (detected_pci_dev)
		pci_dev_put(detected_pci_dev);

	if ((detected_pci_dev = __find_device(lt_dev,id,num)))
		return TRUE;

	if (id == PCI_VENDOR_ID_ATT && num == PCI_DEVICE_ID_ATT_L56XMF) {
		int i;
		for (i=0;i<sizeof(pci_ids)/sizeof(pci_ids[0]);i++) {
			int devid;
			for (devid=pci_ids[i].device_id_start;devid <= pci_ids[i].device_id_last; devid++)
				if ((detected_pci_dev = __find_device(lt_dev, pci_ids[i].vendor_id, devid)))
					return TRUE;
		}
		if (vendor_id && device_id && (detected_pci_dev == __find_device(lt_dev, vendor_id, device_id)))
			return TRUE;
	}
	return FALSE;
}

static
void lt_put_pci_dev(void)
{
	if (detected_pci_dev)
		pci_dev_put(detected_pci_dev);
	detected_pci_dev = 0;
}

static
void __timer_wrapper(unsigned long data)
{
	((void (asmlinkage *)(unsigned long))data)(0);
}

asmlinkage
void lt_add_timer(void (asmlinkage *timerfunction)(unsigned long))
{
	timerList.expires = jiffies+HZ/100; // 10ms delay
	timerList.function = __timer_wrapper;
	timerList.data = (unsigned long)timerfunction;
	add_timer(&timerList);
}

asmlinkage
dword VMODEM_Get_System_Time(void)
{
	struct timeval time;
	// TODO: investigate if it is safe to call gettimeofday from bh context
	do_gettimeofday(&time);
	return time.tv_usec/1000 + time.tv_sec*1000;
}

asmlinkage
void lt_init_timer(void)
{
	init_timer(&timerList);
}

asmlinkage
byte inp(word addr)
{
	return inb(addr);
}

asmlinkage
void outp(word addr, byte value)
{
	outb(value, addr);
}

asmlinkage
word inpw(word addr)
{
	return inw(addr);
}

asmlinkage
void outpw(word addr, word value)
{
	return outw(value, addr);
}

asmlinkage
dword inpd(word addr)
{
	return inl(addr);
}

asmlinkage
void outpd(word addr, dword value)
{
	return outl(value, addr);
}


asmlinkage
byte dp_regread(byte reg)
{
	unsigned long flags;
	byte ret;
	spin_lock_irqsave(&modem_driver_lock, flags);
	ret = dp_regread_nonint (reg);
	spin_unlock_irqrestore(&modem_driver_lock, flags);
	return ret;
}

asmlinkage
void dp_regwrite(byte reg, byte value)
{
	unsigned long flags = 0;
	spin_lock_irqsave(&modem_driver_lock, flags);
	dp_regwrite_nonint(reg, value);
	spin_unlock_irqrestore(&modem_driver_lock, flags);
}

static
byte modemPortOpen(void)
{
	return vxdPortOpen();
}

static
byte modemPortClose(void)
{
	vxdPortClose();
	return 0;
}

int lt_lucent_detect_modem(struct ltmodem_res *pltmodem_res)
{
	int val = 0;
	if (!Forced[3]) {
		val = lucent_detect_modem(pltmodem_res);
		if (val)
			lt_put_pci_dev();
		else
			printk(KERN_INFO "Detected Parameters Irq=%d BaseAddress=0x%x ComAddress=0x%x\n",
			       Irq,BaseAddress,ComAddress);
	}
	if (Forced[0]>0 && Forced[1]>0 && Forced[2]>0) {
		Irq = pltmodem_res->Irq = Forced[0];
		BaseAddress = pltmodem_res->BaseAddress = Forced[1];
		ComAddress = Forced[2];
		printk(KERN_INFO "Forced Parameters Irq=%d BaseAddress=0x%x ComAddress=0x%x\n",
		       Irq,BaseAddress,ComAddress);
		// fake it
		val = 0;
	}
	return val;
}

int lt_get_modem_interface(struct ltmodem_ops *ops)
{
	ops->detect_modem = lt_lucent_detect_modem;
	ops->init_modem = lucent_init_modem;
	ops->PortOpen = modemPortOpen;
	ops->PortClose = modemPortClose;
	ops->read_vuart_register = read_vuart_port;
	ops->write_vuart_register = write_vuart_port;
	ops->app_ioctl_handler = app_ioctl_handler;
	ops->dsp_isr = dp_dsp_isr;
	ops->put_pci_dev = lt_put_pci_dev;
	ops->io_lock = &modem_driver_lock;
	ops->virtual_isr_ptr = &lt_rs_interrupt;
	return 0;
}
EXPORT_SYMBOL(lt_get_modem_interface);

/* broken, unused */
/* 
 * struct pci_dev *lt_get_dev(void)
 * {
 * 	static struct pci_dev *correct_dev;
 * 	if (unlikely(!correct_dev))
 * 		correct_dev = dev;
 * 	return correct_dev;
 * }
 * EXPORT_SYMBOL(lt_get_dev);
 */

int __init lm_init_module(void)
{
 	extern byte eeprom[];
 	printk(KERN_INFO "Loading %s version %s\n", modem_name, modem_version);
	eeprom_flag = 0;
	eeprom[0] = LT_COUNTRY_ID;	// set the country ID for the Lucent modem
 	return 0;
}

void __exit lm_cleanup_module(void)
{
	printk(KERN_INFO "Unloading %s: version %s\n", modem_name, modem_version);
}

module_init(lm_init_module);
module_exit(lm_cleanup_module);
MODULE_DESCRIPTION("Lucent/Agere linmodem controller driver");
MODULE_LICENSE("Proprietary");

void lin_kill(void) {}
void lin_wake_up(void) {}
void lin_interruptible_sleep_on(void) {}
int lin_signal_pending(void) {return 0;}
int function_1(int (*fn)(void *), char *a) {return 0;}
int function_2(char *p) {return 0;}
