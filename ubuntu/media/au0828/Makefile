include $(src)/../../.config

au0828-objs	:= au0828-core.o au0828-i2c.o au0828-cards.o au0828-dvb.o

obj-$(CONFIG_VIDEO_AU0828) += au0828.o

obj-$(CONFIG_DVB_AU8522) += au8522.o

obj-$(CONFIG_MEDIA_TUNER_XC5000) += xc5000.o

EXTRA_CFLAGS += -Idrivers/media/dvb/dvb-core

EXTRA_CFLAGS += $(extra-cflags-y) $(extra-cflags-m)
