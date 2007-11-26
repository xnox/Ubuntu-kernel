# -*- makefile -*-

PERFOBJS_	:= _fault.o _dev.o _misc.o _mmu.o init.o emu.o mmu.o \
		   mmu_fb.o mmu_io.o mmu_tracker.o skiplist.o mtable.o \
		   fault.o context.o ptaccess.o misc.o _traps.o hook.o \
		   _actions.o

mol-objs		:= $(PERFOBJS_) _performance.o
obj-m			:= mol.o

targets := reloc_table.h asm_offsets.h _performance.c

PERFOBJS	= $(addprefix $(obj)/, $(PERFOBJS_))
MOL_ASMFLAGS	= $(CPPFLAGS) $(ASMFLAGS) $(INCLUDES) -D__ASSEMBLY__ -I$(src)
RM		= rm -f

$(obj)/hook.o: 		$(src)/reloc_table.h
$(obj)/_traps.o:	$(src)/asm_offsets.h $(src)/traps.S $(srctree)/$(src)/*.S

$(src)/reloc_table.h: $(obj)/_traps.o
	@$(RM) $@ $@.tmp ; echo "/* WARNING! DO NOT EDIT! AUTOMATICALLY GENERATED! */" > $@.tmp
	@$(srctree)/$(src)/relbuild.pl $< $(src)/traps.S >> $@.tmp
	@$(STRIP) -S -x $<
	@mv $@.tmp $@

$(obj)/_%.o: $(src)/%.S
	@echo "  AS [x]   $@"
	@$(RM) $@ $@.s
	@$(CPP) $(MOL_ASMFLAGS) $< | m4 > $@.m4
	@ASFILTER="| $(src)/asfilter" test -x $(src)/asfilter || ASFILTER="" ; cat $@.m4 $$ASFILTER > $@.s
	@$(AS) $@.s $(AS_FLAGS) -o $@
	@$(RM) $@.s $@.m4


$(src)/asm_offsets.h:	$(src)/archinclude.h $(src)/kernel_vars.h $(src)/mac_registers.h
$(src)/asm_offsets.h:	$(src)/asm_offsets.c $(src)/asm_offsets.inc
	@$(RM) $(src)/tmp-offsets.c $@ ; cat $^ > $(src)/tmp-offsets.c
	@$(CC) $(CPPFLAGS) $(CFLAGS) -I$(srctree)/$(src) -Wall -S $(src)/tmp-offsets.c -o $(src)/tmp-offsets.s
	@echo "/* WARNING! Automatically generated from 'shared/asm_offsets.c' - DO NOT EDIT! */" > $@
	@grep '^#' $(src)/tmp-offsets.s >> $@
	@$(RM) $(src)/tmp-offsets.*


$(src)/_performance.c: $(PERFOBJS)
	@$(RM) $@ $@.tmp; echo "/* WARNING! DO NOT EDIT! AUTOMATICALLY GENERATED! */" > $@.tmp
	@echo "#include \"performance.h\"" >> $@.tmp
	@$(NM) $(PERFOBJS) | awk -- '/gPerf__/ { print "unsigned long "$$2";" }' >> $@.tmp
	@echo "perf_info_t g_perf_info_table[] = {" >> $@.tmp
	@$(NM) $(PERFOBJS) | awk -- '/gPerf__/ { print "  { \""$$2"\",&"$$2"}," }' >> $@.tmp
	@echo "  {0,0} };" >> $@.tmp
	@cat $@.tmp | sed s/_gPerf/gPerf/g > $@
	@$(RM) $@.tmp

