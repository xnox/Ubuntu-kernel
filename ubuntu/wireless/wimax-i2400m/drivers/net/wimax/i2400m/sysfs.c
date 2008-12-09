/*
 * Intel Wireless WiMAX Connection 2400m
 * Sysfs interfaces to show driver and device information
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
 */

#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/spinlock.h>
#include <linux/device.h>
#include "i2400m.h"


#define D_SUBMODULE sysfs
#include "debug-levels.h"

/*
 * cold reset the device deferred work routine
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
	iw->i2400m->bus_reset(iw->i2400m, I2400M_RT_COLD);
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
	result = i2400m_schedule_work(i2400m, 0, __i2400m_reset_cold_work,
				      GFP_KERNEL, NULL, 0);
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
 * We just soft reset the device; the notif endpoint will receive ZLPs
 * and a reboot barker and then the bootstrap code will kick in.
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
	result = i2400m->bus_reset(i2400m, I2400M_RT_SOFT);
	if (result >= 0)
		result = size;
error_no_unsigned:
error_bad_value:
	return result;
}

static
DEVICE_ATTR(i2400m_reset_soft, S_IRUGO | S_IWUSR,
	    NULL, i2400m_reset_soft_store);


/*
 * Show RX statistics
 *
 * Total #payloads | min #payloads in a RX | max #payloads in a RX
 * Total #RXs | Total bytes | min #bytes in a RX | max #bytes in a RX
 *
 * Write 1 to clear.
 */
static
ssize_t i2400m_rx_stats_show(struct device *dev,
			     struct device_attribute *attr,
			     char *buf)
{
	ssize_t result;
	struct i2400m *i2400m = net_dev_to_i2400m(to_net_dev(dev));
	unsigned long flags;

	spin_lock_irqsave(&i2400m->rx_lock, flags);
	result = snprintf(buf, PAGE_SIZE, "%u %u %u %u %u %u %u\n",
			  i2400m->rx_pl_num, i2400m->rx_pl_min,
			  i2400m->rx_pl_max, i2400m->rx_num,
			  i2400m->rx_size_acc,
			  i2400m->rx_size_min, i2400m->rx_size_max);
	spin_unlock_irqrestore(&i2400m->rx_lock, flags);
	return result;
}

static
ssize_t i2400m_rx_stats_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t size)
{
	ssize_t result;
	struct i2400m *i2400m = net_dev_to_i2400m(to_net_dev(dev));
	unsigned val;
	unsigned long flags;

	result = -EINVAL;
	if (sscanf(buf, "%u\n", &val) != 1)
		goto error_no_unsigned;
	if (val != 1)
		goto error_bad_value;
	spin_lock_irqsave(&i2400m->rx_lock, flags);
	i2400m->rx_pl_num = 0;
	i2400m->rx_pl_max = 0;
	i2400m->rx_pl_min = ULONG_MAX;
	i2400m->rx_num = 0;
	i2400m->rx_size_acc = 0;
	i2400m->rx_size_min = ULONG_MAX;
	i2400m->rx_size_max = 0;
	spin_unlock_irqrestore(&i2400m->rx_lock, flags);
	result = size;
error_no_unsigned:
error_bad_value:
	return result;
}

static
DEVICE_ATTR(i2400m_rx_stats, S_IRUGO | S_IWUSR,
	    i2400m_rx_stats_show, i2400m_rx_stats_store);


/*
 * Show TX statistics
 *
 * Total #payloads | min #payloads in a TX | max #payloads in a TX
 * Total #TXs | Total bytes | min #bytes in a TX | max #bytes in a TX
 *
 * Write 1 to clear.
 */
static
ssize_t i2400m_tx_stats_show(struct device *dev,
			     struct device_attribute *attr,
			     char *buf)
{
	ssize_t result;
	struct i2400m *i2400m = net_dev_to_i2400m(to_net_dev(dev));
	unsigned long flags;

	spin_lock_irqsave(&i2400m->tx_lock, flags);
	result = snprintf(buf, PAGE_SIZE, "%u %u %u %u %u %u %u\n",
			  i2400m->tx_pl_num, i2400m->tx_pl_min,
			  i2400m->tx_pl_max, i2400m->tx_num,
			  i2400m->tx_size_acc,
			  i2400m->tx_size_min, i2400m->tx_size_max);
	spin_unlock_irqrestore(&i2400m->tx_lock, flags);
	return result;
}

static
ssize_t i2400m_tx_stats_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t size)
{
	ssize_t result;
	struct i2400m *i2400m = net_dev_to_i2400m(to_net_dev(dev));
	unsigned val;
	unsigned long flags;

	result = -EINVAL;
	if (sscanf(buf, "%u\n", &val) != 1)
		goto error_no_unsigned;
	if (val != 1)
		goto error_bad_value;
	spin_lock_irqsave(&i2400m->tx_lock, flags);
	i2400m->tx_pl_num = 0;
	i2400m->tx_pl_max = 0;
	i2400m->tx_pl_min = ULONG_MAX;
	i2400m->tx_num = 0;
	i2400m->tx_size_acc = 0;
	i2400m->tx_size_min = ULONG_MAX;
	i2400m->tx_size_max = 0;
	spin_unlock_irqrestore(&i2400m->tx_lock, flags);
	result = size;
error_no_unsigned:
error_bad_value:
	return result;
}

static
DEVICE_ATTR(i2400m_tx_stats, S_IRUGO | S_IWUSR,
	    i2400m_tx_stats_show, i2400m_tx_stats_store);

/*
 * Show debug stuff
 */
static
ssize_t i2400m_debug_stuff_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	ssize_t result = 0;
	struct i2400m *i2400m = net_dev_to_i2400m(to_net_dev(dev));
	unsigned long flags;

	result += scnprintf(buf, PAGE_SIZE, "queue is %s\n",
			    netif_queue_stopped(to_net_dev(dev))?
			    "stopped" : "running");

	spin_lock_irqsave(&i2400m->tx_lock, flags);
	result += scnprintf(
		buf + result, PAGE_SIZE - result,
		"TX FIFO in %zu out %zu (%zu used) msg @%d\n",
		i2400m->tx_in, i2400m->tx_out,
		i2400m->tx_out - i2400m->tx_in,
		i2400m->tx_msg? (void *) i2400m->tx_msg - i2400m->tx_buf : -1);
	spin_unlock_irqrestore(&i2400m->tx_lock, flags);

	return result;
}
static
DEVICE_ATTR(i2400m_debug_stuff, S_IRUGO | S_IWUSR,
	    i2400m_debug_stuff_show, NULL);


/*
 * Debug levels control; see debug.h
 */
struct d_level d_level[] = {
	D_SUBMODULE_DEFINE(control),
	D_SUBMODULE_DEFINE(driver),
	D_SUBMODULE_DEFINE(fw),
	D_SUBMODULE_DEFINE(netdev),
	D_SUBMODULE_DEFINE(rfkill),
	D_SUBMODULE_DEFINE(rx),
	D_SUBMODULE_DEFINE(sysfs),
	D_SUBMODULE_DEFINE(tx),
};
size_t d_level_size = ARRAY_SIZE(d_level);

static
DEVICE_ATTR(i2400m_debug_levels, S_IRUGO | S_IWUSR,
	    d_level_show, d_level_store);


static
struct attribute *i2400m_dev_attrs[] = {
	&dev_attr_i2400m_reset_cold.attr,
	&dev_attr_i2400m_reset_soft.attr,
	&dev_attr_i2400m_rx_stats.attr,
	&dev_attr_i2400m_tx_stats.attr,
	&dev_attr_i2400m_debug_stuff.attr,
	&dev_attr_i2400m_debug_levels.attr,
	NULL,
};

struct attribute_group i2400m_dev_attr_group = {
	.name = NULL,		/* we want them in the same directory */
	.attrs = i2400m_dev_attrs,
};
