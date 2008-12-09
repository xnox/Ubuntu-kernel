/* -*-linux-c-*-
 *
 * Intel Wireless WiMAX Connection 2400m
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
 * This implements a pure IP device for the i2400m.
 *
 * TX error handling is tricky; because we have to FIFO/queue the
 * buffers for transmission (as the hardware likes it aggregated), we
 * just give the skb to the TX subsystem and by the time it is
 * transmitted, we have long forgotten about it. So we just don't care
 * too much about it.
 *
 * FIXME: need to move the whole setup to upload firmware on 'ifconfig
 *        DEV up' and to put the device to sleep on 'ifconfig DEV
 *        down'.
 */
#include <linux/if_arp.h>
#include <linux/if_arp-compat.h>	/* @lket@ignore-line */
#include <linux/netdevice.h>
#include "i2400m.h"


#define D_SUBMODULE netdev
#include "debug-levels.h"

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
	int result;
	__le32 *wimax_header;
	struct i2400m *i2400m = net_dev_to_i2400m(net_dev);
	struct device *dev = i2400m_dev(i2400m);

	d_fnstart(3, dev, "(skb %p net_dev %p)\n", skb, net_dev);
	wimax_header = (__le32 *) skb_push(skb, I2400M_WIMAX_HDR_LEN);
	*wimax_header = 0;
	net_dev->trans_start = jiffies;
	d_printf(3, dev, "NETTX: skb %p sending %d bytes to radio\n",
		 skb, skb->len);
	d_dump(4, dev, skb->data, skb->len);
	result = i2400m_tx(i2400m, skb->data, skb->len, I2400M_PT_DATA);
	if (result < 0) {
		netif_stop_queue(net_dev);
		net_dev->stats.tx_dropped++;
		result = NETDEV_TX_BUSY;
	} else {
		net_dev->stats.tx_packets++;
		net_dev->stats.tx_bytes += skb->len;
		result = NETDEV_TX_OK;
		kfree_skb(skb);
	}
	d_fnend(3, dev, "(skb %p net_dev %p) = %d\n", skb, net_dev, result);
	return result;
}


static void i2400m_tx_timeout(struct net_device *net_dev)
{
	/*
	 * We might want to kick the device
	 *
	 * There is not much we can do though, as the device requires
	 * that we send the data aggregated. By the time we receive
	 * this, there might be data pending to be sent or not...
	 */
	net_dev->stats.tx_errors++;
	return;
}


/*
 * i2400m_net_rx - pass a network packet to the stack
 *
 * @i2400m: device instance
 * @skb_rx: the skb where the buffer pointed to by @buf is
 * @buf: pointer to the buffer containing the data
 * @len: buffer's length
 *
 * We just clone the skb and set it up so that it's skb->data pointer
 * points to "buf" and it's length.
 */
void i2400m_net_rx(struct i2400m *i2400m, struct sk_buff *skb_rx,
		   const void *buf, int buf_len)
{
	struct net_device *net_dev = i2400m->wimax_dev.net_dev;
	struct device *dev = i2400m_dev(i2400m);
	struct sk_buff *skb;

	d_fnstart(2, dev, "(i2400m %p buf %p buf_len %d)\n",
		  i2400m, buf, buf_len);
	if ((unsigned long) buf & 0x0f) { /* we like'm 16 byte aligned */
		if (printk_ratelimit())
			dev_err(dev, "NETRX: IP header misaligned (%p)\n",
				buf);
	}
	skb = skb_clone(skb_rx, GFP_KERNEL);
	if (skb == NULL) {
		dev_err(dev, "NETRX: no memory ro clone skb\n"
			"alloc skb failed. returning\n");
		net_dev->stats.rx_dropped++;
		goto error_skb_clone;
	}
	skb_pull(skb, buf - (void *) skb->data);
	skb_trim(skb, (void *) skb->end - buf);
	skb_set_mac_header(skb, 0);
	skb->dev = i2400m->wimax_dev.net_dev;
	skb->protocol = htons(ETH_P_IP);	/* we are IP packets only */
	net_dev->stats.rx_packets++;
	net_dev->stats.rx_bytes += buf_len;
	d_printf(3, dev, "NETRX: receiving %d bytes to network stack\n",
		buf_len);
	d_dump(4, dev, buf, buf_len);
	netif_receive_skb(skb);
error_skb_clone:
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
	d_fnstart(3, NULL, "(net_dev %p)\n", net_dev);
	net_dev->hard_header_len = I2400M_WIMAX_HDR_LEN;
	net_dev->mtu = I2400M_MAX_MTU;
	net_dev->tx_queue_len = I2400M_TX_QLEN;
	net_dev->type = ARPHRD_WIMAX;
	net_dev->features =
		NETIF_F_HIGHDMA | NETIF_F_VLAN_CHALLENGED;
	net_dev->watchdog_timeo = I2400M_TX_TIMEOUT;
	net_dev->open = i2400m_open;
	net_dev->stop = i2400m_stop;
	net_dev->hard_start_xmit = i2400m_hard_start_xmit;
	net_dev->tx_timeout = i2400m_tx_timeout;
	d_fnend(3, NULL, "(net_dev %p) = void\n", net_dev);
}
EXPORT_SYMBOL_GPL(i2400m_netdev_setup);

