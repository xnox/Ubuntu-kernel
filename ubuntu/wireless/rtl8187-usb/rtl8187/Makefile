#EXTRA_CFLAGS += -DCONFIG_IEEE80211_NOWEP=y
#EXTRA_CFLAGS += -DCONFIG_RTL8180_IOMAP
EXTRA_CFLAGS += -I$(TOPDIR)/drivers/net/wireless
EXTRA_CFLAGS += -std=gnu89
EXTRA_CFLAGS += -O2
EXTRA_CFLAGS += -mhard-float -DCONFIG_FORCE_HARD_FLOAT=y 
#EXTRA_CFLAGS += -DJUST_FOR_87SEMESH -D_RTL8187_EXT_PATCH_
EXTRA_CFLAGS += -DCONFIG_RTL8180_PM
EXTRA_CFLAGS += -DJACKSON_NEW_8187 -DJACKSON_NEW_RX
EXTRA_CFLAGS += -DTHOMAS_BEACON -DTHOMAS_TASKLET -DTHOMAS_SKB -DTHOMAS_TURBO
EXTRA_CFLAGS += -DJOHN_IOCTL
EXTRA_CFLAGS += -DLED
EXTRA_CFLAGS += -DPOLLING_METHOD_FOR_RADIO
#CFLAGS += -DJOHN_HWSEC -DJOHN_TKIP
#CFLAGS += -DJOHN_DUMP_TX
#ieee80211_crypt-r8180 := ieee80211_crypt-r8180.o

CC = gcc

ifneq ($(shell uname -r|cut -d. -f1,2), 2.4)

r8187-objs := r8187_core.o r8180_93cx6.o r8180_wx.o r8180_rtl8225.o r8180_rtl8225z2.o r8180_pm.o r8180_dm.o r8187_led.o

#ieee80211_crpt_wep-r8180-objs := ieee80211_crypt_wep-r8180.o

obj-m := r8187.o

KVER  := $(shell uname -r)
KSRC := /lib/modules/$(KVER)/build
INSTALL_PREFIX :=
all: modules

modules:
	$(MAKE) -C $(KSRC) M=$(PWD) CC=$(CC) modules

MODDESTDIR := /lib/modules/$(KVER)/kernel/drivers/net/wireless
#MOD_ALGDESTDIR := /lib/modules/$(KVER)/kernel/drivers/net/wireless/alg
install: modules
	mkdir -p $(MODDESTDIR)
#	mkdir -p $(MOD_ALGDESTDIR)
	install -p -m 644 r8187.ko  $(MODDESTDIR)
#	install -p -m 644 ieee80211_crypt_wep-r8180.ko $(MOD_ALGDESTDIR)
else
#WARN := -W -Wall -Wstrict-prototypes -Wmissing-prototypes
WARN := -W
INCLUDE := -isystem /lib/modules/`uname -r`/build/include
CFLAGS := -O2 -DMODULE -D__KERNEL__ ${WARN} ${INCLUDE}
ifdef CONFIG_SMP
CFLAGS += -D__SMP__ -DSMP
endif
OBJS := ${patsubst %.c, %.o, ${wildcard *.c}}
                                                                                
all:r8187.o
                                                                                
r8187.o:${OBJS}
	$(LD) -r $^ -o $@
endif

.PHONY:clean
clean:
	rm -fr *.mod.c *.mod *.o .*.cmd *.ko *~
