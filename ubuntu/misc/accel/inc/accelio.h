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
 * @file accelio.h
 *  
 * Accelrometer driver IO Interface
 */

#ifndef ACCELIO_H
#define ACCELIO_H

/**
 * To define IOCTL
 */
#define ACCEL_MAGIC_NUMBER 0xFB

/**
 * Acceleration data of x, y and z axes used in driver.
 */
struct accel_raw_data {
	unsigned char accel_raw_x;
	unsigned char accel_raw_y;
	unsigned char accel_raw_z;
};

/**
 * IOCTL to set sensitivity. The sensitivity should be an int number from
 * 1 to 127.
 */
#define IOCTL_ACCEL_SET_SENSE \
	_IOW(ACCEL_MAGIC_NUMBER, 0x01, int)
/**
 * IOCTL to start accelerometer.
 */
#define IOCTL_ACCEL_START \
	_IO(ACCEL_MAGIC_NUMBER, 0x02)
/**
 * IOCTL to stop accelerometer.
 */
#define IOCTL_ACCEL_STOP \
	_IO(ACCEL_MAGIC_NUMBER, 0x03)
/**
 * IOCTL to set g-select. The g-select should be 0 or 1.
 */
#define IOCTL_ACCEL_SET_G_SELECT \
	_IOW(ACCEL_MAGIC_NUMBER, 0x04, int)
/**
 * IOCTL to get g-select. The g-select should be 0 or 1.
 */
#define IOCTL_ACCEL_GET_G_SELECT \
	_IOR(ACCEL_MAGIC_NUMBER, 0x05, int)

#endif /* ACCELIO_H */
