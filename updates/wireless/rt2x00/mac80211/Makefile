include $(src)/../config

CFLAGS_ieee80211.o += -DRC80211_PID_COMPILE

obj-$(LBM_CONFIG_MAC80211) += rt2x_mac80211.o

# objects for PID algorithm
rt2x_rc80211_pid-y := rc80211_pid_algo.o
rt2x_rc80211_pid-$(LBM_CONFIG_MAC80211_DEBUGFS) += rc80211_pid_debugfs.o

# build helper for PID algorithm
rc-pid-y := $(rt2x_rc80211_pid-y)
rc-pid-m := rc80211_pid.o

# mac80211 objects
rt2x_mac80211-y := \
	main.o \
	wext.o \
	sta_info.o \
	wep.o \
	wpa.o \
	mlme.o \
	iface.o \
	rate.o \
	michael.o \
	tkip.o \
	aes_ccm.o \
	cfg.o \
	rx.o \
	tx.o \
	key.o \
	util.o \
	event.o

rt2x_mac80211-$(LBM_CONFIG_MAC80211_LEDS) += led.o
rt2x_mac80211-$(CONFIG_NET_SCHED) += wme.o
rt2x_mac80211-$(LBM_CONFIG_MAC80211_DEBUGFS) += \
	debugfs.o \
	debugfs_sta.o \
	debugfs_netdev.o \
	debugfs_key.o

rt2x_mac80211-$(LBM_CONFIG_MAC80211_MESH) += \
	mesh.o \
	mesh_pathtbl.o \
	mesh_plink.o \
	mesh_hwmp.o


# Build rate control algorithm(s)
CFLAGS_rc80211_pid_algo.o += -DRC80211_PID_COMPILE
rt2x_mac80211-$(LBM_CONFIG_MAC80211_RC_PID) += $(rc-pid-$(LBM_CONFIG_MAC80211_RC_PID))

# Modular rate algorithms are assigned to rt2x_mac80211-m - make separate modules
obj-m += $(rt2x_mac80211-m)
