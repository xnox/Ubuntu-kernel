# Configure machine to run the kernel-qa tests

# List of dependencies to run all tests
DEPS="dialog ffmpeg mplayer vorbis-tools alsa-utils acpidump dmidecode fwts"

# Check if apport-bug supports the --save option (Lucid+)
apport-bug --help | grep "\-\-save" > /dev/null 2>&1
if [ $? -eq "1" ]; then
	echo "Script can run only on Lucid (10.04) or greater releases"
	exit
fi

echo "Installing test dependencies. You will need to enter your password..."
sudo add-apt-repository ppa:firmware-testing-team/ppa-firmware-test-suite
sudo apt-get update --fix-missing
sudo apt-get install --yes $DEPS
