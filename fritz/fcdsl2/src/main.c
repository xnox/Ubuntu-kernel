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

#include <stdarg.h>
#include <asm/uaccess.h>
#include <linux/pci.h>

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/skbuff.h>
#include <linux/errno.h>
#include <linux/capi.h>
#include <linux/ctype.h>
#include <linux/isdn/capilli.h>
#include "driver.h" 
#include "lib.h"
#include "tools.h"
#include "defs.h"

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static char *	REVCONST = "$Revision: $";
char		REVISION[32];

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static struct pci_device_id fcdsl2_id_table[] = {

	{ PCI_VENDOR_ID_AVM, PCI_DEVICE_ID_FCDSL2,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 },
        { /* Terminating entry */ }
} ;

MODULE_DEVICE_TABLE (pci, fcdsl2_id_table);

static int	mod_count	= 0;

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
short int	VCC		= 0x01;
short int	VPI		= 0x01;
short int	VCI		= 0x20;

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
#ifndef NDEBUG
static void base_address (void) {

	LOG("Base address: %p\n", base_address);
	LOG("Compile time: %s\n", __TIME__);
} /* base_address */
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
static int __devinit fcdsl2_probe (
	struct pci_dev *		dev,
	const struct pci_device_id *	id
) {
	int				res;

	assert (dev != NULL);
	UNUSED_ARG(id);
	if (pci_enable_device (dev) < 0) {
		ERROR("Error: Failed to enable " PRODUCT_LOGO "!\n");
		return -ENODEV;
	}
	NOTE("Loading...\n");
	if (!fritz_driver_init ()) {
		ERROR("Error: Driver library not available.\n");
		NOTE("Not loaded.\n");
		return -ENOSYS;
	}
	if (0 != (res = add_card (dev))) {
		NOTE("Not loaded.\n");
		driver_exit ();
		return res;
	}
	NOTE("Loaded.\n");
	assert (capi_card != NULL);
	pci_set_drvdata (dev, capi_card);
	
	return 0;
} /* fcdsl2_probe */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static void __devexit fcdsl2_remove (struct pci_dev * dev) {
	card_p	cp;

	cp = pci_get_drvdata (dev);
	assert (cp != NULL);

	NOTE("Removing...\n");
	remove_ctrls (cp);
	NOTE("Removed.\n");
	driver_exit ();
#ifndef NDEBUG
	if (hallocated() != 0) {
		ERROR("%u bytes leaked.\n", hallocated());
	}
#endif
} /* fcdsl2_remove  */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static struct pci_driver	fcdsl2_driver = {

	.name		= TARGET,
	.id_table	= fcdsl2_id_table,
	.probe		= fcdsl2_probe,
	.remove		= __devexit_p(fcdsl2_remove),
} ;

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static struct capi_driver	fcdsl2_capi_driver = {

	.name		= TARGET,
	.revision	= DRIVER_REV,
} ;

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static int __init fcdsl2_init (void) {
	int	err;
	char *	tmp;
	
#ifndef NDEBUG
	base_address ();
#endif
	if ((NULL != (tmp = strchr (REVCONST, ':'))) && isdigit (*(tmp + 2))) {
		lib_strncpy (REVISION, tmp + 1, sizeof (REVISION));
		tmp = strchr (REVISION, '$');
		*tmp = 0;
	} else {
		lib_strncpy (REVISION, DRIVER_REV, sizeof (REVISION));
	}
	NOTE("%s, revision %s\n", DRIVER_LOGO, REVISION);
        NOTE("(%s built on %s at %s)\n", TARGET, __DATE__, __TIME__);

#ifdef __LP64__
	NOTE("-- 64 bit CAPI driver --\n");
#else
	NOTE("-- 32 bit CAPI driver --\n");
#endif

	if (0 == (err = pci_register_driver (&fcdsl2_driver))) {
		LOG("PCI driver registered.\n");
		register_capi_driver (&fcdsl2_capi_driver);
		LOG("CAPI driver registered.\n");
	}
	return err;
} /* fcdsl_init */
	
/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static void __exit fcdsl2_exit (void) {

	unregister_capi_driver (&fcdsl2_capi_driver);
	LOG("CAPI driver unregistered.\n");
	pci_unregister_driver (&fcdsl2_driver);	
	LOG("PCI driver unregistered.\n");
	assert (mod_count == 0);
} /* fcdsl_exit */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
module_init (fcdsl2_init);
module_exit (fcdsl2_exit);

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/

