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

#include <asm/atomic.h>
#include <asm/uaccess.h>
#include <linux/spinlock.h>
#include <linux/wait.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/sched.h>
#include <linux/ctype.h>
#include <linux/string.h>
#include <linux/capi.h>
#include <linux/kernelcapi.h>
#include <linux/isdn/capilli.h>
#include <linux/isdn/capiutil.h>
#include <linux/isdn/capicmd.h>
#include <stdarg.h>
#include <linux/usb.h>
#include "tools.h"
#include "main.h"
#include "libdefs.h"
#include "tables.h"
#include "queue.h"
#include "defs.h"
#include "lib.h"
#include "driver.h"

#ifndef HZ
# error HZ is not defined...
#endif
#define	TEN_MSECS		(HZ / 100)
#define	STACK_DELAY		(HZ / 2)
#define USB_EVENT_HARD_ERROR    0x800
#define	USB_CTRL_TIMEOUT	3 * HZ
#if defined (__fcusb2__)
#define	USB_CFG_BLK_SIZE	259
#define	USB_BULK_TIMEOUT	3 * HZ
#define	USB_MAX_FW_LENGTH	32768
#define	KMALLOC_LIMIT		128 * 1024
#endif

#define	PPRINTF_LEN		256

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
card_p				capi_card		= NULL;
lib_callback_p			capi_lib		= NULL;
struct capi_ctr *		capi_ctrl		= NULL;

static int			nvers			= 0;
static atomic_t			scheduler_enabled	= ATOMIC_INIT (1);
static atomic_t			got_kicked		= ATOMIC_INIT (0);
static atomic_t			crit_count		= ATOMIC_INIT (0);
static volatile int		hard_error_issued;
static spinlock_t		stack_lock		= SPIN_LOCK_UNLOCKED;
static int			thread_pid		= -1;
static atomic_t			thread_run_flag;
static atomic_t			thread_capi_flag;
static atomic_t			tx_flag			= ATOMIC_INIT (0);
static atomic_t			rx_flag			= ATOMIC_INIT (0);
static atomic_t			resetting_ctrl		= ATOMIC_INIT (0);
int				card_config;
static struct work_struct	closing_work;
static void			closing_worker (struct work_struct *);
static atomic_t			closing_worker_running	= ATOMIC_INIT (0);
static void		     (* close_func) (void *)	= NULL;
static void *			close_data		= NULL;

static DECLARE_WAIT_QUEUE_HEAD(wait);
static DECLARE_WAIT_QUEUE_HEAD(capi_wait);
static DECLARE_WAIT_QUEUE_HEAD(tx_wait);
static DECLARE_WAIT_QUEUE_HEAD(rx_wait);
static DECLARE_COMPLETION(hotplug); /* New DECLARE, <arnd.feldmueller@web.de> */
static DECLARE_COMPLETION(notify);  /* New DECLARE, <arnd.feldmueller@web.de> */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24))
static DECLARE_MUTEX_LOCKED(hotplug);
static DECLARE_MUTEX_LOCKED(notify);
#endif

#define	SCHED_WAKEUP_CAPI	{ atomic_set (&thread_capi_flag, 1); wake_up_interruptible (&capi_wait); }
#define	SCHED_WAKEUP		{ atomic_set (&got_kicked, 1); wake_up_interruptible (&wait); }

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static void			enable_thread (void);
static void 			disable_thread (void);
static int			stack_init (struct usb_device *);
static int			scheduler (void *);
__attr void			usb_TxStart (void);
static void			usb_RxStart (void);
static void			tx_handler (card_p pdc);
static void			rx_handler (card_p pdc);
static void			tx_task (unsigned long);
static void			rx_task (unsigned long);

static DECLARE_TASKLET(tx_tasklet, tx_task, 0);
static DECLARE_TASKLET(rx_tasklet, rx_task, 0);

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static __attr void		scheduler_control (unsigned);
static __attr void		wakeup_control (unsigned);
static __attr void		version_callback (char *);
static __attr unsigned		scheduler_suspend (unsigned long);
static __attr unsigned		scheduler_resume (void);
static __attr unsigned		controller_remove (void);
static __attr unsigned		controller_add (void);

static functions_t		cafuncs = {

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
int nbchans (struct capi_ctr * ctrl) {
	card_p		card;
	unsigned char *	prf;
	int		temp = 2;
    
	assert (ctrl);
	card = (card_p) ctrl->driverdata;
	prf = (unsigned char *) card->string[6];
	if (prf != NULL) {
		temp = prf[2] + 256 * prf[3];
	}
	return temp;
} /* nbchans */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
#if defined (__fxusb__)
static inline const char * name_of (int code) {

	switch (code) {

	default:
	case 1:		return "FRITZ!X USB";
	case 2:		return "FRITZ!X ISDN";
	case 3:		return "FRITZ!X USB";
	}
} /* name_of */
#endif

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

	return select_config (dev, INITIAL_CONFIG, reset_completion);
} /* reset */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
#if defined (__fcusb2__)
static int emit (card_p ctx, char * ptr, int len, int * olen) {
	int	res;

	res = usb_bulk_msg (
		ctx->dev,
		usb_sndbulkpipe (ctx->dev, ctx->epwrite->bEndpointAddress),
		ptr,
		len,
		olen,
		USB_BULK_TIMEOUT
		);
	if (0 != res) {
		ERROR("USB I/O error, code %d!\n", res);
	}
	return res;
} /* emit */
#endif

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static void scan_version (card_p card, const char * ver) {
	int	vlen, i;
	char *	vstr;

	vlen = (unsigned char) ver[0];
	card->version = vstr = (char *) hmalloc (vlen);
	if (NULL == card->version) {
		LOG("Could not allocate version buffer.\n");
		return;
	}
	lib_memcpy (card->version, ver + 1, vlen);
	lib_memset (card->string, 0, sizeof (card->string));
	i = 0;
	while ((i < 8) && (vlen >= (vstr - card->version))) {
		card->string[i++] = vstr + 1;
		vstr += 1 + *vstr;
	} 
#ifdef NDEBUG
	NOTE("Stack version %s\n", card->string[0]);
#endif
	LOG("Library version:    %s\n", card->string[0]);
	LOG("Card type:          %s\n", card->string[1]);
	LOG("Capabilities:       %s\n", card->string[4]);
	LOG("D-channel protocol: %s\n", card->string[5]);
} /* scan_version */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static void copy_version (struct capi_ctr * ctrl) {
	char *	tmp;
	card_p	card;

	assert (ctrl);
	card = (card_p) ctrl->driverdata;
	if (NULL == (tmp = card->string[3])) {
		ERROR("Do not have version information...\n");
		return;
	}
	lib_strncpy (ctrl->serial, tmp, CAPI_SERIAL_LEN);
	lib_memcpy (&ctrl->profile, card->string[6], sizeof (capi_profile));
	strncpy (ctrl->manu, "AVM GmbH", CAPI_MANUFACTURER_LEN);
	ctrl->version.majorversion = 2;
	ctrl->version.minorversion = 0;
	tmp = card->string[0];
	ctrl->version.majormanuversion = (((tmp[0] - '0') & 15) << 4)
					+ ((tmp[2] - '0') & 15);
	ctrl->version.minormanuversion = ((tmp[3] - '0') << 4)
					+ (tmp[5] - '0') * 10
					+ ((tmp[6] - '0') & 15);
} /* copy_version */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static void kill_version (card_p card) {
	int i;

	for (i = 0; i < 8; i++) {
		card->string[i] = NULL;
	}
	if (card->version != NULL) {
		hfree (card->version);
		card->version = NULL;
	}
	nvers = 0;
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
				release_appl (
					capi_ctrl,
					appp->id
				);
				appp = next_appl (card->appls, appp);
			}
		}
		kick_scheduler ();
	}
	return n;
} /* remove_appls */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static void stop (card_p card) {

	(*capi_lib->cm_exit) ();
	if (-1 != thread_pid) {
		disable_thread ();
	}
	queue_exit (&card->queue);
	table_exit (&card->appls);
	kill_version (card);
	card->running = FALSE;
} /* stop */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static int start (card_p card) {

	card->count = 0;
	table_init (&card->appls);
	queue_init (&card->queue);
	(*capi_lib->cm_register_ca_functions) (&cafuncs);
	(*capi_lib->cm_start) ();
	(void) (*capi_lib->cm_init) (0, 0);
	assert (nvers == 1);
	enter_critical ();
	if ((*capi_lib->cm_activate) ()) {
		LOG("Activate failed.\n");
		leave_critical ();
		stop (card);
		return FALSE;
	}
	leave_critical ();
	enable_thread ();
	return (card->running = TRUE); 
} /* start */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static int __kcapi load_ware (struct capi_ctr * ctrl, capiloaddata * ware) {
#if defined (__fcusb2__)
	card_p		ctx;
	int		len, todo, olen;
	char *		tmp;
	char *		ptr;
	int		res	= 13;
#if !defined (NDEBUG)
	unsigned	count	= 0;
#endif

	assert (ware);
	assert (ctrl);
	if (capi_lib == NULL) {
		ERROR("No controller!\n");
		return -EBUSY;
	}
	if (NULL == (ctx = (card_p) ctrl->driverdata)) {
		ERROR("Controller not available!\n");
		return -EIO;
	}
	info (card_config == LOADING_CONFIG);
	if (LOADING_CONFIG != card_config) {
		res = select_config (ctx->dev, LOADING_CONFIG, NULL);
		if (0 != res) {
			ERROR("Could not set load config.\n");
			return -EIO;
		}
		mdelay (10);
	}
	assert (USB_MAX_FW_LENGTH < KMALLOC_LIMIT);
	if ((todo = ware->firmware.len) > USB_MAX_FW_LENGTH) {
		ERROR("Invalid firmware file!\n");
		return -EIO;
	}
	if (NULL == (ptr = tmp = (char *) hmalloc (todo))) {
		ERROR("Cannot build firmware buffer!\n");
		return -ENOMEM;
	}
	if (ware->firmware.user) {
		if (copy_from_user (tmp, ware->firmware.data, todo)) {
			ERROR("Error copying firmware data!\n");
			return -EIO;
		}
	} else {
		lib_memcpy (tmp, ware->firmware.data, todo);
	}
	while (todo > 0) {
		len = (todo > USB_CFG_BLK_SIZE) ? USB_CFG_BLK_SIZE : todo;
		if (0 != (res = emit (ctx, ptr, len, &olen))) {
			LOG(
				"Error in %d-byte block #%d.\n",
				USB_CFG_BLK_SIZE,
				count
			);
			break;
		}
		assert (len == olen);
		ptr  += olen;
		todo -= olen;
#if !defined (NDEBUG)
		++count;
#endif
	}
	if (0 == res) {
		res = emit (ctx, NULL, 0, &olen);
	}	
	hfree (tmp);
	if (0 == res) {
		res = select_config (ctx->dev, RUNNING_CONFIG, NULL);
		mdelay (10);
	}
	if (0 == res) {
		return stack_init (ctx->dev);
	}
	ERROR("Firmware does not respond!\n");
	capi_ctr_reseted (ctrl);
	return -EBUSY;
#else
	UNUSED_ARG (ctrl);
	UNUSED_ARG (ware);

	ERROR("Cannot load firmware onto passive controller.\n");
	return -EIO;
#endif
} /* load_ware */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static void __kcapi reset_ctrl (struct capi_ctr * ctrl) {
	
	UNUSED_ARG (ctrl);
	assert (capi_card != NULL);
	reset (capi_card->dev);
	if (!capi_card->running) {
		return;
	}
	if (!atomic_read (&resetting_ctrl)) {
		NOTE("Resetting USB device...\n");
		start_closing_worker (NULL, NULL);
	}
} /* reset_ctrl */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static char * __kcapi proc_info (struct capi_ctr * ctrl) {
	card_p		card;
	static char	text[80];

	assert (ctrl);
	card = (card_p) ctrl->driverdata;
	snprintf (
		text, 
		sizeof (text),
		"%s %s %d",
		card->version ? card->string[1] : "A1",
		card->version ? card->string[0] : "-",
		card->dev->devnum
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
	card_p			card;
	char *			temp;
	unsigned char		flag;
	int			len = 0;

	assert (ctrl);
	card = (card_p) ctrl->driverdata;
#if defined (__fxusb__)
	assert (card->dc <= MAX_DEVICE_CODE);
	pprintf (page, &len, "%-16s %s (%s)\n", "name", SHORT_LOGO, name_of (card->dc));
#else
	pprintf (page, &len, "%-16s %s\n", "name", SHORT_LOGO);
#endif
	pprintf (page, &len, "%-16s %d\n", "dev", card->dev->devnum);
	temp = card->version ? card->string[1] : "A1";
	pprintf (page, &len, "%-16s %s\n", "type", temp);
	temp = card->version ? card->string[0] : "-";
	pprintf (page, &len, "%-16s %s\n", "ver_driver", temp);
	pprintf (page, &len, "%-16s %s\n", "ver_cardtype", SHORT_LOGO);

	flag = ((unsigned char *) (ctrl->profile.manu))[3];
	if (flag) {
		pprintf (page, &len, "%-16s%s%s%s%s%s%s%s\n", "protocol",
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
		pprintf (page, &len, "%-16s%s%s%s%s\n", "linetype",
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
static int kcapi_init (struct usb_device * dev) {
	struct capi_ctr *	ctrl;
	
	if (NULL != capi_ctrl) {
		ERROR("Cannot handle two controllers!\n");
 		return -EBUSY;
	}
	ctrl = &capi_card->ctr;
	
	ctrl->driver_name = TARGET;
	ctrl->driverdata = (void *) capi_card;
	ctrl->owner = THIS_MODULE;
	snprintf (ctrl->name, 32, "%s-%04x", TARGET, dev->devnum);

	ctrl->load_firmware =	load_ware;
	ctrl->reset_ctr =	reset_ctrl;
	ctrl->register_appl =	register_appl;
	ctrl->release_appl =	release_appl;
	ctrl->send_message =	send_msg;
	ctrl->procinfo =	proc_info;
	ctrl->ctr_read_proc =	ctr_info;
	if (0 != attach_capi_ctr (ctrl)) {
		ERROR("Could not attach the controller.\n");
		return -EBUSY;
	}
	capi_ctrl = ctrl;
	return 0;
} /* kcapi_init */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static int stack_init (struct usb_device * dev) {
	int	res = 0;

#if defined (__fcusb2__)
	UNUSED_ARG (dev);
#endif
	if (!start (capi_card)) {
		ERROR("Initialization failed.\n");
		return -EIO;
	}
#if !defined (__fcusb2__)
	if (0 != (res = kcapi_init (dev))) {
		stop (capi_card);
 		return res;
	}
#endif
	assert (nvers == 1);
	copy_version (capi_ctrl);
	capi_ctr_ready (capi_ctrl);
	return res;
} /* stack_init */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
int __kcapi add_card (struct usb_device * dev) {
	
	init_waitqueue_head (&wait);
	atomic_set (&resetting_ctrl, 0);
#if defined (__fcusb2__)
	return kcapi_init (dev);
#else
	return stack_init (dev);
#endif
} /* add_card */ 

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static void closing_worker (struct work_struct *work) {
	card_p	card = capi_card;

	DECLARE_WAIT_QUEUE_HEAD(close_wait);

	if (atomic_xchg (&closing_worker_running, 1)) {
		LOG("Double worker invocation!\n");
		return;
	}
	info (card != NULL);
	if (card == NULL) {
		LOG("Controller closed, worker exits.\n");
		return;
	}
	info (card->running != 0);
	LOG("Worker started.\n");

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
	if (capi_ctrl != NULL) {
		detach_capi_ctr (capi_ctrl);
	}
	capi_ctrl = NULL;

	/* Dispose driver ressources */	
	free_library ();
	if (NULL != card->rx_urb) {
		LOG("Removing RX URB...\n");
		usb_kill_urb (card->rx_urb);
		usb_free_urb (card->rx_urb);
		card->rx_urb = NULL;
	}
	if (NULL != card->tx_urb) {
		LOG("Removing TX URB...\n");
		usb_kill_urb (card->tx_urb);
		usb_free_urb (card->tx_urb);
		card->tx_urb = NULL;
	}
	if (NULL != card->rx_buffer) hfree (card->rx_buffer);
	if (NULL != card->tx_buffer) hfree (card->tx_buffer);

	/* Invoke callback */
	if (close_func != NULL) {
		LOG("Callback[%p](%p)\n", close_func, close_data);
		(* close_func) (close_data);
		close_func = NULL;
	}
	
	/* Final steps */
	assert (card->running == 0);
	capi_lib = NULL;
	atomic_set (&resetting_ctrl, 0);
	atomic_set (&closing_worker_running, 0);

	LOG("Worker terminated.\n");

#undef CALLBACK

	hfree (capi_card);
	capi_card = NULL;
} /* closing_worker */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
void init_closing_worker (void) {

	atomic_set (&closing_worker_running, 0);
} /* init_closing_worker */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
void start_closing_worker (void (* func) (void *), void * data) {

	if (atomic_xchg (&resetting_ctrl, 1)) {
		LOG("Worker has been scheduled.\n");
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
int msg2stack (unsigned char * msg) {
	unsigned		mlen, appl, hand;
	__u32			ncci;
	struct sk_buff *	skb;
	unsigned char *		mptr;

	assert (capi_card != NULL);
	assert (msg != NULL);
	if (NULL != (skb = queue_get (capi_card->queue))) {
		mptr = (unsigned char *) skb->data;
		mlen = CAPIMSG_LEN(mptr); 
		appl = CAPIMSG_APPID(mptr);

		MLOG(
			"PUT_MESSAGE(appl:%u,cmd:%X,subcmd:%X)\n", 
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
			dev_kfree_skb_any (skb);
		}
	}
	return (skb != NULL);
} /* msg2stack */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
void msg2capi (unsigned char * msg) {
	unsigned		mlen, appl, dlen;
#ifndef __LP64__
	__u32			dummy;
#endif
	unsigned char *		dptr;
	struct sk_buff *	skb;

	assert (capi_card != NULL);
	assert (msg != NULL);
	mlen = CAPIMSG_LEN(msg);
	appl = CAPIMSG_APPID(msg);

	MLOG(
		"GET_MESSAGE(appl:%u,cmd:%X,subcmd:%X)\n", 
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
	assert (capi_ctrl != NULL);
	capi_ctr_handle_message (capi_ctrl, appl, skb);
} /* msg2capi */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/ 
void init (unsigned len, void (__attr2 * reg) (void *, unsigned),
                         void (__attr2 * rel) (void *), 
			 void (__attr2 * dwn) (void)) {

	assert (reg);
	assert (rel);
	assert (dwn);

	capi_card->length   = len;
	capi_card->reg_func = reg;
	capi_card->rel_func = rel;
	capi_card->dwn_func = dwn;
} /* init */

/*---------------------------------------------------------------------------*\
\*-U-------------------------------------------------------------------------*/
static void issue_hard_error (card_p pdc, const char * msg, int arg) {
	
	NOTE(msg, arg);
	if (!hard_error_issued) {
		hard_error_issued = TRUE;
		(*pdc->ev_handler) (USB_EVENT_HARD_ERROR);
	} else {
		NOTE("Extra events ignored.\n");
	}
} /* issue_hard_error */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/ 
static void usb_write_completion (struct urb * purb) {

	UNUSED_ARG (purb);
	tasklet_schedule (&tx_tasklet);
} /* usb_write_completion */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/ 
static void usb_read_completion (struct urb * purb) {

	UNUSED_ARG (purb);
	tasklet_schedule (&rx_tasklet);
} /* usb_read_completion */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/ 
static void usb_write (card_p pdc, unsigned long len) {
	int	res;

	assert (pdc);
	if (!atomic_read (&pdc->is_open)) {
		LOG("Attempt to write on closed device.");
		return;
	}
	assert (atomic_read (&pdc->tx_pending));
	usb_fill_bulk_urb (
		pdc->tx_urb,
		pdc->dev,
		usb_sndbulkpipe (pdc->dev, pdc->epwrite->bEndpointAddress),
		pdc->tx_buffer,
		len,
		usb_write_completion,
		pdc
		);
	if (0 != (res = usb_submit_urb (pdc->tx_urb, GFP_ATOMIC))) {
		atomic_set (&pdc->tx_pending, 0);
		issue_hard_error (
			pdc, 
			"Tx URB submission failed, code %d.\n", 
			res
		);
	}
} /* usb_write */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static void usb_read (card_p pdc) {
	int	res;

	assert (pdc);
	if (!atomic_read (&pdc->is_open)) {
		LOG("Attempt to read from closed device.");
		return;
	}
	usb_fill_bulk_urb (
		pdc->rx_urb,
		pdc->dev,
		usb_rcvbulkpipe (pdc->dev, pdc->epread->bEndpointAddress),
		pdc->rx_buffer,
		MAX_TRANSFER_SIZE,
		usb_read_completion,
		pdc
		);
	if (0 != (res = usb_submit_urb (pdc->rx_urb, GFP_ATOMIC))) {
		atomic_set (&pdc->rx_pending, 0);
		issue_hard_error (
			pdc, 
			"Rx URB submission failed, code %d.\n", 
			res
		);
	}
} /* usb_read */

/*---------------------------------------------------------------------------*\
\*-T-------------------------------------------------------------------------*/
static void tx_handler (card_p pdc) {
	struct urb *	purb;

	assert (pdc != NULL);
	purb = pdc->tx_urb;
	assert (atomic_read (&pdc->tx_pending));
	atomic_set (&tx_flag, 0);
	if (!atomic_read (&pdc->is_open)) {
		atomic_set (&pdc->tx_pending, 0);
		LOG("TX: Device closed!\n");
		return;
	}
	if (0 != purb->status) {
		atomic_set (&pdc->tx_pending, 0);
		issue_hard_error (
			pdc, 
			"Tx URB status: %d\n", 
			purb->status
		);
		return;
	}
	if ((purb->actual_length > 0) 
	&& ((purb->actual_length % 64) == 0)) {
		LOG(
			"Inserting zero-length block! (Wrote %d bytes)\n",
			purb->actual_length
		);
		usb_write (pdc, 0);
	} else {
		atomic_set (&pdc->tx_pending, 0);
		usb_TxStart ();
		kick_scheduler ();
	}
} /* tx_handler */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static void tx_task (unsigned long arg) {
	card_p	pdc = capi_card;
	
	UNUSED_ARG (arg);
	if (in_critical ()) {
		atomic_set (&tx_flag, 1);
	} else if (spin_trylock (&stack_lock)) {
		tx_handler (pdc);
		spin_unlock (&stack_lock);
	} else {
		atomic_set (&tx_flag, 1);
	}
} /* tx_task */
		
/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static void rx_handler (card_p pdc) {
	struct urb *	purb;
	
	assert (pdc != NULL);
	purb = pdc->rx_urb;
	atomic_set (&pdc->rx_pending, 0);
	atomic_set (&rx_flag, 0);
	if (!atomic_read (&pdc->is_open)) {
		LOG("RX: Device closed!\n");
		return;
	}
	if (0 != purb->status) {
		issue_hard_error (
			pdc, 
			"Rx URB status: %d\n", 
			purb->status
		);
		return;
	}
	if (0 != purb->actual_length) {
		(*pdc->rx_avail) (pdc->rx_buffer, purb->actual_length);
		kick_scheduler ();
	}
	usb_RxStart ();
} /* rx_handler */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static void rx_task (unsigned long arg) {
	card_p	pdc = capi_card;
	
	UNUSED_ARG (arg);
	if (in_critical ()) {
		atomic_set (&rx_flag, 1);
	} else if (spin_trylock (&stack_lock)) {
		rx_handler (pdc);
		spin_unlock (&stack_lock);
	} else {
		atomic_set (&rx_flag, 1);
	}
} /* rx_task */

/*---------------------------------------------------------------------------*\
\*-S-------------------------------------------------------------------------*/
static __attr void __stack scheduler_control (unsigned f) {
	int	flag = (int) f;
	int	changed;

	changed = (atomic_read (&scheduler_enabled) != flag);
	if (changed) {
		atomic_set (&scheduler_enabled, flag ? 1 : 0);
		SCHED_WAKEUP;
	}
} /* scheduler_control */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static int scheduler (void * arg) {
	
	UNUSED_ARG (arg);
	daemonize (TARGET);
	snprintf (current->comm, 16, "%s_s", TARGET);
	LOG("Starting scheduler thread '%s'...\n", current->comm);
	while (atomic_read (&thread_run_flag)) {
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
		if (!atomic_read (&thread_run_flag)) {
			info (atomic_read (&thread_run_flag));
			break;
		}
		if (!atomic_read (&thread_capi_flag)) {
			info (atomic_read (&thread_capi_flag));
			continue;
		}
		if (!atomic_read (&scheduler_enabled)) {
			continue;
		}
		if (capi_lib == NULL) {
			LOG("Library lost! Hot unplug?\n");
			break;
		}
		if (spin_trylock (&stack_lock)) {
			os_timer_poll ();
			if ((*capi_lib->cm_schedule) ()) {
				scheduler_control (TRUE);
			}
			spin_unlock (&stack_lock);
		} else {
			kick_scheduler ();
		}
	}
	LOG("Scheduler thread stopped.\n");
	complete(&hotplug); /* Complete Thread Sync here <arnd.feldmueller@web.de> */
	#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24))
	up (&hotplug);
	#endif
	return 0;
} /* scheduler */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
int make_thread (void) {

	if (thread_pid != -1) {
		LOG("Thread[%d] is already running!\n", thread_pid);
		return thread_pid;
	}
	atomic_set (&thread_run_flag, 1);
	atomic_set (&thread_capi_flag, 0);
	init_waitqueue_head (&capi_wait);
	thread_pid = kernel_thread (scheduler, NULL, 0);
	LOG("Scheduler thread[%d] started.\n", thread_pid);
	return thread_pid;
} /* make_thread */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
void kill_thread (void) {

	if (thread_pid != -1) {
		atomic_set (&thread_run_flag, 0);
		if (NULL == find_task_by_pid (thread_pid)) {
			LOG("Thread[%d] has died before!\n", thread_pid);
		} else {
			if (!atomic_read (&thread_capi_flag)) {
				SCHED_WAKEUP_CAPI;
			} else {
				SCHED_WAKEUP;
			}
			LOG("Scheduler thread signalled, waiting...\n");
			wait_for_completion(&hotplug); /* Wait for complete Thread Sync <arnd.feldmueller@web.de> */
			#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24))
			down (&hotplug);
			#endif
			LOG("Scheduler thread[%d] terminated.\n", thread_pid);
		}
		thread_pid = -1;
	} else {
		LOG("No scheduler thread.\n");
	}
} /* kill_thread */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
void enable_thread (void) {

	assert (atomic_read (&thread_run_flag));
	info (!atomic_read (&thread_capi_flag));
	if (!atomic_read (&thread_capi_flag)) {
		SCHED_WAKEUP_CAPI;
		LOG("Thread enabled.\n");
	}
} /* enable_thread */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static void disable_thread (void) {

	assert (atomic_read (&thread_run_flag));
	info (atomic_read (&thread_capi_flag));
	if (atomic_read (&thread_capi_flag)) {
		atomic_set (&thread_capi_flag, 0);
		SCHED_WAKEUP;
		LOG("Thread disabled.\n");
	}
} /* disable_thread */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
void kick_scheduler (void) {

	atomic_set (&scheduler_enabled, 1);
	SCHED_WAKEUP;
} /* kick_scheduler */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/ 
static __attr void __stack wakeup_control (unsigned enable_flag) { 
	
	enable_flag = 0; 
} /* wakeup_control */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/ 
static __attr void __stack version_callback (char * vp) { 
	
	assert (nvers == 0);
	LOG("Version string #%d:\n", nvers);
	scan_version (capi_card, vp);
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
static void stop_base (void) {
	card_p	pdc = capi_card;

	if (NULL == pdc) {
		return;
	}
	atomic_set (&pdc->is_open, 0);
	atomic_set (&pdc->rx_pending, 0);
	atomic_set (&pdc->tx_pending, 0);
	atomic_set (&rx_flag, 0);
	atomic_set (&tx_flag, 0);
	LOG("USB layer stopped.\n");
} /* stop_base */
		
/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
__attr void __stack usb_Open (
	unsigned			max_buffer_size,
	get_next_tx_buffer_func_p	get_next_tx_buffer,
	event_handler_func_p		event_handler,
	new_rx_buffer_avail_func_p	new_rx_buffer_avail,
	complete_func_p			open_complete,
	unsigned			ref_data
) {
	card_p				pdc = capi_card;

	atomic_set (&pdc->is_open, 1);
	atomic_set (&pdc->tx_pending, 0);
	atomic_set (&pdc->rx_pending, 0);
	pdc->max_buf_size = max_buffer_size;
	pdc->tx_get_next  = get_next_tx_buffer;
	pdc->rx_avail     = new_rx_buffer_avail;
	pdc->ev_handler   = event_handler;
	hard_error_issued = FALSE;
	assert (open_complete);
	(*open_complete) (0, ref_data);
	usb_RxStart ();
} /* usb_Open */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
__attr void __stack usb_Close (
	complete_func_p	close_complete,
	unsigned	ref_data
) {

	stop_base ();
	assert (close_complete);
	(*close_complete) (0, ref_data);
	assert (capi_card != NULL);
	if (hard_error_issued && capi_card->running) {
		reset (capi_card->dev);
		LOG("Closing due to hard error!\n");
		start_closing_worker (NULL, NULL);
	}
} /* usb_Close */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static void usb_RxStart (void) {
	card_p	pdc;

	pdc = capi_card;
	assert (pdc != NULL);
	assert (0 == atomic_read (&pdc->rx_pending));
	atomic_set (&pdc->rx_pending, 1);
	usb_read (pdc);
} /* usb_RxStart */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
__attr void __stack usb_TxStart (void) {
	card_p		pdc = capi_card;
	unsigned long	len;

	assert (pdc != NULL);
	if (atomic_xchg (&pdc->tx_pending, 1)) {
		return;
	}
	assert (pdc->tx_get_next);
	len = (*pdc->tx_get_next) (pdc->tx_buffer);
	assert (pdc->max_buf_size);
	assert (len <= pdc->max_buf_size);
	if (0 == len) {
		atomic_set (&pdc->tx_pending, 0);
		return;
	}
	info (atomic_read (&pdc->is_open));
	if (atomic_read (&pdc->is_open)) {
		assert (atomic_read (&pdc->tx_pending));
		usb_write (pdc, len);
	}
} /* usb_TxStart */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
__attr void __stack usb_GetDeviceInfo (device_info_p device_info) {

	assert (device_info);
	device_info->product_id = capi_card->dev->descriptor.idProduct;
	device_info->release_number = capi_card->dev->descriptor.bcdDevice;
	device_info->self_powered = 0;
} /* usb_GetDeviceInfo */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/

