/*
 * Linux WiMAX
 * API for the Linux WiMAX stack to talk with kernel space
 *
 *
 * Copyright (C) 2007 Intel Corporation <linux-wimax@intel.com>
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
 * The WiMAX stack talks to user space via Generic Netlink. It
 * provides user space with means to control the WiMAX devices and to
 * get notifications from them. libwimax exports the same calls you
 * see here so you can call them from user space.
 *
 * The interface is standarized so different devices can be controlled
 * via the same interface. However, a vendor/device messaging pipe is
 * provided that can be used to issue commands and receive
 * notifications in free form.
 *
 * Currently this interface is 'empty' with just a few methods; as
 * more WiMAX hardware is made available, it will start taking shape.
 *
 * USAGE
 *
 * Embed a &struct wimax_dev in your device's private structure and
 * register it. For details, see &struct wimax_dev 's documentation.
 *
 * Once this is done, you can use the API provided by wimax-tools's
 * libwimax for communicating with your driver.
 *
 * DESIGN
 *
 * Although not set on final stone, this very basic interface is
 * mostly completed. Remember this is meant to grow as new common
 * operations are decided upon.
 *
 * As of now, the only function of the common layer is to keep common
 * fields handy and to provide a user/kernel conduit, as well as
 * providing for means to check interface versioning.
 *
 * User space must explicitly send an open command to the kernel
 * [use libwimax:wimax_open()] to open a WiMAX handle. That will be
 * used for all the operations with the stack, akin to a file
 * handle. Once done, liwimax:wimax_close() releases the handle.
 *
 * GENERIC NETLINK
 *
 * Each WiMAX network interface gets assigned a unique Generic Netlink
 * Family ID. This makes it easy (and cheap) to map device to family
 * ID considering that most deployments will have a single WiMAX
 * device per system. for details, see file
 * drivers/net/wimax/id-table.c.
 *
 * For most commands, we want to use generic netlink attributes to
 * attach information to the commands/reports. The exception is the
 * messaging interface, which only uses the payload.
 *
 * TESTING FOR THE INTERFACE AND VERSIONING
 *
 * Each WiMAX device exports two sysfs files declaring the generic
 * netlink family ID associated to the interface and another one which
 * version it supports. The version code has to fit in one byte
 * (restrictions imposed by generic netlink); we use version / 10 for
 * the major version and version % 10 for the minor. This gives 9
 * minors for each major and 25 majors.
 *
 * The inexistence of any of this means the device does not support
 * the WiMAX extensions.
 *
 * The version change protocol is as follow:
 *
 * - Major versions: needs to be increased if an existing message is
 *   changed or removed. Doesn't need to be changed if a new message
 *   is added.
 *
 * - Minor verion: needs to be increased if new messages are being
 *   added or some other consideration that doesn't impact too much
 *   (like some kind of bug fix) and that is kind of left up in the
 *   air to common sense.
 *
 * Your user space code should not try to work if the major version it
 * was compiled for differs from what the kernel offers. As well, it
 * should not work if the minor version of the kernel interface is
 * lower than the one the user space code was compiled for.
 *
 * libwimax's wimax_open() takes care of all this for you.
 *
 * THE OPERATIONS:
 *
 * Each operation is defined in its on file (drivers/net/wimax/op-*.c)
 * for clarity. The parts needed for an operation are:
 *
 *  - a function pointer in &struct wimax_dev: optional, as the
 *    operation might be implemented by the stack and not by the
 *    driver.
 *
 *  - a &struct nla_policy to define the attributes of the generic
 *    netlink command
 *
 *  - a &struct genl_ops to define the operation
 *
 * All the declarations for the operation codes (WIMAX_GNL_OP_*) and
 * generic netlink attributes (WIMAX_GNL_%OPNAME%_*) are declared in
 * include/net/wimax.h; this file is intended to be included by user
 * space to gain those declarations.
 *
 * A few caveats to remember:
 *
 *  - Need to define attribute numbers starting in 0
 *
 *  - the &struct genl_family requires a max attribute id (dunno why);
 *    because we have all the attribute IDs spread around, we define
 *    an arbitrary maximum in net/wimax.h:WIMAX_GNL_ATTR_MAX and then
 *    do a build check in each op-*.c file that declares attributes to
 *    make sure that maximum is enough [see example in
 *    wimax_gnl_doit_open()].
 *
 * THE MESSAGING INTERFACE:
 *
 * This interface is kept intentionally simple. User can send a
 * message of X bytes that the kernel receives through
 * wimax_dev->op_msg_from_user(). The kernel can send replies or
 * notifications back with wimax_msg_to_user() that are sent back to
 * the application that opened the pipe.
 *
 * User space allocates two handles, one for user-to-kernel, one for
 * kernel-to-user communication. The reason for the dual handle
 * interface is that it simplifies the ack model (see op-msg.c) and
 * makes it easier for doing multithreading, as many threads can write
 * and one can read without interference.
 *
 * After user space writes a message (always with the %NLM_F_ACK
 * flag), it will wait for an ack from the kernel's netlink
 * layer. User space will call the driver-specific messaging handler
 * [wimax_dev->op_msg_from_user()], which will transmit to user space
 * using wimax_msg_to_user() (which uses the kernel-to-user
 * handler). Once this is done, netlink will send the ACK back on the
 * user-to-kernel handle.
 *
 * RFKILL:
 *
 * RFKILL support is built into the wimax_dev layer; your driver just
 * needs to call wimax_report_rfkill_hw() to inform of changes in the
 * hardware RF kill switch.
 *
 * User space can set the software RF Kill switch by calling
 * wimax_rfkill().
 *
 * The code for now only supports devices that don't require polling;
 * If your device needs to be polled, create a self-rearming delayed
 * work struct for polling or look into adding polled support to the
 * WiMAX stack.
 *
 * When initializing your hardware (_probe), after calling
 * wimax_dev_add(), query your hw for it's RF Kill switches status and
 * feed it back to the WiMAX stack using
 * wimax_report_rfkill_{hw,sw}(). If any switch is missing, always
 * report it as OFF, so the radio is ON.
 */

#ifndef __LINUX__WIMAX_H__
#define __LINUX__WIMAX_H__
#ifdef __KERNEL__

#include <net/wimax.h>
#include <net/genetlink.h>
#include <linux/netdevice.h>

struct net_device;
struct genl_info;
struct wimax_dev;
struct input_dev;


/**
 * struct wimax_dev - Generic WiMAX device
 *
 * @genl_pid: [private] Generic Netlink PID for the kernel-to-user
 *            path. This will be deprecated in favour of using
 *            broadcast. 0 means nobody is connected.
 *
 * @gnl_family: [private] Generic Netlink pipe ID.
 *
 * @net_dev: [private] Pointer to the &struct net_device this WiMAX
 *           device implements.
 *
 * @rfkill: integration into the RF-Kill infrastructure.
 *
 * @rfkill_input: virtual input device to process the hardware RF Kill
 *       switches.
 *
 * @op_msg_from_user: [fill] Driver-specific operation to
 *       handle a raw message from user space to the driver. The
 *       oposite path is done with wimax_msg_to_user().
 *
 * @op_rfkill_sw_toggle: [fill] Driver-specific operation to act on
 *       userspace (or any other agent) requesting the WiMAX device to
 *       change the RF Kill software switch (0 for disable-radio on or
 *       1 for enable, radio off). If such hardware support is not
 *       present, it is assumed the radio cannot be switched on/off
 *       and is always on and thus operation call should always
 *       succeed. In such case, this function pointer can be left as
 *       NULL.
 *
 * @name: [fill] A way to identify this device. We need to register a
 *     name with many subsystems (input for RFKILL, workqueue
 *     creation, etc). We can't use the network device name as that
 *     might change and in some instances we don't know it yet (until
 *     we don't call register_netdev()). So we generate an unique one
 *     using the driver name and device bus id, place it here and use
 *     it across the board. Recommended naming:
 *     BUSNAME:BUSID (dev->bus->name, dev->bus_id).
 *
 * Description:
 *
 * This structure defines a common interface to access all WiMAX
 * devices from different vendors and provides a common API as well as
 * a free-form device-specific messaging channel.
 *
 * The common API is still very limited and is expected to grow as
 * more devices hit the market and common points are found.
 *
 * Usage:
 *
 *  1. Embed a &struct wimax_dev at *the beginning* your network
 *     device structure so that net_dev->priv points to it.
 *
 *  2. memset() it to zero
 *
 *  3. initialize with wimax_dev_init()
 *
 *  4. fill in the op*() function pointers
 *
 *  5. call wimax_dev_add() *after* registering the network device.
 *
 *  6. Find the state of the HW RF kill switch and call
 *     wimax_rfkill_hw_report() to report it. See below.
 *
 * This will populate sysfs entries in the device's directory that tag
 * it as a device that supports the WiMAX interface (for
 * userspace). wimax_dev_rm() undoes before unregistering the network
 * device. Once wimax_dev_add() is called, you can get requests on the
 * op_*() calls.
 *
 * The stack doesn't provide a method to stop calls from it into your
 * private op functions. You need to implement that at your driver's
 * level.
 *
 *   A way to do it is to have a read write semaphore. write-locked
 *   means no operations from the stack are allowed. unlocked or
 *   read-locked allows them.
 *
 *   Have each op do a down_read_trylock() at the beginning; if it
 *   fails, that means the semaphore is write-locked and no operations
 *   should go through. If it succeeds, proceed and up_read() later
 *   on.
 *
 * RFKILL:
 *
 * At initialization time [after calling wimax_dev_add()], have your
 * driver call wimax_rfkill_hw_report() to indicate the state of the
 * hardware RF kill switch. If none, just call it to indicate it is
 * off [radio always on: wimax_rfkill_hw_report(wimax_dev, 0)].
 *
 * Whenever your driver detects a change in the state of the HW RF
 * kill switch, call wimax_rfkill_hw_report().
 */
struct wimax_dev {
	int genl_pid;
	struct genl_family gnl_family;
	int (*op_msg_from_user)(struct wimax_dev *wimax_dev,
				struct sk_buff *skb,
				const struct genl_info *info);
	int (*op_rfkill_sw_toggle)(struct wimax_dev *wimax_dev,
				   enum wimax_rfkill_state);
	struct net_device *net_dev;
	struct rfkill *rfkill;
	struct input_dev *rfkill_input;
	unsigned rfkill_hw:1, rfkill_sw:1;
	char name[32];
};

extern void wimax_dev_init(struct wimax_dev *);
extern int wimax_dev_add(struct wimax_dev *, struct net_device *);
extern void wimax_dev_rm(struct wimax_dev *);

static inline
struct wimax_dev *net_dev_to_wimax(struct net_device *net_dev)
{
	return netdev_priv(net_dev);
}

/* WiMAX stack public API
 *
 * enum wimax_rfkill_state is declared in net/wimax.h so the exports
 * to user space can use it.
 */
extern int wimax_rfkill(struct wimax_dev *, enum wimax_rfkill_state);
extern void wimax_report_rfkill_hw(struct wimax_dev *, enum wimax_rfkill_state);
extern void wimax_report_rfkill_sw(struct wimax_dev *, enum wimax_rfkill_state);


/*
 * Free-form message sending:
 *
 * If using wimax_msg_to_user_{alloc|send}(), be sure not to modify
 * skb->data in the middle (ie: don't use
 * skb_push()/skb_pull()/skb_reserve() on the skb).
 */
extern struct sk_buff *wimax_msg_to_user_alloc(struct wimax_dev *,
					       size_t, gfp_t);
extern int wimax_msg_to_user_send(struct wimax_dev *, struct sk_buff *);
extern int wimax_msg_to_user(struct wimax_dev *, const void *, size_t, gfp_t);

#endif /* #ifdef __KERNEL__ */
#endif /* #ifndef __LINUX__WIMAX_H__ */
