#!/usr/bin/env /bin/bash
# Use compat_ delayed_work functions to fix argument issues.
sed -i -e "s:inode->i_private:inode->u.generic_ip:g" \
    $1/net/mac80211/*.{c,h}

