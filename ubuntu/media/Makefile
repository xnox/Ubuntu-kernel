#
# All media drivers should use the lum version of the alsa headers
#
NOSTDINC_FLAGS			+= -I$(src)/../sound/alsa-driver/include

include $(src)/../.config

obj-$(CONFIG_GSPCA)		+= gspcav1/
obj-$(CONFIG_USB_OV511_NEW)	+= ov511/
obj-$(CONFIG_QUICKCAM)		+= quickcam/
obj-$(CONFIG_USB_UVCCAM)	+= usbvideo/
ifeq ($(CONFIG_PPC64),)
obj-$(CONFIG_LIRC_DEV)		+= lirc/
endif
obj-$(CONFIG_DRM_PSB)		+= drm-poulsbo/
obj-$(CONFIG_STK11XX)		+= stk11xx/

obj-$(CONFIG_CX88)		+= cx88/
obj-$(CONFIG_SAA7134)		+= saa7134/
