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
#if defined (__fcusb__)
# define PRODUCT_LOGO		"AVM FRITZ!Card USB"
# define PRODUCT_TYPE		1
#elif defined (__fcusb2__)
# define PRODUCT_LOGO		"AVM FRITZ!Card USB v2"
# define PRODUCT_TYPE		1
#elif defined (__fxusb__)
# define PRODUCT_LOGO		"AVM FRITZ!X USB/FRITZ!X ISDN"
# define PRODUCT_TYPE		1
#elif defined (__teumex2k__)
# define PRODUCT_LOGO		"T-Eumex 2000PC SE"
# define PRODUCT_TYPE		2
#elif defined (__teumex4k__)
# define PRODUCT_LOGO		"T-Eumex 4000PC"
# define PRODUCT_TYPE		2
#elif defined (__e2220pc__)
# define PRODUCT_LOGO		"Eumex 2220PC"
# define PRODUCT_TYPE		2
#elif defined (__e5520pc__)
# define PRODUCT_LOGO		"Eumex 5520PC"
# define PRODUCT_TYPE		2
#else
# error You have to define a card identifier...
#endif
#define	INTERFACE		"usb"

#if PRODUCT_TYPE == 2
# define PRODUCT_NAME		"eumex"
#else
# define PRODUCT_NAME		"fritz"
#endif

#define	SHORT_LOGO		PRODUCT_NAME INTERFACE
#define	DRIVER_LOGO		PRODUCT_LOGO " driver"
#define	DRIVER_TYPE_USB
#define	DRIVER_TYPE_ISDN
#define	DRIVER_REV		"0.6.4"

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
#if defined (OSDEBUG) && defined (NDEBUG)
# undef NDEBUG
#endif

#define	UNUSED_ARG(x)	(x)=(x)

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
#endif

