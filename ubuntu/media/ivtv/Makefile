CONFIG_VIDEO_ADV_DEBUG=y
CPPFLAGS += -I$(M)/../i2c-drivers -I$(M) -I$(M)/../driver

obj-m        += driver/ivtv-fb.o
obj-m        += i2c-drivers/saa717x.o

ifeq ($(CONFIG_VIDEO_ADV_DEBUG),y)
EXTRA_CFLAGS += -DCONFIG_VIDEO_ADV_DEBUG=1
endif
