/*
 * Accelerometer Driver
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
 * @file accel.h
 *
 * Accelerometer driver definitions
 */
 
#ifndef ACCEL_H
#define ACCEL_H

#include <linux/module.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/fs.h>

#include <linux/interrupt.h>

#include <acpi/acpi_drivers.h>
#include <acpi/acnamesp.h>

#include <asm/uaccess.h>

#include "inc/accelio.h"

#define CMPC_CLASS_NAME    "cmpc"
#define ACCEL_NAME         "accel"

/**
 * Accelerometer AML method name
 */
#define ACCEL_AML_METHOD   "ACMD"
/**
 * Accelerometer notify value
 */
#define ACCEL_NOTIFY        0x81
/**
 * AML method first parameter value to read data
 */
#define ACCEL_AML_READ_DATA 0x01
/**
 * AML method first parameter value to set sensitivity
 */
#define ACCEL_AML_SET_SENSE 0x02
/**
 * AML method first parameter value to start accelerometer
 */
#define ACCEL_AML_START     0x03
/**
 * AML method first parameter value to stop accelerometer
 */
#define ACCEL_AML_STOP      0x04
/**
 * AML method first parameter value to set g-select
 */
#define ACCEL_AML_SET_G_SELECT 0x05
/**
 * AML method first parameter value to get g-select
 */
#define ACCEL_AML_GET_G_SELECT 0x06

ACPI_MODULE_NAME("accel");
MODULE_AUTHOR("Cui Yi");
MODULE_DESCRIPTION("CMPC Accelerometer Driver");
MODULE_VERSION("0.9.0.0");
MODULE_LICENSE("GPL");

#define dbg_print(format, arg...) \
	printk(KERN_DEBUG "%s " format, __func__, ##arg)

/* function definitions */
/* common driver functions */
static int accel_init(void);
static void accel_exit(void);
static int accel_open(struct inode *inode, struct file *file);
static int accel_release(struct inode *inode, struct file *file);
static int accel_ioctl(struct inode *inode, struct file *file,
	unsigned int cmd, unsigned long arg);
static ssize_t accel_read(struct file *file, char __user *buf,
	size_t size, loff_t *ppos);

/* acpi notify handler */
static void accel_handler(acpi_handle handle, u32 event, void *data);

/* acpi driver ops */
static int accel_add(struct acpi_device *device);
static int accel_remove(struct acpi_device *device, int type);
static int accel_start(struct acpi_device *device);
static int accel_stop(struct acpi_device *device, int type);
static int accel_suspend(struct acpi_device *device, pm_message_t state);
static int accel_resume(struct acpi_device *device);

/* CMPC accel AML commands */
acpi_status accel_aml_start(void);
acpi_status accel_aml_stop(void);
acpi_status accel_aml_set_sense(int sense);
acpi_status accel_aml_read_data(struct accel_raw_data *data);
acpi_status accel_aml_set_g_select(int select);
acpi_status accel_aml_get_g_select(int *select);

/* struct definitions */
/**
 * Accelerometer device id
 */
static const struct acpi_device_id accel_device_ids[] = {
	{"ACCE0000", 0},
	{"", 0},
};

MODULE_DEVICE_TABLE(acpi, accel_device_ids);

/**
 * Accelerometer driver struct
 */
static struct acpi_driver accel_drv = {
	.owner = THIS_MODULE,
	.name  = ACCEL_NAME,
	.class = CMPC_CLASS_NAME,
	.ids   = accel_device_ids,
	.ops   = {
		.add     = accel_add,
		.remove  = accel_remove,
		.start   = accel_start,
		.stop    = accel_stop,
		.suspend = accel_suspend,
		.resume  = accel_resume,
	},
};

/**
 * Accelerometer device state struct
 */
enum accel_state {
	ACCEL_NOT_STARTED = 0,
	ACCEL_STARTED,
	ACCEL_SUSPENDED,
	ACCEL_STOPPED,
	ACCEL_REMOVED,
};

/**
 * Accelerometer device struct
 */
struct accel_dev {
	struct acpi_device *device;	/**< acpi bus device */
	enum accel_state   curr_state;	/**< accel current state */
	enum accel_state   prev_state;	/**< accel previous state */
	wait_queue_head_t  wait_queue;	/**< wait queue for notify */
	int                wait_flag;	/**< flag for wait interruptible */
};

#define INIT_ACCEL_STATE(ACCEL_DEV) \
	(ACCEL_DEV)->curr_state = ACCEL_NOT_STARTED;\
	(ACCEL_DEV)->prev_state = ACCEL_NOT_STARTED;
#define SET_NEW_ACCEL_STATE(ACCEL_DEV, ACCEL_STATE) \
	(ACCEL_DEV)->prev_state = (ACCEL_DEV)->curr_state;\
	(ACCEL_DEV)->curr_state = ACCEL_STATE;
#define REST_PREV_ACCEL_STATE(ACCEL_DEV) \
	(ACCEL_DEV)->curr_state = (ACCEL_DEV)->prev_state;\

/**
 * a pointer to Accelerometer device
 */
static struct accel_dev *accel_dev;

static const struct file_operations vkd_fops = {
	.owner   = THIS_MODULE,
	.open    = accel_open,
	.release = accel_release,
	.read    = accel_read,
	.ioctl   = accel_ioctl,
};

/**
 * a pointer to Accelerometer device file
 */
static struct proc_dir_entry *accel_entry;

#endif  /*  ACCEL_H */
