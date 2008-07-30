# @lket@ignore-start
EXTRA_CFLAGS += -g -I$(src)/../../../include
EXTRA_LDFLAGS += -g
include $(src)/../../../.tmp.config.mk
# @lket@ignore-end

ifdef CONFIG_WIMAX_DEBUG
EXTRA_CFLAGS += -DD_MASTER_CONFIG=10
else
EXTRA_CFLAGS += -DD_MASTER_CONFIG=0
endif

obj-$(CONFIG_WIMAX_I2400M)	+= i2400m/
obj-$(CONFIG_WIMAX)		+= wimax.o

wimax-objs :=		\
	id-table.o	\
	op-close.o	\
	op-msg.o	\
	op-open.o	\
	op-rfkill.o	\
	stack.o		\
	sysfs.o
