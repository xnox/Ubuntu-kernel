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
	touch $@

# Do the actual build, including image and modules
build-modules-%: $(stampdir)/stamp-build-%
	@# Empty for make to be happy
$(stampdir)/stamp-build-%: target_flavour = $*
$(stampdir)/stamp-build-%: build_arch_t = $(call custom_override,build_arch,$*)
$(stampdir)/stamp-build-%: $(stampdir)/stamp-prepare-%
	@echo "Building $*..."
	$(kmake) modules
	@touch $@

# Install the finished build
install-%: pkgdir = $(CURDIR)/debian/linux-ubuntu-modules-$(release)-$(abinum)-$*
install-%: moddir = $(pkgdir)/lib/modules/$(release)-$(abinum)-$*
install-%: firmdir = $(pkgdir)/lib/firmware/$(release)-$(abinum)-$*
install-%: target_flavour = $*
install-%: $(stampdir)/stamp-build-%
	dh_testdir
	dh_testroot
	dh_clean -k -plinux-ubuntu-modules-$(release)-$(abinum)-$*

	install -d $(firmdir)
	cp -a ubuntu-firmware/zd1211 $(firmdir)/
	for i in ubuntu-firmware/*/[[:lower:]]*; do \
	  case $${i##*/} in \
	    zd12*) ;; \
	    *) cp $$i "$(firmdir)/${i##*/}";; \
	  esac; \
	done

	install -d $(moddir)/ubuntu
	cd $(builddir)/build-$*; find . -name '*.ko' | \
		cpio -dumpl $(moddir)/ubuntu

ifeq ($(no_image_strip),)
	find $(pkgdir)/ -name \*.ko -print | xargs strip --strip-debug
endif

	install -d $(pkgdir)/DEBIAN
	for script in postinst postrm; do					\
	  sed -e 's/@@KVER@@/$(release)-$(abinum)-$*/g'				\
	       debian/control-scripts/$$script > $(pkgdir)/DEBIAN/$$script;	\
	  chmod 755 $(pkgdir)/DEBIAN/$$script;					\
	done

binary-modules-%: pkgimg = linux-ubuntu-modules-$(release)-$(abinum)-$*
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
