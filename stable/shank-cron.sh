#!/bin/bash

#
# This script is intended to be called directly from crontab, to run
# periodically sru-workflow-manager (shank bot). Provides lock protection,
# timeout protection and useful logging.
#

# adjust variables below for your environment
lockfile=/home/yourhome/run/shank.lock
logdir=/srv/www/shank-log
oldlogdir="$logdir/old"
shankdir=/home/yourhome/run/kteam-tools/stable

# check if directories above exist
for d in $logdir $oldlogdir $shankdir; do
	if [ ! -d $d ]; then
		echo "Directory $d doesn't exist, aborting"
		exit 1
	fi
done

# locking, to avoid concurrent runs in case we got stuck in a previous run
if ! (set -o noclobber; echo "$$" > "$lockfile") 2> /dev/null; then
	if ! kill -n 0 `cat "$lockfile"` 2>/dev/null; then
		rm -f "$lockfile"
		exec $0 "$@"
	fi
	exit
fi
trap "rm -f \"$lockfile\" 2> /dev/null; exit" EXIT SIGTERM

# run shank bot: we redirect cron stdin/stderr, so we can write output to a
# file; also, if for some reason the connection is stuck (shank bot waits
# "forever" for any data), make the shank bot execution timeout; this is a
# complement to the locking (avoid the bot being stuck forever, allows cron
# timed execution to be lower than the hard coded timeout value)
cur_log="$logdir/shank-log.txt"
[ -f "$cur_log" ] && cp -f "$cur_log" "$cur_log.tmp"
LC_ALL=C date > "$logdir/shank-last-run.txt"
cd "$shankdir"
exec {so}>&1 1>"$cur_log" {se}>&2 2>&1
timeout 1h ./sru-workflow-manager --verbose
exec 1>&$so {so}>&- 2>&$se {se}>&-

# preserve execution log if different from previous run
if [ -f "$cur_log.tmp" ]; then
	n=`stat -c %Z "$cur_log.tmp"`
	difflog="$logdir/difflog-$n.txt"
	if ! diff -up "$cur_log.tmp" "$cur_log" > "$difflog"; then
		mv -f "$cur_log.tmp" "$logdir/shank-log-$n.txt"
	else
		rm -f "$difflog"
		rm -f "$cur_log.tmp"
	fi
fi

# "rotation" of old logs (move to another place)
find "$logdir" -name difflog-\*.txt -mtime +20 -print0 | xargs -n 1 -r --null \
	sh -c "mv \"\$0\" \"$oldlogdir/old-\`basename \$0\`\""
find "$logdir" -name shank-log-\*.txt -mtime +20 -print0 | xargs -n 1 -r --null \
	sh -c "mv \"\$0\" \"$oldlogdir/old-\`basename \$0\`\""

