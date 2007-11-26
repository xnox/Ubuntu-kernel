# 
# Makefile for out-of-tree iwlwifi project
#
# Portion of Makefile allowing out-of-tree module building is derived from
# code provided by Peter Johanson
#
# The rest of the Makefile is Copyright (C) 2007 Intel Corporation
#

export CONFIG_IWLWIFI = y
export CONFIG_IWL3945 ?= m
export CONFIG_IWL4965 ?= m
export CONFIG_IWLWIFI_SPECTRUM_MEASUREMENT ?= y
export CONFIG_IWLWIFI_DEBUG ?= y
export CONFIG_IWLWIFI_HT ?= y
export CONFIG_IWLWIFI_HT_AGG ?= y
export CONFIG_IWLWIFI_SENSITIVITY ?= y
export CONFIG_IWLWIFI_QOS ?= y
export IWLWIFI_VERSION ?= test

ifneq ('$(notdir $(if $(realpath /), $(realpath ${SHELL}), ${SHELL}))','bash')
$(warning )
$(warning WARNING: $$SHELL not set to bash.)
$(warning If you experience build errors, try)
$(warning 'make SHELL=/bin/bash'.)
$(warning )
endif

ifeq ($(KSRC),)
export KSRC_OUTPUT ?= /lib/modules/$(shell uname -r)/build
else
export KSRC_OUTPUT ?= $(KSRC)
endif
export KSRC ?= /lib/modules/$(shell uname -r)/source
export MAC80211_INC ?= $(KSRC)/include/net
export KMISC ?= /lib/modules/$(shell uname -r)/kernel/drivers/net/wireless
export KVER ?= $(shell uname -r)

# If a compatible/ directory exists then build from it, else build
# from the origin/ sources.
#
ifeq ($(DEVMODE),y)
DIR=origin/
else
DIR=compatible/
endif

ifeq ($(CONFIG_IWL4965),m)
	list-m += iwl4965
endif

ifeq ($(CONFIG_IWL3945),m)
	list-m += iwl3945
endif

ifeq ($(CONFIG_IWLWIFI_DEBUG),y)
	EXTRA_CFLAGS += -DCONFIG_IWLWIFI_DEBUG=y
endif

ifeq ($(CONFIG_IWLWIFI_SPECTRUM_MEASUREMENT),y)
	EXTRA_CFLAGS += -DCONFIG_IWLWIFI_SPECTRUM_MEASUREMENT=y
endif

ifeq ($(CONFIG_IWLWIFI_HT),y)
	EXTRA_CFLAGS += -DCONFIG_IWLWIFI_HT=y
endif
ifeq ($(CONFIG_IWLWIFI_HT_AGG),y)
	EXTRA_CFLAGS += -DCONFIG_IWLWIFI_HT_AGG=y
endif

ifeq ($(CONFIG_IWLWIFI_SENSITIVITY),y)
	 EXTRA_CFLAGS += -DCONFIG_IWLWIFI_SENSITIVITY=y
endif

ifeq ($(CONFIG_IWLWIFI_QOS),y)
	EXTRA_CFLAGS += -DCONFIG_IWLWIFI_QOS=y
endif

export EXTRA_CFLAGS

all:  modules utils 

utils:
	@[ ! -d util ] || make -C util

check_inc:

snapshot:
	scripts/make_snapshot

legacy:
	scripts/make_legacy

clean:
	@echo 'Removing stale, temporary, and built files...'
	@[ ! -d util ] || make -C util clean || true
	@rm -rf compatible
	@for i in origin .; do                      	       \
		rm -f $$i/*.{mod.c,mod,o,ko,rej,orig}          \
			$$i/.*.{stamp,flags,cmd}               \
			$$i/.*lst $$i/*.lst $$i/*~ $$i/.*.o.d  \
			$$i/Module.symvers ;                   \
	done
	@rm -rf tmp .tmp_versions compatible

codingstyle:
	@echo "Running indent on sources..."
	@for file in $$(egrep "([^#]*\.h)|([^#]*\.c)" FILES) ; do      \
		indent -npro -kr -i8 -ts8 -sob -l80 -ss -ncs "$$file"; \
		sed -i -e "s:\ *\$$::g" -e "s:\t*\$$::g" $$file;       \
	done

check:
	@echo "Checking sources for comment problems..."
	@scripts/check_comments origin/*.[ch]
	@echo "Checking sources for > 80 problems..."
	@scripts/check_80 origin/*.[ch]

whitespace:
	@echo "Cleaning label and trailing whitespace issues..."
	@scripts/clean_whitespace origin/*.[ch]
	@scripts/clean_labels origin/*.[ch]

dist:
	@scripts/make_snapshot $(IWLWIFI_VERSION)	

distclean: clean
	rm -f tags TAGS

sparse: check_inc source
	@echo "Warning: Compiling in Check Mode!!!"
	$(MAKE) C=2 CF="-Wall -D__CHECK_ENDIAN__" -C $(KSRC) O=$(KSRC_OUTPUT) M=$(PWD)/${DIR} modules

ifeq ($(DEVMODE),y)
modules:
	@echo "Warning: In DEVMOD: No compatability checks are performed!!!"
	$(MAKE) -C $(KSRC) O=$(KSRC_OUTPUT) M=$(PWD)/${DIR} EXTRA_CFLAGS="$(EXTRA_CFLAGS)" modules
else
modules: check_inc source
	$(MAKE) -C $(KSRC) O=$(KSRC_OUTPUT) M=$(PWD)/${DIR} EXTRA_CFLAGS="$(EXTRA_CFLAGS)" modules
endif

patch_kernel: source
	@scripts/patch_kernel $(KSRC)

install: $(addprefix $(DIR),$(addsuffix .ko,$(list-m)))
	@install -d $(KMISC)
	@install -m 644 -c $^ $(KMISC)
	@/sbin/depmod -a ${KVER}
	@echo -e "\nModule " $^ "(s) installed into:\n\t$(KMISC)"
	@echo -e "\nDon't forget to copy firmware to your hotplug's firmware directory "
	@echo "and have the hotplug tools in place."
	@echo -e "\nSee INSTALL for more information."

uninstall:
	rm -rf $(addprefix $(KMISC)/,$(addsuffix .ko,$(list-m)))
	/sbin/depmod -a ${KVER}

check_version: 
	@[ -e compatible/kversion ] && \
		[ "${KSRC}" != "$$(cat compatible/kversion)" ] && \
		echo -e \
"'compatible/' version does not match KSRC.  Try 'make clean'.\n" && \
exit 1 || exit 0

compatible/kversion: \
        $(shell find origin -type f)  \
        $(shell find patches -type f) \
        $(shell find scripts -type f)
	@[ -e compatible/kversion ] && \
		[ "${KSRC}" != "$$(cat compatible/kversion)" ] && \
		exit 1 || exit 0
	@scripts/generate_compatible origin; rc=$$?; \
	chmod a-w compatible/* -R; \
	echo ${KSRC} > compatible/kversion; \
	if [ $${rc} -eq 0 ]; then \
		exit 0; \
	else \
		echo -e "\nMakefile has been modified" \
			"by generate_compatible, please run \`make' again\n"; \
		exit 1; \
	fi


source: check_version compatible/kversion

.phony: check_inc check_valid

