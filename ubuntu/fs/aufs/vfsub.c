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

/* $Id: vfsub.c,v 1.26 2008/01/28 05:01:42 sfjro Exp $ */
// I'm going to slightly mad

#include "aufs.h"

/* ---------------------------------------------------------------------- */

#ifdef CONFIG_AUFS_HINOTIFY
/* cf. fsnotify_change() */
__u32 vfsub_events_notify_change(struct iattr *ia)
{
	__u32 events;
	const unsigned int amtime = (ATTR_ATIME | ATTR_MTIME);

	events = 0;
	if ((ia->ia_valid & (ATTR_UID | ATTR_GID | ATTR_MODE))
	    || (ia->ia_valid & amtime) == amtime)
		events |= IN_ATTRIB;
	if ((ia->ia_valid & ATTR_SIZE)
	    || (ia->ia_valid & amtime) == ATTR_MTIME)
		events |= IN_MODIFY;
	return events;
}

void vfsub_ign_hinode(struct vfsub_args *vargs, __u32 events,
		      struct aufs_hinode *hinode)
{
	struct aufs_hin_ignore *ign;

	AuDebugOn(!hinode);

	ign = vargs->ignore + vargs->nignore++;
	ign->ign_events = events;
	ign->ign_hinode = hinode;
}

void vfsub_ignore(struct vfsub_args *vargs)
{
	int n;
	struct aufs_hin_ignore *ign;

	n = vargs->nignore;
	ign = vargs->ignore;
	while (n-- > 0) {
		au_hin_ignore(ign->ign_hinode, ign->ign_events);
		ign++;
	}
}

void vfsub_unignore(struct vfsub_args *vargs)
{
	int n;
	struct aufs_hin_ignore *ign;

	n = vargs->nignore;
	ign = vargs->ignore;
	while (n-- > 0) {
		au_hin_unignore(ign->ign_hinode, ign->ign_events);
		ign++;
	}
}
#endif /* CONFIG_AUFS_HINOTIFY */

/* ---------------------------------------------------------------------- */

#if 0
int do_vfsub_create(struct inode *dir, struct dentry *dentry, int mode,
		    struct nameidata *nd)
{
	int err;
	struct dentry *parent;
	struct vfsmount *mnt;

	LKTRTrace("i%lu, %.*s, 0x%x\n", dir->i_ino, AuDLNPair(dentry), mode);

	err = -EIO;
	parent = dget_parent(dentry);
	if (parent->d_inode == dir) {
		IMustLock(dir);
		err = vfs_create(dir, dentry, mode, nd);
	}
	if (!err) {
		mnt = NULL;
		if (nd)
			mnt = nd->mnt;
		au_update_fuse_h_inode(mnt, parent); /*ignore*/
		au_update_fuse_h_inode(mnt, dentry); /*ignore*/
	}
	dput(parent);
	AuTraceErr(err);
	return err;
}

int do_vfsub_symlink(struct inode *dir, struct dentry *dentry,
		     const char *symname, int mode)
{
	int err;
	struct dentry *parent;

	LKTRTrace("i%lu, %.*s, %s, 0x%x\n",
		  dir->i_ino, AuDLNPair(dentry), symname, mode);

	err = -EIO;
	parent = dget_parent(dentry);
	if (parent->d_inode == dir) {
		IMustLock(dir);
		err = vfs_symlink(dir, dentry, symname, mode);
	}
	if (!err) {
		au_update_fuse_h_inode(NULL, parent); /*ignore*/
		au_update_fuse_h_inode(NULL, dentry); /*ignore*/
	}
	dput(parent);
	AuTraceErr(err);
	return err;
}

int do_vfsub_mknod(struct inode *dir, struct dentry *dentry, int mode,
		   dev_t dev)
{
	int err;
	struct dentry *parent;

	LKTRTrace("i%lu, %.*s, 0x%x\n", dir->i_ino, AuDLNPair(dentry), mode);

	err = -EIO;
	parent = dget_parent(dentry);
	if (parent->d_inode == dir) {
		IMustLock(dir);
		err = vfs_mknod(dir, dentry, mode, dev);
	}
	if (!err) {
		au_update_fuse_h_inode(NULL, parent); /*ignore*/
		au_update_fuse_h_inode(NULL, dentry); /*ignore*/
	}
	dput(parent);
	AuTraceErr(err);
	return err;
}

int do_vfsub_mkdir(struct inode *dir, struct dentry *dentry, int mode)
{
	int err;
	struct dentry *parent;

	LKTRTrace("i%lu, %.*s, 0x%x\n", dir->i_ino, AuDLNPair(dentry), mode);

	err = -EIO;
	parent = dget_parent(dentry);
	if (parent->d_inode == dir) {
		IMustLock(dir);
		err = vfs_mkdir(dir, dentry, mode);
	}
	if (!err) {
		au_update_fuse_h_inode(NULL, parent); /*ignore*/
		au_update_fuse_h_inode(NULL, dentry); /*ignore*/
	}
	dput(parent);
	AuTraceErr(err);
	return err;
}

int do_vfsub_link(struct dentry *src_dentry, struct inode *dir,
		  struct dentry *dentry)
{
	int err;
	struct dentry *parent;

	LKTRTrace("%.*s, i%lu, %.*s\n",
		  AuDLNPair(src_dentry), dir->i_ino, AuDLNPair(dentry));

	err = -EIO;
	parent = dget_parent(dentry);
	if (parent->d_inode == dir) {
		IMustLock(dir);
		lockdep_off();
		err = vfs_link(src_dentry, dir, dentry);
		lockdep_on();
	}
	if (!err) {
		LKTRTrace("src_i %p, dst_i %p\n",
			  src_dentry->d_inode, dentry->d_inode);
		/* fuse has different memory inode for the same inumber */
		au_update_fuse_h_inode(NULL, src_dentry); /*ignore*/
		au_update_fuse_h_inode(NULL, parent); /*ignore*/
		au_update_fuse_h_inode(NULL, dentry); /*ignore*/
	}
	dput(parent);
	AuTraceErr(err);
	return err;
}

int do_vfsub_rename(struct inode *src_dir, struct dentry *src_dentry,
		    struct inode *dir, struct dentry *dentry)
{
	int err;
	struct dentry *parent, *src_parent;

	LKTRTrace("i%lu, %.*s, i%lu, %.*s\n",
		  src_dir->i_ino, AuDLNPair(src_dentry),
		  dir->i_ino, AuDLNPair(dentry));

	err = -ENOENT;
	parent = NULL;
	src_parent = dget_parent(src_dentry);
	if (src_parent->d_inode == src_dir) {
		err = -EIO;
		parent = dget_parent(dentry);
		if (parent->d_inode == dir)
			err = 0;
	}
	if (!err) {
		IMustLock(dir);
		IMustLock(src_dir);
		lockdep_off();
		err = vfs_rename(src_dir, src_dentry, dir, dentry);
		lockdep_on();
		AuTraceErr(err);
	}
	if (!err) {
		au_update_fuse_h_inode(NULL, parent); /*ignore*/
		au_update_fuse_h_inode(NULL, src_parent); /*ignore*/
		au_update_fuse_h_inode(NULL, src_dentry); /*ignore*/
	}
	dput(parent);
	dput(src_parent);
	AuTraceErr(err);
	return err;
}

int do_vfsub_rmdir(struct inode *dir, struct dentry *dentry)
{
	int err;
	struct dentry *parent;

	LKTRTrace("i%lu, %.*s\n", dir->i_ino, AuDLNPair(dentry));

	err = -ENOENT;
	parent = dget_parent(dentry);
	if (parent->d_inode == dir) {
		IMustLock(dir);
		lockdep_off();
		err = vfs_rmdir(dir, dentry);
		lockdep_on();
	}
	if (!err)
		au_update_fuse_h_inode(NULL, parent); /*ignore*/
	dput(parent);
	AuTraceErr(err);
	return err;
}

int do_vfsub_unlink(struct inode *dir, struct dentry *dentry)
{
	int err;
	struct dentry *parent;

	LKTRTrace("i%lu, %.*s\n", dir->i_ino, AuDLNPair(dentry));

	err = -ENOENT;
	parent = dget_parent(dentry);
	if (parent->d_inode == dir) {
		IMustLock(dir);
		/* vfs_unlink() locks inode */
		lockdep_off();
		err = vfs_unlink(dir, dentry);
		lockdep_on();
	}
	if (!err)
		au_update_fuse_h_inode(NULL, parent); /*ignore*/
	dput(parent);
	AuTraceErr(err);
	return err;
}
#endif

/* ---------------------------------------------------------------------- */

#if defined(CONFIG_AUFS_DLGT) || defined(CONFIG_AUFS_HINOTIFY)
struct permission_args {
	int *errp;
	struct inode *inode;
	int mask;
	struct nameidata *nd;
};

static void call_permission(void *args)
{
	struct permission_args *a = args;
	*a->errp = do_vfsub_permission(a->inode, a->mask, a->nd);
}

int vfsub_permission(struct inode *inode, int mask, struct nameidata *nd,
		     int dlgt)
{
	if (!dlgt)
		return do_vfsub_permission(inode, mask, nd);
	else {
		int err, wkq_err;
		struct permission_args args = {
			.errp	= &err,
			.inode	= inode,
			.mask	= mask,
			.nd	= nd
		};
		wkq_err = au_wkq_wait(call_permission, &args, /*dlgt*/1);
		if (unlikely(wkq_err))
			err = wkq_err;
		return err;
	}
}

/* ---------------------------------------------------------------------- */

struct create_args {
	int *errp;
	struct inode *dir;
	struct dentry *dentry;
	int mode;
	struct nameidata *nd;
};

static void call_create(void *args)
{
	struct create_args *a = args;
	*a->errp = do_vfsub_create(a->dir, a->dentry, a->mode, a->nd);
}

int vfsub_create(struct inode *dir, struct dentry *dentry, int mode,
		 struct nameidata *nd, int dlgt)
{
	if (!dlgt)
		return do_vfsub_create(dir, dentry, mode, nd);
	else {
		int err, wkq_err;
		struct create_args args = {
			.errp	= &err,
			.dir	= dir,
			.dentry	= dentry,
			.mode	= mode,
			.nd	= nd
		};
		wkq_err = au_wkq_wait(call_create, &args, /*dlgt*/1);
		if (unlikely(wkq_err))
			err = wkq_err;
		return err;
	}
}

struct symlink_args {
	int *errp;
	struct inode *dir;
	struct dentry *dentry;
	const char *symname;
	int mode;
};

static void call_symlink(void *args)
{
	struct symlink_args *a = args;
	*a->errp = do_vfsub_symlink(a->dir, a->dentry, a->symname, a->mode);
}

int vfsub_symlink(struct inode *dir, struct dentry *dentry, const char *symname,
		  int mode, int dlgt)
{
	if (!dlgt)
		return do_vfsub_symlink(dir, dentry, symname, mode);
	else {
		int err, wkq_err;
		struct symlink_args args = {
			.errp		= &err,
			.dir		= dir,
			.dentry		= dentry,
			.symname	= symname,
			.mode		= mode
		};
		wkq_err = au_wkq_wait(call_symlink, &args, /*dlgt*/1);
		if (unlikely(wkq_err))
			err = wkq_err;
		return err;
	}
}

struct mknod_args {
	int *errp;
	struct inode *dir;
	struct dentry *dentry;
	int mode;
	dev_t dev;
};

static void call_mknod(void *args)
{
	struct mknod_args *a = args;
	*a->errp = do_vfsub_mknod(a->dir, a->dentry, a->mode, a->dev);
}

int vfsub_mknod(struct inode *dir, struct dentry *dentry, int mode, dev_t dev,
		int dlgt)
{
	if (!dlgt)
		return do_vfsub_mknod(dir, dentry, mode, dev);
	else {
		int err, wkq_err;
		struct mknod_args args = {
			.errp	= &err,
			.dir	= dir,
			.dentry	= dentry,
			.mode	= mode,
			.dev	= dev
		};
		wkq_err = au_wkq_wait(call_mknod, &args, /*dlgt*/1);
		if (unlikely(wkq_err))
			err = wkq_err;
		return err;
	}
}

struct mkdir_args {
	int *errp;
	struct inode *dir;
	struct dentry *dentry;
	int mode;
};

static void call_mkdir(void *args)
{
	struct mkdir_args *a = args;
	*a->errp = do_vfsub_mkdir(a->dir, a->dentry, a->mode);
}

int vfsub_mkdir(struct inode *dir, struct dentry *dentry, int mode, int dlgt)
{
	if (!dlgt)
		return do_vfsub_mkdir(dir, dentry, mode);
	else {
		int err, wkq_err;
		struct mkdir_args args = {
			.errp	= &err,
			.dir	= dir,
			.dentry	= dentry,
			.mode	= mode
		};
		wkq_err = au_wkq_wait(call_mkdir, &args, /*dlgt*/1);
		if (unlikely(wkq_err))
			err = wkq_err;
		return err;
	}
}

/* ---------------------------------------------------------------------- */

struct link_args {
	int *errp;
	struct inode *dir;
	struct dentry *src_dentry, *dentry;
};

static void call_link(void *args)
{
	struct link_args *a = args;
	*a->errp = do_vfsub_link(a->src_dentry, a->dir, a->dentry);
}

int vfsub_link(struct dentry *src_dentry, struct inode *dir,
	       struct dentry *dentry, int dlgt)
{
	if (!dlgt)
		return do_vfsub_link(src_dentry, dir, dentry);
	else {
		int err, wkq_err;
		struct link_args args = {
			.errp		= &err,
			.src_dentry	= src_dentry,
			.dir		= dir,
			.dentry		= dentry
		};
		wkq_err = au_wkq_wait(call_link, &args, /*dlgt*/1);
		if (unlikely(wkq_err))
			err = wkq_err;
		return err;
	}
}

struct rename_args {
	int *errp;
	struct inode *src_dir, *dir;
	struct dentry *src_dentry, *dentry;
	struct vfsub_args *vargs;
};

static void call_rename(void *args)
{
	struct rename_args *a = args;
	vfsub_ignore(a->vargs);
	*a->errp = do_vfsub_rename(a->src_dir, a->src_dentry, a->dir,
				   a->dentry);
	if (unlikely(*a->errp))
		vfsub_unignore(a->vargs);
}

int vfsub_rename(struct inode *src_dir, struct dentry *src_dentry,
		 struct inode *dir, struct dentry *dentry,
		 struct vfsub_args *vargs)
{
	int err;

	if (!vargs->dlgt) {
		vfsub_ignore(vargs);
		err = do_vfsub_rename(src_dir, src_dentry, dir, dentry);
		if (unlikely(err))
			vfsub_unignore(vargs);
	} else {
		int wkq_err;
		struct rename_args args = {
			.errp		= &err,
			.src_dir	= src_dir,
			.src_dentry	= src_dentry,
			.dir		= dir,
			.dentry		= dentry,
			.vargs		= vargs
		};
		wkq_err = au_wkq_wait(call_rename, &args, /*dlgt*/1);
		if (unlikely(wkq_err))
			err = wkq_err;
	}
	return err;
}

struct rmdir_args {
	int *errp;
	struct inode *dir;
	struct dentry *dentry;
	struct vfsub_args *vargs;
};

static void call_rmdir(void *args)
{
	struct rmdir_args *a = args;
	vfsub_ignore(a->vargs);
	*a->errp = do_vfsub_rmdir(a->dir, a->dentry);
	if (unlikely(*a->errp))
		vfsub_unignore(a->vargs);
}

int vfsub_rmdir(struct inode *dir, struct dentry *dentry,
		struct vfsub_args *vargs)
{
	int err;

	if (!vargs->dlgt) {
		vfsub_ignore(vargs);
		err = do_vfsub_rmdir(dir, dentry);
		if (unlikely(err))
			vfsub_unignore(vargs);
	} else {
		int wkq_err;
		struct rmdir_args args = {
			.errp	= &err,
			.dir	= dir,
			.dentry	= dentry,
			.vargs	= vargs
		};
		wkq_err = au_wkq_wait(call_rmdir, &args, /*dlgt*/1);
		if (unlikely(wkq_err))
			err = wkq_err;
	}
	return err;
}

/* ---------------------------------------------------------------------- */

struct read_args {
	ssize_t *errp;
	struct file *file;
	union {
		void *kbuf;
		char __user *ubuf;
	};
	size_t count;
	loff_t *ppos;
};

static void call_read_k(void *args)
{
	struct read_args *a = args;
	LKTRTrace("%.*s, cnt %lu, pos %Ld\n",
		  AuDLNPair(a->file->f_dentry), (unsigned long)a->count,
		  *a->ppos);
	*a->errp = do_vfsub_read_k(a->file, a->kbuf, a->count, a->ppos);
}

ssize_t vfsub_read_u(struct file *file, char __user *ubuf, size_t count,
		     loff_t *ppos, int dlgt)
{
	if (!dlgt)
		return do_vfsub_read_u(file, ubuf, count, ppos);
	else {
		int wkq_err;
		ssize_t err, read;
		struct read_args args = {
			.errp	= &err,
			.file	= file,
			.count	= count,
			.ppos	= ppos
		};

		if (unlikely(!count))
			return 0;

		/*
		 * workaround an application bug.
		 * generally, read(2) or write(2) may return the value shorter
		 * than requested. But many applications don't support it,
		 * for example bash.
		 */
		err = -ENOMEM;
		if (args.count > PAGE_SIZE)
			args.count = PAGE_SIZE;
		args.kbuf = kmalloc(args.count, GFP_TEMPORARY);
		if (unlikely(!args.kbuf))
			goto out;

		read = 0;
		do {
			wkq_err = au_wkq_wait(call_read_k, &args, /*dlgt*/1);
			if (unlikely(wkq_err))
				err = wkq_err;
			if (unlikely(err > 0
				     && copy_to_user(ubuf, args.kbuf, err))) {
				err = -EFAULT;
				goto out_free;
			} else if (!err)
				break;
			else if (unlikely(err < 0))
				goto out_free;
			count -= err;
			/* do not read too much because of file i/o pointer */
			if (unlikely(count < args.count))
				args.count = count;
			ubuf += err;
			read += err;
		} while (count);
		smp_mb(); /* flush ubuf */
		err = read;

	out_free:
		kfree(args.kbuf);
	out:
		return err;
	}
}

ssize_t vfsub_read_k(struct file *file, void *kbuf, size_t count, loff_t *ppos,
		     int dlgt)
{
	if (!dlgt)
		return do_vfsub_read_k(file, kbuf, count, ppos);
	else {
		ssize_t err;
		int wkq_err;
		struct read_args args = {
			.errp	= &err,
			.file	= file,
			.count	= count,
			.ppos	= ppos
		};
		args.kbuf = kbuf;
		wkq_err = au_wkq_wait(call_read_k, &args, /*dlgt*/1);
		if (unlikely(wkq_err))
			err = wkq_err;
		return err;
	}
}

struct write_args {
	ssize_t *errp;
	struct file *file;
	union {
		void *kbuf;
		const char __user *ubuf;
	};
	void *buf;
	size_t count;
	loff_t *ppos;
	struct vfsub_args *vargs;
};

static void call_write_k(void *args)
{
	struct write_args *a = args;
	LKTRTrace("%.*s, cnt %lu, pos %Ld\n",
		  AuDLNPair(a->file->f_dentry), (unsigned long)a->count,
		  *a->ppos);
	vfsub_ignore(a->vargs);
	*a->errp = do_vfsub_write_k(a->file, a->kbuf, a->count, a->ppos);
	if (unlikely(*a->errp < 0))
		vfsub_unignore(a->vargs);
}

ssize_t vfsub_write_u(struct file *file, const char __user *ubuf, size_t count,
		      loff_t *ppos, struct vfsub_args *vargs)
{
	ssize_t err;

	if (!vargs->dlgt) {
		vfsub_ignore(vargs);
		err = do_vfsub_write_u(file, ubuf, count, ppos);
		if (unlikely(err < 0))
			vfsub_unignore(vargs);
	} else {
		ssize_t written;
		int wkq_err;
		struct write_args args = {
			.errp	= &err,
			.file	= file,
			.count	= count,
			.ppos	= ppos,
			.vargs	= vargs
		};

		if (unlikely(!count))
			return 0;

		/*
		 * workaround an application bug.
		 * generally, read(2) or write(2) may return the value shorter
		 * than requested. But many applications don't support it,
		 * for example bash.
		 */
		err = -ENOMEM;
		if (args.count > PAGE_SIZE)
			args.count = PAGE_SIZE;
		args.kbuf = kmalloc(args.count, GFP_TEMPORARY);
		if (unlikely(!args.kbuf))
			goto out;

		written = 0;
		do {
			if (unlikely(copy_from_user(args.kbuf, ubuf,
						    args.count))) {
				err = -EFAULT;
				goto out_free;
			}

			wkq_err = au_wkq_wait(call_write_k, &args, /*dlgt*/1);
			if (unlikely(wkq_err))
				err = wkq_err;
			if (err > 0) {
				count -= err;
				if (count < args.count)
					args.count = count;
				ubuf += err;
				written += err;
			} else if (!err)
				break;
			else if (unlikely(err < 0))
				goto out_free;
		} while (count);
		err = written;

	out_free:
		kfree(args.kbuf);
	}
 out:
	return err;
}

ssize_t vfsub_write_k(struct file *file, void *kbuf, size_t count, loff_t *ppos,
		      struct vfsub_args *vargs)
{
	ssize_t err;

	if (!vargs->dlgt) {
		vfsub_ignore(vargs);
		err = do_vfsub_write_k(file, kbuf, count, ppos);
		if (unlikely(err < 0))
			vfsub_unignore(vargs);
	} else {
		int wkq_err;
		struct write_args args = {
			.errp	= &err,
			.file	= file,
			.count	= count,
			.ppos	= ppos,
			.vargs	= vargs
		};
		args.kbuf = kbuf;
		wkq_err = au_wkq_wait(call_write_k, &args, /*dlgt*/1);
		if (unlikely(wkq_err))
			err = wkq_err;
	}
	return err;
}

struct readdir_args {
	int *errp;
	struct file *file;
	filldir_t filldir;
	void *arg;
};

static void call_readdir(void *args)
{
	struct readdir_args *a = args;
	*a->errp = do_vfsub_readdir(a->file, a->filldir, a->arg);
}

int vfsub_readdir(struct file *file, filldir_t filldir, void *arg, int dlgt)
{
	if (!dlgt)
		return do_vfsub_readdir(file, filldir, arg);
	else {
		int err, wkq_err;
		struct readdir_args args = {
			.errp		= &err,
			.file		= file,
			.filldir	= filldir,
			.arg		= arg
		};
		wkq_err = au_wkq_wait(call_readdir, &args, /*dlgt*/1);
		if (unlikely(wkq_err))
			err = wkq_err;
		return err;
	}
}

struct splice_to_args {
	long *errp;
	struct file *in;
	loff_t *ppos;
	struct pipe_inode_info *pipe;
	size_t len;
	unsigned int flags;
};

static void call_splice_to(void *args)
{
	struct splice_to_args *a = args;
	*a->errp = do_vfsub_splice_to(a->in, a->ppos, a->pipe, a->len,
				      a->flags);
}

long vfsub_splice_to(struct file *in, loff_t *ppos,
		     struct pipe_inode_info *pipe, size_t len,
		     unsigned int flags, int dlgt)
{
	if (!dlgt)
		return do_vfsub_splice_to(in, ppos, pipe, len, flags);
	else {
		long err;
		int wkq_err;
		struct splice_to_args args = {
			.errp	= &err,
			.in	= in,
			.ppos	= ppos,
			.pipe	= pipe,
			.len	= len,
			.flags	= flags
		};
		wkq_err = au_wkq_wait(call_splice_to, &args, /*dlgt*/1);
		if (unlikely(wkq_err))
			err = wkq_err;
		return err;
	}
}

struct splice_from_args {
	long *errp;
	struct pipe_inode_info *pipe;
	struct file *out;
	loff_t *ppos;
	size_t len;
	unsigned int flags;
	struct vfsub_args *vargs;
};

static void call_splice_from(void *args)
{
	struct splice_from_args *a = args;
	vfsub_ignore(a->vargs);
	*a->errp = do_vfsub_splice_from(a->pipe, a->out, a->ppos, a->len,
					a->flags);
	if (unlikely(*a->errp < 0))
		vfsub_unignore(a->vargs);
}

long vfsub_splice_from(struct pipe_inode_info *pipe, struct file *out,
		       loff_t *ppos, size_t len, unsigned int flags,
		       struct vfsub_args *vargs)
{
	long err;

	if (!vargs->dlgt) {
		vfsub_ignore(vargs);
		err = do_vfsub_splice_from(pipe, out, ppos, len, flags);
		if (unlikely(err < 0))
			vfsub_unignore(vargs);
	} else {
		int wkq_err;
		struct splice_from_args args = {
			.errp	= &err,
			.pipe	= pipe,
			.out	= out,
			.ppos	= ppos,
			.len	= len,
			.flags	= flags,
			.vargs	= vargs
		};
		wkq_err = au_wkq_wait(call_splice_from, &args, /*dlgt*/1);
		if (unlikely(wkq_err))
			err = wkq_err;
	}
	return err;
}

/* ---------------------------------------------------------------------- */

struct getattr_args {
	int *errp;
	struct vfsmount *mnt;
	struct dentry *dentry;
	struct kstat *st;
};

static void call_getattr(void *args)
{
	struct getattr_args *a = args;
	*a->errp = do_vfsub_getattr(a->mnt, a->dentry, a->st);
}

int vfsub_getattr(struct vfsmount *mnt, struct dentry *dentry, struct kstat *st,
		  int dlgt)
{
	if (!dlgt)
		return do_vfsub_getattr(mnt, dentry, st);
	else {
		int err, wkq_err;
		struct getattr_args args = {
			.errp	= &err,
			.mnt	= mnt,
			.dentry	= dentry,
			.st	= st
		};
		wkq_err = au_wkq_wait(call_getattr, &args, /*dlgt*/1);
		if (unlikely(wkq_err))
			err = wkq_err;
		return err;
	}
}
#endif /* CONFIG_AUFS_DLGT || CONFIG_AUFS_HINOTIFY */

/* ---------------------------------------------------------------------- */

struct au_vfsub_mkdir_args {
	int *errp;
	struct inode *dir;
	struct dentry *dentry;
	int mode;
	int dlgt;
};

static void au_call_vfsub_mkdir(void *args)
{
	struct au_vfsub_mkdir_args *a = args;
	*a->errp = vfsub_mkdir(a->dir, a->dentry, a->mode, a->dlgt);
}

int vfsub_sio_mkdir(struct inode *dir, struct dentry *dentry, int mode,
		    int dlgt)
{
	int err, do_sio, wkq_err;

	LKTRTrace("i%lu, %.*s\n", dir->i_ino, AuDLNPair(dentry));

	do_sio = au_test_perm(dir, MAY_EXEC | MAY_WRITE, dlgt);
	if (!do_sio)
		err = vfsub_mkdir(dir, dentry, mode, dlgt);
	else {
		struct au_vfsub_mkdir_args args = {
			.errp	= &err,
			.dir	= dir,
			.dentry	= dentry,
			.mode	= mode,
			.dlgt	= dlgt
		};
		wkq_err = au_wkq_wait(au_call_vfsub_mkdir, &args, /*dlgt*/0);
		if (unlikely(wkq_err))
			err = wkq_err;
	}

	AuTraceErr(err);
	return err;
}

struct au_vfsub_rmdir_args {
	int *errp;
	struct inode *dir;
	struct dentry *dentry;
	struct vfsub_args *vargs;
};

static void au_call_vfsub_rmdir(void *args)
{
	struct au_vfsub_rmdir_args *a = args;
	*a->errp = vfsub_rmdir(a->dir, a->dentry, a->vargs);
}

int vfsub_sio_rmdir(struct inode *dir, struct dentry *dentry, int dlgt)
{
	int err, do_sio, wkq_err;
	struct vfsub_args vargs;

	LKTRTrace("i%lu, %.*s\n", dir->i_ino, AuDLNPair(dentry));

	vfsub_args_init(&vargs, /*ign*/NULL, dlgt, /*force_unlink*/0);
	do_sio = au_test_perm(dir, MAY_EXEC | MAY_WRITE, dlgt);
	if (!do_sio)
		err = vfsub_rmdir(dir, dentry, &vargs);
	else {
		struct au_vfsub_rmdir_args args = {
			.errp		= &err,
			.dir		= dir,
			.dentry		= dentry,
			.vargs		= &vargs
		};
		wkq_err = au_wkq_wait(au_call_vfsub_rmdir, &args, /*dlgt*/0);
		if (unlikely(wkq_err))
			err = wkq_err;
	}

	AuTraceErr(err);
	return err;
}

/* ---------------------------------------------------------------------- */

struct notify_change_args {
	int *errp;
	struct dentry *h_dentry;
	struct iattr *ia;
	struct vfsub_args *vargs;
};

static void call_notify_change(void *args)
{
	struct notify_change_args *a = args;
	struct inode *h_inode;

	LKTRTrace("%.*s, ia_valid 0x%x\n",
		  AuDLNPair(a->h_dentry), a->ia->ia_valid);
	h_inode = a->h_dentry->d_inode;
	IMustLock(h_inode);

	*a->errp = -EPERM;
	if (!IS_IMMUTABLE(h_inode) && !IS_APPEND(h_inode)) {
		vfsub_ignore(a->vargs);
		lockdep_off();
		#ifdef CONFIG_SECURITY_APPARMOR
		*a->errp = notify_change(a->h_dentry, NULL, a->ia);
		#else
		*a->errp = notify_change(a->h_dentry, a->ia);
		#endif
		lockdep_on();
		if (!*a->errp)
			au_update_fuse_h_inode(NULL, a->h_dentry); /*ignore*/
		else
			vfsub_unignore(a->vargs);
	}
	AuTraceErr(*a->errp);
}

int vfsub_notify_change(struct dentry *dentry, struct iattr *ia,
			struct vfsub_args *vargs)
{
	int err;
	struct notify_change_args args = {
		.errp		= &err,
		.h_dentry	= dentry,
		.ia		= ia,
		.vargs		= vargs
	};

#ifndef CONFIG_AUFS_DLGT
	call_notify_change(&args);
#else
	if (!vargs->dlgt)
		call_notify_change(&args);
	else {
		int wkq_err;
		wkq_err = au_wkq_wait(call_notify_change, &args, /*dlgt*/1);
		if (unlikely(wkq_err))
			err = wkq_err;
	}
#endif

	AuTraceErr(err);
	return err;
}

/* ---------------------------------------------------------------------- */

struct unlink_args {
	int *errp;
	struct inode *dir;
	struct dentry *dentry;
	struct vfsub_args *vargs;
};

static void call_unlink(void *args)
{
	struct unlink_args *a = args;
	struct inode *h_inode;
	const int stop_sillyrename = (au_test_nfs(a->dentry->d_sb)
				      && atomic_read(&a->dentry->d_count) == 1);

	LKTRTrace("%.*s, stop_silly %d, cnt %d\n",
		  AuDLNPair(a->dentry), stop_sillyrename,
		  atomic_read(&a->dentry->d_count));
	//IMustLock(a->dir);

	if (!stop_sillyrename)
		dget(a->dentry);
	h_inode = a->dentry->d_inode;
	if (h_inode)
		atomic_inc_return(&h_inode->i_count);
#if 0 // partial testing
	{
		struct qstr *name = &a->dentry->d_name;
		if (name->len == sizeof(AUFS_XINO_FNAME) - 1
		    && !strncmp(name->name, AUFS_XINO_FNAME, name->len))
			*a->errp = do_vfsub_unlink(a->dir, a->dentry);
		else
			err = -1;
	}
#else
	*a->errp = do_vfsub_unlink(a->dir, a->dentry);
#endif

	if (!stop_sillyrename)
		dput(a->dentry);
	if (h_inode)
		iput(h_inode);

	AuTraceErr(*a->errp);
}

/*
 * @dir: must be locked.
 * @dentry: target dentry.
 */
int vfsub_unlink(struct inode *dir, struct dentry *dentry,
		 struct vfsub_args *vargs)
{
	int err;
	struct unlink_args args = {
		.errp	= &err,
		.dir	= dir,
		.dentry	= dentry,
		.vargs	= vargs
	};

	if (!vargs->dlgt && !vargs->force_unlink)
		call_unlink(&args);
	else {
		int wkq_err;
		wkq_err = au_wkq_wait(call_unlink, &args, vargs->dlgt);
		if (unlikely(wkq_err))
			err = wkq_err;
	}

	return err;
}

/* ---------------------------------------------------------------------- */

struct statfs_args {
	int *errp;
	void *arg;
	struct kstatfs *buf;
};

static void call_statfs(void *args)
{
	struct statfs_args *a = args;
	*a->errp = vfs_statfs(a->arg, a->buf);
}

int vfsub_statfs(void *arg, struct kstatfs *buf, int dlgt)
{
	int err;
	struct statfs_args args = {
		.errp	= &err,
		.arg	= arg,
		.buf	= buf
	};

#ifndef CONFIG_AUFS_DLGT
	call_statfs(&args);
#else
	if (!dlgt)
		call_statfs(&args);
	else {
		int wkq_err;
		wkq_err = au_wkq_wait(call_statfs, &args, /*dlgt*/1);
		if (unlikely(wkq_err))
			err = wkq_err;
	}
#endif
	return err;
}

/* ---------------------------------------------------------------------- */

#ifdef CONFIG_AUFS_WORKAROUND_FUSE
/* h_mnt can be NULL, is it safe? */
int au_update_fuse_h_inode(struct vfsmount *h_mnt, struct dentry *h_dentry)
{
	int err;
	struct kstat st;

	LKTRTrace("%.*s\n", AuDLNPair(h_dentry));

	err = 0;
	if (unlikely(h_dentry->d_inode
		     //&& atomic_read(&h_dentry->d_inode->i_count)
		     && au_test_fuse(h_dentry->d_sb))) {
		err = vfsub_getattr(h_mnt, h_dentry, &st, /*dlgt*/0);
		if (unlikely(err)) {
			AuDbg("err %d\n", err);
			au_debug_on();
			AuDbgDentry(h_dentry);
			au_debug_off();
			WARN_ON(err);
		}
	}
	return err;
}
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 24) \
	&& (defined(CONFIG_XFS_FS) || defined(CONFIG_XFS_FS_MODULE))
/* h_mnt can be NULL, is it safe? */
dev_t au_h_rdev(struct inode *h_inode, struct vfsmount *h_mnt,
		struct dentry *h_dentry)
{
	dev_t rdev;
	int err;
	struct kstat st;

	LKTRTrace("hi%lu\n", h_inode->i_ino);
	if (h_dentry)
		LKTRTrace("%.*s\n", AuDLNPair(h_dentry));

	rdev = h_inode->i_rdev;
	if (!rdev || !au_test_xfs(h_inode->i_sb))
		goto out;

	rdev = 0;
	if (!h_dentry) {
		err = 0;
		h_dentry = d_find_alias(h_inode);
		if (unlikely(!h_dentry))
			goto failure;
		err = PTR_ERR(h_dentry);
		if (IS_ERR(h_dentry)) {
			h_dentry = NULL;
			goto failure;
		}
		LKTRTrace("%.*s\n", AuDLNPair(h_dentry));
	} else
		dget(h_dentry);

	err = vfsub_getattr(h_mnt, h_dentry, &st, /*dlgt*/0);
	dput(h_dentry);
	if (!err) {
		rdev = st.rdev;
		goto out; /* success */
	}

 failure:
	AuIOErr("failed rdev for XFS inode, hi%lu, %d\n",
		h_inode->i_ino, err);
 out:
	return rdev;
}
#endif /* xfs rdev */

#if 0 // temp
/*
 * This function was born after a discussion with the FUSE developer.
 * The inode attributes on a filesystem who defines i_op->getattr()
 * is unreliable since such fs may not maintain the attributes at lookup.
 * This function doesn't want the result of stat, instead wants the side-effect
 * which refreshes the attributes.
 * Hmm, there seems to be no such filesystem except fuse.
 */
int vfsub_i_attr(struct vfsmount *mnt, struct dentry *dentry, int dlgt)
{
	int err;
	struct inode *inode;
	struct inode_operations *op;
	struct kstat st;

	inode = dentry->d_inode;
	AuDebugOn(!inode);

	err = 0;
	op = inode->i_op;
	if (unlikely(op && op->getattr && !au_test_aufs(dentry->d_sb))) {
		err = security_inode_getattr(mnt, dentry);
		if (!err)
			err = op->getattr(mnt, dentry, &st);
	}
	AuTraceErr(err);
	return err;
}
#endif
