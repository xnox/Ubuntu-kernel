release := $(shell sed -n '1s/^.*(\(.*\)-.*).*$$/\1/p' debian/changelog)

pkgversion := $(shell sed -n '1s/^linux-ubuntu-modules-\([^ ]*\) .*/\1/p' debian/changelog)
revisions := $(shell sed -n 's/^linux-ubuntu-modules-$(pkgversion)\ .*($(release)-\(.*\)).*$$/\1/p' debian/changelog | tac)

revision ?= $(word $(words $(revisions)),$(revisions))
prev_revisions := $(filter-out $(revision),0.0 $(revisions))
prev_revision := $(word $(words $(prev_revisions)),$(prev_revisions))

# This is an internally used mechanism for the daily kernel builds. It
# creates packages who's ABI is suffixed with a minimal representation of
# the current git HEAD sha. If .git/HEAD is not present, then it uses the
# uuidgen program,
#
# AUTOBUILD can also be used by anyone wanting to build a custom kernel
# image, or rebuild the entire set of Ubuntu packages using custom patches
# or configs.
ppa_file    := $(CURDIR)/ppa_build_sha
is_ppa_build    := $(shell if [ -f $(ppa_file) ] ; then echo yes; fi;)
ifndef AUTOBUILD
AUTOBUILD   := $(is_ppa_build)
endif

ifneq ($(NOKERNLOG),)
ubuntu_log_opts += --no-kern-log
endif
ifneq ($(PRINTSHAS),)
ubuntu_log_opts += --print-shas
endif

abinum		:= $(shell echo $(revision) | sed -e 's/\..*//')$(abisuffix)
prev_abinum	:= $(shell echo $(prev_revision) | sed -e 's/\..*//')$(abisuffix)
debnum          := -$(abinum)

arch		:= $(shell dpkg-architecture -qDEB_HOST_ARCH)
confdir		:= $(CURDIR)/debian/config
builddir	:= $(CURDIR)/debian/build
stampdir	:= $(CURDIR)/debian/stamps
udebdir		:= $(CURDIR)/debian/d-i-$(arch)

#
# This is a way to support some external variables. A good example is
# a local setup for ccache and distcc See LOCAL_ENV_CC and
# LOCAL_ENV_DISTCC_HOSTS in the definition of kmake.
# For example:
#   LOCAL_ENV_CC="ccache distcc"
#   LOCAL_ENV_DISTCC_HOSTS="localhost 10.0.2.5 10.0.2.221"
#
local_env_file  := $(CURDIR)/../.hardy-env
have_local_env  := $(shell if [ -f $(local_env_file) ] ; then echo yes; fi;)
ifneq ($(have_local_env),)
include $(local_env_file)
endif

#
# Set this variable to 'true' in the arch makefile in order to
# avoid building udebs for the debian installer. see lpia.mk as
# an example of an architecture specific override.
#
disable_d_i    = no

# Support parallel=<n> in DEB_BUILD_OPTIONS
COMMA=,
ifneq (,$(filter parallel=%,$(subst $(COMMA), ,$(DEB_BUILD_OPTIONS))))
  CONCURRENCY_LEVEL := $(subst parallel=,,$(filter parallel=%,$(subst $(COMMA), ,$(DEB_BUILD_OPTIONS))))
endif

ifeq ($(CONCURRENCY_LEVEL),)
  # Check the environment
  CONCURRENCY_LEVEL := $(shell echo $$CONCURRENCY_LEVEL)
  # No? Check if this is on a buildd
  ifeq ($(CONCURRENCY_LEVEL),)
    ifneq ($(wildcard /CurrentlyBuilding),)
      CONCURRENCY_LEVEL := $(shell expr `getconf _NPROCESSORS_ONLN` \* 2)
    endif
  endif
  # Oh hell, give 'em one
  ifeq ($(CONCURRENCY_LEVEL),)
    CONCURRENCY_LEVEL := 1
  endif
endif

conc_level      = -j$(CONCURRENCY_LEVEL)

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
kmake = make -C $(KDIR)
kmake += ARCH=$(build_arch_t) M=$(builddir)/build-$(target_flavour)
kmake += UBUNTU_FLAVOUR=$(target_flavour)
ifneq ($(LOCAL_ENV_CC),)
kmake += CC=$(LOCAL_ENV_CC) DISTCC_HOSTS=$(LOCAL_ENV_DISTCC_HOSTS)
endif

# Checks if a var is overriden by the custom rules. Called with var and
# flavour as arguments.
custom_override = \
 $(shell if [ -n "$($(1)_$(2))" ]; then echo "$($(1)_$(2))"; else echo "$($(1))"; fi)
