#!/usr/bin/env /bin/bash
# Use compat_ delayed_work functions to fix argument issues.
sed -i -e "s:cancel_delayed_work(:compat_cancel_delayed_work(:g" \
    -e "s:queue_delayed_work(:compat_queue_delayed_work(:g" \
    -e "s:schedule_delayed_work(:compat_schedule_delayed_work(:g" \
    $1/net/mac80211/*.{c,h}
