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

/* $Id: inode.h,v 1.47 2007/11/12 01:40:34 sfjro Exp $ */

#ifndef __AUFS_INODE_H__
#define __AUFS_INODE_H__

#ifdef __KERNEL__

#include <linux/fs.h>
#include <linux/version.h>
#include <linux/aufs_type.h>
#include "misc.h"

struct aufs_hinode;
struct aufs_vdir;
struct aufs_iinfo {
	atomic_t		ii_generation;
	struct super_block	*ii_hsb1;	/* no get/put */

	struct aufs_rwsem	ii_rwsem;
	aufs_bindex_t		ii_bstart, ii_bend;
	struct aufs_hinode	*ii_hinode;
	struct aufs_vdir	*ii_vdir;
};

struct aufs_icntnr {
	struct aufs_iinfo iinfo;
	struct inode vfs_inode;
};

/* ---------------------------------------------------------------------- */

/* inode.c */
int au_refresh_hinode_self(struct inode *inode);
int au_refresh_hinode(struct inode *inode, struct dentry *dentry);
struct inode *au_new_inode(struct dentry *dentry);

/* i_op.c */
extern struct inode_operations aufs_iop, aufs_symlink_iop, aufs_dir_iop;
struct au_wr_dir_args {
	aufs_bindex_t force_btgt;

	unsigned int add_entry:1;
	unsigned int do_lock_srcdir:1;
	unsigned int isdir:1;
};
int au_wr_dir(struct dentry *dentry, struct dentry *src_dentry,
	      struct au_wr_dir_args *args);

/* i_op_add.c */
int aufs_mknod(struct inode *dir, struct dentry *dentry, int mode, dev_t dev);
int aufs_symlink(struct inode *dir, struct dentry *dentry, const char *symname);
int aufs_create(struct inode *dir, struct dentry *dentry, int mode,
		struct nameidata *nd);
int aufs_link(struct dentry *src_dentry, struct inode *dir,
	      struct dentry *dentry);
int aufs_mkdir(struct inode *dir, struct dentry *dentry, int mode);

/* i_op_del.c */
int au_wr_dir_need_wh(struct dentry *dentry, int isdir, aufs_bindex_t *bcpup,
		      struct dentry *locked);
int aufs_unlink(struct inode *dir, struct dentry *dentry);
int aufs_rmdir(struct inode *dir, struct dentry *dentry);

/* i_op_ren.c */
int au_wbr(struct dentry *dentry, aufs_bindex_t btgt);
int aufs_rename(struct inode *src_dir, struct dentry *src_dentry,
		struct inode *dir, struct dentry *dentry);

#if 0 // xattr
/* xattr.c */
int aufs_setxattr(struct dentry *dentry, const char *name, const void *value,
		  size_t sz, int flags);
ssize_t aufs_getxattr(struct dentry *dentry, const char *name, void *value,
		      size_t sz);
ssize_t aufs_listxattr(struct dentry *dentry, char *list, size_t sz);
int aufs_removexattr(struct dentry *dentry, const char *name);
#endif

/* iinfo.c */
struct aufs_iinfo *itoii(struct inode *inode);
aufs_bindex_t ibstart(struct inode *inode);
aufs_bindex_t ibend(struct inode *inode);
struct aufs_vdir *ivdir(struct inode *inode);
struct dentry *au_hi_wh(struct inode *inode, aufs_bindex_t bindex);
struct inode *au_h_iptr_i(struct inode *inode, aufs_bindex_t bindex);
struct inode *au_h_iptr(struct inode *inode);
aufs_bindex_t itoid_index(struct inode *inode, aufs_bindex_t bindex);

void set_ibstart(struct inode *inode, aufs_bindex_t bindex);
void set_ibend(struct inode *inode, aufs_bindex_t bindex);
void set_ivdir(struct inode *inode, struct aufs_vdir *vdir);
void set_hi_wh(struct inode *inode, aufs_bindex_t bindex, struct dentry *h_wh);
void aufs_hiput(struct aufs_hinode *hinode);
#define AUFS_HI_XINO	1
#define AUFS_HI_NOTIFY	2
unsigned int au_hi_flags(struct inode *inode, int isdir);
struct aufs_hinode *itohi(struct inode *inode, aufs_bindex_t bindex);
void set_h_iptr(struct inode *inode, aufs_bindex_t bindex,
		struct inode *h_inode, unsigned int flags);
void au_update_iigen(struct inode *inode);
void au_update_brange(struct inode *inode, int do_put_zero);

int au_iinfo_init(struct inode *inode);
void au_iinfo_fin(struct inode *inode);

/* plink.c */
#ifdef CONFIG_AUFS_DEBUG
void au_list_plink(struct super_block *sb);
#else
static inline void au_list_plink(struct super_block *sb)
{
	/* nothing */
}
#endif
int au_test_plink(struct super_block *sb, struct inode *inode);
struct dentry *au_lkup_plink(struct super_block *sb, aufs_bindex_t bindex,
			     struct inode *inode);
void au_append_plink(struct super_block *sb, struct inode *inode,
		     struct dentry *h_dentry, aufs_bindex_t bindex);
void au_put_plink(struct super_block *sb);
void au_half_refresh_plink(struct super_block *sb, aufs_bindex_t br_id);

/* ---------------------------------------------------------------------- */

/* tiny test for inode number */
/* tmpfs generation is too rough */
static inline int au_test_higen(struct inode *inode, struct inode *h_inode)
{
	//IiMustAnyLock(inode);
	return !(itoii(inode)->ii_hsb1 == h_inode->i_sb
		 && inode->i_generation == h_inode->i_generation);
}

static inline au_gen_t au_iigen(struct inode *inode)
{
	return atomic_read(&itoii(inode)->ii_generation);
}

static inline au_gen_t au_iigen_inc(struct inode *inode)
{
	//AuDbg("i%lu\n", inode->i_ino);
	return atomic_inc_return(&itoii(inode)->ii_generation);
}

static inline au_gen_t au_iigen_dec(struct inode *inode)
{
	//AuDbg("i%lu\n", inode->i_ino);
	return atomic_dec_return(&itoii(inode)->ii_generation);
}

/* ---------------------------------------------------------------------- */

/* lock subclass for iinfo */
enum {
	AuLsc_II_CHILD,		/* child first */
	AuLsc_II_CHILD2,	/* rename(2), link(2), and cpup at hinotify */
	AuLsc_II_CHILD3,	/* copyup dirs */
	AuLsc_II_PARENT,
	AuLsc_II_PARENT2,
	AuLsc_II_PARENT3,
	AuLsc_II_PARENT4,
	AuLsc_II_NEW		/* new inode */
};

/*
 * ii_read_lock_child, ii_write_lock_child,
 * ii_read_lock_child2, ii_write_lock_child2,
 * ii_read_lock_child3, ii_write_lock_child3,
 * ii_read_lock_parent, ii_write_lock_parent,
 * ii_read_lock_parent2, ii_write_lock_parent2,
 * ii_read_lock_parent3, ii_write_lock_parent3,
 * ii_read_lock_parent4, ii_write_lock_parent4,
 * ii_read_lock_new, ii_write_lock_new
 */
#define ReadLockFunc(name, lsc) \
static inline void ii_read_lock_##name(struct inode *i) \
{ rw_read_lock_nested(&itoii(i)->ii_rwsem, AuLsc_II_##lsc); }

#define WriteLockFunc(name, lsc) \
static inline void ii_write_lock_##name(struct inode *i) \
{ rw_write_lock_nested(&itoii(i)->ii_rwsem, AuLsc_II_##lsc); }

#define RWLockFuncs(name, lsc) \
	ReadLockFunc(name, lsc) \
	WriteLockFunc(name, lsc)

RWLockFuncs(child, CHILD);
RWLockFuncs(child2, CHILD2);
RWLockFuncs(child3, CHILD3);
RWLockFuncs(parent, PARENT);
RWLockFuncs(parent2, PARENT2);
RWLockFuncs(parent3, PARENT3);
RWLockFuncs(parent4, PARENT4);
RWLockFuncs(new, NEW);

#undef ReadLockFunc
#undef WriteLockFunc
#undef RWLockFunc

/*
 * ii_read_unlock, ii_write_unlock, ii_downgrade_lock
 */
SimpleUnlockRwsemFuncs(ii, struct inode *i, itoii(i)->ii_rwsem);

/* to debug easier, do not make them inlined functions */
#define IiMustReadLock(i) do { \
	SiMustAnyLock((i)->i_sb); \
	RwMustReadLock(&itoii(i)->ii_rwsem); \
} while (0)

#define IiMustWriteLock(i) do { \
	SiMustAnyLock((i)->i_sb); \
	RwMustWriteLock(&itoii(i)->ii_rwsem); \
} while (0)

#define IiMustAnyLock(i) do { \
	SiMustAnyLock((i)->i_sb); \
	RwMustAnyLock(&itoii(i)->ii_rwsem); \
} while (0)

#define IiMustNoWaiters(i)	RwMustNoWaiters(&itoii(i)->ii_rwsem)

#endif /* __KERNEL__ */
#endif /* __AUFS_INODE_H__ */
