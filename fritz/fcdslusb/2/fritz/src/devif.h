/* 
 * devif.h
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

#ifndef __have_devif_h__
#define __have_devif_h__

#include <asm/atomic.h>
#include <linux/sched.h>
#include "common.h"
#include "libdefs.h"

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
extern atomic_t		n_rx_pending;
extern atomic_t		n_tx_pending;
extern atomic_t		f_stop_rx_pending;
extern atomic_t		f_stop_tx_pending;

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
extern int dif_init (card_p);
extern void dif_exit (void);

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
extern void dif_error (
	int			code,
	hw_buffer_descriptor_p	bdp,
	unsigned		ofs,
	unsigned		len,
	void *			ctx
);

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
extern __attr void dif_xfer_requirements (dif_require_p, unsigned);

extern int dif_buffer_params (unsigned *, unsigned *, unsigned *, unsigned *);

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
#if defined (USB_EVENT_HARD_ERROR)
# undef USB_EVENT_HARD_ERROR
#endif

#define USB_REQUEST_CANCELLED	0x8000
#define USB_RX_REQUEST		0x4000
#define USB_TX_REQUEST		0x2000
#define USB_EVENT_HARD_ERROR	0x0800

#define DMA_REQUEST_CANCELLED	0x8000
#define DMA_RX_REQUEST		0x4000
#define DMA_TX_REQUEST		0x2000
#define DMA_EVENT_HARD_ERROR	0x0800

extern __attr unsigned OSHWBlockOpen (unsigned, hw_block_handle *);
extern __attr void OSHWBlockClose (hw_block_handle);

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
extern __attr unsigned OSHWStart (
	hw_block_handle, 
	hw_completion_func_t, 
	hw_completion_func_t, 
	hw_event_func_t
);
extern __attr void OSHWStop (hw_block_handle);

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
extern __attr hw_buffer_descriptor_p OSHWAllocBuffer (hw_block_handle, unsigned);
extern __attr void OSHWFreeBuffer (hw_buffer_descriptor_p);

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
extern __attr unsigned OSHWGetMaxBlockSize (hw_block_handle);
extern __attr unsigned OSHWGetMaxConcurrentRxBlocks (hw_block_handle);
extern __attr unsigned OSHWGetMaxConcurrentTxBlocks (hw_block_handle);

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
extern __attr void * OSHWExchangeDeviceRequirements (hw_block_handle, void *);

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
extern __attr unsigned OSHWTxBuffer (
	const hw_buffer_descriptor_p,
	unsigned,
	unsigned,
	void *
);

extern __attr unsigned OSHWRxBuffer (
	const hw_buffer_descriptor_p,
	unsigned,
	unsigned,
	void *
);

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
extern __attr unsigned OSHWCancelRx (hw_block_handle);

extern __attr unsigned OSHWCancelTx (hw_block_handle);

#endif
