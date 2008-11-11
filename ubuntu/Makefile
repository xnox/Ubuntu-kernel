#
# Makefile for Ubuntu additional drivers
#

include $(src)/.config

obj-$(CONFIG_ACER_ACPI)		+= acpi/acer_acpi/
obj-$(CONFIG_INPUT_FSC_BTNS)	+= misc/fsc_btns.o
obj-$(CONFIG_ALSA)      	+= sound/
obj-$(CONFIG_BLK_DEV_DRBD)      += block/drbd/
obj-$(CONFIG_BLK_ISCSITARGET)	+= block/iscsitarget/
obj-$(CONFIG_AMT_HECI)		+= misc/heci/
obj-$(CONFIG_BLK_DEV_GNBD)	+= misc/gnbd.o
obj-$(CONFIG_BLK_DEV_DM_BBR)	+= misc/dm-bbr.o
obj-$(CONFIG_SND_BTSCO)		+= snd-bt-sco.o
obj-$(CONFIG_INPUT_ACERHK)	+= misc/acerhk.o
obj-$(CONFIG_AVERATEC_5100P)	+= misc/av5100.o
obj-$(CONFIG_PACKARDBELL_E5)	+= misc/pbe5.o
obj-$(CONFIG_LMPCM_USB)		+= misc/lmpcm_usb.o
obj-$(CONFIG_NOZOMI)		+= misc/nozomi.o

obj-$(CONFIG_NDISWRAPPER)	+= misc/ndiswrapper/
obj-$(CONFIG_MOL)		+= misc/mol/

snd-bt-sco-objs			:= misc/btsco.o
#
# The btsco driver should ultimately be incorportated into the ALSA tree
# or deprecated. At the moment, we need to force the following build options to 
# make sure that struct snd_pcm_str in pcm.h is in sync with the ALSA build configs.
#
CFLAGS_btsco.o += -I$(src)/sound/alsa-driver/include -DCONFIG_SND_VERBOSE_PROCFS -DCONFIG_SND_PCM_OSS_MODULE

CFLAGS_dm-bbr.o			+= -I$(srctree)/drivers/md

ifneq ($(UBUNTU_FLAVOUR),openvz)
obj-$(CONFIG_AUFS)		+= fs/aufs/
endif

obj-$(CONFIG_ASFS_FS)		+= fs/asfs/
ifneq ($(UBUNTU_FLAVOUR),rt)
obj-$(CONFIG_GFS_FS)		+= fs/gfs/
endif
obj-$(CONFIG_SQUASHFS)		+= fs/squashfs/
obj-$(CONFIG_UNION_FS)		+= fs/unionfs/

obj-$(CONFIG_USB_APPLEIR)	+= mactel/appleir.o

obj-m				+= media/

obj-$(CONFIG_NET_ATL2)		+= net/atl2/

obj-$(CONFIG_NET_ACX)		+= wireless/acx/
obj-$(CONFIG_IWLWIFI)		+= wireless/iwlwifi/
obj-$(CONFIG_USB_ATMEL)		+= wireless/at76/
obj-$(CONFIG_PRISM2_USB)	+= wireless/prism2_usb/
obj-$(CONFIG_WIMAX_I2400M)	+= wireless/wimax-i2400m/

ifneq ($(CONFIG_PRISM2_USB),)
obj-m				+= wireless/p80211/
endif

obj-$(CONFIG_IPW2100_FS_AMILO_M7400) += wireless/fsam7400.o

obj-$(CONFIG_MMC_SD8686)       += wireless/marvell/8686_wlan/
obj-$(CONFIG_MMC_SD8688)       += wireless/marvell/8688_wlan/
obj-$(CONFIG_MMC_SD8688)       += wireless/marvell/8688_bt/

obj-$(CONFIG_ET131X)		+= net/et131x/
obj-$(CONFIG_IGB)		+= net/igb/
obj-$(CONFIG_E1000E_ICH9M)	+= net/e1000e-ich9m/

obj-$(CONFIG_TP_SMAPI)		+= misc/tp_smapi.o
obj-$(CONFIG_TP_SMAPI_EC)	+= misc/thinkpad_ec.o
obj-$(CONFIG_TP_SMAPI_HDAPS)	+= hwmon/hdaps_ec.o

obj-$(CONFIG_DMRAID45)		+= md/

obj-$(CONFIG_AGP_INTEL_ICH9M)	+= char/intel-agp-ich9m/

obj-$(CONFIG_HP_MINI)		+= misc/hpmini.o
