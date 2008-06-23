#
# Makefile for USB Network drivers
#

obj-$(CONFIG_USB_NET_CDCETHER)	+= cdc_ether.o
obj-$(CONFIG_USB_NET_RNDIS_HOST)	+= rndis_host.o
obj-$(CONFIG_USB_USBNET)	+= usbnet.o

ifeq ($(CONFIG_USB_DEBUG),y)
EXTRA_CFLAGS += -DDEBUG
endif
