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
#include <linux/interrupt.h>
#include "common.h"
#include "libdefs.h"

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
#if !defined (C6205_PCI_HSR_OFFSET)
#define C6205_PCI_HSR_OFFSET		0
#define C6205_PCI_HDCR_OFFSET		4
#define C6205_PCI_DSPP_OFFSET		8
#endif

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
#define	C6205_PCI_HDCR_WARMRESET	1
#define	C6205_PCI_HDCR_DSPINT		2
#define	C6205_PCI_HDCR_PCIBOOT		4
#define	C6205_PCI_HSR_INTSRC		1
#define	C6205_PCI_HSR_INTAVAL		2
#define	C6205_PCI_HSR_OMTA		4
#define	C6205_PCI_HSR_CFRGERR		8
#define	C6205_PCI_HSR_EEREAD		16

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
typedef int (* irq_callback_t) (void *);

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
extern atomic_t		xfer_flag;

extern void		xfer_handler (card_p);

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
extern int dif_init (card_p, int, timer_func_t, to_pc_ack_func_t);
extern void dif_exit (void);

extern void dif_set_params (
	unsigned	num,
	unsigned	offset,
	ioaddr_p	pwin,
	unsigned	len,
	unsigned	bufofs
);

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
extern __attr void dif_xfer_requirements (dif_require_p, unsigned);

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
extern void set_interrupt_callback (irq_callback_t, void *);
extern void clear_interrupt_callback (void);

extern irqreturn_t device_interrupt (int, void *);

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
extern dma_struct_p dma_alloc (void);
extern void dma_free (dma_struct_p *);

extern int dma_setup (dma_struct_p, unsigned);
extern void dma_free_buffer (dma_struct_p);

extern int dma_init (dma_struct_p, unsigned);
extern void dma_exit (dma_struct_p);

extern dma_struct_p * dma_get_struct_list (unsigned *);
	
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
