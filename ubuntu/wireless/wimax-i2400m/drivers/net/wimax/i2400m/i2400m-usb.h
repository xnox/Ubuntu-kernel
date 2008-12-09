/* -*-linux-c-*-
 *
 * Intel Wireless WiMAX Connection 2400m
 * USB-specific i2400m driver definitions
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
 * Inaky Perez-Gonzalez <inaky.perez-gonzalez@intel.com>
 * Yanir Lubetkin <yanirx.lubetkin@intel.com>
 *  - Initial implementation
 *
 *
 * This driver implements the bus-specific part of the i2400m for
 * USB. Check i2400m.h for a generic driver description.
 *
 * ARCHITECTURE
 *
 * Quite simple; this driver will listen to notifications sent from
 * our notification endpoint (in usb-notif.c); data is ready to read,
 * the code in there schedules a RX read (usb-rc.c) and then passes
 * the data to the generic RX code (rx.c).
 *
 * When the generic driver needs to send data (network or control), it
 * queues up in the TX FIFO (tx.c) and then notifies us through the
 * bus_tx_kick() method (usb-tx.c) which will send the items in the
 * FIFO queue.
 */

#ifndef __I2400M_USB_H__
#define __I2400M_USB_H__

#include "i2400m.h"

/*
 * Host-Device interface for USB
 */

/* Misc constants */
enum {
	I2400M_MAX_NOTIFICATION_LEN = 256,
};

/* Endpoints */
enum {
	I2400M_EP_BULK_OUT = 0,
	I2400M_EP_NOTIFICATION,
	I2400M_EP_RESET_COLD,
	I2400M_EP_BULK_IN,
};


/**
 * struct i2400mu - descriptor for a USB connected i2400m
 *
 * @i2400m: bus-generic i2400m implementation; has to be first (see
 *     it's documentation in i2400m.h).
 *
 * @usb_dev: pointer to our USB device
 *
 * @usb_iface: pointer to our USB interface
 *
 * @urb_edc: error density counter; used to keep a density-on-time tab
 *     on how many soft (retryable or ignorable) errors we get. If we
 *     go over the threshold, we consider the bus transport is failing
 *     too much and reset.
 *
 * @notif_urb: URB for receiving notifications from the device.
 *
 * @tx_urb: URB for TXing data to the device.
 *
 * @tx_urb_active: used to mark when we have an active TX URB in
 *     flight so repeated calls to i2400m_usb_bus_tx_kick() don't step
 *     over each other.
 *
 * @rx_ctx_count: current number of active RX URBs
 *
 * @rx_ctx_pending: current number of pending read requests from the
 *     device. We keep reading until this list reaches zero.
 *
 * @rx_list: list of active RX URBs (actually, RX contexts)
 *
 * @rx_list_wq: woken up when a RX context is destroyed.
 */
struct i2400mu {
	struct i2400m i2400m;		/* FIRST! See doc */

	struct usb_device *usb_dev;
	struct usb_interface *usb_iface;
	struct edc urb_edc;		/* Error density counter */

	struct urb *notif_urb;
	struct urb *tx_urb;
	struct semaphore tx_urb_active;

	atomic_t rx_ctx_count, rx_pending_count;
	struct list_head rx_list;	/* protected by i2400m->lock */
	wait_queue_head_t rx_list_wq;	/* protected by i2400m->lock */
	atomic_t rx_size;
};


static inline
void i2400mu_init(struct i2400mu *i2400mu)
{
	i2400m_init(&i2400mu->i2400m);
	edc_init(&i2400mu->urb_edc);
	sema_init(&i2400mu->tx_urb_active, 1);
	atomic_set(&i2400mu->rx_ctx_count, 0);
	atomic_set(&i2400mu->rx_pending_count, 0);
	INIT_LIST_HEAD(&i2400mu->rx_list);
	init_waitqueue_head(&i2400mu->rx_list_wq);
	atomic_set(&i2400mu->rx_size,
		   PAGE_SIZE - sizeof(struct skb_shared_info));
}

/*
 * Reinitialize what changed after a soft reset that only affects the
 * internal state of the device.
 *
 * The USB transport hasn't been reset, so nothing that deals with it
 * should.
 */
static inline
void i2400mu_init_from_reset(struct i2400mu *i2400mu)
{
	i2400mu->i2400m.boot_mode = 1;
	atomic_set(&i2400mu->rx_ctx_count, 0);
	atomic_set(&i2400mu->rx_pending_count, 0);
}

extern int i2400mu_notification_setup(struct i2400mu *);
extern void i2400mu_notification_release(struct i2400mu *);

extern int i2400mu_rx_setup(struct i2400mu *);
extern void i2400mu_rx_release(struct i2400mu *);
extern int i2400m_bulk_in_submit(struct i2400mu *i2400m, gfp_t);

extern int i2400mu_tx_setup(struct i2400mu *);
extern void i2400mu_tx_release(struct i2400mu *);
extern void i2400mu_bus_tx_kick(struct i2400m *);

extern int i2400mu_dev_bootstrap(struct i2400mu *);
extern int i2400mu_dev_bootstrap_delayed(struct i2400mu *);

extern int __i2400mu_send_barker(struct i2400mu *,
				    const __le32 *, size_t,
				    unsigned endpoint);

extern ssize_t i2400mu_bus_bm_cmd_send(struct i2400m *,
				       const struct i2400m_bootrom_header *,
				       size_t, int);
extern ssize_t i2400mu_bus_bm_wait_for_ack(struct i2400m *,
					   struct i2400m_bootrom_header *,
					   size_t);

/**
 * __i2400m_reset_soft - do a full device reset
 *
 * @i2400m: device descriptor
 *
 * The device will be fully reset internally, but won't be
 * disconnected from the USB bus (so no reenumeration will
 * happen). Firmware upload will be neccessary.
 *
 * The device will send a reboot barker in the notification endpoint
 * that will trigger the driver to reinitialize the state
 * automatically from notif.c:i2400m_notification_grok() into
 * i2400m_dev_bootstrap_delayed().
 *
 * WARNING: no driver state saved/fixed
 */
static inline
int __i2400mu_reset_soft(struct i2400mu *i2400mu)
{
	static const __le32 i2400m_SOFT_BOOT_BARKER[4] = {
		__constant_cpu_to_le32(I2400M_WARM_RESET_BARKER),
		__constant_cpu_to_le32(I2400M_WARM_RESET_BARKER),
		__constant_cpu_to_le32(I2400M_WARM_RESET_BARKER),
		__constant_cpu_to_le32(I2400M_WARM_RESET_BARKER),
	};
	return __i2400mu_send_barker(i2400mu, i2400m_SOFT_BOOT_BARKER,
				     sizeof(i2400m_SOFT_BOOT_BARKER),
				     I2400M_EP_BULK_OUT);
}

/**
 * __i2400mu_reset_cold - do a full device and USB transport reset
 *
 * @i2400m: device descriptor
 *
 * The device will be fully reset internally, disconnected from the
 * USB bus an a reenumeration will happen. Firmware upload will be
 * neccessary. Thus, we don't do any locking or struct
 * reinitialization, as we are going to be fully disconnected and
 * reenumerated.
 *
 * WARNING: No driver state saved/fixed!!!
 */
static inline
int __i2400mu_reset_cold(struct i2400mu *i2400m)
{
	static const __le32 i2400m_COLD_BOOT_BARKER[4] = {
		__constant_cpu_to_le32(I2400M_COLD_RESET_BARKER),
		__constant_cpu_to_le32(I2400M_COLD_RESET_BARKER),
		__constant_cpu_to_le32(I2400M_COLD_RESET_BARKER),
		__constant_cpu_to_le32(I2400M_COLD_RESET_BARKER),
	};
	return __i2400mu_send_barker(i2400m, i2400m_COLD_BOOT_BARKER,
				     sizeof(i2400m_COLD_BOOT_BARKER),
				     I2400M_EP_RESET_COLD);
}

#endif /* #ifndef __I2400M_USB_H__ */
