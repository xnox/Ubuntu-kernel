/* 
 * lib.c
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

#include <asm/atomic.h>
#include <asm-generic/bug.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/delay.h>
#include <linux/usb.h>
#include <linux/percpu.h>
#include "attr.h"
#include "tools.h"
#include "libstub.h"
#include "lib.h"
#include "defs.h"
#include "driver.h"
#include "common.h"
#include "wext.h"

#define	PRINTF_BUFFER_SIZE	1024

#if 0
# define LOG_INTERFACE
#endif

/*---------------------------------------------------------------------------*\
 * 
 * Interface from stack
 *
\*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*\
 * Talk to me
\*---------------------------------------------------------------------------*/
#if !defined (NDEBUG)
static int	nl_needed = 1;
#endif

__attr void report (char *fmt, va_list args) {

#if !defined (NDEBUG)
	char	buffer[PRINTF_BUFFER_SIZE];
	char *	bufptr = buffer;
	int	count;

	if (nl_needed) {
		nl_needed = 0;
		printk ("\n");
	}	
	count = vsnprintf (bufptr, sizeof (buffer), fmt, args);
	if ('\n' == buffer[0]) {
		bufptr++;
	}
	if ('\n' != buffer[count - 1]) {
		assert (count < (int) (sizeof (buffer) - 2));
		buffer[count++] = '\n';
		buffer[count]   = (char) 0;
	}
	NOTE(bufptr);
#else
	fmt = fmt;
	args = args;
#endif

}

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
__attr void os_debug_printf (char *fmt, ...) {
	va_list	args;
	char	buffer [PRINTF_BUFFER_SIZE];

	va_start (args, fmt);
	vsnprintf (buffer, sizeof (buffer), fmt, args);
	NOTE(buffer);
	va_end (args);
}

/*---------------------------------------------------------------------------*\
 * Memory functions
\*---------------------------------------------------------------------------*/
__attr void *os_memory_alloc (void *os_context, unsigned int size) {

#ifdef LOG_INTERFACE
	LOG("os_memory_alloc called.\n");
#endif
	return hmalloc (size);
}
	
/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
__attr void *os_memory_calloc (
		void *os_context, 
		unsigned int number, 
		unsigned int size) {

#ifdef LOG_INTERFACE
	LOG("os_memory_calloc called.\n");
#endif
	return hcalloc (number * size);
}
	
/*static __attr void *os_memory_zero (
		void *os_context, 
		void *p_mem_ptr, 
		unsigned int length) {

	LOG("os_memory_calloc called.\n");
	return NULL;
}*/
	
/*static __attr void os_memory_copy (
		void *p_os_context, 
		void *p_destination, 
		void *p_source, 
		unsigned int size) {

	LOG("os_memory_copy called.\n");
}*/
	
/*static __attr void os_memory_move (
		void *p_os_context, 
		void *p_destination, 
		void *p_source, 
		unsigned int size) {

	LOG("os_memory_copy called.\n");
}*/
	
/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
__attr void os_memory_free (
		void *p_os_context, 
		void *p_mem_ptr, 
		unsigned int size) {

#ifdef LOG_INTERFACE
	LOG("os_memory_free called.\n");
#endif

	hfree(p_mem_ptr);
}
	
/*attr int os_memory_compare (
		void *os_context, 
		unsigned char *buf1, 
		unsigned char *buf2, 
		int count) {

	LOGe called.\n");
	return 0;
}*/

/*---------------------------------------------------------------------------*\
 * Timer functions and data structures
\*---------------------------------------------------------------------------*/
typedef struct {
	card_p pdc;
	spinlock_t lock;
	struct timer_list *timer;
	void (__attr2 *routine) (void*);
	void *timer_context;
	int periodic;
	unsigned int delay_jiffies;
} timer_data_t, *timer_data_p;

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static void generic_timer_function (unsigned long timer_handle) {

	timer_data_p timer_data  = (timer_data_p)timer_handle;
	struct timer_list *timer = timer_data->timer;
	unsigned long flags;

#ifdef LOG_INTERFACE
	LOG("Timer expired (%p).\n", timer_data);
#endif
	
	if ((atomic_read (&timer_data->pdc->shutdown)) || 
		(*wlan_lib->get_built_in_test_status_param) (wlan_card->stack_ctx)) {
		LOG("Timer callback interruption.\n");
		return;
	} else {
		os_protect_lock (wlan_card, wlan_card->system_lock);
		(*timer_data->routine) (timer_data->timer_context);
		os_protect_unlock (wlan_card, wlan_card->system_lock);	
	}
	
	if (timer_data->periodic) {
		timer->expires = jiffies + timer_data->delay_jiffies;
		spin_lock_irqsave (&timer_data->lock, flags);
	       	if (timer_pending (timer)) {
			assert(0);
			ERROR("Active timer in own timer function!\n");
			ERROR("(Perhaps callback started periodic timer?)\n");
		} else {
			add_timer (timer);
		}
		spin_unlock_irqrestore (&timer_data->lock, flags);
	}
}

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
__attr void *os_timer_create (
		void *os_context, 
		void (__attr2 *p_routine) (void*), 
		void *context) {

	timer_data_p timer_data	 = (timer_data_p)hmalloc(sizeof(timer_data_t));
	struct timer_list *timer = (struct timer_list*) hmalloc(sizeof(struct timer_list));

#ifdef LOG_INTERFACE
	LOG("os_timer_create called (%p).\n", timer_data);
#endif
	
	if (!timer_data || !timer)
		return NULL;

	timer_data->pdc = (card_p)os_context;
	spin_lock_init (&timer_data->lock);
	timer_data->timer = timer;
	timer_data->routine = p_routine;
	timer_data->timer_context = context;
	
	init_timer (timer);
	timer->function = &generic_timer_function;
	timer->data = (unsigned long)timer_data;
	
	return (void*)timer_data;
}
	
/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
__attr void os_timer_destroy (void *os_context, void *timer_handle) {

#ifdef LOG_INTERFACE
	LOG("os_timer_destroy called (%p).\n", timer_handle);
#endif

	timer_data_p timer_data  = (timer_data_p)timer_handle;
	struct timer_list *timer = timer_data->timer;
	
	if (timer_pending (timer))
		del_timer_sync (timer);

	hfree(timer);
	hfree(timer_data);
}
	
/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
__attr void os_timer_start (
		void *os_context, 
		void *timer_handle, 
		unsigned int delay_ms, 
		int b_periodic) {

#ifdef LOG_INTERFACE
	LOG("os_timer_start called (%p).\n", timer_handle);
#endif
	
	timer_data_p timer_data  = (timer_data_p)timer_handle;
	struct timer_list *timer = timer_data->timer;
	unsigned long flags;

	if ((atomic_read (&timer_data->pdc->shutdown)) || 
		(*wlan_lib->get_built_in_test_status_param) (wlan_card->stack_ctx)) {
		LOG("Start of timer interrupted.\n");
		return;
	}

	timer_data->periodic = b_periodic;
	timer_data->delay_jiffies = (MSEC2JIFF(delay_ms));

	spin_lock_irqsave (&timer_data->lock, flags);
	if (timer_pending (timer)) {
		info(0);
		LOG("Trying to start active timer!\n");
	} else {
		timer->expires = jiffies + timer_data->delay_jiffies;
		add_timer (timer);
	}
	spin_unlock_irqrestore (&timer_data->lock, flags);
}

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
__attr void os_timer_stop (void *os_context, void *timer_handle) {

#ifdef LOG_INTERFACE
	LOG("os_timer_stop called (%p).\n", timer_handle);
#endif

	timer_data_p timer_data  = (timer_data_p)timer_handle;
	struct timer_list *timer = timer_data->timer;

	if (timer_pending (timer))
		del_timer_sync (timer);
}

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
__attr unsigned int os_time_stamp_ms (void *os_context) {

#ifdef LOG_INTERFACE
	LOG("os_time_stamp_ms called.\n");
#endif
	return (JIFF2MSEC((unsigned int) jiffies_64));
}
	
/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
__attr unsigned int os_time_stamp_us (void *os_context) {

#ifdef LOG_INTERFACE
	LOG("os_time_stamp_us called.\n");
#endif
	return 0;
}

/*---------------------------------------------------------------------------*\
 * Protection services
\*---------------------------------------------------------------------------*/
__attr void *os_atomic_create (void *os_context, int value) {

	atomic_t *atomic;

#ifdef LOG_INTERFACE
	LOG("os_atomic_init called.\n");
#endif

	atomic = (atomic_t*) hmalloc (sizeof (atomic_t));

	if (atomic == NULL)
		return NULL;

	atomic_set (atomic, value);

	return (void*)atomic;
}

__attr void os_atomic_destroy (void *os_context, void *atomic_context) {

	atomic_t *atomic;

#ifdef LOG_INTERFACE
	LOG("os_atomic_destroy called.\n");
#endif

	atomic = (atomic_t*) atomic_context;

	hfree (atomic);
}
__attr int os_atomic_inc (void *os_context, void *atomic_context) {

	atomic_t *atomic;

#ifdef LOG_INTERFACE
	LOG("os_atomic_inc called.\n");
#endif

	atomic = (atomic_t*) atomic_context;

	return atomic_add_return (1, atomic);
}

__attr int os_atomic_dec (void *os_context, void *atomic_context) {

	atomic_t *atomic;

#ifdef LOG_INTERFACE
	LOG("os_atomic_dec called.\n");
#endif

	atomic = (atomic_t*) atomic_context;

	return atomic_sub_return (1, atomic);
}

typedef struct {
	spinlock_t lock;
	unsigned long flags;
#if !defined (NDEBUG)
	int *lock_count;
#endif
} lock_data_t, *lock_data_p;

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
__attr void *os_protect_create (void *os_context) {

#ifdef LOG_INTERFACE
	LOG("os_protect_create called.\n");
#endif

	lock_data_p lock_data = (lock_data_p) hmalloc (sizeof (lock_data_t));

	if (lock_data == NULL)
		return NULL;

	spin_lock_init (&lock_data->lock);
#if !defined (NDEBUG)
	lock_data->lock_count = (int*) alloc_percpu (int);
	if (lock_data->lock_count == NULL) {
		hfree (lock_data);
		return NULL;
	}
#endif
	
	return (void*)lock_data;
}
	
/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
__attr void os_protect_destroy (void *os_context, void* protect_context) {

#ifdef LOG_INTERFACE
	LOG("os_protect_destroy called.\n");
#endif

	lock_data_p lock_data = (lock_data_p)protect_context;

#if !defined (NDEBUG)
	free_percpu (lock_data->lock_count);
#endif
	hfree (lock_data);
}
	
/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
__attr void os_protect_lock (void* os_context, void *protect_context) {

#ifdef LOG_INTERFACE
	LOG("os_protect_lock called.\n");
#endif

	unsigned long local_flags;
#if !defined (NDEBUG)
	int cpu = get_cpu ();
#endif

	lock_data_p lock_data = (lock_data_p)(protect_context);

#if !defined (NDEBUG)
	if (spin_trylock_irqsave (&lock_data->lock, local_flags)) {
		goto OS_PROTECT_LOCK_OUT;
	} else {
		if (*(int*) per_cpu_ptr (lock_data->lock_count, cpu)) {
			ERROR("System lock already locked on this (%d) cpu!\n", get_cpu ());
			BUG();
			goto OS_PROTECT_LOCK_OUT;
		}
	}
#endif
	spin_lock_irqsave (&lock_data->lock, local_flags);
	lock_data->flags = local_flags;
#if !defined (NDEBUG)
OS_PROTECT_LOCK_OUT:
	(*(int*) per_cpu_ptr (lock_data->lock_count, cpu))++;
	put_cpu ();
#endif
}
	
/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
__attr void os_protect_unlock (void *os_context, void *protect_context) {

#ifdef LOG_INTERFACE
	LOG("os_protect_unlock called.\n");
#endif
	
	lock_data_p lock_data = (lock_data_p)(protect_context);
#if !defined (NDEBUG)
	int cpu = get_cpu ();
#endif

#if !defined (NDEBUG)
	if (!*(int*) per_cpu_ptr (lock_data->lock_count, cpu)) {
		ERROR("System lock not locked!\n");
		BUG();
		put_cpu ();
		return;
	}
	else {
		(*(int*) per_cpu_ptr (lock_data->lock_count, cpu))--;
		if (*(int*) per_cpu_ptr (lock_data->lock_count, cpu)) {
			NOTE("Removing lock protection on cpu %d.\n", get_cpu ());
			put_cpu ();
			return;
		}
	}
	put_cpu ();
#endif
	
	spin_unlock_irqrestore (&lock_data->lock, lock_data->flags);
}

/*---------------------------------------------------------------------------*\
 * Hardware access functions
\*---------------------------------------------------------------------------*/
__attr void *os_hw_get_registers_addr (void *os_context) {

#ifdef LOG_INTERFACE
	LOG("os_hw_get_register_addr called.\n");
#endif
/*TODO: Is this sufficient?*/
	return NULL;
}
	
/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
__attr void *os_hw_get_memory_addr (void *os_context) {

#ifdef LOG_INTERFACE
	LOG("os_hw_get_memory_addr called.\n");
#endif
/*TODO: Is this sufficient?*/
	return NULL;
}
	
/*---------------------------------------------------------------------------*\
 * Register access functions
\*---------------------------------------------------------------------------*/
__attr void os_stall_u_sec (void *os_context, unsigned int u_sec) {

#ifdef LOG_INTERFACE
	LOG("os_stall_u_sec called.\n");
#endif

	LOG("Waiting for %d usec.\n", u_sec);
	if (u_sec >= 1000)
		mdelay((unsigned int)(u_sec/1000));
	else
		udelay (u_sec);
}
	
/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
__attr int os_receive_packet (
		void *os_context, 
		void *p_packet, 
		unsigned short length) {

#ifdef LOG_INTERFACE
	LOG("\n\nos_receive_packet called.\n\n\n");
#endif

	card_p pdc = (card_p)os_context;
	
	return net_rx (pdc->net_dev, p_packet, length);
}
	
/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
__attr int os_get_eeeprom_image (
		void *os_context, 
		unsigned char **p_bufffer, 
		unsigned int *length) {

#ifdef LOG_INTERFACE
	LOG("os_get_eeeprom_image called.\n");
#endif
	return 0;
}
	
/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
__attr void os_power_state_busy (void *os_context) {

#ifdef LOG_INTERFACE
	LOG("os_power_state_busy called.\n");
#endif
}
	
/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
__attr void os_power_state_idle (void *os_context) {

#ifdef LOG_INTERFACE
	LOG("os_power_state_idle called.\n");
#endif
}
	
/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
__attr void os_set_wake_on_gpio (void *os_context) {

#ifdef LOG_INTERFACE
	LOG("os_set_wake_on_gpio called.\n");
#endif
}
	
/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
__attr void os_reset_wake_on_gpio (void *os_context) {

#ifdef LOG_INTERFACE
	LOG("os_reset_wake_on_gpio called.\n");
#endif
}

/*---------------------------------------------------------------------------*\
 * USB functions
\*---------------------------------------------------------------------------*/
#define GET_PIPE(dir,type,usb_dev,ep) (usb_##dir##type##pipe(usb_dev, ep))
__attr unsigned int os_usb_write (
		void *adapter, 
		void *msdu, 
		usb_caller_extension_p p_caller_ext,
		usb_caller_extension_p p_caller_err) {

#ifdef LOG_INTERFACE
	LOG("os_usb_write called.\n");
#endif
	card_p card;
	struct usb_device *dev;
	unsigned int pipe;
	
	assert(adapter != NULL);
	info(p_caller_ext == NULL);

	card = (card_p)adapter;
	dev = card->usb_dev;
	
	pipe = GET_PIPE(snd, bulk, dev, 1);
	return usb_write_async (dev, pipe, msdu, card->epwrite->wMaxPacketSize, 
				p_caller_ext, p_caller_err);
}
	
/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
__attr unsigned char *os_usb_rx_buf_get (
		void* p_adapter, 
		unsigned int *n_bytes_read, 
		unsigned int *p_handle) {

#ifdef LOG_INTERFACE
	LOG("os_usb_rx_buf_get called.\n");
#endif
	
	struct urb	*purb;
	void		*ptr;
	urb_context_p	pctx;
	unsigned char	*ret;

	if (unlist_urb_for_data (wlan_card->rx_pool, &purb, &ptr)) {
#ifdef LOG_USB
		LOG("os_usb_rx_buf_get: Getting data from URB %p\n", purb);
#endif
		*n_bytes_read = purb->actual_length;
		/*FIXME: Not 64 bit ready!*/
		*p_handle = (unsigned int)purb;
		ret = (unsigned char*)purb->transfer_buffer;
		pctx = (urb_context_p)ptr;
		atomic_set (&pctx->ref, 1);
	} else {
		ERROR("os_usb_rx_buf_get: No URB with data pending!\n");
		*n_bytes_read = 0;
		*p_handle = (unsigned int)NULL;
		ret = NULL;
	}

#ifdef LOG_USB
	LOG("%x %d\n", ret, *n_bytes_read);
#endif
	
	return ret;
}
	
/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
__attr unsigned int os_usb_rx_buf_inc_ref (unsigned int handle) {

#ifdef LOG_INTERFACE
	LOG("os_usb_rx_buf_inc_ref called.\n");
#endif
	
	struct urb	*purb = (struct urb*)handle;
	void 		*ptr;
	urb_context_p	pctx;

	if (purb == NULL) {
		ERROR("os_usb_rx_buf_inc_ref: NULL pointer URB!");
		return -1;
	}

#ifdef USB_LOG
	LOG("Increment reference count for URB %p.\n", purb);	
#endif
	
	get_ctx (purb, wlan_card->rx_pool, &ptr);
	pctx = (urb_context_p)ptr;
	atomic_inc (&pctx->ref);
	
	return atomic_read (&pctx->ref);
}
	
/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
__attr int os_usb_rx_buf_free (void* p_adapter, unsigned int handle) {

#ifdef LOG_INTERFACE
	LOG("os_usb_rx_buf_free called.\n");
#endif
	
	/*FIXME: Not 64 bit ready!*/
	struct urb	*purb = (struct urb*)handle;
	void 		*ptr;
	urb_context_p	pctx;

	if (purb == NULL) {
		ERROR("os_usb_rx_buf_free: NULL pointer URB!\n");
		return STACK_FALSE;
	}

	get_ctx (purb, wlan_card->rx_pool, &ptr);
	pctx = (urb_context_p)ptr;
	atomic_dec (&pctx->ref);
	if (atomic_read (&pctx->ref) > 0) {
#ifdef USB_LOG
		LOG("os_usb_rx_buf_free: Freeing URB %p rejected.\n", purb);
#endif
		return STACK_FALSE;
	}
	
#ifdef LOG_USB
	LOG("os_usb_rx_buf_free: Releasing URB %p\n", purb);
#endif
	
	release_urb (wlan_card->rx_pool, purb);
	
	usb_start_read (wlan_card);
	
	return STACK_TRUE;
}
	
/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
__attr unsigned int os_usb_write_cmd_sync (
		void *adapter, 
		unsigned char *in_buffer, 
		unsigned int n_bytes_to_write, 
		unsigned char *out_buffer, 
		unsigned int n_bytes_to_read, 
		unsigned int *n_bytes_read) {

#ifdef LOG_INTERFACE
	LOG("os_usb_write_cmd_sync called.\n");
#endif
	card_p card;
	struct usb_device *dev;
	unsigned int pipe, n_bytes_written, res = STACK_USB_ERROR;
	
	assert(adapter != NULL);

	card = (card_p)adapter;
	dev = card->usb_dev;
	
	pipe = GET_PIPE(snd, ctrl, dev, 0);
	if (!(res = usb_read_write_sync (
		dev, pipe, in_buffer, n_bytes_to_write,
		&n_bytes_written, HZ))) {
		pipe = GET_PIPE(rcv, ctrl, dev, 0);
		res = usb_read_write_sync (
			dev, pipe, out_buffer, n_bytes_to_read,
			n_bytes_read, HZ);
	}

	return res;
}
	
/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
__attr unsigned int os_usb_write_cmd_async (
		void *adapter, 
		unsigned char *in_buffer,
	       	unsigned int n_bytes_write, 
		unsigned char *out_buffer,
		unsigned int *n_bytes_read, 
		usb_caller_extension_p caller_ext) {

#ifdef LOG_INTERFACE
	LOG("os_usb_write_cmd_async called.\n");
#endif
	card_p card;
	struct usb_device *dev;
	unsigned int pipe, res = STACK_USB_ERROR;
	
	assert (adapter!=NULL);
	
	card = (card_p)adapter;
	dev = card->usb_dev;

	assert (in_buffer != NULL);
	assert (card != NULL);
	assert (dev != NULL);
	
	pipe = GET_PIPE (snd, ctrl, dev, 0);
	if ((res = usb_read_write_cmd_async (dev, pipe, in_buffer, 
			n_bytes_write, NULL)) == STACK_USB_OK) {
		if (out_buffer != NULL) {
			pipe = GET_PIPE (rcv, ctrl, dev, 0);
			res = usb_read_write_cmd_async (
				dev, pipe, out_buffer, MAX_EP0_IN, caller_ext);
		}
	}

	return res;
}
	
/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
__attr unsigned int os_usb_write_sync (
		void *adapter, 
		int ep, 
		unsigned char *buffer, 
		unsigned int n_bytes_to_write, 
		unsigned int *n_bytes_written) {
	
#ifdef LOG_INTERFACE
	LOG("os_usb_write_sync called.\n");
#endif
	card_p card;
	struct usb_device *dev;
	unsigned int pipe;
	
	assert(adapter != NULL);

	card = (card_p)adapter;
	dev = card->usb_dev;

	switch (ep) {
		case 0:
			pipe = GET_PIPE(snd, ctrl, dev, ep);
			break;
		case 1:
			pipe = GET_PIPE(snd, bulk, dev, ep);
			break;
		case 2:
			pipe = GET_PIPE(rcv, bulk, dev, ep);
			ERROR("\nos_usb_write_sync called with in endpoint?\n\n");
			break;
		default:
			ERROR("Unkown Endpoint.\n");
			goto ERROR_WRITE_SYNC;
	}

	return usb_read_write_sync (
			dev, pipe, buffer, n_bytes_to_write,
			n_bytes_written, HZ);

ERROR_WRITE_SYNC:
	return STACK_USB_ERROR;
}
	
/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
__attr unsigned int os_usb_read_sync (
		void *adapter, 
		int ep, 
		unsigned char *buffer, 
		unsigned int n_bytes_to_read,
		unsigned int timeout_ms, 
		unsigned int *n_bytes_read) {

#ifdef LOG_INTERFACE
	LOG("os_usb_read_sync called.\n");
#endif
	card_p card;
	struct usb_device *dev;
	unsigned int pipe;

	assert(adapter != NULL);

	card = (card_p)adapter;
	dev = card->usb_dev;

	switch (ep) {
		case 0:
			pipe = GET_PIPE(rcv, ctrl, dev, ep);
			break;
		case 1:
			pipe = GET_PIPE(snd, bulk, dev, ep);
			ERROR("\nos_usb_read_sync called with out endpoint?\n\n");
			break;
		case 2:
			pipe = GET_PIPE(rcv, bulk, dev, ep);
			break;
		default:
			ERROR("Unkown Endpoint.\n");
			goto ERROR_READ_SYNC;
	}

	return usb_read_write_sync (
			dev, pipe, (void*)buffer, n_bytes_to_read,
			n_bytes_read, MSEC2JIFF(timeout_ms));
	
ERROR_READ_SYNC:
	return STACK_USB_ERROR;
}
	
/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
__attr int os_usb_is_alive (void *adapter) {

#ifdef LOG_INTERFACE
	LOG("os_usb_is_alive called.\n");
#endif
	
	return STACK_TRUE;
}
	
/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
__attr int os_usb_write_available (void *adapter) {

#ifdef LOG_INTERFACE
	LOG("os_usb_write_availbale called.\n");
#endif
	card_p pdc;

	assert (adapter != NULL);

	pdc = (card_p)adapter;

	if (!POOL_GET_FREE(pdc->tx_pool))	
		return STACK_FALSE;
		
	return STACK_TRUE;
}
	
/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
__attr unsigned int os_usb_write_check_size (void *adapter) {

#ifdef LOG_INTERFACE
	LOG("os_usb_write_check_size called.\n");
#endif

	card_p pdc = (card_p)adapter;

	return POOL_GET_FREE(pdc->tx_pool);
	
}
	
/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
__attr int os_usb_need_file_padding (
		void *p_adapter, 
		unsigned int total_file_xfer_size, 
		unsigned int xfer_size) {

	card_p pdc = (card_p)p_adapter;
	unsigned long packets;
	
#ifdef LOG_INTERFACE
	LOG("os_usb_need_file_padding called.\n");
#endif
	
	packets = ((total_file_xfer_size - 1) / pdc->epwrite->wMaxPacketSize) + 1;
	
	if ((packets % 2))
		return 0;
	else
		return 1;
	
}

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
__attr int os_usb_start_recovery_worker (
		void* p_adapter,
		int (__attr2 *recovery_worker_routine) (void*, void*),
		void* handle) {
	
	return start_recovery_worker (
			p_adapter, 
			recovery_worker_routine, 
			handle);
}

/*---------------------------------------------------------------------------*\
 * IPC functions
\*---------------------------------------------------------------------------*/
__attr int os_ipc_event_send (
		void *h_adapter,
		fwlanusb_event_t event,
	       	void *event_data, 
		unsigned int event_data_size) {

#ifdef LOG_INTERFACE
	LOG("os_ipc_event_send called.\n");
#endif
	wext_event_send (h_adapter, event, event_data, event_data_size);

	return 0;
}
	
/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
__attr int os_ipc_unbound_event_send (void *h_adapter) {

#ifdef LOG_INTERFACE
	LOG("os_ipc_unbound_event_send called.\n");
#endif
	
	return 0;
}

#ifndef __WITHOUT_INTERFACE__
/*---------------------------------------------------------------------------*\
 *  Interface initialisation
\*---------------------------------------------------------------------------*/
static lib_callback_t *	lib	= NULL;
static lib_interface_t	libif	= {
	
	.report 			= &report,
	.dprintf			= &os_debug_printf,
	.memory_alloc 			= &os_memory_alloc,
	.memory_calloc			= &os_memory_calloc,
	.memory_free			= &os_memory_free,
	.timer_create			= &os_timer_create,
	.timer_destroy			= &os_timer_destroy,
	.timer_start			= &os_timer_start,
	.timer_stop			= &os_timer_stop,
	.time_stamp_ms			= &os_time_stamp_ms,
	.time_stamp_us			= &os_time_stamp_us,
	.atomic_create			= &os_atomic_create,
	.atomic_destroy			= &os_atomic_destroy,
	.atomic_inc			= &os_atomic_inc,
	.atomic_dec			= &os_atomic_dec,
	.protect_create			= &os_protect_create,
	.protect_destroy		= &os_protect_destroy,
	.protect_lock			= &os_protect_lock,
	.protect_unlock			= &os_protect_unlock,
	.hw_get_registers_addr		= &os_hw_get_registers_addr,
	.hw_get_memory_addr		= &os_hw_get_memory_addr,
	.stall_u_sec			= &os_stall_u_sec,
	.receive_packet			= &os_receive_packet,
	.get_eeeprom_image		= &os_get_eeeprom_image,
	.power_state_busy		= &os_power_state_busy,
	.power_state_idle		= &os_power_state_idle,
	.set_wake_on_gpio		= &os_set_wake_on_gpio,
	.reset_wake_on_gpio		= &os_reset_wake_on_gpio,
	.usb_write			= &os_usb_write,
	.usb_rx_buf_get			= &os_usb_rx_buf_get,
	.usb_rx_buf_inc_ref		= &os_usb_rx_buf_inc_ref,
	.usb_rx_buf_free		= &os_usb_rx_buf_free,
	.usb_write_cmd_sync		= &os_usb_write_cmd_sync,
	.usb_write_cmd_async		= &os_usb_write_cmd_async,
	.usb_write_sync			= &os_usb_write_sync,
	.usb_read_sync			= &os_usb_read_sync,
	.usb_is_alive			= &os_usb_is_alive,
	.usb_write_available		= &os_usb_write_available,
	.usb_write_check_size		= &os_usb_write_check_size,
	.usb_need_file_padding		= &os_usb_need_file_padding,
	.usb_start_recovery_worker 	= &os_usb_start_recovery_worker,
	.ipc_event_send			= &os_ipc_event_send,
	.ipc_unbound_event_send 	= &os_ipc_unbound_event_send,

	.name			= TARGET

};

/*---------------------------------------------------------------------------*\
 *  Interface functions
\*---------------------------------------------------------------------------*/
lib_callback_t * get_library (void) {

	return lib;
} /* get_library */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
lib_callback_t * link_library (void * context) {

	LOG("Interface exchange... (%d)\n", sizeof (lib_interface_t));
	return (lib = avm_lib_attach (&libif, context));
} /* link_library */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
void free_library (void) {

	if (lib != NULL) {
		lib = NULL;
		avm_lib_detach (&libif);
	}
} /* free_library */
#endif

