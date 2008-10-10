#!/bin/bash
#
# Run this script when you want to update from
# the upstream wireless-testing and compat-wireless-2.6
# git repositories.
#

ROOT=`pwd`
CDIR=${ROOT}/../compat-wireless

CW=compat-wireless-2.6
CWREPO=git://git.kernel.org/pub/scm/linux/kernel/git/mcgrof/${CW}.git
CWDIR=${CDIR}/${CW}

WT=wireless-testing
WTREPO=git://git.kernel.org/pub/scm/linux/kernel/git/linville/{WT}.git
WTDIR=${CDIR}/${WT}

mkdir -p ${CDIR}

if [ ! -d ${CWDIR} ]
then
	pushd ${CDIR}
	git clone ${CWREPO}
	popd
fi

if [ ! -d ${WTDIR} ]
then
	pushd ${CDIR}
	git clone ${WTREPO}
	popd
fi

pushd ${WTDIR}
git checkout -f && git fetch origin && git rebase origin
git ls-files --others |xargs rm -rf
popd

pushd ${CWDIR}
git checkout -f && git fetch origin && git rebase origin
git ls-files --others |xargs rm -rf
GIT_TREE=${WTDIR} scripts/admin-update.sh
popd

rsync -av --delete --exclude=.git ${CWDIR}/ updates/compat-wireless-2.6
