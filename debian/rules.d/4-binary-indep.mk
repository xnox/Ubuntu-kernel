build-indep:

indep_hdrpkg = linux-headers-lbm-$(release)$(debnum)
indep_hdrdir = $(CURDIR)/debian/$(indep_hdrpkg)/usr/src/linux-headers-lbm-$(release)$(debnum)
snd_incl     = $(indep_hdrdir)/sound

install-headers:
	dh_testdir
	dh_testroot
	dh_clean -k -p$(indep_hdrpkg)
	if grep 'CONFIG_ALSA=m' $(confdir)/$(arch) > /dev/null ; then \
	  install -d $(snd_incl); \
	  cp --preserve=timestamps `find ubuntu/sound/alsa-kernel/include -type f` $(snd_incl); \
	fi

install-indep: install-headers

binary-headers: install-headers
	dh_testdir
	dh_testroot
	dh_installchangelogs -p$(indep_hdrpkg)
	dh_installdocs -p$(indep_hdrpkg)
	dh_compress -p$(indep_hdrpkg)
	dh_fixperms -p$(indep_hdrpkg)
	dh_installdeb -p$(indep_hdrpkg)
	dh_gencontrol -p$(indep_hdrpkg)
	dh_md5sums -p$(indep_hdrpkg)
	dh_builddeb -p$(indep_hdrpkg)

binary-indep: binary-headers
