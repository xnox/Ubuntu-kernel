#!/usr/bin/env /bin/bash
# Compatibility for queue_delayed_work -> compat_schedule_delayed_work
#
sed -i -e "s:queue_delayed_work(:compat_queue_delayed_work(:g" \
    -e "s:schedule_delayed_work(:compat_schedule_delayed_work(:g" \
	"$1"/*.{c,h} || exit 1
exit 0

