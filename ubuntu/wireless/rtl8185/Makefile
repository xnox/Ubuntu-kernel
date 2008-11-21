EXTRA_CFLAGS += -I$(TOPDIR)/drivers/net/wireless
EXTRA_CFLAGS += -std=gnu89
EXTRA_CFLAGS += -O2
CC            = gcc
EXTRA_CFLAGS += -DTHOMAS_TURBO

#added for EeePC testing
EXTRA_CFLAGS += -DENABLE_IPS
EXTRA_CFLAGS += -DSW_ANTE
EXTRA_CFLAGS += -DTX_TRACK
EXTRA_CFLAGS += -DHIGH_POWER
EXTRA_CFLAGS += -DSW_DIG
EXTRA_CFLAGS += -DRATE_ADAPT
EXTRA_CFLAGS += -DCONFIG_RTL8180_PM

#+YJ,080626
EXTRA_CFLAGS += -DENABLE_DOT11D

#enable it for legacy power save, disable it for leisure  power save
EXTRA_CFLAGS += -DENABLE_LPS  


r8180-objs := r8180_core.o r8180_sa2400.o r8180_93cx6.o r8180_wx.o r8180_max2820.o r8180_gct.o r8180_rtl8225.o r8180_rtl8255.o r8180_rtl8225z2.o r8185b_init.o r8180_dm.o r8180_pm.o


obj-m += r8180.o
