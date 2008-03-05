#############################################################################
# Configuration Options
#############################################################################

# Debug Option
# DEBUG LEVEL n/1:
# n: NO DEBUG
# 1: PRINTM(MSG,...), PRINTM(FATAL,...), PRINTM(WARN,...) and PRINTM(INFO,...)
CONFIG_DEBUG=1

#############################################################################
# Compiler Flags
#############################################################################

EXTRA_CFLAGS += -Ibt

ifeq ($(CONFIG_DEBUG),1)
	EXTRA_CFLAGS += -DDEBUG_LEVEL1
endif

#############################################################################
# Make Targets
#############################################################################

ifneq ($(KERNELRELEASE),)

BTOBJS = bt/bt_main.o bt/bt_sdio.o bt/bt_proc.o

obj-m := bt8xxx.o
bt8xxx-objs := $(BTOBJS)


# Otherwise we were called directly from the command line; invoke the kernel build system.
else
default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
endif

# End of file;
