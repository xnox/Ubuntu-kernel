#!/bin/bash

LINUX=git://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git
LLREPO=linux-2.6.git
LREPO=linux.git
UBUNTU=git://kernel.ubuntu.com/ubuntu
RELEASES="hardy lucid maverick natty oneiric precise"

if [ ! -d ${LREPO} ]
then
	git clone ${LINUX} ${LREPO}
	(cd ${LREPO}; git fetch origin)
	rm -rf ${LLREPO}
	ln -s ${LREPO} ${LLREPO}
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
