# makefile for ltmodem for 2.6 kernels
# 

DEVICE_FILE := /dev/ttyLTM0

MODULE_NAME := ltmodem

KERNEL_DIR := /lib/modules/`uname -r`/build
#KERNEL_DIR := /usr/src/linux-2.6.10

INSTALL_DIR := /lib/modules/`uname -r`/other

ltmodem-objs := lt_modem.o ltmdmobj.o

ltserial-objs := serial.o

obj-m += ltmodem.o ltserial.o

EXTRA_CFLAGS += -Wall
#EXTRA_CFLAGS += -Wall -DLT_USE_OLD_NAMES -DLT_USE_FIFO

EXTRA_LDFLAGS := -d

MSRC_DIR = $(shell pwd)

module:
	make -C $(KERNEL_DIR) SUBDIRS=$(MSRC_DIR) modules

$(obj)/ltmdmobj.o:
	cp $(src)/ltmdmobj.o $(obj)/ltmdmobj.o

clean:
	rm -fR lt_modem.o ltmodem.o ltserial.ko \
	serial.o ltserial.o ltserial.mod.* ltmodem.mod.o \
	ltmodem.ko ltmodem.mod.c \
	.*.cmd .tmp_versions

# mdk_install target courtesy of Bea Petrovicova
mdk_install:
	# Step 1: Creating the folder for the modem driver in the kernel.
	if [ ! -d $(INSTALL_DIR) ]; then \
		mkdir $(INSTALL_DIR); \
	fi
	# Step 2: Copying files ltmodem.ko & ltserial.ko into kernel.
	cp -v *.ko $(INSTALL_DIR)
	# Step 3: Creating device file $(DEVICE_FILE)
	# NOTE: You might need to change change owner ang \
	#       group of $(DEVICE_FILE) to math /dev/ttyS0. \
	#       Debian users set group to 'dialout'.
	if [ ! -e $(DEVICE_FILE) ]; then \
		mknod --mode=0660 $(DEVICE_FILE) c 62 64; \
	fi
	# Step 4: Adding modem configuration into /etc/modprobe.conf
	if [ -e /etc/modprobe.conf ]; then \
		if ! grep "ltserial" /etc/modprobe.conf; then \
			echo -e "\n# ltmodem configuration" \
				"\nalias char-major-62 ltserial" \
				"\nalias /dev/modem ltserial" >>/etc/modprobe.conf; \
		fi; \
	fi
	# Step 5: Adding winmodem module into /etc/modprobe.preload
	if [ -e /etc/modprobe.preload ]; then \
		if ! grep "ltserial" /etc/modprobe.preload; then \
			echo -e "\n# ltmodem configuration" \
				"\nltserial" >>/etc/modprobe.preload; \
		fi; \
	fi
	# NOTE: Installation complete. To test your modem, try:
	# depmod -a
	# modprobe ltserial
	# lsmod
