VERSION    = 01.00.12

DEFINES   += -DGSPCA_ENABLE_COMPRESSION
DEFINES   += -DVID_HARDWARE_GSPCA=0xFF -DGSPCA_VERSION=\"$(VERSION)\"

CFLAGS += $(DEFINES) 
obj-m += gspca.o
gspca-objs := gspca_core.o decoder/gspcadecoder.o 
