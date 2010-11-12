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

MIRROR_DIR=$WORKING_DIR/zinc-mirror
                               # This directory is mirrored onto zinc every so often.

git clone $KTEAM_REPO

RELEASE=natty
DESKTOP_AMD64=$RELEASE-desktop-amd64
DESKTOP_I386=$RELEASE-desktop-i386

# Fetch the latest daily isos we are going to customize. Note, wget is being
# used because this build is being done on a system in the DC and the network
# hose to where the daily-isos are is HUGE.
#
CONFIG_LIST="$DESKTOP_I386 $DESKTOP_AMD64"
for config in $CONFIG_LIST; do
    wget http://cdimage.ubuntu.com/daily-live/current/$config.iso
done

(
    cd kteam-tools/daily-test-isos

    # Fetch a copy of the kernel teams tests.
    #
    git clone git://kernel.ubuntu.com/manjo/kernel-qa.git
    rm -rf kernel-qa/.git    # This is removed to save space.
    rm kernel-qa/tests/video # FIXME (bjf): removed because of changes to maverick's
                             #              userspace apps which prevented the test
                             #              from working. Should be re-evaluated.

    CONFIG_LIST="$DESKTOP_I386 $DESKTOP_AMD64"
    for config in $CONFIG_LIST; do
        # Build the kernel team's standard, test isos
        #
        ./mk-custom-iso -i $WORKING_DIR/$config.iso
        cp /tmp/$config-custom.iso  $MIRROR_DIR/$config-ktts.iso

        # Build the krenel team's "firmware test suite" enabled test isos
        #
        ./mk-custom-iso -f -i $WORKING_DIR/$config.iso
        cp /tmp/$config-custom.iso  $MIRROR_DIR/$config-fwts.iso

        # Colin King wants img files created which can just be "dd'd" onto a flash drive
        # without having to use the "USB Startup disk creator". This is the code he developed
        # to do that.
        #

        # Get size of ISO image and add 64 MB of slack for fwts test results
        #
        iso=$MIRROR_DIR/$config-fwts.iso
        sz=`stat -c "%s" $iso`
        megs=$(((sz / (1024*1024)) + 64))
        isofile=`basename $iso`
        imgfile=$WORKING_DIR/${isofile%.*}.img

        # Create empty image
        #
        echo Making clean $megs MB USB stick image..
        dd if=/dev/zero of=$imgfile bs=1M count=$megs

        # Generate an image
        #
        if [ -e $WORKING_DIR/lucid-server.qcow2 ]; then
            echo Making bootable USB stick image..
            qemu -hda $WORKING_DIR/lucid-server.qcow2 -hdb $imgfile -cdrom $iso -m 1024 -vga none -nographic
            mv $imgfile $MIRROR_DIR/
        else
            echo **** Error: The virtual machine for creating the image file is missing!
        fi

        # Cleanup
        #
        rm $WORKING_DIR/$config.iso
    done
)


# Cleanup
#
rm -rf kteam-tools

# vi:set ts=4 sw=4 expandtab:
