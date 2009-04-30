/* 
 * driver.c
 * Copyright (C) 2002, AVM GmbH. All rights reserved.
 * 
 * This Software is  free software. You can redistribute and/or
 * modify such free software under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * The free software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this Software; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA, or see
 * http://www.opensource.org/licenses/lgpl-license.html
 * 
 * Contact: AVM GmbH, Alt-Moabit 95, 10559 Berlin, Germany, email: info@avm.de
 */

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/atomic.h>
#include <asm/uaccess.h>
#include <asm/system.h>
#include <linux/version.h>
#include <linux/ioport.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/ctype.h>
#include <linux/string.h>
#include <linux/capi.h>
#include <linux/isdn/capilli.h>
#include <linux/isdn/capiutil.h>
#include <linux/isdn/capicmd.h>
#include <stdarg.h>
#include "libstub.h"
#include "tables.h"
#include "queue.h"
#include "lib.h"
#include "image.h"
#include "main.h"
#include "tools.h"
#include "defs.h"
#include "regs.h"
#include "driver.h"

#if !defined (NDEBUG) && defined (LOG_LINK)
#include "dbgif.h"
#endif

#define	KILOBYTE		1024
#define	MEGABYTE		(1024*KILOBYTE)
#define	DMA_BUFFER_SIZE		(9*KILOBYTE)
#define	_2_MEGS_		(2*MEGABYTE)
#define	_8_MEGS_		(8*MEGABYTE)
#define	TEN_MSECS		(HZ/100)
#define	FW_TIMEOUT		100
#define	TRIGGER_INT_MAX_DELAY	1000		/* microseconds */

#define	PPRINTF_LEN		256

#define	CARD_BIU_CTL_SE		1		/* Swap Bit: Byte swapped */
#define	CARD_BIU_CTL_HE		(1<<9)		/* Host enable */
#define	CARD_BIU_CTL_CR		(1<<10)		/* Clear Reset */
#define	CARD_BIU_CTL_SR		(1<<11)		/* Set Reset */

#define CARD_PCI_INT_ASSERT	(1<<0)
#define	CARD_PCI_INT_ENABLE	(1<<4)
#define	CARD_PCI_INT_ISASSERTED	(1<<8)
#define	CARD_TM_HOST_INTR	28
#define	CARD_TM_HOST_INTR_BIT	(1<<CARD_TM_HOST_INTR)

#define	XFER_TOTM_STATUS	IO_ADR
#define	XFER_TOPC_STATUS	IO_DATA

#define	DMA_RESET_LOOPS		10		/* 100ms wait period a loop */
#define	SDRAM_FIRMWARE_BASE	0x00000000	/* FW load address */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
card_p				capi_card		= NULL;
lib_callback_t *		capi_lib		= NULL;
struct capi_ctr *		capi_controller[2]	= { NULL, NULL } ;

static int			nvers			= 0;
static atomic_t			link_open		= ATOMIC_INIT (0);
static atomic_t			scheduler_enabled	= ATOMIC_INIT (1);
static atomic_t			got_kicked		= ATOMIC_INIT (0);
static atomic_t			tx_pending		= ATOMIC_INIT (0);
static atomic_t			rx_pending		= ATOMIC_INIT (0);
static atomic_t			crit_count		= ATOMIC_INIT (0);
static bundle_t			ctrl_context[2];
static struct capi_ctr		capi_ctrl[2];
static int			capi_ctrl_ix		= 0;
static per_ctrl_t		ctrl_params[2];
static spinlock_t		stack_lock		= SPIN_LOCK_UNLOCKED;
static atomic_t			rx_flag			= ATOMIC_INIT (0);
static atomic_t			tx_flag			= ATOMIC_INIT (0);
static atomic_t			thread_flag		= ATOMIC_INIT (0);
static atomic_t			thread_capi_flag	= ATOMIC_INIT (0);
static int			thread_pid		= -1;

static DECLARE_WAIT_QUEUE_HEAD(wait);
static DECLARE_WAIT_QUEUE_HEAD(capi_wait);
static DECLARE_COMPLETION(thread_sync); /* New DECLARE, <arnd.feldmueller@web.de> */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)) 
static DECLARE_MUTEX_LOCKED(hotplug);
#endif

#define SCHED_WAKEUP_CAPI       { atomic_set (&thread_capi_flag, 1); wake_up_interruptible (&capi_wait); }
#define SCHED_WAKEUP            { atomic_set (&got_kicked, 1); wake_up_interruptible (&wait); }

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static void			tx_task (unsigned long data);
static void			rx_task (unsigned long data);
static void			tx_handler (card_p cp);
static void			rx_handler (card_p cp);
static irqreturn_t		irq_handler (int, void *);
static void			enable_thread (void);
static void			disable_thread (void);
static int			make_thread (void);
static void			kill_thread (void);

static __attr void		scheduler_control (unsigned);
static __attr void		wakeup_control (unsigned);
static __attr void		version_callback (char *);
static __attr unsigned		scheduler_suspend (unsigned long);
static __attr unsigned		scheduler_resume (void);
static __attr unsigned		controller_remove (void);
static __attr unsigned		controller_add (void);

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static DECLARE_TASKLET_DISABLED(tx_tasklet, tx_task, 0);
static DECLARE_TASKLET_DISABLED(rx_tasklet, rx_task, 0);

static functions_t		ca_funcs = {

	7,
	scheduler_control,
	wakeup_control,
	version_callback,
	scheduler_suspend,
	scheduler_resume,
	controller_remove,
	controller_add
} ;

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
#define	PEEK(p)			readl((p)) 
#define	POKE(p,v)		writel((v),(p))

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
#define	PCI_NO_RESOURCE		4
#define PCI_NO_PCI_KERN		3
#define PCI_NO_CARD		2
#define PCI_NO_PCI		1
#define PCI_OK			0

static int alloc_resources (struct pci_dev * dev, card_p cp) {
	int	res;
	
	/* Setting up card's I/O memory window... */
	if (!(cp->io_base = ioremap_nocache (cp->addr_mmio, cp->len_mmio))) {
		LOG("Could not map I/O region into virtual memory.\n");
	io_map_exit:
		return PCI_NO_RESOURCE;
	}
	LOG("I/O memory mapped to 0x%08x\n", cp->io_base);
	if (!(cp->mem_base = ioremap_nocache (cp->addr_sdram, cp->len_sdram))) {
		LOG("Could not map RAM region into virtual memory.\n");
	mem_map_exit:
		iounmap (cp->io_base);
		cp->io_base = NULL;
		goto io_map_exit;
	}
	LOG("Controller RAM mapped to 0x%08x\n", cp->mem_base);
	pci_set_master (dev);
	/* Getting DMA buffers... */
	if (NULL == (cp->rx_dmabuf = kmalloc (DMA_BUFFER_SIZE, GFP_ATOMIC))) {
		LOG("Could not allocate rx dma buffer.\n");
	dma_rx_exit:
		iounmap (cp->mem_base);
		cp->mem_base = NULL;
		goto mem_map_exit;
	}
	if (NULL == (cp->tx_dmabuf = kmalloc (DMA_BUFFER_SIZE, GFP_ATOMIC))) {
		LOG("Could not allocate tx dma buffer.\n");
	dma_tx_exit:
		kfree (cp->rx_dmabuf);
		cp->rx_dmabuf = NULL;
		goto dma_rx_exit;
	}
	res = request_irq (cp->irq, &irq_handler, SA_INTERRUPT | SA_SHIRQ, TARGET, cp);
	if (res) {
		LOG("Could not install irq handler.\n");
		goto dma_tx_exit;
	} else {
		LOG("IRQ #%d assigned to " TARGET " driver.\n", cp->irq);
	}
	cp->tx_dmabuf_b = (void *) virt_to_bus (cp->tx_dmabuf);
	cp->rx_dmabuf_b = (void *) virt_to_bus (cp->rx_dmabuf);
	LOG("DMA buffers: rx %08x (bus %08x)\n", cp->rx_dmabuf, cp->rx_dmabuf_b);
	LOG("DMA buffers: tx %08x (bus %08x)\n", cp->tx_dmabuf, cp->tx_dmabuf_b);
	make_thread ();
	/* That's it */
	return PCI_OK;
} /* alloc_resources */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static void free_interrupt (card_p cp) {

	assert (cp != NULL);
	if (cp->irq != 0) {
		free_irq (cp->irq, cp);
		cp->irq = 0;
	}
} /* free_interrupt */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static void free_resources (card_p cp) {

	assert (cp->rx_dmabuf != NULL);
	assert (cp->tx_dmabuf != NULL);
	assert (cp->io_base != NULL);
	assert (cp->mem_base != NULL);

	kill_thread ();
	free_interrupt (cp);
	kfree (cp->rx_dmabuf);
	kfree (cp->tx_dmabuf);
	iounmap (cp->mem_base);
	iounmap (cp->io_base);
	LOG("Resources freed.\n");
} /* free_resources */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static void stop_card (card_p cp) {
	unsigned val;

	/* Disable interrupts */
	POKE (cp->io_base + IMASK, 0);
	POKE (cp->io_base + ICLEAR, ~0U);
	/* Reset CPU */
	val = PEEK (cp->io_base + BIU_CTL);
	LOG("stop: BIU_CTL = 0x%08x\n", val);
	
	POKE (cp->io_base + BIU_CTL, val & ~CARD_BIU_CTL_CR);
	val = PEEK (cp->io_base + BIU_CTL);
	LOG("stop: BIU_CTL = 0x%08x\n", val);
	
	POKE (cp->io_base + BIU_CTL, val | CARD_BIU_CTL_SR);
	val = PEEK (cp->io_base + BIU_CTL);
	LOG("stop: BIU_CTL = 0x%08x\n", val);
} /* stop_card */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static int reset_card (card_p cp) {
	unsigned val, N;

	LOG("reset: BIU_CTL = 0x%08x\n", PEEK (cp->io_base + BIU_CTL));
	/* Reset CPU */
	stop_card (cp);
	
	/* SDO */
	POKE (cp->io_base + SDO_CTL, 1UL<<31);
	
	/* Reset DMA */
	N = 0;
	while (1 == (PEEK (cp->io_base + BIU_STATUS) & (1<<4))) {	
		LOG("Waiting for DMA completion...\n");
		mdelay (100);
		if (++N == DMA_RESET_LOOPS) {
			LOG("Waited too long.\n");
			return FALSE;
		}
	}

	/* Reset Audio Out */
	val = PEEK (cp->io_base + AO_FREQ);
	POKE (cp->io_base + AO_CTL, 0x80000000);
	POKE (cp->io_base + AO_FREQ, val);
	
	/* Reset Audio In */
	val = PEEK (cp->io_base + AI_FREQ);
	POKE (cp->io_base + AI_CTL, 0x80000000);
	POKE (cp->io_base + AI_FREQ, val);
	
	/* Reset Video Out */
	POKE (cp->io_base + VO_CTL, 0x80000000);
	POKE (cp->io_base + VO_CLOCK, 0x00000000);
	
	/* Reset Video In */
	POKE (cp->io_base + VI_CTL, 0x00080000);
	POKE (cp->io_base + VI_CLOCK, 0x00000000);
	
	/* Reset SSI */
	POKE (cp->io_base + SSI_CTL, 0xc0000000);
	
	/* Reset ICP */
	POKE (cp->io_base + ICP_SR, 0x00000080);
	
	/* Reset IIC */
	POKE (cp->io_base + IIC_CTL, 0x00000000);
	
	/* Reset VLD */
	POKE (cp->io_base + VLD_COMMAND, 0x00000401);
	
	/* Reset Timer */
	val = PEEK (cp->io_base + TIMER1_TCTL);
	POKE (cp->io_base + TIMER1_TCTL, val & ~1);
	val = PEEK (cp->io_base + TIMER2_TCTL);
	POKE (cp->io_base + TIMER2_TCTL, val & ~1);
	val = PEEK (cp->io_base + TIMER3_TCTL);
	POKE (cp->io_base + TIMER3_TCTL, val & ~1);
	val = PEEK (cp->io_base + SYSTIMER_TCTL);
	POKE (cp->io_base + SYSTIMER_TCTL, val & ~1);
	
	/* Reset Debug Support */
	POKE (cp->io_base + BICTL, 0);
	POKE (cp->io_base + BDCTL, 0);
	
	/* Reset JTAG */
	POKE (cp->io_base + JTAG_DATA_IN, 0);
	POKE (cp->io_base + JTAG_DATA_OUT, 0);
	POKE (cp->io_base + JTAG_CTL, 4);

	/* Reset peripheral chip */
	LOG("Peripheral chip reset.\n");
	POKE (cp->io_base + INT_CTL, 1<<2 | 1<<6);

	/* Block Power Down */
	LOG("Block power down.\n");
	POKE (cp->io_base + BLOCK_POWER_DOWN, ~0U);
	
	LOG("Reset completed.\n");
	return TRUE;
} /* reset_card */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static int test_card (card_p cp) {
	int	res;
	
#define	DOTEST(base, reg)	POKE (cp->base + reg, 0xCAFEBABE);		\
				res = (PEEK (cp->base + reg) == 0xCAFEBABE)	\
					? TRUE : FALSE;				\
				info (res);					\
				if (!res) return FALSE;
			
	/* MMIO Test */
	DOTEST (io_base, CONFIG_DATA);
	POKE (cp->io_base + CONFIG_DATA, 0);

	/* SDRAM Test */
	DOTEST (mem_base, SDRAM_FIRMWARE_BASE);
	
	LOG("Test completed.\n");
	return TRUE;
	
#undef DOTEST
	
} /* test_card */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static int params_ok (card_p cp) {
	int		ctl;
	unsigned long	limit;

	assert (cp != NULL);
	if (0 == cp->irq) {
		ERROR(
			"IRQ not assigned by BIOS. Please check BIOS"
			"settings/manual for a proper PnP/PCI-Support.\n"
		);
		return FALSE;
	}

	if (cp->addr_mmio != PEEK (cp->io_base + MMIO_BASE)) {
		ctl = CARD_BIU_CTL_SE | CARD_BIU_CTL_SR | CARD_BIU_CTL_HE;
		LOG(
			"Enable little endian mode, writing 0x%08x (swapped: 0x%08x) to BIU_CTL\n", 
			ctl, 
			swab32 (ctl)
		);
		POKE (cp->io_base + BIU_CTL, swab32 (ctl));
	}
	reset_card (cp);
	limit = (unsigned long) PEEK (cp->io_base + DRAM_BASE) + cp->len_sdram;
	POKE (cp->io_base + DRAM_LIMIT, limit);
	POKE (cp->io_base + DRAM_CACHEABLE_LIMIT, limit);
	assert (cp->addr_mmio == PEEK (cp->io_base + MMIO_BASE));
	return test_card (cp);
} /* params_ok */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static int prepare_card (card_p cp, struct pci_dev * dev) {
	
	assert (cp != NULL);
	assert (dev != NULL);
	assert (pci_resource_len (dev, 0) == _8_MEGS_);
	assert (pci_resource_len (dev, 1) == _2_MEGS_);
	cp->addr_sdram = pci_resource_start (dev, 0);
	cp->len_sdram  = pci_resource_len (dev, 0);
	cp->addr_mmio  = pci_resource_start (dev, 1);
	cp->len_mmio   = pci_resource_len (dev, 1);
	cp->irq        = dev->irq;
	LOG("PCI: " PRODUCT_LOGO ", dev %04x, irq %d, sdram %08x (%dM), mmio %08x (%dM)\n", 
		dev->device, 
		cp->irq,
		cp->addr_sdram, cp->len_sdram / MEGABYTE,
		cp->addr_mmio,  cp->len_mmio  / MEGABYTE
	);
	return alloc_resources (dev, cp);
} /* prepare_card */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static struct capi_ctr * get_capi_ctr (int ctrl) {
	int	ix;
	
	assert (capi_card != NULL);
	assert (capi_card->ctrl2 > 0);
	ix = (capi_card->ctrl2 == ctrl) ? 1 : 0;
	info (NULL != capi_controller[ix]);
	return capi_controller[ix];
} /* get_capi_ctr */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
int nbchans (struct capi_ctr * ctrl) {
	per_ctrl_p	C;
	unsigned char *	prf;
	int		temp = 2;
    
	assert (ctrl != NULL);
	assert (ctrl->driverdata);
	C = GET_CTRL (ctrl);
	prf = (unsigned char *) C->string[6];
	if (prf != NULL) {
		temp = prf[2] + 256 * prf[3];
	}
	return temp;
} /* nbchans */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static void scan_version (struct capi_ctr * ctrl, const char * ver) {
	int		vlen, i;
	char *		vstr;
	per_ctrl_p	C;

	assert (ctrl != NULL);
	assert (ctrl->driverdata);
	C = GET_CTRL (ctrl);
	assert (C != NULL);
	vlen = (unsigned char) ver[0];
	C->version = vstr = (char *) hmalloc (vlen);
	if (NULL == vstr) {
		LOG("Could not allocate version buffer.\n");
		return;
	}
	lib_memcpy (C->version, ver + 1, vlen);
	i = 0;
	for (i = 0; i < 8; i++) {
		C->string[i] = vstr + 1;
		vstr += 1 + *vstr;
	} 
#if defined (NDEBUG)
	NOTE( "Stack version %s\n", C->string[0]);
#endif
	LOG("Library version:    %s\n", C->string[0]);
	LOG("Card type:          %s\n", C->string[1]);
	LOG("Capabilities:       %s\n", C->string[4]);
	LOG("D-channel protocol: %s\n", C->string[5]);
} /* scan_version */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static void copy_version (struct capi_ctr * ctrl) {
	char *		tmp;
	per_ctrl_p	C;
	
	assert (ctrl != NULL);
	assert (ctrl->driverdata);
	C = GET_CTRL (ctrl);
	assert (C != NULL);
	if (NULL == (tmp = C->string[3])) {
		ERROR("Do not have version information...\n");
		return;
	}
	lib_strncpy (ctrl->serial, tmp, CAPI_SERIAL_LEN);
	lib_memcpy (&ctrl->profile, C->string[6], sizeof (capi_profile));
	lib_strncpy (ctrl->manu, "AVM GmbH", CAPI_MANUFACTURER_LEN);
	ctrl->version.majorversion = 2;
	ctrl->version.minorversion = 0;
	tmp = C->string[0];
	ctrl->version.majormanuversion = (((tmp[0] - '0') & 15) << 4)
					+ ((tmp[2] - '0') & 15);
	ctrl->version.minormanuversion = ((tmp[3] - '0') << 4)
					+ (tmp[5] - '0') * 10
					+ ((tmp[6] - '0') & 15);
} /* copy_version */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static void kill_version (struct capi_ctr * ctrl) {
	int		i;
	per_ctrl_p	C;
	
	assert (ctrl != NULL);
	assert (ctrl->driverdata);
	C = GET_CTRL (ctrl);
	assert (C != NULL);
	for (i = 0; i < 8; i++) {
		C->string[i] = NULL;
	}
	if (C->version != NULL) {
		hfree (C->version);
		C->version = NULL;
	}
} /* kill_version */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static void pprintf (char * page, int * len, const char * fmt, ...) {
	va_list args;

	va_start (args, fmt);
	*len += vsnprintf (page + *len, PPRINTF_LEN, fmt, args);
	va_end (args);
} /* pprintf */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static int start (card_p card) {

	card->count = 0;
	table_init (&card->appls);
	queue_init (&card->queue);
	(*capi_lib->cm_register_ca_functions) (&ca_funcs);
	if ((*capi_lib->cm_start) ()) {
		LOG("Starting the stack failed...\n");
		return FALSE;
	}
	(void) (*capi_lib->cm_init) ((unsigned) card->mem_base, card->irq);
	assert (nvers == 2);

	assert (!card->running);
	tasklet_enable (&tx_tasklet);
	tasklet_enable (&rx_tasklet);
	enter_critical ();
	if ((*capi_lib->cm_activate) ()) {
		LOG("Activation failed.\n");
		leave_critical ();
		(*capi_lib->cm_exit) ();
		tasklet_disable (&tx_tasklet);
		tasklet_disable (&rx_tasklet);
		queue_exit (&card->queue);
		table_exit (&card->appls);
		return FALSE;
	}
	leave_critical ();
	enable_thread ();
	return (card->running = TRUE);
} /* start */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static void stop (card_p card) {

	if (!card->running) {
		LOG("Stack not initialized.\n");
		return;
	}

	LOG("Stop:\n");
	if (thread_pid != -1) {
		disable_thread ();
	}
	(*capi_lib->cm_exit) ();
	tasklet_disable (&tx_tasklet);
	tasklet_disable (&rx_tasklet);
	card->running = FALSE;

	queue_exit (&card->queue);
	table_exit (&card->appls);
	if (ctrl_context[0].ctrl != NULL) {
		kill_version (capi_controller[0]);
	}
	kill_version (capi_controller[1]);
	nvers = 0;
} /* stop */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static int relocate_and_load (
	card_p		cp,
	void *		image,
	unsigned	image_len,
	unsigned long *	ptx, 
	unsigned long *	prx
#if defined (LOG_LINK)
	,
	unsigned long *	pstart,
	unsigned long * ptrace
#endif
	) {
	int		res;
	int		size, align;
	unsigned	usize;
	image_p		fw;

#if !defined (NDEBUG)
	int		chk_line;
	char *		chk_stmt;
#define CHECK(x)	chk_line = __LINE__; chk_stmt = #x; \
			if (IMAGE_OK != (res = (x))) goto error_exit; 
#else
#define CHECK(x)	if (IMAGE_OK != (res = (x))) goto error_exit;
#endif
	
	assert (image != NULL);
	assert (ptx   != NULL);
	assert (prx   != NULL);
	assert (image_len > 0);

	if (NULL == (fw = lib_init (cp->mem_base, cp->addr_sdram, cp->addr_mmio, 
				    cp->len_sdram, image, image_len))) {
		return IMAGE_RELOC_ERROR;
	}
	CHECK( lib_reloc (fw) );
	assert (cp->addr_mmio != 0);
	CHECK( lib_patch (fw, "_g_uiMMIOBase", cp->addr_mmio) );

	size = align = -1;
	CHECK( lib_getsize (fw, &size, &align) );
	LOG("load: size = %d, align = %d\n", size, align);
	assert ((unsigned) size <= cp->len_sdram);
	
	usize = IMAGE_ADD_ROUND (size, /* add */ 1<<15, /* round */ 1<<6);
	assert (usize < cp->len_sdram);
	LOG("load: rounded size = %u\n", usize);
	
	CHECK( lib_getimage (fw, IMAGE_FREQUENCY, usize) );
	CHECK( lib_load_symtab (fw) );
	CHECK( lib_findsym (fw, "_g_pToTMTransferInfoBase", ptx) );
	CHECK( lib_findsym (fw, "_g_pToPCTransferInfoBase", prx) );
#if defined (LOG_LINK)
	if (IMAGE_OK != lib_findsym (fw, "_g_pDbgMemStart", pstart)) {
		*pstart = 0;
	}
	if (IMAGE_OK != lib_findsym (fw, "_g_pDbgMemTrace", ptrace)) {
		*ptrace = 0;
	}
#endif
	res = lib_load (fw);
	lib_remove (&fw);
	return res;
	
error_exit:
	lib_remove (&fw);
	LOG("load: In line %d: %s FAILED!\n", chk_line, chk_stmt); 
	LOG("load: reason \"%s\"[%d].\n", lib_error (res), res);
	return res;

#undef CHECK
	
} /* relocate_and_load */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static void start_ware (card_p cp) {
	unsigned int	val;

	POKE (cp->io_base + BLOCK_POWER_DOWN, 0);

	POKE (cp->io_base + IMASK, 0);			
	POKE (cp->io_base + ICLEAR, ~0U);

	val = PEEK (cp->io_base + BIU_CTL);
	LOG("start: 0: 0x%08x\n", val);

	POKE (cp->io_base + BIU_CTL, val & ~((unsigned) CARD_BIU_CTL_SR));
	val = PEEK (cp->io_base + BIU_CTL);
	LOG("start: 1: 0x%08x\n", val);
	
	POKE (cp->io_base + BIU_CTL, val |  ((unsigned) CARD_BIU_CTL_CR));
	val = PEEK (cp->io_base + BIU_CTL);
	LOG("start: 2: 0x%08x\n", val);
} /* start_ware */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
#if defined (TM_TIMESTAMP)

#define TMTIMER_PER_SECOND      (1000*1000)
#define TMTIMER_PER_SEC_2       (TMTIMER_PER_SECOND/2)
#define DIVISOR                 ((IMAGE_FREQUENCY+TMTIMER_PER_SEC_2)/TMTIMER_PER_SECOND)
#define	WRAP			(((unsigned) -1)/DIVISOR)

#define	RX_TIMER		200	/* µs */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
unsigned get_tm_timer (void) {
	unsigned timer;
	
	assert (capi_card != NULL);
	assert (DIVISOR > 0);
	timer = PEEK (capi_card->io_base + TIMER2_TVALUE) / DIVISOR;
	return timer;
} /* get_tm_timer */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static unsigned timer_status (unsigned start, unsigned now) {
	unsigned	diff;

	info (now >= start);
	if (now >= start) {
		diff = now - start;
	} else {
		info (WRAP >= start);
		diff = WRAP - start + now;
	}
	return diff;
} /* timer_status */

#endif

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static void * map_addr_from_tm_ptr (card_p cp, unsigned long X) {
	unsigned long	res = X;
	
	assert (res > cp->addr_sdram);
	res -= cp->addr_sdram;
	assert (res < cp->len_sdram);
	res += (unsigned) cp->mem_base;
	LOG("map: TM 0x%08x mapped to 0x%08x\n", X, res);
	return (void *) res;
} /* deref_map_addr_from_tm_ptr */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static void * deref_map_addr_from_tm_ptr (card_p cp, unsigned long X) {
	unsigned long	res;
	
	res = PEEK (cp->mem_base + (X - (unsigned long) cp->addr_sdram));
	LOG("deref: TM 0x%08x = %08x\n", X, res);
	return map_addr_from_tm_ptr (cp, res);
} /* deref_map_addr_from_tm_ptr */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static int __kcapi load_ware (struct capi_ctr * ctrl, capiloaddata * ware) {
	card_p		cp;
	char *		buf;
	unsigned long	utx;
	unsigned long	urx;
	unsigned	val;
	unsigned	count;
#if !defined (NDEBUG)
	unsigned	biu_ctl;
#if defined (LOG_LINK)
	unsigned long	ustart;
	unsigned long	utrace;
#endif
#endif
	assert (ware != NULL);
	assert (ctrl != NULL);
	assert (ctrl->driverdata != NULL);
	cp = GET_CARD (ctrl);
	if (cp->running) {
		ERROR("Error: Controller firmware has already been loaded.\n");
		return 0;
	}
	if ((unsigned) ware->firmware.len > cp->len_sdram) {
		ERROR("Error: Invalid firmware file.\n");
		return -EIO;
	}
	if (NULL == (buf = (char *) hmalloc_kernel (ware->firmware.len))) {
		ERROR("Error: Could not allocate fimrware buffer.\n");
		return -EIO;
	}
	if (ware->firmware.user) {
		if (copy_from_user (buf, ware->firmware.data, ware->firmware.len)) {
			ERROR("Error copying firmware data!\n");
			hfree (buf);
			return -EIO;
		}
	} else {
		lib_memcpy (buf, ware->firmware.data, ware->firmware.len);
	}
	
	assert (PEEK (cp->io_base + DRAM_BASE) == cp->addr_sdram);
	assert (PEEK (cp->io_base + MMIO_BASE) == cp->addr_mmio);

	utx = urx = 0;
#if !defined (LOG_LINK)
	if (IMAGE_OK != relocate_and_load (cp, buf, ware->firmware.len, &utx, &urx)) {
#else
	if (IMAGE_OK != relocate_and_load (cp, buf, ware->firmware.len, &utx, &urx, &ustart, &utrace)) {
#endif
		ERROR("Error: Firmware could not be relocated.\n");
		hfree (buf);
		return -EIO;
	}
	hfree (buf);
	mdelay (200);
	
	POKE (cp->io_base + CONFIG_DATA, 0);
	assert (PEEK (cp->io_base + CONFIG_DATA) == 0);

#if !defined (NDEBUG)
	biu_ctl = PEEK (cp->io_base + BIU_CTL);
	LOG("BIU_CTL = 0x%08x\n", biu_ctl);
#endif
	assert (PEEK (cp->io_base + MMIO_BASE) == cp->addr_mmio);
	start_ware (cp);
	assert (PEEK (cp->io_base + MMIO_BASE) == cp->addr_mmio);
	
	LOG("Waiting for firmware...\n");
	for (count = 0; count < FW_TIMEOUT; count++) {
		val = PEEK (cp->io_base + CONFIG_DATA);
		if (0x12345678 == val) {
			break;
		}
		mdelay (100); 
	}
	if (FW_TIMEOUT == count) {
		ERROR("Error: Firmware timeout elapsed without response!\n");
		return -EIO;
	}
	LOG("Firmware response time: %u ms.\n", 100 * count);
	POKE (cp->io_base + CONFIG_DATA, 0);
	assert (tx_info == NULL);
	assert (rx_info == NULL);
	tx_info = (xfer_base_p) deref_map_addr_from_tm_ptr (cp, utx);
	rx_info = (xfer_base_p) deref_map_addr_from_tm_ptr (cp, urx);
#if !defined (NDEBUG) && defined (LOG_LINK)
	dbg_setup (cp, ustart, utrace);
	dbg_start ();
#endif
	start (cp);
	if (ctrl_context[0].ctrl != NULL) {
		capi_ctr_ready (capi_controller[0]);
	}
	capi_ctr_ready (capi_controller[1]);
	return 0;
} /* load_ware */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static void __kcapi reset_ctrl (struct capi_ctr * ctrl) {
	card_p		cp;
	appl_t *	appp;
	
	assert (ctrl != NULL);
	assert (ctrl->driverdata != NULL);
	cp = GET_CARD (ctrl);
	info (!cp->running);
	if (!cp->running) {
		NOTE("Controller is already resetted!\n");
		return;
	}
	if (0 != cp->count) {
		NOTE("Removing registered applications!\n");
		info (cp->appls);
		if (cp->appls != NULL) {
			appp = first_appl (cp->appls);
			while (appp != NULL) {
				free_ncci (appp->id, (unsigned) -1);
				appp = next_appl (cp->appls, appp);
			}
		}
	}
	stop (cp);
#if !defined (NDEBUG) && defined (LOG_LINK)
	dbg_stop ();
#endif
	reset_card (cp); 
	if (ctrl_context[0].ctrl != NULL) {
		capi_ctr_reseted (capi_controller[0]);
	}
	capi_ctr_reseted (capi_controller[1]);
} /* reset_ctrl */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static char * __kcapi proc_info (struct capi_ctr * ctrl) {
	card_p		cp;
	per_ctrl_p	C;
	static char	text[80];

	assert (ctrl != NULL);
	assert (ctrl->driverdata != NULL);
	cp = GET_CARD (ctrl);
	C  = GET_CTRL (ctrl);
	if (cp->running) {
		snprintf (
			text, 
			sizeof (text),
			"%s %s io %08lx mem %08lx %u",
			C->version ? C->string[1] : "A1",
			C->version ? C->string[0] : "-",
			cp->addr_mmio, cp->addr_sdram, cp->irq
		);
	} else {
		snprintf (
			text,
			sizeof (text),
			TARGET " device io %08lx mem %08lx irq %u",
			cp->addr_mmio, cp->addr_sdram, cp->irq
		);
	}
	return text;
} /* proc_info */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static int __kcapi ctr_info (
	char *			page, 
	char **			start, 
	off_t			ofs,
	int			count, 
	int *			eof,
	struct capi_ctr *	ctrl
) {
	card_p			cp;
	per_ctrl_p		C;
	char *			temp;
	unsigned char		flag;
	int			len = 0;

	assert (ctrl != NULL);
	assert (ctrl->driverdata != NULL);
	cp = GET_CARD (ctrl);
	C  = GET_CTRL (ctrl);
	pprintf (page, &len, "%-16s %s\n", "name", SHORT_LOGO);
	pprintf (page, &len, "%-16s %d\n", "irq", cp->irq);
	pprintf (page, &len, "%-16s %08x\n", "iobase", cp->io_base);
	pprintf (page, &len, "%-16s %08x\n", "membase", cp->mem_base);

	if (cp->running) {
		temp = C->version ? C->string[1] : "A1";
		pprintf (page, &len, "%-16s %s\n", "type", temp);

		temp = C->version ? C->string[0] : "-";
		pprintf (page, &len, "%-16s %s\n", "ver_driver", temp);
		pprintf (page, &len, "%-16s %s\n", "ver_cardtype", SHORT_LOGO);

		flag = ((unsigned char *) (ctrl->profile.manu))[3];
		if (flag) {
			pprintf(page, &len, "%-16s%s%s%s%s%s%s%s\n", "protocol",
				(flag & 0x01) ? " DSS1" : "",
				(flag & 0x02) ? " CT1" : "",
				(flag & 0x04) ? " VN3" : "",
				(flag & 0x08) ? " NI1" : "",
				(flag & 0x10) ? " AUSTEL" : "",
				(flag & 0x20) ? " ESS" : "",
				(flag & 0x40) ? " 1TR6" : ""
			);
		}
		flag = ((unsigned char *) (ctrl->profile.manu))[5];
		if (flag) {
			pprintf(page, &len, "%-16s%s%s%s%s\n", "linetype",
				(flag & 0x01) ? " point to point" : "",
				(flag & 0x02) ? " point to multipoint" : "",
				(flag & 0x08) ? " leased line without D-channel" : "",
				(flag & 0x04) ? " leased line with D-channel" : ""
			);
		}
	}

	if (len < ofs) {
		return 0;
	}
	*eof = 1;
	*start = page - ofs;
	return ((count < len - ofs) ? count : len - ofs);
} /* ctr_info */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static void init_ctrl (void) {

	capi_ctrl_ix = 0;
	ctrl_context[0].card = ctrl_context[1].card = NULL;
	ctrl_context[0].ctrl = ctrl_context[1].ctrl = NULL;
	capi_controller[0]   = capi_controller[1]   = NULL;
} /* init_ctrl */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static int setup_ctrl (card_p cp, int ix) {
	int			cix, res;
	struct capi_ctr *	cptr;
	
	assert ((0 < ix) && (ix < 3));
	assert (capi_ctrl_ix < 2);

	cptr = &capi_ctrl[capi_ctrl_ix++];
	cptr->driver_name	= TARGET;
	cptr->driverdata	= cp;
	cptr->owner		= THIS_MODULE;
	cptr->load_firmware	= load_ware;
	cptr->reset_ctr		= reset_ctrl;
	cptr->register_appl	= register_appl;
	cptr->release_appl	= release_appl;
	cptr->send_message	= send_msg;
	cptr->procinfo		= proc_info;
	cptr->ctr_read_proc	= ctr_info;
	snprintf (cptr->name, 32, "%s-%08lx-%02u", TARGET, cp->addr_mmio, cp->irq);

	if (0 != (res = attach_capi_ctr (cptr))) {
		--capi_ctrl_ix;
		free_resources (cp);
		ERROR("Error: Could not attach controller %d.\n", ix);
		return -EBUSY;
	}
	if ((1 == ix) && (cptr->cnr != 1)) {
		cix = ix;
	} else {
		cix = ix - 1;
	}
	LOG("Controller %d (%d) --> %d\n", ix, cptr->cnr, cix);
	capi_controller[cix] = cptr;
	ctrl_context[cix].card = cp;
	ctrl_context[cix].ctrl = &ctrl_params[cix];
	capi_controller[cix]->driverdata = (void *) &ctrl_context[cix];
	return 0;
} /* setup_ctrl */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
int __kcapi add_card (struct pci_dev * dev) {
	card_p  cp;
	int	res = 0;
	char *	msg = "";

	if (NULL != capi_card) {
		ERROR("Cannot handle two controllers!\n");
 		return -EBUSY;
	}
	if (NULL == (cp = (card_p) hcalloc (sizeof (card_t)))) {
		ERROR("Card object allocation failed.\n");
		return -EIO;
	}
	capi_card = cp;
	init_waitqueue_head (&wait);

	switch (prepare_card (cp, dev)) {
        
	case PCI_OK:
		break;
	case PCI_NO_RESOURCE:
		msg = "Could not claim required resources.";
		res = -EBUSY;
		break;
        case PCI_NO_PCI_KERN:
		msg = "No PCI kernel support available.";
		res = -ESRCH;
		break;
        case PCI_NO_CARD:
		msg = "Could not locate any " PRODUCT_LOGO ".";
		res = -ESRCH;
		break;
        case PCI_NO_PCI:
		msg = "No PCI busses available.";
		res = -ESRCH;
		break;
        default:
		msg = "Unknown PCI related problem...";
		res = -EINVAL;
		break;
	}
	if (res != 0) {
	msg_exit:
		ERROR("Error: %s\n", msg);
		hfree (cp);
		capi_card = NULL;
		return res;
	}
	if (!params_ok (cp)) {
		free_resources (cp);
		msg = "Invalid parameters!";
		res = -EINVAL;
		goto msg_exit;
	}
	
	inc_use_count ();
	init_ctrl ();
	if (0 != (res = setup_ctrl (cp, 1))) {
		dec_use_count ();
		return res;
	}
	if (ctrl_context[0].ctrl != NULL) {
		if (0 != (res = setup_ctrl (cp, 2))) {
			detach_capi_ctr (capi_controller[0]);
			capi_controller[0] = NULL;
			dec_use_count ();
			return res;
		}
	}
	cp->ctrl2 = capi_controller[1]->cnr;
	cp->ctxp  = NULL;
	cp->rx_buffer = cp->tx_buffer = NULL;
	cp->rx_func   = cp->tx_func   = NULL;
	return 0;
} /* add_card */ 

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
void __kcapi remove_ctrls (card_p cp) {

	info (cp->running == FALSE);
	if (cp->running) {
		ERROR("Remove controller w/o reset!\n");
		reset_ctrl (capi_controller[1]);
	}
	free_resources (cp);
	if (ctrl_context[0].ctrl != NULL) {
		detach_capi_ctr (capi_controller[0]);
	}
	detach_capi_ctr (capi_controller[1]);
	dec_use_count ();
	init_ctrl ();
	hfree (capi_card);
	capi_card = NULL;
} /* remove_ctrl */

/*---------------------------------------------------------------------------*\
\*-C-------------------------------------------------------------------------*/
static inline int in_critical (void) {
	
	return (0 < atomic_read (&crit_count));
} /* in_critical */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static inline void check (void) {

	if (atomic_xchg (&rx_flag, 0)) {
		rx_handler (capi_card);
	}
	if (atomic_xchg (&tx_flag, 0)) {
		tx_handler (capi_card);
	}
} /* check */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
void enter_critical (void) {
	
	atomic_inc (&crit_count);
	if ((1 == atomic_read (&crit_count)) && !in_interrupt ()) {
		check ();
	}
} /* enter_critical */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
void leave_critical (void) {
	
	assert (in_critical ());
	if ((1 == atomic_read (&crit_count)) && !in_interrupt ()) {
		check ();
	}
	atomic_dec (&crit_count);
} /* leave_critical */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
int __stack msg2stack (unsigned char * msg) {
	unsigned		mlen, appl, hand, nctr, temp;
	__u32			ncci;
	unsigned char *		mptr;
	unsigned char *		dptr;
	struct sk_buff *	skb;

	assert (capi_card != NULL);
	assert (msg != NULL);
	if (NULL != (skb = queue_get (capi_card->queue))) {
		mptr = (unsigned char *) skb->data;
		mlen = CAPIMSG_LEN(mptr); 
		appl = CAPIMSG_APPID(mptr);
		nctr = CAPIMSG_CONTROLLER(mptr);
		
		MLOG(
			"PUT_MESSAGE(ctrl:%d,appl:%u,cmd:%X,subcmd:%X)\n", 
			nctr,
			appl, 
			CAPIMSG_COMMAND(mptr),
			CAPIMSG_SUBCOMMAND(mptr)
		);

		if (CAPIMSG_CMD(mptr) == CAPI_DATA_B3_REQ) {
			hand = CAPIMSG_U16(mptr, 18);
			ncci = CAPIMSG_NCCI(mptr);
			temp = (unsigned) (mptr + mlen);
			dptr = mptr + 12;
			*dptr++ = temp & 0xFF;	temp >>= 8;
			*dptr++ = temp & 0xFF;	temp >>= 8;
			*dptr++ = temp & 0xFF;	temp >>= 8;
			*dptr++ = temp & 0xFF;
			lib_memcpy (msg, mptr, mlen);
			queue_park (capi_card->queue, skb, appl, ncci, hand);
		} else {
			lib_memcpy (msg, mptr, mlen);
			assert (skb->list == NULL);
			kfree_skb (skb);
		}
	}
	return (skb != NULL);
} /* msg2stack */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
void __stack msg2capi (unsigned char * msg) {
	unsigned		mlen, appl, dlen, nctr, temp; 
	__u32			dummy;
	unsigned char *		dptr;
	struct sk_buff *	skb;
	struct capi_ctr *	ctrl;

	assert (capi_card != NULL);
	assert (msg != NULL);
	mlen = CAPIMSG_LEN(msg);
	appl = CAPIMSG_APPID(msg);
	nctr = CAPIMSG_CONTROLLER(msg);
		
	MLOG(
		"GET_MESSAGE(ctrl:%u,appl:%d,cmd:%X,subcmd:%X)\n", 
		nctr,
		appl, 
		CAPIMSG_COMMAND(msg),
		CAPIMSG_SUBCOMMAND(msg)
	);
	
	if (CAPIMSG_CMD(msg) == CAPI_DATA_B3_CONF) {
		handle_data_conf (capi_card->appls, capi_card->queue, msg);
	}
	if (!appl_alive (capi_card->appls, appl)) {
		LOG("Message to dying appl #%u!\n", appl);
		return;
	}
	if (CAPIMSG_CMD(msg) == CAPI_DATA_B3_IND) {
		dlen = CAPIMSG_DATALEN(msg);
		skb = alloc_skb (
			mlen + dlen + ((mlen < 30) ? (30 - mlen) : 0), 
			GFP_ATOMIC
		); 
		if (NULL == skb) {
			ERROR("Unable to allocate skb. Message lost.\n");
			return;
		}
		/* Messages are expected to come with 32 bit data pointers. 
		 * The kernel CAPI works with extended (64 bit ready) message 
		 * formats so that the incoming message needs to be fixed, 
		 * i.e. the length gets adjusted and the required 64 bit data 
		 * pointer is added.
		 */
		temp = CAPIMSG_U32(msg, 12);
		dptr = (unsigned char *) temp;
		if (mlen < 30) {
			msg[0] = 30;
			dummy  = 0;	
			assert (sizeof (dummy) == 4);
			lib_memcpy (skb_put (skb, mlen), msg, mlen);
			lib_memcpy (skb_put (skb, 4), &dummy, 4);	
			lib_memcpy (skb_put (skb, 4), &dummy, 4);
		} else {
			lib_memcpy (skb_put (skb, mlen), msg, mlen);
        	}
		lib_memcpy (skb_put (skb, dlen), dptr, dlen); 
	} else {
		if (NULL == (skb = alloc_skb (mlen, GFP_ATOMIC))) {
			ERROR("Unable to allocate skb. Message lost.\n");
			return;
		}
		lib_memcpy (skb_put (skb, mlen), msg, mlen);
	}
	if (NULL == (ctrl = get_capi_ctr (nctr))) {
		ctrl = (capi_controller[0] != NULL)
			? capi_controller[0]
			: capi_controller[1];
	}
	assert (ctrl != NULL);
	capi_ctr_handle_message (ctrl, appl, skb);
} /* msg2capi */

/*-S-------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static __attr void __stack scheduler_control (unsigned ena) {
	int	enabled = (int) ena;
	int	changed;

	enter_critical ();
	changed = (atomic_read (&scheduler_enabled) != enabled);
	leave_critical ();
	if (changed) {
		atomic_set (&scheduler_enabled, enabled ? 1 : 0);
		SCHED_WAKEUP;
	}
} /* scheduler_control */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/ 
static int sched_thread (void * arg) {

	UNUSED_ARG (arg);
	daemonize (TARGET);
	snprintf (current->comm, 16, "%s_s", TARGET);
	LOG("Starting scheduler thread '%s'...\n", current->comm);
	while (atomic_read (&thread_flag)) {
		
		/* Start/stop logic */
		if (!atomic_read (&thread_capi_flag)) {
			wait_event_interruptible (capi_wait, atomic_read (&thread_capi_flag));
		} else {
			if (atomic_read (&scheduler_enabled)) {
				wait_event_interruptible_timeout (wait, atomic_read (&got_kicked), TEN_MSECS);
			} else {
				wait_event_interruptible (wait, atomic_read (&got_kicked));
			}
			atomic_set (&got_kicked, 0);
		}
		
		/* Enable/disable logic */
		if (!atomic_read (&thread_flag)) {
			info (atomic_read (&thread_flag));
			break;
		}
		if (!atomic_read (&thread_capi_flag)) {
			info (atomic_read (&thread_capi_flag));
			continue;
		}
                if (!atomic_read (&scheduler_enabled)) {
			continue;
		}

		/* Body of thread, invoke scheduler */
		if (spin_trylock (&stack_lock)) {
			os_timer_poll ();
			assert (capi_lib->cm_schedule);
			if ((*capi_lib->cm_schedule) ()) {
				scheduler_control (TRUE); 
			}
			spin_unlock (&stack_lock);
		}
	}
	LOG("Scheduler thread stopped.\n");
	complete(&thread_sync); /* Complete Thread Sync here <arnd.feldmueller@web.de> */
	#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24))
	up (&hotplug);
	#endif
	return 0;
} /* sched_thread */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static int make_thread (void) {

	if (-1 != thread_pid) {
		LOG("Thread[%d] already running.\n", thread_pid);
		return thread_pid;
	}
	atomic_set (&thread_flag, 1);
	atomic_set (&thread_capi_flag, 0);
	init_waitqueue_head (&capi_wait);
	thread_pid = kernel_thread (sched_thread, NULL, 0);
	LOG("Thread[%d] started.\n", thread_pid);
	return thread_pid;
} /* make_thread */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static void kill_thread (void) {

	atomic_set (&thread_flag, 0);
	if (NULL == find_task_by_pid (thread_pid)) {
		LOG("Thread[%d] has died before!\n", thread_pid);
	} else {
		if (!atomic_read (&thread_capi_flag)) {
			SCHED_WAKEUP_CAPI;
		} else {
			SCHED_WAKEUP;
		}
		LOG("Thread signalled, waiting for termination...\n");
		wait_for_completion(&thread_sync); /* Wait for complete Thread Sync <arnd.feldmueller@web.de> */
		#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24))
		down (&hotplug);
		#endif
		LOG("Thread[%d] terminated.\n", thread_pid);
	}
	thread_pid = -1;
} /* kill_thread */
/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static void enable_thread (void) {

	info (!atomic_read (&thread_capi_flag));
	if (!atomic_read (&thread_capi_flag)) {
		SCHED_WAKEUP_CAPI;
		LOG("Thread enabled.\n");
	}
} /* enable_thread */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static void disable_thread (void) {

	info (atomic_read (&thread_capi_flag));
	if (atomic_read (&thread_capi_flag)) {
		atomic_set (&thread_capi_flag, 0);
		SCHED_WAKEUP;
		LOG("Thread disabled.\n");
	}
} /* disable_thread */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static void tx_handler (card_p cp) {
	int			is_open;
	completer_func_t	fp;
	void *			bufp;
	int			pending;
#if !defined (NDEBUG)
	xfer_info_t		info;
#endif
	
	atomic_set (&tx_flag, 0);
	is_open = atomic_read (&link_open);
	info (is_open);
	if (!is_open) {
		return;
	}
	pending = atomic_xchg (&tx_pending, 1);
	assert (pending == 0);
	assert (cp != NULL);
#if !defined (NDEBUG)
	assert (PEEK (cp->io_base + XFER_TOTM_STATUS) == PC_PENDING);
	xfer_info_get (tx_info, 0, &info);
	assert (info.state == TM_READY);
#endif
	assert (cp->tx_func != NULL);
	assert (cp->tx_buffer != NULL);
	fp = cp->tx_func;
	bufp = cp->tx_buffer;
	if (NULL != fp) {
		cp->tx_func = NULL;
		cp->tx_buffer = NULL;
		(*fp) (bufp, cp->tx_length, cp->tx_ctx);
		kick_scheduler ();
	}
	atomic_set (&tx_pending, 0);
} /* tx_handler */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static void tx_task (unsigned long data) {
	
	UNUSED_ARG (data);
	if (in_critical ()) {
		atomic_set (&tx_flag, 1);
		kick_scheduler ();
	} else if (spin_trylock (&stack_lock)) {
		tx_handler (capi_card);
		spin_unlock (&stack_lock);
	} else {
		atomic_set (&tx_flag, 1);
		kick_scheduler ();
	}
} /* tx_task */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static void rx_handler (card_p cp) {
	int			is_open;
	completer_func_t	fp;
	void *			bufp;
	xfer_info_t		info;
	int			pending;
#if defined (TM_TIMESTAMP)
	unsigned		deltaT;
#endif
	
	atomic_set (&rx_flag, 0);
	is_open = atomic_read (&link_open);
	info (is_open);
	if (!is_open) {
		return;
	}
	pending = atomic_xchg (&rx_pending, 1);
	assert (pending == 0);
	assert (cp != NULL);
	assert (PEEK (cp->io_base + XFER_TOPC_STATUS) == PC_PENDING);
	xfer_info_get (rx_info, 0, &info);
	assert (info.state == TM_READY);
	assert (((info.length + 3) & ~3) <= DMA_BUFFER_SIZE);
	assert (info.length < cp->rx_length);
	assert (cp->rx_length > 0);
	assert (cp->rx_func != NULL);
	assert (cp->rx_buffer != NULL);
	fp = cp->rx_func;
	bufp = cp->rx_buffer;
	lib_memcpy (cp->rx_buffer, cp->rx_dmabuf, info.length);
	if (NULL != fp) {
		cp->rx_func = NULL;
		cp->rx_buffer = NULL;
#if defined (TM_TIMESTAMP)
		if ((deltaT = timer_status (info.stamp, get_tm_timer ())) > RX_TIMER) {
			LOG("RX timer violation [%uµs]: %u\n", RX_TIMER, deltaT);
		}
#endif
		(*fp) (bufp, info.length, cp->rx_ctx);
		kick_scheduler ();
	}
	atomic_set (&rx_pending, 0);
} /* rx_handler */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static void rx_task (unsigned long data) {
	
	UNUSED_ARG (data);
	if (in_critical ()) {
		atomic_set (&rx_flag, 1);
		kick_scheduler ();
	} else if (spin_trylock (&stack_lock)) {
		rx_handler (capi_card);
		spin_unlock (&stack_lock);
	} else {
		atomic_set (&rx_flag, 1);
		kick_scheduler ();
	}
} /* rx_task */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static irqreturn_t irq_handler (int irq, void * args) {
	unsigned long	flags;
	int		tx_flag	= 0;
	int		rx_flag	= 0;
	int		count	= 0;
	card_p		cp;
	
	UNUSED_ARG (irq);
	if (capi_card != (card_p) args) {
		return IRQ_NONE;
	}
	cp = (card_p) args;
	while (0 != ((flags = PEEK (cp->io_base + INT_CTL)) & CARD_PCI_INT_ASSERT)) {
		++count;
		assert (count < 3);
		assert (0 != (flags & CARD_PCI_INT_ENABLE));
		assert (0 != (flags & CARD_PCI_INT_ISASSERTED));
		flags &= ~(CARD_PCI_INT_ASSERT | CARD_PCI_INT_ISASSERTED);
		POKE (cp->io_base + INT_CTL, flags);
		assert ((PEEK (cp->io_base + INT_CTL) 
				& ~(CARD_PCI_INT_ASSERT | CARD_PCI_INT_ISASSERTED)) != 0);
		if (!atomic_read (&link_open)) {
			return IRQ_HANDLED;
		}
		tx_flag = PEEK (cp->io_base + XFER_TOTM_STATUS) == TM_READY;
		rx_flag = PEEK (cp->io_base + XFER_TOPC_STATUS) == TM_READY;
		if (tx_flag) {
			POKE (cp->io_base + XFER_TOTM_STATUS, PC_PENDING); 
			tasklet_schedule (&tx_tasklet);
		}
		if (rx_flag) {
			POKE (cp->io_base + XFER_TOPC_STATUS, PC_PENDING);
			tasklet_schedule (&rx_tasklet);
		}
	}
	info (0 == (PEEK (cp->io_base + INT_CTL) & CARD_PCI_INT_ASSERT));
	return IRQ_RETVAL(count > 0);
} /* irq_handler */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/ 
static __attr void __stack wakeup_control (unsigned enable_flag) { 
	
	enable_flag = 0; 
} /* wakeup_control */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/ 
static __attr void __stack version_callback (char * vp) { 
	
	assert (nvers < 2);
	if ((0 == nvers) && (NULL == ctrl_context[0].ctrl)) {
		++nvers;
		LOG("Version string #0 rejected.\n");
		return;
	}
	LOG("Version string #%d:\n", nvers);
	scan_version (capi_controller[nvers], vp);
	copy_version (capi_controller[nvers]);
	++nvers;
} /* version_callback */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/ 
static __attr unsigned __stack scheduler_suspend (unsigned long timeout) { 
	
	timeout = 0; 
	return 0; 
} /* scheduler_suspend */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/ 
static __attr unsigned __stack scheduler_resume (void) { 
	
	return 0; 
} /* scheduler_resume */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/ 
static __attr unsigned __stack controller_remove (void) { 

	assert (0);
	return 0; 
} /* controller_remove */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/ 
static __attr unsigned __stack controller_add (void) { 
	
	assert (0);
	return 0; 
} /* controller_add */
	
/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/ 
void kick_scheduler (void) {

	atomic_set (&scheduler_enabled, 1);
	SCHED_WAKEUP;
} /* kick_scheduler */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/ 
static int trigger_tm_interrupt (void) {
	unsigned  count = TRIGGER_INT_MAX_DELAY;
	unsigned  flag  = 0;
#if !defined (NDEBUG)
	unsigned  imask;

	imask = PEEK (capi_card->io_base + IMASK);
	assert (imask & CARD_TM_HOST_INTR_BIT);
#endif
	do {
		flag = PEEK (capi_card->io_base + IPENDING) & CARD_TM_HOST_INTR_BIT;
		if (0 == flag) {
			POKE (capi_card->io_base + IPENDING, CARD_TM_HOST_INTR_BIT);
			break;
		}
		udelay (1);
	} while (count--);
	info (flag == 0);
	return (0 == flag);
} /* trigger_tm_interrupt */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/ 
static int trigger_tx_interrupt (void) {
	
	assert (PEEK (capi_card->io_base + XFER_TOTM_STATUS) != TM_PENDING);
	POKE (capi_card->io_base + XFER_TOTM_STATUS, TM_PENDING);
	return trigger_tm_interrupt ();
} /* trigger_tx_interrupt */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/ 
static int trigger_rx_interrupt (void) {
	
	assert (PEEK (capi_card->io_base + XFER_TOPC_STATUS) != TM_PENDING);
	POKE (capi_card->io_base + XFER_TOPC_STATUS, TM_PENDING);
	return trigger_tm_interrupt ();
} /* trigger_rx_interrupt */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/ 
__attr int LinkOpen (void) {
	int	flag;
	
	flag = atomic_xchg (&link_open, 1);
	assert (0 == flag);
	return 1;
} /* LinkOpen */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/ 
__attr void LinkClose (void) {
	int	flag;
	
	flag = atomic_xchg (&link_open, 0);
	assert (1 == flag);
} /* LinkClose */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/ 
__attr int LinkTxMessage (
	void *			msg,
	xfer_len_t		len,
	completer_func_t	cfunc,
	void *			ctx
) {
	int			flag;
	
	assert (capi_card != NULL);
	assert (len < DMA_BUFFER_SIZE);
	assert (0 == (len & 3));
	flag = atomic_read (&link_open);
	info (flag);
	if (!flag) {
		return 0;
	}
	
	assert (msg != NULL);
	assert (len > 0);
	assert (cfunc != NULL);
	assert (capi_card->tx_buffer == NULL);
	assert (capi_card->tx_func   == NULL);

	capi_card->tx_buffer = msg;
	capi_card->tx_length = len;
	capi_card->tx_func   = cfunc;
	capi_card->tx_ctx    = ctx;
	lib_memcpy (capi_card->tx_dmabuf, msg, len);
	xfer_info_write (tx_info, 0, TM_PENDING, capi_card->tx_dmabuf_b, len);

	return trigger_tx_interrupt ();
} /* LinkTxMessage */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/ 
__attr int LinkRxMessage (
	void *			msg, 
	xfer_len_t		len,
	completer_func_t	cfunc, 
	void *			ctx
) {
	int			flag;

	assert (capi_card != NULL);
	assert (len < DMA_BUFFER_SIZE);
	flag = atomic_read (&link_open);
	info (flag);
	if (!flag) {
		return 0;
	}
	
	assert (msg != NULL);
	assert (len > 0);
	assert ((len & 3) == 0);
	assert (cfunc != NULL);
	assert (capi_card->rx_buffer == NULL);
	assert (capi_card->rx_func   == NULL);

	capi_card->rx_buffer = msg;
	capi_card->rx_length = len;
	capi_card->rx_func   = cfunc;
	capi_card->rx_ctx    = ctx;
	xfer_info_write (rx_info, 0, TM_PENDING, capi_card->rx_dmabuf_b, len);
	return trigger_rx_interrupt ();
} /* LinkRxMessage */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
void __stack init (unsigned len, void (__attr2 * reg) (void *, unsigned),
				 void (__attr2 * rel) (void *),
				 void (__attr2 * dwn) (void))
{
	assert (reg != NULL);
	assert (rel != NULL);
	assert (dwn != NULL);

	capi_card->length   = len;
	capi_card->reg_func = reg;
	capi_card->rel_func = rel;
	capi_card->dwn_func = dwn;
} /* init */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
int fritz_driver_init (void) {

	return (NULL != (capi_lib = link_library (&capi_card)));
} /* fritz_driver_init */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
void driver_exit (void) {

	assert (capi_lib != NULL);
	free_library ();
	capi_lib = NULL;
} /* driver_exit */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/

