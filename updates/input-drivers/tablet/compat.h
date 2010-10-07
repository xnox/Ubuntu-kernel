#ifndef __COMPAT_H__
#define __COMPAT_H__

#include <linux/version.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,33))
#include <generated/autoconf.h>
#else
#include <linux/autoconf.h>
#endif

#include <linux/init.h>
#include <linux/types.h>
#include <linux/input.h>

static inline int input_abs_get_val(struct input_dev *dev, unsigned int axis)
{
	return dev->abs[axis];
}

static inline void input_abs_set_val(struct input_dev *dev,
                                    unsigned int axis, int val)
{
	dev->abs[axis] = val;
}
#endif
