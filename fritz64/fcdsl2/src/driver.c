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
#include <asm/bitops.h>
#include <asm/uaccess.h>
#include <asm/system.h>
#include <linux/version.h>
#include <linux/ioport.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/skbuff.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/ctype.h>
#include <linux/string.h>
#include <linux/capi.h>
#include <linux/kernelcapi.h>
#include <linux/isdn/capicmd.h>
#include <linux/isdn/capiutil.h>
#include <linux/isdn/capilli.h>
#include <stdarg.h>
#include "libstub.h"
#include "tables.h"
#include "queue.h"
#include "lib.h"
#include "main.h"
#include "lock.h"
#include "tools.h"
#include "defs.h"
#include "fw.h"
#include "common.h"
#include "devif.h"
#include "driver.h"

#define	KILOBYTE		1024
#define	MEGABYTE		(1024*KILOBYTE)
#define	_2_MEGS_		(2*MEGABYTE)
#define	_4_MEGS_		(4*MEGABYTE)
#define	_8_MEGS_		(8*MEGABYTE)
#define	TEN_MSECS		(HZ/100)

#define	MAPPED_DATA_LEN		_4_MEGS_
#define	MAPPED_CODE_LEN		_8_MEGS_
#define	IO_REGION_LEN		16
#define	DEBUG_MEM_SIZE		MEGABYTE
#define	DEBUG_TIMEOUT		100
#define	RESET_DELAY		10

#define	PPRINTF_LEN		256

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
#define	DBG_FORMAT_LEN		1024
#define	DBG_MEM_SIZE		(1024 * 1024)
#define	DBG_TIMEOUT		100
#define	DBG_MIN_SIZE		1024

typedef struct __db {

	dma_struct_p		pdma;
	atomic_t		stop;
	unsigned		len;
	unsigned		tout;
	char *			pbuf;
	int			pid;
} dbg_buf_t, * dbg_buf_p;

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
card_p				capi_card		= NULL;
lib_callback_t *		capi_lib		= NULL;
atomic_t			crit_count		= ATOMIC_INIT(0);
volatile unsigned long		stack_lock		= 0;

static int			nvers			= 0;
static atomic_t			scheduler_enabled	= ATOMIC_INIT (1);
static atomic_t			got_kicked		= ATOMIC_INIT (0);
static unsigned			capi_ctrl_ix		= 0;
struct capi_ctr *		capi_controller[2]	= { NULL, NULL } ;
static struct capi_ctr		capi_ctrl[2];
static bundle_t			ctrl_context[2];
static per_ctrl_t		ctrl_params[2];
static atomic_t			thread_flag		= ATOMIC_INIT (0);
static atomic_t			thread_capi_flag	= ATOMIC_INIT (0);
static int			thread_pid		= -1;
static char *			firmware_ptr		= NULL;
static unsigned			firmware_len		= 0;
static unsigned			pc_ack_offset		= 0;
static unsigned			timer_offset		= 0;
static dbg_buf_p		dbgbuf			= NULL;

static DECLARE_WAIT_QUEUE_HEAD(wait);
static DECLARE_WAIT_QUEUE_HEAD(capi_wait);
static DECLARE_WAIT_QUEUE_HEAD(dbg_wait);

static DECLARE_COMPLETION(thread_sync); /* New DECLARE, <arnd.feldmueller@web.de> */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24))
static DECLARE_MUTEX_LOCKED(thread_sync);
#endif

#define SCHED_WAKEUP_CAPI       { atomic_set (&thread_capi_flag, 1); wake_up_interruptible (&capi_wait); }
#define SCHED_WAKEUP            { atomic_set (&got_kicked, 1); wake_up_interruptible (&wait); }

static int			irq_callback (void *);
static void			enable_thread (void);
static int			make_thread (void);
static void			kill_thread (void);
static void			reset_ctrl (struct capi_ctr *);
static void			kill_version (struct capi_ctr *);

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static __attr void		scheduler_control (unsigned);
static __attr void		wakeup_control (unsigned);
static __attr void		version_callback (char *);
static __attr unsigned		scheduler_suspend (unsigned long);
static __attr unsigned		scheduler_resume (void);
static __attr unsigned		controller_remove (void);
static __attr unsigned		controller_add (void);

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
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
#ifdef __LP64__
# define _CAPIMSG_U64(m, off)	\
	 ((u64)m[off]|((u64)m[(off)+1]<<8)|((u64)m[(off)+2]<<16)|((u64)m[(off)+3]<<24) \
	 |((u64)m[(off)+4]<<32)|((u64)m[(off)+5]<<40)|((u64)m[(off)+6]<<48)|((u64)m[(off)+7]<<56))

static void _capimsg_setu64(void *m, int off, __u64 val)
{
	((__u8 *)m)[off] = val & 0xff;
	((__u8 *)m)[off+1] = (val >> 8) & 0xff;
	((__u8 *)m)[off+2] = (val >> 16) & 0xff;
	((__u8 *)m)[off+3] = (val >> 24) & 0xff;
	((__u8 *)m)[off+4] = (val >> 32) & 0xff;
	((__u8 *)m)[off+5] = (val >> 40) & 0xff;
	((__u8 *)m)[off+6] = (val >> 48) & 0xff;
	((__u8 *)m)[off+7] = (val >> 56) & 0xff;
}
#endif

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static __attr uchar  minb (intptr_t addr) { return MINB(addr); }
static __attr ushort minw (intptr_t addr) { return MINW(addr); }
static __attr ulong  minl (intptr_t addr) { return MINL(addr); }
static __attr uchar  pinb (intptr_t addr) { return PINB(addr); }
static __attr ushort pinw (intptr_t addr) { return PINW(addr); }
static __attr ulong  pinl (intptr_t addr) { return PINL(addr); }

static __attr void moutb (uchar  val, intptr_t addr) { MOUTB(addr, val); }
static __attr void moutw (ushort val, intptr_t addr) { MOUTW(addr, val); }
static __attr void moutl (ulong  val, intptr_t addr) { MOUTL(addr, val); }
static __attr void poutb (uchar  val, intptr_t addr) { POUTB(addr, val); }
static __attr void poutw (ushort val, intptr_t addr) { POUTW(addr, val); }
static __attr void poutl (ulong  val, intptr_t addr) { POUTL(addr, val); }

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
#define	PCI_NO_RESOURCE		4
#define PCI_NO_PCI_KERN		3
#define PCI_NO_CARD		2
#define PCI_NO_PCI		1
#define PCI_OK			0

static int alloc_resources (struct pci_dev * dev, card_p cp) {

	UNUSED_ARG (dev);
	if (!request_region (cp->addr_mmio, IO_REGION_LEN, TARGET)) {
		ERROR("Could not claim i/o region.\n");
	io_problem:
		cp->mmio_base = 0;
		return PCI_NO_RESOURCE;
	}
	cp->mmio_base = cp->addr_mmio;
	cp->mmio_access_ok = TRUE;
	LOG("I/O region at 0x%04x\n", cp->mmio_base);
	if (!(cp->data_base = ioremap_nocache (cp->addr_data, cp->len_data))) {
		LOG("Could not map data memory into virtual memory.\n");
	data_map_exit:
		release_region (cp->addr_mmio, IO_REGION_LEN);
		goto io_problem;
	}
	cp->data_access_ok = TRUE;
	LOG("Controller data memory mapped to %lx\n", cp->data_base);
	if (!(cp->code_base = ioremap_nocache (cp->addr_code, cp->len_code))) {
		LOG("Could not map code memory into virtual memory.\n");
	/* code_map_exit: */
		iounmap (cp->data_base);
		cp->data_base = NULL;
		goto data_map_exit;
	}
	cp->code_access_ok = TRUE;
	LOG("Controller code memory mapped to %lx\n", cp->code_base);
	pci_set_master (dev);
	return PCI_OK;
} /* alloc_resources */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static void free_interrupt (card_p cp) {

	LOG("Free IRQ...\n");
	assert (cp);
	if (cp->irq != 0) {
		free_irq (cp->irq, cp);
		cp->irq = 0;
		LOG("IRQ freed.\n");
	}
} /* free_interrupt */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static void free_resources (card_p cp) {

	assert (cp->mmio_base != 0);
	assert (cp->data_base != NULL);
	assert (cp->code_base != NULL);

	free_interrupt (cp);
	clear_interrupt_callback ();
	LOG("Unmapping code, data, i/o regions...\n");
	iounmap (cp->code_base);
	iounmap (cp->data_base);
	release_region (cp->addr_mmio, IO_REGION_LEN);
	info (cp->piowin != NULL);
	if (cp->piowin != NULL) {
		hfree (cp->piowin);
	}
	LOG("Resources freed.\n");
} /* free_resources */

/*---------------------------------------------------------------------------*\
\*-B-------------------------------------------------------------------------*/
#define	DBG_FORMAT_START	0xABCD1234
#define	DBG_FORMAT_STOP		0x4321DCBA

void dump (char * p1, char * p2, char * end, unsigned l1, unsigned l2) {

	p1 += sizeof (unsigned);
	p2 += sizeof (unsigned);
	memdump (p1, l1, p1 - p2, "");
	if (l2 != 0) {
		memdump (p2, l2, 0, "");
	}
} /* dump */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static void dump_debug_buffer (dbg_buf_p dbp) {
	static char *	bp = NULL;
	unsigned	part1_len, part2_len;
	unsigned	total_len, check_len;
	int		delta;
	
	assert (dbp != NULL);
	if ((dbp->pbuf == NULL) || atomic_read (&dbp->stop)) {
		LOG("No debug buffer???\n");
		return;
	}
	
	assert (dbp->pbuf != NULL);
	assert (dbp->pdma != NULL);
	assert (dbp->pbuf >= (char *) dbp->pdma->virt_addr);
	assert (dbp->pbuf < (char *) (dbp->pdma->virt_addr + dbp->len));
	assert ((((unsigned long) dbp->pbuf) & 3) == 0);
	if (bp == NULL) {
		bp = dbp->pbuf;
	}
	part1_len = part2_len = 0;
		
	assert (bp >= dbp->pbuf);
	assert (bp < (dbp->pbuf + dbp->len));
	total_len = * ((unsigned *) bp);
	info (total_len < dbp->len);
	if ((total_len > dbp->len) || ((total_len & 3) != 0)) {
		LOG("Debug thread needs debugging!\n");
		return;
	}

	delta = ((char *) bp + total_len) - (dbp->pbuf + dbp->len);
	if (delta > 0) {
		part2_len = (unsigned) delta;
		part1_len = total_len - part2_len;
	} else {
		part2_len = 0;
		part1_len = total_len;
	}
	assert ((part2_len & 3) == 0);
	assert ((part1_len & 3) == 0);
	assert (part2_len < dbp->len);

	if (part1_len != 0) {
		check_len = (part2_len == 0) 
			? * ((unsigned *) (bp + part1_len) + 1)
			: * ((unsigned *) (dbp->pbuf + part2_len) + 1);
		assert (check_len == total_len);
		
		dump (
			bp, 
			dbp->pbuf, 
			dbp->pbuf + dbp->len, 
			part1_len, 
			part2_len
		);

		if (part2_len == 0) {
			bp += 2 * sizeof (unsigned) + part1_len;
		} else {
			bp = dbp->pbuf + part2_len + sizeof (unsigned);
		}
	}
} /* dump_debug_buffer */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static int debug_thread (void * arg) {
	dbg_buf_p	dbp = (dbg_buf_p) arg;
	unsigned	to;

	assert (dbp != NULL);
	to = (((dbp->tout < 10) ? 10 : dbp->tout) / 10) * TEN_MSECS;
	while (!atomic_read (&dbp->stop)) {
		wait_event_interruptible_timeout (dbg_wait, atomic_read (&dbp->stop), to);
		dump_debug_buffer (dbp);
	}
	LOG("Debug thread stopped.\n");
	if (dbp->pdma != NULL) {
		dma_free_buffer (dbp->pdma);
		dma_free (&dbp->pdma);
	}
	hfree (dbp);
	return 0;
} /* debug_thread */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static int init_debug_buffer (dbg_buf_p dbp) {
	int		res	= 0;
	unsigned	size	= DBG_MEM_SIZE;
	dma_struct_p	pdma;
	
	assert (dbp != NULL);
	assert (size >= DBG_MIN_SIZE);
	assert (DBG_TIMEOUT > 0);
	assert (dbp->pbuf == NULL);
	pdma = dma_alloc ();
	res = FALSE;
	for ( ; size >= DBG_MIN_SIZE; size /= 2) {
		if ((res = dma_setup (pdma, size))) {
			break;
		}
		LOG("Failed to allocate %u byte DMA buffer...\n", size);
	}
	if (!res) {
		dma_exit (pdma);
		dma_free (&pdma);
		return FALSE;
	}
	LOG(
		"Debug DMA buffer, %u bytes, vaddr %p, paddr %p\n", 
		pdma->length, 
		(void *) pdma->virt_addr, 
		(void *) pdma->phys_addr
	);
	assert (size == pdma->length);
	dbp->pdma = pdma;
	dbp->pbuf = (void *) pdma->virt_addr;
	dbp->len  = size;
	dbp->tout = DBG_TIMEOUT;
	atomic_set (&dbp->stop, 0);
	dbp->pid = kernel_thread (debug_thread, dbp, 0);
	return TRUE;
} /* init_debug_buffer */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static void exit_debug_buffer (dbg_buf_p dbp) {
	
	assert (dbp != NULL);
	info (find_task_by_pid (dbp->pid) != NULL);
	LOG("Stopping debug thread...\n");
	atomic_set (&dbp->stop, 1);
	if (find_task_by_pid (dbp->pid)) {
		wake_up_interruptible (&dbg_wait);
	} else {
		hfree (dbp);
	}
} /* exit_debug_buffer */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static void reset (card_p cp) {
	int	done = 0;

	assert (cp != NULL);
	if (fw_ready ()) {
		assert (cp->c6205_ctx != NULL);
		LOG("Loadfile based reset.\n");
		done = fw_send_reset (cp->c6205_ctx);
	}
	if (!done) {
		assert (cp->mmio_base != 0);
		LOG("I/O based reset.\n");
		POUTL (
			cp->mmio_base + C6205_PCI_HDCR_OFFSET, 
			C6205_PCI_HDCR_WARMRESET
		);
	}
	mdelay (RESET_DELAY);
} /* reset */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static inline void kill_versions (void) {
	
	if (ctrl_context[0].ctrl != NULL) {
		kill_version (capi_controller[0]);
	}
	kill_version (capi_controller[1]);
} /* kill_versions */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
int start (card_p cp) {
	int		res = 0;
	unsigned	num = 0, n;
	unsigned	ofs, len, bof;
	ioaddr_p	pwin;
	dma_struct_p *	ppdma;
	
	/* preparing the stack */
	cp->count = 0;
	table_init (&cp->appls);
	queue_init (&cp->queue);

	assert (capi_lib != NULL);
	assert (capi_lib->cm_register_ca_functions != NULL);
	assert (capi_lib->cm_start != NULL);
	assert (capi_lib->cm_init != NULL);
	(*capi_lib->cm_register_ca_functions) (&ca_funcs);
	if ((*capi_lib->cm_start) ()) {
		ERROR("Starting the stack failed...\n");
	early_exit:
		queue_exit (&cp->queue);
		table_exit (&cp->appls);
		return FALSE;
	}
	(void) (*capi_lib->cm_init) (0, cp->irq);
	assert (nvers == 2);
	
	/* install an interrupt handler */
	set_interrupt_callback (&irq_callback, cp);
	res = request_irq (
		cp->irq, 
		&device_interrupt, 
		SA_INTERRUPT | SA_SHIRQ, 
		TARGET, 
		cp
	);
	info (res == 0);
	if (0 != res) {
		ERROR("Could not install irq handler.\n");
	stack_exit:
		assert (capi_lib->cm_exit != NULL);
		(*capi_lib->cm_exit) ();
		clear_interrupt_callback ();
		kill_versions ();
		goto early_exit;
	} else {
		LOG("IRQ #%d assigned to " TARGET " driver.\n", cp->irq);
	}

	/* debug buffer */
	assert (capi_lib->cc_debugging_needed != NULL);
	if ((*capi_lib->cc_debugging_needed) ()) {
		LOG("Setting up debug buffer.\n");
		if (dbgbuf == NULL) {
			if (NULL != (dbgbuf = (dbg_buf_p) hcalloc (sizeof (dbg_buf_t)))) {
				if (!init_debug_buffer (dbgbuf)) {
					hfree (dbgbuf);
					goto stack_exit;
				}
				assert (capi_lib->cc_init_debug != NULL);
				(*capi_lib->cc_init_debug) (
					(void *) dbgbuf->pdma->phys_addr, 
					dbgbuf->pdma->length
				);
			}
		}
	} else {
		LOG("No debug buffer.\n");
	}

	/* establish DMA buffers */
	ppdma = dma_get_struct_list (&num);
	assert (capi_lib->cc_num_link_buffer != NULL);
	assert (capi_lib->cc_init_dma != NULL);
	n = (*capi_lib->cc_num_link_buffer) (TRUE),
	(*capi_lib->cc_init_dma) (
		&ppdma[0], n,
		&ppdma[n], (*capi_lib->cc_num_link_buffer) (FALSE)
	);
	
	assert (capi_lib->cc_link_version != NULL);
	assert (capi_lib->cc_buffer_params != NULL);
	if ((*capi_lib->cc_link_version) () > 1) {
		num = (*capi_lib->cc_buffer_params) (&ofs, &pwin, &len, &bof);
		dif_set_params (num, ofs, pwin, len, bof);
	}
	reset (cp);
	assert (capi_lib->cc_run != NULL);
	res = (*capi_lib->cc_run) ();
	if (res == FALSE) {	/* Failure */
		ERROR("Firmware does not respond!\n");
	reset_exit:
		reset (cp);
		if (dbgbuf != NULL) {
			exit_debug_buffer (dbgbuf);
		}
		goto stack_exit;
	}
	assert (capi_lib->cc_compress_code != NULL);
	(*capi_lib->cc_compress_code) ();

	/* start the stack */
	enter_critical ();
	if ((*capi_lib->cm_activate) ()) {
		ERROR("Activation of the card failed.\n");
		leave_critical ();
		goto reset_exit;
	}
	leave_critical ();
	make_thread ();
	enable_thread ();
	return (cp->running = TRUE);
} /* start */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static void stop (card_p card) {

	if (!card->running) {
		LOG("Stack not initialized.\n");
		return;
	}
	if (dbgbuf != NULL) {
		exit_debug_buffer (dbgbuf);
		dbgbuf = NULL;
	}
	(*capi_lib->cm_exit) ();
	if (thread_pid != -1) {
		kill_thread ();
	}
	card->running = FALSE;

	queue_exit (&card->queue);
	table_exit (&card->appls);
	kill_versions ();
	nvers = 0;
} /* stop */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static int reset_card (card_p cp) {

	reset (cp);
	fw_exit (&cp->c6205_ctx);
	return TRUE;
} /* reset_card */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static int params_ok (card_p cp) {

	UNUSED_ARG (cp);
	return TRUE;
} /* params_ok */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static int prepare_card (card_p cp, struct pci_dev * dev) {

	assert (cp != NULL);
	assert (dev != NULL);
	assert (pci_resource_len (dev, 0) == MAPPED_DATA_LEN);
	assert (pci_resource_len (dev, 1) == MAPPED_CODE_LEN);
	assert (pci_resource_len (dev, 2) == IO_REGION_LEN);

	cp->addr_data = pci_resource_start (dev, 0);
	cp->len_data  = pci_resource_len (dev, 0);
	cp->addr_code = pci_resource_start (dev, 1);
	cp->len_code  = pci_resource_len (dev, 1);
	cp->addr_mmio = pci_resource_start (dev, 2);
	cp->len_mmio  = pci_resource_len (dev, 2);
	cp->irq       = dev->irq;

	LOG(
		"PCI: " PRODUCT_LOGO ", dev %04x, irq %d\n",
		dev->device,
		cp->irq
	);
	LOG(
		"     data %lx (%dM), code %lx (%dM), mmio %04x\n",
		cp->addr_data, cp->len_data / MEGABYTE,
		cp->addr_code, cp->len_code / MEGABYTE,
		cp->addr_mmio
	);

	return alloc_resources (dev, cp);
} /* prepare_card */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static struct capi_ctr * get_capi_ctr (int ctrl) {
	int	ix;
	
	assert (capi_card);
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

	assert (ctrl);
	assert (ctrl->driverdata);
	C = GET_CTRL (ctrl);

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
	NOTE("Stack version %s\n", C->string[0]);
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
	
	assert (ctrl);
	assert (ctrl->driverdata);
	C = GET_CTRL (ctrl);
	
	if (NULL == (tmp = C->string[3])) {
		ERROR("Do not have version information...\n");
		return;
	}
	lib_strncpy (ctrl->serial, tmp, CAPI_SERIAL_LEN);
#if !defined (DRIVER_TYPE_ISDN)
	if (ctrl == capi_controller[0]) {
		lib_memset (&ctrl->profile, 0, sizeof (capi_profile));
	} else {
		lib_memcpy (&ctrl->profile, C->string[6], sizeof (capi_profile));
	}
#else
	lib_memcpy (&ctrl->profile, C->string[6], sizeof (capi_profile));
#endif
	lib_strncpy (ctrl->manu, "AVM-GmbH", CAPI_MANUFACTURER_LEN);
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
	
	assert (ctrl);
	assert (ctrl->driverdata);
	C = GET_CTRL (ctrl);

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
__attr void stack_wait (unsigned msec) {

	LOG("Stack wait request for %u msecs\n", msec);
	assert (!in_interrupt ());
	mdelay (msec);
} /* stack_wait */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
__attr unsigned get_timer (void) {
	unsigned	t = 0;

	if (!capi_card->code_access_ok) {
		t = MINL (capi_card->code_base + timer_offset);
	}
	return t;
} /* get_timer */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
__attr void pc_acknowledge (unsigned char num) {
	
	assert (capi_lib != NULL);
	assert (capi_lib->cc_link_version != NULL);
	assert ((*capi_lib->cc_link_version) () > 3);
#if !defined (NDEBUG) && defined (LOG_LINK)
	LOG("ACK for buffer #%u\n", num);
#endif
	MOUTL (capi_card->data_base + pc_ack_offset, num);
	assert (MINL (capi_card->data_base + pc_ack_offset) == num);
} /* pc_acknowledge */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
int __kcapi load_ware (struct capi_ctr * ctrl, capiloaddata * ware) {
	card_p		cp;
	c6205_context	ctx;
	char *		buffer;
	int		res;
	ioaddr_p	io;

	assert (ctrl != NULL);
	assert (ware != NULL);
	cp = GET_CARD(ctrl);
	if (cp->running) {
		LOG("Firmware has already been loaded!\n");
		return 0;
	}
	assert (firmware_ptr == NULL);
	if (NULL == (buffer = (char *) hmalloc (ware->firmware.len))) {
		ERROR("Could not allocate firmware buffer.\n");
		return -EIO;
	}
        if (ware->firmware.user) {
		if (copy_from_user (buffer, ware->firmware.data, ware->firmware.len)) {
			ERROR("Error copying firmware data!\n");
			return -EIO;
		}
	} else {
		lib_memcpy (buffer, ware->firmware.data, ware->firmware.len);
	}
	LOG("Loaded %d bytes of firmware.\n", ware->firmware.len);
	firmware_ptr = buffer;
	firmware_len = ware->firmware.len;

	if (NULL == (io = (ioaddr_p) hmalloc (3 * sizeof (ioaddr_t)))) {
		LOG("Could not claim memory for i/o structures.\n");
		return -ENOMEM;
	}
	cp->piowin = io;
	
#define	INIT_IO(x,n,f)	x.virt_addr	= (intptr_t) cp->n##_base;	\
			x.phys_addr	= cp->addr_##n;			\
			x.length	= cp->len_##n;			\
			x.inb		= &(f##inb);			\
			x.inw		= &(f##inw);			\
			x.inl		= &(f##inl);			\
			x.outb		= &(f##outb);			\
			x.outw		= &(f##outw);			\
			x.outl		= &(f##outl);
			
	INIT_IO(io[0], data, m);
	INIT_IO(io[1], code, m);
	INIT_IO(io[2], mmio, p);
	
#undef INIT_IO
	
	ctx = fw_init (
		cp, 
		firmware_ptr, 
		firmware_len, 
		3, io, 
		&res
	);
	if ((NULL == ctx) || (res != 0)) {
		ERROR("Error while processing firmware (%d)!\n", res);
	error1:
		fw_exit (&ctx);
		hfree (firmware_ptr);
		hfree (cp->piowin);
		firmware_ptr = NULL;
		cp->piowin = NULL;
		return -EIO;
	}
	cp->c6205_ctx = ctx;
	assert (capi_lib != NULL);
	assert (capi_lib->cc_link_version != NULL);
	assert ((*capi_lib->cc_link_version) () > 1);
	if (!dif_init (
		cp, 
		(*capi_lib->cc_link_version) () > 1, 
		&get_timer, 
		&pc_acknowledge
	)) {
		ERROR("Error while setting up device structures!\n");
	error2:
		goto error1;
	}
	assert (capi_lib->cc_timer_offset != NULL);
	timer_offset = (*capi_lib->cc_timer_offset) ();
	if ((*capi_lib->cc_link_version) () > 3) {
		pc_ack_offset = (*capi_lib->cc_pc_ack_offset) ();
	}

	fw_setup (ctx);
	if (!start (cp)) {
		ERROR("Error while starting protocoll stack!\n");
	/* error3: */
		dif_exit ();
		goto error2;
	}
	if (ctrl_context[0].ctrl != NULL) {
		capi_ctr_ready (capi_controller[0]);
	}
	capi_ctr_ready (capi_controller[1]);
	return 0;
} /* load_ware */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
char * __kcapi proc_info (struct capi_ctr * ctrl) {
	card_p		cp;
	per_ctrl_p	C;
	static char	text[80];

	assert (ctrl);
	assert (ctrl->driverdata);
	cp = GET_CARD (ctrl);
	C  = GET_CTRL (ctrl);
	if (cp->running) {
		snprintf (
			text, 
			sizeof (text),
			"%s %s io %lx %u",
			C->version ? C->string[1] : "A1",
			C->version ? C->string[0] : "-",
			cp->addr_mmio,
			cp->irq
		);
	} else {
		snprintf (
			text,
			sizeof (text),
			TARGET " device io %lx irq %u",
			cp->addr_mmio,
			cp->irq
		);
	}
	return text;
} /* proc_info */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
int __kcapi ctr_info (
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

	assert (ctrl);
	assert (ctrl->driverdata);
	cp = GET_CARD (ctrl);
	C  = GET_CTRL (ctrl);
	pprintf (page, &len, "%-16s %s\n", "name", SHORT_LOGO);
	pprintf (page, &len, "%-16s %d\n", "irq", cp->irq);
	pprintf (page, &len, "%-16s %08lx\n", "data", cp->data_base);
	pprintf (page, &len, "%-16s %08lx\n", "code", cp->code_base);
	pprintf (page, &len, "%-16s %lx\n", "io", cp->mmio_base);

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
static void __kcapi reset_ctrl (struct capi_ctr * ctrl) {
	card_p		cp;
	appl_t *	appp;

	assert (ctrl != NULL);
	assert (ctrl->driverdata != NULL);
	cp = GET_CARD (ctrl);
	assert (cp != NULL);
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
	reset_card (cp);
	if (firmware_ptr != NULL) {
		hfree (firmware_ptr);
		firmware_ptr = NULL;
		firmware_len = 0;
	}
	dif_exit ();
	if (ctrl_context[0].ctrl != NULL) {
		capi_ctr_reseted (capi_controller[0]);
	}
	capi_ctr_reseted (capi_controller[1]);
} /* reset_ctrl */

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
	snprintf (cptr->name, 32, "%s-%lx-%u", TARGET, cp->addr_mmio, cp->irq);

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
	char *	msg;

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
#if !defined (DRIVER_TYPE_ISDN)
	if (0 != (res = setup_ctrl (cp, 2))) {
		dec_use_count ();
		return res;
	}
#else
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
#endif
	cp->ctrl2 = capi_controller[1]->cnr;
	cp->ctxp  = NULL;
	return res;
} /* add_card */ 

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
void __kcapi remove_ctrls (card_p cp) {

	info (!cp->running);
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
static inline void check (void) {
	unsigned long	flags;

	if (test_and_clear_bit (0, &xfer_flag)) {
		local_irq_save (flags);
		xfer_handler (capi_card);
		local_irq_restore (flags);
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
int msg2stack (unsigned char * msg) {
	unsigned		mlen, appl, hand, nctr;
	__u32			ncci;
	unsigned char *		mptr;
	struct sk_buff *	skb;
	
	assert (capi_card != NULL);
	assert (msg != NULL);
	if (NULL != (skb = queue_get (capi_card->queue))) {
		mptr = (unsigned char *) skb->data;
		mlen = CAPIMSG_LEN(mptr); 
		appl = CAPIMSG_APPID(mptr);
		nctr = CAPIMSG_CONTROLLER(mptr);
		
		MLOG(
			"PUT_MESSAGE(ctrl:%u,appl:%u,cmd:%X,subcmd:%X)\n", 
			nctr,
			appl, 
			CAPIMSG_COMMAND(mptr), 
			CAPIMSG_SUBCOMMAND(mptr)
		);

		if (CAPIMSG_CMD(mptr) == CAPI_DATA_B3_REQ) {
			hand = CAPIMSG_U16(mptr, 18);
			ncci = CAPIMSG_NCCI(mptr);
#ifdef __LP64__
			if (mlen < 30) {
				_capimsg_setu64(msg, 22, (__u64)(mptr + mlen));
				capimsg_setu16(mptr, 0, 30);
			}
			else {
				_capimsg_setu64(mptr, 22, (__u64)(mptr + mlen));
			}
#else
			capimsg_setu32(mptr, 12, (__u32)(mptr + mlen));
#endif
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
void msg2capi (unsigned char * msg) {
	unsigned		mlen, appl, dlen, nctr;
#ifndef __LP64__	
	__u32			dummy;
#endif
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
#ifdef __LP64__
		dptr = (unsigned char *) _CAPIMSG_U64(msg, 22);
		lib_memcpy (skb_put (skb, mlen), msg, mlen);
#else
		dptr = (unsigned char *) CAPIMSG_U32(msg, 12);
		if (mlen < 30) {
			msg[0] = 30;
			dummy  = 0;	
			lib_memcpy (skb_put (skb, mlen), msg, mlen);
			lib_memcpy (skb_put (skb, 4), &dummy, 4);	
			lib_memcpy (skb_put (skb, 4), &dummy, 4);
		} else {
			lib_memcpy (skb_put (skb, mlen), msg, mlen);
		}
#endif
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
static int sched_thread (void * arg) {

	UNUSED_ARG (arg);
	daemonize (TARGET);
	snprintf (current->comm, 16, "%s_s", TARGET);
	LOG("Starting kicker thread '%s'...\n", current->comm);
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
		assert (!in_critical ());
		
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
			info (atomic_read (&scheduler_enabled));
			continue;
		}

		/* Body of thread, invoke scheduler */
		if (!test_and_set_bit (0, &stack_lock)) {
			os_timer_poll ();
			assert (capi_lib->cm_schedule);
			if ((*capi_lib->cm_schedule) ()) {
				scheduler_control (TRUE); 
			}
			clear_bit (0, &stack_lock);
		}
	}
	LOG("Scheduler thread stopped.\n");
	complete(&thread_sync); /* Complete Thread Sync here <arnd.feldmueller@web.de> */
	#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24))
	up (&thread_sync);
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
		down (&thread_sync);
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
int irq_callback (void * arg) {
#if !defined (NDEBUG)
	card_p	cp = (card_p) arg;
#endif

	assert (cp == capi_card);
	kick_scheduler ();
	return TRUE;
} /* irq_callback */

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
static __attr unsigned	__stack scheduler_suspend (unsigned long timeout) { 
	
	timeout = 0; 
	return 0; 
} /* scheduler_suspend */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/ 
static __attr unsigned	__stack scheduler_resume (void) { 
	
	return 0; 
} /* scheduler_resume */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/ 
static __attr unsigned	__stack controller_remove (void) { 

	assert (0);
	return 0; 
} /* controller_remove */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/ 
static __attr unsigned	__stack controller_add (void) { 
	
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

	assert (capi_lib);
	free_library ();
	capi_lib = NULL;
} /* driver_exit */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/

