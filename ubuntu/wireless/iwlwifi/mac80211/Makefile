export KSRC ?= /lib/modules/$(shell uname -r)/source/
export BASE ?= /home/repos/wireless-dev/
export MAC80211_VERSION ?= test
origin_files = $(shell [ -d origin ] && find origin -type f -not -name '*~' | sed -e 's,:,\\:,g')
pending_files = $(shell [ -d pending ] && find pending -type f -not -name '*~' | sed -e 's,:,\\:,g')
patch_files = $(shell [ -d patches ] && find patches -type f -not -name '*~' | sed -e 's,:,\\:,g')
stub_files = $(shell [ -d stubs ] && find stubs -type f -not -name '*~' | sed -e 's,:,\\:,g')
script_files = $(shell [ -d scripts ] && find scripts -type f -not -name '*~' | sed -e 's,:,\\:,g')

all: check_shell source

check_shell:
ifneq ('$(notdir $(realpath ${SHELL}))','bash')
$(info )
$(info WARNING: $$SHELL not set to bash.)
$(info )
$(info If you experience build errors, try 'make SHELL=/bin/bash'.)
$(info )
endif

check_version: 
	@[ -e compatible/kversion ] && \
		 [ "${KSRC}" != "$$(cat compatible/kversion)" ] && \
		 echo -e \
"'compatible/' version does not match KSRC.  Try 'make clean'.\n" && \
exit 1 || exit 0

modified: ${origin_files} ${pending_files} 
	@scripts/generate_modified
	@touch modified

compatible/modified: modified ${patch_files}
	@scripts/generate_compatible modified
	@echo ${KSRC} > compatible/kversion
	@touch compatible/modified

compatible/unmodified: ${origin_files} ${pending_files}
	@scripts/generate_compatible origin
	@echo ${KSRC} > compatible/kversion
	@touch compatible/unmodified

unmodified: check_version compatible/unmodified

source: check_version compatible/modified

source_unmodified: check_version compatible/unmodified

update: origin
origin: 
	@scripts/generate_origin


patch_kernel: source
	@scripts/patch_kernel
	@scripts/patch_kernel_post

patch_kernel_unmodified: source_unmodified
	@scripts/patch_kernel

README: stubs/README
	@source $< > $@

distros/mac80211-${MAC80211_VERSION}.tgz: \
	${origin_files} ${pending_files} ${patch_files} \
	${stub_files} ${script_files} \
	README TODO CHANGES FILES ORIGIN_FILES \
	Makefile 
	@[ -d distros ] || mkdir distros
	@. stubs/README > README
	@tar -c -z \
		--transform 's,^,mac80211-${MAC80211_VERSION}/,' \
		-f $@ $^
	@echo "Don't forget to commit and run cg-tag mac80211-${MAC80211_VERSION}."

dist: distros/mac80211-${MAC80211_VERSION}.tgz

clean:
	@echo "Removing staging and built files."
	@find . -name '*ko' -exec rm {} \;	
	@find . -name '*.o' -exec rm {} \;	
	@find . -name '*.mod.c' -exec rm {} \;
	@rm -rf compatible modified

.phony: check_shell check_valid source source_unmodified patch_kernel \
	patch_kernel_unmodified all clean update dist origin 
