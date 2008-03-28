/*
 * Intel Wireless WiMax Connection 2400m
 * Sysfs interfaces
 *
 *
 *
 * Copyright (C) 2007 Intel Corporation <linux-wimax@intel.com>
 * Inaky Perez-Gonzalez <inaky.perez-gonzalez@intel.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 *
 * FIXME: docs
 */

#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/device.h>
#include "../debug.h"
#include "i2400m.h"

/*
 * Show current inflight values
 *
 * Will print the current MAX and THRESHOLD values for the basic flow
 * control. In addition it will report how many times the TX queue needed
 * to be restarted since the last time this query was made.
 */
static
ssize_t req_inflight_show(struct req_inflight *inflight, char *buf)
{
	ssize_t result;
	unsigned long sec_elapsed = (jiffies - inflight->restart_ts) / HZ;
	unsigned long restart_count = atomic_read(&inflight->restart_count);

	result = scnprintf(buf, PAGE_SIZE, "%lu %lu %d %lu %lu %lu\n"
			   "#read: threshold max inflight_count restarts "
			   "seconds restarts/sec\n"
			   "#write: threshold max\n",
			   inflight->threshold, inflight->max,
			   atomic_read(&inflight->count),
			   restart_count, sec_elapsed,
			   sec_elapsed == 0 ? 0 : restart_count / sec_elapsed);
	inflight->restart_ts = jiffies;
	atomic_set(&inflight->restart_count, 0);
	return result;
}

static
ssize_t req_inflight_store(struct req_inflight *inflight,
			   const char *buf, size_t size)
{
	unsigned long in_threshold,
	  in_max;
	ssize_t result;

	result = sscanf(buf, "%lu %lu", &in_threshold, &in_max);
	if (result != 2)
		return -EINVAL;
	if (in_max <= in_threshold)
		return -EINVAL;
	inflight->max = in_max;
	inflight->threshold = in_threshold;
	return size;
}


#warning FIXME: remove these adaptors and just make it transparent
/*							// @lket@ignore-start
 * Glue (or function adaptors) for accesing info on sysfs
 *
 * Linux 2.6.21 changed how 'struct netdevice' does attributes (from
 * having a 'struct class_dev' to having a 'struct device'). That is
 * quite of a pain.
 *
 * So we try to abstract that here. i2400m_SHOW() and i2400m_STORE()
 * create adaptors for extracting the 'struct i2400m' from a 'struct
 * dev' and calling a function for doing a sysfs operation (as we have
 * them factorized already). i2400m_ATTR creates the attribute file
 * (CLASS_DEVICE_ATTR or DEVICE_ATTR) and i2400m_ATTR_NAME produces a
 * class_device_attr_NAME or device_attr_NAME (for group registration).
 */
#include <linux/version.h>

#define i2400m_SHOW(name, fn, param)					\
static									\
ssize_t show_##name(struct device *dev,					\
			   struct device_attribute *attr,		\
			   char *buf) {					\
	struct i2400m *i2400m = net_dev_to_i2400m(to_net_dev(dev));	\
	return fn(&i2400m->param, buf);					\
}

#define i2400m_STORE(name, fn, param)					\
static									\
ssize_t store_##name(struct device *dev,				\
			    struct device_attribute *attr,		\
			    const char *buf, size_t size) {		\
	struct i2400m *i2400m = net_dev_to_i2400m(to_net_dev(dev));	\
	return fn(&i2400m->param, buf, size);				\
}

#define i2400m_ATTR(name, perm)	\
	DEVICE_ATTR(name, perm, show_##name, store_##name)

#define i2400m_ATTR_NAME(a) (dev_attr_##a)

/* Sysfs adaptors */

i2400m_SHOW(i2400m_tx_inflight, req_inflight_show, tx_inflight);
i2400m_STORE(i2400m_tx_inflight, req_inflight_store, tx_inflight);
i2400m_ATTR(i2400m_tx_inflight, S_IRUGO | S_IWUSR);


#ifdef __USE_LEGACY_IOCTL

/*
 * TEMPORARY: select control interface to actively use
 *
 * So for now this is pretty crappy and held together with stitches;
 * make sure no messages are going around when you modify this with
 * _store().
 */
static
ssize_t i2400m_ctl_iface_show(struct device *dev,
			      struct device_attribute *attr,
			      char *buf)
{
	struct i2400m *i2400m = net_dev_to_i2400m(to_net_dev(dev));
	return scnprintf(buf, PAGE_SIZE, "%u\n", i2400m->ctl_iface);
}

static
ssize_t i2400m_ctl_iface_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t size)
{
	struct i2400m *i2400m = net_dev_to_i2400m(to_net_dev(dev));
	unsigned val;
	if (sscanf(buf, "%u\n", &val) != 1)
		return -EINVAL;
	i2400m->ctl_iface = val == 0? 0 : 1;
	return size;
}

static
DEVICE_ATTR(i2400m_ctl_iface, S_IRUGO | S_IWUSR,
	    i2400m_ctl_iface_show, i2400m_ctl_iface_store);
#endif				/* __USE_LEGACY_IOCTL */


/*
 * TEMPORARY: cold reset the device
 *
 * Need to use a workstruct because when done from sysfs, the device
 * lock is taken, so after a reset, the new device "instance" is
 * connected before we have a chance to disconnect the current
 * instance. This creates problems for upper layers, as for example
 * the management daemon for a while could think we have two wimax
 * connections in the system.
 * 
 * Note calling _put before _reset_cold is ok because _put uses
 * netdev's dev_put(), which won't free anything.
 *
 * In any case, it has to be before, as if not we wnter a race
 * coindition calling reset_cold(); it would try to unregister the
 * device, but it will keep the reference count and because reset had
 * a device lock...well, big mess.
 */
static
void __i2400m_reset_cold_work(struct work_struct *ws)
{
	struct i2400m_work *iw =
		container_of(ws, struct i2400m_work, ws);
	i2400m_put(iw->i2400m);
	i2400m_reset_cold(iw->i2400m);
	kfree(iw);
}

static
ssize_t i2400m_reset_cold_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	ssize_t result;
	struct i2400m *i2400m = net_dev_to_i2400m(to_net_dev(dev));
	unsigned val;

	result = -EINVAL;
	if (sscanf(buf, "%u\n", &val) != 1)
		goto error_no_unsigned;
	if (val != 1)
		goto error_bad_value;
	result = i2400m_schedule_work(i2400m, __i2400m_reset_cold_work,
				      GFP_KERNEL);
	if (result >= 0)
		result = size;
error_no_unsigned:
error_bad_value:
	return result;
}

static
DEVICE_ATTR(i2400m_reset_cold, S_IRUGO | S_IWUSR,
	    NULL, i2400m_reset_cold_store);


/*
 * soft reset the device
 *
 * We just soft reset the device to simulate what happens when it
 * crashes; the notif endpoint will receive ZLPs and a reboot barker
 * and then the bootstrap code will kick in.
 */
static
ssize_t i2400m_reset_soft_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	ssize_t result;
	struct i2400m *i2400m = net_dev_to_i2400m(to_net_dev(dev));
	unsigned val;

	result = -EINVAL;
	if (sscanf(buf, "%u\n", &val) != 1)
		goto error_no_unsigned;
	if (val != 1)
		goto error_bad_value;
	result = __i2400m_reset_soft(i2400m);
	if (result >= 0)
		result = size;
error_no_unsigned:
error_bad_value:
	return result;
}

static
DEVICE_ATTR(i2400m_reset_soft, S_IRUGO | S_IWUSR,
	    NULL, i2400m_reset_soft_store);

static
DEVICE_ATTR(i2400m_crash, S_IRUGO | S_IWUSR,
	    NULL, i2400m_reset_soft_store);


static
struct attribute *i2400m_dev_attrs[] = {
	&i2400m_ATTR_NAME(i2400m_tx_inflight).attr,
#ifdef __USE_LEGACY_IOCTL
	&dev_attr_i2400m_ctl_iface.attr,
#endif
	&dev_attr_i2400m_reset_cold.attr,	
	&dev_attr_i2400m_reset_soft.attr,
	&dev_attr_i2400m_crash.attr,
	NULL,
};

struct attribute_group i2400m_dev_attr_group = {
	.name = NULL,		/* we want them in the same directory */
	.attrs = i2400m_dev_attrs,
};
