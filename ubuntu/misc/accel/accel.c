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
 * @file accel.c
 *
 * Accelerometer driver implementation
 */
 
#include "accel.h"

static int __init accel_init(void)
{
	dbg_print("----->\n");
	
	/* get memory for device struct */
	accel_dev = kzalloc(sizeof(struct accel_dev), GFP_KERNEL);
	if (!accel_dev) {
		dbg_print("kmalloc for device struct failed\n");
		dbg_print("<-----\n");
		goto fail0;
	}

	/* register bus driver */
	if (acpi_bus_register_driver(&accel_drv) < 0) {
		dbg_print("acpi_bus_register_driver failed\n");
		goto fail1;
	}

	/* initialize wait queue head for accel notify */
	init_waitqueue_head(&accel_dev->wait_queue);

	dbg_print("<-----\n");
	return 0;

fail1:
	kfree(accel_dev);

fail0:
	dbg_print("<-----\n");
	return -1;
}

static void __exit accel_exit(void)
{
	dbg_print("----->\n");

	acpi_bus_unregister_driver(&accel_drv);

	kfree(accel_dev);
	
	dbg_print("<-----\n");
}

static int accel_open(struct inode *inode, struct file *file)
{
	dbg_print("----->\n");

	if (ACCEL_NOT_STARTED == accel_dev->curr_state) {
		if (ACPI_SUCCESS(accel_aml_start())) {
			SET_NEW_ACCEL_STATE(accel_dev, ACCEL_STARTED);
		} else {
			dbg_print("start accel failed\n");
			return -EFAULT;
		}
	} else if (ACCEL_STARTED != accel_dev->curr_state) {
		dbg_print("accel state error: %d\n", accel_dev->curr_state);
		return -EFAULT;
	}	

	dbg_print("<-----\n");
	return 0;
}

static int accel_release(struct inode *inode, struct file *file)
{
	dbg_print("----->\n");
	
	/* stop accel */
	if (ACCEL_STARTED == accel_dev->curr_state) {
		if (ACPI_SUCCESS(accel_aml_stop())) {
			SET_NEW_ACCEL_STATE(accel_dev, ACCEL_NOT_STARTED);
		} else {
			dbg_print("stop accel failed\n");
			return -EFAULT;
		}
	} else if (ACCEL_STOPPED != accel_dev->curr_state) {
		dbg_print("accel state error: %d\n", accel_dev->curr_state);
		return -EFAULT;
	}

	/* wake up blocked read */
	accel_dev->wait_flag = 1;
	wake_up_interruptible(&accel_dev->wait_queue);

	dbg_print("<-----\n");
	return 0;
}

static int accel_ioctl(struct inode *inode, struct file *file,
	unsigned int cmd, unsigned long arg)
{
	int select;

	dbg_print("----->\n");

	/* IOCTL START/STOP only set accel state to STARTED/NOT_STARTED.
	 * Only system calls can set accel state to SUSPENDED/STOPPED/REMOVED.
	 * If accel is in SUSPENDED/STOPPED/REMOVED, it cannot be started.
	 * Only system calls can resume or start it. But the start system
	 * call cannot start accel really. It only set it to initial state. 
	 */
	switch (cmd) {
	case IOCTL_ACCEL_START:
		if (ACCEL_NOT_STARTED == accel_dev->curr_state) {
			if (ACPI_SUCCESS(accel_aml_start())) {
				SET_NEW_ACCEL_STATE(accel_dev, ACCEL_STARTED);
			} else {
				dbg_print("start accel failed\n");
				return -EFAULT;
			}
		} else if (ACCEL_STARTED != accel_dev->curr_state) {
			dbg_print("accel state error: %d\n", accel_dev->curr_state);
			return -EFAULT;
		}	
		break;

	case IOCTL_ACCEL_STOP:
		if (ACCEL_STARTED == accel_dev->curr_state) {
			if (ACPI_SUCCESS(accel_aml_stop())) {
				SET_NEW_ACCEL_STATE(accel_dev, ACCEL_NOT_STARTED);
			} else {
				dbg_print("stop accel failed\n");
				return -EFAULT;
			}
		} else if (ACCEL_STOPPED != accel_dev->curr_state) {
			dbg_print("accel state error: %d\n", accel_dev->curr_state);
			return -EFAULT;
		}
		break;

	case IOCTL_ACCEL_SET_SENSE:
		/** the sensitivity should never be greater than 128 */
		if (arg > 128) {
			dbg_print("the sensitivity %lx is not correct\n", arg);
			return -EINVAL;
		}

		if (!ACPI_SUCCESS(accel_aml_set_sense(arg))) {
			dbg_print("set sensitivity failed\n");
			return -EFAULT;
		}
		
		break;

	case IOCTL_ACCEL_SET_G_SELECT:
		/** the g-select should be 0 or 1 */
		if ((0 != arg) && (1 != arg)) {
			dbg_print("the g-select %lx is not correct\n", arg);
			return -EINVAL;
		}

		if (!ACPI_SUCCESS(accel_aml_set_g_select(arg))) {
			dbg_print("set g-select failed\n");
			return -EFAULT;
		}
		
		break;

	case IOCTL_ACCEL_GET_G_SELECT:
		if (!ACPI_SUCCESS(accel_aml_get_g_select(&select))) {
			dbg_print("get g-select failed\n");
			return -EFAULT;
		}

		if (copy_to_user((void __user *)arg, &select, sizeof(select))) {
			dbg_print("copy_to_user for g-select failed\n");
			return -EFAULT;
		}

		break;

	default:
		dbg_print("invalid ioctl commands\n");
		return -ENOTTY;
	}
	
	dbg_print("<-----\n");
	return 0;
}

static ssize_t accel_read(struct file *file, char __user *buf,
	size_t size, loff_t *ppos)
{
	struct accel_raw_data data;

	dbg_print("----->\n");

	if (sizeof(data) > size) {
		dbg_print("data size is too samll: %d/%d\n", sizeof(data), size);
		return -EINVAL;
	}

	if (wait_event_interruptible(accel_dev->wait_queue, 
		accel_dev->wait_flag != 0) != 0) {
		dbg_print("waiting is interrupted by signal\n");
		return -ERESTARTSYS;
	}
	accel_dev->wait_flag = 0;
	
	if (accel_dev->curr_state == ACCEL_STARTED) {
		if (!ACPI_SUCCESS(accel_aml_read_data(&data))) {
			dbg_print("accel_aml_read_data failed\n");
			return -EFAULT;
		}
		if (copy_to_user(buf, &data, sizeof(data)) != 0) {
			dbg_print("copy_to_user failed\n");
			return -EIO;
		}

		dbg_print("<-----\n");
		return sizeof(data);
	} else {
		dbg_print("accel read state error: %d\n", accel_dev->curr_state);
		return 0;
	}
}

static void accel_handler(acpi_handle handle, u32 event, void *data)
{
	dbg_print("----->\n");

	switch (event) {
	case ACCEL_NOTIFY:
		accel_dev->wait_flag = 1;
		wake_up_interruptible(&accel_dev->wait_queue);
		break;
	
	default:
		dbg_print("accel notify type error\n");
		break;
	}

	dbg_print("<-----\n");

	/* do not generate proc event
	acpi_bus_generate_proc_event(accel_dev->device, event, 0);
	*/
}

static int accel_add(struct acpi_device *device)
{
	dbg_print("----->\n");

	accel_entry = create_proc_entry(ACCEL_NAME, S_IRUGO, acpi_root_dir);
	
	if (!accel_entry) {
		dbg_print("create_proc_entry failed\n");
		goto fail0;
	}	
	accel_entry->owner = THIS_MODULE;
	accel_entry->data = accel_dev;
	accel_entry->proc_fops = &vkd_fops;
	
	accel_dev->device = device;
	acpi_driver_data(device) = accel_dev;
	strcpy(acpi_device_name(device), ACCEL_NAME);
	sprintf(acpi_device_class(device), "%s/%s", CMPC_CLASS_NAME,
		ACCEL_NAME);

	/* install acpi notify handler */
	if (!ACPI_SUCCESS(acpi_install_notify_handler(device->handle,
		ACPI_DEVICE_NOTIFY, accel_handler, accel_dev))) {
		dbg_print("acpi_install_notify_handler failed\n");
		goto fail1;
	}

	/** set accel initial state */
	INIT_ACCEL_STATE(accel_dev);

	dbg_print("<-----\n");
	return 0;

fail1:
	remove_proc_entry(ACCEL_NAME, acpi_root_dir);
	
fail0:
	dbg_print("<-----\n");
	return -ENODEV;
}

static int accel_remove(struct acpi_device *device, int type)
{
	dbg_print("----->\n");

	/* stop accel if it is started */
	if (ACCEL_STARTED == accel_dev->curr_state) {
		if (!ACPI_SUCCESS(accel_aml_stop())) {
			dbg_print("stop accel failed\n");
			return -EFAULT;
		}
	}

	SET_NEW_ACCEL_STATE(accel_dev, ACCEL_REMOVED);

	acpi_remove_notify_handler(accel_dev->device->handle, ACPI_DEVICE_NOTIFY,
		accel_handler);
	remove_proc_entry(ACCEL_NAME, acpi_root_dir);

	dbg_print("<-----\n");
	return 0;
}

static int accel_start(struct acpi_device *device) 
{
	dbg_print("----->\n");
	
	/* Only set accel state to initial state since only IOCTL
	 * START can really start accel. But system call can stop
	 * accele. If accel is stopped by system call, accel plugin
	 * cannot start it again, until accel_start set the initial
	 * state again.
	 */
	if (ACCEL_STOPPED == accel_dev->curr_state) {
		INIT_ACCEL_STATE(accel_dev);
	} else if (ACCEL_NOT_STARTED != accel_dev->curr_state) {
		dbg_print("accel state error: %d\n", accel_dev->curr_state);
		return -EFAULT;
	}

	dbg_print("<-----\n");
	return 0;
}

static int accel_stop(struct acpi_device *device, int type)
{
	dbg_print("----->\n");

	/* stop accel if it is started*/
	if (ACCEL_STARTED == accel_dev->curr_state) {
		if (!ACPI_SUCCESS(accel_aml_stop())) {
			dbg_print("stop accel failed\n");
			return -EFAULT;
		}
	}

	SET_NEW_ACCEL_STATE(accel_dev, ACCEL_STOPPED);

	dbg_print("<-----\n");
	return 0;
}

static int accel_suspend(struct acpi_device *devicei, pm_message_t state)
{
	dbg_print("----->\n");

	/* stop accel if it is started */
	if (ACCEL_STARTED == accel_dev->curr_state) {
		if (!ACPI_SUCCESS(accel_aml_stop())) {
			dbg_print("stop accel failed\n");
			return -EFAULT;
		}
	}

	SET_NEW_ACCEL_STATE(accel_dev, ACCEL_SUSPENDED);

	dbg_print("<-----\n");
	return 0;
}

static int accel_resume(struct acpi_device *device)
{
	dbg_print("----->\n");

	/* When accel resume, restore the state to previous. If it was
	 * started by IOCTL START before, start it again.
	 */
	REST_PREV_ACCEL_STATE(accel_dev);

	if (ACCEL_STARTED == accel_dev->curr_state) {
		if (!ACPI_SUCCESS(accel_aml_start())) {
			dbg_print("stop accel failed\n");
			return -EFAULT;
		}
	}

	dbg_print("<-----\n");
	return 0;
}

/**
 * Call AML method to start accelerometer.
 *
 * @return	ACPI_SUCCESS if success; other values if error
 */
acpi_status accel_aml_start(void)
{
	struct acpi_object_list params;
	union acpi_object in_obj;

	params.count = 1;
	params.pointer = &in_obj;
	in_obj.integer.value = ACCEL_AML_START;
	in_obj.type = ACPI_TYPE_INTEGER;

	return acpi_evaluate_object(accel_dev->device->handle, ACCEL_AML_METHOD,
		&params, NULL);
}

/**
 * Call AML method to stop accelerometer.
 *
 * @return	ACPI_SUCCESS if success; other values if error
 */
acpi_status accel_aml_stop(void)
{
	struct acpi_object_list params;
	union acpi_object in_obj;

	params.count = 1;
	params.pointer = &in_obj;
	in_obj.integer.value = ACCEL_AML_STOP;
	in_obj.type = ACPI_TYPE_INTEGER;

	return acpi_evaluate_object(accel_dev->device->handle, ACCEL_AML_METHOD,
		&params, NULL);
}

/**
 * Call AML method to set accelerometer sensitivity.
 *
 * @param sense	sensitivity wanted
 * @return	ACPI_SUCCESS if success; other values if error
 */
acpi_status accel_aml_set_sense(int sense)
{
	struct acpi_object_list params;
	union acpi_object in_objs[2];

	params.count = 2;
	params.pointer = &in_objs[0];
	in_objs[0].integer.value = ACCEL_AML_SET_SENSE;
	in_objs[0].type = ACPI_TYPE_INTEGER;
	in_objs[1].integer.value = sense;
	in_objs[1].type = ACPI_TYPE_INTEGER;

	return acpi_evaluate_object(accel_dev->device->handle, ACCEL_AML_METHOD,
		&params, NULL);
}

/**
 * Call AML method to read acceleration data.
 *
 * @param data	a pointer to acceleration data
 * @return	ACPI_SUCCESS if success; other values if error
 */
acpi_status accel_aml_read_data(struct accel_raw_data *data)
{
	struct acpi_object_list params;
	union acpi_object in_obj;
	struct acpi_buffer result = {0, NULL};
	union acpi_object* out_obj;
	acpi_status status;
	
	params.count = 1;
	params.pointer = &in_obj;
	in_obj.integer.value = ACCEL_AML_READ_DATA;
	in_obj.type = ACPI_TYPE_INTEGER;

	/* get the actual buffer size first */
	status = acpi_evaluate_object(accel_dev->device->handle, ACCEL_AML_METHOD,
		&params, &result);
	switch (status) {
	case AE_BUFFER_OVERFLOW:
		result.pointer = kmalloc(result.length, GFP_KERNEL);
		if (!result.pointer) {
			dbg_print("no enough memory\n");
			return AE_NO_MEMORY;
		}
		
		/* get the data for real */
		status = acpi_evaluate_object(accel_dev->device->handle,
			ACCEL_AML_METHOD, &params, &result);
		if (ACPI_SUCCESS(status)) {
			out_obj = (union acpi_object *)result.pointer;
			*data = *((struct accel_raw_data *)(out_obj->buffer.pointer));
			dbg_print("accel_raw_data is {%d, %d, %d}\n", data->accel_raw_x,
				data->accel_raw_y, data->accel_raw_z);
		} else {
			dbg_print("acpi_evaluate_object status error: %s\n",
				acpi_format_exception(status));
		}
		
		kfree(result.pointer);
		break;

	default:
		dbg_print("acpi_evaluate_object status error: %s\n",
			acpi_format_exception(status));
	}

	return status;
}

/**
 * Call AML method to set g-select.
 *
 * @param select	g-select wanted
 * @return	ACPI_SUCCESS if success; other values if error
 */
acpi_status accel_aml_set_g_select(int select)
{
	struct acpi_object_list params;
	union acpi_object in_objs[2];

	params.count = 2;
	params.pointer = &in_objs[0];
	in_objs[0].integer.value = ACCEL_AML_SET_G_SELECT;
	in_objs[0].type = ACPI_TYPE_INTEGER;
	in_objs[1].integer.value = select;
	in_objs[1].type = ACPI_TYPE_INTEGER;

	return acpi_evaluate_object(accel_dev->device->handle, ACCEL_AML_METHOD,
		&params, NULL);
}

/**
 * Call AML method to read g-select.
 *
 * @param select	a pointer to g-select
 * @return	ACPI_SUCCESS if success; other values if error
 */
acpi_status accel_aml_get_g_select(int *select)
{
	struct acpi_object_list params;
	union acpi_object in_obj;
	struct acpi_buffer result;
	union acpi_object out_obj;
	acpi_status status;
	
	params.count = 1;
	params.pointer = &in_obj;
	in_obj.integer.value = ACCEL_AML_GET_G_SELECT;
	in_obj.type = ACPI_TYPE_INTEGER;

	result.length = sizeof(out_obj);
	result.pointer = &out_obj;

	status = acpi_evaluate_object(accel_dev->device->handle, ACCEL_AML_METHOD,
		&params, &result);
	if (ACPI_SUCCESS(status)) {
		if (ACPI_TYPE_INTEGER == out_obj.type) {
			*select = out_obj.integer.value;
			dbg_print("g-select is %d\n", *select);
		} else {
			dbg_print("acpi_evaluate_object return type error for g-select\n");
			status = AE_TYPE;
		}
	}

	return status;
}

module_init(accel_init);
module_exit(accel_exit);
