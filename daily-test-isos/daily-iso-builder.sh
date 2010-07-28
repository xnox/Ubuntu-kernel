#!/bin/bash
#
# This is the script that builds the isos every day.
#
set -e

BASE=/home/bradf               # Home directory of the user that is building the
                               # isos.

REPOS=$BASE/repos              # Location of any git trees that are needed to build
                               # the isos.

KTEAM_REPO=$REPOS/kteam-tools  # This is the 'authoritative' copy of the kteam-tools.
                               # It is only updated after careful review of the latest
                               # changes.

WORKING_DIR=$BASE/work         # The directory where the 'work' of building the isos
                               # is done. This directory also contains the directory
                               # that is mirrored onto zinc.

git clone $KTEAM_REPO

RELEASE=maverick
DESKTOP_AMD64=$RELEASE-desktop-amd64
DESKTOP_I386=$RELEASE-desktop-i386

# Fetch the latest daily isos we are going to customize. Note, wget is being
# used because this build is being done on a system in the DC and the network
# hose to where the daily-isos are is HUGE.
#
wget http://cdimage.ubuntu.com/daily-live/current/$DESKTOP_AMD64.iso
wget http://cdimage.ubuntu.com/daily-live/current/$DESKTOP_I386.iso

(
    cd kteam-tools/daily-test-isos

    # Fetch a copy of the kernel teams tests.
    #
    git clone git://kernel.ubuntu.com/manjo/kernel-qa.git
    rm -rf kernel-qa/.git    # This is removed to save space.
    rm kernel-qa/tests/video # FIXME (bjf): removed because of changes to maverick's 
                             #              userspace apps which prevented the test
                             #              from working. Should be re-evaluated.

    # Build the kernel team's standard, test isos
    #
    ./mk-custom-iso -i $WORKING_DIR/$DESKTOP_I386.iso
    ./mk-custom-iso -i $WORKING_DIR/$DESKTOP_AMD64.iso

    cp /tmp/$DESKTOP_I386-custom.iso  $WORKING_DIR/zinc-mirror/$DESKTOP_I386-ktts.iso
    cp /tmp/$DESKTOP_AMD64-custom.iso $WORKING_DIR/zinc-mirror/$DESKTOP_AMD64-ktts.iso

    # Build the krenel team's "firmware test suite" enabled test isos
    #
    ./mk-custom-iso -f -i $WORKING_DIR/$DESKTOP_I386.iso
    ./mk-custom-iso -f -i $WORKING_DIR/$DESKTOP_AMD64.iso

    cp /tmp/$DESKTOP_I386-custom.iso  $WORKING_DIR/zinc-mirror/$DESKTOP_I386-fwts.iso
    cp /tmp/$DESKTOP_AMD64-custom.iso $WORKING_DIR/zinc-mirror/$DESKTOP_AMD64-fwts.iso
)


# Cleanup
#
rm -rf kteam-tools

# vi:set ts=4 sw=4 expandtab:
