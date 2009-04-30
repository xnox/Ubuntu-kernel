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
#include <linux/init.h>
#include <linux/capi.h>
#include <linux/usb.h>
#include <linux/isdn/capilli.h>
#include "tools.h"
#include "lib.h"
#include "driver.h"

#define	INIT_FAILED		-1
#define	INIT_SUCCESS		0

#define	VENDOR_ID_AVM		0x057C
#if defined (__fcusb__)
#define	PRODUCT_ID		0x0C00
#elif defined (__fcusb2__)
#define	PRODUCT_ID		0x1000
#define	PRODUCT_ID2		0x1900
#elif defined (__fxusb__)
#define	PRODUCT_ID		0x2000
#elif defined (__teumex2k__)
#define	PRODUCT_ID		0x2801
#elif defined (__teumex4k__)
#define	PRODUCT_ID		0x2802
#elif defined (__e2220pc__)
#define	PRODUCT_ID		0x2805
#define	PRODUCT_ID2		0x4401
#elif defined (__e5520pc__)
#define	PRODUCT_ID		0x2806
#define	PRODUCT_ID2		0x4601
#else
#error You have to define a card identifier!
#endif

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
MODULE_LICENSE ("Proprietary");
MODULE_DESCRIPTION ("CAPI4Linux: Driver for " PRODUCT_LOGO);

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static int usb_probe (struct usb_interface *, const struct usb_device_id *);
static void usb_disconnect (struct usb_interface *);

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static struct usb_device_id usb_id_table[] = {

	{ USB_DEVICE(VENDOR_ID_AVM, PRODUCT_ID) },
#if defined (PRODUCT_ID2)
	{ USB_DEVICE(VENDOR_ID_AVM, PRODUCT_ID2) },
#endif
	{ /* Terminating entry */ }
} ;

struct usb_driver usb_driver = {

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
static char		REVISION[32];

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

void dec_use_count (void) {

	assert (mod_count > 0);
	--mod_count;
} /* dec_use_count */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
#if defined (__fxusb__)
static void chk_device (unsigned short bcd, int * dc) {
	int	code;
	
	switch (bcd) {
	
	case 0x0100:	code = 1; break;
	case 0x0301:	code = 2; break;
	case 0x0302:	code = 3; break;
	default:	code = 0; break;
	}
	LOG("Device code %02hx --> id %d\n", bcd, code);
	assert (dc != NULL);
	*dc = code;
} /* chk_device */
#endif

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static int usb_probe (
	struct usb_interface *			iface,
	const struct usb_device_id *		devid
) {
	struct usb_device *			dev;
	card_p					pdc;
#if !defined (__fcusb2__)
	struct usb_config_descriptor *		cfgd;
#else
	struct usb_config_descriptor *		cfgd1;
	struct usb_config_descriptor *		cfgd2;
#endif
#if defined (__fxusb__)
	int					dc = 0;
#endif
	UNUSED_ARG (devid);

	dev = interface_to_usbdev (iface);
	assert (dev != NULL);
	assert (iface->altsetting->desc.bInterfaceNumber == 0);

	/* Device identification */
#if defined (PRODUCT_ID2)
	if ( (VENDOR_ID_AVM != dev->descriptor.idVendor)  || 
	     (  
	       (PRODUCT_ID  != dev->descriptor.idProduct)  &&
	       (PRODUCT_ID2 != dev->descriptor.idProduct)
	   ) ) 
#else
	if ((VENDOR_ID_AVM != dev->descriptor.idVendor)
	||  (PRODUCT_ID != dev->descriptor.idProduct)) 
#endif
	{
	exit_nodev:
		ERROR("Unknown device!\n");
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
	lib_memset (pdc, 0, sizeof (card_t));
	pdc->dev = dev;
	atomic_set (&pdc->is_open, 0);
#if defined (__fxusb__)
	chk_device (dev->descriptor.bcdDevice, &dc);
	pdc->dc = dc;
#endif

#if defined (__fcusb2__)
	/* Claiming interfaces seems to be completely broken, try 
	 * to get along without claiming them... 
	 */
	cfgd1 = &dev->config[0].desc;
	cfgd2 = &dev->config[1].desc;
	pdc->if1 = dev->config[0].interface[0];
	pdc->if2 = dev->config[1].interface[0];
#else
	cfgd = &dev->config[0].desc;
#endif
	pdc->epwrite = &iface->altsetting[0].endpoint[0].desc;
	pdc->epread  = &iface->altsetting[0].endpoint[1].desc;

	/* Configuration */
#if !defined (__fcusb2__)
	card_config = cfgd->bConfigurationValue;
#else
	card_config = cfgd1->bConfigurationValue;
#endif
	LOG("Device is in config #%d.\n", card_config);

	/* Buffers and the like... */
	if (NULL == (pdc->tx_buffer = (char *) hmalloc (MAX_TRANSFER_SIZE))) {
		ERROR("Could not allocate tx buffer.\n");
	exit_notxbuf:
		hfree (pdc);
		goto exit_noctx;
	}
	if (NULL == (pdc->rx_buffer = (char *) hmalloc (MAX_TRANSFER_SIZE))) {
		ERROR("Could not allocate rx buffer.\n");
	exit_norxbuf:
		hfree (pdc->tx_buffer);
		goto exit_notxbuf;
	}
	if (NULL == (pdc->tx_urb = usb_alloc_urb (0, GFP_ATOMIC))) {
		ERROR("Could not allocate tx urb.\n");
	exit_notxurb:
		hfree (pdc->rx_buffer);
		goto exit_norxbuf;
	}
	if (NULL == (pdc->rx_urb = usb_alloc_urb (0, GFP_ATOMIC))) {
		ERROR("Could not allcate rx urb.\n");
	exit_norxurb:
		usb_free_urb (pdc->tx_urb);
		goto exit_notxurb;
	}

	/* Protocoll stack */
	if (NULL == (capi_lib = link_library (pdc))) {
		ERROR("Linking to library failed.\n");
	exit_nolib:
		usb_free_urb (pdc->rx_urb);
		goto exit_norxurb;
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
	pdc = (card_p) usb_get_intfdata (iface);
	assert (pdc != NULL);
	LOG("Disconnect for device #%d.\n", dev->devnum);
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
	if (0 != (urr = usb_register (&usb_driver))) {
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
	unsigned u;
#endif
	NOTE("Removing...\n");
	kill_thread ();
	unregister_capi_driver (&usb_capi_driver);
	usb_deregister (&usb_driver);
#ifndef NDEBUG
	if (0 != (u = hallocated ())) {
		LOG("%u bytes leaked...\n", u);
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

