include $(RT28xx_DIR)/os/linux/config.mk

ifeq ($(RT28xx_MODE), STA)
MOD_NAME = rt$(CHIPSET)sta
DAT_PATH = /etc/Wireless/RT$(CHIPSET)STA
DAT_FILE_NAME = RT$(CHIPSET)STA.dat
endif

obj-m := $(MOD_NAME).o

rt$(CHIPSET)sta-objs := \
	../../common/md5.o\
	../../common/mlme.o\
	../../common/rtmp_wep.o\
	../../common/action.o\
	../../common/cmm_data.o\
	../../common/rtmp_init.o\
	../../common/rtmp_tkip.o\
	../../common/cmm_sync.o\
	../../common/eeprom.o\
	../../common/cmm_sanity.o\
	../../common/cmm_info.o\
	../../common/cmm_wpa.o\
	../../common/dfs.o\
	../../common/spectrum.o\
	../../sta/assoc.o\
	../../sta/aironet.o\
	../../sta/auth.o\
	../../sta/auth_rsp.o\
	../../sta/sync.o\
	../../sta/sanity.o\
	../../sta/rtmp_data.o\
	../../sta/connect.o\
	../../sta/wpa.o\
	../../os/linux/rt_linux.o\
    ../../os/linux/rt_profile.o\
	../../os/linux/rt_main_dev.o\
	../../os/linux/sta_ioctl.o

#ifdef DOT11_N_SUPPORT
ifeq ($(HAS_DOT11_N_SUPPORT),y)
rt$(CHIPSET)sta-objs += \
	../../common/ba_action.o
endif
#endif // DOT11_N_SUPPORT //

ifeq ($(HAS_BLOCK_NET_IF),y)
rt$(CHIPSET)sta-objs += ../../common/netif_block.o
endif

ifeq ($(HAS_QOS_DLS_SUPPORT),y)
rt$(CHIPSET)sta-objs += ../../sta/dls.o
endif

ifeq ($(CHIPSET),3070)
rt$(CHIPSET)sta-objs += \
	../../os/linux/2870_main_dev.o\
	../../common/2870_rtmp_init.o\
	../../common/rtusb_io.o\
	../../common/rtusb_bulk.o\
	../../common/rtusb_data.o\
	../../common/cmm_data_2870.o
endif

ifeq ($(HAS_ATE),y)
rt$(CHIPSET)sta-objs += ../../os/linux/rt_ate.o
endif

clean:
	rm -f ../../*.ko
	rm -f ../../common/*.o
	rm -f ../../common/.*.cmd
	rm -f ../../common/.*.flags
	rm -f ../../common/.*.d
	rm -f ../../os/linux/*.o
	rm -f ../../os/linux/*.ko
	rm -f ../../os/linux/*.mod.o
	rm -f ../../os/linux/*.mod.c
	rm -f ../../os/linux/.*.cmd
	rm -f ../../os/linux/.*.flags
	rm -f ../../os/linux/.*.d
	rm -fr ../../os/linux/.tmp_versions
ifeq ($(RT28xx_MODE),STA)
	rm -f ../../sta/*.o
	rm -f ../../sta/.*.cmd
	rm -f ../../sta/.*.flags
	rm -f ../../sta/.*.d
endif

install:
	rm -rf $(DAT_PATH)
	$(shell [ ! -f /etc/Wireless ] && mkdir /etc/Wireless)
	mkdir $(DAT_PATH)
	cp $(RT28xx_DIR)/$(DAT_FILE_NAME) $(DAT_PATH)/.
	install -d $(LINUX_SRC_MODULE)
	install -m 644 -c $(addsuffix .ko,$(MOD_NAME)) $(LINUX_SRC_MODULE)
	/sbin/depmod -a ${shell uname -r}

uninstall:
#	rm -rf $(DAT_PATH)
	rm -rf $(addprefix $(LINUX_SRC_MODULE),$(addsuffix .ko,$(MOD_NAME)))
	/sbin/depmod -a ${shell uname -r}
