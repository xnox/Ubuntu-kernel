/* 
 * buffers.c
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

#include <asm/atomic.h>
#include <linux/usb.h>
#include "defs.h"
#include "tools.h"
#include "buffers.h"
#include "common.h"

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
#define	lock_pool(p)		spin_lock_irqsave(&p->lock,flags)
#define	unlock_pool(p)		spin_unlock_irqrestore(&p->lock,flags)

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,8)
#define	UNLINK_URB(purb)	usb_unlink_urb(purb)
#else
#define	UNLINK_URB(purb)	usb_kill_urb(purb)
#endif

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
buffer_pool_p init_urbs (unsigned count, unsigned size) {
	buffer_pool_p	pool = NULL;
	unsigned	xxxx = count + 1;
	uintptr_t	ix;

	assert (count > 0);
	assert (size > 0);
	LOG("Allocating %u URBs, %u byte buffers\n", count, size);
	if (NULL == (pool = (buffer_pool_p) hcalloc (sizeof (buffer_pool_t)))) {
		ERROR("Error: Could not allocate buffer structure!\n");
	no_mem_head:
		return NULL;
	}
	if (NULL == (pool->ubf = (ubf_p) hcalloc (xxxx * sizeof (ubf_t)))) {
		ERROR("Error: Could not allocate buffer table! (%u)\n", xxxx);
	no_mem_tab:
		hfree (pool);
		goto no_mem_head;
	}
	if (NULL == (pool->free_ubf = (unsigned *) hcalloc (2 * xxxx * sizeof (unsigned)))) {
		ERROR("Error: Could not allocate buffer control tables! (%u)\n", xxxx);
	no_mem_ctrl:
		hfree (pool->ubf);
		goto no_mem_tab;
	}
	pool->todo_ubf = pool->free_ubf + xxxx;

	for (ix = 0; ix < count; ix++) {
		pool->free_ubf[ix] = ix;
		pool->ubf[ix].flag = URB_DONE;
		pool->ubf[ix].ctx  = NULL;
		if (NULL == (pool->ubf[ix].urb = usb_alloc_urb (0, GFP_ATOMIC))) {
			ERROR("Error: Could not alloc all %u URBs.\n", count);
			while (ix > 0) {
				usb_free_urb (pool->ubf[--ix].urb);
			} 
			goto no_mem_ctrl;
		}
		pool->ubf[ix].urb->context = (void *) ix;
	}
	assert (ix == count);
	pool->ubf[ix].urb = NULL;
	pool->ubf[ix].ctx = NULL;
	pool->count	= count;
	pool->size	= size;
	pool->free_in	= 0;
	pool->free_out	= 0;
	pool->todo_in	= 0;
	pool->todo_out	= 0;
	atomic_set (&pool->free_n, count);
	atomic_set (&pool->todo_n, 0);

	LOG("Allocated %u URBs/buffers.\n", ix);
	return pool;
} /* init_urbs */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
int build_urb_contexts (buffer_pool_p pool, ctx_func_t make_func, ctx_func_t kill_func) {
	unsigned	ix;

	assert (pool != NULL);
	assert (pool->ubf != NULL);
	assert (make_func != NULL);
	assert (kill_func != NULL);
	for (ix = 0; ix < pool->count; ix++) {
		LOG("Context generation callback for URB #%u...\n", ix);
		if (!(* make_func) (&pool->ubf[ix].ctx)) 
			break;
		assert (pool->ubf[ix].ctx != NULL);
	}
	if (ix == pool->count) {
		return TRUE;
	}
	while (--ix > 0) {
		LOG("Context removal callback for URB #%u...\n", ix);
		(void) (* kill_func) (&pool->ubf[ix].ctx);
		assert (pool->ubf[ix].ctx == NULL);
	}
	return FALSE;
} /* build_urb_contexts */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
void remove_urbs (buffer_pool_p * ppool, ctx_func_t func) {
	
	assert (ppool != NULL);
	assert (*ppool != NULL);
	LOG("Removing buffer pool 0x%p...\n", *ppool);
	if (atomic_read (&(*ppool)->free_n) != -1) {
		free_urbs (*ppool, func);
	}
	assert ((*ppool)->free_ubf != NULL);
	assert ((*ppool)->ubf != NULL);
	hfree ((*ppool)->free_ubf);
	hfree ((*ppool)->ubf);
	hfree (*ppool);
	*ppool = NULL;
} /* remove_urbs */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
void free_urbs (buffer_pool_p pool, ctx_func_t func) {
	unsigned	ix;
	
	assert (pool != NULL);
	info (atomic_read (&pool->free_n) > -1);
	if (-1 == atomic_read (&pool->free_n)) {
		return;
	}
	assert (pool->ubf != NULL);
	assert (func != NULL);
	for (ix = 0; ix < pool->count; ix++) {
		LOG("Context removal callback for URB #%d...\n", ix);
		if (pool->ubf[ix].ctx != NULL) {
			(void) (* func) (&pool->ubf[ix].ctx);
		}
		assert (pool->ubf[ix].ctx == NULL);
		assert (pool->ubf[ix].urb != NULL);
		usb_free_urb (pool->ubf[ix].urb);
	}
	LOG("Freed %d URBs/buffers.\n", ix);
	atomic_set (&pool->free_n, -1);
	clear_urbs (pool);
} /* free_urbs */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
void clear_urbs (buffer_pool_p pool) {
	unsigned	ix;

	assert (pool != NULL);
	for (ix = 0; ix <= pool->count; ix++) {
		pool->ubf[ix].urb  = NULL;
		pool->ubf[ix].flag = URB_UNLINKED;
		pool->ubf[ix].ctx  = NULL;
		pool->free_ubf[ix] = pool->count;
		pool->todo_ubf[ix] = pool->count;
	}
	atomic_set (&pool->free_n, -1);
} /* clear_urbs */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
int unlink_urbs (buffer_pool_p pool) {
	unsigned	ix;
	int		num = 0;
	
	assert (pool != NULL);
	assert (pool->ubf != NULL);
	info (-1 != atomic_read (&pool->free_n));
	if (-1 == atomic_read (&pool->free_n)) {
		return 0;
	}
	for (ix = 0; ix < pool->count; ix++) {
		if (pool->ubf[ix].flag == URB_SUBMITTED) {
			num++;
			UNLINK_URB (pool->ubf[ix].urb);
			pool->ubf[ix].flag = URB_UNLINKED;
		}
	}
	return num;
} /* unlink_urbs */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
#if defined (LOG_TRACE_URBS)
int list_submitted_urbs (buffer_pool_p pool, const char * tag) {
	static char	buffer[256];
	char *		bp;
	int		ix, n;

	if ((pool == NULL) || (pool->count == 0)) 
		return 0;
	bp = buffer;
	*bp = (char) 0;
	for (ix = n = 0; ix < pool->count; ix++) {
		if (pool->ubf[ix].flag == URB_SUBMITTED) {
			bp += sprintf (bp, " %d:%luj", ix, jiffies - pool->ubf[ix].tsub);
			++n;
		}
		if ((bp - buffer) > 224) {
			strcpy (bp, "...");
			break;
		}
	}
	LOG("%s:%s (HZ=%u)\n", tag, buffer, HZ);
	return n;
} /* list_submitted_urbs */
#endif

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
int claim_urb (buffer_pool_p pool, struct urb ** ppurb, void ** pctx) {
	unsigned long	flags;
	unsigned	ix;
	
	assert (pool != NULL);
	assert (pool->ubf != NULL);
	assert (pool->free_ubf != NULL);
	assert (ppurb != NULL);
	assert (pctx  != NULL);
	
	lock_pool(pool);
	if (!atomic_read (&pool->free_n)) {
		unlock_pool(pool);
		return 0;
	}
	atomic_dec (&pool->free_n);
	ix = pool->free_ubf[pool->free_out];
	pool->free_out = (pool->free_out + 1) % pool->count;
	unlock_pool(pool);

	assert (ix < pool->count);
	assert (pool->ubf[ix].flag == URB_DONE);
	pool->ubf[ix].flag = URB_SUBMITTED;	/* in spe */
	*ppurb = pool->ubf[ix].urb;
	*pctx  = pool->ubf[ix].ctx;

	return 1;
} /* claim_urb */
	
/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
void release_urb (buffer_pool_p pool, struct urb * purb) {
	unsigned long	flags;
	uintptr_t	ix;
	
	assert (pool != NULL);
	assert (pool->ubf != NULL);
	assert (pool->free_ubf != NULL);
	assert (purb != NULL);
	ix = (uintptr_t) purb->context;
	
	lock_pool(pool);
	pool->free_ubf[pool->free_in] = ix;
	pool->free_in = (pool->free_in + 1) % pool->count;
	unlock_pool(pool);

	assert (ix < pool->count);
	pool->ubf[ix].flag = URB_DONE;
	atomic_inc (&pool->free_n);
} /* release_urb */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
void enlist_urb (buffer_pool_p pool, struct urb * purb) {
	unsigned long	flags;
	uintptr_t	ix;

	assert (pool != NULL);
	assert (pool->ubf != NULL);
	assert (pool->todo_ubf != NULL);
	assert (purb != NULL);
	ix = (uintptr_t) purb->context;

	lock_pool(pool);
	pool->todo_ubf[pool->todo_in] = ix;
	pool->todo_in = (pool->todo_in + 1) % pool->count;
	unlock_pool(pool);

	assert (ix < pool->count);
	assert (pool->ubf[ix].flag == URB_SUBMITTED);
	pool->ubf[ix].flag = URB_COMPLETED;
	atomic_inc (&pool->todo_n);
} /* enlist_urb */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
int unlist_urb (buffer_pool_p pool, struct urb ** ppurb, void ** pctx) {
	unsigned long	flags;
	unsigned	ix;

	assert (pool != NULL);
	assert (pool->ubf != NULL);
	assert (pool->todo_ubf != NULL);
	assert (ppurb != NULL);
	assert (pctx  != NULL);

	lock_pool(pool);
	if (!atomic_read (&pool->todo_n)) {
		unlock_pool(pool);
		return 0;
	}
	atomic_dec (&pool->todo_n);
	ix = pool->todo_ubf[pool->todo_out];
	pool->todo_out = (pool->todo_out + 1) % pool->count;
	unlock_pool(pool);

	assert (ix < pool->count);
	assert (pool->ubf[ix].flag == URB_COMPLETED);
	pool->ubf[ix].flag = URB_PROCESSING;
	*ppurb = pool->ubf[ix].urb;
	*pctx = pool->ubf[ix].ctx;

	return 1;
} /* unlist_urb */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
int urb_waiting (buffer_pool_p pool) {

	assert (pool != NULL);
	return atomic_read (&pool->todo_n);
} /* urb_waiting */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/ 

