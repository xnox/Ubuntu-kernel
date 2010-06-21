ifeq ($(KERNELRELEASE),)
GOBI_VERSION = 0.6

all:	gobi_loader

gobi_loader:
	$(MAKE) -C gobi_loader-$(GOBI_VERSION)

.PHONY: install
install:
	$(MAKE) -C gobi_loader-$(GOBI_VERSION) prefix=$(prefix) install
	DOCDIR=$(prefix)/usr/share/doc/$$(basename $(prefix));		\
	install -d $$DOCDIR;						\
	install -v gobi_loader-$(GOBI_VERSION)/README			\
		$$DOCDIR/README.gobi_loader

.PHONY: clean
clean:
	$(MAKE) -C gobi_loader-$(GOBI_VERSION) clean
else
#warning hello
obj-m += qcserial.o
obj-m += usb_wwan.o
endif
