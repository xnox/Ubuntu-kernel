#ifndef _ASM_VSYSCALL32_H
#define _ASM_VSYSCALL32_H 1

/* Values need to match arch/x86_64/ia32/vsyscall.lds */

#ifdef __ASSEMBLY__
#define __IA32_PAGE_OFFSET 0xc0000000
#define VSYSCALL32_BASE (__IA32_PAGE_OFFSET - PAGE_SIZE)
#define VSYSCALL32_SYSEXIT (VSYSCALL32_BASE + 0x420)
#else
#define VSYSCALL32_BASE ((unsigned long)current->mm->context.vdso)
#define VSYSCALL32_END (VSYSCALL32_BASE + PAGE_SIZE)
#define VSYSCALL32_EHDR ((const struct elf32_hdr *) VSYSCALL32_BASE)

#define __VSYSCALL32_BASE ((unsigned long)(IA32_PAGE_OFFSET - PAGE_SIZE))
#define __VSYSCALL32_END (__VSYSCALL32_BASE + PAGE_SIZE)

#define VSYSCALL32_VSYSCALL ((void *)VSYSCALL32_BASE + 0x400) 
#define VSYSCALL32_SYSEXIT ((void *)VSYSCALL32_BASE + 0x420)
#define VSYSCALL32_SIGRETURN ((void __user *)VSYSCALL32_BASE + 0x500) 
#define VSYSCALL32_RTSIGRETURN ((void __user *)VSYSCALL32_BASE + 0x600) 
#endif

#endif
