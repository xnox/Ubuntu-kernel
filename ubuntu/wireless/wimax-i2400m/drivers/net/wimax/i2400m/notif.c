/* -*-linux-c-*-
 *
 * Intel Wireless WiMax Connection 2400m
 * Notification handling
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
 *
 * Roadmap for main driver entry points:
 *
 * i2400m_notification_setup();
 * i2400m_notification_release();
 * i2400m_notification_cb();
 */
#include "../version.h"
#include <linux/if_arp.h>
#include <linux/netdevice.h>
#include <linux/usb.h>
#include <linux/usb-compat.h>
#include "i2400m.h"
#define D_LOCAL 2
#include "../debug.h"


/**
 * i2400m_fill_submit_notif_urb - utility, prepare and submit a urb
 *
 * @urb: completed urb to handle
 * @flags: flags for usb_submit_urb()
 * @buf: I/O buffer
 * @buf_len: size of the I/O buffer
 * @callback: the completion routine for this urb
 */
int i2400m_fill_submit_notif_urb(struct i2400m *i2400m, struct urb *urb,
				 unsigned long flags, void *buf,
				 size_t buf_len,
				 void (*handler) (struct urb *))
{
	int usb_pipe, ret = 0;
	struct usb_endpoint_descriptor *epd;
	struct device *dev = &i2400m->usb_iface->dev;

	d_fnstart(6, dev, "\n");
	epd = &i2400m->usb_iface->cur_altsetting->
		endpoint[I2400M_EP_NOTIFICATION].desc;
	usb_pipe = usb_rcvintpipe(i2400m->usb_dev, epd->bEndpointAddress);
	usb_fill_int_urb(urb, i2400m->usb_dev, usb_pipe, buf, buf_len,
			 handler, i2400m, 5);
	ret = usb_submit_urb(urb, flags);
	if (ret != 0)
		dev_err(dev, "submit notif urb failed\n");
	d_fnend(6, dev, "returning %d\n", ret);
	return ret;
}


static const
__le32 i2400m_ZERO_BARKER[4] = { 0, 0, 0, 0 };


/**
 * i2400m_notification_cb - callback to be called upon notification reception
 *
 * @urb: the urb received from the notification endpoint
 *
 * The callback is responsible for analysing the received data, identify barkers
 * and respond to the presence of a known pattern in the urb's buffer.
 *
 * In normal operation mode, we can only receive two types of payloads
 * on the notification endpoint:
 *
 *   - a reboot barker, we do a bootstrap (the device has reseted).
 *
 *   - a block of zeroes: there is pending data in the IN endpoint
 */
static int i2400m_notification_grok(struct i2400m *i2400m, const void *buf,
				    size_t buf_len)
{
	int ret;
	struct device *dev = &i2400m->usb_iface->dev;

	d_fnstart(4, dev, "(i2400m %p buf %p buf_len %zu)\n",
		  i2400m, buf, buf_len);
	ret = -EIO;
	if (buf_len < sizeof(i2400m_REBOOT_BARKER)) {
		dev_err(dev, "HW BUG? short notification (%zu vs %u bytes "
			"expected)\n", buf_len, sizeof(i2400m_REBOOT_BARKER));
		goto error_bad_size;
	}
	if (!memcmp(i2400m_REBOOT_BARKER, buf, sizeof(i2400m_REBOOT_BARKER)))
		ret = i2400m_dev_bootstrap_delayed(i2400m);
	else if (!memcmp(i2400m_ZERO_BARKER, buf, sizeof(i2400m_ZERO_BARKER)))
		ret = i2400m_bulk_in_submit(i2400m, GFP_ATOMIC);
	else {	/* Unknown or unexpected data in the notification message */
		ret = -EIO;
		dev_err(dev, "HW BUG? Unknown/unexpected data in notification "
			"message (%zu bytes)\n", buf_len);
		d_dump(5, dev, buf, buf_len);
	}
error_bad_size:
	d_fnend(4, dev, "(i2400m %p buf %p buf_len %zu) = %d\n",
		i2400m, buf, buf_len, ret);
	return ret;
}


/**
 * i2400m_notification_cb - callback to be called upon notification reception
 *
 * @urb: the urb received from the notification endpoint
 *
 * The callback is responsible for analysing the received data, identify barkers
 * and respond to the presence of a known pattern in the urb's buffer.
 */
void i2400m_notification_cb(struct urb *urb)
{
	int ret;
	struct i2400m *i2400m = urb->context;
	struct device *dev = &i2400m->usb_iface->dev;

	d_fnstart(4, dev, "(urb %p status %d actual_length %d)\n",
		  urb, urb->status, urb->actual_length);
	ret = urb->status;
	switch (ret) {
	case 0:
		ret = i2400m_notification_grok(i2400m, urb->transfer_buffer,
					       urb->actual_length);
		if (ret == -EIO && edc_inc(&i2400m->urb_edc, EDC_MAX_ERRORS,
					   EDC_ERROR_TIMEFRAME))
			goto error_exceeded;
		if (ret == -ENOMEM)	/* uff...power cycle? shutdown? */
			goto error_exceeded;
		break;
	case -ECONNRESET:		/* disconnection */
	case -ENOENT:			/* ditto */
	case -ESHUTDOWN:		/* URB killed */
		goto out_wake;		/* Notify around */
	default:			/* Some error? */
		if (edc_inc(&i2400m->urb_edc,
			    EDC_MAX_ERRORS, EDC_ERROR_TIMEFRAME))
			goto error_exceeded;
		dev_err(dev, "notification: URB error %d, retrying\n",
			urb->status);
	}
	ret = usb_submit_urb(i2400m->notif_urb, GFP_ATOMIC);
	if (ret != 0) {
		dev_err(dev, "notification: cannot submit URB: %d\n", ret);
		goto error_submit;
	}

	d_fnend(4, dev, "(urb %p status %d actual_length %d) = void\n",
		urb, urb->status, urb->actual_length);
	return;

error_exceeded:
	dev_err(dev, "maximum errors in notification URB exceeded; "
		"resetting device\n");
error_submit:
#ifdef NEED_USB_DEV_RESET_DELAYED
	usb_dev_reset_delayed(i2400m->usb_dev);
#endif
out_wake:
#warning FIXME: whowever might be waiting here should be warned, but I dont think we have any
	d_fnend(4, dev, "(urb %p status %d actual_length %d) = void\n",
		urb, urb->status, urb->actual_length);
	return;
}


/**
 * i2400m_notification_setup - setup the notification endpoint
 *
 * @i2400m: device descriptor
 * @handler: the callback to be submitted as the urb completion routine
 *
 * This procedure prepares the notification urb and handler for receiving
 * unsolicited barkers from the device. it does not activate the endpoint,
 * this is done seperately by submitting the urb.
 */
int i2400m_notification_setup(struct i2400m *i2400m)
{
	struct device *dev = &i2400m->usb_iface->dev;
	int ret = 0;
	char *buf;

	d_fnstart(2, dev, "(i2400m %p)\n", i2400m);
	buf = kmalloc(I2400M_MAX_NOTIFICATION_LEN, GFP_KERNEL | GFP_DMA);
	if (buf == NULL) {
		dev_err(dev, "notification: buffer allocation failed\n");
		ret = -ENOMEM;
		goto error_buf_alloc;
	}
	i2400m->notif_urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!i2400m->notif_urb) {
		ret = -ENOMEM;
		dev_err(dev, "notification: cannot allocate URB\n");
		goto error_alloc_urb;
	}
	ret = i2400m_fill_submit_notif_urb(i2400m, i2400m->notif_urb,
					   GFP_KERNEL,
					   buf, I2400M_MAX_NOTIFICATION_LEN,
					   i2400m_notification_cb);
	if (ret != 0) {
		dev_err(dev, "notification: cannot submit URB: %d\n", ret);
		goto error_submit;
	}
	d_fnend(2, dev, "(i2400m %p) = %d\n", i2400m, ret);
	return ret;

error_submit:
	usb_free_urb(i2400m->notif_urb);
error_alloc_urb:
	kfree(buf);
error_buf_alloc:
	d_fnend(2, dev, "returning %d\n", ret);
	return ret;
}


/**
 *i2400m_notification_release - tear down of the notification mechanism
 *
 * @i2400m: device descriptor
 *
 * kill the interrupt endpoint urb, free any allocated resources
 */
void i2400m_notification_release(struct i2400m *i2400m)
{
	struct device *dev = &i2400m->usb_iface->dev;

	d_fnstart(2, dev, "(i2400m %p)\n", i2400m);
	if (i2400m->notif_urb != NULL) {
		usb_kill_urb(i2400m->notif_urb);
		kfree(i2400m->notif_urb->transfer_buffer);
		usb_free_urb(i2400m->notif_urb);
		i2400m->notif_urb = NULL;
	}
	d_fnend(2, dev, "(i2400m %p)\n", i2400m);
}

