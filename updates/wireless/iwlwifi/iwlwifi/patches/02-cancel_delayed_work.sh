#!/usr/bin/env /bin/bash
# Compatibility for cancel_delayed_work -> compat_cancel_delayed_work
sed -i -e 's,cancel_delayed_work(,compat_cancel_delayed_work(,g' "$1"/*.{c,h} || exit 1
exit 0
