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
 *  /file cmpc_vkd.c
 *
 *  /version 1.0
 *  /author Cui Yi & Zhao Shirley
 *  /date 05-29-2008
 */
 
#include "cmpc_vkd.h"

static int __init cmpc_vkd_init(void)
{
	dbg_print("----->\n");
	
	/** get memory forstruct input_dev *input; device struct */
	vkd_dev = kzalloc(sizeof(struct cmpc_vkd_dev), GFP_KERNEL);
	if (!vkd_dev) {
		dbg_print("kmalloc for device struct failed\n");
		dbg_print("<-----\n");
		goto fail0;
	}

	/** make dir */
	vkd_class_dir = proc_mkdir(CMPC_CLASS_NAME, acpi_root_dir);
	if (!vkd_class_dir) {
		dbg_print("proc_mkdir failed\n");
		goto fail1;
	}
	vkd_class_dir->owner = THIS_MODULE;
	
	/** register bus driver */
	if (acpi_bus_register_driver(&vkd_drv) < 0) {
		dbg_print("acpi_bus_register_driver failed\n");
		goto fail2;
	}

	init_waitqueue_head(&outq);
	
	que_head = 0;
	que_tail = -1;
	num = 0;

	dbg_print("<-----\n");
	return 0;

fail2:
	remove_proc_entry(CMPC_CLASS_NAME, acpi_root_dir);

fail1:
	kfree(vkd_dev);

fail0:
	dbg_print("<-----\n");
	return -1;
}

static void __exit cmpc_vkd_exit(void)
{
	dbg_print("----->\n");

	acpi_bus_unregister_driver(&vkd_drv);
	remove_proc_entry(CMPC_CLASS_NAME, acpi_root_dir);

	kfree(vkd_dev);
	
	dbg_print("<-----\n");
}

static int cmpc_vkd_open(struct inode *inode, struct file *file)
{
	dbg_print("----->\n");
	dbg_print("<-----\n");
	return 0;
}

static int cmpc_vkd_release(struct inode *inode, struct file *file)
{
	dbg_print("----->\n");
	if (flag) 
                //notify the data is available
                wake_up_interruptible(&outq);

	dbg_print("<-----\n");
	return 0;
}

static int cmpc_vkd_ioctl(struct inode *inode, struct file *file,
	unsigned int cmd, unsigned long arg)
{
	dbg_print("----->\n");
	dbg_print("<-----\n");
	return 0;
}

static ssize_t cmpc_vkd_read(struct file *file, char __user *buf,
	size_t size, loff_t *ppos)
{
	int result = 0;
	dbg_print("----->\n");

	if (wait_event_interruptible(outq, flag) != 0)
	{
		return - ERESTARTSYS;
	}

//	while(1)
//	{
		flag = 0;

		if(num <= 0)
		{
			dbg_print("fifo que is empty. \n");
			return 0;
		}

		que_tail = (que_tail+1)%MAX_NUM;

		u32 to_user_event;
		to_user_event = fifo_que[que_tail];

		dbg_print("to user event %x\n", to_user_event);

		if (copy_to_user(buf, &to_user_event, sizeof(u32)))
		{
			dbg_print("copy to user failed\n");
			return - EFAULT;
		}

		num --;

		if(num >0)
			flag = 1;
//	}

	dbg_print("<-----\n");
	return 0;
}

static ssize_t cmpc_vkd_write(struct file *file, const char __user *buf,
	size_t size, loff_t *ppos)
{
	dbg_print("----->\n");
	dbg_print("<-----\n");
	return 0;
}

static loff_t cmpc_vkd_llseek(struct file *file, loff_t offset, int orig)
{
	dbg_print("----->\n");
	dbg_print("<-----\n");
	return 0;
}

static void cmpc_vkd_handler(acpi_handle handle, u32 event, void *data)
{
	dbg_print("----->\n");
	dbg_print("acpi event: %d\n", event);
	dbg_print("<-----\n");

//	if (event == TSD_DEVICE_INFO_CHANGE)
	{//notify the caller
		flag = 1;
		//notify the data is available
		wake_up_interruptible(&outq);
	}

	dbg_print("head=%d, tail=%d\n", que_head, que_tail);
	
	if(num == MAX_NUM)
	{
		dbg_print("fifo que is full. \n");
	}
	else
	{
		fifo_que[que_head] = event;
		que_head = (que_head+1)%MAX_NUM;
		num ++;
	}

	acpi_bus_generate_proc_event(vkd_dev->device, event, 123);	
}

static int cmpc_vkd_add(struct acpi_device *device) {
	struct acpi_object_list params;
	union acpi_object in_objs;
	
	dbg_print("----->\n");

	vkd_entry = create_proc_entry(CMPC_VKD_NAME, S_IRUGO, acpi_root_dir);
	
	if (!vkd_entry) {
		dbg_print("create_proc_entry failed\n");
		goto fail0;
	}	
	vkd_entry->owner = THIS_MODULE;
	vkd_entry->data = vkd_dev;
	vkd_entry->proc_fops = &vkd_fops;
	
	vkd_dev->device = device;
	acpi_driver_data(device) = vkd_dev;
	strcpy(acpi_device_name(device), CMPC_VKD_NAME);
	sprintf(acpi_device_class(device), "%s/%s", CMPC_CLASS_NAME,
		CMPC_VKD_NAME);

	/** install acpi notify handler */
	if (!ACPI_SUCCESS(acpi_install_notify_handler(device->handle,
		ACPI_DEVICE_NOTIFY, cmpc_vkd_handler, vkd_dev))) {
		dbg_print("acpi_install_notify_handler failed\n");
		goto fail1;
	}

	/** enable EC notify */
/*	params.count = 1;
	params.pointer = &in_objs;
	in_objs.integer.value = 1;
	in_objs.type = ACPI_TYPE_INTEGER;
	if (!ACPI_SUCCESS(acpi_evaluate_object(
		device->handle, "MHKC", &params, NULL))) {
		dbg_print("acpi_evaluate_object failed\n");
		goto fail2;
	}
*/	
	dbg_print("<-----\n");
	return 0;

fail2:
	acpi_remove_notify_handler(vkd_dev->device->handle, ACPI_DEVICE_NOTIFY, 
		cmpc_vkd_handler);

fail1:
	remove_proc_entry(CMPC_VKD_NAME, acpi_root_dir);
	
fail0:
	dbg_print("<-----\n");
	return -ENODEV;
}

static int cmpc_vkd_resume(struct acpi_device *device) {
	dbg_print("----->\n");
	dbg_print("<-----\n");
	return 0;
}

static int cmpc_vkd_remove(struct acpi_device *device, int type) {
	dbg_print("----->\n");
	acpi_remove_notify_handler(vkd_dev->device->handle, ACPI_DEVICE_NOTIFY, 
		cmpc_vkd_handler);
	remove_proc_entry(CMPC_VKD_NAME, acpi_root_dir);
	dbg_print("<-----\n");
	return 0;
}

module_init(cmpc_vkd_init);
module_exit(cmpc_vkd_exit);
