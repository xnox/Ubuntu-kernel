/*
 * Linux WiMax
 * Netlink layer for the kernel/userspace interface
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
 * FIXME: docs
 */
#include "version.h"
#include <linux/device.h>
#include <net/genetlink.h>
#include <linux/netdevice.h>
#include <net/wimax.h>
#include "wimax-internal.h"

#define D_LOCAL 3
#include "debug.h"


/**
 * wimax_dev_init - initialize a newly allocated & zeroed wimax_dev instance
 *
 * @wimax_dev: pointer to struct wimax_dev to initialize.
 */
void wimax_dev_init(struct wimax_dev *wimax_dev)
{
	/* Does nothing for now, but it might in the future, so we
	 * leave it here for forward compatibility, transparent to
	 * the developer */
	wimax_dev->gnl_family.id = GENL_ID_GENERATE;
	wimax_dev->gnl_family.hdrsize = 0;
	wimax_dev->gnl_family.version = WIMAX_GNL_VERSION;
	wimax_dev->gnl_family.maxattr = WIMAX_GNL_ATTR_MAX;
}
EXPORT_SYMBOL_GPL(wimax_dev_init);


static
struct genl_ops *wimax_gnl_ops[] = {
	&wimax_gnl_msg_from_user,
	&wimax_gnl_open,
	&wimax_gnl_close,
};


/*
 * wimax_gnl_add - setup generic netlink pipe
 *
 * @wimax_dev: device descriptor
 */
int wimax_gnl_add(struct wimax_dev *wimax_dev)
{
	int result, cnt;
	struct net_device *net_dev = wimax_dev->net_dev;
	struct device *dev = net_dev->dev.parent;
	
	d_fnstart(4, dev, "(wimax_dev %p)\n", wimax_dev);
	snprintf(wimax_dev->gnl_family.name, sizeof(wimax_dev->gnl_family.name),
		 "WiMax %s", net_dev->name);
	result = genl_register_family(&wimax_dev->gnl_family);
	if (unlikely(result < 0)) {
		dev_err(dev, "cannot register generic netlink family: %d\n",
			result);
		goto error_register_family;
	}
	for (cnt = 0; cnt < ARRAY_SIZE(wimax_gnl_ops); cnt++) {
		result = genl_register_ops(&wimax_dev->gnl_family,
					   wimax_gnl_ops[cnt]);
		d_printf(4, dev, "registering generic netlink op code "
			 "%u: %d\n", wimax_gnl_ops[cnt]->cmd, result);
		if (unlikely(result < 0)) {
			dev_err(dev, "cannot register generic netlink op code "
				"%u: %d\n", wimax_gnl_ops[cnt]->cmd, result);
			goto error_register_ops;
		}
	}		
	d_fnend(4, dev, "(wimax_dev %p net_dev %p) = 0\n", wimax_dev, net_dev);
	return 0;

error_register_ops:
	for (cnt--; cnt >= 0; cnt--)
		genl_unregister_ops(&wimax_dev->gnl_family,
				    wimax_gnl_ops[cnt]);
	genl_unregister_family(&wimax_dev->gnl_family);
error_register_family:
	d_fnend(4, dev, "(wimax_dev %p net_dev %p) = %d\n",
		wimax_dev, net_dev, result);
	return result;
}


/*
 * wimax_gnl_rm - tear down the generic netlink pipe
 *
 * @wimax_dev: device descriptor
 */
void wimax_gnl_rm(struct wimax_dev *wimax_dev)
{
	int cnt, result;
	struct device *dev = wimax_dev->net_dev->dev.parent;
	
	d_fnstart(4, dev, "(wimax_dev %p)\n", wimax_dev);
	for (cnt = ARRAY_SIZE(wimax_gnl_ops) - 1; cnt >= 0; cnt--)
		result = genl_unregister_ops(&wimax_dev->gnl_family,
					     wimax_gnl_ops[cnt]);
	genl_unregister_family(&wimax_dev->gnl_family);
	d_fnend(4, dev, "(wimax_dev %p) = void\n", wimax_dev);
}


/**
 * wimax_dev_add - register a new wimax device
 *
 * @wimax_dev: pointer to the wimax description embedded in your
 *             net_dev's priv data. You must have called
 *             wimax_dev_init() on it before.
 * @net_dev: net device the wimax_dev is associated with. We expect
 *             you called SET_NETDEV_DEV() and register_netdev() on it
 *             before calling us.
 *
 * Registers the new Wimax device, sets up the user-kernel control
 * pipe (generic netlink) and common wimax infrastructure.
 *
 * After this function returns, you might get user space control
 * requests via netlink or from sysfs that might translate into calls
 * into wimax_dev->ops.
 */
int wimax_dev_add(struct wimax_dev *wimax_dev, struct net_device *net_dev)
{
	int result;
	struct device *dev = net_dev->dev.parent;
	d_fnstart(3, dev, "(wimax_dev %p net_dev %p)\n", wimax_dev, net_dev);
	wimax_dev->net_dev = net_dev;
	result = wimax_gnl_add(wimax_dev);
	if (result < 0)
		goto error_gnl_add;
	result = wimax_id_table_add(wimax_dev->gnl_family.id, net_dev);
	if (unlikely(result < 0)) {
		dev_err(dev, "cannot register family id: %d\n",
			result);
		goto error_id_table_add;
	}
	result = sysfs_create_group(&net_dev->dev.kobj, &wimax_dev_attr_group);
	if (result < 0) {
		dev_err(dev, "cannot initialize sysfs attributes: %d\n",
			result);
		goto error_sysfs_create_group;
	}
	d_fnend(3, dev, "(wimax_dev %p net_dev %p) = 0\n", wimax_dev, net_dev);
	return 0;

error_sysfs_create_group:
	wimax_id_table_rm(wimax_dev->gnl_family.id);
error_id_table_add:
	wimax_gnl_rm(wimax_dev);
error_gnl_add:
	d_fnend(3, dev, "(wimax_dev %p net_dev %p) = %d\n",
		wimax_dev, net_dev, result);
	return result;
}
EXPORT_SYMBOL_GPL(wimax_dev_add);


/**
 * wimax_dev_rm - unregister an existing wimax device
 *
 * @wimax_dev: pointer to the wimax description embedded in your
 *             net_dev's priv data.
 *
 * Unregisters a Wimax device previously registered for use with
 * wimax_add_rm().
 *
 * IMPORTANT! Must call before calling unregister_netdev().
 *
 * After this function returns, you will not get any more user space
 * control requests (via netlink or sysfs) and thus to wimax_dev->ops.
 */
void wimax_dev_rm(struct wimax_dev *wimax_dev, struct net_device *net_dev)
{
	d_fnstart(3, NULL, "(wimax_dev %p)\n", wimax_dev);
	sysfs_remove_group(&net_dev->dev.kobj, &wimax_dev_attr_group);
	wimax_id_table_rm(wimax_dev->gnl_family.id);
	wimax_gnl_rm(wimax_dev);
	d_fnend(3, NULL, "(wimax_dev %p) = void\n", wimax_dev);
}
EXPORT_SYMBOL_GPL(wimax_dev_rm);


/* Initialize / shutdown the wimax stack */
static
int __init wimax_subsys_init(void)
{
	return 0;
}
module_init(wimax_subsys_init);

static
void __exit wimax_subsys_exit(void)
{
	wimax_id_table_release();
}
module_exit(wimax_subsys_exit);

MODULE_AUTHOR("Inaky Perez-Gonzalez <inaky.perez-gonzalez@intel.com>");
MODULE_DESCRIPTION("Linux Wimax stack");
MODULE_LICENSE("GPL");
MODULE_VERSION(WIMAX_VERSION);

