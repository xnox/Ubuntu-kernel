# COMMON

CARD		:= fcdslslusb
LIBDIR		:= /var/lib/fritz
OBJECTS		:= main.o driver.o tools.o devif.o tables.o \
		   queue.o lib.o buffers.o

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
HEADERS		:= defs.h driver.h lib.h libdefs.h libstub.h lock.h \
		   main.h queue.h tables.h tools.h buffers.h common.h \
		   devif.h fw.h

ifeq ($(KDIR),)
KDIR		:= /lib/modules/$(shell uname -r)/build
endif

all:		$(CARD).o
	
$(CARD).o:	$(LIBDIR) $(SOURCES) $(HEADERS)
		@cp -f ../lib/$(CARD)-lib.o $(LIBDIR)
		$(MAKE) -C $(KDIR) SUBDIRS=$$PWD modules

clean:
		$(RM) $(OBJECTS)
		$(RM) $(CARD).o $(CARD).ko 
		@$(RM) .*.cmd $(CARD).mod.* 

$(LIBDIR):	
		mkdir -p $(LIBDIR)

endif

