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

/* $Id: cpup.c,v 1.60 2008/01/21 04:55:55 sfjro Exp $ */

#include "aufs.h"

/* violent cpup_attr_*() functions don't care inode lock */
void au_cpup_attr_timesizes(struct inode *inode)
{
	struct inode *h_inode;

	LKTRTrace("i%lu\n", inode->i_ino);
	//IMustLock(inode);
	h_inode = au_h_iptr(inode);
	AuDebugOn(!h_inode);
	//IMustLock(!h_inode);

	inode->i_atime = h_inode->i_atime;
	inode->i_mtime = h_inode->i_mtime;
	inode->i_ctime = h_inode->i_ctime;
	spin_lock(&inode->i_lock);
	i_size_write(inode, i_size_read(h_inode));
	inode->i_blocks = h_inode->i_blocks;
	spin_unlock(&inode->i_lock);
}

void au_cpup_attr_nlink(struct inode *inode)
{
	struct inode *h_inode;

	LKTRTrace("i%lu\n", inode->i_ino);
	//IMustLock(inode);
	AuDebugOn(!inode->i_mode);

	h_inode = au_h_iptr(inode);
	inode->i_nlink = h_inode->i_nlink;

	/*
	 * fewer nlink makes find(1) noisy, but larger nlink doesn't.
	 * it may includes whplink directory.
	 */
	if (unlikely(S_ISDIR(h_inode->i_mode))) {
		aufs_bindex_t bindex, bend;
		bend = ibend(inode);
		for (bindex = ibstart(inode) + 1; bindex <= bend; bindex++) {
			h_inode = au_h_iptr_i(inode, bindex);
			if (h_inode)
				au_add_nlink(inode, h_inode);
		}
	}
}

void au_cpup_attr_changeable(struct inode *inode)
{
	struct inode *h_inode;

	LKTRTrace("i%lu\n", inode->i_ino);
	//IMustLock(inode);
	h_inode = au_h_iptr(inode);
	AuDebugOn(!h_inode);

	inode->i_mode = h_inode->i_mode;
	inode->i_uid = h_inode->i_uid;
	inode->i_gid = h_inode->i_gid;
	au_cpup_attr_timesizes(inode);

	//??
	inode->i_flags = h_inode->i_flags;
}

void au_cpup_igen(struct inode *inode, struct inode *h_inode)
{
	inode->i_generation = h_inode->i_generation;
	itoii(inode)->ii_hsb1 = h_inode->i_sb;
}

void au_cpup_attr_all(struct inode *inode)
{
	struct inode *h_inode;

	LKTRTrace("i%lu\n", inode->i_ino);
	//IMustLock(inode);
	h_inode = au_h_iptr(inode);
	AuDebugOn(!h_inode);

	au_cpup_attr_changeable(inode);
	if (inode->i_nlink > 0)
		au_cpup_attr_nlink(inode);

	switch (inode->i_mode & S_IFMT) {
	case S_IFBLK:
	case S_IFCHR:
		inode->i_rdev = au_h_rdev(h_inode, /*h_mnt*/NULL,
					  /*h_dentry*/NULL);
	}
	inode->i_blkbits = h_inode->i_blkbits;
	au_cpup_attr_blksize(inode, h_inode);
	au_cpup_igen(inode, h_inode);
}

/* ---------------------------------------------------------------------- */

/* Note: dt_dentry and dt_hidden_dentry are not dget/dput-ed */

/* keep the timestamps of the parent dir when cpup */
void au_dtime_store(struct au_dtime *dt, struct dentry *dentry,
		    struct dentry *h_dentry, struct aufs_hinode *hdir)
{
	struct inode *inode;

	LKTRTrace("%.*s, hdir %d\n", AuDLNPair(dentry), !!hdir);
	AuDebugOn(!dentry || !h_dentry || !h_dentry->d_inode);

	dt->dt_dentry = dentry;
	dt->dt_h_dentry = h_dentry;
	dt->dt_hdir = hdir;
	inode = h_dentry->d_inode;
	dt->dt_atime = inode->i_atime;
	dt->dt_mtime = inode->i_mtime;
	//smp_mb();
}

void au_dtime_revert(struct au_dtime *dt)
{
	struct iattr attr;
	int err;
	struct aufs_hin_ignore ign;
	struct vfsub_args vargs;

	LKTRTrace("%.*s\n", AuDLNPair(dt->dt_dentry));

	attr.ia_atime = dt->dt_atime;
	attr.ia_mtime = dt->dt_mtime;
	attr.ia_valid = ATTR_FORCE | ATTR_MTIME | ATTR_MTIME_SET
		| ATTR_ATIME | ATTR_ATIME_SET;

	vfsub_args_init(&vargs, &ign, au_need_dlgt(dt->dt_dentry->d_sb), 0);
	if (unlikely(dt->dt_hdir))
		vfsub_ign_hinode(&vargs, IN_ATTRIB, dt->dt_hdir);
	err = vfsub_notify_change(dt->dt_h_dentry, &attr, &vargs);
	if (unlikely(err))
		AuWarn("restoring timestamps failed(%d). ignored\n", err);
}

/* ---------------------------------------------------------------------- */

static int cpup_iattr(struct dentry *h_dst, struct dentry *h_src, int dlgt)
{
	int err, sbits;
	struct iattr ia;
	struct inode *h_isrc, *h_idst;
	struct vfsub_args vargs;

	LKTRTrace("%.*s\n", AuDLNPair(h_dst));
	h_idst = h_dst->d_inode;
	//IMustLock(h_idst);
	h_isrc = h_src->d_inode;
	//IMustLock(h_isrc);

	ia.ia_valid = ATTR_FORCE | ATTR_MODE | ATTR_UID | ATTR_GID
		| ATTR_ATIME | ATTR_MTIME
		| ATTR_ATIME_SET | ATTR_MTIME_SET;
	ia.ia_mode = h_isrc->i_mode;
	ia.ia_uid = h_isrc->i_uid;
	ia.ia_gid = h_isrc->i_gid;
	ia.ia_atime = h_isrc->i_atime;
	ia.ia_mtime = h_isrc->i_mtime;
	sbits = !!(ia.ia_mode & (S_ISUID | S_ISGID));

	vfsub_args_init(&vargs, NULL, dlgt, /*force_unlink*/0);
	err = vfsub_notify_change(h_dst, &ia, &vargs);
	//if (LktrCond) err = -1;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 24)
	/* is this nfs only? */
	if (!err && sbits && au_test_nfs(h_dst->d_sb)) {
		ia.ia_valid = ATTR_FORCE | ATTR_MODE;
		ia.ia_mode = h_isrc->i_mode;
		err = vfsub_notify_change(h_dst, &ia, &vargs);
	}
#endif
	if (!err)
		h_idst->i_flags = h_isrc->i_flags; //??

	AuTraceErr(err);
	return err;
}

/*
 * to support a sparse file which is opened with O_APPEND,
 * we need to close the file.
 */
static int cpup_regular(struct dentry *dentry, aufs_bindex_t bdst,
			aufs_bindex_t bsrc, loff_t len)
{
	int err, i;
	struct super_block *sb;
	struct inode *h_inode;
	enum { SRC, DST };
	struct {
		aufs_bindex_t bindex;
		unsigned int flags;
		struct dentry *dentry;
		struct file *file;
		void *label, *label_file;
	} *h, hidden[] = {
		{
			.bindex = bsrc,
			.flags = O_RDONLY | O_NOATIME | O_LARGEFILE,
			.file = NULL,
			.label = &&out,
			.label_file = &&out_src_file
		},
		{
			.bindex = bdst,
			.flags = O_WRONLY | O_NOATIME | O_LARGEFILE,
			.file = NULL,
			.label = &&out_src_file,
			.label_file = &&out_dst_file
		}
	};

	LKTRTrace("dentry %.*s, bdst %d, bsrc %d, len %lld\n",
		  AuDLNPair(dentry), bdst, bsrc, len);
	AuDebugOn(bsrc <= bdst);
	AuDebugOn(!len);
	sb = dentry->d_sb;
	AuDebugOn(au_test_ro(sb, bdst, dentry->d_inode));
	/* bsrc branch can be ro/rw. */

	h = hidden;
	for (i = 0; i < 2; i++, h++) {
		h->dentry = au_h_dptr_i(dentry, h->bindex);
		AuDebugOn(!h->dentry);
		h_inode = h->dentry->d_inode;
		AuDebugOn(!h_inode || !S_ISREG(h_inode->i_mode));
		h->file = au_h_open(dentry, h->bindex, h->flags, /*file*/NULL);
		//if (LktrCond)
		//{fput(h->file);sbr_put(sb, h->bindex);h->file=ERR_PTR(-1);}
		err = PTR_ERR(h->file);
		if (IS_ERR(h->file))
			goto *h->label;
		err = -EINVAL;
		if (unlikely(!h->file->f_op))
			goto *h->label_file;
	}

	/* stop updating while we copyup */
	IMustLock(hidden[SRC].dentry->d_inode);
	err = au_copy_file(hidden[DST].file, hidden[SRC].file, len, sb);

 out_dst_file:
	fput(hidden[DST].file);
	sbr_put(sb, hidden[DST].bindex);
 out_src_file:
	fput(hidden[SRC].file);
	sbr_put(sb, hidden[SRC].bindex);
 out:
	AuTraceErr(err);
	return err;
}

/* return with hidden dst inode is locked */
static int cpup_entry(struct dentry *dentry, aufs_bindex_t bdst,
		      aufs_bindex_t bsrc, loff_t len,
		      struct au_cpup_flags *flags, int dlgt)
{
	int err, symlen;
	struct dentry *h_src, *h_dst, *h_parent, *parent, *gparent;
	struct inode *h_inode, *h_dir, *dir;
	struct au_dtime dt;
	umode_t mode;
	char *sym;
	mm_segment_t old_fs;
	struct super_block *sb;
	struct vfsub_args vargs;
	struct aufs_hinode *hgdir;
	const int do_dt = flags->dtime;

	LKTRTrace("%.*s, i%lu, bdst %d, bsrc %d, len %Ld, dtime %u\n",
		  AuDLNPair(dentry), dentry->d_inode->i_ino, bdst, bsrc, len,
		  do_dt);
	sb = dentry->d_sb;
	AuDebugOn(bdst >= bsrc || au_test_ro(sb, bdst, NULL));
	/* bsrc branch can be ro/rw. */

	h_src = au_h_dptr_i(dentry, bsrc);
	AuDebugOn(!h_src);
	h_inode = h_src->d_inode;
	AuDebugOn(!h_inode);

	/* stop refrencing while we are creating */
	parent = dget_parent(dentry);
	dir = parent->d_inode;
	h_dst = au_h_dptr_i(dentry, bdst);
	AuDebugOn(h_dst && h_dst->d_inode);
	h_parent = h_dst->d_parent; /* dir inode is locked */
	h_dir = h_parent->d_inode;
	IMustLock(h_dir);

	if (do_dt) {
		hgdir = NULL;
		if (unlikely(au_flag_test_udba_inotify(sb)
			     && !IS_ROOT(parent))) {
			gparent = dget_parent(parent);
			hgdir = itohi(gparent->d_inode, bdst);
			dput(gparent);
		}
		au_dtime_store(&dt, parent, h_parent, hgdir);
	}

	mode = h_inode->i_mode;
	switch (mode & S_IFMT) {
	case S_IFREG:
		/* stop updating while we are referencing */
		IMustLock(h_inode);
		err = au_h_create(h_dir, h_dst, mode | S_IWUSR, dlgt, NULL,
				  au_nfsmnt(sb, bdst));
		//if (LktrCond) {vfs_unlink(h_dir, h_dst); err = -1;}
		if (!err) {
			loff_t l = i_size_read(h_inode);
			if (len == -1 || l < len)
				len = l;
			if (len) {
				err = cpup_regular(dentry, bdst, bsrc, len);
				//if (LktrCond) err = -1;
			}
			if (unlikely(err)) {
				int rerr;
				vfsub_args_init(&vargs, NULL, dlgt, 0);
				rerr = vfsub_unlink(h_dir, h_dst, &vargs);
				if (rerr) {
					AuIOErr("failed unlinking cpup-ed %.*s"
						"(%d, %d)\n",
						AuDLNPair(h_dst), err, rerr);
					err = -EIO;
				}
			}
		}
		break;
	case S_IFDIR:
		err = vfsub_mkdir(h_dir, h_dst, mode, dlgt);
		//if (LktrCond) {vfs_rmdir(h_dir, h_dst); err = -1;}
		if (!err) {
			/* setattr case: dir is not locked */
			if (0 && ibstart(dir) == bdst)
				au_cpup_attr_nlink(dir);
			au_cpup_attr_nlink(dentry->d_inode);
		}
		break;
	case S_IFLNK:
		err = -ENOMEM;
		sym = __getname();
		//if (LktrCond) {__putname(sym); sym = NULL;}
		if (unlikely(!sym))
			break;
		old_fs = get_fs();
		set_fs(KERNEL_DS);
		symlen = h_inode->i_op->readlink(h_src, (char __user *)sym,
						 PATH_MAX);
		err = symlen;
		//if (LktrCond) err = symlen = -1;
		set_fs(old_fs);
		if (symlen > 0) {
			sym[symlen] = 0;
			err = vfsub_symlink(h_dir, h_dst, sym, mode, dlgt);
			//if (LktrCond)
			//{vfs_unlink(h_dir, h_dst); err = -1;}
		}
		__putname(sym);
		break;
	case S_IFCHR:
	case S_IFBLK:
		AuDebugOn(!capable(CAP_MKNOD));
		/*FALLTHROUGH*/
	case S_IFIFO:
	case S_IFSOCK:
		err = vfsub_mknod(h_dir, h_dst, mode,
				  au_h_rdev(h_inode, /*h_mnt*/NULL, h_src),
				  dlgt);
		//if (LktrCond) {vfs_unlink(h_dir, h_dst); err = -1;}
		break;
	default:
		AuIOErr("Unknown inode type 0%o\n", mode);
		err = -EIO;
	}

	if (do_dt)
		au_dtime_revert(&dt);
	dput(parent);
	AuTraceErr(err);
	return err;
}

/*
 * copyup the @dentry from @bsrc to @bdst.
 * the caller must set the both of hidden dentries.
 * @len is for trucating when it is -1 copyup the entire file.
 */
int au_cpup_single(struct dentry *dentry, aufs_bindex_t bdst,
		   aufs_bindex_t bsrc, loff_t len, struct au_cpup_flags *flags)
{
	int err, rerr, isdir, dlgt;
	struct dentry *h_src, *h_dst, *parent, *h_parent, *gparent;
	struct inode *dst_inode, *h_dir, *inode, *src_inode, *dir;
	struct super_block *sb;
	aufs_bindex_t old_ibstart;
	struct au_dtime dt;
	struct vfsub_args vargs;
	struct aufs_sbinfo *sbinfo;
	struct aufs_hinode *hgdir;

	LKTRTrace("%.*s, i%lu, bdst %d, bsrc %d, len %Ld, dtime %u\n",
		  AuDLNPair(dentry), dentry->d_inode->i_ino, bdst, bsrc, len,
		  flags->dtime);
	sb = dentry->d_sb;
	AuDebugOn(bsrc <= bdst);
	h_dst = au_h_dptr_i(dentry, bdst);
	AuDebugOn(!h_dst || h_dst->d_inode);
	h_parent = h_dst->d_parent; /* dir inode is locked */
	h_dir = h_parent->d_inode;
	IMustLock(h_dir);
	h_src = au_h_dptr_i(dentry, bsrc);
	AuDebugOn(!h_src || !h_src->d_inode);
	inode = dentry->d_inode;
	IiMustWriteLock(inode);
	parent = dget_parent(dentry);
	dir = parent->d_inode;

	sbinfo = stosi(sb);
	dlgt = au_need_dlgt(sb);
	dst_inode = au_h_iptr_i(inode, bdst);
	if (unlikely(dst_inode)) {
		if (unlikely(!AuFlag(sbinfo, f_plink))) {
			err = -EIO;
			AuIOErr("i%lu exists on a upper branch "
				"but plink is disabled\n", inode->i_ino);
			goto out;
		}

		if (dst_inode->i_nlink) {
			h_src = au_lkup_plink(sb, bdst, inode);
			err = PTR_ERR(h_src);
			if (IS_ERR(h_src))
				goto out;
			AuDebugOn(!h_src->d_inode);
			err = vfsub_link(h_src, h_dir, h_dst, dlgt);
			dput(h_src);
			goto out;
		} else
			//todo: cpup_wh_file
			/* udba work */
			au_update_brange(inode, 1);
	}

	old_ibstart = ibstart(inode);
	err = cpup_entry(dentry, bdst, bsrc, len, flags, dlgt);
	if (unlikely(err))
		goto out;
	dst_inode = h_dst->d_inode;
	vfsub_i_lock_nested(dst_inode, AuLsc_I_CHILD2);

	//todo: test dlgt
	err = cpup_iattr(h_dst, h_src, dlgt);
	//if (LktrCond) err = -1;
#if 0 // xattr
	if (0 && !err)
		err = cpup_xattrs(h_src, h_dst);
#endif
	isdir = S_ISDIR(dst_inode->i_mode);
	if (!err) {
		if (bdst < old_ibstart)
			set_ibstart(inode, bdst);
		set_h_iptr(inode, bdst, igrab(dst_inode),
			   au_hi_flags(inode, isdir));
		vfsub_i_unlock(dst_inode);
		src_inode = h_src->d_inode;
		if (!isdir
		    && src_inode->i_nlink > 1
		    && AuFlag(sbinfo, f_plink))
			au_append_plink(sb, inode, h_dst, bdst);
		goto out; /* success */
	}

	/* revert */
	vfsub_i_unlock(dst_inode);
	hgdir = NULL;
	if (unlikely(au_flag_test_udba_inotify(sb) && !IS_ROOT(parent))) {
		gparent = dget_parent(parent);
		hgdir = itohi(gparent->d_inode, bdst);
		dput(gparent);
	}
	au_dtime_store(&dt, parent, h_parent, hgdir);
	vfsub_args_init(&vargs, NULL, dlgt, 0);
	if (!isdir)
		rerr = vfsub_unlink(h_dir, h_dst, &vargs);
	else
		rerr = vfsub_rmdir(h_dir, h_dst, &vargs);
	//rerr = -1;
	au_dtime_revert(&dt);
	if (rerr) {
		AuIOErr("failed removing broken entry(%d, %d)\n", err, rerr);
		err = -EIO;
	}

 out:
	dput(parent);
	AuTraceErr(err);
	return err;
}

struct au_cpup_single_args {
	int *errp;
	struct dentry *dentry;
	aufs_bindex_t bdst, bsrc;
	loff_t len;
	struct au_cpup_flags *flags;
};

static void au_call_cpup_single(void *args)
{
	struct au_cpup_single_args *a = args;
	*a->errp = au_cpup_single(a->dentry, a->bdst, a->bsrc, a->len,
				  a->flags);
}

int au_sio_cpup_single(struct dentry *dentry, aufs_bindex_t bdst,
		       aufs_bindex_t bsrc, loff_t len,
		       struct au_cpup_flags *flags)
{
	int err, wkq_err;
	struct dentry *h_dentry;
	umode_t mode;

	LKTRTrace("%.*s, i%lu, bdst %d, bsrc %d, len %Ld, dtime %u\n",
		  AuDLNPair(dentry), dentry->d_inode->i_ino, bdst, bsrc, len,
		  flags->dtime);

	h_dentry = au_h_dptr_i(dentry, bsrc);
	mode = h_dentry->d_inode->i_mode & S_IFMT;
	if ((mode != S_IFCHR && mode != S_IFBLK)
	    || capable(CAP_MKNOD))
		err = au_cpup_single(dentry, bdst, bsrc, len, flags);
	else {
		struct au_cpup_single_args args = {
			.errp		= &err,
			.dentry		= dentry,
			.bdst		= bdst,
			.bsrc		= bsrc,
			.len		= len,
			.flags		= flags
		};
		wkq_err = au_wkq_wait(au_call_cpup_single, &args, /*dlgt*/0);
		if (unlikely(wkq_err))
			err = wkq_err;
	}

	AuTraceErr(err);
	return err;
}

/*
 * copyup the @dentry from the first active hidden branch to @bdst,
 * using au_cpup_single().
 */
int au_cpup_simple(struct dentry *dentry, aufs_bindex_t bdst, loff_t len,
		   struct au_cpup_flags *flags)
{
	int err;
	struct inode *inode;
	aufs_bindex_t bsrc, bend;

	LKTRTrace("%.*s, bdst %d, len %Ld, dtime %u\n",
		  AuDLNPair(dentry), bdst, len, flags->dtime);
	inode = dentry->d_inode;
	AuDebugOn(!S_ISDIR(inode->i_mode) && dbstart(dentry) < bdst);

	bend = dbend(dentry);
	for (bsrc = bdst + 1; bsrc <= bend; bsrc++)
		if (au_h_dptr_i(dentry, bsrc))
			break;
	AuDebugOn(!au_h_dptr_i(dentry, bsrc));

	err = au_lkup_neg(dentry, bdst);
	//err = -1;
	if (!err) {
		err = au_cpup_single(dentry, bdst, bsrc, len, flags);
		if (!err)
			return 0; /* success */

		/* revert */
		set_h_dptr(dentry, bdst, NULL);
		set_dbstart(dentry, bsrc);
	}

	AuTraceErr(err);
	return err;
}

struct au_cpup_simple_args {
	int *errp;
	struct dentry *dentry;
	aufs_bindex_t bdst;
	loff_t len;
	struct au_cpup_flags *flags;
};

static void au_call_cpup_simple(void *args)
{
	struct au_cpup_simple_args *a = args;
	*a->errp = au_cpup_simple(a->dentry, a->bdst, a->len, a->flags);
}

int au_sio_cpup_simple(struct dentry *dentry, aufs_bindex_t bdst, loff_t len,
		       struct au_cpup_flags *flags)
{
	int err, do_sio, dlgt, wkq_err;
	struct dentry *parent;
	struct inode *h_dir, *dir;

	LKTRTrace("%.*s, b%d, len %Ld, dtime %u\n",
		  AuDLNPair(dentry), bdst, len, flags->dtime);

	parent = dget_parent(dentry);
	dir = parent->d_inode;
	h_dir = au_h_iptr_i(dir, bdst);
	dlgt = au_need_dlgt(dir->i_sb);
	do_sio = au_test_perm(h_dir, MAY_EXEC | MAY_WRITE, dlgt);
	if (!do_sio) {
		/*
		 * testing CAP_MKNOD is for generic fs,
		 * but CAP_FSETID is for xfs only, currently.
		 */
		umode_t mode = dentry->d_inode->i_mode;
		do_sio = (((mode & (S_IFCHR | S_IFBLK))
			   && !capable(CAP_MKNOD))
			  || ((mode & (S_ISUID | S_ISGID))
			      && !capable(CAP_FSETID)));
	}
	if (!do_sio)
		err = au_cpup_simple(dentry, bdst, len, flags);
	else {
		struct au_cpup_simple_args args = {
			.errp		= &err,
			.dentry		= dentry,
			.bdst		= bdst,
			.len		= len,
			.flags		= flags
		};
		wkq_err = au_wkq_wait(au_call_cpup_simple, &args, /*dlgt*/0);
		if (unlikely(wkq_err))
			err = wkq_err;
	}

	dput(parent);
	AuTraceErr(err);
	return err;
}

/* ---------------------------------------------------------------------- */

/*
 * copyup the deleted file for writing.
 */
int au_cpup_wh(struct dentry *dentry, aufs_bindex_t bdst, loff_t len,
	       struct file *file)
{
	int err;
	struct dentry *parent, *h_parent, *wh_dentry, *h_dentry_bdst,
		*h_dentry_bstart, *gparent;
	struct inode *h_dir;
	struct super_block *sb;
	struct au_dtime dt;
	struct aufs_dinfo *dinfo;
	aufs_bindex_t bstart;
	struct vfsub_args vargs;
	struct aufs_hinode *hgdir;
	struct aufs_ndx ndx = {
		.nd	= NULL,
		//.br	= NULL
	};
	struct au_cpup_flags cflags = {
		.dtime = 0
	};

	LKTRTrace("%.*s, bdst %d, len %Lu\n", AuDLNPair(dentry), bdst, len);
	AuDebugOn(S_ISDIR(dentry->d_inode->i_mode)
		  || (file && !(file->f_mode & FMODE_WRITE)));
	DiMustWriteLock(dentry);

	parent = dget_parent(dentry);
	IiMustAnyLock(parent->d_inode);
	h_parent = au_h_dptr_i(parent, bdst);
	AuDebugOn(!h_parent);
	h_dir = h_parent->d_inode;
	AuDebugOn(!h_dir);

	sb = parent->d_sb;
	ndx.nfsmnt = au_nfsmnt(sb, bdst);
	ndx.dlgt = au_need_dlgt(sb);
	wh_dentry = lkup_whtmp(h_parent, &dentry->d_name, &ndx);
	//if (LktrCond) {dput(wh_dentry); wh_dentry = ERR_PTR(-1);}
	err = PTR_ERR(wh_dentry);
	if (IS_ERR(wh_dentry))
		goto out;

	hgdir = NULL;
	if (unlikely(au_flag_test_udba_inotify(sb) && !IS_ROOT(parent))) {
		gparent = dget_parent(parent);
		hgdir = itohi(gparent->d_inode, bdst);
		dput(gparent);
	}
	au_dtime_store(&dt, parent, h_parent, hgdir);
	dinfo = dtodi(dentry);
	bstart = dinfo->di_bstart;
	h_dentry_bdst = dinfo->di_hdentry[0 + bdst].hd_dentry;
	dinfo->di_bstart = bdst;
	dinfo->di_hdentry[0 + bdst].hd_dentry = wh_dentry;
	h_dentry_bstart = dinfo->di_hdentry[0 + bstart].hd_dentry;
	if (file)
		dinfo->di_hdentry[0 + bstart].hd_dentry
			= au_h_fptr(file)->f_dentry;
	err = au_cpup_single(dentry, bdst, bstart, len, &cflags);
	//if (LktrCond) err = -1;
	if (!err && file) {
		err = au_reopen_nondir(file);
		//err = -1;
		dinfo->di_hdentry[0 + bstart].hd_dentry = h_dentry_bstart;
	}
	dinfo->di_hdentry[0 + bdst].hd_dentry = h_dentry_bdst;
	dinfo->di_bstart = bstart;
	if (unlikely(err))
		goto out_wh;

	AuDebugOn(!d_unhashed(dentry));
	/* dget first to force sillyrename on nfs */
	dget(wh_dentry);
	vfsub_args_init(&vargs, NULL, ndx.dlgt, 0);
	err = vfsub_unlink(h_dir, wh_dentry, &vargs);
	//if (LktrCond) err = -1;
	if (unlikely(err)) {
		AuIOErr("failed remove copied-up tmp file %.*s(%d)\n",
			AuDLNPair(wh_dentry), err);
		err = -EIO;
	}
	au_dtime_revert(&dt);
	set_hi_wh(dentry->d_inode, bdst, wh_dentry);

 out_wh:
	dput(wh_dentry);
 out:
	dput(parent);
	AuTraceErr(err);
	//au_debug_off();
	return err;
}

struct au_cpup_wh_args {
	int *errp;
	struct dentry *dentry;
	aufs_bindex_t bdst;
	loff_t len;
	struct file *file;
};

static void au_call_cpup_wh(void *args)
{
	struct au_cpup_wh_args *a = args;
	*a->errp = au_cpup_wh(a->dentry, a->bdst, a->len, a->file);
}

int au_sio_cpup_wh(struct dentry *dentry, aufs_bindex_t bdst, loff_t len,
		   struct file *file)
{
	int err, wkq_err;
	struct dentry *parent;
	struct inode *dir, *h_dir;

	AuTraceEnter();
	parent = dget_parent(dentry);
	dir = parent->d_inode;
	IiMustAnyLock(dir);
	h_dir = au_h_iptr_i(dir, bdst);

	if (!au_test_perm(h_dir, MAY_EXEC | MAY_WRITE,
			  au_need_dlgt(dentry->d_sb)))
		err = au_cpup_wh(dentry, bdst, len, file);
	else {
		struct au_cpup_wh_args args = {
			.errp	= &err,
			.dentry	= dentry,
			.bdst	= bdst,
			.len	= len,
			.file	= file
		};
		wkq_err = au_wkq_wait(au_call_cpup_wh, &args, /*dlgt*/0);
		if (unlikely(wkq_err))
			err = wkq_err;
	}
	dput(parent);

	AuTraceErr(err);
	return err;
}

/* ---------------------------------------------------------------------- */

/*
 * generic routine for both of copy-up and copy-down.
 * Although I've tried building a path by dcsub, I gave up this approach.
 * Since the ancestor directory may be moved/renamed during copy.
 */
/* cf. revalidate function in file.c */
int au_cp_dirs(struct dentry *dentry, aufs_bindex_t bdst, struct dentry *locked,
	       int (*cp)(struct dentry *dentry, aufs_bindex_t bdst,
			 struct dentry *h_parent, void *arg),
	       void *arg)
{
	int err;
	struct super_block *sb;
	struct dentry *d, *parent, *h_parent, *gparent, *real_parent;
	unsigned int udba;

	LKTRTrace("%.*s, b%d, parent i%lu, locked %p\n",
		  AuDLNPair(dentry), bdst, parent_ino(dentry), locked);
	sb = dentry->d_sb;
	AuDebugOn(au_test_ro(sb, bdst, NULL));
	err = 0;
	parent = dget_parent(dentry);
	IiMustWriteLock(parent->d_inode);
	if (unlikely(IS_ROOT(parent)))
		goto out;
	if (locked) {
		DiMustAnyLock(locked);
		IiMustAnyLock(locked->d_inode);
	}

	/* slow loop, keep it simple and stupid */
	real_parent = parent;
	udba = au_flag_test_udba_inotify(sb);
	while (1) {
		dput(parent);
		parent = dget_parent(dentry);
		h_parent = au_h_dptr_i(parent, bdst);
		if (h_parent)
			goto out; /* success */

		/* find top dir which is needed to cpup */
		do {
			d = parent;
			dput(parent);
			parent = dget_parent(d);
			if (parent != locked) {
				di_read_lock_parent3(parent, !AuLock_IR);
				h_parent = au_h_dptr_i(parent, bdst);
				di_read_unlock(parent, !AuLock_IR);
			} else
				h_parent = au_h_dptr_i(parent, bdst);
		} while (!h_parent);

		if (d != real_parent)
			di_write_lock_child3(d);

		/* somebody else might create while we were sleeping */
		if (!au_h_dptr_i(d, bdst) || !au_h_dptr_i(d, bdst)->d_inode) {
			struct inode *h_dir = h_parent->d_inode,
				*dir = parent->d_inode;

			if (au_h_dptr_i(d, bdst))
				au_update_dbstart(d);
			//AuDebugOn(dbstart(d) <= bdst);
			if (parent != locked)
				di_read_lock_parent3(parent, AuLock_IR);
			gparent = NULL;
			if (unlikely(udba && !IS_ROOT(parent))) {
				gparent = dget_parent(parent);
				if (gparent != locked)
					ii_read_lock_parent4(gparent->d_inode);
				else {
					dput(gparent);
					gparent = NULL;
				}
			}
			hdir_lock(h_dir, dir, bdst);
			err = cp(d, bdst, h_parent, arg);
			//if (LktrCond) err = -1;
			hdir_unlock(h_dir, dir, bdst);
			if (unlikely(gparent)) {
				ii_read_unlock(gparent->d_inode);
				dput(gparent);
			}
			if (parent != locked)
				di_read_unlock(parent, AuLock_IR);
		}

		if (d != real_parent)
			di_write_unlock(d);
		if (unlikely(err))
			break;
	}

 out:
	dput(parent);
	AuTraceErr(err);
	return err;
}

static int au_cpup_dir(struct dentry *dentry, aufs_bindex_t bdst,
		       struct dentry *h_parent, void *arg)
{
	int err;
	struct au_cpup_flags cflags = {
		.dtime = 1
	};

	err = au_sio_cpup_simple(dentry, bdst, -1, &cflags);

	AuTraceErr(err);
	return err;
}

int au_cpup_dirs(struct dentry *dentry, aufs_bindex_t bdst,
		 struct dentry *locked)
{
	int err;

	err = au_cp_dirs(dentry, bdst, locked, au_cpup_dir, NULL);

	AuTraceErr(err);
	return err;
}

int au_test_and_cpup_dirs(struct dentry *dentry, aufs_bindex_t bdst,
			  struct dentry *locked)
{
	int err;
	struct dentry *parent;
	struct inode *dir;

	parent = dget_parent(dentry);
	dir = parent->d_inode;
	LKTRTrace("%.*s, b%d, parent i%lu, locked %p\n",
		  AuDLNPair(dentry), bdst, dir->i_ino, locked);
	DiMustReadLock(parent);
	IiMustReadLock(dir);

	err = 0;
	if (au_h_iptr_i(dir, bdst))
		goto out;

	di_read_unlock(parent, AuLock_IR);
	di_write_lock_parent2(parent);
	/* someone else might change our inode while we were sleeping */
	if (unlikely(!au_h_iptr_i(dir, bdst)))
		err = au_cpup_dirs(dentry, bdst, locked);
	di_downgrade_lock(parent, AuLock_IR);

 out:
	dput(parent);
	AuTraceErr(err);
	return err;
}
