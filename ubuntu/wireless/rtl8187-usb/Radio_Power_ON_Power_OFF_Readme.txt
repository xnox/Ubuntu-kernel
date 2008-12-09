
1. The Change For Deliverring Power State:

	Now we add the RadioPower.sh script in the driver root path. 

	When you run ./wlan0up, this script will be copied to /etc/acpi/events.

	And the driver can deliver the power state "RFON" or "RFOFF" into 

	/etc/acpi/events/RadioPower.sh from driver.

	So you can change this script based on the power state RFON or RFOFF.

2. For Example:

	Now the RadioPower.sh's content is: 
	
	if[ "$1" = ""RFON ]; then 
		echo "===================>Now Polling Method Turn RF ON!" > /etc/acpi/events/RadioPowerTest
	else
		echo "===================>Now Polling Method Turn RF OFF!" > /etc/acpi/events/RadioPowerTest
	fi

	So when you turn on RF using Polling Method, you can see "===================>>Now Polling Method Turn RF ON!" 
	using command: cat /etc/acpi/events/RadioPowerTest.

	And when you turn off RF using Polling Method, you can see "===================>>Now Polling Method Turn RF OFF!" 
	using command: cat /etc/acpi/events/RadioPowerTest.
