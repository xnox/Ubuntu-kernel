/* 
 * main.c
 * Copyright (C) 2005, AVM GmbH. All rights reserved.
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
#include <linux/usb.h>
#include "tools.h"
#include "lib.h"
#include "driver.h"

#define	INIT_FAILED		-1
#define	INIT_SUCCESS		0

#define	VENDOR_ID_AVM		0x057C
#if defined (__fwlanusb__)
# define	PRODUCT_ID_1	0x5601
# define	PRODUCT_ID_2	0x6201
# define	PRODUCT_ID_3	0x62FF
#else
# error You have to define a card identifier!
#endif

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
MODULE_LICENSE ("Proprietary");
MODULE_DESCRIPTION ("Driver for " PRODUCT_LOGO);

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static int fwlanusb_probe (struct usb_interface *, const struct usb_device_id *);
static void fwlanusb_disconnect (struct usb_interface *);

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static struct usb_device_id usb_id_table[] = {

	{ USB_DEVICE(VENDOR_ID_AVM, PRODUCT_ID_1) },
	{ USB_DEVICE(VENDOR_ID_AVM, PRODUCT_ID_2) },
	{ USB_DEVICE(VENDOR_ID_AVM, PRODUCT_ID_3) },
	{ /* Terminating entry */ }
} ;

struct usb_driver usb_driver = {

	.name =		TARGET,
	.id_table =	usb_id_table,
	.probe =	fwlanusb_probe,
	.disconnect =	fwlanusb_disconnect,
} ;

MODULE_DEVICE_TABLE (usb, usb_id_table);

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static char * 		REVCONST = "$Revision: $";
static char		REVISION[32];

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
static int fwlanusb_probe (
	struct usb_interface *iface,
	const struct usb_device_id *devid) {
	
	struct usb_device	*dev;
	card_p			pdc;
	unsigned char		*eject;
	int			actual_len;

	dev = interface_to_usbdev (iface);

	/* Device identification */
	if (VENDOR_ID_AVM == dev->descriptor.idVendor) {
		switch (dev->descriptor.idProduct) {
			case PRODUCT_ID_1:
			case PRODUCT_ID_2:
				NOTE("Found FRITZ!Wlan device.\n");
				break;
			case PRODUCT_ID_3:
				NOTE("Found FRITZ!Wlan device in cdrom mode.\n");
				/* Do fast eject to avoid timeout switching to wlan mode. 
				 * Blacklist this product and device id in the usb hotplugger to
				 * prevent loading kernel usb storage/cdrom support. */
				if (NULL == (eject = (unsigned char*) hmalloc (sizeof (scsi_stop_unit))))
					return -ENOMEM;
				lib_memcpy (eject, scsi_stop_unit, sizeof (scsi_stop_unit));
				NOTE("Doing fast eject.\n");
				if (usb_bulk_msg (dev, 
						usb_sndbulkpipe (dev, iface->altsetting[0].endpoint[0].desc.bEndpointAddress), 
						eject, sizeof (scsi_stop_unit), 
						&actual_len, 
						5 * HZ) < 0) {
					ERROR("Fast eject failed.\n");
				}
				hfree (eject);
				return 0;
			default:
				ERROR("Unknown device!\n");
				return -ENODEV;
		}
	} else {
		ERROR("Unknown device!\n");
exit_nodev:
		return -ENODEV;
	}

	/*Device context*/
	if (NULL == (pdc = (card_p) hmalloc (sizeof (card_t)))) {
		ERROR("Could not allocate device context.\n");
		goto exit_nodev;
	}
	wlan_card = pdc;
	lib_memset (pdc, 0, sizeof(card_t));
	pdc->usb_dev = dev;
	
	LOG(
		"Probe for device #%d, vendor %x, product %x.\n", 
		dev->devnum,
		dev->descriptor.idVendor,
		dev->descriptor.idProduct
	);

	/*USB*/
	pdc->epwrite = &iface->altsetting[0].endpoint[0].desc;
        pdc->epread  = &iface->altsetting[0].endpoint[1].desc;
	
#ifndef __WITHOUT_INTERFACE__
	/*Stack*/
	if (NULL == (wlan_lib = link_library (pdc))) {
		ERROR("Linking to library failed.\n");
exit_nolib:
		hfree(wlan_card);
		goto exit_nodev;
	}
#endif

	if (add_card (pdc) < 0) {
#ifndef __WITHOUT_INTERFACE__
		free_library ();
		goto exit_nolib;
#else
		hfree(wlan_card);
		goto exit_nodev;
#endif
	}	
	
	return 0;
} /* usb_probe */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static void fwlanusb_disconnect (struct usb_interface * iface) {

	struct usb_device	*dev;

	LOG("usb_disconnect called.\n");

	dev = interface_to_usbdev (iface);

	if (dev->descriptor.idProduct == PRODUCT_ID_3) {
		return;
	}

	delete_card (wlan_card);
	free_library ();
	hfree (wlan_card);

} /* usb_disconnect */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static int __init fwlanusb_init (void) {
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
	NOTE("-- 64 bit driver --\n");
#else
	NOTE("-- 32 bit driver --\n");
#endif

	NOTE("Loading...\n");
	if (0 != (urr = usb_register (&usb_driver))) {
		ERROR("Failed to register USB driver! (%d)\n", urr);
		goto exit;
	}
	res = INIT_SUCCESS;
exit:
	NOTE("%soaded.\n", res == INIT_SUCCESS ? "L" : "Not l");
	return res;
} /* fwlanusb_init */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static void __exit fwlanusb_exit (void) {
#ifndef NDEBUG
	unsigned u;
#endif
	NOTE("Removing...\n");
	usb_deregister (&usb_driver);
#ifndef NDEBUG
	if (0 != (u = hallocated ())) {
		LOG("%u bytes leaked...\n", u);
	}
#endif
	NOTE("Removed.\n");
} /* fwlanusb_exit */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
module_init (fwlanusb_init);
module_exit (fwlanusb_exit);

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/

