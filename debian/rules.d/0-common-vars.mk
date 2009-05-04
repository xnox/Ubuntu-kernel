# Rip some version information from our changelog
#  
gitver=$(shell if test -f .git/HEAD; then cat .git/HEAD; else uuidgen; fi)

pkgname           := $(shell dpkg-parsechangelog | awk '/^Source/{print $$2}')

abi_version        = $(shell dpkg-parsechangelog | grep ^Version | sed 's/.*-//;s/\..*//')
kernel_version     = $(shell dpkg-parsechangelog | grep ^Source | sed 's/.*-//')
kernel_abi_version = $(kernel_version)-$(abi_version)
lrm_version        = $(shell dpkg-parsechangelog | awk '/Version:/{print $$2}')
lrm_versions      := $(shell sed -n 's/^$(pkgname)\ .*(\(.*\)).*$$/\1/p' debian/changelog | tac)
prev_lrm_versions := $(filter-out $(lrm_version),0.0 $(lrm_versions))
prev_lrm_version  := $(word $(words $(prev_lrm_versions)),$(prev_lrm_versions))
