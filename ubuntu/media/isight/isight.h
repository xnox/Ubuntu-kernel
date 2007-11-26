/*
 * Copyright (C) 2006 Ivan N. Zlatev <contact@i-nz.net>
 *
 * Based on extract.c by Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 * Firmware loading specifics by Johannes Berg <johannes@sipsolutions.net>
 * at http://johannes.sipsolutions.net/MacBook/iSight
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA
 */

#ifndef _ISIGHT_H_
#define _ISIGHT_H_

#include <linux/usb.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/crypto.h>
#include <linux/firmware.h>
#include <linux/mm.h>
#include <linux/scatterlist.h>

#include "uvcvideo.h"

int is_isight (struct usb_device *dev);
int isight_load_firmware (struct usb_device *dev);
int isight_decode_video (struct uvc_video_queue *, struct uvc_buffer *, const __u8 *data, unsigned int len);

#endif

