/*
 * Linux WiMax
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
 * FIXME: docs
 *
 * NETLINK AND ACKING RECEPTION OF COMMANDS
 *
 * FIXME: fix all this 'command' -> 'message'
 * 
 * Netlink will send an ACK back to user space on command execution if
 * the NLM_F_ACK flag is set. It will always send an ACK in the form
 * of an error message if there is an error handling the message or
 * the callback returns < 0.
 *
 * However, the ACK (on NLM_F_ACK) is sent after running the callback
 * (which makes some sense); however, if the callback sends something,
 * then you have an ordering issue when you want to split things
 * between sending the request and the reply to the request coming
 * later in and not being handled as a netlink ACK.
 *
 * - Clarification of that last part
 *
 * A command is usually (a) 'Send CMD to destination', (b) 'receive
 * EVENT from destination as an answer to that command'.
 *
 * Layer A sends that and users the wimax_msg_*() interface to do
 * so:
 *
 *     ...
 *     wimax_msg_write(CMD);
 *     wimax_msg_read(EVENT);
 *     ...
 *
 * wimax_msg_write() just writes the command over netlink and
 * receives from netlink an ack that confirms the 'write' went ok. Now
 * if the EVENT produced by CMD came before the netlink ACK to CMD,
 * then wimax_msg_write() will get it when expecting an ACK and the
 * whole thing will be confused, because there is no way to pass it to
 * the EVENT reader. We can't peek to see if the next message in the
 * queue is an ACK message or an actual data message.
 *
 * - Ugly hack / workaround
 *
 * So to solve this we have to force an ugly interface -- we declare
 * function wimax_cmd_ack() that your command has to call AFTER he has
 * successfully processed and delivered it to its final destination
 * and BEFORE sending back any possible reply (event) to it. That way
 * the sequence is always:
 *
 *    CMD             kernel <- user
 *    netlink ACK     kernel -> user
 *    EVENT           kernel -> user
 *
 * In case of error:
 *
 *    CMD             kernel <- user
 *    netlink ERR     kernel -> user
 *
 * In the kernel:
 *
 *          ...
 *          check_a();
 *          if (some_condition)
 *                   return -ERRNO;
 *          // all is good, send the reply -- no -ERRNO allowed from
 *          // now on
 *          wimax_msg_ack();
 *          send_an_reply_event();
 *          ....
 *          return 0;
 */
#include "version.h"
#include <linux/device.h>
#include <net/genetlink.h>
#include <linux/netdevice.h>
#include <net/wimax.h>
#include "wimax-internal.h"

#define D_LOCAL 2
#include "debug.h"


/**
 * wimax_msg_ack - ack the successful delivery of a WiMax control message
 *
 * @wimax_dev: wimax device descriptor
 * @skb_msg: skb where the message was sent over (won't be modified)
 * @gfp_flags: allocation flags use
 * @genl_info: genl_info passed along with @skb_msg by the Generic
 * Netlink code through the doit() op. Needed to grab for-reply
 * headers. 
 * 
 * Sends a netlink success ack back to the sender of the message. Need
 * to execute this BEFORE sending any event as a result of the command
 * execution.
 *
 * For example, if your driver needs to forward the command over a USB
 * pipe, you'd call this after the USB subsystem reports your command
 * was properly sent BUT before it replies.
 *
 * This call is modeled right after netlink_ack(). Yes, I think I
 * could be using it. FIXME.
 */
void wimax_msg_ack(struct wimax_dev *wimax_dev, struct sk_buff *skb_msg,
		   gfp_t gfp_flags, const struct genl_info *genl_info)
{
	struct sk_buff *skb;
	struct nlmsghdr *nl;
	struct nlmsgerr *err_msg;
	struct net_device *net_dev = wimax_dev->net_dev;

	skb = nlmsg_new(sizeof(*err_msg), gfp_flags);
	if (skb == NULL)
		goto error_nlmsg_new;
	nl = __nlmsg_put(skb, NETLINK_CB(skb_msg).pid, 0,
			 NLMSG_ERROR, sizeof(*err_msg), 0);
	err_msg = nlmsg_data(nl);
	err_msg->error = 0;
	memcpy(&err_msg->msg, genl_info->nlhdr, sizeof(err_msg->msg));
	netlink_unicast(skb_msg->sk, skb, NETLINK_CB(skb_msg).pid,
			MSG_DONTWAIT);
	return;

error_nlmsg_new:
	/* Lame */
	dev_err(net_dev->dev.parent,
		"wimax stack: memory error during ack allocation\n");
	return;
}
EXPORT_SYMBOL_GPL(wimax_msg_ack);


/**
 *
 * @wimax_dev: WiMax device instance
 * @buf: pointer to the message to send.
 * @size: size of the buffer pointed to by @buf (in bytes),
 *        including the header.
 * @gfp_flags: flags for memory allocation.
 * @pid: netlink PID destination (can be taken from 'struct
 *       genl_info').
 * @return 0 if ok, < 0 errno code on error
 *
 * WARNING: FIXME, dang stupid that we have to copy around; it should
 *          be possible to chain the @buf buffer up to the skb
 *          without copying around.
 *
 * NOTE: I am assuming that once you pass an skb to the genl stack for
 *       sending, it owns it and will free it when done.
 */
int wimax_msg_to_user(struct wimax_dev *wimax_dev,
		      const void *buf, size_t size,
		      gfp_t gfp_flags)
{
	int result = -ENOMEM;
	struct device *dev = wimax_dev->net_dev->dev.parent;
	void *msg, *payload;
	struct sk_buff *skb;

	skb = genlmsg_new(size, gfp_flags);
	if (skb == NULL)
		goto error_msg_new;
	msg = genlmsg_put(skb, wimax_dev->genl_pid, 0, &wimax_dev->gnl_family,
			  0, WIMAX_GNL_OP_MSG_TO_USER);
	if (msg == NULL)
		goto error_msg_put_reply;
	payload = skb_put(skb, size);
	memcpy(payload, buf, size);
	genlmsg_end(skb, msg);
	d_printf(1, dev, "CTX: wimax msg, %zu bytes\n", size);
	d_dump(2, dev, buf, size);
	result = genlmsg_unicast(skb, wimax_dev->genl_pid);
	return 0;

error_msg_put_reply:
	nlmsg_free(skb);
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

