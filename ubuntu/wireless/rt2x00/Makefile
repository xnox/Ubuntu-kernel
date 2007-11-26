rt2x00lib-objs := rt2x00dev.o rt2x00mac.o rt2x00rfkill.o rt2x00firmware.o

EXTRA_CFLAGS = -DCONFIG_RT2X00_LIB_RFKILL -DCONFIG_RT2X00_LIB_FIRMWARE -I$(src)

# rt2x00debug.o -DCONFIG_RT2X00_LIB_DEBUGFS

obj-m += rt2x00lib.o rt2x00pci.o rt2x00usb.o rt2400pci.o rt2500pci.o \
	rt61pci.o rt2500usb.o rt73usb.o eeprom_93cx6.o
