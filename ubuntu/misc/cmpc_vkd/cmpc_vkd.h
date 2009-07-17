/*
 * Virtual Key Driver
 * Copyright (C) 2008  Intel Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2, as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/**
 *  /file cmpc_vkd.h
 *
 *  /version 1.0
 *  /author Cui Yi & Zhao Shirley
 *  /date 05-29-2008
 */
 
#ifndef __CMPC_VKD_H__
#define __CMPC_VKD_H__

#include <linux/module.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/wait.h>
#include <asm/semaphore.h>
#include <linux/interrupt.h>

#include <acpi/acpi_drivers.h>
#include <acpi/acnamesp.h>
#include <asm/uaccess.h>

#define CMPC_CLASS_NAME "cmpc"
#define CMPC_VKD_NAME   "cmpc_vkd"

#define VKD_PRESS_EVENT     0x81

#define MAX_NUM	8
#define dbg_print(format, arg...) \
	printk(KERN_DEBUG "%s " format, __func__, ##arg)

ACPI_MODULE_NAME("cmpc_vkd");
MODULE_AUTHOR("Cui Yi & Shirley Zhao");
MODULE_DESCRIPTION("CMPC Virtual Key Driver");
MODULE_VERSION("1.0.0.0");
MODULE_LICENSE("GPL");

/** function definitions */
/** common driver functions */
static int cmpc_vkd_init(void);
static void cmpc_vkd_exit(void);
static int cmpc_vkd_open(struct inode *inode, struct file *file);
static int cmpc_vkd_release(struct inode *inode, struct file *file);
static int cmpc_vkd_ioctl(struct inode *inode, struct file *file,
	unsigned int cmd, unsigned long arg);
static ssize_t cmpc_vkd_read(struct file *file, char __user *buf,
	size_t size, loff_t *ppos);
static ssize_t cmpc_vkd_write(struct file *file, const char __user *buf,
	size_t size, loff_t *ppos);
static loff_t cmpc_vkd_llseek(struct file *file, loff_t offset, int orig);

/** acpi notify handler */
static void cmpc_vkd_handler(acpi_handle handle, u32 event, void *data);

/** acpi driver ops */
static int cmpc_vkd_add(struct acpi_device *device);
static int cmpc_vkd_resume(struct acpi_device *device);
static int cmpc_vkd_remove(struct acpi_device *device, int type);

static const struct acpi_device_id vkd_device_ids[] = {
/*	{"IBM0068", 0},*/
	{"FnBT0000",0},
	{"", 0},
};

MODULE_DEVICE_TABLE(acpi, vkd_device_ids);

static struct acpi_driver vkd_drv = {
	.owner = THIS_MODULE,
	.name  = CMPC_VKD_NAME,
	.class = CMPC_CLASS_NAME,
	.ids   = vkd_device_ids,
	.ops   = {
		.add    = cmpc_vkd_add,
		.resume = cmpc_vkd_resume,
		.remove = cmpc_vkd_remove,
	},
};

/** CMPC Virtual Key Drivercmpc_vkd_ struct */
struct cmpc_vkd_dev {
	struct acpi_device *device;	/** acpi bus device */
	u32                event;	/** acpi event */
};

static struct cmpc_vkd_dev *vkd_dev;

static const struct file_operations vkd_fops = {
	.owner   = THIS_MODULE,
	.open    = cmpc_vkd_open,
	.release = cmpc_vkd_release,
	.read    = cmpc_vkd_read,
	.write   = cmpc_vkd_write,
	.llseek  = cmpc_vkd_llseek,
	.ioctl   = cmpc_vkd_ioctl,
};

static wait_queue_head_t outq;
static int flag = 0;

u32		fifo_que[MAX_NUM];
int		que_head = 0;
int	    	que_tail = -1;
int 		num = 0;

static struct proc_dir_entry *vkd_class_dir;
static struct proc_dir_entry *vkd_entry;
#endif  /*  __CMPC_VKD_H__ */
