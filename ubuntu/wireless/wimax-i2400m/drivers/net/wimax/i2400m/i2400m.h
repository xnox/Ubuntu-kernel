/* -*-linux-c-*-
 *
 * Intel Wireless WiMax Connection 2400m
 * Declarations for internal APIs
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
 * Inaky Perez-Gonzalez <inaky.perez-gonzalez@intel.com>
 * Yanir Lubetkin <yanirx.lubetkin@intel.com>
 *  - Initial implementation
 *
 *
 * FIXME: docs
 */

#ifndef __I2400M_H__
#define __I2400M_H__

#include <linux/usb.h>
#include <linux/netdevice.h>
#include <linux/wimax.h>
#include <asm/byteorder.h>
#include <config.h>		/* @lket@ignore-line */
#include "edc.h"
#ifdef __USE_LEGACY_IOCTL
#include "besor_legacy.h"
#endif /* __USE_LEGACY_IOCTL */

/*
 * struct req_inflight - Basic flow control
 *
 * We maintain a basic flow control counter. "count" how many TX URBs
 * are outstanding. Only allow "max" TX URBs to be outstanding. If
 * this value is reached the queue will be stopped. The queue will be
 * restarted when there are "threshold" URBs outstanding.  Maintain a
 * counter of how many time the TX queue needed to be restarted due to
 * the "max" being exceeded and the "threshold" reached again. The
 * timestamp "restart_ts" is to keep track from when the counter was
 * last queried (see sysfs handling of file winet_tx_inflight).
 */
struct req_inflight {
	atomic_t count;
	unsigned long max;
	unsigned long threshold;
	unsigned long restart_ts;
	atomic_t restart_count;
};

/*
 * Defaults for inflight request control
 */
enum {
	i2400m_TX_INFLIGHT_MAX = 1000,
	i2400m_TX_INFLIGHT_THRESHOLD = 100,
};

static inline void req_inflight_setup(struct req_inflight *inflight)
{
	inflight->max = i2400m_TX_INFLIGHT_MAX;
	inflight->threshold = i2400m_TX_INFLIGHT_THRESHOLD;
	inflight->restart_ts = jiffies;
}


/**
 * i2400m_tx - TX state and metadata. common to data and control planes.
 *
 * @lock: tx data access lock
 * @state: current state of the tx transaction
 * @submitted_buf: buffer submitted to the device, pending kfree().
 * @msg_buf: buffer containing the pending message to be sent
 * @msg_start: start of actual within the message buffer
 * @msg_len: lenght of the message
 * @urb: the active tx urb
 */
struct i2400m_tx {
	spinlock_t lock;
	int state;
	void *submitted_buf;
	void *msg_buf;
	void *msg_start;
	size_t msg_len;
	struct urb *urb;
	struct sk_buff *skb;
	unsigned long seq_id;
};


/**
 * struct i2400m - descriptor for the hardware state
 *
 * @wimax_dev: Wimax device -- Due to the funny way a net_device is
 *             allocated, we need to force this to be the first
 *             field so that we can get from net_dev_priv() the right
 *             pointer.
 */
struct i2400m {
	struct wimax_dev wimax_dev;	/* FIRST! See doc */
	struct usb_device *usb_dev;
	struct usb_interface *usb_iface;
	struct net_device *net_dev;

	spinlock_t lock;
	struct edc urb_edc;		/* Error density counter */
	unsigned boot_mode:1;

	atomic_t rx_ctx_count, rx_pending_count;
	struct req_inflight tx_inflight;
	struct urb *notif_urb;
	struct i2400m_tx *tx_data;
#ifdef __USE_LEGACY_IOCTL
	struct i2400m_legacy_ctl *legacy_ctl;
	unsigned ctl_iface;	/* 0 - legacy ioctl, !0 - new */
#endif				/* __USE_LEGACY_IOCTL */
};

static inline void i2400m_init(struct i2400m *i2400m)
{
	wimax_dev_init(&i2400m->wimax_dev);
	spin_lock_init(&i2400m->lock);
	edc_init(&i2400m->urb_edc);
	i2400m->boot_mode = 1;
	atomic_set(&i2400m->rx_ctx_count, 0);
	atomic_set(&i2400m->rx_pending_count, 0);
#ifdef __USE_LEGACY_IOCTL
	i2400m->ctl_iface = 1;
#endif
}

/*
 * Similar to above, but reinitialize only what affects the internal
 * state of the device. The USB transport hasn't been reset, so
 * nothing that deals with it should.
 */
static inline void i2400m_init_from_reset(struct i2400m *i2400m)
{
	i2400m->boot_mode = 1;
	atomic_set(&i2400m->rx_ctx_count, 0);
	atomic_set(&i2400m->rx_pending_count, 0);
}

static inline struct i2400m *i2400m_get(struct i2400m *i2400m)
{
	dev_hold(i2400m->net_dev);
	return i2400m;
}

static inline void i2400m_put(struct i2400m *i2400m)
{
	dev_put(i2400m->net_dev);
}

static inline struct i2400m *wimax_dev_to_i2400m(struct wimax_dev *wimax_dev)
{
	return container_of(wimax_dev, struct i2400m, wimax_dev);
}

static inline struct i2400m *net_dev_to_i2400m(struct net_device *net_dev)
{
	return wimax_dev_to_i2400m(netdev_priv(net_dev));
}

extern void i2400m_net_rx(struct i2400m *, void *, int);
extern void i2400m_netdev_setup(struct net_device *net_dev);

/* sub modules */
extern int i2400m_sysfs_setup(struct device_driver *);
extern void i2400m_sysfs_release(struct device_driver *);
extern int i2400m_notification_setup(struct i2400m *);
extern void i2400m_notification_release(struct i2400m *);
extern void i2400m_rx_release(struct i2400m *);
extern int i2400m_tx_setup(struct i2400m *);
extern void i2400m_tx_release(struct i2400m *);

extern int i2400m_bulk_in_submit(struct i2400m *i2400m, gfp_t);
extern struct attribute_group i2400m_dev_attr_group;


/*
 *
 * Device specific interface!
 *
 *
 * Packet types for the host-device interface
 */
enum i2400m_pt {
	I2400M_PT_DATA = 0,
	I2400M_PT_CTRL,
	I2400M_PT_TRACE,	/* For device debug */
	I2400M_PT_RESET_WARM,	/* device reset */
	I2400M_PT_RESET_COLD,	/* USB[transport] reset, like reconnect */
	I2400M_PT_ILLEGAL
};

enum {
	I2400M_PKT_PAD = 16,	/* Alignment of packet data */
	I2400M_MAX_NOTIFICATION_LEN = 256,
	/* protocol barkers: sync sequences; for notifications they
	 * are sent in groups of four. */
	I2400M_H2D_PREVIEW_BARKER = 0xCAFE900D,
	I2400M_COLD_RESET_BARKER = 0xc01dc01d,
	I2400M_WARM_RESET_BARKER = 0x50f750f7,
	I2400M_REBOOT_BARKER = 0xDEADBEEF,
	I2400M_ACK_BARKER = 0xFEEDBABE,
	I2400M_D2H_MSG_BARKER = 0xBEEFBABE
};


static const __le32 i2400m_REBOOT_BARKER[4] = {
	__constant_cpu_to_le32(0xdeadbeef),
	__constant_cpu_to_le32(0xdeadbeef),
	__constant_cpu_to_le32(0xdeadbeef),
	__constant_cpu_to_le32(0xdeadbeef)
};


/*
 * definition needed in order to parse a bulk_in buffer:
 * buffer contains a message header followed by one or more packet headers
 */
enum {
	I2400M_MAX_MSG_LEN = 0x10000,
	I2400M_MAX_PACKETS_IN_MSG = 60,
	I2400M_PKT_SIZE = 0x3EFF
};


/* USB transport definitions */
/**
 * enum - enumerate the endpoints by their logical role
 */
enum {
	I2400M_EP_BULK_OUT = 0,
	I2400M_EP_NOTIFICATION,
	I2400M_EP_RESET_COLD,
	I2400M_EP_BULK_IN,
};


/**
 * bulk_in_msg_hdr - header of each bulk in buffer.
 *
 * @barker: preamble
 * @sequence: sequence number of this message
 * @num_pkts: number of distinct packets in this bulk
 * @padding: amount of padding bytes at the end of the bulk
 */
struct i2400m_device_msg_hdr {
	__le32 barker;
	__le32 sequence;
	__le16 num_pkts;
	__le16 rsv1;
	__le16 padding;
	__le16 rsv2;
} __attribute__ ((packed));


/**
 * bulk_in_pkt_hdr - packet descriptor.
 *
 * @size: packet size
 * @type: packet type
 */
union i2400m_device_pkt_hdr {
	struct {
#if defined(__LITTLE_ENDIAN)
		u32 size:14;
		u32 rsv1:2;
		u32 type:4;
		u32 rsv2:12;
#elif defined(__BIG_ENDIAN)
		u32 rsv2:12;
		u32 type:4;
		u32 rsv1:2;
		u32 size:14;
#else
#error "Neither __LITTLE_ENDIAN nor __BIG_ENDIAN are defined."
#endif
	} __attribute__ ((packed));
	__le32 value;
} __attribute__ ((packed));


/**
 * bulk_in_hdr - used to cast over the bulk in buffer.
 *
 * @size: header of the entire message
 * @type: array of headers for the specific packets
 */
struct i2400m_msg_preview {
	struct i2400m_device_msg_hdr msg_hdr;
	union i2400m_device_pkt_hdr pkts[I2400M_MAX_PACKETS_IN_MSG];
} __attribute__ ((packed));


/* utility functions */
extern int i2400m_write_async(struct i2400m *, struct sk_buff *,
			      const void *, size_t, enum i2400m_pt);
struct i2400m_work 	/* Helper for scheduling simple work functions */
{
	struct work_struct ws;
	struct i2400m *i2400m;
};
extern int i2400m_schedule_work(struct i2400m *i2400m,
				void (*fn)(struct work_struct *), gfp_t);
extern int i2400m_diag_off(struct i2400m *);

/* device bootstrap - firmware download */
extern int i2400m_dev_bootstrap(struct i2400m *);
extern int i2400m_dev_bootstrap_delayed(struct i2400m *);

/* reset support */
extern int __i2400m_send_barker(struct i2400m *i2400m,
				const __le32 *barker,
				size_t barker_size,
				unsigned endpoint);


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
#include <linux/usb.h>
static inline
int __i2400m_reset_soft(struct i2400m *i2400m)
{
	static const __le32 i2400m_SOFT_BOOT_BARKER[4] = {
		__constant_cpu_to_le32(0x50f750f7),
		__constant_cpu_to_le32(0x50f750f7),
		__constant_cpu_to_le32(0x50f750f7),
		__constant_cpu_to_le32(0x50f750f7),
	};
	return __i2400m_send_barker(i2400m, i2400m_SOFT_BOOT_BARKER,
				      sizeof(i2400m_SOFT_BOOT_BARKER),
				      I2400M_EP_BULK_OUT);
}

extern void i2400m_reset_soft(struct i2400m *);	/* resorts to USB reset */

/**
 * __i2400m_reset_cold - do a full device and USB transport reset
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
int __i2400m_reset_cold(struct i2400m *i2400m)
{
	static const __le32 i2400m_COLD_BOOT_BARKER[4] = {
		__constant_cpu_to_le32(0xc01dc01d),
		__constant_cpu_to_le32(0xc01dc01d),
		__constant_cpu_to_le32(0xc01dc01d),
		__constant_cpu_to_le32(0xc01dc01d),
	};
	return __i2400m_send_barker(i2400m, i2400m_COLD_BOOT_BARKER,
				    sizeof(i2400m_COLD_BOOT_BARKER),
				    I2400M_EP_RESET_COLD);
}

extern void i2400m_reset_cold(struct i2400m *);	/* resorts to USB reset */

#endif /* #ifndef __I2400M_H__ */
