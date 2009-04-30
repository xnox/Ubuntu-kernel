/* 
 * buffers.h
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

#ifndef __have_buffers_h__
#define __have_buffers_h__

#include <linux/usb.h>
#include <linux/spinlock.h>

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/

#define URB_WAITING		-6
#define	URB_SUBMITTED		-1
#define	URB_COMPLETED		-2
#define	URB_PROCESSING		-3
#define URB_PROCESSING_DATA	-7
#define URB_DONE		-8
#define	URB_FREE		-4
#define	URB_UNLINKED		-5

typedef struct __ubf {

	struct urb *		urb;
	int			flag;
	void *			ctx;
} ubf_t, * ubf_p;

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
typedef struct __pool {

	unsigned		count;
	unsigned		size;
	spinlock_t		lock;
	
	int			free_in;
	int			free_out;
	int			todo_in;
	int			todo_out;
	int			wait_in;
	int			wait_out;
	int			data_in;
	int			data_out;
	
	atomic_t		free_n;
	atomic_t		todo_n;
	atomic_t		wait_n;
	atomic_t		data_n;
	
	ubf_p			ubf;
	unsigned *		free_ubf;
	unsigned *		todo_ubf;
	unsigned *		wait_ubf;
	unsigned *		data_ubf;
} buffer_pool_t, * buffer_pool_p;

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
typedef int (* ctx_func_t) (void **);

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
extern buffer_pool_p init_urbs (unsigned, unsigned);
extern int build_urb_contexts (buffer_pool_p, ctx_func_t, ctx_func_t);

extern void remove_urbs (buffer_pool_p *, ctx_func_t);
extern void free_urbs (buffer_pool_p, ctx_func_t);

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
extern int unlink_urbs (buffer_pool_p, int);
extern void clear_urbs (buffer_pool_p);

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
extern int claim_urb (buffer_pool_p, struct urb **, void **);
extern void release_urb (buffer_pool_p, struct urb *);

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
extern int need_serialize_wait_urb (buffer_pool_p pool, struct urb *purb);
extern int need_start_serialized_urb (buffer_pool_p pool, struct urb ** ppurb);

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
extern void enlist_urb (buffer_pool_p, struct urb *);
extern int unlist_urb (buffer_pool_p, struct urb **, void **);
extern int urb_waiting (buffer_pool_p);

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
extern void enlist_urb_for_data (buffer_pool_p, struct urb *);
extern int unlist_urb_for_data (buffer_pool_p, struct urb **, void **);

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
/*FIXME: Need locking? */
#define POOL_GET_FREE(pool) 	(atomic_read(&pool->free_n))
#define POOL_GET_TODO(pool) 	(atomic_read(&pool->todo_n))
#define POOL_GET_WAIT(pool) 	(atomic_read(&pool->wait_n))
#define POOL_GET_DATA(pool) 	(atomic_read(&pool->data_n))
extern void get_ctx (struct urb *purb, buffer_pool_p pool, void **pctx);

#endif
