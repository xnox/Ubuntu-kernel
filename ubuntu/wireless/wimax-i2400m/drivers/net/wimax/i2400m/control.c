/*
 * Intel Wireless WiMAX Connection 2400m
 * Miscellaneous control functions for using the device
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
 */

#include "i2400m.h"
#include <linux/kernel.h>
#include <net/wimax-i2400m.h>


#define D_SUBMODULE control
#include "debug-levels.h"


/*
 * Return if a TLV is of a give type and size
 *
 * @tlv_hdr: pointer to the TLV
 * @tlv_type: type of the TLV we are looking for
 * @tlv_size: expected size of the TLV we are looking for (if -1,
 *            don't check the size). This includes the header
 * Returns: 0 if the TLV matches
 *          < 0 if it doesn't match at all
 *          > 0 total TLV + payload size, if the type matches, but not
 *              the size
 */
static
ssize_t i2400m_tlv_match(const struct i2400m_tlv_hdr *tlv,
		     enum i2400m_tlv tlv_type, ssize_t tlv_size)
{
	if (le16_to_cpu(tlv->type) != tlv_type)	/* Not our type? skip */
		return -1;
	if (tlv_size != -1
	    && le16_to_cpu(tlv->length) + sizeof(*tlv) != tlv_size)
		return le16_to_cpu(tlv->length)  + sizeof(*tlv);
	return 0;
}


/*
 * Given a buffer of TLVs, iterate over them
 *
 * @i2400m: device instance
 * @tlv_buf: pointer to the beginning of the TLV buffer
 * @buf_size: buffer size in bytes
 * @tlv_pos: seek position; this is assumed to be a pointer returned
 *           by i2400m_tlv_buffer_walk() [and thus, validated]. The
 *           TLV returned will be the one following this one.
 *
 * Usage:
 *
 * tlv_itr = NULL;
 * while (tlv_itr = i2400m_tlv_buffer_walk(i2400m, buf, size, tlv_itr))  {
 *         ...
 *         // Do stuff with tlv_itr, DON'T MODIFY IT
 *         ...
 * }
 */
static
const struct i2400m_tlv_hdr *i2400m_tlv_buffer_walk(
	struct i2400m *i2400m,
	const void *tlv_buf, size_t buf_size,
	const struct i2400m_tlv_hdr *tlv_pos)
{
	struct device *dev = i2400m_dev(i2400m);
	const struct i2400m_tlv_hdr *tlv_top = tlv_buf + buf_size;
	size_t offset, length, avail_size;
	unsigned type;

	if (tlv_pos == NULL)	/* Take the first one? */
		tlv_pos = tlv_buf;
	else			/* Nope, the next one */
		tlv_pos = (void *) tlv_pos
			+ le16_to_cpu(tlv_pos->length) + sizeof(*tlv_pos);
	if (tlv_pos == tlv_top) {	/* buffer done */
		tlv_pos = NULL;
		goto error_beyond_end;
	}
	if (tlv_pos > tlv_top) {
		tlv_pos = NULL;
		WARN_ON(1);
		goto error_beyond_end;
	}
	offset = (void *) tlv_pos - (void *) tlv_buf;
	avail_size = buf_size - offset;
	if (avail_size < sizeof(*tlv_pos)) {
		dev_err(dev, "HW BUG? tlv_buf %p [%zu bytes], tlv @%zu: "
			"short header\n", tlv_buf, buf_size, offset);
		goto error_short_header;
	}
	type = le16_to_cpu(tlv_pos->type);
	length = le16_to_cpu(tlv_pos->length);
	if (avail_size < sizeof(*tlv_pos) + length) {
		dev_err(dev, "HW BUG? tlv_buf %p [%zu bytes], "
			"tlv type 0x%04x @%zu: "
			"short data (%zu bytes vs %zu needed)\n",
			tlv_buf, buf_size, type, offset, avail_size,
			sizeof(*tlv_pos) + length);
		goto error_short_header;
	}
error_short_header:
error_beyond_end:
	return tlv_pos;
}


/*
 * Find a TLV in a buffer of sequential TLVs
 *
 * @i2400m: device descriptor
 * @tlv_hdr: pointer to the first TLV in the sequence
 * @size: size of the buffer in bytes; all TLVs are assumed to fit
 *        fully in the buffer (otherwise we'll complain).
 * @tlv_type: type of the TLV we are looking for
 * @tlv_size: expected size of the TLV we are looking for (if -1,
 *            don't check the size). This includes the header
 *
 * Returns: NULL if the TLV is not found, otherwise a pointer to
 *          it. If the sizes don't match, an error is printed and NULL
 *          returned.
 */
static
const struct i2400m_tlv_hdr *i2400m_tlv_find(
	struct i2400m *i2400m,
	const struct i2400m_tlv_hdr *tlv_hdr, size_t size,
	enum i2400m_tlv tlv_type, ssize_t tlv_size)
{
	ssize_t match;
	struct device *dev = i2400m_dev(i2400m);
	const struct i2400m_tlv_hdr *tlv = NULL;
	while ((tlv = i2400m_tlv_buffer_walk(i2400m, tlv_hdr, size, tlv))) {
		match = i2400m_tlv_match(tlv, tlv_type, tlv_size);
		if (match == 0)		/* found it :) */
			break;
		if (match > 0)
			dev_warn(dev, "TLV type 0x%04x found with size "
				 "mismatch (%zu vs %zu needed)\n",
				 tlv_type, match, tlv_size);
	}
	return tlv;
}


static const struct
{
	char *msg;
	int errno;
} ms_to_errno[I2400M_MS_MAX] = {
	[I2400M_MS_DONE_OK] = { "", 0 },
	[I2400M_MS_DONE_IN_PROGRESS] = { "", 0 },
	[I2400M_MS_INVALID_OP] = { "invalid opcode", -ENOSYS },
	[I2400M_MS_BAD_STATE] = { "invalid state", -EILSEQ },
	[I2400M_MS_ILLEGAL_VALUE] = { "illegal value", -EINVAL },
	[I2400M_MS_MISSING_PARAMS] = { "missing parameters", -ENOMSG },
	[I2400M_MS_VERSION_ERROR] = { "bad version", -EIO },
	[I2400M_MS_ACCESSIBILITY_ERROR] = { "accesibility error", -EIO },
	[I2400M_MS_BUSY] = { "busy", -EBUSY },
	[I2400M_MS_CORRUPTED_TLV] = { "corrupted TLV", -EILSEQ },
	[I2400M_MS_UNINITIALIZED] = { "not unitialized", -EILSEQ },
	[I2400M_MS_UNKNOWN_ERROR] = { "unknown error", -EIO },
	[I2400M_MS_PRODUCTION_ERROR] = { "production error", -EIO },
	[I2400M_MS_NO_RF] = { "no RF", -EIO },
	[I2400M_MS_NOT_READY_FOR_POWERSAVE] =
		{ "not ready for powersave", -EACCES },
	[I2400M_MS_THERMAL_CRITICAL] = { "thermal critical", -EL3HLT },
};


/*
 * i2400m_msg_check_status - translate a message's status code
 *
 * @i2400m: device descriptor
 * @l3l4_hdr: message header
 * @strbuf: buffer to place a formatted error message (unless NULL).
 * @strbuf_size: max amount of available space; larger messages will
 * be truncated.
 *
 * Returns: errno code corresponding to the status code in @l3l4_hdr
 *          and a message in @strbuf describing the error.
 */
int i2400m_msg_check_status(struct i2400m *i2400m,
			    const struct i2400m_l3l4_hdr *l3l4_hdr,
			    char *strbuf, size_t strbuf_size)
{
	int result;
	enum i2400m_ms status = le16_to_cpu(l3l4_hdr->status);
	const char *str;

	if (status == 0)
		return 0;
	if (status > ARRAY_SIZE(ms_to_errno)) {
		str = "unknown status code";
		result = -EBADR;
	} else {
		str = ms_to_errno[status].msg;
		result = ms_to_errno[status].errno;
	}
	if (strbuf)
		snprintf(strbuf, strbuf_size, "%s (%d)", str, status);
	return result;
}


/*
 * Act on a TLV System State reported by the device
 *
 * @i2400m: device descriptor
 * @ss: validated System State TLV
 *
 * This will set the carrier up on down based on the device's general state.
 */
void i2400m_report_tlv_system_state(struct i2400m *i2400m,
				    const struct i2400m_tlv_system_state *ss)
{
	if (unlikely(i2400m->ready == 0))	/* act if up */
		return;
	switch (le32_to_cpu(ss->state)) {
	case I2400M_SYSTEM_STATE_DATA_PATH_CONNECTED:
	case I2400M_SYSTEM_STATE_IDLE:
		netif_carrier_on(i2400m->wimax_dev.net_dev);
		break;
	default:
		netif_carrier_off(i2400m->wimax_dev.net_dev);
		break;
	};
}


/*
 * Parse a 'state report' and extract carrier on/off infromation
 *
 * @i2400m: device descriptor
 * @l3l4_hdr: pointer to message; it has been already validated for
 *            consistent size.
 * @size: size of the message (header + payload). The header length
 *        declaration is assumed to be congruent with @size (as in
 *        sizeof(*l3l4_hdr) + l3l4_hdr->length == size)
 *
 * Extract from the report state the system state TLV and infer from
 * there if we have a carrier or not. Update our local state and tell
 * netdev.
 *
 * When setting the carrier, it's fine to set OFF twice for example,
 * as netif_carrier_of() will not generate to OFF events (just on the
 * transitions from ON to OFF and viceversa).
 */
static
void i2400m_report_state_hook(struct i2400m *i2400m,
			      const struct i2400m_l3l4_hdr *l3l4_hdr,
			      size_t size, const char *tag)
{
	struct device *dev = i2400m_dev(i2400m);
	const struct i2400m_tlv_hdr *tlv;
	const struct i2400m_tlv_system_state *ss;
	const struct i2400m_tlv_rf_switches_status *rfss;
	size_t tlv_size = le16_to_cpu(l3l4_hdr->length);

	d_fnstart(4, dev, "(i2400m %p, l3l4_hdr %p, size %zu, %s)\n",
		  i2400m, l3l4_hdr, size, tag);
	tlv = NULL;

	while ((tlv = i2400m_tlv_buffer_walk(i2400m, &l3l4_hdr->pl,
					     tlv_size, tlv))) {
		if (0 == i2400m_tlv_match(tlv, I2400M_TLV_SYSTEM_STATE,
					  sizeof(*ss))) {
			ss = container_of(tlv, typeof(*ss), hdr);
			d_printf(2, dev, "%s: system state TLV "
				 "found (0x%04x), state 0x%08x\n",
				 tag, I2400M_TLV_SYSTEM_STATE,
				 le32_to_cpu(ss->state));
			i2400m_report_tlv_system_state(i2400m, ss);
		}
		if (0 == i2400m_tlv_match(tlv, I2400M_TLV_RF_STATUS,
					  sizeof(*rfss))) {
			rfss = container_of(tlv, typeof(*rfss), hdr);
			d_printf(2, dev, "%s: RF status TLV "
				 "found (0x%04x), sw 0x%02x hw 0x%02x\n",
				 tag, I2400M_TLV_RF_STATUS,
				 le32_to_cpu(rfss->sw_rf_switch),
				 le32_to_cpu(rfss->hw_rf_switch));
			i2400m_report_tlv_rf_switches_status(i2400m, rfss);
		}
	}
	d_fnend(4, dev, "(i2400m %p, l3l4_hdr %p, size %zu, %s) = void\n",
		i2400m, l3l4_hdr, size, tag);
}


/*
 * i2400m_report_hook - (maybe) act on a report
 *
 * @i2400m: device descriptor
 * @l3l4_hdr: pointer to message; it has been already validated for
 *            consistent size.
 * @size: size of the message (header + payload). The header length
 *        declaration is assumed to be congruent with @size (as in
 *        sizeof(*l3l4_hdr) + l3l4_hdr->length == size)
 *
 * Extract information we might need (like carrien on/off) from a
 * device report.
 */
void i2400m_report_hook(struct i2400m *i2400m,
			const struct i2400m_l3l4_hdr *l3l4_hdr, size_t size)
{
	struct device *dev = i2400m_dev(i2400m);
	unsigned msg_type;

	d_fnstart(3, dev, "(i2400m %p l3l4_hdr %p size %zu)\n",
		  i2400m, l3l4_hdr, size);
	/* Chew on the message, we might need some information from
	 * here */
	msg_type = le16_to_cpu(l3l4_hdr->type);
	switch (msg_type) {
	case I2400M_MT_REPORT_STATE:	/* carrier detection... */
		i2400m_report_state_hook(i2400m,
					 l3l4_hdr, size, "REPORT STATE");
		break;
	/* If the device is ready for power save, then ask it to do
	 * it. When we get the ack for that (in the hooks for command
	 * execution), we'll tell the USB subsystem to go for it. */
	case I2400M_MT_REPORT_POWERSAVE_READY:	/* zzzzz */
		if (l3l4_hdr->status == cpu_to_le16(I2400M_MS_DONE_OK)) {
#if 0
			d_printf(1, dev, "ready for powersave, requesting\n");
			i2400m_cmd_enter_powersave(i2400m);
#else
#warning FIXME: not requesting power save
			d_printf(1, dev, "ready for powersave, NOT requesting\n");
#endif

		}
		break;
	};
	d_fnend(3, dev, "(i2400m %p l3l4_hdr %p size %zu) = void\n",
		i2400m, l3l4_hdr, size);
}


/*
 * i2400m_msg_ack_hook - process cmd/set/get ack for internal status
 *
 * @i2400m: device descriptor
 * @l3l4_hdr: pointer to message; it has been already validated for
 *            consistent size.
 * @size: size of the message
 *
 * Extract information we might need from acks to commands and act on
 * it. This is akin to i2400m_report_hook(). Note most of this
 * processing should be done in the function that calls the
 * command. This is here for some cases where it can't happen...
 */
void i2400m_msg_ack_hook(struct i2400m *i2400m,
			 const struct i2400m_l3l4_hdr *l3l4_hdr, size_t size)
{
	int result;
	struct device *dev = i2400m_dev(i2400m);
	unsigned ack_type, ack_status;
	char strerr[32];

	/* Chew on the message, we might need some information from
	 * here */
	ack_type = le16_to_cpu(l3l4_hdr->type);
	ack_status = le16_to_cpu(l3l4_hdr->status);
	switch (ack_type) {
	case I2400M_MT_CMD_ENTER_POWERSAVE:
		/* This is just left here for the sake of example, as
		 * the processing is done somewhere else. */
		if (0) {
			result = i2400m_msg_check_status(
				i2400m, l3l4_hdr, strerr, sizeof(strerr));
			if (result >= 0) {
				d_printf(1, dev, "ready for power save\n");
				if (i2400m->bus_autopm_enable)
					i2400m->bus_autopm_enable(i2400m);
			}
		}
		break;
	};
	return;
}


/*
 * i2400m_msg_size_check() - verify message size and header are congruent
 *
 * It is ok if the total message size is larger than the expected
 * size, as there can be padding.
 */
int i2400m_msg_size_check(struct i2400m *i2400m,
			  const struct i2400m_l3l4_hdr *l3l4_hdr,
			  size_t msg_size)
{
	int result;
	struct device *dev = i2400m_dev(i2400m);
	size_t expected_size;
	d_fnstart(4, dev, "(i2400m %p l3l4_hdr %p msg_size %zu)\n",
		  i2400m, l3l4_hdr, msg_size);
	if (msg_size < sizeof(*l3l4_hdr)) {
		dev_err(dev, "bad size for message header "
			"(expected at least %u, got %u)\n",
			sizeof(*l3l4_hdr), msg_size);
		result = -EIO;
		goto error_hdr_size;
	}
	expected_size = le16_to_cpu(l3l4_hdr->length) + sizeof(*l3l4_hdr);
	if (msg_size < expected_size) {
		dev_err(dev, "bad size for message code 0x%04x "
			"(expected %u, got %u)\n", le16_to_cpu(l3l4_hdr->type),
			expected_size, msg_size);
		result = -EIO;
	} else
		result = 0;
error_hdr_size:
	d_fnend(4, dev,
		"(i2400m %p l3l4_hdr %p msg_size %zu) = %d\n",
		i2400m, l3l4_hdr, msg_size, result);
	return result;
}


/**
 * i2400m_msg_to_dev - Send a control message to the device and get a response
 *
 * @i2400m: device descriptor
 *
 * @msg_skb: an skb  *
 *
 * @buf: pointer to the buffer containing the message to be sent; it
 *           has to start with a &struct i2400M_l3l4_hdr and then
 *           followed by the payload. Once this function returns, the
 *           buffer can be reused.
 *
 * @buf_len: buffer size
 *
 * Returns:
 *
 * Pointer to skb containing the ack message. You need to check the
 * pointer with IS_ERR(), as it might be an error code. Error codes
 * could happen because:
 *
 *  - the message wasn't formatted correctly
 *  - couldn't send the message
 *  - failed waiting for a response
 *  - the ack message wasn't formatted correctly
 *
 * The returned skb has been allocated with wimax_msg_to_user_alloc(),
 * so it has enough header space to be passed directly to user
 * space with wimax_msg_to_user_send(). skb->data points to the ACK
 * payload.
 *
 * The skb has to be freed with kfree_skb() once done.
 *
 * Description:
 *
 * This function delivers a message/command to the device and waits
 * for an ack to be received. The format is described in
 * net/wimax-i2400m.h. In summary, a command/get/set is followed by an
 * ack.
 *
 * This function will not check the ack status, that's left up to the
 * caller.  Once done with the ack skb, it has to be kfree_skb()ed.
 *
 * The i2400m handles only one message at the same time, thus we need
 * the mutex to exclude other players.
 *
 * We write the message and then wait for an answer to come back. The
 * RX path intercepts control messages and handles them in
 * i2400m_rx_ctl(). Reports (notifications) are (maybe) processed
 * locally and then forwarded (as needed) to user space on the WiMAX
 * stack message pipe. Acks are saved and passed back to us through an
 * skb in i2400m->ack_skb() which is ready to be given to generic
 * netlink if need be.
 */
struct sk_buff *i2400m_msg_to_dev(struct i2400m *i2400m,
				  const void *buf, size_t buf_len)
{
	int result;
	struct device *dev = i2400m_dev(i2400m);
	const struct i2400m_l3l4_hdr *msg_l3l4_hdr;
	struct i2400m_l3l4_hdr *ack_l3l4_hdr;
	struct sk_buff *ack_skb;
	unsigned msg_type;
	size_t ack_size;

	d_fnstart(3, dev, "(i2400m %p buf %p len %zu)\n",
		  i2400m, buf, buf_len);

	if (i2400m->boot_mode)
		return ERR_PTR(-ENODEV);

	msg_l3l4_hdr = buf;
	/* Check msg & payload consistency */
	result = i2400m_msg_size_check(i2400m, msg_l3l4_hdr, buf_len);
	ack_skb = ERR_PTR(result);
	if (result < 0)
		goto error_bad_msg;
	msg_type = le16_to_cpu(msg_l3l4_hdr->type);
	d_printf(1, dev, "CMD/GET/SET 0x%04x %zu bytes\n",
		 msg_type, buf_len);
	d_dump(2, dev, buf, buf_len);

	mutex_lock(&i2400m->msg_mutex);		/* Send the message */
	init_completion(&i2400m->msg_completion);
	result = i2400m_tx(i2400m, buf, buf_len, I2400M_PT_CTRL);
	if (result < 0) {
		ack_skb = ERR_PTR(result);
		dev_err(dev, "can't send message 0x%04x: %d\n",
			le16_to_cpu(msg_l3l4_hdr->type), result);
		goto error_tx;
	}
	/* The RX path in rx.c will put any response for this message
	 * in i2400m->ack_skb and wake us up. */
	result = wait_for_completion_interruptible_timeout(
		&i2400m->msg_completion, HZ);
	if (result == 0) {
		dev_err(dev, "timeout waiting for reply to message 0x%04x\n",
			msg_type);
		ack_skb = ERR_PTR(-ETIMEDOUT);
		goto error_wait_for_completion;
	} else if (result < 0) {
		dev_err(dev, "error waiting for reply to message 0x%04x: %d\n",
			msg_type, result);
		ack_skb = ERR_PTR(result);
		goto error_wait_for_completion;
	}
	result = i2400m->ack_status;
	ack_skb = ERR_PTR(result);
	if (result < 0)
		goto error_ack_status;
	ack_l3l4_hdr = (void *) i2400m->ack_skb->data;
	ack_size = i2400m->ack_skb->len;
	/* Check stuff */
	result = i2400m_msg_size_check(i2400m, ack_l3l4_hdr,
				       i2400m->ack_skb->len);
	if (result < 0) {
		dev_err(dev, "HW BUG? reply to message 0x%04x: %d\n",
			msg_type, result);
		ack_skb = ERR_PTR(result);
		goto error_bad_ack_size;
	}
	if (msg_type != le16_to_cpu(ack_l3l4_hdr->type)) {
		dev_err(dev, "HW BUG? bad reply 0x%04x to message 0x%04x\n",
			le16_to_cpu(ack_l3l4_hdr->type), msg_type);
		ack_skb = ERR_PTR(-EIO);
		goto error_bad_ack_type;
	}
	i2400m_msg_ack_hook(i2400m, ack_l3l4_hdr, ack_size);
	ack_skb = i2400m->ack_skb;
	i2400m->ack_skb = NULL;
error_ack:
error_ack_status:
error_wait_for_completion:
error_tx:
	mutex_unlock(&i2400m->msg_mutex);
error_bad_msg:
	d_fnend(3, dev, "(i2400m %p buf %p len %zu) = %p\n",
		i2400m, buf, buf_len, ack_skb);
	return ack_skb;

error_bad_ack_type:
error_bad_ack_size:
	kfree_skb(i2400m->ack_skb);
	goto error_ack;
}


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
 * i2400m_cmd_diag_off - Turns all of device's diagnostics output off
 *
 * The write subsystem takes ownership of the skb, so we don't need to
 * free it.
 */
int i2400m_cmd_diag_off(struct i2400m *i2400m)
{
	int result;
	struct device *dev = i2400m_dev(i2400m);
	struct sk_buff *ack_skb;
	char strerr[32];

	struct i2400m_cmd_monitor_control *cmd;

	result = -ENOMEM;
	cmd = kzalloc(sizeof(*cmd), GFP_KERNEL);
	if (cmd)
		goto error_alloc;
	cmd->hdr.type = cpu_to_le16(I2400M_MT_CMD_MONITOR_CONTROL);
	cmd->hdr.length = cpu_to_le16(sizeof(*cmd) - sizeof(cmd->hdr));
	cmd->hdr.version = cpu_to_le16(I2400M_L3L4_VERSION);
	cmd->tlv.type = cpu_to_le16(I2400M_TLV_TYPE_MONITOR_CONTROL);
	cmd->tlv.length = cpu_to_le16(1);
	cmd->val = I2400M_MONITOR_CONTROL_OFF;

	ack_skb = i2400m_msg_to_dev(i2400m, cmd, sizeof(*cmd));
	result = PTR_ERR(ack_skb);
	if (IS_ERR(ack_skb)) {
		dev_err(dev, "Failed to issue 'Diagnostics off' command: %d\n",
			result);
		goto error_msg_to_dev;
	}
	result = i2400m_msg_check_status(i2400m, (void *) ack_skb->data,
					 strerr, sizeof(strerr));
	if (result < 0)
		dev_err(dev, "'Diagnostics off' (0x%04x) command failed: "
			"%d - %s\n", I2400M_MT_CMD_MONITOR_CONTROL, result,
			strerr);
	kfree_skb(ack_skb);
error_msg_to_dev:
	kfree(cmd);
error_alloc:
	return result;

}
EXPORT_SYMBOL_GPL(i2400m_cmd_diag_off);


/*
 * Definitions for the Enter Power Save command
 *
 * The Enter Power Save command requests the device to go into power
 * saving mode. The device will ack or nak the command depending on it
 * being ready for it. If it acks, we tell the USB subsystem to
 *
 * As well, the device might request to go into power saving mode by
 * sending a report (REPORT_POWERSAVE_READY), in which case, we issue
 * this command. The hookups in the RX coder allow
 */
enum {
	I2400M_WAKEUP_ENABLED  = 0x01,
	I2400M_WAKEUP_DISABLED = 0x02,
	I2400M_TLV_TYPE_WAKEUP_MODE = 144,
};

struct i2400m_cmd_enter_power_save {
	struct i2400m_l3l4_hdr hdr;
	struct i2400m_tlv_hdr tlv;
	__le32 val;
} __attribute__((packed));


/*
 * Request entering power save
 *
 * This command is (mainly) executed when the device indicates that it
 * is ready to go into powersave mode via a REPORT_POWERSAVE_READY. If
 * the command suceeds, then we enable USB autopm.
 */
int i2400m_cmd_enter_powersave(struct i2400m *i2400m)
{
	int result;
	struct device *dev = i2400m_dev(i2400m);
	struct sk_buff *ack_skb;
	struct i2400m_cmd_enter_power_save *cmd;
	char strerr[32];

	result = -ENOMEM;
	cmd = kzalloc(sizeof(*cmd), GFP_KERNEL);
	if (cmd == NULL)
		goto error_alloc;
	cmd->hdr.type = cpu_to_le16(I2400M_MT_CMD_ENTER_POWERSAVE);
	cmd->hdr.length = cpu_to_le16(sizeof(*cmd) - sizeof(cmd->hdr));
	cmd->hdr.version = cpu_to_le16(I2400M_L3L4_VERSION);
	cmd->tlv.type = cpu_to_le16(I2400M_TLV_TYPE_WAKEUP_MODE);
	cmd->tlv.length = cpu_to_le16(sizeof(cmd->val));
	cmd->val = cpu_to_le32(I2400M_WAKEUP_ENABLED);

	ack_skb = i2400m_msg_to_dev(i2400m, cmd, sizeof(*cmd));
	result = PTR_ERR(ack_skb);
	if (IS_ERR(ack_skb)) {
		dev_err(dev, "Failed to issue 'Enter power save' command: %d\n",
			result);
		goto error_msg_to_dev;
	}
	result = i2400m_msg_check_status(i2400m, (void *) ack_skb->data,
					 strerr, sizeof(strerr));
	if (result < 0)
		dev_err(dev, "'Enter power save' (0x%04x) command failed: "
			"%d - %s\n", I2400M_MT_CMD_ENTER_POWERSAVE,
			result, strerr);
	else {
		d_printf(1, dev, "device ready to power save\n");
#warning FIXME: disabled entering power management mode, USB dies with it
#if 1
		d_printf(0, dev, "FIXME: not entering power saving in host\n");
#else
		if (i2400m->bus_autopm_enable)
			i2400m->bus_autopm_enable(i2400m);
#endif
	}
	kfree_skb(ack_skb);
error_msg_to_dev:
	kfree(cmd);
error_alloc:
	return result;

}


/*
 * Definitions for getting device information
 */
enum {
	I2400M_TLV_DETAILED_DEVICE_INFO = 140
};

/**
 * i2400m_get_device_info - Query the device for detailed device information
 *
 * @i2400m: device descriptor
 *
 * Returns: an skb whose skb->data points to a 'struct
 *    i2400m_tlv_detailed_device_info'. When done, kfree_skb() it. The
 *    skb is *guaranteed* to contain the whole TLV data structure.
 *
 *    On error, IS_ERR(skb) is true and ERR_PTR(skb) is the error
 *    code.
 */
struct sk_buff *i2400m_get_device_info(struct i2400m *i2400m)
{
	int result;
	struct device *dev = i2400m_dev(i2400m);
	struct sk_buff *ack_skb;
	struct i2400m_l3l4_hdr *cmd, *ack;
	const struct i2400m_tlv_hdr *tlv;
	const struct i2400m_tlv_detailed_device_info *ddi;
	char strerr[32];

	ack_skb = ERR_PTR(-ENOMEM);
	cmd = kzalloc(sizeof(*cmd), GFP_KERNEL);
	if (cmd == NULL)
		goto error_alloc;
	cmd->type = cpu_to_le16(I2400M_MT_GET_DEVICE_INFO);
	cmd->length = 0;
	cmd->version = cpu_to_le16(I2400M_L3L4_VERSION);

	ack_skb = i2400m_msg_to_dev(i2400m, cmd, sizeof(*cmd));
	if (IS_ERR(ack_skb)) {
		dev_err(dev, "Failed to issue 'get device info' command: %ld\n",
			PTR_ERR(ack_skb));
		goto error_msg_to_dev;
	}
	result = i2400m_msg_check_status(i2400m, (void *) ack_skb->data,
					 strerr, sizeof(strerr));
	if (result < 0) {
		dev_err(dev, "'get device info' (0x%04x) command failed: "
			"%d - %s\n", I2400M_MT_GET_DEVICE_INFO, result,
			strerr);
		goto error_cmd_failed;
	}
	ack = (void *) ack_skb->data;
	tlv = i2400m_tlv_find(i2400m, ack->pl, ack_skb->len - sizeof(*ack),
			      I2400M_TLV_DETAILED_DEVICE_INFO, sizeof(*ddi));
	if (tlv == NULL) {
		dev_err(dev, "GET DEVICE INFO: "
			"detailed device info TLV not found (0x%04x)\n",
			I2400M_TLV_DETAILED_DEVICE_INFO);
		result = -EIO;
		goto error_no_tlv;
	}
	skb_pull(ack_skb, (void *) tlv - (void *) ack_skb->data);
error_msg_to_dev:
	kfree(cmd);
error_alloc:
	return ack_skb;

error_no_tlv:
error_cmd_failed:
	kfree_skb(ack_skb);
	kfree(cmd);
	return ERR_PTR(result);
}


/* Firmware interface versions we support */
enum {
	I2400M_HDIv_MAJOR_TOP = 9,
	I2400M_HDIv_MAJOR_BOTTOM = 8,
	I2400M_HDIv_MINOR = 0,
};


/**
 * i2400m_firmware_check - check firmware versions and stuff
 *
 * @i2400m: device descriptor
 *
 * Returns: 0 if ok, < 0 errno code an error and a message in the
 *    kernel log.
 *
 * Long function, but quite simple; first chunk launches the command
 * and double checks the reply for the right TLV. Then we process the
 * TLV (where the meat is).
 */
int i2400m_firmware_check(struct i2400m *i2400m)
{
	int result;
	struct device *dev = i2400m_dev(i2400m);
	struct sk_buff *ack_skb;
	struct i2400m_l3l4_hdr *cmd, *ack;
	const struct i2400m_tlv_hdr *tlv;
	const struct i2400m_tlv_l4_message_versions *l4mv;
	char strerr[32];
	unsigned major, minor, branch;

	result = -ENOMEM;
	cmd = kzalloc(sizeof(*cmd), GFP_KERNEL);
	if (cmd == NULL)
		goto error_alloc;
	cmd->type = cpu_to_le16(I2400M_MT_GET_LM_VERSION);
	cmd->length = 0;
	cmd->version = cpu_to_le16(I2400M_L3L4_VERSION);

	ack_skb = i2400m_msg_to_dev(i2400m, cmd, sizeof(*cmd));
	if (IS_ERR(ack_skb)) {
		result = PTR_ERR(ack_skb);
		dev_err(dev, "Failed to issue 'get lm version' command: %-d\n",
			result);
		goto error_msg_to_dev;
	}
	result = i2400m_msg_check_status(i2400m, (void *) ack_skb->data,
					 strerr, sizeof(strerr));
	if (result < 0) {
		dev_err(dev, "'get lm version' (0x%04x) command failed: "
			"%d - %s\n", I2400M_MT_GET_LM_VERSION, result,
			strerr);
		goto error_cmd_failed;
	}
	ack = (void *) ack_skb->data;
	tlv = i2400m_tlv_find(i2400m, ack->pl, ack_skb->len - sizeof(*ack),
			      I2400M_TLV_L4_MESSAGE_VERSIONS, sizeof(*l4mv));
	if (tlv == NULL) {
		dev_err(dev, "get lm version: TLV not found (0x%04x)\n",
			I2400M_TLV_L4_MESSAGE_VERSIONS);
		result = -EIO;
		goto error_no_tlv;
	}
	l4mv = container_of(tlv, typeof(*l4mv), hdr);
	major = le16_to_cpu(l4mv->major);
	minor = le16_to_cpu(l4mv->minor);
	branch = le16_to_cpu(l4mv->branch);
	result = -EINVAL;
	if (major < I2400M_HDIv_MAJOR_BOTTOM && major > I2400M_HDIv_MAJOR_TOP) {
		dev_err(dev, "unsupported major fw interface version "
			"%u.%u.%u\n", major, minor, branch);
		goto error_bad_major;
	}
	result = 0;
	if (minor != I2400M_HDIv_MINOR)
		dev_warn(dev, "untested minor fw firmware version %u.%u.%u\n",
			 major, minor, branch);
error_bad_major:
	dev_info(dev, "firmware interface version %u.%u.%u\n",
		 major, minor, branch);
error_no_tlv:
error_cmd_failed:
	kfree_skb(ack_skb);
error_msg_to_dev:
	kfree(cmd);
error_alloc:
	return result;
}


/*
 * Send an initialization command to the device
 */
int i2400m_cmd_init(struct i2400m *i2400m)
{
	int result;
	struct device *dev = i2400m_dev(i2400m);
	struct sk_buff *ack_skb;
	struct i2400m_l3l4_hdr *cmd;
	char strerr[32];

	result = -ENOMEM;
	cmd = kzalloc(sizeof(*cmd), GFP_KERNEL);
	if (cmd == NULL)
		goto error_alloc;
	cmd->type = cpu_to_le16(I2400M_MT_CMD_INIT);
	cmd->length = 0;
	cmd->version = cpu_to_le16(I2400M_L3L4_VERSION);

	ack_skb = i2400m_msg_to_dev(i2400m, cmd, sizeof(*cmd));
	result = PTR_ERR(ack_skb);
	if (IS_ERR(ack_skb)) {
		dev_err(dev, "Failed to issue 'init' command: %d\n",
			result);
		goto error_msg_to_dev;
	}
	result = i2400m_msg_check_status(i2400m, (void *) ack_skb->data,
					 strerr, sizeof(strerr));
	if (result < 0)
		dev_err(dev, "'cmd init' (0x%04x) command failed: %d - %s\n",
			I2400M_MT_CMD_INIT, result, strerr);
	kfree_skb(ack_skb);
error_msg_to_dev:
	kfree(cmd);
error_alloc:
	return result;

}
EXPORT_SYMBOL_GPL(i2400m_cmd_init);


/*
 * Query the device for its state, update the WiMAX stack's idea of it
 *
 * @i2400m: device descriptor
 *
 * Returns: 0 if ok, < 0 errno code on error.
 *
 * Executes a 'Get State' command and parses the returned
 * TLVs.
 *
 * Because this is almost identical to a 'Report State', we use
 * i2400m_report_state_hook() to parse the answer. This will set the
 * carrier state, as well as the RF Kill switches state.
 */
int i2400m_cmd_get_state(struct i2400m *i2400m)
{
	int result;
	struct device *dev = i2400m_dev(i2400m);
	struct sk_buff *ack_skb;
	struct i2400m_l3l4_hdr *cmd;
	const struct i2400m_l3l4_hdr *ack;
	char strerr[32];

	result = -ENOMEM;
	cmd = kzalloc(sizeof(*cmd), GFP_KERNEL);
	if (cmd == NULL)
		goto error_alloc;
	cmd->type = cpu_to_le16(I2400M_MT_GET_STATE);
	cmd->length = 0;
	cmd->version = cpu_to_le16(I2400M_L3L4_VERSION);

	ack_skb = i2400m_msg_to_dev(i2400m, cmd, sizeof(*cmd));
	if (IS_ERR(ack_skb)) {
		dev_err(dev, "Failed to issue 'get state' command: %ld\n",
			PTR_ERR(ack_skb));
		result = PTR_ERR(ack_skb);
		goto error_msg_to_dev;
	}
	result = i2400m_msg_check_status(i2400m, (void *) ack_skb->data,
					 strerr, sizeof(strerr));
	if (result < 0) {
		dev_err(dev, "'get state' (0x%04x) command failed: "
			"%d - %s\n", I2400M_MT_GET_STATE, result, strerr);
		goto error_cmd_failed;
	}
	ack = (void *) ack_skb->data;
	i2400m_report_state_hook(i2400m, ack, ack_skb->len - sizeof(*ack),
				 "GET STATE");
	result = 0;
	kfree_skb(ack_skb);
error_cmd_failed:
error_msg_to_dev:
	kfree(cmd);
error_alloc:
	return result;
}


/*
 * Send a configuration command to the device with the settings we want
 *
 */
int i2400m_set_init_config(struct i2400m *i2400m)
{
	int result;
	struct device *dev = i2400m_dev(i2400m);
	struct sk_buff *ack_skb;
	struct {
		struct i2400m_l3l4_hdr cmd;
		struct i2400m_tlv_config_idle_parameters tlv_idle;
	} __attribute__((packed)) *buf;
	struct i2400m_tlv_config_idle_parameters *tlv_idle;
	char strerr[32];

	d_fnstart(3, dev, "(i2400m %p)\n", i2400m);

	result = -ENOMEM;
	buf = kzalloc(sizeof(*buf), GFP_KERNEL);
	if (buf == NULL)
		goto error_alloc;

	buf->cmd.type = cpu_to_le16(I2400M_MT_SET_INIT_CONFIG);
	buf->cmd.length = cpu_to_le16(sizeof(*buf) - sizeof(buf->cmd));
	buf->cmd.version = cpu_to_le16(I2400M_L3L4_VERSION);

	tlv_idle = &buf->tlv_idle;
	tlv_idle->hdr.type = cpu_to_le16(I2400M_TLV_CONFIG_IDLE_PARAMETERS);
	tlv_idle->hdr.length = cpu_to_le16(
		sizeof(*tlv_idle) - sizeof(tlv_idle->hdr));
	tlv_idle->idle_timeout = 0;	/* Disable auto-entering idle */
	tlv_idle->idle_paging_interval = 0;	/* FIXME */

	ack_skb = i2400m_msg_to_dev(i2400m, &buf->cmd, sizeof(*buf));
	result = PTR_ERR(ack_skb);
	if (IS_ERR(ack_skb)) {
		dev_err(dev, "Failed to issue 'init config' command: %d\n",
			result);
		goto error_msg_to_dev;
	}
	result = i2400m_msg_check_status(i2400m, (void *) ack_skb->data,
					 strerr, sizeof(strerr));
	if (result < 0)
		dev_err(dev, "'init config' (0x%04x) command failed: %d - %s\n",
			I2400M_MT_CMD_INIT, result, strerr);
	kfree_skb(ack_skb);
error_msg_to_dev:
	kfree(buf);
error_alloc:
	d_fnend(3, dev, "(i2400m %p) = %d\n", i2400m, result);
	return result;

}
EXPORT_SYMBOL_GPL(i2400m_set_init_config);


