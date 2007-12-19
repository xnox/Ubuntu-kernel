release := $(shell sed -n '1s/^.*(\(.*\)-.*).*$$/\1/p' debian/changelog)

pkgversion := $(shell sed -n '1s/^linux-ubuntu-modules-\([^ ]*\) .*/\1/p' debian/changelog)
revisions := $(shell sed -n 's/^linux-ubuntu-modules-$(pkgversion)\ .*($(release)-\(.*\)).*$$/\1/p' debian/changelog | tac)

revision ?= $(word $(words $(revisions)),$(revisions))
prev_revisions := $(filter-out $(revision),0.0 $(revisions))
prev_revision := $(word $(words $(prev_revisions)),$(prev_revisions))


ifneq ($(NOKERNLOG),)
ubuntu_log_opts += --no-kern-log
endif
ifneq ($(PRINTSHAS),)
ubuntu_log_opts += --print-shas
endif

abinum		:= $(shell echo $(revision) | sed -e 's/\..*//')$(abisuffix)
prev_abinum	:= $(shell echo $(prev_revision) | sed -e 's/\..*//')$(abisuffix)

arch		:= $(shell dpkg-architecture -qDEB_HOST_ARCH)
confdir		:= $(CURDIR)/debian/config
builddir	:= $(CURDIR)/debian/build
stampdir	:= $(CURDIR)/debian/stamps
udebdir		:= $(CURDIR)/debian/d-i-$(arch)

# Override KDIR if you want to use a non-standard kernel location.
# You really should use a kernel of the same version as is in changelog or complete
# horkage will ensue. KDIR is typically used on a buildd where you don't have
# privileges to install kernel headers, e.g., test builds after an ABI bump.
#
# You should also note that whatever kernel you point at must have at least
# had 'make silentoldconfig prepare scripts' run. Furthermore, you have to
# do it for each flavour. For example:
#
#	cd ~/ubuntu-hardy
#	cat debian/config/i386/config debian/config/i386/config.generic > .config
#	make silentoldconfig prepare scripts
#
#	cd ~/ubuntu-hardy-lum
#	fakeroot debian/rules binary-debs flavours=generic KDIR=~/ubuntu-hardy
#

KDIR		= /lib/modules/$(release)-$(abinum)-$(target_flavour)/build

# target_flavour is filled in for each step
kmake = make -C $(KDIR) \
	ARCH=$(build_arch_t) M=$(builddir)/build-$(target_flavour) \
	UBUNTU_FLAVOUR=$(target_flavour)

# Checks if a var is overriden by the custom rules. Called with var and
# flavour as arguments.
custom_override = \
 $(shell if [ -n "$($(1)_$(2))" ]; then echo "$($(1)_$(2))"; else echo "$($(1))"; fi)
