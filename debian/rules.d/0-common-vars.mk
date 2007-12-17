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

arch		:= $(shell dpkg-architecture -qDEB_HOST_ARCH)
confdir		:= $(CURDIR)/debian/config
builddir	:= $(CURDIR)/debian/build
stampdir	:= $(CURDIR)/debian/stamps
udebdir		:= $(CURDIR)/debian/d-i-$(arch)

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
  # Oh hell, give 'em one
  ifeq ($(CONCURRENCY_LEVEL),)
    CONCURRENCY_LEVEL := 1
  endif
endif

conc_level		= -j$(CONCURRENCY_LEVEL)

# target_flavour is filled in for each step
kmake = make -C /lib/modules/$(release)-$(abinum)-$(target_flavour)/build \
	ARCH=$(build_arch_t) M=$(builddir)/build-$(target_flavour) \
	UBUNTU_FLAVOUR=$(target_flavour)

# Checks if a var is overriden by the custom rules. Called with var and
# flavour as arguments.
custom_override = \
 $(shell if [ -n "$($(1)_$(2))" ]; then echo "$($(1)_$(2))"; else echo "$($(1))"; fi)
