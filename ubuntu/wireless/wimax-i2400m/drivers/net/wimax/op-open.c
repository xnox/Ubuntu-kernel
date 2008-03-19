/*
 * Linux WiMax
 * Netlink layer, open operation
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
 * Only one attribute: the RX pid for the message pipe (see the design
 * section, we use two handles for the messaging pipe, one for
 * user-to-kernel [RX], one for kernel-to-user [TX]).
 *
 * We need to define attribute numbers starting in 0 [I think]
 */
#include <net/genetlink.h>
#include <net/wimax.h>
#include "wimax-internal.h"

#define D_LOCAL 0
#include "debug.h"



static struct nla_policy wimax_gnl_open_policy[] = {
	[WIMAX_GNL_OPEN_MSG_FROM_USER_PID] = {
		.type = NLA_U32
	},
};


static
int wimax_gnl_doit_open(struct sk_buff *skb, struct genl_info *info)
{
	int result = -ENODEV;
	struct net_device *net_dev;
	struct wimax_dev *wimax_dev;
	struct device *dev;
	struct nlmsghdr *nlh;
	struct nlattr *tb[WIMAX_GNL_OPEN_MAX];

	/* If this fails to build, increase net/wimax.h:WIMAX_GNL_ATTR_MAX */
	BUILD_BUG_ON(WIMAX_GNL_OPEN_MAX >= WIMAX_GNL_ATTR_MAX);

	d_fnstart(3, NULL, "(skb %p info %p)\n", skb, info);
	net_dev = wimax_get_netdev_by_info(info);
	if (net_dev == NULL)
		goto error_no_wimax_dev;
	dev = net_dev->dev.parent;
	wimax_dev = net_dev_to_wimax(net_dev);
	
	nlh = (void *) skb->data;
	result = nla_parse(tb, ARRAY_SIZE(tb),
			   nlmsg_attrdata(nlh, sizeof(struct genlmsghdr)),
			   nlmsg_attrlen(nlh, sizeof(struct genlmsghdr)),
			   wimax_gnl_open_policy);
	if (result < 0) {
		dev_err(dev, "WIMAX_GNL_OPEN: can't parse message: %d\n",
			result);
		goto error_parse;
	}
	result = -EINVAL;
	if (tb[WIMAX_GNL_OPEN_MSG_FROM_USER_PID] == NULL) {
		dev_err(dev, "WIMAX_GNL_OPEN: can't find MSG_FROM_USER_PID "
			"attribute\n");
		goto error_no_pid;
	}
	
	result = 0;
	__wimax_flush_queue(wimax_dev, net_dev);
	wimax_dev->genl_pid =
		nla_get_u32(tb[WIMAX_GNL_OPEN_MSG_FROM_USER_PID]);
	d_printf(2, dev, "msg_from_user PID is %d\n", wimax_dev->genl_pid);
error_no_pid:
error_parse:
	dev_put(net_dev);
error_no_wimax_dev:
	d_fnend(3, NULL, "(skb %p info %p) = %d\n", skb, info, result);
	return result;
}


struct genl_ops wimax_gnl_open = {
	.cmd = WIMAX_GNL_OP_OPEN,
	.flags = 0,
	.policy = wimax_gnl_open_policy,
	.doit = wimax_gnl_doit_open,
	.dumpit = NULL,
};
