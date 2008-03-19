/*
 * Linux WiMax
 * API for user space
 *
 *
 * Copyright (C) 2007 Intel Corporation <linux-wimax@intel.com>
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
 * WARNING: Still in flux, don't trust this documentation that much
 *
 * The WiMax stack communicates with user space by means of a generic
 * netlink pipe. Calls are forwarded from user space via generic
 * netlink and device specific messages are passed back and forth with
 * an specific interface.
 *
 * libwimax (in user space) provides high level tools for handling all this.
 *
 * This file describes the user/kernel protocol; stuff intended for
 * kernel usage only goes into include/linux/wimax.h.
 *
 *
 * GENERIC NETLINK
 *
 * Each WiMax network interface gets assigned a unique Generic Netlink
 * Family ID. This makes it easy (and cheap) to map device to family
 * ID considering that most deployments will have a single wimax
 * device. See file drivers/net/wimax/id-table.c.
 *
 * We use only the payload to encapsulate messages. This is so because
 * we are inheriting this format and the decission for now makes
 * sense. Future developments might change this.
 *
 * See file drivers/net/wimax/stack.c for special considerations on
 * the protocol for ACKing messages from userspace. Every time a
 * request is sent to the kernel, it is ACKed by netlink and then by
 * the WiMax stack -- see below for the protocol's details.
 *
 *
 * TESTING FOR THE INTERFACE AND VERSIONING
 *
 * Each WiMax device exports two sysfs files declaring the generic
 * netlink family ID associated to the interface and another one which
 * version it supports. The version code has to fit in one byte
 * (restrictions imposed by generic netlink); we use version / 10 for
 * the major version and version % 10 for the minor. This gives 9
 * minors for each major and 25 majors.
 *
 * The inexistence of any of this means the device does not support
 * the WiMax extensions.
 *
 * The version change protocol is as follow:
 *
 * - Major versions: needs to be increased if an existing message is
 *   changed or removed. Doesn't need to be changed if a new message
 *   is added.
 * - Minor verion: needs to be increased if new messages are being
 *   added or some other consideration that doesn't impact too much
 *   (like some kind of bug fix) and that is kind of left up in the
 *   air to common sense.
 *
 * Your user space code should not try to work if the major version it
 * was compiled for differs from what the kernel offers.
 *
 * libwimax's wimax_open() takes care of all this for you.
 */

#ifndef __NET__WIMAX_H__
#define __NET__WIMAX_H__

#include <linux/types.h>

enum {
	/** Version of the interface (unsigned decimal, MMm, max 25.5) */
	WIMAX_GNL_VERSION = 01,
	/* Generic NetLink attributes */
	WIMAX_GNL_ATTR_INVALID = 0x00,
	WIMAX_GNL_ATTR_MAX = 5,
};


/*
 * Generic NetLink ops
 */
enum {
	WIMAX_GNL_OP_MSG_FROM_USER,	/* User to kernel request */
	WIMAX_GNL_OP_MSG_TO_USER,	/* Kernel to user event */
	WIMAX_GNL_OP_OPEN,	/* Open a handle to a wimax device */
	WIMAX_GNL_OP_CLOSE,	/* Close a handle to a wimax device */
	WIMAX_GNL_OP_MAX,
};


/*
 * Attributes for the open operation
 *
 * NOTE: if you start the attr index with zero, it doesn't work --
 *       unkown reason, so start with '1'. 
 */
enum {
	WIMAX_GNL_OPEN_MSG_FROM_USER_PID = 1,
	WIMAX_GNL_OPEN_MAX,
};

#endif /* #ifndef __NET__WIMAX_H__ */
