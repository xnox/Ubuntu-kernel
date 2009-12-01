#!/bin/bash
#
# <hostname> <dist> [<arch>]
#
HOST=""
DIST=""
ARCH=""
VERBOSE=false
while [ $# -gt 0 ]; do
	case $1 in
		-v|--verbose)
			VERBOSE=true
			;;
		-*|--*)
			echo "Unknown option $1" >&2
			exit 1
			;;
		*)
			if [ "$HOST" = "" ]; then
				HOST="$1"
			elif [ "$DIST" = "" ]; then
				DIST="$1"
			elif [ "$ARCH" = "" ]; then
				ARCH="$1"
			else
				echo "Too many arguments" >&2
				exit 1
			fi
			;;
	esac
	shift
done

if [ "$HOST" = "" -o "$DIST" = "" ]; then
	echo "Usage: $(basename $0) <hostname> <dist> [<arch>]"
	exit 1
fi
if [ ! -r $(dirname $0)/other-pkgs/others-$DIST ]; then
	echo "No package definition for $DIST" >&2
	exit 1
fi

#
# Find out which chroot command is used
#
CMD=""
INFO=$(
	cat <<-EOD
	HOSTARCH="\$(dpkg --print-architecture)"
	echo "HOSTARCH=\"\$HOSTARCH\""
	if [ "$ARCH" = "" ]; then
		NAME="$DIST"
	elif [ "\$HOSTARCH" = "$ARCH"  ]; then
		NAME="$DIST"
	else
		NAME="$DIST-$ARCH"
	fi
	echo "CHROOTNAME=\"\$NAME\""
	if schroot -l 2>/dev/null | grep -q "$DIST\$EXT"; then
		echo "CMD=\"schroot -q -c\""
	elif dchroot -l 2>/dev/null | grep -q "$DIST\$EXT"; then
		echo "CMD=\"dchroot -q -c\""
	fi
	EOD
)
eval "$(echo "$INFO"|ssh $HOST)"

if $VERBOSE; then
	echo "INFO: Host architecture: $HOSTARCH"
fi
if [ "$CMD" = "" ]; then
	echo "No chroot defined for $CHROOTNAME" >&2
	exit 1
fi
if $VERBOSE; then
	echo "INFO: Using '$CMD$CHROOTNAME' to access chroot."
fi
if [ "$ARCH" = "" ]; then
	ARCH="$HOSTARCH"
fi
PKGS="$($(dirname $0)/list-other-pkgs $DIST $ARCH)"

cat <<EOD | ssh $HOST $CMD$CHROOTNAME 2>/dev/null
RC=0
MISS=""
for i in $PKGS; do
	if [ "\$(dpkg -l \$i 2>/dev/null | grep ^ii)" == "" ]; then
		if [ "\$MISS" = "" ]; then
			MISS="\$i"
		else
			MISS="\$MISS \$i"
		fi
	fi
done
if [ "\$MISS" != "" ]; then
	echo "HOST: $HOST"
	echo "DIST: $DIST($ARCH)"
	echo "ERROR: The following packages are not installed:"
	echo "\$MISS"
	RC=1
fi
if [ \$RC -eq 0 ]; then
	echo "The chroot setup on $HOST is ok"
fi
exit \$RC
EOD
