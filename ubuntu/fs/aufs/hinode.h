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

/* $Id: hinode.h,v 1.7 2007/10/22 02:15:35 sfjro Exp $ */

#ifndef __AUFS_HINODE_H__
#define __AUFS_HINODE_H__

#ifdef __KERNEL__

#include <linux/fs.h>
#include <linux/inotify.h>
#include <linux/version.h>
#include <linux/aufs_type.h>
//#include "branch.h"
#include "inode.h"
//#include "vfsub.h"

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 18)
#else
struct inotify_watch {
	/* empty */
};
#endif

/* ---------------------------------------------------------------------- */

struct aufs_hinotify {
#ifdef CONFIG_AUFS_HINOTIFY
	struct inotify_watch	hin_watch;
	struct inode		*hin_aufs_inode;	/* no get/put */

	/* an array of atomic_t X au_hin_nignore */
	atomic_t		hin_ignore[0];
#endif
};

struct aufs_hinode {
	struct inode		*hi_inode;
	aufs_bindex_t		hi_id;
#ifdef CONFIG_AUFS_HINOTIFY
	struct aufs_hinotify	*hi_notify;
#endif

	/* reference to the copied-up whiteout with get/put */
	struct dentry		*hi_whdentry;
};

struct aufs_hin_ignore {
#ifdef CONFIG_AUFS_HINOTIFY
	__u32			ign_events;
	struct aufs_hinode	*ign_hinode;
#endif
};

/* ---------------------------------------------------------------------- */

#ifdef CONFIG_AUFS_HINOTIFY
static inline
void au_hin_init(struct aufs_hinode *hinode, struct aufs_hinotify *val)
{
	hinode->hi_notify = val;
}

/* hinotify.c */

int au_hin_alloc(struct aufs_hinode *hinode, struct inode *inode,
		   struct inode *h_inode);
void au_hin_free(struct aufs_hinode *hinode);
void au_do_hdir_lock(struct inode *h_dir, struct inode *dir,
		     aufs_bindex_t bindex, unsigned int lsc);
void hdir_unlock(struct inode *h_dir, struct inode *dir, aufs_bindex_t bindex);
void hdir_lock_rename(struct dentry **h_parents, struct inode **dirs,
		      aufs_bindex_t bindex, int issamedir);
void hdir_unlock_rename(struct dentry **h_parents, struct inode **dirs,
			aufs_bindex_t bindex, int issamedir);
void au_reset_hinotify(struct inode *inode, unsigned int flags);

void au_hin_ignore(struct aufs_hinode *hinode, __u32 events);
void au_hin_unignore(struct aufs_hinode *hinode, __u32 events);

int __init au_inotify_init(void);
void au_inotify_fin(void);
#else
static inline
void au_hin_init(struct aufs_hinode *hinode, struct aufs_hinotify *val)
{
	/* empty */
}

static inline
int au_hin_alloc(struct aufs_hinode *hinode, struct inode *inode,
		   struct inode *h_inode)
{
	return -EOPNOTSUPP;
}

static inline void au_hin_free(struct aufs_hinode *hinode)
{
	/* nothing */
}

static inline void au_do_hdir_lock(struct inode *h_dir, struct inode *dir,
				   aufs_bindex_t bindex, unsigned int lsc)
{
	vfsub_i_lock_nested(h_dir, lsc);
}

static inline
void hdir_unlock(struct inode *h_dir, struct inode *dir, aufs_bindex_t bindex)
{
	vfsub_i_unlock(h_dir);
}

static inline
void hdir_lock_rename(struct dentry **h_parents, struct inode **dirs,
		      aufs_bindex_t bindex, int issamedir)
{
	vfsub_lock_rename(h_parents[0], h_parents[1]);
}

static inline
void hdir_unlock_rename(struct dentry **h_parents, struct inode **dirs,
			aufs_bindex_t bindex, int issamedir)
{
	vfsub_unlock_rename(h_parents[0], h_parents[1]);
}

static inline void au_reset_hinotify(struct inode *inode, unsigned int flags)
{
	/* nothing */
}

static inline __u32 au_notify_change_events(struct iattr *ia)
{
	return 0;
}

static inline void au_hin_ignore(struct aufs_hinotify *hinotify, __u32 events)
{
	/* nothing */
}

static inline void au_hin_unignore(struct aufs_hinotify *hinotify, __u32 events)
{
	/* nothing */
}

#define au_inotify_init()	0
#define au_inotify_fin()	do {} while (0)
#endif /* CONFIG_AUFS_HINOTIFY */

/* ---------------------------------------------------------------------- */

/*
 * hgdir_lock, hdir_lock, hdir2_lock
 */
#define LockFunc(name, lsc) \
static inline \
void name##_lock(struct inode *h_dir, struct inode *dir, aufs_bindex_t bindex) \
{ au_do_hdir_lock(h_dir, dir, bindex, AuLsc_I_##lsc); }

LockFunc(hdir, PARENT);
LockFunc(hdir2, PARENT2);

#undef LockFunc

/* ---------------------------------------------------------------------- */

#endif /* __KERNEL__ */
#endif /* __AUFS_HINODE_H__ */
