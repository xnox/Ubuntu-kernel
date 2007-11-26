binary-udebs: binary-debs debian/control
	dh_testdir
	dh_testroot

	imagelist=$$(cat $(udebdir)/kernel-versions | grep ^${arch} | \
		     awk '{print $$4}') && \
	for i in $$imagelist; do \
	  dpkg -x $$(ls ../linux-ubuntu-modules-$$i\_*${arch}.deb) \
		$(udebdir)/; \
	  /sbin/depmod -b $(udebdir) $$i; \
	done

	cd $(udebdir); export SOURCEDIR=$(udebdir) && \
	  kernel-wedge install-files && \
	  kernel-wedge check

        # Build just the udebs
	cd $(udebdir); dilist=$$(dh_listpackages -s | grep "\-di$$") && \
	for i in $$dilist; do \
	  dh_fixperms -p$$i; \
	  dh_gencontrol -p$$i; \
	  dh_builddeb -p$$i; \
	done

	mv debian/*.udeb ../
