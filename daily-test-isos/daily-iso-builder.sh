#!/bin/bash
set -e
git clone zinc.canonical.com:/srv/kernel.ubuntu.com/git/bradf/isodev

isodev/rsync-ubuntu-image cdimage/daily-live/current/maverick-desktop-amd64.iso
isodev/rsync-ubuntu-image cdimage/daily-live/current/maverick-desktop-i386.iso

(
    cd isodev
    git clone zinc.canonical.com:/srv/kernel.ubuntu.com/git/manjo/kernel-qa.git
    rm -rf kernel-qa/.git
    rm kernel-qa/tests/video
    ./mk-custom-iso -i /home/bradf/work/isos/ubuntu/cdimage/daily-live/current/maverick-desktop-i386.iso
    ./mk-custom-iso -i /home/bradf/work/isos/ubuntu/cdimage/daily-live/current/maverick-desktop-amd64.iso
)
cp /tmp/maverick-desktop-i386-custom.iso .
cp /tmp/maverick-desktop-amd64-custom.iso .
rm -rf isodev
