/* 
 * fw.h
 * Copyright (C) 2003, AVM GmbH. All rights reserved.
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

#ifndef __have_fw_h__
#define __have_fw_h__

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
#include "libdefs.h"
#include "common.h"

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
extern __attr2 ioaddr_p fw_get_window (unsigned);

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
extern __attr2 c6205_context fw_init (card_p, void *, unsigned, 
					unsigned, ioaddr_p, int *);
extern __attr2 void fw_exit (c6205_context *);

extern __attr2 int fw_ready (void);
extern __attr2 int fw_setup (c6205_context);
extern __attr2 int fw_send_reset (c6205_context);
extern __attr2 unsigned fw_get_timer_offset (c6205_context);

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/

#endif

