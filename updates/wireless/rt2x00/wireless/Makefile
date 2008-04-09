include $(src)/../config

rt2x_wext-y	:= wext.o

obj-$(LBM_CONFIG_WIRELESS_EXT) += rt2x_wext.o
obj-$(LBM_CONFIG_CFG80211) += rt2x_cfg80211.o

rt2x_cfg80211-y += core.o sysfs.o radiotap.o util.o reg.o
rt2x_cfg80211-$(LBM_CONFIG_NL80211) += nl80211.o
