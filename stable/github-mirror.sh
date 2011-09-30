#!/bin/bash

#
# This script is designed to mirror the Ubuntu git repositories
# onto github.com. It assumes its being run from the kernel-ppa
# account with the proper ssh keys.
#

RELEASES="hardy lucid maverick natty oneiric"
UBUNTU_GIT="git://kernel.ubuntu.com/ubuntu"
LREPO=linux
CANONICAL_REPO="git@github.com:Canonical-kernel/${LREPO}.git"
LINUS_REPO="git://github.com/torvalds/${LREPO}.git"

#
# Make the initial clone
#
if [ ! -d linux ]
then
	if ! git clone ${LINUS_REPO} ${LREPO}
	then
		echo Could not clone from ${LINUS_REPO}
		exit 1
	fi
	(cd ${LREPO}; git remote set-url origin ${CANONICAL_REPO})
fi

cd ${LREPO}

#
# Update from the origin
#
git checkout -f master
git clean -f -d; git ls-files --others --directory |xargs rm -rf
git fetch ${LINUS_REPO}
git fetch ${LINUS_REPO} master
git reset --hard FETCH_HEAD
git push origin +master

#
# Fetch each Ubuntu repository and push all of the branches.
#
for i in ${RELEASES}
do
	#
	# Get rid of the remote refs so that one need only fetch
	# the remote.
	#
	if git remote | grep $i ; then git remote rm $i; fi

	git remote add $i ${UBUNTU_GIT}/ubuntu-$i.git
	git fetch $i
	git fetch $i --tags
	for j in `git branch -a|egrep "remotes\/$i\/"`
	do
		git reset --hard refs/$j
		git clean -f -d; git ls-files --others --directory |xargs rm -rf
		git push origin +`echo $j|sed 's/remotes\///'`
	done
done

git push origin --tags -f
