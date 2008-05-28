agpgart-y := backend.o frontend.o generic.o isoch.o

agpgart-$(CONFIG_COMPAT)	+= compat_ioctl.o

obj-$(CONFIG_AGP)		+= agpgart.o
obj-m				+= intel-agp-ich9m.o
