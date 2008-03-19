/* -*-linux-c-*-
 *
 * Intel Wireless WiMax Connection 2400m
 * Miscelaneous control functions for setting up the device
 *
 *
 *
 * Copyright (C) 2007-2008 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 * Intel Corporation <linux-wimax@intel.com>
 * Inaky Perez-Gonzalez <inaky.perez-gonzalez@intel.com>
 *  - Initial implementation
 *
 *
 * FIXME: docs  
 */

#include "i2400m.h"
#include <net/wimax-i2400m.h>
#define D_LOCAL 2
#include "../debug.h"


/*
 * Set diagnostics on/off command
 */
struct i2400m_cmd_monitor_control {
	struct i2400m_l3l4_hdr hdr;
	struct i2400m_tlv_hdr tlv;
	u8 val;
} __attribute__((packed));

enum {
	I2400M_MONITOR_CONTROL_ON  = 0x01,
	I2400M_MONITOR_CONTROL_OFF = 0x02,
	I2400M_TLV_TYPE_MONITOR_CONTROL = 0x4002,
};


/*
 * i2400m_diag_off - Turns all of device's diagnostics output off
 *
 * The write subsystem takes ownership of the skb, so we don't need to
 * free it.  
 */ 
int i2400m_diag_off(struct i2400m *i2400m) 
{
	int result;
	struct device *dev = &i2400m->usb_iface->dev;
	struct sk_buff *skb;

	struct i2400m_cmd_monitor_control *cmd;
	
	/* If I write this, I suddenly stop receiving responses to
	 * commands, so I try something else ... */
	result = -ENOMEM;
	skb = dev_alloc_skb(sizeof(*cmd));
	if (skb == NULL)
		goto error_alloc;
	
	cmd = (void *) skb_put(skb, sizeof(*cmd));
	memset(cmd, 0, sizeof(*cmd));
	cmd->hdr.type = I2400M_MT_CMD_MONITOR_CONTROL;
	cmd->hdr.length = sizeof(*cmd) - sizeof(cmd->hdr);
	cmd->hdr.version = I2400M_L3L4_VERSION;
	cmd->tlv.type = I2400M_TLV_TYPE_MONITOR_CONTROL;
	cmd->tlv.length = 1;
	cmd->val = I2400M_MONITOR_CONTROL_OFF;

	result = i2400m_write_async(i2400m, skb, NULL, 0, I2400M_PT_CTRL);
	d_printf(1, dev, "Diagnostics turned off: %d\n", result);
error_alloc:
	return result;
}
