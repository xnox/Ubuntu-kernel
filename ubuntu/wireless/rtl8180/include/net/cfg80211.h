#ifndef __NET_CFG80211_H
#define __NET_CFG80211_H

#include <linux/netlink.h>
#include <linux/skbuff.h>
#include <net/genetlink.h>

/*
 * 802.11 configuration in-kernel interface
 *
 * Copyright 2006 Johannes Berg <johannes@sipsolutions.net>
 */

#define SSID_MAX_LEN	32

/**
 * struct cfg80211_config - description of a configuration (request)
 */
struct cfg80211_config {
	/* see below */
	u32 valid;

	s8 ssid_len;
	u8 ssid[SSID_MAX_LEN];

	s32 rx_sensitivity;
	u32 transmit_power;
	u32 fragmentation_threshold;
	u32 channel;
};

#define CFG80211_CFG_VALID_SSID			(1<<0)
#define CFG80211_CFG_VALID_RX_SENSITIVITY	(1<<1)
#define CFG80211_CFG_VALID_TRANSMIT_POWER	(1<<2)
#define CFG80211_CFG_VALID_FRAG_THRESHOLD	(1<<3)
#define CFG80211_CFG_VALID_CHANNEL		(1<<4)

struct scan_channel {
	u32 channel;
	int active;
};

struct scan_params {
	/* number of items in 'channels' array
	 * or -1 to indicate scanning all channels
	 * (in that case 'channels' is NULL) */
	int n_channels;

	/* use only when n_channels is -1 to determine
	 * whether scanning should be active or not */
	int active;

	/* the channel list if any */
	struct scan_channel *channels;
};

/* from net/wireless.h */
struct wiphy;

/**
 * struct cfg80211_ops - backend description for wireless configuration
 *
 * This struct is registered by fullmac card drivers and/or wireless stacks
 * in order to handle configuration requests on their interfaces.
 *
 * All callbacks except where otherwise noted should return 0
 * on success or a negative error code.
 *
 * @inject_packet: inject the given frame with the NL80211_FLAG_*
 *		   flags onto the given queue.
 *
 * @add_virtual_intf: create a new virtual interface with the given name
 *
 * @del_virtual_intf: remove the virtual interface determined by ifindex.
 *
 * @configure: configure the given interface as requested in the config struct.
 *	       must not ignore any configuration item, if something is
 *	       is requested that cannot be fulfilled return an error.
 *             This call does not actually initiate any association or such.
 *
 * @get_config: fill the given config structure with the current configuration
 *
 * @get_config_valid: return a bitmask of CFG80211_CFG_VALID_* indicating
 *		      which parameters can be set.
 *
 * @associate: associate with previously given settings (SSID, BSSID
 *             if userspace roaming is enabled)
 *
 * @reassociate: reassociate with current settings (SSID, BSSID if
 *		 userspace roaming is enabled)
 *
 * @disassociate: disassociate from current AP
 *
 * @deauth: deauth from current AP
 *
 * @initiate_scan: ...
 *
 * @set_roaming: set who gets to control roaming, the roaming_control
 *		 parameter is passed NL80211_ROAMING_CONTROL_* values.
 *
 * @get_roaming: return where roaming control currently is done or
 *		 a negative error.
 *
 * @set_fixed_bssid: set BSSID to use with userspace roaming, forces
 *		     reassociation if changing.
 * @get_fixed_bssid: get BSSID that is used with userspace roaming,
 *		     the bssid parameter has space for 6 bytes
 *
 * @get_association: get BSSID of the BSS that the device is currently
 *		     associated to and return 1, or return 0 if not
 *		     associated (or a negative error code)
 * @get_auth_list: get list of BSSIDs of all BSSs the device has
 *		   authenticated with, must call next_bssid for each,
 *		   next_bssid returns non-zero on error, the given data
 *		   is to be passed to that callback
 */
struct cfg80211_ops {
	int	(*inject_packet)(struct wiphy *wiphy, void *frame, int framelen,
				 u32 flags, int queue);


	int	(*add_virtual_intf)(struct wiphy *wiphy, char *name,
				    unsigned int type);
	int	(*del_virtual_intf)(struct wiphy *wiphy, int ifindex);


	int	(*configure)(struct wiphy *wiphy, struct net_device *dev,
			     struct cfg80211_config *cfg);
	void	(*get_config)(struct wiphy *wiphy, struct net_device *dev,
			      struct cfg80211_config *cfg);
	u32	(*get_config_valid)(struct wiphy *wiphy,
				    struct net_device *dev);


	int	(*associate)(struct wiphy *wiphy, struct net_device *dev);
	int	(*reassociate)(struct wiphy *wiphy, struct net_device *dev);
	int	(*disassociate)(struct wiphy *wiphy, struct net_device *dev);
	int	(*deauth)(struct wiphy *wiphy, struct net_device *dev);


	int	(*initiate_scan)(struct wiphy *wiphy, struct net_device *dev,
				 struct scan_params *params);


	int	(*set_roaming)(struct wiphy *wiphy, struct net_device *dev,
			       int roaming_control);
	int	(*get_roaming)(struct wiphy *wiphy, struct net_device *dev);
	int	(*set_fixed_bssid)(struct wiphy *wiphy, struct net_device *dev,
				   u8 *bssid);
	int	(*get_fixed_bssid)(struct wiphy *wiphy, struct net_device *dev,
				   u8 *bssid);


	int	(*get_association)(struct wiphy *wiphy, struct net_device *dev,
				   u8 *bssid);

	int	(*get_auth_list)(struct wiphy *wiphy, struct net_device *dev,
				 void *data,
				 int (*next_bssid)(void *data, u8 *bssid));
};


/* helper functions specific to nl80211 */
extern void *nl80211hdr_put(struct sk_buff *skb, u32 pid,
			    u32 seq, int flags, u8 cmd);
extern void *nl80211msg_new(struct sk_buff **skb, u32 pid,
			    u32 seq, int flags, u8 cmd);

#endif /* __NET_CFG80211_H */
