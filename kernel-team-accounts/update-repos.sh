#!/bin/bash

LOCALDIR=/usr3/ubuntu
REPOS="dapper hardy jaunty karmic lucid maverick"
LINUX=linux-2.6
LINUX_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/torvalds/${LINUX}.git
UBUNTU_REPO=git://kernel.ubuntu.com/ubuntu

if [ ! -d ${LOCALDIR} ]
then
	echo You must create ${LOCALDIR} first
	exit 1
fi
cd ${LOCALDIR}

if [ ! -d ${LINUX} ]
then
	git clone ${LINUX_REPO}
fi

(cd ${LINUX};
git fetch origin
git fetch --tags origin
git rebase origin)

for i in ${REPOS}
do
	if [ ! -d ubuntu-$i ]
	then
		git clone --reference ${LINUX} ${UBUNTU_REPO}/ubuntu-$i.git ubuntu-$i
	else
		(cd ubuntu-$i;git-force-update)
	fi
done

