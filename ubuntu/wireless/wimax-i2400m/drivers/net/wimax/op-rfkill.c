/*
 * Intel Wireless WiMAX Connection 2400m
 * RF-kill framework integration
 *
 *
 * Copyright (C) 2008 Intel Corporation <linux-wimax@intel.com>
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
 * FIXME DOCS
 *
 * We create a non-polled generic device embedded into the WiMAX
 * subsystem.
 *
 * FIXME: implement polled support?
 *
 * All device drivers have to do is after wimax_dev_init(), call
 * wimax_report_rfkill_hw() to update initial state and every time it
 * changes. See wimax.h:struct wimax_dev for more information.
 */

#include <config.h>	/* RFKILL supports WiMAX? @lket@ignore-file */
#include <linux/wimax.h>
#include <net/genetlink.h>
#include <net/wimax.h>
#include <linux/rfkill.h>
#include <linux/input.h>
#include "wimax-internal.h"

#define D_SUBMODULE op_rfkill
#include "debug-levels.h"

#ifdef CONFIG_RFKILL




/**
 * wimax_report_rfkill_hw - report hardware RF kill switch state
 *
 * @wimax_dev: device descriptor
 * @state: new state of the RF Kill switch. WIMAX_RFKILL_OFF disabled
 *     (radio on), WIMAX_RFKILL_ON enabled (radio off).
 *
 * When the hardware detects a change in the state of the RF switch,
 * it must call this function to let the generic WiMAX driver that the
 * state has changed so it can be properly propagated.
 *
 * The WiMAX stack caches the state (the driver doesn't need to). As
 * well, as the change is propagated it will come back as a request to
 * change the software state to mirror the hardware state.
 *
 * If your hardware doesn't have a hardware kill switch, just report
 * it on initialization as disabled (WIMAX_RFKILL_OFF, radio on).
 */
void wimax_report_rfkill_hw(struct wimax_dev *wimax_dev,
			    enum wimax_rfkill_state state)
{
	struct device *dev = &wimax_dev->net_dev->dev;
	d_fnstart(3, dev, "(wimax_dev %p state %u)\n", wimax_dev, state);
	if (state != wimax_dev->rfkill_hw) {
		wimax_dev->rfkill_hw = state? 0 : 1;
		input_report_key(wimax_dev->rfkill_input, KEY_WIMAX, state);
	}
	d_fnend(3, dev, "(wimax_dev %p state %u) = void\n", wimax_dev, state);
}
EXPORT_SYMBOL_GPL(wimax_report_rfkill_hw);


/**
 * wimax_report_rfkill_sw - report software RF kill switch state
 *
 * @wimax_dev: device descriptor
 * @state: new state of the RF Kill switch. WIMAX_RFKILL_OFF disabled
 *     (radio on), WIMAX_RFKILL_ON enabled (radio off).
 *
 * Report to the stack changes in the software RF Kill status.
 *
 * In theory these request always comes from software, so this serves
 * as a feedback loop from the driver confirming the change happened.
 * This is not always so, and not all devices report the change when
 * we drive it.
 *
 * In any case, the main use is during initialization, so the driver
 * can query the device for its current SW RF Kill switch state and
 * feed it to the system.
 */
void wimax_report_rfkill_sw(struct wimax_dev *wimax_dev,
			    enum wimax_rfkill_state state)
{
	struct device *dev = &wimax_dev->net_dev->dev;
	d_fnstart(3, dev, "(wimax_dev %p state %u)\n", wimax_dev, state);
	if (state != wimax_dev->rfkill_sw)
		wimax_dev->rfkill_sw = state? 0 : 1;
	d_fnend(3, dev, "(wimax_dev %p state %u) = void\n", wimax_dev, state);
}
EXPORT_SYMBOL_GPL(wimax_report_rfkill_sw);


/*
 * Callback for the RF Kill toggle operation
 *
 * This function is called by:
 *
 * - The rfkill subsystem when the RF-Kill key is pressed in the
 *   hardware and the driver notifies through
 *   wimax_report_rfkill_hw(). The rfkill subsystem ends up calling back
 *   here so the software RF Kill switch state is changed to reflect
 *   the hardware switch state.
 *
 * - When the user sets the state through sysfs' rfkill/state file
 *
 * - When the user calls wimax_rfkill().
 *
 *
 * WARNING! When we call rfkill_unregister(), this will be called with
 * state 0!
 */
static
int wimax_rfkill_toggle_radio(void *data, enum rfkill_state state)
{
	int result = 0;
	struct wimax_dev *wimax_dev = data;
	struct device *dev = &wimax_dev->net_dev->dev;
	d_fnstart(3, dev, "(wimax_dev %p state %u)\n", wimax_dev, state);
	if (wimax_dev->rfkill_sw != state) {
		result = wimax_dev->op_rfkill_sw_toggle?
			wimax_dev->op_rfkill_sw_toggle(wimax_dev, state)
			: 0;
		if (result >= 0) {
			result = 0;
			wimax_dev->rfkill_sw = state;
		}
	}
	d_fnend(3, dev, "(wimax_dev %p state %u) = %d\n",
		wimax_dev, state, result);
	return result;
}


/**
 * wimax_rfkill - Set the SW RF Kill switch for a wimax device
 *
 * @wimax_dev: device descriptor
 * @state: new state. If state is WIMAX_RFKILL_QUERY, it won't set a
 *     new state, only return the current one.
 *
 * Returns: >= 0 toggle state if ok, < 0 errno code on error. The
 *     toggle state is returned as:
 *
 *       bit 0: HW RF Kill state
 *       bit 1: SW RF Kill toggle state
 *
 *     0 means disabled (radio on), 1 means enabled radio off.
 *
 * Called by the user when he wants to request the WiMAX radio to be
 * switched on or off. If the
 */
int wimax_rfkill(struct wimax_dev *wimax_dev, enum wimax_rfkill_state state)
{
	int result;
	if (state == WIMAX_RFKILL_ON || state == WIMAX_RFKILL_OFF) {
		result = wimax_rfkill_toggle_radio(wimax_dev, state);
		if (result < 0)
			return result;
	} else if (state != WIMAX_RFKILL_QUERY)
		return -EINVAL;
	return wimax_dev->rfkill_sw << 1 | wimax_dev->rfkill_hw;
}
EXPORT_SYMBOL(wimax_rfkill);


/*
 * Register a new WiMAX device's RF Kill support
 */
int wimax_rfkill_add(struct wimax_dev *wimax_dev)
{
	int result;
	struct rfkill *rfkill;
	struct input_dev *input_dev;
	struct device *dev = &wimax_dev->net_dev->dev;

	d_fnstart(3, dev, "(wimax_dev %p)\n", wimax_dev);
	/* Initialize RF Kill */
	result = -ENOMEM;
	rfkill = rfkill_allocate(dev, RFKILL_TYPE_WIMAX);
	if (rfkill == NULL)
		goto error_rfkill_allocate;
	wimax_dev->rfkill = rfkill;

	rfkill->name = wimax_dev->name;
	rfkill->state = RFKILL_STATE_OFF;
	rfkill->data = wimax_dev;
	rfkill->toggle_radio = wimax_rfkill_toggle_radio;
	rfkill->user_claim_unsupported = 1;

	/* Initialize the input device for the hw key */
	input_dev = input_allocate_device();
	if (input_dev == NULL)
		goto error_input_allocate;
	wimax_dev->rfkill_input = input_dev;
	d_printf(1, dev, "rfkill %p input %p\n", rfkill, input_dev);

	input_dev->name = wimax_dev->name;
	/* FIXME: get a real device bus ID and stuff? do we care? */
	input_dev->id.bustype = BUS_HOST;
	input_dev->id.vendor = 0xffff;
	input_dev->evbit[0] = BIT(EV_KEY);
	set_bit(KEY_WIMAX, input_dev->keybit);

	/* Register both */
	result = input_register_device(wimax_dev->rfkill_input);
	if (result < 0)
		goto error_input_register;
	result = rfkill_register(wimax_dev->rfkill);
	if (result < 0)
		goto error_rfkill_register;

	/* If there is no SW toggle op, SW RFKill is always on */
	if (wimax_dev->op_rfkill_sw_toggle == NULL)
		wimax_dev->rfkill_sw = WIMAX_RFKILL_OFF;

	d_fnend(3, dev, "(wimax_dev %p) = 0\n", wimax_dev);
	return 0;

	/* if rfkill_register() suceeds, can't use rfkill_free() any
	 * more, only rfkill_unregister() [it owns the refcount]; with
	 * the input device we have the same issue--hence the if. */
error_rfkill_register:
	input_unregister_device(wimax_dev->rfkill_input);
	wimax_dev->rfkill_input = NULL;
error_input_register:
	if (wimax_dev->rfkill_input)
		input_free_device(wimax_dev->rfkill_input);
error_input_allocate:
	rfkill_free(wimax_dev->rfkill);
error_rfkill_allocate:
	d_fnend(3, dev, "(wimax_dev %p) = %d\n", wimax_dev, result);
	return result;
}


/*
 * Deregister a WiMAX device's RF Kill support
 *
 * Sick, we can't call rfkill_free() after rfkill_unregister()...oh
 * well.
 */
void wimax_rfkill_rm(struct wimax_dev *wimax_dev)
{
	struct device *dev = &wimax_dev->net_dev->dev;
	d_fnstart(3, dev, "(wimax_dev %p)\n", wimax_dev);
	rfkill_unregister(wimax_dev->rfkill);	/* frees */
	input_unregister_device(wimax_dev->rfkill_input);
	d_fnend(3, dev, "(wimax_dev %p)\n", wimax_dev);
}


#else /* #ifdef CONFIG_RFKILL */

void wimax_report_rfkill_hw(struct wimax_dev *wimax_dev,
			    enum wimax_rfkill_state state)
{
}
EXPORT_SYMBOL_GPL(wimax_report_rfkill_hw);

void wimax_report_rfkill_sw(struct wimax_dev *wimax_dev,
			    enum wimax_rfkill_state state)
{
}
EXPORT_SYMBOL_GPL(wimax_report_rfkill_sw);

int wimax_rfkill(struct wimax_dev *wimax_dev,
		 enum wimax_rfkill_state state)
{
	return 0;
}
EXPORT_SYMBOL_GPL(wimax_rfkill);

int wimax_rfkill_add(struct wimax_dev *wimax_dev)
{
	return 0;
}

void wimax_rfkill_rm(struct wimax_dev *wimax_dev)
{
}

#endif /* #ifdef CONFIG_RFKILL */


/*
 * Exporting to user space over generic netlink
 *
 * Parse the rfkill command from user space, return a combination
 * value that describe the states of the different toggles.
 *
 * Only one attribute: the new state requested (on, off or no change,
 * just query).
 */

static struct nla_policy wimax_gnl_rfkill_policy[] = {
	[WIMAX_GNL_RFKILL_STATE] = {
		.type = NLA_U32		/* enum wimax_rfkill_state */
	},
};


static
int wimax_gnl_doit_rfkill(struct sk_buff *skb, struct genl_info *info)
{
	int result = -ENODEV;
	struct net_device *net_dev;
	struct wimax_dev *wimax_dev;
	struct device *dev;
	struct nlmsghdr *nlh;
	struct nlattr *tb[WIMAX_GNL_RFKILL_MAX];
	enum wimax_rfkill_state new_state;

	/* If this fails to build, increase net/wimax.h:WIMAX_GNL_ATTR_MAX */
	BUILD_BUG_ON(WIMAX_GNL_RFKILL_MAX >= WIMAX_GNL_ATTR_MAX);
	BUILD_BUG_ON(WIMAX_RFKILL_ON != RFKILL_STATE_ON);
	BUILD_BUG_ON(WIMAX_RFKILL_OFF != RFKILL_STATE_OFF);

	d_fnstart(3, NULL, "(skb %p info %p)\n", skb, info);
	net_dev = wimax_get_netdev_by_info(info);
	if (net_dev == NULL)
		goto error_no_wimax_dev;
	dev = net_dev->dev.parent;
	wimax_dev = net_dev_to_wimax(net_dev);

	nlh = (void *) skb->data;
	result = nla_parse(tb, ARRAY_SIZE(tb),
			   nlmsg_attrdata(nlh, sizeof(struct genlmsghdr)),
			   nlmsg_attrlen(nlh, sizeof(struct genlmsghdr)),
			   wimax_gnl_rfkill_policy);
	if (result < 0) {
		dev_err(dev, "WIMAX_GNL_RFKILL: can't parse message: %d\n",
			result);
		goto error_parse;
	}
	result = -EINVAL;
	if (tb[WIMAX_GNL_RFKILL_STATE] == NULL) {
		dev_err(dev, "WIMAX_GNL_RFKILL: can't find RFKILL_STATE "
			"attribute\n");
		goto error_no_pid;
	}

	result = 0;
	new_state = nla_get_u32(tb[WIMAX_GNL_RFKILL_STATE]);
	result = wimax_rfkill(wimax_dev, new_state);
error_no_pid:
error_parse:
	dev_put(net_dev);
error_no_wimax_dev:
	d_fnend(3, NULL, "(skb %p info %p) = %d\n", skb, info, result);
	return result;
}


struct genl_ops wimax_gnl_rfkill = {
	.cmd = WIMAX_GNL_OP_RFKILL,
	.flags = 0,
	.policy = wimax_gnl_rfkill_policy,
	.doit = wimax_gnl_doit_rfkill,
	.dumpit = NULL,
};
