#!/usr/bin/env /bin/bash
# Type-cast reused void* ax25_ptr to struct wireless_dev *
sed -i -e \
 's,dev\->ieee80211_ptr,((struct wireless_dev *)dev->ieee80211_ptr),g' \
 $1/net/wireless/*.[ch] 
sed -i -e 's,dev\->ieee80211_ptr\->wiphy,((struct wireless_dev *)dev->ieee80211_ptr)->wiphy,g'\
 $1/net/mac80211/wme.c $1/net/mac80211/debugfs_netdev.c
