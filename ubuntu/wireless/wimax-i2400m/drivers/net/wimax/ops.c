/*
 * Linux WiMax
 * Implementation of common wimax operations
 *
 * DEPRECATED!!!!!
 *
 * 
 * Copyright (C) 2005-2006 Intel Corporation <linux-wimax@intel.com>
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
 *
 *
 */
#include <linux/device.h>
#include <linux/genetlink.h>
#include <linux/netlink.h>
#include <net/wimax-i2400m.h>
#include "wimax-internal.h"

#define D_LOCAL 1
#include "debug.h"


/*
 * FIXME: this has to flush the message queue so a new "opener" gets
 *        only fresh messages and not stuff queued from the old one.
 *
 *        Hard to implement, as we have no way to tell. We might be
 *        able to queue what is cached in the socket, but we'd have to
 *        purge per family ID. From the device, we'd probably have to
 *        reset it to flush its queue.
 *
 *        Left as an open
 */
int __wimax_flush_queue(struct wimax_dev *wimax_dev, struct net_device *net_dev)
{
	int result = 0;
	struct device *dev = net_dev->dev.parent;
	d_fnstart(4, dev, "(wimax_dev %p)\n", wimax_dev);
#warning FIXME: finish me, flush queue
	d_printf(0, dev, "FIXME: finish me, flush queue\n");
	d_fnend(4, dev, "(wimax_dev %p) = %d\n", wimax_dev, result);
	return 0;
}

