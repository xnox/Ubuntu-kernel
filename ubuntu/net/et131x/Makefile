#/******************************************************************************
#
#                Copyright (c) 2002-2004 Agere Systems
#                         All Rights Reserved.
#
#          THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AGERE SYSTEMS
#
#                The copyright notice above does not evidence any 
#               actual or intended publication of such source code.
#
# ******************************************************************************
#
#   MODULE NAME       : Makefile
#
#   ABSTRACT          : The "Master" makefile for building the ET131x driver.
#
#   ENVIRONMENT       : Kernel Mode
#
#   DEVELOPER NOTES   :
#
#   REUSE INSTRUCTIONS: 
#
#   HISTORICAL DATA   : 
#         CHANGED BY     DATE        COMMENTS
#         ==========     ==========  ===========================================
#         Victor Soriano 03/21/2005  Initial Creation
#
#
# ******************************************************************************
# *  VERSION CONTROL INFORMATION
# ******************************************************************************
#
#      $Archive: $
#     $Workfile: $
#         $Date: 2006/01/20 21:29:45 $
#     $Revision: 1.12 $
#      $Modtime: $
#       $Author: vjs $
#   $NoKeywords: $
#
# *****************************************************************************/


# *****************************************************************************/
#  Definintions common to all builds
# *****************************************************************************/

KERNEL_VER := $(shell uname -r)
MODULE_DIR := /lib/modules/$(KERNEL_VER)
KERNEL_DIR := $(MODULE_DIR)/build
KERNEL_24  := $(if $(wildcard $(KERNEL_DIR)/Rules.make),1,0)
KERNEL_64B := $(shell uname -p)

ifeq ($(KERNEL_24),1)
KERNEL_INC := $(KERNEL_DIR)/include
else
KERNEL_SRC := /lib/modules/$(KERNEL_VER)/source
KSRC       := $(if $(wildcard $(KERNEL_SRC)),1,0)
ifeq ($(KSRC),1)
KERNEL_INC := $(KERNEL_SRC)/include
else
KERNEL_INC := $(KERNEL_DIR)/include
endif
endif


MODNAME     = et131x
MODNAME_EXT = et131x.o

OBJS    = et131x_main.o \
          et131x_initpci.o \
          et131x_isr.o \
          et131x_netdev.o \
          et131x_supp.o \
          et131x_config.o \
          et131x_debug.o

OBJS   += ET1310_jagcore.o \
          ET1310_tx.o \
          ET1310_rx.o \
          ET1310_phy.o \
          ET1310_mac.o \
          ET1310_pm.o \
          ET1310_eeprom.o




# *****************************************************************************/
#  Using the KERNEL_24 variable defined above, test to see if we're building
#  in a 2.4.x or 2.6.x environment and use the appropriate build directives.
# *****************************************************************************/
ifeq ($(KERNEL_24),1)


CC = gcc
LD = ld

CFLAGS = -D__KERNEL__ -DMODULE -DET131X_DBG -I$(KERNEL_INC) -I$(KERNEL_INC)/linux -I$(PWD) -O -Wall

ifeq ($(KERNEL_64B),x86_64)
CFLAGS += -mcmodel=kernel
endif

all: $(MODNAME_EXT)

$(MODNAME_EXT): $(OBJS)
	$(LD) -r $^ -o $@

clean:
	rm -f *.o *.d *~ core

modules_install:
	cp $(MODNAME).o $(MODULE_DIR)/kernel/drivers/net
	/sbin/depmod -a -q

else


obj-m        := $(MODNAME_EXT)
$(MODNAME)-y += $(OBJS)

EXTRA_CFLAGS := -I$(KERNEL_INC)/linux -DET131X_DBG

# The following build rules were defined as such in order to properly support
# initial versions of the 2.6 kernel where the kbuild environment had not yet
# stabilized (i.e. Suse 9.1, 2.6.4-52). The two changes which needed to be made
# were:
#    
#    1. The make command defines the SUBDIRS variable to be the location of the
#       module code, instead of using the newer M variable. In Suse 9.1, using
#       M would for some reason cause a rebuild the entire modules tree without
#       building our module.
#   
#    2. A seperate 'clean' rule had to be defined which manually cleans our
#       module directory. On newer 2.6 kernels, calling into the kernel dir's
#       Makefile would clean our module directory properly, but this doesn't
#       happen in older kernels (again, Suse 9.1, 2.6.4-52). 


modules modules_install:
	#@$(MAKE) -C $(KERNEL_DIR) M=$(CURDIR) $@
	@$(MAKE) -C $(KERNEL_DIR) SUBDIRS=$(CURDIR) $@

clean:
	rm -f *.o *.ko *.d *~ .*.d .*.cmd .*.swp *.mod*
	rm -rf .tmp_versions


endif

