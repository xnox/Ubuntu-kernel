/*
 * Intel Wireless WiMAX Connection 2400m
 * Backend implementation of rfkill support
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
 * The WiMAX kernel stack integrates into RF-Kill and keeps the
 * switches's status. We just need to:
 *
 * - report changes in the HW RF Kill switch [with
 *   wimax_rfkill_{sw,hw}_report(), which happens when we detect those
 *   indications coming through hardware reports]. We also do it on
 *   initialization to let the stack know the intial HW state.
 *
 * - implement indications from the stack to change the SW RF Kill
 *   switch (coming from sysfs, the wimax stack or user space).
 */
#include "i2400m.h"
#include <net/wimax-i2400m.h>



#define D_SUBMODULE rfkill
#include "debug-levels.h"


/*
 * WiMAX stack operation: implement SW RFKill toggling
 *
 * @wimax_dev: device descriptor
 * @skb: skb where the message has been received; skb->data is
 *       expected to point to the message payload.
 * @genl_info: passed by the generic netlink layer
 *
 * Generic Netlink will call this function when a message is sent from
 * userspace to change the software RF-Kill switch status.
 *
 * This function will set the device's sofware RF-Kill switch state to
 * match what is requested.
 *
 * NOTE: the i2400m has a strict state machine; we can only set the
 *       RF-Kill switch when it is on, the HW RF-Kill is on and the
 *       device is initialized. So we ignore errors steaming from not
 *       being in the right state (-EILSEQ).
 */
int i2400m_op_rfkill_sw_toggle(struct wimax_dev *wimax_dev,
			       enum wimax_rfkill_state state)
{
	int result;
	struct i2400m *i2400m = wimax_dev_to_i2400m(wimax_dev);
	struct device *dev = i2400m_dev(i2400m);
	struct sk_buff *ack_skb;
	struct {
		struct i2400m_l3l4_hdr hdr;
		struct i2400m_tlv_rf_operation sw_rf;
	} __attribute__((packed)) *cmd;
	char strerr[32];

	d_fnstart(4, dev, "(wimax_dev %p state %d)\n", wimax_dev, state);
	if (!down_read_trylock(&i2400m->stack_rwsem))
		return -EUNATCH;

	result = -ENOMEM;
	cmd = kzalloc(sizeof(*cmd), GFP_KERNEL);
	if (cmd == NULL)
		goto error_alloc;
	cmd->hdr.type = cpu_to_le16(I2400M_MT_CMD_RF_CONTROL);
	cmd->hdr.length = sizeof(cmd->sw_rf);
	cmd->hdr.version = cpu_to_le16(I2400M_L3L4_VERSION);
	cmd->sw_rf.hdr.type = cpu_to_le16(I2400M_TLV_RF_OPERATION);
	cmd->sw_rf.hdr.length = cpu_to_le16(sizeof(cmd->sw_rf.status));
	switch (state) {
	case WIMAX_RFKILL_ON:	/* RFKILL ON, radio OFF */
		cmd->sw_rf.status = cpu_to_le32(2);
		break;
	case WIMAX_RFKILL_OFF:	/* RFKILL OFF, radio ON */
		cmd->sw_rf.status = cpu_to_le32(1);
		break;
	default:
		BUG();
	}

	ack_skb = i2400m_msg_to_dev(i2400m, cmd, sizeof(*cmd));
	result = PTR_ERR(ack_skb);
	if (IS_ERR(ack_skb)) {
		dev_err(dev, "Failed to issue 'RF Control' command: %d\n",
			result);
		goto error_msg_to_dev;
	}
	result = i2400m_msg_check_status(i2400m, (void *) ack_skb->data,
					 strerr, sizeof(strerr));
	result = result == -EILSEQ? 0 : result;	/* See doc */
	if (result < 0)
		dev_err(dev, "'RF Control' (0x%04x) command failed: %d - %s\n",
			I2400M_MT_CMD_RF_CONTROL, result, strerr);
	kfree_skb(ack_skb);
error_msg_to_dev:
error_alloc:
	up_read(&i2400m->stack_rwsem);
	d_fnend(4, dev, "(wimax_dev %p state %d) = %d\n",
		wimax_dev, state, result);
	return result;
}


/*
 * Inform the WiMAX stack of changes in the RF Kill switches reported
 * by the device
 *
 * @i2400m: device descriptor
 * @rfss: TLV for RF Switches status; already validated
 */
void i2400m_report_tlv_rf_switches_status(
	struct i2400m *i2400m,
	const struct i2400m_tlv_rf_switches_status *rfss)
{
	struct device *dev = i2400m_dev(i2400m);

	switch (le32_to_cpu(rfss->sw_rf_switch)) {	/* Chew SW state */
	case 1:	/* RF Kill disabled (radio on) */
		wimax_report_rfkill_sw(&i2400m->wimax_dev, WIMAX_RFKILL_OFF);
		break;
	case 2:	/* RF Kill enabled (radio off) */
		wimax_report_rfkill_sw(&i2400m->wimax_dev, WIMAX_RFKILL_ON);
		break;
	default:
		dev_err(dev, "HW BUG? Unknown RF SW state 0x%x\n",
			le32_to_cpu(rfss->sw_rf_switch));
		return;
	}

	switch (le32_to_cpu(rfss->hw_rf_switch)) {	/* Chew HW state */
	case 1:	/* RF Kill disabled (radio on) */
		wimax_report_rfkill_hw(&i2400m->wimax_dev, WIMAX_RFKILL_OFF);
		break;
	case 2:	/* RF Kill enabled (radio off) */
		wimax_report_rfkill_hw(&i2400m->wimax_dev, WIMAX_RFKILL_ON);
		break;
	default:
		dev_err(dev, "HW BUG? Unknown RF HW state 0x%x\n",
			le32_to_cpu(rfss->hw_rf_switch));
		return;
	}
}
