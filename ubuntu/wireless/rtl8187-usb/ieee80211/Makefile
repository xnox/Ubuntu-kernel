EXTRA_CFLAGS += -I$(TOPDIR)/drivers/net/wireless
EXTRA_CFLAGS += -O2 
EXTRA_CFLAGS += -DJACKSON_NEW_8187 -DJACKSON_NEW_RX
#CFLAGS += -DJOHN_HWSEC -DJOHN_TKIP -DJOHN_CCMP
EXTRA_CFLAGS += -DTHOMAS_TURBO
EXTRA_CFLAGS += -DPOLLING_METHOD_FOR_RADIO
#CFLAGS += -DJOHN_DUMP
#EXTRA_CFLAGS += -DJUST_FOR_87SEMESH -D_RTL8187_EXT_PATCH_
#EXTRA_CFLAGS += -D_RTL8187_EXT_PATCH_
CC = gcc

ifneq ($(shell uname -r|cut -d. -f1,2), 2.4)

ieee80211-rtl-objs := ieee80211_softmac.o \
                      ieee80211_rx.o \
		      ieee80211_tx.o \
		      ieee80211_wx.o \
		      ieee80211_module.o \
		      ieee80211_softmac_wx.o

ieee80211_crypt-rtl-objs := ieee80211_crypt.o
ieee80211_crypt_tkip-rtl-objs := ieee80211_crypt_tkip.o
ieee80211_crypt_ccmp-rtl-objs := ieee80211_crypt_ccmp.o
ieee80211_crypt_wep-rtl-objs := ieee80211_crypt_wep.o

obj-m +=ieee80211-rtl.o
obj-m +=ieee80211_crypt-rtl.o
obj-m +=ieee80211_crypt_wep-rtl.o
obj-m +=ieee80211_crypt_tkip-rtl.o
obj-m +=ieee80211_crypt_ccmp-rtl.o

KVER  := $(shell uname -r)
KSRC := /lib/modules/$(KVER)/build
INSTALL_PREFIX :=

all: modules

#clean:
#	rm -f *.mod.c *.mod *.o .*.cmd *.ko 
#	rm -rf $(PWD)/tmp

modules:
#	$(MAKE) -C $(KSRC) SUBDIRS=$(PWD) MODVERDIR=$(PWD) modules

	$(MAKE) -C $(KSRC) M=$(PWD) CC=$(CC) modules

else

#WARN := -W -Wall -Wstrict-prototypes -Wmissing-prototypes
WARN := -W
INCLUDE := -isystem /lib/modules/`uname -r`/build/include
CFLAGS := -O2 -DMODULE -D__KERNEL__ -DEXPORT_SYMTAB -D__NO_VERSION__ ${WARN} ${INCLUDE}
ifdef CONFIG_SMP
CFLAGS += -D__SMP__ -DSMP
endif

OBJS := ${patsubst %.c, %.o, ${wildcard *.c}}

all:${OBJS} ieee80211_crypt-rtl.o michael_mic-rtl.o aes-rtl.o ieee80211_crypt_wep-rtl.o ieee80211_crypt_tkip-rtl.o ieee80211_crypt_ccmp-rtl.o crypto-rtl.o ieee80211-rtl.o

ieee80211_crypt-rtl.o: ieee80211_crypt.o
	mv $^ $@	

michael_mic-rtl.o: michael_mic.o
	mv $^ $@

aes-rtl.o: aes.o
	mv $^ $@

ieee80211_crypt_wep-rtl.o: ieee80211_crypt_wep.o
	mv $^ $@

ieee80211_crypt_tkip-rtl.o: ieee80211_crypt_tkip.o
	mv $^ $@

ieee80211_crypt_ccmp-rtl.o: ieee80211_crypt_ccmp.o
	mv $^ $@

crypto-rtl.o: arc4.o api.o autoload.o cipher.o compress.o digest.o scatterwalk.o proc.o
	$(LD) -r $^ -o $@

ieee80211-rtl.o: ieee80211_rx.o ieee80211_tx.o ieee80211_wx.o ieee80211_module.o ieee80211_softmac_wx.o ieee80211_softmac.o
	$(LD) -r $^ -o $@
endif

.PHONY: clean
clean:
	rm -fr *.mod.c *.mod *.o .*.cmd *.mod.* *.ko *.o *~
#	rm -rf $(PWD)/tmp
