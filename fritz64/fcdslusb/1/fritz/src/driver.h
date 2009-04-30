/* 
 * driver.h
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

#ifndef __have_driver_h__
#define __have_driver_h__

#include <asm/semaphore.h>
#include <asm/atomic.h>

#include <linux/skbuff.h>
#include <linux/capi.h>
#include <linux/isdn/capilli.h>
#include <linux/usb.h>
#include "tables.h"
#include "queue.h"
#include "libdefs.h"
#include "tools.h"
#include "buffers.h"

#define	MAX_TRANSFER_SIZE		2048	/* was: 8192 */
#define MAX_URB_COUNT_TX		1
#define MAX_URB_COUNT_RX		8

#pragma pack (1)

typedef struct {

	unsigned short product_id;
	unsigned short release_number;
	unsigned       self_powered;	
} device_info_t, * device_info_p;

#pragma pack ()

typedef __attr2 void (* complete_func_p) (unsigned status, unsigned ref_data);
typedef __attr2 unsigned (* get_next_tx_buffer_func_p) (char * pbuf);
typedef __attr2 void (* event_handler_func_p) (unsigned eventmask);
typedef __attr2 void (* new_rx_buffer_avail_func_p) (char * pbuf, unsigned length);

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
typedef long		intptr_t;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24))
typedef unsigned long	uintptr_t;
#endif

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
typedef struct {

	char *					version;
	char *					string[8];
	struct capi_ctr *			kctrl;
} per_ctrl_t, * per_ctrl_p;

typedef struct {

	int					ctrl2;
	void *					ctxp;
	struct usb_device *			dev;
	struct capi_ctr *			ctr;
	get_next_tx_buffer_func_p		tx_get_next;
	event_handler_func_p			ev_handler;
	new_rx_buffer_avail_func_p		rx_avail;
	unsigned				max_buf_size;
	int					running;
	atomic_t				is_open;

	buffer_pool_p				tx_pool;
	buffer_pool_p				rx_pool;
	
	struct usb_endpoint_descriptor *	epread;
	struct usb_endpoint_descriptor *	epwrite;
	atomic_t				tx_pending;
	atomic_t				rx_pending;

	char *					version;
	char *					string[8];
	unsigned				count;
	appltab_t *				appls;
	queue_t *				queue;
	unsigned				length;

	void (__attr2 *				reg_func) (void *, unsigned);
	void (__attr2 *				rel_func) (void *);
	void (__attr2 *				dwn_func) (void);

} card_t, * card_p;

typedef struct {

	card_p					card;
	per_ctrl_p				ctrl;
} bundle_t, * bundle_p;

#define	GET_CARD(ctr)	(((bundle_p) (ctr)->driverdata)->card)
#define	GET_CTRL(ctr)	(((bundle_p) (ctr)->driverdata)->ctrl)

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
#define	UNDEF_CONFIG			-1
#define	RESET_CONFIG			0
#define	LOADING_CONFIG			1
#define	RUNNING_CONFIG			1

extern int				card_config;

extern card_p				capi_card;
extern lib_callback_t *			capi_lib;
extern struct capi_ctr *		capi_controller[2];

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
extern void enter_critical (void);
extern void leave_critical (void);

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
extern int add_card (struct usb_device *);

extern int nbchans (struct capi_ctr *);

extern int msg2stack (unsigned char *);
extern void msg2capi (unsigned char *);

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
extern int make_thread (void);
extern void kill_thread (void);

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
extern int local_init_urbs (void);
extern void local_unlink_urbs (void);
extern void local_free_urbs (void);

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
extern int make_ctx(void **);
extern int kill_ctx(void **);

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
extern void start_closing_worker (void (* func) (void *), void * data);
extern void init_closing_worker (void);

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
extern void kick_scheduler (void);

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
extern void init (unsigned, void (__attr2 *) (void *, unsigned),
			    void (__attr2 *) (void *),
			    void (__attr2 *) (void));

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
#endif

