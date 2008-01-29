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

/* $Id: dentry.h,v 1.37 2007/11/05 01:37:02 sfjro Exp $ */

#ifndef __AUFS_DENTRY_H__
#define __AUFS_DENTRY_H__

#ifdef __KERNEL__

#include <linux/fs.h>
#include <linux/aufs_type.h>
#include "misc.h"
#include "vfsub.h"

/* nameidata open_intent */
enum {
	AuIntent_AUFS,
	AuIntent_BRANCH,
	AuIntent_Last
};

struct aufs_hdintent {
	struct list_head	hdi_list;
	struct file		*hdi_file[AuIntent_Last];
};

struct aufs_hdentry {
	struct dentry		*hd_dentry;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 19) \
	&& !defined(AuNoNfsBranch)
	spinlock_t		hd_lock; /* intest_list */
	struct list_head	*hd_intent_list;
#endif
};

struct aufs_dinfo {
	atomic_t		di_generation;

	struct aufs_rwsem	di_rwsem;
	aufs_bindex_t		di_bstart, di_bend, di_bwh, di_bdiropq;
	struct aufs_hdentry	*di_hdentry;
};

/* nameidata extension */
struct aufs_ndx {
	struct vfsmount *nfsmnt;
	int dlgt;
	struct nameidata *nd;
	struct aufs_branch *br;
	struct file *nd_file;
};

/* ---------------------------------------------------------------------- */

/* dentry.c */
extern struct dentry_operations aufs_dop;
#if !defined(AuNoNfsBranch) || defined(CONFIG_AUFS_DLGT)
struct dentry *au_lkup_one(const char *name, struct dentry *parent, int len,
			   struct aufs_ndx *ndx);
#else
static inline
struct dentry *au_lkup_one(const char *name, struct dentry *parent, int len,
			   struct aufs_ndx *ndx)
{
	//todo: ndx->nd_file = NULL;
	return vfsub_lookup_one_len(name, parent, len);
}
#endif
struct dentry *au_sio_lkup_one(const char *name, struct dentry *parent, int len,
			       struct aufs_ndx *ndx);
int au_lkup_dentry(struct dentry *dentry, aufs_bindex_t bstart, mode_t type,
		   struct nameidata *nd);
int au_lkup_neg(struct dentry *dentry, aufs_bindex_t bindex);
int au_refresh_hdentry(struct dentry *dentry, mode_t type);
int au_reval_dpath(struct dentry *dentry, au_gen_t sgen);

/* dinfo.c */
int au_alloc_dinfo(struct dentry *dentry);
struct aufs_dinfo *dtodi(struct dentry *dentry);

void di_read_lock(struct dentry *d, int flags, unsigned int lsc);
void di_read_unlock(struct dentry *d, int flags);
void di_downgrade_lock(struct dentry *d, int flags);
void di_write_lock(struct dentry *d, unsigned int lsc);
void di_write_unlock(struct dentry *d);
void di_write_lock2_child(struct dentry *d1, struct dentry *d2, int isdir);
void di_write_lock2_parent(struct dentry *d1, struct dentry *d2, int isdir);
void di_write_unlock2(struct dentry *d1, struct dentry *d2);

aufs_bindex_t dbstart(struct dentry *dentry);
aufs_bindex_t dbend(struct dentry *dentry);
aufs_bindex_t dbwh(struct dentry *dentry);
aufs_bindex_t dbdiropq(struct dentry *dentry);
struct dentry *au_h_dptr_i(struct dentry *dentry, aufs_bindex_t bindex);
struct dentry *au_h_dptr(struct dentry *dentry);

aufs_bindex_t dbtail(struct dentry *dentry);
aufs_bindex_t dbtaildir(struct dentry *dentry);
aufs_bindex_t dbtail_generic(struct dentry *dentry);

void set_dbstart(struct dentry *dentry, aufs_bindex_t bindex);
void set_dbend(struct dentry *dentry, aufs_bindex_t bindex);
void set_dbwh(struct dentry *dentry, aufs_bindex_t bindex);
void set_dbdiropq(struct dentry *dentry, aufs_bindex_t bindex);
void hdput(struct aufs_hdentry *hdentry);
void set_h_dptr(struct dentry *dentry, aufs_bindex_t bindex,
		struct dentry *h_dentry);

static inline void au_do_h_dentry_init(struct aufs_hdentry *hdentry)
{
	hdentry->hd_dentry = NULL;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 19) \
	&& !defined(AuNoNfsBranch)
static inline void au_h_dentry_init(struct aufs_hdentry *hdentry)
{
	au_do_h_dentry_init(hdentry);
	spin_lock_init(&hdentry->hd_lock);
}

static inline void au_h_dentry_init_all(struct aufs_hdentry *hdentry, int n)
{
	while (n--)
		spin_lock_init(&hdentry[n].hd_lock);
}

int au_set_h_intent(struct dentry *dentry, aufs_bindex_t bindex,
		    struct file *file, struct file *h_file);
struct file *au_h_intent(struct dentry *dentry, aufs_bindex_t bindex,
			 struct file *file);
#else
static inline
struct file *au_h_intent(struct dentry *dentry, aufs_bindex_t bindex,
			 struct file *file)
{
	return ERR_PTR(-ENOSYS);
}

static inline void au_h_dentry_init(struct aufs_hdentry *hdentry)
{
	au_do_h_dentry_init(hdentry);
}

static inline void au_h_dentry_init_all(struct aufs_hdentry *hdentry, int n)
{
	/* nothing */
}
#endif

void au_update_digen(struct dentry *dentry);
void au_update_dbrange(struct dentry *dentry, int do_put_zero);
void au_update_dbstart(struct dentry *dentry);
void au_update_dbend(struct dentry *dentry);
int au_find_dbindex(struct dentry *dentry, struct dentry *h_dentry);

/* ---------------------------------------------------------------------- */

static inline au_gen_t au_digen(struct dentry *d)
{
	return atomic_read(&dtodi(d)->di_generation);
}

#ifdef CONFIG_AUFS_HINOTIFY
static inline au_gen_t au_digen_dec(struct dentry *d)
{
	return atomic_dec_return(&dtodi(d)->di_generation);
}
#endif /* CONFIG_AUFS_HINOTIFY */

/* ---------------------------------------------------------------------- */

/* lock subclass for dinfo */
enum {
	AuLsc_DI_CHILD,		/* child first */
	AuLsc_DI_CHILD2,	/* rename(2), link(2), and cpup at hinotify */
	AuLsc_DI_CHILD3,	/* copyup dirs */
	AuLsc_DI_PARENT,
	AuLsc_DI_PARENT2,
	AuLsc_DI_PARENT3
};

/*
 * di_read_lock_child, di_write_lock_child,
 * di_read_lock_child2, di_write_lock_child2,
 * di_read_lock_child3, di_write_lock_child3,
 * di_read_lock_parent, di_write_lock_parent,
 * di_read_lock_parent2, di_write_lock_parent2,
 * di_read_lock_parent3, di_write_lock_parent3,
 */
#define ReadLockFunc(name, lsc) \
static inline void di_read_lock_##name(struct dentry *d, int flags) \
{ di_read_lock(d, flags, AuLsc_DI_##lsc); }

#define WriteLockFunc(name, lsc) \
static inline void di_write_lock_##name(struct dentry *d) \
{ di_write_lock(d, AuLsc_DI_##lsc); }

#define RWLockFuncs(name, lsc) \
	ReadLockFunc(name, lsc) \
	WriteLockFunc(name, lsc)

RWLockFuncs(child, CHILD);
RWLockFuncs(child2, CHILD2);
RWLockFuncs(child3, CHILD3);
RWLockFuncs(parent, PARENT);
RWLockFuncs(parent2, PARENT2);
RWLockFuncs(parent3, PARENT3);

#undef ReadLockFunc
#undef WriteLockFunc
#undef RWLockFunc

/* to debug easier, do not make them inlined functions */
#define DiMustReadLock(d) do { \
	SiMustAnyLock((d)->d_sb); \
	RwMustReadLock(&dtodi(d)->di_rwsem); \
} while (0)

#define DiMustWriteLock(d) do { \
	SiMustAnyLock((d)->d_sb); \
	RwMustWriteLock(&dtodi(d)->di_rwsem); \
} while (0)

#define DiMustAnyLock(d) do { \
	SiMustAnyLock((d)->d_sb); \
	RwMustAnyLock(&dtodi(d)->di_rwsem); \
} while (0)

#define DiMustNoWaiters(d)	RwMustNoWaiters(&dtodi(d)->di_rwsem)

#endif /* __KERNEL__ */
#endif /* __AUFS_DENTRY_H__ */
