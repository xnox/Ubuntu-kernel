/*
 * Linux WiMAX
 * Internal API for kernel space WiMAX stack
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
 */

#ifndef __WIMAX_INTERNAL_H__
#define __WIMAX_INTERNAL_H__
#ifdef __KERNEL__

#include <linux/device.h>
#include <linux/wimax.h>

extern int wimax_id_table_add(int, struct net_device *);
extern struct net_device *wimax_get_netdev_by_info(struct genl_info *);
extern void wimax_id_table_rm(int);
extern void wimax_id_table_release(void);

extern int wimax_rfkill_add(struct wimax_dev *);
extern void wimax_rfkill_rm(struct wimax_dev *);

extern const struct attribute_group wimax_dev_attr_group;

#endif /* #ifdef __KERNEL__ */
#endif /* #ifndef __WIMAX_INTERNAL_H__ */
