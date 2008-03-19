/*
 * @lket@ignore-file -- We actually don't want this file for upstream :)
 *
 * Intel Wireless WiMax Connection 2400m
 * Compatibility glue for kernel.h; include right after.
 *
 *
 * NOTE: Most of this code is ripped of from the Linux kernel for
 * backwards compatibility purposes, so the original licensing terms
 * apply; when the code is new, then the licensing terms below apply.
 *
 *
 * Copyright (C) 2006-2007 Intel Corporation <linux-wimax@intel.com>
 * Inaky Perez-Gonzalez <inaky.perez-gonzalez@intel.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 *
 * FIXME: docs
 */

/*
 * Introduced in 2.6.23
 *
 * http://git.kernel.org/?p=linux/kernel/git/torvalds/linux-2.6.git;
 * a=blobdiff;f=include/linux/kernel.h;
 * h=47160fe378c98f84b5c5295a7f987744f163a956;
 * hp=f592df74b3cfb48637e3806e1e048a384582e53c;
 * hb=a83308e60f63749dc1d08acb0d8fa9e2ec13c9a7;
 * hpb=f3d79b20df961880697c8442e1f7bc7969ce50a4
 *
 * Yeah, paste it all together...checkpatch is a bitch
 */
#ifndef PTR_ALIGN
#define PTR_ALIGN(p, a)		((typeof(p))ALIGN((unsigned long)(p), (a)))
#endif
