/* 
 * image.h
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

#ifndef __have_image_h__
#define __have_image_h__

#include "libdefs.h"

#define IMAGE_OK                0
#define IMAGE_RELOC_ERROR       -1

#define IMAGE_FREQUENCY         166665000
#define IMAGE_ADD_ROUND(x,y,z)  ((((unsigned int) (x))+(y)+(z)-1)&~((z)-1))
#define	XFER_INFO_MAX		1

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
typedef struct __img image_t, * image_p;

typedef	enum __xfers {

	INVALID,
	TM_PENDING,
	TM_ENQUEUED,
	TM_READY,
	PC_PENDING
} xfer_state_t;

typedef enum __xfert {

	TO_TM,
	TO_PC
} xfer_type_t;

typedef struct __xferi {

	xfer_state_t	state;
	void *		address;
	unsigned	length;
#if defined (TM_TIMESTAMP)
	unsigned	stamp;
#endif
} xfer_info_t, * xfer_info_p;

typedef struct __xferb {

	xfer_type_t	type;
	int		initialized;
	unsigned	__dummy1;
	xfer_info_t	info[XFER_INFO_MAX];
	unsigned	__dummy2;
} xfer_base_t, * xfer_base_p;

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
extern xfer_base_p	tx_info;
extern xfer_base_p	rx_info;

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
extern __attr2 image_p lib_init (void *, unsigned long, unsigned long, 
				 unsigned long, void *, unsigned);
extern __attr2 void lib_remove (image_p *);

extern __attr2 const char * lib_error (int);

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
extern __attr2 int lib_reloc (image_p);
extern __attr2 int lib_patch (image_p, const char *, unsigned long);
extern __attr2 int lib_getsize (image_p, int *, int *);
extern __attr2 int lib_getimage (image_p, unsigned, unsigned);
extern __attr2 int lib_load_symtab (image_p);
extern __attr2 int lib_readsym (image_p, char *, unsigned long *);
extern __attr2 int lib_findsym (image_p, char *, unsigned long *);
extern __attr2 int lib_load (image_p);

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
extern __attr2 int xfer_info_init (xfer_base_p, xfer_type_t);
extern __attr2 void xfer_info_reset (xfer_base_p);

extern __attr2 void xfer_info_set (xfer_base_p, unsigned, xfer_info_p);
extern __attr2 void xfer_info_get (xfer_base_p, unsigned, xfer_info_p);

extern __attr2 void xfer_info_write (
		xfer_base_p, 
		unsigned, 
		xfer_state_t, 
		void *, 
		unsigned
		);

extern __attr2 void xfer_info_read (
		xfer_base_p,
		unsigned,
		xfer_state_t *,
		void **,
		unsigned *
#if defined (TM_TIMESTAMP)
,		unsigned *
#endif
		);

#define	xfer_info_ptr(base)	(&base.pinfo[0])
#define	xfer_info_num(base)	XFER_INFO_MAX
#define	xfer_info_type(base)	(base.type)

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/

#endif

