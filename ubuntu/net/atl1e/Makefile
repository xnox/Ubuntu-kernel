CFILES = at_common_main.c \
	 atl1c_hw.c atl1c_main.c atl1c_param.c \
	 atl1e_hw.c atl1e_main.c atl1e_param.c \
	 kcompat.c

HFILES = at_common.h atl1c.h atl1c_hw.h atl1e.h \
	 atl1e_hw.h at_osdep.h kcompat.h

obj-m += atl1e.o
atl1e-objs := $(CFILES:.c=.o)
