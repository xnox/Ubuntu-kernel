#
# Makefile for the linux squashfs routines.
#

obj-m += squashfs.o

squashfs-y += inode.o
squashfs-y += squashfs2_0.o

EXTRA_CFLAGS += -I$(src) -DCONFIG_SQUASHFS_FRAGMENT_CACHE_SIZE="3"
