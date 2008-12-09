/*
 * Linux WiMAX
 * Sysfs interfaces
 *
 *
 * Copyright (C) 2005-2006 Intel Corporation <linux-wimax@intel.com>
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
 * Expose the generic netlink family ID and version over sysfs.
 */

#include <linux/wimax.h>

/* We don't really care about debugging levels here, we just want the
 * definitions */

#define D_SUBMODULE sysfs
#include "debug-levels.h"

static
ssize_t wimax_dev_gnl_version_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct wimax_dev *wimax_dev = net_dev_to_wimax(to_net_dev(dev));
	return scnprintf(buf, PAGE_SIZE, "%02u", wimax_dev->gnl_family.version);
}
static
DEVICE_ATTR(gnl_version, S_IRUGO, wimax_dev_gnl_version_show, NULL);


static
ssize_t wimax_dev_gnl_family_id_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct wimax_dev *wimax_dev = net_dev_to_wimax(to_net_dev(dev));
	return scnprintf(buf, PAGE_SIZE, "%d", wimax_dev->gnl_family.id);
}
static
DEVICE_ATTR(gnl_family_id, S_IRUGO, wimax_dev_gnl_family_id_show, NULL);

struct d_level d_level[] = {
	D_SUBMODULE_DEFINE(id_table),
	D_SUBMODULE_DEFINE(op_close),
	D_SUBMODULE_DEFINE(op_msg),
	D_SUBMODULE_DEFINE(op_open),
	D_SUBMODULE_DEFINE(op_rfkill),
	D_SUBMODULE_DEFINE(stack),
};
size_t d_level_size = ARRAY_SIZE(d_level);

static
DEVICE_ATTR(debug_levels, S_IRUGO | S_IWUSR, d_level_show, d_level_store);


/* Pack'em sysfs attribute files */
static
struct attribute *wimax_dev_attrs[] = {
	&dev_attr_gnl_version.attr,
	&dev_attr_gnl_family_id.attr,
	&dev_attr_debug_levels.attr,
	NULL,
};

const struct attribute_group wimax_dev_attr_group = {
	.name = "wimax",
	.attrs = wimax_dev_attrs,
};
