do-binary-udebs:
	dh_testdir
	dh_testroot

	imagelist=$$(cat $(udebdir)/kernel-versions | grep ^${arch} | \
		     awk '{print $$4}') && \
	for i in $$imagelist; do \
	  dpkg -x $$(ls ../linux-backports-modules-$$i\_*${arch}.deb) \
		$(udebdir)/; \
	  if [ -d $(udebdir)/lib/modules/$$i ]; then \
	    /sbin/depmod -b $(udebdir) $$i; \
	  else \
	    touch $(udebdir)/no-modules; \
	  fi; \
	done

	#
	# Generate empty udebs on prupose. This is a sledgehammer
	# approach to an FTBS on amd64. Probably needs to be fixed
	# for Hardy.
	#
	touch $(udebdir)/no-modules

	cd $(udebdir); export SOURCEDIR=$(udebdir) && \
	  (test -f $(udebdir)/no-modules || (kernel-wedge install-files && \
	  kernel-wedge check))

        # Build just the udebs
	cd $(udebdir); dilist=$$(dh_listpackages -s | grep "\-di$$") && \
	for i in $$dilist; do \
	  dh_fixperms -p$$i; \
	  dh_gencontrol -p$$i; \
	  dh_builddeb -p$$i; \
	done

	mv debian/*.udeb ../

binary-udebs: binary-debs debian/control
	debian/rules do-binary-udebs
