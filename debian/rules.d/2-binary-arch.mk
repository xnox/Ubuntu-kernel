# We don't want make removing intermediary stamps
.SECONDARY :

build-arch: $(addprefix build-modules-,$(flavours))

# Prepare the out-of-tree build directory

printenv:
	@echo "confdir    : $(confdir)"
	@echo "stampdir   : $(stampdir)"

COMPAT_KDIR=/lib/modules/$(release)-$(abinum)-$(target_flavour)
make_compat = make $(conc_level) KLIB=$(COMPAT_KDIR) MADWIFI=
ifneq ($(LOCAL_ENV_CC),)
make_compat += CC=$(LOCAL_ENV_CC) DISTCC_HOSTS=$(LOCAL_ENV_DISTCC_HOSTS)
endif

prepare-%: $(stampdir)/stamp-prepare-%
	@# Empty for make to be happy
$(stampdir)/stamp-prepare-%: target_flavour = $*
$(stampdir)/stamp-prepare-%: $(confdir)/$(arch)
	@echo "Preparing $*..."
	install -d $(builddir)/build-$*
	cd updates; find . | cpio -dumpl $(builddir)/build-$*
	cd $(builddir)/build-$*/iwlwifi && ./MUNGE
	cd $(builddir)/build-$*/compat-wireless-2.6 && ./MUNGE
	cat $^ > $(builddir)/build-$*/.config
	# XXX: generate real config
	touch $(builddir)/build-$*/ubuntu-config.h
	touch $(builddir)/build-$*/ubuntu-build
	touch $@

# Do the actual build, including image and modules
build-modules-%: $(stampdir)/stamp-build-%
	@# Empty for make to be happy
$(stampdir)/stamp-build-%: target_flavour = $*
$(stampdir)/stamp-build-%: build_arch_t = $(call custom_override,build_arch,$*)
$(stampdir)/stamp-build-%: $(stampdir)/stamp-prepare-%
	@echo "Building $*..."
	if ! echo $(no_compat_wireless_flavours)|grep $(target_flavour) > /dev/null && grep 'CONFIG_COMPAT_WIRELESS=m' $(builddir)/build-$*/.config > /dev/null ; then \
		cd $(builddir)/build-$*/compat-wireless-2.6 && $(make_compat); \
 		if true ; then cd $(builddir)/build-$*/iwlwifi && $(make_compat); fi; \
	fi
	$(kmake) $(conc_level) modules
	@touch $@

# Install the finished build
install-%: pkgdir = $(CURDIR)/debian/linux-backports-modules-$(release)-$(abinum)-$*
install-%: moddir = $(pkgdir)/lib/modules/$(release)-$(abinum)-$*
install-%: firmdir = $(pkgdir)/lib/firmware/$(release)-$(abinum)-$*
install-%: basehdrpkg = linux-headers-lbm-$(release)$(debnum)
install-%: hdrpkg = $(basehdrpkg)-$*
install-%: hdrdir = $(CURDIR)/debian/$(hdrpkg)/usr/src/$(hdrpkg)
install-%: target_flavour = $*
install-%: $(stampdir)/stamp-build-%
	dh_testdir
	dh_testroot
	dh_clean -k -plinux-backports-modules-$(release)-$(abinum)-$*

	install -d $(firmdir)
	#
	# This firmware file name has to be consistent with IWL4965_UCODE_API in iwl4965-base.c
	#
	cp firmware/iwlwifi/*4965*/*.ucode $(firmdir)/iwlwifi-4965-2-lbm.ucode
	cp firmware/iwlwifi/*3945*/*.ucode $(firmdir)/iwlwifi-3945-1-lbm.ucode
	cp firmware/iwlwifi/*5000*/*.ucode $(firmdir)/iwlwifi-5000-1-lbm.ucode
	#
	# Preface each module name with lbm_ so that it doesn't conflict
	# with existing module names.
	#
	install -d $(moddir)/updates
	find $(builddir)/build-$* -type f -name '*.ko' | while read f ; do cp -v $${f} $(moddir)/updates/`basename $${f}`; done

ifeq ($(no_image_strip),)
	find $(pkgdir)/ -type f -name \*.ko -print | xargs -r strip --strip-debug
endif

	install -d $(pkgdir)/DEBIAN
	for script in postinst postrm; do					\
	  sed -e 's/@@KVER@@/$(release)-$(abinum)-$*/g'				\
	       debian/control-scripts/$$script > $(pkgdir)/DEBIAN/$$script;	\
	  chmod 755 $(pkgdir)/DEBIAN/$$script;					\
	done

	# The flavour specific headers package
	if [ -z "$(filter $(no_alsa_flavours),$(target_flavour))" ] && grep 'CONFIG_ALSA=m' $(builddir)/build-$*/.config > /dev/null ; then \
      install -d $(hdrdir)/sound; \
      cp `find $(builddir)/build-$*/sound/alsa-kernel/include -type f` $(hdrdir)/sound; \
    fi
	dh_testdir
	dh_testroot
	dh_installchangelogs -p$(hdrpkg)
	dh_installdocs -p$(hdrpkg)
	dh_compress -p$(hdrpkg)
	dh_fixperms -p$(hdrpkg)
	dh_installdeb -p$(hdrpkg)
	dh_gencontrol -p$(hdrpkg)
	dh_md5sums -p$(hdrpkg)
	dh_builddeb -p$(hdrpkg)

binary-modules-%: pkgimg = linux-backports-modules-$(release)-$(abinum)-$*
binary-modules-%: install-%
	dh_testdir
	dh_testroot

	dh_installchangelogs -p$(pkgimg)
	dh_installdocs -p$(pkgimg)
	dh_compress -p$(pkgimg)
	dh_fixperms -p$(pkgimg)
	dh_installdeb -p$(pkgimg)
	dh_gencontrol -p$(pkgimg)
	dh_md5sums -p$(pkgimg)
	dh_builddeb -p$(pkgimg) -- -Zbzip2 -z9

binary-debs: $(addprefix binary-modules-,$(flavours))
binary-arch: binary-debs binary-udebs
