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
#include <linux/usb.h>
#include <linux/delay.h>
#include <linux/ctype.h>
#include <linux/string.h>
#include <linux/capi.h>
#include <linux/kernelcapi.h>
#include <linux/isdn/capilli.h>
#include <linux/isdn/capiutil.h>
#include <linux/isdn/capicmd.h>
#include <stdarg.h>
#include "defs.h"
#include "libstub.h"
#include "tables.h"
#include "queue.h"
#include "buffers.h"
#include "lib.h"
#include "main.h"
#include "tools.h"
#include "fw.h"
#include "devif.h"
#include "driver.h"

#ifndef HZ
# error HZ is not defined...
#endif
#define	KILOBYTE		1024
#define	MEGABYTE		(1024*KILOBYTE)

#define	DEBUG_MEM_SIZE		MEGABYTE
#define	DEBUG_TIMEOUT		100
#define	RESET_DELAY		10

#define	TEN_MSECS		(HZ / 100)
#define	STACK_DELAY		(HZ / 2)
#define	USB_CTRL_TIMEOUT	3 * HZ
#define	USB_CFG_BLK_SIZE	777
#define	USB_BULK_TIMEOUT	3 * HZ
#define	USB_MAX_FW_LENGTH	393216

#define	PPRINTF_LEN		256

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
#define	DBG_FORMAT_LEN		1024
#define	DBG_MEM_SIZE		(1024 * 1024)
#define	DBG_TIMEOUT		100
#define	DBG_MIN_SIZE		1024

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
card_p				capi_card		= NULL;
lib_callback_t *		capi_lib		= NULL;
volatile unsigned long		stack_lock		= ATOMIC_INIT(0);
struct capi_ctr *		capi_controller[2]	= { NULL, NULL } ;
int				card_config;

static int			nvers			= 0;
static atomic_t			crit_count		= ATOMIC_INIT(0);
static atomic_t			scheduler_enabled	= ATOMIC_INIT(1);
static atomic_t			got_kicked		= ATOMIC_INIT(0);
static struct capi_ctr		capi_ctrl[2];
static unsigned			capi_ctrl_ix		= 0;
static bundle_t			ctrl_context[2];
static per_ctrl_t		ctrl_params[2];
static atomic_t			thread_flag		= ATOMIC_INIT(0);
static atomic_t			thread_capi_flag	= ATOMIC_INIT(0);
static int			thread_pid		= -1;
static atomic_t			tx_flag			= ATOMIC_INIT(0);
static atomic_t			rx_flag			= ATOMIC_INIT(0);
static char *			firmware_ptr		= NULL;
static unsigned			firmware_len		= 0;
static int			hard_error_issued	= FALSE;
static volatile unsigned long	resetting_ctrl		= 0;
static struct work_struct	closing_work;
static void			closing_worker (struct work_struct *);
static volatile unsigned long	closing_worker_running	= 0;
static void		     (* close_func) (void *)	= NULL;
static void *			close_data		= NULL;

static void			init_ctrl (void);
static int			kcapi_init (struct usb_device *);
static int			stack_init (void);
static void			kill_version (struct capi_ctr *);
static void			tx_task (unsigned long data);
static void			rx_task (unsigned long data);
static void			tx_handler (card_p pdc);
static void			rx_handler (card_p pdc);
static int			scheduler (void *);
static void			enable_thread (void);
static void			disable_thread (void);

static DECLARE_TASKLET(tx_tasklet, tx_task, 0);
static DECLARE_TASKLET(rx_tasklet, rx_task, 0);
static DECLARE_WAIT_QUEUE_HEAD(wait);
static DECLARE_WAIT_QUEUE_HEAD(capi_wait);
static DECLARE_COMPLETION(hotplug); /* New DECLARE, <arnd.feldmueller@web.de> */
static DECLARE_COMPLETION(config);  /* New DECLARE, <arnd.feldmueller@web.de> */
static DECLARE_COMPLETION(notify);  /* New DECLARE, <arnd.feldmueller@web.de> */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24))
static DECLARE_MUTEX_LOCKED(hotplug);
static DECLARE_MUTEX_LOCKED(config);
static DECLARE_MUTEX_LOCKED(notify);
#endif

static DECLARE_WAIT_QUEUE_HEAD(dbg_wait);
static DECLARE_COMPLETION(thread_sync); /* New DECLARE, <arnd.feldmueller@web.de> */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24))
static DECLARE_MUTEX_LOCKED(thread_sync);
#endif

#define SCHED_WAKEUP_CAPI       { atomic_set (&thread_capi_flag, 1); wake_up_interruptible (&capi_wait); }
#define SCHED_WAKEUP            { atomic_set (&got_kicked, 1); wake_up_interruptible (&wait); }

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static __attr void		scheduler_control (unsigned);
static __attr void		wakeup_control (unsigned);
static __attr void		version_callback (char *);
static __attr unsigned		scheduler_suspend (unsigned long);
static __attr unsigned		scheduler_resume (void);
static __attr unsigned		controller_remove (void);
static __attr unsigned		controller_add (void);

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
static int make_ctx (void ** pctx) {
	urb_context_p	uctx;
	
	assert (pctx != NULL);
	info (*pctx == NULL);
	if (NULL == (uctx = (urb_context_p) hcalloc (sizeof (urb_context_t)))) {
		ERROR("Error: Could not allocate transfer context!\n");
	}
	*pctx = (void *) uctx;
	return (uctx != NULL);
} /* make_ctx */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static int kill_ctx (void ** pctx) {
	urb_context_p	uctx;

	assert (pctx != NULL);
	assert (*pctx != NULL);
	uctx = (urb_context_p) *pctx;
	hfree (uctx);
	*pctx = NULL;
	return TRUE;
} /* kill_ctx */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
int start (card_p cp) {
	int		res = 0;
	unsigned	tn, ts, rn, rs;

	/* Allocating buffers */
	if (!dif_buffer_params (&tn, &ts, &rn, &rs)) {
		ERROR("Error: Could not get buffer geometry!\n");
		return 0;
	}
	if (NULL == (cp->tx_pool = init_urbs (tn, ts))) {
		return 0;
	}
	if (NULL == (cp->rx_pool = init_urbs (rn, rs))) {
		remove_urbs (&cp->tx_pool, kill_ctx);
		return 0;
	}
	if (build_urb_contexts (cp->tx_pool, make_ctx, kill_ctx)
	&&  build_urb_contexts (cp->rx_pool, make_ctx, kill_ctx)) {
		LOG("URB pool and context has been allocated!\n");
	} else {
		remove_urbs (&cp->tx_pool, kill_ctx);
		remove_urbs (&cp->rx_pool, kill_ctx);
		return 0;
	}
	
	/* Preparing the stack */
	cp->count = 0;
	table_init (&cp->appls);
	queue_init (&cp->queue);

	assert (capi_lib != NULL);
	assert (capi_lib->cm_register_ca_functions != NULL);
	assert (capi_lib->cm_start != NULL);
	assert (capi_lib->cm_init != NULL);
	assert (capi_lib->cm_exit != NULL);
	(*capi_lib->cm_register_ca_functions) (&ca_funcs);
	if ((*capi_lib->cm_start) ()) {
		ERROR("Starting the stack failed...\n");
	early_exit:
		remove_urbs (&cp->rx_pool, kill_ctx);
		remove_urbs (&cp->tx_pool, kill_ctx);
		queue_exit (&cp->queue);
		table_exit (&cp->appls);
		return FALSE;
	}
	(void) (*capi_lib->cm_init) (0, 0);
	assert (nvers == 2);
	
	/* Debug buffer */
	assert (capi_lib->cc_debugging_needed != NULL);
	if ((*capi_lib->cc_debugging_needed) ()) {
		LOG("Debugging enabled.\n");
	}

	assert (capi_lib->cc_run != NULL);
	res = (*capi_lib->cc_run) ();
	if (res == FALSE) {
		ERROR("Firmware does not respond!\n");
	reset_exit:
		(*capi_lib->cm_exit) ();
		goto early_exit;
	}
	assert (capi_lib->cc_compress_code != NULL);
	(*capi_lib->cc_compress_code) ();
	
	/* Start the stack */
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
	
	LOG("Stop:\n");
	(*capi_lib->cm_exit) ();
	if (thread_pid != -1) {
		disable_thread ();
	}
	card->running = FALSE;

	remove_urbs (&card->tx_pool, kill_ctx);
	remove_urbs (&card->rx_pool, kill_ctx);
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
static int remove_appls (card_p card) {
	appl_t *	appp;
	int		n = 0;

	if (0 != card->count) {
		LOG("Removing registered applications.\n");
		assert (card->appls != NULL);
		if (card->appls != NULL) {
			appp = first_appl (card->appls);
			while (appp != NULL) {
				++n;
				release_appl (capi_controller[1], appp->id);
				appp = next_appl (card->appls, appp);
			}
		}
		kick_scheduler ();
	}
	return n;
} /* remove_appls */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static void closing_worker (struct work_struct *work) {
	card_p	card = capi_card;

	DECLARE_WAIT_QUEUE_HEAD(close_wait);

	if (test_and_set_bit (0, &closing_worker_running)) {
		LOG("Double worker invocation!\n");
		return;
	}
	info (card != NULL);
	if (card == NULL) {
		LOG("Controller closed, worker exits.\n");
		return;
	}
	info (card->running != FALSE);
	NOTE("Closing...\n");

	/* Remove applications */
	if (0 != card->count) {
		assert (card->appls != NULL);
		remove_appls (card);
		LOG("Waiting...\n");
		init_waitqueue_head (&close_wait);
		wait_event_interruptible_timeout (close_wait, card->count == 0, STACK_DELAY);
		LOG("Wakeup!\n");
	}

	/* Stop protocoll stack */
	if (card->running) {
		stop (card);
	}

	/* Kernelcapi notification */
	if (capi_controller[0] != NULL) {
		detach_capi_ctr (capi_controller[0]);
	}
	detach_capi_ctr (capi_controller[1]);

	/* Dispose driver ressources */
	dif_exit ();
	fw_exit (&capi_card->c6205_ctx);
	if (firmware_ptr != NULL) {
		hfree (firmware_ptr);
		firmware_ptr = NULL;
		firmware_len = 0;
	}
	free_library ();
	init_ctrl ();

	/* Invoke callback */
	if (close_func != NULL) {
		LOG("Callback[%p](%p)\n", close_func, close_data);
		(* close_func) (close_data);
		close_func = NULL;
	}

	/* Final steps */
	assert (card->running == 0);
	capi_card = NULL;
	capi_lib = NULL;
	hfree (card);
	clear_bit (0, &resetting_ctrl);
	clear_bit (0, &closing_worker_running);

	LOG("Worker terminated.\n");

#undef CALLBACK

	hfree (capi_card);
	capi_card = NULL;
} /* closing_worker */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
void init_closing_worker (void) {

	clear_bit (0, &closing_worker_running);
} /* init_closing_worker */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
void start_closing_worker (void (* func) (void *), void * data) {

	if ((capi_card == NULL) || (test_and_set_bit (0, &resetting_ctrl)))  {
		LOG("Worker has to be scheduled only once.\n");
		return;
	}

	LOG("Preparing worker[%p, %p]...\n", func, data);
	assert (close_func == NULL);
	close_func = func;
	close_data = data;

	INIT_WORK (&closing_work, (work_func_t)closing_worker);
	schedule_work (&closing_work);
	LOG("Worker scheduled.\n");
} /* start_closing_worker */

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
    
	assert (ctrl);
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
static void select_completion (struct urb * purb) {
	struct usb_ctrlrequest	* req;

	assert (NULL != purb);
	req = (struct usb_ctrlrequest *) purb->context;
	LOG(
		"Completed SET_CONFIGURATION(%d), status %x.\n",
		req->wValue,
		purb->status
	);
	if (purb->status == 0) {
		card_config = req->wValue;
	}
	hfree (req);
	usb_free_urb (purb);
} /* select_completion */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
int select_config (
	struct usb_device *		dev, 
	int				confix, 
	usb_complete_t			cfunc
) {
	struct usb_ctrlrequest *	req;
	struct urb *			purb;
	int				cfg, res;
	
	cfg = (confix == UNDEF_CONFIG) ? 0 : confix;
	if (cfg == card_config) {
		LOG("select_config(%d) exited, already configured.\n", cfg);
		return 0;
	}
	LOG("select_config(%d)\n", cfg);

	req = (struct usb_ctrlrequest *) hmalloc (sizeof (struct usb_ctrlrequest));
	if (NULL == req) {
		ERROR("Not enough memory for device request.\n");
		return -ENOMEM;
	}
	if (NULL == (purb = usb_alloc_urb (0, GFP_ATOMIC))) {
		ERROR("Not enough memory for device request URB.\n");
		hfree (req);
		return -ENOMEM;
	}
	req->bRequestType	= 0;
	req->bRequest		= USB_REQ_SET_CONFIGURATION;
	req->wValue		= cfg;
	req->wIndex		= 0;
	req->wLength		= 0;
	usb_fill_control_urb (
		purb,
		dev,
		usb_sndctrlpipe (dev, 0),
		(unsigned char *) req,
		NULL,
		0,
		(cfunc == NULL) ? select_completion : cfunc,
		req
	);
	if (0 != (res = usb_submit_urb (purb, GFP_ATOMIC))) {
		hfree (req);
	}
	return res;
} /* select_config */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static void reset_completion (struct urb * purb) {
	struct usb_ctrlrequest *	req;

	assert (NULL != purb);
	info (purb->status == 0);

	LOG("Completed device reset, status %x.\n", purb->status);
	req = (struct usb_ctrlrequest *) purb->context;
	hfree (req);
	usb_free_urb (purb);
} /* reset_completion */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static int reset (struct usb_device * dev) {

	return select_config (dev, RESET_CONFIG, reset_completion);
} /* reset */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
void shutdown (void) {

	assert (capi_card != NULL);
	if (!capi_card->running || test_bit (0, &resetting_ctrl)) {
		return;
	}
	NOTE("Resetting USB device...\n");
	reset (capi_card->dev);
	start_closing_worker (NULL, NULL);
} /*shutdown */

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
#if defined (__fcdslslusb__) || defined (__fcdslusba__)
	if (ctrl == capi_controller[0]) {
		lib_memset (&ctrl->profile, 0, sizeof (capi_profile));
	} else {
		lib_memcpy (&ctrl->profile, C->string[6], sizeof (capi_profile));
	}
#else
	lib_memcpy (&ctrl->profile, C->string[6], sizeof (capi_profile));
#endif
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
\*-F-------------------------------------------------------------------------*/
static int __kcapi load_ware (struct capi_ctr * ctrl, capiloaddata * ware) {
	card_p		cp;
	char *		buffer;
	int		res = 0;

	assert (ctrl != NULL);
	assert (ware != NULL);
	if (NULL == (cp = GET_CARD(ctrl))) {
		ERROR("Controller not available!\n");
		return -EIO;
	}
	if (cp->running) {
		LOG("Firmware has already been loaded!\n");
		return 0;
	}

	assert (firmware_ptr == NULL);
	if (USB_MAX_FW_LENGTH < ware->firmware.len) {
		ERROR("Invalid firmware file.\n");
		return -EIO;
	}
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
	cp->c6205_ctx = fw_init (
				cp, 
				firmware_ptr, 
				firmware_len, 
				&res
			);
	if ((NULL == cp->c6205_ctx) || (res != 0)) {
		ERROR("Error while processing firmware (%d)!\n", res);
	error_noctx:
		fw_exit (&cp->c6205_ctx);
		hfree (firmware_ptr);
		firmware_ptr = NULL;
		return -EIO;
	}
	assert (capi_lib != NULL);
	assert (capi_lib->cc_link_version != NULL);
	assert ((*capi_lib->cc_link_version) () > 1);
	if (!dif_init (cp)) {
		ERROR("Error while setting up device structures!\n");
	error_init:
		goto error_noctx;
	}

	fw_setup (cp->c6205_ctx);
	if (0 != stack_init ()) {
		ERROR("Error while starting protocoll stack!\n");
	/* error_stack: */
		dif_exit ();
		goto error_init;
	}
	return 0;
} /* load_ware */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
int __kcapi add_card (struct usb_device * dev) {

	assert (capi_card != NULL);
	if (test_bit (0, &resetting_ctrl)) {
		ERROR("Reconnect too fast!\n");
		return -EBUSY;
	}
	init_waitqueue_head (&wait);
	return kcapi_init (dev);
} /* add_card */ 

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static void __kcapi reset_ctrl (struct capi_ctr * ctrl) {

	UNUSED_ARG (ctrl);
	shutdown ();
} /* reset_ctrl */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static char * __kcapi proc_info (struct capi_ctr * ctrl) {
	card_p		cp;
	per_ctrl_p	C;
	static char	text[80];

	assert (ctrl);
	assert (ctrl->driverdata);
	cp = GET_CARD (ctrl);
	C  = GET_CTRL (ctrl);
	snprintf (
		text, 
		sizeof (text),
		"%s %s %d",
		C->version ? C->string[1] : "A1",
		C->version ? C->string[0] : "-",
		cp->dev->devnum
	);
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

	assert (ctrl);
	assert (ctrl->driverdata);
	cp = GET_CARD (ctrl);
	C  = GET_CTRL (ctrl);
	pprintf (page, &len, "%-16s %s\n", "name", SHORT_LOGO);
	pprintf (page, &len, "%-16s %d\n", "dev", cp->dev->devnum);
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
	capi_controller[0] = capi_controller[1] = NULL;
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
	snprintf (cptr->name, 32, "%s-%04x", TARGET, cp->dev->devnum);

	if (0 != (res = attach_capi_ctr (cptr))) {
		--capi_ctrl_ix;
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
static int kcapi_init (struct usb_device * dev) {
	int     res;

	UNUSED_ARG (dev);
	if (NULL != capi_controller[1]) {
		ERROR("Cannot handle two controllers.\n");
		return -EBUSY;
	}
	init_ctrl ();
#if defined (__fcdslslusb__) || defined (__fcdslusba__)
	if (0 != (res = setup_ctrl (capi_card, 2))) {
		init_ctrl ();
		return res;
	}
#else
	if (0 != (res = setup_ctrl (capi_card, 1))) {
		return res;
	}
	if (ctrl_context[0].ctrl != NULL) {
		if (0 != (res = setup_ctrl (capi_card, 2))) {
			detach_capi_ctr (capi_controller[0]);
			init_ctrl ();
			return res;
		}
	}
#endif
	capi_card->ctrl2 = capi_controller[1]->cnr;
	capi_card->ctxp = NULL;
	return 0;
} /* kcapi_init */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static int stack_init (void) {

	if (!start (capi_card)) {
		ERROR("Initialization failed.\n");
		return -EIO;
	}
	if (capi_controller[0] != NULL) {
		capi_ctr_ready (capi_controller[0]);
	}
	capi_ctr_ready (capi_controller[1]);
	return 0;
} /* stack_init */

/*---------------------------------------------------------------------------*\
\*-C-------------------------------------------------------------------------*/
static inline int in_critical (void) {

	return (atomic_read (&crit_count) > 0);
} /* in_critical */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static inline void check (void) {

        if (atomic_read (&rx_flag) > 0) {
		rx_handler (capi_card);
	}
	if (atomic_read (&tx_flag) > 0) {
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
			"PUT_MESSAGE(ctrl:%d,appl:%u,cmd:%X,subcmd:%X)\n", 
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
void __stack msg2capi (unsigned char * msg) {
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
		ctrl = (NULL != capi_controller[0]) 
			? capi_controller[0]
			: capi_controller[1];
	}
	assert (ctrl != NULL);
	capi_ctr_handle_message (ctrl, appl, skb);
} /* msg2capi */

/*-S-------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/ 
void kick_scheduler (void) {

	if ((thread_pid != -1) && (NULL != find_task_by_pid (thread_pid))) {
		atomic_set (&scheduler_enabled, 1);
		SCHED_WAKEUP;
	} 
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

/*-S-------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static int scheduler (void * arg) {

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
		if (!test_and_set_bit (0, &stack_lock)) {
			os_timer_poll ();
			assert (capi_lib->cm_schedule != NULL);
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
} /* scheduler */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
int make_thread (void) {

	if (-1 != thread_pid) {
		LOG("Thread[%d] already running.\n", thread_pid);
		return thread_pid;
	}
	atomic_set (&thread_flag, 1);
	atomic_set (&thread_capi_flag, 0);
	init_waitqueue_head (&capi_wait);
	thread_pid = kernel_thread (scheduler, NULL, 0);
	LOG("Thread[%d] started.\n", thread_pid);
	return thread_pid;
} /* make_thread */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
void kill_thread (void) {

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
static void disable_thread (void) {

	info (atomic_read (&thread_capi_flag));
	if (atomic_read (&thread_capi_flag)) {
		atomic_set (&thread_capi_flag, 0);
		SCHED_WAKEUP;
		LOG("Thread disabled.\n");
	}
} /* disable_thread */

/*---------------------------------------------------------------------------*\
\*-T-------------------------------------------------------------------------*/ 
static void tx_handler (card_p pdc) {
	struct urb *		purb;
	void *			ptr;
	urb_context_p		pctx;
	hw_usb_request_p	urp;
	int			status;
	unsigned		alength;
	
	assert (pdc != NULL);
	atomic_dec (&tx_flag);
	if (!unlist_urb (pdc->tx_pool, &purb, &ptr)) {
		LOG("TX completion w/o URB!\n");
		return;
	}

	info (!atomic_read (&f_stop_tx_pending));
	atomic_dec (&n_tx_pending);
	
	pctx = (urb_context_p) ptr;
	urp = (hw_usb_request_p) pctx->bdp->context;
	assert (urp != NULL);
	assert (pctx->buffer == (pctx->bdp->buffer + urp->offset));
	assert (test_and_clear_bit (0, &urp->in_use));

	status = purb->status;
	alength = purb->actual_length;
	release_urb (pdc->tx_pool, purb);
	if (0 != purb->status) {
		LOG(
			"TX URB(%p,%d) status: %ld\n", 
			purb,
			(uintptr_t) purb->context,
			purb->status
		);
		dif_error (
			USB_EVENT_HARD_ERROR, 
			pctx->bdp,
			urp->offset,
			urp->length,
			urp->context
			);
	} else {
		assert (pctx->callback != NULL);
		(* pctx->callback) (
			pctx->bdp,
			urp->offset,
			purb->actual_length,
			urp->context
		);
	}
	kick_scheduler ();
} /* tx_handler */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/ 
static void rx_handler (card_p pdc) {
	struct urb *		purb;
	void *			ptr;
	urb_context_p		pctx;
	hw_usb_request_p	urp;
	int			status;
	unsigned		alength;
	
	assert (pdc != NULL);
	atomic_dec (&rx_flag);
	if (!unlist_urb (pdc->rx_pool, &purb, &ptr)) {
		LOG("Completion w/o URB!\n");
		return;
	}
	info (!atomic_read (&f_stop_rx_pending));
	atomic_dec (&n_rx_pending);
	
	pctx = (urb_context_p) ptr;
	urp = (hw_usb_request_p) pctx->bdp->context;
	assert (urp != NULL);
	assert (pctx->buffer == (pctx->bdp->buffer + urp->offset));
	assert (test_and_clear_bit (0, &urp->in_use));

	status = purb->status;
	alength = purb->actual_length;
	release_urb (pdc->rx_pool, purb);
	if (0 != status) {
		LOG(
			"RX URB(%p,%d) status: %ld\n", 
			purb,
			(uintptr_t) purb->context,
			purb->status
		);
		dif_error (
			USB_EVENT_HARD_ERROR, 
			pctx->bdp,
			urp->offset,
			urp->length,
			urp->context
			);
	} else {
		assert (pctx->callback != NULL);
		(* pctx->callback) (
			pctx->bdp,
			urp->offset,
			alength,
			urp->context
		);
	}
	kick_scheduler ();
} /* rx_handler */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/ 
static void tx_task (unsigned long data) {

	UNUSED_ARG (data);
	if (in_critical ()) return;
	if (!test_and_set_bit (0, &stack_lock)) {
		tx_handler (capi_card);
		clear_bit (0, &stack_lock);
	}
} /* tx_task */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/ 
static void rx_task (unsigned long data) {

	UNUSED_ARG (data);
	if (in_critical ()) return;
	if (!test_and_set_bit (0, &stack_lock)) {
		rx_handler (capi_card);
		clear_bit (0, &stack_lock);
	}
} /* rx_task */

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
\*-U-------------------------------------------------------------------------*/
static void issue_hard_error (card_p pdc, urb_context_p ctx, const char * msg, int arg) {
	hw_usb_request_p	urp;

	UNUSED_ARG(pdc);
	NOTE(msg, arg);
	if (!hard_error_issued) {
		hard_error_issued = TRUE;
		urp = (hw_usb_request_p) ctx->bdp->context;
		assert (urp != NULL);
		dif_error (
			USB_EVENT_HARD_ERROR, 
			ctx->bdp, 
			urp->offset,
			urp->length,
			urp->context
		);
	} else {
		NOTE("Extra events ignored.\n");
	}
} /* issue_hard_error */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
#if !defined (USE_SYSTEM_ZERO_CODE)
static void usb_zero_completion (struct urb * purb) {

	info (purb->status == 0);
	return;
} /* usb_zero_compl,etion */

static struct urb * get_zero_urb (card_p pdc) {
	static struct urb	zero_urb;
	static int		zero_urb_ready = 0;
	
	if (!zero_urb_ready) {
		usb_fill_bulk_urb (
			&zero_urb, pdc->dev, 
			usb_sndbulkpipe (pdc->dev, pdc->epwrite->bEndpointAddress),
			0, 
			0, 
			usb_zero_completion, 
			NULL
		);
		zero_urb_ready = 1;
	}
	return &zero_urb;
} /* get_zero_urb */
#endif

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static void usb_write_completion (struct urb * purb) {

	assert (purb != NULL);
	atomic_inc (&tx_flag);
	enlist_urb (capi_card->tx_pool, purb);
	tasklet_schedule (&tx_tasklet);
} /* usb_write_completion */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static void usb_read_completion (struct urb * purb) {
	
	assert (purb != NULL);
	atomic_inc (&rx_flag);
	enlist_urb (capi_card->rx_pool, purb);
	tasklet_schedule (&rx_tasklet);
} /* usb_read_completion */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/ 
static int usb_write (
	card_p			pdc, 
	struct urb *		purb, 
	urb_context_p		pctx, 
	void *			buffer, 
	unsigned		length
) {
	int			res;
#if !defined (USE_SYSTEM_ZERO_CODE)
	static struct urb *	urb0;
	unsigned long		flags;
	static spinlock_t	order_lock = SPIN_LOCK_UNLOCKED;
#endif
	
	assert (pdc != NULL);
	assert (buffer != NULL);
	usb_fill_bulk_urb (
		purb,
		pdc->dev,
		usb_sndbulkpipe (pdc->dev, pdc->epwrite->bEndpointAddress),
		buffer,
		length,
		usb_write_completion,
		purb->context
		);

#if defined (USE_SYSTEM_ZERO_CODE)
	if ((length > 0) && ((length % 64) == 0)) {
		LOG("Zero byte URB required!\n");
	}
	/* Queued bulk transfer! */
	urb->transfer_flags |= USB_ZERO_PACKET;
	res = usb_submit_urb (purb, GFP_ATOMIC);
#else
	/* Queued bulk transfer! */
	if ((length > 0) && ((length % 64) == 0)) {
		LOG("Writing zero byte URB!\n");
		urb0 = get_zero_urb (pdc);
		spin_lock_irqsave (&order_lock, flags);
		if (0 == (res = usb_submit_urb (purb, GFP_ATOMIC))) {
			res = usb_submit_urb (urb0, GFP_ATOMIC);
		}
		spin_unlock_irqrestore (&order_lock, flags);
	} else {
		spin_lock_irqsave (&order_lock, flags);
		res = usb_submit_urb (purb, GFP_ATOMIC);
		spin_unlock_irqrestore (&order_lock, flags);
	}
#endif
	if (0 != res) {
		atomic_dec (&n_tx_pending);
		issue_hard_error (pdc, pctx, "TX URB submission failed, code %d.\n", res);
		return FALSE;
	}
	return TRUE;
} /* usb_write */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static int usb_read (
	card_p		pdc, 
	struct urb *	purb, 
	urb_context_p	pctx, 
	void *		buffer,
	unsigned	length
) {
	int		res;

	assert (pdc != NULL);
	assert (buffer != NULL);
	usb_fill_bulk_urb (
		purb,
		pdc->dev,
		usb_rcvbulkpipe (pdc->dev, pdc->epread->bEndpointAddress),
		buffer,
		length,
		usb_read_completion,
		purb->context
		);
	
	/* Queued bulk transfer! */
	if (0 != (res = usb_submit_urb (purb, GFP_ATOMIC))) {
		atomic_dec (&n_rx_pending);
		issue_hard_error (pdc, pctx, "Rx URB submission failed, code %d.\n", res);
		return FALSE;
	}
	return TRUE;
} /* usb_read */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/ 
int os_usb_submit_rx (
	int			pipe,
	void *			buf,
	unsigned		len,
	hw_completion_func_t	completion, 
	hw_buffer_descriptor_p	dp
) {
	struct urb *		purb;
	void *			ptr;
	urb_context_p		pctx;

	assert (pipe == capi_card->epread->bEndpointAddress);
	assert (capi_card->rx_pool != NULL);
	if (!claim_urb (capi_card->rx_pool, &purb, &ptr)) {
		LOG("No RX URB/buffer available.\n");
		return FALSE;
	}
	assert (ptr != NULL);
	pctx = (urb_context_p) ptr;
	pctx->callback = completion;
	pctx->buffer = buf;
	pctx->bdp = dp;

	return usb_read (capi_card, purb, pctx, buf, len);
} /* os_usb_submit_rx */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/ 
int os_usb_submit_tx (
	int			pipe,
	void *			buf,
	unsigned		len,
	hw_completion_func_t	completion, 
	hw_buffer_descriptor_p	dp
) {
	struct urb *		purb;
	void *			ptr;
	urb_context_p		pctx;

	assert (pipe == capi_card->epwrite->bEndpointAddress);
	assert (capi_card->tx_pool != NULL);
	if (!claim_urb (capi_card->tx_pool, &purb, &ptr)) {
		LOG("No TX URB/buffer available.\n");
		return FALSE;
	}
	assert (ptr != NULL);
	pctx = (urb_context_p) ptr;
	pctx->callback = completion;
	pctx->buffer = buf;
	pctx->bdp = dp;

	return usb_write (capi_card, purb, pctx, buf, len);
} /* os_usb_submit_tx */

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
