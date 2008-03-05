# File: Makefile
#
# (c) Copyright © 2003-2006, Marvell International Ltd. 
#
# This software file (the "File") is distributed by Marvell International 
# Ltd. under the terms of the GNU General Public License Version 2, June 1991 
# (the "License").  You may use, redistribute and/or modify this File in 
# accordance with the terms and conditions of the License, a copy of which 
# is available along with the File in the gpl.txt file or by writing to 
# the Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 
# 02111-1307 or on the worldwide web at http://www.gnu.org/licenses/gpl.txt.
#
# THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE 
# IMPLIED WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE 
# ARE EXPRESSLY DISCLAIMED.  The License provides additional details about 
# this warranty disclaimer.

#CC=		$(CROSS)gcc
#LD=		$(CROSS)ld
#BACKUP=		/root/backup
#YMD=		`date +%Y%m%d%H%M`

#############################################################################
# Configuration Options
#############################################################################

# Debug Option
# DEBUG LEVEL n/1/2:
# n: NO DEBUG
# 1: Only PRINTM(MSG,...), PRINTM(FATAL,...), ...
# 2: All PRINTM()
CONFIG_DEBUG=0

# Proc in /proc/net/wlan
CONFIG_PROC_DEBUG=y

# Re-association in driver
CONFIG_REASSOCIATION=y

# Manufacturing firmware support
CONFIG_MFG_CMD_SUPPORT=y

# Big-endian platform
CONFIG_BIG_ENDIAN=n

#############################################################################
# Select Platform Tools
#############################################################################

MODEXT = ko
EXTRA_CFLAGS += -I$(src)/wlan

LD += -S
KERNELVERSION_X86 := 	$(shell uname -r)


#############################################################################
# Compiler Flags
#############################################################################

	#EXTRA_CFLAGS += -I$(KERNELDIR)/include

	EXTRA_CFLAGS += -DFPNUM='"38"'

ifeq ($(CONFIG_DEBUG),1)
	EXTRA_CFLAGS += -DDEBUG_LEVEL1
endif

ifeq ($(CONFIG_DEBUG),2)
	EXTRA_CFLAGS += -DDEBUG_LEVEL1
	EXTRA_CFLAGS += -DDEBUG_LEVEL2
	DBG=	-dbg
endif

ifeq ($(CONFIG_PROC_DEBUG),y)
	EXTRA_CFLAGS += -DPROC_DEBUG
	export CONFIG_PROC_DEBUG
endif

ifeq ($(CONFIG_REASSOCIATION),y)
	EXTRA_CFLAGS += -DREASSOCIATION
endif

ifeq ($(CONFIG_MFG_CMD_SUPPORT),y)
	EXTRA_CFLAGS += -DMFG_CMD_SUPPORT
endif

ifeq ($(CONFIG_BIG_ENDIAN),y)
	EXTRA_CFLAGS += -DBIG_ENDIAN
endif

#############################################################################
# Make Targets
#############################################################################

ifneq ($(KERNELRELEASE),)

WLANOBJS =	wlan/wlan_main.o wlan/wlan_init.o wlan/wlan_wext.o \
		wlan/wlan_rx.o wlan/wlan_tx.o \
		wlan/wlan_cmd.o wlan/wlan_cmdresp.o \
		wlan/wlan_scan.o wlan/wlan_join.o

WLANOBJS += wlan/wlan_wmm.o
WLANOBJS += wlan/wlan_11d.o
WLANOBJS += wlan/wlan_11h.o
WLANOBJS += wlan/wlan_meas.o

ifdef CONFIG_PROC_FS
WLANOBJS += wlan/wlan_proc.o
ifeq ($(CONFIG_PROC_DEBUG), y)
WLANOBJS += wlan/wlan_debug.o
endif
endif

WLANOBJS += wlan/wlan_sdio.o
obj-m := 8688_a2.o
8688_a2-objs := $(WLANOBJS)

# Otherwise we were called directly from the command line; invoke the kernel build system.
else

default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

endif

###############################################################

export		CC LD EXTRA_CFLAGS KERNELDIR

.PHONY: app/wlanconfig wlan clean distclean


	@echo "Finished Making Marvell Wlan Linux Driver"

wlan app/wlanconfig:
	$(MAKE) -C $@

echo:
	@echo "Please make sure the following are done before building:"
	@echo "	1. Modify wlan/wlan_drv.h to set correct version number"
	@echo "	2. Update the README"

build:		echo default

	@if [ ! -d $(BINDIR) ]; then \
		mkdir $(BINDIR); \
	fi 
	cp -f 8688_a2.$(MODEXT) $(BINDIR)/sd8688$(DBG).$(MODEXT)

	cp -f README $(BINDIR)
	$(MAKE) -C app/wlanconfig $@ INSTALLDIR=$(BINDIR)
	cp -r config $(BINDIR)

clean:
	-find . -name "*.o" -exec rm {} \;
	-find . -name "*.ko" -exec rm {} \;
	-find . -name ".*.cmd" -exec rm {} \;
	-find . -name "*.mod.c" -exec rm {} \;
	-find . -name "Module.symvers" -exec rm {} \;
	-rm -rf .tmp_versions
	$(MAKE) -C app/wlanconfig $@

install: default

	cp -f io/sdio/$(PLATFORM)/sdio.$(MODEXT) $(INSTALLDIR)
	$(MAKE) -C app/wlanconfig $@ INSTALLDIR=$(INSTALLDIR)
	cp -f 8688_a2.$(MODEXT) $(INSTALLDIR)/sd8688$(DBG).$(MODEXT)
	echo "sd8688 Driver Installed"

distclean:
	-find . -name "*.o" -exec rm {} \;
	-find . -name "*.orig" -exec rm {} \;
	-find . -name "*.swp" -exec rm {} \;
	-find . -name "*.*~" -exec rm {} \;
	-find . -name "*~" -exec rm {} \;
	-find . -name "*.d" -exec rm {} \;
	-find . -name "*.a" -exec rm {} \;
	-find . -name "tags" -exec rm {} \;
	-find . -name ".*" -exec rm -rf 2> /dev/null \;
	-find . -name "*.ko" -exec rm {} \;
	-find . -name ".*.cmd" -exec rm {} \;
	-find . -name "*.mod.c" -exec rm {} \;
	-rm -rf .tmp_versions
	$(MAKE) -C app/wlanconfig $@

# End of file
