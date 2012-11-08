#!/bin/bash
#
# Remove obsolete kernel developers.
#
#

. kernel_devs.conf

let index=0
for i in ${kdev[@]}
do
	if [ ! -z "${kdev_obsolete[${index}]}" ]
	then
		echo "Obsoleting ${kdev_name[${index}]}"
		deluser ${kdev[${index}]} sbuild
		deluser --remove-home ${kdev[${index}]}
	fi

	let index=${index}+1
done

