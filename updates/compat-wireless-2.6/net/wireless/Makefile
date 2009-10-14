obj-$(CONFIG_CFG80211) += cfg80211.o
obj-$(CONFIG_LIB80211) += lib80211.o
obj-$(CONFIG_LIB80211_CRYPT_WEP) += lib80211_crypt_wep.o
obj-$(CONFIG_LIB80211_CRYPT_CCMP) += lib80211_crypt_ccmp.o
obj-$(CONFIG_LIB80211_CRYPT_TKIP) += lib80211_crypt_tkip.o

cfg80211-y += core.o sysfs.o radiotap.o util.o reg.o scan.o nl80211.o
cfg80211-y += mlme.o ibss.o sme.o chan.o
cfg80211-$(CONFIG_CFG80211_DEBUGFS) += debugfs.o
cfg80211-$(CONFIG_WIRELESS_EXT) += wext-compat.o wext-sme.o

ccflags-y += -D__CHECK_ENDIAN__
# Compat-wireless kernel compatibility code
cfg80211-$(CONFIG_COMPAT_WIRELESS_22) += compat-2.6.22.o
cfg80211-$(CONFIG_COMPAT_WIRELESS_23) += compat-2.6.23.o
cfg80211-$(CONFIG_COMPAT_WIRELESS_24) += compat-2.6.24.o
cfg80211-$(CONFIG_COMPAT_WIRELESS_25) += compat-2.6.25.o
cfg80211-$(CONFIG_COMPAT_WIRELESS_26) += compat-2.6.26.o
cfg80211-$(CONFIG_COMPAT_WIRELESS_27) += compat-2.6.27.o
cfg80211-$(CONFIG_COMPAT_WIRELESS_28) += compat-2.6.28.o
cfg80211-$(CONFIG_COMPAT_WIRELESS_29) += compat-2.6.29.o
cfg80211-$(CONFIG_COMPAT_WIRELESS_30) += compat-2.6.30.o
cfg80211-$(CONFIG_COMPAT_WIRELESS_31) += compat-2.6.31.o
cfg80211-$(CONFIG_COMPAT_WIRELESS_32) += compat-2.6.32.o

