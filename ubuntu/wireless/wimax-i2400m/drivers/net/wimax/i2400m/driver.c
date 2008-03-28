/* -*-linux-c-*-
 *
 * Intel Wireless WiMax Connection 2400m
 * Linux driver model glue (initialization / shutdown)
 *
 *
 *
 * Copyright (C) 2007-2008 Intel Corporation <linux-wimax@intel.com>
 * Inaky Perez-Gonzalez <inaky.perez-gonzalez@intel.com>
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
 *
 * Roadmap for main driver entry points:
 *
 *
 * i2400m_reset_soft()
 *   __i2400m_send_barker()
 *
 * i2400m_reset_cold()
 *   __i2400m_send_barker()
 *
 * i2400m_probe()
 *     alloc_netdev()
 *         i2400m_netdev_setup()
 *             i2400m_netdev_init()
 *     i2400m_setup()
 *         i2400m_dev_bootstrap() FIXME: will change
 *         i2400m_notification_setup();
 *         i2400m_legacy_ctl_setup();
 *         ...
 *     register_netdev()
 *     wimax_dev_add()
 *     ...
 *
 * i2400m_disconnect
 *     ...
 *     wimax_dev_rm()
 *     unregister_netdev();
 *     i2400m_release();
 *         i2400m_tx_release();
 *         i2400m_notification_release();
 *     free_netdev(net_dev);
 *
 */
#include "i2400m.h"
#include <net/wimax-i2400m.h>
#include "../version.h"

#ifdef __USE_LEGACY_IOCTL
#include "besor_legacy.h"
#endif /* __USE_LEGACY_IOCTL */

#define D_LOCAL 1
#include "../debug.h"


/**
 * i2400m_schedule_work - schedule work on a i2400m
 *
 * @i2400m: device descriptor
 *
 * @fn: function to run to execute work. It gets passed a 'struct
 *      work_struct' that is wrapped in a 'struct i2400m_work'. Once
 *      done, you have to (1) i2400m_put(i2400m_work->i2400m) and then
 *      (2) kfree(i2400m_work).
 *
 * @gfp_flags: GFP flags for memory allocation.
 *
 * We do this quite often, so this just saves typing; allocate a
 * wrapper for a i2400m, get a ref to it launch the work.
 *
 */
int i2400m_schedule_work(struct i2400m *i2400m,
			 void (*fn)(struct work_struct *), gfp_t gfp_flags)
{
	int result;
	struct i2400m_work *iw;

	result = -ENOMEM;
	iw = kzalloc(sizeof(*iw), gfp_flags);
	if (iw == NULL)
		goto error_kzalloc;
	iw->i2400m = i2400m_get(i2400m);
	INIT_WORK(&iw->ws, fn);
	result = schedule_work(&iw->ws);
error_kzalloc:
	return result;

}


static int i2400m_setup(struct i2400m *i2400m)
{
	int result = -ENODEV;
	struct device *dev = &i2400m->usb_iface->dev;

	d_fnstart(3, dev, "(i2400m %p)\n", i2400m);
	result = i2400m_dev_bootstrap(i2400m);
	if (result < 0) {
		dev_err(dev, "cannot bootstrap device: %d\n", result);
		goto error_bootstrap;
	}
	result = i2400m_notification_setup(i2400m);
	if (result < 0)
		goto error_notification_setup;
	/* Fit in here RX setup when we have it */
	result = i2400m_tx_setup(i2400m);
	if (result < 0)
		goto error_tx_setup;
#ifdef __USE_LEGACY_IOCTL
	result = i2400m_legacy_ctl_setup(i2400m);
	if (result < 0) {
		dev_err(dev, "cannot setup i2400m's interrupt endpoint: %d\n",
			result);
		goto error_legacy_ctl_setup;
	}
#endif /* __USE_LEGACY_IOCTL */
	i2400m_diag_off(i2400m);
	d_fnend(3, dev, "(i2400m %p) = %d\n", i2400m, result);
	return result;

#ifdef __USE_LEGACY_IOCTL
error_legacy_ctl_setup:
	i2400m_tx_release(i2400m);
#endif /* __USE_LEGACY_IOCTL */
error_tx_setup:
	i2400m_notification_release(i2400m);
error_notification_setup:
error_bootstrap:
	d_fnend(3, dev, "(i2400m %p) = %d\n", i2400m, result);
	return result;
}


static void i2400m_release(struct i2400m *i2400m)
{
	struct device *dev = &i2400m->usb_iface->dev;

	d_fnstart(3, dev, "(i2400m %p)\n", i2400m);
#ifdef __USE_LEGACY_IOCTL
	i2400m_legacy_ctl_release(i2400m);
#endif /* __USE_LEGACY_IOCTL */
	i2400m_tx_release(i2400m);
	i2400m_rx_release(i2400m);
	i2400m_notification_release(i2400m);
	d_fnend(3, dev, "(i2400m %p) = void\n", i2400m);
}


/**
 * i2400m_reset_cold - do a full device and USB transport reset
 *
 * @i2400m: device descriptor
 *
 * Full internal reset + bus disconnect and reconnect. If it fails, we
 * resort to a USB reset.
 */
void i2400m_reset_cold(struct i2400m *i2400m)
{
	int result;
	struct device *dev = &i2400m->usb_iface->dev;
	result = __i2400m_reset_cold(i2400m);
	if (result >= 0)
		return;
	dev_err(dev, "cold reset failed (%d); trying USB reset\n", result);
	result = usb_reset_device(i2400m->usb_dev);
	switch (result) {
	case 0:
	case -ENODEV:
	case -ENOENT:
	case -ESHUTDOWN:
		break;		/* We assume the device is disconnected */
	default:
		dev_err(dev, "USB reset failed (%d); giving up on resetting "
			"device; please try a physical power cycle\n", result);
	}
}


/**
 * i2400m_reset_soft - soft reset, and if it fails, usb reset
 *
 * @i2400m: device descriptor
 */
void i2400m_reset_soft(struct i2400m *i2400m)
{
	struct device *dev = &i2400m->usb_iface->dev;
	int result = 0;
	
	d_fnstart(4, dev, "(i2400m %p)\n", i2400m);
	result = __i2400m_reset_soft(i2400m);
	if (result >= 0)
		return;
	dev_err(dev, "soft reset failed (%d); trying USB reset\n", result);
	result = usb_reset_device(i2400m->usb_dev);
	switch (result) {
	case 0:
	case -ENODEV:
	case -ENOENT:
	case -ESHUTDOWN:
		break;		/* We assume the device is disconnected */
	default:
		dev_err(dev, "USB reset failed (%d); giving up on resetting "
			"device; please try a physical power cycle\n", result);
	}
}


static
void i2400m_dev_bootstrap_work(struct work_struct *ws)
{
	int result;
	struct i2400m_work *iw = container_of(ws, struct i2400m_work, ws);
	struct i2400m *i2400m = iw->i2400m;
	struct device *dev = &i2400m->usb_iface->dev;

	dev_err(dev, "device rebooted, reinitializing\n");
	netif_stop_queue(i2400m->net_dev);
#warning FIXME: stop the control path
	i2400m_release(i2400m);
	i2400m_init_from_reset(i2400m);
	result = i2400m_setup(i2400m);
	if (result < 0) {
		dev_err(dev, "reboot: can't reinitialize device (%d); "
			"resetting\n", result);
#warning FIXME: enable once USB reset is fixed
#if 0
		usb_reset_dev(i2400m->usb_dev);
#endif
	}
#warning FIXME: send a note to the user saying we reinitialized
	i2400m_put(i2400m);
	kfree(iw);
}


/**
 * i2400m_dev_bootstrap_delayed - run a bootstrap in thread context
 *
 * Schedule a device bootstrap out of a thread context, so it is safe
 * to call from atomic context.
 */
int i2400m_dev_bootstrap_delayed(struct i2400m *i2400m)
{
	return i2400m_schedule_work(i2400m, i2400m_dev_bootstrap_work,
				    GFP_ATOMIC);
}


/*
 * __i2400m_msg_size_check() - verify message size and header are congruent
 *
 * It is ok if the total message size is larger than the expected
 * size, as there can be padding.  
 */ 
static
int __i2400m_msg_size_check(struct i2400m *i2400m,
			    const struct i2400m_l3l4_hdr *l3l4_hdr,
			    size_t msg_size)
{
	int result;
	struct device *dev = &i2400m->usb_iface->dev;
	size_t expected_size;
	d_fnstart(4, dev, "(i2400m %p l3l4_hdr %p msg_size %zu)\n",
		  i2400m, l3l4_hdr, msg_size);
	expected_size = le16_to_cpu(l3l4_hdr->length) + sizeof(*l3l4_hdr);
	if (msg_size < expected_size) {
		dev_err(dev, "bad size for message code 0x%04x "
			"(expected %u, got %u)\n", le16_to_cpu(l3l4_hdr->type),
			expected_size, msg_size);
		result = -EIO;
	}
	else
		result = 0;
	d_fnend(4, dev,
		"(i2400m %p l3l4_hdr %p msg_size %zu) = %d\n",
		i2400m, l3l4_hdr, msg_size, result);
	return result;
}


/**
 * i2400m_op_msg_from_user - relay a message from user space
 *
 *
 * This is a driver specific format that is prefixed with a 'struct
 * i2400m_l3l4_hdr'. This function just verifies that the header
 * declaration and the payload are consistant and then deals with it,
 * either forwarding it to the device or procesing it locally.
 *
 * Driver (and device) expect the messages to be coded in Little
 * Endian.
 */
static
int i2400m_op_msg_from_user(struct wimax_dev *wimax_dev, struct sk_buff *skb,
			    const struct genl_info *genl_info)
{
	int result;
	struct i2400m *i2400m = wimax_dev_to_i2400m(wimax_dev);
	struct device *dev = &i2400m->usb_iface->dev;
	
	d_fnstart(4, dev, "(wimax_dev %p [i2400m %p] skb %p "
		  "genl_info %p)\n", wimax_dev, i2400m, skb, genl_info);
	skb_get(skb);	/* i2400m_write_async() puts it in the callback */
	result = __i2400m_msg_size_check(i2400m, (void *) skb->data, skb->len);
	if (result < 0)
		goto error_bad_size;
	result = i2400m_write_async(i2400m, skb, NULL, 0, I2400M_PT_CTRL);
	if (result >= 0)
		wimax_msg_ack(wimax_dev, skb, GFP_KERNEL, genl_info);
error_bad_size:
	d_fnend(4, dev, "(wimax_dev %p [i2400m %p] skb %p genl_info %p) = %d\n",
		wimax_dev, i2400m, skb, genl_info, result);
	return result;
}


/**
 * i2400m_probe - Probe a i2400m interface and register it
 *
 * @iface:   USB interface to link to
 * @id:      USB class/subclass/protocol id
 * @returns: 0 if ok, < 0 errno code on error.
 *
 * Does basic housekeeping stuff and then allocs a netdev with space
 * for the i2400m data. Initializes, registers in i2400m, registers in
 * netdev, initialize more stuff that depends on netdev (_net_setup())
 * and ready to go.
 */
static int i2400m_probe(struct usb_interface *iface,
			const struct usb_device_id *id)
{
	int result;
	struct net_device *net_dev;
	struct device *dev = &iface->dev;
	struct i2400m *i2400m;
	struct usb_device *usb_dev = interface_to_usbdev(iface);

	if (usb_dev->speed != USB_SPEED_HIGH)
		dev_err(dev, "device not connected as high speed\n");

	/* Allocate instance [calls i2400m_netdev_setup() on it]. */
	result = -ENOMEM;
	net_dev = alloc_netdev(sizeof(*i2400m), "wmx%d", i2400m_netdev_setup);
	if (net_dev == NULL) {
		dev_err(dev, "no memory for network device instance\n");
		goto error_alloc_netdev;
	}
	SET_NETDEV_DEV(net_dev, dev);
	i2400m = net_dev_to_i2400m(net_dev);
	i2400m->net_dev = net_dev;
	i2400m->usb_dev = usb_get_dev(usb_dev);
	i2400m->usb_iface = iface;
	usb_set_intfdata(iface, i2400m);
	result = i2400m_setup(i2400m);
	if (result < 0) {
		dev_err(dev, "cannot setup device: %d\n", result);
		goto error_i2400m_setup;
	}
	result = register_netdev(net_dev);	/* Okey dokey, bring it up */
	if (result < 0) {
		dev_err(dev, "cannot register i2400m network device: %d\n",
			result);
		goto error_register_netdev;
	}
	i2400m->wimax_dev.op_msg_from_user = i2400m_op_msg_from_user;
	result = wimax_dev_add(&i2400m->wimax_dev, net_dev);
	if (result < 0)
		goto error_wimax_dev_add;
	/* Now setup all that requires a registered net device. */
	result = sysfs_create_group(&net_dev->dev.kobj, &i2400m_dev_attr_group);
	if (result < 0) {
		dev_err(dev, "cannot setup i2400m's sysfs: %d\n", result);
		goto error_sysfs_setup;
	}
	return 0;

error_sysfs_setup:
	wimax_dev_rm(&i2400m->wimax_dev, net_dev);
error_wimax_dev_add:
	unregister_netdev(net_dev);
error_register_netdev:
	i2400m_release(i2400m);
error_i2400m_setup:
	usb_set_intfdata(iface, NULL);
	usb_put_dev(i2400m->usb_dev);
	free_netdev(net_dev);
error_alloc_netdev:
	return result;
}


/*
 * i2400m_msg_disconnect - sends a disconnect message to userspace
 *
 * Sent by the driver when device is removed if there is anyone
 * listening; upon receipt of this message, the device is gone and
 * further tries to access it will result in an error.
 */
static
void i2400m_msg_disconnect(struct i2400m *i2400m)
{
	int result;
	struct device *dev = &i2400m->usb_iface->dev;
	struct {
		struct i2400m_l3l4_hdr hdr;
		struct i2400m_tlv_system_state pl;
	} msg;

	memset(&msg, 0, sizeof(msg));
	msg.hdr.type = cpu_to_le16(I2400M_MT_REPORT_STATE);
	msg.hdr.length = cpu_to_le16(sizeof(msg.pl));
	msg.hdr.version = cpu_to_le16(WIMAX_GNL_VERSION);
	msg.hdr.status = 0;
	msg.pl.hdr.type = cpu_to_le16(I2400M_TLV_SYSTEM_STATE);
	msg.pl.hdr.length = cpu_to_le16(sizeof(msg.pl) - sizeof(msg.pl.hdr));
	msg.pl.state = cpu_to_le32(I2400M_ST_SYS_DEVICE_DISCONNECT);

	result = wimax_msg_to_user(&i2400m->wimax_dev, &msg, sizeof(msg),
				   GFP_KERNEL);
	if (result < 0)
		dev_err(dev, "cannot send 'DEVICE-DISCONNECT` "
			"message: %d\n", result);
}


/**
 * i2400m_disconnect - Disconect a i2400m from the system.
 *
 * i2400m_stop() has been called before, so al the rx and tx contexts
 * have been taken down already. Make sure the queue is stopped,
 * unregister netdev and i2400m, free and kill.
 */
static
void i2400m_disconnect(struct usb_interface *iface)
{
	struct i2400m *i2400m = usb_get_intfdata(iface);
	struct net_device *net_dev = i2400m->net_dev;
	struct device *dev = &iface->dev;

	d_fnstart(3, dev, "(iface %p i2400m %p)\n", iface, i2400m);
	netif_stop_queue(net_dev);
	sysfs_remove_group(&net_dev->dev.kobj, &i2400m_dev_attr_group);
	i2400m_msg_disconnect(i2400m);
	wimax_dev_rm(&i2400m->wimax_dev, net_dev);
	unregister_netdev(net_dev);
	i2400m_release(i2400m);
	usb_set_intfdata(iface, NULL);
	usb_put_dev(i2400m->usb_dev);
	free_netdev(net_dev);
	d_fnend(3, dev, "(iface %p i2400m %p) = void\n", iface, i2400m);
}

static
struct usb_device_id i2400m_id_table[] = {
	{USB_DEVICE(0x8086, 0x1403)},
	{},
};
MODULE_DEVICE_TABLE(usb, i2400m_id_table);

static
struct usb_driver i2400m_driver = {
	.name = KBUILD_MODNAME,
	.probe = i2400m_probe,
	.disconnect = i2400m_disconnect,
	.id_table = i2400m_id_table,
};

static
int __init i2400m_driver_init(void)
{
	int result;

	result = usb_register(&i2400m_driver);
	if (result < 0)
		goto error_usb_register;
	return 0;

error_usb_register:
	return result;
}
module_init(i2400m_driver_init);


static
int __i2400m_do_reset_soft(struct device *dev, void *context)
{
	struct usb_interface *usb_iface = to_usb_interface(dev);
	struct i2400m *i2400m = usb_get_intfdata(usb_iface);
	__i2400m_reset_soft(i2400m);
	return 0;
}

static
void __exit i2400m_driver_exit(void)
{
	int result;
	result = driver_for_each_device(&i2400m_driver.drvwrap.driver,
					NULL, NULL, __i2400m_do_reset_soft);
	usb_deregister(&i2400m_driver);
}
module_exit(i2400m_driver_exit);

MODULE_AUTHOR("Inaky Perez-Gonzalez <inaky.perez-gonzalez@intel.com>");
MODULE_DESCRIPTION("Intel 2400M WiMax networking for USB");
MODULE_LICENSE("GPL");
MODULE_VERSION(WIMAX_VERSION);
