/*
 *  wmi.c - ACPI Windows Management Interface Driver ($Revision: 1.1 $)
 *
 *  Copyright (C) 2004 Jamey Hicks <jamey.hicks@hp.com>
 *  Copyright (C) 2001, 2002 Andy Grover <andrew.grover@intel.com>
 *  Copyright (C) 2001, 2002 Paul Diefenbaugh <paul.s.diefenbaugh@intel.com>
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or (at
 *  your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <acpi/acpi.h>
#include <acpi/actypes.h>
#include <acpi/acpi_bus.h>
#include <acpi/acpi_drivers.h>
#include <asm/uaccess.h>

#define WMI_COMPONENT		0x40000000
#define WMI_CLASS			"wmi"
#define WMI_HID 			"PNP0C14"
#define WMI_DRIVER_NAME		"ACPI WMI Driver"
#define WMI_DEVICE_NAME		"Windows Management Interface"
#define WMI_FILE_STATE		"state"
#define WMI_FILE_WLAN		"wlan"
#define WMI_FILE_BLUETOOTH	"bluetooth"
#define WMI_NOTIFY_STATUS		0x80
#define WMI_STATUS_OFFLINE		0x0D
#define WMI_STATUS_ONLINE		0x0F
#define WMI_STATUS_UNKNOWN		0xFF

#define _COMPONENT		WMI_COMPONENT
ACPI_MODULE_NAME		("wmi")

MODULE_AUTHOR("Jamey Hicks");
MODULE_DESCRIPTION(WMI_DRIVER_NAME);
MODULE_LICENSE("GPL");

int wmi_add (struct acpi_device *device);
int wmi_remove (struct acpi_device *device, int type);
static int wmi_start (struct acpi_device *device);
static int wmi_stop (struct acpi_device *device, int type);
static int wmi_open_fs(struct inode *inode, struct file *file);

static struct acpi_driver wmi_driver = {
	.name =		WMI_DRIVER_NAME,
	.class =	WMI_CLASS,
	.ids =		WMI_HID,
	.ops =		{
				.add =		wmi_add,
				.remove =	wmi_remove,
				.start =	wmi_start,
				.stop =		wmi_stop,
			},
};

struct wmi {
	acpi_handle		handle;
	unsigned long           wlan;
	unsigned long           bluetooth;
	unsigned long		state[3];
};

static int wmi_set_state (struct wmi		*wmi);

static struct file_operations wmi_fops = {
	.open		= wmi_open_fs,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

/* --------------------------------------------------------------------------
                               WMI Management
   -------------------------------------------------------------------------- */

static int
wmi_get_state (
	struct wmi		*wmi)
{
	acpi_status		status = AE_OK;
	union acpi_object       params_objects[2];
	struct acpi_object_list params;
	int i;

	ACPI_FUNCTION_TRACE("wmi_get_state");

	if (!wmi)
		return_VALUE(-EINVAL);

	for (i = 0; i < 3; i++) {
		params.count = 1;
		params.pointer = params_objects;
		params_objects[0].integer.type = ACPI_TYPE_INTEGER;
		params_objects[0].integer.value = i;
		printk("calling WQBA\n");
		status = acpi_evaluate_integer(wmi->handle, "WQBA", &params, &wmi->state[i]);
		if (ACPI_FAILURE(status)) {
			ACPI_DEBUG_PRINT((ACPI_DB_ERROR,
					  "Error reading WMI state\n"));
			wmi->state[i] = WMI_STATUS_UNKNOWN;
			return_VALUE(-ENODEV);
		}
	}
	wmi->wlan = (wmi->state[1] == 3) ? 1 : 0;
	wmi->bluetooth = (wmi->state[2] == 1) ? 1 : 0;

	return_VALUE(0);
}

static int
wmi_set_state (struct wmi		*wmi)
{
	acpi_status		status = AE_OK;
	union acpi_object       params_objects[2];
	struct acpi_object_list params;
	int val;
	struct acpi_buffer ret;

	ACPI_FUNCTION_TRACE("wmi_set_state");
	printk("calling WSBA\n");

	if (!wmi)
		return_VALUE(-EINVAL);

	printk("calling WSBA index=1 val=%ld\n", wmi->state[1]);
	val = wmi->wlan ? 1 : 2;
	params.count = 2;
	params.pointer = params_objects;
	params_objects[0].integer.type = ACPI_TYPE_INTEGER;
	params_objects[0].integer.value = 1;
	params_objects[1].integer.type = ACPI_TYPE_INTEGER;
	params_objects[1].integer.value = val;
	status = acpi_evaluate_object(wmi->handle, "WSBA", &params, &ret);
	if (ACPI_FAILURE(status)) {
		ACPI_DEBUG_PRINT((ACPI_DB_ERROR,
				  "Error calling wsba state: status=%x\n", status));
	}

	printk("calling WSBA index=2 val=%ld\n", wmi->state[1]);
	val = wmi->bluetooth ? 0 : 1;
	params.count = 2;
	params.pointer = params_objects;
	params_objects[0].integer.type = ACPI_TYPE_INTEGER;
	params_objects[0].integer.value = 2;
	params_objects[1].integer.type = ACPI_TYPE_INTEGER;
	params_objects[1].integer.value = val;
	status = acpi_evaluate_object(wmi->handle, "WSBA", &params, &ret);
	if (ACPI_FAILURE(status)) {
		ACPI_DEBUG_PRINT((ACPI_DB_ERROR,
				  "Error calling wsba state: status=%x\n", status));
	}

	return_VALUE(0);
}
/* --------------------------------------------------------------------------
                              FS Interface (/proc)
   -------------------------------------------------------------------------- */

struct proc_dir_entry		*wmi_dir;

int wmi_seq_show(struct seq_file *seq, void *offset)
{
	struct wmi		*wmi = (struct wmi *) seq->private;
	int i;

	ACPI_FUNCTION_TRACE("wmi_seq_show");

	if (!wmi)
		return 0;

	if (wmi_get_state(wmi)) {
		seq_puts(seq, "ERROR: Unable to read WMI state\n");
		return 0;
	}

	seq_puts(seq, "state:                   ");
	for (i = 0; i < 3; i++)
		seq_printf(seq, " %ld", wmi->state[i]);
	seq_puts(seq, "\n");
	return 0;
}
	
static int wmi_open_fs(struct inode *inode, struct file *file)
{
	return single_open(file, wmi_seq_show, PDE(inode)->data);
}

static int 
wlan_write_state (
	struct file		*file,
	const char __user	*buffer,
	size_t			count,
	loff_t			*ppos)
{
	struct seq_file	*m = (struct seq_file *) file->private_data;
	struct wmi	*wmi = (struct wmi *) m->private;
	char	str[64];
	int	error = 0;

	printk("wmi_write_state: wmi=%p\n", wmi);

	if (count > sizeof(str) - 1)
		goto Done;
	memset(str,0,sizeof(str));
	if (copy_from_user(str, buffer, count))
		return -EFAULT;

	if (strncmp(str, "on", 2) == 0) {
	  wmi->wlan = 1;
	} else if (strncmp(str, "off", 3) == 0) {
	  wmi->wlan = 0;
	} else {
	  wmi->wlan = simple_strtoul(str, NULL, 0);
	}
	error = wmi_set_state(wmi);

 Done:
	return error ? error : count;
}

int wlan_seq_show(struct seq_file *seq, void *offset)
{
	struct wmi		*wmi = (struct wmi *) seq->private;

	ACPI_FUNCTION_TRACE("wlan_seq_show");

	if (!wmi)
		return 0;

	if (wmi_get_state(wmi)) {
		seq_puts(seq, "ERROR: Unable to read wmi state\n");
		return 0;
	}

	seq_puts(seq, wmi->wlan ? "on\n" : "off\n");
	return 0;
}

static int wlan_open_fs(struct inode *inode, struct file *file)
{
	return single_open(file, wlan_seq_show, PDE(inode)->data);
}

static struct file_operations wlan_fops = {
	.open		= wlan_open_fs,
	.read		= seq_read,
	.write		= wlan_write_state,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int 
bluetooth_write_state (
	struct file		*file,
	const char __user	*buffer,
	size_t			count,
	loff_t			*ppos)
{
	struct seq_file	*m = (struct seq_file *) file->private_data;
	struct wmi	*wmi = (struct wmi *) m->private;
	char	str[64];
	int	error = 0;

	printk("wmi_write_state: wmi=%p\n", wmi);

	if (count > sizeof(str) - 1)
		goto Done;
	memset(str,0,sizeof(str));
	if (copy_from_user(str, buffer, count))
		return -EFAULT;

	if (strncmp(str, "on", 2) == 0) {
	  wmi->bluetooth = 1;
	} else if (strncmp(str, "off", 3) == 0) {
	  wmi->bluetooth = 0;
	} else {
	  wmi->bluetooth = simple_strtoul(str, NULL, 0);
	}
	error = wmi_set_state(wmi);

 Done:
	return error ? error : count;
}

int bluetooth_seq_show(struct seq_file *seq, void *offset)
{
	struct wmi		*wmi = (struct wmi *) seq->private;

	ACPI_FUNCTION_TRACE("bluetooth_seq_show");

	if (!wmi)
		return 0;

	if (wmi_get_state(wmi)) {
		seq_puts(seq, "ERROR: Unable to read wmi state\n");
		return 0;
	}

	seq_puts(seq, wmi->bluetooth ? "on\n" : "off\n");
	return 0;
}

static int bluetooth_open_fs(struct inode *inode, struct file *file)
{
	return single_open(file, bluetooth_seq_show, PDE(inode)->data);
}

static struct file_operations bluetooth_fops = {
	.open		= bluetooth_open_fs,
	.read		= seq_read,
	.write		= bluetooth_write_state,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int
wmi_add_fs (
	struct acpi_device	*device)
{
	struct proc_dir_entry	*entry = NULL;

	ACPI_FUNCTION_TRACE("wmi_add_fs");

	if (!acpi_device_dir(device)) {
		acpi_device_dir(device) = proc_mkdir(acpi_device_bid(device),
						     wmi_dir);
		if (!acpi_device_dir(device))
			return_VALUE(-ENODEV);
	}

	/* 'state' [R] */
	entry = create_proc_entry(WMI_FILE_STATE,
				  S_IRUGO, acpi_device_dir(device));
	if (!entry)
		ACPI_DEBUG_PRINT((ACPI_DB_ERROR,
			"Unable to create '%s' fs entry\n",
			WMI_FILE_STATE));
	else {
		entry->proc_fops = &wmi_fops;
		entry->data = acpi_driver_data(device);
	}

	entry = create_proc_entry(WMI_FILE_WLAN,
				  S_IFREG|S_IRUGO|S_IWUSR, acpi_device_dir(device));
	if (!entry)
		ACPI_DEBUG_PRINT((ACPI_DB_ERROR,
			"Unable to create '%s' fs entry\n",
			WMI_FILE_STATE));
	else {
		entry->proc_fops = &wlan_fops;
		entry->data = acpi_driver_data(device);
	}

	entry = create_proc_entry(WMI_FILE_BLUETOOTH,
				  S_IFREG|S_IRUGO|S_IWUSR, acpi_device_dir(device));
	if (!entry)
		ACPI_DEBUG_PRINT((ACPI_DB_ERROR,
			"Unable to create '%s' fs entry\n",
			WMI_FILE_STATE));
	else {
		entry->proc_fops = &bluetooth_fops;
		entry->data = acpi_driver_data(device);
	}

	return_VALUE(0);
}


static int
wmi_remove_fs (
	struct acpi_device	*device)
{
	ACPI_FUNCTION_TRACE("wmi_remove_fs");

	if (acpi_device_dir(device)) {
		remove_proc_entry(WMI_FILE_STATE, acpi_device_dir(device));
		remove_proc_entry(WMI_FILE_WLAN, acpi_device_dir(device));
		remove_proc_entry(WMI_FILE_BLUETOOTH, acpi_device_dir(device));
		remove_proc_entry(acpi_device_bid(device), wmi_dir);
		acpi_device_dir(device) = NULL;
	}

	return_VALUE(0);
}


/* --------------------------------------------------------------------------
                                   Driver Model
   -------------------------------------------------------------------------- */

void
wmi_notify (
	acpi_handle		handle,
	u32			event,
	void			*data)
{
	struct wmi		*wmi = (struct wmi *) data;
	struct acpi_device	*device = NULL;

	ACPI_FUNCTION_TRACE("wmi_notify");

	if (!wmi)
		return;

	if (acpi_bus_get_device(wmi->handle, &device))
		return_VOID;

	switch (event) {
	case WMI_NOTIFY_STATUS:
		wmi_get_state(wmi);
		acpi_bus_generate_event(device, event, (u32) wmi->state);
		break;
	default:
		ACPI_DEBUG_PRINT((ACPI_DB_INFO,
			"Unsupported event [0x%x]\n", event));
		break;
	}

	return_VOID;
}


int
wmi_add (
	struct acpi_device	*device)
{
	int			result = 0;
	struct wmi		*wacom = NULL;

	ACPI_FUNCTION_TRACE("wmi_add");
	printk("wmi_add device=%p\n", device);

	if (!device)
		return_VALUE(-EINVAL);

	wacom = kmalloc(sizeof(struct wmi), GFP_KERNEL);
	if (!wacom)
		return_VALUE(-ENOMEM);
	memset(wacom, 0, sizeof(struct wmi));

	wacom->handle = device->handle;
	strcpy(acpi_device_name(device), WMI_DEVICE_NAME);
	strcpy(acpi_device_class(device), WMI_CLASS);
	acpi_driver_data(device) = wacom;

	result = wmi_get_state(wacom);
	if (result)
		goto end;

	result = wmi_add_fs(device);
	if (result)
		goto end;

#if 0
	status = acpi_install_notify_handler(wmi->handle,
		ACPI_DEVICE_NOTIFY, wmi_notify, ac);
	if (ACPI_FAILURE(status)) {
		ACPI_DEBUG_PRINT((ACPI_DB_ERROR,
			"Error installing notify handler\n"));
		result = -ENODEV;
		goto end;
	}
#endif

	printk(KERN_INFO PREFIX "%s [%s] (%s)\n", 
		acpi_device_name(device), acpi_device_bid(device), 
		wacom->state?"on-line":"off-line");

end:
	if (result) {
		wmi_remove_fs(device);
		kfree(wacom);
	}

	return_VALUE(result);
}


int
wmi_remove (
	struct acpi_device	*device,
	int			type)
{
	struct wmi		*wmi = NULL;

	ACPI_FUNCTION_TRACE("wmi_remove");

	if (!device || !acpi_driver_data(device))
		return_VALUE(-EINVAL);

	wmi = (struct wmi *) acpi_driver_data(device);

	wmi_remove_fs(device);

	kfree(wmi);

	return_VALUE(0);
}

static int
wmi_start (
	struct acpi_device	*device)
{
	struct wmi *wacom = NULL;

	ACPI_FUNCTION_TRACE("wmi_start");

	if (!device)
		return_VALUE(-EINVAL);

	wacom = acpi_driver_data(device);

	if (!wacom)
		return_VALUE(-EINVAL);

	return_VALUE(AE_OK);
}


static int
wmi_stop (
	struct acpi_device	*device,
	int			type)
{
	struct wmi		*wacom = NULL;

	ACPI_FUNCTION_TRACE("wmi_stop");

	if (!device)
		return_VALUE(-EINVAL);

	wacom = acpi_driver_data(device);

	return_VALUE(0);
}

int __init
wmi_init (void)
{
	int			result = 0;

	ACPI_FUNCTION_TRACE("wmi_init");

	wmi_dir = proc_mkdir(WMI_CLASS, acpi_root_dir);
	if (!wmi_dir)
		return_VALUE(-ENODEV);

	result = acpi_bus_register_driver(&wmi_driver);
	if (result < 0) {
		remove_proc_entry(WMI_CLASS, acpi_root_dir);
		return_VALUE(-ENODEV);
	}

	return_VALUE(0);
}


void __exit
wmi_exit (void)
{
	ACPI_FUNCTION_TRACE("wmi_exit");

	acpi_bus_unregister_driver(&wmi_driver);

	remove_proc_entry(WMI_CLASS, acpi_root_dir);

	return_VOID;
}


module_init(wmi_init);
module_exit(wmi_exit);
