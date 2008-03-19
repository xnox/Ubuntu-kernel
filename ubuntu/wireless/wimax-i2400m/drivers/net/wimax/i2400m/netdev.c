/* -*-linux-c-*-
 *
 * Intel Wireless WiMax Connection 2400m
 * Glue with the networking stack
 *
 *
 *
 * Copyright (C) 2007 Intel Corporation <linux-wimax@intel.com>
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
#include <linux/if_arp.h>
#include <linux/netdevice.h>
#include "i2400m.h"

#define D_LOCAL 1
#include "../debug.h"

enum {
/* netdev interface */
	I2400M_MAX_MTU = 1536,
	I2400M_TX_TIMEOUT = HZ,
	I2400M_TX_QLEN = 5,
	I2400M_WIMAX_HDR_LEN = 4,
	I2400M_PAD_16BIT = 2
};


static int i2400m_open(struct net_device *net_dev)
{
	netif_start_queue(net_dev);
	return 0;
}


static int i2400m_stop(struct net_device *net_dev)
{
	netif_stop_queue(net_dev);
	return 0;
}


static int i2400m_hard_start_xmit(struct sk_buff *skb,
				  struct net_device *net_dev)
{
	__le32 *wimax_header;
	struct i2400m *i2400m = net_dev_to_i2400m(net_dev);
	struct device *dev = &i2400m->usb_iface->dev;

	d_fnstart(2, dev, "\n");
	wimax_header = (__le32 *) skb_push(skb, I2400M_WIMAX_HDR_LEN);
	*wimax_header = 0;
	net_dev->trans_start = jiffies;
	d_printf(3, dev, "NETTX: skb %p sending %d bytes to radio\n",
		 skb, skb->len);
	d_dump(4, dev, skb->data, skb->len);
	i2400m_write_async(i2400m, skb, skb->data, skb->len, I2400M_PT_DATA);
	i2400m->wimax_dev.stats.tx_packets++;
	i2400m->wimax_dev.stats.tx_bytes += skb->len;
	d_fnend(2, dev, "returning\n");
	return 0;
}


static void i2400m_tx_timeout(struct net_device *net_dev)
{
	struct wimax_dev *wimax_dev = net_dev_to_wimax(net_dev);
#warning FIXME: review the actions to take on timeout
	/*
	 * We might want to kick the device
	 */
	wimax_dev->stats.tx_errors++;
	return;
}


static struct net_device_stats *i2400m_get_stats(struct net_device *net_dev)
{
	struct wimax_dev *wimax_dev = net_dev_to_wimax(net_dev);
	return &wimax_dev->stats;
}


static int i2400m_do_ioctl(struct net_device *net_dev,
			   struct ifreq *ifr, int cmd)
{
	return 0;
}


/*
 * i2400m_net_rx - pass a network packet to the stack
 *
 * @i2400m: device instance
 * @buf: pointer to the buffer containing the data
 * @len: buffer's length
 */
#warning FIXME: use skb_clone() to avoid copying
#warning FIXME: what about IPv6?
void i2400m_net_rx(struct i2400m *i2400m, void *buf, int buf_len)
{
	struct device *dev = &i2400m->usb_iface->dev;
	struct sk_buff *skb;

	d_fnstart(2, dev, "(i2400m %p buf %p buf_len %d)\n",
		  i2400m, buf, buf_len);
	skb = dev_alloc_skb(buf_len + I2400M_PAD_16BIT);
	if (!skb) {
		dev_err(&i2400m->usb_iface->dev,
			"alloc skb failed. returning\n");
		i2400m->wimax_dev.stats.rx_dropped++;
		goto out;
	}
	skb_reserve(skb, I2400M_PAD_16BIT);	/* align IP on 16B boundary */
	memcpy(skb_put(skb, buf_len), buf, buf_len);
	skb->dev = i2400m->net_dev;
	/* we are IP packets only */
	skb->protocol = htons(ETH_P_IP);
	skb->ip_summed = CHECKSUM_UNNECESSARY;
	i2400m->wimax_dev.stats.rx_packets++;
	i2400m->wimax_dev.stats.rx_bytes += buf_len;
	d_printf(3, dev, "NETRX: receiving %d bytes to network stack\n",
		buf_len);
	d_dump(4, dev, buf, buf_len);
	netif_rx(skb);
out:
	d_fnend(2, dev, "(i2400m %p buf %p buf_len %d) = void\n",
		i2400m, buf, buf_len);
}


/**
 * i2400m_netdev_setup - Setup setup @net_dev's i2400m private data
 *
 * Called by alloc_netdev()
 */
void i2400m_netdev_setup(struct net_device *net_dev)
{
	struct i2400m *i2400m = net_dev_to_i2400m(net_dev);

	d_fnstart(3, NULL, "(net_dev %p)\n", net_dev);
	memset(i2400m, 0, sizeof(*i2400m));
	i2400m_init(i2400m);
	net_dev->hard_header_len = I2400M_WIMAX_HDR_LEN;
	net_dev->mtu = I2400M_MAX_MTU;
	net_dev->tx_queue_len = I2400M_TX_QLEN;
	net_dev->type = ARPHRD_NONE;

	/* With certain configurations, HW offload is breaking and not
	 * computing checksums properly, so we disable it for
	 * now. When ready, add | NETIF_F_HW_CSUM */
	net_dev->features =
		NETIF_F_HIGHDMA | NETIF_F_VLAN_CHALLENGED;
	net_dev->watchdog_timeo = I2400M_TX_TIMEOUT;

	net_dev->open = i2400m_open;	/* All these in netdev.c */
	net_dev->stop = i2400m_stop;
	net_dev->hard_start_xmit = i2400m_hard_start_xmit;
	net_dev->tx_timeout = i2400m_tx_timeout;
	net_dev->get_stats = i2400m_get_stats;
#ifdef __USE_LEGACY_IOCTL
	net_dev->do_ioctl = i2400m_legacy_ioctl;
#else
	net_dev->do_ioctl = i2400m_do_ioctl;
#endif
	d_fnend(3, NULL, "(net_dev %p) = void\n", net_dev);
}

