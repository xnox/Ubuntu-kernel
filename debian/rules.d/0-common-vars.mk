release := $(shell sed -n '1s/^.*(\(.*\)-.*).*$$/\1/p' debian/changelog)

pkgversion := $(shell sed -n '1s/^linux-backports-modules-\([^ ]*\) .*/\1/p' debian/changelog)
revisions := $(shell sed -n 's/^linux-backports-modules-$(pkgversion)\ .*($(release)-\(.*\)).*$$/\1/p' debian/changelog | tac)

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

ppa_file    := $(CURDIR)/ppa_build_sha
is_ppa_build    := $(shell if [ -f $(ppa_file) ] ; then echo yes; fi;)

#
# Specifically exclude these flavours from creating a sound include directory in
# the linux-headers-lum package.
#
no_compat_wireless_flavours="xen virtual"

#
# Explicitly enable compat-wireless at release time.
#
do_compat_wireless := 3.3 3.4 3.5
do_net=true

#
# Compat wireless versions for which packages are created.
#
CWDIRS=$(foreach ver,$(do_compat_wireless), cw-$(ver) )

# Support parallel=<n> in DEB_BUILD_OPTIONS (see #209008)
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
  # Default to hogging them all (and then some)
  ifeq ($(CONCURRENCY_LEVEL),)
    CONCURRENCY_LEVEL := $(shell expr `getconf _NPROCESSORS_ONLN` \* 2)
  endif
endif

conc_level		= -j$(CONCURRENCY_LEVEL)

# See ubuntu-hardy-lum debian/rules.d/0-common-vars.mk for an explanation
# of the use of this variable.

KDIR		= /lib/modules/$(release)-$(abinum)-$(target_flavour)/build

# target_flavour is filled in for each step
kmake = make -C $(KDIR) \
	ARCH=$(build_arch_t) M=$(builddir)/build-$(target_flavour) \
	UBUNTU_FLAVOUR=$(target_flavour)
ifneq ($(LOCAL_ENV_CC),)
kmake += CC=$(LOCAL_ENV_CC) DISTCC_HOSTS=$(LOCAL_ENV_DISTCC_HOSTS)
endif

# Checks if a var is overriden by the custom rules. Called with var and
# flavour as arguments.
custom_override = \
 $(shell if [ -n "$($(1)_$(2))" ]; then echo "$($(1)_$(2))"; else echo "$($(1))"; fi)
