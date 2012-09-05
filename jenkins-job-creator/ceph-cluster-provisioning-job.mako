<?xml version='1.0' encoding='UTF-8'?>
<project>
    <actions/>
    <description></description>
    <keepDependencies>false</keepDependencies>
    <properties/>
    <scm class="hudson.scm.NullSCM"/>
    <assignedNode>${data.vh_name}</assignedNode>
    <canRoam>false</canRoam>
    <disabled>false</disabled>
    <blockBuildWhenDownstreamBuilding>false</blockBuildWhenDownstreamBuilding>
    <blockBuildWhenUpstreamBuilding>false</blockBuildWhenUpstreamBuilding>
    <triggers class="vector"/>
    <concurrentBuild>false</concurrentBuild>
    <builders>
        <hudson.tasks.Shell>
            <command>
# On the virtual-host we clean up any old cruft. However, in the usual case the virtual-host
# was reprovisioned from scratch so there shouldn't be anything to cleanup.
#
cd /var/lib/jenkins
rm -rf kernel-testing
rsync -ar --exclude '.git' -e "ssh -o StrictHostKeyChecking=no" ${data.hw['jenkins server']}:kt.bjf/ ./kernel-testing/

# Instll the packages we require for creating VMs
#
sudo apt-get install -y qemu-kvm koan virt-manager

# We add the jenkins user to the libvirtd group so that we can run virt-manager
# if we need to. This is handy for debugging installation issues.
#
sudo sed -ie 's/^\(libvirtd.*\)/\1jenkins/' /etc/group

# Setup the bridged network for the VMs. This allows us to get to them from anywhere
# on the network.
#
export NIC=`grep 'inet dhcp' /etc/network/interfaces | sed 's/iface \(.*\) inet dhcp/\1/'`
echo &quot;\n\nauto br0\niface br0 inet dhcp\n        bridge_ports     $NIC\n        bridge_stp         off\n        bridge_fd            0\n        bridge_maxwait 0\n&quot; | sudo tee -a /etc/network/interfaces
cat /etc/network/interfaces
sudo ifup br0
ifconfig -a

export CEPH_NODE_LIST="ceph-node-0 ceph-node-1 ceph-node-2"

# Fixup the .ssh/config so that the nodes can ssh to teach other without
# passwords.
#
for NODE in $CEPH_NODE_LIST; do
    echo "\nHost $NODE\n  StrictHostKeyChecking no\n  UserKnownHostsFile=/dev/null\n\n" >> .ssh/config
done

for NODE in $CEPH_NODE_LIST; do
    # Provision a new VM
    #
    sudo koan --virt --server=${data.hw['orchestra server']} --profile=${data.sut_name} --virt-name=$NODE --virt-bridge=br0 --virt-path=/opt/$NODE-a --vm-poll

    # Wait for the new VM to come up and configure it's ssh so that we can do anything
    # we want with it.
    #
    set +e

    cd /var/lib/jenkins/kernel-testing

    # It's first going to come up with the sut_name. We then need to change the name and
    # reboot it and wait for the new name.
    #
    ./wait-for-system ${data.sut_name}
    ssh ${data.sut_name} sudo sed -i -e "s/${data.sut_name}/$NODE/" /etc/hostname
    ssh ${data.sut_name} sudo apt-get --yes install ceph ceph-mds ceph-common
    ssh ${data.sut_name} sudo reboot
    ./wait-for-system $NODE

    # Fix .ssh config on slave so it can copy from kernel-jenkins
    #
    scp -r /var/lib/jenkins/.ssh $NODE:
    ssh $NODE sudo scp -r .ssh /root/
done

# -------------------------------------------------------------------------------------
# This needs to be generalized
#
ssh ceph-node-0 sudo mkdir -p /var/lib/ceph/osd/ceph-0
ssh ceph-node-0 sudo mkdir -p /var/lib/ceph/mon/ceph-a
ssh ceph-node-0 sudo mkdir -p /var/lib/ceph/mds/ceph-a

ssh ceph-node-1 sudo mkdir -p /var/lib/ceph/osd/ceph-1
ssh ceph-node-1 sudo mkdir -p /var/lib/ceph/mon/ceph-b
ssh ceph-node-1 sudo mkdir -p /var/lib/ceph/mds/ceph-b

ssh ceph-node-2 sudo mkdir -p /var/lib/ceph/osd/ceph-2
ssh ceph-node-2 sudo mkdir -p /var/lib/ceph/mon/ceph-c
ssh ceph-node-2 sudo mkdir -p /var/lib/ceph/mds/ceph-c

/var/lib/jenkins/kernel-testing/jenkins-job-creator/cc $CEPH_NODE_LIST > ceph.conf

for NODE in $CEPH_NODE_LIST; do
    scp ceph.conf $NODE:
    ssh $NODE sudo mv ceph.conf /etc/ceph/ceph.conf
done

ssh ceph-node-0 sudo mkcephfs -a -c /etc/ceph/ceph.conf -k ceph.keyring

for NODE in $CEPH_NODE_LIST; do
    ssh $NODE sudo service ceph start
done

ssh ceph-node-0 sudo ceph health

            </command>
        </hudson.tasks.Shell>
    </builders>
    <publishers/>
    <buildWrappers/>
</project>
