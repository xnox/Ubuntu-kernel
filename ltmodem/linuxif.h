/*name and version number:@(#)linuxif.h	1.13*/
/*date of get: 		  04/18/01 10:59:51*/
/*date of delta:	  02/20/01 19:05:19*/
/****************************************************************
File :  linuxif.h
Description :
	Contains the interface functions for Linux

Procedures Contained :

****************************************************************/
/****************************************************************
MRS changes
- support for all kernel versions post 2.2
- return value consistency for no compiler warnings

Copyright (C) 1999, 2000, 2001 Lucent Technologies Inc.
Copyright (C) 2001, 2002, 2003 Agere Systems Inc. All rights reserved.

****************************************************************/
#ifndef __LT_LINUXIF_H__
#define __LT_LINUXIF_H__
#include <linux/pci.h>
#include <asm/linkage.h>

#define LT_COUNTRY_ID	0x19

#define TRUE 1
#define FALSE 0

typedef unsigned char byte;
typedef unsigned short word;
typedef unsigned long dword;
typedef int BOOL;

#define asstr2(x) #x
#define asstr(x) asstr2(x)

#define ltserialstring	 "ltserial"

struct ltmodem_res
{
	word BaseAddress;
	byte Irq;
};

struct ltmodem_ops
{
	int (*detect_modem)(struct ltmodem_res *pltmodem_res /*, int passnumber*/ /*, struct pci_dev **dev */);
	void (asmlinkage *init_modem)(void);
	byte (*PortOpen)(void);
	byte (*PortClose)(void);
	int (asmlinkage *read_vuart_register)(int offset);
	void (asmlinkage *write_vuart_register)(int offset, int value);
	int (asmlinkage *app_ioctl_handler)(unsigned int cmd, unsigned long arg);
	byte (asmlinkage *dsp_isr)(void);
	void (*put_pci_dev)(void);
	spinlock_t *io_lock;
	int (* asmlinkage * virtual_isr_ptr)(void);
};


/* serial <-> ltmodem interface. Exported from lt_modem.c */
int lt_get_modem_interface(struct ltmodem_ops *ops);

/* this magic Lucent/Agere ioctrl's are not used, but are implemented */
#define IOCTL_MODEM_APP_1	_IOR(62,41,char *)
#define IOCTL_MODEM_APP_2	_IOR(62,42,char *)
#define IOCTL_MODEM_APP_3	_IOR(62,43,char *)
#define IOCTL_MODEM_APP_4	_IOR(62,44,char *)
#define IOCTL_MODEM_APP_5	_IOR(62,45,char *)
#define IOCTL_MODEM_APP_6	_IOR(62,46,char *)
#define IOCTL_MODEM_APP_7	_IOR(62,47,char *)
#define IOCTL_MODEM_APP_8	_IOR(62,48,char *)

/*
 * binary core API
 */
asmlinkage int lucent_detect_modem(void *);
asmlinkage void lucent_init_modem(void);
asmlinkage int read_vuart_port(int offset);
asmlinkage void write_vuart_port(int offset, int value);
asmlinkage int app_ioctl_handler(unsigned int cmd, unsigned long arg);
asmlinkage byte dp_dsp_isr(void);
asmlinkage byte vxdPortOpen(void);
asmlinkage void vxdPortClose(void);
asmlinkage byte dp_regread_nonint(byte reg);
asmlinkage byte dp_regwrite_nonint(byte reg,byte value);

extern unsigned short BaseAddress;
extern unsigned short ComAddress;
extern unsigned char Irq;

/*
 * API used by binary core
 */

/*
 * I don't know who put this 0x10, etc comments,
 * but they reflect esp offsets to this fields in lucent_detect_modem
 */
struct lt_pci_dev_info
{
	unsigned short	irq;			// 0x10
	unsigned short	vendor;			// 0x12
	unsigned short	device;			// 0x14
	unsigned short	subsystem_vendor;	// 0x16
	unsigned short	subsystem_device;	// 0x18
	unsigned short	devfn;			// 0x1a
	unsigned char	bus_num;		// 0x1c
	unsigned long 	Base_Address[6];	// 0x20
};

#define LT_DEFINE_PCI_OP(rw,size,type)							\
asmlinkage									\
int lt_pcibios_##rw##_config_##size(unsigned char bus, unsigned char dev_fn,	\
				  unsigned char where, unsigned type val)	\
{										\
	struct pci_dev *pcidev = pci_find_slot(bus, dev_fn);			\
	if (!pcidev)								\
		return PCIBIOS_DEVICE_NOT_FOUND;				\
	return pci_##rw##_config_##size(pcidev, where, val);			\
}

#define LT_DECLARE_PCI_OP(rw,size,type)							\
asmlinkage int lt_pcibios_##rw##_config_##size(unsigned char bus, unsigned char dev_fn,	\
	 				unsigned char where, unsigned type val)

LT_DECLARE_PCI_OP(read, byte, char *);
LT_DECLARE_PCI_OP(read, word, short *);
LT_DECLARE_PCI_OP(read, dword, int *);
LT_DECLARE_PCI_OP(write, byte, char);
LT_DECLARE_PCI_OP(write, word, short);
LT_DECLARE_PCI_OP(write, dword, int);

asmlinkage byte Get_PCI_INTERRUPT_LINE(void);
asmlinkage byte Get_PCI_BASE_ADDRESS_1(void);
asmlinkage byte Get_PCI_BASE_ADDRESS_2(void);
asmlinkage dword Get_PCI_BASE_ADDRESS_IO_MASK(void);
asmlinkage byte Get_PCI_BASE_ADDRESS_SPACE_IO(void);
asmlinkage BOOL lt_pci_present(void);
asmlinkage BOOL lt_pci_find_device (struct lt_pci_dev_info *lt_dev, unsigned int id, unsigned int num);

asmlinkage void lt_add_timer(void (asmlinkage *timerfunction)(unsigned long));
asmlinkage dword VMODEM_Get_System_Time(void);
asmlinkage void lt_init_timer(void);
asmlinkage byte inp(word addr);
asmlinkage void outp(word addr, byte value);
asmlinkage word inpw(word addr);
asmlinkage void outpw(word addr, word value);
asmlinkage dword inpd(word addr);
asmlinkage void outpd(word addr, dword value);
asmlinkage byte dp_regread(byte reg);
asmlinkage void dp_regwrite(byte reg, byte value);
asmlinkage void *ltmodem_memset(void *p, int val, size_t cnt);

/* for userspace application. They were disabled in original driver and I haven't seen or heard of such app */
/* they are empty and thus are not asmlinkage */
void lin_kill(void);
void lin_wake_up(void);
void lin_interruptible_sleep_on(void);
int lin_signal_pending(void);
int function_1(int (*fn)(void *), char *a);
int function_2(char *p);

#endif /* __LT_LINUXIF_H__ */
