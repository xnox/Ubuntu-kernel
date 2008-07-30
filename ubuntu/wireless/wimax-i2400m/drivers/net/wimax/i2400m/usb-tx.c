/* -*-linux-c-*-
 *
 * Intel Wireless WiMAX Connection 2400m
 * USB specific TX handling
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
 *  - Split transport/device specific
 *
 *
 * Takes the TX messages in the i2400m's driver TX FIFO and sends them
 * to the device until there are no more.
 *
 * If we fail sending the message, we just drop it. There isn't much
 * we can do at this point. We could also retry, but the USB stack has
 * already retried and still failed, so there is not much of a
 * point. As well, most of the traffic is network, which has recovery
 * methods for dropped packets.
 */
#include "i2400m-usb.h"
#include <linux/usb-compat.h>		/* @lket@ignore-line */


#define D_SUBMODULE tx
#include "usb-debug-levels.h"


static
void i2400mu_tx_submit(struct i2400mu *);


/*
 * Handle completion of the USB TX of a message
 *
 * In case of errors, ignore them (don't resubmit), but if they go
 * over the threshold, reset the device. See file header.
 */
static
void i2400mu_tx_cb(struct urb *urb)
{
	int result;
	struct i2400mu *i2400mu = urb->context;
	struct i2400m *i2400m = &i2400mu->i2400m;
	struct device *dev = &i2400mu->usb_iface->dev;

	d_fnstart(4, dev, "(urb %p status %d actual_length %d)\n",
		  urb, urb->status, urb->actual_length);
	result = urb->status;
	switch (result) {
	case 0:
		break;
	case -ECONNRESET:		/* disconnection */
	case -ENOENT:			/* ditto */
	case -ESHUTDOWN:		/* URB killed */
		up(&i2400mu->tx_urb_active);
		goto out;	/* Notify around */
	default:			/* Some error? */
		if (edc_inc(&i2400mu->urb_edc,
			    EDC_MAX_ERRORS, EDC_ERROR_TIMEFRAME)) {
			dev_err(dev, "TX: maximum errors in URB exceeded; "
				"resetting device\n");
			up(&i2400mu->tx_urb_active);
			usb_dev_reset_delayed(i2400mu->usb_dev);
			goto out;
		}
		dev_err(dev, "TX: URB error %d, dropping\n",
			urb->status);
	}
	usb_mark_last_busy(i2400mu->usb_dev);
	i2400m_tx_msg_sent(i2400m);	/* ack sending, advance the FIFO */
	i2400mu_tx_submit(i2400mu);
out:
	d_fnend(4, dev, "(urb %p status %d actual_length %d) = void\n",
		urb, urb->status, urb->actual_length);
	return;
}


/*
 * Get the next TX message in the TX FIFO and send it to the device
 *
 * If there are no more, mark TX as inactive.
 */
static
void i2400mu_tx_submit(struct i2400mu *i2400mu)
{
	int result;
	struct i2400m *i2400m = &i2400mu->i2400m;
	struct device *dev = &i2400mu->usb_iface->dev;
	struct i2400m_msg_hdr *tx_msg;
	size_t tx_msg_size;
	int usb_pipe;
	struct usb_endpoint_descriptor *epd;

	d_fnstart(4, dev, "(i2400mu %p)\n", i2400mu);
	tx_msg = i2400m_tx_msg_get(i2400m, &tx_msg_size);
	if (tx_msg == NULL) {
		d_printf(2, dev, "TX: no more messages, deactivating\n");
		up(&i2400mu->tx_urb_active);
		goto out_deactivated;
	}
	d_printf(2, dev, "TX: submitting %zu bytes\n", tx_msg_size);
	d_dump(5, dev, tx_msg, tx_msg_size);

	epd = usb_get_epd(i2400mu->usb_iface, I2400M_EP_BULK_OUT);
	usb_pipe = usb_sndbulkpipe(i2400mu->usb_dev, epd->bEndpointAddress);
	usb_fill_bulk_urb(i2400mu->tx_urb, i2400mu->usb_dev, usb_pipe,
			  tx_msg, tx_msg_size, i2400mu_tx_cb, i2400mu);
	result = usb_submit_urb(i2400mu->tx_urb, GFP_ATOMIC);
	if (result < 0) {
		dev_err(dev, "TX: cannot submit URB; tx_msg @%zu %zu b: %d\n",
			(void *) tx_msg - i2400m->tx_buf, tx_msg_size, result);
		up(&i2400mu->tx_urb_active);
		usb_dev_reset_delayed(i2400mu->usb_dev);
	}
	d_printf(2, dev, "TX: %ub submitted\n", tx_msg_size);
out_deactivated:
	d_fnend(4, dev, "(i2400mu %p) = void\n", i2400mu);
}


/*
 * i2400m TX engine notifies us that there is data in the FIFO ready
 * for TX
 *
 * If there is a URB in flight, don't do anything; when it finishes,
 * it will see there is data in the FIFO and send it. Else, just
 * submit a write.
 */
void i2400mu_bus_tx_kick(struct i2400m *i2400m)
{
	struct i2400mu *i2400mu = container_of(i2400m, struct i2400mu, i2400m);
	struct device *dev = &i2400mu->usb_iface->dev;

	d_fnstart(3, dev, "(i2400m %p) = void\n", i2400m);
	if (down_trylock(&i2400mu->tx_urb_active)) {
		/* URB is actively pruning the list */
		d_printf(2, dev, "TX kick: already active\n");
		goto out;
	}
	d_printf(2, dev, "TX kick: activating\n");
	i2400mu_tx_submit(i2400mu);
out:
	d_fnend(3, dev, "(i2400m %p) = void\n", i2400m);
}


int i2400mu_tx_setup(struct i2400mu *i2400mu)
{
	struct i2400m *i2400m = &i2400mu->i2400m;
	struct device *dev = &i2400mu->usb_iface->dev;
	i2400m->tx_block_size = 16;
	i2400mu->tx_urb = usb_alloc_urb(GFP_KERNEL, 0);
	if (i2400mu->tx_urb == NULL) {
		dev_err(dev, "TX: cannot allocate URB\n");
		return -ENOMEM;
	}
	return 0;
}

void i2400mu_tx_release(struct i2400mu *i2400mu)
{
	if (i2400mu->tx_urb) {
		usb_kill_urb(i2400mu->tx_urb);
		usb_free_urb(i2400mu->tx_urb);
		i2400mu->tx_urb = NULL;
	}

}
