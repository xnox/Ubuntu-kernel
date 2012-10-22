#include <linux/highmem.h>
#include <linux/module.h>

void *kmap(struct page *page)
{
	might_sleep();
	if (!PageHighMem(page))
		return page_address(page);
	return kmap_high(page);
}

void kunmap(struct page *page)
{
	if (in_interrupt())
		BUG();
	if (!PageHighMem(page))
		return;
	kunmap_high(page);
}

/*
 * kmap_atomic/kunmap_atomic is significantly faster than kmap/kunmap because
 * no global lock is needed and because the kmap code must perform a global TLB
 * invalidation when the kmap pool wraps.
 *
 * However when holding an atomic kmap is is not legal to sleep, so atomic
 * kmaps are appropriate for short, tight code paths only.
 */
static void *__kmap_atomic(struct page *page, enum km_type type, pgprot_t prot)
{
	enum fixed_addresses idx;
	unsigned long vaddr;

	/* even !CONFIG_PREEMPT needs this, for in_atomic in do_page_fault */
	pagefault_disable();

	idx = type + KM_TYPE_NR*smp_processor_id();
	BUG_ON(!pte_none(*(kmap_pte-idx)));

	if (!PageHighMem(page))
		return page_address(page);

	vaddr = __fix_to_virt(FIX_KMAP_BEGIN + idx);
	set_pte_at(&init_mm, vaddr, kmap_pte-idx, mk_pte(page, prot));
	/*arch_flush_lazy_mmu_mode();*/

	return (void*) vaddr;
}

void *kmap_atomic(struct page *page, enum km_type type)
{
	return __kmap_atomic(page, type, kmap_prot);
}

/* Same as kmap_atomic but with PAGE_KERNEL_RO page protection. */
void *kmap_atomic_pte(struct page *page, enum km_type type)
{
	return __kmap_atomic(page, type,
	                     test_bit(PG_pinned, &page->flags)
	                     ? PAGE_KERNEL_RO : kmap_prot);
}

void kunmap_atomic(void *kvaddr, enum km_type type)
{
	unsigned long vaddr = (unsigned long) kvaddr & PAGE_MASK;
	enum fixed_addresses idx = type + KM_TYPE_NR*smp_processor_id();

	/*
	 * Force other mappings to Oops if they'll try to access this pte
	 * without first remap it.  Keeping stale mappings around is a bad idea
	 * also, in case the page changes cacheability attributes or becomes
	 * a protected page in a hypervisor.
	 */
	if (vaddr == __fix_to_virt(FIX_KMAP_BEGIN+idx))
		kpte_clear_flush(kmap_pte-idx, vaddr);
	else {
#ifdef CONFIG_DEBUG_HIGHMEM
		BUG_ON(vaddr < PAGE_OFFSET);
		BUG_ON(vaddr >= (unsigned long)high_memory);
#endif
	}

	pagefault_enable();
}

/* This is the same as kmap_atomic() but can map memory that doesn't
 * have a struct page associated with it.
 */
void *kmap_atomic_pfn(unsigned long pfn, enum km_type type)
{
	enum fixed_addresses idx;
	unsigned long vaddr;

	pagefault_disable();

	idx = type + KM_TYPE_NR*smp_processor_id();
	vaddr = __fix_to_virt(FIX_KMAP_BEGIN + idx);
	set_pte(kmap_pte-idx, pfn_pte(pfn, kmap_prot));
	/*arch_flush_lazy_mmu_mode();*/

	return (void*) vaddr;
}

struct page *kmap_atomic_to_page(void *ptr)
{
	unsigned long idx, vaddr = (unsigned long)ptr;
	pte_t *pte;

	if (vaddr < FIXADDR_START)
		return virt_to_page(ptr);

	idx = virt_to_fix(vaddr);
	pte = kmap_pte - (idx - FIX_KMAP_BEGIN);
	return pte_page(*pte);
}

void clear_highpage(struct page *page)
{
	void *kaddr;

	if (likely(xen_feature(XENFEAT_highmem_assist))
	    && PageHighMem(page)) {
		struct mmuext_op meo;

		meo.cmd = MMUEXT_CLEAR_PAGE;
		meo.arg1.mfn = pfn_to_mfn(page_to_pfn(page));
		if (HYPERVISOR_mmuext_op(&meo, 1, NULL, DOMID_SELF) == 0)
			return;
	}

	kaddr = kmap_atomic(page, KM_USER0);
	clear_page(kaddr);
	kunmap_atomic(kaddr, KM_USER0);
}

void copy_highpage(struct page *to, struct page *from)
{
	void *vfrom, *vto;

	if (likely(xen_feature(XENFEAT_highmem_assist))
	    && (PageHighMem(from) || PageHighMem(to))) {
		unsigned long from_pfn = page_to_pfn(from);
		unsigned long to_pfn = page_to_pfn(to);
		struct mmuext_op meo;

		meo.cmd = MMUEXT_COPY_PAGE;
		meo.arg1.mfn = pfn_to_mfn(to_pfn);
		meo.arg2.src_mfn = pfn_to_mfn(from_pfn);
		if (mfn_to_pfn(meo.arg2.src_mfn) == from_pfn
		    && mfn_to_pfn(meo.arg1.mfn) == to_pfn
		    && HYPERVISOR_mmuext_op(&meo, 1, NULL, DOMID_SELF) == 0)
			return;
	}

	vfrom = kmap_atomic(from, KM_USER0);
	vto = kmap_atomic(to, KM_USER1);
	copy_page(vto, vfrom);
	kunmap_atomic(vfrom, KM_USER0);
	kunmap_atomic(vto, KM_USER1);
}

EXPORT_SYMBOL(kmap);
EXPORT_SYMBOL(kunmap);
EXPORT_SYMBOL(kmap_atomic);
EXPORT_SYMBOL(kmap_atomic_pte);
EXPORT_SYMBOL(kunmap_atomic);
EXPORT_SYMBOL(kmap_atomic_to_page);
EXPORT_SYMBOL(clear_highpage);
EXPORT_SYMBOL(copy_highpage);
