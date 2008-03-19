/* -*-linux-c-*-
 *
 * Intel Wireless WiMax Connection 2400m
 * Firmware uploader
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
 * Yanir Lubetkin <yanirx.lubetkin@intel.com>
 * Inaky Perez-Gonzalez <inaky.perez-gonzalez@intel.com>
 *  - Initial implementation
 *
 *
 * THE PROCEDURE
 *
 * The 2400m works in two modes: boot-mode or normal mode. In boot
 * mode we can execute only a handful of commands targeted at
 * uploading the firmware and launching it.
 *
 * The 2400m enters boot mode when it is first connected to the
 * system, when it crashes and when you ask it to reboot.
 *
 * Upon entrance to boot mode, the device sends a few zero length
 * packets (ZLPs) on the notification endpoint, then a reboot barker
 * (4 le32 words with value 0xdeadbeef). We need ack it by sending a
 * reboot barker on the bulk out endpoint. He will ack with a reboot
 * ack barker (4 le32 words with value 0xfeedbabe) and then the device
 * is fully rebooted. At this point we can upload the firmware. This
 * is accomplished by the i2400m_bootrom_init() function.
 *
 * We can then upload the ELF file(s) data section by using a write
 * command. All the commands follow a well-defined format, will ack
 * reception on the notification endpoint and can be protected with a
 * checksum. The function i2400m_bm_cmd() takes care of all the
 * execution details. If a command execution causes a device reboot,
 * you need to go back to _bootrom_init().
 *
 * Once firmware is uploaded, we are good to go :)
 *
 * When we don't know in which mode we are, we first try by sending a
 * soft reset request that will take us to boot-mode. If we time out
 * waiting for a reboot barker, that means maybe we are already in
 * boot mode, so we send a reboot barker.
 *
 * COMMAND EXECUTION:
 *
 * This code (and process) is single threaded; for executing commands,
 * we post a URB to the notification buffer, post the command, wait
 * for data on the notification buffer. We don't need to worry about
 * others as we know we are the only ones in there.
 *
 * ROADMAP:
 *
 * i2400m_dev_bootstrap
 *   request_firmware
 *   i2400m_bootrom_init
 *     i2400m_bm_cmd
 *   i2400m_download_file
 *     i2400m_download_buffer
 *       i2400m_download_chunk
 *         i2400m_bm_cmd
 *     i2400m_download_post_action
 *       i2400m_download_buffer
 *       i2400m_jump_to_main
 *   release_firmware
 *
 * i2400m_bm_cmd
 *   i2400m_notif_submit
 *   __i2400m_bm_cmd_send
 *     __i2400m_bm_cmd_prepare(cmd);
 *     i2400m_tx_bulk_out
 *   __i2400m_bm_wait_for_ack
 *     i2400m_notif_submit
 *   __i2400m_bm_ack_verify
 */
#warning FIXME: endianness in ELF headers -- might or might not be an issue? our fw files are probably LE native
#include <linux/firmware.h>
#include <linux/sched.h>
#include "i2400m.h"

#define D_LOCAL 1
#include "../debug.h"


/* Boot mode commands */
enum {
	I2400M_BOC_PING = 0,
	I2400M_BOC_READ,
	I2400M_BOC_WRITE,
	I2400M_BOC_JUMP,
	I2400M_BOC_REBOOT,
	I2400M_BOC_WR_PERSIST,
	I2400M_BOC_RD_PERSIST,
	I2400M_BOC_RD_MOD_WR,
	I2400M_BOC_LAST = 15
};

/**
 * i2400m_bootstrap_cmd - Command descriptor passed to the device's boot rom
 *
 * @command_signature: preamble
 * @command_reserved: unused
 * @command_direct_access: indicates the write mode
 * @command_response_required: flags the boot rom to respond on completion
 * @command_use_checksum: should checksum be performed on the attached buffer
 * @command_response_code: type of response to send on successful completion
 * @command_opcode: what should be done upon reception of the descriptor
 */
union i2400m_bootstrap_cmd {
	struct {
#if defined(__LITTLE_ENDIAN)
		u32 command_opcode:4;
		u32 command_response_code:4;
		u32 command_use_checksum:1;
		u32 command_response_required:1;
		u32 command_direct_access:1;
		u32 command_reserved:5;
		u32 command_signature:16;
#elif defined(__BIG_ENDIAN)
		u32 command_signature:16;
		u32 command_reserved:5;
		u32 command_direct_access:1;
		u32 command_response_required:1;
		u32 command_use_checksum:1;
		u32 command_response_code:4;
		u32 command_opcode:4;
#else
#error "Neither __LITTLE_ENDIAN nor __BIG_ENDIAN are defined."
#endif
	};
	__le32 value;
} __attribute__ ((packed));


/**
 * i2400m_bootrom_header - First or only part of any buffer sent to the rom
 *
 * @cmd: the above command descriptor
 * @target_addr: where on the device memory should the action be performed.
 * @data_size: for read/write, amount of data to be read/written
 * @block_checksum: checksum value (if applicable)
 * @payload: the beginning of data attached to this header
 */
struct i2400m_bootrom_header {
	union i2400m_bootstrap_cmd cmd;
	__le32 target_addr;
	__le32 data_size;
	__le32 block_checksum;
	char payload[0];
} __attribute__ ((packed));


/* firmware related constants */
enum {
	/*
	 * 16k - 256.
	 *
	 * Optimal block size is 256, and we want to transfer as much
	 * as possible in a single shot. This forces us to allocate a
	 * way too big buffer tho. So to make space for headers and
	 * stuff, we bit a chunk off (256).
	 */
	I2400M_DIRECT_CHUNK_MAX_LEN = 16128,
	I2400M_INDIRECT_CHUNK_MAX_LEN = 0x800,
	I2400M_BOOT_COMMAND_SIGNATURE = 0xCBBC,
	I2400M_MSG_TO_1_REGISTER = 0x86020,
	I2400M_MAX_LOADABLE_ADDR = 0x0C000000,
	I2400M_NVM_MAC_ADDR = 0x00203FE8,
	I2400M_BOOT_RESP_SIZE = 256,
	I2400M_FW_NFILES = 7,
	I2400M_MAX_FW_NAME = 50,
	I2400M_BOOT_RETRIES = 3,
};

const __le32 i2400m_ACK_BARKER[4] = {
	__constant_cpu_to_le32(0xfeedbabe),
	__constant_cpu_to_le32(0xfeedbabe),
	__constant_cpu_to_le32(0xfeedbabe),
	__constant_cpu_to_le32(0xfeedbabe)
};

/* Action to be performed after command comletion */
enum {
	I2400M_PDO_NOTHING = 0,
	I2400M_PDO_NOTIFY,
	I2400M_PDO_JMP
};

/**
 * struct i2400m_fw_file_data - metadata regarding a firmware file
 *
 * @name: file name on disk
 * @direct_access: should this data be written to the device in direct mode
 * @post_action: what should be done upon sucessful completion of data write
 * @use_checksum: should a checksum validation be performed on the buffer parts
 * @fw_entry: buffer of the firmware data to be written
 */
struct i2400m_fw_file_data {
	char name[I2400M_MAX_FW_NAME];
	int direct_access;
	int post_action;
	int use_chksum;
	struct firmware *fw_entry;
};

/* *INDENT-OFF* */
struct i2400m_fw_file_data i2400m_fw_data[I2400M_FW_NFILES] = {
	{ "Poke.elf",              1, I2400M_PDO_NOTHING, 1, NULL },
	{ "integCDMA.data.elf",    1, I2400M_PDO_NOTHING, 1, NULL },
	{ "integCDMA.op.elf",      1, I2400M_PDO_NOTHING, 1, NULL },
	{ "DM_FPGA_USB.elf",       1, I2400M_PDO_NOTIFY,  0, NULL },
	{ "DspsIniFile.elf",       1, I2400M_PDO_NOTHING, 1, NULL },
	{ "FW_DynamicConfig.elf",  1, I2400M_PDO_NOTHING, 1, NULL },
	{ "MacPhyIntegration.elf", 1, I2400M_PDO_JMP,     1, NULL }
};
/* *INDENT-ON* */

/* little endian firmware elf file parsing structures and conversion routines */
typedef struct elf32le_hdr {
	unsigned char e_ident[EI_NIDENT];
	__le16 e_type;
	__le16 e_machine;
	__le32 e_version;
	__le32 e_entry;		/* Entry point */
	__le32 e_phoff;
	__le32 e_shoff;
	__le32 e_flags;
	__le16 e_ehsize;
	__le16 e_phentsize;
	__le16 e_phnum;
	__le16 e_shentsize;
	__le16 e_shnum;
	__le16 e_shstrndx;
} Elf32le_Ehdr;

typedef struct elf32le_phdr {
	__le32 p_type;
	__le32 p_offset;
	__le32 p_vaddr;
	__le32 p_paddr;
	__le32 p_filesz;
	__le32 p_memsz;
	__le32 p_flags;
	__le32 p_align;
} Elf32le_Phdr;

void elf32_ehdr_le_to_cpu(Elf32le_Ehdr * leh, Elf32_Ehdr * h)
{
	h->e_type = le16_to_cpu(leh->e_type);
	h->e_machine = le16_to_cpu(leh->e_machine);
	h->e_version = le32_to_cpu(leh->e_version);
	h->e_entry = le32_to_cpu(leh->e_entry);
	h->e_phoff = le32_to_cpu(leh->e_phoff);
	h->e_shoff = le32_to_cpu(leh->e_shoff);
	h->e_flags = le32_to_cpu(leh->e_flags);
	h->e_ehsize = le16_to_cpu(leh->e_ehsize);
	h->e_phentsize = le16_to_cpu(leh->e_phentsize);
	h->e_phnum = le16_to_cpu(leh->e_phnum);
	h->e_shentsize = le16_to_cpu(leh->e_shentsize);
	h->e_shnum = le16_to_cpu(leh->e_shnum);
	h->e_shstrndx = le16_to_cpu(leh->e_shstrndx);
}

void elf32_phdr_le_to_cpu(Elf32le_Phdr * leh, Elf32_Phdr * h)
{
	h->p_type = le32_to_cpu(leh->p_type);
	h->p_offset = le32_to_cpu(leh->p_offset);
	h->p_vaddr = le32_to_cpu(leh->p_vaddr);
	h->p_paddr = le32_to_cpu(leh->p_paddr);
	h->p_filesz = le32_to_cpu(leh->p_filesz);
	h->p_memsz = le32_to_cpu(leh->p_memsz);
	h->p_flags = le32_to_cpu(leh->p_flags);
	h->p_align = le32_to_cpu(leh->p_align);
}


/*
 * __i2400m_send_barker - Sends a barker buffer to the device
 *
 * This helper will allocate a kmalloced buffer and use it to transmit
 * (then free it). Reason for this is that other arches cannot use
 * stack/vmalloc/text areas for DMA transfers.
 */
int __i2400m_send_barker(struct i2400m *i2400m,
			 const __le32 *barker,
			 size_t barker_size,
			 unsigned endpoint)
{
	struct usb_endpoint_descriptor *epd = NULL;
	int pipe, actual_len, ret;
	struct device *dev = &i2400m->usb_iface->dev;
	void *buffer;

	d_fnstart(4, dev, "(i2400m %p)\n", i2400m);
	ret = -ENOMEM;
	buffer = kmalloc(barker_size, GFP_KERNEL);
	if (buffer == NULL)
		goto error_kzalloc;
	epd = &i2400m->usb_iface->cur_altsetting
		->endpoint[endpoint].desc;
	pipe = usb_sndbulkpipe(i2400m->usb_dev, epd->bEndpointAddress);
	memcpy(buffer, barker, barker_size);
	ret = usb_bulk_msg(i2400m->usb_dev, pipe, buffer, barker_size,
			   &actual_len, HZ);
	if (ret < 0)
		d_printf(0, dev, "E: barker error: %d\n", ret);
	else if (actual_len != barker_size) {
		d_printf(0, dev, "E: only %d bytes transmitted\n", actual_len);
		ret = -EIO;
	}
	kfree(buffer);
error_kzalloc:
	d_fnend(4, dev, "(i2400m %p) = %d\n", i2400m, ret);
	return ret;
}



/**
 * i2400m_tx_bulk_out - synchronous write to the device
 *
 * Takes care of updating EDC counts and thus, handle device errors.
 */
ssize_t i2400m_tx_bulk_out(struct i2400m *i2400m, void *buf, size_t buf_size)
{
	int result;
	int len;
	struct usb_endpoint_descriptor *epd;
	int pipe;

	epd = &i2400m->usb_iface->cur_altsetting
		->endpoint[I2400M_EP_BULK_OUT].desc;
	pipe = usb_sndbulkpipe(i2400m->usb_dev, epd->bEndpointAddress);
	result = usb_bulk_msg(i2400m->usb_dev, pipe, buf, buf_size, &len, HZ);
	if (result < 0)
		return result;
#warning FIXME: plug in EDC support for error checking
	return len;
}


static void __i2400m_bm_notif_cb(struct urb *urb)
{
	complete(urb->context);
}

/**
 * i2400m_notif_submit - submit a read to the notification endpoint
 *
 * @i2400m: device descriptor
 * @urb: urb to use
 * @buf: buffer where to place the data
 * @completion: completion varible to complete when done
 */
int i2400m_notif_submit(struct i2400m *i2400m, struct urb *urb,
			void *buf, size_t buf_size,
			struct completion *completion)
{
	struct usb_endpoint_descriptor *epd;
	int pipe;

	epd = &i2400m->usb_iface->cur_altsetting
		->endpoint[I2400M_EP_NOTIFICATION].desc;
	pipe = usb_rcvintpipe(i2400m->usb_dev, epd->bEndpointAddress);
	usb_fill_int_urb(urb, i2400m->usb_dev, pipe, buf, buf_size,
			 __i2400m_bm_notif_cb, completion,
			 epd->bInterval);
	return usb_submit_urb(urb, GFP_KERNEL);
}


/*
 * Prepare a boot-mode command for delivery
 *
 * Adds signatures, computes checksum and flips byte sex. After
 * calling this function, DO NOT modify the command header as the
 * checksum won't work anymore.
 */
static
void __i2400m_bm_cmd_prepare(struct i2400m_bootrom_header *cmd)
{
#warning FIXME: dont use a bitfield for the command header (LE mess)
	cmd->cmd.command_signature = I2400M_BOOT_COMMAND_SIGNATURE;
	if (cmd->cmd.command_use_checksum) {
		int i;
		u32 checksum = 0;
		const u32 *checksum_ptr = (void *) cmd->payload;
		for (i = 0; i < cmd->data_size / 4; i++)
			checksum += cpu_to_le32(*checksum_ptr++);
		checksum += cmd->cmd.value + cmd->target_addr + cmd->data_size;
		cmd->block_checksum = cpu_to_le32(checksum);
	}
	cmd->cmd.value = cpu_to_le32(cmd->cmd.value);
	cmd->target_addr = cpu_to_le32(cmd->target_addr);
	cmd->data_size = cpu_to_le32(cmd->data_size);
}


/* Flags for i2400m_bm_cmd() */
enum {
	/* Send the command header verbatim, no processing */
	I2400M_BM_CMD_RAW	= 1 << 2,
};


/*
 * Send a boot-mode command over the bulk-out pipe
 *
 * Command can be a raw command, which requires no preparation (and
 * which might not even be following the command format). Checks that
 * the right amount of data was transfered.
 *
 * @flags: pass thru from i2400m_bm_cmd()
 * @return: cmd_size if ok, < 0 errno code on error.
 */
static
ssize_t __i2400m_bm_cmd_send(struct i2400m *i2400m,
			     struct i2400m_bootrom_header *cmd, size_t cmd_size,
			     int flags)
{
	ssize_t result = -ENOMEM;
	struct device *dev = &i2400m->usb_iface->dev;
	int opcode = cmd == NULL? -1 : cmd->cmd.command_opcode;

	d_fnstart(6, dev, "(i2400m %p cmd %p size %zu)\n",
		  i2400m, cmd, cmd_size);
	if ((flags & I2400M_BM_CMD_RAW) == 0) {
		cmd->cmd.command_response_required = 1;
		__i2400m_bm_cmd_prepare(cmd);
	}
	result = i2400m_tx_bulk_out(i2400m, cmd, cmd_size);
	if (result < 0) {
		dev_err(dev, "boot-mode cmd %d: cannot send: %d\n",
			opcode, result);
		goto error_cmd_send;
	}
	if (result != cmd_size) {		/* all was transferred? */
		dev_err(dev, "boot-mode cmd %d: incomplete transfer "
			"(%zu vs %zu submitted)\n",  opcode, result, cmd_size);
		result = -EIO;
		goto error_cmd_size;
	}
error_cmd_send:
error_cmd_size:
	d_fnend(6, dev, "(i2400m %p cmd %p size %zu) = %d\n",
		i2400m, cmd, cmd_size, result);
	return result;
}


/*
 * waits for a non zero packet on the notification endpoint
 *
 * @returns: how many bytes were received by the notification handler
 *
 * Resubmits the notification request until an error, a non-zero
 * packet is received or until we timeout in retries.
 */
static ssize_t __i2400m_bm_wait_for_ack(
	struct i2400m *i2400m, struct urb *urb,
	struct i2400m_bootrom_header *ack, size_t ack_size,
	struct completion *completion)
{
	ssize_t result = -ENOMEM;
	struct device *dev = &i2400m->usb_iface->dev;

	d_fnstart(6, dev, "(i2400m %p urb %p ack %p size %zu completion %p)\n",
		  i2400m, urb, ack, ack_size, completion);
	while (1) {
		result = -ETIMEDOUT;
		if (wait_for_completion_interruptible_timeout(
			    completion, HZ) == 0) {
			usb_kill_urb(urb);
			goto error_notif_timeout;
		}
		result = urb->status;			/* How was the ack? */
		switch (result) {
		case 0:
			break;
		case -ECONNRESET:			/* disconnection? */
		case -ENOENT:				/* ditto? */
			result = -ESHUTDOWN;		/* whatever...*/
		case -ESHUTDOWN:			/* URB killed? */
			goto error_notif_urb;
		default:				/* any other? */
#warning FIXME: plug in EDC support for error checking
			goto error_notif_urb;
		}
		if (urb->actual_length > 0)
			break;
		d_printf(6, dev, "ZLP received, retrying\n");
		init_completion(completion);
		result = i2400m_notif_submit(i2400m, urb, ack, ack_size,
					     completion);
		if (result < 0)
			goto error_notif_urb_submit;
	}
	result = urb->actual_length;
error_notif_urb_submit:
error_notif_urb:
error_notif_timeout:
	d_fnend(6, dev, "(i2400m %p urb %p ack %p size %zu completion %p)"
		" = %d\n", i2400m, urb, ack, ack_size, completion, result);
	return result;
}


/*
 * Verify the ack data received and optionally ask for more
 *
 * Will run sanity checks on the received data and make sure that we
 * get all the required data
 *
 * Way too long function -- maybe it should be further split
 */
static
ssize_t __i2400m_bm_ack_verify(struct i2400m *i2400m, int opcode,
			       struct i2400m_bootrom_header *ack,
			       size_t ack_size, size_t ack_len, int flags)
{
	ssize_t result = -ENOMEM;
	struct device *dev = &i2400m->usb_iface->dev;

	d_fnstart(6, dev, "(i2400m %p opcode %d ack %p size %zu len %zd)\n",
		  i2400m, opcode, ack, ack_size, ack_len);
	if (ack_len < sizeof(*ack)) {
		result = -EIO;
		dev_err(dev, "boot-mode cmd %d: HW BUG? notification didn't "
			"return enough data (%zu bytes vs %zu expected)\n",
			opcode, ack_len, sizeof(*ack));
		goto error_ack_short;
	}
	if (ack_len == sizeof(i2400m_REBOOT_BARKER)
		 && memcmp(ack, i2400m_REBOOT_BARKER, sizeof(*ack)) == 0) {
		result = -ERESTARTSYS;
		d_printf(6, dev, "boot-mode cmd %d: HW reboot barker\n",
			 opcode);
		goto error_reboot;
	}
	if (ack_len == sizeof(i2400m_ACK_BARKER)
		 && memcmp(ack, i2400m_ACK_BARKER, sizeof(*ack)) == 0) {
		result = -EISCONN;
		d_printf(3, dev, "boot-mode cmd %d: HW reboot ack barker\n",
			 opcode);
		goto error_reboot_ack;
	}
	result = 0;
	if (flags & I2400M_BM_CMD_RAW)
		goto out_raw;
	ack->cmd.value = le32_to_cpu(ack->cmd.value);
	ack->data_size = le32_to_cpu(ack->data_size);
	ack->target_addr = le32_to_cpu(ack->target_addr);
	ack->block_checksum = le32_to_cpu(ack->block_checksum);
	d_printf(6, dev, "boot-mode cmd %d: notification for opcode %u "
		 "response %u csum %u rr %u da %u signature 0x%x\n",
		 opcode, ack->cmd.command_opcode,
		 ack->cmd.command_response_code, ack->cmd.command_use_checksum,
		 ack->cmd.command_response_required,
		 ack->cmd.command_direct_access, ack->cmd.command_signature);
	result = -EIO;
	if (ack->cmd.command_signature != I2400M_BOOT_COMMAND_SIGNATURE) {
		dev_err(dev, "boot-mode cmd %d: HW BUG? wrong signature 0x%04x\n",
			opcode, ack->cmd.command_signature);
		goto error_ack_signature;
	}
	if (opcode != -1 && opcode != ack->cmd.command_opcode) {
		dev_err(dev, "boot-mode cmd %d: HW BUG? "
			"received response for opcode %u, expected %u\n",
			opcode, ack->cmd.command_opcode, opcode);
		goto error_ack_opcode;
	}
	if (ack->cmd.command_response_code != 0) {	/* failed? */
		dev_err(dev, "boot-mode cmd %d: error; hw response %u\n",
			opcode, ack->cmd.command_response_code);
		goto error_ack_failed;
	}
	if (ack_size < ack->data_size + sizeof(*ack)) {
		dev_err(dev, "boot-mode cmd %d: SW BUG "
			"driver provided only %zu bytes for %u bytes "
			"of data\n", opcode, ack_size,
			ack->data_size + sizeof(*ack));
		goto error_ack_short_buffer;
	}
	while (ack_len < ack_size) {
		int len;
		struct usb_endpoint_descriptor *epd;
		int pipe;

		epd = &i2400m->usb_iface->cur_altsetting
			->endpoint[I2400M_EP_NOTIFICATION].desc;
		pipe = usb_rcvintpipe(i2400m->usb_dev, epd->bEndpointAddress);
		len = 0;
		result = usb_interrupt_msg(i2400m->usb_dev, pipe,
					   (void *) ack + ack_len,
					   ack_size - ack_len, &len, HZ);
		ack_len += len;
		if (result == 0)
			break;
		if (result < 0) {
			dev_err(dev, "boot-mode cmd %d: can't read extra "
				"data: %d\n", opcode, result);
			goto error_ack_failed;
		}
	}
	if (ack_len < ack->data_size + sizeof(*ack)) {
		result = -EIO;
		dev_err(dev, "boot-mode cmd %d: HW BUG "
			"device provided only %zu bytes when asked for %u "
			"bytes of data\n", opcode, ack_size,
			ack->data_size + sizeof(*ack));
		goto error_ack_failed;
	}
	result = ack_len;
	/* Don't you love this stack of empty targets? Well, I don't
	 * either, but it helps track exactly who comes in here and
	 * why :) */
error_ack_short_buffer:
error_ack_failed:
error_ack_opcode:
error_ack_signature:
out_raw:
error_reboot_ack:
error_reboot:
error_ack_short:
	d_fnend(6, dev, "(i2400m %p opcode %d ack %p size %zu len %zd) = %zd\n",
		i2400m, opcode, ack, ack_size, ack_len, result);
	return result;
}


/**
 * i2400m_bm_cmd - Execute a boot mode command
 *
 * @cmd: buffer containing the command data (pointing at the header).
 *       This function will set some of the bits of the command header
 *       (hence that it can't be a const pointer) and then will flip
 *       it to be little endian, so upon return, assume the header is
 *       all messed up.
 *
 *       We assume all has been initialized to zero except for the
 *       command_opcode, command_use_checksum (see FIXME) and
 *       command_direct.
 *
 *       A raw buffer can be also sent, just cast it and set flags to
 *       I2400M_BM_CMD_RAW.
 *
 *       If NULL, no command is sent (we just wait for an ack).
 *
 * @cmd_size: size. Aligned to 16 aligned; pad accordingly.
 *
 * @ack: buffer where to place the acknowledgement. If it is a
 *       regullar command response, all fields will be returned with
 *       the right, native endianess.
 *
 * @ack_size: size of @ack, 16 aligned; you need to provide at least
 *       sizeof(*ack) bytes and then enough to contain the return data
 *       from the command
 *
 * @flags: see I2400M_BM_CMD_* above.
 *
 * @returns: bytes received by the notification; if < 0, an errno code
 *       denoting an error or:
 *
 *       -ERESTARTSYS  Reboot notification received
 *       -EISCONN      Reboot ack notification received
 *
 * Executes a boot-mode command and gets a response, doing basic
 * validation on it; if a zero length response is received, it retries
 * waiting for a response until a non-zero one is received (timing out
 * after @I2400M_BOOT_RETRIES retries).
 */
static
ssize_t i2400m_bm_cmd(struct i2400m *i2400m,
		      struct i2400m_bootrom_header *cmd, size_t cmd_size,
		      struct i2400m_bootrom_header *ack, size_t ack_size,
		      int flags)
{
	ssize_t result = -ENOMEM;
	struct device *dev = &i2400m->usb_iface->dev;
	struct urb notif_urb;
	DECLARE_COMPLETION_ONSTACK(notif_completion);
	int opcode = cmd == NULL? -1 : cmd->cmd.command_opcode;

	d_fnstart(6, dev, "(i2400m %p cmd %p size %zu ack %p size %zu)\n",
		  i2400m, cmd, cmd_size, ack, ack_size);
	BUG_ON(ack_size < sizeof(*ack));
	BUG_ON(ALIGN(ack_size, 16) != ack_size);
	BUG_ON(ALIGN(cmd_size, 16) != cmd_size);
	BUG_ON(i2400m->boot_mode == 0);

	usb_init_urb(&notif_urb);	/* ready notifications */
	usb_get_urb(&notif_urb);
	result = i2400m_notif_submit(i2400m, &notif_urb, ack, ack_size,
				     &notif_completion);
	if (result < 0) {
		dev_err(dev, "boot-mode cmd %d: cannot submit notification "
			"URB: %d\n", opcode, result);
		goto error_notif_urb_submit;
	}
	if (cmd != NULL) {		/* send the command */
		result = __i2400m_bm_cmd_send(i2400m, cmd, cmd_size, flags);
		if (result < 0)
			goto error_cmd_send;
		if ((flags & I2400M_BM_CMD_RAW) == 0)
			d_printf(6, dev, "boot-mode cmd %d: "
				 "addr 0x%04x size %u block csum 0x%04x\n",
				 opcode, cmd->target_addr, cmd->data_size,
				 cmd->block_checksum);
	}
	result = __i2400m_bm_wait_for_ack(i2400m, &notif_urb, ack, ack_size,
					  &notif_completion);
	if (result < 0) {
		dev_err(dev, "boot-mode cmd %d: error waiting for an ack: %d\n",
			opcode, result);
		goto error_wait_for_ack;
	}
	/* verify the ack and read more if neccessary [result is the
	 * final amount of bytes we get in the ack]  */
	result = __i2400m_bm_ack_verify(i2400m, opcode, ack, ack_size,
					notif_urb.actual_length, flags);
	if (result < 0)
		goto error_bad_ack;
	/* Don't you love this stack of empty targets? Well, I don't
	 * either, but it helps track exactly who comes in here and
	 * why :) */
error_bad_ack:
error_wait_for_ack:
error_cmd_send:
	usb_kill_urb(&notif_urb);
error_notif_urb_submit:
	d_fnend(6, dev, "(i2400m %p cmd %p size %zu ack %p size %zu) = %d\n",
		i2400m, cmd, cmd_size, ack, ack_size, result);
	return result;
}


/**
 * i2400m_jump_to_main - instruct the boot rom to pass control to the firmware
 *
 * @i2400m: device descriptor
 * @addr: the address to jump to
 */
static int i2400m_jump_to_main(struct i2400m *i2400m, u32 addr)
{
	int ret = 0;
	struct i2400m_bootrom_header bhdr, reply;
	struct device *dev = &i2400m->usb_iface->dev;

	d_fnstart(5, &i2400m->usb_iface->dev, "addr=%p\n", (void *) addr);
	memset(&bhdr, 0, sizeof(bhdr));
	bhdr.cmd.command_opcode = I2400M_BOC_JUMP;
	bhdr.cmd.command_response_required = 1;
	bhdr.cmd.command_signature = I2400M_BOOT_COMMAND_SIGNATURE;
	bhdr.cmd.value = cpu_to_le32(bhdr.cmd.value);
	bhdr.target_addr = cpu_to_le32(addr);
	ret = i2400m_bm_cmd(i2400m, &bhdr, sizeof(bhdr),
			   &reply, sizeof(reply), I2400M_BM_CMD_RAW);
	if (ret)
		dev_err(dev, "write jump instruction failed: %d\n", ret);
	d_fnend(5, &i2400m->usb_iface->dev, "returning %d\n", ret);
	return ret;
}


/**
 * i2400m_download_chunk - write a single rom chunk to the device memory space
 *
 * @i2400m: device descriptor
 * @buf: the buffer to write
 * @buf_len: length of the buffer to write
 * @addr: address in the device memory space
 * @direct: bootrom write mode
 * @do_csum: should a checksum validation be performed
 *
 * This function writes a chunk of data to the device address space using the
 * boot rom protocol.
 *
 * Well, we do something kind of ugly and is allocate a BIG (more than
 * 16k) chunk of memory; kmalloc seems not to complain too much and
 * speeds up the upload process. For now, we'll keep it. If you want
 * to tweak it down, change I2400M_DIRECT_CHUNK_MAX_LEN.
 */
static int i2400m_download_chunk(struct i2400m *i2400m, const void *chunk,
				 size_t __chunk_len, unsigned long addr,
				 unsigned int direct, unsigned int do_csum)
{
	int ret;
	size_t chunk_len = ALIGN(__chunk_len, I2400M_PKT_PAD);
	struct device *dev = &i2400m->usb_iface->dev;
	struct {
		struct i2400m_bootrom_header cmd;
		u8 cmd_payload[chunk_len];
		struct i2400m_bootrom_header ack;
	} __attribute__((packed)) *buf;

	d_fnstart(5, &i2400m->usb_iface->dev, "len=%d, addr=0x%8.8lx\n",
		  chunk_len, addr);
	buf = kzalloc(sizeof(*buf), GFP_KERNEL);
	if (buf == NULL) {
		ret = -ENOMEM;
		dev_err(dev, "cannot allocate command/ack buffer: %d\n", ret);
		goto error_buf_alloc;
	}
	memcpy(buf->cmd_payload, chunk, chunk_len);
	buf->cmd.cmd.command_opcode = I2400M_BOC_WRITE;
	buf->cmd.cmd.command_response_required = 1;
	/* no checksum if the length is not divisable by 4 */
	buf->cmd.cmd.command_use_checksum = __chunk_len & 0x3? 0 : do_csum;
	/* indirect if the length is not divisable by 16 */
	buf->cmd.cmd.command_direct_access = __chunk_len & 0xf? 0 : direct;
	buf->cmd.target_addr = addr;
	buf->cmd.data_size = __chunk_len;
	ret = i2400m_bm_cmd(i2400m, &buf->cmd, sizeof(buf->cmd) + chunk_len,
			    &buf->ack, sizeof(buf->ack), 0);
	if (ret >= 0)
		ret = 0;
	kfree(buf);
error_buf_alloc:
	d_fnend(5, &i2400m->usb_iface->dev, "returning %d\n", ret);
	return ret;
}


/**
 * i2400m_download_buffer - write a continous buffer to the device memory space
 *
 * @i2400m: device descriptor
 * @buf: the buffer to write
 * @buf_len: length of the buffer to write
 * @addr: address in the device memory space
 * @direct: bootrom write mode
 * @do_csum: should a checksum validation be performed
 *
 * this function writes a buffer to the device address space using the boot rom
 * protocol. It writes the buffer in chunks of 0x4000 bytes including a boot rom
 * header, and payload.
 * if the payload size is not a multiple of 16, direct mode cannot be used, and
 * the chunk is splitted to two parts, one to be written directly and the other,
 * indirectly. If the payload size is not a multiple of 4, checksum cannot be
 * performed on the buffer, and validation is skipped.
 */
static int i2400m_download_buffer(struct i2400m *i2400m, const void *buf,
				  size_t buf_len, unsigned long addr,
				  unsigned int direct, unsigned int do_csum)
{
	int ret = 0, remaining = buf_len;
	int chunk_len, cur_len;
	const void *payload = buf;

	d_fnstart(5, &i2400m->usb_iface->dev, "len=%d, addr=0x%8.8lx\n",
		  buf_len, addr);
	chunk_len = (direct != 0 ? I2400M_DIRECT_CHUNK_MAX_LEN :
		     I2400M_INDIRECT_CHUNK_MAX_LEN) -
		sizeof(struct i2400m_bootrom_header);
	while (remaining > 0) {
		cur_len = remaining > chunk_len ? chunk_len : remaining;
		/*
		 * handle the last chunk case: if it is not aligned to 16 bytes,
		 * we need to split the last chunk to something aligned to send
		 * in direct (if in a direct mode), and send the last remainder
		 * indirectly.
		 */
		if (cur_len & 0xf && cur_len > I2400M_INDIRECT_CHUNK_MAX_LEN) {
			/*
			 * split it in order to send one part
			 * directly and the other indirectly
			 */
			cur_len &= 0x0000ff00;
		}
		ret = i2400m_download_chunk(i2400m, payload, cur_len,
					    addr + buf_len - remaining,
					    direct, do_csum);
		if (ret != 0)	/* Caller will complain */
			goto error_download_chunk;
		remaining -= cur_len;
		payload += cur_len;
	}
error_download_chunk:
	d_fnend(5, &i2400m->usb_iface->dev, "returning %d\n", ret);
	return ret;
}


/*
 * Execute firmware-file specific post-download actins
 */
static int i2400m_download_post_action(struct i2400m *i2400m, u32 addr, int act)
{
	int ret = 0;

	switch (act) {
	case I2400M_PDO_NOTIFY:	/* device related voodoo */
		ret = i2400m_download_buffer(i2400m, (const char *) &addr,
					     sizeof(addr),
					     I2400M_MSG_TO_1_REGISTER, 0, 0);
		break;
	case I2400M_PDO_JMP:
		ret = i2400m_jump_to_main(i2400m, le32_to_cpu(addr));
		break;
	default:
		break;
	}
	return ret;
}


/**
 * i2400m_download_file - download a single elf file to the device
 *
 * @i2400m: device descriptor
 * @fw_data:  contains the elf image and associated metadata
 *
 * downloads every segment of the elf image to it's proper
 * location in the device's address space
 */
static int i2400m_download_file(struct i2400m *i2400m,
				struct i2400m_fw_file_data *fw_data)
{
	struct device *dev = &i2400m->usb_iface->dev;
	int i, ret = 0;

	/* parse and cast the elf structures */
	Elf32le_Ehdr *lehdr;
	Elf32le_Phdr *lephdr;
	Elf32_Ehdr hdr;
	Elf32_Phdr phdr;

	d_fnstart(3, &i2400m->usb_iface->dev, "(i2400m %p file %s)\n",
		  i2400m, fw_data->name);
	lehdr = (Elf32le_Ehdr *) fw_data->fw_entry->data;
	elf32_ehdr_le_to_cpu(lehdr, &hdr);
	/* sanity */
	if (hdr.e_phoff + (hdr.e_phnum * sizeof(phdr)) !=
	    fw_data->fw_entry->size) {
		ret = -EINVAL;
		dev_err(dev, "fw file %s corrupted "
			"(computed size %zu vs %zu)\n", fw_data->name,
			hdr.e_phoff + (hdr.e_phnum * sizeof(phdr)),
			fw_data->fw_entry->size);
		goto error_fw_file;
	}
	/* write each program segment to its in-mem address on the device  */
	for (i = 0; i < hdr.e_phnum; i++) {
		lephdr = (Elf32le_Phdr *) (fw_data->fw_entry->data +
					   hdr.e_phoff + i * hdr.e_phentsize);
		elf32_phdr_le_to_cpu(lephdr, &phdr);
		if (phdr.p_paddr >= I2400M_MAX_LOADABLE_ADDR) {
			dev_err(dev, "fw file %s, section #%d: "
				"load address 0x%08x exceeds max load address"
				" 0x%08x - ignoring section\n",
				fw_data->name, i, phdr.p_paddr,
				I2400M_MAX_LOADABLE_ADDR);
			continue;
		}
		ret = i2400m_download_buffer(i2400m, fw_data->fw_entry->data +
					     phdr.p_offset, phdr.p_filesz,
					     phdr.p_paddr,
					     fw_data->direct_access,
					     fw_data->use_chksum);
		if (ret != 0) {
			dev_err(dev, "fw file %s, section #%d: "
				"download failed: %d\n", fw_data->name, i, ret);
			goto error_download_buffer;
		}
	}
	ret = i2400m_download_post_action(i2400m, lehdr->e_entry,
					  fw_data->post_action);
	if (ret != 0)
		dev_err(dev, "fw file %s: post action %d failed: %d\n",
			fw_data->name, i2400m_fw_data[i].post_action, ret);
error_download_buffer:
error_fw_file:
	d_fnend(3, &i2400m->usb_iface->dev, "(i2400m %p file %s) = %d\n",
		i2400m, fw_data->name, ret);
	return ret;
}


/* Flags for i2400m_bootrom_init */
enum {
	I2400M_BRI_SOFT       = 1 << 1,
	I2400M_BRI_NO_REBOOT  = 1 << 2,
};


/**
 * i2400m_bootrom_init - Reboots a powered device into boot mode
 *
 * @i2400m: device descriptor
 * @flags:
 *      I2400M_BRI_SOFT: a reboot notification has been seen
 *                       already, so don't wait for it.
 *      I2400M_BRI_NO_REBOOT: Don't send a reboot command, but wait
 *                            for a reboot barker notification. This
 *                            is a one shot; if the state machine
 *                            needs to send a reboot command it will.
 *
 * Tries hard enough to put the device in boot-mode. There are two
 * main phases to this:
 *
 * a. (1) send a reboot command and (2) get a reboot barker
 * b. (1) ack the reboot sending a reboot barker and (2) getting an
 *        ack barker in return
 *
 * We want to skip (a) in some cases [soft]. The state machine is
 * horrible, but it is basically: on each phase, send what has to be
 * sent (if any), wait for the answer and act on the answer. We might
 * have to backtrack and retry, so we keep a max tries counter for
 * that.
 *
 * If we get a timeout after sending a soft reset, we try a boot-mode
 * reset, as it might happen that the device is already in boot
 * mode. Then that re-boots the device again and we are game.
 */
static int i2400m_bootrom_init(struct i2400m *i2400m, int flags)
{
	int result;
	struct device *dev = &i2400m->usb_iface->dev;
	struct
	{
		struct i2400m_bootrom_header cmd;
		struct i2400m_bootrom_header ack;
	} __attribute__((packed)) *buf;
	int count = I2400M_BOOT_RETRIES;

	BUILD_BUG_ON(sizeof(buf->cmd) != sizeof(i2400m_REBOOT_BARKER));
	BUILD_BUG_ON(sizeof(buf->ack) != sizeof(i2400m_ACK_BARKER));

	d_fnstart(4, dev, "(i2400m %p)\n", i2400m);
	result = -ENOMEM;
	buf = kzalloc(sizeof(*buf), GFP_KERNEL);
	if (buf == NULL)
		goto error_buf_alloc;
	if (flags & I2400M_BRI_SOFT)
		goto do_reboot_ack;
do_reboot:
	if (--count < 0)
		goto error_timeout;
	d_printf(4, dev, "device reboot: reboot command [%d # left]\n",
		 count);
	if ((flags & I2400M_BRI_NO_REBOOT) == 0)
		__i2400m_reset_soft(i2400m);
	result = i2400m_bm_cmd(i2400m, NULL, 0,
			       &buf->ack, sizeof(buf->ack),
			       I2400M_BM_CMD_RAW);
	flags &= ~I2400M_BRI_NO_REBOOT;
	switch (result) {
	case -ERESTARTSYS:
		d_printf(4, dev, "device reboot: got reboot barker\n");
		break;
	case -EISCONN:	/* we don't know how it got here...but we follow it */
		d_printf(4, dev, "device reboot: got ack barker - whatever\n");
		goto do_reboot;
	case -ETIMEDOUT:	/* device has timed out, we might be in boot
				 * mode already, do aboot-mode reboot */
		dev_info(dev, "soft reset timed out, "
			 "trying a boot mode reboot\n");
		memcpy(&buf->cmd, i2400m_REBOOT_BARKER, sizeof(buf->cmd));
		result = i2400m_bm_cmd(i2400m, &buf->cmd, sizeof(buf->cmd),
				       &buf->ack, sizeof(buf->ack),
				       I2400M_BM_CMD_RAW);
		goto do_reboot;
	case -EPROTO:
	case -ESHUTDOWN:	/* dev is gone */
		goto error_dev_gone;
	default:
		dev_err(dev, "device reboot: error %d while waiting "
			"for reboot barker - rebooting\n", result);
		goto do_reboot;
	}
	/* At this point we ack back with 4 REBOOT barkers and expect
	 * 4 ACK barkers. This is ugly, as we send a raw command --
	 * hence the cast. _bm_cmd() will catch the reboot ack
	 * notification and report it as -EISCONN. */
do_reboot_ack:
	d_printf(4, dev, "device reboot ack: sending ack [%d # left]\n",
		 count);
	memcpy(&buf->cmd, i2400m_REBOOT_BARKER, sizeof(i2400m_REBOOT_BARKER));
	result = i2400m_bm_cmd(i2400m, &buf->cmd, sizeof(buf->cmd),
			       &buf->ack, sizeof(buf->ack), I2400M_BM_CMD_RAW);
	switch (result) {
	case -ERESTARTSYS:
		d_printf(4, dev, "reboot ack: got reboot barker - retrying\n");
		if (--count < 0)
			goto error_timeout;
		goto do_reboot_ack;
	case -EISCONN:
		d_printf(4, dev, "reboot ack: got ack barker - good\n");
		break;
	case -EPROTO:
	case -ESHUTDOWN:	/* dev is gone */
		goto error_dev_gone;
	default:
		dev_err(dev, "device reboot ack: error %d while waiting for "
			"reboot ack barker - rebooting\n", result);
		goto do_reboot;
	}
	d_printf(2, dev, "device reboot ack: got ack barker - boot done\n");
	result = 0;
exit_timeout:
error_dev_gone:
	kfree(buf);
error_buf_alloc:
	d_fnend(4, dev, "(i2400m %p) = %d\n", i2400m, result);
	return result;

error_timeout:
	dev_err(dev, "Timed out waiting for reboot ack\n");
	result = -ETIMEDOUT;
	goto exit_timeout;
}


/**
 * i2400m_dev_bootstrap - brings up the device to an operational state
 *
 * @i2400m: device descriptor
 *
 * can be called either from probe, or after a warm reset.
 * can not be called from within an interrupt.
 * all usb operations during this process are synchronous.
 * except listening on the interrupt endpoint and flagging reset if detected.
 */
int i2400m_dev_bootstrap(struct i2400m *i2400m)
{
	int i, ret = 0;
	struct device *dev = &i2400m->usb_iface->dev;
	int count = I2400M_BOOT_RETRIES;

	d_fnstart(5, dev, "(i2400m %p)\n", i2400m);
	WARN_ON(i2400m->boot_mode != 1);
	/* Load firmware files to memory. */
	for (i = 0; i < I2400M_FW_NFILES; i++) {
		ret = request_firmware((const struct firmware **)
				       &i2400m_fw_data[i].fw_entry,
				       i2400m_fw_data[i].name,
				       &i2400m->usb_iface->dev);
		if (ret) {
			dev_err(dev, "fw file %s: request failed: %d\n",
				i2400m_fw_data[i].name, ret);
			goto error_fw_req;
		}
	}
	/* reset the device and get a reset barker from it  */
hw_reboot:
	if (count-- == 0) {
		ret = -ERESTARTSYS;
		dev_err(dev, "device rebooted too many times, aborting\n");
		goto error_too_many_reboots;
	}
	/* Init again (known clean state). */
	ret = i2400m_bootrom_init(i2400m, I2400M_BRI_NO_REBOOT);
	if (ret) {
		dev_err(dev, "bootrom init failed: %d\n", ret);
		goto error_init_bootrom;
	}
	/* download firmware files */
	for (i = 0; i < I2400M_FW_NFILES; i++) {
		ret = i2400m_download_file(i2400m, &i2400m_fw_data[i]);
		if (ret == -ERESTARTSYS) {
			dev_err(dev, "fw file %s: device rebooted, "
				"%d tries left\n", i2400m_fw_data[i].name,
				count);
			goto hw_reboot;
		}
		if (ret < 0) {
			dev_err(dev, "fw file %s: download failed: %d\n",
				i2400m_fw_data[i].name, ret);
			goto error_file_download;
		}
		d_printf(2, dev, "fw file %s successfully uploaded\n",
			 i2400m_fw_data[i].name);
	}
	d_printf(1, dev, "firmware successfully uploaded\n");
	i2400m->boot_mode = 0;
error_file_download:
error_too_many_reboots:
error_init_bootrom:
error_fw_req:
	for (i = 0; i < I2400M_FW_NFILES; i++) {
		if (i2400m_fw_data[i].fw_entry) {
			release_firmware(i2400m_fw_data[i].fw_entry);
			i2400m_fw_data[i].fw_entry = NULL;
		}
	}
	d_fnend(5, dev, "(i2400m %p) = %d\n", i2400m, ret);
	return ret;
}
