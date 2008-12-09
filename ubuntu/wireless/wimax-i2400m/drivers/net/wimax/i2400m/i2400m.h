/*
 * Intel Wireless WiMAX Connection 2400m
 * Declarations for bus-generic internal APIs
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
 * Yanir Lubetkin <yanirx.lubetkin@intel.com>
 *  - Initial implementation
 *
 *
 * GENERAL DRIVER ARCHITECTURE
 *
 * The i2400m driver is split in the following two major parts:
 *
 *  - bus specific driver
 *  - bus generic driver (this part)
 *
 * The bus specific driver sets up stuff specific to the bus the
 * device is connected to (USB, SDIO, PCI, tam-tam...non-authoritative
 * nor binding list) and then handles data frames for the the
 * bus-generic driver...which does the stuff that is common to all the
 * bus types.
 *
 * By functionality, the break up is:
 *
 *  - Firmware upload: fw.c - takes care of uploading firmware to the
 *        device. bus-specific driver has to do some stuff in here.
 *
 *  - RX handling: rx.c - receives data from the bus-specific code
 *        and feeds them to the network or WiMAX stack or uses it to
 *        modify the driver state. bus-specific driver only has to
 *        receive frames and pass them to this module.
 *
 *  - TX handling: tx.c - manages the TX FIFO queue and provides means
 *        for the bus-specific TX code to easily manage the FIFO
 *        queue. bus-specific code just pull frames from this module
 *        to send them to the device.
 *
 *  - netdev glue: netdev.c - interface with Linux networking
 *        stack. Pass around data frames, configure the device,
 *        etc... bus-generic only.
 *
 *        This code implements a pure IP device. There is no ARP,
 *        ethernet headers and the such.
 *
 *  - control ops: control.c - implements various commmands for
 *        controlling the device. bus-generic only.
 *
 *  - device model glue: driver.c - implements helpers for creating
 *        device-model glue. The bulk of these operations are done in
 *        the bus-specific code.
 *
 * APIs AND HEADER FILES
 *
 * This bus generic code exports three APIs:
 *
 *  - HDI (host-device interface) definitions common to all busses
 *  - internal API for the bus-generic part
 *  - external API for the bus-specific drivers
 *
 * Definitions of message formats are in net/wimax-i2400m.h; these can
 * be also used by user space code.
 */

#ifndef __I2400M_H__
#define __I2400M_H__

#include <linux/usb.h>
#include <linux/netdevice.h>
#include <linux/completion.h>
#include <linux/rwsem.h>
#include <asm/atomic.h>
#include <linux/wimax.h>
#include <net/wimax-i2400m.h>
#include <asm/byteorder.h>
#include <config.h>		/* @lket@ignore-line */
#include "edc.h"

/*
 * Host-Device interface common to all busses
 */

/* Packet types for the host-device interface */
enum i2400m_pt {
	I2400M_PT_DATA = 0,
	I2400M_PT_CTRL,
	I2400M_PT_TRACE,	/* For device debug */
	I2400M_PT_RESET_WARM,	/* device reset */
	I2400M_PT_RESET_COLD,	/* USB[transport] reset, like reconnect */
	I2400M_PT_ILLEGAL
};

/* Misc constants */
enum {
	I2400M_PL_PAD = 16,	/* Payload data size alignment */
	I2400M_PL_SIZE_MAX = 0x3EFF,
	I2400M_MAX_PLS_IN_MSG = 60,
	/* protocol barkers: sync sequences; for notifications they
	 * are sent in groups of four. */
	I2400M_H2D_PREVIEW_BARKER = 0xcafe900d,
	I2400M_COLD_RESET_BARKER = 0xc01dc01d,
	I2400M_WARM_RESET_BARKER = 0x50f750f7,
	I2400M_NBOOT_BARKER = 0xdeadbeef,
	I2400M_SBOOT_BARKER = 0x0ff1c1a1,
	I2400M_ACK_BARKER = 0xfeedbabe,
	I2400M_D2H_MSG_BARKER = 0xbeefbabe,
#ifndef I2400M_FW		/* Temporary */
	/* Firmware uploading */
	I2400M_BOOT_RETRIES = 3,
	/* Size of the Boot Mode Command buffer */
	I2400M_BM_CMD_BUF_SIZE = 16 * 1024,
	I2400M_BM_ACK_BUF_SIZE = 256,
#endif
};


/*
 * Hardware payload descriptor
 *
 * Bitfields encoded in a struct to enforce typing semantics.
 *
 * Look in rx.c and tx.c for a full description of the format.
 */
struct i2400m_pld {
	__le32 val;
} __attribute__ ((packed));

#define I2400M_PLD_SIZE_MASK 0x00003fff
#define I2400M_PLD_TYPE_SHIFT 16
#define I2400M_PLD_TYPE_MASK 0x000f0000

static inline
size_t i2400m_pld_size(const struct i2400m_pld *pld)
{
	return I2400M_PLD_SIZE_MASK & le32_to_cpu(pld->val);
}

static inline
enum i2400m_pt i2400m_pld_type(const struct i2400m_pld *pld)
{
	return (I2400M_PLD_TYPE_MASK & le32_to_cpu(pld->val))
		>> I2400M_PLD_TYPE_SHIFT;
}

static inline
void i2400m_pld_set(struct i2400m_pld *pld, size_t size,
		    enum i2400m_pt type)
{
	pld->val = cpu_to_le32(
		((type << I2400M_PLD_TYPE_SHIFT) & I2400M_PLD_TYPE_MASK)
		|  (size & I2400M_PLD_SIZE_MASK));
}


/*
 * Header for a TX message or RX message
 *
 * @barker: preamble
 * @size: used for management of the FIFO queue buffer; before
 *     sending, this is converted to be a real preamble. This
 *     indicates the real size of the TX message that starts at this
 *     point. If the highest bit is set, then this message is to be
 *     skipped.
 * @sequence: sequence number of this message
 * @offset: offset where the message itself starts -- see the comments
 *     in the file header about message header and payload descriptor
 *     alignment.
 * @num_pls: number of payloads in this message
 * @padding: amount of padding bytes at the end of the message to make
 *           it be of block-size aligned
 *
 * Look in rx.c and tx.c for a full description of the format.
 */
struct i2400m_msg_hdr {
	union {
		__le32 barker;
		u32 size;
	};
	union {
		__le32 sequence;
		u32 offset;
	};
	__le16 num_pls;
	__le16 rsv1;
	__le16 padding;
	__le16 rsv2;
	struct i2400m_pld pld[0];
} __attribute__ ((packed));


/* How do you want to reset the device? */
enum i2400m_reset_type {
	I2400M_RT_SOFT,	/* first measure */
	I2400M_RT_COLD,	/* second measure */
	I2400M_RT_BUS,	/* call in artillery */
};


/* Boot-mode (firmware upload mode) commands */
#ifndef I2400M_FW

/* Boot mode opcodes */
enum i2400m_brh_opcode {
	I2400M_BRH_READ = 1,
	I2400M_BRH_WRITE = 2,
	I2400M_BRH_JUMP = 3,
	I2400M_BRH_SIGNED_JUMP = 8,
	I2400M_BRH_HASH_PAYLOAD_ONLY = 9,
};

/**
 * i2400m_bootrom_header - Header for a boot-mode command
 *
 * @cmd: the above command descriptor
 * @target_addr: where on the device memory should the action be performed.
 * @data_size: for read/write, amount of data to be read/written
 * @block_checksum: checksum value (if applicable)
 * @payload: the beginning of data attached to this header
 */
struct i2400m_bootrom_header {
	__le32 command;
	__le32 target_addr;
	__le32 data_size;
	__le32 block_checksum;
	char payload[0];
} __attribute__ ((packed));


static inline
__le32 i2400m_brh_command(enum i2400m_brh_opcode opcode, unsigned use_checksum,
			  unsigned direct_access)
{
	return cpu_to_le32(
		0xcbbc0000
		| (direct_access? 0x00000400 : 0)
		| 0x00000200 /* response always required */
		| (use_checksum? 0x00000100 : 0)
		| (opcode & 0x0f));
}

static inline
void i2400m_brh_set_opcode(struct i2400m_bootrom_header *hdr,
			   enum i2400m_brh_opcode opcode)
{
	hdr->command = cpu_to_le32(
		(le32_to_cpu(hdr->command) & ~0x0000000f)
		| (opcode & 0x00000000f));
}

static inline
unsigned i2400m_brh_get_opcode(const struct i2400m_bootrom_header *hdr)
{
	return le32_to_cpu(hdr->command) & 0x0000000f;
}

static inline
unsigned i2400m_brh_get_response(const struct i2400m_bootrom_header *hdr)
{
	return (le32_to_cpu(hdr->command) & 0x000000f0) >> 4;
}

static inline
unsigned i2400m_brh_get_use_checksum(const struct i2400m_bootrom_header *hdr)
{
	return le32_to_cpu(hdr->command) & 0x00000100;
}

static inline
unsigned i2400m_brh_get_response_required(
	const struct i2400m_bootrom_header *hdr)
{
	return le32_to_cpu(hdr->command) & 0x00000200;
}

static inline
unsigned i2400m_brh_get_direct_access(const struct i2400m_bootrom_header *hdr)
{
	return le32_to_cpu(hdr->command) & 0x00000400;
}

static inline
unsigned i2400m_brh_get_signature(const struct i2400m_bootrom_header *hdr)
{
	return (le32_to_cpu(hdr->command) & 0xffff0000) >> 16;
}

#else
struct i2400m_bootrom_header;
#endif

/*
 * i2400m driver data structures
 */

/**
 * struct i2400m - descriptor for the hardware state
 *
 * Members marked with [fill] must be filled out/initialized before
 * calling i2400m_setup().
 *
 * @wimax_dev: Wimax device -- Due to the way a net_device is
 *     allocated, we need to force this to be the first field so that
 *     we can get from net_dev_priv() the right pointer.
 *
 * @lock: spinlock to protect the RX members
 *
 * @stack_rwsem: gatekeeper for operations coming from the stack. When
 *     this semaphoire is write-locked, the device can't accept any
 *     operations coming from the stack. So the stack operation entry
 *     points do read-trylocks-or-fail (many can happen at the same
 *     time), and when we need to shut it down to all, we do a
 *     write-lock.
 *
 * @bus_tx_kick: [fill] notification for the bus layer to know there
 *     is data in the TX fifo ready to send.
 *
 *     This function cannot sleep.
 *
 * @bus_autopm_enable: [fill] bus specific operation to tell the bus the
 *     device is ready to go powersave. This is required by USB. If
 *     your bus doesn't need it, feel free to leave this as NULL.
 *
 * @bus_reset: [fill] operation to reset the device in various ways
 *     In most cases, the bus-specific code can provide optimized
 *     versions.
 *
 *     If soft or cold reset fail, this function is expected to do a
 *     bus-specific reset (eg: USB reset) to get the device to a
 *     working state (even if it implies device disconecction).
 *
 *     Note the soft reset is used by the firmware uploader to
 *     reinitialize the device.
 *
 * @bus_bm_cmd_send: sends a boot-mode command described by the
 *     argument (of the given size). Flags are defined by the '
 *     i2400m_bm_cmd_flags'. This is synchronous and has to return 0
 *     if ok or < 0 errno code in any error condition.
 *
 * @bus_bm_wait_for_ack: Wait for reception of a response to a
 *     previously sent boot-mode command (or to any kind of boot-mode
 *     output from the device). Will read until all the indicated size
 *     is read or timeout. Reading more or less data than asked for is
 *     an error condition. Return 0 if ok, < 0 errno code on error.
 *
 * @tx_lock: spinlock to protect TX members
 *
 * @tx_buf: FIFO buffer for TX; we queue data here
 *
 * @tx_in: FIFO index for incoming data. Note this doesn't wrap around
 *     and it is always greater than @tx_out.
 *
 * @tx_out: FIFO index for outgoing data
 *
 * @tx_msg: current TX message that is active in the FIFO for
 *     appending payloads.
 *
 * @tx_block_size: [fill] SDIO imposes a 256 block size, USB 16, so we
 *     have a tx_blk_size variable that the bus layer sets to tell the
 *     engine how much of that we need.
 *
 * @tx_sequence: current sequence number for TX messages from the
 *     device to the host.
 *
 * @tx_msg_size: size of the current message being transmitted by the
 *     bus-specific code.
 *
 * @txrx_pl_num: total number of payloads sent|received
 *
 * @txrx_pl_max: maximum number of payloads sent|received in a TX message
 *
 * @txrx_pl_min: minimum number of payloads sent|received in a TX message
 *
 * @txrx_num: number of TX|RX messages sent|received
 *
 * @txrx_size_acc: number of bytes in all TX|RX messages sent|received
 *     (this is different to net_dev's statistics as it also counts
 *     control messages).
 *
 * @txrx_size_min: smallest TX|RX message sent|received.
 *
 * @txrx_size_max: buggest TX|RX message sent|received.
 *
 * @msg_mutex: mutex used to send control commands to the device (we
 *     only allow one at a time).
 *
 * @msg_completion: used to wait for an ack to a control command sent
 *     to the device.
 *
 * @ack_status: used to store the reception status of an ack to a
 *     control command. Only valid after @msg_completion is woken
 *     up. Only updateable if @msg_completion is armed.
 *
 * @ack_skb: used to store the actual ack to a control command.
 *
 * @bm_cmd_buf: boot mode command buffer for composing firmware upload
 *     commands. Allocated, managed and used by the bus-specific driver.
 *
 * @bm_cmd_buf: boot mode acknoledge buffer for staging reception of
 *     responses to commands. Allocated, managed and used by the
 *     bus-specific driver.
 *
 * @work_queue: work queue for scheduling miscellaneous work. This is
 *     only to be used for scheduling TX or RX processing, as well as
 *     reset operations. Processing of the actual RX data has to
 *     happen in the global workqueue or you will deadlock (sending
 *     and receiving commands might be using this workqueue).
 *
 * Boot Mode Command/Ack Buffers:
 *
 * Staging area for boot-mode (fw upload) commands and responses
 *
 * USB can't r/w to stack, vmalloc, etc...as well, we end up having to
 * alloc/free a lot to compose commands, so we use these for stagging
 * and not having to realloc all the eim.
 *
 * This assumes the code always runs serialized. Only one thread can
 * call i2400m_bm_cmd() at the same time.
 */
struct i2400m {
	struct wimax_dev wimax_dev;	/* FIRST! See doc */

	struct rw_semaphore stack_rwsem;
	unsigned boot_mode:1;
	unsigned sboot:1;		/* signed or unsigned fw boot */
	unsigned ready:1;		/* all probing steps done */

	void (*bus_tx_kick)(struct i2400m *);
	void (*bus_autopm_enable)(struct i2400m *);
	int (*bus_reset)(struct i2400m *, enum i2400m_reset_type);
	ssize_t (*bus_bm_cmd_send)(struct i2400m *,
				   const struct i2400m_bootrom_header *,
				   size_t, int flags);
	ssize_t (*bus_bm_wait_for_ack)(struct i2400m *,
				       struct i2400m_bootrom_header *, size_t);

	spinlock_t tx_lock;		/* protect TX state */
	void *tx_buf;
	size_t tx_in, tx_out;
	struct i2400m_msg_hdr *tx_msg;
	size_t tx_block_size, tx_sequence, tx_msg_size;
	/* TX stats */
	unsigned tx_pl_num, tx_pl_max, tx_pl_min,
		tx_num, tx_size_acc, tx_size_min, tx_size_max;

	/* RX stats */
	spinlock_t rx_lock;		/* protect RX state */
	unsigned rx_pl_num, rx_pl_max, rx_pl_min,
		rx_num, rx_size_acc, rx_size_min, rx_size_max;

	struct mutex msg_mutex;		/* serialize command execution */
	struct completion msg_completion;
	int ack_status;
	struct sk_buff *ack_skb;

	void *bm_ack_buf;		/* for receiving acks over USB */
	void *bm_cmd_buf;		/* for issuing commands over USB */

	struct workqueue_struct *work_queue;
};


/* Initialize a 'struct i2400m' from all zeroes */
static inline
void i2400m_init(struct i2400m *i2400m)
{
	wimax_dev_init(&i2400m->wimax_dev);

	init_rwsem(&i2400m->stack_rwsem);
	i2400m->boot_mode = 1;

	spin_lock_init(&i2400m->tx_lock);
	i2400m->tx_pl_min = ULONG_MAX;
	i2400m->tx_size_min = ULONG_MAX;

	spin_lock_init(&i2400m->rx_lock);
	i2400m->rx_pl_min = ULONG_MAX;
	i2400m->rx_size_min = ULONG_MAX;

	mutex_init(&i2400m->msg_mutex);
	init_completion(&i2400m->msg_completion);
}


/* bus-generic internal APIs */

static inline struct i2400m *wimax_dev_to_i2400m(struct wimax_dev *wimax_dev)
{
	return container_of(wimax_dev, struct i2400m, wimax_dev);
}
static inline struct i2400m *net_dev_to_i2400m(struct net_device *net_dev)
{
	return wimax_dev_to_i2400m(netdev_priv(net_dev));
}

extern void i2400m_netdev_setup(struct net_device *net_dev);
extern int i2400m_sysfs_setup(struct device_driver *);
extern void i2400m_sysfs_release(struct device_driver *);
extern int i2400m_tx_setup(struct i2400m *);
extern void i2400m_tx_release(struct i2400m *);
extern void i2400m_net_rx(struct i2400m *, struct sk_buff *, const void *, int);
enum i2400m_pt;
extern int i2400m_tx(struct i2400m *, const void *, size_t, enum i2400m_pt);
extern struct attribute_group i2400m_dev_attr_group;

/* API for the bus-specific drivers */

static inline
struct i2400m *i2400m_get(struct i2400m *i2400m)
{
	dev_hold(i2400m->wimax_dev.net_dev);
	return i2400m;
}

static inline
void i2400m_put(struct i2400m *i2400m)
{
	dev_put(i2400m->wimax_dev.net_dev);
}

extern int i2400m_setup(struct i2400m *);
extern void i2400m_setup_mac_addr(struct i2400m *, const u8[ETH_ALEN]);
extern int i2400m_setup_device(struct i2400m *);
extern int i2400m_setup_device_post(struct i2400m *);
extern void i2400m_release(struct i2400m *);

extern int i2400m_rx(struct i2400m *, struct sk_buff *);
extern void i2400m_tx_msg_sent(struct i2400m *);
extern struct i2400m_msg_hdr *i2400m_tx_msg_get(struct i2400m *, size_t *);

/* Firmware upload stuff */
#ifndef I2400M_FW		/* Temporary */
enum i2400m_bm_cmd_flags {	/* Flags for i2400m_bm_cmd() */
	/* Send the command header verbatim, no processing */
	I2400M_BM_CMD_RAW	= 1 << 2,
};
extern void i2400m_bm_cmd_prepare(struct i2400m_bootrom_header *);
#endif
extern int i2400m_dev_bootstrap(struct i2400m *);


static const __le32 i2400m_NBOOT_BARKER[4] = {
	__constant_cpu_to_le32(I2400M_NBOOT_BARKER),
	__constant_cpu_to_le32(I2400M_NBOOT_BARKER),
	__constant_cpu_to_le32(I2400M_NBOOT_BARKER),
	__constant_cpu_to_le32(I2400M_NBOOT_BARKER)
};

static const __le32 i2400m_SBOOT_BARKER[4] = {
	__constant_cpu_to_le32(I2400M_SBOOT_BARKER),
	__constant_cpu_to_le32(I2400M_SBOOT_BARKER),
	__constant_cpu_to_le32(I2400M_SBOOT_BARKER),
	__constant_cpu_to_le32(I2400M_SBOOT_BARKER)
};

static const __le32 i2400m_WARM_RESET_BARKER[4] = {
	__constant_cpu_to_le32(I2400M_WARM_RESET_BARKER),
	__constant_cpu_to_le32(I2400M_WARM_RESET_BARKER),
	__constant_cpu_to_le32(I2400M_WARM_RESET_BARKER),
	__constant_cpu_to_le32(I2400M_WARM_RESET_BARKER)
};


/* utility functions */
static inline
struct device *i2400m_dev(struct i2400m *i2400m)
{
	return i2400m->wimax_dev.net_dev->dev.parent;
}

/*
 * Helper for scheduling simple work functions
 *
 * This struct can get any kind of payload attached (normally in the
 * form of a struct where you pack the stuff you want to pass to the
 * _work function).
 */
struct i2400m_work {
	struct work_struct ws;
	struct i2400m *i2400m;
	u8 pl[0];
};
extern int i2400m_schedule_work(struct i2400m *, unsigned,
				void (*)(struct work_struct *), gfp_t,
				const void *, size_t);

extern int i2400m_msg_check_status(struct i2400m *,
				   const struct i2400m_l3l4_hdr *,
				   char *, size_t);
extern int i2400m_msg_size_check(struct i2400m *,
				 const struct i2400m_l3l4_hdr *, size_t);
extern struct sk_buff *i2400m_msg_to_dev(struct i2400m *, const void *, size_t);
extern void i2400m_msg_ack_hook(struct i2400m *,
				const struct i2400m_l3l4_hdr *, size_t);
extern void i2400m_report_hook(struct i2400m *,
			       const struct i2400m_l3l4_hdr *, size_t);
extern int i2400m_cmd_enter_powersave(struct i2400m *);
extern int i2400m_cmd_diag_off(struct i2400m *);
extern int i2400m_cmd_init(struct i2400m *);
extern int i2400m_cmd_get_state(struct i2400m *);
extern struct sk_buff *i2400m_get_device_info(struct i2400m *);
extern int i2400m_firmware_check(struct i2400m *);
extern int i2400m_set_init_config(struct i2400m *);

static inline
struct usb_endpoint_descriptor *usb_get_epd(struct usb_interface *iface, int ep)
{
	return &iface->cur_altsetting->endpoint[ep].desc;
}

extern int i2400m_op_rfkill_sw_toggle(struct wimax_dev *,
				      enum wimax_rfkill_state);
extern void i2400m_report_tlv_rf_switches_status(
	struct i2400m *, const struct i2400m_tlv_rf_switches_status *);


/*
 * Do a millisecond-sleep for allowing wireshark to dump all the data
 * packets. Used only for debugging.
 */
static inline
void __i2400m_msleep(unsigned ms)
{
#if 1
#else
	msleep(ms);
#endif
}

#endif /* #ifndef __I2400M_H__ */
