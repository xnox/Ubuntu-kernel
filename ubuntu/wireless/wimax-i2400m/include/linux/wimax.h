/*
 * Linux WiMax
 * API for kernel space
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
 * FIXME: doc: overview of the API, different parts and pointers
 *
 * WARNING: Still in flux, don't trust the deocumentation that much
 */

#ifndef __LINUX__WIMAX_H__
#define __LINUX__WIMAX_H__
#ifdef __KERNEL__

#include <net/wimax.h>
#include <net/genetlink.h>
#include <linux/netdevice.h>

struct net_device;
struct genl_info;
struct wimax_dev;


/**
 * General instance of a Wimax device
 *
 * For now this just packs what we think are/will be the common
 * fields in a WiMax drivers and the control path from user space to
 * the driver. The control path comprises the WiMax generic
 * user-to-kernel API (very limited as of now) with a direct pipe to
 * the driver for device-specific features.
 *
 * @genl_pid: generic netlink PID destination for the process who has
 *            openned the generic netlink control connection to the
 *            device. 0 means nobody is connected.
 *
 * @stats: Network device statistics
 * 
 * @gnl_family: generic netlink pipe, for command/control from
 *              userspace.
 *
 * @op_msg_from_user:
 *
 *       Driver-specific operation to handle a raw message from user
 *       space to the driver. The oposite path is done with
 *       wimax_msg_to_user(). 
 *
 * Usage:
 *
 * Embed one of this in your network device, memset it to zero,
 * initialize with wimax_dev_init(); fill in the op*() function
 * pointers and call wimax_dev_add() after registering the network
 * device. This will populate sysfs entries in the device's directory
 * that tag it as a device that supports the Wimax control interface
 * (for userspace). wimax_dev_rm() undoes before unregistering the
 * network device.
 *
 * FIXME: semaphore to make sure we stop the control path
 */
struct wimax_dev {
	int genl_pid;
	struct net_device_stats stats;
	struct genl_family gnl_family;
	int (*op_msg_from_user)(struct wimax_dev *wimax_dev,
				struct sk_buff *skb,
				const struct genl_info *info);
	struct net_device *net_dev;
};

extern void wimax_dev_init(struct wimax_dev *);
extern int wimax_dev_add(struct wimax_dev *, struct net_device *);
extern void wimax_dev_rm(struct wimax_dev *, struct net_device *);

static inline
struct wimax_dev *net_dev_to_wimax(struct net_device *net_dev)
{
	return netdev_priv(net_dev);
}

extern void wimax_msg_ack(struct wimax_dev *, struct sk_buff *, gfp_t,
			  const struct genl_info *);
extern int wimax_msg_to_user(struct wimax_dev *, const void *, size_t, gfp_t);

#endif /* #ifdef __KERNEL__ */
#endif /* #ifndef __LINUX__WIMAX_H__ */
