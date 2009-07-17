ifneq ($(KERNELRELEASE),) 
	obj-m := cmpc_pm.o 
else 
	KERNELDIR := /lib/modules/$(shell uname -r)/build 
	PWD := $(shell pwd) 
default: 
	$(MAKE) -C $(KERNELDIR) SUBDIRS=$(PWD) modules 
clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions
endif 
