/*
 * @lket@ignore-file -- We actually don't want this file for upstream :)
 *
 * Intel Wireless WiMax Connection 2400m
 * Compatibility glue for usb.h; include right after.
 *
 *
 * NOTE: Most of this code is ripped of from the Linux kernel for
 * backwards compatibility purposes, so the original licensing terms
 * apply; when the code is new, then the licensing terms below apply.
 *
 *
 * Copyright (C) 2006-2007 Intel Corporation <linux-wimax@intel.com>
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
 * FIXME: docs
 */

#include <config.h>			/* sure dirty, wth */
#ifdef NEED_USB_DEV_RESET_DELAYED
struct usb_device;
extern void usb_dev_reset_delayed(struct usb_device *);
#endif
