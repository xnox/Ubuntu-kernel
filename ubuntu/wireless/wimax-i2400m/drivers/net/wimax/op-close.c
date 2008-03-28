/*
 * Linux WiMax
 * Netlink layer, close operation
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
 */
#include <net/genetlink.h>
#include <net/wimax.h>
#include "wimax-internal.h"

#define D_LOCAL 0
#include "debug.h"


static struct nla_policy wimax_gnl_close_policy[] = {
};


static
int wimax_gnl_doit_close(struct sk_buff *skb, struct genl_info *info)
{
	int result = -ENODEV;
	struct net_device *net_dev;
	struct wimax_dev *wimax_dev;
	struct device *dev;

	d_fnstart(3, NULL, "(skb %p info %p)\n", skb, info);
	net_dev = wimax_get_netdev_by_info(info);
	if (net_dev == NULL)
		goto error_no_wimax_dev;
	dev = net_dev->dev.parent;
	wimax_dev = net_dev_to_wimax(net_dev);

	result = 0;
	__wimax_flush_queue(wimax_dev, net_dev);
	wimax_dev->genl_pid = 0;
	
	dev_put(net_dev);
error_no_wimax_dev:
	d_fnend(3, NULL, "(skb %p info %p) = %d\n", skb, info, result);
	return result;
}


struct genl_ops wimax_gnl_close = {
	.cmd = WIMAX_GNL_OP_CLOSE,
	.flags = 0,
	.policy = wimax_gnl_close_policy,
	.doit = wimax_gnl_doit_close,
	.dumpit = NULL,
};
