#
# Enable all the fake config variables for UWB and company
#

vars="CONFIG_WIMAX CONFIG_WIMAX_I2400M"

for var in $vars
do
    echo ".tmp.config.mk:export $var := m"
    echo "include/config.h:#define $var 1"
done
