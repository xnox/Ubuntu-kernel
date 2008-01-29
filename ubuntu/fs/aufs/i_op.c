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

/* $Id: i_op.c,v 1.53 2008/01/28 05:01:42 sfjro Exp $ */

//#include <linux/fs.h>
//#include <linux/namei.h>
#include <linux/security.h>
#include "aufs.h"

#ifdef CONFIG_AUFS_DLGT
struct security_inode_permission_args {
	int *errp;
	struct inode *h_inode;
	int mask;
	struct nameidata *fake_nd;
};

static void call_security_inode_permission(void *args)
{
	struct security_inode_permission_args *a = args;
	LKTRTrace("fsuid %d\n", current->fsuid);
	*a->errp = security_inode_permission(a->h_inode, a->mask, a->fake_nd);
}
#endif /* CONFIG_AUFS_DLGT */

static int h_permission(struct inode *h_inode, int mask,
			struct nameidata *fake_nd, int brperm, int dlgt)
{
	int err, submask;
	const int write_mask = (mask & (MAY_WRITE | MAY_APPEND));

	LKTRTrace("ino %lu, mask 0x%x, brperm 0x%x\n",
		  h_inode->i_ino, mask, brperm);

	err = -EACCES;
	if (unlikely((write_mask && IS_IMMUTABLE(h_inode))
		     || ((mask & MAY_EXEC) && S_ISREG(h_inode->i_mode)
			 && fake_nd && fake_nd->mnt
			 && (fake_nd->mnt->mnt_flags & MNT_NOEXEC))
		    ))
		goto out;

	/* skip hidden fs test in the case of write to ro branch */
	submask = mask & ~MAY_APPEND;
	if (unlikely((write_mask && !br_writable(brperm))
		     || !h_inode->i_op
		     || !h_inode->i_op->permission)) {
		//LKTRLabel(generic_permission);
		err = generic_permission(h_inode, submask, NULL);
	} else {
		//LKTRLabel(h_inode->permission);
		err = h_inode->i_op->permission(h_inode, submask, fake_nd);
		AuTraceErr(err);
	}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24) || defined(CONFIG_AUFS_SEC_PERM_PATCH)
	if (!err) {
#ifndef CONFIG_AUFS_DLGT
		err = security_inode_permission(h_inode, mask, fake_nd);
#else
		if (!dlgt)
			err = security_inode_permission(h_inode, mask,
							fake_nd);
		else {
			int wkq_err;
			struct security_inode_permission_args args = {
				.errp		= &err,
				.h_inode	= h_inode,
				.mask		= mask,
				.fake_nd	= fake_nd
			};
			wkq_err = au_wkq_wait(call_security_inode_permission,
					      &args, /*dlgt*/1);
			if (unlikely(wkq_err))
				err = wkq_err;
		}
#endif /* CONFIG_AUFS_DLGT */
	}
#endif

 out:
	AuTraceErr(err);
	return err;
}

static int silly_lock(struct inode *inode, struct nameidata *nd)
{
	int locked = 0;
	struct super_block *sb = inode->i_sb;

	LKTRTrace("i%lu, nd %p\n", inode->i_ino, nd);

#ifdef CONFIG_AUFS_FAKE_DM
	si_read_lock(sb, !AuLock_FLUSH);
	ii_read_lock_child(inode);
#else
	if (!nd || !nd->dentry) {
		si_read_lock(sb, !AuLock_FLUSH);
		ii_read_lock_child(inode);
	} else if (nd->dentry->d_inode != inode) {
		locked = 1;
		/* lock child first, then parent */
		si_read_lock(sb, !AuLock_FLUSH);
		ii_read_lock_child(inode);
		di_read_lock_parent(nd->dentry, 0);
	} else {
		locked = 2;
		aufs_read_lock(nd->dentry, AuLock_IR);
	}
#endif /* CONFIG_AUFS_FAKE_DM */
	return locked;
}

static void silly_unlock(int locked, struct inode *inode, struct nameidata *nd)
{
	struct super_block *sb = inode->i_sb;

	LKTRTrace("locked %d, i%lu, nd %p\n", locked, inode->i_ino, nd);

#ifdef CONFIG_AUFS_FAKE_DM
	ii_read_unlock(inode);
	si_read_unlock(sb);
#else
	switch (locked) {
	case 0:
		ii_read_unlock(inode);
		si_read_unlock(sb);
		break;
	case 1:
		di_read_unlock(nd->dentry, 0);
		ii_read_unlock(inode);
		si_read_unlock(sb);
		break;
	case 2:
		aufs_read_unlock(nd->dentry, AuLock_IR);
		break;
	default:
		BUG();
	}
#endif /* CONFIG_AUFS_FAKE_DM */
}

static int aufs_permission(struct inode *inode, int mask, struct nameidata *nd)
{
	int err, locked, dlgt;
	aufs_bindex_t bindex, bend;
	struct inode *h_inode;
	struct super_block *sb;
	struct nameidata fake_nd, *p;
	const int write_mask = (mask & (MAY_WRITE | MAY_APPEND));
	const int nondir = !S_ISDIR(inode->i_mode);

	LKTRTrace("ino %lu, mask 0x%x, nondir %d, write_mask %d, "
		  "nd %d{%d, %d}\n",
		  inode->i_ino, mask, nondir, write_mask,
		  !!nd, nd ? !!nd->dentry : 0, nd ? !!nd->mnt : 0);

	sb = inode->i_sb;
	locked = silly_lock(inode, nd);
	dlgt = au_need_dlgt(sb);

	if (nd)
		fake_nd = *nd;
	if (/* unlikely */(nondir || write_mask)) {
		h_inode = au_h_iptr(inode);
		AuDebugOn(!h_inode
			  || ((h_inode->i_mode & S_IFMT)
			      != (inode->i_mode & S_IFMT)));
		err = 0;
		bindex = ibstart(inode);
		p = au_fake_dm(&fake_nd, nd, sb, bindex);
		/* actual test will be delegated to LSM */
		if (IS_ERR(p))
			AuDebugOn(PTR_ERR(p) != -ENOENT);
		else {
			err = h_permission(h_inode, mask, p,
					   sbr_perm(sb, bindex), dlgt);
			au_fake_dm_release(p);
		}
		if (write_mask && !err) {
			/* test whether the upper writable branch exists */
			err = -EROFS;
			for (; bindex >= 0; bindex--)
				if (!br_rdonly(stobr(sb, bindex))) {
					err = 0;
					break;
				}
		}
		goto out;
	}

	/* non-write to dir */
	err = 0;
	bend = ibend(inode);
	for (bindex = ibstart(inode); !err && bindex <= bend; bindex++) {
		h_inode = au_h_iptr_i(inode, bindex);
		if (!h_inode)
			continue;
		AuDebugOn(!S_ISDIR(h_inode->i_mode));

		p = au_fake_dm(&fake_nd, nd, sb, bindex);
		/* actual test will be delegated to LSM */
		if (IS_ERR(p))
			AuDebugOn(PTR_ERR(p) != -ENOENT);
		else {
			err = h_permission(h_inode, mask, p,
					   sbr_perm(sb, bindex), dlgt);
			au_fake_dm_release(p);
		}
	}

 out:
	silly_unlock(locked, inode, nd);
	AuTraceErr(err);
	return err;
}

/* ---------------------------------------------------------------------- */

static struct dentry *aufs_lookup(struct inode *dir, struct dentry *dentry,
				  struct nameidata *nd)
{
	struct dentry *ret, *parent;
	int err, npositive;
	struct inode *inode, *h_inode;
	struct nameidata tmp_nd, *ndp;
	//todo: no more lower nameidata, only here. re-use it.

	LKTRTrace("dir %lu, %.*s, nd{0x%x}\n",
		  dir->i_ino, AuDLNPair(dentry), nd ? nd->flags : 0);
	AuDebugOn(IS_ROOT(dentry));
	IMustLock(dir);

	/* nd can be NULL */
	parent = dentry->d_parent; /* dir inode is locked */
	aufs_read_lock(parent, !AuLock_FLUSH);
	err = au_alloc_dinfo(dentry);
	//if (LktrCond) err = -1;
	ret = ERR_PTR(err);
	if (unlikely(err))
		goto out;

	ndp = au_dup_nd(stosi(dir->i_sb), &tmp_nd, nd);
	npositive = au_lkup_dentry(dentry, dbstart(parent), /*type*/0, ndp);
	err = npositive;
	LKTRLabel(here);
	//err = -1;
	ret = ERR_PTR(err);
	if (unlikely(err < 0))
		goto out_unlock;
	inode = NULL;
	if (npositive) {
		/*
		 * stop 'race'-ing between hardlinks under different parents.
		 */
		h_inode = au_h_dptr(dentry)->d_inode;
		AuDebugOn(!h_inode);
		if (h_inode->i_nlink == 1 || S_ISDIR(h_inode->i_mode))
			inode = au_new_inode(dentry);
		else {
			static DEFINE_MUTEX(mtx);
			mutex_lock(&mtx);
			inode = au_new_inode(dentry);
			mutex_unlock(&mtx);
		}
		ret = (void *)inode;
	}
	if (!IS_ERR(inode)) {
#if 1
		/* d_splice_alias() also supports d_add() */
		ret = d_splice_alias(inode, dentry);
		if (unlikely(IS_ERR(ret) && inode))
			ii_write_unlock(inode);
#else
		d_add(dentry, inode);
#endif
		//AuDbgDentry(dentry);
		AuDebugOn(nd
			  && (nd->flags & LOOKUP_OPEN)
			  && nd->intent.open.file
			  && nd->intent.open.file->f_dentry);
		au_store_fmode_exec(nd, inode);
	}

 out_unlock:
	di_write_unlock(dentry);
 out:
	aufs_read_unlock(parent, !AuLock_IR);
	AuTraceErrPtr(ret);
	return ret;
}

/* ---------------------------------------------------------------------- */

//todo: simplify
/*
 * decide the branch and the parent dir where we will create a new entry.
 * returns new bindex or an error.
 * copyup the parent dir if needed.
 */
int au_wr_dir(struct dentry *dentry, struct dentry *src_dentry,
	      struct au_wr_dir_args *args)
{
	int err;
	aufs_bindex_t bcpup, bstart, src_bstart;
	struct super_block *sb;
	struct dentry *parent, *src_parent = NULL, *h_parent;
	struct inode *dir, *src_dir = NULL;
	struct aufs_sbinfo *sbinfo;

	LKTRTrace("%.*s, src %p, {%d, %d, %d, %d}\n",
		  AuDLNPair(dentry), src_dentry, args->force_btgt,
		  args->add_entry, args->do_lock_srcdir, args->isdir);
	//AuDbgDentry(dentry);

	sb = dentry->d_sb;
	sbinfo = stosi(sb);
	parent = dget_parent(dentry);
	bstart = dbstart(dentry);
	bcpup = bstart;
	if (args->force_btgt < 0) {
		if (src_dentry) {
			src_bstart = dbstart(src_dentry);
			if (src_bstart < bstart)
				bcpup = src_bstart;
		} else if (args->add_entry) {
			err = AuWbrCreate(sbinfo, dentry, args->isdir);
			bcpup = err;
		}

		if (bcpup < 0 || au_test_ro(sb, bcpup, dentry->d_inode)) {
			if (args->add_entry)
				err = AuWbrCopyup(sbinfo, dentry);
			else {
				di_read_lock_parent(parent, !AuLock_IR);
				err = AuWbrCopyup(sbinfo, dentry);
				di_read_unlock(parent, !AuLock_IR);
			}
			//err = -1;
			bcpup = err;
			if (unlikely(err < 0))
				goto out;
		}
	} else {
		bcpup = args->force_btgt;
		AuDebugOn(au_test_ro(sb, bcpup, dentry->d_inode));
	}
	LKTRTrace("bstart %d, bcpup %d\n", bstart, bcpup);
	if (bstart < bcpup)
		au_update_dbrange(dentry, /*do_put_zero*/1);

	err = bcpup;
	if (bcpup == bstart)
		goto out; /* success */

	/* copyup the new parent into the branch we process */
	if (src_dentry) {
		src_parent = dget_parent(src_dentry);
		src_dir = src_parent->d_inode;
		if (args->do_lock_srcdir)
			di_write_lock_parent2(src_parent);
	}

	dir = parent->d_inode;
	if (args->add_entry) {
		au_update_dbstart(dentry);
		IMustLock(dir);
		DiMustWriteLock(parent);
		IiMustWriteLock(dir);
	} else
		di_write_lock_parent(parent);

	err = 0;
	if (!au_h_dptr_i(parent, bcpup)) {
		if (bstart < bcpup)
			err = au_cpdown_dirs(dentry, bcpup, src_parent);
		else
			err = au_cpup_dirs(dentry, bcpup, src_parent);
	}
	//err = -1;
	if (!err && args->add_entry) {
		h_parent = au_h_dptr_i(parent, bcpup);
		AuDebugOn(!h_parent || !h_parent->d_inode);
		vfsub_i_lock_nested(h_parent->d_inode, AuLsc_I_PARENT);
		err = au_lkup_neg(dentry, bcpup);
		//err = -1;
		vfsub_i_unlock(h_parent->d_inode);
		if (bstart < bcpup && dbstart(dentry) < 0) {
			set_dbstart(dentry, 0);
			au_update_dbrange(dentry, /*do_put_zero*/0);
		}
	}

	if (!args->add_entry)
		di_write_unlock(parent);
	if (args->do_lock_srcdir)
		di_write_unlock(src_parent);
	dput(src_parent);
	if (!err)
		err = bcpup; /* success */
	//err = -EPERM;
 out:
	dput(parent);
	LKTRTrace("err %d\n", err);
	AuTraceErr(err);
	return err;
}

/* ---------------------------------------------------------------------- */

static int aufs_setattr(struct dentry *dentry, struct iattr *ia)
{
	int err;
	aufs_bindex_t bstart, bcpup;
	struct inode *h_inode, *inode, *dir, *h_dir, *gdir;
	struct dentry *h_dentry, *parent, *hi_wh, *gparent;
	unsigned int udba;
	struct aufs_hin_ignore ign;
	struct vfsub_args vargs;
	struct super_block *sb;
	__u32 events;
	struct au_cpup_flags cflags;
	struct au_wr_dir_args wr_dir_args = {
		.force_btgt	= -1,
		.add_entry	= 0,
		.do_lock_srcdir	= 0,
		.isdir		= S_ISDIR(dentry->d_inode->i_mode)
	};

	LKTRTrace("%.*s, ia_valid 0x%x\n", AuDLNPair(dentry), ia->ia_valid);
	inode = dentry->d_inode;
	IMustLock(inode);

	aufs_read_lock(dentry, AuLock_DW);
	bstart = dbstart(dentry);
	err = au_wr_dir(dentry, /*src_dentry*/NULL, &wr_dir_args);
	bcpup = err;
	//err = -1;
	if (unlikely(err < 0))
		goto out;

	/* crazy udba locks */
	sb = dentry->d_sb;
	udba = au_flag_test_udba_inotify(sb);
	parent = NULL;
	dir = NULL;
	if (!IS_ROOT(dentry)) {
		parent = dget_parent(dentry);
		dir = parent->d_inode;
		di_read_lock_parent(parent, AuLock_IR);
	}
	//todo: meaningless lock if CONFIG_AUFS_DEBUG is disabled.
	gparent = NULL;
	gdir = NULL;
	if (unlikely(udba && parent && !IS_ROOT(parent))) {
		gparent = dget_parent(parent);
		gdir = gparent->d_inode;
		ii_read_lock_parent2(gdir);
	}

	h_dentry = au_h_dptr(dentry);
	h_inode = h_dentry->d_inode;
	AuDebugOn(!h_inode);

#define HiLock(bindex) \
	do { \
		if (!wr_dir_args.isdir) \
			vfsub_i_lock_nested(h_inode, AuLsc_I_CHILD); \
		else \
			hdir2_lock(h_inode, inode, bindex); \
	} while (0)
#define HiUnlock(bindex) \
	do { \
		if (!wr_dir_args.isdir) \
			vfsub_i_unlock(h_inode); \
		else \
			hdir_unlock(h_inode, inode, bindex); \
	} while (0)

	if (bstart != bcpup) {
		loff_t size = -1;

		if ((ia->ia_valid & ATTR_SIZE)
		    && ia->ia_size < i_size_read(inode)) {
			size = ia->ia_size;
			ia->ia_valid &= ~ATTR_SIZE;
		}
		hi_wh = NULL;
		h_dir = au_h_iptr_i(dir, bcpup);
		hdir_lock(h_dir, dir, bcpup);
		HiLock(bstart);
		if (!d_unhashed(dentry)) {
			cflags.dtime = 1;
			err = au_sio_cpup_simple(dentry, bcpup, size, &cflags);
		} else {
			hi_wh = au_hi_wh(inode, bcpup);
			if (!hi_wh) {
				err = au_sio_cpup_wh(dentry, bcpup, size,
						     /*file*/NULL);
				if (!err)
					hi_wh = au_hi_wh(inode, bcpup);
			}
#if 0
			revalidate hi_wh
#endif
		}

		//err = -1;
		HiUnlock(bstart);
		hdir_unlock(h_dir, dir, bcpup);
		if (unlikely(err || !ia->ia_valid))
			goto out_unlock;

		if (!hi_wh)
			h_dentry = au_h_dptr(dentry);
		else
			h_dentry = hi_wh; /* do not dget here */
		h_inode = h_dentry->d_inode;
		AuDebugOn(!h_inode);
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 24)
	if (ia->ia_valid & (ATTR_KILL_SUID | ATTR_KILL_SGID))
		ia->ia_valid &= ~ATTR_MODE;
#endif

	vfsub_args_init(&vargs, &ign, au_need_dlgt(sb), 0);
	if (unlikely(udba && dir)) {
		events = vfsub_events_notify_change(ia);
		if (events)
			vfsub_ign_hinode(&vargs, events, itohi(dir, bcpup));
	}
	HiLock(bcpup);
	err = vfsub_notify_change(h_dentry, ia, &vargs);
	//err = -1;
	if (!err)
		au_cpup_attr_changeable(inode);
	HiUnlock(bcpup);
#undef HiLock
#undef HiUnlock

 out_unlock:
	if (parent) {
		di_read_unlock(parent, AuLock_IR);
		dput(parent);
	}
	if (unlikely(gdir)) {
		ii_read_unlock(gdir);
		dput(gparent);
	}
 out:
	aufs_read_unlock(dentry, AuLock_DW);
	AuTraceErr(err);
	return err;
}

/* currently, for fuse only */
#ifdef CONFIG_AUFS_WORKAROUND_FUSE
static int aufs_getattr(struct vfsmount *mnt, struct dentry *dentry,
			struct kstat *st)
{
	int err;
	struct inode *inode, *h_inode;
	struct dentry *h_dentry;

	LKTRTrace("%.*s\n", AuDLNPair(dentry));

	err = 0;
	aufs_read_lock(dentry, AuLock_IR);
	inode = dentry->d_inode;
	h_inode = au_h_iptr(inode);
	if (unlikely(au_test_fuse(h_inode->i_sb))) {
		h_dentry = d_find_alias(h_inode);
		/* simply gave up updating fuse inode */
		if (h_dentry) {
			/*ignore*/
			if (!au_update_fuse_h_inode(NULL, h_dentry))
				au_cpup_attr_all(inode);
			dput(h_dentry);
		}
	}
	generic_fillattr(inode, st);

	aufs_read_unlock(dentry, AuLock_IR);
	return err;
}
#endif /* CONFIG_AUFS_WORKAROUND_FUSE */

/* ---------------------------------------------------------------------- */

static int h_readlink(struct dentry *dentry, int bindex, char __user *buf,
		      int bufsiz)
{
	struct super_block *sb;
	struct dentry *h_dentry;

	LKTRTrace("%.*s, b%d, %d\n", AuDLNPair(dentry), bindex, bufsiz);

	h_dentry = au_h_dptr_i(dentry, bindex);
	if (unlikely(!h_dentry->d_inode->i_op
		     || !h_dentry->d_inode->i_op->readlink))
		return -EINVAL;

	sb = dentry->d_sb;
	if (!au_test_ro(sb, bindex, dentry->d_inode)) {
		touch_atime(sbr_mnt(sb, bindex), h_dentry);
		au_update_fuse_h_inode(NULL, h_dentry); /*ignore*/
		dentry->d_inode->i_atime = h_dentry->d_inode->i_atime;
	}
	return h_dentry->d_inode->i_op->readlink(h_dentry, buf, bufsiz);
}

static int aufs_readlink(struct dentry *dentry, char __user *buf, int bufsiz)
{
	int err;

	LKTRTrace("%.*s, %d\n", AuDLNPair(dentry), bufsiz);

	aufs_read_lock(dentry, AuLock_IR);
	err = h_readlink(dentry, dbstart(dentry), buf, bufsiz);
	//err = -1;
	aufs_read_unlock(dentry, AuLock_IR);
	AuTraceErr(err);
	return err;
}

static void *aufs_follow_link(struct dentry *dentry, struct nameidata *nd)
{
	int err;
	char *buf;
	mm_segment_t old_fs;

	LKTRTrace("%.*s, nd %.*s\n", AuDLNPair(dentry), AuDLNPair(nd->dentry));

	err = -ENOMEM;
	buf = __getname();
	//buf = NULL;
	if (unlikely(!buf))
		goto out;

	aufs_read_lock(dentry, AuLock_IR);
	old_fs = get_fs();
	set_fs(KERNEL_DS);
	err = h_readlink(dentry, dbstart(dentry), (char __user *)buf, PATH_MAX);
	//err = -1;
	set_fs(old_fs);
	aufs_read_unlock(dentry, AuLock_IR);

	if (err >= 0) {
		buf[err] = 0;
		/* will be freed by put_link */
		nd_set_link(nd, buf);
		return NULL; /* success */
	}
	__putname(buf);

 out:
	path_release(nd);
	AuTraceErr(err);
	return ERR_PTR(err);
}

static void aufs_put_link(struct dentry *dentry, struct nameidata *nd,
			  void *cookie)
{
	LKTRTrace("%.*s\n", AuDLNPair(dentry));
	__putname(nd_get_link(nd));
}

/* ---------------------------------------------------------------------- */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 22)
static void aufs_truncate_range(struct inode *inode, loff_t start, loff_t end)
{
	AuUnsupport();
}
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 23)
static long aufs_fallocate(struct inode *inode, int mode, loff_t offset,
			   loff_t len)
{
	AuUnsupport();
	return -ENOSYS;
}
#endif

/* ---------------------------------------------------------------------- */

struct inode_operations aufs_symlink_iop = {
	.permission	= aufs_permission,
	.setattr	= aufs_setattr,
#ifdef CONFIG_AUFS_WORKAROUND_FUSE
	.getattr	= aufs_getattr,
#endif

	.readlink	= aufs_readlink,
	.follow_link	= aufs_follow_link,
	.put_link	= aufs_put_link
};

struct inode_operations aufs_dir_iop = {
	.create		= aufs_create,
	.lookup		= aufs_lookup,
	.link		= aufs_link,
	.unlink		= aufs_unlink,
	.symlink	= aufs_symlink,
	.mkdir		= aufs_mkdir,
	.rmdir		= aufs_rmdir,
	.mknod		= aufs_mknod,
	.rename		= aufs_rename,

	.permission	= aufs_permission,
	.setattr	= aufs_setattr,
#ifdef CONFIG_AUFS_WORKAROUND_FUSE
	.getattr	= aufs_getattr,
#endif

#if 0 // xattr
	.setxattr	= aufs_setxattr,
	.getxattr	= aufs_getxattr,
	.listxattr	= aufs_listxattr,
	.removexattr	= aufs_removexattr,
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 23)
	.fallocate	= aufs_fallocate
#endif
};

struct inode_operations aufs_iop = {
	.permission	= aufs_permission,
	.setattr	= aufs_setattr,
#ifdef CONFIG_AUFS_WORKAROUND_FUSE
	.getattr	= aufs_getattr,
#endif

#if 0 // xattr
	.setxattr	= aufs_setxattr,
	.getxattr	= aufs_getxattr,
	.listxattr	= aufs_listxattr,
	.removexattr	= aufs_removexattr,
#endif

	//void (*truncate) (struct inode *);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 22)
	.truncate_range	= aufs_truncate_range,
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 23)
	.fallocate	= aufs_fallocate
#endif
};
