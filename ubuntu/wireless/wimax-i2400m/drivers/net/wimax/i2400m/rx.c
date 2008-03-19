/* -*-linux-c-*-
 *
 * Intel Wireless WiMax Connection 2400m
 * RX handling
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
 * Inaky Perez-Gonzalez <inaky.perez-gonzalez@intel.com>
 *  - Initial implementation
 *
 *
 * FIXME: docs
 */
#include "../version.h"
#include <linux/kernel.h>
#include <linux/kernel-compat.h>	/* @lket@ignore-line */
#include <linux/if_arp.h>
#include <linux/netdevice.h>
#include <linux/workqueue.h>
#include <linux/usb.h>
#include <linux/usb-compat.h>

#ifdef __USE_LEGACY_IOCTL
#include "besor_legacy.h"
#endif /* __USE_LEGACY_IOCTL */

#include "i2400m.h"
#define D_LOCAL 1
#include "../debug.h"


enum {
	I2400M_RX_CTX_MAX = 5
};


/*
 * i2400m_rx_ctx - data reception context
 *
 * We need to do deferred processing, as we are going to do some
 * muching that is not that ok for an interrupt context, so we'll
 * defer to a worqueue.
 *
 * We do some throtling here as if the device keeps asking us to
 * receive data we might go and queue too many reads; we allow a
 * maximum of I2400M_RX_CTX_MAX requests pending;
 * i2400m_rx_ctx_setup() and i2400m_rx_ctx_destroy() handle that. If
 * the current count is over the max, we don't submit another
 * one. FIXME: this will go away once we just preallocate a bunch of
 * contexts and limit ourselves to use only those.
 *
 * So instead of allocating three different items (urb, buffer,
 * workqueue struct), we pack it up in a single one. This simplifies
 * refcount management.
 *
 * In here we pack all the stuff we need to handle a bunch of received
 * packets.
 *
 * NOTE: It is safe to keep a seemingly not referenced pointer to
 *       i2400m in the rx_ctx struct, because we take one to the
 *       usb_iface the device is attached to in
 *       i2400m_rx_ctx_setup().
 */
#warning FIXME: these guys should be preallocated at _rx_setup() have at least two
#warning FIXME: move buffer to be a big skb we can clone later on
struct i2400m_rx_ctx {
	struct i2400m *i2400m;
	struct work_struct ws;
	struct urb urb;
#warning FIXME: too big an allocation - check if we really need this much
	unsigned char buffer[I2400M_MAX_MSG_LEN];
};

static
void i2400m_rx_ctx_setup(struct i2400m *i2400m, struct i2400m_rx_ctx *ctx)
{
	/* INIT_WORK() is done when arming the work */
	usb_init_urb(&ctx->urb);
	usb_get_urb(&ctx->urb);	/* we manage the URB life cycles */
	usb_get_intf(i2400m->usb_iface);
	ctx->i2400m = i2400m;	/* See note about refcount */
	atomic_inc(&i2400m->rx_ctx_count);
};

static
void i2400m_rx_ctx_destroy(struct i2400m_rx_ctx *ctx)
{
	struct i2400m *i2400m = ctx->i2400m;
	atomic_dec(&i2400m->rx_ctx_count);
	usb_put_intf(i2400m->usb_iface);
	kfree(ctx);
};


/**
 * i2400m_rx_packet - act on a received packet and payload
 *
 * @i2400m: device instance
 * @packet: pointer to packet descriptor
 * @payload: pointer to packet payload; the amount of available data
 *           is guaranteeed to be as much as declared in packet->size.
 *
 * Upon reception of a packet, look at its guts in the packet
 * descriptor and decide what to do with it.
 */
#warning FIXME: payload should be const
static
void i2400m_rx_packet(struct i2400m *i2400m,
		      const union i2400m_device_pkt_hdr *packet,
		      void *payload)
{
	struct device *dev = &i2400m->usb_iface->dev;
	struct wimax_dev *wimax_dev = &i2400m->wimax_dev;

	/* sanity */
	switch (packet->type) {
	case I2400M_PT_DATA:
		d_printf(2, dev, "RX: data packet %zu bytes\n", packet->size);
		i2400m_net_rx(i2400m, payload, packet->size);
		break;
	case I2400M_PT_CTRL:
#ifdef __USE_LEGACY_IOCTL	/* Ugly switch, but soon to be removed */
		if (i2400m->ctl_iface == 0) {
			d_printf(2, dev,
				 "RX: control packet [legacy] %zu bytes\n",
				packet->size);
			i2400m_legacy_ctl_cb(i2400m, payload, packet->size);
			break;
		}
#endif
		if (wimax_dev->genl_pid != 0) {
			d_printf(2, dev,
				 "RX: control packet [genl] %zu bytes\n",
				 packet->size);
			d_dump(3, dev, payload, packet->size);
#warning FIXME: should check retval and complain on error
			wimax_msg_to_user(
				wimax_dev, payload, packet->size,
				GFP_ATOMIC);
		} else {
			d_printf(2, dev, "RX: control packet [no listeners] "
				 "%zu bytes\n", packet->size);
			d_dump(3, dev, payload, packet->size);
		}
		break;
	case I2400M_PT_TRACE:
		d_printf(2, dev, "RX: trace packet %zu bytes\n", packet->size);
		d_dump(6, dev, payload, packet->size);
#warning FIXME: bubble up to userspace as a GNL trace_event
#warning "FIXME: Inaky - your trace callback here\
 - you MUST copy the buffer in the callback + callback can not sleep"
		break;
	default:	/* Anything else shouldn't come to the host */
		if (printk_ratelimit())
			dev_err(dev, "RX: HW BUG? unexpected packet type %u\n",
				packet->type);
	}
}


static
int i2400m_rx_ctx_submit(struct i2400m_rx_ctx *rx_ctx,
			   gfp_t gfp_flags)
{
	int ret;
	struct device *dev = &rx_ctx->i2400m->usb_iface->dev;

	d_fnstart(2, dev, "(rx_ctx %p)\n", rx_ctx);
	ret = usb_submit_urb(&rx_ctx->urb, gfp_flags);
	if (ret != 0)
		dev_err(dev, "RX: cannot submit urb: %d\n", ret);
	d_fnend(2, dev, "(rx_ctx %p) = %d\n", rx_ctx, ret);
	return ret;
}


/**
 * i2400_bulk_in_dpc - 2nd stage incoming data handler
 *
 * @arg: opaque - actually a pointer to the incoming bulk urb.
 *
 * dpc = deferred proc call. naming relic from that other OS.
 * called from within a tasklet, this function parses the incoming urb,
 * calls the proper higher level callbacks, and frees the urb and its buffer.
 *
 * Parse in a buffer of data that contains a message sent from the device.
 *
 * The format of the buffer is:
 *
 * HEADER                      (struct i2400m_device_msg_hdr)
 * PAYLOAD DESCRIPTOR 0        (struct i2400m_device_pkt_hdr)
 * PAYLOAD DESCRIPTOR 1
 * ...
 * PAYLOAD DESCRIPTOR N
 * PAYLOAD 0                   (raw bytes)
 * PAYLOAD 1
 * ...
 * PAYLOAD N
 *
 * Now, depending on what each packet is, we need to do one thing or
 * the other and thus we'll multiplex the call.
 *
 * FIXME: a single buffer might contain multiple payloads, used by
 *        potentially different subsystems. Devise a way to share the
 *        buffer so we don't have to copy around; maybe we can just
 *        set skb->destructor on the skb we pass up as it might
 *        do just what we need (in generic netlink we also need a
 *        skb...sooo).
 *
 * FIXME: Allocating a 64k buffer and using that as an RX buffer is
 *        not something that makes me specially proud; I bet we never
 *        really fill it. Change to have a smaller buffer (make it a
 *        floating size)--check with the FW guys if the hw will scale
 *        back payloads if the RX buffer is too small.
 */
static void i2400m_bulk_in_work(struct work_struct *ws)
{
	int i, result;
	struct i2400m_rx_ctx *rx_ctx =
		container_of(ws, struct i2400m_rx_ctx, ws);
	struct urb *in_urb = &rx_ctx->urb;
	struct i2400m *i2400m = rx_ctx->i2400m;
	struct device *dev = &i2400m->usb_iface->dev;
	const struct i2400m_msg_preview *msg;
	const struct i2400m_device_msg_hdr *msg_hdr;
	union i2400m_device_pkt_hdr packet;
#warning FIXME: payload should be const
	void *buf;
	size_t pl_itr, buf_size, pending;

	d_fnstart(4, dev, "(urb %p [%p])\n", in_urb, in_urb->transfer_buffer);
	/* Check message header */
	buf_size = in_urb->actual_length;
	if (buf_size < sizeof(*msg_hdr)) {
		dev_err(dev, "RX: HW BUG? message with short header (%zu "
			"vs %zu bytes expected)\n", buf_size, sizeof(*msg_hdr));
		goto error_short_hdr;
	}
	msg = buf = in_urb->transfer_buffer;
	msg_hdr = &msg->msg_hdr;
	if (le32_to_cpu(msg_hdr->barker) != I2400M_D2H_MSG_BARKER) {
		dev_err(dev, "RX: HW BUG? message received with unknown "
			"barker 0x%08x\n", le32_to_cpu(msg_hdr->barker));
		goto error_invalid_barker;
	}
	if (msg_hdr->num_pkts == 0) {
		dev_err(dev, "RX: HW BUG? zero payload packets in message\n");
		goto error_no_payload;
	}
	if (le16_to_cpu(msg_hdr->num_pkts) > I2400M_MAX_PACKETS_IN_MSG) {
		dev_err(dev, "RX: HW BUG? message contains more payload "
			"than maximum; ignoring.\n");
		goto error_too_many_packets;
	}
	/* Check payload descriptor(s) */
	pl_itr = sizeof(*msg_hdr) +		/* PAYLOAD0 offset */
		le16_to_cpu(msg_hdr->num_pkts) * sizeof(msg->pkts[0]);
	pl_itr = ALIGN(pl_itr, I2400M_PKT_PAD);
	if (pl_itr > buf_size) {	/* got all the payload headers? */
		dev_err(dev, "RX: HW BUG? message too short (%zu bytes) for "
			"%u payload descriptors (%zu each, total %u)\n",
			buf_size, le16_to_cpu(msg_hdr->num_pkts),
			sizeof(msg->pkts[0]), pl_itr);
		goto error_short_pl_desc;
	}
	/* Walk each payload packet--check we really got it */
	for (i = 0; i < le16_to_cpu(msg_hdr->num_pkts); i++) {
		packet.value = le32_to_cpu(msg->pkts[i].value);
		/* sanity */
#warning FIXME: rename to I2400M_MAX_PKT_SIZE?
		if (packet.size > I2400M_PKT_SIZE) {
			dev_err(dev, "RX: HW BUG? packet #%d: size %u is "
				"bigger than maximum %u; ignoring message\n",
				i, packet.size, I2400M_PKT_SIZE);
			goto error_packet_size_invalid;
		}
		if (pl_itr + packet.size > buf_size) {	/* enough? */
			dev_err(dev, "RX: HW BUG? packet #%d: size %u at "
				"offset %zu goes beyond the received buffer "
				"size (%zu bytes); ignoring message\n",
				i, packet.size, pl_itr, buf_size);
			goto error_packet_size_out_of_bounds;
		}
		if (packet.type >= I2400M_PT_ILLEGAL) {
			dev_err(dev, "RX: HW BUG? illegal packet type %u; "
				"ignoring message\n", packet.type);
			goto error_packet_type_invalid;
		}
		i2400m_rx_packet(i2400m, &packet, buf + pl_itr);
		pl_itr += ALIGN((size_t) packet.size, I2400M_PKT_PAD);
	}
error_packet_type_invalid:
error_packet_size_out_of_bounds:
error_packet_size_invalid:
error_short_pl_desc:
error_too_many_packets:
error_no_payload:
error_invalid_barker:
error_short_hdr:
#warning FIXME: handle -EIOs with EDC
#warning FIXME: split off
	pending = atomic_dec_return(&i2400m->rx_pending_count);
	d_printf(2, dev, "RX: work pending %u active %u\n",
		 pending, atomic_read(&i2400m->rx_ctx_count));
	if (pending >= atomic_read(&i2400m->rx_ctx_count)) {
		d_printf(0, dev, "I: recycling rx_ctx %p "
			 "(%u pending %u active)\n", rx_ctx, pending,
			 atomic_read(&i2400m->rx_ctx_count));
		result = i2400m_rx_ctx_submit(rx_ctx, GFP_KERNEL);
		if (result < 0)
			dev_err(dev, "RX: can't submit URB, we'll miss data: "
				"%d\n", result);
	}
	else
		i2400m_rx_ctx_destroy(rx_ctx);
	d_fnend(4, dev, "\n");
}


/**
 * i2400_bulk_in_cb - Handle USB reception and spawn workqueue
 *
 * @urb: the urb for which this function was called
 */
static void i2400m_bulk_in_cb(struct urb *urb)
{
	int ret;
	struct i2400m_rx_ctx *rx_ctx =
		container_of(urb, struct i2400m_rx_ctx, urb);
	struct i2400m *i2400m = rx_ctx->i2400m;
	struct device *dev = &i2400m->usb_iface->dev;

	d_fnstart(4, dev, "(urb %p [i2400m %p ctx %p] status %d length %d)\n",
		  urb, i2400m, rx_ctx, urb->status, urb->actual_length);
	ret = urb->status;
	switch (ret) {
	case 0:
		if (urb->actual_length == 0)	/* ZLP? resubmit */
			goto resubmit;
		INIT_WORK(&rx_ctx->ws, i2400m_bulk_in_work);
		d_printf(4, dev, "RX: work scheduled for rx_ctx %p\n", rx_ctx);
		schedule_work(&rx_ctx->ws);
		break;
	case -ECONNRESET:		/* disconnection */
	case -ENOENT:			/* ditto */
	case -ESHUTDOWN:		/* URB killed */
		d_printf(2, dev, "RX: URB failed, device disconnected: %d\n",
			 ret);
		i2400m_rx_ctx_destroy(rx_ctx);
		goto out;
	default:			/* Some error? */
		if (edc_inc(&i2400m->urb_edc,
			    EDC_MAX_ERRORS, EDC_ERROR_TIMEFRAME)) {
			dev_err(dev, "RX: maximum errors in URB exceeded; "
				"resetting device\n");
			i2400m_rx_ctx_destroy(rx_ctx);
			usb_dev_reset_delayed(i2400m->usb_dev);
			goto out;
		}
		dev_err(dev, "RX: URB error %d, retrying\n", urb->status);
		goto resubmit;
	}
out:
	d_fnend(4, dev, "(urb %p [i2400m %p ctx %p] status %d length %d)\n",
		urb, i2400m, rx_ctx, urb->status, urb->actual_length);
	return;

resubmit:
	ret = usb_submit_urb(&rx_ctx->urb, GFP_ATOMIC);
	if (ret != 0) {
		dev_err(dev, "RX: cannot submit notification URB: %d\n", ret);
		i2400m_rx_ctx_destroy(rx_ctx);
	}
	goto out;
}


/**
 * i2400m_bulk_in_submit - Read a message from the device
 *
 * @i2400m: device instance
 *
 * When the message is read the callback will be called and the
 * message processing will be started. We call this function when we
 * get a message in the notification endpoint that is 16 bytes of
 * zeroes.
 *
 * FIXME: this function is called from an atomic context, which is
 *        another reason why the hug allocations we make are not
 *        good.
 */
int i2400m_bulk_in_submit(struct i2400m *i2400m, gfp_t gfp_flags)
{
	int ret, rx_ctx_count;
	struct device *dev = &i2400m->usb_iface->dev;
	int usb_pipe;
	struct usb_endpoint_descriptor *epd;
	struct i2400m_rx_ctx *rx_ctx;

	d_fnstart(2, dev, "(i2400m %p gfp_flags 0x%08x)\n", i2400m, gfp_flags);

	atomic_inc(&i2400m->rx_pending_count);
	rx_ctx_count = atomic_read(&i2400m->rx_ctx_count);
	d_printf(2, dev, "RX: pending %u active %u\n",
		 atomic_read(&i2400m->rx_pending_count), rx_ctx_count);
	if (rx_ctx_count > I2400M_RX_CTX_MAX) {
		ret = 0;
		if (printk_ratelimit())
			dev_err(dev, "RX: throttling down read requests, "
				"%u pending\n", rx_ctx_count);
		else
			d_printf(0, dev, "RX: throttling down read requests, "
				 "%u pending\n", rx_ctx_count);
		goto error_too_many;
	}
	ret = -ENOMEM;
	rx_ctx = kzalloc(sizeof(*rx_ctx), gfp_flags);
	if (rx_ctx == NULL) {
		dev_err(dev, "RX: cannot allocate context\n");
		goto error_ctx_alloc;
	}
	i2400m_rx_ctx_setup(i2400m, rx_ctx);

	epd = &i2400m->usb_iface->cur_altsetting	/* Setup URB */
		->endpoint[I2400M_EP_BULK_IN].desc;
	usb_pipe = usb_rcvbulkpipe(i2400m->usb_dev, epd->bEndpointAddress);
	usb_fill_bulk_urb(&rx_ctx->urb, i2400m->usb_dev, usb_pipe,
			  rx_ctx->buffer, sizeof(rx_ctx->buffer),
			  i2400m_bulk_in_cb, rx_ctx);
	ret = i2400m_rx_ctx_submit(rx_ctx, gfp_flags);
	if (ret != 0)
		goto error_urb_submit;
	d_fnend(2, dev, "(i2400m %p gfp_flags 0x%08x) = %d\n",
		i2400m, gfp_flags, ret);
	return ret;

error_urb_submit:
	i2400m_rx_ctx_destroy(rx_ctx);
error_ctx_alloc:
error_too_many:
	d_fnend(2, dev, "(i2400m %p gfp_flags 0x%08x) = %d\n",
		i2400m, gfp_flags, ret);
	return ret;
}


void i2400m_rx_release(struct i2400m *i2400m) 
{
	flush_scheduled_work();	/* rx work struct handlers */
}
