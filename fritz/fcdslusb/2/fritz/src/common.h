/* 
 * common.h
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

#ifndef __have_common_h__
#define __have_common_h__

#include <asm/atomic.h>
#include "attr.h"

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
#define USB_MAX_NUM_RX_BUFFERS	4
#define	USB_MAX_RX_XFER_SIZE	(2 * 1024)
#define MAX_TRANSFER_SIZE	(8 * 1024)

struct __card;
struct __ndi;
struct c6205_ctx;

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
typedef void * (__attr * code_func_t) (void *, unsigned, unsigned *);
typedef unsigned (__attr * timer_func_t) (void);
typedef void (__attr * to_pc_ack_func_t) (unsigned char);
typedef void (__attr * wait_func_t) (unsigned);
typedef unsigned (__attr * print_func_t) (void *, void *, unsigned);
typedef unsigned (__attr * xfer_func_t) (void *, unsigned);

typedef struct __card		card_t, * card_p;
typedef struct c6205_ctx *	c6205_context;

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
typedef	long			intptr_t;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24))
typedef	unsigned long		uintptr_t;
#endif

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
typedef struct __dreq {

	unsigned		tx_block_num;
	unsigned		tx_block_size;
	unsigned		tx_max_trans_len;
	unsigned		rx_block_num;
	unsigned		rx_block_size;
	unsigned		rx_max_trans_len;
	code_func_t		get_code;
	void *			context;
	print_func_t		dbg_print;
	void *			dbg_context;
} dif_require_t, * dif_require_p;

typedef struct __ireq {

	xfer_func_t	tx_func;
} in_require_t, * in_require_p;

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
typedef void * hw_block_handle;

typedef struct __buf_desc {					

	hw_block_handle			handle;		/* const */
	unsigned char *			buffer;		/* const */
	unsigned long			length;		/* const */
	void *				context;	/* const */
} hw_buffer_descriptor_t, * hw_buffer_descriptor_p;

typedef struct __ureq {

	unsigned long			offset;
	unsigned long			length;
	void *				context;
	hw_buffer_descriptor_p		desc;
#if !defined (NDEBUG)
	atomic_t			in_use;
#endif
} hw_usb_request_t, * hw_usb_request_p;

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
typedef void (__attr * hw_completion_func_t) (
	const hw_buffer_descriptor_p	desc, 
	unsigned			offset,
	unsigned			length,
	void *				context
);

typedef void (__attr * hw_event_func_t) (
	unsigned			event_mask,
	const hw_buffer_descriptor_p	desc,
	unsigned			offset,
	unsigned			length,
	void *				context
);

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/

#endif

