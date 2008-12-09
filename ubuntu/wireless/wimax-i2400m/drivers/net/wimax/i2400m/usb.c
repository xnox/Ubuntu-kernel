/* -*-linux-c-*-
 *
 * Intel Wireless WiMAX Connection 2400m
 * Linux driver model glue for USB device (initialization / shutdown)
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
 * See i2400m-usb.h for a general description of this driver.
 *
 * This file implements driver model glue, firmware upload hookups for
 * when the device reboots and little else.
 *
 * Roadmap for main entry points:
 *
 * i2400m_probe()
 *     alloc_netdev()
 *         i2400m_netdev_setup()
 *             i2400m_netdev_init()
 *     i2400m_dev_bootstrap()
 *     i2400mu_notification_setup()
 *     i2400mu_tx_setup();
 *     i2400m_setup()
 *
 * i2400m_disconnect
 *     ...
 *     i2400m_release()
 *     i2400mu_tx_release()
 *     i2400mu_notification_release()
 *     free_netdev(net_dev)
 *
 */
#include "i2400m-usb.h"
#include <net/wimax-i2400m.h>
#include "../version.h"


#define D_SUBMODULE usb
#include "usb-debug-levels.h"


/*
 * The USB device has rebooted - reupload the firmware
 */
static
void i2400mu_dev_bootstrap_work(struct work_struct *ws)
{
	int result;
	struct i2400m_work *iw = container_of(ws, struct i2400m_work, ws);
	struct i2400m *i2400m = iw->i2400m;
	struct i2400mu *i2400mu = container_of(i2400m, struct i2400mu,
					       i2400m);
	struct device *dev = i2400m_dev(i2400m);

#warning FIXME: there is probably a race here when setting ack_status
	/*
	 * Need a i2400m_ack_status_set() operation; this will update
	 * ack_status only if the completion is armed. We'd really
	 * need a conditional variable for this; now it can be done by
	 * using the completions' wq spinlock. If completion->done is
	 * < 0 then there are waiters. We update ack_status and unlock
	 * the spinlock.
	 */

	i2400m->boot_mode = 1;		/* block calls to _msg_to_dev() */
	if (i2400m->ready == 0) {
		/* We are still in _probe(), we let it do the reinit
		 * thing FIXME: there is probably a race here */
		dev_err(dev, "device rebooted\n");
		i2400m->ack_status = -ERESTARTSYS;
		complete(&i2400m->msg_completion);
		goto out;
	}
	dev_err(dev, "device rebooted, reinitializing\n");
	i2400m->ready = 0;
	netif_stop_queue(i2400m->wimax_dev.net_dev);
	down_write(&i2400m->stack_rwsem);
	i2400m->ack_status = -ERESTARTSYS;
	complete(&i2400m->msg_completion);

	i2400mu_notification_release(i2400mu);	/* Release USB state */
	i2400mu_tx_release(i2400mu);

	i2400mu_init_from_reset(i2400mu);		/* Reinit USB state */
	result = i2400m_dev_bootstrap(i2400m);
	if (result < 0) {
		dev_err(dev, "reboot: cannot bootstrap USB device: %d\n",
			result);
		goto error_reset;
	}
	result = i2400mu_notification_setup(i2400mu);
	if (result < 0) {
		dev_err(dev, "reboot: cannot setup USB notifs: %d\n", result);
		goto error_reset;
	}
	result = i2400mu_tx_setup(i2400mu);
	if (result < 0) {
		dev_err(dev, "reboot: cannot setup USB TX: %d\n", result);
		i2400mu_notification_release(i2400mu);
		goto error_reset;
	}
	result = i2400m_setup_device(i2400m);
	if (result < 0)
		goto error_setup_device;
	result = i2400m_setup_device_post(i2400m);
	if (result < 0)
		goto error_setup_device;
#warning FIXME: send a note to the user saying we reinitialized
	netif_start_queue(i2400m->wimax_dev.net_dev);
	up_write(&i2400m->stack_rwsem);
error_setup_device:
error_reset:		/* Device is dead, don't up the semaphore */
	i2400m_put(i2400m);
	kfree(iw);
out:
	return;
}


/**
 * i2400m_dev_bootstrap_delayed - run a bootstrap in thread context
 *
 * Schedule a device bootstrap out of a thread context, so it is safe
 * to call from atomic context.
 */
int i2400mu_dev_bootstrap_delayed(struct i2400mu *i2400mu)
{
	return i2400m_schedule_work(&i2400mu->i2400m, 0,
				    i2400mu_dev_bootstrap_work,
				    GFP_ATOMIC, NULL, 0);
}


/*
 * Tell the bus layer the i2400m device is ready for suspend
 *
 * This is needed by USB, other buses might not need it
 */
static
void i2400mu_bus_autopm_enable(struct i2400m *i2400m)
{
	struct i2400mu *i2400mu =
		container_of(i2400m, struct i2400mu, i2400m);
	usb_autopm_enable(i2400mu->usb_iface);
}



/*
 * Sends a barker buffer to the device
 *
 * This helper will allocate a kmalloced buffer and use it to transmit
 * (then free it). Reason for this is that other arches cannot use
 * stack/vmalloc/text areas for DMA transfers.
 */
int __i2400mu_send_barker(struct i2400mu *i2400mu,
			  const __le32 *barker,
			  size_t barker_size,
			  unsigned endpoint)
{
	struct usb_endpoint_descriptor *epd = NULL;
	int pipe, actual_len, ret;
	struct device *dev = &i2400mu->usb_iface->dev;
	void *buffer;

	ret = -ENOMEM;
	buffer = kmalloc(barker_size, GFP_KERNEL);
	if (buffer == NULL)
		goto error_kzalloc;
	epd = usb_get_epd(i2400mu->usb_iface, endpoint);
	pipe = usb_sndbulkpipe(i2400mu->usb_dev, epd->bEndpointAddress);
	memcpy(buffer, barker, barker_size);
	ret = usb_bulk_msg(i2400mu->usb_dev, pipe, buffer, barker_size,
			   &actual_len, HZ);
	if (ret < 0)
		d_printf(0, dev, "E: barker error: %d\n", ret);
	else if (actual_len != barker_size) {
		d_printf(0, dev, "E: only %d bytes transmitted\n", actual_len);
		ret = -EIO;
	}
	kfree(buffer);
error_kzalloc:
	return ret;
}


/*
 * Reset a device at different levels
 *
 * Soft and cold resets get a USB reset if they fail.
 */
static
int i2400mu_bus_reset(struct i2400m *i2400m, enum i2400m_reset_type rt)
{
	int result;
	struct i2400mu *i2400mu =
		container_of(i2400m, struct i2400mu, i2400m);
	struct device *dev = i2400m_dev(i2400m);

	if (rt == I2400M_RT_SOFT)
		result = __i2400mu_reset_soft(i2400mu);
	else if (rt == I2400M_RT_COLD)
		result = __i2400mu_reset_cold(i2400mu);
	else if (rt == I2400M_RT_BUS) {
		result = usb_reset_device(i2400mu->usb_dev);
		switch (result) {
		case 0:
		case -ENODEV:
		case -ENOENT:
		case -ESHUTDOWN:
			result = 0;
			break;	/* We assume the device is disconnected */
		default:
			dev_err(dev, "USB reset failed (%d), giving up!\n",
				result);
		}
		goto out;
	} else
		BUG();
	if (result < 0) {	/* only for soft or cold */
		dev_err(dev, "%s reset failed (%d); trying USB reset\n",
			rt == I2400M_RT_SOFT? "soft" : "cold", result);
		result = i2400mu_bus_reset(i2400m, I2400M_RT_BUS);
	}
out:
	return result;
}


static
void i2400mu_netdev_setup(struct net_device *net_dev)
{
	struct i2400m *i2400m = net_dev_to_i2400m(net_dev);
	struct i2400mu *i2400mu = container_of(i2400m, struct i2400mu, i2400m);
	i2400mu_init(i2400mu);
	i2400m_netdev_setup(net_dev);
}


/*
 * Debug levels control; see debug.h
 */
struct d_level d_level[] = {
	D_SUBMODULE_DEFINE(usb),
	D_SUBMODULE_DEFINE(fw),
	D_SUBMODULE_DEFINE(notif),
	D_SUBMODULE_DEFINE(rx),
	D_SUBMODULE_DEFINE(tx),
};
size_t d_level_size = ARRAY_SIZE(d_level);

static
DEVICE_ATTR(i2400m_usb_debug_levels, S_IRUGO | S_IWUSR,
	    d_level_show, d_level_store);


/*
 * Probe a i2400m interface and register it
 *
 * @iface:   USB interface to link to
 * @id:      USB class/subclass/protocol id
 * @returns: 0 if ok, < 0 errno code on error.
 *
 * Does basic housekeeping stuff and then allocs a netdev with space
 * for the i2400m data. Initializes, registers in i2400m, registers in
 * netdev, initialize more stuff that depends on netdev (_net_setup())
 * and ready to go.
 *
 * RX needs no setup and the cleanup is done by
 * i2400m_notification_release() [as the notif subsystem is the one
 * that launches RXs].
 */
static
int i2400mu_probe(struct usb_interface *iface,
		     const struct usb_device_id *id)
{
	int result;
	struct net_device *net_dev;
	struct device *dev = &iface->dev;
	struct i2400m *i2400m;
	struct i2400mu *i2400mu;
	struct usb_device *usb_dev = interface_to_usbdev(iface);

	if (usb_dev->speed != USB_SPEED_HIGH)
		dev_err(dev, "device not connected as high speed\n");

	/* Allocate instance [calls i2400m_netdev_setup() on it]. */
	result = -ENOMEM;
	net_dev = alloc_netdev(sizeof(*i2400mu), "wmx%d",
			       i2400mu_netdev_setup);
	if (net_dev == NULL) {
		dev_err(dev, "no memory for network device instance\n");
		goto error_alloc_netdev;
	}
	SET_NETDEV_DEV(net_dev, dev);
	i2400m = net_dev_to_i2400m(net_dev);
	i2400mu = container_of(i2400m, struct i2400mu, i2400m);
	i2400m->wimax_dev.net_dev = net_dev;
	i2400mu->usb_dev = usb_get_dev(usb_dev);
	i2400mu->usb_iface = iface;
	usb_set_intfdata(iface, i2400mu);

	i2400m->bus_tx_kick = i2400mu_bus_tx_kick;
	i2400m->bus_autopm_enable = i2400mu_bus_autopm_enable;
	i2400m->bus_reset = i2400mu_bus_reset;
	i2400m->bus_bm_cmd_send = i2400mu_bus_bm_cmd_send;
	i2400m->bus_bm_wait_for_ack = i2400mu_bus_bm_wait_for_ack;

	result = i2400m_dev_bootstrap(i2400m);
	if (result < 0) {
		dev_err(dev, "cannot bootstrap device: %d\n", result);
		goto error_bootstrap;
	}
	result = i2400mu_notification_setup(i2400mu);
	if (result < 0)
		goto error_notif_setup;
	result = i2400mu_tx_setup(i2400mu);
	if (result < 0)
		goto error_usb_tx_setup;

	iface->needs_remote_wakeup = 1;		/* autosuspend (0.1s delay) */
	device_init_wakeup(dev, 1);
	usb_autopm_disable(i2400mu->usb_iface);
	usb_dev->autosuspend_delay = 100;
	usb_dev->autosuspend_disabled = 0;

	result = i2400m_setup(i2400m);		/* bus-generic setup */
	if (result < 0) {
		dev_err(dev, "cannot setup device: %d\n", result);
		goto error_setup;
	}
	result = device_create_file(&net_dev->dev,
				    &dev_attr_i2400m_usb_debug_levels);
	if (result < 0) {
		dev_err(dev, "cannot create sysfs debug level control: %d\n",
			result);
		goto error_create_file;
	}
	i2400m->ready = 1;
	return 0;

error_create_file:
	i2400m_release(i2400m);
error_setup:
	i2400mu_tx_release(i2400mu);
error_usb_tx_setup:
	i2400mu_notification_release(i2400mu);
error_notif_setup:
error_bootstrap:
	usb_set_intfdata(iface, NULL);
	usb_put_dev(i2400mu->usb_dev);
	free_netdev(net_dev);
error_alloc_netdev:
	return result;
}


/*
 * Disconect a i2400m from the system.
 *
 * i2400m_stop() has been called before, so al the rx and tx contexts
 * have been taken down already. Make sure the queue is stopped,
 * unregister netdev and i2400m, free and kill.
 */
static
void i2400mu_disconnect(struct usb_interface *iface)
{
	struct i2400mu *i2400mu = usb_get_intfdata(iface);
	struct i2400m *i2400m = &i2400mu->i2400m;
	struct net_device *net_dev = i2400m->wimax_dev.net_dev;
	struct device *dev = &iface->dev;

	d_fnstart(3, dev, "(iface %p i2400m %p)\n", iface, i2400m);
	netif_stop_queue(net_dev);
	down_write(&i2400m->stack_rwsem);

	device_remove_file(&net_dev->dev, &dev_attr_i2400m_usb_debug_levels);
	i2400m_release(i2400m);

	i2400mu_tx_release(i2400mu);
	i2400mu_notification_release(i2400mu);

	usb_set_intfdata(iface, NULL);
	usb_put_dev(i2400mu->usb_dev);
	up_write(&i2400m->stack_rwsem);	/* Avoid BUG: held lock freed */
	free_netdev(net_dev);
	d_fnend(3, dev, "(iface %p i2400m %p) = void\n", iface, i2400m);
}


static
int i2400mu_suspend(struct usb_interface *iface, pm_message_t message)
{
	struct device *dev = &iface->dev;
	struct i2400mu *i2400mu = usb_get_intfdata(iface);

	d_fnstart(3, dev, "(iface %p)\n", iface);
	i2400mu_notification_release(i2400mu);
	d_fnend(3, dev, "(iface %p) = 0\n", iface);
	return 0;
}


static int i2400mu_resume(struct usb_interface *iface)
{
	int ret = 0;
	struct i2400mu *i2400mu = usb_get_intfdata(iface);
	struct device *dev = &iface->dev;

	d_fnstart(3, dev, "(iface %p)\n", iface);
	iface->pm_usage_cnt = 1;
	ret = i2400mu_notification_setup(i2400mu);
	d_fnend(3, dev, "(iface %p) = %d\n", iface, ret);
	return ret;
}


static
struct usb_device_id i2400mu_id_table[] = {
	{ USB_DEVICE(0x8086, 0x1403) },	/* old boot ROM */
	{ USB_DEVICE(0x8086, 0x0180) },	/* new boot ROM */
	{},
};
MODULE_DEVICE_TABLE(usb, i2400mu_id_table);

static
struct usb_driver i2400mu_driver = {
	.name = KBUILD_MODNAME,
	.suspend = i2400mu_suspend,
	.resume = i2400mu_resume,
	.probe = i2400mu_probe,
	.disconnect = i2400mu_disconnect,
	.id_table = i2400mu_id_table,
	.supports_autosuspend = 1,
};

static
int __init i2400mu_driver_init(void)
{
	int result;

	result = usb_register(&i2400mu_driver);
	if (result < 0)
		goto error_usb_register;
	return 0;

error_usb_register:
	return result;
}
module_init(i2400mu_driver_init);


/*
 * Only do a soft reset; we are not interested in the device trying to
 * reconnect (in fact, we'll ignore it).
 */
static
int __i2400m_do_reset_soft(struct device *dev, void *context)
{
	struct usb_interface *usb_iface = to_usb_interface(dev);
	struct i2400mu *i2400mu = usb_get_intfdata(usb_iface);
	__i2400mu_reset_soft(i2400mu);
	return 0;
}

static
void __exit i2400mu_driver_exit(void)
{
	int result;
	result = driver_for_each_device(&i2400mu_driver.drvwrap.driver,
					NULL, NULL, __i2400m_do_reset_soft);
	usb_deregister(&i2400mu_driver);
}
module_exit(i2400mu_driver_exit);

MODULE_AUTHOR("Intel Corporation <linux-wimax@intel.com>");
MODULE_DESCRIPTION("Intel 2400M WiMAX networking for USB");
MODULE_LICENSE("GPL");
MODULE_VERSION(WIMAX_VERSION);
