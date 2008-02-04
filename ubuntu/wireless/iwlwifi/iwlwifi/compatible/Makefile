CONFIG_IWL4965=m
CONFIG_IWL3945=m
NOSTDINC_FLAGS += -I$(src)/../../mac80211/compatible/include
EXTRA_CFLAGS += -DCONFIG_IWLWIFI_QOS=1
obj-$(CONFIG_IWL3945)	+= iwl3945.o
iwl3945-objs		= iwl3945-base.o iwl-3945.o iwl-3945-rs.o

obj-$(CONFIG_IWL4965)	+= iwl4965.o
iwl4965-objs		= iwl4965-base.o iwl-4965.o iwl-4965-rs.o
