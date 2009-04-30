/* 
 * driver.c
 * Copyright (C) 2005, AVM GmbH. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/workqueue.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/usb.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <asm/atomic.h>
#include "libdefs.h"
#include "driver.h"
#include "tools.h"
#include "lib.h"
#include "buffers.h"
#include "common.h"
#include "wext.h"


/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
card_p				wlan_card		= NULL;
#ifndef __WITHOUT_INTERFACE__	
lib_callback_p			wlan_lib		= NULL;
#endif
unsigned char			scsi_stop_unit[31] 	= { 
	0x55, 0x53, 0x42, 0x43, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x06, 0x1B,	0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static int usb_init(card_p);
static void usb_deinit (card_p pdc);
static int stack_init (card_p);
static void stack_deinit (card_p pdc);
static int net_init (card_p pdc);
static void net_deinit (card_p pdc);

int net_open (struct net_device *dev);
int net_stop (struct net_device *dev);
int net_tx (struct sk_buff *skb, struct net_device *dev);
struct net_device_stats *net_stats (struct net_device *dev);

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19))
static void usb_cmd_complete (struct urb *purb, struct pt_regs *ptr);
static void usb_read_complete (struct urb *purb, struct pt_regs *ptr);
static void usb_write_complete (struct urb *purb, struct pt_regs *ptr);
#else
static void usb_cmd_complete (struct urb *purb);
static void usb_read_complete (struct urb *purb);
static void usb_write_complete (struct urb *purb);
#endif

#ifdef USE_TASKLET
static void rx_task (unsigned long data);
static void tx_task (unsigned long data);
static void cmd_task (unsigned long data);
#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20))
static void recovery_worker (void*);
#else
static void recovery_worker (struct work_struct*);
#endif

static struct work_struct recovery_work;
static atomic_t recovery_started = ATOMIC_INIT (0);
struct recovery {
	card_p	pdc;
	int (__attr2 *routine) (void*, void*);
	void	*handle;
} recover;

#ifdef USE_TASKLET
static DECLARE_TASKLET(rx_tasklet, rx_task, 0);
static DECLARE_TASKLET(tx_tasklet, tx_task, 0);
static DECLARE_TASKLET(cmd_tasklet, cmd_task, 0);
#endif

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static int make_ctx_with_buf (void ** pctx) {
	urb_context_p	uctx;
	
	assert (pctx != NULL);
	info (*pctx == NULL);
	if (NULL == (uctx = (urb_context_p) hcalloc (sizeof (urb_context_t)))) {
		ERROR("Error: Could not allocate transfer context!\n");
		goto make_ctx_with_buf_out;
	}
	lib_memset(&uctx->caller_extension, 0, sizeof (usb_caller_extension_t));
	lib_memset(&uctx->caller_error, 0, sizeof (usb_caller_extension_t));
	lib_memset(&uctx->reg, 0, sizeof (struct usb_ctrlrequest));
	uctx->adapter = NULL;
	if ((uctx->buffer = (unsigned char*) hcalloc 
			    (sizeof (unsigned char) * MAX_TRANSFER_SIZE)) == NULL) {
		ERROR("Allocating buffer for URB failed!\n");
		hfree (uctx); 
		uctx = NULL;
	}
	atomic_set (&uctx->ref, 0);
	*pctx = (void *) uctx;
make_ctx_with_buf_out:
	return (uctx != NULL);
} /* make_ctx_with_buf */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static int kill_ctx_with_buf (void ** pctx) {
	urb_context_p	uctx;

	assert (pctx != NULL);
	assert (*pctx != NULL);
	uctx = (urb_context_p) *pctx;
	assert (uctx->buffer != NULL);
	hfree (uctx->buffer);
	hfree (uctx);
	*pctx = NULL;
	return TRUE;
} /* kill_ctx */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
int add_card (card_p pdc) {

	int res;	
	
	pdc->system_lock = os_protect_create (pdc);
	pdc->start_scan = 0;
	atomic_set (&pdc->shutdown, 0);
	
	if ((res = usb_init(pdc)) < 0) {
		ERROR ("usb_init failed!\n");
ADD_CARD_EXIT_NO_STACK:	
		os_protect_destroy (pdc, pdc->system_lock);
		return res;
	}

	if ((res = stack_init (pdc)) < 0) {
		ERROR ("stack_init failed!\n");
ADD_CARD_EXIT_NO_NET:
		usb_deinit (pdc);
		goto ADD_CARD_EXIT_NO_STACK;
		return res;
	}

	if ((res = net_init (pdc)) < 0) {
		ERROR ("net_init failed!\n");
		stack_deinit (pdc);
		goto ADD_CARD_EXIT_NO_NET;
	}
	
	return res;
} /* add_card */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static int usb_init(card_p pdc) {

	if ((pdc->cmd_pool = init_urbs (MAX_URB_COUNT_CMD, MAX_TRANSFER_SIZE))
		       	== NULL ) {
		ERROR("Could not allocate cmd URB pool!\n");
NO_CMD_POOL:
		return -ENOMEM;
	}
	
	if ((pdc->tx_pool = init_urbs (MAX_URB_COUNT_TX, MAX_TRANSFER_SIZE)) 
			== NULL ) {
		ERROR("Could not allocate tx URB pool!\n");
NO_TX_POOL:
		remove_urbs (&pdc->cmd_pool, kill_ctx_with_buf);
		goto NO_CMD_POOL;
	}

	if ((pdc->rx_pool = init_urbs (MAX_URB_COUNT_RX, MAX_TRANSFER_SIZE)) 
			== NULL ) {
		ERROR("Could not allocate rx URB pool!\n");
NO_RX_POOL:
		remove_urbs (&pdc->tx_pool, kill_ctx_with_buf);
		goto NO_TX_POOL;
	}

	/*FIXME: Buffer size of cmd URBs*/	
	if (build_urb_contexts (pdc->cmd_pool, make_ctx_with_buf, kill_ctx_with_buf)
	&&  build_urb_contexts (pdc->tx_pool, make_ctx_with_buf, kill_ctx_with_buf)
	&&  build_urb_contexts (pdc->rx_pool, make_ctx_with_buf, kill_ctx_with_buf)) {
		LOG("URB pool and context has been allocated!\n");
	} else {
NO_CONTEXTS:
		remove_urbs (&pdc->rx_pool, kill_ctx_with_buf);
		goto NO_RX_POOL;
	}

	if (usb_reset_device (pdc->usb_dev) < 0) {
		NOTE("Firmware still loaded.\n");
		goto NO_CONTEXTS;
	}
	
	if (usb_clear_halt (pdc->usb_dev, usb_sndbulkpipe
				(pdc->usb_dev, pdc->epwrite->bEndpointAddress))) {
		LOG("Could not reset halt flag for endpoint %x!\n",
				pdc->epwrite->bEndpointAddress);
		LOG("(Device may not work properly.)\n");
	}
	if (usb_clear_halt (pdc->usb_dev, usb_rcvbulkpipe
				(pdc->usb_dev, pdc->epread->bEndpointAddress))) {
		LOG("Could not reset halt flag for endpoint %x!\n",
				pdc->epread->bEndpointAddress);
		LOG("(Device may not work properly.)\n");
	}

	return 0;
}

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static int stack_init (card_p pdc) {

#ifndef __WITHOUT_INTERFACE__	
	if ((pdc->stack_ctx = 
		(*wlan_lib->config_mgr_create) ((void*)pdc)) == NULL) {
#else
	if ((void*)stack_config_mgr_create ((void*)pdc) == NULL) {
#endif
		ERROR("stack_init: Creating of config manager failed.\n");
STACK_INIT_NO_MGR_START:
		return -EIO;	
	}

	usb_start_read (pdc);
	
#ifndef __WITHOUT_INTERFACE__	
	if ((*wlan_lib->config_mgr_start) (pdc->stack_ctx) == STACK_ERROR) {
#else
	if (stack_config_mgr_start (pdc->stack_ctx) == STACK_ERROR) {
#endif
		ERROR("stack_init: Starting of config manager failed.\n");
STACK_INIT_NO_INTERRUPTS:
#ifndef __WITHOUT_INTERFACE__
		(*wlan_lib->config_mgr_unload) (pdc->stack_ctx);
#else
		stack_config_mgr_unload (pdc->stack_ctx);
#endif
		goto STACK_INIT_NO_MGR_START;
	}
	
	NOTE("Config manager successully created and started.\n");

#ifndef __WITHOUT_INTERFACE__	
	if ((*wlan_lib->config_mgr_enable_interrupts) (pdc->stack_ctx) != STACK_OK) {
#else
	if (stack_config_mgr_enable_interrupts (pdc->stack_ctx) != STACK_OK) {
#endif
		ERROR("stack_init: Could not enable interrupts.\n");
#ifndef __WITHOUT_INTERFACE__
		(*wlan_lib->config_mgr_stop) (pdc->stack_ctx);
#else	
		stack_config_mgr_stop (pdc->stack_ctx);
#endif
		goto STACK_INIT_NO_INTERRUPTS;
	}

#ifndef __WITHOUT_INTERFACE__	
	if ((*wlan_lib->register_events) (pdc->stack_ctx) != STACK_OK) {
#else
	if (stack_register_events (pdc->stack_ctx) != STACK_OK) {
#endif
		NOTE("stack_init: Could not register events. \
			(Driver may not work probably.)\n");
	}

#ifndef __WITHOUT_INTERFACE__	
	if ((*wlan_lib->config_mgr_set_param) (pdc->stack_ctx, NULL) != STACK_OK) {
#else
	if (stack_config_set_param (pdc->stack_ctx, NULL) != STACK_OK) {
#endif
	}
	
	return 0;
}

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static int net_init (card_p pdc) {
	
	struct net_device *dev = NULL;
	int res;
	struct sockaddr mac;

	if (NULL == (dev = alloc_netdev (0, "wlan%d", ether_setup))) {
		ERROR("Could not allocate network device.\n");
		return -ENOMEM;
	}

	dev->priv		= pdc;
	dev->open		= net_open;
	dev->stop		= net_stop;
	dev->hard_start_xmit	= net_tx;
	dev->get_stats		= net_stats;

	fwlanusb_get_hw_mac (dev, NULL, NULL, (char*) &mac);
	lib_memcpy (dev->dev_addr, mac.sa_data, 6);	
	
	dev->wireless_handlers  = &fwlanusb_handler_def;

	pdc->net_dev = dev;
	SET_NETDEV_DEV(dev, &pdc->usb_dev->dev);
	
	if ((res = register_netdev (dev)) < 0) {
		ERROR("Could not register network device.\n");
	}

	return res;
}

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static void stack_deinit (card_p pdc) {

#ifndef __WITHOUT_INTERFACE__
	(*wlan_lib->config_mgr_stop) (pdc->stack_ctx);
	(*wlan_lib->config_mgr_unload) (pdc->stack_ctx);
#else	
	stack_config_mgr_stop (pdc->stack_ctx);
	stack_config_mgr_unload (pdc->stack_ctx);
#endif
}

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static void usb_deinit (card_p pdc) {

	unlink_urbs (pdc->cmd_pool, 0);
	unlink_urbs (pdc->rx_pool, 0);
	unlink_urbs (pdc->tx_pool, 0);
	remove_urbs (&pdc->cmd_pool, kill_ctx_with_buf);
	remove_urbs (&pdc->rx_pool, kill_ctx_with_buf);
	remove_urbs (&pdc->tx_pool, kill_ctx_with_buf); 
}

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static void net_deinit (card_p pdc) {

	struct net_device *dev = pdc->net_dev;
	
	unregister_netdev (dev);
}

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
void delete_card (card_p pdc) {

	atomic_set (&pdc->shutdown, 1);
	net_deinit (pdc);
	stack_deinit (pdc);
	usb_deinit (pdc);
	os_protect_destroy (pdc, pdc->system_lock);
}

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
int net_open (struct net_device *dev) {

	netif_start_queue (dev);
	
	return 0;
}

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
int net_stop (struct net_device *dev) {

	netif_stop_queue (dev);
	
	return 0;
}

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
int net_rx (struct net_device *dev, void *p_packet, unsigned short length) {

	struct sk_buff *skb;
	card_p pdc = (card_p) dev->priv;
	unsigned char buffer[4096];

	(*wlan_lib->msdu_2_skb) (p_packet, buffer, length);

	skb = dev_alloc_skb (length);
	if (!skb) {
		LOG("net_rx: low on mem - packet dropped\n");
		(*wlan_lib->config_mgr_mem_mngr_free_msdu) (pdc->stack_ctx, p_packet);
		(pdc->net_stats.rx_dropped)++;
		return STACK_FALSE;
	}

	lib_memcpy (skb_put (skb, length), buffer, length);
	skb->dev = dev;
	skb->protocol = eth_type_trans (skb, dev);
	skb->ip_summed = CHECKSUM_UNNECESSARY;

	(pdc->net_stats.rx_packets)++;
	(pdc->net_stats.rx_bytes) += length;

	netif_rx (skb);
	(*wlan_lib->config_mgr_mem_mngr_free_msdu) (pdc->stack_ctx, p_packet);
	
	return STACK_TRUE;
}

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
int net_tx (struct sk_buff *skb, struct net_device *dev) {

	card_p pdc = (card_p) dev->priv;

	assert(skb_shinfo (skb)->nr_frags == 0);
	
	dev->trans_start = jiffies;

	(pdc->net_stats.tx_packets)++;
	(pdc->net_stats.tx_bytes) += skb->len;

	if ((*wlan_lib->send_packet) (pdc->stack_ctx, skb->data, skb->len)) {
		(pdc->net_stats.tx_errors)++;
	}

	dev_kfree_skb (skb);
	
	return 0;
}

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
struct net_device_stats *net_stats (struct net_device *dev) {

	card_p pdc = (card_p) dev->priv;

	return &pdc->net_stats;
}

/*--U------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19))
static void usb_write_complete (struct urb *purb1, struct pt_regs *ptr) {
#else
static void usb_write_complete (struct urb *purb1) {
#endif

#ifndef USE_TASKLET
	struct urb	*purb;
	void 		*vptr;
	urb_context_p	pctx;
#endif

#ifdef LOG_USB	
	LOG("usb_write_complete: TX URB %p completed.\n", purb1);
#endif
	enlist_urb (wlan_card->tx_pool, purb1);

#ifdef USE_TASKLET
	tasklet_schedule (&tx_tasklet);

}

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static void tx_task (unsigned long data) {

	struct urb	*purb;
	void 		*vptr;
	urb_context_p	pctx;
#endif
	
	unlist_urb (wlan_card->tx_pool, &purb, &(vptr));
#ifdef LOG_USB
	LOG("tx_task: Processing TX URB %p.\n", purb);
#endif
	
	if (purb->status != 0) {
		ERROR("TX URB %p: status %d\n", purb, purb->status);
		pctx = (urb_context_p)vptr;
		if (pctx->caller_error.caller_completion_routine != NULL) {
			(pctx->caller_error.caller_completion_routine) 	
				(pctx->caller_error.param1,
			 	 pctx->caller_error.param2, 
		 	 	 pctx->caller_error.param3);
		}
	}
	
	release_urb (wlan_card->tx_pool, purb);

}

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19))
static void usb_read_complete (struct urb *purb1, struct pt_regs *ptr) {
#else
static void usb_read_complete (struct urb *purb1) {
#endif

#ifndef USE_TASKLET
	struct urb	*purb;
	void 		*vptr;
#endif

#ifdef LOG_USB	
	LOG("usb_read_complete: RX URB %p completed.\n", purb1);
#endif
	enlist_urb (wlan_card->rx_pool, purb1);

#ifdef USE_TASKLET
	tasklet_schedule (&rx_tasklet);
}

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static void rx_task (unsigned long data) {

	struct urb	*purb;
	void 		*vptr;
#endif
	
	unlist_urb (wlan_card->rx_pool, &purb, &(vptr));
#ifdef LOG_USB
	LOG("rx_task: Processing RX URB %p.\n", purb);
#endif

	if (purb->status != 0) {
		ERROR("RX URB %p: status %d\n", purb, purb->status);
		release_urb (wlan_card->rx_pool, purb);
		usb_start_read (wlan_card);
		return;
	}
	
	
	enlist_urb_for_data (wlan_card->rx_pool, purb);

	if (!atomic_read(&recovery_started))
		(*wlan_lib->config_mgr_handle_interrupts) (wlan_card->stack_ctx);
	
}

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
void usb_start_read (card_p pdc) {
	
	struct urb	*purb;
	void		*ptr;
	urb_context_p	pctx;
	int 		count, res;

#ifdef LOG_USB
	LOG("usb_start_read called.\n");
#endif

	if (atomic_read (&pdc->shutdown))
		return;
	
	for (count = 0; count < MAX_URB_COUNT_RX; count++) {
		if (!claim_urb (pdc->rx_pool, &purb, &ptr)) {
			//LOG ("No read URB/buffer available.\n");
			return;
		}
		
		pctx = (urb_context_p)ptr;
		
		usb_fill_bulk_urb (	purb, 
					pdc->usb_dev, 
					usb_rcvbulkpipe 
						(pdc->usb_dev, pdc->epread->bEndpointAddress),
					(void*)pctx->buffer,
					MAX_TRANSFER_SIZE,
					usb_read_complete,
					purb->context	);

		if ((res = usb_submit_urb (purb, GFP_ATOMIC)) < 0) {
			ERROR("Submitting read URB failed (%d)!\n", res);
		}
#ifdef LOG_USB
		LOG("usb_start_read: Submitted URB %p\n", purb);
#endif
	}
}


/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19))
static void usb_cmd_complete (struct urb *purb1, struct pt_regs *ptr) {
#else
static void usb_cmd_complete (struct urb *purb1) {
#endif

#ifndef USE_TASKLET
	struct urb	*purb;
	void 		*vptr;
	struct urb	*pnew_urb;
	urb_context_p 	pctx;
	int		res;
#endif

#ifdef LOG_USB	
	LOG ("usb_cmd_complete: USB cmd complete for URB: %p\n", purb1);
#endif
	
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19))
	UNUSED_ARG (ptr);
#endif
	assert (purb1 != NULL);

	enlist_urb (wlan_card->cmd_pool, purb1);

#ifdef USE_TASKLET
	tasklet_schedule (&cmd_tasklet);
}

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static void cmd_task (unsigned long data) {

	struct urb	*purb;
	void 		*vptr;
	struct urb	*pnew_urb;
	urb_context_p 	pctx;
	int		res;
#endif
	
	unlist_urb (wlan_card->cmd_pool, &purb, &(vptr));
#ifdef LOG_USB
	LOG("cmd_task: Processing cmd URB %p.\n", purb);
#endif

	pctx = (urb_context_p)vptr;
	
	/*FIXME: issue hard error*/	
	if (0 != purb->status) {
		ERROR("Cmd URB status: %d\n", purb->status);
		goto USB_CMD_COMPLETE_END;
	}

	if (pctx->caller_extension.caller_completion_routine != NULL) {
		(pctx->caller_extension.caller_completion_routine) 	
			(pctx->caller_extension.param1,
			 pctx->caller_extension.param2, 
		 	 pctx->caller_extension.param3);
	}

	/*FIXME: issue hard error*/	
	if (need_start_serialized_urb (wlan_card->cmd_pool, &pnew_urb)) {
#ifdef LOG_USB
		LOG("usb_cmd_complete: Need to submit waiting cmd URB %p.\n", pnew_urb);
#endif
		if ((res = usb_submit_urb (pnew_urb, GFP_ATOMIC)) < 0) {
			ERROR ("Submitting cmd URB failed (%d)!\n", res);
		} else {
#ifdef LOG_USB
			LOG("usb_cmd_complete: Cmd URB: %p submitted\n", pnew_urb);
#endif
		}
	}

USB_CMD_COMPLETE_END:
	release_urb (wlan_card->cmd_pool, purb);
	//LOG("Cmd task\n");
}

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
unsigned int usb_read_write_sync (
		struct usb_device *dev, unsigned int pipe, unsigned char *data, 
		int len, int *actual_length, int timeout) {

	int res = -100;

	if (usb_pipebulk (pipe)) {
		/*FIXME - Why does usb_bulk_msg only work with local buffer?*/
		unsigned char *local_data = 
			(unsigned char*)hmalloc(sizeof(unsigned char) * len);
		if (!local_data) {
			ERROR ("usb_read_write_sync: Out of memory!\n");
			return STACK_USB_ERROR;
		}
		if (usb_pipeout (pipe))
			lib_memcpy (local_data, data, len);
		res = usb_bulk_msg (dev, pipe, local_data, len, actual_length, timeout);
		if (usb_pipein (pipe))
			lib_memcpy (data, local_data, *actual_length);
		hfree (local_data);
	} else {
		if (usb_pipecontrol (pipe)) {
			__u8 request = VENDOR_SPECIFIC_REQUEST; 
			__u8 requesttype = USB_TYPE_VENDOR | USB_RECIP_DEVICE;
			__u16 size = (__u16)len;
		       	if (usb_pipeout(pipe))
				requesttype |= USB_DIR_OUT;
			else
				requesttype |= USB_DIR_IN;
			*actual_length = usb_control_msg (
					dev, pipe, request, requesttype, 0, 0,
					data, size, timeout);
			if (*actual_length < 0)
				res = *actual_length;
			else
				res = STACK_USB_OK;
		} else {
			ERROR ("usb_read_write_sync: Unknown pipe!\n");
		}
	}

	if (res < 0) {
		ERROR ("usb_read_write_sync: usb_bulk/control_msg = %d\n", res);
		res = STACK_USB_ERROR;
	}

	return (unsigned int)res;
}

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
unsigned int usb_read_write_cmd_async (
		struct usb_device *dev, unsigned int pipe, unsigned char *data, 
		int len, usb_caller_extension_p caller_ext) {

	int res = -100;

	struct urb		*purb;
	void			*ptr;
	urb_context_p		pctx;
	
	if (!claim_urb (wlan_card->cmd_pool, &purb, &ptr)) {
		LOG ("No cmd URB/buffer available.\n");
		return STACK_USB_ERROR;
	}

#ifdef LOG_USB
	LOG("usb_read_write_cmd_async: Async cmd %s; URB: %p\n",
			usb_pipeout(pipe)?"write":"read", purb);
#endif

	pctx = (urb_context_p)ptr;
	if (caller_ext) {
		lib_memcpy(&pctx->caller_extension, caller_ext, sizeof (usb_caller_extension_t));
	} else {
		pctx->caller_extension.caller_completion_routine = NULL;
	}
	pctx->caller_error.caller_completion_routine = NULL;

	pctx->reg.bRequestType = USB_TYPE_VENDOR | USB_RECIP_DEVICE;
	if (usb_pipeout(pipe)) {
		pctx->reg.bRequestType |= USB_DIR_OUT;
		lib_memcpy (pctx->buffer, data, len);
		data = pctx->buffer;
	}
	else { 
		pctx->reg.bRequestType |= USB_DIR_IN;
	}
	pctx->reg.bRequest = VENDOR_SPECIFIC_REQUEST;
	pctx->reg.wValue = 0;
	pctx->reg.wIndex = 0;
	pctx->reg.wLength = cpu_to_le16 ((__u16)len);
	
	usb_fill_control_urb (purb, dev, pipe, (void*)&pctx->reg, data,
		len, usb_cmd_complete, purb->context);

	if (need_serialize_wait_urb (wlan_card->cmd_pool, purb)) {
#ifdef LOG_USB
		LOG("usb_read_write_cmd_async: Need serialize cmd URB %p.\n", purb);
#endif
		return STACK_USB_OK;
	}

	if ((res = usb_submit_urb (purb, GFP_ATOMIC)) < 0) {
		ERROR ("Submitting cmd URB failed (%d)!\n", res);
		res = STACK_USB_ERROR;
	} else {
#ifdef LOG_USB
		LOG("usb_read_write_cmd_async: Submitting cmd URB %p.\n", purb);
#endif	
	}
	
	return (unsigned int)res;
}

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
unsigned int usb_write_async (
	struct usb_device *dev, unsigned int pipe, void *msdu, unsigned short ep1, 
	usb_caller_extension_p caller_ext, usb_caller_extension_p caller_err) {
	
	struct urb		*purb;
	void			*ptr;
	urb_context_p		pctx;
	int			len, res = STACK_USB_ERROR;

	if (!claim_urb (wlan_card->tx_pool, &purb, &ptr)) {
		ERROR ("No Tx URB/buffer available.\n");
		return STACK_USB_ERROR;
	}

	pctx = (urb_context_p)ptr;
	
#ifndef __WITHOUT_INTERFACE__	
	len = (*wlan_lib->msdu_2_buffer) (ep1, MAX_TRANSFER_SIZE, msdu, pctx->buffer);
#else
	len = stack_msdu_2_buffer (ep1, MAX_TRANSFER_SIZE, msdu, pctx->buffer);
#endif
	if (caller_err) {
		lib_memcpy(&pctx->caller_error, caller_err, sizeof (usb_caller_extension_t));
	} else {
		pctx->caller_error.caller_completion_routine = NULL;
	}
	pctx->caller_extension.caller_completion_routine = NULL;

	usb_fill_bulk_urb (	purb, dev, pipe, (void*)pctx->buffer,
				len, usb_write_complete, purb->context);

	purb->transfer_flags |= URB_ZERO_PACKET;

	if ((res = usb_submit_urb (purb, GFP_ATOMIC)) < 0) {
		ERROR ("Submitting tx URB failed (%d)!\n", res);
		release_urb (wlan_card->tx_pool, purb);
		res = STACK_USB_ERROR;
	} else {
#ifdef LOG_USB
		LOG("usb_read_write_async: Submitting tx URB %p.\n", purb);
#endif	
	}

	return (unsigned int)res;
}

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20))
static void recovery_worker (void *param) {
#else
static void recovery_worker (struct work_struct *work) {
#endif

	card_p pdc = recover.pdc;
	int done, count = 0;

	assert(pdc != NULL);
	
	LOG("Recovery worker started.\n");
	
	for (;;) {
		done = 1;
		if (POOL_GET_TODO(pdc->cmd_pool))
			done = 0;
		if (POOL_GET_TODO(pdc->rx_pool))
			done = 0;
		if (POOL_GET_TODO(pdc->tx_pool))
			done = 0;
		if (done) break;
		if (!(++count % 100)) break;
		set_current_state (TASK_INTERRUPTIBLE);
		schedule_timeout (msecs_to_jiffies (1000));
	}

	os_protect_lock (wlan_card, wlan_card->system_lock);
	recover.routine (recover.handle, NULL);

	while (POOL_GET_DATA(pdc->rx_pool))
		(*wlan_lib->config_mgr_handle_interrupts) (pdc->stack_ctx);

	atomic_set (&recovery_started, 0);
	usb_start_read (pdc);
	os_protect_unlock (wlan_card, wlan_card->system_lock);

}

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
int start_recovery_worker (
	void* p_adapter,
	int (__attr2 *recovery_worker_routine) (void*, void*),
	void* handle) {

	card_p pdc;
	struct urb *purb;
	
	assert (p_adapter != NULL);

	pdc = (card_p)p_adapter;
	
	if (atomic_read (&recovery_started)) 
		return STACK_FALSE;
	
	atomic_set (&recovery_started, 1);

	while (need_start_serialized_urb (pdc->cmd_pool, &purb)) {
		release_urb (pdc->cmd_pool, purb);
	}
	
	unlink_urbs (pdc->cmd_pool, 1);
	unlink_urbs (pdc->rx_pool, 1);
	unlink_urbs (pdc->tx_pool, 1);
	
	recover.pdc = pdc;
	recover.routine = recovery_worker_routine;
	recover.handle = handle;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20))
	INIT_WORK (&recovery_work, recovery_worker, NULL);
#else
	INIT_WORK (&recovery_work, recovery_worker);
#endif
	LOG("Scheduling recovery worker ...\n");
	schedule_work (&recovery_work);
	
	return STACK_TRUE;
}

