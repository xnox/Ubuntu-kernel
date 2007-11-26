#
# Makefile for the linux asfs filesystem routines.
#

obj-m += asfs.o

asfs-y += dir.o extents.o file.o inode.o namei.o nodes.o objects.o super.o symlink.o
asfs-$(CONFIG_ASFS_RW) += adminspace.o bitfuncs.o 
