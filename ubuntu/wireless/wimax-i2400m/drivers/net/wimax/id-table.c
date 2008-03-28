/*
 * Linux WiMax
 * Mappping of generic netlink family IDs to net devices
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
 * We assign a single generic netlink family ID to each device (to
 * simplify lookup).
 *
 * We keep a table <id, net_dev> so we can lookup (and refcount) the
 * net_dev from the ID. Protected by a lock. When the array is full
 * (id == 0 is considered empty), we just double it's size. We start
 * with 1, as most systems will only have one adapter.
 *
 * All is protected with a rwlock, as most of the ops are read.
 *
 * Didn't want to use an attribute name on the netlink message because
 * that implied a heavier search over all the netdevs; seemed kind of
 * a waste given most systems will have just a single WiMax
 * adapter. This one should work with almost no overhead.
 */
#include "version.h"
#include <linux/device.h>
#include <net/genetlink.h>
#include <linux/netdevice.h>
#include <net/wimax.h>
#include "wimax-internal.h"

#define D_LOCAL 0
#include "debug.h"


static rwlock_t wimax_id_table_lock = __RW_LOCK_UNLOCKED(wimax_id_table_lock);
static size_t wimax_id_table_size;
static struct wimax_id_table
{
	int id;
	struct net_device *net_dev;
} *wimax_id_table = NULL;


/**
 * wimax_id_table_add - add a gennetlink familiy ID / net_dev mapping
 *
 * @id: family ID to add to the table
 * @net_dev: net_dev of the wimax device to associate to the family
 *           ID.
 *
 * Look for an empty spot in the ID table; if none found, double the
 * table's size and get the first spot.
 */
int wimax_id_table_add(int id, struct net_device *net_dev)
{
	int result = 0;
	unsigned cnt;
	struct wimax_id_table *new_table;
	size_t new_size;

	write_lock(&wimax_id_table_lock);
	for (cnt = 0; cnt < wimax_id_table_size; cnt++)
		if (wimax_id_table[cnt].id == 0)
			goto found;
	/* Humm, no slots available, double the size; if it is empty,
	 * we just create one slot -- I sure doubt most machines will
	 * have more than one wimax adapter.*/
	new_size = wimax_id_table_size? 2 * wimax_id_table_size : 1;
	new_table = krealloc(wimax_id_table, new_size, GFP_ATOMIC);
	if (unlikely(new_table == NULL)) {
		result = -ENOMEM;
		goto out_unlock;
	}
	memset(new_table + wimax_id_table_size, 0,
	       wimax_id_table_size * sizeof(wimax_id_table[0]));
	cnt = wimax_id_table_size;
	wimax_id_table_size = new_size;
	wimax_id_table = new_table;
found:
	wimax_id_table[cnt].id = id;
	wimax_id_table[cnt].net_dev = net_dev;
	result = 0;
out_unlock:
	write_unlock(&wimax_id_table_lock);
	return result;
}


/*
 * wimax_get_netdev_by_info - lookup a netdev from the gennetlink info
 *
 * The generic netlink family ID has been filled out in the
 * nlmsghdr->nlmsg_type field, so we pull it from there, look it up in
 * the mapping table and reference the net_device.
 */
struct net_device *wimax_get_netdev_by_info(struct genl_info *info)
{
	struct net_device *net_dev;
	unsigned cnt;
	int id = info->nlhdr->nlmsg_type;

	d_fnstart(3, NULL, "(info %p [id %d])\n", info, id);
	read_lock(&wimax_id_table_lock);
	for (cnt = 0; cnt < wimax_id_table_size; cnt++)
		if (wimax_id_table[cnt].id == id) {
			net_dev = wimax_id_table[cnt].net_dev;
			dev_hold(net_dev);
			goto out_unlock;
		}
	printk(KERN_ERR "wimax: no device associated to ID %d\n", id);
	net_dev = NULL;
out_unlock:
	read_unlock(&wimax_id_table_lock);
	d_fnend(3, NULL, "(info %p) = %p\n", info, net_dev);
	return net_dev;
}


/*
 * wimax_id_table_rm - Remove a gennetlink familiy ID / net_dev mapping
 *
 * @id: family ID to remove from the table
 */
void wimax_id_table_rm(int id)
{
	unsigned cnt;
	write_lock(&wimax_id_table_lock);
	for (cnt = 0; cnt < wimax_id_table_size; cnt++)
		if (wimax_id_table[cnt].id == id) {
			wimax_id_table[cnt].id = 0;
			wimax_id_table[cnt].net_dev = NULL;
			break;
		}
	write_unlock(&wimax_id_table_lock);
}


/*
 * Release the gennetlink family id / mapping table
 *
 * On debug, verify that the table is empty upon removal.
 */
void wimax_id_table_release(void)
{
#ifdef CONFIG_BUG
	unsigned cnt;
	for (cnt = 0; cnt < wimax_id_table_size; cnt++) {
		if (wimax_id_table[cnt].id == 0
		    && wimax_id_table[cnt].net_dev == NULL)
			continue;	/* this one is good... */
		printk(KERN_ERR "BUG: %s index %u not cleared (%d/%p)\n",
		       __FUNCTION__, cnt, wimax_id_table[cnt].id,
		       wimax_id_table[cnt].net_dev);
		WARN_ON(1);
	}
#endif
	kfree(wimax_id_table);
}
