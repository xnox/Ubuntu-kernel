#!/bin/bash

LINUX=git://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux-2.6.git
LREPO=linux-2.6.git
UBUNTU=git://kernel.ubuntu.com/ubuntu
RELEASES="dapper hardy karmic lucid maverick natty"

cd `dirname $0`

if [ ! -d ${LREPO} ]
then
	git clone ${LINUX} ${LREPO}
	(cd ${LREPO}; git fetch origin)
else
	(cd ${LREPO}; git fetch origin;git fetch origin master;git reset --hard FETCH_HEAD)
fi

for i in ${RELEASES}
do
	if [ ! -d ubuntu-${i}.git ]
	then
		git clone --reference ${LREPO} ${UBUNTU}/ubuntu-${i}.git ubuntu-${i}.git
	else
		(cd ubuntu-${i}.git;git fetch origin;git fetch origin master;git reset --hard FETCH_HEAD)
	fi
done

#
# Create and update a copy of the kteam-tools repo
#
if [ ! -d kteam-tools ]
then
	git clone ${UBUNTU}/kteam-tools.git kteam-tools
else
	(cd kteam-tools; git fetch origin;git fetch origin master;git reset --hard FETCH_HEAD)
fi
