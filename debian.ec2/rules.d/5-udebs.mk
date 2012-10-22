# Do udebs if not disabled in the arch-specific makefile
binary-udebs: binary-debs debian/control
ifeq ($(disable_d_i),)
	@$(MAKE) --no-print-directory -f $(DEBIAN)/rules DEBIAN=$(DEBIAN) \
		do-binary-udebs
endif

do-binary-udebs:
	echo debug $^
	dh_testdir
	dh_testroot

	# unpack the kernels into a temporary directory
	mkdir -p debian/d-i-${arch}

	imagelist=$$(cat $(builddir)/kernel-versions | grep ^${arch} | awk '{print $$4}') && \
	for i in $$imagelist; do \
	  dpkg -x $$(ls ../$(bin_base_pkg_name)-$$i\_$(release)-$(revision)_${arch}.deb) \
		debian/d-i-${arch}; \
	  /sbin/depmod -b debian/d-i-${arch} $$i; \
	done

	touch ignore-dups
	export SOURCEDIR=$(CURDIR)/debian/d-i-${arch} && \
	  cd $(builddir) && \
	  kernel-wedge install-files && \
	  kernel-wedge check

        # Build just the udebs
	dilist=$$(dh_listpackages -s | grep "\-di$$") && \
	[ -z "$dilist" ] || \
	for i in $$dilist; do \
	  dh_fixperms -p$$i; \
	  dh_gencontrol -p$$i; \
	  dh_builddeb -p$$i; \
	done
