# We don't want make removing intermediary stamps
.SECONDARY :

build-arch: $(addprefix build-modules-,$(flavours))

# Prepare the out-of-tree build directory

printenv:
	@echo "confdir    : $(confdir)"
	@echo "stampdir   : $(stampdir)"

prepare-%: $(stampdir)/stamp-prepare-%
	@# Empty for make to be happy
$(stampdir)/stamp-prepare-%: target_flavour = $*
$(stampdir)/stamp-prepare-%: $(confdir)/$(arch)
	@echo "Preparing $*..."
	install -d $(builddir)/build-$*
	cd ubuntu; find . | cpio -dumpl $(builddir)/build-$*
	cat $^ > $(builddir)/build-$*/.config
	# XXX: generate real config
	touch $(builddir)/build-$*/ubuntu-config.h
	touch $(builddir)/build-$*/ubuntu-build
	echo filtered target_flavour $(filter xen virtual,$(target_flavour))
	if [ -z "$(filter xen virtual,$(target_flavour))" ] && grep 'CONFIG_ALSA=m' $(builddir)/build-$*/.config > /dev/null ; then \
	  cd $(builddir)/build-$*/sound/alsa-driver && make SND_TOPDIR=`pwd` all-deps; \
	  cd $(builddir)/build-$*/sound/alsa-driver && aclocal && autoconf; \
	  cd $(builddir)/build-$*/sound/alsa-driver && ./configure --with-kernel=$(KDIR); \
	  sed -i 's/CONFIG_SND_S3C2412_SOC_I2S=m/CONFIG_SND_S3C2412_SOC_I2S=/' $(builddir)/build-$*/sound/alsa-driver/toplevel.config; \
	  cd $(builddir)/build-$*/sound/alsa-driver && make SND_TOPDIR=`pwd` dep; \
	fi
	touch $@

# Do the actual build, including image and modules
build-modules-%: $(stampdir)/stamp-build-%
	@# Empty for make to be happy
$(stampdir)/stamp-build-%: target_flavour = $*
$(stampdir)/stamp-build-%: build_arch_t = $(call custom_override,build_arch,$*)
$(stampdir)/stamp-build-%: $(stampdir)/stamp-prepare-%
	@echo "Building $*..."
	$(kmake) $(conc_level) modules
	@touch $@

# Install the finished build
install-%: pkgdir = $(CURDIR)/debian/linux-ubuntu-modules-$(release)-$(abinum)-$*
install-%: moddir = $(pkgdir)/lib/modules/$(release)-$(abinum)-$*
install-%: firmdir = $(pkgdir)/lib/firmware/$(release)-$(abinum)-$*
install-%: hdrpkg = linux-headers-lum-$(release)-$(abinum)-$*
install-%: hdrdir = $(CURDIR)/debian/$(hdrpkg)/usr/src/$(hdrpkg)
install-%: target_flavour = $*
install-%: $(stampdir)/stamp-build-%
	dh_testdir
	dh_testroot
	dh_clean -k -plinux-ubuntu-modules-$(release)-$(abinum)-$*
	dh_clean -k -p$(hdrpkg)

	install -d $(firmdir)
	cp -a ubuntu-firmware/zd1211 $(firmdir)/
	for i in ubuntu-firmware/*/[[:lower:]]*; do \
	  case $${i##*/} in \
	    zd121*) ;; \
	    *) cp $$i "$(firmdir)/${i##*/}";; \
	  esac; \
	done

	install -d $(moddir)/ubuntu
	cd $(builddir)/build-$*; find . -name '*.ko' | \
		cpio -dumpl $(moddir)/ubuntu

ifeq ($(no_image_strip),)
	find $(pkgdir)/ -name \*.ko -print | while read f ; do strip --strip-debug "$$f"; done
endif

	install -d $(pkgdir)/DEBIAN
	for script in postinst postrm; do					\
	  sed -e 's/@@KVER@@/$(release)-$(abinum)-$*/g'				\
	       debian/control-scripts/$$script > $(pkgdir)/DEBIAN/$$script;	\
	  chmod 755 $(pkgdir)/DEBIAN/$$script;					\
	done

	# The flavour specific headers package
	install -d $(hdrdir)/sound
	cp  $(builddir)/build-$*/sound/alsa-driver/include/config{,1}.h  $(hdrdir)/sound
	cp `find $(builddir)/build-$*/sound/alsa-kernel/include -type f` $(hdrdir)/sound
	# WiMAX headers
	install -d $(hdrdir)/include/linux $(hdrdir)/include/net
	cp `find $(builddir)/build-$*/wireless/wimax-i2400m/include/linux -type f` $(hdrdir)/include/linux
	cp `find $(builddir)/build-$*/wireless/wimax-i2400m/include/net -type f` $(hdrdir)/include/net

binary-modules-%: pkgimg = linux-ubuntu-modules-$(release)-$(abinum)-$*
binary-modules-%: hdrpkg = linux-headers-lum-$(release)-$(abinum)-$*
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

	dh_installchangelogs -p$(hdrpkg)
	dh_installdocs -p$(hdrpkg)
	dh_compress -p$(hdrpkg)
	dh_fixperms -p$(hdrpkg)
	dh_installdeb -p$(hdrpkg)
	dh_gencontrol -p$(hdrpkg)
	dh_md5sums -p$(hdrpkg)
	dh_builddeb -p$(hdrpkg)


binary-debs: $(addprefix binary-modules-,$(flavours))
binary-arch: binary-debs binary-udebs
