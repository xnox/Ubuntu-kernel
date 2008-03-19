# @lket@ignore-start -- We are NOT in the LK source

USE_LEGACY_IOCTL := 1

ifeq ($(USE_LEGACY_IOCTL),0)
USE_LEGACY_IOCTL_DEFINE :=
USE_LEGACY_IOCTL_MODULES :=
else
USE_LEGACY_IOCTL_DEFINE := -D__USE_LEGACY_IOCTL
USE_LEGACY_IOCTL_MODULES := besor.o
endif


EXTRA_CFLAGS += -g -I$(src)/../../../../include $(USE_LEGACY_IOCTL_DEFINE)
EXTRA_LDFLAGS += -g
include $(src)/../../../../.tmp.config.mk
# @lket@ignore-end

obj-$(CONFIG_WIMAX_I2400M) += i2400m.o

i2400m-objs :=		\
	control.o	\
	driver.o	\
	fw.o		\
	sysfs.o		\
	netdev.o	\
	notif.o		\
	tx.o		\
	rx.o		\
	$(USE_LEGACY_IOCTL_MODULES)

i2400m-objs +=	compat.o		# @lket@ignore-line
