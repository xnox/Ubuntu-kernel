#!/usr/bin/env /bin/bash
# IEEE80211_STYPE_QOS_DATA's mask is 0x0080
# the bit position is 7
sed -i -e 's,ilog2(IEEE80211_STYPE_QOS_DATA),7 /* QoS data pos */,g' \
	$1/net/mac80211/ieee80211.c
