/* -*-linux-c-*-
 *
 * Intel Wireless WiMax Connection 2400m
 * TX handling
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
#include <linux/if_arp.h>
#include <linux/netdevice.h>

#ifdef __USE_LEGACY_IOCTL
#include "besor_legacy.h"
#endif /* __USE_LEGACY_IOCTL */

#include "i2400m.h"
#define D_LOCAL 1
#include "../debug.h"

/**
 * enum - tx subsystem state
 */
enum {
	I2400M_TX_IDLE = 0,
	I2400M_TX_BUSY
};


/**
 * i2400m_fill_submit_tx_urb - utility, prepare and submit a urb
 *
 * @urb: completed urb to handle
 * @flags: flags for usb_submit_urb()
 * @buf: I/O buffer
 * @buf_len: size of the I/O buffer
 * @callback: the completion routine for this urb
 */
int i2400m_fill_submit_tx_urb(struct i2400m *i2400m, struct urb *urb,
			      unsigned long flags, void *buf,
			      size_t buf_len,
			      void (*handler) (struct urb *))
{
	int usb_pipe, ret = 0;
	struct usb_endpoint_descriptor *epd;
	struct device *dev = &i2400m->usb_iface->dev;

	d_fnstart(4, dev, "\n");
	epd = &i2400m->usb_iface->cur_altsetting->endpoint[I2400M_EP_BULK_OUT].
		desc;
	usb_pipe = usb_sndbulkpipe(i2400m->usb_dev, epd->bEndpointAddress);
	usb_fill_bulk_urb(urb, i2400m->usb_dev, usb_pipe, buf, buf_len, handler,
			  i2400m);
	ret = usb_submit_urb(urb, flags);
	if (ret != 0)
		dev_err(dev, "submit TX urb failed\n");
	d_fnend(4, dev, "returning %d\n", ret);
	return ret;
}


static int i2400m_append_to_message(struct i2400m *i2400m, const void *buf,
				    size_t buf_len, int pkt_type)
{
	int i, ret = 0;
	struct device *dev = &i2400m->usb_iface->dev;
	struct i2400m_msg_preview *mp;
	union i2400m_device_pkt_hdr *pkt;
	size_t offset = 0, pkt_size;

	d_fnstart(4, dev, "buf=%p, buflen = %d, type=%d\n", buf, buf_len,
		  pkt_type);
	mp = (struct i2400m_msg_preview *) i2400m->tx_data->msg_buf;
	d_printf(4, dev, "mp = %p\n", mp);
	/* append the payload to the rest of the payloads */
	for (i = 0; i < mp->msg_hdr.num_pkts; i++) {	/* get the offset */
		pkt_size = mp->pkts[i].size;
		offset += ALIGN(pkt_size, I2400M_PKT_PAD);
	}
	d_printf(4, dev, "offset = %d\n", offset);
	if (I2400M_MAX_MSG_LEN - offset < buf_len) {	/* no space - discard */
		dev_err(dev, "message is full (%d < %d\n",
			I2400M_MAX_MSG_LEN - offset, buf_len);
		ret = -ENOSPC;
		goto out;
	}
	d_printf(4, dev, "copying data\n");
	memcpy(i2400m->tx_data->msg_buf + sizeof(*mp) + offset, buf, buf_len);
	d_printf(4, dev, "copied data\n");
	pkt = &mp->pkts[mp->msg_hdr.num_pkts];
	pkt->size = buf_len;
	pkt->type = pkt_type;
	mp->msg_hdr.num_pkts++;
out:
	d_fnend(4, dev, "returning %d\n", ret);
	return ret;
}

static void i2400m_prepare_tx_message(struct i2400m *i2400m)
{
	struct device *dev = &i2400m->usb_iface->dev;
	size_t preview_size, offset = 0, xfer_size, pkt_size;
	struct i2400m_msg_preview *mp;
	int i;

	d_fnstart(4, dev, "\n");
	mp = (struct i2400m_msg_preview *) i2400m->tx_data->msg_buf;
	preview_size = ALIGN(sizeof(mp->msg_hdr) + mp->msg_hdr.num_pkts *
			     sizeof(union i2400m_device_pkt_hdr),
			     I2400M_PKT_PAD);
	d_printf(4, dev, "preview size = %d, num_pkts=%d\n", preview_size,
		 mp->msg_hdr.num_pkts);
	xfer_size = preview_size;
	for (i = 0; i < mp->msg_hdr.num_pkts; i++) {	/* get the offset */
		pkt_size = mp->pkts[i].size;
		xfer_size += ALIGN(pkt_size, I2400M_PKT_PAD);
		/* and adjust packet header */
		mp->pkts[i].value = cpu_to_le32(mp->pkts[i].value);
	}
	mp->msg_hdr.num_pkts = cpu_to_le16(mp->msg_hdr.num_pkts);
	d_printf(4, dev, "xfer size = %d\n", xfer_size);
	offset = sizeof(*mp) - preview_size;
	i2400m->tx_data->msg_start = i2400m->tx_data->msg_buf + offset;
	memmove(i2400m->tx_data->msg_start, i2400m->tx_data->msg_buf,
		preview_size);
	mp->msg_hdr.padding = 0;
	i2400m->tx_data->msg_len = xfer_size;
	d_fnend(4, dev, "returning\n");
}

static int i2400m_setup_message(struct i2400m *i2400m, const void *buf,
				size_t buf_len, int pkt_type)
{
	int ret = 0;
	struct i2400m_msg_preview *mp;
	struct device *dev = &i2400m->usb_iface->dev;

	d_fnstart(4, dev, "\n");
	/* must be atomic - under spinlock */
	i2400m->tx_data->msg_buf = kmalloc(I2400M_MAX_MSG_LEN, GFP_ATOMIC);
	if (i2400m->tx_data->msg_buf == NULL) {
		ret = -ENOMEM;
		dev_err(dev, "message allocation failed\n");
		goto out;
	}
	mp = (struct i2400m_msg_preview *) i2400m->tx_data->msg_buf;
	memset(mp, 0, sizeof(*mp));
	mp->msg_hdr.barker = cpu_to_le32(I2400M_H2D_PREVIEW_BARKER);
	mp->msg_hdr.sequence = cpu_to_le32(i2400m->tx_data->seq_id++);
	d_printf(4, dev, "appending first packet\n");
	i2400m_append_to_message(i2400m, buf, buf_len, pkt_type);
out:
	d_fnend(4, dev, "returning %d\n", ret);
	return ret;
}


/*
 * Deferred deallocation/put of an SKB
 *
 * If the SKB has a destructor (skb->destructor), then the kernel will
 * complain if the put/kfree op happens in IRQ context; so to be clean
 * and (I guess) safe, we provide a method to execute the op in
 * process space via a workqueue.
 *
 * We only invoke this method if there is a destructor in the SKB,
 * which seems to be the case (for now) only with generic netlink
 * guys.
 */
struct i2400m_kfree_skb {
	struct work_struct ws;
	struct sk_buff *skb;
};

static
void __i2400m_kfree_skb(struct work_struct *ws)
{
	struct i2400m_kfree_skb *k =
		container_of(ws, struct i2400m_kfree_skb, ws);
	dev_kfree_skb(k->skb);
	kfree(k);
}

static
void __i2400m_kfree_skb_deferred(struct i2400m *i2400m, struct sk_buff *skb)
{
	struct device *dev = &i2400m->usb_iface->dev;
	struct i2400m_kfree_skb *k;
	k = kzalloc(sizeof(*k), GFP_ATOMIC);
	if (k == NULL) {
		dev_err(dev, "TX: cannot allocate work struct to free skb %p "
			"in process context (-ENOMEM); skb will leak\n", skb);
		return;
	}
	INIT_WORK(&k->ws, __i2400m_kfree_skb);
	k->skb = skb;
	schedule_work(&k->ws);
}


/**
 * i2400m_write_cb - handle end of TX operation
 *
 * @urb: completed urb to handle
 */
static void i2400m_write_cb(struct urb *urb)
{
	unsigned long flags;
	int ret = 0;
	struct i2400m *i2400m = urb->context;
	struct device *dev = &i2400m->usb_iface->dev;
	struct sk_buff *skb;

	d_fnstart(4, dev, "(i2400m %p urb %p)\n", i2400m, urb);
	spin_lock_irqsave(&i2400m->tx_data->lock, flags);
	skb = i2400m->tx_data->skb;
	if (skb) {
		if (skb->destructor)
			__i2400m_kfree_skb_deferred(i2400m, skb);
		else
			dev_kfree_skb(skb);
		i2400m->tx_data->skb = NULL;
	}
	if (i2400m->tx_data->submitted_buf) {
		kfree(i2400m->tx_data->submitted_buf);
		i2400m->tx_data->submitted_buf = NULL;
	}
	if (i2400m->tx_data->msg_buf != NULL) {
		i2400m_prepare_tx_message(i2400m);
		i2400m->tx_data->submitted_buf = i2400m->tx_data->msg_buf;
		ret = i2400m_fill_submit_tx_urb(i2400m, i2400m->tx_data->urb,
						GFP_ATOMIC,
						i2400m->tx_data->msg_start,
						i2400m->tx_data->msg_len,
						i2400m_write_cb);
		i2400m->tx_data->msg_buf = NULL;
	}
	else {
		i2400m->tx_data->state = I2400M_TX_IDLE;
	}
	spin_unlock_irqrestore(&i2400m->tx_data->lock, flags);
	if (ret != 0)
		dev_err(dev, "error submitting the pending message\n");
	d_fnend(4, dev, "(i2400m %p urb %p) = void\n", i2400m, urb);
}

/**
 * i2400m_write_async - initiate a USB write operation to the bulk_out endpoint
 *
 * @i2400m: device descriptor
 * @buf: buffer to be written to the device
 * @buf_len: length in bytes of the buffer
 * @pkt_type: type of the packet
 * 
 * returns 0 on success, error code otherwise
 */
int i2400m_write_async(struct i2400m *i2400m, struct sk_buff *skb,
		       const void *buf, size_t buf_len,
		       enum i2400m_pt pkt_type)
{
	long flags;
	int ret = 0;
	struct device *dev = &i2400m->usb_iface->dev;

	if (skb) {
		buf = skb->data;
		buf_len = skb->len;
	}
	d_fnstart(3, dev, "(i2400m %p skb %p buf %p buf_len %zu pkt_type %d)\n",
		  i2400m, skb, buf, buf_len, pkt_type);
	spin_lock_irqsave(&i2400m->tx_data->lock, flags);
	if (i2400m->tx_data->msg_buf == NULL) {
		ret = i2400m_setup_message(i2400m, buf, buf_len, pkt_type);
		if (ret != 0) {
			dev_err(dev, "failed message setup (%d)\n", ret);
			goto out;
		}
	}
	else {
		ret = i2400m_append_to_message(i2400m, buf, buf_len, pkt_type);
		if (ret != 0) {		/* message is full */
			dev_err(dev, "failed appending - packet dropped\n");
			goto out;
		}
	}
	if (i2400m->tx_data->state == I2400M_TX_IDLE) {
		i2400m_prepare_tx_message(i2400m);
		i2400m->tx_data->skb = skb;
		i2400m->tx_data->submitted_buf = i2400m->tx_data->msg_buf;
		i2400m->tx_data->msg_buf = NULL;
		i2400m->tx_data->state = I2400M_TX_BUSY;
		d_printf(4, dev, "sending message - length=%d\n",
			 i2400m->tx_data->msg_len);
		ret = i2400m_fill_submit_tx_urb(i2400m, i2400m->tx_data->urb,
						GFP_ATOMIC,
						i2400m->tx_data->msg_start,
						i2400m->tx_data->msg_len,
						i2400m_write_cb);
	}
	else {
		dev_kfree_skb(skb);
	}
out:
	spin_unlock_irqrestore(&i2400m->tx_data->lock, flags);
	d_fnend(3, dev, "(i2400m %p skb %p buf %p buf_len %zu pkt_type %d) "
		"= %d\n", i2400m, skb, buf, buf_len, pkt_type, ret);
	return ret;
}

/**
 * i2400m_tx_setup - setup the tx subsystem's resources
 *
 * @i2400m: device descriptor
 */
int i2400m_tx_setup(struct i2400m *i2400m)
{
	int ret = 0;
	struct device *dev = &i2400m->usb_iface->dev;

	d_fnstart(4, dev, "\n");
	i2400m->tx_data = kzalloc(sizeof(*(i2400m->tx_data)), GFP_KERNEL);
	i2400m->tx_data->state = I2400M_TX_IDLE;
	spin_lock_init(&i2400m->tx_data->lock);
	i2400m->tx_data->urb = usb_alloc_urb(0, GFP_KERNEL);
	if (i2400m->tx_data->urb == NULL) {
		ret = -ENOMEM;
		dev_err(dev, "failed allocating tx urb\n");
	}
	d_fnend(4, dev, "returning %d\n", ret);
	return ret;
}

/**
 * i2400m_tx_release - release the tx subsystem's resources
 *
 * @i2400m: device descriptor
 */
void i2400m_tx_release(struct i2400m *i2400m)
{
	struct device *dev = &i2400m->usb_iface->dev;

	d_fnstart(4, dev, "\n");
	usb_free_urb(i2400m->tx_data->urb);
	kfree(i2400m->tx_data);
	i2400m->tx_data = NULL;
	d_fnend(4, dev, "\n");
}

