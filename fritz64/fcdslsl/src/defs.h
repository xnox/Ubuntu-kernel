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
#if defined (__fcdslsl__)
# define PCI_DEVICE_ID_FCDSL2	0x2700
# define PRODUCT_LOGO		"AVM FRITZ!Card DSL SL"
#elif defined (__fcdsl2__)
# define PCI_DEVICE_ID_FCDSL2	0x2900
# define PRODUCT_LOGO		"AVM FRITZ!Card DSL v2.0"
# define DRIVER_TYPE_ISDN
#else
# error Card specifier missing!
#endif
#define INTERFACE		"pci"

#define SHORT_LOGO		TARGET "-" INTERFACE
#define DRIVER_LOGO		PRODUCT_LOGO " driver"
#define	DRIVER_TYPE_DSL
#define	DRIVER_TYPE_DSL_RAP
#define	DRIVER_REV		"0.3.2"

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
#if defined (OSDEBUG) && defined (NDEBUG)
# undef NDEBUG
#endif

#define	UNUSED_ARG(x)	(x)=(x)

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
#define	TOOLS_MEM_DUMP
#define	TOOLS_PTR_QUEUE

/* 	
 * The memory mapped i/o and the port i/o share the same interface, i.e. the
 * function prototype using 'unsigned long' address values is the same. This
 * leads to 32-bit adresses for linux32 and to 64-bit addresses for linux64,
 * memory i/o uses 'void *' and should work on both platforms.
 */
#define	MINB(addr)		readb((void *) addr)
#define	MINW(addr)		readw((void *) addr)
#define	MINL(addr)		readl((void *) addr)
#define	MOUTB(addr,v)		writeb((v),((void *) addr))
#define	MOUTW(addr,v)		writew((v),((void *) addr))
#define	MOUTL(addr,v)		writel((v),((void *) addr))

#define	PINB(addr)		inb(addr)
#define	PINW(addr)		inw(addr)
#define	PINL(addr)		inl(addr)
#define	POUTB(addr,v)		outb((v),(addr))
#define	POUTW(addr,v)		outw((v),(addr))
#define	POUTL(addr,v)		outl((v),(addr))

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
#endif
