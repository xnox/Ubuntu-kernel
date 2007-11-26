/*******************************************************************************

  Copyright(c) 2004 Intel Corporation. All rights reserved.

  Portions of this file are based on the WEP enablement code provided by the
  Host AP project hostap-drivers v0.1.3
  Copyright (c) 2001-2002, SSH Communications Security Corp and Jouni Malinen
  <jkmaline@cc.hut.fi>
  Copyright (c) 2002-2003, Jouni Malinen <jkmaline@cc.hut.fi>

  This program is free software; you can redistribute it and/or modify it
  under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation.

  This program is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc., 59
  Temple Place - Suite 330, Boston, MA  02111-1307, USA.

  The full GNU General Public License is included in this distribution in the
  file called LICENSE.

  Contact Information:
  James P. Ketrenos <ipw2100-admin@linux.intel.com>
  Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497

*******************************************************************************/

#include <linux/compiler.h>
#include <linux/errno.h>
#include <linux/if_arp.h>
#include <linux/in6.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/pci.h>
#include <linux/skbuff.h>
#include <linux/slab.h>
#include <linux/tcp.h>
#include <linux/types.h>
#include <linux/version.h>
#include <linux/wireless.h>
#include <linux/etherdevice.h>
#include <asm/uaccess.h>
#include <net/arp.h>

#include <net/rtl_ieee80211.h>

MODULE_DESCRIPTION("802.11 data/management/control stack");
MODULE_AUTHOR("Copyright (C) 2004 Intel Corporation <jketreno@linux.intel.com>");
MODULE_LICENSE("GPL");

#define DRV_NAME "ieee80211"

static inline int rtl_ieee80211_networks_allocate(struct rtl_ieee80211_device *ieee)
{
	if (ieee->networks)
		return 0;

	ieee->networks = kmalloc(
		MAX_NETWORK_COUNT * sizeof(struct rtl_ieee80211_network),
		GFP_KERNEL);
	if (!ieee->networks) {
		printk(KERN_WARNING "%s: Out of memory allocating beacons\n",
		       ieee->dev->name);
		return -ENOMEM;
	}

	memset(ieee->networks, 0,
	       MAX_NETWORK_COUNT * sizeof(struct rtl_ieee80211_network));

	return 0;
}

static inline void rtl_ieee80211_networks_free(struct rtl_ieee80211_device *ieee)
{
	if (!ieee->networks)
		return;
	kfree(ieee->networks);
	ieee->networks = NULL;
}

static inline void rtl_ieee80211_networks_initialize(struct rtl_ieee80211_device *ieee)
{
	int i;

	INIT_LIST_HEAD(&ieee->network_free_list);
	INIT_LIST_HEAD(&ieee->network_list);
	for (i = 0; i < MAX_NETWORK_COUNT; i++)
		list_add_tail(&ieee->networks[i].list, &ieee->network_free_list);
}


struct net_device *rtl_alloc_ieee80211(int sizeof_priv)
{
	struct rtl_ieee80211_device *ieee;
	struct net_device *dev;
	int i,err;

	IEEE80211_DEBUG_INFO("Initializing...\n");

	dev = alloc_etherdev(sizeof(struct rtl_ieee80211_device) + sizeof_priv);
	if (!dev) {
		IEEE80211_ERROR("Unable to network device.\n");
		goto failed;
	}
	ieee = netdev_priv(dev);
	dev->hard_start_xmit = rtl_ieee80211_xmit;

	ieee->dev = dev;

	err = rtl_ieee80211_networks_allocate(ieee);
	if (err) {
		IEEE80211_ERROR("Unable to allocate beacon storage: %d\n",
				err);
		goto failed;
	}
	rtl_ieee80211_networks_initialize(ieee);

	/* Default fragmentation threshold is maximum payload size */
	ieee->fts = DEFAULT_FTS;
	ieee->scan_age = DEFAULT_MAX_SCAN_AGE;
	ieee->open_wep = 1;

	/* Default to enabling full open WEP with host based encrypt/decrypt */
	ieee->host_encrypt = 1;
	ieee->host_decrypt = 1;
	ieee->ieee802_1x = 1; /* Default to supporting 802.1x */

	INIT_LIST_HEAD(&ieee->crypt_deinit_list);
	init_timer(&ieee->crypt_deinit_timer);
	ieee->crypt_deinit_timer.data = (unsigned long)ieee;
	ieee->crypt_deinit_timer.function = rtl_ieee80211_crypt_deinit_handler;

	spin_lock_init(&ieee->lock);

 	ieee->wpa_enabled = 0;
 	ieee->tkip_countermeasures = 0;
 	ieee->drop_unencrypted = 0;
 	ieee->privacy_invoked = 0;
 	ieee->ieee802_1x = 1;
	ieee->raw_tx = 0;
	
	rtl_ieee80211_softmac_init(ieee);
	
	for (i = 0; i < IEEE_IBSS_MAC_HASH_SIZE; i++)
		INIT_LIST_HEAD(&ieee->ibss_mac_hash[i]);
	
	ieee->last_seq_num = -1;
	ieee->last_frag_num = -1;
	ieee->last_packet_time = 0;

	return dev;

 failed:
	if (dev)
		free_netdev(dev);
	return NULL;
}


void rtl_free_ieee80211(struct net_device *dev)
{
	struct rtl_ieee80211_device *ieee = netdev_priv(dev);

	int i;
	struct list_head *p, *q;
	
	
	rtl_ieee80211_softmac_free(ieee);
	del_timer_sync(&ieee->crypt_deinit_timer);
	rtl_ieee80211_crypt_deinit_entries(ieee, 1);

	for (i = 0; i < WEP_KEYS; i++) {
		struct rtl_ieee80211_crypt_data *crypt = ieee->crypt[i];
		if (crypt) {
			if (crypt->ops) {
				crypt->ops->deinit(crypt->priv);
				module_put(crypt->ops->owner);
			}
			kfree(crypt);
			ieee->crypt[i] = NULL;
		}
	}

	rtl_ieee80211_networks_free(ieee);
	
	for (i = 0; i < IEEE_IBSS_MAC_HASH_SIZE; i++) {
		list_for_each_safe(p, q, &ieee->ibss_mac_hash[i]) {
			kfree(list_entry(p, struct ieee_ibss_seq, list));
			list_del(p);
		}
	}

	
	free_netdev(dev);
}

extern int rtl_ieee80211_crypto_init(void);
extern void __exit rtl_ieee80211_crypto_deinit(void);

static int __init rtl_ieee80211_init(void)
{
	rtl_ieee80211_crypto_init();

	return 0;
}

static void __exit rtl_ieee80211_exit(void)
{
	rtl_ieee80211_crypto_deinit();
}

module_exit(rtl_ieee80211_exit);
module_init(rtl_ieee80211_init);

EXPORT_SYMBOL(rtl_alloc_ieee80211);
EXPORT_SYMBOL(rtl_free_ieee80211);
