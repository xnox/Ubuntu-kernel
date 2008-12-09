/* -*-linux-c-*-
 *
 * Intel Wireless WiMAX Connection 2400m
 * Transport/bus independent RX handling
 *
 *
 *
 * Copyright (C) 2007-2008 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 * Intel Corporation <linux-wimax@intel.com>
 * Yanir Lubetkin <yanirx.lubetkin@intel.com>
 *  - Initial implementation
 * Inaky Perez-Gonzalez <inaky.perez-gonzalez@intel.com>
 *  - Use skb_clone(), break up processing in chunks
 *  - Split transport/device specific
 *  - Make buffer size dynamic to exert less memory pressure
 *
 *
 * This handles the RX path.
 *
 * We receive an RX message from the bus-specific driver, which
 * contains one or more payloads that have potentially different
 * destinataries (data or control paths).
 *
 * So we just take that payload from the transport specific code in
 * the form of an skb, break it up in chunks (a cloned skb each in the
 * case of network packets) and pass it to netdev or to the
 * command/ack handler (and from there to the WiMAX stack).
 *
 * TRANSMISSION FORMAT:
 *
 * The format of the buffer is:
 *
 * HEADER                      (struct i2400m_msg_hdr)
 * PAYLOAD DESCRIPTOR 0        (struct i2400m_pld)
 * PAYLOAD DESCRIPTOR 1
 * ...
 * PAYLOAD DESCRIPTOR N
 * PAYLOAD 0                   (raw bytes)
 * PAYLOAD 1
 * ...
 * PAYLOAD N
 *
 * See tx.c for a deeper description on alignment requirements and
 * other fun facts of it.
 *
 * ROADMAP
 *
 * i2400m_rx()
 *   i2400m_rx_msg_hdr_check()
 *   i2400m_rx_pkt_descr_check()
 *   i2400m_rx_payload()
 *     i2400m_net_rx()
 *     i2400m_rx_ctl()
 *       i2400m_report_hook()
 *       wimax_msg_to_user()
 */
#include <linux/kernel.h>
#include <linux/kernel-compat.h>	/* @lket@ignore-line */
#include <linux/if_arp.h>
#include <linux/netdevice.h>
#include <linux/workqueue.h>
#include "i2400m.h"


#define D_SUBMODULE rx
#include "debug-levels.h"

struct i2400m_report_hook_args {
	struct sk_buff *skb_rx;
	const struct i2400m_l3l4_hdr *l3l4_hdr;
	size_t size;
};


/*
 * Unpack arguments from the deferred call, execute it and drop
 * references.
 *
 * Silly NOTE: References are needed because we are a separate thread;
 *     otherwise the buffer changes under us because it is released by
 *     the original caller.
 */
static
void i2400m_report_hook_work(struct work_struct *ws)
{
	struct i2400m_work *iw =
		container_of(ws, struct i2400m_work, ws);
	struct i2400m_report_hook_args *args = (void *) iw->pl;
	i2400m_report_hook(iw->i2400m, args->l3l4_hdr, args->size);
	kfree_skb(args->skb_rx);
	i2400m_put(iw->i2400m);
	kfree(iw);
}


/*
 * Receive and (minimally) process a control payload
 *
 * @i2400m: device descriptor
 * @skb_rx: skb that contains the payload (for reference counting)
 * @payload: pointer to message
 * @size: size of the message
 *
 * There are two types of control RX messages: reports (asynchronous,
 * like your every day life interrupts) and 'acks' (reponses to a
 * command, get or set request).
 *
 * If it is a report, we run hooks on it (to extract information for
 * things we need to do in the driver) and then pass it over to the
 * WiMAX stack to send it to user space.
 *
 * NOTE: The driver uses i2400m->work_queue for TX and RX (and this
 *     function is called from that context), so if we try to call any
 *     commands from here, we'll deadlock. So processing of
 *     notifications is deferred to yet another workqueue (the
 *     system's generic).
 *
 * If it is not a report, it is an ack to a previosuly executed
 * command, set or get, so wake up whoever is waiting for it from
 * i2400m_msg_to_dev(). It might end up passing it up to the user.
 *
 * Note that the sizes we pass to other functions from here are the
 * sizes of the _l3l4_hdr + payload, not full buffer sizes, as we have
 * verified in _msg_size_check() that they are congruent.
 *
 * For reports: We can't clone the original skb where the data is
 * because we need to send this up via netlink; netlink has to add
 * headers and we can't overwrite what's preceeding the payload...as
 * it is another message. So we just dup them.
 */
static
void i2400m_rx_ctl(struct i2400m *i2400m, struct sk_buff *skb_rx,
		   const void *payload, size_t size)
{
	int result;
	struct device *dev = i2400m_dev(i2400m);
	struct wimax_dev *wimax_dev = &i2400m->wimax_dev;
	const struct i2400m_l3l4_hdr *l3l4_hdr = payload;
	unsigned msg_type;

	result = i2400m_msg_size_check(i2400m, l3l4_hdr, size);
	if (result < 0) {
		dev_err(dev, "HW BUG? device sent a bad message: %d\n",
			result);
		goto error_check;
	}
	msg_type = le16_to_cpu(l3l4_hdr->type);
	d_printf(1, dev, "%s 0x%04x: %u bytes\n",
		 msg_type & I2400M_MT_REPORT_MASK? "REPORT" : "CMD/SET/GET",
		 msg_type, size);
	d_dump(2, dev, l3l4_hdr, size);
	if (msg_type & I2400M_MT_REPORT_MASK) {
		struct i2400m_report_hook_args args = {
			.skb_rx = skb_rx,
			.l3l4_hdr = l3l4_hdr,
			.size = size
		};
		skb_get(skb_rx);
		i2400m_schedule_work(i2400m, 0, i2400m_report_hook_work,
				     GFP_KERNEL, &args, sizeof(args));
		if (unlikely(i2400m->ready == 0))	/* only send if up */
			return;
		result = wimax_msg_to_user(&i2400m->wimax_dev, l3l4_hdr, size,
					   GFP_KERNEL);
		if (result < 0)
			dev_err(dev, "error sending report to userspace: %d\n",
				result);
	} else {		/* an ack to a CMD, GET or SET */
		i2400m->ack_skb = wimax_msg_to_user_alloc(wimax_dev, size,
							  GFP_KERNEL);
		if (i2400m->ack_skb == NULL) {
			dev_err(dev, "CMD/GET/SET ack: cannot allocate SKB\n");
			result = -ENOMEM;
		} else
			memcpy(i2400m->ack_skb->data, payload, size);
		i2400m->ack_status = result;
		complete(&i2400m->msg_completion);
	}
error_check:
	return;
}


/*
 * Act on a received payload
 *
 * @i2400m: device instance
 * @payload: pointer to payload descriptor
 * @payload: pointer to payload payload; the amount of available data
 *           is guaranteeed to be as much as declared in payload->size.
 *
 * Upon reception of a payload, look at its guts in the payload
 * descriptor and decide what to do with it.
 */
static
void i2400m_rx_payload(struct i2400m *i2400m,
		      struct sk_buff *skb_rx,
		      const struct i2400m_pld *pld,
		      const void *payload)
{
	struct device *dev = i2400m_dev(i2400m);
	size_t pl_size = i2400m_pld_size(pld);
	enum i2400m_pt pl_type = i2400m_pld_type(pld);

	switch (pl_type) {
	case I2400M_PT_DATA:
		d_printf(3, dev, "RX: data payload %zu bytes\n", pl_size);
		i2400m_net_rx(i2400m, skb_rx, payload, pl_size);
		break;
	case I2400M_PT_CTRL:
		i2400m_rx_ctl(i2400m, skb_rx, payload, pl_size);
		break;
	case I2400M_PT_TRACE:
		d_printf(4, dev, "RX: trace payload %zu bytes\n", pl_size);
		d_dump(5, dev, payload, pl_size);
		if (d_test(1))
			i2400m_rx_ctl(i2400m, skb_rx, payload, pl_size);
		break;
	default:	/* Anything else shouldn't come to the host */
		if (printk_ratelimit())
			dev_err(dev, "RX: HW BUG? unexpected payload type %u\n",
				pl_type);
	}
}


/*
 * Check received buffer's message header
 *
 * @i2400m: device descriptor
 * @msg_hdr: message header
 * @buf_size: size of the received buffer
 *
 * Check that the declarations done by a RX buffer message header are
 * sane and consistent.
 */
static
int i2400m_rx_msg_hdr_check(struct i2400m *i2400m,
			    const struct i2400m_msg_hdr *msg_hdr,
			    size_t buf_size)
{
	int result = -EIO;
	struct device *dev = i2400m_dev(i2400m);
	if (buf_size < sizeof(*msg_hdr)) {
		dev_err(dev, "RX: HW BUG? message with short header (%zu "
			"vs %zu bytes expected)\n", buf_size, sizeof(*msg_hdr));
		goto error;
	}
	if (msg_hdr->barker != cpu_to_le32(I2400M_D2H_MSG_BARKER)) {
		dev_err(dev, "RX: HW BUG? message received with unknown "
			"barker 0x%08x\n", le32_to_cpu(msg_hdr->barker));
		goto error;
	}
	if (msg_hdr->num_pls == 0) {
		dev_err(dev, "RX: HW BUG? zero payload packets in message\n");
		goto error;
	}
	if (le16_to_cpu(msg_hdr->num_pls) > I2400M_MAX_PLS_IN_MSG) {
		dev_err(dev, "RX: HW BUG? message contains more payload "
			"than maximum; ignoring.\n");
		goto error;
	}
	result = 0;
error:
	return result;
}


/*
 * Check a payload descriptor
 *
 * @i2400m: device descriptor
 * @pld: payload descriptor
 * @pl_itr: offset (in bytes) in the received buffer the payload is
 *          located
 * @buf_size: size of the received buffer
 *
 * Given a payload descriptor part of a RX buffer, check it is sane
 * and that the data it declares fits in the buffer.
 */
static
int i2400m_rx_pl_descr_check(struct i2400m *i2400m,
			      const struct i2400m_pld *pld,
			      size_t pl_itr, size_t buf_size)
{
	int result = -EIO;
	struct device *dev = i2400m_dev(i2400m);
	size_t pl_size = i2400m_pld_size(pld);
	enum i2400m_pt pl_type = i2400m_pld_type(pld);

	if (pl_size > I2400M_PL_SIZE_MAX) {
		dev_err(dev, "RX: HW BUG? payload @%zu: size %u is "
			"bigger than maximum %u; ignoring message\n",
			pl_itr, pl_size, I2400M_PL_SIZE_MAX);
		goto error;
	}
	if (pl_itr + pl_size > buf_size) {	/* enough? */
		dev_err(dev, "RX: HW BUG? payload @%zu: size %zu "
			"goes beyond the received buffer "
			"size (%zu bytes); ignoring message\n",
			pl_itr, pl_size, buf_size);
		goto error;
	}
	if (pl_type >= I2400M_PT_ILLEGAL) {
		dev_err(dev, "RX: HW BUG? illegal payload type %u; "
			"ignoring message\n", pl_type);
		goto error;
	}
	result = 0;
error:
	return result;
}


/**
 * i2400m_rx - Receive a buffer of data from the device
 *
 * @i2400m: device descriptor
 * @skb: skbuff where the data has been received
 *
 * Parse in a buffer of data that contains a message sent from the
 * device. See the file header for the format. Run all checks on the
 * buffer header, then run over each payload's descriptors, verify
 * their consistency and act on each payload's contents.
 * If everything is succesful, update the device's statistics.
 *
 * Note: You need to set the skb to contain only the length of the
 * received buffer; for that, use skb_trim(skb, RECEIVED_SIZE).
 */
int i2400m_rx(struct i2400m *i2400m, struct sk_buff *skb)
{
	int i, result;
	struct device *dev = i2400m_dev(i2400m);
	const struct i2400m_msg_hdr *msg_hdr;
	size_t pl_itr, pl_size;
	unsigned long flags;

	d_fnstart(4, dev, "(i2400m %p skb %p [size %zu])\n",
		  i2400m, skb, skb->len);
	result = -EIO;
	msg_hdr = (void *) skb->data;
	result = i2400m_rx_msg_hdr_check(i2400m, msg_hdr, skb->len);
	if (result < 0)
		goto error_msg_hdr_check;

	pl_itr = sizeof(*msg_hdr) +	/* Check payload descriptor(s) */
		le16_to_cpu(msg_hdr->num_pls) * sizeof(msg_hdr->pld[0]);
	pl_itr = ALIGN(pl_itr, I2400M_PL_PAD);
	if (pl_itr > skb->len) {	/* got all the payload descriptors? */
		dev_err(dev, "RX: HW BUG? message too short (%zu bytes) for "
			"%u payload descriptors (%zu each, total %u)\n",
			skb->len, le16_to_cpu(msg_hdr->num_pls),
			sizeof(msg_hdr->pld[0]), pl_itr);
		goto error_pl_descr_short;
	}
	/* Walk each payload payload--check we really got it */
	for (i = 0; i < le16_to_cpu(msg_hdr->num_pls); i++) {
		/* work around old gcc warnings */
		pl_size = i2400m_pld_size(&msg_hdr->pld[i]);
		result = i2400m_rx_pl_descr_check(i2400m, &msg_hdr->pld[i],
						      pl_itr, skb->len);
		if (result < 0)
			goto error_pl_descr_check;
		i2400m_rx_payload(i2400m, skb, &msg_hdr->pld[i],
				  skb->data + pl_itr);
		pl_itr += ALIGN(pl_size, I2400M_PL_PAD);
	}
	/* Update device statistics */
	spin_lock_irqsave(&i2400m->rx_lock, flags);
	i2400m->rx_pl_num += i;
	if (i > i2400m->rx_pl_max)
		i2400m->rx_pl_max = i;
	if (i < i2400m->rx_pl_min)
		i2400m->rx_pl_min = i;
	i2400m->rx_num++;
	i2400m->rx_size_acc += skb->len;
	if (skb->len < i2400m->rx_size_min)
		i2400m->rx_size_min = skb->len;
	if (skb->len > i2400m->rx_size_max)
		i2400m->rx_size_max = skb->len;
	spin_unlock_irqrestore(&i2400m->rx_lock, flags);
error_pl_descr_check:
error_pl_descr_short:
error_msg_hdr_check:
	d_fnend(4, dev, "(i2400m %p skb %p [size %zu]) = %d\n",
		i2400m, skb, skb->len, result);
	return result;
}
EXPORT_SYMBOL_GPL(i2400m_rx);
