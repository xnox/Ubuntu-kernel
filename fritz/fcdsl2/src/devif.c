/* 
 * devif.c
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

#include <asm/io.h>
#include <asm/atomic.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include "tools.h"
#include "lib.h"
#include "lock.h"
#include "fw.h"	
#include "driver.h"
#include "devif.h"

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
#define	DMA_BUFFER_NUM_MAX	8
#define	DMA_INVALID_NUMBER	0
#define	DMA_ALIGNMENT		8
#define	DMA_ALIGN_MASK		(DMA_ALIGNMENT-1)
#define	MAX_HEADER_RETRANSMIT	100
#define	MAX_LENGTH		16 * 1024

#define	INC(x)			(x)=(((x)==255)?1:(x)+1)

#define	MY_INT(x)		(((x)&C6205_PCI_HSR_INTSRC)!=0)
#define	PCI_INT_LINE_ON(x)	(((x)&C6205_PCI_HSR_INTAVAL)!=0)

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static atomic_t			is_stopped		= ATOMIC_INIT (FALSE);
atomic_t			xfer_flag		= ATOMIC_INIT (0);
static atomic_t			window_ok		= ATOMIC_INIT (0);
#if !defined (NDEBUG)
static atomic_t			is_open			= ATOMIC_INIT (FALSE);
static atomic_t			is_started		= ATOMIC_INIT (FALSE);
static atomic_t			n_buffers		= ATOMIC_INIT (0);
static unsigned			last_to_pc_ack;
static unsigned			tx_handled_count	= 0;
static unsigned			rx_handled_count	= 0;
#endif

static to_pc_ack_func_t		to_pc_ack_func		= NULL;
static timer_func_t		get_timer_func		= NULL;
static hw_completion_func_t	rx_callback		= NULL;
static hw_completion_func_t	tx_callback		= NULL;
static hw_event_func_t		event_callback		= NULL;
static xfer_func_t		tx_function		= NULL;
static dif_require_t		dif_req			= { 0, };
static unsigned			buf_disc		= 0;
static ptr_queue_p		tx_list			= NULL;
static ptr_queue_p		rx_list			= NULL;
static unsigned char		tx_num			= 1;
static unsigned char		rx_num			= 1;
static unsigned			rx_hdr_fail;
static unsigned			rx_blk_fail;
static unsigned			crrx_local;
static unsigned			crrx_global;
static unsigned			ro_num;
static unsigned			ro_offset;
static ioaddr_p			ro_pwin;
static unsigned			ro_len;
static unsigned			ro_next;
static unsigned			ro_bufofs;
static int			ro_flag			= FALSE;
static lock_t			dev_lock;
static card_p			card_link		= NULL;

static int		     (* int_callback) (void *)	= NULL;
static void *			int_context		= NULL;
static void			xfer_task (unsigned long);
static int			xfer_enabled		= FALSE;

static DECLARE_TASKLET_DISABLED(xfer_tasklet, xfer_task, 0);

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
typedef struct __rbi {

	hw_buffer_descriptor_p	pbd;
	unsigned		ofs;
	unsigned		len;
	void *			ctx;
} buf_info_t, * buf_info_p;

/*****************************************************************************\
 * D M A   B U F F E R   M A N A G E M E N T
\*****************************************************************************/ 

struct __ndi {

	unsigned char		num;
	unsigned char		cr;
	unsigned short		len;
	unsigned		timer;
} __attribute__ ((packed));

static dma_struct_p		dma_struct[2 * DMA_BUFFER_NUM_MAX];
#if !defined (NDEBUG)
static dma_struct_p		tx_last_buffer = NULL;
static dma_struct_p		rx_last_buffer = NULL;
#endif

/*---------------------------------------------------------------------------*\
\*-D-------------------------------------------------------------------------*/
dma_struct_p * dma_get_struct_list (unsigned * pnum) {
#if !defined (NDEBUG) && defined (LOG_LINK)
	unsigned	n;

	LOG("List of %u DMA structures: %p\n", buf_disc, dma_struct);
	for (n = 0; n < buf_disc; n++) {
		assert (dma_struct[n] != NULL);
		LOG(
			"%02u) %cX vaddr/paddr = %lx/%lx\n", 
			n, 
			dma_struct[n]->tx_flag ? 'T' : 'R',
			dma_struct[n]->virt_addr,
			dma_struct[n]->phys_addr
		);
	}
#endif
	assert (pnum != NULL);
	*pnum = buf_disc;
	return dma_struct;
} /* dma_get_struct_list */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
dma_struct_p dma_alloc (void) {
	dma_struct_p	pdma = NULL;
	
	pdma = (dma_struct_p) hcalloc (sizeof (dma_struct_t));
	info (pdma != NULL);
	return pdma;
} /* dma_alloc */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
void dma_free (dma_struct_p * ppdma) {

	assert (ppdma != NULL);
	assert (*ppdma != NULL);
	assert ((*ppdma)->virt_addr == 0UL);
	hfree (*ppdma);
	*ppdma = NULL;
} /* dma_free */
		
/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static unsigned dma_min_buf_size (dma_struct_p pdma, int tx_flag) {
	unsigned	bs;

	if (pdma != NULL) {
		tx_flag = pdma->tx_flag;
	}
	bs = sizeof (num_dma_info_t) + sizeof (unsigned);
	if (!tx_flag) {
		bs += sizeof (unsigned);
	}
	return bs;
} /* dma_min_buf_size */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static unsigned dma_eff_buf_size (dma_struct_p pdma) {

	assert (pdma != NULL);
	assert (pdma->length >= dma_min_buf_size (pdma, 0));
	return pdma->length - dma_min_buf_size (pdma, 0);
} /* dma_eff_buf_size */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static char * dma_eff_buf_virt (dma_struct_p pdma) {

	assert (pdma != NULL);
	return ((char *) pdma->virt_addr) + sizeof (num_dma_info_t);
} /* dma_eff_buf_virt */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static void dma_reset_buf (dma_struct_p pdma) {

	assert (pdma != NULL);
	lib_memset ((void *) pdma->virt_addr, 0, pdma->length);
	LOG("DMA buffer %p reset\n", pdma->virt_addr);
} /* dma_reset_buf */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static void dma_invalidate (dma_struct_p pdma, int fall) {
	num_dma_info_p	pndi = (num_dma_info_p) pdma->virt_addr;
	unsigned short	len;
	
	assert (pdma != NULL);
	assert (pndi != NULL);
	len = (fall) ? pdma->length : pndi->len + dma_min_buf_size (pdma, 0);
	assert (len <= pdma->length);
	assert (len >= dma_min_buf_size (pdma, 0));
	assert (DMA_INVALID_NUMBER == 0);
	assert (len >= sizeof (num_dma_info_t));

	lib_memset (pndi, 0, len);
} /* dma_invalidate */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
int dma_setup (dma_struct_p pdma, unsigned buf_size) {
	void	* buf1;
	void	* buf2;
	
	assert (buf_size > dma_min_buf_size (pdma, 0));
	assert (buf_size > 0);
	assert (pdma != NULL);
	assert (pdma->virt_addr == 0UL);
	if (NULL == (buf1 = kmalloc (buf_size + DMA_ALIGNMENT, GFP_ATOMIC))) {
		pdma->virt_addr = 0UL;
		return FALSE;
	}
	if ((((intptr_t) buf1) & DMA_ALIGN_MASK) != 0) {
		LOG("Buffer is not %d-aligned (%p)\n", DMA_ALIGNMENT, buf1);
		buf2 = (void *) ((((intptr_t) buf1) + DMA_ALIGNMENT - 1) & ~DMA_ALIGN_MASK);
	} else {
		buf2 = buf1;
	}
	assert ((((intptr_t) buf2) & DMA_ALIGN_MASK) == 0);
	lib_memset (buf2, 0, buf_size);
	pdma->length	= buf_size;
	pdma->alloc_ptr = buf1;
	pdma->virt_addr = (intptr_t) buf2;
	pdma->phys_addr = virt_to_bus (buf2);
	LOG(
		"Allocated DMA buffer vaddr %lx, paddr %lx, length %u\n", 
		pdma->virt_addr, 
		pdma->phys_addr,
		pdma->length
	);
	assert (pdma->length == buf_size);
	return TRUE;
} /* dma_setup */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
int dma_init (dma_struct_p pdma, unsigned buf_size) {
	int	res;

	if (TRUE == (res = dma_setup (pdma, buf_size))) {
		dma_invalidate (pdma, TRUE);
	}
	return res;
} /* dma_init */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static int dma_init_xx (dma_struct_p pdma, unsigned buf_size, int tx_flag) {
	int	res;
	
	if (TRUE == (res = dma_init (pdma, buf_size))) {
		pdma->tx_flag = tx_flag;
		LOG("... is a %s DMA buffer\n", pdma->tx_flag ? "TX" : "RX");
	}
	return res;
} /* dma_init_xx */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
void dma_free_buffer (dma_struct_p pdma) {
	
	assert (pdma != NULL);
	info (pdma->virt_addr != 0UL);
	if (pdma->virt_addr != 0UL) {
		kfree (pdma->alloc_ptr);
		pdma->virt_addr = pdma->phys_addr = 0UL;
	}
} /* dma_free_buffer */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
void dma_exit (dma_struct_p pdma) {
	
	assert (pdma != NULL);
	info (pdma->virt_addr != 0UL);
	if (pdma->virt_addr != 0UL) {
		LOG(
			"Freeing %s DMA buffer vaddr %lx\n",
			pdma->tx_flag ? "TX" : "RX",
			pdma->virt_addr 
		);
		dma_invalidate (pdma, TRUE);
		dma_free_buffer (pdma);
	}
} /* dma_exit */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static unsigned char * ndi_number_back_ptr (dma_struct_p pdma, unsigned len) {
	num_dma_info_p	pndi = (num_dma_info_p) pdma->virt_addr;
	unsigned	ofs;
	
	assert (pdma != NULL);
	assert (pndi != NULL);
	ofs = len + sizeof (num_dma_info_t);
	assert ((ofs + sizeof (unsigned char)) < pdma->length);
	return ((unsigned char *) pndi) + ofs; 
} /* ndi_number_back_ptr */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static int ndi_length_valid (dma_struct_p pdma, unsigned short len, unsigned short maxlen) {
	num_dma_info_p	pndi = (num_dma_info_p) pdma->virt_addr;

	assert (pdma != NULL);
	assert (pndi != NULL);
	if (((pndi->len) & 3) != 0) {
		info (0);
		return FALSE;
	}
	if (len > maxlen) {
		info (0);
		return FALSE;
	}
	return TRUE;
} /* ndi_length_valid */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
#if !defined (NDEBUG)
static int ndi_header_valid (dma_struct_p pdma, unsigned short maxlen) {
	num_dma_info_p	pndi = (num_dma_info_p) pdma->virt_addr;

	assert (pdma != NULL);
	assert (pndi != NULL);
	if (pndi->num == DMA_INVALID_NUMBER) {
		info (0);
		return FALSE;
	}
	return ndi_length_valid (pdma, pndi->len, maxlen);
} /* ndi_header_valid */
#endif

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
#if !defined (NDEBUG)
static int ndi_buffer_valid (dma_struct_p pdma) {
	num_dma_info_p	pndi = (num_dma_info_p) pdma->virt_addr;
	
	assert (pdma != NULL);
	assert (pndi != NULL);
	if (!ndi_header_valid (pdma, dma_eff_buf_size (pdma))) {
		return FALSE;
	}
	if (!pdma->tx_flag) {
		unsigned char * uc = ndi_number_back_ptr (pdma, pndi->len);

		if (pndi->num != *uc) {
			return FALSE;
		}
	}
	return TRUE;
} /* ndi_buffer_valid */
#endif

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static void dma_validate_cs (dma_struct_p pdma, unsigned char num, unsigned cs) {
	num_dma_info_p	pndi = (num_dma_info_p) pdma->virt_addr;
	intptr_t 	bnum;
	
	assert (pdma != NULL);
	assert (pndi != NULL);
	bnum = 3 + (intptr_t) ndi_number_back_ptr (pdma, pndi->len);
	bnum &= ~3;
#if !defined (NDEBUG)
	assert ((bnum + sizeof (unsigned)) <= (pdma->virt_addr + pdma->length));
	if ((bnum + sizeof (unsigned)) > (pdma->virt_addr + pdma->length)) {
		LOG("Back num %x, buffer (virtual) %lx\n", bnum, pdma->virt_addr);
	}
#endif
	*((unsigned *) bnum) = cs;
	assert (num != DMA_INVALID_NUMBER);
	pndi->num = num;
	assert (ndi_buffer_valid (pdma));
} /* dma_validate_cs */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static void dma_validate (dma_struct_p pdma, unsigned char cr, unsigned short len, unsigned timer) {
	num_dma_info_p	pndi = (num_dma_info_p) pdma->virt_addr;
	
	assert (pdma != NULL);
	assert (pndi != NULL);
	assert (len <= dma_eff_buf_size (pdma));
	assert ((len + dma_min_buf_size (pdma, pdma->tx_flag)) <= pdma->length);
	
	pndi->cr	= cr;
	pndi->len	= len;
	pndi->timer	= timer;
} /* dma_validate */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static unsigned char ndi_get_cr (dma_struct_p pdma) {
	num_dma_info_p	pndi = (num_dma_info_p) pdma->virt_addr;

	assert (pdma != NULL);
	assert (pndi != NULL);
	return pndi->cr;
} /* ndi_get_cr */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static unsigned short ndi_get_length (dma_struct_p pdma) {
	num_dma_info_p	pndi = (num_dma_info_p) pdma->virt_addr;

	assert (pdma != NULL);
	assert (pndi != NULL);
	return pndi->len;
} /* ndi_get_length */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static void ndi_set_number (dma_struct_p pdma, unsigned char x) {
	num_dma_info_p	pndi = (num_dma_info_p) pdma->virt_addr;

	assert (pdma != NULL);
	assert (pndi != NULL);
	pndi->num = x;
} /* ndi_set_number */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static unsigned char ndi_get_number (dma_struct_p pdma) {
	num_dma_info_p	pndi = (num_dma_info_p) pdma->virt_addr;

	assert (pdma != NULL);
	assert (pndi != NULL);
	return pndi->num;
} /* ndi_get_number */

/*****************************************************************************\
 * H E L P E R   F U N C T I O N S
\*****************************************************************************/ 

static unsigned check_sum (unsigned char num, unsigned wcnt, unsigned * addr) {
	unsigned	sum = 0x55aa55aa;
	
	assert (ro_flag);
	assert (addr != NULL);
	assert (wcnt > 0);
#if defined (LOG_CHECKSUM)
	LOG("[start] sum = 0x%x\n", sum);
#endif
	sum ^= (unsigned) num;
#if defined (LOG_CHECKSUM)
	LOG("[number] sum = 0x%x, num = 0x%x\n", sum, num);
#endif
	do {
		sum ^= *addr;
#if defined (LOG_CHECKSUM)
		LOG("[%u] sum = 0x%x, num = 0x%x\n", wcnt, sum, *addr);
#endif
		++addr;
	} while (--wcnt);
	return sum;
} /* check_sum */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static void trigger_interrupt (card_p cp) {

	assert (cp != NULL);
	assert (cp->mmio_base != 0);
	POUTL (cp->mmio_base + C6205_PCI_HDCR_OFFSET, C6205_PCI_HDCR_DSPINT);
} /* trigger_interrupt */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
#if 0 && !defined (NDEBUG) && defined (LOG_LINK)
static void dump_buffers (int tx_flag) {
	unsigned	n, ofs;
	unsigned	offset, val0, val1;
	
	ofs = tx_flag ? 0 : DMA_BUFFER_NUM_MAX;
	LOG("Dump buffers in %cX direction...\n", tx_flag ? 'T' : 'R');
	for (n = ofs; n < (ofs + DMA_BUFFER_NUM_MAX); n++) {
		if (dma_struct[n] != NULL) {
			memdump (
				(void *) dma_struct[n]->virt_addr, 
				32, 
				dma_struct[n]->virt_addr,
				tx_flag ? "TX buffer" : "RX buffer"
			);
		}
		if (!tx_flag && (ro_pwin != NULL)) {
			offset = ro_offset + (n - 8) * ro_len + ro_bufofs;
			val0 = ro_pwin->inl (ro_pwin->virt_addr + offset);
			val1 = ro_pwin->inl (ro_pwin->virt_addr + offset + 4);
			LOG("RX%u prefix: %08X %08X\n", n, val0, val1);
		}
	}
	LOG("End of dump\n");
} /* dump_buffers */
#endif

/*****************************************************************************\
 * T R A N S F E R   M A N A G E M E N T
\*****************************************************************************/ 

static int receive_buf (card_p cp, dma_struct_p pdma) {
	unsigned	count = 0;
	unsigned	offset, chksum;
	unsigned	wc, wcnt, tmp1;
	unsigned *	mem;
	unsigned *	tmp2;
	int		res;
	
	assert (cp != NULL);
	assert (pdma != NULL);
	assert (ro_pwin != NULL);
	offset = ro_offset + ro_next * ro_len + ro_bufofs;
	mem = (unsigned *) pdma->virt_addr;
	assert (mem != NULL);
	do {
		count++;
		*mem	   = ro_pwin->inl (ro_pwin->virt_addr + offset);
		*(mem + 1) = ro_pwin->inl (ro_pwin->virt_addr + offset + sizeof (unsigned));
	} while ((*mem != ~*(mem + 1)) && (count < MAX_HEADER_RETRANSMIT));
	if (count > 2) {
		rx_hdr_fail += count - 2;
	}
	if (count > MAX_HEADER_RETRANSMIT) {
		LOG("Too many header re-transmits (%u)...\n", count);
		return FALSE;
	}
	if (ndi_get_number (pdma) != rx_num) {
#if 0 && !defined (NDEBUG) && defined (LOG_LINK)
		LOG(
			"Unexpected number, want:%u, have:%u...\n",
			rx_num,
			ndi_get_number (pdma)
		);
#endif
		return FALSE;
	}
	res = ndi_length_valid (pdma, ndi_get_number (pdma), dma_eff_buf_size (pdma));
	if (!res) {
#if !defined (NDEBUG) && defined (LOG_LINK)
		LOG("Invalid length (%d)...\n", ndi_get_length (pdma));
#endif
		return FALSE;
	}
	assert (0 == (sizeof (num_dma_info_t) % 4));
	count = 0;
	offset += sizeof (num_dma_info_t);

	/* wcnt: datalen + num + checksum */
	wcnt = (ndi_get_length (pdma) + 4 + 4) >> 2;
	tmp1 = sizeof (num_dma_info_t) >> 2;
	tmp2 = mem + tmp1;
	do {
		count++;
		for (wc = 0; wc < wcnt; wc++) {
			*(tmp2 + wc) = ro_pwin->inl (ro_pwin->virt_addr + offset + 4 * wc);
		}
		chksum = check_sum (0, wcnt + tmp1, mem);
	} while ((chksum != 0) && (count < MAX_HEADER_RETRANSMIT));
	if (count > 1) {
		rx_blk_fail += count - 1;
	}
	if (count > MAX_HEADER_RETRANSMIT) {
		LOG("Too many block re-transmits (%u)...\n", count);
		return FALSE;
	}
	assert (ndi_buffer_valid (pdma));
	return TRUE;
} /* receive_buf */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static void receive_done (void) {

	if (++ro_next == dif_req.rx_block_num) {
		ro_next = 0;
	}
} /* receive_done */

/*---------------------------------------------------------------------------*\
\*-I-------------------------------------------------------------------------*/
static unsigned int_complete_buf (int tx_flag) {
	ptr_queue_p 		pq;
	hw_completion_func_t	pcf;
	dma_struct_p		pdma;
	buf_info_p		pinfo;
	buf_info_t		info;
	void *			phlp;
	int			res;

	if (tx_flag) {
		pq  = tx_list;
		pcf = tx_callback;
	} else {
		pq  = rx_list;
		pcf = rx_callback;
	}
	phlp = NULL;
	res = q_dequeue (pq, &phlp);
	assert (res == TRUE);
	pinfo = (buf_info_p) phlp;
	assert (pinfo != NULL);
	info = *pinfo;
	
	assert (info.pbd != NULL);
	assert (info.ofs == 0);
	pdma = (dma_struct_p) info.pbd->context;

	assert (pdma != NULL);
	assert (info.len <= dma_eff_buf_size (pdma));
	assert (ndi_get_length (pdma) <= dma_eff_buf_size (pdma));

	enter_critical ();
	assert (ndi_buffer_valid (pdma));
	assert (pcf != NULL);
	(* pcf) (info.pbd, info.ofs, ndi_get_length (pdma), info.ctx);
	leave_critical ();
	return 1;
} /* int_complete_buf */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static unsigned int int_work_buffers (void) {
	unsigned	tx_handled	= 0;
	unsigned	rx_handled	= 0;
	void *		phlp		= NULL;
	buf_info_p	pinfo;
	unsigned	cr;
	dma_struct_p	pdma;
	
	while (q_peek (rx_list, &phlp)) {
		pinfo = (buf_info_p) phlp;
		assert (pinfo != NULL);
		assert (pinfo->pbd != NULL);
		pdma = (dma_struct_p) pinfo->pbd->context;
		assert (pdma != NULL);
		assert (card_link != NULL);
		if (!atomic_read (&window_ok)) {
			LOG("I/O window not ready!\n");
			break;
		} 
		if (!receive_buf (card_link, pdma)) {
			break;
		}
#if 0 && !defined (NDEBUG) && defined (LOG_LINK)
		LOG("Received buf #%u\n", ndi_get_number (pdma));
#endif
		assert (ndi_get_number (pdma) == rx_num);
		INC(rx_num);
		receive_done ();

		cr = ndi_get_cr (pdma);
		rx_handled += int_complete_buf (FALSE);
		while (cr-- > 0) {
			tx_handled += int_complete_buf (TRUE);
		}
		phlp = NULL;
	}
#if !defined (NDEBUG)
	assert ((tx_handled == 0) || (rx_handled > 0));
	tx_handled_count += tx_handled;
	rx_handled_count += rx_handled;
#endif
	return (tx_handled + rx_handled);
} /* int_work_buffers */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
void set_interrupt_callback (int (* cbf) (void *), void * ctx) {

	assert (int_callback == NULL);
	assert (int_context  == NULL);
	int_callback = cbf;
	int_context  = ctx;
} /* set_interrupt_callback */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
void clear_interrupt_callback (void) {

	info (int_callback != NULL);
	info (int_context  != NULL);
	int_callback = NULL;
	int_context  = NULL;
} /* clear_interrupt_callback */

/*---------------------------------------------------------------------------*\
\*-T-------------------------------------------------------------------------*/
void xfer_handler (card_p cp) {
	unsigned int	traffic_cnt = 0;
	
	UNUSED_ARG(cp);
	atomic_set (&xfer_flag, 0);
	traffic_cnt += int_work_buffers ();
	if (traffic_cnt > 0) {
		info (int_callback != NULL);
		if (int_callback != NULL) {
#if !defined (NDEBUG)
			int res =
#endif
			(*int_callback) (int_context);
			assert (res == TRUE);
		}
	}
} /* xfer_handler */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static void xfer_task (unsigned long data) {

	UNUSED_ARG (data);
	if (in_critical ()) {
		atomic_set (&xfer_flag, 1);
	} else if (spin_trylock (&stack_lock)) {
		xfer_handler (capi_card);
		spin_unlock (&stack_lock);
	} else {
		atomic_set (&xfer_flag, 1);
	}
} /* xfer_task */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
irqreturn_t device_interrupt (int irq, void * args) {
	unsigned long	intpins;
	card_p		cp	= (card_p) args;

	UNUSED_ARG (irq);
	assert (capi_card == cp);

	intpins = PINL (cp->mmio_base + C6205_PCI_HSR_OFFSET);
	if (!MY_INT (intpins)) {
		return IRQ_NONE;
	}
	do {
		POUTL (
			cp->mmio_base + C6205_PCI_HSR_OFFSET, 
			C6205_PCI_HSR_INTSRC
		);
		tasklet_schedule (&xfer_tasklet);
		intpins = PINL (cp->mmio_base + C6205_PCI_HSR_OFFSET);
	} while (MY_INT (intpins));
	return IRQ_HANDLED;
} /* device_interrupt */

/*****************************************************************************\
 * D E V I C E   I N T E R F A C E
\*****************************************************************************/ 

__attr void dif_xfer_requirements (dif_require_p rp, unsigned ofs) {

	assert (rp != NULL);
	assert (rp->tx_block_num > 0);
	assert (rp->tx_block_size > 0);
	assert (rp->tx_max_trans_len == 0);
	assert (rp->rx_block_num > 0);
	assert (rp->rx_block_size > 0);
	assert (rp->rx_max_trans_len == 0);
	assert (rp->tx_block_num <= DMA_BUFFER_NUM_MAX);
	assert (rp->rx_block_num <= DMA_BUFFER_NUM_MAX);

	dif_req = *rp;
	assert (dif_req.tx_block_size >= dma_min_buf_size (NULL, TRUE));
	dif_req.tx_max_trans_len = dif_req.tx_block_size - dma_min_buf_size (NULL, TRUE);
	assert (dif_req.rx_block_size >= (dma_min_buf_size (NULL, FALSE) + ofs));
	dif_req.rx_max_trans_len = dif_req.rx_block_size - dma_min_buf_size (NULL, FALSE) - ofs;
	dif_req.tx_block_size += 4 * sizeof (unsigned char);
	
#if !defined (NDEBUG) && defined (LOG_LINK)
	LOG(
		"Requirements: TX %u/%u/%u, RX %u/%u/%u, Func/Ctx %p/%p\n",
		dif_req.tx_block_num,
		dif_req.tx_block_size,
		dif_req.tx_max_trans_len,
		dif_req.rx_block_num,
		dif_req.rx_block_size,
		dif_req.rx_max_trans_len,
		dif_req.get_code,
		dif_req.context
	);
#endif
} /* dif_xfer_requirements */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
void dif_reset (void) {

	LOG("Resetting device structures...\n");
	
	atomic_set (&window_ok, 0);
	ro_num		= 0;
	ro_offset	= 0;
	ro_pwin		= NULL;
	ro_len		= 0;
	ro_next		= 0;
	ro_bufofs	= 0;
	rx_hdr_fail	= 0;
	rx_blk_fail	= 0;

	lock (dev_lock);
	q_reset (tx_list);
	q_reset (rx_list);
	crrx_local = 0;
	crrx_global = 0;
	tx_num = 1;
	rx_num = 1;
#if !defined (NDEBUG)
	tx_last_buffer = 0;
	rx_last_buffer = 0;
	tx_handled_count = 0;
	rx_handled_count = 0;
#endif
	atomic_set (&is_stopped, FALSE);
	unlock (dev_lock);
	LOG("Reset done.\n");
} /* dif_reset */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
void dif_set_params (
	unsigned	num,
	unsigned	offset,
	ioaddr_p	pwin,
	unsigned	len,
	unsigned	bufofs
) {
#if !defined (NDEBUG) && defined (LOG_LINK)
	unsigned	n, ofs;
#endif
	assert (num > 0);
	assert ((offset % 4) == 0);
	assert (len > 0);
	assert ((len % 4) == 0);
	assert (pwin != NULL);

	ro_num = num;
	ro_offset = offset;
	ro_pwin = pwin;
	ro_len = len;
	ro_bufofs = bufofs;
	atomic_set (&window_ok, 1);
	LOG("I/O window has been set!\n");

#if !defined (NDEBUG) && defined (LOG_LINK)
	LOG("dif_set_params:\n");
	for (n = 0; n < ro_num; n++) {
		ofs = ro_offset + n * ro_len + ro_bufofs;
		LOG("buffer(%u), vaddr 0x%08x\n", n, pwin->virt_addr + ofs);
	}
#endif
} /* dif_set_params */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
int dif_init (card_p cp, int ro, timer_func_t timerf, to_pc_ack_func_t ackf) {

	info (get_timer_func == NULL);
	info (to_pc_ack_func == NULL);
	LOG("Initializing device structures...\n");

	lock_init (&dev_lock);
	assert (!xfer_enabled);
	tasklet_enable (&xfer_tasklet);
	xfer_enabled = TRUE;

	buf_disc	= 0;
	ro_num		= 0;
	ro_offset	= 0;
	ro_pwin		= NULL;
	ro_len		= 0;
	ro_next		= 0;
	ro_bufofs	= 0;
	rx_hdr_fail	= 0;
	rx_blk_fail	= 0;
	tx_num		= 1;
	rx_num		= 1;
	card_link	= cp;
	crrx_local	= 0;
	crrx_global	= 0;
	get_timer_func	= timerf ? timerf : &get_timer;
	to_pc_ack_func	= ackf   ? ackf   : &pc_acknowledge;
	info (ro == TRUE);
	ro_flag = ro;
	
	if (NULL == (tx_list = q_make (DMA_BUFFER_NUM_MAX))) {
		LOG("Could not allocate TX queue\n");
		return FALSE;
	}
	if (NULL == (rx_list = q_make (DMA_BUFFER_NUM_MAX))) {
		LOG("Could not allocate RX queue\n");
		q_remove (&tx_list);
		return FALSE;
	}
	q_attach_mem (tx_list, DMA_BUFFER_NUM_MAX, sizeof (buf_info_t));
	q_attach_mem (rx_list, DMA_BUFFER_NUM_MAX, sizeof (buf_info_t));

	lib_memset (
		&dma_struct, 
		0, 
		2 * DMA_BUFFER_NUM_MAX * sizeof (dma_struct_p)
	);
	lib_memset (&dif_req, 0, sizeof (dif_require_t));
	return ro_flag;
} /* dif_init */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
void dif_exit (void) {

	dif_reset ();
	LOG("Deinitializing device...\n");
	info (get_timer_func != NULL);
	get_timer_func = NULL;
	info (to_pc_ack_func != NULL);
	to_pc_ack_func = NULL;
	if (tx_list != NULL) {
		q_remove (&tx_list);
	}
	if (rx_list != NULL) {
		q_remove (&rx_list);
	}
	if (xfer_enabled) {
		tasklet_disable (&xfer_tasklet);
		xfer_enabled = FALSE;
	}
	lock_exit (&dev_lock);
} /* dif_exit */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
int dif_start (void) {

	LOG("Starting device interface...\n");
	return (xfer_enabled = TRUE);
} /* dif_start */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static void dif_stop (int tx_flag) {
	void *		phlp = NULL;
	buf_info_p	pi;
	ptr_queue_p	pq;
	dma_struct_p	pdma;
	unsigned	n;

	LOG("Stopping device interface (%d)...\n", tx_flag);
	pq = tx_flag ? tx_list : rx_list;
	assert (pq != NULL);
	while (q_dequeue (pq, &phlp)) {
		pi = (buf_info_p) phlp;
		assert (pi != NULL);
		assert (pi->pbd != NULL);
		pdma = (dma_struct_p) pi->pbd->context;
		assert (pdma != NULL);

		dma_invalidate (pdma, TRUE);
		assert (event_callback != NULL);
		lock (dev_lock);
		(* event_callback) (
			DMA_REQUEST_CANCELLED, 
			pi->pbd, 
			pi->ofs, 
			pi->len, 
			pi->ctx
		);
		unlock (dev_lock);
		
		phlp = NULL;
	}
	for (n = 0; n < (2 * DMA_BUFFER_NUM_MAX); n++) {
		info (dma_struct[n] != NULL);
		if (dma_struct[n] != NULL) {
			dma_reset_buf (dma_struct[n]);
		}
	}
} /* dif_stop */

/*****************************************************************************\
 * H A R D W A R E   I N T E R F A C E
\*****************************************************************************/ 

__attr unsigned OSHWBlockOpen (unsigned index, hw_block_handle * handle) {

	UNUSED_ARG (index);
	assert (handle != NULL);
	info (*handle == NULL);
	*handle = (hw_block_handle) card_link;
	assert (!atomic_xchg (&is_open, 1));
	return TRUE;
} /* OSHWBlockOpen */

/*---------------------------------------------------------------------------*\
\*-O-------------------------------------------------------------------------*/
__attr void OSHWBlockClose (hw_block_handle handle) {

	assert (handle == (hw_block_handle) card_link);
	assert (atomic_read (&n_buffers) == 0);
	assert (atomic_read (&is_open));
	assert (!atomic_read (&is_started));
	assert (atomic_xchg (&is_open, 0));
} /* OSHWBlockClose */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
__attr unsigned OSHWStart (
	hw_block_handle		handle,
	hw_completion_func_t	rx_completer,
	hw_completion_func_t	tx_completer,
	hw_event_func_t		event_handler
) {
	assert (handle == (hw_block_handle) card_link);
	assert (atomic_read (&is_open));
	assert (!atomic_read (&is_started));
	assert (rx_callback == NULL);
	assert (tx_callback == NULL);
	assert (event_callback == NULL);

	rx_callback	= rx_completer;
	tx_callback	= tx_completer;
	event_callback	= event_handler;
	assert (!atomic_xchg (&is_started, 1));

	return dif_start ();
} /* OSHWStart */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
__attr void OSHWStop (hw_block_handle handle) {

	assert (handle == (hw_block_handle) card_link);
	assert (atomic_read (&is_open));
	assert (atomic_read (&is_started));

	dif_stop (TRUE);
	dif_stop (FALSE);
	atomic_set (&is_stopped, TRUE);

	rx_callback	= NULL;
	tx_callback	= NULL;
	event_callback	= NULL;
	assert (atomic_xchg (&is_started, 0));
} /* OSHWStop */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
__attr hw_buffer_descriptor_p OSHWAllocBuffer (
	hw_block_handle	handle, 
	unsigned	length
) {
	hw_buffer_descriptor_p	pbd;
	dma_struct_p		pstr;
#if !defined (NDEBUG)
	unsigned		start, stop, n;
#endif
	
	LOG("Allocating DMA buffer of %u bytes\n", length);
	assert (handle == (hw_block_handle) card_link);
	assert (atomic_read (&is_open));
	assert (length < OSHWGetMaxBlockSize (handle));
	assert (dif_req.tx_block_num > 0);
	assert (dif_req.rx_block_num > 0);
	assert ((length == dif_req.tx_block_size) || (length == dif_req.rx_block_size));

	if (NULL == (pbd = (hw_buffer_descriptor_p) hmalloc (sizeof (hw_buffer_descriptor_t)))) {
		return NULL;
	}
	
	if (NULL == (pstr = dma_alloc ())) {
		hfree (pbd);
		return NULL;
	}
	if (!dma_init_xx (pstr, length, (buf_disc < dif_req.tx_block_num))) {
		hfree (pbd);
		dma_exit (pstr);
		dma_free (&pstr);
		return NULL;
	}
	pbd->handle	= handle;
	pbd->buffer	= dma_eff_buf_virt (pstr);
	pbd->length	= length;
	pbd->context	= pstr;

	assert (buf_disc < 2 * DMA_BUFFER_NUM_MAX);
	assert (dma_struct[buf_disc] == NULL);
	dma_struct[buf_disc++] = pstr;

#if !defined (NDEBUG)
	atomic_inc (&n_buffers);
	start = stop = 0;
	if (buf_disc == dif_req.tx_block_num) {
		stop  = buf_disc;
	} else if (buf_disc == (dif_req.tx_block_num + dif_req.rx_block_num)) {
		start = dif_req.tx_block_num;
		stop  = buf_disc;
	}
	if (start != stop) {
		assert (stop > 1);
		for (n = start; n < (stop - 1); n++) {
			assert (dma_struct[n] != NULL);
			assert (dma_struct[n + 1] != NULL);
			dma_struct[n]->context = dma_struct[n + 1];
			LOG(
				"DMA context of structure #%u(%p) set to %p\n", 
				n, dma_struct[n], dma_struct[n + 1]
			);
		}
		dma_struct[n]->context = dma_struct[start];
		LOG(
			"DMA context of structure #%u(%p) set to %p\n", 
			n, dma_struct[n], dma_struct[start]
		);
	}
#endif
	
	return pbd;
} /* OSHWAllocBuffer */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
__attr void OSHWFreeBuffer (hw_buffer_descriptor_p desc) {
	dma_struct_p	pstr;

	if (desc == NULL) {
		LOG("Attempt to free NULL descriptor!\n");
		return;
	}
	pstr = (dma_struct_p) desc->context;
	assert (pstr != NULL);
	desc->buffer = (unsigned char *) pstr->virt_addr;
	
	assert (atomic_read (&is_open));
	assert (atomic_read (&n_buffers) > 0);
	dma_exit (pstr);
	hfree (pstr);
	hfree (desc);

	assert (buf_disc > 0);
	assert (buf_disc <= 2 * DMA_BUFFER_NUM_MAX);
	buf_disc--;
	assert (dma_struct[buf_disc] != NULL);
	dma_struct[buf_disc] = NULL;
#if !defined (NDEBUG)
	atomic_dec (&n_buffers);
#endif
} /* OSHWFreeBuffer */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
__attr unsigned OSHWGetMaxBlockSize (hw_block_handle handle) {

	assert (handle == (hw_block_handle) card_link);
	return 4 * 1024;
} /* OSHWGetMaxBlockSize */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
__attr unsigned OSHWGetMaxConcurrentRxBlocks (hw_block_handle handle) {

	assert (handle == (hw_block_handle) card_link);
	return dif_req.rx_block_num;
} /* OSHWGetMaxConcurrentRxBlocks */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
__attr unsigned OSHWGetMaxConcurrentTxBlocks (hw_block_handle handle) {

	assert (handle == (hw_block_handle) card_link);
	return dif_req.tx_block_num;
} /* OSHWGetMaxConcurrentRxBlocks */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
__attr void * OSHWExchangeDeviceRequirements (
	hw_block_handle	handle, 
	void *		pin
) {
	in_require_p	pr = (in_require_p) pin;
	
	assert (handle == (hw_block_handle) card_link);
	info (pr != NULL);
	if (pr != NULL) {
		tx_function = pr->tx_func;
		assert (tx_function != NULL);
	}
	assert (dif_req.tx_block_num > 0);
	assert (dif_req.tx_block_size > 0);
	assert (dif_req.tx_max_trans_len > 0);
	assert (dif_req.rx_block_num > 0);
	assert (dif_req.rx_block_size > 0);
	assert (dif_req.rx_max_trans_len > 0);
	LOG("Returning requirements, TX %u/%u/%u, RX %u/%u/%u\n", 
		dif_req.tx_block_num,
		dif_req.tx_block_size,
		dif_req.tx_max_trans_len,
		dif_req.rx_block_num,
		dif_req.rx_block_size,
		dif_req.rx_max_trans_len
	);
	return (void *) &dif_req;
} /* OSHWExchangeRequirements */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
__attr unsigned OSHWTxBuffer (
	const hw_buffer_descriptor_p	desc,
	unsigned			offset,
	unsigned			length,
	void *				context
) {
	buf_info_t			info	= { desc, offset, length, context } ;
	dma_struct_p			pdma;
	unsigned			chksum, timer = 0;
	int				res;
	
	assert (!atomic_read (&is_stopped));
	assert (atomic_read (&is_open));
	assert (info.pbd != NULL);
	pdma = (dma_struct_p) info.pbd->context;
	assert (pdma != NULL);
	assert (info.ofs == 0);
	assert (info.len <= dif_req.tx_max_trans_len);
	assert (info.len <= dma_eff_buf_size (pdma));
	assert (info.len <= MAX_LENGTH);
	
	assert (q_get_count (tx_list) < dif_req.tx_block_num);

#if !defined (NDEBUG)
	assert (get_timer_func != NULL);
	timer = (*get_timer_func) ();
#endif
	dma_validate (pdma, 0, info.len, timer);
	ndi_set_number (pdma, 0);	/* 0 for chksum */
	chksum = check_sum (
			tx_num, 
			(sizeof (num_dma_info_t) + ndi_get_length (pdma) + 3) >> 2, 
			(unsigned *) pdma->virt_addr
		 );
	dma_validate_cs (pdma, tx_num, chksum);
	res = q_enqueue_mem (tx_list, &info, sizeof (buf_info_t));
	assert (res != 0);
	assert (q_get_count (tx_list) <= DMA_BUFFER_NUM_MAX);
	assert (q_get_count (tx_list) <= dif_req.tx_block_num);
#if !defined (NDEBUG)
	if (tx_last_buffer != NULL) {
		assert ((void *) pdma == tx_last_buffer->context);
	} else {
		assert (pdma == dma_struct [0]);
	}
	tx_last_buffer = pdma;
#endif
	INC(tx_num);
	assert (card_link != NULL);
	trigger_interrupt (card_link);
	
	return TRUE;
} /* OSHWTxBuffer */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
__attr unsigned OSHWRxBuffer (
	const hw_buffer_descriptor_p	desc,
	unsigned			offset,
	unsigned			length,
	void *				context
) {
	buf_info_t			info	= { desc, offset, length, context } ;
	dma_struct_p			pdma;
	int				res;
#if !defined (NDEBUG)
	unsigned			time;
#endif
	
	assert (!atomic_read (&is_stopped));
	assert (atomic_read (&is_open));
	assert (info.pbd != NULL);
	pdma = (dma_struct_p) info.pbd->context;
	assert (pdma != NULL);
	assert (info.ofs == 0);
	assert (info.len <= dif_req.rx_max_trans_len);

	assert (info.len <= dma_eff_buf_size (pdma));
	assert (info.len <= MAX_LENGTH);
	
	assert (q_get_count (rx_list) < dif_req.rx_block_num);
	res = q_enqueue_mem (rx_list, &info, sizeof (buf_info_t));
	assert (res != 0);
#if !defined (NDEBUG)
	if (rx_last_buffer != NULL) {
		assert (rx_last_buffer->context == (void *) pdma);
	} else {
		assert (dif_req.tx_block_num > 0);
		assert (dif_req.tx_block_num < (2 * DMA_BUFFER_NUM_MAX));
		assert (pdma == dma_struct [dif_req.tx_block_num]);
	}
	rx_last_buffer = pdma;
	assert (get_timer_func != NULL);
	time = (*get_timer_func) ();
	last_to_pc_ack = time;
#endif
	assert (to_pc_ack_func != NULL);
	(*to_pc_ack_func) (ndi_get_number (pdma));

	return TRUE;
} /* OSHWRxBuffer */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
__attr unsigned OSHWCancelRx (hw_block_handle handle) {

	UNUSED_ARG (handle);
	assert (0);
	return FALSE;
} /* OSHWCancelRx */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
__attr unsigned OSHWCancelTx (hw_block_handle handle) {

	UNUSED_ARG (handle);
	assert (0);
	return FALSE;
} /* OSHWCancelTx */

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
