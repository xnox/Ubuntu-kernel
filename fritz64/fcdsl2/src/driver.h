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
#include <linux/pci.h>
#include <linux/spinlock.h>
#include <linux/capi.h>
#include <linux/isdn/capilli.h>
#include "tables.h"
#include "queue.h" 
#include "libdefs.h" 
#include "common.h"

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
struct __card {

	/* Software */
	int			ctrl2;
	void *			ctxp;
	unsigned		length;
	unsigned		count;
	appltab_t *		appls;
	queue_t *		queue;
	int			running;

	void (__attr2 *		reg_func) (void *, unsigned);
	void (__attr2 *		rel_func) (void *);
	void (__attr2 *		dwn_func) (void);

	/* Hardware */
	
	c6205_context		c6205_ctx;
	ioaddr_p		piowin;

	unsigned long		addr_data;
	unsigned long		len_data;
	void *			data_base;
	int			data_access_ok;

	unsigned long		addr_code;
	unsigned long		len_code;
	void *			code_base;
	int			code_access_ok;

	unsigned long		addr_mmio;
	unsigned long		len_mmio;
	unsigned		mmio_base;
	int			mmio_access_ok;

	int			irq;
	
	/* TX */
	void *			tx_dmabuf;
	void *			tx_dmabuf_b;
	void *			tx_base;
	void *			tx_buffer;
	unsigned		tx_length;
	hw_completion_func_t	tx_func;
	void *			tx_ctx;
	
	/* RX */
	void *			rx_dmabuf;
	void *			rx_dmabuf_b;
	void *			rx_base;
	void *			rx_buffer;
	unsigned		rx_length;
	hw_completion_func_t	rx_func;
	void *			rx_ctx;
} ;

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
typedef struct __per_ctrl {
#if !defined (NDEBUG)
	int			cid;
#endif
	char *			version;
	char *			string[8];
	struct capi_ctr *	kctrl;
} per_ctrl_t, * per_ctrl_p;

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
typedef struct __bundle {

	card_p		card;
	per_ctrl_p	ctrl;
} bundle_t, * bundle_p;

#define	GET_CARD(ctr)	(((bundle_p) (ctr)->driverdata)->card)
#define	GET_CTRL(ctr)	(((bundle_p) (ctr)->driverdata)->ctrl)

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
extern card_p				capi_card;
extern lib_callback_t *			capi_lib;
extern struct capi_ctr *		capi_controller[2];
extern atomic_t				crit_count;
extern volatile unsigned long		stack_lock;

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
extern void remove_ctrls (card_p); 
extern int add_card (struct pci_dev *); 

extern int nbchans (struct capi_ctr *);

extern int msg2stack (unsigned char *);
extern void msg2capi (unsigned char *);

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
extern __attr void stack_wait (unsigned);
extern __attr unsigned get_timer (void);
extern __attr void pc_acknowledge (unsigned char);

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
extern void enter_critical (void);
extern void leave_critical (void);

static inline int in_critical (void) {

	return (0 < atomic_read (&crit_count));
} /* in_critical */

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
extern int fritz_driver_init (void);
extern void driver_exit (void);

#endif

