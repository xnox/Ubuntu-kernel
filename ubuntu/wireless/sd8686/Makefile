# File: Makefile
#
# (c) Copyright <A9> 2003-2006, Marvell International Ltd.
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


#############################################################################
# Configuration Options
#############################################################################

# Debug Option
# DEBUG LEVEL n/1/2:
# n: NO DEBUG
# 1: Only PRINTM(MSG,...), PRINTM(FATAL,...), ...
# 2: All PRINTM()
CONFIG_DEBUG=1

# Proc in /proc/net/wlan
CONFIG_PROC_DEBUG=n

# Linux Power Management
CONFIG_ENABLE_PM=n

# Re-association in driver
CONFIG_REASSOCIATION=y

# Manufacturing firmware support
CONFIG_MFG_CMD_SUPPORT=y


#############################################################################
# Compiler Flags
#############################################################################

CFLAGS += -I$(src)/os/linux
CFLAGS += -I$(src)/wlan
CFLAGS += -I$(src)/if/if_sdio_fb
CFLAGS += -DFB
CFLAGS += -DFPNUM='"13"'

ifeq ($(CONFIG_DEBUG),1)
	CFLAGS += -DDEBUG_LEVEL1
endif

ifeq ($(CONFIG_DEBUG),2)
	CFLAGS += -DDEBUG_LEVEL1
	CFLAGS += -DDEBUG_LEVEL2
	DBG=	-dbg
endif

ifeq ($(CONFIG_PROC_DEBUG),y)
	CFLAGS += -DPROC_DEBUG
	export CONFIG_PROC_DEBUG
endif

ifeq ($(CONFIG_ENABLE_PM),y)
	CFLAGS += -DENABLE_PM
endif

ifeq ($(CONFIG_REASSOCIATION),y)
	CFLAGS += -DREASSOCIATION
endif

ifeq ($(CONFIG_MFG_CMD_SUPPORT),y)
	CFLAGS += -DMFG_CMD_SUPPORT
endif

#############################################################################

WLANOBJS =	wlan/wlan_main.o wlan/wlan_fw.o wlan/wlan_wext.o \
		wlan/wlan_rx.o wlan/wlan_tx.o \
		wlan/wlan_cmd.o wlan/wlan_cmdresp.o \
		wlan/wlan_proc.o wlan/wlan_scan.o wlan/wlan_join.o \
		wlan/wlan_wmm.o wlan/wlan_11d.o \
		if/if_sdio_fb/if_sdio.o

ifeq ($(CONFIG_PROC_DEBUG), y)
WLANOBJS += wlan/wlan_debug.o
endif

obj-m += sd8xxx.o
sd8xxx-objs := $(WLANOBJS)
