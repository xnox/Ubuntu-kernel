/* 
 * wext.c
 * Copyright (C) 2005, AVM GmbH. All rights reserved.
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

#include <linux/netdevice.h>
#include <linux/if_arp.h>
#include <linux/wireless.h>
#include <net/iw_handler.h>
#include "wext.h"
#include "driver.h"
#include "tools.h"
#include "lib.h"
#include "libdefs.h"
#include "common.h"

struct iw_statistics wstats = { 0 };
static unsigned int frequencies[] = { 	2412, 2417, 2422, 2427, 2432, 2437, 2442, 
					2447, 2452, 2457, 2462, 2467, 2472, 2484 };

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static struct iw_statistics *fwlanusb_get_wireless_stats (
		struct net_device * dev){

	card_p 			pdc = (card_p)dev->priv;
	fwlanusb_ioctl_t 	ioctl = { IOCTL_GET_RSSI };
	
	wstats.qual.updated = 	IW_QUAL_QUAL_INVALID | IW_QUAL_LEVEL_UPDATED | 
				IW_QUAL_NOISE_INVALID;

	(*wlan_lib->do_ioctl) (pdc, &ioctl);

	wstats.qual.level = ioctl.data.rssi.value;

	return &wstats;
}

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static int fwlanusb_commit (
		struct net_device *dev, struct iw_request_info *info,
		union iwreq_data *wrqu, char *extra) {

	return 0;
}

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static int fwlanusb_get_name (
		struct net_device *dev, struct iw_request_info *info,
		union iwreq_data *wrqu, char *extra) {
	
	snprintf(wrqu->name, IFNAMSIZ, "IEEE 802.11b/g");
	return 0;
}

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static int fwlanusb_set_freq(
		struct net_device *dev, struct iw_request_info *info,
		union iwreq_data *wrqu, char *extra) {
	
	card_p 			pdc = (card_p)dev->priv;
	fwlanusb_ioctl_t 	ioctl = { IOCTL_SET_CHANNEL };
	int			i;

	if (wrqu->freq.m < 1000) {
	    	if ((wrqu->freq.m > 14) || (wrqu->freq.m < 0) || (wrqu->freq.e))
			return -EINVAL;
		ioctl.data.channel.value = wrqu->freq.m;
	} else {
		if (wrqu->freq.e != 1)
			return -EOPNOTSUPP;
		if ((wrqu->freq.m > 248400000) || (wrqu->freq.m < 241200000))
			return -EINVAL;
		for (i=0; i<14; i++) {
			if (frequencies[i] == (wrqu->freq.m / 100000))
				ioctl.data.channel.value = i + 1; 
		}
	}
				
	if ((*wlan_lib->do_ioctl) (pdc, &ioctl) == IOCTL_ERROR) {
		return -EIO;
	}
	
	return 0;
}

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static int fwlanusb_get_freq(
		struct net_device *dev, struct iw_request_info *info,
		union iwreq_data *wrqu, char *extra) {

	card_p 			pdc = (card_p)dev->priv;
	fwlanusb_ioctl_t 	ioctl = { IOCTL_GET_CHANNEL };

	if ((*wlan_lib->do_ioctl) (pdc, &ioctl) == IOCTL_ERROR) {
		return -EIO;
	}

	wrqu->freq.m = ioctl.data.channel.value;
	wrqu->freq.e = 0;
	
	return 0;
}

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static int fwlanusb_set_mode(
		struct net_device *dev, struct iw_request_info *info,
		union iwreq_data *wrqu, char *extra) {

	card_p 			pdc = (card_p)dev->priv;
	fwlanusb_ioctl_t 	ioctl = { IOCTL_SET_MODE };

	if ((wrqu->mode < IW_MODE_AUTO) || (wrqu->mode > IW_MODE_MONITOR))
		return -EINVAL;

	switch (wrqu->mode) {
		case IW_MODE_ADHOC:
			ioctl.data.mode.value = IOCTL_MODE_ADHOC;
			break;
		case IW_MODE_INFRA:
			ioctl.data.mode.value = IOCTL_MODE_INFRA;
			break;
		case IW_MODE_AUTO:
			ioctl.data.mode.value = IOCTL_MODE_AUTO;
			break;
		default:
			return -EOPNOTSUPP;
	}	

	if ((*wlan_lib->do_ioctl) (pdc, &ioctl) == IOCTL_ERROR) {
		return -EIO;
	}
	
	return 0;
}

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static int fwlanusb_get_mode(
		struct net_device *dev, struct iw_request_info *info,
		union iwreq_data *wrqu, char *extra) {

	card_p 			pdc = (card_p)dev->priv;
	fwlanusb_ioctl_t 	ioctl = { IOCTL_GET_MODE };
	
	/*FIXME: Why does ad-hoc mode not work*/
	
	if ((*wlan_lib->do_ioctl) (pdc, &ioctl) == IOCTL_ERROR) {
		return -EIO;
	}

	switch (ioctl.data.mode.value) {
		case IOCTL_MODE_ADHOC:
			wrqu->mode = IW_MODE_ADHOC;
			break;
		case IOCTL_MODE_INFRA:
			wrqu->mode = IW_MODE_INFRA;
			break;
		case IOCTL_MODE_AUTO:
			wrqu->mode = IW_MODE_AUTO;
			break;
		default:
			return -EIO;
	}	

	return 0;
}

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static int fwlanusb_get_range(
		struct net_device *dev, struct iw_request_info *info,
		union iwreq_data *wrqu, char *extra) {
	
	struct iw_range *range = (struct iw_range *) extra;

	wrqu->data.length = sizeof(struct iw_range);
	memset(range, 0, sizeof(struct iw_range));

	range->we_version_compiled = WIRELESS_EXT;
	range->we_version_source = 9;

	range->enc_capa = IW_ENC_CAPA_WPA | IW_ENC_CAPA_WPA2 | 
		IW_ENC_CAPA_CIPHER_TKIP | IW_ENC_CAPA_CIPHER_CCMP;

	IW_EVENT_CAPA_SET_KERNEL(range->event_capa);
	IW_EVENT_CAPA_SET(range->event_capa, SIOCGIWAP);

	/*FIXME: Finish this*/

	return 0;
}

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static int fwlanusb_set_ap(
		struct net_device *dev, struct iw_request_info *info,
		union iwreq_data *wrqu, char *extra) {

	card_p 			pdc = (card_p)dev->priv;
	fwlanusb_ioctl_t 	ioctl = { IOCTL_SET_BSSID };

	lib_memcpy (ioctl.data.mac.address, wrqu->ap_addr.sa_data, ETH_ALEN);

	if ((*wlan_lib->do_ioctl) (pdc, &ioctl) == IOCTL_ERROR) {
		return -EIO;
	}

	return 0;
}

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static int fwlanusb_get_ap(
		struct net_device *dev, struct iw_request_info *info,
		union iwreq_data *wrqu, char *extra) {

	card_p 			pdc = (card_p)dev->priv;
	fwlanusb_ioctl_t 	ioctl = { IOCTL_GET_BSSID };

	if ((*wlan_lib->do_ioctl) (pdc, &ioctl) == IOCTL_ERROR) {
		return -EIO;
	}

	wrqu->ap_addr.sa_family = ARPHRD_ETHER;
	lib_memcpy (wrqu->ap_addr.sa_data, ioctl.data.mac.address, ETH_ALEN);

	return 0;
}

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static int fwlanusb_set_mlme (
		struct net_device *dev, struct iw_request_info *info,
		union iwreq_data *wrqu, char *extra) {

	card_p pdc = (card_p)dev->priv;
	struct iw_mlme *mlme = (struct iw_mlme*) extra;
	fwlanusb_ioctl_t ioctl;

	switch (mlme->cmd) {
		case IW_MLME_DEAUTH:
			ioctl.type = IOCTL_DEAUTH;
			break;
		case IW_MLME_DISASSOC:
			ioctl.type = IOCTL_DISASSOC;
			break;
		default:
			ERROR("Unknown mlme cmd.\n");
	}

	ioctl.data.reason.value = mlme->reason_code;

	if ((*wlan_lib->do_ioctl) (pdc, &ioctl) == IOCTL_ERROR) {
		return -EIO;
	}

	return 0;
}

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static int fwlanusb_set_scan (
		struct net_device *dev, struct iw_request_info *info,
		union iwreq_data *wrqu, char *essid) {

	card_p 			pdc = (card_p)dev->priv;
	fwlanusb_ioctl_t 	ioctl = { IOCTL_SET_SCAN };
	
	if ((*wlan_lib->do_ioctl) (pdc, &ioctl) == IOCTL_ERROR) {
		return -EIO;
	}

	pdc->start_scan = jiffies;
	
	return 0;
}

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static int fwlanusb_get_scan (
		struct net_device *dev, struct iw_request_info *info,
		union iwreq_data *wrqu, char *extra) {

	card_p 			pdc = (card_p)dev->priv;
	fwlanusb_ioctl_t 	ioctl = { IOCTL_GET_SCAN };
	int			i, ret, sites = 20;
	char 			*stream = extra;
	struct iw_event		iwe;
	site_p			site;

	if (time_after (pdc->start_scan + (2 * HZ), jiffies))
		return -EAGAIN;
	
SCAN:
	if ((ioctl.data.scan.site = (site_p) hcalloc (sizeof (site_t) * sites)) == NULL)
		return -ENOMEM;
	ioctl.data.scan.nr = sites;
	ret = (*wlan_lib->do_ioctl) (pdc, &ioctl);
	if ( ret == IOCTL_ERROR) {
		hfree(ioctl.data.scan.site);
		return -EIO;
	}
	if ( ret == IOCTL_ERROR_NO_MEM) {
		hfree(ioctl.data.scan.site);
		if (sites == 50)
			/*More then 50 sites - unhealthy*/
			return -EIO;
		else
			/*Do it again*/
			sites = 50;
		goto SCAN;
	}

	for (i=0; i<ioctl.data.scan.nr; i++) {
		site = ioctl.data.scan.site + i;
		
		/*MAC*/	
		iwe.cmd = SIOCGIWAP;
		iwe.u.ap_addr.sa_family = ARPHRD_ETHER;
		lib_memcpy(iwe.u.ap_addr.sa_data, site->mac.address, ETH_ALEN);
		stream = iwe_stream_add_event(stream, extra + wrqu->data.length, &iwe, IW_EV_ADDR_LEN);
	
		/*SSID*/
		iwe.cmd = SIOCGIWESSID;
		iwe.u.data.length =  site->ssid.length;
		iwe.u.data.flags = 1;
		stream = iwe_stream_add_point(stream, extra + wrqu->data.length, &iwe, site->ssid.name);

		/*WPA/WPA2 IE*/
		iwe.cmd = IWEVGENIE;
		iwe.u.data.length = site->ies.length;
		stream = iwe_stream_add_point(stream, extra + wrqu->data.length, &iwe, site->ies.data);
	}
	
	hfree(ioctl.data.scan.site);

	wrqu->data.length = (stream - extra);
	wrqu->data.flags = 0;

	return 0;	
}
	
/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static int fwlanusb_set_essid (
		struct net_device *dev, struct iw_request_info *info,
		union iwreq_data *wrqu, char *essid) {

	card_p pdc = (card_p)dev->priv;
	fwlanusb_ioctl_t ioctl = { IOCTL_SET_SSID };

	if (wrqu->essid.flags) {
		if (wrqu->essid.length <= (IW_ESSID_MAX_SIZE + 1)) {
#if (WIRELESS_EXT < 21)	
			ioctl.data.ssid.length = wrqu->essid.length - 1;
#else
			ioctl.data.ssid.length = wrqu->essid.length;
#endif
			lib_memcpy(ioctl.data.ssid.name, essid, 
					ioctl.data.ssid.length);
			if ((*wlan_lib->do_ioctl) (pdc, &ioctl) == IOCTL_ERROR)
				return -EIO;
		}
		else
			return -E2BIG;
	} else {
		ioctl.data.ssid.length = 0;
		if ((*wlan_lib->do_ioctl) (pdc, &ioctl) == IOCTL_ERROR)
			return -EIO;
	}
	
	return 0;
	
}

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static int fwlanusb_get_essid (
		struct net_device *dev, struct iw_request_info *info,
		union iwreq_data *wrqu, char *essid) {

	card_p pdc = (card_p)dev->priv;
	fwlanusb_ioctl_t ioctl = { IOCTL_GET_SSID };
	int ssid_length;

	if ((*wlan_lib->do_ioctl) (pdc, &ioctl) == IOCTL_ERROR)
		return -EIO;

	ssid_length = ioctl.data.ssid.length;

	if (ssid_length) {
		wrqu->essid.flags = 1;
#if (WIRELESS_EXT < 21)	
		wrqu->essid.length = ssid_length + 1;
#else
		wrqu->essid.length = ssid_length;
#endif
		lib_memcpy (essid, ioctl.data.ssid.name, ssid_length);
		essid[ssid_length] = '\0';
	} else {
		wrqu->essid.flags = 0;
		wrqu->essid.length = 0;
	}
	
	return 0;
	
}

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static int fwlanusb_set_encode (
		struct net_device *dev, struct iw_request_info *info,
		union iwreq_data *wrqu, char *extra) {

	card_p pdc = (card_p)dev->priv;
	fwlanusb_ioctl_t ioctl;
	int auth, enc, index = (wrqu->encoding.flags & IW_ENCODE_INDEX) - 1, 
	    enc_on = 0;

	if (wrqu->encoding.length > 0) {

		if ((wrqu->encoding.length != 5) && (wrqu->encoding.length != 13))
		       return -EINVAL;

		if (!(wrqu->encoding.flags & IW_ENCODE_NOKEY)) {
		
			if ((index < 0) || (index > 3))
				ioctl.data.wep_key.def = 1;
			else {
				ioctl.data.wep_key.def = 0;
				ioctl.data.wep_key.index = index;
			}
			ioctl.data.wep_key.length = wrqu->encoding.length;
			memcpy (ioctl.data.wep_key.key, extra, wrqu->encoding.length);	

			ioctl.type = IOCTL_SET_WEP_KEY;
			if ((*wlan_lib->do_ioctl) (pdc, &ioctl) == IOCTL_ERROR) {
				return -EIO;
			}
			if (ioctl.data.wep_key.def == ioctl.data.wep_key.index)
				enc_on = 1;
		}
	} else {
		if ((index >= 0) && (index <=3)) {
			ioctl.data.wep_key.index = index;
			ioctl.data.wep_key.length = 0;
			
			ioctl.type = IOCTL_SET_WEP_KEY;
			if ((*wlan_lib->do_ioctl) (pdc, &ioctl) == IOCTL_ERROR) {
				return -EIO;
			}
		} else {
			if (!wrqu->encoding.flags & IW_ENCODE_MODE)
				return -EINVAL;
		}
	}

	if ((wrqu->encoding.flags & IW_ENCODE_DISABLED) && (!enc_on)) {
		auth = IOCTL_AUTH_OPEN;
		enc = IOCTL_ENC_NONE;
	} else {
		if (wrqu->encoding.flags & IW_ENCODE_RESTRICTED)
			auth = IOCTL_AUTH_SHARED;
		else
			auth = IOCTL_AUTH_OPEN;
		if (wrqu->encoding.length == 13)
			enc = IOCTL_ENC_WEP104;
		else
			enc = IOCTL_ENC_WEP40;
	}
	
	ioctl.type = IOCTL_SET_ENC;
	ioctl.data.auth.value = enc;
	if ((*wlan_lib->do_ioctl) (pdc, &ioctl) == IOCTL_ERROR) {
		return -EIO;
	}

	ioctl.type = IOCTL_SET_AUTH;
	ioctl.data.auth.value = auth;
	if ((*wlan_lib->do_ioctl) (pdc, &ioctl) == IOCTL_ERROR) {
		return -EIO;
	}

	return 0;
}

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static int fwlanusb_get_encode (
		struct net_device *dev, struct iw_request_info *info,
		union iwreq_data *wrqu, char *extra) {

	card_p pdc = (card_p)dev->priv;
	fwlanusb_ioctl_t ioctl;
	int auth, enc, index = (wrqu->encoding.flags & IW_ENCODE_INDEX) - 1;

	ioctl.type = IOCTL_GET_AUTH;
	if ((*wlan_lib->do_ioctl) (pdc, &ioctl) == IOCTL_ERROR)
		return -EIO;
	auth = ioctl.data.auth.value;
	
	ioctl.type = IOCTL_GET_ENC;
	if ((*wlan_lib->do_ioctl) (pdc, &ioctl) == IOCTL_ERROR)
		return -EIO;
	enc = ioctl.data.enc.value;

	if (enc == IOCTL_ENC_NONE)
		wrqu->encoding.flags = IW_ENCODE_DISABLED;
	else {
		if (auth == IOCTL_AUTH_SHARED)
		       	wrqu->encoding.flags = IW_ENCODE_RESTRICTED;
		else
			wrqu->encoding.flags = IW_ENCODE_OPEN;
	}
	
	ioctl.type = IOCTL_GET_WEP_KEY;
	if ((index < 0) || (index > 0))
		ioctl.data.wep_key.def = 1;
	else {
		ioctl.data.wep_key.def = 0;
		ioctl.data.wep_key.index = index;
	}
	if ((*wlan_lib->do_ioctl) (pdc, &ioctl) == IOCTL_ERROR)
		return -EIO;
	wrqu->encoding.flags |= ioctl.data.wep_key.def + 1;
	wrqu->encoding.length = ioctl.data.wep_key.length;
	lib_memcpy (extra, ioctl.data.wep_key.key, ioctl.data.wep_key.length);
	
	return 0;
}

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
int fwlanusb_set_genie (
		struct net_device *dev, struct iw_request_info *info,
		union iwreq_data *wrqu, char *extra) {

	return 0;
}

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
int fwlanusb_get_genie (
		struct net_device *dev, struct iw_request_info *info,
		union iwreq_data *wrqu, char *extra) {

	return 0;
}

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
int fwlanusb_set_auth (
		struct net_device *dev, struct iw_request_info *info,
		union iwreq_data *wrqu, char *extra) {

	card_p pdc = (card_p)dev->priv;
	struct iw_param *param = &wrqu->param;
	fwlanusb_ioctl_t ioctl;

	switch (param->flags & IW_AUTH_INDEX) {
		case IW_AUTH_WPA_VERSION:
			ioctl.type = IOCTL_SET_WPA_VERSION;
			switch (param->value) {
				case IW_AUTH_WPA_VERSION_DISABLED:
					return 0;
				case IW_AUTH_WPA_VERSION_WPA:
					ioctl.data.wpa_version.value = IOCTL_WPA_VERSION_WPA;
					break;
				case IW_AUTH_WPA_VERSION_WPA2:
					ioctl.data.wpa_version.value = IOCTL_WPA_VERSION_WPA2;
					break;
				default:
					ERROR("WPA version not supported.\n");
					return -ENOTSUPP;
			}
			if ((*wlan_lib->do_ioctl) (pdc, &ioctl) == IOCTL_ERROR) 
				return -EIO;
			break;
		case IW_AUTH_CIPHER_PAIRWISE:
		case IW_AUTH_CIPHER_GROUP:
			ioctl.type = IOCTL_SET_ENC;
			switch (param->value) {
				case IW_AUTH_CIPHER_NONE:
					ioctl.data.enc.value = IOCTL_ENC_NONE;
					break;
				case IW_AUTH_CIPHER_WEP40:
					ioctl.data.enc.value = IOCTL_ENC_WEP40;
					break;
				case IW_AUTH_CIPHER_WEP104:
					ioctl.data.enc.value = IOCTL_ENC_WEP104;
					break;
				case IW_AUTH_CIPHER_TKIP:
					ioctl.data.enc.value = IOCTL_ENC_TKIP;
					break;
				case IW_AUTH_CIPHER_CCMP:
					ioctl.data.enc.value = IOCTL_ENC_CCMP;
					break;
				default:
					ERROR("Cipher not supported.\n");
					return -ENOTSUPP;
			}
			if ((*wlan_lib->do_ioctl) (pdc, &ioctl) == IOCTL_ERROR)
				return -EIO;
			break;
		case IW_AUTH_KEY_MGMT:
			ioctl.type = IOCTL_SET_KEY_MGMT;
			switch (param->value) {
				case IW_AUTH_KEY_MGMT_802_1X:
					ioctl.data.key_mgmt.value = IOCTL_KEY_MGMT_802_1X;
					break;
				case IW_AUTH_KEY_MGMT_PSK:
					ioctl.data.key_mgmt.value = IOCTL_KEY_MGMT_PSK;
					break;
				default:
					ERROR("Key mgmt not supported.\n");
					return -ENOTSUPP;
			}
			if ((*wlan_lib->do_ioctl) (pdc, &ioctl) == IOCTL_ERROR)
				return -EIO;
			break;
		case IW_AUTH_TKIP_COUNTERMEASURES:
			break;
		case IW_AUTH_DROP_UNENCRYPTED:
			break;
		case IW_AUTH_80211_AUTH_ALG:
			ioctl.type = IOCTL_SET_AUTH;
			switch (param->value) {
				case IW_AUTH_ALG_OPEN_SYSTEM:
					ioctl.data.auth.value = IOCTL_AUTH_OPEN;
					break;
				case IW_AUTH_ALG_SHARED_KEY:
					ioctl.data.auth.value = IOCTL_AUTH_SHARED;
					break;
				default:
					ERROR("Auth algo not supported.\n");
					return -ENOTSUPP;
			}
			if ((*wlan_lib->do_ioctl) (pdc, &ioctl) == IOCTL_ERROR)
				return -EIO;
			break;
		case IW_AUTH_WPA_ENABLED:
			break;
		case IW_AUTH_RX_UNENCRYPTED_EAPOL:
			break;
	}

	return 0;
}

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
int fwlanusb_get_auth (
		struct net_device *dev, struct iw_request_info *info,
		union iwreq_data *wrqu, char *extra) {

	card_p pdc = (card_p)dev->priv;
	struct iw_param *param = &wrqu->param;
	fwlanusb_ioctl_t ioctl;

	switch (param->flags & IW_AUTH_INDEX) {
		case IW_AUTH_WPA_VERSION:
			ioctl.type = IOCTL_GET_AUTH;
			if ((*wlan_lib->do_ioctl) (pdc, &ioctl) == IOCTL_ERROR)
				return -EIO;
			switch (ioctl.data.wpa_version.value) {
				case IOCTL_WPA_VERSION_WPA:
					param->value = IW_AUTH_WPA_VERSION_WPA;
					break;
				case IOCTL_WPA_VERSION_WPA2:
					param->value = IW_AUTH_WPA_VERSION_WPA2;
					break;
				default:
					param->value = IW_AUTH_WPA_VERSION_DISABLED;
			}
			break;
		case IW_AUTH_CIPHER_PAIRWISE:
		case IW_AUTH_CIPHER_GROUP:
			ioctl.type = IOCTL_GET_ENC;
			if ((*wlan_lib->do_ioctl) (pdc, &ioctl) == IOCTL_ERROR)
				return -EIO;
			switch (ioctl.data.enc.value) {
				case IOCTL_ENC_WEP40:
					param->value = IW_AUTH_CIPHER_WEP40;
					break;
				case IOCTL_ENC_WEP104:
					param->value = IW_AUTH_CIPHER_WEP104;
					break;
				case IOCTL_ENC_TKIP:
					param->value = IW_AUTH_CIPHER_TKIP;
					break;
				case IOCTL_ENC_CCMP:
					param->value = IW_AUTH_CIPHER_CCMP;
					break;
				default:
					param->value = IW_AUTH_CIPHER_NONE;
			}
			break;
		case IW_AUTH_KEY_MGMT:
			ioctl.type = IOCTL_GET_KEY_MGMT;
			if ((*wlan_lib->do_ioctl) (pdc, &ioctl) == IOCTL_ERROR) {
				return -EIO;
			}
			switch (ioctl.data.auth.value) {
				case IOCTL_KEY_MGMT_802_1X:
					param->value = IW_AUTH_KEY_MGMT_802_1X;
					break;
				case IOCTL_KEY_MGMT_PSK:
					param->value = IW_AUTH_KEY_MGMT_PSK;
					break;
				default:
					return -EIO;
			}
			break;
		case IW_AUTH_TKIP_COUNTERMEASURES:
			break;
		case IW_AUTH_DROP_UNENCRYPTED:
			break;
		case IW_AUTH_80211_AUTH_ALG:
			ioctl.type = IOCTL_GET_AUTH;
			if ((*wlan_lib->do_ioctl) (pdc, &ioctl) == IOCTL_ERROR) {
				return -EIO;
			}
			switch (ioctl.data.auth.value) {
				case IOCTL_AUTH_OPEN:
					param->value = IW_AUTH_ALG_OPEN_SYSTEM;
					break;
				case IOCTL_AUTH_SHARED:
					param->value = IW_AUTH_ALG_SHARED_KEY;
					break;
				default:
					return -EIO;
			}
			break;
		case IW_AUTH_WPA_ENABLED:
			break;
		case IW_AUTH_RX_UNENCRYPTED_EAPOL:
			break;
	}

	return 0;
}

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
int fwlanusb_set_encodeext (
		struct net_device *dev, struct iw_request_info *info,
		union iwreq_data *wrqu, char *extra) {

	card_p 			pdc = (card_p)dev->priv;
	fwlanusb_ioctl_t 	ioctl;
	struct iw_encode_ext *	ext = (struct iw_encode_ext *)extra;
	int index;

	index = (wrqu->encoding.flags & IW_ENCODE_INDEX) - 1;

	if (ext->key_len > 0) {
		switch (ext->alg) {
			case IW_ENCODE_ALG_NONE:
				break;
			case IW_ENCODE_ALG_WEP:
				break;
			case IW_ENCODE_ALG_TKIP:
			case IW_ENCODE_ALG_CCMP:
				assert ((ext->ext_flags & (IW_ENCODE_EXT_SET_TX_KEY | IW_ENCODE_EXT_GROUP_KEY)));
				ioctl.type = IOCTL_SET_KEY;
				ioctl.data.key.l = sizeof (ioctl_key_t);
				ioctl.data.key.index = index;
				ioctl.data.key.index |= 0x10000000;
				if (ext->ext_flags & IW_ENCODE_EXT_SET_TX_KEY)
					ioctl.data.key.index |= 0xC0000000;
				lib_memcpy (ioctl.data.key.mac.address, ext->addr.sa_data, ETH_ALEN);
				if (ext->ext_flags & IW_ENCODE_EXT_RX_SEQ_VALID) {
					ioctl.data.key.rsc = *(long long*)ext->rx_seq;
					ioctl.data.key.index |= 0x20000000;
				} else {
					ioctl.data.key.rsc = 0;
				}
				if ((ext->key_len) > 32)
					return -ENOTSUPP;
				ioctl.data.key.length = ext->key_len;
				lib_memcpy (ioctl.data.key.key, ext->key, ext->key_len);
				break;
			default:
				break;
		}

		if ((*wlan_lib->do_ioctl) (pdc, &ioctl) == IOCTL_ERROR)
				return -EIO;
	}

	return 0;
}

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
int fwlanusb_get_encodeext (
		struct net_device *dev, struct iw_request_info *info,
		union iwreq_data *wrqu, char *extra) {

	return 0;
}

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
int fwlanusb_get_hw_mac (
		struct net_device *dev, struct iw_request_info *info,
		union iwreq_data *wrqu, char *extra) {

	card_p pdc = (card_p)dev->priv;
	fwlanusb_ioctl_t ioctl = { IOCTL_GET_HW_MAC };
	struct sockaddr *mac = (struct sockaddr *) extra;
	
	if ((*wlan_lib->do_ioctl) (pdc, &ioctl) == IOCTL_ERROR)
		return -EIO;

	lib_memcpy (mac->sa_data, ioctl.data.mac.address, ETH_ALEN);
	mac->sa_family = ARPHRD_ETHER;
	
	return 0;
}

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
int fwlanusb_disassociate(
		struct net_device *dev, struct iw_request_info *info,
		union iwreq_data *wrqu, char *extra) {
	
	card_p pdc = (card_p)dev->priv;
	fwlanusb_ioctl_t ioctl = { .type = IOCTL_DISASSOC, 
				   .data.reason.value = 1 };

	if ((*wlan_lib->do_ioctl) (pdc, &ioctl) == IOCTL_ERROR)
		return -EIO;

	return 0;
}

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static int fwlanusb_set_g_plus_plus (
		struct net_device *dev, struct iw_request_info *info,
		union iwreq_data *wrqu, char *extra) {

	card_p pdc = (card_p)dev->priv;
	fwlanusb_ioctl_t ioctl = { IOCTL_SET_G_PLUS_PLUS };
	
	ioctl.data.g_plus_plus.value = *(__u32*)wrqu;

	if ((ioctl.data.g_plus_plus.value != 0) && 
	    (ioctl.data.g_plus_plus.value != 1))
		return -EINVAL;

	if ((*wlan_lib->do_ioctl) (pdc, &ioctl) == IOCTL_ERROR)
		return -EIO;

	return 0;
}

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static int fwlanusb_get_g_plus_plus (
		struct net_device *dev, struct iw_request_info *info,
		union iwreq_data *wrqu, char *extra) {

	card_p pdc = (card_p)dev->priv;
	fwlanusb_ioctl_t ioctl = { IOCTL_GET_G_PLUS_PLUS };

	if ((*wlan_lib->do_ioctl) (pdc, &ioctl) == IOCTL_ERROR)
		return -EIO;

	*(__u32*)wrqu = ioctl.data.g_plus_plus.value;
	
	return 0;
}

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static int fwlanusb_set_dbg_severity (
		struct net_device *dev, struct iw_request_info *info,
		union iwreq_data *wrqu, char *extra) {

	card_p pdc = (card_p)dev->priv;
	fwlanusb_ioctl_t ioctl = { IOCTL_SET_DBG_SEVERITY };

	ioctl.data.dbg_sev.value = *(__u32*)wrqu;
	
	if ((*wlan_lib->do_ioctl) (pdc, &ioctl) == IOCTL_ERROR)
		return -EIO;

	return 0;
}
	
/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static int fwlanusb_get_dbg_severity (
		struct net_device *dev, struct iw_request_info *info,
		union iwreq_data *wrqu, char *extra) {

	card_p pdc = (card_p)dev->priv;
	fwlanusb_ioctl_t ioctl = { IOCTL_GET_DBG_SEVERITY };

	if ((*wlan_lib->do_ioctl) (pdc, &ioctl) == IOCTL_ERROR)
		return -EIO;

	*(__u32*)wrqu = ioctl.data.dbg_sev.value;
	
	return 0;
}

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static int fwlanusb_set_dbg_module (
		struct net_device *dev, struct iw_request_info *info,
		union iwreq_data *wrqu, char *extra) {

	card_p pdc = (card_p)dev->priv;
	fwlanusb_ioctl_t ioctl = { IOCTL_SET_DBG_MODULE };

	ioctl.data.dbg_mod.value = *(__u32*)wrqu;
	
	if ((*wlan_lib->do_ioctl) (pdc, &ioctl) == IOCTL_ERROR)
		return -EIO;

	return 0;
}
	
/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static int fwlanusb_get_dbg_module (
		struct net_device *dev, struct iw_request_info *info,
		union iwreq_data *wrqu, char *extra) {

	card_p pdc = (card_p)dev->priv;
	fwlanusb_ioctl_t ioctl = { IOCTL_GET_DBG_MODULE };

	if ((*wlan_lib->do_ioctl) (pdc, &ioctl) == IOCTL_ERROR)
		return -EIO;

	*(__u32*)wrqu = ioctl.data.dbg_mod.value;

	return 0;
}

/*---------------------------------------------------------------------------*\
\*---------------------------------------------------------------------------*/
static const iw_handler		fwlanusb_handler[] =
{
	fwlanusb_commit,		/* SIOCSIWCOMMIT */
	fwlanusb_get_name,		/* SIOCGIWNAME */
	NULL,				/* SIOCSIWNWID */
	NULL,				/* SIOCGIWNWID */
	fwlanusb_set_freq,		/* SIOCSIWFREQ */
	fwlanusb_get_freq,		/* SIOCGIWFREQ */
	fwlanusb_set_mode,		/* SIOCSIWMODE */
	fwlanusb_get_mode,		/* SIOCGIWMODE */
	NULL,				/* SIOCSIWSENS */
	NULL,				/* SIOCGIWSENS */
	NULL,				/* SIOCSIWRANGE */
	fwlanusb_get_range,		/* SIOCGIWRANGE */
	NULL,				/* SIOCSIWPRIV */
	NULL,				/* SIOCGIWPRIV */
	NULL,				/* SIOCSIWSTATS */
	NULL,				/* SIOCGIWSTATS */
	NULL,				/* SIOCSIWSPY */
	NULL,				/* SIOCGIWSPY */
	NULL,				/* SIOCSIWTHRSPY */
	NULL,				/* SIOCGIWTHRSPY */
	fwlanusb_set_ap,		/* SIOCSIWAP */
	fwlanusb_get_ap,		/* SIOCGIWAP */
	fwlanusb_set_mlme,		/* SIOCSIWMLME */
	NULL,				/* SIOCGIWAPLIST */
	fwlanusb_set_scan,		/* SIOCSIWSCAN */
	fwlanusb_get_scan,		/* SIOCGIWSCAN */
	fwlanusb_set_essid,		/* SIOCSIWESSID */
	fwlanusb_get_essid,		/* SIOCGIWESSID */
	NULL,				/* SIOCSIWNICKN */
	NULL,				/* SIOCGIWNICKN */
	NULL,				/* -- hole -- */
	NULL,				/* -- hole -- */
	NULL,				/* SIOCSIWRATE */
	NULL,				/* SIOCGIWRATE */
	NULL,				/* SIOCSIWRTS */
	NULL,				/* SIOCGIWRTS */
	NULL,				/* SIOCSIWFRAG */
	NULL,				/* SIOCGIWFRAG */
	NULL,				/* SIOCSIWTXPOW */
	NULL,				/* SIOCGIWTXPOW */
	NULL,				/* SIOCSIWRETRY */
	NULL,				/* SIOCGIWRETRY */
	fwlanusb_set_encode,		/* SIOCSIWENCODE */
	fwlanusb_get_encode,		/* SIOCGIWENCODE */
	NULL,				/* SIOCSIWPOWER */
	NULL,				/* SIOCGIWPOWER */
	NULL,				/* -- hole -- */
	NULL,				/* -- hole -- */
	fwlanusb_set_genie,		/* SIOCSIWGENIE */
	fwlanusb_get_genie,		/* SIOCGIWGENIE */
	fwlanusb_set_auth,		/* SIOCSIWAUTH */
	fwlanusb_get_auth,		/* SIOCGIWAUTH */
	fwlanusb_set_encodeext,		/* SIOCSIWENCODEEXT */
	fwlanusb_get_encodeext,		/* SIOCGIWENCODEEXT */
	NULL,				/* SIOCSIWPMKSA */

};

static const iw_handler fwlanusb_private_handler[] =
{
	NULL,				/* SIOCIWFIRSTPRIV     */
	fwlanusb_get_hw_mac,		/* SIOCIWFIRSTPRIV + 1 */
	NULL,				/* SIOCIWFIRSTPRIV + 2 */
	fwlanusb_disassociate,		/* SIOCIWFIRSTPRIV + 3 */
	fwlanusb_set_g_plus_plus,	/* SIOCIWFIRSTPRIV + 4 */
	fwlanusb_get_g_plus_plus,	/* SIOCIWFIRSTPRIV + 5 */
	fwlanusb_set_dbg_severity,	/* SIOCIWFIRSTPRIV + 6 */
	fwlanusb_get_dbg_severity,	/* SIOCIWFIRSTPRIV + 7 */
	fwlanusb_set_dbg_module,	/* SIOCIWFIRSTPRIV + 8 */
	fwlanusb_get_dbg_module,	/* SIOCIWFIRSTPRIV + 9 */
};

static const struct iw_priv_args fwlanusb_private_args[] = {
/*	{ cmd,         set_args,                            get_args, name } */
	{ SIOCGIPMAC, 0, IW_PRIV_TYPE_ADDR | IW_PRIV_SIZE_FIXED | 1 , "gethwmac" },
	{ SIOCGIWDISASSOC, 0, 0, "disassociate" },
	{ SIOCSIWGPLUSPLUS, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "setgplusplus" },
	{ SIOCGIWGPLUSPLUS, 0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getgplusplus" },
  	{ SIOCSDBGSEVY, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "setdbgseverity" },
  	{ SIOCGDBGSEVY, 0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getdbgseverity" },
  	{ SIOCSDBGMODULE, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "setdbgmodule" },
  	{ SIOCGDBGMODULE, 0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getdbgmodule" },
};


const struct iw_handler_def	fwlanusb_handler_def =
{
	.num_standard		= sizeof(fwlanusb_handler)/sizeof(iw_handler),
	.num_private		= sizeof(fwlanusb_private_handler)/sizeof(iw_handler),
	.num_private_args 	= sizeof(fwlanusb_private_args)/sizeof(struct iw_priv_args),
	.standard		= fwlanusb_handler,
	.private		= fwlanusb_private_handler,
	.private_args		= fwlanusb_private_args,
	.get_wireless_stats 	= fwlanusb_get_wireless_stats,
};

void wext_event_send (card_p pdc, fwlanusb_event_t event, 
		void *event_data, unsigned int event_data_size) {

	union iwreq_data wrqu;


	if (atomic_read (&pdc->shutdown)) 
		return;

	lib_memset (&wrqu, 0, sizeof (union iwreq_data));

	switch (event) {
		case EVENT_ASSOCIATED:
			if (fwlanusb_get_ap (pdc->net_dev, NULL, &wrqu, NULL)) {
				return;
			}
			wireless_send_event (pdc->net_dev, SIOCGIWAP, &wrqu, NULL);
			break;
		case EVENT_DISASSOCIATED:
			wireless_send_event (pdc->net_dev, SIOCGIWAP, &wrqu, NULL);
			break;
		default:
			ERROR("Unkown event %d.\n", event);
	}
}

