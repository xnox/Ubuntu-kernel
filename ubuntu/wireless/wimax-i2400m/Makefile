
# @lket@ignore-file
#
# We are NOT in the LK source, we are a separate module

KDIR ?= /lib/modules/$(shell uname -r)/build

EXTRA_CFLAGS += -g -I$(src)/include
EXTRA_LDFLAGS += -g-


obj-m := drivers/net/


.PHONY: all clean warning TAGS
all:
	$(MAKE) -C $(KDIR) M=$(shell pwd)
clean:
	rm -rf cscope* ncscope* Module*.symvers .tmp_* TAGS 	\
		include/config.h 				\
		drivers/net/wimax/version.h 			\
		drivers/net/wimax/bugs.h			\
		$(src)/.tmp.config.mk
	find -name \*.rej -o -name \*~ -o -name \*.orig -o -name \*.o \
		-o -name core -o -name .depend -o -name \*.o -o -name \*.cmd \
		-o -name \*.ko -o -name \*.mod.c \
	    | xargs rm -f
	find -iname \*.[chS] > cscope.files

TAGS:
	find -name \*.[chS] | etags -

CSCOPE cscope:
	find -iname \*.[chS] > cscope.files


src ?= $(shell pwd)
.PHONY: $(src)/.tmp.config.mk
-include $(src)/.tmp.config.mk
$(src)/.tmp.config.mk:: $(src)/lket-ct-config
	@$(src)/lket-ct-config $(src) $(TOPDIR)

.PHONY: config install
config:
	$(src)/lket-ct-config $(src) $(KDIR)
install:
	make -C $(KDIR) M=$(shell pwd) modules_install


