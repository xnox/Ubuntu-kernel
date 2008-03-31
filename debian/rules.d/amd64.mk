build_arch	= x86_64
flavours	= generic
#
# No custom binaries for the PPA build.
#
ifeq ($(is_ppa_build),)
flavours	= server rt xen openvz
endif
