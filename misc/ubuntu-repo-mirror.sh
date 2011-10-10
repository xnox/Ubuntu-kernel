#!/bin/bash
#
# This script mirrors git repositories from zinc.canonical.com onto github.com.
# This assumes that the account running this mirror script has the keys from
# kernel-ppa@canonical.com. You must also use some .ssh/config magic:
#
# Host github.com
#    IdentityFile /home/timg/.ssh/kernel_ppa_rsa
#

MIRROR_REPO_PATH=Ubuntu-kernel
MIRROR_REPO_HOST=git@github.com:Canonical-kernel
SOURCE_REPO_HOST=git://kernel.ubuntu.com/ubuntu
RELEASES="hardy lucid maverick natty oneiric"
WORK_DIR=~/ubuntu-repo-mirror
REMOTE=ubuntu

mkdir -p ${WORK_DIR} || exit 1

cd ${WORK_DIR}

if [ ! -d ${MIRROR_REPO_PATH} ]
then
	git clone ${MIRROR_REPO_HOST}/${MIRROR_REPO_PATH}.git || exit 1
fi

cd ${MIRROR_REPO_PATH}
git checkout -f master

#
# Clean out all remotes.
#
git remote show | grep -v origin | while read r
do
	git remote rm $r
done

#
# Drop all local branches except master
#
for i in `git branch -a | grep -v remotes | egrep -v "^\*[ \t]*master"`
do
	git branch -D ${i}
done

#
# Replicate the branches in each repository, substituting '/' for '-' since
# github doesn't seem to support branch names of the form RELEASE/master. It really
# wants something like RELEASE-master.
#
for i in ${RELEASES}
do
	git remote add ${i} ${SOURCE_REPO_HOST}/ubuntu-${i}.git
	git fetch ${i}

	for j in `git branch -a | egrep "remotes\/${i}"|grep -v origin|sed "s;remotes\/${i}\/;;"`
	do
		git branch ${i}-${j} remotes/${i}/${j}
		git push origin +${i}-${j}
	done

	git push origin --tags
done

