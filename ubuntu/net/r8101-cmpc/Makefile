################################################################################
# 
# Copyright(c) Realtek Semiconductor Corp. All rights reserved.
# 
# This program is free software; you can redistribute it and/or modify it 
# under the terms of the GNU General Public License as published by the Free 
# Software Foundation; either version 2 of the License, or (at your option) 
# any later version.
# 
# This program is distributed in the hope that it will be useful, but WITHOUT 
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or 
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for 
# more details.
# 
# You should have received a copy of the GNU General Public License along with
# this program; if not, write to the Free Software Foundation, Inc., 59 
# Temple Place - Suite 330, Boston, MA  02111-1307, USA.
# 
# The full GNU General Public License is included in this distribution in the
# file called LICENSE.
# 
################################################################################

KVER		:= $(shell uname -r)
KDIR		:= /lib/modules/$(KVER)/build
KMISC		:= /lib/modules/2.6.24-19-lpia/kernel/drivers/misc/
KEXT		:= $(shell echo $(KVER) | sed -ne 's/^2\.[567]\..*/k/p')o
KFLAG		:= 2$(shell echo $(KVER) | sed -ne 's/^2\.[4]\..*/4/p')x

ifeq ($(RTL_IOCTL),y)
EXTRA_CFLAGS	+= -DRTL_IOCTL_SUPPORT
endif

modules:
ifeq ($(KFLAG),24x)
	$(MAKE) -f Makefile_linux24x
else
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules
	strip --strip-debug r8101.$(KEXT)
endif

clean:
	rm -rf *.o *.ko *~ core* .dep* .*.d .*.cmd *.mod.c *.a *.s .*.flags .tmp_versions Module.symvers Modules.symvers rset


install:
	install -m 744 -c r8101.$(KEXT) $(KMISC)

ifneq ($(KFLAG),24x)
ifeq ($(RTL_IOCTL),y)
r8101-objs :=  r8101_n.o rtl_ioctl.o
else
r8101-objs :=  r8101_n.o
endif
obj-m += r8101.o
endif#($(KFLAG),24x)
