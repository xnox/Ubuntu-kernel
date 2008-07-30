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
 * This file declares the user/kernel protocol; stuff intended for
 * kernel usage as well as full protocol and stack documentation is
 * rooted in include/linux/wimax.h.
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
	WIMAX_GNL_OP_RFKILL,	/* Run wimax_rfkill() */
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


/*
 * RF Kill support
 *
 * Make sure WIMAX_RFKILL_OFF and WIMAX_RFKILL_ON match
 * RFKILL_STATE_ON and RFKILL_STATE_OFF or chaos will ensue. Don't
 * want to use them here as it would force #including <linux/rfkill.h>
 * and then we would not be able to use this file verbatim for user
 * space API exporting.
 *
 * Just in case, there are a couple of BUILD_BUG_ON() checks on this
 * in drivers/net/wimax/op-rfkill.c
 */

enum wimax_rfkill_state {
	WIMAX_RFKILL_OFF = 0,
	WIMAX_RFKILL_ON,
	WIMAX_RFKILL_QUERY
};

enum {
	WIMAX_GNL_RFKILL_STATE = 1,
	WIMAX_GNL_RFKILL_MAX,
};

#endif /* #ifndef __NET__WIMAX_H__ */
