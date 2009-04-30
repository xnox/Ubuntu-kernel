/* 
 * devif.c
 * Copyright (C) 2003, AVM GmbH. All rights reserved.
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
#include <asm/atomic.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include "defs.h"
#include "lib.h"
#include "fw.h"	
#include "driver.h"
#include "devif.h"
#include "tools.h"

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static atomic_t			is_stopped		= ATOMIC_INIT (FALSE);
static atomic_t			n_cancelled_rx;
static atomic_t			n_cancelled_tx;
static int			pipe_in;
static int			pipe_out;
#if !defined (NDEBUG)
static atomic_t			is_open			= ATOMIC_INIT (FALSE);
static atomic_t			is_started		= ATOMIC_INIT (FALSE);
static atomic_t			n_buffers		= ATOMIC_INIT (0);
#endif

atomic_t			n_rx_pending;
atomic_t			n_tx_pending;
atomic_t			f_stop_rx_pending;
atomic_t			f_stop_tx_pending;

static hw_completion_func_t	rx_callback		= NULL;
static hw_completion_func_t	tx_callback		= NULL;
static hw_event_func_t		event_callback		= NULL;
static dif_require_t		dif_req			= { 0, };
static int			dif_set			= 0;
static card_p			card_link		= NULL;

#if !defined (NDEBUG) && defined (LOG_LINK)
#define	MAX_BUFFERS	32

static hw_buffer_descriptor_p	dbg_bdp[MAX_BUFFERS];
static unsigned			dbg_bdix = 0;
#endif

/*****************************************************************************\
 * D E V I C E   I N T E R F A C E
\*****************************************************************************/ 

#if defined (LOG_DEBUG_HOOK)
static print_func_t		lib_dbg_print	= NULL;

static __attr unsigned dif_print (void * context, void * buffer, unsigned length) {
	extern void	list_submitted_urbs (buffer_pool_p, const char *);
	unsigned	ret;

	ret = (lib_dbg_print == NULL) ? length : (* lib_dbg_print) (context, buffer, length);
	if (card_link != NULL) {
		list_submitted_urbs (card_link->tx_pool, "TX URBs");
		list_submitted_urbs (card_link->rx_pool, "RX URBs");
	}
	return ret;
} /* dif_print */
#endif

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
__attr void dif_xfer_requirements (dif_require_p rp, unsigned dummy) {

	UNUSED_ARG (dummy);
	assert (rp != NULL);
	assert (rp->tx_block_num > 0);
	assert (rp->tx_block_size > 0);
	assert (rp->tx_max_trans_len == 0);
	assert (rp->rx_block_num > 0);
	assert (rp->rx_block_size > 0);
	assert (rp->rx_max_trans_len == 0);
	
	assert (rp->tx_block_num <= 8);
	assert (rp->rx_block_num <= 8);

	dif_req = *rp;
	dif_req.tx_max_trans_len = rp->tx_block_size;
	dif_req.rx_max_trans_len = rp->rx_block_size;

#if defined (LOG_DEBUG_HOOK)
	lib_dbg_print = dif_req.dbg_print;
	dif_req.dbg_print = dif_print;
#endif
			
#if !defined (NDEBUG) && defined (LOG_LINK)
	LOG(
		"Requirements: TX %u/%u/%u, RX %u/%u/%u, Func/Ctx %p/%p\n",
		dif_req.tx_block_num,
		dif_req.tx_block_size,
		dif_req.tx_max_trans_len,
		dif_req.rx_block_num,
		dif_req.rx_block_size,
		dif_req.rx_max_trans_len,
		dif_req.get_code,
		dif_req.context
	);
#endif
	dif_set = 1;
} /* dif_xfer_requirements */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
int dif_buffer_params (unsigned * txnum, unsigned * txsize, unsigned * rxnum, unsigned * rxsize) {
	int res	= 0;
	
	if (dif_set) {
		assert (txnum != NULL);
		assert (txsize != NULL);
		*txnum  = dif_req.tx_block_num;
		*txsize = dif_req.tx_max_trans_len;

		assert (rxnum != NULL);
		assert (rxsize != NULL);
		*rxnum  = dif_req.rx_block_num;
		*rxsize = dif_req.rx_max_trans_len;

		res = 1;
	}
	assert (res);
	return res;
} /* dif_buffer_params */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
void dif_reset (void) {

	LOG("Resetting device structures...\n");
	/* nop */
} /* dif_reset */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
int dif_init (card_p cp) {

	LOG("Initializing device structures...\n");
	card_link = cp;
	pipe_in   = cp->epread->bEndpointAddress;
	pipe_out  = cp->epwrite->bEndpointAddress;

	atomic_set (&n_rx_pending, 0);
	atomic_set (&n_tx_pending, 0);
	atomic_set (&f_stop_rx_pending, 0);
	atomic_set (&f_stop_tx_pending, 0);
	atomic_set (&n_cancelled_rx, 0);
	atomic_set (&n_cancelled_tx, 0);

	lib_memset (&dif_req, 0, sizeof (dif_require_t));
	return 1;
} /* dif_init */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
void dif_exit (void) {

	LOG("Deinitializing device...\n");
	dif_reset ();
} /* dif_exit */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
void dif_error (int code, hw_buffer_descriptor_p bdp, unsigned ofs, unsigned len, void * ctx) {

	assert (event_callback != NULL);
	LOG("Event(0x%x) indication!\n");
	(* event_callback) (code, bdp, ofs, len, ctx);
	shutdown ();
} /* dif_error */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
int dif_start (void) {

	return TRUE;
} /* dif_start */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static void dif_stop (int tx_flag) {
	unsigned	N;
	
	LOG("Stopping device...\n");
	assert (atomic_read (&is_started) != 0);
	assert (pipe_in != 0);
	assert (pipe_out != 0);
	if (tx_flag) {
		N = OSHWCancelTx ((hw_block_handle) card_link);
		LOG("Cancelled %u TX URBs.\n", N);
	} else {
		N = OSHWCancelRx ((hw_block_handle) card_link);
		LOG("Cancelled %u TX URBs.\n", N);
	}
} /* dif_stop */

/*****************************************************************************\
 * H A R D W A R E   I N T E R F A C E
\*****************************************************************************/ 

__attr unsigned OSHWBlockOpen (unsigned index, hw_block_handle * handle) {

	UNUSED_ARG (index);
	assert (handle != NULL);
	info (*handle == NULL);
	*handle = (hw_block_handle) card_link;
	assert (!atomic_xchg (&is_open, 1));
	return TRUE;
} /* OSHWBlockOpen */

/*---------------------------------------------------------------------------*\
\*-O-------------------------------------------------------------------------*/
__attr void OSHWBlockClose (hw_block_handle handle) {

	assert (handle == (hw_block_handle) card_link);
	assert (atomic_read (&n_buffers) == 0);
	assert (atomic_read (&is_open));
	assert (!atomic_read (&is_started));
	assert (atomic_xchg (&is_open, 0));
} /* OSHWBlockClose */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
__attr unsigned OSHWStart (
	hw_block_handle		handle,
	hw_completion_func_t	rx_completer,
	hw_completion_func_t	tx_completer,
	hw_event_func_t		event_handler
) {
	assert (handle == (hw_block_handle) card_link);
	assert (pipe_in != 0);
	assert (pipe_out != 0);
	assert (atomic_read (&is_open));
	assert (!atomic_read (&is_started));
	assert (rx_callback == NULL);
	assert (tx_callback == NULL);
	assert (event_callback == NULL);
	assert (rx_completer != NULL);
	assert (tx_completer != NULL);
	assert (event_handler != NULL);

	rx_callback	= rx_completer;
	tx_callback	= tx_completer;
	event_callback	= event_handler;
	assert (!atomic_xchg (&is_started, 1));

	return dif_start ();
} /* OSHWStart */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
__attr void OSHWStop (hw_block_handle handle) {

	assert (handle == (hw_block_handle) card_link);
	assert (atomic_read (&is_open));
	assert (atomic_read (&is_started));

	dif_stop (TRUE);
	dif_stop (FALSE);
	atomic_set (&is_stopped, TRUE);

	rx_callback	= NULL;
	tx_callback	= NULL;
	event_callback	= NULL;
	assert (atomic_xchg (&is_started, 0));

	shutdown ();
} /* OSHWStop */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
__attr hw_buffer_descriptor_p OSHWAllocBuffer (hw_block_handle handle, unsigned length) {
	hw_buffer_descriptor_p	pbd;
	hw_usb_request_p	urp;
	
	LOG("Allocating buffer of %u bytes\n", length);
	assert (handle == (hw_block_handle) card_link);
	assert (atomic_read (&is_open) != 0);

	if (NULL == (pbd = (hw_buffer_descriptor_p) hmalloc (length + sizeof (hw_buffer_descriptor_t)))) {
		return NULL;
	}
	if (NULL == (urp = (hw_usb_request_p) hmalloc (sizeof (hw_usb_request_t)))) {
		hfree (pbd);
		return NULL;
	}
	pbd->handle  = handle;
	pbd->buffer  = (unsigned char *) (pbd + 1);
	pbd->length  = length;
	pbd->context = urp;
	
	urp->length  = 0;
	urp->offset  = 0;
	urp->context = NULL;
	urp->desc    = pbd;
	
#if !defined (NDEBUG)
	atomic_set (&urp->in_use, 0);
	atomic_inc (&n_buffers);
#if defined (LOG_LINK)
	assert (dbg_bdix < MAX_BUFFERS);
	dbg_bdp[dbg_bdix++] = pbd;
#endif
#endif
	return pbd;
} /* OSHWAllocBuffer */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
__attr void OSHWFreeBuffer (hw_buffer_descriptor_p desc) {
	hw_usb_request_p	rp;

	assert (desc != NULL);
	assert (atomic_read (&is_open));
	assert (atomic_read (&n_buffers) > 0);
	rp = (hw_usb_request_p) desc->context;
	assert (rp != NULL);
	assert (rp->desc == desc);
	hfree (rp);
	desc->context = NULL;
	assert (desc->handle == (hw_block_handle) card_link);
	hfree (desc);
#if !defined (NDEBUG)
	atomic_dec (&n_buffers);
#endif
} /* OSHWFreeBuffer */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
__attr unsigned OSHWGetMaxBlockSize (hw_block_handle handle) {

	assert (atomic_read (&is_open));
	assert (handle == (hw_block_handle) card_link);
	return MAX_TRANSFER_SIZE;
} /* OSHWGetMaxBlockSize */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
__attr unsigned OSHWGetMaxConcurrentRxBlocks (hw_block_handle handle) {

	assert (atomic_read (&is_open));
	assert (handle == (hw_block_handle) card_link);
	return 8;
} /* OSHWGetMaxConcurrentRxBlocks */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
__attr unsigned OSHWGetMaxConcurrentTxBlocks (hw_block_handle handle) {

	assert (atomic_read (&is_open));
	assert (handle == (hw_block_handle) card_link);
	return 8;
} /* OSHWGetMaxConcurrentRxBlocks */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
__attr void * OSHWExchangeDeviceRequirements (hw_block_handle handle, void * pin) {
	
	UNUSED_ARG (pin);
	assert (handle == (hw_block_handle) card_link);
	assert (dif_req.tx_block_num > 0);
	assert (dif_req.tx_block_size > 0);
	assert (dif_req.tx_max_trans_len > 0);
	assert (dif_req.rx_block_num > 0);
	assert (dif_req.rx_block_size > 0);
	assert (dif_req.rx_max_trans_len > 0);

	LOG("Returning requirements, TX %u/%u/%u, RX %u/%u/%u\n", 
		dif_req.tx_block_num,
		dif_req.tx_block_size,
		dif_req.tx_max_trans_len,
		dif_req.rx_block_num,
		dif_req.rx_block_size,
		dif_req.rx_max_trans_len
	);
	return (void *) &dif_req;
} /* OSHWExchangeRequirements */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
__attr unsigned OSHWTxBuffer (
	const hw_buffer_descriptor_p	desc,
	unsigned			offset,
	unsigned			length,
	void *				context
) {
	hw_usb_request_p		urp;
	int				res;
	char *				buf;
	
	assert (atomic_read (&is_started));
	assert (desc != NULL);
	assert (desc->handle == (hw_block_handle) card_link);
	assert (desc->length >= (offset + length));

	urp = (hw_usb_request_p) desc->context;
	assert (urp != NULL);
	assert (urp->desc == desc);
	assert (!atomic_xchg (&urp->in_use, 1));

	assert (pipe_out != 0);
	info (!atomic_read (&f_stop_tx_pending));
	if (!atomic_read (&f_stop_tx_pending)) {
		assert (
			((char *) desc->buffer - (char *) desc)
			== sizeof (hw_buffer_descriptor_t)
		);
		buf = desc->buffer + offset;
		atomic_inc (&n_tx_pending);
		urp->length = length;
		urp->offset = offset;
		urp->context = context;
		
		res = os_usb_submit_tx (
			pipe_out, 
			buf, 
			length, 
			tx_callback, 
			desc
		);
		info (res);
		return TRUE;
	} else {
		return FALSE;
	}
} /* OSHWTxBuffer */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
__attr unsigned OSHWRxBuffer (
	const hw_buffer_descriptor_p	desc,
	unsigned			offset,
	unsigned			length,
	void *				context
) {
	hw_usb_request_p		urp;
	int				res;
	char *				buf;
	
	assert (atomic_read (&is_started));
	assert (desc != NULL);
	assert (desc->handle == (hw_block_handle) card_link);
	
	urp = (hw_usb_request_p) desc->context;
	assert (urp != NULL);
	assert (urp->desc == desc);
	assert (!atomic_xchg (&urp->in_use, 1));
	
	assert (pipe_in != 0);
	info (!atomic_read (&f_stop_rx_pending));
	if (!atomic_read (&f_stop_rx_pending)) {
		assert (
			((char *) desc->buffer - (char *) desc)
			== sizeof (hw_buffer_descriptor_t)
		);
		buf = desc->buffer + offset;
		atomic_inc (&n_rx_pending);
		urp->length = length;
		urp->offset = offset;
		urp->context = context;

		res = os_usb_submit_rx (
			pipe_in, 
			buf, 
			length, 
			rx_callback, 
			desc
		);
		info (res);
		return TRUE;
	} else {
		return FALSE;
	}
} /* OSHWRxBuffer */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
__attr unsigned OSHWCancelRx (hw_block_handle handle) {
	unsigned	ncr;
	
	UNUSED_ARG (handle);
	LOG("Cancel RX\n");
	assert (atomic_read (&is_started));
	atomic_set (&f_stop_rx_pending, 1);
	atomic_set (&n_cancelled_rx, 0);
	ncr = unlink_urbs (capi_card->rx_pool);
	atomic_set (&n_cancelled_rx, ncr);
	atomic_set (&f_stop_rx_pending, 0);
	return ncr;
} /* OSHWCancelRx */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
__attr unsigned OSHWCancelTx (hw_block_handle handle) {
	unsigned	nct;
	
	UNUSED_ARG (handle);
	LOG("Cancel TX\n");
	assert (atomic_read (&is_started));
	atomic_set (&f_stop_tx_pending, 1);
	atomic_set (&n_cancelled_tx, 0);
	nct = unlink_urbs (capi_card->tx_pool);
	atomic_set (&n_cancelled_tx, nct);
	atomic_set (&f_stop_tx_pending, 0);
	return nct;
} /* OSHWCancelTx */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
