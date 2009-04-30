/* 
 * common.h
 * Copyright (C) 2007, AVM GmbH. All rights reserved.
 * 
 * This Software is  free software. You can redistribute and/or
 * modify such free software under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * The free software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this Software; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA, or see
 * http://www.opensource.org/licenses/lgpl-license.html
 * 
 * Contact: AVM GmbH, Alt-Moabit 95, 10559 Berlin, Germany, email: info@avm.de
 */

#ifndef __have_common_h__
#define __have_common_h__

#include "attr.h"

/*---------------------------------------------------------------------------*\
 * Changes in common.h require recompilation of binary part!
\*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
typedef struct {

	unsigned int (__attr2 *caller_completion_routine) 
		(void*, void*, void*);
	void *param1;
	void *param2;
	void *param3;
	void *do_not_touch;
	
} usb_caller_extension_t, *usb_caller_extension_p;

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
#define IOCTL_SET_CHANNEL	0x0001
#define IOCTL_GET_CHANNEL	0x0002
#define IOCTL_SET_MODE 		0x0003
#define IOCTL_GET_MODE 		0x0004
#define IOCTL_SET_SCAN 		0x0005
#define IOCTL_GET_SCAN 		0x0006
#define IOCTL_SET_SSID 		0x0007
#define IOCTL_GET_SSID 		0x0008
#define IOCTL_SET_BSSID 	0x0009
#define IOCTL_GET_BSSID 	0x000A
#define IOCTL_SET_AUTH		0x000B
#define IOCTL_GET_AUTH		0x000C
#define IOCTL_SET_ENC		0x000D
#define IOCTL_GET_ENC		0x000E
#define IOCTL_SET_KEY_MGMT	0x000F
#define IOCTL_GET_KEY_MGMT	0x0010
#define IOCTL_SET_WPA_VERSION	0x0011
#define IOCTL_GET_WPA_VERSION	0x0012
#define IOCTL_SET_WEP_KEY	0x0013
#define IOCTL_GET_WEP_KEY	0x0014
#define IOCTL_SET_KEY		0x0015
#define IOCTL_GET_KEY		0x0016

#define IOCTL_GET_HW_MAC 	0x0017
#define IOCTL_GET_RSSI		0x0018
#define IOCTL_DISASSOC 		0x0019
#define IOCTL_DEAUTH 		0x001A
#define IOCTL_SET_G_PLUS_PLUS	0x001B
#define IOCTL_GET_G_PLUS_PLUS	0x001C
#define IOCTL_SET_DBG_SEVERITY	0x001D
#define IOCTL_GET_DBG_SEVERITY	0x001E
#define IOCTL_SET_DBG_MODULE	0x001F
#define IOCTL_GET_DBG_MODULE	0x0020

#define IOCTL_OK		0
#define IOCTL_ERROR 		1
#define IOCTL_ERROR_NO_MEM 	2

#define IOCTL_MODE_ADHOC	0
#define IOCTL_MODE_INFRA	1
#define IOCTL_MODE_AUTO		2

#define IOCTL_AUTH_OPEN		0
#define IOCTL_AUTH_SHARED	1

#define IOCTL_ENC_NONE		0
#define IOCTL_ENC_WEP40		1
#define IOCTL_ENC_WEP104	2
#define IOCTL_ENC_TKIP		3
#define IOCTL_ENC_CCMP		4

#define IOCTL_WPA_VERSION_WPA	0
#define IOCTL_WPA_VERSION_WPA2	1

#define IOCTL_KEY_MGMT_802_1X	0
#define IOCTL_KEY_MGMT_PSK	1

typedef struct {
	char	address[6];
} ioctl_mac_t;

struct ioctl_std {
	int 	value;
};

typedef struct ioctl_std ioctl_channel_t;

typedef struct ioctl_std ioctl_mode_t;

typedef struct ioctl_std ioctl_reason_t;

typedef struct {
	int	length;
	char	name[32];
} ioctl_ssid_t;

typedef struct {
	int 	length;
	char	data[512];
} ioctl_ie_t;

typedef struct {
	ioctl_mac_t 	mac;
	ioctl_ssid_t	ssid;
	ioctl_ie_t	ies;
} site_t, *site_p;

typedef struct {
	int	nr;
	site_p 	site;
} ioctl_scan_t, *ioctl_scan_p;

typedef struct ioctl_std ioctl_auth_t;

typedef struct ioctl_std ioctl_enc_t;

typedef struct ioctl_std ioctl_wpa_version_t;

typedef struct ioctl_std ioctl_key_mgmt_t;

typedef struct {
	int	 	def;
	int 		index;
	int		length;
	unsigned char	key[32];
} ioctl_wep_key_t, *ioctl_wep_key_p;

typedef struct {
	int		l;
	int		index;
	int		length;
	ioctl_mac_t	mac;
	long long	rsc;
	unsigned char	key[32];
} ioctl_key_t, *ioctl_key_p;

typedef struct ioctl_std ioctl_rssi_t;

typedef struct ioctl_std ioctl_g_plus_plus_t;

typedef struct ioctl_std ioctl_dbg_sev_t;

typedef struct ioctl_std ioctl_dbg_mod_t;

typedef struct {
	char type;
	union {
		ioctl_channel_t 	channel;
		ioctl_mode_t 		mode;
		ioctl_scan_t 		scan;
		ioctl_ssid_t 		ssid;
		ioctl_auth_t 		auth;
		ioctl_enc_t  		enc;
		ioctl_wep_key_t  	wep_key;
		ioctl_key_t  		key;
		ioctl_mac_t  		mac;
		ioctl_rssi_t		rssi;
		ioctl_reason_t		reason;
		ioctl_wpa_version_t 	wpa_version;
		ioctl_key_mgmt_t	key_mgmt;
		ioctl_g_plus_plus_t	g_plus_plus;
		ioctl_dbg_sev_t		dbg_sev;
		ioctl_dbg_mod_t		dbg_mod;
	} data;
} fwlanusb_ioctl_t, *fwlanusb_ioctl_p;

#define EVENT_ASSOCIATED	0
#define EVENT_DISASSOCIATED	1

typedef unsigned int fwlanusb_event_t;

#endif /*__have_common_h*/

