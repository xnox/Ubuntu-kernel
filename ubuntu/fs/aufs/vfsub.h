/*
 * Copyright (C) 2007 Junjiro Okajima
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

/* $Id: vfsub.h,v 1.28 2008/01/28 05:01:42 sfjro Exp $ */

#ifndef __AUFS_VFSUB_H__
#define __AUFS_VFSUB_H__

#ifdef __KERNEL__

#include <linux/fs.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 23) \
	&& defined(CONFIG_AUFS_SPLICE_PATCH)
#include <linux/splice.h>
#endif
#ifdef CONFIG_AUFS_HINOTIFY
#include <linux/inotify.h>
#endif

/* ---------------------------------------------------------------------- */

struct aufs_hin_ignore;
struct vfsub_args {
#ifdef CONFIG_AUFS_HINOTIFY
	/* inotify events to be ignored */
	int			nignore;
	struct aufs_hin_ignore	*ignore;
#endif

	/* operation with delegation */
	unsigned int		dlgt:1;

	/* force unlinking */
	unsigned int		force_unlink:1;
};

struct aufs_hinode;
#ifdef CONFIG_AUFS_HINOTIFY
static inline
void do_vfsub_args_reinit(struct vfsub_args *vargs, struct aufs_hin_ignore *ign)
{
	vargs->nignore = 0;
	vargs->ignore = ign;
}

static inline
void vfsub_args_reinit(struct vfsub_args *vargs)
{
	vargs->nignore = 0;
}

__u32 vfsub_events_notify_change(struct iattr *ia);
void vfsub_ign_hinode(struct vfsub_args *vargs, __u32 events,
		      struct aufs_hinode *hinode);
void vfsub_ign_inode(struct vfsub_args *vargs, __u32 events,
		     struct inode *inode, struct inode *h_inode);

void vfsub_ignore(struct vfsub_args *vargs);
void vfsub_unignore(struct vfsub_args *vargs);
#else
static inline
void do_vfsub_args_reinit(struct vfsub_args *vargs, struct aufs_hin_ignore *ign)
{
	/* empty */
}

static inline
void vfsub_args_reinit(struct vfsub_args *vargs)
{
	/* empty */
}

static inline __u32 vfsub_events_notify_change(struct iattr *ia)
{
	return 0;
}

static inline
void vfsub_ign_hinode(struct vfsub_args *vargs, __u32 events,
		      struct aufs_hinode *hinode)
{
	/* empty */
}

static inline
void vfsub_ign_inode(struct vfsub_args *vargs, __u32 events,
		     struct inode *inode, struct inode *h_inode)
{
	/* empty */
}

static inline void vfsub_ignore(struct vfsub_args *vargs)
{
	/* empty */
}

static inline void vfsub_unignore(struct vfsub_args *vargs)
{
	/* empty */
}
#endif /* CONFIG_AUFS_HINOTIFY */

static inline
void vfsub_args_init(struct vfsub_args *vargs, struct aufs_hin_ignore *ign,
		     int dlgt, int force_unlink)
{
	do_vfsub_args_reinit(vargs, ign);
	vargs->dlgt = !!dlgt;
	vargs->force_unlink = !!force_unlink;
}

/* ---------------------------------------------------------------------- */

/* lock subclass for hidden inode */
/* default MAX_LOCKDEP_SUBCLASSES(8) is not enough */
// todo: reduce it
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 18)
#define I_MUTEX_QUOTA 0
#endif
enum {
	AuLsc_I_Begin = I_MUTEX_QUOTA, /* 4 */
	AuLsc_I_PARENT,		/* hidden inode, parent first */
	AuLsc_I_CHILD,
	AuLsc_I_PARENT2,	/* copyup dirs */
	AuLsc_I_CHILD2,
	AuLsc_I_End
};

/* simple abstraction */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 16)
static inline void vfsub_i_lock(struct inode *i)
{
	down(&i->i_sem);
}

static inline void vfsub_i_lock_nested(struct inode *i, unsigned lsc)
{
	vfsub_i_lock(i);
}

static inline void vfsub_i_unlock(struct inode *i)
{
	up(&i->i_sem);
}

static inline int vfsub_i_trylock(struct inode *i)
{
	return down_trylock(&i->i_sem);
}

#define IMustLock(i)	AuDebugOn(!down_trylock(&(i)->i_sem))
#else
static inline void vfsub_i_lock(struct inode *i)
{
	mutex_lock(&i->i_mutex);
}

static inline void vfsub_i_lock_nested(struct inode *i, unsigned lsc)
{
	mutex_lock_nested(&i->i_mutex, lsc);
}

static inline void vfsub_i_unlock(struct inode *i)
{
	mutex_unlock(&i->i_mutex);
}

static inline int vfsub_i_trylock(struct inode *i)
{
	return mutex_trylock(&i->i_mutex);
}

#define IMustLock(i)	MtxMustLock(&(i)->i_mutex)
#endif /* LINUX_VERSION_CODE */

static inline
struct dentry *vfsub_lock_rename(struct dentry *d1, struct dentry *d2)
{
	struct dentry *d;

	lockdep_off();
	d = lock_rename(d1, d2);
	lockdep_on();
	return d;
}

static inline void vfsub_unlock_rename(struct dentry *d1, struct dentry *d2)
{
	lockdep_off();
	unlock_rename(d1, d2);
	lockdep_on();
}

/* ---------------------------------------------------------------------- */

#ifdef CONFIG_AUFS_WORKAROUND_FUSE
int au_update_fuse_h_inode(struct vfsmount *h_mnt, struct dentry *h_dentry);
#else
static inline
int au_update_fuse_h_inode(struct vfsmount *h_mnt, struct dentry *h_dentry)
{
	return 0;
}
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 24) \
	&& (defined(CONFIG_XFS_FS) || defined(CONFIG_XFS_FS_MODULE))
dev_t au_h_rdev(struct inode *h_inode, struct vfsmount *h_mnt,
		struct dentry *h_dentry);
#else
static inline
dev_t au_h_rdev(struct inode *h_inode, struct vfsmount *h_mnt,
		struct dentry *h_dentry)
{
	return h_inode->i_rdev;
}
#endif

/* simple abstractions, for future use */
static inline
int do_vfsub_permission(struct inode *inode, int mask, struct nameidata *nd)
{
	LKTRTrace("i%lu, mask 0x%x, nd %d\n", inode->i_ino, mask, !!nd);
	IMustLock(inode);
	return permission(inode, mask, nd);
}

static inline
struct file *vfsub_filp_open(const char *path, int oflags, int mode)
{
	struct file *err;

	LKTRTrace("%s\n", path);

	lockdep_off();
	err = filp_open(path, oflags, mode);
	lockdep_on();
	if (!IS_ERR(err))
		au_update_fuse_h_inode(err->f_vfsmnt, err->f_dentry); /*ignore*/
	return err;
}

static inline
int vfsub_path_lookup(const char *name, unsigned int flags,
		      struct nameidata *nd)
{
	int err;

	LKTRTrace("%s\n", name);

	/* lockdep_off(); */
	err = path_lookup(name, flags, nd);
	/* lockdep_on(); */
	if (!err)
		au_update_fuse_h_inode(nd->mnt, nd->dentry); /*ignore*/
	return err;
}

static inline
struct dentry *vfsub_lookup_one_len(const char *name, struct dentry *parent,
				    int len)
{
	struct dentry *d;

	LKTRTrace("%.*s/%.*s\n", AuDLNPair(parent), len, name);
	IMustLock(parent->d_inode);

	d = lookup_one_len(name, parent, len);
	if (!IS_ERR(d))
		au_update_fuse_h_inode(NULL, d); /*ignore*/
	return d;
}

#ifdef CONFIG_AUFS_LHASH_PATCH
static inline
struct dentry *vfsub__lookup_hash(struct qstr *name, struct dentry *parent,
				  struct nameidata *nd)
{
	struct dentry *d;

	LKTRTrace("%.*s/%.*s, nd %d\n",
		  AuDLNPair(parent), AuLNPair(name), !!nd);
	IMustLock(parent->d_inode);

	d = __lookup_hash(name, parent, nd);
	if (!IS_ERR(d))
		au_update_fuse_h_inode(NULL, d); /*ignore*/
	return d;
}
#endif

/* because of the internal lock, never use vfs_path_lookup() */
#if 0 //LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 23)
static inline
int vfsub_vfs_path_lookup(struct dentry *parent, struct vfsmount *mnt,
			  char *name, unsigned int flags, struct nameidata *nd)
{
	int err;

	LKTRTrace("%.*s/%s, 0x%x, nd{}\n", AuDLNPair(parent), name, flags);

	/* vfs_path_lookup() will lock the parent inode */
	lockdep_off();
	err = vfs_path_lookup(parent, mnt, name, flags, nd);
	lockdep_on();
	if (!err)
		au_update_fuse_h_inode(mnt, nd->dentry); /*ignore*/

	AuTraceErr(err);
	return err;
}
#endif

/* ---------------------------------------------------------------------- */

#if 0
int do_vfsub_create(struct inode *dir, struct dentry *dentry, int mode,
		    struct nameidata *nd);
int do_vfsub_symlink(struct inode *dir, struct dentry *dentry,
		     const char *symname, int mode);
int do_vfsub_mknod(struct inode *dir, struct dentry *dentry, int mode,
		   dev_t dev);
int do_vfsub_link(struct dentry *src_dentry, struct inode *dir,
		  struct dentry *dentry);
int do_vfsub_rename(struct inode *src_dir, struct dentry *src_dentry,
		    struct inode *dir, struct dentry *dentry);
int do_vfsub_mkdir(struct inode *dir, struct dentry *dentry, int mode);
int do_vfsub_rmdir(struct inode *dir, struct dentry *dentry);
int do_vfsub_unlink(struct inode *dir, struct dentry *dentry);
#else
static inline
int do_vfsub_create(struct inode *dir, struct dentry *dentry, int mode,
		    struct nameidata *nd)
{
	int err;
	struct vfsmount *mnt;

	LKTRTrace("i%lu, %.*s, 0x%x\n", dir->i_ino, AuDLNPair(dentry), mode);
	IMustLock(dir);

	err = vfs_create(dir, dentry, mode, nd);
	if (!err) {
		mnt = NULL;
		if (nd)
			mnt = nd->mnt;
		/* dir inode is locked */
		au_update_fuse_h_inode(mnt, dentry->d_parent); /*ignore*/
		au_update_fuse_h_inode(mnt, dentry); /*ignore*/
	}
	return err;
}

static inline
int do_vfsub_symlink(struct inode *dir, struct dentry *dentry,
		     const char *symname, int mode)
{
	int err;

	LKTRTrace("i%lu, %.*s, %s, 0x%x\n",
		  dir->i_ino, AuDLNPair(dentry), symname, mode);
	IMustLock(dir);

#ifdef CONFIG_VSERVER
	err = vfs_symlink(dir, dentry, symname, mode, NULL);
#elif defined(CONFIG_SECURITY_APPARMOR)
	err = vfs_symlink(dir, dentry, NULL, symname, mode);
#else
	err = vfs_symlink(dir, dentry, symname, mode);
#endif
	if (!err) {
		/* dir inode is locked */
		au_update_fuse_h_inode(NULL, dentry->d_parent); /*ignore*/
		au_update_fuse_h_inode(NULL, dentry); /*ignore*/
	}
	return err;
}

static inline
int do_vfsub_mknod(struct inode *dir, struct dentry *dentry, int mode,
		   dev_t dev)
{
	int err;

	LKTRTrace("i%lu, %.*s, 0x%x\n", dir->i_ino, AuDLNPair(dentry), mode);
	IMustLock(dir);

#ifdef CONFIG_VSERVER
	err = vfs_mknod(dir, dentry, mode, dev, NULL);
#elif defined(CONFIG_SECURITY_APPARMOR)
	err = vfs_mknod(dir, dentry, NULL, mode, dev);
#else
	err = vfs_mknod(dir, dentry, mode, dev);
#endif
	if (!err) {
		/* dir inode is locked */
		au_update_fuse_h_inode(NULL, dentry->d_parent); /*ignore*/
		au_update_fuse_h_inode(NULL, dentry); /*ignore*/
	}
	return err;
}

static inline
int do_vfsub_link(struct dentry *src_dentry, struct inode *dir,
		  struct dentry *dentry)
{
	int err;

	LKTRTrace("%.*s, i%lu, %.*s\n",
		  AuDLNPair(src_dentry), dir->i_ino, AuDLNPair(dentry));
	IMustLock(dir);

	lockdep_off();
#ifdef CONFIG_VSERVER
	err = vfs_link(src_dentry, dir, dentry, NULL);
#elif defined(CONFIG_SECURITY_APPARMOR)
	err = vfs_link(src_dentry, NULL, dir, dentry, NULL);
#else
	err = vfs_link(src_dentry, dir, dentry);
#endif
	lockdep_on();
	if (!err) {
		LKTRTrace("src_i %p, dst_i %p\n",
			  src_dentry->d_inode, dentry->d_inode);
		/* fuse has different memory inode for the same inumber */
		au_update_fuse_h_inode(NULL, src_dentry); /*ignore*/
		/* dir inode is locked */
		au_update_fuse_h_inode(NULL, dentry->d_parent); /*ignore*/
		au_update_fuse_h_inode(NULL, dentry); /*ignore*/
	}
	return err;
}

static inline
int do_vfsub_rename(struct inode *src_dir, struct dentry *src_dentry,
		    struct inode *dir, struct dentry *dentry)
{
	int err;

	LKTRTrace("i%lu, %.*s, i%lu, %.*s\n",
		  src_dir->i_ino, AuDLNPair(src_dentry),
		  dir->i_ino, AuDLNPair(dentry));
	IMustLock(dir);
	IMustLock(src_dir);

	lockdep_off();
	#ifdef CONFIG_SECURITY_APPARMOR
	err = vfs_rename(src_dir, src_dentry, NULL, dir, dentry, NULL);
	#else
	err = vfs_rename(src_dir, src_dentry, dir, dentry);
	#endif
	lockdep_on();
	if (!err) {
		/* dir inode is locked */
		au_update_fuse_h_inode(NULL, dentry->d_parent); /*ignore*/
		au_update_fuse_h_inode(NULL, src_dentry->d_parent); /*ignore*/
		au_update_fuse_h_inode(NULL, src_dentry); /*ignore*/
	}
	return err;
}

static inline
int do_vfsub_mkdir(struct inode *dir, struct dentry *dentry, int mode)
{
	int err;

	LKTRTrace("i%lu, %.*s, 0x%x\n", dir->i_ino, AuDLNPair(dentry), mode);
	IMustLock(dir);

#ifdef CONFIG_VSERVER
	err = vfs_mkdir(dir, dentry, mode, NULL);
#elif defined(CONFIG_SECURITY_APPARMOR)
	err = vfs_mkdir(dir, dentry, NULL, mode);
#else
	err = vfs_mkdir(dir, dentry, mode);
#endif
	if (!err) {
		/* dir inode is locked */
		au_update_fuse_h_inode(NULL, dentry->d_parent); /*ignore*/
		au_update_fuse_h_inode(NULL, dentry); /*ignore*/
	}
	return err;
}

static inline int do_vfsub_rmdir(struct inode *dir, struct dentry *dentry)
{
	int err;

	LKTRTrace("i%lu, %.*s\n", dir->i_ino, AuDLNPair(dentry));
	IMustLock(dir);

	lockdep_off();
#if defined(CONFIG_VSERVER) || defined(CONFIG_SECURITY_APPARMOR)
	err = vfs_rmdir(dir, dentry, NULL);
#else
	err = vfs_rmdir(dir, dentry);
#endif
	lockdep_on();
	/* dir inode is locked */
	if (!err)
		au_update_fuse_h_inode(NULL, dentry->d_parent); /*ignore*/
	return err;
}

static inline int do_vfsub_unlink(struct inode *dir, struct dentry *dentry)
{
	int err;

	LKTRTrace("i%lu, %.*s\n", dir->i_ino, AuDLNPair(dentry));
	IMustLock(dir);

	/* vfs_unlink() locks inode */
	lockdep_off();
#if defined(CONFIG_VSERVER) || defined(CONFIG_SECURITY_APPARMOR)
	err = vfs_unlink(dir, dentry, NULL);
#else
	err = vfs_unlink(dir, dentry);
#endif
	lockdep_on();
	/* dir inode is locked */
	if (!err)
		au_update_fuse_h_inode(NULL, dentry->d_parent); /*ignore*/
	return err;
}
#endif

/* ---------------------------------------------------------------------- */

static inline
ssize_t do_vfsub_read_u(struct file *file, char __user *ubuf, size_t count,
			loff_t *ppos)
{
	ssize_t err;

	LKTRTrace("%.*s, cnt %lu, pos %Ld\n",
		  AuDLNPair(file->f_dentry), (unsigned long)count, *ppos);

	if (0 /*!au_test_nfs(file->f_vfsmnt->mnt_sb)*/)
		err = vfs_read(file, ubuf, count, ppos);
	else {
		lockdep_off();
		err = vfs_read(file, ubuf, count, ppos);
		lockdep_on();
	}
	if (err >= 0)
		au_update_fuse_h_inode(file->f_vfsmnt, file->f_dentry);
	/*ignore*/
	return err;
}

// kernel_read() ??
static inline
ssize_t do_vfsub_read_k(struct file *file, void *kbuf, size_t count,
			loff_t *ppos)
{
	ssize_t err;
	mm_segment_t oldfs;

	oldfs = get_fs();
	set_fs(KERNEL_DS);
	err = do_vfsub_read_u(file, (char __user *)kbuf, count, ppos);
	set_fs(oldfs);
	return err;
}

static inline
ssize_t do_vfsub_write_u(struct file *file, const char __user *ubuf,
			 size_t count, loff_t *ppos)
{
	ssize_t err;

	LKTRTrace("%.*s, cnt %lu, pos %Ld\n",
		  AuDLNPair(file->f_dentry), (unsigned long)count, *ppos);

	lockdep_off();
	err = vfs_write(file, ubuf, count, ppos);
	lockdep_on();
	if (err >= 0)
		au_update_fuse_h_inode(file->f_vfsmnt, file->f_dentry);
	/*ignore*/
	return err;
}

static inline
ssize_t do_vfsub_write_k(struct file *file, void *kbuf, size_t count,
			 loff_t *ppos)
{
	ssize_t err;
	mm_segment_t oldfs;

	oldfs = get_fs();
	set_fs(KERNEL_DS);
	err = do_vfsub_write_u(file, (const char __user *)kbuf, count, ppos);
	set_fs(oldfs);
	return err;
}

static inline
int do_vfsub_readdir(struct file *file, filldir_t filldir, void *arg)
{
	int err;

	LKTRTrace("%.*s\n", AuDLNPair(file->f_dentry));

	lockdep_off();
	err = vfs_readdir(file, filldir, arg);
	lockdep_on();
	if (err >= 0)
		au_update_fuse_h_inode(file->f_vfsmnt, file->f_dentry);
	/*ignore*/
	return err;
}

/* ---------------------------------------------------------------------- */

#ifdef CONFIG_AUFS_SPLICE_PATCH
static inline
long do_vfsub_splice_to(struct file *in, loff_t *ppos,
			struct pipe_inode_info *pipe, size_t len,
			unsigned int flags)
{
	long err;

	LKTRTrace("%.*s, pos %Ld, len %lu, 0x%x\n",
		  AuDLNPair(in->f_dentry), *ppos, (unsigned long)len, flags);

	lockdep_off();
	err = do_splice_to(in, ppos, pipe, len, flags);
	lockdep_on();
	if (err >= 0)
		au_update_fuse_h_inode(in->f_vfsmnt, in->f_dentry); /*ignore*/
	return err;
}

static inline
long do_vfsub_splice_from(struct pipe_inode_info *pipe, struct file *out,
			  loff_t *ppos, size_t len, unsigned int flags)
{
	long err;

	LKTRTrace("%.*s, pos %Ld, len %lu, 0x%x\n",
		  AuDLNPair(out->f_dentry), *ppos, (unsigned long)len, flags);

	lockdep_off();
	err = do_splice_from(pipe, out, ppos, len, flags);
	lockdep_on();
	if (err >= 0)
		au_update_fuse_h_inode(out->f_vfsmnt, out->f_dentry); /*ignore*/
	return err;
}
#else
static inline
long do_vfsub_splice_to(struct file *in, loff_t *ppos,
			struct pipe_inode_info *pipe, size_t len,
			unsigned int flags)
{
	return -ENOSYS;
}

static inline
long do_vfsub_splice_from(struct pipe_inode_info *pipe, struct file *out,
			  loff_t *ppos, size_t len, unsigned int flags)
{
	return -ENOSYS;
}
#endif /* CONFIG_AUFS_SPLICE_PATCH */

/* ---------------------------------------------------------------------- */

static inline loff_t vfsub_llseek(struct file *file, loff_t offset, int origin)
{
	loff_t err;

	LKTRTrace("%.*s\n", AuDLNPair(file->f_dentry));

	lockdep_off();
	err = vfs_llseek(file, offset, origin);
	lockdep_on();
	return err;
}

static inline int do_vfsub_getattr(struct vfsmount *mnt, struct dentry *dentry,
				   struct kstat *st)
{
	LKTRTrace("%.*s\n", AuDLNPair(dentry));
	return vfs_getattr(mnt, dentry, st);
}

/* ---------------------------------------------------------------------- */

#if defined(CONFIG_AUFS_DLGT) || defined(CONFIG_AUFS_HINOTIFY)
int vfsub_permission(struct inode *inode, int mask, struct nameidata *nd,
		     int dlgt);

int vfsub_create(struct inode *dir, struct dentry *dentry, int mode,
		 struct nameidata *nd, int dlgt);
int vfsub_symlink(struct inode *dir, struct dentry *dentry, const char *symname,
		  int mode, int dlgt);
int vfsub_mknod(struct inode *dir, struct dentry *dentry, int mode, dev_t dev,
		int dlgt);
int vfsub_link(struct dentry *src_dentry, struct inode *dir,
	       struct dentry *dentry, int dlgt);
int vfsub_rename(struct inode *src_dir, struct dentry *src_dentry,
		 struct inode *dir, struct dentry *dentry,
		 struct vfsub_args *vargs);
int vfsub_mkdir(struct inode *dir, struct dentry *dentry, int mode, int dlgt);
int vfsub_rmdir(struct inode *dir, struct dentry *dentry,
		struct vfsub_args *vargs);

ssize_t vfsub_read_u(struct file *file, char __user *ubuf, size_t count,
		     loff_t *ppos, int dlgt);
ssize_t vfsub_read_k(struct file *file, void *kbuf, size_t count, loff_t *ppos,
		     int dlgt);
ssize_t vfsub_write_u(struct file *file, const char __user *ubuf, size_t count,
		      loff_t *ppos, struct vfsub_args *vargs);
ssize_t vfsub_write_k(struct file *file, void *kbuf, size_t count, loff_t *ppos,
		      struct vfsub_args *vargs);
int vfsub_readdir(struct file *file, filldir_t filldir, void *arg, int dlgt);
long vfsub_splice_to(struct file *in, loff_t *ppos,
		     struct pipe_inode_info *pipe, size_t len,
		     unsigned int flags, int dlgt);
long vfsub_splice_from(struct pipe_inode_info *pipe, struct file *out,
		       loff_t *ppos, size_t len, unsigned int flags,
		       struct vfsub_args *vargs);

int vfsub_getattr(struct vfsmount *mnt, struct dentry *dentry, struct kstat *st,
		  int dlgt);
#else

static inline
int vfsub_permission(struct inode *inode, int mask, struct nameidata *nd,
		     int dlgt)
{
	return do_vfsub_permission(inode, mask, nd);
}

static inline
int vfsub_create(struct inode *dir, struct dentry *dentry, int mode,
		 struct nameidata *nd, int dlgt)
{
	return do_vfsub_create(dir, dentry, mode, nd);
}

static inline
int vfsub_symlink(struct inode *dir, struct dentry *dentry, const char *symname,
		  int mode, int dlgt)
{
	return do_vfsub_symlink(dir, dentry, symname, mode);
}

static inline
int vfsub_mknod(struct inode *dir, struct dentry *dentry, int mode, dev_t dev,
		int dlgt)
{
	return do_vfsub_mknod(dir, dentry, mode, dev);
}

static inline
int vfsub_link(struct dentry *src_dentry, struct inode *dir,
	       struct dentry *dentry, int dlgt)
{
	return do_vfsub_link(src_dentry, dir, dentry);
}

static inline
int vfsub_rename(struct inode *src_dir, struct dentry *src_dentry,
		 struct inode *dir, struct dentry *dentry,
		 struct vfsub_args *vargs)
{
	int err;

	vfsub_ignore(vargs);
	err = do_vfsub_rename(src_dir, src_dentry, dir, dentry);
	if (unlikely(err))
		vfsub_unignore(vargs);
	return err;
}

static inline
int vfsub_mkdir(struct inode *dir, struct dentry *dentry, int mode, int dlgt)
{
	return do_vfsub_mkdir(dir, dentry, mode);
}

static inline
int vfsub_rmdir(struct inode *dir, struct dentry *dentry,
		struct vfsub_args *vargs)
{
	int err;

	vfsub_ignore(vargs);
	err = do_vfsub_rmdir(dir, dentry);
	if (unlikely(err))
		vfsub_unignore(vargs);
	return err;
}

static inline
ssize_t vfsub_read_u(struct file *file, char __user *ubuf, size_t count,
		     loff_t *ppos, int dlgt)
{
	return do_vfsub_read_u(file, ubuf, count, ppos);
}

static inline
ssize_t vfsub_read_k(struct file *file, void *kbuf, size_t count, loff_t *ppos,
		     int dlgt)
{
	return do_vfsub_read_k(file, kbuf, count, ppos);
}

static inline
ssize_t vfsub_write_u(struct file *file, const char __user *ubuf, size_t count,
		      loff_t *ppos, struct vfsub_args *vargs)
{
	int err;

	vfsub_ignore(vargs);
	err = do_vfsub_write_u(file, ubuf, count, ppos);
	if (unlikely(err < 0))
		vfsub_unignore(vargs);
	return err;
}

static inline
ssize_t vfsub_write_k(struct file *file, void *kbuf, size_t count, loff_t *ppos,
		      struct vfsub_args *vargs)
{
	int err;

	vfsub_ignore(vargs);
	err = do_vfsub_write_k(file, kbuf, count, ppos);
	if (unlikely(err < 0))
		vfsub_unignore(vargs);
	return err;
}

static inline
int vfsub_readdir(struct file *file, filldir_t filldir, void *arg, int dlgt)
{
	return do_vfsub_readdir(file, filldir, arg);
}

static inline
long vfsub_splice_to(struct file *in, loff_t *ppos,
		     struct pipe_inode_info *pipe, size_t len,
		     unsigned int flags, int dlgt)
{
	return do_vfsub_splice_to(in, ppos, pipe, len, flags);
}

static inline
long vfsub_splice_from(struct pipe_inode_info *pipe, struct file *out,
		       loff_t *ppos, size_t len, unsigned int flags,
		       struct vfsub_args *vargs)
{
	long err;

	vfsub_ignore(vargs);
	err = do_vfsub_splice_from(pipe, out, ppos, len, flags);
	if (unlikely(err < 0))
		vfsub_unignore(vargs);
	return err;
}

static inline int vfsub_getattr(struct vfsmount *mnt, struct dentry *dentry,
				struct kstat *st, int dlgt)
{
	return do_vfsub_getattr(mnt, dentry, st);
}
#endif /* CONFIG_AUFS_DLGT || CONFIG_AUFS_HINOTIFY */

/* ---------------------------------------------------------------------- */

int vfsub_sio_mkdir(struct inode *dir, struct dentry *dentry, int mode,
		    int dlgt);
int vfsub_sio_rmdir(struct inode *dir, struct dentry *dentry, int dlgt);

/* ---------------------------------------------------------------------- */

int vfsub_notify_change(struct dentry *dentry, struct iattr *ia,
			struct vfsub_args *vargs);
int vfsub_unlink(struct inode *dir, struct dentry *dentry,
		 struct vfsub_args *vargs);
int vfsub_statfs(void *arg, struct kstatfs *buf, int dlgt);

#endif /* __KERNEL__ */
#endif /* __AUFS_VFSUB_H__ */
