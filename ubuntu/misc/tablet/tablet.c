/**
 *  /file tablet.c
 *
 *  /version 0.6.0.0
 *  /date 05-29-2008
 */
 
#include "tablet.h"

ACPI_MODULE_NAME("cmpc_tablet");
MODULE_DESCRIPTION("CMPC Tablet Sensor Driver");
MODULE_VERSION("0.9.0.0");
MODULE_LICENSE("GPL");

#define dbg_print(format, arg...) \
	printk(KERN_DEBUG "%s " format, __func__, ##arg)

/** function definitions */
/** common driver functions */
static int cmpc_tsd_init(void);
static void cmpc_tsd_exit(void);
static int cmpc_tsd_open(struct inode *inode, struct file *file);
static int cmpc_tsd_release(struct inode *inode, struct file *file);
static int cmpc_tsd_ioctl(struct inode *inode, struct file *file,
	unsigned int cmd, unsigned long arg);
static ssize_t cmpc_tsd_read(struct file *file, char __user *buf,
	size_t size, loff_t *ppos);
static ssize_t cmpc_tsd_write(struct file *file, const char __user *buf,
	size_t size, loff_t *ppos);
static loff_t cmpc_tsd_llseek(struct file *file, loff_t offset, int orig);

/** acpi notify handler */
static void cmpc_tsd_handler(acpi_handle handle, u32 event, void *data);

/** acpi driver ops */
static int cmpc_tsd_add(struct acpi_device *device);
static int cmpc_tsd_resume(struct acpi_device *device);
static int cmpc_tsd_remove(struct acpi_device *device, int type);

static int cmpc_tsd_get_status(int *res);

static const struct acpi_device_id tsd_device_ids[] = {
	{"TBLT0000", 0},
	{"", 0},
};

MODULE_DEVICE_TABLE(acpi, tsd_device_ids);

static struct acpi_driver tsd_drv = {
	.owner = THIS_MODULE,
	.name  = CMPC_TSD_NAME,
	.class = CMPC_CLASS_NAME,
	.ids   = tsd_device_ids,
	.ops   = {
		.add    = cmpc_tsd_add,
		.resume = cmpc_tsd_resume,
		.remove = cmpc_tsd_remove,
	},
};

/** CMPC Virtual Key Drivercmpc_tsd_ struct */
struct cmpc_tsd_dev {
	struct acpi_device *device;	/** acpi bus device */
	u32                event;	/** acpi event */
};

static struct cmpc_tsd_dev *tsd_dev;

static const struct file_operations tsd_fops = {
	.owner   = THIS_MODULE,
	.open    = cmpc_tsd_open,
	.release = cmpc_tsd_release,
	.read    = cmpc_tsd_read,
	.write   = cmpc_tsd_write,
	.llseek  = cmpc_tsd_llseek,
	.ioctl   = cmpc_tsd_ioctl,
};

static wait_queue_head_t outq;
static int flag = 0;

static struct proc_dir_entry *tsd_entry;

static int __init cmpc_tsd_init(void)
{
	dbg_print("----->\n");
	
	/** get memory forstruct input_dev *input; device struct */
	tsd_dev = kzalloc(sizeof(struct cmpc_tsd_dev), GFP_KERNEL);
	if (!tsd_dev) {
		dbg_print("kmalloc for device struct failed\n");
		dbg_print("<-----\n");
		goto fail0;
	}

	
	/** register bus driver */
	if (acpi_bus_register_driver(&tsd_drv) < 0) {
		dbg_print("acpi_bus_register_driver failed\n");
		goto fail1;
	}

/*	major = register_chrdev(0, "tablet sensor", &tsd_fops);
	if (major < 0) return major;
	dbg_print("registered. major = %d\n", major);
*/
	init_waitqueue_head(&outq);
	dbg_print("<-----\n");
	return 0;


fail1:
	kfree(tsd_dev);

fail0:
	dbg_print("<-----\n");
	return -1;
}

static void __exit cmpc_tsd_exit(void)
{
	dbg_print("----->\n");

	acpi_bus_unregister_driver(&tsd_drv);
	kfree(tsd_dev);
	
	dbg_print("<-----\n");
}

static int cmpc_tsd_open(struct inode *inode, struct file *file)
{
	dbg_print("----->\n");
	dbg_print("<-----\n");
	return 0;
}

static int cmpc_tsd_release(struct inode *inode, struct file *file)
{
	dbg_print("----->\n");
	if (flag) 
        //notify the data is available
        wake_up_interruptible(&outq);
   
	dbg_print("<-----\n");
	return 0;
}

static int cmpc_tsd_ioctl(struct inode *inode, struct file *file,
	unsigned int cmd, unsigned long arg)
{
	int result=0;
	dbg_print("----->\n");
	switch (cmd)
	{
	case IOCTL_REGISTER_EVENT:
		/** install acpi notify handler */
		if (!ACPI_SUCCESS(acpi_install_notify_handler(tsd_dev->device->handle,
			ACPI_DEVICE_NOTIFY, cmpc_tsd_handler, tsd_dev))) {
			dbg_print("acpi_install_notify_handler failed\n");
		}
		break;
	case IOCTL_UNREGISTER_EVENT:
		if (!ACPI_SUCCESS(acpi_remove_notify_handler(tsd_dev->device->handle, ACPI_DEVICE_NOTIFY, 
		cmpc_tsd_handler))) {
			dbg_print("acpi_remove_notify_handler failed\n");
		}
		break;
	case IOCTL_DBG_SIGNAL_EVENT:
		cmpc_tsd_get_status(&result);
		if (copy_to_user((void __user  *)arg, &result, sizeof(int)))
		//if (put_user(result,(int *) arg))
		{
			return - EFAULT;
		}
		break;
	default:
		dbg_print("not supported cmd\n");

		return -EINVAL;
	}
	dbg_print("<-----\n");
	return 0;
}


/** get current tablet sensor status: clamshell or tablet mode */
static int cmpc_tsd_get_status(int *res) {

	struct acpi_object_list params;
	union acpi_object in_objs;
	union acpi_object out_obj;
	struct acpi_buffer result, *resultp;
	acpi_status status;
	int success = 0;
	dbg_print("----->\n");
	*res = 0;

	result.length = sizeof(out_obj);
	result.pointer = &out_obj;
	resultp = &result;

	params.count = 1;
	params.pointer = &in_objs;
	in_objs.integer.value = 1;
	in_objs.type = ACPI_TYPE_INTEGER;
	if (!ACPI_SUCCESS(status = acpi_evaluate_object(
		tsd_dev->device->handle, "TCMD", &params, resultp))) {
		dbg_print("acpi_evaluate_object failed\n");
		goto fail;
	}
	if (res)
		*res = out_obj.integer.value;

	success = status == AE_OK && out_obj.type == ACPI_TYPE_INTEGER;
	if (!success)
		dbg_print("acpi_evaluate_object failed\n");
	return success;

fail:
	dbg_print("<-----\n");
	return -1;
}

static ssize_t cmpc_tsd_read(struct file *file, char __user *buf,
	size_t size, loff_t *ppos)
{
	int result = 0;
	dbg_print("----->\n");
	if (wait_event_interruptible(outq, flag) != 0)
	{
		return - ERESTARTSYS;
	}

	flag = 0;
	cmpc_tsd_get_status(&result);

	if (copy_to_user(buf, &result, sizeof(int)))
	{
		return - EFAULT;
	}

	dbg_print("<-----\n");
	return 0;
//	return sizeof(int);
}

static ssize_t cmpc_tsd_write(struct file *file, const char __user *buf,
	size_t size, loff_t *ppos)
{
	dbg_print("----->\n");
	dbg_print("<-----\n");
	return 0;
}

static loff_t cmpc_tsd_llseek(struct file *file, loff_t offset, int orig)
{
	dbg_print("----->\n");
	dbg_print("<-----\n");
	return 0;
}


static void cmpc_tsd_handler(acpi_handle handle, u32 event, void *data)
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
	acpi_bus_generate_proc_event(tsd_dev->device, event, 123);	
}


static int cmpc_tsd_add(struct acpi_device *device) {
	
	dbg_print("----->\n");

	tsd_entry = create_proc_entry(CMPC_TSD_NAME, S_IRUGO, acpi_root_dir);
	
	if (!tsd_entry) {
		dbg_print("create_proc_entry failed\n");
		goto fail0;
	}	
	tsd_entry->owner = THIS_MODULE;
	tsd_entry->data = tsd_dev;
	tsd_entry->proc_fops = &tsd_fops;
	
	tsd_dev->device = device;
	acpi_driver_data(device) = tsd_dev;
	strcpy(acpi_device_name(device), CMPC_TSD_NAME);
	sprintf(acpi_device_class(device), "%s/%s", CMPC_CLASS_NAME,
		CMPC_TSD_NAME);

	/** install acpi notify handler */
	if (!ACPI_SUCCESS(acpi_install_notify_handler(device->handle,
		ACPI_DEVICE_NOTIFY, cmpc_tsd_handler, tsd_dev))) {
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

/*fail2:
	acpi_remove_notify_handler(tsd_dev->device->handle, ACPI_DEVICE_NOTIFY, 
		cmpc_tsd_handler);
*/
fail1:
	remove_proc_entry(CMPC_TSD_NAME, acpi_root_dir);
	
fail0:
	dbg_print("<-----\n");
	return -ENODEV;
}

static int cmpc_tsd_resume(struct acpi_device *device) {
	dbg_print("----->\n");
	dbg_print("<-----\n");
	return 0;
}

static int cmpc_tsd_remove(struct acpi_device *device, int type) {
	dbg_print("----->\n");
	acpi_remove_notify_handler(tsd_dev->device->handle, ACPI_DEVICE_NOTIFY, 
		cmpc_tsd_handler);
	remove_proc_entry(CMPC_TSD_NAME, acpi_root_dir);
	dbg_print("<-----\n");
	return 0;
}

module_init(cmpc_tsd_init);
module_exit(cmpc_tsd_exit);
