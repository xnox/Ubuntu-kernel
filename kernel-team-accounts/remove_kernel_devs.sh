#!/bin/bash
#
# Add kernel developer accountsaaccording to the information in
# kernel_devs.conf
#

. kernel_devs.conf

HOME=/home

let index=0
for i in ${kdev[@]}
do
	if who | grep $i > /dev/null
	then
		echo Cannot remove a user that is logged in.
	elif grep $i /etc/passwd
	then
		echo $i
		deluser --remove-home $i
	fi
	let index=${index}+1
done

