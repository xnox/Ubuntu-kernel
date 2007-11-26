#
# Makefile for Ubuntu additional drivers
#

include $(src)/.config

obj-$(CONFIG_ACPI_DEV)		+= acpi/dev_acpi.o
obj-$(CONFIG_ACPI_PCC)		+= acpi/pcc_acpi.o
obj-$(CONFIG_ACPI_SONY)		+= acpi/sony_acpi.o
obj-$(CONFIG_ACPI_TC1100)	+= acpi/tc1100-wmi.o

obj-$(CONFIG_AMT_HECI)		+= misc/heci/
obj-$(CONFIG_BLK_DEV_GNBD)	+= misc/gnbd.o
obj-$(CONFIG_BLK_DEV_DM_BBR)	+= misc/dm-bbr.o
obj-$(CONFIG_SND_BTSCO)		+= snd-bt-sco.o
obj-$(CONFIG_INPUT_ACERHK)	+= misc/acerhk.o
obj-$(CONFIG_INPUT_FSC_BTNS)	+= misc/fsc_btns.o
obj-$(CONFIG_AVERATEC_5100P)	+= misc/av5100.o
obj-$(CONFIG_PACKARDBELL_E5)	+= misc/pbe5.o
obj-$(CONFIG_LMPCM_USB)		+= misc/lmpcm_usb.o
obj-$(CONFIG_NOZOMI)		+= misc/nozomi.o

obj-$(CONFIG_NDISWRAPPER)	+= misc/ndiswrapper/
obj-$(CONFIG_MOL)		+= misc/mol/
obj-$(CONFIG_TIFM)		+= misc/tifm/

snd-bt-sco-objs			:= misc/btsco.o
CFLAGS_dm-bbr.o			+= -I$(srctree)/drivers/md

obj-$(CONFIG_ASFS_FS)		+= fs/asfs/
obj-$(CONFIG_GFS_FS)		+= fs/gfs/
obj-$(CONFIG_SQUASHFS)		+= fs/squashfs/
obj-$(CONFIG_UNION_FS)		+= fs/unionfs/

obj-$(CONFIG_USB_APPLEIR)	+= mactel/appleir.o

obj-$(CONFIG_GSPCA)		+= media/gspcav1/
obj-$(CONFIG_USB_OV511_NEW)	+= media/ov511/
obj-$(CONFIG_QUICKCAM)		+= media/quickcam/
obj-$(CONFIG_USB_UVCCAM)	+= media/usbvideo/
obj-$(CONFIG_USB_ISIGHT)	+= media/isight/
obj-$(CONFIG_LIRC_DEV)		+= media/lirc/
obj-$(CONFIG_SND_HDA_INTEL)	+= media/snd-hda-intel/
obj-$(CONFIG_DRM_PSB)		+= media/drm-poulsbo/
obj-$(CONFIG_IVTV_FB)		+= media/ivtv/

obj-$(CONFIG_NET_IPG)		+= net/ipg/
obj-$(CONFIG_E1000_ICH9)	+= net/e1000-ich9/
obj-$(CONFIG_NET_ATL2)		+= net/atl2/

obj-$(CONFIG_SECURITY_APPARMOR)	+= security/apparmor/

obj-$(CONFIG_NET_ACX)		+= wireless/acx/
obj-$(CONFIG_IPW3945)		+= wireless/ipw3945/
obj-$(CONFIG_IWLWIFI)		+= wireless/iwlwifi/
obj-$(CONFIG_USB_ATMEL)		+= wireless/at76/
obj-$(CONFIG_RTL8187)		+= wireless/rtl818x/
obj-$(CONFIG_PRISM2_USB)	+= wireless/prism2_usb/

ifneq ($(CONFIG_PRISM2_USB),)
obj-m				+= wireless/p80211/
endif
ifneq ($(CONFIG_RTL8187),)
obj-m				+= misc/eeprom_93cx6.o
endif

obj-$(CONFIG_RT2X00)		+= wireless/rt2x00/

obj-$(CONFIG_IPW2100_FS_AMILO_M7400) += wireless/fsam7400.o

obj-$(CONFIG_P54)		+= wireless/p54/
obj-$(CONFIG_MARVELL_8686)	+= wireless/sd8686/
obj-$(CONFIG_NET_RTL818X)	+= wireless/rtl8180/rtl_ieee80211/
obj-$(CONFIG_NET_RTL818X)	+= wireless/rtl8180/rtl8180/
obj-$(CONFIG_ET131X)		+= net/et131x/

obj-$(CONFIG_VMWARE) += vmblock/ vmhgfs/ vmmemctl/ vmxnet/

