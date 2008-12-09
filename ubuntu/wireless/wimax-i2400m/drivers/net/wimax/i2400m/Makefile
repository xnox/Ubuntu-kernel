# @lket@ignore-start -- We are NOT in the LK source

# Uncomment to use old FW upload method
#I2400M_FW=-1.1
ifdef I2400M_FW
EXTRA_CFLAGS += -DI2400M_FW
endif

EXTRA_CFLAGS += -g -I$(src)/../../../../include
EXTRA_LDFLAGS += -g
include $(src)/../../../../.tmp.config.mk
# @lket@ignore-end

ifdef CONFIG_WIMAX_DEBUG
EXTRA_CFLAGS += -DD_MASTER_CONFIG=10
else
EXTRA_CFLAGS += -DD_MASTER_CONFIG=0
endif

obj-$(CONFIG_WIMAX_I2400M) += i2400m.o
obj-$(CONFIG_WIMAX_I2400M_USB) += i2400m-usb.o

i2400m-objs :=		\
	control.o	\
	driver.o	\
	fw$(I2400M_FW).o\
	op-rfkill.o	\
	sysfs.o		\
	netdev.o	\
	tx.o		\
	rx.o

# Uncoment next line to use the old fw (v1.1) upload style

i2400m-usb-objs :=		\
	usb-fw$(I2400M_FW).o	\
	usb-notif.o		\
	usb-tx.o		\
	usb-rx.o		\
	usb.o			\

i2400m-usb-objs +=	usb-compat.o		# @lket@ignore-line
