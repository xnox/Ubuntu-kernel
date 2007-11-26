/**************************************************************************
 * Copyright (c) Intel Corp. 2007.
 * All Rights Reserved.
 *
 * Intel funded Tungsten Graphics (http://www.tungstengraphics.com) to
 * develop this driver.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS, AUTHORS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 **************************************************************************/
/*
 */
#include "drmP.h"
#include "psb_drv.h"
#include "psb_reg.h"

/*
 * Code for the SGX MMU:
 */

/*
 * clflush on one processor only:
 * If I've received the correct information from Intel engineers, clflush only
 * flushes the cache line of the current processor. Therefore modifications 
 * to memory and the following clflush needs to be encapsulated either in a 
 * spinlocked region or using preempt_disable / preempt_enable. This guarantees
 * that the write and the following clflush executes on the same processor.
 */

/*
 * kmap atomic:
 * The usage of the slots must be completely encapsulated within a spinlock, and
 * no other functions that may be using the locks for other purposed may be
 * called from within the locked region.
 * Since the slots are per processor, this will guarantee that we are the only
 * user.
 */

/*
 * TODO: Inserting ptes from an interrupt handler:
 * This may be desirable for some SGX functionality where the GPU can fault in
 * needed pages. For that, we need to make an atomic insert_pages function, that
 * may fail. 
 * If it fails, the caller need to insert the page using a workqueue function,
 * but on average it should be fast.
 */

struct psb_mmu_driver {
	/* protects driver- and pd structures. Always take in read mode
	 * before taking the page table spinlock. 
	 */
	struct rw_semaphore sem;

	/* protects page tables, directory tables and pt tables.
	 * and pt structures. 
	 */
	spinlock_t lock;

	atomic_t needs_tlbflush;
	u8 __iomem *register_map;
	struct psb_mmu_pd *default_pd;
	u32 bif_ctrl;
	int has_clflush;
	int clflush_add;
	unsigned long clflush_mask;
};

struct psb_mmu_pd;

struct psb_mmu_pt {
	struct psb_mmu_pd *pd;
	u32 index;
	u32 count;
	struct page *p;
	u32 *v;
};

struct psb_mmu_pd {
	struct psb_mmu_driver *driver;
	int hw_context;
	struct psb_mmu_pt **tables;
	struct page *p;
	u32 pd_mask;
};

static inline u32 psb_mmu_pt_index(__u32 offset)
{
	return (offset >> PSB_PTE_SHIFT) & 0x3FF;
}
static inline u32 psb_mmu_pd_index(__u32 offset)
{
	return (offset >> PSB_PDE_SHIFT);
}

#if defined(CONFIG_X86)
static inline void psb_clflush(void *addr)
{
	__asm__ __volatile__("clflush (%0)\n"::"r"(addr):"memory");
}

static inline void psb_mmu_clflush(struct psb_mmu_driver *driver, void *addr)
{
	if (!driver->has_clflush)
		return;

	mb();
	psb_clflush(addr);
	mb();
}
#else

static inline void psb_mmu_clflush(struct psb_mmu_driver *driver, void *addr)
{;
}

#endif

static inline void psb_iowrite32(const struct psb_mmu_driver *d,
				 u32 val, __u32 offset)
{
	iowrite32(val, d->register_map + offset);
}

static inline u32 psb_ioread32(const struct psb_mmu_driver *d, __u32 offset)
{
	return ioread32(d->register_map + offset);
}

static void psb_mmu_flush_pd_locked(struct psb_mmu_driver *driver, int force)
{
	if (atomic_read(&driver->needs_tlbflush) || force) {
		u32 val = psb_ioread32(driver, PSB_CR_BIF_CTRL);
		psb_iowrite32(driver, val | _PSB_CB_CTRL_INVALDC, PSB_CR_BIF_CTRL);
		wmb();
		psb_iowrite32(driver, val & ~_PSB_CB_CTRL_INVALDC, PSB_CR_BIF_CTRL);
		(void)psb_ioread32(driver, PSB_CR_BIF_CTRL);
	}
	atomic_set(&driver->needs_tlbflush, 0);
}

static void psb_mmu_flush_pd(struct psb_mmu_driver *driver, int force)
{
	down_write(&driver->sem);
	psb_mmu_flush_pd_locked(driver, force);
	up_write(&driver->sem);
}

void psb_mmu_flush(struct psb_mmu_driver *driver)
{
	u32 val;

	down_write(&driver->sem);
	val = psb_ioread32(driver, PSB_CR_BIF_CTRL);
	if (atomic_read(&driver->needs_tlbflush))
		psb_iowrite32(driver, val | _PSB_CB_CTRL_INVALDC, PSB_CR_BIF_CTRL);
	else
		psb_iowrite32(driver, val | _PSB_CB_CTRL_FLUSH, PSB_CR_BIF_CTRL);
	wmb();
	psb_iowrite32(driver, val & ~(_PSB_CB_CTRL_FLUSH | _PSB_CB_CTRL_INVALDC),
		      PSB_CR_BIF_CTRL);
	(void)psb_ioread32(driver, PSB_CR_BIF_CTRL);
	atomic_set(&driver->needs_tlbflush, 0);
	up_write(&driver->sem);
}

void psb_mmu_set_pd_context(struct psb_mmu_pd *pd, int hw_context)
{
	u32 offset = (hw_context == 0) ? PSB_CR_BIF_DIR_LIST_BASE0 :
	    PSB_CR_BIF_DIR_LIST_BASE1 + hw_context * 4;

	drm_ttm_cache_flush();
	down_write(&pd->driver->sem);
	psb_iowrite32(pd->driver, (page_to_pfn(pd->p) << PAGE_SHIFT), offset);
	wmb();
	psb_mmu_flush_pd_locked(pd->driver, 1);
	pd->hw_context = hw_context;
	up_write(&pd->driver->sem);

}

static inline unsigned long psb_pd_addr_end(unsigned long addr,
					    unsigned long end)
{

	addr = (addr + PSB_PDE_MASK + 1) & ~PSB_PDE_MASK;
	return (addr < end) ? addr : end;
}

struct psb_mmu_pd *psb_mmu_alloc_pd(struct psb_mmu_driver *driver)
{
	struct psb_mmu_pd *pd = kmalloc(sizeof(*pd), GFP_KERNEL);

	if (!pd)
		return NULL;

	pd->p = alloc_page(GFP_DMA32);

	if (!pd->p)
		goto out_err1;

	pd->tables = vmalloc_user(sizeof(struct psb_mmu_pt *) * 1024);
	if (!pd->tables)
		goto out_err2;

	pd->hw_context = -1;
	pd->pd_mask = PSB_PTE_VALID;
	pd->driver = driver;

	return pd;

      out_err2:
	__free_page(pd->p);
      out_err1:
	kfree(pd);
	return NULL;
}

void psb_mmu_free_pt(struct psb_mmu_pt *pt)
{
	__free_page(pt->p);
	kfree(pt);
}

void psb_mmu_free_pagedir(struct psb_mmu_pd *pd)
{
	struct psb_mmu_driver *driver = pd->driver;
	struct psb_mmu_pt *pt;
	int i;

	down_write(&driver->sem);
	if (pd->hw_context != -1) {
		psb_iowrite32(driver, 0,
			      PSB_CR_BIF_DIR_LIST_BASE0 + pd->hw_context * 4);
		psb_mmu_flush_pd_locked(driver, 1);
	}

	/* Should take the spinlock here, but we don't need to do that
	   since we have the semaphore in write mode. */

	for (i = 0; i < 1024; ++i) {
		pt = pd->tables[i];
		if (pt)
			psb_mmu_free_pt(pt);
	}

	vfree(pd->tables);
	__free_page(pd->p);
	kfree(pd);
	up_write(&driver->sem);
}

static struct psb_mmu_pt *psb_mmu_alloc_pt(struct psb_mmu_pd *pd)
{
	struct psb_mmu_pt *pt = kmalloc(sizeof(*pt), GFP_KERNEL);

	if (!pt)
		return NULL;

	pt->p = alloc_page(GFP_DMA32);
	if (!pt->p) {
		kfree(pt);
		return NULL;
	}
	clear_page(kmap(pt->p));
	kunmap(pt->p);
	pt->count = 0;
	pt->pd = pd;
	pt->index = 0;

	return pt;
}

struct psb_mmu_pt *psb_mmu_pt_alloc_map_lock(struct psb_mmu_pd *pd,
					     unsigned long addr)
{
	u32 index = psb_mmu_pd_index(addr);
	struct psb_mmu_pt *pt;
	volatile u32 *v;
	spinlock_t *lock = &pd->driver->lock;

	spin_lock(lock);
	pt = pd->tables[index];
	while (!pt) {
		spin_unlock(lock);
		pt = psb_mmu_alloc_pt(pd);
		if (!pt)
			return NULL;
		spin_lock(lock);

		if (pd->tables[index]) {
			spin_unlock(lock);
			psb_mmu_free_pt(pt);
			spin_lock(lock);
			pt = pd->tables[index];
			continue;
		}

		v = kmap_atomic(pd->p, KM_USER0);
		pd->tables[index] = pt;
		v[index] = (page_to_pfn(pt->p) << 12) | pd->pd_mask;
		pt->index = index;
		kunmap_atomic((void *)v, KM_USER0);

		if (pd->hw_context != -1) {
			psb_mmu_clflush(pd->driver, (void *)&v[index]);
			atomic_set(&pd->driver->needs_tlbflush, 1);
		}
	}
	pt->v = kmap_atomic(pt->p, KM_USER0);
	return pt;
}

static struct psb_mmu_pt *psb_mmu_pt_map_lock(struct psb_mmu_pd *pd,
					      unsigned long addr)
{
	u32 index = psb_mmu_pd_index(addr);
	struct psb_mmu_pt *pt;
	spinlock_t *lock = &pd->driver->lock;

	spin_lock(lock);
	pt = pd->tables[index];
	if (!pt) {
		spin_unlock(lock);
		return NULL;
	}
	pt->v = kmap_atomic(pt->p, KM_USER0);
	return pt;
}

static void psb_mmu_pt_unmap_unlock(struct psb_mmu_pt *pt)
{
	struct psb_mmu_pd *pd = pt->pd;
	volatile u32 *v;

	kunmap_atomic(pt->v, KM_USER0);
	if (pt->count == 0) {
		v = kmap_atomic(pd->p, KM_USER0);
		v[pt->index] &= ~PSB_PTE_VALID;
		pd->tables[pt->index] = NULL;

		if (pd->hw_context != -1) {
			psb_mmu_clflush(pd->driver, (void *)&v[pt->index]);
			atomic_set(&pd->driver->needs_tlbflush, 1);
		}
		kunmap_atomic(pt->v, KM_USER0);
		spin_unlock(&pd->driver->lock);
		psb_mmu_free_pt(pt);
		return;
	}
	spin_unlock(&pd->driver->lock);
}

static inline void psb_mmu_set_pte(struct psb_mmu_pt *pt, unsigned long addr,
				   u32 pte)
{
	pt->v[psb_mmu_pt_index(addr)] = pte;
}

static inline void psb_mmu_invalidate_pte(struct psb_mmu_pt *pt,
					  unsigned long addr)
{
	pt->v[psb_mmu_pt_index(addr)] &= ~PSB_PTE_VALID;
}

#if 0
static u32 psb_mmu_check_pte_locked(struct psb_mmu_pd *pd, __u32 mmu_offset)
{
	u32 *v;
	u32 pfn;

	v = kmap_atomic(pd->p, KM_USER0);
	if (!v) {
		printk(KERN_INFO "Could not kmap pde page.\n");
		return 0;
	}
	pfn = v[psb_mmu_pd_index(mmu_offset)];
	//      printk(KERN_INFO "pde is 0x%08x\n",pfn);
	kunmap_atomic(v, KM_USER0);
	if (((pfn & 0x0F) != PSB_PTE_VALID)) {
		printk(KERN_INFO "Strange pde at 0x%08x: 0x%08x.\n",
		       mmu_offset, pfn);
	}
	v = ioremap(pfn & 0xFFFFF000, 4096);
	if (!v) {
		printk(KERN_INFO "Could not kmap pte page.\n");
		return 0;
	}
	pfn = v[psb_mmu_pt_index(mmu_offset)];
	// printk(KERN_INFO "pte is 0x%08x\n",pfn);
	iounmap(v);
	if (((pfn & 0x0F) != PSB_PTE_VALID)) {
		printk(KERN_INFO "Strange pte at 0x%08x: 0x%08x.\n",
		       mmu_offset, pfn);
	}
	return pfn >> PAGE_SHIFT;
}

static void psb_mmu_check_mirrored_gtt(struct psb_mmu_pd *pd,
				       u32 mmu_offset, __u32 gtt_pages)
{
	u32 start;
	u32 next;

	printk(KERN_INFO "Checking mirrored gtt 0x%08x %d\n",
	       mmu_offset, gtt_pages);
	down_read(&pd->driver->sem);
	start = psb_mmu_check_pte_locked(pd, mmu_offset);
	mmu_offset += PAGE_SIZE;
	gtt_pages -= 1;
	while (gtt_pages--) {
		next = psb_mmu_check_pte_locked(pd, mmu_offset);
		if (next != start + 1) {
			printk(KERN_INFO "Ptes out of order: 0x%08x, 0x%08x.\n",
			       start, next);
		}
		start = next;
		mmu_offset += PAGE_SIZE;
	}
	up_read(&pd->driver->sem);
}

#endif

void psb_mmu_mirror_gtt(struct psb_mmu_pd *pd,
			u32 mmu_offset, __u32 gtt_start, __u32 gtt_pages)
{
	u32 *v;
	u32 start = psb_mmu_pd_index(mmu_offset);
	struct psb_mmu_driver *driver = pd->driver;

	down_read(&driver->sem);
	spin_lock(&driver->lock);

	v = kmap_atomic(pd->p, KM_USER0);
	v += start;

	while (gtt_pages--) {
		*v++ = gtt_start | pd->pd_mask;
		gtt_start += PAGE_SIZE;
	}

	drm_ttm_cache_flush();
	kunmap_atomic(v, KM_USER0);
	spin_unlock(&driver->lock);

	if (pd->hw_context != -1)
		atomic_set(&pd->driver->needs_tlbflush, 1);

	up_read(&pd->driver->sem);
	psb_mmu_flush_pd(pd->driver, 0);
}

struct psb_mmu_pd *psb_mmu_get_default_pd(struct psb_mmu_driver *driver)
{
	struct psb_mmu_pd *pd;

	down_read(&driver->sem);
	pd = driver->default_pd;
	up_read(&driver->sem);

	return pd;
}

/* Returns the physical address of the PD shared by sgx/msvdx */
u32 psb_get_default_pd_addr(struct psb_mmu_driver *driver)
{
	struct psb_mmu_pd *pd;
	
	pd = psb_mmu_get_default_pd(driver);
	return ((page_to_pfn(pd->p) << PAGE_SHIFT));
}

void psb_mmu_driver_takedown(struct psb_mmu_driver *driver)
{
	psb_iowrite32(driver, driver->bif_ctrl, PSB_CR_BIF_CTRL);
	psb_mmu_free_pagedir(driver->default_pd);
	kfree(driver);
}

struct psb_mmu_driver *psb_mmu_driver_init(u8 __iomem * registers)
{
	struct psb_mmu_driver *driver;

	driver = (struct psb_mmu_driver *)kmalloc(sizeof(*driver), GFP_KERNEL);

	if (!driver)
		return NULL;

	driver->default_pd = psb_mmu_alloc_pd(driver);
	if (!driver->default_pd)
		goto out_err1;

	spin_lock_init(&driver->lock);
	init_rwsem(&driver->sem);
	down_write(&driver->sem);
	driver->register_map = registers;
	atomic_set(&driver->needs_tlbflush, 1);

	driver->bif_ctrl = psb_ioread32(driver, PSB_CR_BIF_CTRL);
	psb_iowrite32(driver, driver->bif_ctrl | _PSB_CB_CTRL_CLEAR_FAULT,
		      PSB_CR_BIF_CTRL);
	psb_iowrite32(driver, driver->bif_ctrl & ~_PSB_CB_CTRL_CLEAR_FAULT,
		      PSB_CR_BIF_CTRL);

	driver->has_clflush = 0;

#if defined(CONFIG_X86)
	if (boot_cpu_has(X86_FEATURE_CLFLSH)) {
		u32 tfms, misc, cap0, cap4, clflush_size;

		/*
		 * clflush size is determined at kernel setup for x86_64 but not for
		 * i386. We have to do it here.
		 */

		cpuid(0x00000001, &tfms, &misc, &cap0, &cap4);
		clflush_size = ((misc >> 8) & 0xff) * 8;
		driver->has_clflush = 1;
		driver->clflush_add = PAGE_SIZE * clflush_size / sizeof(u32);
		driver->clflush_mask = driver->clflush_add -1;
		driver->clflush_mask = ~driver->clflush_mask;
	}
#endif

	up_write(&driver->sem);
	return driver;

      out_err1:
	kfree(driver);
	return NULL;
}

static inline u32 psb_mmu_mask_pte(__u32 pfn, int type)
{
	u32 mask = PSB_PTE_VALID;

	if (type & PSB_MMU_CACHED_MEMORY)
		mask |= PSB_PTE_CACHED;
	if (type & PSB_MMU_RO_MEMORY)
		mask |= PSB_PTE_RO;
	if (type & PSB_MMU_WO_MEMORY)
		mask |= PSB_PTE_WO;

	return (pfn << PAGE_SHIFT) | mask;
}

#if defined(CONFIG_X86)
static void psb_mmu_flush_ptes(struct psb_mmu_pd *pd, unsigned long address,
			       u32 num_pages, __u32 desired_tile_stride,
			       u32 hw_tile_stride)
{
	struct psb_mmu_pt *pt;
	u32 rows = 1;
	u32 i;
	unsigned long addr;
	unsigned long end;
	unsigned long next;
	unsigned long add;
	unsigned long row_add;
	unsigned long clflush_add = pd->driver->clflush_add;
	unsigned long clflush_mask = pd->driver->clflush_mask;

	if (!pd->driver->has_clflush) {
		drm_ttm_cache_flush();
		return;
	}

	if (hw_tile_stride)
		rows = num_pages / desired_tile_stride;
	else
		desired_tile_stride = num_pages;

	add = desired_tile_stride << PAGE_SHIFT;
	row_add = hw_tile_stride << PAGE_SHIFT;
	mb();
	for (i = 0; i < rows; ++i) {

		addr = address;
		end = addr + add;

		do {
			next = psb_pd_addr_end(addr, end);
			pt = psb_mmu_pt_map_lock(pd, addr);
			if (!pt)
				continue;
			do {
				psb_clflush(&pt->v[psb_mmu_pt_index(addr)]);
			} while (addr += clflush_add, 
				 (addr & clflush_mask) < next);

			psb_mmu_pt_unmap_unlock(pt);
		} while (addr = next, next != end);
		address += row_add;
	}
	mb();
}
#else
static void psb_mmu_flush_ptes(struct psb_mmu_pd *pd, unsigned long address,
			       u32 num_pages, __u32 desired_tile_stride,
			       u32 hw_tile_stride)
{
	drm_ttm_cache_flush();
}
#endif

void psb_mmu_remove_pfn_sequence(struct psb_mmu_pd *pd,
				 unsigned long address,
				 uint32_t num_pages)
{
	struct psb_mmu_pt *pt;
	unsigned long addr;
	unsigned long end;
	unsigned long next;
	unsigned long f_address = address;

	down_read(&pd->driver->sem);
	preempt_disable();

	addr = address;
	end = addr + (num_pages << PAGE_SHIFT);

	do {
		next = psb_pd_addr_end(addr, end);
		pt = psb_mmu_pt_alloc_map_lock(pd, addr);
		if (!pt)
			goto out;
		do {
			psb_mmu_invalidate_pte(pt, addr);
			--pt->count;
		} while (addr += PAGE_SIZE, addr < next);
		psb_mmu_pt_unmap_unlock(pt);

	} while (addr = next, next != end);

      out:
	if (pd->hw_context != -1)
		psb_mmu_flush_ptes(pd, f_address, num_pages, 1, 1);

	preempt_enable_no_resched();
	up_read(&pd->driver->sem);

	if (pd->hw_context != -1)
		psb_mmu_flush(pd->driver);

	return;
}

void psb_mmu_remove_pages(struct psb_mmu_pd *pd, unsigned long address,
			  u32 num_pages, __u32 desired_tile_stride,
			  u32 hw_tile_stride)
{
	struct psb_mmu_pt *pt;
	u32 rows = 1;
	u32 i;
	unsigned long addr;
	unsigned long end;
	unsigned long next;
	unsigned long add;
	unsigned long row_add;
	unsigned long f_address = address;

	if (hw_tile_stride)
		rows = num_pages / desired_tile_stride;
	else
		desired_tile_stride = num_pages;

	add = desired_tile_stride << PAGE_SHIFT;
	row_add = hw_tile_stride << PAGE_SHIFT;

	down_read(&pd->driver->sem);

	/* Make sure we only need to flush this processor's cache */

	preempt_disable();

	for (i = 0; i < rows; ++i) {

		addr = address;
		end = addr + add;

		do {
			next = psb_pd_addr_end(addr, end);
			pt = psb_mmu_pt_map_lock(pd, addr);
			if (!pt)
				continue;
			do {
				psb_mmu_invalidate_pte(pt, addr);
				--pt->count;

			} while (addr += PAGE_SIZE, addr < next);
			psb_mmu_pt_unmap_unlock(pt);

		} while (addr = next, next != end);
		address += row_add;
	}
	if (pd->hw_context != -1)
		psb_mmu_flush_ptes(pd, f_address, num_pages, desired_tile_stride,
				   hw_tile_stride);

	preempt_enable_no_resched();
	up_read(&pd->driver->sem);

	if (pd->hw_context != -1)
		psb_mmu_flush(pd->driver);
}

int psb_mmu_insert_pfn_sequence(struct psb_mmu_pd *pd, uint32_t start_pfn,
				 unsigned long address, uint32_t num_pages,
				 int type)
{
	struct psb_mmu_pt *pt;
	u32 pte;
	unsigned long addr;
	unsigned long end;
	unsigned long next;
	unsigned long f_address = address;
	int ret = -ENOMEM;

	down_read(&pd->driver->sem);
	preempt_disable();

	addr = address;
	end = addr + (num_pages << PAGE_SHIFT);

	do {
		next = psb_pd_addr_end(addr, end);
		pt = psb_mmu_pt_alloc_map_lock(pd, addr);
		if (!pt) {
			ret = -ENOMEM;
			goto out;
		}
		do {
			pte = psb_mmu_mask_pte(start_pfn++, type);
			psb_mmu_set_pte(pt, addr, pte);
			pt->count++;
		} while (addr += PAGE_SIZE, addr < next);
		psb_mmu_pt_unmap_unlock(pt);

	} while (addr = next, next != end);
	ret = 0;

      out:
	if (pd->hw_context != -1)
		psb_mmu_flush_ptes(pd, f_address, num_pages, 1, 1);

	preempt_enable_no_resched();
	up_read(&pd->driver->sem);

	if (pd->hw_context != -1)
		psb_mmu_flush(pd->driver);

	return 0;
}


int psb_mmu_insert_pages(struct psb_mmu_pd *pd, struct page **pages,
			 unsigned long address, u32 num_pages,
			 u32 desired_tile_stride, u32 hw_tile_stride, int type)
{
	struct psb_mmu_pt *pt;
	u32 rows = 1;
	u32 i;
	u32 pte;
	unsigned long addr;
	unsigned long end;
	unsigned long next;
	unsigned long add;
	unsigned long row_add;
	unsigned long f_address = address;
	int ret = -ENOMEM;

	if (hw_tile_stride) {
		if (num_pages % desired_tile_stride != 0)
			return -EINVAL;
		rows = num_pages / desired_tile_stride;
	} else {
		desired_tile_stride = num_pages;
	}

	add = desired_tile_stride << PAGE_SHIFT;
	row_add = hw_tile_stride << PAGE_SHIFT;

	down_read(&pd->driver->sem);
	preempt_disable();

	for (i = 0; i < rows; ++i) {

		addr = address;
		end = addr + add;

		do {
			next = psb_pd_addr_end(addr, end);
			pt = psb_mmu_pt_alloc_map_lock(pd, addr);
			if (!pt)
				goto out;
			do {
				pte = psb_mmu_mask_pte(page_to_pfn(*pages++),
						       type);
				psb_mmu_set_pte(pt, addr, pte);
				pt->count++;
			} while (addr += PAGE_SIZE, addr < next);
			psb_mmu_pt_unmap_unlock(pt);

		} while (addr = next, next != end);

		address += row_add;
	}
	ret = 0;
      out:
	if (pd->hw_context != -1)
		psb_mmu_flush_ptes(pd, f_address, num_pages, desired_tile_stride,
				   hw_tile_stride);

	preempt_enable_no_resched();
	up_read(&pd->driver->sem);

	if (pd->hw_context != -1)
		psb_mmu_flush(pd->driver);

	return 0;
}

void psb_mmu_enable_requestor(struct psb_mmu_driver *driver, u32 mask)
{
	mask &= _PSB_MMU_ER_MASK;
	psb_iowrite32(driver, psb_ioread32(driver, PSB_CR_BIF_CTRL) & ~mask,
		      PSB_CR_BIF_CTRL);
	(void)psb_ioread32(driver, PSB_CR_BIF_CTRL);
}

void psb_mmu_disable_requestor(struct psb_mmu_driver *driver, u32 mask)
{
	mask &= _PSB_MMU_ER_MASK;
	psb_iowrite32(driver, psb_ioread32(driver, PSB_CR_BIF_CTRL) | mask,
		      PSB_CR_BIF_CTRL);
	(void)psb_ioread32(driver, PSB_CR_BIF_CTRL);
}

void psb_mmu_test(struct psb_mmu_driver *driver, u32 offset)
{
	struct page *p;
	unsigned long pfn;
	int ret = 0;
	struct psb_mmu_pd *pd;
	u32 *v;
	u32 *vmmu;

	pd = driver->default_pd;
	if (!pd) {
		printk(KERN_WARNING "Could not get default pd\n");
	}

	p = alloc_page(GFP_DMA32);

	if (!p) {
		printk(KERN_WARNING "Failed allocating page\n");
		return;
	}

	v = kmap(p);
	memset(v, 0x67, PAGE_SIZE);

	pfn = (offset >> PAGE_SHIFT);

	ret = psb_mmu_insert_pages(pd, &p, pfn << PAGE_SHIFT, 1, 0, 0,
				   PSB_MMU_CACHED_MEMORY);
	if (ret) {
		printk(KERN_WARNING "Failed inserting mmu page\n");
		goto out_err1;
	}

	/* Ioremap the page through the GART aperture */

	vmmu = ioremap(pfn << PAGE_SHIFT, PAGE_SIZE);
	if (!vmmu) {
		printk(KERN_WARNING "Failed ioremapping page\n");
		goto out_err2;
	}

	/* Read from the page with mmu disabled. */
	printk(KERN_INFO "Page first dword is 0x%08x\n", ioread32(vmmu));

	/* Enable the mmu for host accesses and read again. */
	psb_mmu_enable_requestor(driver, _PSB_MMU_ER_HOST);

	printk(KERN_INFO "MMU Page first dword is (0x67676767) 0x%08x\n",
	       ioread32(vmmu));
	*v = 0x15243705;
	printk(KERN_INFO "MMU Page new dword is (0x15243705) 0x%08x\n",
	       ioread32(vmmu));
	iowrite32(0x16243355, vmmu);
	(void)ioread32(vmmu);
	printk(KERN_INFO "Page new dword is (0x16243355) 0x%08x\n", *v);

	printk(KERN_INFO "Int stat is 0x%08x\n",
	       psb_ioread32(driver, PSB_CR_BIF_INT_STAT));
	printk(KERN_INFO "Fault is 0x%08x\n",
	       psb_ioread32(driver, PSB_CR_BIF_FAULT));

	/* Disable MMU for host accesses and clear page fault register */
	psb_mmu_disable_requestor(driver, _PSB_MMU_ER_HOST);
	iounmap(vmmu);
      out_err2:
	psb_mmu_remove_pages(pd, pfn << PAGE_SHIFT, 1, 0, 0);
      out_err1:
	kunmap(p);
	__free_page(p);
}
