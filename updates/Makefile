#
# Makefile for Ubuntu additional drivers
#

include $(src)/.config

#
# Declare to have additional module symbol versions.
# Must be here as it does not seem to have effect from a sub-directory.
#
KBUILD_EXTMOD	:= $(src)/alsa-driver

obj-y	+= wireless-staging/
obj-y	+= thinkpad-acpi/
obj-y	+= wwan-drivers/
obj-y	+= input-drivers/
