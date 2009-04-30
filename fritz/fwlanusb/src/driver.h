/* 
 * driver.h
 * Copyright (C) 2007, AVM GmbH. All rights reserved.
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
#include <linux/usb.h>
#include <linux/netdevice.h>
#include "buffers.h"
#include "common.h"
#include "lib.h"

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
#define	MAX_TRANSFER_SIZE	4116
#define MAX_EP0_IN		384
#define MAX_EP0_OUT		2048	
#define MAX_URB_COUNT_CMD	32
#define MAX_URB_COUNT_TX	32
#define MAX_URB_COUNT_RX	8

#define VENDOR_SPECIFIC_REQUEST	18
#define STACK_USB_OK		0
#define STACK_USB_ERROR 	0xFFFFFFFF

#define STACK_OK		0
#define STACK_ERROR		1
#define STACK_TRUE		1
#define STACK_FALSE		0

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
typedef struct {

	/*Stack*/
	void				*system_lock;
	void				*stack_ctx;
	unsigned char			do_not_touch_me[500];

	/*USB*/
	struct usb_device	 	*usb_dev;
	struct usb_endpoint_descriptor	*epread;
	struct usb_endpoint_descriptor	*epwrite;
	buffer_pool_p			cmd_pool;
	buffer_pool_p			tx_pool;
	buffer_pool_p			rx_pool;
	void				*buffer;
	int				len;

	/*Network*/
	struct net_device		*net_dev;
	struct net_device_stats		net_stats;

	unsigned long			start_scan;
	atomic_t			shutdown;

} card_t, *card_p;

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
typedef struct __urb_context {

	usb_caller_extension_t 	caller_extension;
	usb_caller_extension_t	caller_error;
	unsigned char 		*buffer;		/*EP1 URBs*/
	struct usb_ctrlrequest	reg;			/*EP0 URBs*/
	atomic_t		ref;			/*EP1 IN URBs*/
	card_p			adapter;

} urb_context_t, *urb_context_p;

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
extern card_p			wlan_card;
#ifndef __WITHOUT_INTERFACE__	
extern lib_callback_p		wlan_lib;
#endif
extern unsigned char		scsi_stop_unit[31];

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
int add_card (card_p);
void delete_card (card_p);
void usb_start_read (card_p pdc);
unsigned int usb_read_write_sync (
	struct usb_device *, 
	unsigned int, 
	unsigned char *, 
	int, 
	int *, 
	int);
unsigned int usb_read_write_cmd_async (
	struct usb_device *, 
	unsigned int, 
	unsigned char *, 
	int, usb_caller_extension_p);
unsigned int usb_write_async (
	struct usb_device *, 
	unsigned int, 
	void *, 
	unsigned short,
	usb_caller_extension_p, 
	usb_caller_extension_p);
int start_recovery_worker (void*, int (__attr2 *) (void*, void*), void*);
int net_rx (struct net_device *, void*, unsigned short);

#endif /*__have_driver_h__*/

