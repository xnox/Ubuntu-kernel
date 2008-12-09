/*
 *
 * Intel Wireless WiMax Connection 2400m
 * User space message interface
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
 * THE PROTOCOL
 *
 * Each message is composed of:
 *
 * HEADER
 * [TLV0 + PAYLOAD0]
 * [TLV1 + PAYLOAD1]
 * [...]
 * [TLVN + PAYLOADN]
 *
 * The HEADER is defined by 'struct i2400m_l3l4_hdr'. The payloads are
 * defined by a TLV structure (Type Length Value) which is a 'header'
 * (struct i2400m_tlv_hdr) and then the payload.
 *
 * All integers are represented as Little Endian (reason being so far
 * seems like most deployments and devices we know of use Little
 * Endian).
 *
 *
 * REQUESTS AND EVENTS
 *
 * The requests can be clasified as follows:
 *
 * - COMMAND:  implies a request from user space to the kernel
 *             requesting an action being performed. The kernel will
 *             send an event (with the same type as the command),
 *             status and no (TLV) payload. It might cause other EVENTS
 *             to be sent later on in time (not with the same type).
 *
 * - GET/SET:  similar to COMMAND, but the reply EVENT is guaranteed to
 *             happen inmediately and will not cause other EVENTs
 *
 * - EVENT:    asynchronous messages sent from the kernel, maybe as a
 *             consequence of previous COMMANDs but disassociated from
 *             them.
 *
 * Only one request might be pending at the same time (ie: don't
 * parallelize nor post another GET request before the previous
 * COMMAND has been acknowledged with it's corresponding EVENT by the
 * kernel).
 *
 * The different requests and their formats are described below; the
 * data types are named 'struct i2400m_msg_OPNAME', OPNAME matching the
 * operation.
 */

#ifndef __NET__WIMAX_I2400M_H__
#define __NET__WIMAX_I2400M_H__

#include <linux/types.h>

/* Interface version */
enum {
	I2400M_L3L4_VERSION             = 0x0100,
};

/* Message types */
enum {
	I2400M_MT_RESERVED              = 0x0000,
	I2400M_MT_INVALID               = 0xffff,
	I2400M_MT_REPORT_MASK		= 0x8000,

	I2400M_MT_GET_SCAN_RESULT  	= 0x4202,
	I2400M_MT_SET_SCAN_PARAM   	= 0x4402,
	I2400M_MT_CMD_RF_CONTROL   	= 0x4602,
	I2400M_MT_CMD_SCAN         	= 0x4603,
	I2400M_MT_CMD_CONNECT      	= 0x4604,
	I2400M_MT_CMD_DISCONNECT   	= 0x4605,
	I2400M_MT_GET_LM_VERSION   	= 0x5201,
	I2400M_MT_GET_DEVICE_INFO  	= 0x5202,
	I2400M_MT_GET_LINK_STATUS  	= 0x5203,
	I2400M_MT_GET_STATISTICS   	= 0x5204,
	I2400M_MT_GET_STATE        	= 0x5205,
	I2400M_MT_GET_MEDIA_STATUS	= 0x5206,
	I2400M_MT_SET_INIT_CONFIG	= 0x5404,
	I2400M_MT_CMD_INIT	        = 0x5601,
	I2400M_MT_CMD_TERMINATE		= 0x5602,
	I2400M_MT_CMD_MODE_OF_OPERATION = 0x5603,
	I2400M_MT_CMD_RESET_DEVICE	= 0x5604,
	I2400M_MT_CMD_MONITOR_CONTROL   = 0x5605,
	I2400M_MT_CMD_ENTER_POWERSAVE   = 0x5606,
	I2400M_MT_SET_EAP_SUCCESS       = 0x6402,
	I2400M_MT_SET_EAP_FAIL          = 0x6403,
	I2400M_MT_SET_EAP_KEY          	= 0x6404,
	I2400M_MT_CMD_SEND_EAP_RESPONSE = 0x6602,
	I2400M_MT_REPORT_SCAN_RESULT    = 0xc002,
	I2400M_MT_REPORT_STATE		= 0xd002,
	I2400M_MT_REPORT_POWERSAVE_READY = 0xd005,
	I2400M_MT_REPORT_EAP_REQUEST    = 0xe002,
	I2400M_MT_REPORT_EAP_RESTART    = 0xe003,
	I2400M_MT_REPORT_ALT_ACCEPT    	= 0xe004,
	I2400M_MT_REPORT_KEY_REQUEST 	= 0xe005,
};


/*
 * Message Ack Status codes
 *
 * When a message is replied-to, this status is reported.
 */
enum i2400m_ms {
	I2400M_MS_DONE_OK                  = 0,
	I2400M_MS_DONE_IN_PROGRESS         = 1,
	I2400M_MS_INVALID_OP               = 2,
	I2400M_MS_BAD_STATE                = 3,
	I2400M_MS_ILLEGAL_VALUE            = 4,
	I2400M_MS_MISSING_PARAMS           = 5,
	I2400M_MS_VERSION_ERROR            = 6,
	I2400M_MS_ACCESSIBILITY_ERROR      = 7,
	I2400M_MS_BUSY                     = 8,
	I2400M_MS_CORRUPTED_TLV            = 9,
	I2400M_MS_UNINITIALIZED            = 10,
	I2400M_MS_UNKNOWN_ERROR            = 11,
	I2400M_MS_PRODUCTION_ERROR         = 12,
	I2400M_MS_NO_RF                    = 13,
	I2400M_MS_NOT_READY_FOR_POWERSAVE  = 14,
	I2400M_MS_THERMAL_CRITICAL         = 15,
	I2400M_MS_MAX
};


/**
 * i2400m_tlv - enumeration of the different types of TLVs
 *
 * TLVs stand for type-length-value and are the header for a payload
 * composed of almost anything. Each payload has a type assigned
 * and a length.
 */
enum i2400m_tlv {
	I2400M_TLV_SYSTEM_STATE = 141,
	I2400M_TLV_RF_OPERATION = 162,
	I2400M_TLV_RF_STATUS = 163,
	I2400M_TLV_CONFIG_IDLE_PARAMETERS = 601,
};


struct i2400m_tlv_hdr {
	__le16 type;
	__le16 length;		/* payload's */
	__u8   pl[0];
} __attribute__((packed));


struct i2400m_l3l4_hdr {
	__le16 type;
	__le16 length;		/* payload's */
	__le16 version;
	__le16 resv1;
	__le16 status;
	__le16 resv2;
	struct i2400m_tlv_hdr pl[0];
} __attribute__((packed));


/**
 * i2400m_system_state - different states of the device
 */
enum i2400m_system_state {
	I2400M_SYSTEM_STATE_UNINITIALIZED = 1,
	I2400M_SYSTEM_STATE_INIT,
	I2400M_SYSTEM_STATE_READY,
	I2400M_SYSTEM_STATE_SCAN,
	I2400M_SYSTEM_STATE_STANDBY,
	I2400M_SYSTEM_STATE_CONNECTING,
	I2400M_SYSTEM_STATE_WIMAX_CONNECTED,
	I2400M_SYSTEM_STATE_DATA_PATH_CONNECTED,
	I2400M_SYSTEM_STATE_IDLE,
	I2400M_SYSTEM_STATE_DISCONNECTING,
	I2400M_SYSTEM_STATE_OUT_OF_ZONE,
	I2400M_SYSTEM_STATE_SLEEPACTIVE,
	I2400M_SYSTEM_STATE_PRODUCTION,
	I2400M_SYSTEM_STATE_CONFIG,
	I2400M_SYSTEM_STATE_RF_OFF,
	I2400M_SYSTEM_STATE_RF_SHUTDOWN,
	I2400M_SYSTEM_STATE_DEVICE_DISCONNECT,
	I2400M_SYSTEM_STATE_MAX,
};


/**
 * i2400m_tlv_system_state - report on the state of the system
 *
 * @state: see enum i2400m_system_state
 */
struct i2400m_tlv_system_state {
	struct i2400m_tlv_hdr hdr;
	__le32 state;
} __attribute__((packed));


enum {
	I2400M_TLV_L4_MESSAGE_VERSIONS = 129
};

struct i2400m_tlv_l4_message_versions {
	struct i2400m_tlv_hdr hdr;
	__le16 major;
	__le16 minor;
	__le16 branch;
	__le16 reserved;
} __attribute__((packed));


struct i2400m_tlv_detailed_device_info {
	struct i2400m_tlv_hdr hdr;
	__u8 reserved1[400];
	__u8 mac_address[6];
	__u8 reserved2[2];
} __attribute__((packed));


struct i2400m_tlv_rf_switches_status {
	struct i2400m_tlv_hdr hdr;
	__u8 sw_rf_switch;	/* 1 ON, 2 OFF */
	__u8 hw_rf_switch;	/* 1 ON, 2 OFF */
} __attribute__((packed));


struct i2400m_tlv_rf_operation {
	struct i2400m_tlv_hdr hdr;
	__le32 status;	/* 1 ON, 2 OFF */
} __attribute__((packed));



struct i2400m_tlv_config_idle_parameters {
	struct i2400m_tlv_hdr hdr;
	__le32 idle_timeout;	/* 100 to 300000 ms [5min], 100 increments
				 * 0 disabled */
	__le32 idle_paging_interval;	/* frames */
} __attribute__((packed));

#endif /* #ifndef __NET__WIMAX_I2400M_H__ */
