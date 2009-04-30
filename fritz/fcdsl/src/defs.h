/* 
 * defs.h
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

#ifndef __have_defs_h__
#define __have_defs_h__

#ifndef LINUX_VERSION_CODE
# include <linux/version.h>
#endif

#ifndef TRUE
# define TRUE	(1==1)
# define FALSE	(1==0)
#endif

#if !defined(SA_SHIRQ)
#define SA_SHIRQ IRQF_SHARED
#endif
#if !defined(SA_INTERRUPT)
#define SA_INTERRUPT IRQF_DISABLED
#endif

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
#if defined (__fcdsl__)
# define PRODUCT_LOGO			"AVM FRITZ!Card DSL"
# define INTERFACE			"pci"
#else
# error You have to define a card identifier...
#endif

#define SHORT_LOGO			"fcdsl-" INTERFACE
#define DRIVER_LOGO			PRODUCT_LOGO " driver"
#define	DRIVER_TYPE_ISDN
#define	DRIVER_TYPE_DSL
#define	DRIVER_TYPE_DSL_TM
#define	DRIVER_REV			"0.5.2"

#define PCI_DEVICE_ID_TM                0x5402
#define PCI_DEVICE_ID_AVM_FCDSL         0x0f00

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
#if defined (OSDEBUG) && defined (NDEBUG)
# undef NDEBUG
#endif

#define	UNUSED_ARG(x)	(x)=(x)

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
#endif
