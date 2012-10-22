#ifndef _X86_64_PGALLOC_H
#define _X86_64_PGALLOC_H

#include <asm/pda.h>
#include <linux/threads.h>
#include <linux/mm.h>
#include <asm/io.h>		/* for phys_to_virt and page_to_pseudophys */

pmd_t *early_get_pmd(unsigned long va);
void early_make_page_readonly(void *va, unsigned int feature);

#define __user_pgd(pgd) ((pgd) + PTRS_PER_PGD)

#define pmd_populate_kernel(mm, pmd, pte) \
		set_pmd(pmd, __pmd(_PAGE_TABLE | __pa(pte)))

static inline void pud_populate(struct mm_struct *mm, pud_t *pud, pmd_t *pmd)
{
	if (unlikely(PagePinned(virt_to_page((mm)->pgd)))) {
		BUG_ON(HYPERVISOR_update_va_mapping(
			       (unsigned long)pmd,
			       pfn_pte(virt_to_phys(pmd)>>PAGE_SHIFT, 
				       PAGE_KERNEL_RO), 0));
		set_pud(pud, __pud(_PAGE_TABLE | __pa(pmd)));
	} else {
		*(pud) =  __pud(_PAGE_TABLE | __pa(pmd));
	}
}

/*
 * We need to use the batch mode here, but pgd_pupulate() won't be
 * be called frequently.
 */
static inline void pgd_populate(struct mm_struct *mm, pgd_t *pgd, pud_t *pud)
{
	if (unlikely(PagePinned(virt_to_page((mm)->pgd)))) {
		BUG_ON(HYPERVISOR_update_va_mapping(
			       (unsigned long)pud,
			       pfn_pte(virt_to_phys(pud)>>PAGE_SHIFT, 
				       PAGE_KERNEL_RO), 0));
		set_pgd(pgd, __pgd(_PAGE_TABLE | __pa(pud)));
		set_pgd(__user_pgd(pgd), __pgd(_PAGE_TABLE | __pa(pud)));
	} else {
		*(pgd) =  __pgd(_PAGE_TABLE | __pa(pud));
		*(__user_pgd(pgd)) = *(pgd);
	}
}

#define pmd_pgtable(pmd) pmd_page(pmd)

static inline void pmd_populate(struct mm_struct *mm, pmd_t *pmd, struct page *pte)
{
	if (unlikely(PagePinned(virt_to_page((mm)->pgd)))) {
		BUG_ON(HYPERVISOR_update_va_mapping(
			       (unsigned long)__va(page_to_pfn(pte) << PAGE_SHIFT),
			       pfn_pte(page_to_pfn(pte), PAGE_KERNEL_RO), 0));
		set_pmd(pmd, __pmd(_PAGE_TABLE | (page_to_pfn(pte) << PAGE_SHIFT)));
	} else {
		*(pmd) = __pmd(_PAGE_TABLE | (page_to_pfn(pte) << PAGE_SHIFT));
	}
}

extern void __pmd_free(pgtable_t);
static inline void pmd_free(struct mm_struct *mm, pmd_t *pmd)
{
	BUG_ON((unsigned long)pmd & (PAGE_SIZE-1));
	__pmd_free(virt_to_page(pmd));
}

extern pmd_t *pmd_alloc_one(struct mm_struct *mm, unsigned long addr);

static inline pud_t *pud_alloc_one(struct mm_struct *mm, unsigned long addr)
{
	return (pud_t *)pmd_alloc_one(mm, addr);
}

static inline void pud_free(struct mm_struct *mm, pud_t *pud)
{
	BUG_ON((unsigned long)pud & (PAGE_SIZE-1));
	__pmd_free(virt_to_page(pud));
}

static inline void pgd_list_add(pgd_t *pgd)
{
	struct page *page = virt_to_page(pgd);
	unsigned long flags;

	spin_lock_irqsave(&pgd_lock, flags);
	list_add(&page->lru, &pgd_list);
	spin_unlock_irqrestore(&pgd_lock, flags);
}

static inline void pgd_list_del(pgd_t *pgd)
{
	struct page *page = virt_to_page(pgd);
	unsigned long flags;

	spin_lock_irqsave(&pgd_lock, flags);
	list_del(&page->lru);
	spin_unlock_irqrestore(&pgd_lock, flags);
}

extern void pgd_test_and_unpin(pgd_t *);

static inline pgd_t *pgd_alloc(struct mm_struct *mm)
{
	/*
	 * We allocate two contiguous pages for kernel and user.
	 */
	unsigned boundary;
	pgd_t *pgd = (pgd_t *)__get_free_pages(GFP_KERNEL|__GFP_REPEAT, 1);
	if (!pgd)
		return NULL;
	pgd_list_add(pgd);
	pgd_test_and_unpin(pgd);
	/*
	 * Copy kernel pointers in from init.
	 * Could keep a freelist or slab cache of those because the kernel
	 * part never changes.
	 */
	boundary = pgd_index(__PAGE_OFFSET);
	memset(pgd, 0, boundary * sizeof(pgd_t));
	memcpy(pgd + boundary,
	       init_level4_pgt + boundary,
	       (PTRS_PER_PGD - boundary) * sizeof(pgd_t));

	memset(__user_pgd(pgd), 0, PAGE_SIZE); /* clean up user pgd */
	/*
	 * Set level3_user_pgt for vsyscall area
	 */
	__user_pgd(pgd)[pgd_index(VSYSCALL_START)] =
		__pgd(__pa_symbol(level3_user_pgt) | _PAGE_TABLE);
	return pgd;
}

static inline void pgd_free(struct mm_struct *mm, pgd_t *pgd)
{
	pgd_test_and_unpin(pgd);
	pgd_list_del(pgd);
	free_pages((unsigned long)pgd, 1);
}

static inline pte_t *pte_alloc_one_kernel(struct mm_struct *mm, unsigned long address)
{
	pte_t *pte = (pte_t *)get_zeroed_page(GFP_KERNEL|__GFP_REPEAT);
	if (pte)
		make_page_readonly(pte, XENFEAT_writable_page_tables);

	return pte;
}

extern pgtable_t pte_alloc_one(struct mm_struct *mm, unsigned long addr);

/* Should really implement gc for free page table pages. This could be
   done with a reference count in struct page. */

static inline void pte_free_kernel(struct mm_struct *mm, pte_t *pte)
{
	BUG_ON((unsigned long)pte & (PAGE_SIZE-1));
	make_page_writable(pte, XENFEAT_writable_page_tables);
	free_page((unsigned long)pte); 
}

extern void __pte_free(pgtable_t);
static inline void pte_free(struct mm_struct *mm, pgtable_t pte)
{
	__pte_free(pte);
}

#define __pte_free_tlb(tlb,pte)				\
do {							\
	pgtable_page_dtor((pte));				\
	tlb_remove_page((tlb), (pte));			\
} while (0)

#define __pmd_free_tlb(tlb,x)   tlb_remove_page((tlb),virt_to_page(x))
#define __pud_free_tlb(tlb,x)   tlb_remove_page((tlb),virt_to_page(x))

#endif /* _X86_64_PGALLOC_H */
