/* -*-linux-c-*-
 *
 * Intel Wireless WiMax Connection 2400m
 * IOCTL legacy control path
 *
 *
 *
 * Copyright (C) 2005-2007 Intel Corporation <linux-wimax@intel.com>
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

#include <linux/netdevice.h>
#include <linux/workqueue.h>
#include <net/wimax-i2400m.h>

#ifdef __USE_LEGACY_IOCTL
#include "besor_legacy.h"
#endif /* __USE_LEGACY_IOCTL */

#include "i2400m.h"
#define D_LOCAL 6
#include "../debug.h"

struct l4_hdr {
	__le16 type;
	__le16 len;
	__le16 reserved[4];
	unsigned char val[0];
} __attribute__ ((packed));

struct tlv_hdr {
	__le16 type;
	__le16 len;
	unsigned char val[0];
} __attribute__ ((packed));

struct besor_ctl_xfer {
	char *inbuf;
	unsigned int inlen;
	char *outbuf;
	unsigned int outlen;
};

enum {
	I2400M_TLV_TYPE_SYSTEM_STATE = 141,
	I2400M_TLV_TYPE_REPORT_STATE_REASON = 150,
	I2400M_TLV_TYPE_LINK_STATUS = 160,
	I2400M_TLV_TYPE_MEDIA_STATUS = 161
};

enum {
	I2400M_IOC_CTRL = 0,
	I2400M_IOC_NOTIF,
	I2400M_IOC_TRACE,
	I2400M_IOC_STATE,
	I2400M_IOC_SOFT_RESET,
	I2400M_IOC_COLD_RESET,
	I2400M_IOC_DIAG_OFF
};

enum {
	I2400M_MOD_ID_DEBUG = 0x2,
	I2400M_MOD_ID_L4_ADAPTER = 0x4,
	I2400M_MOD_ID_PAL = 0x6,
	I2400M_MOD_ID_NDS = 0x8,
	I2400M_MOD_ID_GENERAL = 0xA,
	I2400M_MOD_ID_SUPPLICANT = 0xC,
	I2400M_MOD_ID_PRODUCTION = 0xE
};

enum {
	I2400M_REQ_TYPE_NONE = 0x0,
	I2400M_REQ_TYPE_GET = 0x1,
	I2400M_REQ_TYPE_SET = 0x2,
	I2400M_REQ_TYPE_CMD = 0x3
};

enum {
	I2400M_OPCODE_SHIFT = 0,
	I2400M_REQ_TYPE_SHIFT = 9,
	I2400M_MOD_ID_SHIFT = 11,
	I2400M_NOTIF_SHIFT = 15
};


/**
  Specific OpCode values
 */


/* this is a workaround to shut down device diagnostics
   normal firmware should come with trace/diag off by default
   and only diag software should use these factory API.
*/
enum{
	diag_on = 1,
	diag_off = 2,
	l4_diag_onoff_msg = 0x5605,
	l4_diag_onoff_tlv = 0x4002
};

int i2400m_legacy_dev_diag_mode(struct i2400m *i2400m, int mode)
{
	int result = 0;
	struct l4_hdr *l4;
	struct tlv_hdr *tlv;
	struct device *dev = &i2400m->usb_iface->dev;
	d_fnstart(0, dev, "\n");
	l4 = kmalloc(sizeof(*l4)+sizeof(*tlv)+sizeof(u8), GFP_KERNEL);
	if (l4 == NULL){
		result = -ENOMEM;
		d_printf(3, dev, "memory allocation failed\n");
		goto error_l4_alloc;
	}
	memset(l4, 0, sizeof(*l4)+sizeof(*tlv)+sizeof(u8));
	tlv = (struct tlv_hdr *)l4->val;
	l4->type = l4_diag_onoff_msg;
	l4->len = sizeof(*tlv)+sizeof(u8);
	tlv->type = l4_diag_onoff_tlv;
	tlv->len = 1;
	tlv->val[0] = diag_off;
	result = i2400m_write_async(i2400m,NULL, l4, sizeof(*l4), I2400M_PT_CTRL);
	if (result < 0)
		d_printf(3, dev, "send diag off: "
			 "failed writing to the device\n");
	kfree(l4);
error_l4_alloc:
	d_fnend(0, dev, "returning %d\n", result);
	return result;
}

void i2400m_legacy_cold_boot_task(struct work_struct *ws)
{
	struct i2400m_legacy_ctl* lc;
	struct i2400m *i2400m;
	lc = container_of(ws, struct i2400m_legacy_ctl, ws);
	i2400m = lc->parent;
	i2400m_reset_cold(i2400m);
}
void i2400m_legacy_soft_boot_task(struct work_struct *ws)
{
	struct i2400m_legacy_ctl* lc;
	struct i2400m *i2400m;
	lc = container_of(ws, struct i2400m_legacy_ctl, ws);
	i2400m = lc->parent;
	i2400m_reset_soft(i2400m);
}

int i2400m_legacy_send_init_dev(struct i2400m *i2400m)
{
	int ret = 0;
	struct device *dev = &i2400m->usb_iface->dev;
#warning FIXME: payload on stack, cannot be done
	struct i2400m_l3l4_hdr msg;

	d_fnstart(0, dev, "\n");
	memset(&msg, 0, sizeof(msg));
	msg.type = cpu_to_le16(I2400M_MT_CMD_INIT);
	d_printf(0, dev, "opcode 0x%4.4x, no tlvs\n", msg.type);
	ret = i2400m_write_async(i2400m, NULL, &msg, sizeof(msg),
				 I2400M_PT_CTRL);
	if (ret != 0)
		dev_err(dev, "sending init message failed\n");
	d_fnend(0, dev, "returning %d\n", ret);
	return ret;
}

int i2400m_legacy_parse_tlv(struct i2400m *i2400m,
			    const void *buf, size_t buf_len)
{
	struct device *dev = &i2400m->usb_iface->dev;
	struct tlv_hdr *tlv = (struct tlv_hdr *) buf;
	int type, len;
	__le32 *val;
	u32 new_state;

	type = le16_to_cpu(tlv->type);
	len = le16_to_cpu(tlv->len);
	val = (__le32 *) tlv->val;
	d_fnstart(0, dev, "type=%d, len=%d\n", type, len);
	switch (type) {
	case I2400M_TLV_TYPE_SYSTEM_STATE:
		new_state = le32_to_cpu(*val);
		d_printf(0, dev, "device reported state %d\n", new_state);
		d_printf(0, dev, "old state %ld\n",
			 i2400m->legacy_ctl->sys_state);
		/* here is the tricky one!!!
		   if we have a state chage to UNINITIALIZED
		   we will send a INIT command from within the driver
		 */
		if (new_state == I2400M_SYSTEM_STATE_CONFIG)
			i2400m_legacy_send_init_dev(i2400m);
		i2400m->legacy_ctl->sys_state = new_state;
		break;
	case I2400M_TLV_TYPE_LINK_STATUS:
		i2400m->legacy_ctl->link_state = le32_to_cpu(val);
		break;
	case I2400M_TLV_TYPE_MEDIA_STATUS:
		i2400m->legacy_ctl->media_state = le32_to_cpu(val);
		break;
	default:
		break;
	}
	d_fnend(0, dev, "\n");
	return len + sizeof(*tlv);
}

void i2400m_legacy_parse_state_report(struct i2400m *i2400m,
				      const void *buf, size_t len)
{
	int parsed;
	struct device *dev = &i2400m->usb_iface->dev;

	d_fnstart(0, dev, "\n");
	while (len > 0) {
		parsed = i2400m_legacy_parse_tlv(i2400m, buf, len);
		len -= parsed;
		buf += parsed;
	}
	d_fnend(0, dev, "\n");
}

void i2400m_legacy_ctl_cb(struct i2400m *i2400m, void *buf, size_t buf_len)
{
	struct i2400m_l3l4_hdr *msg = buf;
	struct device *dev = &i2400m->usb_iface->dev;
	struct msg_item *item;
	u16 op = le16_to_cpu(msg->type);
	u16 len = le16_to_cpu(msg->length);
	unsigned long flags;

	d_fnstart(0, dev, "(i2400m %p buf %p buf_len %zu)\n",
		  i2400m, buf, buf_len);
	if (len > buf_len) {
		dev_err(dev, "discarding - len > buf_len - illegal packet\n");
		goto out;
	}
	if (op == I2400M_MT_REPORT_STATE) {
		d_printf(0, dev, "report state\n");
		i2400m_legacy_parse_state_report(
			i2400m, msg->pl, msg->length);
		//i2400m_legacy_build_state_notif();
	}
	else if (op == I2400M_MT_CMD_INIT)
		d_printf(0, dev, "FIXME: unimplemented cmd init\n");
	else if (op == I2400M_MT_CMD_TERMINATE)
		d_printf(0, dev, "FIXME: unimplemented cmd terminate\n");
	else {	/* Append to the message queue for user space delivery */
		item = kmalloc(sizeof(*item) + buf_len, GFP_ATOMIC);
		if (!item) {
			dev_err(dev,
				"aboritng: message item allocation failure\n");
			goto out;
		}
		memcpy(item->buf, buf, buf_len);
		item->len = buf_len;
		spin_lock_irqsave(&i2400m->legacy_ctl->lock, flags);
		list_add_tail(&item->node, &i2400m->legacy_ctl->msg_queue);
		spin_unlock_irqrestore(&i2400m->legacy_ctl->lock, flags);
		complete(&i2400m->legacy_ctl->comp);
	}
out:
	d_fnend(0, dev, "\n");
	return;
}

int i2400m_legacy_xfer_to_dev(struct i2400m *i2400m,
			      struct besor_ctl_xfer *bioc)
{
	void *buf = NULL;
	int ret = 0;

	buf = kmalloc(bioc->inlen, GFP_KERNEL);
	if (!buf) {
		ret = -ENOMEM;
		goto out;
	}
	ret = copy_from_user(buf, bioc->inbuf, bioc->inlen);
	if (ret != 0) {
		goto out;
	}
#if 0
	d_printf(0, dev, "dumping outgoing packet:\n");
	hexdump(payload, hdr->pkts[i].size);
#endif
	ret = i2400m_write_async(i2400m, NULL, buf, bioc->inlen,
				 I2400M_PT_CTRL);
out:
	return ret;
}

int i2400m_legacy_xfer_to_app(void *dest, struct besor_ctl_xfer *bioc,
			      void *buf, size_t buf_len)
{
	int ret;

	ret = copy_to_user(bioc->outbuf, buf, buf_len);
	if (ret != 0) {
		goto out;
	}
	bioc->outlen = buf_len;
	ret = copy_to_user(dest, bioc, sizeof(*bioc));
	if (ret != 0) {
		goto out;
	}
out:
	return ret;
}

int i2400m_legacy_ioctl(struct net_device *net_dev, struct ifreq *rq, int cmd)
{
	void *resp_buf = NULL;
	struct besor_ctl_xfer bioc;
	struct i2400m *i2400m = net_dev_to_i2400m(net_dev);
	struct device *dev = &i2400m->usb_iface->dev;
	int ret;
	size_t resp_len;
	unsigned long flags;
	struct msg_item *item = NULL;

	d_fnstart(0, dev, "cmd=%d\n", cmd - SIOCDEVPRIVATE);
	cmd -= SIOCDEVPRIVATE;
	if(rq->ifr_ifru.ifru_data){
		ret = copy_from_user(&bioc, rq->ifr_ifru.ifru_data, sizeof(bioc));
		if (ret) {
			d_printf(0, dev, "header: copy_from_user failed, pid is %d\n",
				current->pid);
			goto out;
		}
	}
	d_printf(0,dev,"legcy_ioctl:before switch\n");
	switch (cmd) {
	case I2400M_IOC_CTRL:
		init_completion(&i2400m->legacy_ctl->comp);
		ret = i2400m_legacy_xfer_to_dev(i2400m, &bioc);
		if (ret) {
			d_printf(0, dev, "failed sending to device: ret=%d\n",
				 ret);
			goto out;
		}
		ret = wait_for_completion_interruptible_timeout(&i2400m->
								legacy_ctl->
								comp, 10 * HZ);
		if (ret == 0) {	/* request timed out */
			ret = -ETIMEDOUT;
			dev_err(dev,
				"control command timed out - returning %d\n",
				ret);
			goto out;
		}
		else if (ret < 0) {
			ret = -ERESTARTSYS;
			dev_err(dev,
				"control command interrupted - returning %d\n",
				ret);
			goto out;
		}
		spin_lock_irqsave(&i2400m->legacy_ctl->lock, flags);
		item = list_first_entry(&i2400m->legacy_ctl->msg_queue,
					struct msg_item, node);
		if (&item->node != &i2400m->legacy_ctl->msg_queue)
			list_del(&item->node);
		else
			d_printf(0, dev, "completion with list empty\n");
		spin_unlock_irqrestore(&i2400m->legacy_ctl->lock, flags);
		if (&item->node == &i2400m->legacy_ctl->msg_queue) {
			item = NULL;
			goto out;
		}
		resp_len = min(bioc.outlen, item->len);
		ret = i2400m_legacy_xfer_to_app(rq->ifr_ifru.ifru_data,
						&bioc, item->buf, resp_len);
		if (ret != 0) {
			dev_err(dev, "copy data to user failed %d\n", ret);
			goto out;
		}
		break;
	case I2400M_IOC_NOTIF:
		resp_len =
			min(bioc.outlen,
			    kfifo_len(i2400m->legacy_ctl->notif_q));
		resp_buf = kmalloc(resp_len, GFP_KERNEL);
		if (resp_buf == NULL) {
			ret = -ENOMEM;
			goto out;
		}
		kfifo_get(i2400m->legacy_ctl->notif_q, resp_buf, resp_len);
		ret = i2400m_legacy_xfer_to_app(rq->ifr_ifru.ifru_data, &bioc,
						resp_buf, resp_len);
		break;
	case I2400M_IOC_TRACE:
		resp_len =
			min(bioc.outlen,
			    kfifo_len(i2400m->legacy_ctl->trace_q));
		resp_buf = kmalloc(resp_len, GFP_KERNEL);
		if (resp_buf == NULL) {
			ret = -ENOMEM;
			goto out;
		}
		kfifo_get(i2400m->legacy_ctl->trace_q, resp_buf, resp_len);
		ret = i2400m_legacy_xfer_to_app(rq->ifr_ifru.ifru_data, &bioc,
						resp_buf, resp_len);

		break;
	case I2400M_IOC_STATE:
		resp_len = min(bioc.outlen, sizeof(u32));

	d_printf(0,dev,"legcy_ioctl:I2400m_IOC_STATE:resp_len:%d\n",resp_len);
		resp_buf = &i2400m->legacy_ctl->sys_state;
		ret = i2400m_legacy_xfer_to_app(rq->ifr_ifru.ifru_data, &bioc,
						resp_buf, resp_len);
		resp_buf = NULL;	/* to avoid free of non allocated area */
	d_printf(0,dev,"legacy_ioctl:After Calling  xfter_to_app\n");
		if (ret != 0)
			d_printf(0, dev, "header: xfer to user failed\n");
		goto out;
		break;
	case I2400M_IOC_SOFT_RESET:
		INIT_WORK(&i2400m->legacy_ctl->ws,i2400m_legacy_soft_boot_task);
		schedule_work(&i2400m->legacy_ctl->ws);
		break;
	case I2400M_IOC_COLD_RESET:
		INIT_WORK(&i2400m->legacy_ctl->ws,i2400m_legacy_cold_boot_task);
		schedule_work(&i2400m->legacy_ctl->ws);
		break;
	case I2400M_IOC_DIAG_OFF:
		ret = i2400m_legacy_dev_diag_mode(i2400m, diag_off);
		ret = wait_for_completion_interruptible_timeout(&i2400m->
								legacy_ctl->
								comp, 10 * HZ);
		if (ret == 0) {	/* request timed out */
			ret = -ETIMEDOUT;
			dev_err(dev,
				"control command timed out - returning %d\n",
				ret);
			goto out;
		}
		else if (ret < 0) {
			ret = -ERESTARTSYS;
			dev_err(dev,
				"control command interrupted - returning %d\n",
				ret);
			goto out;
		}
		ret = 0;
		spin_lock_irqsave(&i2400m->legacy_ctl->lock, flags);
		item = list_first_entry(&i2400m->legacy_ctl->msg_queue,
					struct msg_item, node);
		if (&item->node != &i2400m->legacy_ctl->msg_queue)
			list_del(&item->node);
		else
			d_printf(0, dev, "completion with list empty\n");
		spin_unlock_irqrestore(&i2400m->legacy_ctl->lock, flags);
		if (&item->node == &i2400m->legacy_ctl->msg_queue) {
			item = NULL;
			goto out;
		}
		break;
	default:
		ret = -EOPNOTSUPP;
	}
out:
	if (resp_buf != NULL)
		kfree(resp_buf);
	if (item != NULL)
		kfree(item);
	d_fnend(0, dev, "returning %d\n", ret);
	return ret;
}

/**
 * i2400m_legacy_ctl_setup - initiate the legacy ioctl system
 *
 * @i2400m: device descriptor
 */
int i2400m_legacy_ctl_setup(struct i2400m *i2400m)
{
	int ret = 0;
	struct device *dev = &i2400m->usb_iface->dev;

	d_fnstart(0, dev, "\n");
	i2400m->legacy_ctl = kzalloc(sizeof(*i2400m->legacy_ctl), GFP_KERNEL);
	if (i2400m->legacy_ctl == NULL) {
		dev_err(dev, "malloc legacy_ctl failed\n");
		goto error_resources;
	}
	INIT_LIST_HEAD(&i2400m->legacy_ctl->msg_queue);
	init_completion(&i2400m->legacy_ctl->comp);
	spin_lock_init(&i2400m->legacy_ctl->lock);
	i2400m->legacy_ctl->notif_q = kfifo_alloc(PAGE_SIZE,
						  GFP_KERNEL,
						  &i2400m->legacy_ctl->lock);
	if (i2400m->legacy_ctl == NULL) {
		dev_err(dev, "kfifo_alloc notif_q failed\n");
		goto error_resources;
	}
	i2400m->legacy_ctl->trace_q = kfifo_alloc(10 * PAGE_SIZE,
						  GFP_KERNEL,
						  &i2400m->legacy_ctl->lock);
	if (i2400m->legacy_ctl == NULL) {
		dev_err(dev, "kfifo_alloc trace_q failed\n");
		goto error_resources;
	}
	i2400m->legacy_ctl->sys_state = I2400M_SYSTEM_STATE_UNINITIALIZED;
	i2400m->legacy_ctl->parent = i2400m;
out:
	d_fnend(0, dev, "returning %d\n", ret);
	return ret;
error_resources:
	ret = -ENOMEM;
	if (i2400m->legacy_ctl) {
		kfree(i2400m->legacy_ctl);
		goto out;
	}
	if (i2400m->legacy_ctl->notif_q)
		kfifo_free(i2400m->legacy_ctl->notif_q);
	if (i2400m->legacy_ctl->trace_q)
		kfifo_free(i2400m->legacy_ctl->trace_q);
	goto out;
}

void free_msg_queue(struct list_head *l)
{
	struct msg_item *item;

	while (!list_empty(l)) {
		item = list_first_entry(l, struct msg_item, node);

		list_del(&item->node);
		kfree(item);
	}
}

/**
 * i2400m_legacy_ctl_release - release the legacy ioctl system
 *
 * @i2400m: device descriptor
 */
void i2400m_legacy_ctl_release(struct i2400m *i2400m)
{
	free_msg_queue(&i2400m->legacy_ctl->msg_queue);
	kfifo_free(i2400m->legacy_ctl->notif_q);
	kfifo_free(i2400m->legacy_ctl->trace_q);
	kfree(i2400m->legacy_ctl);
}
