OBJS := hal.o iw_ndis.o loader.o rtl.o ndis.o ntoskernel.o \
	ntoskernel_io.o pe_linker.o proc.o usb.o wrapper.o pnp.o \
	wrapndis.o wrapmem.o crt.o
EXTRA_CFLAGS += -DINIT_WORK_NAR

ifeq ($(CONFIG_NDISWRAPPER_WQ),y)
OBJS += workqueue.o
EXTRA_CFLAGS += -DWRAP_WQ
endif

ifeq ($(CONFIG_X86_64),y)
OBJS += win2lin_stubs.o
else
OBJS += divdi3.o
endif

obj-m := ndiswrapper.o
ndiswrapper-objs := $(OBJS)


targets :=	win2lin_stubs.h		\
		hal_exports.h 		\
		rtl_exports.h	 	\
		crt_exports.h		\
		ndis_exports.h 		\
		ntoskernel_exports.h 	\
		usb_exports.h 		\
		wrapper_exports.h	\
		ntoskernel_io_exports.h


$(obj)/win2lin_stubs.o: $(obj)/win2lin_stubs.h $(src)/win2lin_stubs.S
$(obj)/win2lin_stubs.h: $(src)/hal.c $(src)/rtl.c $(src)/ndis.c $(src)/crt.c \
		$(src)/ntoskernel.c $(src)/ntoskernel_io.c $(src)/pnp.c \
		$(src)/wrapndis.c $(src)/usb.c
	@for file in $^; do \
		echo; \
		echo "# automatically generated from $$file"; \
		sed -n \
			-e 's/.*WIN_FUNC(\([^\,]\+\) *\, *\([0-9]\+\)).*/\
			   win2lin(\1, \2)/p'   \
			-e 's/.*WIN_FUNC_PTR(\([^\,]\+\) *\, *\([0-9]\+\)).*/\
			   win2lin(\1, \2)/p'   \
			$$file | sed -e 's/[ \t      ]\+//' | sort -u; \
	done > $@


$(obj)/hal.o: $(obj)/hal_exports.h
$(obj)/hal_exports.h: $(src)/hal.c

$(obj)/rtl.o: $(obj)/rtl_exports.h
$(obj)/rtl_exports.h: $(src)/rtl.c

$(obj)/crt.o: $(obj)/crt_exports.h
$(obj)/crt_exports.h: $(src)/crt.c

$(obj)/ndis.o: $(obj)/ndis_exports.h
$(obj)/ndis_exports.h: $(src)/ndis.c

$(obj)/ntoskernel.o: $(obj)/ntoskernel_exports.h
$(obj)/ntoskernel_exports.h: $(src)/ntoskernel.c

$(obj)/usb.o: $(obj)/usb_exports.h
$(obj)/usb_exports.h: $(src)/usb.c

$(obj)/wrapper.o: $(obj)/wrapper_exports.h
$(obj)/wrapper_exports.h: $(src)/wrapper.c

$(obj)/ntoskernel_io.o: $(obj)/ntoskernel_io_exports.h
$(obj)/ntoskernel_io_exports.h: $(src)/ntoskernel_io.c


quiet_cmd_exports = EXPORTS $@
      cmd_exports = \
        ( \
                echo "/* automatically generated from $< */"; \
                echo '\#ifdef CONFIG_X86_64'; \
                sed -n \
                        -e 's/.*WIN_FUNC(\([^\,]\+\) *\, *\([0-9]\+\)).*/WIN_FUNC_DECL(\1, \2)/p' \
                        -e 's/.*WIN_FUNC_PTR(\([^\,]\+\) *\, *\([0-9]\+\)).*/WIN_FUNC_DECL(\1, \2)/p' \
                   $< | sed -e 's/[ \t  ]\+//' | sort -u; \
                echo '\#endif'; \
                echo "struct wrap_export $(shell basename $< .c)_exports[] = {";\
                sed -n \
                        -e 's/.*WIN_FUNC(_win_\([^\,]\+\) *\, *\([0-9]\+\)).*/WIN_WIN_SYMBOL(\1,\2),/p' \
                        -e 's/.*WIN_FUNC(\([^\,]\+\) *\, *\([0-9]\+\)).*/WIN_SYMBOL(\1,\2),/p' \
                        -e 's/.*WIN_SYMBOL_MAP(\("[^"]\+"\)[ ,\]\+\([^)]\+\)).*/{\1,(generic_func)\2},/p'  \
                           $< | sed -e 's/[ \t  ]*/   /' | sort -u; \
                echo '   {NULL, NULL}'; \
                echo '};'; \
        ) > $@

$(obj)/%_exports.h: $(src)/%.c
	$(call if_changed,exports)
