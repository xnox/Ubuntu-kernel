#
# Makefile for the tablet drivers
#

NOSTDINC_FLAGS += -include $(M)/input-drivers/tablet/compat.h

# Multipart objects.
wacom-objs	:= wacom_wac.o wacom_sys.o

obj-$(CONFIG_TABLET_USB_WACOM)	+= wacom.o
