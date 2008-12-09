/*
 * Linux WiMAX
 * Generic netlink messaging interface
 *
 *
 * Copyright (C) 2007-2008 Intel Corporation <linux-wimax@intel.com>
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
 * NETLINK AND ACKING RECEPTION OF COMMANDS
 *
 * Netlink will send an ACK back to user space on command execution if
 * the NLM_F_ACK flag is set. In practice this is always sent because
 * libnl always sets NLM_F_ACK. When the generic netlink's doit()
 * handler returns, the return code is sent in the form of a &struct
 * nlmsgerr.
 *
 * However, the ACK (on NLM_F_ACK) is sent after running the callback
 * (which makes some sense); however, if the callback sends something,
 * then you have an ordering issue when you want to split things
 * between sending the request and the reply to the request coming
 * later in and not being handled as a netlink ACK.
 *
 * This is why we use two handles to speak to uses space; one is only
 * for user-to-kernel (write to kernel, read ack from kernel). The
 * other one is strictly to transmit from kernel to user.
 *
 * This way when a message is sent that causes data being sent back to
 * user space, it uses different netlink handles for the ack and the
 * data and thus user space doesn't get to confuse about ther ordering
 * (data coming in before the ack).
 *
 * Side benefit, makes threading easier.
 */
#include <linux/device.h>
#include <net/genetlink.h>
#include <linux/netdevice.h>
#include <net/wimax.h>
#include "wimax-internal.h"


#define D_SUBMODULE op_msg
#include "debug-levels.h"


/**
 * wimax_msg_to_user_alloc - allocate a new skb/msg for sending to userspace
 *
 * @wimax_dev: WiMAX device instance
 * @size: size of the message to send (in bytes), including the header.
 * @gfp_flags: flags for memory allocation.
 *
 * Returns: 0 if ok, < 0 errno code on error
 *
 * Description:
 *
 * Allocates an skb for fitting in a free form message and
 * preinitializes it. Once this call is done you can copy data to the
 * space (skb->data, as much as skb->len bytes) and deliver it with
 * wimax_msg2u_send().
 *
 * IMPORTANT:
 *
 * Don't use skb_push()/skb_pull()/skb_reserve() on the skb, as
 * wimax_msg_to_user_send() depends on skb->data being placed at the
 * beginning of the user message.
 */
struct sk_buff *wimax_msg_to_user_alloc(struct wimax_dev *wimax_dev,
					size_t size, gfp_t gfp_flags)
{
	void *msg;
	struct sk_buff *skb;

	skb = genlmsg_new(size, gfp_flags);
	if (skb == NULL)
		return NULL;
	msg = genlmsg_put(skb, wimax_dev->genl_pid, 0, &wimax_dev->gnl_family,
			  0, WIMAX_GNL_OP_MSG_TO_USER);
	BUG_ON(msg == NULL);
	skb_put(skb, size);
	/* make skb->data point to the payload so callers can use it */
	skb_pull(skb, msg - (void *) skb->data);
	return skb;
}
EXPORT_SYMBOL_GPL(wimax_msg_to_user_alloc);


/**
 * wimax_msg_to_user_send - send a pre-allocated message to user space
 *
 * @skb: &struct sk_buff returned by wimax_msg_to_user_alloc() and
 *       with its payload initialized with a message. Note the
 *       ownership of @skb is transferred to this function.
 *
 * Returns: 0 if ok, < 0 errno code on error
 *
 * Description:
 *
 * Sends a free-form message that was preallocated with
 * wimax_msg_to_user_alloc() and filled up.
 *
 * Assumes that once you pass an skb to the genl stack for sending,
 * it owns it and will release it when done.
 *
 * IMPORTANT:
 *
 * Don't use skb_push()/skb_pull()/skb_reserve() on the skb, as
 * wimax_msg_to_user_send() depends on skb->data being placed at the
 * beginning of the user message.
 */
int wimax_msg_to_user_send(struct wimax_dev *wimax_dev, struct sk_buff *skb)
{
	struct device *dev = wimax_dev->net_dev->dev.parent;
	void *msg = skb->data;
	size_t size = skb->len;

	skb_push(skb, skb_headroom(skb));	/* for _unicast() */
	genlmsg_end(skb, msg);
	d_printf(1, dev, "CTX: wimax msg, %zu bytes\n", size);
	d_dump(2, dev, msg, size);
	return genlmsg_unicast(skb, wimax_dev->genl_pid);
}
EXPORT_SYMBOL_GPL(wimax_msg_to_user_send);



/**
 * wimax_msg_to_user - send a message to user space
 *
 * @wimax_dev: WiMAX device instance
 * @buf: pointer to the message to send.
 * @size: size of the buffer pointed to by @buf (in bytes),
 *        including the header.
 * @gfp_flags: flags for memory allocation.
 * @pid: netlink PID destination (can be taken from 'struct
 *       genl_info').
 *
 * Returns: 0 if ok, < 0 errno code on error
 *
 * Description:
 *
 * Sends a free-form message to user space on the kernel-to-user
 * handle of the &struct wimax_dev.
 *
 * FIXME:
 *
 * It needs to copy around the message to a new skb; should add an
 * interface that takes an skb with enough header prefix. Don't know
 * how feasible that is.
 *
 * NOTES:
 *
 * Assumes that once you pass an skb to the genl stack for sending,
 * it owns it and will release it when done.
 */
int wimax_msg_to_user(struct wimax_dev *wimax_dev,
		      const void *buf, size_t size,
		      gfp_t gfp_flags)
{
	int result = -ENOMEM;
	struct sk_buff *skb;

	skb = wimax_msg_to_user_alloc(wimax_dev, size, gfp_flags);
	if (skb == NULL)
		goto error_msg_new;
	memcpy(skb->data, buf, size);
	result = wimax_msg_to_user_send(wimax_dev, skb);
error_msg_new:
	return result;
}
EXPORT_SYMBOL_GPL(wimax_msg_to_user);


/*
 * Relays a message from user space to the driver
 *
 * The skb is passed to the driver-specific function with the netlink
 * and generic netlink headers already stripped.
 */
static
int wimax_gnl_doit_msg_from_user(struct sk_buff *skb, struct genl_info *info)
{
	int result = -ENODEV;
	struct net_device *net_dev;
	struct wimax_dev *wimax_dev;
	struct device *dev;
	struct nlmsghdr *nl;
	struct genlmsghdr *genl;
	void *buf;

	d_fnstart(3, NULL, "(skb %p info %p)\n", skb, info);
	net_dev = wimax_get_netdev_by_info(info);
	if (net_dev == NULL)
		goto error_no_wimax_dev;
	result = -ENOSYS;
	dev = net_dev->dev.parent;
	wimax_dev = net_dev_to_wimax(net_dev);
	if (wimax_dev->op_msg_from_user == NULL)
		goto error_noop;

	nl = (void *) skb->data;
	genl = (void *) skb->data + sizeof(*nl);
	buf = skb_pull(skb, sizeof(*nl) + sizeof(*genl));
	d_printf(1, dev,
		 "CRX: nlmsghdr len %u type %u flags 0x%04x seq 0x%x pid %u\n",
		 nl->nlmsg_len, nl->nlmsg_type, nl->nlmsg_flags, nl->nlmsg_seq,
		 nl->nlmsg_pid);
	d_printf(1, dev, "CRX: genlmsghdr cmd %u version %u\n",
		 genl->cmd, genl->version);
	d_printf(1, dev, "CRX: wimax message %zu bytes\n", skb->len);
	d_dump(2, dev, buf, skb->len);

	result = wimax_dev->op_msg_from_user(wimax_dev, skb, info);
error_noop:
	dev_put(net_dev);
error_no_wimax_dev:
	d_fnend(3, NULL, "(skb %p info %p) = %d\n", skb, info, result);
	return result;
}


/*
 * Generic Netlink glue
 *
 * We use no attributes, we map to devices by using the family ID and
 * pack everything in the payload.
 */

struct nla_policy wimax_gnl_policy[] = {
};

struct genl_ops wimax_gnl_msg_from_user = {
	.cmd = WIMAX_GNL_OP_MSG_FROM_USER,
	.flags = 0,
	.policy = wimax_gnl_policy,
	.doit = wimax_gnl_doit_msg_from_user,
	.dumpit = NULL,
};

