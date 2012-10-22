/*
 *  linux/arch/x86_64/mm/init.c
 *
 *  Copyright (C) 1995  Linus Torvalds
 *  Copyright (C) 2000  Pavel Machek <pavel@suse.cz>
 *  Copyright (C) 2002,2003 Andi Kleen <ak@suse.de>
 *
 *  Jun Nakajima <jun.nakajima@intel.com>
 *	Modified for Xen.
 */

#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/ptrace.h>
#include <linux/mman.h>
#include <linux/mm.h>
#include <linux/swap.h>
#include <linux/smp.h>
#include <linux/init.h>
#include <linux/pagemap.h>
#include <linux/bootmem.h>
#include <linux/proc_fs.h>
#include <linux/pci.h>
#include <linux/pfn.h>
#include <linux/poison.h>
#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/memory_hotplug.h>
#include <linux/nmi.h>

#include <asm/processor.h>
#include <asm/system.h>
#include <asm/uaccess.h>
#include <asm/pgtable.h>
#include <asm/pgalloc.h>
#include <asm/dma.h>
#include <asm/fixmap.h>
#include <asm/e820.h>
#include <asm/apic.h>
#include <asm/tlb.h>
#include <asm/mmu_context.h>
#include <asm/proto.h>
#include <asm/smp.h>
#include <asm/sections.h>

#include <xen/features.h>

#ifndef Dprintk
#define Dprintk(x...)
#endif

const struct dma_mapping_ops* dma_ops;
EXPORT_SYMBOL(dma_ops);

#if CONFIG_XEN_COMPAT <= 0x030002
unsigned int __kernel_page_user;
EXPORT_SYMBOL(__kernel_page_user);
#endif

int after_bootmem;

DEFINE_PER_CPU(struct mmu_gather, mmu_gathers);
extern unsigned long start_pfn;

extern pmd_t level2_fixmap_pgt[PTRS_PER_PMD];
extern pte_t level1_fixmap_pgt[PTRS_PER_PTE];

/*
 * Use this until direct mapping is established, i.e. before __va() is 
 * available in init_memory_mapping().
 */

#define addr_to_page(addr, page)				\
	(addr) &= PHYSICAL_PAGE_MASK;				\
	(page) = ((unsigned long *) ((unsigned long)		\
	(((mfn_to_pfn((addr) >> PAGE_SHIFT)) << PAGE_SHIFT) +	\
	__START_KERNEL_map)))

static void __meminit early_make_page_readonly(void *va, unsigned int feature)
{
	unsigned long addr, _va = (unsigned long)va;
	pte_t pte, *ptep;
	unsigned long *page = (unsigned long *) init_level4_pgt;

	BUG_ON(after_bootmem);

	if (xen_feature(feature))
		return;

	addr = (unsigned long) page[pgd_index(_va)];
	addr_to_page(addr, page);

	addr = page[pud_index(_va)];
	addr_to_page(addr, page);

	addr = page[pmd_index(_va)];
	addr_to_page(addr, page);

	ptep = (pte_t *) &page[pte_index(_va)];

	pte.pte = ptep->pte & ~_PAGE_RW;
	if (HYPERVISOR_update_va_mapping(_va, pte, 0))
		BUG();
}

static void __make_page_readonly(void *va)
{
	pgd_t *pgd; pud_t *pud; pmd_t *pmd; pte_t pte, *ptep;
	unsigned long addr = (unsigned long) va;

	pgd = pgd_offset_k(addr);
	pud = pud_offset(pgd, addr);
	pmd = pmd_offset(pud, addr);
	ptep = pte_offset_kernel(pmd, addr);

	pte.pte = ptep->pte & ~_PAGE_RW;
	if (HYPERVISOR_update_va_mapping(addr, pte, 0))
		xen_l1_entry_update(ptep, pte); /* fallback */

	if ((addr >= VMALLOC_START) && (addr < VMALLOC_END))
		__make_page_readonly(__va(pte_pfn(pte) << PAGE_SHIFT));
}

static void __make_page_writable(void *va)
{
	pgd_t *pgd; pud_t *pud; pmd_t *pmd; pte_t pte, *ptep;
	unsigned long addr = (unsigned long) va;

	pgd = pgd_offset_k(addr);
	pud = pud_offset(pgd, addr);
	pmd = pmd_offset(pud, addr);
	ptep = pte_offset_kernel(pmd, addr);

	pte.pte = ptep->pte | _PAGE_RW;
	if (HYPERVISOR_update_va_mapping(addr, pte, 0))
		xen_l1_entry_update(ptep, pte); /* fallback */

	if ((addr >= VMALLOC_START) && (addr < VMALLOC_END))
		__make_page_writable(__va(pte_pfn(pte) << PAGE_SHIFT));
}

void make_page_readonly(void *va, unsigned int feature)
{
	if (!xen_feature(feature))
		__make_page_readonly(va);
}

void make_page_writable(void *va, unsigned int feature)
{
	if (!xen_feature(feature))
		__make_page_writable(va);
}

void make_pages_readonly(void *va, unsigned nr, unsigned int feature)
{
	if (xen_feature(feature))
		return;

	while (nr-- != 0) {
		__make_page_readonly(va);
		va = (void*)((unsigned long)va + PAGE_SIZE);
	}
}

void make_pages_writable(void *va, unsigned nr, unsigned int feature)
{
	if (xen_feature(feature))
		return;

	while (nr-- != 0) {
		__make_page_writable(va);
		va = (void*)((unsigned long)va + PAGE_SIZE);
	}
}

/*
 * NOTE: pagetable_init alloc all the fixmap pagetables contiguous on the
 * physical space so we can cache the place of the first one and move
 * around without checking the pgd every time.
 */

void show_mem(void)
{
	long i, total = 0, reserved = 0;
	long shared = 0, cached = 0;
	pg_data_t *pgdat;
	struct page *page;

	printk(KERN_INFO "Mem-info:\n");
	show_free_areas();
	printk(KERN_INFO "Free swap:       %6ldkB\n", nr_swap_pages<<(PAGE_SHIFT-10));

	for_each_online_pgdat(pgdat) {
               for (i = 0; i < pgdat->node_spanned_pages; ++i) {
			/* this loop can take a while with 256 GB and 4k pages
			   so update the NMI watchdog */
			if (unlikely(i % MAX_ORDER_NR_PAGES == 0)) {
				touch_nmi_watchdog();
			}
			if (!pfn_valid(pgdat->node_start_pfn + i))
				continue;
			page = pfn_to_page(pgdat->node_start_pfn + i);
			total++;
			if (PageReserved(page))
				reserved++;
			else if (PageSwapCache(page))
				cached++;
			else if (page_count(page))
				shared += page_count(page) - 1;
               }
	}
	printk(KERN_INFO "%lu pages of RAM\n", total);
	printk(KERN_INFO "%lu reserved pages\n",reserved);
	printk(KERN_INFO "%lu pages shared\n",shared);
	printk(KERN_INFO "%lu pages swap cached\n",cached);
}


static __init void *spp_getpage(void)
{ 
	void *ptr;
	if (after_bootmem)
		ptr = (void *) get_zeroed_page(GFP_ATOMIC); 
	else if (start_pfn < table_end) {
		ptr = __va(start_pfn << PAGE_SHIFT);
		start_pfn++;
		memset(ptr, 0, PAGE_SIZE);
	} else
		ptr = alloc_bootmem_pages(PAGE_SIZE);
	if (!ptr || ((unsigned long)ptr & ~PAGE_MASK))
		panic("set_pte_phys: cannot allocate page data %s\n", after_bootmem?"after bootmem":"");

	Dprintk("spp_getpage %p\n", ptr);
	return ptr;
} 

#define pgd_offset_u(address) (__user_pgd(init_level4_pgt) + pgd_index(address))
#define pud_offset_u(address) (level3_user_pgt + pud_index(address))

static __init void set_pte_phys(unsigned long vaddr,
			 unsigned long phys, pgprot_t prot, int user_mode)
{
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte, new_pte;

	Dprintk("set_pte_phys %lx to %lx\n", vaddr, phys);

	pgd = (user_mode ? pgd_offset_u(vaddr) : pgd_offset_k(vaddr));
	if (pgd_none(*pgd)) {
		printk("PGD FIXMAP MISSING, it should be setup in head.S!\n");
		return;
	}
	pud = (user_mode ? pud_offset_u(vaddr) : pud_offset(pgd, vaddr));
	if (pud_none(*pud)) {
		pmd = (pmd_t *) spp_getpage(); 
		make_page_readonly(pmd, XENFEAT_writable_page_tables);
		set_pud(pud, __pud(__pa(pmd) | _KERNPG_TABLE | _PAGE_USER));
		if (pmd != pmd_offset(pud, 0)) {
			printk("PAGETABLE BUG #01! %p <-> %p\n", pmd, pmd_offset(pud,0));
			return;
		}
	}
	pmd = pmd_offset(pud, vaddr);
	if (pmd_none(*pmd)) {
		pte = (pte_t *) spp_getpage();
		make_page_readonly(pte, XENFEAT_writable_page_tables);
		set_pmd(pmd, __pmd(__pa(pte) | _KERNPG_TABLE | _PAGE_USER));
		if (pte != pte_offset_kernel(pmd, 0)) {
			printk("PAGETABLE BUG #02!\n");
			return;
		}
	}
	if (pgprot_val(prot))
		new_pte = pfn_pte(phys >> PAGE_SHIFT, prot);
	else
		new_pte = __pte(0);

	pte = pte_offset_kernel(pmd, vaddr);
	if (!pte_none(*pte) && __pte_val(new_pte) &&
	    __pte_val(*pte) != (__pte_val(new_pte) & __supported_pte_mask))
		pte_ERROR(*pte);
	set_pte(pte, new_pte);

	/*
	 * It's enough to flush this one mapping.
	 * (PGE mappings get flushed as well)
	 */
	__flush_tlb_one(vaddr);
}

static __init void set_pte_phys_ma(unsigned long vaddr,
				   unsigned long phys, pgprot_t prot)
{
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte, new_pte;

	Dprintk("set_pte_phys %lx to %lx\n", vaddr, phys);

	pgd = pgd_offset_k(vaddr);
	if (pgd_none(*pgd)) {
		printk("PGD FIXMAP MISSING, it should be setup in head.S!\n");
		return;
	}
	pud = pud_offset(pgd, vaddr);
	if (pud_none(*pud)) {

		pmd = (pmd_t *) spp_getpage(); 
		make_page_readonly(pmd, XENFEAT_writable_page_tables);
		set_pud(pud, __pud(__pa(pmd) | _KERNPG_TABLE | _PAGE_USER));
		if (pmd != pmd_offset(pud, 0)) {
			printk("PAGETABLE BUG #01! %p <-> %p\n", pmd, pmd_offset(pud,0));
			return;
		}
	}
	pmd = pmd_offset(pud, vaddr);
	if (pmd_none(*pmd)) {
		pte = (pte_t *) spp_getpage();
		make_page_readonly(pte, XENFEAT_writable_page_tables);
		set_pmd(pmd, __pmd(__pa(pte) | _KERNPG_TABLE | _PAGE_USER));
		if (pte != pte_offset_kernel(pmd, 0)) {
			printk("PAGETABLE BUG #02!\n");
			return;
		}
	}
	new_pte = pfn_pte_ma(phys >> PAGE_SHIFT, prot);

	pte = pte_offset_kernel(pmd, vaddr);
	if (!pte_none(*pte) && __pte_val(new_pte) &&
#ifdef CONFIG_ACPI
	    /* __acpi_map_table() fails to properly call clear_fixmap() */
	    (vaddr < __fix_to_virt(FIX_ACPI_END) ||
	     vaddr > __fix_to_virt(FIX_ACPI_BEGIN)) &&
#endif
	    __pte_val(*pte) != (__pte_val(new_pte) & __supported_pte_mask))
		pte_ERROR(*pte);
	set_pte(pte, new_pte);

	/*
	 * It's enough to flush this one mapping.
	 * (PGE mappings get flushed as well)
	 */
	__flush_tlb_one(vaddr);
}

/* NOTE: this is meant to be run only at boot */
void __init 
__set_fixmap (enum fixed_addresses idx, unsigned long phys, pgprot_t prot)
{
	unsigned long address = __fix_to_virt(idx);

	if (idx >= __end_of_fixed_addresses) {
		printk("Invalid __set_fixmap\n");
		return;
	}
	switch (idx) {
	case VSYSCALL_LAST_PAGE ... VSYSCALL_FIRST_PAGE:
		set_pte_phys(address, phys, prot, 0);
		set_pte_phys(address, phys, prot, 1);
		break;
	case FIX_EARLYCON_MEM_BASE:
		xen_l1_entry_update(level1_fixmap_pgt + pte_index(address),
				    pfn_pte_ma(phys >> PAGE_SHIFT, prot));
		break;
	default:
		set_pte_phys_ma(address, phys, prot);
		break;
	}
}

unsigned long __meminitdata table_start, table_end;

static __meminit void *alloc_static_page(unsigned long *phys)
{
	unsigned long va = (start_pfn << PAGE_SHIFT) + __START_KERNEL_map;

	if (after_bootmem) {
		void *adr = (void *)get_zeroed_page(GFP_ATOMIC);

		*phys = __pa(adr);
		return adr;
	}

	*phys = start_pfn << PAGE_SHIFT;
	start_pfn++;
	memset((void *)va, 0, PAGE_SIZE);
	return (void *)va;
}

#define PTE_SIZE PAGE_SIZE

static inline int make_readonly(unsigned long paddr)
{
	extern char __vsyscall_0;
	int readonly = 0;

	/* Make new page tables read-only. */
	if (!xen_feature(XENFEAT_writable_page_tables)
	    && (paddr >= (table_start << PAGE_SHIFT))
	    && (paddr < (table_end << PAGE_SHIFT)))
		readonly = 1;
	/* Make old page tables read-only. */
	if (!xen_feature(XENFEAT_writable_page_tables)
	    && (paddr >= (xen_start_info->pt_base - __START_KERNEL_map))
	    && (paddr < (start_pfn << PAGE_SHIFT)))
		readonly = 1;

	/*
	 * No need for writable mapping of kernel image. This also ensures that
	 * page and descriptor tables embedded inside don't have writable
	 * mappings. Exclude the vsyscall area here, allowing alternative
	 * instruction patching to work.
	 */
	if ((paddr >= __pa_symbol(&_text)) && (paddr < __pa_symbol(&_end))
	    && !(paddr >= __pa_symbol(&__vsyscall_0)
	         && paddr < __pa_symbol(&__vsyscall_0) + PAGE_SIZE))
		readonly = 1;

	return readonly;
}

#ifndef CONFIG_XEN
/* Must run before zap_low_mappings */
__meminit void *early_ioremap(unsigned long addr, unsigned long size)
{
	unsigned long vaddr;
	pmd_t *pmd, *last_pmd;
	int i, pmds;

	pmds = ((addr & ~PMD_MASK) + size + ~PMD_MASK) / PMD_SIZE;
	vaddr = __START_KERNEL_map;
	pmd = level2_kernel_pgt;
	last_pmd = level2_kernel_pgt + PTRS_PER_PMD - 1;
	for (; pmd <= last_pmd; pmd++, vaddr += PMD_SIZE) {
		for (i = 0; i < pmds; i++) {
			if (pmd_present(pmd[i]))
				goto next;
		}
		vaddr += addr & ~PMD_MASK;
		addr &= PMD_MASK;
		for (i = 0; i < pmds; i++, addr += PMD_SIZE)
			set_pmd(pmd + i,__pmd(addr | _KERNPG_TABLE | _PAGE_PSE));
		__flush_tlb();
		return (void *)vaddr;
	next:
		;
	}
	printk("early_ioremap(0x%lx, %lu) failed\n", addr, size);
	return NULL;
}

/* To avoid virtual aliases later */
__meminit void early_iounmap(void *addr, unsigned long size)
{
	unsigned long vaddr;
	pmd_t *pmd;
	int i, pmds;

	vaddr = (unsigned long)addr;
	pmds = ((vaddr & ~PMD_MASK) + size + ~PMD_MASK) / PMD_SIZE;
	pmd = level2_kernel_pgt + pmd_index(vaddr);
	for (i = 0; i < pmds; i++)
		pmd_clear(pmd + i);
	__flush_tlb();
}
#endif

static void __meminit
phys_pmd_init(pmd_t *pmd_page, unsigned long address, unsigned long end)
{
	int i = pmd_index(address);

	for (; i < PTRS_PER_PMD; i++) {
		unsigned long pte_phys;
		pmd_t *pmd = pmd_page + i;
		pte_t *pte, *pte_save;
		int k;

		if (address >= end)
			break;

		if (__pmd_val(*pmd)) {
			address += PMD_SIZE;
			continue;
		}

		pte = alloc_static_page(&pte_phys);
		pte_save = pte;
		for (k = 0; k < PTRS_PER_PTE; pte++, k++, address += PTE_SIZE) {
			unsigned long pteval = address | _PAGE_NX | _KERNPG_TABLE;

			if (address >= end ||
			    (!after_bootmem &&
			     (address >> PAGE_SHIFT) >= xen_start_info->nr_pages))
				pteval = 0;
			else if (make_readonly(address))
				pteval &= ~_PAGE_RW;
			set_pte(pte, __pte(pteval & __supported_pte_mask));
		}
		if (!after_bootmem) {
			early_make_page_readonly(pte_save, XENFEAT_writable_page_tables);
			*pmd = __pmd(pte_phys | _KERNPG_TABLE);
		} else {
			make_page_readonly(pte_save, XENFEAT_writable_page_tables);
			set_pmd(pmd, __pmd(pte_phys | _KERNPG_TABLE));
		}
	}
}

static void __meminit
phys_pmd_update(pud_t *pud, unsigned long address, unsigned long end)
{
	pmd_t *pmd = pmd_offset(pud,0);
	spin_lock(&init_mm.page_table_lock);
	phys_pmd_init(pmd, address, end);
	spin_unlock(&init_mm.page_table_lock);
	__flush_tlb_all();
}

static void __meminit phys_pud_init(pud_t *pud_page, unsigned long addr, unsigned long end)
{ 
	int i = pud_index(addr);

	for (; i < PTRS_PER_PUD; i++, addr = (addr & PUD_MASK) + PUD_SIZE ) {
		unsigned long pmd_phys;
		pud_t *pud = pud_page + pud_index(addr);
		pmd_t *pmd;

		if (addr >= end)
			break;

		if (__pud_val(*pud)) {
			phys_pmd_update(pud, addr, end);
			continue;
		}

		pmd = alloc_static_page(&pmd_phys);

		spin_lock(&init_mm.page_table_lock);
		*pud = __pud(pmd_phys | _KERNPG_TABLE);
		phys_pmd_init(pmd, addr, end);
		spin_unlock(&init_mm.page_table_lock);

		early_make_page_readonly(pmd, XENFEAT_writable_page_tables);
	}
	__flush_tlb();
} 

void __init xen_init_pt(void)
{
	unsigned long addr, *page;

	/* Find the initial pte page that was built for us. */
	page = (unsigned long *)xen_start_info->pt_base;
	addr = page[pgd_index(__START_KERNEL_map)];
	addr_to_page(addr, page);

#if CONFIG_XEN_COMPAT <= 0x030002
	/* On Xen 3.0.2 and older we may need to explicitly specify _PAGE_USER
	   in kernel PTEs. We check that here. */
	if (HYPERVISOR_xen_version(XENVER_version, NULL) <= 0x30000) {
		unsigned long *pg;
		pte_t pte;

		/* Mess with the initial mapping of page 0. It's not needed. */
		BUILD_BUG_ON(__START_KERNEL <= __START_KERNEL_map);
		addr = page[pud_index(__START_KERNEL_map)];
		addr_to_page(addr, pg);
		addr = pg[pmd_index(__START_KERNEL_map)];
		addr_to_page(addr, pg);
		pte.pte = pg[pte_index(__START_KERNEL_map)];
		BUG_ON(!(pte.pte & _PAGE_PRESENT));

		/* If _PAGE_USER isn't set, we obviously do not need it. */
		if (pte.pte & _PAGE_USER) {
			/* _PAGE_USER is needed, but is it set implicitly? */
			pte.pte &= ~_PAGE_USER;
			if ((HYPERVISOR_update_va_mapping(__START_KERNEL_map,
							  pte, 0) != 0) ||
			    !(pg[pte_index(__START_KERNEL_map)] & _PAGE_USER))
				/* We need to explicitly specify _PAGE_USER. */
				__kernel_page_user = _PAGE_USER;
		}
	}
#endif

	/* Construct mapping of initial pte page in our own directories. */
	init_level4_pgt[pgd_index(__START_KERNEL_map)] = 
		__pgd(__pa_symbol(level3_kernel_pgt) | _PAGE_TABLE);
	memcpy(level3_kernel_pgt + pud_index(__START_KERNEL_map),
	       page + pud_index(__START_KERNEL_map),
	       (PTRS_PER_PUD - pud_index(__START_KERNEL_map))
	       * sizeof(*level3_kernel_pgt));

	__user_pgd(init_level4_pgt)[pgd_index(VSYSCALL_START)] =
		__pgd(__pa_symbol(level3_user_pgt) | _PAGE_TABLE);

	/* Do an early initialization of the fixmap area. */
	addr = __fix_to_virt(FIX_EARLYCON_MEM_BASE);
	if (pud_present(level3_kernel_pgt[pud_index(addr)])) {
		unsigned long adr = page[pud_index(addr)];

		addr_to_page(adr, page);
		memcpy(level2_fixmap_pgt, page, PAGE_SIZE);
	}
	level3_kernel_pgt[pud_index(addr)] =
		__pud(__pa_symbol(level2_fixmap_pgt) | _PAGE_TABLE);
	level2_fixmap_pgt[pmd_index(addr)] =
		__pmd(__pa_symbol(level1_fixmap_pgt) | _PAGE_TABLE);

	early_make_page_readonly(init_level4_pgt,
				 XENFEAT_writable_page_tables);
	early_make_page_readonly(__user_pgd(init_level4_pgt),
				 XENFEAT_writable_page_tables);
	early_make_page_readonly(level3_kernel_pgt,
				 XENFEAT_writable_page_tables);
	early_make_page_readonly(level3_user_pgt,
				 XENFEAT_writable_page_tables);
	early_make_page_readonly(level2_fixmap_pgt,
				 XENFEAT_writable_page_tables);
	early_make_page_readonly(level1_fixmap_pgt,
				 XENFEAT_writable_page_tables);

	if (!xen_feature(XENFEAT_writable_page_tables)) {
		xen_pgd_pin(__pa_symbol(init_level4_pgt));
		xen_pgd_pin(__pa_symbol(__user_pgd(init_level4_pgt)));
	}
}

static void __init extend_init_mapping(unsigned long tables_space)
{
	unsigned long va = __START_KERNEL_map;
	unsigned long phys, addr, *pte_page;
	pmd_t *pmd;
	pte_t *pte, new_pte;
	unsigned long *page = (unsigned long *)init_level4_pgt;

	addr = page[pgd_index(va)];
	addr_to_page(addr, page);
	addr = page[pud_index(va)];
	addr_to_page(addr, page);

	/* Kill mapping of low 1MB. */
	while (va < (unsigned long)&_text) {
		if (HYPERVISOR_update_va_mapping(va, __pte_ma(0), 0))
			BUG();
		va += PAGE_SIZE;
	}

	/* Ensure init mappings cover kernel text/data and initial tables. */
	while (va < (__START_KERNEL_map
		     + (start_pfn << PAGE_SHIFT)
		     + tables_space)) {
		if (!(pmd_index(va) | pte_index(va))) {
			pud_t *pud;

			page = (unsigned long *)init_level4_pgt;
			addr = page[pgd_index(va)];
			addr_to_page(addr, page);
			pud = (pud_t *)&page[pud_index(va)];
			if (pud_none(*pud)) {
				page = alloc_static_page(&phys);
				early_make_page_readonly(
					page, XENFEAT_writable_page_tables);
				set_pud(pud, __pud(phys | _KERNPG_TABLE));
			} else {
				addr = page[pud_index(va)];
				addr_to_page(addr, page);
			}
		}
		pmd = (pmd_t *)&page[pmd_index(va)];
		if (pmd_none(*pmd)) {
			pte_page = alloc_static_page(&phys);
			early_make_page_readonly(
				pte_page, XENFEAT_writable_page_tables);
			set_pmd(pmd, __pmd(phys | _KERNPG_TABLE));
		} else {
			addr = page[pmd_index(va)];
			addr_to_page(addr, pte_page);
		}
		pte = (pte_t *)&pte_page[pte_index(va)];
		if (pte_none(*pte)) {
			new_pte = pfn_pte(
				(va - __START_KERNEL_map) >> PAGE_SHIFT, 
				__pgprot(_KERNPG_TABLE));
			xen_l1_entry_update(pte, new_pte);
		}
		va += PAGE_SIZE;
	}

	/* Finally, blow away any spurious initial mappings. */
	while (1) {
		if (!(pmd_index(va) | pte_index(va))) {
			page = (unsigned long *)init_level4_pgt;
			addr = page[pgd_index(va)];
			addr_to_page(addr, page);
			if (pud_none(((pud_t *)page)[pud_index(va)]))
				break;
			addr = page[pud_index(va)];
			addr_to_page(addr, page);
		}
		pmd = (pmd_t *)&page[pmd_index(va)];
		if (pmd_none(*pmd))
			break;
		if (HYPERVISOR_update_va_mapping(va, __pte_ma(0), 0))
			BUG();
		va += PAGE_SIZE;
	}
}

static void __init find_early_table_space(unsigned long end)
{
	unsigned long puds, pmds, ptes, tables;

	puds = (end + PUD_SIZE - 1) >> PUD_SHIFT;
	pmds = (end + PMD_SIZE - 1) >> PMD_SHIFT;
	ptes = (end + PTE_SIZE - 1) >> PAGE_SHIFT;

	tables = round_up(puds * 8, PAGE_SIZE) + 
		round_up(pmds * 8, PAGE_SIZE) + 
		round_up(ptes * 8, PAGE_SIZE); 

	extend_init_mapping(tables);

	table_start = start_pfn;
	table_end = table_start + (tables>>PAGE_SHIFT);

	early_printk("kernel direct mapping tables up to %lx @ %lx-%lx\n",
		end, table_start << PAGE_SHIFT,
		(table_start << PAGE_SHIFT) + tables);
}

static void xen_finish_init_mapping(void)
{
	unsigned long i, start, end;

	/* Re-vector virtual addresses pointing into the initial
	   mapping to the just-established permanent ones. */
	xen_start_info = __va(__pa(xen_start_info));
	xen_start_info->pt_base = (unsigned long)
		__va(__pa(xen_start_info->pt_base));
	if (!xen_feature(XENFEAT_auto_translated_physmap)) {
		phys_to_machine_mapping =
			__va(__pa(xen_start_info->mfn_list));
		xen_start_info->mfn_list = (unsigned long)
			phys_to_machine_mapping;
	}
	if (xen_start_info->mod_start)
		xen_start_info->mod_start = (unsigned long)
			__va(__pa(xen_start_info->mod_start));

	/* Destroy the Xen-created mappings beyond the kernel image as
	 * well as the temporary mappings created above. Prevents
	 * overlap with modules area (if init mapping is very big).
	 */
	start = PAGE_ALIGN((unsigned long)_end);
	end   = __START_KERNEL_map + (table_end << PAGE_SHIFT);
	for (; start < end; start += PAGE_SIZE)
		if (HYPERVISOR_update_va_mapping(start, __pte_ma(0), 0))
			BUG();

	/* Allocate pte's for initial fixmaps from 'start_pfn' allocator. */
	table_end = ~0UL;

	/*
	 * Prefetch pte's for the bt_ioremap() area. It gets used before the
	 * boot-time allocator is online, so allocate-on-demand would fail.
	 */
	for (i = FIX_BTMAP_END; i <= FIX_BTMAP_BEGIN; i++)
		__set_fixmap(i, 0, __pgprot(0));

	/* Switch to the real shared_info page, and clear the dummy page. */
	set_fixmap(FIX_SHARED_INFO, xen_start_info->shared_info);
	HYPERVISOR_shared_info = (shared_info_t *)fix_to_virt(FIX_SHARED_INFO);
	memset(empty_zero_page, 0, sizeof(empty_zero_page));

	/* Set up mapping of lowest 1MB of physical memory. */
	for (i = 0; i < NR_FIX_ISAMAPS; i++)
		if (is_initial_xendomain())
			set_fixmap(FIX_ISAMAP_BEGIN - i, i * PAGE_SIZE);
		else
			__set_fixmap(FIX_ISAMAP_BEGIN - i,
				     virt_to_mfn(empty_zero_page)
				     << PAGE_SHIFT,
				     PAGE_KERNEL_RO);

	/* Disable the 'start_pfn' allocator. */
	table_end = start_pfn;
}

/* Setup the direct mapping of the physical memory at PAGE_OFFSET.
   This runs before bootmem is initialized and gets pages directly from the 
   physical memory. To access them they are temporarily mapped. */
void __init_refok init_memory_mapping(unsigned long start, unsigned long end)
{ 
	unsigned long next;

	Dprintk("init_memory_mapping\n");

	/* 
	 * Find space for the kernel direct mapping tables.
	 * Later we should allocate these tables in the local node of the memory
	 * mapped.  Unfortunately this is done currently before the nodes are 
	 * discovered.
	 */
	if (!after_bootmem)
		find_early_table_space(end);

	start = (unsigned long)__va(start);
	end = (unsigned long)__va(end);

	for (; start < end; start = next) {
		unsigned long pud_phys; 
		pgd_t *pgd = pgd_offset_k(start);
		pud_t *pud;

		if (after_bootmem)
			pud = pud_offset(pgd, start & PGDIR_MASK);
		else
			pud = alloc_static_page(&pud_phys);
		next = start + PGDIR_SIZE;
		if (next > end) 
			next = end; 
		phys_pud_init(pud, __pa(start), __pa(next));
		if (!after_bootmem) {
			early_make_page_readonly(pud, XENFEAT_writable_page_tables);
			set_pgd(pgd_offset_k(start), mk_kernel_pgd(pud_phys));
		}
	}

	if (!after_bootmem) {
		BUG_ON(start_pfn != table_end);
		xen_finish_init_mapping();
	}

	__flush_tlb_all();
}

#ifndef CONFIG_NUMA
void __init paging_init(void)
{
	unsigned long max_zone_pfns[MAX_NR_ZONES];
	memset(max_zone_pfns, 0, sizeof(max_zone_pfns));
	max_zone_pfns[ZONE_DMA] = MAX_DMA_PFN;
	max_zone_pfns[ZONE_DMA32] = MAX_DMA32_PFN;
	max_zone_pfns[ZONE_NORMAL] = end_pfn;

	memory_present(0, 0, end_pfn);
	sparse_init();
	free_area_init_nodes(max_zone_pfns);

	SetPagePinned(virt_to_page(init_mm.pgd));
}
#endif

/* Unmap a kernel mapping if it exists. This is useful to avoid prefetches
   from the CPU leading to inconsistent cache lines. address and size
   must be aligned to 2MB boundaries. 
   Does nothing when the mapping doesn't exist. */
void __init clear_kernel_mapping(unsigned long address, unsigned long size) 
{
	unsigned long end = address + size;

	BUG_ON(address & ~LARGE_PAGE_MASK);
	BUG_ON(size & ~LARGE_PAGE_MASK); 
	
	for (; address < end; address += LARGE_PAGE_SIZE) { 
		pgd_t *pgd = pgd_offset_k(address);
		pud_t *pud;
		pmd_t *pmd;
		if (pgd_none(*pgd))
			continue;
		pud = pud_offset(pgd, address);
		if (pud_none(*pud))
			continue; 
		pmd = pmd_offset(pud, address);
		if (!pmd || pmd_none(*pmd))
			continue; 
		if (0 == (__pmd_val(*pmd) & _PAGE_PSE)) {
			/* Could handle this, but it should not happen currently. */
			printk(KERN_ERR 
	       "clear_kernel_mapping: mapping has been split. will leak memory\n"); 
			pmd_ERROR(*pmd); 
		}
		set_pmd(pmd, __pmd(0)); 		
	}
	__flush_tlb_all();
} 

/*
 * Memory hotplug specific functions
 */
void online_page(struct page *page)
{
	ClearPageReserved(page);
	init_page_count(page);
	__free_page(page);
	totalram_pages++;
	num_physpages++;
}

#ifdef CONFIG_MEMORY_HOTPLUG
/*
 * Memory is added always to NORMAL zone. This means you will never get
 * additional DMA/DMA32 memory.
 */
int arch_add_memory(int nid, u64 start, u64 size)
{
	struct pglist_data *pgdat = NODE_DATA(nid);
	struct zone *zone = pgdat->node_zones + ZONE_NORMAL;
	unsigned long start_pfn = start >> PAGE_SHIFT;
	unsigned long nr_pages = size >> PAGE_SHIFT;
	int ret;

	init_memory_mapping(start, (start + size -1));

	ret = __add_pages(zone, start_pfn, nr_pages);
	if (ret)
		goto error;

	return ret;
error:
	printk("%s: Problem encountered in __add_pages!\n", __func__);
	return ret;
}
EXPORT_SYMBOL_GPL(arch_add_memory);

#if !defined(CONFIG_ACPI_NUMA) && defined(CONFIG_NUMA)
int memory_add_physaddr_to_nid(u64 start)
{
	return 0;
}
EXPORT_SYMBOL_GPL(memory_add_physaddr_to_nid);
#endif

#endif /* CONFIG_MEMORY_HOTPLUG */

#ifdef CONFIG_MEMORY_HOTPLUG_RESERVE
/*
 * Memory Hotadd without sparsemem. The mem_maps have been allocated in advance,
 * just online the pages.
 */
int __add_pages(struct zone *z, unsigned long start_pfn, unsigned long nr_pages)
{
	int err = -EIO;
	unsigned long pfn;
	unsigned long total = 0, mem = 0;
	for (pfn = start_pfn; pfn < start_pfn + nr_pages; pfn++) {
		if (pfn_valid(pfn)) {
			online_page(pfn_to_page(pfn));
			err = 0;
			mem++;
		}
		total++;
	}
	if (!err) {
		z->spanned_pages += total;
		z->present_pages += mem;
		z->zone_pgdat->node_spanned_pages += total;
		z->zone_pgdat->node_present_pages += mem;
	}
	return err;
}
#endif

static struct kcore_list kcore_mem, kcore_vmalloc, kcore_kernel, kcore_modules,
			 kcore_vsyscall;

void __init mem_init(void)
{
	long codesize, reservedpages, datasize, initsize;
	unsigned long pfn;

	pci_iommu_alloc();

	/* clear the zero-page */
	memset(empty_zero_page, 0, PAGE_SIZE);

	reservedpages = 0;

	/* this will put all low memory onto the freelists */
#ifdef CONFIG_NUMA
	totalram_pages = numa_free_all_bootmem();
#else
	totalram_pages = free_all_bootmem();
#endif
	/* XEN: init pages outside initial allocation. */
	for (pfn = xen_start_info->nr_pages; pfn < max_pfn; pfn++) {
		ClearPageReserved(pfn_to_page(pfn));
		init_page_count(pfn_to_page(pfn));
	}
	reservedpages = end_pfn - totalram_pages -
					absent_pages_in_range(0, end_pfn);

	after_bootmem = 1;

	codesize =  (unsigned long) &_etext - (unsigned long) &_text;
	datasize =  (unsigned long) &_edata - (unsigned long) &_etext;
	initsize =  (unsigned long) &__init_end - (unsigned long) &__init_begin;

	/* Register memory areas for /proc/kcore */
	kclist_add(&kcore_mem, __va(0), max_low_pfn << PAGE_SHIFT); 
	kclist_add(&kcore_vmalloc, (void *)VMALLOC_START, 
		   VMALLOC_END-VMALLOC_START);
	kclist_add(&kcore_kernel, &_stext, _end - _stext);
	kclist_add(&kcore_modules, (void *)MODULES_VADDR, MODULES_LEN);
	kclist_add(&kcore_vsyscall, (void *)VSYSCALL_START, 
				 VSYSCALL_END - VSYSCALL_START);

	printk("Memory: %luk/%luk available (%ldk kernel code, %ldk reserved, %ldk data, %ldk init)\n",
		(unsigned long) nr_free_pages() << (PAGE_SHIFT-10),
		end_pfn << (PAGE_SHIFT-10),
		codesize >> 10,
		reservedpages << (PAGE_SHIFT-10),
		datasize >> 10,
		initsize >> 10);
}

void free_init_pages(char *what, unsigned long begin, unsigned long end)
{
	unsigned long addr;

	if (begin >= end)
		return;

	printk(KERN_INFO "Freeing %s: %luk freed\n", what, (end - begin) >> 10);
	for (addr = begin; addr < end; addr += PAGE_SIZE) {
		ClearPageReserved(virt_to_page(addr));
		init_page_count(virt_to_page(addr));
		memset((void *)(addr & ~(PAGE_SIZE-1)),
		       POISON_FREE_INITMEM, PAGE_SIZE); 
		if (addr >= __START_KERNEL_map) {
			/* make_readonly() reports all kernel addresses. */
			__make_page_writable(__va(__pa(addr)));
			change_page_attr_addr(addr, 1, __pgprot(0));
		}
		free_page(addr);
		totalram_pages++;
	}
	if (addr > __START_KERNEL_map)
		global_flush_tlb();
}

void free_initmem(void)
{
	free_init_pages("unused kernel memory",
			(unsigned long)(&__init_begin),
			(unsigned long)(&__init_end));
}

#ifdef CONFIG_DEBUG_RODATA

void mark_rodata_ro(void)
{
	unsigned long start = (unsigned long)_stext, end;

#ifdef CONFIG_HOTPLUG_CPU
	/* It must still be possible to apply SMP alternatives. */
	if (num_possible_cpus() > 1)
		start = (unsigned long)_etext;
#endif

#ifdef CONFIG_KPROBES
	start = (unsigned long)__start_rodata;
#endif

	end = (unsigned long)__end_rodata;
	start = (start + PAGE_SIZE - 1) & PAGE_MASK;
	end &= PAGE_MASK;
	if (end <= start)
		return;

	change_page_attr_addr(start, (end - start) >> PAGE_SHIFT, PAGE_KERNEL_RO);

	printk(KERN_INFO "Write protecting the kernel read-only data: %luk\n",
	       (end - start) >> 10);

	/*
	 * change_page_attr_addr() requires a global_flush_tlb() call after it.
	 * We do this after the printk so that if something went wrong in the
	 * change, the printk gets out at least to give a better debug hint
	 * of who is the culprit.
	 */
	global_flush_tlb();
}
#endif

#ifdef CONFIG_BLK_DEV_INITRD
void free_initrd_mem(unsigned long start, unsigned long end)
{
	free_init_pages("initrd memory", start, end);
}
#endif

void __init reserve_bootmem_generic(unsigned long phys, unsigned len) 
{ 
#ifdef CONFIG_NUMA
	int nid = phys_to_nid(phys);
#endif
	unsigned long pfn = phys >> PAGE_SHIFT;
	if (pfn >= end_pfn) {
		/* This can happen with kdump kernels when accessing firmware
		   tables. */
		if (pfn < end_pfn_map)
			return;
		printk(KERN_ERR "reserve_bootmem: illegal reserve %lx %u\n",
				phys, len);
		return;
	}

	/* Should check here against the e820 map to avoid double free */
#ifdef CONFIG_NUMA
  	reserve_bootmem_node(NODE_DATA(nid), phys, len);
#else       		
	reserve_bootmem(phys, len);    
#endif
#ifndef CONFIG_XEN
	if (phys+len <= MAX_DMA_PFN*PAGE_SIZE) {
		dma_reserve += len / PAGE_SIZE;
		set_dma_reserve(dma_reserve);
	}
#endif
}

int kern_addr_valid(unsigned long addr) 
{ 
	unsigned long above = ((long)addr) >> __VIRTUAL_MASK_SHIFT;
       pgd_t *pgd;
       pud_t *pud;
       pmd_t *pmd;
       pte_t *pte;

	if (above != 0 && above != -1UL)
		return 0; 
	
	pgd = pgd_offset_k(addr);
	if (pgd_none(*pgd))
		return 0;

	pud = pud_offset(pgd, addr);
	if (pud_none(*pud))
		return 0; 

	pmd = pmd_offset(pud, addr);
	if (pmd_none(*pmd))
		return 0;
	if (pmd_large(*pmd))
		return pfn_valid(pmd_pfn(*pmd));

	pte = pte_offset_kernel(pmd, addr);
	if (pte_none(*pte))
		return 0;
	return pfn_valid(pte_pfn(*pte));
}

/* A pseudo VMA to allow ptrace access for the vsyscall page.  This only
   covers the 64bit vsyscall page now. 32bit has a real VMA now and does
   not need special handling anymore. */

static struct vm_area_struct gate_vma = {
	.vm_start = VSYSCALL_START,
	.vm_end = VSYSCALL_START + (VSYSCALL_MAPPED_PAGES << PAGE_SHIFT),
	.vm_page_prot = PAGE_READONLY_EXEC,
	.vm_flags = VM_READ | VM_EXEC
};

struct vm_area_struct *get_gate_vma(struct task_struct *tsk)
{
#ifdef CONFIG_IA32_EMULATION
	if (test_tsk_thread_flag(tsk, TIF_IA32))
		return NULL;
#endif
	return &gate_vma;
}

int in_gate_area(struct task_struct *task, unsigned long addr)
{
	struct vm_area_struct *vma = get_gate_vma(task);
	if (!vma)
		return 0;
	return (addr >= vma->vm_start) && (addr < vma->vm_end);
}

/* Use this when you have no reliable task/vma, typically from interrupt
 * context.  It is less reliable than using the task's vma and may give
 * false positives.
 */
int in_gate_area_no_task(unsigned long addr)
{
	return (addr >= VSYSCALL_START) && (addr < VSYSCALL_END);
}

const char *arch_vma_name(struct vm_area_struct *vma)
{
	if (vma->vm_mm && vma->vm_start == (long)vma->vm_mm->context.vdso)
		return "[vdso]";
	if (vma == &gate_vma)
		return "[vsyscall]";
	return NULL;
}

#ifdef CONFIG_SPARSEMEM_VMEMMAP
/*
 * Initialise the sparsemem vmemmap using huge-pages at the PMD level.
 */
int __meminit vmemmap_populate(struct page *start_page,
						unsigned long size, int node)
{
	unsigned long addr = (unsigned long)start_page;
	unsigned long end = (unsigned long)(start_page + size);
	unsigned long next;
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;

	for (; addr < end; addr = next) {
		next = pmd_addr_end(addr, end);

		pgd = vmemmap_pgd_populate(addr, node);
		if (!pgd)
			return -ENOMEM;
		pud = vmemmap_pud_populate(pgd, addr, node);
		if (!pud)
			return -ENOMEM;

		pmd = pmd_offset(pud, addr);
		if (pmd_none(*pmd)) {
			pte_t entry;
			void *p = vmemmap_alloc_block(PMD_SIZE, node);
			if (!p)
				return -ENOMEM;

			entry = pfn_pte(__pa(p) >> PAGE_SHIFT, PAGE_KERNEL);
			mk_pte_huge(entry);
			set_pmd(pmd, __pmd(pte_val(entry)));

			printk(KERN_DEBUG " [%lx-%lx] PMD ->%p on node %d\n",
				addr, addr + PMD_SIZE - 1, p, node);
		} else
			vmemmap_verify((pte_t *)pmd, node, addr, next);
	}

	return 0;
}
#endif
