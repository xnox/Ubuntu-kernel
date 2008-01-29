#!/usr/bin/make -f
# Makefile for building the aufs module in Debian

# Parts from conf.mk
# Configuration for aufs in Debian
export CONFIG_AUFS                = m
export CONFIG_AUFS_BRANCH_MAX_127 = y
export CONFIG_AUFS_RR_SQUASHFS    = y
export EXTRA_CFLAGS               = -I $(src)/include \
                                    -DCONFIG_AUFS_BRANCH_MAX_127  -UCONFIG_AUFS \
                                    -DCONFIG_AUFS_MODULE \
                                    -DCONFIG_AUFS_RR_SQUASHFS
# Disable sysaufs on ARM, as there is no cmpxchg
ifneq ($(shell dpkg-architecture -qDEB_BUILD_ARCH),arm)
export CONFIG_AUFS_SYSAUFS  = y
export EXTRA_CFLAGS        += -DCONFIG_AUFS_SYSAUFS
endif

ifneq (,$(KSRC))
# see for ksize patch
ifneq (,$(shell grep '^.*[[:space:]]ksize[[:space:]]vmlinux[[:space:]]EXPORT_SYMBOL' $(KSRC)/Module.symvers))
export CONFIG_AUFS_KSIZE_PATCH = y
export EXTRA_CFLAGS        += -DCONFIG_AUFS_KSIZE_PATCH
endif
# see for lhash patch
ifneq (,$(shell grep '^.*[[:space:]]__lookup_hash[[:space:]]vmlinux[[:space:]]EXPORT_SYMBOL' $(KSRC)/Module.symvers))
export CONFIG_AUFS_LHASH_PATCH  = y
DISABLE_FAKE_DM=y
export EXTRA_CFLAGS        += -DCONFIG_AUFS_LHASH_PATCH
endif
# splice patch
ifneq (,$(shell grep '^.*[[:space:]]do_splice_from[[:space:]]vmlinux[[:space:]]EXPORT_SYMBOL' $(KSRC)/Module.symvers))
export CONFIG_AUFS_SPLICE_PATCH = y
export EXTRA_CFLAGS        += -DCONFIG_AUFS_SPLICE_PATCH
endif
# sysfs_get_dentry patch
ifneq (,$(shell grep '^.*[[:space:]]sysfs_get_dentry[[:space:]]vmlinux[[:space:]]EXPORT_SYMBOL' $(KSRC)/Module.symvers))
export CONFIG_AUFS_SYSFS_GET_DENTRY_PATCH = y
export EXTRA_CFLAGS        += -DCONFIG_AUFS_SYSFS_GET_DENTRY_PATCH
endif
# put_filp patch
ifneq (,$(shell grep '^.*[[:space:]]put_filp[[:space:]]vmlinux[[:space:]]EXPORT_SYMBOL' $(KSRC)/Module.symvers))
export CONFIG_AUFS_PUT_FLIP_PATCH = y
export EXTRA_CFLAGS        += -DCONFIG_AUFS_PUT_FLIP_PATCH
DISABLE_FAKE_DM=y
endif
# Check for sec_perm patch
ifneq (,$(shell grep '^.*[[:space:]]security_inode_permission[[:space:]]vmlinux[[:space:]]EXPORT_SYMBOL' $(KSRC)/Module.symvers))
export CONFIG_AUFS_SEC_PERM_PATCH = y
export EXTRA_CFLAGS        += -DCONFIG_AUFS_SEC_PERM_PATCH
endif
endif
# If lhash and put_filp are not available, activate FAKE_DM.
ifndef DISABLE_FAKE_DM
export CONFIG_AUFS_FAKE_DM  = y
export EXTRA_CFLAGS        += -DCONFIG_AUFS_FAKE_DM
endif

## INCLUDE UPSTREAM MAKEFILE
include $(src)/Makefile.upstream
