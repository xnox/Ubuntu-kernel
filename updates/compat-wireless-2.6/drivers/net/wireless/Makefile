#
# Makefile for the Linux Wireless network device drivers.
#

obj-$(CONFIG_IPW2100) += ipw2100.o

obj-$(CONFIG_IPW2200) += ipw2200.o

obj-$(CONFIG_B43)		+= b43/
obj-$(CONFIG_B43LEGACY)		+= b43legacy/
obj-$(CONFIG_ZD1211RW)		+= zd1211rw/

obj-$(CONFIG_USB_NET_RNDIS_WLAN)	+= rndis_wlan.o

obj-$(CONFIG_LIBERTAS)		+= libertas/

obj-$(CONFIG_LIBERTAS_THINFIRM)	+= libertas_tf/

rtl8180-objs		:= rtl8180_dev.o rtl8180_rtl8225.o rtl8180_sa2400.o rtl8180_max2820.o rtl8180_grf5101.o
rtl8187-objs		:= rtl8187_dev.o rtl8187_rtl8225.o

obj-$(CONFIG_RTL8180)	+= rtl8180.o
obj-$(CONFIG_RTL8187)	+= rtl8187.o

obj-$(CONFIG_ADM8211)	+= adm8211.o

obj-$(CONFIG_IWLWIFI)	+= iwlwifi/
obj-$(CONFIG_RT2X00)	+= rt2x00/

obj-$(CONFIG_P54_COMMON)	+= p54/

obj-$(CONFIG_ATH5K)	+= ath5k/
obj-$(CONFIG_ATH9K)	+= ath9k/

obj-$(CONFIG_MAC80211_HWSIM)	+= mac80211_hwsim.o
