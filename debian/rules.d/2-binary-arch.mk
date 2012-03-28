# We don't want make removing intermediary stamps
.SECONDARY :

build-arch: $(addprefix build-modules-,$(flavours))

# Prepare the out-of-tree build directory

printenv:
	@echo "confdir           =  $(confdir)"
	@echo "stampdir          = $(stampdir)"
	@echo "release           = $(release)"
	@echo "revisions         = $(revisions)"
	@echo "revision          = $(revision)"
	@echo "prev_revisions    = $(prev_revisions)"
	@echo "prev_revision     = $(prev_revision)"
	@echo "abinum            = $(abinum)"
	@echo "flavours          = $(flavours)"
	@echo "do_nouveau_package   = $(do_nouveau_package)"

COMPAT_KDIR=/lib/modules/$(release)-$(abinum)-$(target_flavour)
make_compat = make $(conc_level)
make_compat += KLIB=$(COMPAT_KDIR) KLIB_BUILD=$(COMPAT_KDIR)/build
make_compat += MADWIFI=
make_compat += OLD_IWL=

ifneq ($(LOCAL_ENV_CC),)
make_compat += CC=$(LOCAL_ENV_CC) DISTCC_HOSTS=$(LOCAL_ENV_DISTCC_HOSTS)
endif

prepare-%: $(stampdir)/stamp-prepare-%
	@# Empty for make to be happy

$(stampdir)/stamp-prepare-%: target_flavour = $*
$(stampdir)/stamp-prepare-%: $(confdir)/$(arch)
	@echo "Preparing $*..."
	install -d $(builddir)/build-$*
	cd updates; tar cf - * | tar -C $(builddir)/build-$* -xf -
	if ls debian/patches/*.patch; then \
		for i in debian/patches/*.patch; do \
			patch -d $(builddir)/build-$* -p2 < $$i; \
		done; \
	fi
	#
	# compat-wireless preparation
	#
	# Gross hackery to make the compat firmware class unique to this ABI
	#
	for i in $(CWDIRS); do \
		cw_dir=$(builddir)/build-$*/$$i; \
		sed -i 's/compat_firmware/compat_firmware_'$(abinum)_$(target_flavour)'/g' \
			$${cw_dir}/compat/compat_firmware_class.c \
			$${cw_dir}/compat/scripts/compat_firmware_install \
			$${cw_dir}/udev/ubuntu/50-compat_firmware.rules; \
		mv -v $${cw_dir}/udev/ubuntu/50-compat_firmware.rules $${cw_dir}/udev/ubuntu/50-compat_firmware_$(abinum)_$(target_flavour).rules; \
		mv -v $${cw_dir}/udev/ubuntu/compat_firmware.sh $${cw_dir}/udev/ubuntu/compat_firmware_$(abinum)_$(target_flavour).sh; \
	done

ifeq ($(do_nouveau_package),true)
	$(builddir)/build-$*/MUNGE-NOUVEAU
	echo "obj-y += nouveau/" >>$(builddir)/build-$*/Makefile
endif
	cd $(builddir)/build-$*/alsa-driver && ./configure --with-kernel=$(COMPAT_KDIR)/build
	cat $(confdir)/$(arch) > $(builddir)/build-$*/.config
	# XXX: generate real config
	touch $(builddir)/build-$*/ubuntu-config.h
	touch $(builddir)/build-$*/ubuntu-build
	touch $@

# Do the actual build, including image and modules
build-modules-%: $(stampdir)/stamp-build-%
	@# Empty for make to be happy

$(stampdir)/stamp-build-%: target_flavour = $*
$(stampdir)/stamp-build-%: build_arch_t = $(call custom_override,build_arch,$*)
$(stampdir)/stamp-build-%: prepare-%
	@echo "Building $*..."
	for i in $(CWDIRS); do \
		cd $(builddir)/build-$*/$$i && $(make_compat); \
	done
	cd $(builddir)/build-$*/alsa-driver && make $(conc_level)
	cd $(builddir)/build-$*/wwan-drivers && make $(conc_level)
	$(kmake) $(conc_level) modules
	touch $@

# Install the finished build
install-%: impkgdir = $(CURDIR)/debian/linux-backports-modules-input-$(release)-$(abinum)-$*
install-%: immoddir = $(impkgdir)/lib/modules/$(release)-$(abinum)-$*
install-%: wwpkgdir = $(CURDIR)/debian/linux-backports-modules-wwan-$(release)-$(abinum)-$*
install-%: wwmoddir = $(wwpkgdir)/lib/modules/$(release)-$(abinum)-$*
install-%: wwsrcdir = $(CURDIR)/updates/wwan-drivers
install-%: cspkgdir = $(CURDIR)/debian/linux-backports-modules-alsa-$(release)-$(abinum)-$*
install-%: csmoddir = $(cspkgdir)/lib/modules/$(release)-$(abinum)-$*
install-%: nvpkgdir = $(CURDIR)/debian/linux-backports-modules-nouveau-$(release)-$(abinum)-$*
install-%: nvmoddir = $(nvpkgdir)/lib/modules/$(release)-$(abinum)-$*
install-%: lbmbasehdrpkg = linux-headers-lbm-$(release)$(debnum)
install-%: lbmhdrpkg = $(lbmbasehdrpkg)-$*
install-%: hdrdir = $(CURDIR)/debian/$(lbmhdrpkg)/usr/src/$(lbmhdrpkg)
install-%: target_flavour = $*
install-%: build-modules-%
	dh_testdir
	dh_testroot

	for i in $(CWDIRS); do \
		cw=$$i; \
		cwpkgdir=$(CURDIR)/debian/linux-backports-modules-$${cw}-$(release)-$(abinum)-$(target_flavour); \
		cwblddir=$(builddir)/build-$(target_flavour)/$${cw}; \
		cwmoddir=$${cwpkgdir}/lib/modules/$(release)-$(abinum)-$(target_flavour)/updates; \
		firmdir=$${cwpkgdir}/lib/firmware/updates/$(release)-$(abinum)-$(target_flavour); \
\
		dh_clean -k -plinux-backports-modules-$${cw}-$(release)-$(abinum)-$(target_flavour); \
\
		install -d $${cwmoddir}/$${cw}; \
		find $(builddir)/build-$(target_flavour)/$${cw} -type f -name '*.ko' | \
		while read f ; do \
			cp -v $${f} $${cwmoddir}/$${cw}/`basename $${f}`; \
		done; \
\
		install -d $${cwmoddir}/$${cw}-staging; \
		find $(builddir)/build-$(target_flavour)/wireless-staging -type f -name '*.ko' | \
		while read f ; do \
			cp -v $${f} $${cwmoddir}/$${cw}-staging/`basename $${f}`; \
		done; \
\
		find $${cwpkgdir}/ -type f -name \*.ko -print | xargs -r strip --strip-debug; \
\
		install -d $${cwpkgdir}/DEBIAN; \
		for script in postinst postrm; do					\
	  		sed -e 's/@@KVER@@/$(release)-$(abinum)-$(target_flavour)/g'				\
	       			debian/control-scripts/$$script > $${cwpkgdir}/DEBIAN/$$script;	\
	  		chmod 755 $${cwpkgdir}/DEBIAN/$$script;					\
		done; \
\
		install -d $${cwpkgdir}/lib/udev; \
		install --mode=0755 $${cwblddir}/udev/ubuntu/compat_firmware_$(abinum)_$(target_flavour).sh $${cwpkgdir}/lib/udev; \
		install -d $${cwpkgdir}/lib/udev/rules.d; \
		install --mode=0644 $${cwblddir}/udev/ubuntu/50-compat_firmware_$(abinum)_$(target_flavour).rules $${cwpkgdir}/lib/udev/rules.d; \
\
		install -d $${firmdir}; \
		cp firmware/iwlwifi/*.ucode $${firmdir}/; \
		cp firmware/rt28x0/rt2870.bin $${firmdir}/; \
	done

	#
	# Build the ALSA snapshot packages.
	#
	install -d $(csmoddir)/updates/alsa
	find $(builddir)/build-$*/alsa-driver -type f -name '*.ko' | while read f ; do cp -v $${f} $(csmoddir)/updates/alsa/`basename $${f}`; done

	find $(cspkgdir)/ -type f -name \*.ko -print | xargs -r strip --strip-debug
	# This platform driver needs to be included as it links to ALSA
	cp $(builddir)/build-$*/thinkpad-acpi/thinkpad_acpi.ko \
		$(csmoddir)/updates/alsa
	strip --strip-debug $(csmoddir)/updates/alsa/thinkpad_acpi.ko

	install -d $(cspkgdir)/DEBIAN
	for script in postinst postrm; do					\
	  sed -e 's/@@KVER@@/$(release)-$(abinum)-$*/g'				\
	       debian/control-scripts/$$script > $(cspkgdir)/DEBIAN/$$script;	\
	  chmod 755 $(cspkgdir)/DEBIAN/$$script;					\
	done

	#
	# Build the NOUVEAU snapshot packages.
	#
ifeq ($(do_nouveau_package),true)
	install -d $(nvmoddir)/updates/nouveau
	find $(builddir)/build-$*/nouveau -type f -name '*.ko' | while read f ; do cp -v $${f} $(nvmoddir)/updates/nouveau/`basename $${f}`; done

	find $(nvpkgdir)/ -type f -name \*.ko -print | xargs -r strip --strip-debug

	install -d $(nvpkgdir)/DEBIAN
	for script in postinst postrm; do					\
	  sed -e 's/@@KVER@@/$(release)-$(abinum)-$*/g'				\
	       debian/control-scripts/$$script > $(nvpkgdir)/DEBIAN/$$script;	\
	  chmod 755 $(nvpkgdir)/DEBIAN/$$script;					\
	done
endif

	#
	# Build the wwan-drivers packages.
	#
	install -d $(wwmoddir)/updates/wwan
	find $(builddir)/build-$*/wwan-drivers -type f -name '*.ko' |	       \
	while read f; do						       \
		install -v $$f $(wwmoddir)/updates/wwan/;		       \
		strip --strip-debug $(wwmoddir)/updates/wwan/$$(basename $$f); \
	done
	$(MAKE) -C $(wwsrcdir) prefix=$(wwpkgdir) install
	install -d $(wwpkgdir)/DEBIAN

	#
	# Hack to make the udev rule and firmware loader specific to the
	# package.
	#
	POSTFIX="$(abinum)-$(target_flavour)"				\
	R='PROGRAM="\/bin\/uname -r",';					\
	R=$$R' RESULT!="'$(release)-$$POSTFIX'",';			\
	R=$$R' GOTO="gobi_rules_end"';					\
	sed -i -e "s/^\(LABEL=\"gobi_rules\"\)$$/\0\n$$R\n/"		\
	    -e "s/\(gobi_loader\)/\0-$$POSTFIX/"			\
	    $(wwpkgdir)/lib/udev/rules.d/60-gobi.rules

	TGTFILE="$(wwpkgdir)/lib/udev/rules.d/60-gobi";			\
	chmod 644 $${TGTFILE}.rules;					\
	mv $${TGTFILE}.rules $${TGTFILE}-$(abinum)-$(target_flavour).rules

	TGTFILE="$(wwpkgdir)/lib/udev/gobi_loader";			\
	mv $$TGTFILE $$TGTFILE-$(abinum)-$(target_flavour)

	for script in postinst postrm; do				       \
		sed -e 's/@@KVER@@/$(release)-$(abinum)-$*/g'		       \
			debian/control-scripts/$$script			       \
			>$(wwpkgdir)/DEBIAN/$$script;			       \
		chmod 755 $(wwpkgdir)/DEBIAN/$$script;			       \
	done
	
	#
	# Build the input-drivers package.
	#
	install -d $(immoddir)/updates/input
	find $(builddir)/build-$*/input-drivers -type f -name '*.ko' |	       \
	while read f; do						       \
		install -v $$f $(immoddir)/updates/input/;		       \
		strip --strip-debug $(immoddir)/updates/input/$$(basename $$f);\
	done
	#$(MAKE) -C $(imsrcdir) prefix=$(impkgdir) install
	install -d $(impkgdir)/DEBIAN

	#
	# The flavour specific headers package
	#
	install -d $(hdrdir)/include
	for i in asm linux media sound; do \
		tar -C $(builddir)/build-$*/alsa-driver/include -chf - $$i | tar -C $(hdrdir)/include -xf -; \
	done

	dh_testdir
	dh_testroot
	dh_installchangelogs -p$(lbmhdrpkg)
	dh_installdocs -p$(lbmhdrpkg)
	dh_compress -p$(lbmhdrpkg)
	dh_fixperms -p$(lbmhdrpkg)
	dh_installdeb -p$(lbmhdrpkg)
	dh_gencontrol -p$(lbmhdrpkg)
	dh_md5sums -p$(lbmhdrpkg)
	dh_builddeb -p$(lbmhdrpkg)

cw_pkg_list_pre = $(addprefix linux-backports-modules-,$(CWDIRS))
cw_pkg_list_suf = $(addsuffix -$(release)-$(abinum)-$*,$(cw_pkg_list_pre))

package_list =
package_list += $(cw_pkg_list_suf)
package_list += linux-backports-modules-alsa-$(release)-$(abinum)-$*
ifeq ($(do_nouveau_package),true)
package_list += linux-backports-modules-nouveau-$(release)-$(abinum)-$*
endif
package_list += linux-backports-modules-wwan-$(release)-$(abinum)-$*
package_list += linux-backports-modules-input-$(release)-$(abinum)-$*

binary-modules-%: install-%
	dh_testdir
	dh_testroot

	for i in $(package_list) ; do \
	dh_installchangelogs -p$$i; \
	dh_installdocs -p$$i; \
	dh_compress -p$$i; \
	dh_fixperms -p$$i; \
	dh_installdeb -p$$i; \
	dh_gencontrol -p$$i; \
	dh_md5sums -p$$i; \
	dh_builddeb -p$$i -- -Zbzip2 -z9; \
	done

binary-debs: $(addprefix binary-modules-,$(flavours))
binary-arch: binary-debs binary-udebs
