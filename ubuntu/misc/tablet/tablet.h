/**
 *  /file tablet.h
 *
 *  /version 0.6.0.0
 *  /date 05-29-2008
 */
 
#ifndef __CMPC_TABLETSENSOR_DRIVER_H__
#define __CMPC_TABLETSENSOR_DRIVER_H__

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
#define CMPC_TSD_NAME   "tablet"

#define TABLET_SENSOR_MAGIC 0xfc
#define REGISTER_EVENT_CMD 0x0
#define DBG_SIGNAL_EVENT_CMD 0x01
#define UNREGISTER_EVENT_CMD 0x10

// Tablet sensor status change notification value
#define TSD_DEVICE_INFO_CHANGE     0x81

#define IOCTL_REGISTER_EVENT _IO( TABLET_SENSOR_MAGIC, REGISTER_EVENT_CMD )

#define IOCTL_UNREGISTER_EVENT _IO( TABLET_SENSOR_MAGIC, UNREGISTER_EVENT_CMD )

#define IOCTL_DBG_SIGNAL_EVENT _IOR( TABLET_SENSOR_MAGIC, DBG_SIGNAL_EVENT_CMD, int)


#endif  /*  __CMPC_tsd_H__ */
