# COMMON

CARD		:= fwlanusb
LIBDIR		:= /tmp/lib/fritz/
INSTALLDIR	:= /lib/modules/$(shell uname -r)/extra/
OBJECTS		:= main.o driver.o tools.o lib.o buffers.o wext.o 

ifneq ($(KERNELRELEASE),)

# KERNEL

EXTRA_CFLAGS	+= -D__$(CARD)__ -DTARGET=\"$(CARD)\"  
ifndef DEBUG
EXTRA_CFLAGS	+= -DNDEBUG
endif
EXTRA_LDFLAGS	+= $(LIBDIR)/$(CARD)-lib.o

obj-m		:= $(CARD).o
$(CARD)-objs	:= $(OBJECTS)

else

# ARCHIVE

SOURCES		:= $(patsubst %.o,%.c,$(OBJECTS))
HEADERS		:= defs.h driver.h lib.h libdefs.h libstub.h tools.h lock.h \
			buffers.h wext.h

ifeq ($(KDIR),)
KDIR		:= /lib/modules/$(shell uname -r)/build
endif

all:		$(CARD).o
	
$(CARD).o:	$(LIBDIR) $(SOURCES) $(HEADERS)
		@cp -f ../lib/$(CARD)-lib.o $(LIBDIR)
		$(MAKE) -C $(KDIR) SUBDIRS=$(shell pwd) modules 
	       	
clean:
		$(RM) $(OBJECTS)
		$(RM) $(CARD).o $(CARD).ko 
		@$(RM) .*.cmd $(CARD).mod.* Module.symvers $(RM) -r .tmp_versions

$(LIBDIR):	
		if ! test -d $(LIBDIR); then mkdir -p $(LIBDIR); fi

install:	
		@if ! test -d $(INSTALLDIR); then mkdir -p $(INSTALLDIR); fi
		@cp -vf fwlanusb.ko $(INSTALLDIR)
		depmod -a

endif

