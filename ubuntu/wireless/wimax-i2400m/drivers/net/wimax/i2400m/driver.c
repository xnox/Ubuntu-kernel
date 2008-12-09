/*
 * Intel Wireless WiMAX Connection 2400m
 * Miscellaneous generic operations
 *
 *
 *
 * Copyright (C) 2007-2008 Intel Corporation <linux-wimax@intel.com>
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
 * See i2400m.h for driver documentation. This contains helpers for
 * the driver model glue and the WiMAX stack ops.
 *
 * ROADMAP:
 *
 * i2400m_op_msg_from_user()
 *     i2400m_msg_to_dev()
 *     wimax_msg_to_user_send()
 *
 * i2400m_setup()
 *     i2400m_tx_setup()
 *     i2400m_setup_device()
 *     register_netdev()
 *     wimax_dev_add()
 *     i2400m_setup_device_post()
 *
 * i2400m_release()
 *     i2400m_msg_disconnect()
 *     wimax_dev_rm()
 *     unregister_netdev()
 */
#include "i2400m.h"
#include <net/wimax-i2400m.h>
#include "../version.h"


#define D_SUBMODULE driver
#include "debug-levels.h"


/**
 * i2400m_schedule_work - schedule work on a i2400m
 *
 * @i2400m: device descriptor
 *
 * @devices: use the device's workqueue (i2400m->work_queue) (!0) or
 *     the system's one (0).
 *
 * @fn: function to run to execute work. It gets passed a 'struct
 *     work_struct' that is wrapped in a 'struct i2400m_work'. Once
 *     done, you have to (1) i2400m_put(i2400m_work->i2400m) and then
 *     (2) kfree(i2400m_work).
 *
 * @gfp_flags: GFP flags for memory allocation.
 *
 * @pl: pointer to a payload buffer that you want to pass to the _work
 *     function. Use this to pack (for example) a struct with extra
 *     arguments.
 *
 * @pl_size: size of the payload buffer.
 *
 * We do this quite often, so this just saves typing; allocate a
 * wrapper for a i2400m, get a ref to it, pack arguments and launch
 * the work.
 *
 * A usual workflow is:
 *
 * struct my_work_args {
 *         void *something;
 *         int whatever;
 * };
 * ...
 *
 * struct my_work_args my_args = {
 *         .something = FOO,
 *         .whaetever = BLAH
 * };
 * i2400m_schedule_work(i2400m, 1, my_work_function, GFP_KERNEL,
 *                      &args, sizeof(args))
 *
 * And now the work function can unpack the arguments and call the
 * real function (or do the job itself):
 *
 * static
 * void my_work_fn((struct work_struct *ws)
 * {
 *         struct i2400m_work *iw =
 *	           container_of(ws, struct i2400m_work, ws);
 *	   struct my_work_args *my_args = (void *) iw->pl;
 *
 *	   my_work(iw->i2400m, my_args->something, my_args->whatevert);
 * }
 *
 */
int i2400m_schedule_work(struct i2400m *i2400m, unsigned devices,
			 void (*fn)(struct work_struct *), gfp_t gfp_flags,
			 const void *pl, size_t pl_size)
{
	int result;
	struct i2400m_work *iw;

	result = -ENOMEM;
	iw = kzalloc(sizeof(*iw) + pl_size, gfp_flags);
	if (iw == NULL)
		goto error_kzalloc;
	iw->i2400m = i2400m_get(i2400m);
	memcpy(iw->pl, pl, pl_size);
	INIT_WORK(&iw->ws, fn);
	if (devices)
		result = queue_work(i2400m->work_queue, &iw->ws);
	else
		result = schedule_work(&iw->ws);
error_kzalloc:
	return result;
}
EXPORT_SYMBOL_GPL(i2400m_schedule_work);


/*
 * WiMAX stack operation: relay a message from user space
 *
 * @wimax_dev: device descriptor
 * @skb: skb where the message has been received; skb->data is
 *       expected to point to the message payload.
 * @genl_info: passed by the generic netlink layer
 *
 * Generic Netlink will call this function when a message is sent from
 * userspace.
 *
 * This is a driver specific message format that is prefixed with a
 * 'struct i2400m_l3l4_hdr'. Driver (and device) expect the messages
 * to be coded in Little Endian.
 *
 * This function just verifies that the header declaration and the
 * payload are consistent and then deals with it, either forwarding it
 * to the device or procesing it locally.
 *
 * In the i2400m, messages are basically commands that will carry an
 * ack, so we use i2400m_msg_to_dev() and then deliver the ack back to
 * user space. Reports from the device are processed and sent to user
 * in rx.c, where we grok them.
 */
static
int i2400m_op_msg_from_user(struct wimax_dev *wimax_dev, struct sk_buff *skb,
			    const struct genl_info *genl_info)
{
	int result;
	struct i2400m *i2400m = wimax_dev_to_i2400m(wimax_dev);
	struct device *dev = i2400m_dev(i2400m);
	struct sk_buff *ack_skb;

	d_fnstart(4, dev, "(wimax_dev %p [i2400m %p] skb %p "
		  "genl_info %p)\n", wimax_dev, i2400m, skb, genl_info);
	if (!down_read_trylock(&i2400m->stack_rwsem))
		return -EUNATCH;
	ack_skb = i2400m_msg_to_dev(i2400m, skb->data, skb->len);
	result = PTR_ERR(ack_skb);
	if (IS_ERR(ack_skb))
		goto error_msg_to_dev;
	result = wimax_msg_to_user_send(&i2400m->wimax_dev, ack_skb);
error_msg_to_dev:
	up_read(&i2400m->stack_rwsem);
	d_fnend(4, dev, "(wimax_dev %p [i2400m %p] skb %p genl_info %p) = %d\n",
		wimax_dev, i2400m, skb, genl_info, result);
	return result;
}


/**
 * i2400m_setup_mac_addr - Install the device's MAC addr
 *
 * @i2400m: device descriptor
 * @mac_addr: 6-byte MAC addresss
 *
 * Given a MAC address, update the internal concept of it.
 *
 * This HAS TO BE CALLED by the _probe() method of your driver;
 * normally it is done before uploading firmware.
 */
void i2400m_setup_mac_addr(struct i2400m *i2400m, const u8 mac_addr[ETH_ALEN])
{
	int result = -ENODEV;
	struct device *dev = i2400m_dev(i2400m);
	struct net_device *net_dev = i2400m->wimax_dev.net_dev;

	d_fnstart(3, dev,
		  "(i2400m %p mac_addr %02x:%02x:%02x:%02x:%02x:%02x) = %d\n",
		  i2400m, mac_addr[0], mac_addr[1],
		  mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5], result);
	net_dev->addr_len = ETH_ALEN;
	memcpy(net_dev->perm_addr, mac_addr, ETH_ALEN);
	memcpy(net_dev->dev_addr, mac_addr, ETH_ALEN);
	d_fnend(3, dev,
		"(i2400m %p mac_addr %02x:%02x:%02x:%02x:%02x:%02x) = %d\n",
		i2400m, mac_addr[0], mac_addr[1],
		mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5], result);
}
EXPORT_SYMBOL_GPL(i2400m_setup_mac_addr);


/**
 * i2400m_setup_device - Setup the device after firmware upload
 *
 * @i2400m: device descriptor
 *
 * Returns: 0 if ok, < 0 errno code on error.
 *
 * Sends an initialization command, gathers the MAC addr, etc
 *
 * This setup is only for stuff that doesn't require a netdevice and a
 * wimax_dev to be up and running. Initialization stuff that requires
 * that is done at i2400m_setup_device_post().
 */
int i2400m_setup_device(struct i2400m *i2400m)
{
	int result = -ENODEV;
	struct device *dev = i2400m_dev(i2400m);
	struct sk_buff *skb;

	d_fnstart(3, dev, "(i2400m %p)\n", i2400m);
	result = i2400m_set_init_config(i2400m);
	if (result < 0)
		goto error;
	result = i2400m_cmd_init(i2400m);
	if (result < 0)
		goto error;
	i2400m_cmd_diag_off(i2400m);
	/* Extract firmware info */
	result = i2400m_firmware_check(i2400m);
	if (result < 0)
		goto error;
	/* We do this here instead of _netdev_setup(), as if this
	 * fails, we can bail out */
	skb = i2400m_get_device_info(i2400m);
	if (IS_ERR(skb)) {
		result = PTR_ERR(skb);
		goto error;
	}
	/* some stuff might be missing here...*/
error:
	d_fnend(3, dev, "(i2400m %p) = %d\n", i2400m, result);
	return result;
}
EXPORT_SYMBOL_GPL(i2400m_setup_device);


/**
 * i2400m_setup_device_post - Setup after firmware upload & registration
 *
 * @i2400m: device descriptor
 *
 * Returns: 0 if ok, < 0 errno code on error.
 *
 * Sets up stuff that requires a netdevice and a wimax_dev to be up
 * and running. Here it just calls a get state; parsing the result
 * (RF Status TLV) will set the hardware and software RF-Kill status.
 */
int i2400m_setup_device_post(struct i2400m *i2400m)
{
	int result;
	struct device *dev = i2400m_dev(i2400m);

	d_fnstart(3, dev, "(i2400m %p)\n", i2400m);

	result = i2400m_cmd_get_state(i2400m);

	d_fnend(3, dev, "(i2400m %p) = %d\n", i2400m, result);
	return result;
}
EXPORT_SYMBOL_GPL(i2400m_setup_device_post);


/**
 * i2400m_setup - bus-generic setup function for the i2400m device
 *
 * @i2400m: device descriptor (bus-specific parts have been initialized)
 *
 * Returns: 0 if ok, < 0 errno code on error.
 *
 * Initializes the bus-generic parts of the i2400m driver; the
 * bus-specific parts have been initialized, function pointers filled
 * out by the bus-specific probe function.
 *
 * As well, this registers the WiMAX and net device nodes. Once this
 * function returns, the device is operative and has to be ready to
 * receive and send network traffic and WiMAX control operations.
 */
int i2400m_setup(struct i2400m *i2400m)
{
	int result = -ENODEV;
	struct device *dev = i2400m_dev(i2400m);
	struct wimax_dev *wimax_dev = &i2400m->wimax_dev;
	struct net_device *net_dev = i2400m->wimax_dev.net_dev;

	d_fnstart(3, dev, "(i2400m %p)\n", i2400m);

	snprintf(wimax_dev->name, sizeof(wimax_dev->name),
		 "%s:%s", dev->bus->name, dev->bus_id);

	i2400m->work_queue = create_freezeable_workqueue(wimax_dev->name);
	if (i2400m->work_queue == NULL) {
		result = -ENOMEM;
		dev_err(dev, "cannot create workqueue\n");
		goto error_create_workqueue;
	}
	result = i2400m_tx_setup(i2400m);
	if (result < 0)
		goto error_tx_setup;
	result = i2400m_setup_device(i2400m);
	if (result < 0)
		goto error_setup_device;
	result = register_netdev(net_dev);	/* Okey dokey, bring it up */
	if (result < 0) {
		dev_err(dev, "cannot register i2400m network device: %d\n",
			result);
		goto error_register_netdev;
	}
	netif_carrier_off(net_dev);

	i2400m->wimax_dev.op_msg_from_user = i2400m_op_msg_from_user;
	i2400m->wimax_dev.op_rfkill_sw_toggle = i2400m_op_rfkill_sw_toggle;
	result = wimax_dev_add(&i2400m->wimax_dev, net_dev);
	if (result < 0)
		goto error_wimax_dev_add;
	/* Now setup all that requires a registered net and wimax device. */
	result = sysfs_create_group(&net_dev->dev.kobj, &i2400m_dev_attr_group);
	if (result < 0) {
		dev_err(dev, "cannot setup i2400m's sysfs: %d\n", result);
		goto error_sysfs_setup;
	}
	result = i2400m_setup_device_post(i2400m);
	if (result < 0)
		goto error_setup_device_post;
	i2400m->ready = 1;
	d_fnend(3, dev, "(i2400m %p) = %d\n", i2400m, result);
	return result;

error_setup_device_post:
	sysfs_remove_group(&i2400m->wimax_dev.net_dev->dev.kobj,
			   &i2400m_dev_attr_group);
error_sysfs_setup:
	wimax_dev_rm(&i2400m->wimax_dev);
error_wimax_dev_add:
	unregister_netdev(net_dev);
error_register_netdev:
error_setup_device:
	i2400m_tx_release(i2400m);
error_tx_setup:
	destroy_workqueue(i2400m->work_queue);
error_create_workqueue:
	d_fnend(3, dev, "(i2400m %p) = %d\n", i2400m, result);
	return result;
}
EXPORT_SYMBOL_GPL(i2400m_setup);


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
	struct device *dev = i2400m_dev(i2400m);
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
	msg.pl.state = cpu_to_le32(I2400M_SYSTEM_STATE_DEVICE_DISCONNECT);

	result = wimax_msg_to_user(&i2400m->wimax_dev, &msg, sizeof(msg),
				   GFP_KERNEL);
	if (result < 0)
		dev_err(dev, "cannot send 'DEVICE-DISCONNECT` "
			"message: %d\n", result);
}


/**
 * i2400m_release - release the bus-generic driver resources
 *
 * Sends a disconnect message and undoes any setup done by i2400m_setup()
 */
void i2400m_release(struct i2400m *i2400m)
{
	struct device *dev = i2400m_dev(i2400m);

	d_fnstart(3, dev, "(i2400m %p)\n", i2400m);
	sysfs_remove_group(&i2400m->wimax_dev.net_dev->dev.kobj,
			   &i2400m_dev_attr_group);
	i2400m_msg_disconnect(i2400m);
	wimax_dev_rm(&i2400m->wimax_dev);
	unregister_netdev(i2400m->wimax_dev.net_dev);
	destroy_workqueue(i2400m->work_queue);
	d_fnend(3, dev, "(i2400m %p) = void\n", i2400m);
}
EXPORT_SYMBOL_GPL(i2400m_release);


static
int __init i2400m_driver_init(void)
{
	return 0;
}
module_init(i2400m_driver_init);

static
void __exit i2400m_driver_exit(void)
{
	return;
}
module_exit(i2400m_driver_exit);

MODULE_AUTHOR("Intel Corporation <linux-wimax@intel.com>");
MODULE_DESCRIPTION("Intel 2400M WiMAX networking bus-generic driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(WIMAX_VERSION);
