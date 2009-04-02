/*
 * Copyright (C) 2008-2009 Hewlett-Packard Development Company, L.P.
 *
 * Author: Kevin Barnett (kevin.barnett@hp.com)
 *
 * This driver enables status monitoring and control of the following devices on
 * the HP Mini via sysfs:
 *
 * - Bluetooth (device & radio)
 * - Wifi (radio only)
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/hwmon-sysfs.h>
#include <linux/hwmon.h>
#include <linux/sysfs.h>
#include <linux/acpi.h>

/* module and version information */
#define DRIVER_NAME             "HP Mini"
#define DRIVER_VERSION          "1.10"
#define DRIVER_RELEASE_DATE     "31-Mar-2009"
#define PREFIX                  DRIVER_NAME ": "

/* Change this to non-zero to enable debug output. */
#define HP_MINI_DEBUG   0

#if HP_MINI_DEBUG
#define DPRINT(format, args...) printk(PREFIX format, ##args)
#else
#define DPRINT(format, args...)
#endif

static struct platform_device *hpmini_platform_device;

/*
 * manifest constants, structures and functions for HP-specific WMI implementation
 */

#define HP_WMI_READ_WRITE_DATA_GUID         "5FB7F034-2C63-45e9-BE91-3D44E2C707E4"

#define HP_WMI_METHOD_READ_WRITE_4_BYTES    2

struct wmi_input_data_block
{
    u32     signature;
    u32     command;
    u32     command_type;
    u32     data_size;
    u32     data;
};

struct wmi_output_data_block
{
    u32     signature;
    u32     data_size;
};

#define HP_WMI_INPUT_DATA_BLOCK_SIGNATURE   0x55434553

#define HP_WMI_COMMAND_READ_BIOS_CONFIG                 1
#define HP_WMI_COMMAND_TYPE_GET_NETWORK_DEVICE_STATE    5

#define HP_WMI_NETWORK_STATE_WIFI_ON        0x100
#define HP_WMI_NETWORK_STATE_BLUETOOTH_ON   0x10000   

#define HP_WMI_COMMAND_WRITE_BIOS_CONFIG                2
#define HP_WMI_COMMAND_TYPE_SET_NETWORK_DEVICE_STATE    5

#define HP_WMI_NETWORK_STATE_ENABLE_WIFI                    0x1   
#define HP_WMI_NETWORK_STATE_ENABLE_DISABLE_MASK_WIFI       0x100   
#define HP_WMI_NETWORK_STATE_ENABLE_BLUETOOTH               0x2   
#define HP_WMI_NETWORK_STATE_ENABLE_DISABLE_MASK_BLUETOOTH  0x200   

static acpi_status set_network_device_state(u32 network_device_state)
{
	acpi_status status;
	struct acpi_buffer input;
    struct wmi_input_data_block input_block;
    
    DPRINT("%s\n", __FUNCTION__);

    input_block.signature = HP_WMI_INPUT_DATA_BLOCK_SIGNATURE;
    input_block.command = HP_WMI_COMMAND_WRITE_BIOS_CONFIG;
    input_block.command_type = HP_WMI_COMMAND_TYPE_SET_NETWORK_DEVICE_STATE;
    input_block.data_size = sizeof(input_block.data);
    input_block.data = network_device_state;

	input.length = sizeof(input_block);
	input.pointer = &input_block;

	status = wmi_evaluate_method(HP_WMI_READ_WRITE_DATA_GUID,       /* GUID */
                                 1,                                 /* instance */
                                 HP_WMI_METHOD_READ_WRITE_4_BYTES,  /* method_id */
                                 &input,                            /* in */
                                 NULL);                             /* out */

	return(status);
}

static acpi_status get_network_device_state(u32 *network_device_state)
{
	acpi_status status;
	struct acpi_buffer input;
	struct acpi_buffer output;
	union acpi_object *obj;
    struct wmi_input_data_block input_block;
    
    DPRINT("%s\n", __FUNCTION__);

    input_block.signature = HP_WMI_INPUT_DATA_BLOCK_SIGNATURE;
    input_block.command = HP_WMI_COMMAND_READ_BIOS_CONFIG;
    input_block.command_type = HP_WMI_COMMAND_TYPE_GET_NETWORK_DEVICE_STATE;
    input_block.data_size = 0;

	input.length = sizeof(input_block);
	input.pointer = &input_block;

	output.length = ACPI_ALLOCATE_BUFFER;
	output.pointer = NULL;

	status = wmi_evaluate_method(HP_WMI_READ_WRITE_DATA_GUID,       /* GUID */
                                 1,                                 /* instance */
                                 HP_WMI_METHOD_READ_WRITE_4_BYTES,  /* method_id */
                                 &input,                            /* in */
                                 &output);                          /* out */

    DPRINT("%s: status = 0x%x\n", __FUNCTION__, status);

	if (status != AE_OK)
    {
		return(status);
    }

	obj = (union acpi_object *) output.pointer;
	if (obj == NULL)
    {
		return(AE_NULL_OBJECT);
    }
    
	if (obj->type != ACPI_TYPE_BUFFER)
    {
		kfree(obj);
		return(AE_TYPE);
	}
	
	if (obj->buffer.length != 12)
    {
		kfree(obj);
        return(AE_BAD_DATA);
    }

    *network_device_state = *((u32 *) &obj->buffer.pointer[8]);
	
	kfree(obj);
	
	return(AE_OK);
}

/*
 * sysfs callback functions and files
 */

static ssize_t bluetooth_show(struct device *dev, struct device_attribute *devattr, char *buf)
{
    acpi_status status;
    u32 network_device_state;
	
    status = get_network_device_state(&network_device_state);

    if (status != AE_OK)
    {
        return(-1);
    }

    return(snprintf(buf, PAGE_SIZE, "%u\n", (network_device_state & HP_WMI_NETWORK_STATE_BLUETOOTH_ON) ? 1 : 0));
}


static ssize_t bluetooth_set(struct device *dev, struct device_attribute *devattr,
               const char *buf, size_t count)
{
	unsigned long val;
    acpi_status status;
    u32 network_device_state;

    val = simple_strtoul(buf, NULL, 10);

    DPRINT("%s: new value = %lu\n", __FUNCTION__, val);
    
    if (val)
    {
        /* enable the device */
    	network_device_state = HP_WMI_NETWORK_STATE_ENABLE_DISABLE_MASK_BLUETOOTH | HP_WMI_NETWORK_STATE_ENABLE_BLUETOOTH;
    }
    else
    {
        /* disable the device */
    	network_device_state = HP_WMI_NETWORK_STATE_ENABLE_DISABLE_MASK_BLUETOOTH;
    }

	status = set_network_device_state(network_device_state);

    if (status != AE_OK)
    {
        count = -1;
    }
	
    return(count);
}

static ssize_t wifi_show(struct device *dev, struct device_attribute *devattr, char *buf)
{
    acpi_status status;
    u32 network_device_state;
	
    status = get_network_device_state(&network_device_state);

    if (status != AE_OK)
    {
        return(-1);
    }

    return(snprintf(buf, PAGE_SIZE, "%u\n", (network_device_state & HP_WMI_NETWORK_STATE_WIFI_ON) ? 1 : 0));
}

static ssize_t wifi_set(struct device *dev, struct device_attribute *devattr,
               const char *buf, size_t count)
{
	unsigned long val;
    acpi_status status;
    u32 network_device_state;

    val = simple_strtoul(buf, NULL, 10);

    DPRINT("%s: new value = %lu\n", __FUNCTION__, val);
    
    if (val)
    {
        /* enable the device */
    	network_device_state = HP_WMI_NETWORK_STATE_ENABLE_DISABLE_MASK_WIFI | HP_WMI_NETWORK_STATE_ENABLE_WIFI;
    }
    else
    {
        /* disable the device */
    	network_device_state = HP_WMI_NETWORK_STATE_ENABLE_DISABLE_MASK_WIFI;
    }

	status = set_network_device_state(network_device_state);

    if (status != AE_OK)
    {
        count = -1;
    }
	
    return(count);
}

static SENSOR_DEVICE_ATTR(bluetooth,            /* name */
                          S_IWUGO | S_IRUGO,    /* mode */
                          bluetooth_show,       /* show */
                          bluetooth_set,        /* store */
                          0);                   /* index */

static SENSOR_DEVICE_ATTR(wifi,                 /* name */
                          S_IWUGO | S_IRUGO,    /* mode */
                          wifi_show,            /* show */
                          wifi_set,             /* store */
                          0);                   /* index */

static struct attribute *hpmini_attributes[] =
{
    &sensor_dev_attr_bluetooth.dev_attr.attr,
    &sensor_dev_attr_wifi.dev_attr.attr,
    NULL
};

static const struct attribute_group hpmini_group_radio =
{
    .attrs = hpmini_attributes,
};

static int __devinit hpmini_probe(struct platform_device *dev)
{
    return(0);
}

static int __devexit hpmini_remove(struct platform_device *dev)
{
    return(0);
}

static void hpmini_shutdown(struct platform_device *dev)
{
}

#ifdef CONFIG_PM

static int hpmini_suspend(struct platform_device *dev, pm_message_t state)
{
    return(0);
}

static int hpmini_resume(struct platform_device *dev)
{
    return(0);
}

#endif  /* CONFIG_PM */

static struct platform_driver hpmini_driver =
{
    .driver     =
    {
        .name   = "hpmini",
        .owner  = THIS_MODULE,
    },
    .probe      = hpmini_probe,
    .remove     = __devexit_p(hpmini_remove),
    .shutdown   = hpmini_shutdown,
#ifdef CONFIG_PM
    .suspend    = hpmini_suspend,
    .resume     = hpmini_resume,
#endif
};

static int __init hp_mini_init_module(void)
{
    int status;

    printk(KERN_INFO PREFIX "module loaded: v%s (%s)\n",
           DRIVER_VERSION,
           DRIVER_RELEASE_DATE);

    status = platform_driver_register(&hpmini_driver);
    if (status != 0)
    {
        return(status);
    }

    hpmini_platform_device = platform_device_alloc("hpmini", -1);
    if (hpmini_platform_device == NULL)
    {
        platform_driver_unregister(&hpmini_driver);
        return(-ENOMEM);
    }

    status = platform_device_add(hpmini_platform_device);
    if (status != 0)
    {
        platform_device_put(hpmini_platform_device);
        platform_driver_unregister(&hpmini_driver);
        return(status);
    }

    /* Initialize the sysfs interface. */
    status = sysfs_create_group(&hpmini_platform_device->dev.kobj,
                                &hpmini_group_radio); 

    if (status != 0)
    {
        platform_device_unregister(hpmini_platform_device);
        platform_device_put(hpmini_platform_device);
        platform_driver_unregister(&hpmini_driver);
        return(status);
    }

    return(0);
}

static void __exit hp_mini_cleanup_module(void)
{
    sysfs_remove_group(&hpmini_platform_device->dev.kobj,
                       &hpmini_group_radio); 
    platform_device_unregister(hpmini_platform_device);
    platform_driver_unregister(&hpmini_driver);
    printk(KERN_INFO PREFIX "module unloaded\n");
}

module_init(hp_mini_init_module);
module_exit(hp_mini_cleanup_module);

MODULE_AUTHOR("Kevin Barnett <kevin.barnett@hp.com>");
MODULE_DESCRIPTION(DRIVER_NAME " driver v" DRIVER_VERSION);
MODULE_VERSION(DRIVER_VERSION);
MODULE_LICENSE("GPL");
