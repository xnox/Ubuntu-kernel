#!/bin/bash
set -e
if [ -e kteam-tools ];then
    rm -rf kteam-tools
fi

git clone git://kernel.ubuntu.com/ubuntu/kteam-tools.git

wget http://cdimage.ubuntu.com/daily-live/current/maverick-desktop-amd64.iso
wget http://cdimage.ubuntu.com/daily-live/current/maverick-desktop-i386.iso

(
    cd kteam-tools/daily-test-isos
    git clone git://kernel.ubuntu.com/manjo/kernel-qa.git
    rm -rf kernel-qa/.git
    rm kernel-qa/tests/video
    ./mk-custom-iso -i /home/bradf/work/maverick-desktop-i386.iso
    ./mk-custom-iso -i /home/bradf/work/maverick-desktop-amd64.iso
)
cp /tmp/maverick-desktop-i386-custom.iso  /home/bradf/work/zinc-mirror/
cp /tmp/maverick-desktop-amd64-custom.iso /home/bradf/work/zinc-mirror/
rm -rf kteam-tools
