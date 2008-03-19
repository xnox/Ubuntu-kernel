# @lket@ignore-start
EXTRA_CFLAGS += -g -I$(src)/../../../include
EXTRA_LDFLAGS += -g
include $(src)/../../../.tmp.config.mk
# @lket@ignore-end

obj-$(CONFIG_WIMAX_I2400M)	+= i2400m/
obj-$(CONFIG_WIMAX)		+= wimax.o

wimax-objs :=		\
	id-table.o	\
	ops.o	 	\
	op-close.o	\
	op-msg.o	\
	op-open.o	\
	stack.o		\
	sysfs.o
