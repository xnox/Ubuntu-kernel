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

COMPAT_KDIR=/lib/modules/$(release)-$(abinum)-$(target_flavour)
NET_BUILD_KERNEL=$(release)-$(abinum)-$(target_flavour)
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

	cat $(confdir)/$(arch) > $(builddir)/build-$*/.config
ifeq ($(do_net),true)
	cat $(confdir)/$(arch) > $(builddir)/build-$*/net/.config
endif
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
ifeq ($(do_net),true)
	BUILD_KERNEL=$(NET_BUILD_KERNEL) $(kmake) $(conc_level) obj=net modules
endif
	touch $@

# Install the finished build
install-%: csmoddir = $(cspkgdir)/lib/modules/$(release)-$(abinum)-$*
install-%: netpkgdir = $(CURDIR)/debian/linux-backports-modules-net-$(release)-$(abinum)-$*
install-%: netmoddir = $(netpkgdir)/lib/modules/$(release)-$(abinum)-$*
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
		find $${cwpkgdir}/ -type f -name \*.ko -print | xargs -r strip --strip-debug; \
\
		install -d $${cwpkgdir}/DEBIAN; \
		for script in postinst postrm; do                                       \
			sed -e 's/@@KVER@@/$(release)-$(abinum)-$(target_flavour)/g'                            \
				debian/control-scripts/$$script > $${cwpkgdir}/DEBIAN/$$script; \
			chmod 755 $${cwpkgdir}/DEBIAN/$$script;                                 \
		done; \
		install -d $${cwpkgdir}/lib/udev; \
		install --mode=0755 $${cwblddir}/udev/ubuntu/compat_firmware_$(abinum)_$(target_flavour).sh $${cwpkgdir}/lib/udev; \
		install -d $${cwpkgdir}/lib/udev/rules.d; \
		install --mode=0644 $${cwblddir}/udev/ubuntu/50-compat_firmware_$(abinum)_$(target_flavour).rules $${cwpkgdir}/lib/udev/rules.d; \
\
		install -d $${firmdir}; \
		if [ -d firmware/iwlwifi ] ; then cp firmware/iwlwifi/*/*.ucode $${firmdir}/; fi; \
	done

ifeq ($(do_net),true)
	#
	# Build the network package.
	#
	install -d $(netmoddir)/updates/net
	find $(builddir)/build-$*/net -type f -name '*.ko' | \
	while read f ; do \
		cp -v $${f} $(netmoddir)/updates/net/`basename $${f}`; \
	done

	find $(netpkgdir)/ -type f -name \*.ko -print | xargs -r strip --strip-debug

	install -d $(netpkgdir)/DEBIAN
	for script in postinst postrm; do					\
	  sed -e 's/@@KVER@@/$(release)-$(abinum)-$*/g'				\
	       debian/control-scripts/$$script > $(netpkgdir)/DEBIAN/$$script;	\
	  chmod 755 $(netpkgdir)/DEBIAN/$$script;					\
	done
endif

	#
	# The flavour specific headers package
	#
	install -d $(hdrdir)/include
ifeq ($(do_net),true)
	tar -C $(builddir)/build-$*/net -chf - include | tar -C $(hdrdir) -xf -
endif
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

packages-true =
packages-true += $(cw_pkg_list_suf)
packages-$(do_net) += linux-backports-modules-net-$(release)-$(abinum)-$*

binary-modules-%: install-%
	dh_testdir
	dh_testroot

	for i in $(packages-true) ; do \
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
