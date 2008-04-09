ifeq ($(src),)
#INSTALL_MOD_PATH	:= $(shell pwd)/lib
INSTALL_MOD_DIR		:= updates
KDIR	?= /lib/modules/$(shell uname -r)/build
src	:= $(shell pwd)

export src INSTALL_MOD_PATH INSTALL_MOD_DIR

all:
	make -C $(KDIR) M=$(src)

clean:
	make -C $(KDIR) M=$(src) clean

install:	all
	make -C $(KDIR) M=$(src) modules_install
else
obj-y	+= wireless/
obj-y	+= mac80211/
obj-y	+= rt2x00/
endif

