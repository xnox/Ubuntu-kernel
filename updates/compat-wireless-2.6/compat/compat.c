/*
 * Copyright 2007	Luis R. Rodriguez <mcgrof@winlab.rutgers.edu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Compatibility file for Linux wireless for kernels 2.6.22 - tip
 * The headers don't need to be modified as we're simply adding them.
 */

#include <net/compat.h>

/* All things not in 2.6.22, 2.6.23 and 2.6.24 */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,25))

/* Backport work for QoS dependencies (kernel/pm_qos_params.c)
 * ipw2100 now makes use of
 * pm_qos_add_requirement(), 
 * pm_qos_update_requirement() and
 * pm_qos_remove_requirement() from it
 *
 * */

/*
 * locking rule: all changes to target_value or requirements or notifiers lists
 * or pm_qos_object list and pm_qos_objects need to happen with pm_qos_lock
 * held, taken with _irqsave.  One lock to rule them all
 */
struct requirement_list {
	struct list_head list;
	union {
		s32 value;
		s32 usec;
		s32 kbps;
	};
	char *name;
};

static s32 max_compare(s32 v1, s32 v2);
static s32 min_compare(s32 v1, s32 v2);

struct pm_qos_object {
	struct requirement_list requirements;
	struct blocking_notifier_head *notifiers;
	struct miscdevice pm_qos_power_miscdev;
	char *name;
	s32 default_value;
	s32 target_value;
	s32 (*comparitor)(s32, s32);
};

static struct pm_qos_object null_pm_qos;
static BLOCKING_NOTIFIER_HEAD(cpu_dma_lat_notifier);
static struct pm_qos_object cpu_dma_pm_qos = {
	.requirements = {LIST_HEAD_INIT(cpu_dma_pm_qos.requirements.list)},
	.notifiers = &cpu_dma_lat_notifier,
	.name = "cpu_dma_latency",
	.default_value = 2000 * USEC_PER_SEC,
	.target_value = 2000 * USEC_PER_SEC,
	.comparitor = min_compare
};

static BLOCKING_NOTIFIER_HEAD(network_lat_notifier);
static struct pm_qos_object network_lat_pm_qos = {
	.requirements = {LIST_HEAD_INIT(network_lat_pm_qos.requirements.list)},
	.notifiers = &network_lat_notifier,
	.name = "network_latency",
	.default_value = 2000 * USEC_PER_SEC,
	.target_value = 2000 * USEC_PER_SEC,
	.comparitor = min_compare
};


static BLOCKING_NOTIFIER_HEAD(network_throughput_notifier);
static struct pm_qos_object network_throughput_pm_qos = {
	.requirements =
		{LIST_HEAD_INIT(network_throughput_pm_qos.requirements.list)},
	.notifiers = &network_throughput_notifier,
	.name = "network_throughput",
	.default_value = 0,
	.target_value = 0,
	.comparitor = max_compare
};


static struct pm_qos_object *pm_qos_array[] = {
	&null_pm_qos,
	&cpu_dma_pm_qos,
	&network_lat_pm_qos,
	&network_throughput_pm_qos
};

static DEFINE_SPINLOCK(pm_qos_lock);

/* static helper functions */
static s32 max_compare(s32 v1, s32 v2)
{
	return max(v1, v2);
}

static s32 min_compare(s32 v1, s32 v2)
{
	return min(v1, v2);
}

static void update_target(int target)
{
	s32 extreme_value;
	struct requirement_list *node;
	unsigned long flags;
	int call_notifier = 0;

	spin_lock_irqsave(&pm_qos_lock, flags);
	extreme_value = pm_qos_array[target]->default_value;
	list_for_each_entry(node,
			&pm_qos_array[target]->requirements.list, list) {
		extreme_value = pm_qos_array[target]->comparitor(
				extreme_value, node->value);
	}
	if (pm_qos_array[target]->target_value != extreme_value) {
		call_notifier = 1;
		pm_qos_array[target]->target_value = extreme_value;
		pr_debug(KERN_ERR "new target for qos %d is %d\n", target,
			pm_qos_array[target]->target_value);
	}
	spin_unlock_irqrestore(&pm_qos_lock, flags);

	if (call_notifier)
		blocking_notifier_call_chain(pm_qos_array[target]->notifiers,
			(unsigned long) extreme_value, NULL);
}


/**
 * pm_qos_add_requirement - inserts new qos request into the list
 * @pm_qos_class: identifies which list of qos request to us
 * @name: identifies the request
 * @value: defines the qos request
 *
 * This function inserts a new entry in the pm_qos_class list of requested qos
 * performance charactoistics.  It recomputes the agregate QoS expectations for
 * the pm_qos_class of parrameters.
 */
int pm_qos_add_requirement(int pm_qos_class, char *name, s32 value)
{
	struct requirement_list *dep;
	unsigned long flags;

	dep = kzalloc(sizeof(struct requirement_list), GFP_KERNEL);
	if (dep) {
		if (value == PM_QOS_DEFAULT_VALUE)
			dep->value = pm_qos_array[pm_qos_class]->default_value;
		else
			dep->value = value;
		dep->name = kstrdup(name, GFP_KERNEL);
		if (!dep->name)
			goto cleanup;

		spin_lock_irqsave(&pm_qos_lock, flags);
		list_add(&dep->list,
			&pm_qos_array[pm_qos_class]->requirements.list);
		spin_unlock_irqrestore(&pm_qos_lock, flags);
		update_target(pm_qos_class);

		return 0;
	}

cleanup:
	kfree(dep);
	return -ENOMEM;
}
EXPORT_SYMBOL_GPL(pm_qos_add_requirement);

/**
 * pm_qos_update_requirement - modifies an existing qos request
 * @pm_qos_class: identifies which list of qos request to us
 * @name: identifies the request
 * @value: defines the qos request
 *
 * Updates an existing qos requierement for the pm_qos_class of parameters along
 * with updating the target pm_qos_class value.
 *
 * If the named request isn't in the lest then no change is made.
 */
int pm_qos_update_requirement(int pm_qos_class, char *name, s32 new_value)
{
	unsigned long flags;
	struct requirement_list *node;
	int pending_update = 0;

	spin_lock_irqsave(&pm_qos_lock, flags);
	list_for_each_entry(node,
		&pm_qos_array[pm_qos_class]->requirements.list, list) {
		if (strcmp(node->name, name) == 0) {
			if (new_value == PM_QOS_DEFAULT_VALUE)
				node->value =
				pm_qos_array[pm_qos_class]->default_value;
			else
				node->value = new_value;
			pending_update = 1;
			break;
		}
	}
	spin_unlock_irqrestore(&pm_qos_lock, flags);
	if (pending_update)
		update_target(pm_qos_class);

	return 0;
}
EXPORT_SYMBOL_GPL(pm_qos_update_requirement);

/**
 * pm_qos_remove_requirement - modifies an existing qos request
 * @pm_qos_class: identifies which list of qos request to us
 * @name: identifies the request
 *
 * Will remove named qos request from pm_qos_class list of parrameters and
 * recompute the current target value for the pm_qos_class.
 */
void pm_qos_remove_requirement(int pm_qos_class, char *name)
{
	unsigned long flags;
	struct requirement_list *node;
	int pending_update = 0;

	spin_lock_irqsave(&pm_qos_lock, flags);
	list_for_each_entry(node,
		&pm_qos_array[pm_qos_class]->requirements.list, list) {
		if (strcmp(node->name, name) == 0) {
			kfree(node->name);
			list_del(&node->list);
			kfree(node);
			pending_update = 1;
			break;
		}
	}
	spin_unlock_irqrestore(&pm_qos_lock, flags);
	if (pending_update)
		update_target(pm_qos_class);
}
EXPORT_SYMBOL_GPL(pm_qos_remove_requirement);


#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(2,6,25) */

/* All things not in 2.6.22 and 2.6.23 */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24))

/* Part of net/ethernet/eth.c as of 2.6.24 */
char *print_mac(char *buf, const u8 *addr)
{
	sprintf(buf, MAC_FMT,
		addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
	return buf;
}
EXPORT_SYMBOL(print_mac);

/* On net/core/dev.c as of 2.6.24 */
int __dev_addr_delete(struct dev_addr_list **list, int *count,
                      void *addr, int alen, int glbl)
{
	struct dev_addr_list *da;

	for (; (da = *list) != NULL; list = &da->next) {
		if (memcmp(da->da_addr, addr, da->da_addrlen) == 0 &&
			alen == da->da_addrlen) {
			if (glbl) {
				int old_glbl = da->da_gusers;
				da->da_gusers = 0;
				if (old_glbl == 0)
					break;
			}
			if (--da->da_users)
				return 0;

			*list = da->next;
			kfree(da);
			(*count)--;
			return 0;
		}
	}
	return -ENOENT;
}

/* On net/core/dev.c as of 2.6.24. This is not yet used by mac80211 but
 * might as well add it */
int __dev_addr_add(struct dev_addr_list **list, int *count,
                   void *addr, int alen, int glbl)
{
	struct dev_addr_list *da;

	for (da = *list; da != NULL; da = da->next) {
		if (memcmp(da->da_addr, addr, da->da_addrlen) == 0 &&
			da->da_addrlen == alen) {
			if (glbl) {
				int old_glbl = da->da_gusers;
				da->da_gusers = 1;
				if (old_glbl)
					return 0;
			}
			da->da_users++;
			return 0;
		}
	}

	da = kmalloc(sizeof(*da), GFP_ATOMIC);
	if (da == NULL)
		return -ENOMEM;
	memcpy(da->da_addr, addr, alen);
	da->da_addrlen = alen;
	da->da_users = 1;
	da->da_gusers = glbl ? 1 : 0;
	da->next = *list;
	*list = da;
	(*count)++;
	return 0;
}

/* 2.6.22 and 2.6.23 have eth_header_cache_update defined as extern in include/linux/etherdevice.h
 * and actually defined in net/ethernet/eth.c but 2.6.24 exports it. Lets export it here */

/**
 * eth_header_cache_update - update cache entry
 * @hh: destination cache entry
 * @dev: network device
 * @haddr: new hardware address
 *
 * Called by Address Resolution module to notify changes in address.
 */
void eth_header_cache_update(struct hh_cache *hh,
                             struct net_device *dev,
                             unsigned char *haddr)
{
	memcpy(((u8 *) hh->hh_data) + HH_DATA_OFF(sizeof(struct ethhdr)),
		haddr, ETH_ALEN);
}
EXPORT_SYMBOL(eth_header_cache_update);

/* 2.6.22 and 2.6.23 have eth_header_cache defined as extern in include/linux/etherdevice.h
 * and actually defined in net/ethernet/eth.c but 2.6.24 exports it. Lets export it here */

/**
 * eth_header_cache - fill cache entry from neighbour
 * @neigh: source neighbour
 * @hh: destination cache entry
 * Create an Ethernet header template from the neighbour.
 */
int eth_header_cache(struct neighbour *neigh, struct hh_cache *hh)
{
	__be16 type = hh->hh_type;
	struct ethhdr *eth;
	const struct net_device *dev = neigh->dev;

	eth = (struct ethhdr *)
	    (((u8 *) hh->hh_data) + (HH_DATA_OFF(sizeof(*eth))));

	if (type == htons(ETH_P_802_3))
		return -1;

	eth->h_proto = type;
	memcpy(eth->h_source, dev->dev_addr, ETH_ALEN);
	memcpy(eth->h_dest, neigh->ha, ETH_ALEN);
	hh->hh_len = ETH_HLEN;
	return 0;
}
EXPORT_SYMBOL(eth_header_cache);

/* 2.6.22 and 2.6.23 have eth_header() defined as extern in include/linux/etherdevice.h
 * and actually defined in net/ethernet/eth.c but 2.6.24 exports it. Lets export it here */

/**
 * eth_header - create the Ethernet header
 * @skb:	buffer to alter
 * @dev:	source device
 * @type:	Ethernet type field
 * @daddr: destination address (NULL leave destination address)
 * @saddr: source address (NULL use device source address)
 * @len:   packet length (<= skb->len)
 *
 *
 * Set the protocol type. For a packet of type ETH_P_802_3 we put the length
 * in here instead. It is up to the 802.2 layer to carry protocol information.
 */
int eth_header(struct sk_buff *skb, struct net_device *dev, unsigned short type,
	       void *daddr, void *saddr, unsigned len)
{
	struct ethhdr *eth = (struct ethhdr *)skb_push(skb, ETH_HLEN);

	if (type != ETH_P_802_3)
		eth->h_proto = htons(type);
	else
		eth->h_proto = htons(len);

	/*
	 *      Set the source hardware address.
	 */

	if (!saddr)
		saddr = dev->dev_addr;
	memcpy(eth->h_source, saddr, dev->addr_len);

	if (daddr) {
		memcpy(eth->h_dest, daddr, dev->addr_len);
		return ETH_HLEN;
	}

	/*
	 *      Anyway, the loopback-device should never use this function...
	 */

	if (dev->flags & (IFF_LOOPBACK | IFF_NOARP)) {
		memset(eth->h_dest, 0, dev->addr_len);
		return ETH_HLEN;
	}

	return -ETH_HLEN;
}

EXPORT_SYMBOL(eth_header);

/* 2.6.22 and 2.6.23 have eth_rebuild_header defined as extern in include/linux/etherdevice.h
 * and actually defined in net/ethernet/eth.c but 2.6.24 exports it. Lets export it here */

/**
 * eth_rebuild_header- rebuild the Ethernet MAC header.
 * @skb: socket buffer to update
 *
 * This is called after an ARP or IPV6 ndisc it's resolution on this
 * sk_buff. We now let protocol (ARP) fill in the other fields.
 *
 * This routine CANNOT use cached dst->neigh!
 * Really, it is used only when dst->neigh is wrong.
 */
int eth_rebuild_header(struct sk_buff *skb)
{
	struct ethhdr *eth = (struct ethhdr *)skb->data;
	struct net_device *dev = skb->dev;

	switch (eth->h_proto) {
#ifdef CONFIG_INET
	case __constant_htons(ETH_P_IP):
		return arp_find(eth->h_dest, skb);
#endif
	default:
		printk(KERN_DEBUG
		       "%s: unable to resolve type %X addresses.\n",
		       dev->name, (int)eth->h_proto);

		memcpy(eth->h_source, dev->dev_addr, ETH_ALEN);
		break;
	}

	return 0;
}
EXPORT_SYMBOL(eth_rebuild_header);

/* 2.6.24 will introduce struct pci_dev is_pcie bit. To help
 * with the compatibility code (compat.diff) being smaller, we provide a helper
 * so in cases where that will be used we can simply slap ifdefs with this
 * routine. Use compat_ prefex to not pollute namespace.  */
int compat_is_pcie(struct pci_dev *pdev)
{
	int cap;
	cap = pci_find_capability(pdev, PCI_CAP_ID_EXP);
	return cap ? 1 : 0;
}
EXPORT_SYMBOL(compat_is_pcie);

#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24) */

/* All things not in 2.6.22 */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,23))

/* Part of net/core/dev_mcast.c as of 2.6.23. This is a slightly different version.
 * Since da->da_synced is not part of 2.6.22 we need to take longer route when 
 * syncing */

/**
 *	dev_mc_sync	- Synchronize device's multicast list to another device
 *	@to: destination device
 *	@from: source device
 *
 * 	Add newly added addresses to the destination device and release
 * 	addresses that have no users left. The source device must be
 * 	locked by netif_tx_lock_bh.
 *
 *	This function is intended to be called from the dev->set_multicast_list
 *	function of layered software devices.
 */
int dev_mc_sync(struct net_device *to, struct net_device *from)
{
	struct dev_addr_list *da, *next, *da_to;
	int err = 0;

	netif_tx_lock_bh(to);
	da = from->mc_list;
	while (da != NULL) {
		int synced = 0;
		next = da->next;
		da_to = to->mc_list;
		/* 2.6.22 does not have da->da_synced so lets take the long route */
		while (da_to != NULL) {
			if (memcmp(da_to->da_addr, da->da_addr, da_to->da_addrlen) == 0 &&
				da->da_addrlen == da_to->da_addrlen)
				synced = 1;
				break;
		}
		if (!synced) {
			err = __dev_addr_add(&to->mc_list, &to->mc_count,
					     da->da_addr, da->da_addrlen, 0);
			if (err < 0)
				break;
			da->da_users++;
		} else if (da->da_users == 1) {
			__dev_addr_delete(&to->mc_list, &to->mc_count,
					  da->da_addr, da->da_addrlen, 0);
			__dev_addr_delete(&from->mc_list, &from->mc_count,
					  da->da_addr, da->da_addrlen, 0);
		}
		da = next;
	}
	if (!err)
		__dev_set_rx_mode(to);
	netif_tx_unlock_bh(to);

	return err;
}
EXPORT_SYMBOL(dev_mc_sync);


/* Part of net/core/dev_mcast.c as of 2.6.23. This is a slighty different version. 
 * Since da->da_synced is not part of 2.6.22 we need to take longer route when 
 * unsyncing */

/**
 *      dev_mc_unsync   - Remove synchronized addresses from the destination
 *			  device
 *	@to: destination device
 *	@from: source device
 *
 *	Remove all addresses that were added to the destination device by
 *	dev_mc_sync(). This function is intended to be called from the
 *	dev->stop function of layered software devices.
 */
void dev_mc_unsync(struct net_device *to, struct net_device *from)
{
	struct dev_addr_list *da, *next, *da_to;

	netif_tx_lock_bh(from);
	netif_tx_lock_bh(to);

	da = from->mc_list;
	while (da != NULL) {
		bool synced = false;
		next = da->next;
		da_to = to->mc_list;
		/* 2.6.22 does not have da->da_synced so lets take the long route */
		while (da_to != NULL) {
			if (memcmp(da_to->da_addr, da->da_addr, da_to->da_addrlen) == 0 &&
				da->da_addrlen == da_to->da_addrlen)
				synced = true;
				break;
		}
		if (!synced) {
			da = next;
			continue;
		}
		__dev_addr_delete(&to->mc_list, &to->mc_count,
			da->da_addr, da->da_addrlen, 0);
		__dev_addr_delete(&from->mc_list, &from->mc_count,
			da->da_addr, da->da_addrlen, 0);
		da = next;
	}
	__dev_set_rx_mode(to);

	netif_tx_unlock_bh(to);
	netif_tx_unlock_bh(from);
}
EXPORT_SYMBOL(dev_mc_unsync);

/* Added as of 2.6.23 on net/core/dev.c. Slightly modifed, no dev->set_rx_mode on
 * 2.6.22 so ignore that. */

/*
 *	Upload unicast and multicast address lists to device and
 *	configure RX filtering. When the device doesn't support unicast
 *	filtering it is put in promiscous mode while unicast addresses
 *	are present.
 */
void __dev_set_rx_mode(struct net_device *dev)
{
	/* dev_open will call this function so the list will stay sane. */
	if (!(dev->flags&IFF_UP))
		return;

	if (!netif_device_present(dev))
		return;

/* This needs to be ported to 2.6.22 framework */
#if 0
	/* Unicast addresses changes may only happen under the rtnl,
	 * therefore calling __dev_set_promiscuity here is safe.
	 */
	if (dev->uc_count > 0 && !dev->uc_promisc) {
		__dev_set_promiscuity(dev, 1);
		dev->uc_promisc = 1;
	} else if (dev->uc_count == 0 && dev->uc_promisc) {
		__dev_set_promiscuity(dev, -1);
		dev->uc_promisc = 0;
	}
#endif

	if (dev->set_multicast_list)
		dev->set_multicast_list(dev);
}

#ifdef PCI_DISABLE_MWI
int pci_try_set_mwi(struct pci_dev *dev)
{
	return 0;
}
EXPORT_SYMBOL(pci_try_set_mwi);
#else
/**
 * pci_try_set_mwi - enables memory-write-invalidate PCI transaction
 * @dev: the PCI device for which MWI is enabled
 *
 * Enables the Memory-Write-Invalidate transaction in %PCI_COMMAND.
 * Callers are not required to check the return value.
 *
 * RETURNS: An appropriate -ERRNO error value on error, or zero for success.
 */
int pci_try_set_mwi(struct pci_dev *dev)
{
	int rc = pci_set_mwi(dev);
	return rc;
}
EXPORT_SYMBOL(pci_try_set_mwi);
#endif


#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(2,6,23)) */

