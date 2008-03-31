build_arch	= i386
flavours	= 386 generic
#
# No custom binaries for the PPA build.
#
ifeq ($(is_ppa_build),)
flavours	+= server rt virtual xen openvz
endif
