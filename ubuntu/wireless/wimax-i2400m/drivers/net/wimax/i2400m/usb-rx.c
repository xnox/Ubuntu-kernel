/* -*-linux-c-*-
 *
 * Intel Wireless WiMAX Connection 2400m
 * USB RX handling
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
 * This handles the RX path on USB.
 *
 * When a notification is received that says 'there is RX data ready',
 * we call i2400mu_bulk_in_submit() which reads a buffer from USB
 * and passes it to i2400m_rx() in the generic handling code. The RX
 * buffer has an specific format that is described in rx.c.
 *
 * We pack a 'read context' (struct i2400mu_rx_ctx) for the following
 * reasons:
 *
 *  - We might get a lot of notifications and we don't want to submit
 *    a zillion reads, we throtle the amount of pending read
 *    requests (i2400m->rx_count, i2400m->rx_pending).
 *
 *  - RX data processing can get heavy enough so that it is not
 *    appropiate for doing it in the USB callback and we defer it to a
 *    work queue (i2400mu_rx_ctx->ws).
 *
 *  - We can't use USB anchors to cancel ongoing transactions right
 *    away because I need to keep compatibility with kernels as early
 *    as 2.6.22, so we have some code that does basically the same. It
 *    will go away.
 *
 * We provide a read buffer of an arbitrary size (short of a page);
 * if the callback reports -EOVERFLOW, it means it was too small, so
 * we just double the size and retry (being careful to append, as
 * sometimes the device provided some data). Most of the times the
 * buffer should be not needing readjustment.
 *
 * ROADMAP
 *
 * i2400mu_bulk_in_submit()	- called from notif.c when we get a
 *   i2400mu_rx_ctx_alloc()         'data ready' notification
 *   i2400mu_rx_ctx_setup()
 *   i2400mu_rx_ctx_submit()     - continues in i2400mu_bulk_in_cb()
 *   ...
 *
 * i2400mu_rx_release()
 *
 * i2400mu_bulk_in_cb()
 *   i2400mu_bulk_in_work()      - scheduled in work queue
 *     i2400mu_rx()              - From here on, transport-independent
 *       ...
 *   i2400mu_rx_ctx_submit()     - if there are transactions pending
 *   i2400mu_rx_ctx_destroy()
 *
 * i2400mu_rx_release()
 */
#include <linux/workqueue.h>
#include <linux/usb.h>
#include <linux/usb-compat.h>
#include "i2400m-usb.h"


#define D_SUBMODULE rx
#include "usb-debug-levels.h"


enum {
	I2400MU_RX_CTX_MAX = 5
};


/*
 * i2400mu_rx_ctx - data reception context
 *
 * In here we pack all the stuff we need to handle a bunch of received
 * payloads. See file header for a description.
 *
 * We need to do deferred processing, as we are going to do some
 * muching that is not that ok for an interrupt context, so we'll
 * defer to a worqueue.
 *
 * We do some throtling here as if the device keeps asking us to
 * receive data we might go and queue too many reads; we allow a
 * maximum of I2400MU_RX_CTX_MAX requests pending;
 * i2400mu_bulk_in_submit(), i2400mu_rx_ctx_setup() and
 * i2400mu_rx_ctx_destroy() handle that. If the current count is over
 * the max, we don't submit another one. When we are done processing a
 * read, if there is need for more (pending reads) we resubmit it.
 *
 * The read buffer we place it in an skb, that way we can get the
 * different payloads in the RXed buffer, clone an skb pointing to
 * them and feed it to the networking stack; when done, netdev will
 * cleanly release everything for us and we don't have to copy around.
 *
 * NOTE: It is safe to keep a seemingly not referenced pointer to
 *       i2400m in the rx_ctx struct, because we take one to the
 *       usb_iface the device is attached to in
 *       i2400mu_rx_ctx_setup().
 *
 * @list_node: lame, but I can't yet use anchors because I need this
 *             driver to work on 2.6.22...FIXME: move to anchors
 *             eventually.
 */
struct i2400mu_rx_ctx {
	struct i2400mu *i2400mu;
	struct work_struct ws;
	struct urb urb;
	struct sk_buff *skb;
	struct list_head list_node;
};

static
struct i2400mu_rx_ctx *i2400mu_rx_ctx_alloc(struct i2400m *i2400m,
						  gfp_t gfp)
{
	struct i2400mu_rx_ctx *rx_ctx;
	struct device *dev = i2400m_dev(i2400m);

	rx_ctx = kzalloc(sizeof(*rx_ctx), gfp);
	if (rx_ctx == NULL)
		dev_err(dev, "RX: cannot allocate context\n");
	return rx_ctx;
};

static
void i2400mu_rx_ctx_setup(struct i2400mu *i2400mu,
			  struct i2400mu_rx_ctx *ctx)
{
	/* INIT_WORK() is done when arming the work */
	usb_init_urb(&ctx->urb);
	usb_get_urb(&ctx->urb);	/* we manage the URB life cycles */
	usb_get_intf(i2400mu->usb_iface);
	ctx->i2400mu = i2400mu;	/* See note about refcount */
	atomic_inc(&i2400mu->rx_ctx_count);
};

static
void i2400mu_rx_ctx_destroy(struct i2400mu_rx_ctx *ctx)
{
	struct i2400mu *i2400mu = ctx->i2400mu;
	kfree_skb(ctx->skb);
	atomic_dec(&i2400mu->rx_ctx_count);
	usb_put_intf(i2400mu->usb_iface);
	kfree(ctx);
};


static void i2400m_bulk_in_cb(struct urb *urb);

static
void i2400mu_rx_ctx_deanchor(struct i2400mu_rx_ctx *ctx)
{
	struct i2400mu *i2400mu = ctx->i2400mu;
	struct i2400m *i2400m = &i2400mu->i2400m;
	unsigned long flags;

	spin_lock_irqsave(&i2400m->rx_lock, flags);
	list_del(&ctx->list_node);
	if (list_empty(&i2400mu->rx_list))
		wake_up(&i2400mu->rx_list_wq);
	spin_unlock_irqrestore(&i2400m->rx_lock, flags);
}


static
void __i2400mu_rx_ctx_prep(struct i2400mu_rx_ctx *rx_ctx,
			   void *ptr, size_t size)
{
	struct i2400mu *i2400mu = rx_ctx->i2400mu;
	int usb_pipe;
	struct usb_endpoint_descriptor *epd;

	epd = usb_get_epd(i2400mu->usb_iface, I2400M_EP_BULK_IN);
	usb_pipe = usb_rcvbulkpipe(i2400mu->usb_dev, epd->bEndpointAddress);
	usb_fill_bulk_urb(&rx_ctx->urb, i2400mu->usb_dev, usb_pipe,
			  ptr, size, i2400m_bulk_in_cb, rx_ctx);
}


/*
 * Submit an RX context
 *
 * NOTE: we allocate the skb here because if we recycle the rx_ctx
 * (resubmit it) the previous skb's data buffer might be used by
 * others (via skb_clone).
 */
static
int i2400mu_rx_ctx_submit(struct i2400mu_rx_ctx *rx_ctx, gfp_t gfp_flags)
{
	int ret;
	struct i2400mu *i2400mu = rx_ctx->i2400mu;
	struct i2400m *i2400m = &i2400mu->i2400m;
	struct net_device *net_dev = i2400m->wimax_dev.net_dev;
	struct device *dev = &i2400mu->usb_iface->dev;
	unsigned long flags;
	size_t rx_size = atomic_read(&i2400mu->rx_size);

	d_fnstart(3, dev, "(rx_ctx %p)\n", rx_ctx);

	ret = -ENOMEM;
	kfree_skb(rx_ctx->skb);	/* We are going to overwrite it */
	rx_ctx->skb = __netdev_alloc_skb(net_dev, rx_size, gfp_flags);
	if (rx_ctx == NULL)
		goto error_skb_alloc;
	__i2400mu_rx_ctx_prep(rx_ctx, rx_ctx->skb->data, rx_size);
	spin_lock_irqsave(&i2400m->rx_lock, flags);
	list_add(&rx_ctx->list_node, &i2400mu->rx_list);
	spin_unlock_irqrestore(&i2400m->rx_lock, flags);
	ret = usb_submit_urb(&rx_ctx->urb, gfp_flags);
	if (ret != 0) {
		dev_err(dev, "RX: cannot submit urb: %d\n", ret);
		i2400mu_rx_ctx_deanchor(rx_ctx);
	}
error_skb_alloc:
	d_fnend(3, dev, "(rx_ctx %p) = %d\n", rx_ctx, ret);
	return ret;
}


/**
 * i2400m_bulk_in_work - 2nd stage incoming data handler
 *
 * @ws: pointer to the workstruct that is contained inside an &struct
 *      i2400mu_rx_ctx which contains the data we need
 *
 * Call for processing of a received buffer and then maybe resubmit
 * the USB rx context if there is stuff pending for read.
 */
static void i2400m_bulk_in_work(struct work_struct *ws)
{
	int result;
	struct i2400mu_rx_ctx *rx_ctx =
		container_of(ws, struct i2400mu_rx_ctx, ws);
	struct urb *in_urb = &rx_ctx->urb;
	struct i2400mu *i2400mu = rx_ctx->i2400mu;
	struct i2400m *i2400m = &i2400mu->i2400m;
	struct device *dev = &i2400mu->usb_iface->dev;
	size_t pending;

	d_fnstart(4, dev, "(urb %p [%p])\n", in_urb, in_urb->transfer_buffer);
	result = i2400m_rx(i2400m, rx_ctx->skb);
	if (result < -EIO
	    && edc_inc(&i2400mu->urb_edc,
		       EDC_MAX_ERRORS, EDC_ERROR_TIMEFRAME)) {
		dev_err(dev, "RX: maximum errors in received buffer exceeded; "
			"resetting device\n");
		i2400mu_rx_ctx_destroy(rx_ctx);
		usb_dev_reset_delayed(i2400mu->usb_dev);
		return;
	}
	pending = atomic_dec_return(&i2400mu->rx_pending_count);
	d_printf(3, dev, "RX: work pending %u active %u\n",
		 pending, atomic_read(&i2400mu->rx_ctx_count));
	if (pending >= atomic_read(&i2400mu->rx_ctx_count)) {
		d_printf(1, dev, "I: recycling rx_ctx %p "
			 "(%u pending %u active)\n", rx_ctx, pending,
			 atomic_read(&i2400mu->rx_ctx_count));
		result = i2400mu_rx_ctx_submit(rx_ctx, GFP_KERNEL);
		if (result < 0)
			dev_err(dev, "RX: can't submit URB, we'll miss data: "
				"%d\n", result);
	} else
		i2400mu_rx_ctx_destroy(rx_ctx);
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
	struct i2400mu_rx_ctx *rx_ctx =
		container_of(urb, struct i2400mu_rx_ctx, urb);
	struct i2400mu *i2400mu = rx_ctx->i2400mu;
	struct i2400m *i2400m = &i2400mu->i2400m;
	struct net_device *net_dev = i2400m->wimax_dev.net_dev;
	struct device *dev = &i2400mu->usb_iface->dev;

	d_fnstart(4, dev, "(urb %p [i2400mu %p ctx %p] status %d length %d)\n",
		  urb, i2400mu, rx_ctx, urb->status, urb->actual_length);
	ret = urb->status;
	switch (ret) {
	case 0:
		if (urb->actual_length == 0)	/* ZLP? resubmit */
			goto resubmit;
		i2400mu_rx_ctx_deanchor(rx_ctx);
		skb_put(rx_ctx->skb, urb->actual_length);
		INIT_WORK(&rx_ctx->ws, i2400m_bulk_in_work);
		d_printf(4, dev, "RX: %d (%d total) bytes scheduled for "
			 "rx_ctx %p\n", urb->actual_length, rx_ctx->skb->len,
			 rx_ctx);
		queue_work(i2400mu->i2400m.work_queue, &rx_ctx->ws);
		break;
		/* Too small a buffer for the data; double the size
		 * from now on, reallocate and append (we got partial
		 * data). Should happen very very seldomly and yes,
		 * the code is ugly. If we get two reallocations in a
		 * row, we need to copy everything, so use the
		 * previous skb as a reference, not just the URB.
		 */
	case -EOVERFLOW: {
		size_t rx_size;
		struct sk_buff *skb;
		rx_size = 2 * atomic_read(&i2400mu->rx_size);
		if (rx_size <= (1 << 16))	/* cap it */
			atomic_set(&i2400mu->rx_size, rx_size);
		else if (printk_ratelimit()) {
			dev_err(dev, "BUG? rx_size up to %d\n", rx_size);
			goto drop_rx_ctx;
		}
		skb = netdev_alloc_skb(net_dev, rx_size);
		if (skb == NULL) {
			if (printk_ratelimit())
				dev_err(dev, "RX: Can't reallocate skb to %d; "
					"RX dropped\n", rx_size);
			ret = -ENOMEM;
			goto drop_rx_ctx;
		}
		skb_put(rx_ctx->skb, urb->actual_length);
		skb_put(skb, rx_ctx->skb->len);
		memcpy(skb->data, rx_ctx->skb->data, rx_ctx->skb->len);
		d_printf(4, dev, "RX: size changed to %d, received %d, "
			 "copying %d, capacity %d\n",
			 rx_size, urb->actual_length, skb->len,
			 skb_end_pointer(skb) - skb->head);
		kfree_skb(rx_ctx->skb);
		__i2400mu_rx_ctx_prep(rx_ctx, skb->data + skb->len,
				     rx_size - skb->len);
		rx_ctx->skb = skb;
		goto resubmit;
	}
	case -ECONNRESET:		/* disconnection */
	case -ENOENT:			/* ditto */
	case -ESHUTDOWN:		/* URB killed */
		d_printf(2, dev, "RX: URB failed, device disconnected: %d\n",
			 ret);
		goto drop_rx_ctx;
	default:			/* Some error? */
		if (edc_inc(&i2400mu->urb_edc,
			    EDC_MAX_ERRORS, EDC_ERROR_TIMEFRAME)) {
			dev_err(dev, "RX: maximum errors in URB exceeded; "
				"resetting device\n");
			usb_dev_reset_delayed(i2400mu->usb_dev);
			goto drop_rx_ctx;
		}
		dev_err(dev, "RX: URB error %d, retrying\n", urb->status);
		goto resubmit;
	}
	usb_mark_last_busy(i2400mu->usb_dev);
out:
	d_fnend(4, dev, "(urb %p [i2400mu %p ctx %p] status %d length %d)\n",
		urb, i2400mu, rx_ctx, urb->status, urb->actual_length);
	return;

resubmit:
	usb_mark_last_busy(i2400mu->usb_dev);
	ret = usb_submit_urb(&rx_ctx->urb, GFP_ATOMIC);
	if (ret == 0)
		goto out;
	dev_err(dev, "RX: cannot submit notification URB: %d\n", ret);
drop_rx_ctx:
	i2400mu_rx_ctx_deanchor(rx_ctx);
	i2400mu_rx_ctx_destroy(rx_ctx);
	goto out;
}


/*
 * Read a message from the device
 *
 * @i2400m: device instance
 * @gfp_flags: GFP flags to use
 *
 * When the message is read the callback will be called and the
 * message processing will be started. We call this function when we
 * get a message in the notification endpoint that is 16 bytes of
 * zeroes.
 *
 * We defer processing because we do a bit of heavy allocation, we
 * might want to consider this further in the future.
 */
int i2400m_bulk_in_submit(struct i2400mu *i2400mu, gfp_t gfp_flags)
{
	int ret, rx_ctx_count;
	struct i2400m *i2400m = &i2400mu->i2400m;
	struct device *dev = &i2400mu->usb_iface->dev;
	struct i2400mu_rx_ctx *rx_ctx;

	d_fnstart(3, dev, "(i2400m %p gfp_flags 0x%08x)\n", i2400m, gfp_flags);

	atomic_inc(&i2400mu->rx_pending_count);
	rx_ctx_count = atomic_read(&i2400mu->rx_ctx_count);
	d_printf(3, dev, "RX: pending %u active %u\n",
		 atomic_read(&i2400mu->rx_pending_count), rx_ctx_count);
	if (rx_ctx_count > I2400MU_RX_CTX_MAX) {
		ret = 0;
		d_printf(1, dev, "RX: throttling down read requests, "
			 "%u pending\n", rx_ctx_count);
		goto error_too_many;
	}
	ret = -ENOMEM;
	rx_ctx = i2400mu_rx_ctx_alloc(i2400m, gfp_flags);
	if (rx_ctx == NULL)
		goto error_ctx_alloc;
	i2400mu_rx_ctx_setup(i2400mu, rx_ctx);
	ret = i2400mu_rx_ctx_submit(rx_ctx, gfp_flags);
	if (ret != 0)
		goto error_urb_submit;
	d_fnend(3, dev, "(i2400m %p gfp_flags 0x%08x) = %d\n",
		i2400m, gfp_flags, ret);
	return ret;

error_urb_submit:
	i2400mu_rx_ctx_destroy(rx_ctx);
error_ctx_alloc:
error_too_many:
	d_fnend(3, dev, "(i2400m %p gfp_flags 0x%08x) = %d\n",
		i2400m, gfp_flags, ret);
	return ret;
}


/*
 * IMPORTANT:
 *
 * Because it can happen that we call i2400m_release() twice (device
 * resets, we _release() to clean state, i2400m_setup() fails, we
 * reset, i2400m_disconnect() is called), all the i2400m_*_release()
 * functions have to double check that they haven't done their job
 * before.
 */
void i2400mu_rx_release(struct i2400mu *i2400mu)
{
	struct device *dev = &i2400mu->usb_iface->dev;
	unsigned long flags;
	struct i2400mu_rx_ctx *rx_ctx, *nxt;
	struct i2400m *i2400m = &i2400mu->i2400m;

	d_fnstart(4, dev, "(i2400m %pu)\n", i2400mu);
	spin_lock_irqsave(&i2400m->rx_lock, flags);
	list_for_each_entry_safe(rx_ctx, nxt, &i2400mu->rx_list, list_node) {
		spin_unlock_irqrestore(&i2400m->rx_lock, flags);
		usb_kill_urb(&rx_ctx->urb);
		spin_lock_irqsave(&i2400m->rx_lock, flags);
	}
	spin_unlock_irqrestore(&i2400m->rx_lock, flags);
	wait_event(i2400mu->rx_list_wq, list_empty(&i2400mu->rx_list));
	/* the i2400m->work_queue has been destroyed already when we
	 * get here, so nothing should be queued on it */
	d_fnend(4, dev, "(i2400m %pu) = void\n", i2400mu);
}
