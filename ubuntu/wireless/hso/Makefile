#
# Makefile for the Linux WWAN device drivers.
#
# NOTE: This make file can serve as both an external Makefile (launched
#       directly by the user), or as the sub-dir Makefile used by the kernel
# 	build system.

# If CONFIG_HSO isn't set, we'll assume the user has never configured
# their kernel to include this module and set up some defaults.
#
# NOTE: If you have previously added the HSO project to your kernel 
# 	and configured it for inclusion, these settings will be 
#	overridden by your kernel configuration.
EXTERNAL_BUILD=y
CONFIG_HSO_DEBUG=n


ifeq ($(CONFIG_HSO_DEBUG),y)
	EXTRA_CFLAGS += -g -Wa,-adhlms=$@.lst
endif

ifdef KBUILD_EXTMOD
	EXTRA_CFLAGS += -I$(KBUILD_EXTMOD)
endif

ifdef CONFIG_HSO_DEBUG
EXTRA_CFLAGS += -DCONFIG_HSO_DEBUG=$(CONFIG_HSO_DEBUG)
endif

obj-m += hso.o



