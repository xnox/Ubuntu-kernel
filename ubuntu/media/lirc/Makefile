include $(src)/../../.config

EXTRA_CFLAGS =-DIRCTL_DEV_MAJOR=61 -DLIRC_SERIAL_TRANSMITTER -I$(src)

obj-$(CONFIG_LIRC_DEV)		+= lirc_dev/lirc_dev.o
obj-$(CONFIG_LIRC_ATIUSB)	+= lirc_atiusb/lirc_atiusb.o
obj-$(CONFIG_LIRC_BT829)	+= lirc_bt829/lirc_bt829.o
obj-$(CONFIG_LIRC_CMDIR)	+= lirc_cmdir/lirc_cmdir.o lirc_cmdir/commandir.o
obj-$(CONFIG_LIRC_I2C)		+= lirc_i2c/lirc_i2c.o
obj-$(CONFIG_LIRC_IGORPLUGUSB)	+= lirc_igorplugusb/lirc_igorplugusb.o
obj-$(CONFIG_LIRC_IMON)		+= lirc_imon/lirc_imon.o
obj-$(CONFIG_LIRC_IT87)		+= lirc_it87/lirc_it87.o
obj-$(CONFIG_LIRC_MCEUSB)	+= lirc_mceusb/lirc_mceusb.o
obj-$(CONFIG_LIRC_MCEUSB2)	+= lirc_mceusb2/lirc_mceusb2.o
obj-$(CONFIG_LIRC_PVR150)	+= lirc_pvr150/lirc_pvr150.o
obj-$(CONFIG_LIRC_PARALLEL)	+= lirc_parallel/lirc_parallel.o
obj-$(CONFIG_LIRC_SASEM)	+= lirc_sasem/lirc_sasem.o
obj-$(CONFIG_LIRC_SERIAL)	+= lirc_serial/lirc_serial.o
obj-$(CONFIG_LIRC_SERIAL_IGOR)	+= lirc_serial_igor/lirc_serial_igor.o
obj-$(CONFIG_LIRC_SIR)		+= lirc_sir/lirc_sir.o
obj-$(CONFIG_LIRC_STREAMZAP)	+= lirc_streamzap/lirc_streamzap.o
obj-$(CONFIG_LIRC_TTUSBIR)	+= lirc_ttusbir/lirc_ttusbir.o
obj-$(CONFIG_LIRC_GPIO)		+= lirc_gpio/lirc_gpio.o
obj-$(CONFIG_LIRC_TTUSBIR)	+= lirc_ttusbir/lirc_ttusbir.o
lirc_gpio/lirc_gpio-objs 	+= lirc_gpio/bttv_deprecated.o
