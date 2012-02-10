/* Copyright 2002,2003 Andi Kleen, SuSE Labs */

/* vsyscall handling for 32bit processes. Map a stub page into it 
   on demand because 32bit cannot reach the kernel's fixmaps */

#include <linux/mm.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/gfp.h>
#include <linux/init.h>
#include <linux/stringify.h>
#include <linux/security.h>
#include <asm/mman.h>
#include <asm/proto.h>
#include <asm/tlbflush.h>
#include <asm/ia32_unistd.h>
#include <asm/vsyscall32.h>

#include <bc/vmpages.h>

extern unsigned char syscall32_syscall[], syscall32_syscall_end[];
extern unsigned char syscall32_sysenter[], syscall32_sysenter_end[];
extern int sysctl_vsyscall32;

char *syscall32_page;
EXPORT_SYMBOL_GPL(syscall32_page);
struct page *syscall32_pages[1];
EXPORT_SYMBOL_GPL(syscall32_pages);
static int use_sysenter = -1;

struct linux_binprm;

/* Setup a VMA at program startup for the vsyscall page */
int syscall32_setup_pages(struct linux_binprm *bprm, int exstack,
				unsigned long map_address)
{
	int npages = (__VSYSCALL32_END - __VSYSCALL32_BASE) >> PAGE_SHIFT;
	struct mm_struct *mm = current->mm;
	unsigned long flags;
	unsigned long addr = map_address ? : __VSYSCALL32_BASE;
	int ret;

	if (sysctl_vsyscall32 == 0 && map_address == 0)
		return 0;

	flags = VM_READ | VM_EXEC | VM_MAYREAD | VM_MAYEXEC | VM_MAYWRITE |
		mm->def_flags;

	ret = -ENOMEM;
	if (ub_memory_charge(mm, __VSYSCALL32_END - __VSYSCALL32_BASE,
			flags, NULL, UB_SOFT))
		goto err_charge;

	down_write(&mm->mmap_sem);
	addr = get_unmapped_area(NULL, addr, PAGE_SIZE * npages, 0,
			MAP_PRIVATE | MAP_FIXED);
	if (unlikely(addr & ~PAGE_MASK)) {
		ret = addr;
		goto err_ins;
	}
	/*
	 * MAYWRITE to allow gdb to COW and set breakpoints
	 *
	 * Make sure the vDSO gets into every core dump.
	 * Dumping its contents makes post-mortem fully interpretable later
	 * without matching up the same kernel and hardware config to see
	 * what PC values meant.
	 */
	/* Could randomize here */
	ret = install_special_mapping(mm, addr, PAGE_SIZE * npages,
				      VM_READ|VM_EXEC|
				      VM_MAYREAD|VM_MAYWRITE|VM_MAYEXEC|
				      VM_ALWAYSDUMP,
				      syscall32_pages);
	if (ret == 0) {
		mm->context.vdso = (void *)addr;
		current_thread_info()->sysenter_return = VSYSCALL32_SYSEXIT;
	}
	up_write(&mm->mmap_sem);
	if (ret < 0)
err_ins:
		ub_memory_uncharge(mm, __VSYSCALL32_END - __VSYSCALL32_BASE, flags, NULL);
err_charge:
	return ret;
}
EXPORT_SYMBOL_GPL(syscall32_setup_pages);

static int __init init_syscall32(void)
{ 
	syscall32_page = (void *)get_zeroed_page(GFP_KERNEL);
	if (!syscall32_page) 
		panic("Cannot allocate syscall32 page"); 
	syscall32_pages[0] = virt_to_page(syscall32_page);
 	if (use_sysenter > 0) {
 		memcpy(syscall32_page, syscall32_sysenter,
 		       syscall32_sysenter_end - syscall32_sysenter);
 	} else {
  		memcpy(syscall32_page, syscall32_syscall,
  		       syscall32_syscall_end - syscall32_syscall);
  	}	
	return 0;
} 
	
__initcall(init_syscall32); 

/* May not be __init: called during resume */
void syscall32_cpu_init(void)
{
	if (use_sysenter < 0)
 		use_sysenter = (boot_cpu_data.x86_vendor == X86_VENDOR_INTEL);

	/* Load these always in case some future AMD CPU supports
	   SYSENTER from compat mode too. */
	checking_wrmsrl(MSR_IA32_SYSENTER_CS, (u64)__KERNEL_CS);
	checking_wrmsrl(MSR_IA32_SYSENTER_ESP, 0ULL);
	checking_wrmsrl(MSR_IA32_SYSENTER_EIP, (u64)ia32_sysenter_target);

	wrmsrl(MSR_CSTAR, ia32_cstar_target);
}
