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

kernel-testing/jenkins-job-creator/ceph-configurator --metadata-servers=3 --objectstore-servers=3 --monitor-servers=3

cd ceph-config
for NODE in *; do
    # Provision a new VM
    #
    sudo koan --virt --server=${data.hw['orchestra server']} --profile=${data.sut_name} --virt-name=$NODE --virt-bridge=br0 --virt-path=/opt/$NODE-a --vm-poll

    # Wait for the new VM to come up and configure it's ssh so that we can do anything
    # we want with it.
    #
    set +e

    ssh-keygen -f &quot;/var/lib/jenkins/.ssh/known_hosts&quot; -R $NODE

    cd /var/lib/jenkins/kernel-testing

    # It's first going to come up with the sut_name. We then need to change the name and
    # reboot it and wait for the new name.
    #
    ./wait-for-system ${data.sut_name}
    ssh -o StrictHostKeyChecking=no ${data.sut_name} sudo sed -i -e "s/${data.sut_name}/$NODE/"
    ssh -o StrictHostKeyChecking=no ${data.sut_name} sudo reboot
    ./wait-for-system $NODE

    # Fix .ssh config on slave so it can copy from kernel-jenkins
    #
    scp -o StrictHostKeyChecking=no -r /var/lib/jenkins/.ssh $NODE:
done
            </command>
        </hudson.tasks.Shell>
    </builders>
    <publishers/>
    <buildWrappers/>
</project>
