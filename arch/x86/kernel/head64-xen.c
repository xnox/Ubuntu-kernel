/*
 *  prepare to run common code
 *
 *  Copyright (C) 2000 Andrea Arcangeli <andrea@suse.de> SuSE
 *
 *  Jun Nakajima <jun.nakajima@intel.com>
 *	Modified for Xen.
 */

/* PDA is not ready to be used until the end of x86_64_start_kernel(). */
#define arch_use_lazy_mmu_mode() false

#include <linux/init.h>
#include <linux/linkage.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/percpu.h>
#include <linux/start_kernel.h>
#include <linux/module.h>

#include <asm/processor.h>
#include <asm/proto.h>
#include <asm/smp.h>
#include <asm/setup.h>
#include <asm/desc.h>
#include <asm/pgtable.h>
#include <asm/tlbflush.h>
#include <asm/sections.h>
#include <asm/kdebug.h>
#include <asm/e820.h>

unsigned long start_pfn;

#ifndef CONFIG_XEN
static void __init zap_identity_mappings(void)
{
	pgd_t *pgd = pgd_offset_k(0UL);
	pgd_clear(pgd);
	__flush_tlb_all();
}

/* Don't add a printk in there. printk relies on the PDA which is not initialized 
   yet. */
static void __init clear_bss(void)
{
	memset(__bss_start, 0,
	       (unsigned long) __bss_stop - (unsigned long) __bss_start);
}
#endif

static void __init copy_bootdata(char *real_mode_data)
{
#ifndef CONFIG_XEN
	char * command_line;

	memcpy(&boot_params, real_mode_data, sizeof boot_params);
	if (boot_params.hdr.cmd_line_ptr) {
		command_line = __va(boot_params.hdr.cmd_line_ptr);
		memcpy(boot_command_line, command_line, COMMAND_LINE_SIZE);
	}
#else
	int max_cmdline;
	
	if ((max_cmdline = MAX_GUEST_CMDLINE) > COMMAND_LINE_SIZE)
		max_cmdline = COMMAND_LINE_SIZE;
	memcpy(boot_command_line, xen_start_info->cmd_line, max_cmdline);
	boot_command_line[max_cmdline-1] = '\0';
#endif
}

#include <xen/interface/memory.h>
unsigned long *machine_to_phys_mapping;
EXPORT_SYMBOL(machine_to_phys_mapping);
unsigned int machine_to_phys_order;
EXPORT_SYMBOL(machine_to_phys_order);

#define EBDA_ADDR_POINTER 0x40E

static __init void reserve_ebda(void)
{
#ifndef CONFIG_XEN
	unsigned ebda_addr, ebda_size;

	/*
	 * there is a real-mode segmented pointer pointing to the
	 * 4K EBDA area at 0x40E
	 */
	ebda_addr = *(unsigned short *)__va(EBDA_ADDR_POINTER);
	ebda_addr <<= 4;

	if (!ebda_addr)
		return;

	ebda_size = *(unsigned short *)__va(ebda_addr);

	/* Round EBDA up to pages */
	if (ebda_size == 0)
		ebda_size = 1;
	ebda_size <<= 10;
	ebda_size = round_up(ebda_size + (ebda_addr & ~PAGE_MASK), PAGE_SIZE);
	if (ebda_size > 64*1024)
		ebda_size = 64*1024;

	reserve_early(ebda_addr, ebda_addr + ebda_size, "EBDA");
#endif
}

void __init x86_64_start_kernel(char * real_mode_data)
{
	struct xen_machphys_mapping mapping;
	unsigned long machine_to_phys_nr_ents;
	int i;

	xen_setup_features();

	xen_start_info = (struct start_info *)real_mode_data;
	if (!xen_feature(XENFEAT_auto_translated_physmap))
		phys_to_machine_mapping =
			(unsigned long *)xen_start_info->mfn_list;
	start_pfn = (__pa(xen_start_info->pt_base) >> PAGE_SHIFT) +
		xen_start_info->nr_pt_frames;

	machine_to_phys_mapping = (unsigned long *)MACH2PHYS_VIRT_START;
	machine_to_phys_nr_ents = MACH2PHYS_NR_ENTRIES;
	if (HYPERVISOR_memory_op(XENMEM_machphys_mapping, &mapping) == 0) {
		machine_to_phys_mapping = (unsigned long *)mapping.v_start;
		machine_to_phys_nr_ents = mapping.max_mfn + 1;
	}
	while ((1UL << machine_to_phys_order) < machine_to_phys_nr_ents )
		machine_to_phys_order++;

#ifndef CONFIG_XEN
	/* clear bss before set_intr_gate with early_idt_handler */
	clear_bss();

	/* Make NULL pointers segfault */
	zap_identity_mappings();

	/* Cleanup the over mapped high alias */
	cleanup_highmap();

	for (i = 0; i < IDT_ENTRIES; i++) {
#ifdef CONFIG_EARLY_PRINTK
		set_intr_gate(i, &early_idt_handlers[i]);
#else
		set_intr_gate(i, early_idt_handler);
#endif
	}
	load_idt((const struct desc_ptr *)&idt_descr);
#endif

	early_printk("Kernel alive\n");

 	for (i = 0; i < NR_CPUS; i++)
 		cpu_pda(i) = &boot_cpu_pda[i];

	pda_init(0);
	copy_bootdata(__va(real_mode_data));

	reserve_early(__pa_symbol(&_text), __pa_symbol(&_end), "TEXT DATA BSS");

	reserve_early(round_up(__pa_symbol(&_end), PAGE_SIZE),
		      start_pfn << PAGE_SHIFT, "Xen provided");

	reserve_ebda();

	/*
	 * At this point everything still needed from the boot loader
	 * or BIOS or kernel text should be early reserved or marked not
	 * RAM in e820. All other memory is free game.
	 */

	start_kernel();
}
