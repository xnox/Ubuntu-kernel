/*
 * Copyright (C) 2005, 2006, 2007 Junjiro Okajima
 *
 * This program, aufs is free software; you can redistribute it and/or modify
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

/* $Id: aufs.h,v 1.40 2007/12/03 01:37:26 sfjro Exp $ */

#ifndef __AUFS_H__
#define __AUFS_H__

#ifdef __KERNEL__

#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 18)
#include <linux/uaccess.h>
#else
#include <asm/uaccess.h>
#endif

/* ---------------------------------------------------------------------- */

/* limited support before 2.6.16, curretly 2.6.15 only. */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 16)
#define timespec_to_ns(ts)	({ (long long)(ts)->tv_sec; })
#define D_CHILD			d_child
#else
#define D_CHILD			d_u.d_child
#endif

#include <linux/types.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 17)
typedef unsigned long blkcnt_t;
#endif

#include <linux/list.h>
#include <linux/sysfs.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 22)
typedef struct kset au_subsys_t;
#define au_subsys_to_kset(subsys) (subsys)
#else
typedef struct subsystem au_subsys_t;
#define au_subsys_to_kset(subsys) ((subsys).kset)
#endif

#include <linux/gfp.h>
#ifndef GFP_TEMPORARY
#define GFP_TEMPORARY GFP_KERNEL
#endif

#include <linux/fs.h>
#ifndef FMODE_EXEC
/* introduced linux-2.6.17 */
#define FMODE_EXEC 0
#endif

#include <linux/compiler.h>
#ifndef __packed
#define __packed	__attribute__((packed))
#endif
#ifndef __aligned
#define __aligned(x)	__attribute__((aligned(x)))
#endif

/* ---------------------------------------------------------------------- */

#define _AuNoNfsBranchMsg "NFS branch is not supported"
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 16)
#define AuNoNfsBranch
#define AuNoNfsBranchMsg _AuNoNfsBranchMsg
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 19) \
	&& (!defined(CONFIG_AUFS_LHASH_PATCH) \
	    || !(defined(CONFIG_AUFS) \
		 || defined(CONFIG_AUFS_PUT_FILP_PATCH)) \
	    || defined(CONFIG_AUFS_FAKE_DM))
#define AuNoNfsBranch
#define AuNoNfsBranchMsg _AuNoNfsBranchMsg \
	", try some configurations and patches included in aufs source CVS"
#endif

/* ---------------------------------------------------------------------- */

#include "debug.h"

#include "branch.h"
#include "cpup.h"
#include "dcsub.h"
#include "dentry.h"
#include "dir.h"
#include "file.h"
#include "hinode.h"
#include "inode.h"
#include "misc.h"
#include "module.h"
#include "opts.h"
#include "super.h"
#include "sysaufs.h"
#include "vfsub.h"
#include "whout.h"
#include "wkq.h"
//#include "xattr.h"

/* ---------------------------------------------------------------------- */

#ifdef CONFIG_AUFS_MODULE

/* call ksize() or not */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 22) \
	&& !defined(CONFIG_AUFS_KSIZE_PATCH)
#define ksize(p)	(0U)
#endif

#endif /* CONFIG_AUFS_MODULE */

#endif /* __KERNEL__ */
#endif /* __AUFS_H__ */
