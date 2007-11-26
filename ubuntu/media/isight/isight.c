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

#include <linux/usb.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/crypto.h>
#include <linux/firmware.h>
#include <linux/mm.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>

#include "isight.h"
#include "uvcvideo.h"

#define isight_printk(msg...) \
	printk(KERN_DEBUG "uvcvideo: iSight: " msg)

#define TIMEOUT 300
#define N_ELEMENT(array) (sizeof (array) / sizeof ((array)[0]))

static const struct {
	unsigned char sha1sum[20];
	long offset;
} offsets[] = {
	{ { 0x86, 0x43, 0x0c, 0x04, 0xf9, 0xb6, 0x7c, 0x5c,
		  0x3d, 0x84, 0x40, 0x91, 0x38, 0xa7, 0x67, 0x98,
		  0x27, 0x02, 0x5e, 0xc2 }, 5172 /* 0x1434 */ },
	{ { 0xa1, 0x4c, 0x15, 0x9b, 0x17, 0x6d, 0x27, 0xa6,
		  0xe9, 0x8d, 0xcb, 0x5d, 0xea, 0x5d, 0x78, 0xb8,
		  0x1e, 0x15, 0xad, 0x41 }, 9176 /* 0x23D8 */ },
	{ { 0xc6, 0xc9, 0x4d, 0xd7, 0x7b, 0x86, 0x4f, 0x8b,
		  0x2d, 0x31, 0xab, 0xf3, 0xcb, 0x2d, 0xe4, 0xc9,
		  0xd1, 0x39, 0xe1, 0xbf }, 0x1434 },
	{ { 0x01, 0xe2, 0x91, 0xd5, 0x29, 0xe7, 0xc1, 0x8d,
		  0xee, 0xa2, 0xeb, 0xa2, 0x52, 0xd1, 0x81, 0x14,
		  0xe0, 0x96, 0x27, 0x6e }, 0x2060 },
};



/* A record looks like:
   __be16 data_length;
   __be16 value;
   __u8   data[data_length];
 */

static int isight_upload_firmware (struct usb_device *dev, 
				   unsigned char *data, size_t size, long offset)
{
	int position = 0, success = 0;
	int record_size, record_val, chunk_size;
	unsigned char *chunk_ptr;
	unsigned char *data_k;

	if (offset > size) {
		return -1;
	}

	data_k = kmalloc (size, GFP_KERNEL);
	if (data_k == NULL) {
		isight_printk ("Unable to kalloc memory. \n");
		return -1;
	}
	memcpy (data_k, data, size);
	data = data_k;

	data += offset;

	if (usb_control_msg (dev, usb_rcvctrlpipe (dev, 0), 0xA0, 0x40, 0xe600, 0, 
				"\1", 1, TIMEOUT) < 0) {
		isight_printk ("firmware loading init failed\n");
		success = -1;
		goto end;
	}

	while (1) {
		if (position > size) {
			goto end;
		}

		record_size = (data[position + 0] << 8) | data[position + 1];
		record_val = (data[position + 2] << 8) | data[position + 3];

		position += 4;

		if (record_size == 0x8001) { /* success */
			goto end;
		} 
		else if (record_size == 0) {
			continue;
		}
	       	else if (record_size < 0 || record_size >= 1024 || 
				position + record_size > size) {
			isight_printk ("invalid firmware record_size: %X \n", record_size);
			success = -1;
			goto end;
		}
		
		/* Upload to usb bus in 50 bytes chunks, 
		   where the last can be less than 50
		 */
		
		chunk_ptr = &data[position];
		position += record_size;

		while (record_size > 0) {
			chunk_size = record_size > 50 ? 50 : record_size;
			record_size -= chunk_size;
			if (usb_control_msg (dev, usb_sndctrlpipe (dev, 0), 0xA0, 0x40,  
						record_val, 0, chunk_ptr, chunk_size,
					       	TIMEOUT) != chunk_size) {
				isight_printk ("firmware upload failed.\n");
				success = -1;
				goto end;
			}
			chunk_ptr += chunk_size;
			record_val += 50;
		}
	}

end:
	if (usb_control_msg (dev, usb_sndctrlpipe (dev, 0), 0xA0, 0x40, 0xe600, 0,
				    "\0", 1, TIMEOUT) < 0) {
		isight_printk ("firmware loading finish-up failed.\n");
		success = -1;
	}

	kfree (data_k);
	return success;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)
static void isight_sha1 (char *result, unsigned char *data, int size)
{
	struct crypto_tfm *tfm;
	struct scatterlist sg[1];

	if (result == NULL)
		return;
	memset (result, 0x00, 20);
	sg_set_buf (sg, data, size);
	tfm = crypto_alloc_tfm ("sha1", 0);
	
	if (tfm != NULL) {
		crypto_digest_init (tfm);
		crypto_digest_update (tfm, sg, 1);
		crypto_digest_final (tfm, result);
		crypto_free_tfm (tfm);
	}
}
#else
static void isight_sha1 (char *result, unsigned char *data, int size)
{
       struct hash_desc desc;
       struct scatterlist sg[1];

       if (result == NULL)
               return;
       memset (result, 0x00, 20);
       sg_set_buf (sg, data, size);

       desc.tfm = crypto_alloc_hash ("sha1", 0, CRYPTO_ALG_ASYNC);
       desc.flags = CRYPTO_TFM_REQ_MAY_SLEEP;

       if (desc.tfm != NULL) {
               crypto_hash_init (&desc);
               crypto_hash_update (&desc, sg, size);
               crypto_hash_final (&desc, result);
               crypto_free_hash (desc.tfm);
       }
}
#endif

/* returns the offset where the firmware blob starts, else -1 */
static long isight_extract_firmware_offset (unsigned char *data, size_t size)
{
	char sha1sum[20];
	int i, offset;

	if (data == NULL ||size <= 0) {
		return -1;
	}

	isight_sha1 (sha1sum, data, size);

	offset = -1;
	for (i=0; i < N_ELEMENT (offsets); i++) {
		if (memcmp (offsets[i].sha1sum, sha1sum, 20) == 0) {
			offset = offsets[i].offset;
			break;
		}
	}

	return offset;
}


int isight_load_firmware (struct usb_device *dev)
{
	long offset;
	size_t size;
	int success = -1;
	const struct firmware *fw;
	unsigned char *data_k = NULL;


	if (dev->descriptor.idVendor == 0x05ac &&
	    dev->descriptor.idProduct == 0x8300 &&
	    dev->descriptor.bDeviceClass == 0xff &&
	    dev->descriptor.bDeviceSubClass == 0xff &&
	    dev->descriptor.bDeviceProtocol == 0xff) {

		/* request firmware using the firmware_class kernel interface */
		if (request_firmware (&fw, "AppleUSBVideoSupport", &dev->dev) != 0 ) {
			isight_printk ("firmware file AppleUSBVideoSupport not found.\n");
			return -1;
		}
		
		/* copy to kernel memory - required*/
		data_k = kmalloc (fw->size, GFP_KERNEL);
		if (data_k == NULL) {
			isight_printk ("can't kalloc memory.");
			return -1;
		}
		memcpy (data_k, fw->data, fw->size);
		size = fw->size;
		release_firmware (fw);

		offset = isight_extract_firmware_offset (data_k, size);
		if (offset == -1) {
			isight_printk ("invalid firmware file\n");
		} 
		else {
			if (isight_upload_firmware (dev, data_k, size, offset) == 0) {
				isight_printk ("firmware successfully loaded.\n");
				success = 0;
			}
		}
		kfree (data_k);
	} 
	else if (dev->descriptor.idVendor == 0x05ac &&
	    	 dev->descriptor.idProduct == 0x8501) {
		isight_printk ("firmware already loaded.\n");
		success = 0;
	}

	return success;
}


int isight_decode_video (struct uvc_video_queue *queue, struct uvc_buffer *buf, 
		const __u8 *data, unsigned int len)
{
	static const __u8 hdr[] = {
		0x11, 0x22, 0x33, 0x44, 0xde, 0xad, 0xbe, 0xef,
		0xde, 0xad, 0xfa, 0xce
	};

	unsigned int maxlen, nbytes;
	__u8 *mem;
	int is_header = 0;

	if (buf == NULL)
		return 0;

	/* Built-in iSight webcams are completely broken. They implement most
	 * of UVC 1.0, but the Apple engineers decided to use a completely
	 * different packet format, although the video data is in YUV. Were
	 * they on crack or just lazy ? As the hardware is 8051-based, it
	 * might be interesting to write an open-source firmware.
	 *
	 * Instead of sending a header at the beginning of each isochronous
	 * transfer payload, the webcam sends a single header per image (on
	 * its own in a packet), followed by packets containing data only.
	 *
	 * Offset	Size (bytes)	Description
	 * ------------------------------------------------------------------
	 * 0x00		1		Header length
	 * 0x01		1		Flags (UVC-compliant)
	 * 0x02		4		Always equal to '11223344'
	 * 0x06		8		Always equal to 'deadbeefdeadface'
	 * 0x0e		16		Unknown
	 *
	 * The header can be prefixed by an optional, unknown-purpose byte.
	 */

	/* Detect the packet type. */
	if ((len >= 14 && memcmp (&data[2], hdr, 12) == 0) ||
	    (len >= 15 && memcmp (&data[3], hdr, 12) == 0)) {
		uvc_trace(UVC_TRACE_FRAME, "iSight header found\n");
		is_header = 1;
	}

	/* Synchronize to the input stream by waiting for a header packet. */
	if (buf->state != UVC_BUF_STATE_ACTIVE) {
		if (!is_header) {
			uvc_trace(UVC_TRACE_FRAME, "Dropping packet (out of "
				"sync).\n");
			return 0;
		}

		buf->state = UVC_BUF_STATE_ACTIVE;
	}

	/* Mark the buffer as done if we're at the beginning of a new frame.
	 *
	 * Empty buffers (bytesused == 0) don't trigger end of frame detection
	 * as it doesn't make sense to return an empty buffer.
	 */
	if (is_header && buf->buf.bytesused != 0) {
		buf->state = UVC_BUF_STATE_DONE;
		return -EAGAIN;
	}

	/* Copy the video data to the buffer. Skip header packets, as they
	 * contain no data.
	 */
	if (!is_header) {
		maxlen = buf->buf.length - buf->buf.bytesused;
		mem = queue->mem + buf->buf.m.offset + buf->buf.bytesused;
		nbytes = min(len, maxlen);
		memcpy(mem, data, nbytes);
		buf->buf.bytesused += nbytes;

		/* Drop the current frame if the buffer size was exceeded. */
		if (len > maxlen || buf->buf.bytesused == buf->buf.length) {
			uvc_trace(UVC_TRACE_FRAME, "Frame complete (overflow).\n");
			buf->state = UVC_BUF_STATE_DONE;
		}
	}

	return 0;
}

int is_isight (struct usb_device *dev)
{
	if (dev->descriptor.idVendor == 0x05ac &&
	     (dev->descriptor.idProduct == 0x8300) &&
	    dev->descriptor.bDeviceClass == 0xff &&
	    dev->descriptor.bDeviceSubClass == 0xff &&
	    dev->descriptor.bDeviceProtocol == 0xff) {
		return 0;
	} 
	else if (dev->descriptor.idVendor == 0x05ac &&
			dev->descriptor.idProduct == 0x8501) {
		return 0;
	}
	return -1;
}
