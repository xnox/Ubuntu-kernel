CONFIG_IWL4965=m
CONFIG_IWL3945=m
CONFIG_IWL3945_LEDS=y
CONFIG_IWL4965_LEDS=y
NOSTDINC_FLAGS += -I$(src)/../../mac80211/compatible/include
EXTRA_CFLAGS += -DCONFIG_IWL4965_QOS -DCONFIG_IWL3945_QOS
EXTRA_CFLAGS += -DCONFIG_IWL4965_LEDS -DCONFIG_IWL3945_LEDS
obj-$(CONFIG_IWL3945)	+= iwl3945.o
iwl3945-objs		= iwl3945-base.o iwl-3945.o iwl-3945-rs.o
ifeq ($(CONFIG_IWL3945_LEDS),y)
	iwl3945-objs += iwl-3945-led.o
endif


obj-$(CONFIG_IWL4965)	+= iwl4965.o
iwl4965-objs		= iwl4965-base.o iwl-4965.o iwl-4965-rs.o
ifeq ($(CONFIG_IWL4965_LEDS),y)
	iwl4965-objs += iwl-4965-led.o
endif

