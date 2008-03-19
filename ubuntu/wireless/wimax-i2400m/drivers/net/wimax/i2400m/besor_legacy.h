/* -*-linux-c-*-
 *
 * Intel Wireless WiMax Connection 2400m
 * IOCTL legacy control path
 *
 *
 *
 * Copyright (C) 2005-2007 Intel Corporation <linux-wimax@intel.com>
 * Yanir Lubetkin <yanirx.lubetkin@intel.com>
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
 */
#ifndef __BESOR_LEGACY_H__
#define __BESOR_LEGACY_H__

#include <linux/list.h>
#include <linux/completion.h>
#include <linux/kfifo.h>
#include <linux/netdevice.h>
#include <linux/workqueue.h>

struct i2400m;

/**
 * i2400m_legacy_ctl - legacy ioctl systm state and metadata.
 *
 */
enum {
	I2400M_SYSTEM_STATE_UNINITIALIZED = 0x01,
	I2400M_SYSTEM_STATE_INIT,
	I2400M_SYSTEM_STATE_READY,
	I2400M_SYSTEM_STATE_SCAN,
	I2400M_SYSTEM_STATE_STANDBY,
	I2400M_SYSTEM_STATE_CONNECTING,
	I2400M_SYSTEM_STATE_WIMAX_CONNECTED,
	I2400M_SYSTEM_STATE_DATA_PATH_CONNECTED,
	I2400M_SYSTEM_STATE_IDLE,
	I2400M_SYSTEM_STATE_DISCONNECTING,
	I2400M_SYSTEM_STATE_OUT_OF_ZONE,
	I2400M_SYSTEM_STATE_SLEEP_ACTIVE,
	I2400M_SYSTEM_STATE_PRODUCTION,
	I2400M_SYSTEM_STATE_CONFIG,

	I2400M_SYSTEM_STATE_TERMINATED = I2400M_SYSTEM_STATE_UNINITIALIZED,

};

struct msg_item {
	struct list_head node;
	size_t len;
	unsigned char buf[0];
};

struct i2400m_legacy_ctl {
	spinlock_t lock;
	/* control */
	struct completion comp;
	struct list_head msg_queue;
	/* notification */
	struct kfifo *notif_q;
	/* trace */
	struct kfifo *trace_q;
	unsigned long sys_state;
	unsigned long media_state;
	unsigned long link_state;
	struct work_struct ws;
	struct i2400m* parent;
};

extern void i2400m_legacy_ctl_cb(struct i2400m *, void *, size_t);
extern int i2400m_legacy_ioctl(struct net_device *net_dev, struct ifreq *rq,
			       int);
extern int i2400m_legacy_ctl_setup(struct i2400m *);
extern void i2400m_legacy_ctl_release(struct i2400m *);

#endif
