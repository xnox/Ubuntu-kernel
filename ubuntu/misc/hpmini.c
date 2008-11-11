/*
 * Copyright (C) 2008 Hewlett-Packard Development Company, L.P.
 *
 * Author: Kevin Barnett (kevin.barnett@hp.com)
 *
 * This driver enables status monitoring and control of the following devices on
 * the HP Mini via sysfs:
 *
 * - Bluetooth (device & radio)
 * - Wifi (radio only)
 * - WWAN (device & radio)
 * - Ethernet (device only - there is no radio)
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
#include <linux/pci.h>
#include <linux/ioport.h>
#include <linux/spinlock.h>
#include <linux/hwmon-sysfs.h>
#include <linux/hwmon.h>
#include <linux/sysfs.h>
#include <asm/uaccess.h>
#include <asm/io.h>

/* module and version information */
#define DRIVER_NAME             "HP Mini"
#define DRIVER_VERSION          "1.00"
#define DRIVER_RELEASE_DATE     "06-Nov-2008"
#define PREFIX                  DRIVER_NAME ": "

/* Change this to non-zero to enable debug output. */
#define HP_MINI_DEBUG   0

/* Change this to non-zero to allow the Ethernet support to be disabled.  This is
 * not allowed at present because we don't handle the Ethernet support being disabled
 * very gracefully.
 */
#define ETHERNET_DISABLE_ALLOWED    0

#if HP_MINI_DEBUG
#define DPRINT(format, args...) printk(KERN_DEBUG PREFIX format, ##args)
#else
#define DPRINT(format, args...)
#endif

static struct mutex gpio_mutex;

static struct platform_device *hpmini_platform_device;

/* the base address of I/O space for GPIO as read from the ICH7 LPC Interface Bridge */
static u32 gpio_base_address;

/* address of 32-bit GPIO base address register on the ICH7 LPC Interface Bridge */ 
#define GPIO_BASE_ADDRESS_REGISTER  0x48

/* Only bits 15:6 of the GPIO base address register are valid. */
#define GPIO_BASE_ADDRESS_MASK      0xffc0

/* offsets from the I/O base address for specific GPIO signals */
#define GPIO_9_THROUGH_10_OFFSET    0xd
#define GPIO_26_THROUGH_28_OFFSET   0xf

/* GPIO signals */
#define BLUETOOTH_OFF               0x2     /* GPIO9 */
#define WWAN_RADIO_ON               0x4     /* GPIO26 */
#define WIFI_RADIO_ON               0x8     /* GPIO27 */
#define ETHERNET_ON                 0x10    /* GPIO28 */

/*
 * sysfs callback functions and files
 */

static ssize_t bluetooth_show(struct device *dev, struct device_attribute *devattr, char *buf)
{
	u8 data;

	data = inb(gpio_base_address + GPIO_9_THROUGH_10_OFFSET);

    DPRINT("%s: status = 0x%x\n", __FUNCTION__, data);

    return(snprintf(buf, PAGE_SIZE, "%u\n", (data & BLUETOOTH_OFF) ? 0 : 1));
}

static ssize_t bluetooth_set(struct device *dev, struct device_attribute *devattr,
               const char *buf, size_t count)
{
    unsigned long val;
	u8 original_data;
	u8 new_data;

    val = simple_strtoul(buf, NULL, 10);

    DPRINT("%s: new value = %lu\n", __FUNCTION__, val);

	mutex_lock(&gpio_mutex);

	original_data = inb(gpio_base_address + GPIO_9_THROUGH_10_OFFSET);
    new_data = original_data;

    if (val)
    {
        new_data &= ~BLUETOOTH_OFF;
    }
    else
    {
        new_data |= BLUETOOTH_OFF;
    }

    if (new_data != original_data)
    {
	    outb(new_data, gpio_base_address + GPIO_9_THROUGH_10_OFFSET);
    }

	mutex_unlock(&gpio_mutex);

    return(count);
}

static ssize_t wifi_show(struct device *dev, struct device_attribute *devattr, char *buf)
{
	u8 data;

	data = inb(gpio_base_address + GPIO_26_THROUGH_28_OFFSET);

    DPRINT("%s: status = 0x%x\n", __FUNCTION__, data);

    return(snprintf(buf, PAGE_SIZE, "%u\n", (data & WIFI_RADIO_ON) ? 1 : 0));
}

static ssize_t wifi_set(struct device *dev, struct device_attribute *devattr,
               const char *buf, size_t count)
{
    unsigned long val;
	u8 original_data;
	u8 new_data;

    val = simple_strtoul(buf, NULL, 10);

    DPRINT("%s: new value = %lu\n", __FUNCTION__, val);

	mutex_lock(&gpio_mutex);

	original_data = inb(gpio_base_address + GPIO_26_THROUGH_28_OFFSET);
    new_data = original_data;

    if (val)
    {
        new_data |= WIFI_RADIO_ON;
    }
    else
    {
        new_data &= ~WIFI_RADIO_ON;
    }

    if (new_data != original_data)
    {
	    outb(new_data, gpio_base_address + GPIO_26_THROUGH_28_OFFSET);
    }

	mutex_unlock(&gpio_mutex);

    return(count);
}

static ssize_t wwan_show(struct device *dev, struct device_attribute *devattr, char *buf)
{
	u8 data;

	data = inb(gpio_base_address + GPIO_26_THROUGH_28_OFFSET);

    DPRINT("%s: status = 0x%x\n", __FUNCTION__, data);

    return(snprintf(buf, PAGE_SIZE, "%u\n", (data & WWAN_RADIO_ON) ? 1 : 0));
}

static ssize_t wwan_set(struct device *dev, struct device_attribute *devattr,
               const char *buf, size_t count)
{
    unsigned long val;
	u8 original_data;
	u8 new_data;

    val = simple_strtoul(buf, NULL, 10);

    DPRINT("%s: new value = %lu\n", __FUNCTION__, val);

	mutex_lock(&gpio_mutex);

	original_data = inb(gpio_base_address + GPIO_26_THROUGH_28_OFFSET);
    new_data = original_data;

    if (val)
    {
        new_data |= WWAN_RADIO_ON;
    }
    else
    {
        new_data &= ~WWAN_RADIO_ON;
    }

    if (new_data != original_data)
    {
    	outb(new_data, gpio_base_address + GPIO_26_THROUGH_28_OFFSET);
    }

	mutex_unlock(&gpio_mutex);

    return(count);
}

static ssize_t ethernet_show(struct device *dev, struct device_attribute *devattr, char *buf)
{
	u8 data;

	data = inb(gpio_base_address + GPIO_26_THROUGH_28_OFFSET);

    DPRINT("%s: status = 0x%x\n", __FUNCTION__, data);

    return(snprintf(buf, PAGE_SIZE, "%u\n", (data & ETHERNET_ON) ? 1 : 0));
}

static ssize_t ethernet_set(struct device *dev, struct device_attribute *devattr,
               const char *buf, size_t count)
{
    unsigned long val;
	u8 original_data;
	u8 new_data;

    val = simple_strtoul(buf, NULL, 10);

    DPRINT("%s: new value = %lu\n", __FUNCTION__, val);

#if !ETHERNET_DISABLE_ALLOWED
    if (!val)
    {
        printk(KERN_INFO PREFIX "%s: disabling Ethernet is not allowed, so failing attempt to disable Ethernet\n", __FUNCTION__);
        return(-1);
    }
#endif

	mutex_lock(&gpio_mutex);

	original_data = inb(gpio_base_address + GPIO_26_THROUGH_28_OFFSET);
    new_data = original_data;

    if (val)
    {
        new_data |= ETHERNET_ON;
    }
    else
    {
        new_data &= ~ETHERNET_ON;
    }

    if (new_data != original_data)
    {
    	outb(new_data, gpio_base_address + GPIO_26_THROUGH_28_OFFSET);
    }

	mutex_unlock(&gpio_mutex);

    return(count);
}

static SENSOR_DEVICE_ATTR(bluetooth,            /* name */
                          S_IWUSR | S_IRUGO,    /* mode */
                          bluetooth_show,       /* show */
                          bluetooth_set,        /* store */
                          0);                   /* index */

static SENSOR_DEVICE_ATTR(wifi,                 /* name */
                          S_IWUSR | S_IRUGO,    /* mode */
                          wifi_show,            /* show */
                          wifi_set,             /* store */
                          0);                   /* index */

static SENSOR_DEVICE_ATTR(wwan,                 /* name */
                          S_IWUSR | S_IRUGO,    /* mode */
                          wwan_show,            /* show */
                          wwan_set,             /* store */
                          0);                   /* index */

static SENSOR_DEVICE_ATTR(ethernet,             /* name */
                          S_IWUSR | S_IRUGO,    /* mode */
                          ethernet_show,        /* show */
                          ethernet_set,         /* store */
                          0);                   /* index */

static struct attribute *hpmini_attributes[] =
{
    &sensor_dev_attr_bluetooth.dev_attr.attr,
    &sensor_dev_attr_wifi.dev_attr.attr,
    &sensor_dev_attr_wwan.dev_attr.attr,
    &sensor_dev_attr_ethernet.dev_attr.attr,
    NULL
};

static const struct attribute_group hpmini_group_radio =
{
    .attrs = hpmini_attributes,
};

static int __devinit hpmini_probe(struct platform_device *dev)
{
    int status;
    struct pci_dev *pcidev;

    pcidev = pci_get_device(PCI_VENDOR_ID_INTEL,            /* vendor */
                            PCI_DEVICE_ID_INTEL_ICH7_1,     /* device */
                            NULL);                          /* from */

    if (pcidev == NULL)
    {
        printk(KERN_ERR PREFIX "ICH7-M LPC interface bridge not found\n");
        return(-EIO);
    }

    status = pci_enable_device(pcidev);

    if (status != 0)
    {
        printk(KERN_ERR PREFIX "unable to enable ICH7-M LPC interface bridge\n");
        return(-EIO);
    }

    pci_read_config_dword(pcidev, GPIO_BASE_ADDRESS_REGISTER, &gpio_base_address);

    /* Only bits 15:6 of the GPIO base address register are valid, so mask off
     * all of the other bits.
     */
    gpio_base_address &= GPIO_BASE_ADDRESS_MASK;     

    DPRINT("base address = 0x%x\n", gpio_base_address);

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

	mutex_init(&gpio_mutex);

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
