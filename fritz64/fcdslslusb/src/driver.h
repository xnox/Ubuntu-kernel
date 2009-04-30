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

#include <asm/atomic.h>

#include <linux/skbuff.h>
#include <linux/spinlock.h>
#include <linux/usb.h>
#include <linux/capi.h>
#include <linux/isdn/capilli.h>
#include "tables.h"
#include "queue.h" 
#include "buffers.h"
#include "libdefs.h" 
#include "common.h"

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
#pragma pack (1)

typedef struct {

	unsigned short				product_id;
	unsigned short				release_number;
	unsigned				self_powered;
} device_info_t, * device_info_p;

#pragma pack ()

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
typedef struct __urb_ctx {

	hw_completion_func_t			callback;
	unsigned char *				buffer;
	hw_buffer_descriptor_p			bdp;
} urb_context_t, * urb_context_p;

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
struct __card {

	int					ctrl2;
	void *					ctxp;
	
	unsigned				length;
	unsigned				count;
	int					running;

	appltab_t *				appls;
	queue_t *				queue;

	void (__attr2 *				reg_func) (void *, unsigned);
	void (__attr2 *				rel_func) (void *);
	void (__attr2 *				dwn_func) (void);

	c6205_context				c6205_ctx;

	struct usb_device *			dev;
	struct usb_endpoint_descriptor *	epread;
	struct usb_endpoint_descriptor *	epwrite;

	buffer_pool_p				tx_pool;
	buffer_pool_p				rx_pool;
} ;

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
typedef struct __per_ctrl {
#if !defined (NDEBUG)
	int					cid;
#endif
	char *					version;
	char *					string[8];
	struct capi_ctr *			kctrl;
} per_ctrl_t, * per_ctrl_p;

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
typedef struct __bundle {

	card_p					card;
	per_ctrl_p				ctrl;
} bundle_t, * bundle_p;

#define	GET_CARD(ctr)	(((bundle_p) (ctr)->driverdata)->card)
#define	GET_CTRL(ctr)	(((bundle_p) (ctr)->driverdata)->ctrl)

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
#define UNDEF_CONFIG				-1
#define RESET_CONFIG				0
#define LOADING_CONFIG				1
#define RUNNING_CONFIG				1

extern int					card_config;

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
extern card_p					capi_card;
extern lib_callback_t *				capi_lib;
extern struct capi_ctr *			capi_controller[2];
extern volatile unsigned long			stack_lock;

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
extern int os_usb_submit_rx (int, void *, unsigned, hw_completion_func_t, hw_buffer_descriptor_p);
extern int os_usb_submit_tx (int, void *, unsigned, hw_completion_func_t, hw_buffer_descriptor_p);

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
extern void start_closing_worker (void (*) (void *), void *);
extern void init_closing_worker (void);

extern void shutdown (void);

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
extern void kick_scheduler (void);

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
extern void enter_critical (void);
extern void leave_critical (void);

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
extern void init (unsigned, void (__attr2 *) (void *, unsigned),
			    void (__attr2 *) (void *),
			    void (__attr2 *) (void));

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/

#endif

