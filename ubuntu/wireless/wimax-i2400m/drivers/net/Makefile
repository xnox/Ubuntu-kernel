# @lket@ignore-file -- We are NOT in the LK source but modify drivers/Makefile
EXTRA_CFLAGS += -g -I$(src)/../../include
EXTRA_LDFLAGS += -g
-include $(src)/../../.tmp.config.mk

# @lket@add-start drivers/Makefile ^obj-\$(CONFIG_USB)
obj-$(CONFIG_WIMAX)		+= wimax/
# @lket@add-end
