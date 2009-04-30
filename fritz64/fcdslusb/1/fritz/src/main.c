/* 
 * main.c
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

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/ctype.h>
#include <linux/skbuff.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/capi.h>
#include <linux/usb.h>
#include <linux/isdn/capilli.h>
#include "lib.h"
#include "tools.h"
#include "driver.h"
#include "buffers.h"
#include "main.h"

#define	INIT_FAILED		-1
#define	INIT_SUCCESS		0

#define	VENDOR_ID_AVM		0x057C
#if   defined (__fcdslusb__)
#define	PRODUCT_ID		0x2300
#else
#error You have to define a card identifier!
#endif

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
short int	VCC = 0x01;
short int	VPI = 0x01;
short int	VCI = 0x20;

module_param (VCC, short, 0);
module_param (VPI, short, 0);
module_param (VCI, short, 0);

MODULE_PARM_DESC (VCC, "VCC - Virtual Channel Connection");
MODULE_PARM_DESC (VPI, "VPI - Virtual Path Identifier");
MODULE_PARM_DESC (VCI, "VCI - Virtual Channel Identifier");

MODULE_LICENSE ("Proprietary");
MODULE_DESCRIPTION ("CAPI4Linux: Driver for " PRODUCT_LOGO);

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static int usb_probe (struct usb_interface *, const struct usb_device_id *);
static void usb_disconnect (struct usb_interface *);

static struct usb_device_id usb_id_table[] = {
	
	{ USB_DEVICE(VENDOR_ID_AVM, PRODUCT_ID) },
	{ /* Terminating entry */ }
} ;

static struct usb_driver usb_driver = {

//	.owner = THIS_MODULE,
	.name =		TARGET,
	.id_table =	usb_id_table,
	.probe =	usb_probe,
	.disconnect =	usb_disconnect,
} ;

MODULE_DEVICE_TABLE (usb, usb_id_table);

static struct capi_driver usb_capi_driver = {

	.name =		TARGET,
	.revision =	DRIVER_REV,
} ;

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static char * 		REVCONST = "$Revision: $";
char          		REVISION[32];

static int		mod_count = 0;

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
#if !defined (NDEBUG)
static void base_address (void) {

	LOG("Base address: %p\n", base_address);
	LOG("Compile time: %s\n", __TIME__);
} /* base_address*/
#endif

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
void inc_use_count (void) { 
	
	++mod_count;
} /* inc_use_count */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
void dec_use_count (void) {
	
	assert (mod_count > 0);
	--mod_count;
} /* dec_use_count */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static int usb_probe (
	struct usb_interface *		iface, 
	const struct usb_device_id *	devid
) {
	card_p				pdc;
	struct usb_device *		dev;
	struct usb_config_descriptor *	cfgd;
	
	UNUSED_ARG (devid);

	dev = interface_to_usbdev (iface);
	assert (dev != NULL);
	
	if ((VENDOR_ID_AVM != dev->descriptor.idVendor)
	||  (PRODUCT_ID != dev->descriptor.idProduct)) 
	{
	exit_nodev:
		LOG("Unknown device!\n");
		return -ENODEV;
	}
	
	LOG(
		"Probe for device #%d, vendor %x, product %x.\n", 
		dev->devnum,
		dev->descriptor.idVendor,
		dev->descriptor.idProduct
	);
	if (NULL == (pdc = (card_p) hmalloc (sizeof (card_t)))) {
		ERROR("Could not allocate device context.\n");
	exit_noctx:
		goto exit_nodev;
	}
	capi_card = pdc;
	memset (pdc, 0, sizeof (card_t));
	pdc->dev = dev;
	atomic_set (&pdc->is_open, 0);

	/* Interface */
	cfgd = &dev->config[0].desc;
	assert (cfgd != NULL);
	pdc->epwrite = &iface->altsetting[0].endpoint[0].desc;
        pdc->epread  = &iface->altsetting[0].endpoint[1].desc;
	
	/* Configuration */
	card_config = cfgd->bConfigurationValue;
	LOG("Device is in config #%d.\n", card_config);

	/* Buffers, URBs, etc. ... */
	if (NULL == (pdc->tx_pool = init_urbs (MAX_URB_COUNT_TX, MAX_TRANSFER_SIZE))) {
		ERROR("Error while allocating Tx URBs.\n");
	exit_notxurbs:
		goto exit_noctx;
	}
	if (!build_urb_contexts (pdc->tx_pool, make_ctx, kill_ctx)) {
		ERROR("Error while initializing Tx URBs.\n");
	exit_notxurbsinit:
		remove_urbs(&pdc->tx_pool, kill_ctx);
		goto exit_notxurbs;
	}
	if (NULL == (pdc->rx_pool = init_urbs (MAX_URB_COUNT_RX, MAX_TRANSFER_SIZE))) {
		ERROR("Error while allocating Rx URBs.\n");
	exit_norxurbs:
		goto exit_notxurbsinit;
	}
	if (!build_urb_contexts (pdc->rx_pool, make_ctx, kill_ctx)) {
		ERROR("Error while initializing Rx URBs.\n");
	exit_norxurbsinit:
		remove_urbs(&pdc->rx_pool, kill_ctx);
		goto exit_norxurbs;
	}
	if (NULL == (capi_lib = link_library (&capi_card))) {
		ERROR("Linking to library failed.\n");
	exit_nolib:
		goto exit_norxurbsinit;
	}
	init_closing_worker ();
	make_thread ();
	if (0 != add_card (dev)) {
		kill_thread ();
		free_library ();
		goto exit_nolib;
	}
	usb_set_intfdata (iface, pdc);
	return 0;
} /* usb_probe */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static void usb_disconnect (struct usb_interface * iface) {
#if !defined (NDEBUG)
	struct usb_device *	dev = interface_to_usbdev (iface);
#endif
	card_p			pdc;

	assert (dev != NULL);
	pdc = usb_get_intfdata (iface);
	assert (pdc != NULL);
	LOG("Disconnect for device #%d.\n", dev->devnum);
	kill_thread ();
	start_closing_worker (NULL, NULL);
} /* usb_disconnect */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static int __init usb_init (void) {
	int	urr, res = INIT_FAILED;
	char *	tmp;

#if !defined (NDEBUG)
	base_address ();
#endif
	if ((NULL != (tmp = strchr (REVCONST, ':'))) && isdigit (*(tmp + 2))) {
		lib_strncpy (REVISION, tmp + 1, sizeof (REVISION));
		tmp = strchr (REVISION, '$');
		*tmp = 0;
	} else {
		lib_strcpy (REVISION, DRIVER_REV);
	}
        NOTE("%s, revision %s\n", DRIVER_LOGO, REVISION);
	NOTE("(%s built on %s at %s)\n", TARGET, __DATE__, __TIME__);

#ifdef __LP64__
	NOTE("-- 64 bit CAPI driver --\n");
#else
	NOTE("-- 32 bit CAPI driver --\n");
#endif

	NOTE("Loading...\n");
	if (0 > (urr = usb_register (&usb_driver))) {
		ERROR("Failed to register USB driver! (%d)\n", urr);
		goto exit;
	}
	register_capi_driver (&usb_capi_driver);
	res = INIT_SUCCESS;
exit:
	NOTE("%soaded.\n", res == INIT_SUCCESS ? "L" : "Not l");
	return res;
} /* usb_init */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static void __exit usb_exit (void) {
#ifndef NDEBUG
	unsigned n;
#endif
	NOTE("Removing...\n");
	unregister_capi_driver (&usb_capi_driver);
	usb_deregister (&usb_driver);
#ifndef NDEBUG
	if (0 != (n = hallocated ())) {
		ERROR("%u(%d) bytes leaked...\n", n, (int) n);
	}
#endif
	NOTE("Removed.\n");
	assert (mod_count == 0);
} /* usb_exit */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
module_init (usb_init);
module_exit (usb_exit);

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/

